#!/usr/bin/env bash
# 至砺算：方块游戏平台 CEF 系统字体修复（与显卡无关）。
#
# 根因（2026-11-25，方块平台 4.1.2.1 / CEF 43.3 / Proton 11 wine-11.0）：
# Wine 前缀的 NONCLIENTMETRICS 五类系统字体是 Tahoma，方块平台自带的旧版
# CEF 在 AddFont() 期间创建 SkTypeface 失败，回退调用 GetDefaultSystemFont()
# 时 is_initialized_ 尚未置位，导致无限重入：浏览器主线程不断取 DC/建字体，
# 耗尽 Wine 的 GDI 句柄表（"out of GDI object handles"），窗口建出但永不显示。
# 与 11 对战平台登录后卡死是同一条链，复用其已验证的注册表修复：
# 用 SPI_SETNONCLIENTMETRICS 把五类系统字体改为本机存在的 Arial。
#
# 本脚本是 11/fix-11-fonts.sh 的薄封装，只固定前缀与字体，
# 真正的备份 / 幂等 / 还原逻辑都在共享脚本里：
#   ~/.local/share/cube-4121/font-fix/WindowMetrics.reg.bak
# 不做看门狗、自动重启或守护。
#
# 用法：
#   ./cube/fix-cube-fonts.sh              # 备份注册表并设为 Arial（幂等）
#   ./cube/fix-cube-fonts.sh --status     # 只读体检
#   ./cube/fix-cube-fonts.sh --uninstall  # 从备份还原注册表
#   ./cube/fix-cube-fonts.sh --help
#
# 环境变量：CUBE_PREFIX / CUBE_SYSTEM_FONT（默认 Arial）
# License: Apache-2.0
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$HERE/.." && pwd)"
PREFIX="${CUBE_PREFIX:-$HOME/.deepinwine/cube-4121}"
PREFIX="${PREFIX/#\~/$HOME}"
FONT="${CUBE_SYSTEM_FONT:-Arial}"
SHARED="$REPO/11/fix-11-fonts.sh"

usage() {
    cat <<EOF
用法：$(basename "$0") [--status|--uninstall|--help]

  （无参数） 备份并把五类系统字体设为 $FONT（幂等）
  --status    只读体检：当前五类系统字体是否均为目标字体
  --uninstall 从备份还原 WindowMetrics 注册表
  --help      显示本说明

前缀：$PREFIX
字体：$FONT
环境变量：CUBE_PREFIX / CUBE_SYSTEM_FONT
说明：本修复只解决 CEF 字体初始化重入；其它异常按崩溃处理约定取证。
EOF
}

case "${1:-}" in
    --help|-h) usage; exit 0 ;;
    --status|--uninstall|"") ;;
    *) echo "未知参数：$1（用 --help 查看用法）" >&2; usage; exit 1 ;;
esac

[[ -f "$SHARED" ]] || { echo "错误：缺少共享字体修复脚本：$SHARED" >&2; exit 1; }

exec env E11_WINE11_PREFIX="$PREFIX" E11_SYSTEM_FONT="$FONT" \
    timeout 240 bash "$SHARED" "$@"
