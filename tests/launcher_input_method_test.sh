#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
temporary="$(mktemp -d)"
trap 'rm -rf "${temporary}"' EXIT

app="${temporary}/app"
system_plugins="${temporary}/system-plugins"
mkdir -p \
  "${app}/bin" \
  "${app}/plugins/platforminputcontexts" \
  "${system_plugins}/platforminputcontexts" \
  "${temporary}/home" \
  "${temporary}/state"
touch "${app}/plugins/platforminputcontexts/libfcitxplatforminputcontextplugin.so"

cat >"${app}/bin/wensousou" <<'EOF'
#!/usr/bin/env bash
printf 'QT_PLUGIN_PATH=%s\n' "${QT_PLUGIN_PATH:-}"
printf 'QT_IM_MODULE=%s\n' "${QT_IM_MODULE:-}"
EOF
chmod +x "${app}/bin/wensousou"

run_launcher() {
  HOME="${temporary}/home" \
  XDG_STATE_HOME="${temporary}/state" \
  WENSOUSOU_HOME="${app}" \
  WENSOUSOU_SYSTEM_QT_PLUGIN_PATH="${system_plugins}" \
    "${ROOT_DIR}/packaging/wensousou.sh" --diagnose-index
}

default_output="$(unset QT_IM_MODULE QT_PLUGIN_PATH; run_launcher)"
if [[ "${default_output}" != *"QT_PLUGIN_PATH=${app}/plugins"* ]]; then
  echo "Launcher did not expose the bundled Qt plugin path." >&2
  exit 1
fi
if [[ "${default_output}" == *"${system_plugins}"* ]]; then
  echo "Launcher exposed the full system Qt plugin path and may load an incompatible theme." >&2
  exit 1
fi
if [[ "${default_output}" != *"QT_IM_MODULE=fcitx"* ]]; then
  echo "Launcher did not default QT_IM_MODULE to fcitx." >&2
  exit 1
fi

custom_output="$(QT_IM_MODULE=ibus run_launcher)"
if [[ "${custom_output}" != *"QT_IM_MODULE=ibus"* ]]; then
  echo "Launcher did not preserve an existing QT_IM_MODULE value." >&2
  exit 1
fi

echo "Launcher input method test passed."
