#!/usr/bin/env bash
# 至砺算：星际争霸 1.08 鼠标不跟手修复（与砺算显卡驱动无关）。
#
# 根因（2026-11-26）：Spark 原前缀的 HKCU\Control Panel\Mouse 带 Windows
# 指针加速：MouseSpeed=1，MouseThreshold1=6，MouseThreshold2=10。
# Wine 11 全屏 XWayland 下，游戏拿到的 WM_MOUSEMOVE 位移先经过 Wine 的
# 指针加速，再叠加全屏指针 warp/回中；小位移被放大、大位移被压缩，
# 主观上就是光标不跟手、点选和边scroll错位。
#
# 修复：只把当前独立前缀的这三项 REG_SZ 改为 0，关闭 Windows 指针加速；
# MouseSensitivity 保持 Windows 默认 10。改前导出 .reg 备份，幂等，
# 可 --uninstall 还原。不改宿主 libinput、系统驱动或 Steam 全局配置，
# 不做存活监控、自动重启或守护。
#
# 用法：
#   ./starcraft/fix-starcraft-mouse.sh             # 幂等应用
#   ./starcraft/fix-starcraft-mouse.sh --status    # 只读体检
#   ./starcraft/fix-starcraft-mouse.sh --uninstall # 从备份还原
#   ./starcraft/fix-starcraft-mouse.sh --help
#
# 环境变量：STARCRAFT_PREFIX / STARCRAFT_PROTON_DIR / STARCRAFT_WINE /
#   STARCRAFT_MOUSE_FIX_DATA
# License: Apache-2.0
set -euo pipefail

PREFIX="${STARCRAFT_PREFIX:-$HOME/.deepinwine/Starcraft-wine11}"
PREFIX="${PREFIX%/}"
PROTON_DIR="${STARCRAFT_PROTON_DIR:-$HOME/.local/share/Steam/steamapps/common/Proton 11.0}"
PROTON_DIR="${PROTON_DIR%/}"
WINE="${STARCRAFT_WINE:-$PROTON_DIR/files/bin/wine}"
DATA="${STARCRAFT_MOUSE_FIX_DATA:-$HOME/.local/share/starcraft/mouse-fix}"
BACKUP="$DATA/Mouse.reg.bak"
KEY='HKCU\Control Panel\Mouse'

die() { echo "错误：$*" >&2; exit 1; }

usage() {
    cat <<EOF
用法：$(basename "$0") [--status|--uninstall|--help]

  （无参数） 备份鼠标注册表并把指针加速三项设为 0（幂等）
  --status    只读体检：MouseSpeed / MouseThreshold1 / MouseThreshold2
  --uninstall 从 .reg 备份还原
  --help      显示本说明

环境变量：STARCRAFT_PREFIX / STARCRAFT_PROTON_DIR / STARCRAFT_WINE /
          STARCRAFT_MOUSE_FIX_DATA
前缀：$PREFIX
备份：$BACKUP
说明：只关闭该前缀的 Windows 指针加速；不动宿主鼠标设置或显卡驱动。
EOF
}

run_wine() {
    timeout 40 env WINEPREFIX="$PREFIX" WINEDEBUG=-all "$WINE" "$@"
}

prefix_busy() {
    local target="$1" f pid cmd
    for f in /proc/[0-9]*/environ; do
        [[ -r "$f" ]] || continue
        pid="${f#/proc/}"; pid="${pid%/environ}"
        { tr '\0' '\n' < "$f"; } 2>/dev/null | grep -qxF "WINEPREFIX=$target" || continue
        cmd="$(tr '\0' ' ' < "/proc/$pid/cmdline" 2>/dev/null || true)"
        # 只拦真正的游戏进程：本脚本自己的 reg query/export 也会短暂启动
        # wineserver，若把 wineserver 也算占用会误报“前缀忙”。游戏在跑时
        # 一定同时存在 starcraft.exe，所以这里只按它判定。
        if [[ "$cmd" == *starcraft.exe* ]]; then
            printf '%s' "$pid"
            return 0
        fi
    done
    return 1
}

query_mouse() {
    run_wine reg query "$KEY" 2>/dev/null | tr -d '\r'
}

value_of() {
    awk -v k="$2" '$1 == k { print $3; exit }' <<< "$1"
}

status() {
    local rc=0 out speed t1 t2 key val
    [[ -d "$PREFIX" ]] || { echo "  [--] 前缀不存在：$PREFIX"; return 1; }
    [[ -x "$WINE" ]] || { echo "  [--] wine 缺失：$WINE"; return 1; }

    out="$(query_mouse)" || rc=1
    for key in MouseSpeed MouseThreshold1 MouseThreshold2; do
        val="$(value_of "$out" "$key")"
        if [[ "$val" == "0" ]]; then
            echo "  [OK] $key = 0（指针加速已关闭）"
        else
            echo "  [--] $key = ${val:-<缺失>}（期望 0，表示指针加速仍开启）"
            rc=1
        fi
    done
    speed="$(value_of "$out" MouseSensitivity)"
    echo "  [ii] MouseSensitivity = ${speed:-<缺失>}（保持 Windows 默认 10，不改动）"
    return "$rc"
}

apply() {
    [[ -d "$PREFIX" ]] || die "前缀不存在：$PREFIX"
    [[ -x "$WINE" ]] || die "找不到 wine：$WINE（可设 STARCRAFT_PROTON_DIR）"
    if busy_pid="$(prefix_busy "$PREFIX")"; then
        die "该 Wine 前缀仍有实例运行（pid=$busy_pid），请先关闭星际争霸再修复"
    fi

    mkdir -p "$DATA"
    if [[ ! -s "$BACKUP" ]]; then
        run_wine reg export "$KEY" "$BACKUP" >/dev/null
        echo "  已备份：$KEY -> $BACKUP"
    else
        echo "  [=] 已有备份，保留：$BACKUP"
    fi

    for key in MouseSpeed MouseThreshold1 MouseThreshold2; do
        run_wine reg add "$KEY" /v "$key" /t REG_SZ /d 0 /f >/dev/null
    done
    echo "  已关闭该前缀的 Windows 指针加速（三项均设为 0）。"
    status
}

uninstall() {
    [[ -s "$BACKUP" ]] || die "无备份，拒绝还原：$BACKUP"
    if busy_pid="$(prefix_busy "$PREFIX")"; then
        die "该 Wine 前缀仍有实例运行（pid=$busy_pid），请先关闭星际争霸再还原"
    fi
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
