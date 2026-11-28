#!/usr/bin/env bash
# 至砺算：《战舰世界》7G100 硬件加速单次启动器。
# 默认直接经 Steam Linux Runtime + Proton 11 启动；--steam 交给已配置的 Steam 客户端。
# 不做存活监控、自动重启、看门狗或守护；退出码原样透传。
#
# 用法：./run-wows-gf.sh [--steam] [--status] [游戏参数...]
# 环境变量：WOWS_STEAM_ROOT / WOWS_GAME_DIR / WOWS_GAME_BIN / WOWS_PROTON /
#           WOWS_GF_RUNTIME / WOWS_DXVK_HUD / WOWS_DIAG / WOWS_PROTON_LOG
# License: Apache-2.0
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APPID=552990
STEAM_ROOT="${WOWS_STEAM_ROOT:-$HOME/.local/share/Steam}"
STEAM_ROOT="${STEAM_ROOT/#\~/$HOME}"
GAME_DIR="${WOWS_GAME_DIR:-$STEAM_ROOT/steamapps/common/World of Warships}"
GAME_DIR="${GAME_DIR/#\~/$HOME}"
PROTON="${WOWS_PROTON:-$STEAM_ROOT/steamapps/common/Proton 11.0}"
PROTON="${PROTON/#\~/$HOME}"
RUNTIME="${WOWS_GF_RUNTIME:-$HOME/.local/share/worldofwarships-gf-accel}"
RUNTIME="${RUNTIME/#\~/$HOME}"
PREFIX="$STEAM_ROOT/steamapps/compatdata/$APPID"
RT="$STEAM_ROOT/steamapps/common/SteamLinuxRuntime_4"

MODE=direct
STATUS_ONLY=0

die() { echo "错误：$*" >&2; exit 1; }

usage() {
    cat <<EOF
用法：$(basename "$0") [选项] [游戏参数...]
  --steam     通过 Steam 客户端启动，需先在游戏属性中配置 LaunchOptions
  --status    只读体检
  --help      显示本说明

默认：Steam Linux Runtime + Proton 11 + gf 兼容垫片 + 游戏目录内 DXVK 2.3
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --steam) MODE=steam; shift ;;
        --status) STATUS_ONLY=1; shift ;;
        --help|-h) usage; exit 0 ;;
        --) shift; break ;;
        *) break ;;
    esac
done

find_game_bin() {
    if [[ -n "${WOWS_GAME_BIN:-}" ]]; then
        echo "${WOWS_GAME_BIN/#\~/$HOME}"
        return
    fi
    while IFS= read -r exe; do dirname "$exe"; done < <(
        find "$GAME_DIR/bin" -type f -path '*/bin64/WorldOfWarships64.exe' -print 2>/dev/null | sort -V
    ) | tail -1
}

GAME_BIN="$(find_game_bin)"
GAME_MAIN="$GAME_BIN/WorldOfWarships64.exe"

status() {
    local rc=0 item
    for item in "$GAME_MAIN" "$PROTON/proton" "$RT/_v2-entry-point" \
                "$RUNTIME/gf_icd.json" "$RUNTIME/lib/osrelease_redirect.so" \
                "$RUNTIME/lib/libvulkan_gf_shim.so" "$RUNTIME/lib/libSC.so" \
                "$GAME_BIN/d3d11.dll" "$GAME_BIN/dxgi.dll"; do
        if [[ -s "$item" ]]; then echo "  [OK] $item"; else echo "  [--] 缺失：$item"; rc=1; fi
    done
    exit "$rc"
}

if [[ "$STATUS_ONLY" == 1 ]]; then status; fi

[[ -s "$GAME_MAIN" ]] || die "找不到游戏主程序：$GAME_MAIN"
[[ -x "$PROTON/proton" ]] || die "找不到 Proton 11：$PROTON"
[[ -x "$RT/_v2-entry-point" ]] || die "找不到 Steam Linux Runtime：$RT"
[[ -s "$RUNTIME/gf_icd.json" ]] || die "gf 运行时未部署，请先运行 setup-wows-gf.sh"
[[ -s "$RUNTIME/lib/libvulkan_gf_shim.so" ]] || die "gf shim 未部署"
[[ -s "$RUNTIME/lib/libSC.so" ]] || die "libSC.so 未部署"
[[ -s "$GAME_BIN/d3d11.dll" && -s "$GAME_BIN/dxgi.dll" ]] || die "游戏目录内 DXVK 2.3 未部署"

if [[ "$MODE" == steam ]]; then
    [[ -x /usr/bin/steam ]] || die "找不到 Steam 客户端"
    echo "[wows-gf] 交给 Steam 客户端启动 AppID $APPID"
    exec /usr/bin/steam -applaunch "$APPID"
fi

export SteamAppId="$APPID" SteamGameId="$APPID"
export STEAM_COMPAT_CLIENT_INSTALL_PATH="$STEAM_ROOT"
export STEAM_COMPAT_DATA_PATH="$PREFIX"
export STEAM_COMPAT_INSTALL_PATH="$GAME_DIR"
export STEAM_COMPAT_LIBRARY_PATHS="$STEAM_ROOT/steamapps"
export STEAM_COMPAT_PROTON=1
export LD_PRELOAD="$RUNTIME/lib/osrelease_redirect.so"
export LD_LIBRARY_PATH="$RUNTIME/lib"
export __GLX_VENDOR_LIBRARY_NAME=mesa
export LIBGL_ALWAYS_SOFTWARE=1
export VK_ICD_FILENAMES="$RUNTIME/gf_icd.json"
export VK_DRIVER_FILES="$RUNTIME/gf_icd.json"
export VK_LOADER_LAYERS_DISABLE=VK_LAYER_VALVE_steam_overlay_64,VK_LAYER_VALVE_steam_overlay_32
export DXVK_HUD="${WOWS_DXVK_HUD:-fps}"

if [[ "${WOWS_DIAG:-0}" == 1 ]]; then
    export PROTON_LOG=1 DXVK_LOG_LEVEL=info DXVK_LOG_PATH="$RUNTIME"
else
    export PROTON_LOG="${WOWS_PROTON_LOG:-0}" DXVK_LOG_LEVEL=none
fi

echo "[wows-gf] 启动：$GAME_MAIN"
exec "$RT/_v2-entry-point" --verb=waitforexitandrun -- \
    "$PROTON/proton" waitforexitandrun "$GAME_DIR/WorldOfWarships.exe" "$@"
