#!/usr/bin/env bash
# 至砺算：11 对战平台单应用启动器，供桌面入口调用。
#
# 只在 wine 进程树上设置 Proton 11 的运行库环境与独立前缀，
# 不污染全局 shell、不修改 Steam 自身文件。单次前台启动，
# 不做存活监控、自动重启或守护；退出码原样透传。
#
# 用法：
#   ./11/run-11duizhan.sh [参数...]   # 启动平台（参数透传给 11loader.exe）
#   ./11/run-11duizhan.sh --status    # 只读体检
#   ./11/run-11duizhan.sh --help
#
# 环境变量：E11_PROTON / E11_WINE11_PREFIX / E11_INSTALL_DIR / E11_WINEDEBUG
# License: Apache-2.0
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROTON="${E11_PROTON:-$HOME/.local/share/Steam/steamapps/common/Proton 11.0}"
PROTON="${PROTON/#\~/$HOME}"
PREFIX="${E11_WINE11_PREFIX:-$HOME/.deepinwine/11duizhan-wine11}"
PREFIX="${PREFIX/#\~/$HOME}"
INSTALL_DIR_REL="${E11_INSTALL_DIR:-11duizhan}"
INSTALL_DIR_REL="${INSTALL_DIR_REL#/}"
MAIN="$PREFIX/drive_c/$INSTALL_DIR_REL/11loader.exe"
WINE="$PROTON/files/bin/wine"

die() { echo "错误：$*" >&2; exit 1; }

usage() {
    cat <<EOF
用法：$(basename "$0") [选项] [参数...]

  （无参数） 前台启动 11 对战平台（参数透传给 11loader.exe）
  --status   只读体检：wine、安装前缀、主程序 PE
  --help     显示本说明

环境变量：E11_PROTON、E11_WINE11_PREFIX、E11_INSTALL_DIR、E11_WINEDEBUG。
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

libs="$PROTON/files/lib/x86_64-linux-gnu:$PROTON/files/lib/i386-linux-gnu"
[[ -n "${LD_LIBRARY_PATH:-}" ]] && libs="$libs:$LD_LIBRARY_PATH"
cd "$(dirname "$MAIN")"
exec env WINEPREFIX="$PREFIX" \
    WINEDLLPATH="$PROTON/files/lib/vkd3d:$PROTON/files/lib/wine" \
    LD_LIBRARY_PATH="$libs" \
    WINEDLLOVERRIDES='mscoree,mshtml=' \
    WINEDEBUG="${E11_WINEDEBUG:--all}" \
    "$WINE" "$MAIN" "$@"
