#!/usr/bin/env bash
# 至砺算：KK 官方对战平台（魔兽争霸 III）的安装链路与显卡无关。本脚本用
# Proton 11.0 的 wine-11.0 在独立前缀里跑官方 NSIS 静默安装器；不注入全局
# gf 环境、不改系统文件、不做崩溃后自动重启/守护。
#
# 官方安装器属厂商专有二进制，.gitignore 排除；获取方式与 SHA 见 README.md。
# 安装器自带的 Windows 快捷方式由 Wine 自己映射到
# ~/.local/share/applications/wine/Programs/KK官方对战平台，本脚本不动它。
#
# 用法：
#   ./war3/install-kk-platform.sh [安装器.exe]   # 安装（幂等：已装则只做体检）
#   ./war3/install-kk-platform.sh --status       # 只读体检
#   ./war3/install-kk-platform.sh --help
#
# 环境变量：KK_PROTON / KK_WINE11_PREFIX / KK_INSTALL_TIMEOUT /
#   KK_INSTALL_LOG / KK_WINEDEBUG
# License: Apache-2.0
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROTON="${KK_PROTON:-$HOME/.local/share/Steam/steamapps/common/Proton 11.0}"
PROTON="${PROTON/#\~/$HOME}"
PREFIX="${KK_WINE11_PREFIX:-$HOME/.deepinwine/kkduizhan-wine11}"
PREFIX="${PREFIX/#\~/$HOME}"
INSTALL_DIR="$PREFIX/drive_c/Program Files (x86)/kkduizhan"
MAIN_EXE="$INSTALL_DIR/Platform.exe"
UNINSTALL_EXE="$INSTALL_DIR/Uninstall.exe"
WINE="$PROTON/files/bin/wine"
WINESERVER="$PROTON/files/bin/wineserver"
TIMEOUT="${KK_INSTALL_TIMEOUT:-600}"
LOG="${KK_INSTALL_LOG:-/tmp/kk-platform-install-$(date +%Y%m%d-%H%M%S).log}"
INSTALLER_GLOB='4_pf_files_KKDuiZhanDownloader-*.exe'

die() { echo "错误：$*" >&2; exit 1; }

usage() {
    cat <<EOF
用法：$(basename "$0") [选项] [安装器.exe]

  （无参数）   安装/体检：自动找安装器 → 初始化独立前缀 → NSIS /S 静默安装 → 校验主程序
  --status     只读体检：前缀、安装目录、主程序 PE、卸载器、Wine 菜单项
  --help       显示本说明

  安装器查找顺序：命令行参数 → $SCRIPT_DIR → ~/下载 → ~/Downloads。
  环境变量：KK_PROTON、KK_WINE11_PREFIX、KK_INSTALL_TIMEOUT、
            KK_INSTALL_LOG、KK_WINEDEBUG。
  卸载：安装目录内的 Uninstall.exe /S（安装器自带，本脚本不代跑）。
EOF
}

# 只在 wine 进程树上设置 Proton 运行库环境，不污染当前 shell / Steam 客户端。
wine_env() {
    local libs="$PROTON/files/lib/x86_64-linux-gnu:$PROTON/files/lib/i386-linux-gnu"
    [[ -n "${LD_LIBRARY_PATH:-}" ]] && libs="$libs:$LD_LIBRARY_PATH"
    env WINEPREFIX="$PREFIX" \
        WINEDLLPATH="$PROTON/files/lib/vkd3d:$PROTON/files/lib/wine" \
        LD_LIBRARY_PATH="$libs" \
        WINEDLLOVERRIDES='mscoree,mshtml=' \
        WINEDEBUG="${KK_WINEDEBUG:--all}" \
        "$@"
}

is_pe() { [[ -s "$1" && "$(head -c2 "$1")" == "MZ" ]]; }

