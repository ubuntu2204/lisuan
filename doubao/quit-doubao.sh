#!/usr/bin/env bash
# 至砺算：豆包「退出 / 唤回 / 状态」入口。
#
# 为什么需要它（2026-11-08 取证）：
#   1) 豆包右上角 X 的语义是「隐藏主窗口到通知栏」，不是退出进程。实测点 X 后
#      主窗口 WM_STATE 变成 Withdrawn、User Data/saman_app_state 里
#      chat_app_state.visible=false，而 17 个 Doubao.exe 进程照常在跑。
#   2) 托盘图标确实上了顶栏（Ubuntu 的 ubuntu-appindicators 扩展自带 legacy
#      XEmbed 托盘，legacy-tray-enabled=true），但只是个空壳：图标窗口由 Wine 的
#      explorer.exe 代管（_XEMBED_INFO flags=1，16x16，顶栏右侧），而客户端侧
#      能接菜单的对象是空的 —— 启动就报「[Tray] BuildMainStatusIconContextMenu
#      main_status_icon_ is null」。同时云配置/登录链路一直失败（本机日志
#      Cloud config not exists or invalid 300 次、user_id not ready 23 次、
#      account_token_manager_initial_fetch_error），托盘菜单就建不出来，
#      于是既没有「退出豆包」菜单，也不能靠托盘唤回窗口。
#   3) 客户端没有「关闭主窗口时退出程序」这类设置项：Doubao.dll 里的
#      saman.general_settings.* 只有 double_click_close_tab / file_open_method /
#      link_open_method / primary_browser 四个键；桌面悬浮球右键菜单也只有
#      「打开豆包窗口 / 停用豆包桌面悬浮球」等，没有退出项。
#   4) 顶部状态栏（或任何 WM 级「退出」）发的也是 WM_DELETE_WINDOW，同样退不掉：
#      只关主窗口 / 隐藏悬浮球后再关 / 主窗口与悬浮球都关，三种组合下 17 个进程
#      都一直存活。而顶栏图标右键：真实指针、XSendEvent、windowfocus+Menu 键、
#      windowraise、新鲜启动、销毁主窗口后 —— 6 种输入全部既无菜单也无 app 侧
#      tray 日志。所以「有时好使」取决于客户端自己那次云配置/登录态有没有拉到，
#      不是图形栈问题；判「是否真退出」只看 --status 的进程数，窗口消失不算退。
#
# 因此这里只做三件事，全部显式、按需调用：
#   --quit（默认） 彻底退出本前缀的豆包进程；
#   --show         唤回主窗口（再走一次启动器，客户端单实例自己 ShowWindow）；
#   --status       只读报告进程、主窗口映射状态、托盘可用性。
# 不加看门狗、不做崩溃自动重启、不改客户端版本、不影响 Steam / 其它 Wine 前缀。
#
# 2026-11-08 实测：--quit 从 SIGTERM 到 17 个进程全部退出只用 1 秒，
# repair/record.json 的 running_exception 仍为 0（不会触发「启动异常」修复弹窗）；
# --quit 后 --show 重新启动 6 秒出主窗口。
set -euo pipefail

PREFIX="${DOUBAO_WINE11_PREFIX:-$HOME/.deepinwine/doubao-wine11}"
PROTON="${DOUBAO_PROTON:-$HOME/.local/share/Steam/steamapps/common/Proton 11.0}"
WINESERVER="${DOUBAO_WINESERVER:-$PROTON/files/bin/wineserver}"
LAUNCHER="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/run-doubao-wine11.sh"
LAUNCH_LOG="${DOUBAO_WINE11_LOG:-$HOME/.local/share/doubao-wine11/launch.log}"
QUIT_WAIT="${DOUBAO_QUIT_WAIT:-10}"

