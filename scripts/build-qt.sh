#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
CACHE_DIR="${ROOT_DIR}/third_party/cache"
WORK_PARENT="$(cd -- "${ROOT_DIR}/.." && pwd)"
QT_PREFIX="${QT_PREFIX:-${WORK_PARENT}/wensousou-qt-5.15.2}"
QT_ARCHIVE="${CACHE_DIR}/qt-everywhere-src-5.15.2.tar.xz"
QT_WORK_ROOT="${WENSOUSOU_QT_WORK_ROOT:-${WORK_PARENT}/wensousou-qt-build-5.15.2-$(id -u)}"
QT_SOURCE="${QT_WORK_ROOT}/qt-everywhere-src-5.15.2"
QT_BUILD="${QT_WORK_ROOT}/build"
QT_URL="https://mirrors.tuna.tsinghua.edu.cn/qt/archive/qt/5.15/5.15.2/single/qt-everywhere-src-5.15.2.tar.xz"
QT_CONFIG_MARKER="${QT_PREFIX}/.wensousou-fontconfig-dbus-v2"
OFFLINE="${WENSOUSOU_OFFLINE:-0}"

qt_is_ready() {
  [[ -x "${QT_PREFIX}/bin/qmake" ]] &&
    [[ -f "${QT_PREFIX}/lib/libQt5Widgets.so" ]] &&
    [[ -f "${QT_PREFIX}/lib/cmake/Qt5/Qt5Config.cmake" ]] &&
    [[ -f "${QT_PREFIX}/plugins/platforms/libqxcb.so" ]] &&
    [[ -f "${QT_CONFIG_MARKER}" ]]
}

