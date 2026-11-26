#!/usr/bin/env bash
# 至砺算：星际争霸 1.08（32 位 PE）单应用启动器，供桌面入口调用。
#
# 全屏根因：Spark 包自带的 deepin-wine5（Wine 5.0）用 XRandR 实现
# DirectDraw 全屏，而本机 GNOME Wayland 的 XWayland 不能切换分辨率，
# 游戏调用的 SetDisplayMode(640x480) 落不到显示器上；Alt+Enter 虽能送到
# 游戏窗口，却不会再触发显示模式切换。改用 Proton 11 自带的 Wine 11 后，
# 同一个游戏在 XWayland 上直接创建 1920x1080 的 _NET_WM_STATE_FULLSCREEN
# 窗口，无需模拟按键，也不改游戏二进制或厂商前缀。
#
# 首次运行从 Spark 前缀复制一份到独立前缀 ~/.deepinwine/Starcraft-wine11，
# 原前缀保持不动；只在游戏进程树注入 Mesa 软件 GL 与 Proton 11 的 Wine
# 运行时，不污染 Steam 客户端与全局 shell。单次前台启动、退出码原样透传；
# 不做守护、看门狗或崩溃后自动重启（见 .codebuddy/rules/崩溃处理约定.md）。
#
# 用法：
#   ./starcraft/run-starcraft.sh [参数...]   # 启动游戏（参数透传）
#   ./starcraft/run-starcraft.sh --status    # 只读体检
#   ./starcraft/run-starcraft.sh --help
#
# 环境变量：STARCRAFT_DIR / STARCRAFT_WINE / STARCRAFT_PROTON_DIR /
#   STARCRAFT_PREFIX / STARCRAFT_SOURCE_PREFIX / STARCRAFT_EXE /
#   STARCRAFT_WINEDEBUG / STARCRAFT_WINDOW_WAIT /
#   STARCRAFT_NO_FULLSCREEN=1（禁用备用 Alt+Enter，不阻止游戏自身全屏）
# License: Apache-2.0
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROTON_DIR="${STARCRAFT_PROTON_DIR:-$HOME/.local/share/Steam/steamapps/common/Proton 11.0}"
PROTON_DIR="${PROTON_DIR%/}"
SOURCE_PREFIX="${STARCRAFT_SOURCE_PREFIX:-$HOME/.deepinwine/Deepin-starcraft}"
SOURCE_PREFIX="${SOURCE_PREFIX%/}"
PREFIX="${STARCRAFT_PREFIX:-$HOME/.deepinwine/Starcraft-wine11}"
PREFIX="${PREFIX%/}"
GAME_DIR="${STARCRAFT_DIR:-$PREFIX/drive_c/Program Files/starcraft}"
GAME_DIR="${GAME_DIR%/}"
WINE="${STARCRAFT_WINE:-$PROTON_DIR/files/bin/wine}"
EXE="${STARCRAFT_EXE:-starcraft.exe}"
WINDOW_WAIT="${STARCRAFT_WINDOW_WAIT:-30}"
NO_FULLSCREEN="${STARCRAFT_NO_FULLSCREEN:-0}"

die() { echo "错误：$*" >&2; exit 1; }

usage() {
    cat <<EOF
用法：$(basename "$0") [选项] [参数...]

  （无参数） 前台启动《星际争霸》；Wine 11 直接创建全屏窗口
  --status   只读体检：wine、主程序、前缀、xdotool/wmctrl
  --help     显示本说明

环境变量：
  STARCRAFT_DIR / STARCRAFT_WINE / STARCRAFT_PROTON_DIR /
  STARCRAFT_PREFIX / STARCRAFT_SOURCE_PREFIX / STARCRAFT_EXE
  STARCRAFT_WINEDEBUG      覆盖 WINEDEBUG（默认 -all）
  STARCRAFT_WINDOW_WAIT    等待游戏窗口的最大秒数（默认 30）
  STARCRAFT_NO_FULLSCREEN=1 禁用备用 Alt+Enter（不阻止游戏自身全屏）

Wine 11 下游戏启动时即为全屏；仅当异常地未全屏时才发一次备用 Alt+Enter，
不循环按键、不强制改窗口管理器状态。
崩溃后不会自动重启；按 .codebuddy/rules/崩溃处理约定.md 取证。
EOF
}

