#!/usr/bin/env bash
# 至砺算：让 War3 的存档/录像/自定义地图目录对当前用户可写。
#
# 背景：War3 安装在 /opt/apps/war3/files，属主 root；游戏运行时会往
# Save/（进度、Profile）、Replay/（录像）、Maps/（自定义地图）写文件，
# 目录不可写时这些内容无法保存。本脚本只改这三个目录的属主，幂等可回滚。
#
# 崩溃处理：与启动无关，不涉及看门狗或自动重启。
#
# 用法：
#   ./war3/fix-war3-writable.sh              # 授权写权限（需要 sudo）
#   ./war3/fix-war3-writable.sh --status     # 只读体检
#   ./war3/fix-war3-writable.sh --uninstall  # 恢复原属主
#   ./war3/fix-war3-writable.sh --help
# License: Apache-2.0
set -euo pipefail

GAME_DIR="${WAR3_DIR:-/opt/apps/war3/files}"
GAME_DIR="${GAME_DIR%/}"
TARGETS=(Save Replay Maps)
BACKUP="$HOME/.local/share/war3-wine11/writable-backup.txt"

die() { echo "错误：$*" >&2; exit 1; }

usage() {
    cat <<EOF
用法：$(basename "$0") [--status|--uninstall|--help]

  （无参数）   把 ${TARGETS[*]} 目录属主改为当前用户（需要 sudo）
  --status     只读体检：目录属主与可写性
  --uninstall  按备份恢复原属主
  --help       显示本说明

环境变量：WAR3_DIR
EOF
}

status() {
    local rc=0 d p
    for d in "${TARGETS[@]}"; do
        p="$GAME_DIR/$d"
        if [[ ! -d "$p" ]]; then
            echo "  [--] 目录不存在：$p"
            continue
        fi
        if [[ -w "$p" ]]; then
            echo "  [OK] $d 可写（属主 $(stat -c '%U:%G' "$p")）"
        else
            echo "  [--] $d 不可写（属主 $(stat -c '%U:%G' "$p")）：$p"; rc=1
        fi
    done
    return "$rc"
}

apply() {
    local d p
    mkdir -p "$(dirname "$BACKUP")"
    : > "$BACKUP"
    for d in "${TARGETS[@]}"; do
        p="$GAME_DIR/$d"
        [[ -d "$p" ]] || continue
        printf '%s\t%s\t%s\n' "$d" "$(stat -c '%U' "$p")" "$(stat -c '%G' "$p")" >> "$BACKUP"
    done
    echo "  已记录原属主：$BACKUP"
    for d in "${TARGETS[@]}"; do
        p="$GAME_DIR/$d"
        [[ -d "$p" ]] || continue
        sudo chown -R "$(id -un):$(id -gn)" "$p"
        echo "  已授权：$p -> $(id -un):$(id -gn)"
    done
}

uninstall() {
    [[ -f "$BACKUP" ]] || die "没有备份记录：$BACKUP"
    local d owner group p
    while IFS=$'\t' read -r d owner group; do
        [[ -n "$d" ]] || continue
        p="$GAME_DIR/$d"
        [[ -d "$p" ]] || continue
        sudo chown -R "$owner":"$group" "$p"
        echo "  已恢复：$p -> $owner:$group"
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
