#!/usr/bin/env bash
# 至砺算：星际争霸 1.08（32 位 PE）经 Mesa zink + Venus 转发使用 7G100
# 硬件渲染的独立启动器。软件方案 run-starcraft.sh 保持原样。
#
# 渲染链（与 left4dead2 共用 Venus 服务端）：
#   32 位 starcraft.exe -> wined3d -> Mesa GLX (zink)
#     -> 32 位 virtio/Venus ICD -> Unix socket
#     -> 64 位 virgl_test_server --venus -> libvulkan_gf.so -> 7G100
#
# 为什么不用 new WoW64 + 原生 gf ICD：1998 年的 DirectDraw 程序在 Wine 11
# new WoW64 前缀下会弹 "DirectDraw was unable to be acquired / Data File
# Error"；改用原有 32 位前缀 + zink 后全屏窗口正常，并由 Venus 枚举到
# "Virtio-GPU Venus (LISUAN 7G100 Series Graphics)"。
#
# 前置：left4dead2 的 l4d2-venus.service 必须 active 且 socket 存在。
# 本脚本只检查，不自动启动/重启服务，不做看门狗或守护；退出码原样透传。
#
# 用法：
#   ./starcraft/run-starcraft-hw.sh             # 前台启动（硬件渲染）
#   ./starcraft/run-starcraft-hw.sh --status    # 只读体检
#   ./starcraft/run-starcraft-hw.sh --help
#
# 环境变量：STARCRAFT_PREFIX / STARCRAFT_PROTON_DIR / STARCRAFT_WINE /
#   STARCRAFT_EXE / STARCRAFT_WINEDEBUG / STARCRAFT_VENUS_SOCKET /
#   STARCRAFT_VENUS_UNIT / STARCRAFT_WINDOW_WAIT / STARCRAFT_NO_FULLSCREEN
# License: Apache-2.0
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROTON_DIR="${STARCRAFT_PROTON_DIR:-$HOME/.local/share/Steam/steamapps/common/Proton 11.0}"
PROTON_DIR="${PROTON_DIR%/}"
WINE="${STARCRAFT_WINE:-$PROTON_DIR/files/bin/wine}"
PREFIX="${STARCRAFT_PREFIX:-$HOME/.deepinwine/Starcraft-wine11}"
PREFIX="${PREFIX%/}"
GAME_DIR="${STARCRAFT_DIR:-$PREFIX/drive_c/Program Files/starcraft}"
GAME_DIR="${GAME_DIR%/}"
EXE="${STARCRAFT_EXE:-starcraft.exe}"
WINDOW_WAIT="${STARCRAFT_WINDOW_WAIT:-30}"
NO_FULLSCREEN="${STARCRAFT_NO_FULLSCREEN:-0}"
SOCKET="${STARCRAFT_VENUS_SOCKET:-${XDG_RUNTIME_DIR:-/tmp}/l4d2-venus.sock}"
VENUS_UNIT="${STARCRAFT_VENUS_UNIT:-l4d2-venus.service}"
VIRTIO_ICD="${STARCRAFT_VIRTIO_ICD:-/usr/share/vulkan/icd.d/virtio_icd.json}"
ZINK_32="/usr/lib/i386-linux-gnu/dri/zink_dri.so"

die() { echo "错误：$*" >&2; exit 1; }

usage() {
    cat <<EOF
用法：$(basename "$0") [选项] [参数...]

  （无参数） 前台启动《星际争霸》7G100 硬件渲染
  --status   只读体检：Wine、游戏、Venus 服务端、32 位 zink、ICD
  --help     显示本说明

环境变量：
  STARCRAFT_PREFIX / STARCRAFT_PROTON_DIR / STARCRAFT_WINE / STARCRAFT_EXE /
  STARCRAFT_WINEDEBUG / STARCRAFT_VENUS_SOCKET / STARCRAFT_VENUS_UNIT /
  STARCRAFT_WINDOW_WAIT / STARCRAFT_NO_FULLSCREEN=1（禁用备用 Alt+Enter）

前缀：$PREFIX
Venus socket：$SOCKET（服务：$VENUS_UNIT）
崩溃后不会自动重启；按 .codebuddy/rules/崩溃处理约定.md 取证。
EOF
}

venus_ready() {
    systemctl --user is-active --quiet "$VENUS_UNIT" && [[ -S "$SOCKET" ]]
}

status() {
    local rc=0
    if [[ -x "$WINE" ]]; then echo "  [OK] Wine：$WINE"; else echo "  [--] wine 缺失：$WINE"; rc=1; fi
    if [[ -s "$GAME_DIR/$EXE" && "$(head -c2 "$GAME_DIR/$EXE")" == "MZ" ]]; then
        echo "  [OK] 主程序：$GAME_DIR/$EXE"
    else
        echo "  [--] 主程序缺失：$GAME_DIR/$EXE"; rc=1
    fi
    if [[ -f "$ZINK_32" ]]; then echo "  [OK] 32 位 zink：$ZINK_32"; else echo "  [--] 缺 32 位 zink：$ZINK_32"; rc=1; fi
    if [[ -f "$VIRTIO_ICD" ]]; then echo "  [OK] Venus ICD：$VIRTIO_ICD"; else echo "  [--] 缺 Venus ICD：$VIRTIO_ICD"; rc=1; fi
    if venus_ready; then
        echo "  [OK] Venus 服务端：$VENUS_UNIT active，socket=$SOCKET"
    else
        echo "  [--] Venus 服务端未就绪（$VENUS_UNIT / $SOCKET）；请查 journalctl --user -u $VENUS_UNIT"
        rc=1
    fi
    echo "  [ii] 渲染链：wined3d -> Mesa zink -> Venus ICD -> $SOCKET -> gf -> 7G100"
    echo "  [ii] 不使用 LIBGL_ALWAYS_SOFTWARE（启动时会显式清除该变量）"
    return "$rc"
}

