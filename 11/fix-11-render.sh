#!/usr/bin/env bash
# 至砺算：禁用 11 平台注入 War3 的自带渲染替换层（DxRender/DxWorker）。
#
# 根因取证（2026-11-19，2026-11-20 补充）：
# 平台以 YY\war3.exe + 补丁 Game.dll 拉起 War3 时，会按地图/版本选择
# W3L/Patch/War3/gp_<版本>\ 下的 War3Shell/DxRender/DxWorker/assocket 注入。
# 该渲染替换层在 Wine 下挂住游戏（进程存活、IPC 握手成功、无窗，主线程
# futex 等待）。对照：同 YY\war3.exe + 同 Game.dll 不注入 shell 单独启动
# 正常出窗。禁用 DxRender/DxWorker 后平台拉起实测通过（2026-11-19 20:47
# 出窗、约 18 分钟后正常 gameexit，无 dump，用户确认可正常进魔兽3）。
#
# 重要（2026-11-20）：平台会按房间/地图切换 gp_ 版本。11-19 只禁用了
# gp_1290，11-20 19:04 的失败拉起实际使用 gp_1292（Assocket 日志
# war3shell path=...\gp_1292，DxWorker.dll 日志 dxrender init），
# window-check 10 秒 not-found；故必须禁用 **所有** gp_* 下的这两个 DLL。
# run-11duizhan.sh 每次启动都会调用本脚本，新下载的 gp_* 也会覆盖。
#
# 本脚本把 DLL 改名为 .e11-disabled（改名即备份），让 shell 的 LoadLibrary
# 落空，游戏回退原生 D3D8→DXVK(lvp) 渲染。这是**绕过**平台渲染替换层，
# 不是修好它；War3Shell/assocket/IPCProxyEx 仍注入，联机 IPC 保留。
# 不动 W3L/core 下的同名 DLL（平台自身使用），只改 Patch/War3/gp_*。
# 若禁用后某功能受损，用 --uninstall 还原。只动本机前缀内副本。
#
# 用法：
#   ./11/fix-11-render.sh              # 禁用所有 gp_* 的 DxRender/DxWorker（幂等）
#   ./11/fix-11-render.sh --status     # 只读体检
#   ./11/fix-11-render.sh --uninstall  # 还原所有改名
#   ./11/fix-11-render.sh --help
#
# 环境变量：E11_WINE11_PREFIX
# License: Apache-2.0
set -euo pipefail

PREFIX="${E11_WINE11_PREFIX:-$HOME/.deepinwine/11duizhan-263}"
PREFIX="${PREFIX/#\~/$HOME}"
BASE="$PREFIX/drive_c/11duizhan/W3L/Patch/War3"
TARGETS=(DxRender.dll DxWorker.dll)

die() { echo "错误：$*" >&2; exit 1; }

usage() {
    cat <<EOF
用法：$(basename "$0") [--status|--uninstall|--help]

  （无参数）   禁用所有 W3L/Patch/War3/gp_*/DxRender.dll 与 DxWorker.dll
                （改名为 .e11-disabled，幂等）
  --status     只读体检：逐目录列出启用/禁用状态
  --uninstall  还原所有改名
  --help       显示本说明

环境变量：E11_WINE11_PREFIX
EOF
}

patch_dirs() {
    [[ -d "$BASE" ]] || die "目录不存在：$BASE"
    find "$BASE" -maxdepth 1 -mindepth 1 -type d -name 'gp_*' 2>/dev/null | sort
}

status() {
    local rc=0 d f n=0 state
    while IFS= read -r d; do
        [[ -n "$d" ]] || continue
        state=""
        for f in "${TARGETS[@]}"; do
            if [[ -f "$d/$f" ]]; then
                state="$state ${f%.dll}=启用"; rc=1
            elif [[ -f "$d/$f.e11-disabled" ]]; then
                state="$state ${f%.dll}=禁用"
            else
                state="$state ${f%.dll}=缺失"; rc=1
            fi
        done
        echo "  ${d##*/}:$state"
        n=$((n + 1))
    done < <(patch_dirs)
    [[ "$n" -gt 0 ]] || { echo "  [--] 未找到任何 gp_* 补丁目录：$BASE"; return 1; }
    return "$rc"
}

apply() {
    local d f n=0
    while IFS= read -r d; do
        [[ -n "$d" ]] || continue
        for f in "${TARGETS[@]}"; do
            if [[ -f "$d/$f" ]]; then
                mv "$d/$f" "$d/$f.e11-disabled"
                echo "  已禁用：${d##*/}/$f"
                n=$((n + 1))
            elif [[ -f "$d/$f.e11-disabled" ]]; then
                :
            else
                echo "  警告：缺失 ${d##*/}/$f（跳过）" >&2
            fi
        done
    done < <(patch_dirs)
    echo "  本次新禁用 $n 个 DLL"
    status
}

uninstall() {
    local d f n=0
    while IFS= read -r d; do
        [[ -n "$d" ]] || continue
        for f in "${TARGETS[@]}"; do
            if [[ -f "$d/$f.e11-disabled" ]]; then
                mv "$d/$f.e11-disabled" "$d/$f"
                echo "  已还原：${d##*/}/$f"
                n=$((n + 1))
            fi
        done
    done < <(patch_dirs)
    echo "  本次还原 $n 个 DLL"
    status
}

case "${1:-}" in
    --help|-h) usage; exit 0 ;;
    --status) status; exit $? ;;
    --uninstall) uninstall; exit 0 ;;
    --*) die "未知参数：$1（用 --help 查看用法）" ;;
    *) apply ;;
esac
