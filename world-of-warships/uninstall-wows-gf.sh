#!/usr/bin/env bash
# 至砺算：回滚《战舰世界》7G100 硬件加速部署。
# 只移动本脚本部署的 DXVK DLL、恢复 shim 备份；不删除游戏、存档或运行时目录。
#
# 用法：./uninstall-wows-gf.sh [--restore-steam-config]
# 环境变量：WOWS_STEAM_ROOT / WOWS_GAME_DIR / WOWS_GAME_BIN / WOWS_GF_RUNTIME
# License: Apache-2.0
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APPID=552990
STEAM_ROOT="${WOWS_STEAM_ROOT:-$HOME/.local/share/Steam}"
STEAM_ROOT="${STEAM_ROOT/#\~/$HOME}"
GAME_DIR="${WOWS_GAME_DIR:-$STEAM_ROOT/steamapps/common/World of Warships}"
GAME_DIR="${GAME_DIR/#\~/$HOME}"
RUNTIME="${WOWS_GF_RUNTIME:-$HOME/.local/share/worldofwarships-gf-accel}"
RUNTIME="${RUNTIME/#\~/$HOME}"
RESTORE_STEAM=0

die() { echo "错误：$*" >&2; exit 1; }
say() { echo "[wows-gf] $*"; }

while [[ $# -gt 0 ]]; do
    case "$1" in
        --restore-steam-config) RESTORE_STEAM=1 ;;
        --help|-h)
            echo "用法：$(basename "$0") [--restore-steam-config]"
            echo "  --restore-steam-config  在 Steam 完全停止后恢复 localconfig.vdf.bak-wows-gf"
            exit 0 ;;
        *) die "未知参数：$1" ;;
    esac
    shift
done

if pgrep -f 'WorldOfWarships64.exe .*apptype=high' >/dev/null; then
    die "游戏仍在运行，请先正常退出"
fi

if [[ -n "${WOWS_GAME_BIN:-}" ]]; then
    GAME_BIN="${WOWS_GAME_BIN/#\~/$HOME}"
else
    GAME_BIN="$(
        while IFS= read -r exe; do dirname "$exe"; done < <(
            find "$GAME_DIR/bin" -type f -path '*/bin64/WorldOfWarships64.exe' -print 2>/dev/null | sort -V
        ) | tail -1
    )"
fi
[[ -n "$GAME_BIN" ]] || die "找不到游戏主程序目录"

shim="$RUNTIME/lib/libvulkan_gf_shim.so"
backup="$RUNTIME/lib/libvulkan_gf_shim.so.bak-wows-setup"
if [[ -f "$backup" ]]; then
    cp -p "$backup" "$shim"
    say "已恢复 shim：$backup -> $shim"
else
    say "未找到 shim 备份，保留当前运行时"
fi

disabled="$GAME_BIN/.dxvk23-disabled"
for dll in d3d11 dxgi; do
    src="$GAME_BIN/$dll.dll"
    if [[ -f "$src" ]]; then
        mkdir -p "$disabled"
        [[ ! -e "$disabled/$dll.dll" ]] || die "回滚目标已存在：$disabled/$dll.dll"
        mv "$src" "$disabled/$dll.dll"
        say "已停用游戏目录内 DXVK：$src -> $disabled/$dll.dll"
    fi
done

if [[ "$RESTORE_STEAM" == 1 ]]; then
    if pgrep -f 'steamrt64/steam .*srt-logger-opened' >/dev/null; then
        die "Steam 客户端正在运行；请先完全退出，再恢复配置"
    fi
    lc="$(find "$STEAM_ROOT/userdata" -maxdepth 3 -path '*/config/localconfig.vdf' -print | head -1)"
    [[ -s "$lc" && -s "$lc.bak-wows-gf" ]] || die "找不到 localconfig.vdf 或备份"
    cp -p "$lc.bak-wows-gf" "$lc"
    say "已恢复 Steam 配置：$lc.bak-wows-gf -> $lc"
fi

say "回滚完成；游戏、存档和运行时目录均未删除"
