#!/usr/bin/env bash
# 至砺算：禁用 11 平台注入 War3 的自带渲染替换层（DxRender/DxWorker）。
#
# 根因取证（2026-11-19，2026-11-20 两次补充）：
# 平台以 YY\war3.exe + 补丁 Game.dll 拉起 War3 时，按房间/地图选择
# W3L/Patch/War3/gp_<版本>\ 下的 War3Shell/DxRender/DxWorker/assocket 注入。
# 该渲染替换层在 Wine 下挂住游戏（进程存活、IPC 握手成功、无窗，主线程
# futex 等待）。禁用后 2026-11-19 20:47 实测出窗、约 18 分钟后正常 gameexit。
#
# 11-20 19:04：只禁 gp_1290 后平台改用 gp_1292，拉起再次挂死。
# 11-20 19:18：脚本已把所有 gp_* 的 DLL 改名，但平台进房间时又把
# gp_1292/DxRender.dll 与 DxWorker.dll 还原（ctime 19:18:30，mtime 保持原值，
# 内容与 .e11-disabled 完全相同），War3Shell 再次 LoadLibrary 成功 → 挂死。
# 因此只用“改名/删文件”不可靠。本脚本现在**同时**把每个 gp_* 下本机副本
# War3Shell.dll 中的 UTF-16/ASCII 名称 DxRender.dll / DxWorker.dll 改成
# 同长度的 DxRendex.dll / DxWorkex.dll（不存在的文件），平台即使还原 DLL
# 本体，LoadLibrary 也会失败，游戏回退原生 D3D8→DXVK(lvp)。
#
# 这是**绕过**平台渲染替换层，不是修好它；War3Shell/assocket/IPCProxyEx
# 仍注入，联机 IPC 保留。只改本机前缀内的副本；每份 War3Shell.dll 第一次
# 修改前备份为 War3Shell.dll.e11-render-orig，--uninstall 全量还原。
# 不动 W3L/core 下的同名 DLL（平台自身使用）。无守护/看门狗/自动重启。
#
# 用法：
#   ./11/fix-11-render.sh              # 禁用所有 gp_* 渲染层（幂等）
#   ./11/fix-11-render.sh --status     # 只读体检
#   ./11/fix-11-render.sh --uninstall  # 还原改名与字符串补丁
#   ./11/fix-11-render.sh --help
#
# 环境变量：E11_WINE11_PREFIX
# License: Apache-2.0
set -euo pipefail

PREFIX="${E11_WINE11_PREFIX:-$HOME/.deepinwine/11duizhan-263}"
PREFIX="${PREFIX/#\~/$HOME}"
BASE="$PREFIX/drive_c/11duizhan/W3L/Patch/War3"
TARGETS=(DxRender.dll DxWorker.dll)
SHELL_NAME='War3Shell.dll'
SHELL_BAK='War3Shell.dll.e11-render-orig'
DISABLED_SUFFIX='.e11-disabled'

die() { echo "错误：$*" >&2; exit 1; }

usage() {
    cat <<EOF
用法：$(basename "$0") [--status|--uninstall|--help]

  （无参数）   禁用所有 W3L/Patch/War3/gp_*/ 下的 DxRender/DxWorker：
               改名为 .e11-disabled，并把 War3Shell.dll 里的 DLL 名称
               改成不存在的 DxRendex.dll / DxWorkex.dll（幂等）
  --status     只读体检：逐目录列出 DLL 状态与 War3Shell 补丁状态
  --uninstall  还原全部改名与字符串补丁
  --help       显示本说明

环境变量：E11_WINE11_PREFIX
EOF
}

patch_dirs() {
    [[ -d "$BASE" ]] || die "目录不存在：$BASE"
    find "$BASE" -maxdepth 1 -mindepth 1 -type d -name 'gp_*' 2>/dev/null | sort
}

