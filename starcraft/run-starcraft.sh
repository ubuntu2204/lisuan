#!/usr/bin/env bash
# 至砺算：星际争霸 1.08（32 位 PE）单应用启动器，供桌面入口调用。
#
# 游戏装在 Spark 包的 deepin-wine5 前缀 ~/.deepinwine/Deepin-starcraft 中。
# 直接启动 starcraft.exe（跳过 HanStar Plus 中文启动器），窗口出现后向该
# Wine 窗口发送一次 Alt+Enter —— 这是《星际争霸》自己的全屏切换键，
# 和本机暗黑 2 在 Wine 里按 Alt+Enter 进入全屏是同一条路径，不依赖
# wmctrl 强制改窗口状态，也不修改游戏二进制或厂商前缀。
#
# 只在游戏进程树注入 Mesa 软件 GL 变量，不污染 Steam 客户端与全局 shell。
# 单次前台启动、退出码原样透传；不做守护、看门狗或崩溃后自动重启
# （见 .codebuddy/rules/崩溃处理约定.md）。
#
# 用法：
#   ./starcraft/run-starcraft.sh [参数...]   # 启动游戏（参数透传）
#   ./starcraft/run-starcraft.sh --status    # 只读体检
#   ./starcraft/run-starcraft.sh --help
#
# 环境变量：STARCRAFT_DIR / STARCRAFT_WINE / STARCRAFT_PREFIX /
#   STARCRAFT_EXE / STARCRAFT_WINEDEBUG / STARCRAFT_WINDOW_WAIT /
#   STARCRAFT_NO_FULLSCREEN=1（禁用自动全屏）
# License: Apache-2.0
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GAME_DIR="${STARCRAFT_DIR:-$HOME/.deepinwine/Deepin-starcraft/drive_c/Program Files/starcraft}"
GAME_DIR="${GAME_DIR%/}"
WINE="${STARCRAFT_WINE:-/usr/bin/deepin-wine5-stable}"
PREFIX="${STARCRAFT_PREFIX:-$HOME/.deepinwine/Deepin-starcraft}"
PREFIX="${PREFIX%/}"
EXE="${STARCRAFT_EXE:-starcraft.exe}"
WINDOW_WAIT="${STARCRAFT_WINDOW_WAIT:-30}"
NO_FULLSCREEN="${STARCRAFT_NO_FULLSCREEN:-0}"

die() { echo "错误：$*" >&2; exit 1; }

usage() {
    cat <<EOF
用法：$(basename "$0") [选项] [参数...]

  （无参数） 前台启动《星际争霸》，窗口出现后按一次 Alt+Enter 进入全屏
  --status   只读体检：wine、主程序、前缀、xdotool/wmctrl
  --help     显示本说明

环境变量：
  STARCRAFT_DIR / STARCRAFT_WINE / STARCRAFT_PREFIX / STARCRAFT_EXE
  STARCRAFT_WINEDEBUG      覆盖 WINEDEBUG（默认 -all）
  STARCRAFT_WINDOW_WAIT    等待游戏窗口的最大秒数（默认 30）
  STARCRAFT_NO_FULLSCREEN=1 禁用自动全屏（用于排障）

全屏由游戏自身处理 Alt+Enter；不强制改窗口管理器状态。
崩溃后不会自动重启；按 .codebuddy/rules/崩溃处理约定.md 取证。
EOF
}

status() {
    local rc=0
    [[ -x "$WINE" ]] && echo "  [OK] wine：$WINE" || { echo "  [--] wine 缺失：$WINE"; rc=1; }
    if [[ -s "$GAME_DIR/$EXE" && "$(head -c2 "$GAME_DIR/$EXE")" == "MZ" ]]; then
        echo "  [OK] 主程序：$GAME_DIR/$EXE"
    else
        echo "  [--] 主程序缺失或不是 PE：$GAME_DIR/$EXE"; rc=1
    fi
    if [[ -f "$PREFIX/system.reg" ]]; then
        echo "  [OK] Wine 前缀：$PREFIX"
    else
        echo "  [--] Wine 前缀不存在：$PREFIX"; rc=1
    fi
    if [[ "$NO_FULLSCREEN" != "1" ]]; then
        for tool in wmctrl xdotool; do
            command -v "$tool" >/dev/null 2>&1 && echo "  [OK] 全屏工具：$tool" || \
                { echo "  [--] 缺少 $tool（或用 STARCRAFT_NO_FULLSCREEN=1）"; rc=1; }
        done
    else
        echo "  [OK] 已禁用自动全屏（STARCRAFT_NO_FULLSCREEN=1）"
    fi
    return "$rc"
}

case "${1:-}" in
    --help|-h) usage; exit 0 ;;
    --status) status; exit $? ;;
