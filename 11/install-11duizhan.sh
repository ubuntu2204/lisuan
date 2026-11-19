#!/usr/bin/env bash
# 至砺算：11 对战平台（5211game.com）官网安装包的静默安装与体检。
#
# 该安装包是 Inno Setup 5.4.2（不是 NSIS），故静默参数是 /VERYSILENT 等，
# 不用 /S。用 Proton 11.0 的 wine-11.0 在独立前缀里安装；只在 wine 进程树
# 上设置 WINEPREFIX / WINEDLLPATH / LD_LIBRARY_PATH / WINEDLLOVERRIDES，
# 不污染全局 shell、不修改 Steam 自身文件、不做崩溃后自动重启/守护。
# 安装包属厂商专有二进制，.gitignore 排除，获取方式与 SHA 见 11/README.md。
#
# 用法：
#   ./11/install-11duizhan.sh [安装器.exe]   # 安装（幂等：已装则只体检）
#   ./11/install-11duizhan.sh --status       # 只读体检
#   ./11/install-11duizhan.sh --help
#
# 环境变量：E11_PROTON / E11_WINE11_PREFIX / E11_INSTALL_DIR /
#   E11_INSTALL_TIMEOUT / E11_INSTALL_LOG / E11_WINEDEBUG
# License: Apache-2.0
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROTON="${E11_PROTON:-$HOME/.local/share/Steam/steamapps/common/Proton 11.0}"
PROTON="${PROTON/#\~/$HOME}"
PREFIX="${E11_WINE11_PREFIX:-$HOME/.deepinwine/11duizhan-263}"
PREFIX="${PREFIX/#\~/$HOME}"
DRIVE_C="$PREFIX/drive_c"
# 安装目录（相对 drive_c）；Inno 的 /DIR 会收到 C:\<此路径>。
INSTALL_DIR_REL="${E11_INSTALL_DIR:-11duizhan}"
INSTALL_DIR_REL="${INSTALL_DIR_REL#/}"
WIN_DIR="C:\\${INSTALL_DIR_REL//\//\\}"
TIMEOUT="${E11_INSTALL_TIMEOUT:-900}"
LOG="${E11_INSTALL_LOG:-/tmp/11duizhan-install-$(date +%Y%m%d-%H%M%S).log}"
INNO_LOG="${LOG%.log}-inno.log"
WIN_INNO_LOG="Z:${INNO_LOG//\//\\}"
INSTALLER_GLOB='5211Setup_*.exe'
# 正式版 2.6.3.x 用 11Loader.exe，3.0 公测版用 11loader.exe；Linux 区分大小写。
MAIN_NAMES=('11Loader.exe' '11loader.exe')
UNINST_NAME='unins000.exe'
WINE="$PROTON/files/bin/wine"
WINESERVER="$PROTON/files/bin/wineserver"

die() { echo "错误：$*" >&2; exit 1; }

usage() {
    cat <<EOF
用法：$(basename "$0") [选项] [安装器.exe]

  （无参数） 安装/体检：找安装包 → 初始化独立前缀 → Inno Setup 静默安装 → 校验主程序
  --status   只读体检：前缀、安装目录、主程序 PE、卸载器
  --help     显示本说明

  安装器查找顺序：命令行参数 → $SCRIPT_DIR → ~/下载 → ~/Downloads。
  环境变量：E11_PROTON、E11_WINE11_PREFIX、E11_INSTALL_DIR、
            E11_INSTALL_TIMEOUT、E11_INSTALL_LOG、E11_WINEDEBUG。
EOF
}

# 返回主程序完整路径；未找到返回非零。
find_main() {
    local d n found
    for d in "$DRIVE_C/$INSTALL_DIR_REL" \
             "$DRIVE_C/Program Files/11duizhan" \
             "$DRIVE_C/Program Files (x86)/11duizhan" \
             "$DRIVE_C/11duizhan"; do
        for n in "${MAIN_NAMES[@]}"; do
            [[ -f "$d/$n" ]] && { printf '%s\n' "$d/$n"; return 0; }
        done
    done
    found="$(find "$DRIVE_C" -maxdepth 4 -type f \
        \( -name "${MAIN_NAMES[0]}" -o -name "${MAIN_NAMES[1]}" \) -print 2>/dev/null | sed -n '1p')"
    [[ -n "$found" ]] || return 1
    printf '%s\n' "$found"
}

