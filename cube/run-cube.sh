#!/usr/bin/env bash
# 至砺算：方块游戏平台（Cube Game）单应用启动器，供桌面入口调用。
#
# 只在 wine 进程树上设置 Proton 11 的运行库环境与独立前缀，
# 不污染全局 shell、不修改 Steam 自身文件。单次前台启动，
# 不做存活监控、自动重启或守护；退出码原样透传。
#
# 平台主窗口是 Depth 32/ARGB 窗口，在 Wayland/XWayland 合成下会完全不可见；
# 默认注入 XLIB_SKIP_ARGB_VISUALS=1 让它使用 Depth 24（与 11 平台同一根因）。
#
# 启动前必须先执行一次 ./cube/fix-cube-fonts.sh：CEF 43.3 在 Wine 11 下因
# 系统字体与 UISettings stub 递归耗尽 GDI 句柄，窗口会建出来但永不显示。
# 本脚本默认用 WINEDLLOVERRIDES=windows.ui=n 让 UISettings 激活失败，
# CEF 随即走非 WinRT 回退路径；字体修复负责消除另一条 GDI 泄漏链。
#
# 用法：
#   ./cube/run-cube.sh [参数...]   # 启动平台（参数透传给 Cube.exe）
#   ./cube/run-cube.sh --status    # 只读体检
#   ./cube/run-cube.sh --help
#
# 环境变量：CUBE_PROTON / CUBE_PREFIX / CUBE_WINEDEBUG /
#   CUBE_SKIP_ARGB_VISUALS（=0 可关闭 ARGB 规避，用于 A/B 对照）/
#   CUBE_VK_ICD（覆盖 Vulkan ICD；=skip 不注入，用于 A/B）
# License: Apache-2.0
set -euo pipefail

PROTON="${CUBE_PROTON:-$HOME/.local/share/Steam/steamapps/common/Proton 11.0}"
PROTON="${PROTON/#\~/$HOME}"
PREFIX="${CUBE_PREFIX:-$HOME/.deepinwine/cube-4121}"
PREFIX="${PREFIX/#\~/$HOME}"
INSTALL_REL='Program Files (x86)/Cube/Cube Platform'
MAIN="$PREFIX/drive_c/$INSTALL_REL/Cube.exe"
WINE="$PROTON/files/bin/wine"

die() { echo "错误：$*" >&2; exit 1; }

usage() {
    cat <<EOF
用法：$(basename "$0") [选项] [参数...]

  （无参数） 前台启动方块游戏平台（参数透传给 Cube.exe）
  --status   只读体检：wine、安装前缀、主程序 PE
  --help     显示本说明

环境变量：CUBE_PROTON、CUBE_PREFIX、CUBE_WINEDEBUG、
          CUBE_SKIP_ARGB_VISUALS（默认 1，规避 Depth 32 窗口不可见）、
          CUBE_VK_ICD（默认 lavapipe ICD；=skip 不注入）、
          CUBE_WINEDLLOVERRIDES（默认 windows.ui=n，用于 A/B）。
前置：先运行 ./cube/fix-cube-fonts.sh（五类系统字体改为 Arial）。
崩溃后不会自动重启；请按 .codebuddy/rules/崩溃处理约定.md 取证定位。
EOF
}

case "${1:-}" in
    --help|-h) usage; exit 0 ;;
    --status)
        [[ -x "$WINE" ]] && echo "  [OK] wine：$WINE" || { echo "  [--] wine 缺失：$WINE"; exit 1; }
        [[ -s "$MAIN" && "$(head -c2 "$MAIN")" == "MZ" ]] && \
            echo "  [OK] 主程序：$MAIN" || { echo "  [--] 主程序缺失或不是 PE：$MAIN"; exit 1; }
        exit 0
        ;;
esac

[[ -x "$WINE" ]] || die "找不到 Proton 11 的 wine：$WINE"
[[ -s "$MAIN" && "$(head -c2 "$MAIN")" == "MZ" ]] || die "主程序缺失或不是有效 PE：$MAIN"

# 本机 gf 驱动只提供 64 位 Vulkan ICD，32 位应用链只能走 llvmpipe（lavapipe）。
vk_icd="${CUBE_VK_ICD:-/usr/share/vulkan/icd.d/lvp_icd.json}"
vk_env=()
if [[ "$vk_icd" != "skip" && -f "$vk_icd" ]]; then
    vk_env=(VK_ICD_FILENAMES="$vk_icd")
fi

libs="$PROTON/files/lib/x86_64-linux-gnu:$PROTON/files/lib/i386-linux-gnu"
[[ -n "${LD_LIBRARY_PATH:-}" ]] && libs="$libs:$LD_LIBRARY_PATH"
cd "$(dirname "$MAIN")"
exec env WINEPREFIX="$PREFIX" \
    WINEDLLPATH="$PROTON/files/lib/vkd3d:$PROTON/files/lib/wine" \
    LD_LIBRARY_PATH="$libs" \
    WINEDLLOVERRIDES="${CUBE_WINEDLLOVERRIDES-windows.ui=n}" \
    XLIB_SKIP_ARGB_VISUALS="${CUBE_SKIP_ARGB_VISUALS:-1}" \
    WINEDEBUG="${CUBE_WINEDEBUG:--all}" \
    "${vk_env[@]}" \
    "$WINE" "$MAIN" "$@"
