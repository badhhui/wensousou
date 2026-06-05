#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd -- "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"
TIKA_JAR="${ROOT_DIR}/third_party/cache/tika-app-3.3.1.jar"
BUILD_JDK="${ROOT_DIR}/third_party/runtime/build-jdk"
JAVAC="${JAVAC:-}"
JAR="${JAR:-}"

if [[ -z "${JAVAC}" && -x "${BUILD_JDK}/bin/javac" ]]; then
  JAVAC="${BUILD_JDK}/bin/javac"
fi
if [[ -z "${JAR}" && -x "${BUILD_JDK}/bin/jar" ]]; then
  JAR="${BUILD_JDK}/bin/jar"
fi
if [[ -z "${JAVAC}" ]]; then
  JAVAC="$(command -v javac || true)"
fi
if [[ -z "${JAR}" ]]; then
  JAR="$(command -v jar || true)"
fi
if [[ -z "${JAVAC}" || -z "${JAR}" ]]; then
  echo "Missing javac or jar. Run scripts/prepare-deps.sh on the connected UOS ARM64 build machine." >&2
  exit 1
fi
if [[ ! -f "${TIKA_JAR}" ]]; then
  echo "Missing ${TIKA_JAR}; run scripts/prepare-deps.sh first." >&2
  exit 1
fi

rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}/classes"
"${JAVAC}" -encoding UTF-8 -source 8 -target 8 \
  -cp "${TIKA_JAR}" \
  -d "${BUILD_DIR}/classes" \
  "${SCRIPT_DIR}/src/com/wensousou/parser/ParserWorker.java"
cat >"${BUILD_DIR}/MANIFEST.MF" <<'EOF'
Manifest-Version: 1.0
Main-Class: com.wensousou.parser.ParserWorker
Class-Path: tika-app-3.3.1.jar

EOF
"${JAR}" cfm "${BUILD_DIR}/parser-worker.jar" "${BUILD_DIR}/MANIFEST.MF" \
  -C "${BUILD_DIR}/classes" .
echo "Built ${BUILD_DIR}/parser-worker.jar"
