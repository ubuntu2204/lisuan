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
# 启动修复（2026-11-18）：默认再传 -noupdate -workdir <安装目录>，跳过
# Wine 下会长时间停在「正在更新」的 update.exe。用户在此阶段重复点击
# 触发单实例冲突（第二个进程 0x80010106）后退出；这是绕过，不是修好
# 厂商更新器。E80_ALLOW_UPDATE=1 可恢复更新检查，显式传参也原样透传。
#
# 用法：
#   ./80/run-80duizhan.sh [参数...]   # 启动平台（参数透传给 80launch.exe）
#   ./80/run-80duizhan.sh --status    # 只读体检
#   ./80/run-80duizhan.sh --help
#
# 环境变量：E80_PROTON / E80_WINE11_PREFIX / E80_INSTALL_DIR / E80_WINEDEBUG /
#   E80_ALLOW_UPDATE=1（恢复默认关闭的更新检查）
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

  （无参数） 前台启动 80 对战平台；默认加 -noupdate -workdir <安装目录>
              跳过 Wine 下会卡住的更新器，参数可显式覆盖
  --status   只读体检：wine、安装前缀、主程序 PE
  --help     显示本说明

环境变量：E80_PROTON、E80_WINE11_PREFIX、E80_INSTALL_DIR、E80_WINEDEBUG、
          E80_ALLOW_UPDATE=1 可恢复更新检查。
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

# 默认跳过厂商更新器：它在 Wine 下会长时间停在「正在更新」，重复点击
# 触发单实例 COM 冲突后闪退。显式传参或 E80_ALLOW_UPDATE=1 可恢复原行为。
if [[ $# -eq 0 && "${E80_ALLOW_UPDATE:-0}" != "1" ]]; then
    rel="${MAIN#"$PREFIX/drive_c/"}"
    dir_rel="${rel%/*}"
    dir_rel="${dir_rel//\\//}"
    set -- -noupdate -workdir "C:/$dir_rel/"
fi

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
