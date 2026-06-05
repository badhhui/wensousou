#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
WORK_PARENT="$(cd -- "${ROOT_DIR}/.." && pwd)"
QT_PREFIX="${QT_PREFIX:-${WORK_PARENT}/wensousou-qt-5.15.2}"
BUILD_DIR="${ROOT_DIR}/build/uos-arm64"
DIST_DIR="${ROOT_DIR}/dist/offline-kit"
PACKAGE_ROOT="${ROOT_DIR}/build/package-root"
APP_ROOT="${PACKAGE_ROOT}/opt/wensousou"
VERSION="1.1.1"
DEB="${DIST_DIR}/wensousou_${VERSION}_arm64.deb"
MULTIARCH="$(gcc -print-multiarch)"
FCITX_PLUGIN="/usr/lib/${MULTIARCH}/qt5/plugins/platforminputcontexts/libfcitxplatforminputcontextplugin.so"

if [[ "$(uname -s)" != "Linux" || "$(uname -m)" != "aarch64" ]]; then
  echo "Run this script on the connected UOS ARM64 build machine." >&2
  exit 1
fi
required_commands=(cmake make gcc g++ dpkg-deb unzip tar strings)
if [[ "${WENSOUSOU_OFFLINE:-0}" != "1" ]]; then
  required_commands+=(curl)
fi
for command in "${required_commands[@]}"; do
  command -v "${command}" >/dev/null || {
    echo "Missing build command: ${command}" >&2
    exit 1
  }
done

"${ROOT_DIR}/scripts/prepare-deps.sh"
"${ROOT_DIR}/scripts/build-qt.sh"
"${ROOT_DIR}/parser/build.sh"
JAVA="${ROOT_DIR}/third_party/runtime/jre/bin/java" \
  "${ROOT_DIR}/tests/parser_worker_test.sh"
"${ROOT_DIR}/tests/launcher_input_method_test.sh"

echo "Cleaning application build directory to avoid stale objects..."
rm -rf "${BUILD_DIR}"
cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="${QT_PREFIX}" \
  -DBUILD_TESTING=ON
cmake --build "${BUILD_DIR}" --parallel "$(nproc)"
(cd "${BUILD_DIR}" && ctest --output-on-failure)

rm -rf "${PACKAGE_ROOT}" "${DIST_DIR}"
mkdir -p \
  "${APP_ROOT}/bin" \
  "${APP_ROOT}/lib" \
  "${APP_ROOT}/plugins/platforms" \
  "${APP_ROOT}/plugins/platforminputcontexts" \
  "${APP_ROOT}/parser" \
  "${APP_ROOT}/runtime" \
  "${APP_ROOT}/licenses" \
  "${PACKAGE_ROOT}/usr/bin" \
  "${PACKAGE_ROOT}/usr/share/applications" \
  "${PACKAGE_ROOT}/usr/share/icons/hicolor/scalable/apps" \
  "${PACKAGE_ROOT}/DEBIAN" \
  "${DIST_DIR}/licenses"

cp "${BUILD_DIR}/wensousou" "${APP_ROOT}/bin/"
cp "${ROOT_DIR}/resources/libsimple-linux-aarch64.so" "${APP_ROOT}/lib/libsimple.so"
cp "${ROOT_DIR}/parser/build/parser-worker.jar" "${APP_ROOT}/parser/"
cp "${ROOT_DIR}/third_party/cache/tika-app-3.3.1.jar" "${APP_ROOT}/parser/"
cp -a "${ROOT_DIR}/third_party/runtime/jre" "${APP_ROOT}/runtime/"
cp -a "${QT_PREFIX}/plugins/platforms/libqxcb.so" "${APP_ROOT}/plugins/platforms/"
if [[ ! -f "${FCITX_PLUGIN}" ]]; then
  echo "Missing Fcitx Qt5 input method plugin: ${FCITX_PLUGIN}" >&2
  echo "Install fcitx-frontend-qt5, then rerun this script." >&2
  exit 1
fi
cp -a "${FCITX_PLUGIN}" "${APP_ROOT}/plugins/platforminputcontexts/"
cp -a "${QT_PREFIX}"/lib/libQt5Core.so* "${APP_ROOT}/lib/"
cp -a "${QT_PREFIX}"/lib/libQt5*.so* "${APP_ROOT}/lib/"
if [[ -d "${QT_PREFIX}/qml" ]]; then
  cp -a "${QT_PREFIX}/qml" "${APP_ROOT}/"
else
  echo "Missing Qt QML imports in ${QT_PREFIX}; remove the old Qt cache and rebuild." >&2
  exit 1