# 输出：patched=1/0 changed=1/0；只改本机副本，第一次修改前备份。
patch_shell() {
    local f="$1"
    [[ -f "$f" ]] || { echo "0 0"; return 0; }
    python3 - "$f" "$SHELL_BAK" <<'PY'
import sys
from pathlib import Path
p = Path(sys.argv[1]); bak = p.with_name(sys.argv[2])
b = p.read_bytes()
pairs = [(b'DxRender.dll', b'DxRendex.dll'), (b'DxWorker.dll', b'DxWorkex.dll')]
changed = False
for old, new in pairs:
    for enc in ('utf-16le', 'ascii'):
        o, n = old.decode().encode(enc), new.decode().encode(enc)
        step = 2 if enc == 'utf-16le' else 1
        idx = 7  # 两个名字的第 8 个字符都是 r
        pos, starts = 0, []
        while True:
            i = b.find(o, pos)
            if i < 0: break
            starts.append(i); pos = i + 1
        for i in starts:
            at = i + idx * step
            if b[at:at+step] == o[idx*step:(idx+1)*step]:
                b = b[:at] + n[idx*step:(idx+1)*step] + b[at+step:]
                changed = True
patched = all(old.decode().encode('utf-16le') not in b for old, _ in pairs)
if changed:
    if not bak.exists():
        bak.write_bytes(Path(sys.argv[1]).read_bytes())
    p.write_bytes(b)
print(f'{1 if patched else 0} {1 if changed else 0}')
PY
}

shell_patched() {
    local f="$1/$SHELL_NAME"
    [[ -f "$f" ]] || return 1
    python3 - "$f" <<'PY'
import sys
from pathlib import Path
b = Path(sys.argv[1]).read_bytes()
unpatched = (b'DxRender.dll'.decode().encode('utf-16le') in b or
             b'DxWorker.dll'.decode().encode('utf-16le') in b)
sys.exit(1 if unpatched else 0)
PY
}

status() {
    local rc=0 d f state patched=0
    while IFS= read -r d; do
        [[ -n "$d" ]] || continue
        state=""
        for f in "${TARGETS[@]}"; do
            if [[ -f "$d/$f" ]]; then
                state="$state ${f%.dll}=存在"
            elif [[ -f "$d/$f$DISABLED_SUFFIX" ]]; then
                state="$state ${f%.dll}=已改名"
            else
                state="$state ${f%.dll}=缺失"
            fi
        done
        if shell_patched "$d"; then
            state="$state shell=已补丁"; patched=1
        else
            state="$state shell=未补丁"; patched=0; rc=1
        fi
        echo "  ${d##*/}:$state"
    done < <(patch_dirs)
    [[ "$patched" -eq 0 ]] && rc=1
    return "$rc"
}

apply() {
    local d f n=0 result
    while IFS= read -r d; do
        [[ -n "$d" ]] || continue
        for f in "${TARGETS[@]}"; do
            if [[ -f "$d/$f" ]]; then
                mv "$d/$f" "$d/$f$DISABLED_SUFFIX"
                echo "  已改名：${d##*/}/$f"
                n=$((n + 1))
            fi
        done
        result="$(patch_shell "$d/$SHELL_NAME")"
        if [[ "$(awk '{print $2}' <<<"$result")" == 1 ]]; then
            echo "  已补丁：${d##*/}/$SHELL_NAME（LoadLibrary 指向不存在的 DxRendex/DxWorkex）"
            n=$((n + 1))
        fi
    done < <(patch_dirs)
    echo "  本次变更 $n 项"
    status
}

uninstall() {
    local d f n=0
    while IFS= read -r d; do
        [[ -n "$d" ]] || continue
        for f in "${TARGETS[@]}"; do
            if [[ -f "$d/$f$DISABLED_SUFFIX" ]]; then
                mv "$d/$f$DISABLED_SUFFIX" "$d/$f"
                echo "  已还原改名：${d##*/}/$f"
                n=$((n + 1))
            fi
        done
        if [[ -f "$d/$SHELL_BAK" ]]; then
            cp -f "$d/$SHELL_BAK" "$d/$SHELL_NAME"
            rm -f "$d/$SHELL_BAK"
            echo "  已还原字符串：${d##*/}/$SHELL_NAME"
            n=$((n + 1))
        fi
    done < <(patch_dirs)
    echo "  本次还原 $n 项"
    status
}

case "${1:-}" in
    --help|-h) usage; exit 0 ;;
    --status) status; exit $? ;;
    --uninstall) uninstall; exit 0 ;;
    --*) die "未知参数：$1（用 --help 查看用法）" ;;
    *) apply ;;
esac