wine_env() {
    local libs="$PROTON/files/lib/x86_64-linux-gnu:$PROTON/files/lib/i386-linux-gnu"
    [[ -n "${LD_LIBRARY_PATH:-}" ]] && libs="$libs:$LD_LIBRARY_PATH"
    env WINEPREFIX="$PREFIX" \
        WINEDLLPATH="$PROTON/files/lib/vkd3d:$PROTON/files/lib/wine" \
        LD_LIBRARY_PATH="$libs" \
        WINEDLLOVERRIDES='mscoree,mshtml=' \
        WINEDEBUG="${E11_WINEDEBUG:--all}" \
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
    local rc=0 main
    if [[ -d "$PREFIX/drive_c/windows" ]]; then
        echo "  [OK] Wine 前缀：$PREFIX"
    else
        echo "  [--] Wine 前缀不存在：$PREFIX"; rc=1
    fi
    if main="$(find_main)"; then
        echo "  [OK] 安装目录：$(dirname "$main")（$(du -sh "$(dirname "$main")" | cut -f1)）"
        if is_pe "$main"; then
            echo "  [OK] 主程序：$main（$(stat -c%s "$main") 字节）"
        else
            echo "  [--] 主程序不是有效 PE：$main"; rc=1
        fi
        if is_pe "$(dirname "$main")/$UNINST_NAME"; then
            echo "  [OK] 卸载器：$(dirname "$main")/$UNINST_NAME"
        else
            echo "  [--] 未见卸载器 $UNINST_NAME"; rc=1
        fi
    else
        echo "  [--] 未找到主程序 ${MAIN_NAMES[*]}（安装目录？）"; rc=1
    fi
    return "$rc"
}

install() {
    local main
    if main="$(find_main)" && is_pe "$main"; then
        echo "  已安装，无需重复运行（Inno Setup 静默安装不会重复下载整套平台文件）。"
        status
        return 0
    fi
    [[ -x "$WINE" ]] || die "找不到 Proton 11 的 wine：$WINE"
    local installer
    installer="$(find_installer "${INSTALLER_ARG:-}")" || \
        die "未找到安装包 $INSTALLER_GLOB（放在 $SCRIPT_DIR、~/下载 或 ~/Downloads，或用命令行参数指定）"
    is_pe "$installer" || die "安装包不是有效 PE：$installer"
    mkdir -p "$PREFIX" "$(dirname "$LOG")" "$(dirname "$INNO_LOG")"
    : >> "$LOG" || die "无法写入安装日志：$LOG"
    echo "    安装包：$installer"
    echo "    前缀  ：$PREFIX"
    echo "    安装到：$WIN_DIR"
    echo "    日志  ：$LOG"
    echo "    Inno 日志：$INNO_LOG"

    local rc=0
    if wine_env timeout "$TIMEOUT" "$WINE" "$installer" \
        /VERYSILENT /SUPPRESSMSGBOXES /NORESTART \
        "/DIR=$WIN_DIR" "/LOG=$WIN_INNO_LOG" >"$LOG" 2>&1; then
        rc=0
    else
        rc=$?
    fi
    echo "    安装器返回码：$rc"
    case "$rc" in
        0) ;;
        124) die "安装超时（${TIMEOUT}s）；可提高 E11_INSTALL_TIMEOUT，详见 $LOG" ;;
        *)   die "安装器失败（返回码 $rc）；详见 $LOG 与 $INNO_LOG" ;;
    esac
    wine_env timeout 60 "$WINESERVER" -w >/dev/null 2>&1 || true

    main="$(find_main)" || die "返回码 0 但未找到 ${MAIN_NAMES[*]}（详见 $LOG 与 $INNO_LOG）"
    is_pe "$main" || die "主程序不是有效 PE：$main（详见 $LOG）"
    echo "    安装完成：$main（$(stat -c%s "$main") 字节）"
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