usage() {
    cat <<EOF
用法：$(basename "$0") [--quit|--show|--status|--help]

  --quit（默认） 彻底退出豆包。先 SIGTERM 主浏览器进程等 ${QUIT_WAIT} 秒，
                 仍存活则用本前缀的 wineserver -k 收尾（只影响 $PREFIX）。
  --show         唤回主窗口。客户端是单实例，再启动一次会自己把窗口显示出来
                 （点 X 隐入通知栏那种情况实测 5 秒内回来）；
                 没在跑时等同于正常启动（实测 6 秒出界面）。
  --status       只读检查：进程数、主窗口 Map State、顶栏托盘图标与菜单可用性。
  --help         显示本说明。

说明：豆包右上角 X 只「隐藏到通知栏」，不退出进程；本机托盘图标虽然显示在
顶栏，但客户端侧建不出菜单，所以 X 之后要用 --show 唤回、用 --quit 真正退出。
可用环境变量覆盖：DOUBAO_WINE11_PREFIX / DOUBAO_PROTON / DOUBAO_WINESERVER /
DOUBAO_QUIT_WAIT。
EOF
}

# 属于本前缀的豆包 Windows 进程：environ 的 WINEPREFIX 命中本前缀，
# 且 cmdline 是豆包的可执行文件（含 wine 启动器那条 unix 路径）。
prefix_procs() {
    local dir p cmd me
    me=$(id -u)
    for dir in /proc/[0-9]*; do
        p="${dir#/proc/}"
        # 只扫自己的进程：别人的 environ 读不到，硬读会在终端刷一堆「权限不够」。
        [[ "$(stat -c %u "$dir" 2>/dev/null)" == "$me" ]] || continue
        [[ -r "$dir/cmdline" ]] || continue
        # 有些自有进程（如 systemd --user）的 environ 虽然属主是自己但内核不给读，
        # 故把读取关进子 shell 并丢掉 stderr，免得到处刷「权限不够」。
        { tr '\0' '\n' < "$dir/environ" 2>/dev/null | grep -Fxq "WINEPREFIX=$PREFIX"; } 2>/dev/null || continue
        cmd=$(tr '\0' ' ' < "$dir/cmdline" 2>/dev/null) || continue
        case "$cmd" in
            *Doubao.exe*|*aha_doctor.exe*|*Doubao_proxy*|*DoubaoLinkRouter.exe*)
                printf '%s\t%s\n' "$p" "$cmd" ;;
        esac
    done
}

# 主浏览器进程：cmdline 里没有 --type= 的那条（wine 启动器那条含 unix 路径，同样无 --type=）
main_pid() {
    prefix_procs | awk -F'\t' '$2 !~ /--type=/ && $2 ~ /Doubao\.exe/ {print $1; exit}'
}

# 主窗口：有标题且面积最大的那个（豆包主窗口 1200x800，标题形如「xxx - 豆包」）。
# 面积阈值用来排除 1x1 的「Default IME」等带名辅助窗口；
# 没有带标题的大窗口时退回面积最大者。
main_window() {
    local pid="$1" w best=0 bestw="" nbest=0 nw="" area name
    command -v xdotool >/dev/null 2>&1 || return 0
    for w in $(xdotool search --pid "$pid" 2>/dev/null); do
        area=$(xwininfo -id "$w" 2>/dev/null |
            awk '/Width:/{a=$2} /Height:/{h=$2} END{print a*h+0}')
        [[ "${area:-0}" -ge 100000 ]] || continue
        [[ "${area:-0}" -gt "$best" ]] && { best=$area; bestw=$w; }
        name=$(xdotool getwindowname "$w" 2>/dev/null || true)
        if [[ -n "$name" && "${area:-0}" -gt "$nbest" ]]; then
            nbest=$area; nw=$w
        fi
    done
    printf '%s' "${nw:-$bestw}"
}

window_state() {
    local w="${1:-}"
    [[ -n "$w" ]] || { printf '无窗口'; return 0; }
    xwininfo -id "$w" 2>/dev/null | grep -o 'IsViewable\|IsUnMapped' | head -1 || printf '未知'
}

