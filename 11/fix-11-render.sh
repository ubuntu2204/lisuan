#!/usr/bin/env bash
# 至砺算：禁用 11 平台注入 War3 的自带渲染替换层（DxRender/DxWorker）。
#
# 根因取证（2026-11-19）：平台以 YY\war3.exe + 补丁 Game.dll 拉起 War3 时注入
# W3L/Patch/War3/gp_1290/ 的 War3Shell/DxRender/DxWorker/assocket 等 DLL；
# 该组合在 Wine 下挂住（进程存活、IPC 握手成功、无窗，主线程 futex 等待）。
# 对照：同 YY\war3.exe + 同 Game.dll 不注入 shell 单独启动 → 正常出窗（lvp ICD）。
# 禁用 DxRender/DxWorker 后平台拉起实测通过：20:47 出窗进入游戏、游玩约 18 分钟
# 后正常退出（21:05 gameexit），无崩溃 dump，用户确认「可以正常进魔兽争霸3」。
#
# 本脚本把这两个 DLL 改名为 .e11-disabled（改名即备份），让 shell 的
# LoadLibrary 落空，游戏回退原生 D3D8→DXVK(lvp) 渲染。这是**绕过**平台渲染
# 替换层，不是修好它；War3Shell/assocket/IPCProxyEx 仍注入，联机 IPC 保留。
# 若禁用后某功能受损，用 --uninstall 还原。只动本机前缀内副本；
# 平台下次拉起游戏时生效，无需重启平台。
#
# 用法：
#   ./11/fix-11-render.sh              # 禁用 DxRender/DxWorker（幂等）
#   ./11/fix-11-render.sh --status     # 只读体检
#   ./11/fix-11-render.sh --uninstall  # 还原改名
#   ./11/fix-11-render.sh --help
#
# 环境变量：E11_WINE11_PREFIX
# License: Apache-2.0
set -euo pipefail

PREFIX="${E11_WINE11_PREFIX:-$HOME/.deepinwine/11duizhan-263}"
PREFIX="${PREFIX/#\~/$HOME}"
DIR="$PREFIX/drive_c/11duizhan/W3L/Patch/War3/gp_1290"
TARGETS=(DxRender.dll DxWorker.dll)

die() { echo "错误：$*" >&2; exit 1; }

usage() {
    cat <<EOF
用法：$(basename "$0") [--status|--uninstall|--help]

  （无参数）   禁用 ${TARGETS[*]}（改名为 .e11-disabled，幂等）
  --status     只读体检：各 DLL 当前状态
  --uninstall  还原改名
  --help       显示本说明

环境变量：E11_WINE11_PREFIX
EOF
}

status() {
    local rc=0 f
    for f in "${TARGETS[@]}"; do
        if [[ -f "$DIR/$f" ]]; then
            echo "  [启用] $f（平台注入渲染层生效中）"
        elif [[ -f "$DIR/$f.e11-disabled" ]]; then
            echo "  [禁用] $f -> $f.e11-disabled"
        else
            echo "  [--] 缺失：$DIR/$f"; rc=1
        fi
    done
    return "$rc"
}

apply() {
    [[ -d "$DIR" ]] || die "目录不存在：$DIR"
    local f
    for f in "${TARGETS[@]}"; do
        if [[ -f "$DIR/$f" ]]; then
            mv "$DIR/$f" "$DIR/$f.e11-disabled"
            echo "  已禁用：$f"
        elif [[ -f "$DIR/$f.e11-disabled" ]]; then
            echo "  已是禁用态：$f"
        else
            die "缺失：$DIR/$f"
        fi
    done
    status
}

uninstall() {
    local f
    for f in "${TARGETS[@]}"; do
        if [[ -f "$DIR/$f.e11-disabled" ]]; then
            mv "$DIR/$f.e11-disabled" "$DIR/$f"
            echo "  已还原：$f"
        else
            echo "  无需还原：$f"
        fi
    done
    status
}

case "${1:-}" in
    --help|-h) usage; exit 0 ;;
    --status) status; exit $? ;;
    --uninstall) uninstall; exit 0 ;;
    --*) die "未知参数：$1（用 --help 查看用法）" ;;
    *) apply ;;
esac