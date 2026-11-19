#!/usr/bin/env bash
# 至砺算：11 对战平台登录后卡死的系统字体修复（与显卡无关）。
#
# 根因（2026-11-19，CEF 105.3.38 + Proton 11 wine-11.0）：
# Wine 前缀的 NONCLIENTMETRICS 系统字体是 Tahoma，本机没有该字体；CEF 的
# gfx::win::SystemFonts::Initialize() 在 AddFont() 期间用这个字体名创建
# SkTypeface 失败，回退调用 GetDefaultSystemFont()，但 is_initialized_ 要到
# AddFont() 全部完成后才置 true，于是再次 Initialize()，形成无限重入：
#   GetDefaultSystemFont → PlatformFontSkia::CreateFromNameAndSize →
#   InitFromDetails → EnsuresDefaultFontIsInitialized → GetDefaultSystemFont …
# 每轮取显示 DC / 创建字体，先耗尽 Wine GDI 句柄表，再触发 CEF 的
# base::debug::CollectGDIUsageAndDie 断言（int3）或栈溢出。
#
# 修复：把前缀内五类系统字体（caption / smcaption / menu / message / status）
# 通过 SPI_SETNONCLIENTMETRICS 改为本机存在的字体（默认 Arial）。只改当前
# WINEPREFIX 的 HKCU\Control Panel\Desktop\WindowMetrics，改前备份，可还原。
# 不做存活监控、自动重启或守护。
#
# 用法：
#   ./11/fix-11-fonts.sh              # 幂等应用（默认 Arial）
#   ./11/fix-11-fonts.sh --status     # 只读体检
#   ./11/fix-11-fonts.sh --uninstall  # 从 .reg 备份还原注册表
#   ./11/fix-11-fonts.sh --help
#
# 环境变量：E11_WINE11_PREFIX / E11_PROTON / E11_SYSTEM_FONT /
#   E11_FONT_FIX_DATA
# License: Apache-2.0
set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PREFIX="${E11_WINE11_PREFIX:-$HOME/.deepinwine/11duizhan-263}"
PREFIX="${PREFIX/#\~/$HOME}"
PROTON="${E11_PROTON:-$HOME/.local/share/Steam/steamapps/common/Proton 11.0}"
PROTON="${PROTON/#\~/$HOME}"
WINE="$PROTON/files/bin/wine"
FONT="${E11_SYSTEM_FONT:-Arial}"
DATA="${E11_FONT_FIX_DATA:-$HOME/.local/share/$(basename "$PREFIX")/font-fix}"
HELPER_SRC="$REPO_DIR/system-font-helper.c"
HELPER="$DATA/system-font-helper.exe"
BACKUP="$DATA/WindowMetrics.reg.bak"
KEY='HKCU\Control Panel\Desktop\WindowMetrics'

die() { echo "错误：$*" >&2; exit 1; }

usage() {
    cat <<EOF
用法：$(basename "$0") [--status|--uninstall|--help]

  （无参数） 备份注册表并把五类系统字体设为 $FONT（幂等）
  --status    只读体检：当前五类系统字体是否均为目标字体
  --uninstall 从备份还原 WindowMetrics 注册表
  --help      显示本说明

环境变量：E11_WINE11_PREFIX / E11_PROTON / E11_SYSTEM_FONT（默认 Arial） /
          E11_FONT_FIX_DATA
前缀：$PREFIX
字体：$FONT
备份：$BACKUP
说明：只解决 CEF 字体初始化重入；其它异常按崩溃处理约定取证。
EOF
}

run_wine() {
    timeout 40 env WINEPREFIX="$PREFIX" WINEDLLOVERRIDES='mscoree,mshtml=' \
        WINEDEBUG=-all "$WINE" "$@"
}

build_helper() {
    command -v i686-w64-mingw32-gcc >/dev/null || die "缺 i686-w64-mingw32-gcc"
    [[ -f "$HELPER_SRC" ]] || die "缺源码：$HELPER_SRC"
    mkdir -p "$DATA"
    i686-w64-mingw32-gcc -O2 -o "$HELPER" "$HELPER_SRC" \
        -static-libgcc -lgdi32 -luser32
}

status() {
    local rc=0 out matched
    [[ -d "$PREFIX" ]] || { echo "  [--] 前缀不存在：$PREFIX"; return 1; }
    [[ -x "$WINE" ]] || { echo "  [--] wine 缺失：$WINE"; return 1; }
    if [[ ! -x "$HELPER" ]]; then
        echo "  [--] 未构建字体探针：$HELPER（先执行一次修复）"
        return 1
    fi
    out="$(run_wine "$HELPER" check 2>&1)" || rc=1
    if [[ $rc -ne 0 ]]; then
        printf '%s\n' "$out"
        return 1
    fi
    matched="$(printf '%s\n' "$out" | grep -Fc "=$FONT" || true)"
    printf '%s\n' "$out" | sed 's/^/  /'
    if [[ "$matched" == "5" ]]; then
        echo "  [OK] 五类系统字体均为 $FONT"
        return 0
    fi
    echo "  [--] 目标字体匹配 $matched/5（期望 5）"
    return 1
}

apply() {
    [[ -d "$PREFIX" ]] || die "前缀不存在：$PREFIX"
    [[ -x "$WINE" ]] || die "找不到 wine：$WINE"
    if command -v fc-match >/dev/null; then
        echo "  fc-match $FONT -> $(fc-match "$FONT" | head -1)"
    fi
    mkdir -p "$DATA"
    if [[ ! -s "$BACKUP" ]]; then
        run_wine reg export "$KEY" "$BACKUP"
        echo "  已备份：$BACKUP"
    else
        echo "  [=] 已有备份，保留：$BACKUP"
    fi
    build_helper
    run_wine "$HELPER" set "$FONT"
    status
    echo "  完成。建议重新登录验证：GDI / 栈溢出 / 0x80000003 均应为 0。"
}

uninstall() {
    [[ -s "$BACKUP" ]] || die "无备份，拒绝还原：$BACKUP"
    run_wine reg import "$BACKUP"
    echo "  已从备份还原：$KEY"
    status || true
}

case "${1:-}" in
    --help|-h) usage; exit 0 ;;
    --status) status; exit $? ;;
    --uninstall) uninstall; exit 0 ;;
    --*) die "未知参数：$1（用 --help 查看用法）" ;;
    *) apply ;;
esac
