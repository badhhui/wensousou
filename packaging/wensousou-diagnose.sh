#!/usr/bin/env bash
set -u

APP_HOME="${WENSOUSOU_HOME:-/opt/wensousou}"
SYSTEM_QT_PLUGIN_PATH="${WENSOUSOU_SYSTEM_QT_PLUGIN_PATH:-/usr/lib/aarch64-linux-gnu/qt5/plugins}"
BUNDLED_FCITX_PLUGIN="${APP_HOME}/plugins/platforminputcontexts/libfcitxplatforminputcontextplugin.so"
SYSTEM_FCITX_PLUGIN="${SYSTEM_QT_PLUGIN_PATH}/platforminputcontexts/libfcitxplatforminputcontextplugin.so"
STATE_HOME="${XDG_STATE_HOME:-${HOME}/.local/state}/wensousou"
status=0

check_file() {
  if [[ -e "$1" ]]; then
    printf '[OK] %s\n' "$1"
  else
    printf '[MISSING] %s\n' "$1"
    status=1
  fi
}

printf '== 文搜搜安装诊断 ==\n'
printf 'DISPLAY=%s\n' "${DISPLAY:-}"
printf 'WAYLAND_DISPLAY=%s\n' "${WAYLAND_DISPLAY:-}"
printf 'XDG_SESSION_TYPE=%s\n' "${XDG_SESSION_TYPE:-}"
printf 'QT_PLUGIN_PATH=%s\n' "${QT_PLUGIN_PATH:-}"
printf 'QT_IM_MODULE=%s\n' "${QT_IM_MODULE:-}"
printf 'GTK_IM_MODULE=%s\n' "${GTK_IM_MODULE:-}"
printf 'XMODIFIERS=%s\n' "${XMODIFIERS:-}"
printf '\n== 文件 ==\n'
check_file "${APP_HOME}/bin/wensousou"
check_file "${APP_HOME}/plugins/platforms/libqxcb.so"
check_file "${BUNDLED_FCITX_PLUGIN}"
if [[ -e "${SYSTEM_FCITX_PLUGIN}" ]]; then
  printf '[OK] %s\n' "${SYSTEM_FCITX_PLUGIN}"
else
  printf '[INFO] 系统 Fcitx Qt5 插件不存在；将使用文搜搜内置插件。\n'
fi
check_file "${APP_HOME}/lib/libQt5DBus.so.5"
check_file "${APP_HOME}/lib/libQt5Gui.so.5"
check_file "${APP_HOME}/lib/libsimple.so"
check_file "${APP_HOME}/runtime/jre/bin/java"
check_file "${APP_HOME}/parser/parser-worker.jar"

printf '\n== XCB 插件依赖 ==\n'
if [[ -e "${APP_HOME}/plugins/platforms/libqxcb.so" ]]; then
  LD_LIBRARY_PATH="${APP_HOME}/lib${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" \
    ldd "${APP_HOME}/plugins/platforms/libqxcb.so"
fi

printf '\n== Fcitx 进程 ==\n'
if ps -ef | grep -E '[ /]fcitx([[:space:]]|$)' | grep -v grep; then
  printf '[OK] 检测到 Fcitx 进程。\n'
else
  printf '[WARN] 未检测到 Fcitx 进程，请先启动系统输入法。\n'
fi

printf '\n== 内置 Fcitx Qt5 插件依赖 ==\n'
if [[ -e "${BUNDLED_FCITX_PLUGIN}" ]]; then
  if LD_LIBRARY_PATH="${APP_HOME}/lib${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" \
      ldd "${BUNDLED_FCITX_PLUGIN}" | tee /dev/stderr | grep -q 'not found'; then
    status=1
  fi
fi

printf '\n== Qt GUI 字体依赖 ==\n'
if [[ -e "${APP_HOME}/lib/libQt5Gui.so.5" ]]; then
  LD_LIBRARY_PATH="${APP_HOME}/lib${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" \
    ldd "${APP_HOME}/lib/libQt5Gui.so.5" |
    grep -E 'fontconfig|freetype|not found' || true
fi

printf '\n== 自检 ==\n'
if ! /usr/bin/wensousou --self-check; then
  status=1
fi

printf '\n== 实际索引状态 ==\n'
/usr/bin/wensousou --diagnose-index "${1:-}" || status=1

printf '\n== launcher.log 末尾 ==\n'
tail -n 80 "${STATE_HOME}/launcher.log" 2>/dev/null || true
printf '\n== wensousou.log 末尾 ==\n'
tail -n 80 "${STATE_HOME}/wensousou.log" 2>/dev/null || true

exit "${status}"
