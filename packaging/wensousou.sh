#!/usr/bin/env bash
set -euo pipefail

APP_HOME="${WENSOUSOU_HOME:-/opt/wensousou}"
BUNDLED_FCITX_PLUGIN="${APP_HOME}/plugins/platforminputcontexts/libfcitxplatforminputcontextplugin.so"
STATE_HOME="${XDG_STATE_HOME:-${HOME}/.local/state}/wensousou"
mkdir -p "${STATE_HOME}"

export WENSOUSOU_HOME="${APP_HOME}"
export LD_LIBRARY_PATH="${APP_HOME}/lib${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"
export QT_PLUGIN_PATH="${APP_HOME}/plugins"
export QT_QPA_PLATFORM=xcb
if [[ -z "${QT_IM_MODULE:-}" && -e "${BUNDLED_FCITX_PLUGIN}" ]]; then
  export QT_IM_MODULE=fcitx
fi

{
  printf '\n[%s] launching wensousou\n' "$(date '+%Y-%m-%dT%H:%M:%S%z')"
  printf 'DISPLAY=%s\n' "${DISPLAY:-}"
  printf 'WAYLAND_DISPLAY=%s\n' "${WAYLAND_DISPLAY:-}"
  printf 'XDG_SESSION_TYPE=%s\n' "${XDG_SESSION_TYPE:-}"
  printf 'QT_QPA_PLATFORM=%s\n' "${QT_QPA_PLATFORM}"
  printf 'QT_PLUGIN_PATH=%s\n' "${QT_PLUGIN_PATH}"
  printf 'QT_IM_MODULE=%s\n' "${QT_IM_MODULE:-}"
  printf 'GTK_IM_MODULE=%s\n' "${GTK_IM_MODULE:-}"
  printf 'XMODIFIERS=%s\n' "${XMODIFIERS:-}"
} >>"${STATE_HOME}/launcher.log"

case "${1:-}" in
  --self-check|--diagnose-index)
    exec "${APP_HOME}/bin/wensousou" "$@"
    ;;
  *)
    exec "${APP_HOME}/bin/wensousou" "$@" >>"${STATE_HOME}/launcher.log" 2>&1
    ;;
esac
