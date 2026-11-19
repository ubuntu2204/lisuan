#!/usr/bin/env bash
# 至砺算：把 11 对战平台的魔兽争霸 III 路径指向本机真实安装目录。
#
# 依据（2026-11-19 厂商 PE 字符串取证）：bins/11Client.exe 与 W3L/core/War3Lobby.exe
# 都通过官方暴雪注册表键 Software\Blizzard Entertainment\Warcraft III 的 InstallPath
# 定位 War3（另有 AutoSearchWar3Enabled / saveWar3PathToClient 走同一路径）。
# 11 前缀默认没有该键，平台会提示找不到魔兽。本机 War3（1.24 冰封王座）原装在
# /opt/apps/war3/files（见 war3/），属主 root；而平台拉起前要在游戏目录建 YY\
# 子目录放补丁版 war3.exe（game.ini 的 PureWar3ExePath）并复制 war3shell.dll 等
# 注入件，root 目录不可写且本机无 sudo → 拉不起。故推荐把游戏复制到前缀内
# drive_c/war3（用户可写）再指 C:\war3\；E11_WAR3_DIR 在 drive_c 下自动映射 C:，
# 其余绝对路径映射 Z:。本脚本往 11 前缀注册表写 InstallPath / InstallPathX
# （资料片与本体同目录），HKCU 与 HKLM 的 32 位视图都写（客户端是 32 位 PE）。
# 幂等、备份、可还原。
#
# 只改 11 前缀内这两个注册表值，不动厂商二进制、不动 War3 安装目录。
# 平台客户端在跑时拒绝写入（退出时可能用内存里的旧值覆盖注册表）。
#
# 注意：平台拉起 War3 时继承平台进程环境；run-11duizhan.sh 已默认注入 32 位
# llvmpipe ICD（VK_ICD_FILENAMES，E11_VK_ICD 可覆盖），根因同 war3/README.md
# 的 gf 只有 64 位 Vulkan 驱动一节。
#
# 用法：
#   ./11/fix-11-war3path.sh              # 写入路径（幂等）
#   ./11/fix-11-war3path.sh --status     # 只读体检
#   ./11/fix-11-war3path.sh --uninstall  # 按备份还原（原无值则删除）
#   ./11/fix-11-war3path.sh --help
#
# 环境变量：E11_PROTON / E11_WINE11_PREFIX / E11_WAR3_DIR /
#   E11_FORCE=1（主进程已退出、仅剩阻塞在 anon_pipe_read 的 CEF 渲染残留时
#   强制写入；需先用 --status 与进程取证确认主进程不在）
# License: Apache-2.0
set -euo pipefail

PROTON="${E11_PROTON:-$HOME/.local/share/Steam/steamapps/common/Proton 11.0}"
PROTON="${PROTON/#\~/$HOME}"
PREFIX="${E11_WINE11_PREFIX:-$HOME/.deepinwine/11duizhan-263}"
PREFIX="${PREFIX/#\~/$HOME}"
GAME_DIR="${E11_WAR3_DIR:-/opt/apps/war3/files}"
GAME_DIR="${GAME_DIR%/}"
WINE="$PROTON/files/bin/wine"
KEY='Software\Blizzard Entertainment\Warcraft III'
BACKUP="$HOME/.local/share/11duizhan-wine11/war3path-backup.tsv"

die() { echo "错误：$*" >&2; exit 1; }

usage() {
    cat <<EOF
用法：$(basename "$0") [--status|--uninstall|--help]

  （无参数）   把 11 前缀注册表的 War3 InstallPath/InstallPathX 指向 $GAME_DIR
  --status     只读体检：注册表值、War3.exe、Z: 盘映射
  --uninstall  按备份还原原值；原来没有值则删除
  --help       显示本说明

环境变量：E11_PROTON、E11_WINE11_PREFIX、E11_WAR3_DIR、
          E11_FORCE=1（仅剩 CEF 渲染残留时强制写入，需先取证确认主进程已退出）
EOF
}

