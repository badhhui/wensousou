#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
CACHE_DIR="${ROOT_DIR}/third_party/cache"

SQLITE_VERSION="3530100"
SQLITE_URL="https://www.sqlite.org/2026/sqlite-amalgamation-${SQLITE_VERSION}.zip"
TIKA_URL="https://repo.maven.apache.org/maven2/org/apache/tika/tika-app/3.3.1/tika-app-3.3.1.jar"
JRE_URL="https://mirrors.tuna.tsinghua.edu.cn/Adoptium/11/jre/aarch64/linux/OpenJDK11U-jre_aarch64_linux_hotspot_11.0.31_11.tar.gz"
JDK_URL="https://mirrors.tuna.tsinghua.edu.cn/Adoptium/11/jdk/aarch64/linux/OpenJDK11U-jdk_aarch64_linux_hotspot_11.0.31_11.tar.gz"
QT_URL="https://mirrors.tuna.tsinghua.edu.cn/qt/archive/qt/5.15/5.15.2/single/qt-everywhere-src-5.15.2.tar.xz"

mkdir -p "${CACHE_DIR}"

download() {
  local url="$1"
  local destination="$2"
  if [[ -s "${destination}" ]]; then
    echo "Using cached $(basename "${destination}")"
    return
  fi
  echo "Downloading ${url}"
  curl --fail --location --retry 3 --continue-at - \
    --output "${destination}.tmp" "${url}"
  mv "${destination}.tmp" "${destination}"
}

download "${SQLITE_URL}" "${CACHE_DIR}/sqlite-amalgamation-${SQLITE_VERSION}.zip"
download "${TIKA_URL}" "${CACHE_DIR}/tika-app-3.3.1.jar"
download "${JRE_URL}" "${CACHE_DIR}/OpenJDK11U-jre_aarch64_linux_hotspot_11.0.31_11.tar.gz"
download "${JDK_URL}" "${CACHE_DIR}/OpenJDK11U-jdk_aarch64_linux_hotspot_11.0.31_11.tar.gz"
download "${QT_URL}" "${CACHE_DIR}/qt-everywhere-src-5.15.2.tar.xz"

if command -v sha256sum >/dev/null 2>&1; then
  HASH_COMMAND=(sha256sum)
elif command -v shasum >/dev/null 2>&1; then
  HASH_COMMAND=(shasum -a 256)
else
  echo "missing checksum command: sha256sum or shasum" >&2
  exit 1
fi

(cd "${CACHE_DIR}" && "${HASH_COMMAND[@]}" \
  "sqlite-amalgamation-${SQLITE_VERSION}.zip" \
  "tika-app-3.3.1.jar" \
  "OpenJDK11U-jre_aarch64_linux_hotspot_11.0.31_11.tar.gz" \
  "OpenJDK11U-jdk_aarch64_linux_hotspot_11.0.31_11.tar.gz" \
  "qt-everywhere-src-5.15.2.tar.xz" \
  >SHA256SUMS)

echo "All offline build dependencies are cached in ${CACHE_DIR}"