# 只取「本次启动」的日志段：日志里每次启动都有一行「=== 时间 | exe ===」头。
current_session_log() {
    [[ -r "$LAUNCH_LOG" ]] || return 0
    local hdr
    hdr=$(grep -an '^=== ' "$LAUNCH_LOG" 2>/dev/null | tail -1)
    [[ -n "$hdr" ]] || { cat "$LAUNCH_LOG"; return 0; }
    tail -n +"${hdr%%:*}" "$LAUNCH_LOG"
}

# 托盘现场：Ubuntu 的 appindicator 扩展有 legacy XEmbed 托盘（legacy-tray-enabled），
# 图标窗口由 Wine 的 explorer.exe 代管，不在 _NET_SYSTEM_TRAY_S0 owner 的
# 子窗口里（只查那里会漏判）。这里直接找 explorer.exe 的 16x16 可见窗口，
# 再看日志里客户端侧能不能建出菜单。
tray_report() {
    [[ -n "${DISPLAY:-}" ]] || export DISPLAY=:0
    local last_err="${1:-}" icon="" p w g
    for p in $(pgrep -f 'explorer\.exe' 2>/dev/null); do
        for w in $(xdotool search --pid "$p" 2>/dev/null); do
            g=$(xwininfo -id "$w" 2>/dev/null | awk '/Width:/{a=$2} /Height:/{h=$2} END{print a"x"h}')
            [[ "$(xwininfo -id "$w" 2>/dev/null | grep -o 'IsViewable')" == "IsViewable" && "$g" == "16x16" ]] && icon="$w"
        done
    done
    if [[ -n "$icon" ]]; then
        printf '图标窗口 %s（%s）在顶栏' "$icon" \
            "$(xwininfo -id "$icon" 2>/dev/null | awk '/Absolute upper-left X/{x=$NF} /Absolute upper-left Y/{y=$NF} END{print x","y}')"
    else
        printf '顶栏未见 16x16 托盘图标窗口'
    fi
    if command -v gsettings >/dev/null 2>&1; then
        printf '（appindicator legacy-tray=%s）' \
            "$(gsettings get org.gnome.shell.extensions.appindicator legacy-tray-enabled 2>/dev/null | tr -d "'")"
    fi
    if [[ -n "$last_err" ]]; then
        printf '；但本次启动里客户端侧 StatusIcon 为空（main_status_icon_ is null）→ 右键建不出菜单'
    else
        printf '；本次启动未见 main_status_icon_ is null → 托盘菜单应该可用'
    fi
}

do_status() {
    local procs n mp w
    procs=$(prefix_procs)
    n=$(printf '%s\n' "$procs" | grep -c '[0-9]' || true)
    mp=$(main_pid || true)
    printf '前缀        : %s\n' "$PREFIX"
    printf '豆包进程    : %s 个\n' "$n"
    if [[ -n "$mp" ]]; then
        w=$(main_window "$mp" || true)
        local st
        st=$(window_state "$w")
        printf '主浏览器    : pid %s\n' "$mp"
        printf '主窗口      : %s（%s）\n' "${w:-未发现}" "$st"
        case "$st" in
            IsUnMapped) printf '判定        : 窗口已隐入通知栏、进程仍在 —— 这就是点 X 的效果，不是崩溃\n' ;;
            IsViewable) printf '判定        : 主窗口正常显示\n' ;;
            *)          printf '判定        : 未找到主窗口，请看 %s\n' "$LAUNCH_LOG" ;;
        esac
    else
        printf '主浏览器    : 未在运行\n'
    fi
    local t
    t=$(current_session_log | grep -a 'main_status_icon_ is null' | tail -1)
    printf '托盘现场    : %s\n' "$(tray_report "$t")"
    [[ -n "$t" ]] && printf '托盘日志    : %s\n' "${t:0:120}"
}

