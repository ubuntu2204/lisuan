#!/usr/bin/env bash
# 豆包托盘菜单「体检 + 修复」入口。
#
# 背景（2026-11-08 取证，见 doubao/README.md 2.6.3）：
#   豆包的托盘右键菜单是 Chromium 自绘的独立顶层窗口（Chrome_WidgetWin_1，165x284）。
#   实测规律：
#     · 客户端每次启动后，**第一次**右键托盘图标一定能弹出菜单；
#     · 菜单一旦被关闭过一次（按 ESC、点菜单外、点菜单项都会），
#       之后**永久**不再弹出，直到重启客户端；
#     · 菜单一直开着不关的话，连续右键都正常；
#     · Wine / GNOME / appindicator 这一侧完全正常 —— 坏态下右键依然产生
#       `notify_owner relaying 0x204` 与
#       `send_inter_thread_message hwnd <Doubao_StatusTrayWindow> msg 8001 wp 8 lp 204`，
#       目标窗口句柄与好态完全一致，是客户端收到消息后自己不建菜单窗口；
#     · 6 种外部复位手段全部无效：激活/最小化主窗口、广播 TaskbarCreated、
#       左键托盘、单实例再拉、Wine 内部注入 WM_APP+1、打开再关闭另一个 Chromium 菜单。
#   结论：这是客户端托盘模块自身的状态问题，本机无法从外部复位，
#   **唯一可靠的恢复手段是重启客户端**。本脚本把这件事自动化。
#
# 用法：
#   tray-doctor.sh --status   只读报告（不碰客户端状态）
#   tray-doctor.sh --probe    主动探测：合成一次右键，看菜单是否出现
#                            （⚠ 会在屏幕上真的弹出菜单，且**不会**替你关闭；
#                              关闭后托盘菜单即进入坏态，属预期行为）
#   tray-doctor.sh --fix      探测；若坏则重启客户端并唤回主窗口
#
set -uo pipefail

SELF_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROBE="$SELF_DIR/tray-probe.py"
QUIT="$SELF_DIR/quit-doubao.sh"
LAUNCHER="$SELF_DIR/run-doubao-wine11.sh"

usage() {
    sed -n '2,30p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
}

proc_count() {
    # 豆包主浏览器 + 它的各类子进程（renderer / utility / crashpad）
    pgrep -f 'Doubao\.exe' 2>/dev/null | wc -l
}

main_pid() {
    local p
    for p in $(pgrep -f 'Doubao\.exe' 2>/dev/null); do
        if ! tr '\0' ' ' < "/proc/$p/cmdline" 2>/dev/null | grep -q -- '--type='; then
            echo "$p"; return 0
        fi
    done
    return 1
}

do_status() {
    local mp n
    mp=$(main_pid || true)
    n=$(proc_count)
    printf '豆包进程    : %s 个\n' "$n"
    if [[ -z "$mp" ]]; then
        printf '主浏览器    : 未在运行\n'
        printf '托盘菜单    : 不适用（客户端没跑）\n'
        return 1
    fi
    printf '主浏览器 pid: %s\n' "$mp"
    printf '本次启动日志: %s\n' "${DOUBAO_WINE11_LOG:-$HOME/.local/share/doubao-wine11/launch.log}"
    printf '托盘图标    : '
    local icons
    icons=$(python3 "$PROBE" --list 2>/dev/null | head -1)
    printf '%s\n' "${icons:-未找到（客户端可能刚启动，托盘图标还没注册）}"
    printf '托盘菜单    : 需要 --probe 才能判定（坏态下没有任何外部可观测特征）\n'
    printf '提醒        : 菜单一旦被关闭过，托盘右键就再也不弹，只能 --fix 重启客户端。\n'
    return 0
}

do_probe() {
    if [[ -z "$(main_pid || true)" ]]; then
        echo "客户端没在跑，无法探测。" >&2
        return 2
    fi
    echo "探测中（会真的弹出托盘菜单，本脚本不会替你关闭）…"
    python3 "$PROBE" --button 3 --timeout "${PROBE_TIMEOUT:-6}"
    local rc=$?
    case $rc in
        0) printf '判定: 托盘菜单正常（菜单已留在屏幕上，直接用即可）。\n' ;;
        1) printf '判定: 托盘菜单已坏 —— 右键送达了客户端但菜单窗口没被创建。\n' ;;
        3) printf '判定: 顶栏没找到托盘图标窗口（appindicator / systray 侧问题）。\n' ;;
        *) printf '判定: 探测异常。\n' ;;
    esac
    return $rc
}

do_fix() {
    local rc
    do_probe
    rc=$?
    if [[ $rc -eq 0 ]]; then
        printf '\n托盘菜单可用，无需重启。\n'
        return 0
    fi
    if [[ $rc -eq 3 ]]; then
        printf '\n顶栏托盘图标都不在，重启客户端未必能补上（图标由 Wine systray 代管）。\n' >&2
        printf '先确认 Ubuntu 的 appindicator 扩展已启用 legacy 托盘：\n' >&2
        printf '  gsettings get org.gnome.shell.extensions.appindicator legacy-tray-enabled\n' >&2
        return 3
    fi

    printf '\n开始重启客户端以复位托盘状态…\n'
    bash "$QUIT" --quit || return 1
    sleep 2
    setsid bash "$LAUNCHER" >/dev/null 2>&1 < /dev/null &
    sleep 8
    bash "$QUIT" --show || true

    # 客户端起来后托盘图标不是立刻注册的，要等它出现在顶栏
    printf '等待托盘图标重新注册…'
    local waited=0 icon=""
    while [[ $waited -lt 90 ]]; do
        icon=$(python3 "$PROBE" --list 2>/dev/null | head -1)
        [[ -n "$icon" ]] && break
        sleep 3
        waited=$((waited + 3))
        printf '.'
    done
    printf '\n'
    if [[ -z "$icon" ]]; then
        printf '托盘图标没出现（等了 %d 秒）。这通常是 Wine systray / appindicator 侧的问题。\n' "$waited" >&2
        return 3
    fi
    printf '托盘图标已就位：%s\n' "$icon"

    # 复位后必须再验一次：第一次右键才是好的，探测不能代替用户点
    do_probe
    rc=$?
    if [[ $rc -eq 0 ]]; then
        printf '\n已复位，托盘菜单可用（菜单已留在屏幕上）。\n'
    else
        printf '\n已重启客户端，但托盘菜单仍探测不到（rc=%d）。请手动右键一次确认。\n' "$rc" >&2
    fi
    printf '（再关掉它，托盘菜单又会进入坏态 —— 这是客户端自身的缺陷，不是本机环境问题。）\n'
    return 0
}

case "${1:---status}" in
    --status) do_status ;;
    --probe)  do_probe ;;
    --fix)    do_fix ;;
    --help|-h) usage ;;
    *) usage; exit 2 ;;
esac