# 与 run-11duizhan.sh 相同的隔离环境，只在 wine 进程树上生效
wine_reg() {
    local libs="$PROTON/files/lib/x86_64-linux-gnu:$PROTON/files/lib/i386-linux-gnu"
    [[ -n "${LD_LIBRARY_PATH:-}" ]] && libs="$libs:$LD_LIBRARY_PATH"
    env WINEPREFIX="$PREFIX" \
        WINEDLLPATH="$PROTON/files/lib/vkd3d:$PROTON/files/lib/wine" \
        LD_LIBRARY_PATH="$libs" \
        WINEDLLOVERRIDES='mscoree,mshtml=' \
        WINEDEBUG="${E11_WINEDEBUG:--all}" \
        "$WINE" reg "$@"
}

# Linux 绝对路径 -> Wine 路径；前缀 drive_c 下映射 C:，其余映射 Z:，暴雪惯例带尾反斜杠
win_path() {
    local rel
    if [[ "$GAME_DIR" == "$PREFIX/drive_c/"* ]]; then
        rel="${GAME_DIR#"$PREFIX/drive_c/"}"
        printf 'C:\\%s\\' "${rel//\//\\}"
    else
        printf 'Z:%s\\' "${GAME_DIR//\//\\}"
    fi
}

# 读某个视图下某值的当前数据；不存在输出 "-"（wine reg 输出带 CRLF，先去掉 \r）
read_value() { # $1=hive(HKCU|HKLM) $2=value名 $3=额外reg参数(如 /reg:32)
    local out
    out="$(wine_reg query "$1\\$KEY" /v "$2" ${3:-} 2>/dev/null || true)"
    tr -d '\r' <<<"$out" | awk -v v="$2" '$1==v && $2=="REG_SZ" {s=""; for(i=3;i<=NF;i++) s=s (i>3?" ":"") $i; print s; found=1} END{if(!found) print "-"}'
}

# 平台客户端是否占用该前缀（排除 wineserver/系统服务进程；
# Wine 进程命令行会被清空，按 /proc/<pid>/environ 的前缀判断）
client_on_prefix() {
    local pid comm
    for pid in /proc/[0-9]*; do
        [[ -r "$pid/environ" ]] || continue
        ( tr '\0' '\n' < "$pid/environ" | grep -qxF "WINEPREFIX=$PREFIX" ) 2>/dev/null || continue
        comm="$(cat "$pid/comm" 2>/dev/null || true)"
        case "$comm" in
            wineserver|winedevice.exe|services.exe|plugplay.exe|rpcss.exe|svchost.exe|explorer.exe|tabtip.exe|winemenubuilder.exe) continue ;;
        esac
        printf '%s(%s)' "${pid#/proc/}" "$comm"
        return 0
    done
    return 1
}

