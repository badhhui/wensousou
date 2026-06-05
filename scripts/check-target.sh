#!/usr/bin/env bash
set -euo pipefail

DEB="${1:-}"
if [[ -z "${DEB}" || ! -f "${DEB}" ]]; then
  echo "Usage: $0 wensousou_1.0.19_arm64.deb" >&2
  exit 2
fi
if [[ "$(uname -m)" != "aarch64" ]]; then
  echo "Unsupported architecture: $(uname -m); expected aarch64." >&2
  exit 1
fi
if ! command -v dpkg-deb >/dev/null; then
  echo "dpkg-deb is required." >&2
  exit 1
fi

glibc_version="$(ldd --version | head -n 1 | grep -Eo '[0-9]+\.[0-9]+' | tail -n 1)"
if [[ "$(printf '%s\n%s\n' "${glibc_version}" "2.28" | sort -V | head -n 1)" != "2.28" ]]; then
  echo "glibc ${glibc_version} is too old; expected at least 2.28." >&2
  exit 1
fi

temporary="$(mktemp -d)"
trap 'rm -rf "${temporary}"' EXIT
dpkg-deb -x "${DEB}" "${temporary}"
app="${temporary}/opt/wensousou"

echo "Checking bundled Qt XCB plugin dependencies..."
if LD_LIBRARY_PATH="${app}/lib" ldd "${app}/plugins/platforms/libqxcb.so" |
    grep -q 'not found'; then
  LD_LIBRARY_PATH="${app}/lib" ldd "${app}/plugins/platforms/libqxcb.so" |
    grep 'not found' >&2 || true
  echo "Missing desktop library dependencies. Install them from the UOS installation media." >&2
  exit 1
fi

echo "Checking bundled Fcitx Qt5 input method plugin dependencies..."
if LD_LIBRARY_PATH="${app}/lib" \
    ldd "${app}/plugins/platforminputcontexts/libfcitxplatforminputcontextplugin.so" |
    grep -q 'not found'; then
  LD_LIBRARY_PATH="${app}/lib" \
    ldd "${app}/plugins/platforminputcontexts/libfcitxplatforminputcontextplugin.so" |
    grep 'not found' >&2 || true
  echo "Missing Fcitx input method dependencies. Install libdbus-1-3 from the UOS installation media." >&2
  exit 1
fi

echo "Checking Qt GUI font dependencies..."
if LD_LIBRARY_PATH="${app}/lib" ldd "${app}/lib/libQt5Gui.so.5" |
    grep -q 'not found'; then
  LD_LIBRARY_PATH="${app}/lib" ldd "${app}/lib/libQt5Gui.so.5" |
    grep 'not found' >&2 || true
  echo "Missing font rendering dependencies. Install libfontconfig1 and libfreetype6 from the UOS installation media." >&2
  exit 1
fi

echo "Running application self-check..."
WENSOUSOU_HOME="${app}" \
WENSOUSOU_DB="${temporary}/self-check.db" \
LD_LIBRARY_PATH="${app}/lib" \
"${app}/bin/wensousou" --self-check

echo "Target preflight passed. Install with:"
echo "  sudo dpkg -i ${DEB}"
