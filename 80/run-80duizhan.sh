#!/usr/bin/env bash
# 至砺算：80 对战平台单应用启动器，供桌面入口调用。
#
# 只在 wine 进程树上设置 Proton 11 的运行库环境与独立前缀，
# 不污染全局 shell、不修改 Steam 自身文件。单次前台启动，
# 不做存活监控、自动重启或守护；退出码原样透传。
#
# 启动修复（2026-11-17）：32 位 80launch.exe 不能吃继承来的 64 位
# VK_ICD_FILENAMES（gf ICD），且 winex11 探测 OpenGL 时会因 4c54:5006
# 找不到 DRI 驱动而卡在未映射窗口。这里只在此进程树移除该变量并强制
# Mesa 软件 GLX：env -u VK_ICD_FILENAMES + LIBGL_ALWAYS_SOFTWARE=1
# + __GLX_VENDOR_LIBRARY_NAME=mesa；A/B 实测三项同时生效才稳定出窗口。
#
# 用法：
#   ./80/run-80duizhan.sh [参数...]   # 启动平台（参数透传给 80launch.exe）
#   ./80/run-80duizhan.sh --status    # 只读体检
#   ./80/run-80duizhan.sh --help
#
# 环境变量：E80_PROTON / E80_WINE11_PREFIX / E80_INSTALL_DIR / E80_WINEDEBUG
# License: Apache-2.0
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROTON="${E80_PROTON:-$HOME/.local/share/Steam/steamapps/common/Proton 11.0}"
PROTON="${PROTON/#\~/$HOME}"
PREFIX="${E80_WINE11_PREFIX:-$HOME/.deepinwine/80duizhan-wine11}"
PREFIX="${PREFIX/#\~/$HOME}"
INSTALL_DIR_REL="${E80_INSTALL_DIR:-Program Files (x86)/80duizhan}"
INSTALL_DIR_REL="${INSTALL_DIR_REL#/}"
MAIN="$PREFIX/drive_c/$INSTALL_DIR_REL/80launch.exe"
WINE="$PROTON/files/bin/wine"

die() { echo "错误：$*" >&2; exit 1; }

find_main() {
    local d
    [[ -s "$MAIN" ]] && { printf '%s\n' "$MAIN"; return 0; }
    for d in "$PREFIX/drive_c/Program Files (x86)" \
             "$PREFIX/drive_c/Program Files" \
             "$PREFIX/drive_c/80duizhan"; do
        [[ -d "$d" ]] || continue
        MAIN="$(find "$d" -maxdepth 3 -type f -name 80launch.exe \
            -print -quit 2>/dev/null || true)"
        [[ -s "$MAIN" ]] && { printf '%s\n' "$MAIN"; return 0; }
    done
    return 1
}

usage() {
    cat <<EOF
用法：$(basename "$0") [选项] [参数...]

  （无参数） 前台启动 80 对战平台（参数透传给 80launch.exe）
  --status   只读体检：wine、安装前缀、主程序 PE
  --help     显示本说明

环境变量：E80_PROTON、E80_WINE11_PREFIX、E80_INSTALL_DIR、E80_WINEDEBUG。
崩溃后不会自动重启；请按 .codebuddy/rules/崩溃处理约定.md 取证定位。
EOF
}

case "${1:-}" in
    --help|-h) usage; exit 0 ;;
    --status)
        [[ -x "$WINE" ]] && echo "  [OK] wine：$WINE" || { echo "  [--] wine 缺失：$WINE"; exit 1; }
        MAIN="$(find_main || true)"
        [[ -s "$MAIN" && "$(head -c2 "$MAIN")" == "MZ" ]] && \
            echo "  [OK] 主程序：$MAIN" || { echo "  [--] 主程序缺失或不是 PE：$MAIN"; exit 1; }
        exit 0
        ;;
esac

[[ -x "$WINE" ]] || die "找不到 Proton 11 的 wine：$WINE"
MAIN="$(find_main || true)"
[[ -s "$MAIN" && "$(head -c2 "$MAIN")" == "MZ" ]] || die "主程序缺失或不是有效 PE：$MAIN"

libs="$PROTON/files/lib/x86_64-linux-gnu:$PROTON/files/lib/i386-linux-gnu"
[[ -n "${LD_LIBRARY_PATH:-}" ]] && libs="$libs:$LD_LIBRARY_PATH"
cd "$(dirname "$MAIN")"
exec env -u VK_ICD_FILENAMES \
    WINEPREFIX="$PREFIX" \
    WINEDLLPATH="$PROTON/files/lib/vkd3d:$PROTON/files/lib/wine" \
    LD_LIBRARY_PATH="$libs" \
    WINEDLLOVERRIDES='mscoree,mshtml=' \
    LIBGL_ALWAYS_SOFTWARE=1 \
    __GLX_VENDOR_LIBRARY_NAME=mesa \
    WINEDEBUG="${E80_WINEDEBUG:--all}" \
    "$WINE" "$MAIN" "$@"