do_show() {
    [[ -x "$LAUNCHER" || -f "$LAUNCHER" ]] || { echo "错误：找不到启动器 $LAUNCHER" >&2; return 1; }
    local mp w i
    mp=$(main_pid || true)
    if [[ -n "$mp" ]]; then
        w=$(main_window "$mp" || true)
        if [[ -z "$w" ]]; then
            printf '客户端在跑（pid %s）但主窗口已被销毁，单实例再拉不会重建它。\n' "$mp"
            printf '两条路：① 左下/右下角悬浮球右键→「打开豆包窗口」；② 本脚本 --quit 后重新启动。\n' >&2
            return 1
        fi
        printf '客户端在跑（pid %s，窗口 %s %s），用单实例再拉一次以显示主窗口…\n' \
            "$mp" "$w" "$(window_state "$w")"
    else
        printf '客户端没在跑，正常启动…\n'
    fi
    setsid bash "$LAUNCHER" >/dev/null 2>&1 < /dev/null &
    for i in $(seq 1 30); do
        sleep 2
        mp=$(main_pid || true)
        [[ -n "$mp" ]] || continue
        w=$(main_window "$mp" || true)
        if [[ -n "$w" && "$(window_state "$w")" == "IsViewable" ]]; then
            printf '已唤回：窗口 %s IsViewable（用时 %s 秒）\n' "$w" "$((i * 2))"
            return 0
        fi
    done
    printf '错误：%s 秒内主窗口仍未 IsViewable。\n' "$((30 * 2))" >&2
    printf '如果是点 X 后窗口被销毁（不是隐入通知栏），悬浮球右键→「打开豆包窗口」或 --quit 后重启。\n' >&2
    printf '现场日志：%s\n' "$LAUNCH_LOG" >&2
    return 1
}

do_quit() {
    local mp procs n left i
    procs=$(prefix_procs)
    n=$(printf '%s\n' "$procs" | grep -c '[0-9]' || true)
    if [[ "$n" == 0 ]]; then
        printf '本前缀没有豆包进程，无需退出。\n'
        return 0
    fi
    mp=$(main_pid || true)
    if [[ -n "$mp" ]]; then
        printf '第 1 步：SIGTERM 主浏览器进程 %s（等最多 %s 秒）…\n' "$mp" "$QUIT_WAIT"
        kill -TERM "$mp" 2>/dev/null || true
        for i in $(seq 1 "$QUIT_WAIT"); do
            sleep 1
            left=$(prefix_procs | grep -c '[0-9]' || true)
            [[ "$left" == 0 ]] && { printf '已退出（%s 秒）。\n' "$i"; return 0; }
        done
        printf '仍有 %s 个进程存活。\n' "$left"
    fi
    if [[ -x "$WINESERVER" ]]; then
        printf '第 2 步：wineserver -k（只作用于 %s）…\n' "$PREFIX"
        WINEPREFIX="$PREFIX" "$WINESERVER" -k 2>/dev/null || true
    else
        printf '提示：找不到 %s，跳过 wineserver -k。\n' "$WINESERVER" >&2
    fi
    for i in $(seq 1 10); do
        sleep 1
        left=$(prefix_procs | grep -c '[0-9]' || true)
        [[ "$left" == 0 ]] && { printf '已退出。\n'; return 0; }
    done
    printf '第 3 步：仍剩 %s 个进程，逐个 SIGTERM。\n' "$left" >&2
    prefix_procs | cut -f1 | while read -r p; do kill -TERM "$p" 2>/dev/null || true; done
    sleep 2
    left=$(prefix_procs | grep -c '[0-9]' || true)
    if [[ "$left" == 0 ]]; then
        printf '已退出。\n'
    else
        printf '错误：还有 %s 个进程没退掉，请把它们贴出来再排查（不做强杀，避免误伤）。\n' "$left" >&2
        prefix_procs | sed 's/^/  /' >&2
        return 1
    fi
}

case "${1:---quit}" in
    --quit)   do_quit ;;
    --show)   do_show ;;
    --status) do_status ;;
    --help|-h) usage ;;
    *) usage; exit 2 ;;
esac