check_game() {
    [[ -x "$WINE" ]] || die "找不到 Proton 11 的 wine：$WINE"
    [[ "$GAME_DIR" == /* ]] || die "E11_WAR3_DIR 必须是绝对路径：$GAME_DIR"
    [[ -s "$GAME_DIR/War3.exe" && "$(head -c2 "$GAME_DIR/War3.exe")" == "MZ" ]] || \
        die "War3.exe 缺失或不是有效 PE：$GAME_DIR/War3.exe"
    if [[ "$GAME_DIR" != "$PREFIX/drive_c/"* ]]; then
        [[ -L "$PREFIX/dosdevices/z:" && "$(readlink "$PREFIX/dosdevices/z:")" == "/" ]] || \
            die "前缀缺少 Z: -> / 映射：$PREFIX/dosdevices/z:"
    fi
}

status() {
    local rc=0 want v
    want="$(win_path)"
    [[ -x "$WINE" ]] && echo "  [OK] wine：$WINE" || { echo "  [--] wine 缺失：$WINE"; rc=1; }
    if [[ -s "$GAME_DIR/War3.exe" && "$(head -c2 "$GAME_DIR/War3.exe")" == "MZ" ]]; then
        echo "  [OK] War3 主程序：$GAME_DIR/War3.exe"
    else
        echo "  [--] War3 主程序缺失或不是 PE：$GAME_DIR/War3.exe"; rc=1
    fi
    for v in InstallPath InstallPathX; do
        local got
        got="$(read_value HKCU "$v" '')"
        if [[ "$got" == "$want" ]]; then
            echo "  [OK] HKCU\\$KEY $v = $got"
        else
            echo "  [--] HKCU\\$KEY $v = $got（期望 $want）"; rc=1
        fi
    done
    return "$rc"
}

apply() {
    check_game
    local busy
    if busy="$(client_on_prefix)"; then
        if [[ "${E11_FORCE:-0}" == "1" ]]; then
            echo "  [!!] E11_FORCE=1：忽略残留进程 $busy 强制写入（需已确认主进程退出）" >&2
        else
            die "11 平台进程正在该前缀上运行：$busy
请先退出平台再设置路径，否则平台退出时可能用旧值覆盖注册表。
若主进程已退出、仅剩 CEF 渲染残留（阻塞在 anon_pipe_read），取证后可用 E11_FORCE=1。"
        fi
    fi
    local want v prev
    want="$(win_path)"
    # 幂等：四个值都已就位时直接返回，不重写备份（保住“原无值”记录）
    local already=1
    for v in InstallPath InstallPathX; do
        [[ "$(read_value HKCU "$v" '')" == "$want" ]] || already=0
        [[ "$(read_value HKLM "$v" '/reg:32')" == "$want" ]] || already=0
    done
    if [[ "$already" == "1" ]]; then
        echo "  已是目标路径，无需重复写入：$want"
        status
        return 0
    fi
    mkdir -p "$(dirname "$BACKUP")"
    : > "$BACKUP"
    for v in InstallPath InstallPathX; do
        prev="$(read_value HKCU "$v" '')"
        printf 'HKCU\t%s\t%s\n' "$v" "$prev" >> "$BACKUP"
        prev="$(read_value HKLM "$v" '/reg:32')"
        printf 'HKLM32\t%s\t%s\n' "$v" "$prev" >> "$BACKUP"
    done
    echo "  已记录原值：$BACKUP"
    for v in InstallPath InstallPathX; do
        wine_reg add "HKCU\\$KEY" /v "$v" /t REG_SZ /d "$want" /f >/dev/null
        wine_reg add "HKLM\\$KEY" /v "$v" /t REG_SZ /d "$want" /f /reg:32 >/dev/null
    done
    echo "  已写入 War3 路径：$want"
    echo "  （HKCU 与 HKLM 32 位视图的 InstallPath / InstallPathX）"
    status
}

uninstall() {
    [[ -f "$BACKUP" ]] || die "没有备份记录：$BACKUP"
    local hive v data flag
    while IFS=$'\t' read -r hive v data; do
        [[ -n "$hive" ]] || continue
        flag=()
        [[ "$hive" == "HKLM32" ]] && { hive=HKLM; flag=(/reg:32); }
        if [[ "$data" == "-" ]]; then
            wine_reg delete "$hive\\$KEY" /v "$v" /f "${flag[@]}" >/dev/null 2>&1 || true
            echo "  已删除（原无值）：$hive\\$KEY $v"
        else
            wine_reg add "$hive\\$KEY" /v "$v" /t REG_SZ /d "$data" /f "${flag[@]}" >/dev/null
            echo "  已还原：$hive\\$KEY $v = $data"
        fi
    done < "$BACKUP"
    rm -f "$BACKUP"
}

case "${1:-}" in
    --help|-h) usage; exit 0 ;;
    --status) status; exit $? ;;
    --uninstall) uninstall; exit 0 ;;
    --*) die "未知参数：$1（用 --help 查看用法）" ;;
    *) apply ;;
esac