status() {
    local rc=0
    if [[ -s "$GAME_DIR/$EXE" && "$(head -c2 "$GAME_DIR/$EXE")" == "MZ" ]]; then
        echo "  [OK] 主程序：$GAME_DIR/$EXE"
    else
        echo "  [--] 主程序缺失或不是 PE：$GAME_DIR/$EXE"; rc=1
    fi
    if [[ -f "$PREFIX/system.reg" ]]; then
        echo "  [OK] Wine 11 前缀：$PREFIX"
    elif [[ -f "$SOURCE_PREFIX/system.reg" && -s "$SOURCE_PREFIX/drive_c/Program Files/starcraft/$EXE" ]]; then
        echo "  [ii] 尚未迁移；首次启动会从 Spark 前缀复制：$SOURCE_PREFIX -> $PREFIX"
    else
        echo "  [--] Wine 前缀不存在且无法迁移：$PREFIX"; rc=1
    fi
    if [[ -x "$WINE" ]]; then
        echo "  [OK] Wine 运行时：$WINE"
    else
        echo "  [--] Wine 运行时缺失：$WINE（可设 STARCRAFT_PROTON_DIR）"; rc=1
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

[[ -x "$WINE" ]] || die "找不到 Wine：$WINE（可设 STARCRAFT_PROTON_DIR）"

# 同一前缀上并发实例会让 1998 年的 DirectDraw 代码抢独占模式失败；
# 命中即显式退出，不自动重试、不自动拉起。
prefix_busy() {
    local target="$1" f pid cmd
    for f in /proc/[0-9]*/environ; do
        [[ -r "$f" ]] || continue
        pid="${f#/proc/}"; pid="${pid%/environ}"
        { tr '\0' '\n' < "$f"; } 2>/dev/null | grep -qxF "WINEPREFIX=$target" || continue
        cmd="$(tr '\0' ' ' < "/proc/$pid/cmdline" 2>/dev/null || true)"
        if [[ "$cmd" == *wineserver* || "$cmd" == *starcraft.exe* ]]; then
            printf '%s' "$pid"
            return 0
        fi
    done
    return 1
}

# 首次运行迁移：从 Spark 的 deepin-wine5 前缀复制出一份供 Wine 11 使用，
# 原前缀不动；复制过程中不允许源前缀仍有游戏/wineserver 在跑。
if [[ "$PREFIX" != "$SOURCE_PREFIX" && ! -f "$PREFIX/system.reg" ]]; then
    [[ -f "$SOURCE_PREFIX/system.reg" ]] || \
        die "找不到 Spark 原前缀：$SOURCE_PREFIX（可设 STARCRAFT_SOURCE_PREFIX）"
    if busy_pid="$(prefix_busy "$SOURCE_PREFIX")"; then
        die "原前缀仍有实例运行（pid=$busy_pid），无法复制：$SOURCE_PREFIX"
    fi
    mkdir -p "$(dirname "$PREFIX")"
    tmp="${PREFIX}.migrate.$$"
    rm -rf "$tmp"
    echo "[starcraft] 首次运行：复制前缀 $SOURCE_PREFIX -> $PREFIX" >&2
    cp -a "$SOURCE_PREFIX" "$tmp"
    mv "$tmp" "$PREFIX"
fi

[[ -s "$GAME_DIR/$EXE" && "$(head -c2 "$GAME_DIR/$EXE")" == "MZ" ]] || \
    die "主程序缺失或不是有效 PE：$GAME_DIR/$EXE"
[[ -f "$PREFIX/system.reg" ]] || die "Wine 前缀不存在：$PREFIX"
if busy_pid="$(prefix_busy "$PREFIX")"; then
    die "该 Wine 前缀已有实例在运行（pid=$busy_pid）：$PREFIX
请先关闭已打开的游戏窗口再启动；重复点击桌面图标会触发此提示。"
fi

find_window() {
    wmctrl -l 2>/dev/null | awk 'tolower($0) ~ /brood war|starcraft/ {print $1; exit}'
}

libs="$PROTON_DIR/files/lib/x86_64-linux-gnu:$PROTON_DIR/files/lib/i386-linux-gnu"
[[ -n "${LD_LIBRARY_PATH:-}" ]] && libs="$libs:$LD_LIBRARY_PATH"

cd "$GAME_DIR"
env \
    WINEPREFIX="$PREFIX" \
    PROTON_USE_WOW64=1 \
    WINEDLLPATH="$PROTON_DIR/files/lib/vkd3d:$PROTON_DIR/files/lib/wine" \
    LD_LIBRARY_PATH="$libs" \
    WINEDEBUG="${STARCRAFT_WINEDEBUG:--all}" \
    LIBGL_ALWAYS_SOFTWARE=1 \
    __GLX_VENDOR_LIBRARY_NAME=mesa \
    WINEDLLOVERRIDES='mscoree,mshtml,winemenubuilder=' \
    "$WINE" "$EXE" "$@" &
wine_pid=$!

is_fullscreen() {
    xprop -id "$1" _NET_WM_STATE 2>/dev/null | grep -q _NET_WM_STATE_FULLSCREEN && return 0
    local geom x y width height
    geom="$(wmctrl -lG 2>/dev/null | awk -v w="$1" '$1 == w {print $3, $4, $5, $6}')"
    [[ -n "$geom" ]] || return 1
    read -r x y width height <<< "$geom"
    [[ "$x" -eq 0 && "$y" -eq 0 && "$width" -ge 1900 && "$height" -ge 1000 ]]
}

if [[ "$NO_FULLSCREEN" != "1" ]]; then
    if ! command -v wmctrl >/dev/null 2>&1 || ! command -v xdotool >/dev/null 2>&1; then
        echo "[starcraft] 警告：缺少 wmctrl/xdotool，本次不检查全屏状态" >&2
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
                echo "[starcraft] Wine 11 已直接创建全屏窗口 ($wid)，无需 Alt+Enter" >&2
            else
                # 仅在异常地没进全屏时发一次备用按键；绝不循环，也绝不在
                # 已全屏时再按一次切回窗口。
                wmctrl -i -a "$wid" 2>/dev/null || true
                sleep 0.5
                xdotool key --clearmodifiers alt+Return 2>/dev/null || true
                sleep 2
                is_fullscreen "$wid" || \
                    echo "[starcraft] 警告：未检测到全屏状态，请检查日志（不做自动重试）" >&2
            fi
        elif kill -0 "$wine_pid" 2>/dev/null; then
            echo "[starcraft] 警告：${WINDOW_WAIT}s 内未找到游戏窗口" >&2
        fi
    fi
fi

rc=0
wait "$wine_pid" || rc=$?
exit "$rc"