fi
if compgen -G "${QT_PREFIX}/lib/libQt5XcbQpa.so*" >/dev/null; then
  cp -a "${QT_PREFIX}"/lib/libQt5XcbQpa.so* "${APP_ROOT}/lib/"
fi
if compgen -G "${QT_PREFIX}/lib/libQt5DBus.so*" >/dev/null; then
  cp -a "${QT_PREFIX}"/lib/libQt5DBus.so* "${APP_ROOT}/lib/"
else
  echo "Missing Qt DBus runtime in ${QT_PREFIX}; remove the old Qt cache and rebuild." >&2
  exit 1
fi
cp -a "$(gcc -print-file-name=libstdc++.so.6)" "${APP_ROOT}/lib/"
cp -a "$(gcc -print-file-name=libgcc_s.so.1)" "${APP_ROOT}/lib/"

cat >"${APP_ROOT}/bin/qt.conf" <<'EOF'
[Paths]
Plugins = ../plugins
Qml2Imports = ../qml
EOF
install -m 755 "${ROOT_DIR}/packaging/wensousou.sh" "${PACKAGE_ROOT}/usr/bin/wensousou"
install -m 755 "${ROOT_DIR}/packaging/wensousou-diagnose.sh" \
  "${PACKAGE_ROOT}/usr/bin/wensousou-diagnose"
install -m 644 "${ROOT_DIR}/packaging/wensousou.desktop" \
  "${PACKAGE_ROOT}/usr/share/applications/wensousou.desktop"
install -m 644 "${ROOT_DIR}/resources/wensousou.svg" \
  "${PACKAGE_ROOT}/usr/share/icons/hicolor/scalable/apps/wensousou.svg"
install -m 644 "${ROOT_DIR}/THIRD_PARTY_NOTICES.md" "${APP_ROOT}/licenses/"
if [[ -f /usr/share/doc/fcitx-frontend-qt5/copyright ]]; then
  install -m 644 /usr/share/doc/fcitx-frontend-qt5/copyright \
    "${APP_ROOT}/licenses/Fcitx-Qt5-copyright"
fi
if [[ -f "${ROOT_DIR}/third_party/cache/qt-everywhere-src-5.15.2/LICENSE.LGPL3" ]]; then
  install -m 644 "${ROOT_DIR}/third_party/cache/qt-everywhere-src-5.15.2/LICENSE.LGPL3" \
    "${APP_ROOT}/licenses/Qt-LICENSE.LGPL3"
fi
unzip -p "${ROOT_DIR}/third_party/cache/tika-app-3.3.1.jar" META-INF/LICENSE \
  >"${APP_ROOT}/licenses/Apache-Tika-LICENSE" || true
unzip -p "${ROOT_DIR}/third_party/cache/tika-app-3.3.1.jar" META-INF/NOTICE \
  >"${APP_ROOT}/licenses/Apache-Tika-NOTICE" || true

cat >"${PACKAGE_ROOT}/DEBIAN/control" <<EOF
Package: wensousou
Version: ${VERSION}
Section: office
Priority: optional
Architecture: arm64
Maintainer: WenSouSou Maintainers
Depends: libc6 (>= 2.28), libx11-6, libxcb1, libxkbcommon-x11-0, libfontconfig1, libfreetype6, libdbus-1-3
Description: Offline full-text document search for UOS ARM64
 Qt desktop search application with SQLite FTS5 and simple tokenizer.
EOF

check_glibc_requirement() {
  local file="$1"
  local maximum
  maximum="$(strings "${file}" | grep -Eo 'GLIBC_[0-9.]+' | sed 's/GLIBC_//' | sort -Vu | tail -n 1 || true)"
  if [[ -n "${maximum}" && "$(printf '%s\n%s\n' "${maximum}" "2.28" | sort -V | tail -n 1)" != "2.28" ]]; then
    echo "ABI check failed: ${file} requires GLIBC_${maximum}, target limit is GLIBC_2.28" >&2
    exit 1
  fi
}

while IFS= read -r -d '' native; do
  check_glibc_requirement "${native}"
done < <(find "${APP_ROOT}" -type f \( -name '*.so*' -o -perm -0100 \) -print0)

mkdir -p "${DIST_DIR}"
dpkg-deb --build --root-owner-group "${PACKAGE_ROOT}" "${DEB}"
install -m 755 "${ROOT_DIR}/scripts/check-target.sh" "${DIST_DIR}/"
install -m 644 "${ROOT_DIR}/INSTALL-OFFLINE.md" "${DIST_DIR}/"
install -m 644 "${ROOT_DIR}/THIRD_PARTY_NOTICES.md" "${DIST_DIR}/licenses/"
(cd "${DIST_DIR}" && sha256sum "$(basename "${DEB}")" >SHA256SUMS)
echo "Offline kit created at ${DIST_DIR}"