esac

[[ -x "$WINE" ]] || die "找不到 wine：$WINE"
[[ -s "$GAME_DIR/$EXE" && "$(head -c2 "$GAME_DIR/$EXE")" == "MZ" ]] || \
    die "主程序缺失或不是有效 PE：$GAME_DIR/$EXE"
[[ -f "$PREFIX/system.reg" ]] || die "Wine 前缀不存在：$PREFIX"

# 同一前缀上并发实例会让 1998 年的 DirectDraw 代码抢独占模式失败；
# 命中即显式退出，不自动重试、不自动拉起。
prefix_busy() {
    local f pid cmd
    for f in /proc/[0-9]*/environ; do
        [[ -r "$f" ]] || continue
        pid="${f#/proc/}"; pid="${pid%/environ}"
        { tr '\0' '\n' < "$f"; } 2>/dev/null | grep -qxF "WINEPREFIX=$PREFIX" || continue
        cmd="$(tr '\0' ' ' < "/proc/$pid/cmdline" 2>/dev/null || true)"
        if [[ "$cmd" == *wineserver* || "$cmd" == *starcraft.exe* ]]; then
            printf '%s' "$pid"
            return 0
        fi
    done
    return 1
}
if busy_pid="$(prefix_busy)"; then
    die "该 Wine 前缀已有实例在运行（pid=$busy_pid）：$PREFIX
请先关闭已打开的游戏窗口再启动；重复点击桌面图标会触发此提示。"
fi

find_window() {
    wmctrl -l 2>/dev/null | awk 'tolower($0) ~ /brood war|starcraft/ {print $1; exit}'
}

cd "$GAME_DIR"
env \
    WINEPREFIX="$PREFIX" \
    WINEDEBUG="${STARCRAFT_WINEDEBUG:--all}" \
    LIBGL_ALWAYS_SOFTWARE=1 \
    __GLX_VENDOR_LIBRARY_NAME=mesa \
    WINEDLLOVERRIDES='winemenubuilder=' \
    "$WINE" "$EXE" "$@" &
wine_pid=$!

if [[ "$NO_FULLSCREEN" != "1" ]]; then
    if ! command -v wmctrl >/dev/null 2>&1 || ! command -v xdotool >/dev/null 2>&1; then
        echo "[starcraft] 警告：缺少 wmctrl/xdotool，本次不自动全屏" >&2
    else
        wid=""
        for _ in $(seq 1 "$((WINDOW_WAIT * 2))"); do
            kill -0 "$wine_pid" 2>/dev/null || break
            wid="$(find_window)"
            [[ -n "$wid" ]] && break
            sleep 0.5
        done
        if [[ -n "$wid" ]]; then
            is_fullscreen() {
                xprop -id "$1" _NET_WM_STATE 2>/dev/null | grep -q _NET_WM_STATE_FULLSCREEN && return 0
                local geom x y width height
                geom="$(wmctrl -lG 2>/dev/null | awk -v w="$1" '$1 == w {print $3, $4, $5, $6}')"
                [[ -n "$geom" ]] || return 1
                read -r x y width height <<< "$geom"
                [[ "$x" -eq 0 && "$y" -eq 0 && "$width" -ge 1900 && "$height" -ge 1000 ]]
            }
            # DirectDraw 主表面刚建好时按键会被忽略（实测），延后并有限重试。
            # 每次先查状态，绝不在已全屏时再按一次切回窗口。
            sleep 2
            sent=0
            for _ in 1 2 3 4; do
                wmctrl -l 2>/dev/null | awk '{print $1}' | grep -qxF "$wid" || break
                is_fullscreen "$wid" && break
                wmctrl -i -a "$wid" 2>/dev/null || true
                sleep 0.5
                xdotool key --window "$wid" --clearmodifiers alt+Return 2>/dev/null && sent=1
                sleep 2
                is_fullscreen "$wid" && break
            done
            if (( sent )); then
                echo "[starcraft] 已向游戏窗口 ($wid) 发送 Alt+Enter（游戏内全屏切换）" >&2
            else
                echo "[starcraft] 警告：Alt+Enter 发送失败，游戏可能仍是窗口模式" >&2
            fi
            is_fullscreen "$wid" || \
                echo "[starcraft] 警告：未检测到全屏状态，请检查日志或改用 STARCRAFT_NO_FULLSCREEN=1 排障" >&2
        elif kill -0 "$wine_pid" 2>/dev/null; then
            echo "[starcraft] 警告：${WINDOW_WAIT}s 内未找到游戏窗口，未发送 Alt+Enter" >&2
        fi
    fi
fi

rc=0
wait "$wine_pid" || rc=$?
exit "$rc"