case "${1:-}" in
    --help|-h) usage; exit 0 ;;
    --status) status; exit $? ;;
esac

[[ -x "$WINE" ]] || die "找不到 Wine：$WINE（可设 STARCRAFT_PROTON_DIR）"
[[ -s "$GAME_DIR/$EXE" && "$(head -c2 "$GAME_DIR/$EXE")" == "MZ" ]] || \
    die "主程序缺失或不是有效 PE：$GAME_DIR/$EXE"
[[ -f "$ZINK_32" && -f "$VIRTIO_ICD" ]] || { status >&2; exit 1; }
venus_ready || { status >&2; die "Venus 服务端未就绪；不会自动拉起服务"; }

if command -v pgrep >/dev/null 2>&1 && pgrep -f "$GAME_DIR/$EXE" >/dev/null 2>&1; then
    die "该游戏目录已有实例在运行；请先关闭已开的星际争霸"
fi

find_window() {
    wmctrl -l 2>/dev/null | awk 'tolower($0) ~ /brood war|starcraft/ {print $1; exit}'
}

is_fullscreen() {
    xprop -id "$1" _NET_WM_STATE 2>/dev/null | grep -q _NET_WM_STATE_FULLSCREEN && return 0
    local geom x y width height
    geom="$(wmctrl -lG 2>/dev/null | awk -v w="$1" '$1 == w {print $3, $4, $5, $6}')"
    [[ -n "$geom" ]] || return 1
    read -r x y width height <<< "$geom"
    [[ "$x" -eq 0 && "$y" -eq 0 && "$width" -ge 1900 && "$height" -ge 1000 ]]
}

libs="$PROTON_DIR/files/lib/x86_64-linux-gnu:$PROTON_DIR/files/lib/i386-linux-gnu"
[[ -n "${LD_LIBRARY_PATH:-}" ]] && libs="$libs:$LD_LIBRARY_PATH"

cd "$GAME_DIR"
env -u LIBGL_ALWAYS_SOFTWARE \
    WINEPREFIX="$PREFIX" \
    PROTON_USE_WOW64=1 \
    WINEDLLPATH="$PROTON_DIR/files/lib/vkd3d:$PROTON_DIR/files/lib/wine" \
    LD_LIBRARY_PATH="$libs" \
    WINEDEBUG="${STARCRAFT_WINEDEBUG:--all}" \
    __GLX_VENDOR_LIBRARY_NAME=mesa \
    MESA_LOADER_DRIVER_OVERRIDE=zink \
    VK_ICD_FILENAMES="$VIRTIO_ICD" \
    VTEST_SOCKET_NAME="$SOCKET" \
    VN_DEBUG=vtest \
    VK_LOADER_LAYERS_DISABLE='VK_LAYER_VALVE_steam_overlay_32,VK_LAYER_VALVE_steam_overlay_64' \
    WINEDLLOVERRIDES='mscoree,mshtml,winemenubuilder=' \
    "$WINE" "$EXE" "$@" &
wine_pid=$!

if [[ "$NO_FULLSCREEN" != "1" ]]; then
    if ! command -v wmctrl >/dev/null 2>&1 || ! command -v xdotool >/dev/null 2>&1; then
        echo "[starcraft-hw] 警告：缺少 wmctrl/xdotool，本次不检查全屏状态" >&2
    else
        wid=""
        for _ in $(seq 1 "$((WINDOW_WAIT * 2))"); do
            kill -0 "$wine_pid" 2>/dev/null || break
            wid="$(find_window)"
            [[ -n "$wid" ]] && break
            sleep 0.5
        done
        if [[ -n "$wid" ]]; then
            sleep 2
            if is_fullscreen "$wid"; then
                echo "[starcraft-hw] 已创建全屏窗口 ($wid)；渲染链见 --status" >&2
            else
                wmctrl -i -a "$wid" 2>/dev/null || true
                sleep 0.5
                xdotool key --clearmodifiers alt+Return 2>/dev/null || true
                sleep 2
                is_fullscreen "$wid" || \
                    echo "[starcraft-hw] 警告：未检测到全屏状态，请检查日志（不做自动重试）" >&2
            fi
        elif kill -0 "$wine_pid" 2>/dev/null; then
            echo "[starcraft-hw] 警告：${WINDOW_WAIT}s 内未找到游戏窗口" >&2
        fi
    fi
fi

rc=0
wait "$wine_pid" || rc=$?
exit "$rc"
