#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
CACHE_DIR="${ROOT_DIR}/third_party/cache"
SQLITE_DIR="${ROOT_DIR}/third_party/sqlite"
RUNTIME_DIR="${ROOT_DIR}/third_party/runtime"
OFFLINE="${WENSOUSOU_OFFLINE:-0}"

SQLITE_VERSION="3530100"
SQLITE_URL="https://www.sqlite.org/2026/sqlite-amalgamation-${SQLITE_VERSION}.zip"
TIKA_URL="https://repo.maven.apache.org/maven2/org/apache/tika/tika-app/3.3.1/tika-app-3.3.1.jar"
JRE_URL="https://mirrors.tuna.tsinghua.edu.cn/Adoptium/11/jre/aarch64/linux/OpenJDK11U-jre_aarch64_linux_hotspot_11.0.31_11.tar.gz"
JDK_URL="https://mirrors.tuna.tsinghua.edu.cn/Adoptium/11/jdk/aarch64/linux/OpenJDK11U-jdk_aarch64_linux_hotspot_11.0.31_11.tar.gz"

mkdir -p "${CACHE_DIR}" "${SQLITE_DIR}" "${RUNTIME_DIR}"

download() {
  local url="$1"
  local destination="$2"
  if [[ ! -s "${destination}" ]]; then
    if [[ "${OFFLINE}" == "1" ]]; then
      echo "Missing cached dependency while offline: ${destination}" >&2
      echo "Copy third_party/cache/ from a prepared build directory, then rerun." >&2
      exit 1
    fi
    echo "Downloading ${url}"
    curl --fail --location --retry 3 --continue-at - \
      --output "${destination}.tmp" "${url}"
    mv "${destination}.tmp" "${destination}"
  fi
}

download "${SQLITE_URL}" "${CACHE_DIR}/sqlite-amalgamation-${SQLITE_VERSION}.zip"
download "${TIKA_URL}" "${CACHE_DIR}/tika-app-3.3.1.jar"

if [[ ! -f "${SQLITE_DIR}/sqlite3.c" ]]; then
  rm -rf "${CACHE_DIR}/sqlite-amalgamation-${SQLITE_VERSION}"
  unzip -q "${CACHE_DIR}/sqlite-amalgamation-${SQLITE_VERSION}.zip" -d "${CACHE_DIR}"
  cp "${CACHE_DIR}/sqlite-amalgamation-${SQLITE_VERSION}/sqlite3.c" "${SQLITE_DIR}/"
  cp "${CACHE_DIR}/sqlite-amalgamation-${SQLITE_VERSION}/sqlite3.h" "${SQLITE_DIR}/"
  cp "${CACHE_DIR}/sqlite-amalgamation-${SQLITE_VERSION}/sqlite3ext.h" "${SQLITE_DIR}/"
fi

if [[ "$(uname -s)" == "Linux" && "$(uname -m)" == "aarch64" ]]; then
  download "${JRE_URL}" "${CACHE_DIR}/OpenJDK11U-jre_aarch64_linux_hotspot_11.0.31_11.tar.gz"
  download "${JDK_URL}" "${CACHE_DIR}/OpenJDK11U-jdk_aarch64_linux_hotspot_11.0.31_11.tar.gz"
  if [[ ! -x "${RUNTIME_DIR}/jre/bin/java" ]]; then
    rm -rf "${RUNTIME_DIR}/jre" "${RUNTIME_DIR}/jre-extract"
    mkdir -p "${RUNTIME_DIR}/jre-extract"
    tar -xzf "${CACHE_DIR}/OpenJDK11U-jre_aarch64_linux_hotspot_11.0.31_11.tar.gz" \
      -C "${RUNTIME_DIR}/jre-extract"
    mv "$(find "${RUNTIME_DIR}/jre-extract" -mindepth 1 -maxdepth 1 -type d | head -n 1)" \
      "${RUNTIME_DIR}/jre"
    rm -rf "${RUNTIME_DIR}/jre-extract"
  fi
  if [[ ! -x "${RUNTIME_DIR}/build-jdk/bin/javac" ]]; then
    rm -rf "${RUNTIME_DIR}/build-jdk" "${RUNTIME_DIR}/build-jdk-extract"
    mkdir -p "${RUNTIME_DIR}/build-jdk-extract"
    tar -xzf "${CACHE_DIR}/OpenJDK11U-jdk_aarch64_linux_hotspot_11.0.31_11.tar.gz" \
      -C "${RUNTIME_DIR}/build-jdk-extract"
    mv "$(find "${RUNTIME_DIR}/build-jdk-extract" -mindepth 1 -maxdepth 1 -type d | head -n 1)" \
      "${RUNTIME_DIR}/build-jdk"
    rm -rf "${RUNTIME_DIR}/build-jdk-extract"
  fi
fi

echo "Dependencies prepared."
