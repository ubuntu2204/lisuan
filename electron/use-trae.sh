#!/bin/bash
# use-trae.sh - Trae / Trae CN（字节 AI IDE, Electron）启动 wrapper，砺算 7G100 + Ubuntu 24.04
#
# 默认软件渲染稳定模式（X11 + ANGLE/SwiftShader），绕开 gf EGL 的黑屏/花屏；
# 需要硬件加速对比时加 --hw（可能复现显示错误）。
# 复用 use-electron-gf.sh 的通用开关；gf env 只注入本应用进程树。
#
# 国际版 trae / 国内版 trae-cn 均可：
#   bash use-trae.sh                        # 优先国际版；未装则回退国内版
#   bash use-trae.sh --app trae-cn          # 只启动国内版
#   bash use-trae.sh --hw                   # 硬件加速对比（默认软件渲染）
#
# 把应用菜单指到本 wrapper（国际版和国内版桌面项都装）：
#   bash use-trae.sh --install-desktop
# 只装某一个变体：
#   bash use-trae.sh --app trae-cn --install-desktop
# 还原：
#   bash use-trae.sh --uninstall-desktop
#
# License: Apache-2.0

set -u

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BASE="$DIR/use-electron-gf.sh"

APP=""
APP_SET=0
ARGS=()
while [ $# -gt 0 ]; do
  case "$1" in
    --app)
      APP="${2:-}"
      APP_SET=1
      shift 2
      ;;
    --app=*)
      APP="${1#--app=}"
      APP_SET=1
      shift
      ;;
    *)
      ARGS+=("$1")
      shift
      ;;
  esac
done

case "$APP" in
  ""|trae|trae-cn) ;;
  *) echo "!! 未知 --app: $APP（仅支持 trae / trae-cn）" >&2; exit 1 ;;
esac

MODE=""
START=0
for a in "${ARGS[@]}"; do
  case "$a" in
    --install-desktop|--uninstall-desktop) MODE="$a" ;;
    --start) START=1 ;;
  esac
done

default_app() {
  if [ -e "${TRAE_BIN:-/usr/share/trae/trae}" ] || command -v trae >/dev/null 2>&1; then
    echo trae
  else
    echo trae-cn
  fi
}

# 菜单用的"点图标即用"：未指定 --app 时同时修国际版和国内版桌面项。
# 以前的 wrapper 只修 trae.desktop，Trae CN 仍从系统原入口裸启（无软件渲染参数）。
if [ "$APP_SET" = 0 ] && [ -n "$MODE" ]; then
  rc=0
  for variant in trae trae-cn; do
    "$BASE" --app "$variant" "$MODE" || rc=1
  done
  if [ "$MODE" = "--install-desktop" ] && [ "$START" = 1 ]; then
    LAUNCH_ARGS=()
    for a in "${ARGS[@]}"; do
      case "$a" in --install-desktop) ;; *) LAUNCH_ARGS+=("$a") ;; esac
    done
    exec "$BASE" --app "$(default_app)" "${LAUNCH_ARGS[@]}"
  fi
  exit "$rc"
fi

if [ "$APP_SET" = 1 ]; then
  exec "$BASE" --app "$APP" "${ARGS[@]}"
fi
exec "$BASE" --app "$(default_app)" "${ARGS[@]}"