find_installer() {
    local arg="${1:-}" f
    if [[ -n "$arg" ]]; then
        [[ -f "$arg" ]] || die "安装器不存在：$arg"
        printf '%s\n' "$(realpath "$arg")"
        return 0
    fi
    for d in "$SCRIPT_DIR" "$HOME/下载" "$HOME/Downloads"; do
        [[ -d "$d" ]] || continue
        f="$(find "$d" -maxdepth 1 -type f -name "$INSTALLER_GLOB" \
            -printf '%f\n' 2>/dev/null | sort | sed -n '1p')"
        if [[ -n "$f" ]]; then
            printf '%s\n' "$d/$f"
            return 0
        fi
    done
    return 1
}

status() {
    local rc=0
    if [[ -d "$PREFIX/drive_c/windows" ]]; then
        echo "  [OK] Wine 前缀：$PREFIX"
    else
        echo "  [--] Wine 前缀不存在：$PREFIX"; rc=1
    fi
    if [[ -d "$INSTALL_DIR" ]]; then
        echo "  [OK] 安装目录：$INSTALL_DIR（$(du -sh "$INSTALL_DIR" | cut -f1)）"
    else
        echo "  [--] 安装目录不存在：$INSTALL_DIR"; rc=1
    fi
    if is_pe "$MAIN_EXE"; then
        echo "  [OK] 主程序：$MAIN_EXE（$(stat -c%s "$MAIN_EXE") 字节）"
    else
        echo "  [--] 主程序缺失或不是有效 PE：$MAIN_EXE"; rc=1
    fi
    if is_pe "$UNINSTALL_EXE"; then
        echo "  [OK] 卸载器：$UNINSTALL_EXE"
    else
        echo "  [--] 卸载器缺失：$UNINSTALL_EXE"; rc=1
    fi
    if [[ -d "$HOME/.local/share/applications/wine/Programs/KK官方对战平台" ]]; then
        echo "  [OK] Wine 菜单项：KK官方对战平台"
    else
        echo "  [ii] 未见 Wine 菜单项（不影响安装结果）"
    fi
    return "$rc"
}

install() {
    if is_pe "$MAIN_EXE"; then
        echo "  已安装，无需重复运行（NSIS /S 不会重复下载整套平台文件）。"
        status
        return 0
    fi
    [[ -x "$WINE" ]] || die "找不到 Proton 11 的 wine：$WINE"
    local installer
    installer="$(find_installer "${INSTALLER_ARG:-}")" || \
        die "未找到官方安装器 $INSTALLER_GLOB（放在 $SCRIPT_DIR、~/下载 或 ~/Downloads，或用命令行参数指定）"
    is_pe "$installer" || die "安装器不是有效 PE：$installer"
    mkdir -p "$PREFIX" "$(dirname "$LOG")"
    : >> "$LOG" || die "无法写入安装日志：$LOG"
    echo "    安装器：$installer"
    echo "    前缀  ：$PREFIX"
    echo "    日志  ：$LOG"

    local rc=0
    if wine_env timeout "$TIMEOUT" "$WINE" "$installer" /S >"$LOG" 2>&1; then
        rc=0
    else
        rc=$?
    fi
    echo "    安装器返回码：$rc"
    case "$rc" in
        0) ;;
        2)   die "安装器 Abort（文件占用/权限）；详见 $LOG" ;;
        124) die "安装超时（${TIMEOUT}s）；可提高 KK_INSTALL_TIMEOUT，详见 $LOG" ;;
        *)   die "安装器失败（返回码 $rc）；详见 $LOG" ;;
    esac
    wine_env timeout 60 "$WINESERVER" -w >/dev/null 2>&1 || true

    is_pe "$MAIN_EXE" || die "返回码 0 但主程序缺失或不是有效 PE：$MAIN_EXE（详见 $LOG）"
    is_pe "$UNINSTALL_EXE" || die "缺少 Uninstall.exe，安装不完整：$INSTALL_DIR"
    echo "    安装完成：$MAIN_EXE（$(stat -c%s "$MAIN_EXE") 字节）"
    status
    echo "    注意：本次只验收安装产物；平台 GUI 启动/登录未验收，见 README.md。"
}

INSTALLER_ARG=""
case "${1:-}" in
    --help|-h) usage; exit 0 ;;
    --status) status; exit $? ;;
    --*) die "未知参数：$1（用 --help 查看用法）" ;;
    *) INSTALLER_ARG="${1:-}" ;;
esac
install