check_qt_build_dependencies() {
  local dependency
  local -a missing=()
  local -a required=(
    "xcb >= 1.11"
    "xcb-icccm >= 0.3.9"
    "xcb-util >= 0.3.8"
    "xcb-image >= 0.3.9"
    "xcb-keysyms >= 0.3.9"
    "xcb-renderutil >= 0.3.9"
    "xcb-randr"
    "xcb-render"
    "xcb-shape"
    "xcb-shm"
    "xcb-sync"
    "xcb-xfixes"
    "xcb-xinerama"
    "xcb-xkb"
    "xkbcommon >= 0.5.0"
    "xkbcommon-x11"
    "fontconfig"
    "freetype2"
    "dbus-1"
  )

  command -v pkg-config >/dev/null || {
    echo "Missing build command: pkg-config" >&2
    exit 1
  }
  for dependency in "${required[@]}"; do
    pkg-config --exists "${dependency}" || missing+=("${dependency}")
  done
  if ((${#missing[@]})); then
    echo "Missing Qt build dependencies:" >&2
    printf '  - %s\n' "${missing[@]}" >&2
    echo "Install them on the connected UOS build machine, then rerun this script:" >&2
    echo "  ./scripts/install-uos-build-deps.sh" >&2
    exit 1
  fi
}

check_cxx_toolchain() {
  local test_dir="${QT_WORK_ROOT}/toolchain-test"
  rm -rf "${test_dir}"
  mkdir -p "${test_dir}"
  printf '%s\n' 'int main() { return 0; }' >"${test_dir}/main.cpp"
  if ! g++ "${test_dir}/main.cpp" -o "${test_dir}/main"; then
    echo "Cannot compile a minimal C++ program with g++." >&2
    echo "Repair the UOS build toolchain, then rerun this script." >&2
    exit 1
  fi
  rm -rf "${test_dir}"
}

show_qt_config_log() {
  local config_log="${QT_BUILD}/config.log"
  if [[ -f "${config_log}" ]]; then
    echo >&2
    echo "Qt configuration failed. Last 120 lines of ${config_log}:" >&2
    tail -n 120 "${config_log}" >&2
  fi
}

if [[ "$(uname -s)" != "Linux" || "$(uname -m)" != "aarch64" ]]; then
  echo "Qt runtime must be built on the connected UOS ARM64 machine." >&2
  exit 1
fi
if LC_ALL=C grep -q '[^ -~]' <<<"${QT_PREFIX}${QT_WORK_ROOT}"; then
  echo "Qt build and install paths must contain ASCII characters only:" >&2
  echo "  QT_PREFIX=${QT_PREFIX}" >&2
  echo "  WENSOUSOU_QT_WORK_ROOT=${QT_WORK_ROOT}" >&2
  exit 1
fi
if qt_is_ready; then
  echo "Qt already exists at ${QT_PREFIX}"
  exit 0
fi
check_qt_build_dependencies
check_cxx_toolchain

mkdir -p "${CACHE_DIR}"
if [[ ! -s "${QT_ARCHIVE}" ]]; then
  if [[ "${OFFLINE}" == "1" ]]; then
    echo "Missing cached Qt source archive while offline: ${QT_ARCHIVE}" >&2
    echo "Copy third_party/cache/ from a prepared build directory, then rerun." >&2
    exit 1
  fi
  curl --fail --location --retry 3 --continue-at - \
    --output "${QT_ARCHIVE}.tmp" "${QT_URL}"
  mv "${QT_ARCHIVE}.tmp" "${QT_ARCHIVE}"
fi
rm -rf "${QT_WORK_ROOT}"
mkdir -p "${QT_WORK_ROOT}"
tar -xJf "${QT_ARCHIVE}" -C "${QT_WORK_ROOT}"

if [[ ! -f "${QT_SOURCE}/qtbase/mkspecs/linux-g++/qmake.conf" ]]; then
  echo "Missing Qt mkspec: linux-g++" >&2
  exit 1
fi

# UOS 1070 ships xcb-util 0.3.8.1. Qt's XCB plugin uses APIs already available
# in that release, so relax the upstream pkg-config gate from 0.3.9 to 0.3.8.
sed -i \
  's/xcb-util >= 0[.]3[.]9/xcb-util >= 0.3.8/g' \
  "${QT_SOURCE}/qtbase/src/gui/configure.json"
if ! grep -Fq '"args": "xcb-util >= 0.3.8"' "${QT_SOURCE}/qtbase/src/gui/configure.json"; then
  echo "Failed to apply the UOS xcb-util compatibility patch." >&2
  grep -n 'xcb-util' "${QT_SOURCE}/qtbase/src/gui/configure.json" >&2 || true
  exit 1
fi

# On some UOS releases the bootstrap qmake does not locate the adjacent
# bin/qt.conf automatically. Pass it explicitly so HostSpec and TargetSpec are
# available while generating the qtbase makefile.
sed -i \
  's|"$outpath/bin/qmake" "$relpathMangled" -- "$@"|"$outpath/bin/qmake" -qtconf "$QTCONFFILE" "$relpathMangled" -- "$@"|' \
  "${QT_SOURCE}/qtbase/configure"
if ! grep -Fq '"$outpath/bin/qmake" -qtconf "$QTCONFFILE"' "${QT_SOURCE}/qtbase/configure"; then
  echo "Failed to apply the UOS qt.conf compatibility patch." >&2
  exit 1
fi

rm -rf "${QT_PREFIX}" "${QT_BUILD}"
mkdir -p "${QT_BUILD}"
pushd "${QT_BUILD}" >/dev/null
unset QMAKESPEC XQMAKESPEC QMAKEPATH QMAKEFEATURES
if ! "${QT_SOURCE}/qtbase/configure" \
  -platform linux-g++ \
  -prefix "${QT_PREFIX}" \
  -opensource -confirm-license \
  -release -shared \
  -no-icu -no-opengl \
  -fontconfig -system-freetype \
  -dbus-linked -no-cups \
  -xcb \
  -nomake examples -nomake tests; then
  show_qt_config_log
  exit 1
fi
make -j"$(nproc)"
make install
touch "${QT_CONFIG_MARKER}"
popd >/dev/null
echo "Qt installed at ${QT_PREFIX}"
