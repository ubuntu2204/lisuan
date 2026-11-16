#!/usr/bin/env bash
# 至砺算：生成 KK 硬件渲染专用的 Proton 副本 "Proton - KK HW"。
#
# 为什么需要副本：gf 缺 VK_KHR_maintenance5，必须把 Proton 自带 DXVK v2.7.1
# 降级到 v2.3。直接改全局 Proton 11 会被 Steam 更新覆盖、并可能被校验为
# "内容文件损坏"；复制为自定义 Proton（Steam 不更新）即可避免。
#
# 幂等：已存在则只校验 DXVK/libSC 状态，不重复复制。
# 用法：bash kk/setup-kk-hw-proton.sh [--status]
set -euo pipefail

REPO="$(cd "$(dirname "$0")/.." && pwd)"
MM="$REPO/minion-masters"
BASE="$HOME/.local/share/Steam/steamapps/common"
SRC="$BASE/Proton 11.0"
DST="$BASE/Proton - KK HW"
DE="$DST/files/lib/wine/dxvk"

say() { echo "[kk-hw-proton] $*"; }
die() { echo "[kk-hw-proton] !! $*" >&2; exit 1; }

status() {
    echo "专用 Proton：$DST"
    [ -x "$DST/proton" ] && echo "  [OK] Proton 副本存在" || { echo "  [--] 副本不存在"; return 1; }
    echo "  DXVK: $(tail -1 "$DE/version" 2>/dev/null || echo 未知)"
    echo "  libSC: $(readlink "$DST/files/lib/x86_64-linux-gnu/libSC.so" 2>/dev/null || echo 未链接)"
    local so="$DST/files/lib/wine/i386-unix/winex11.so"
    local done_md5="89c544018d0df630d61260f3ca63ab11"
    local m; m=$(md5sum "$so" 2>/dev/null | awk '{print $1}')
    [ "$m" = "$done_md5" ] && echo "  offscreen 补丁: [OK] 已打（黑块修复）" || echo "  offscreen 补丁: [--] 未打/版本变化（跑一次 setup 应用）"
}

case "${1:-}" in
    --status) status; exit $?;;
    -h|--help)
        echo "用法：$(basename "$0") [--status]"; exit 0;;
esac

[ -x "$SRC/proton" ] || die "未安装 Proton 11.0：$SRC"
[ -f "$MM/dxvk23/version" ] || die "缺 DXVK v2.3 资产：$MM/dxvk23"
[ -f "$MM/gf-compat/lib/libSC.so" ] || die "缺 libSC.so：$MM/gf-compat/lib"

if [ ! -d "$DST" ]; then
    say "复制 Proton 11.0 -> Proton - KK HW（约 1.4G，一次性）"
    cp -a "$SRC" "$DST" || die "复制失败"
else
    say "专用 Proton 已存在，校验/修复 DXVK 与 libSC"
fi

say "降级 DXVK 到 v2.3（gf 缺 maintenance5，v2.7.1 不可用）"
chmod -R u+w "$DE" 2>/dev/null || true
for f in d3d9.dll d3d10core.dll d3d11.dll dxgi.dll; do
    cp -f "$MM/dxvk23/i386-windows/$f" "$DE/i386-windows/$f" || die "复制 32 位 $f 失败"
    cp -f "$MM/dxvk23/x86_64-windows/$f" "$DE/x86_64-windows/$f" || die "复制 64 位 $f 失败"
done
# version 文件是厂商 444 只读；先删再写（chmod 已放开目录写权限）
rm -f "$DE/version" && cp "$MM/dxvk23/version" "$DE/version" || die "写 version 失败"
say "  DXVK: $(tail -1 "$DE/version")"

say "链接 libSC.so（gf vkCreateDevice 依赖）"
ln -sfn "$MM/gf-compat/lib/libSC.so" "$DST/files/lib/x86_64-linux-gnu/libSC.so"
say "  libSC -> $(readlink "$DST/files/lib/x86_64-linux-gnu/libSC.so")"

# ---- winex11.so 补丁：消除大厅 web 黑块（2026-11-17 实测）----
# 根因：needs_offscreen_rendering() 把 KK 主框架客户面判为 offscreen=TRUE
# （分层子窗 SetParent 后 SYSRGN 复杂 → needs_client_window_clipping 返回
# TRUE），client surface 被 detach、渲染转投 offscreen pixmap 从不呈现 →
# 大厅 web 区纯黑。强制该函数恒返回 FALSE，client surface 直接绘到可见 X
# 窗口，实测活动弹窗/游戏 web 内容恢复显示。
# 只改专用 Proton 副本，不改全局。补丁点绑定 md5 白名单，版本变化即报错，
# 不盲目套用字节偏移。备份 .orig-nooffscreen，可用 --restore-offscreen 还原。
patch_winex11() {
    local so="$DST/files/lib/wine/i386-unix/winex11.so"
    local md5_orig="be7ff1a68ac598c5e1b63505edddde50"   # 未打补丁的本版 .so
    local md5_done="89c544018d0df630d61260f3ca63ab11"   # 已打补丁
    local off=$((0x1ddc2))                                # mov eax,1 (TRUE) 出口
    local m cur
    m=$(md5sum "$so" | awk '{print $1}')
    case "$m" in
        "$md5_done")
            say "  winex11.so 已打 offscreen 补丁"; return 0;;
        "$md5_orig")
            [ -f "$so.orig-nooffscreen" ] || cp -a "$so" "$so.orig-nooffscreen"
            chmod u+w "$so"
            cur=$(xxd -p -s "$off" -l 5 "$so")
            [ "$cur" = "b801000000" ] || die "补丁点字节非预期($cur)，拒绝修改"
            printf '\x31\xc0\x90\x90\x90' | dd of="$so" bs=1 seek="$off" conv=notrunc status=none
            say "  winex11.so 已打 offscreen 补丁（强制 needs_offscreen_rendering=FALSE）";;
        *)
            say "  !! winex11.so md5=$m 非已知版本，跳过补丁（版本升级后需重新定位补丁点）";;
    esac
}
case "${1:-}" in
    --restore-offscreen)
        SO="$DST/files/lib/wine/i386-unix/winex11.so"
        [ -f "$SO.orig-nooffscreen" ] && { cp -a "$SO.orig-nooffscreen" "$SO"; say "已还原 winex11.so"; } || say "无备份可还原"
        exit 0;;
esac
patch_winex11

status
say "完成。KK 硬件启动器会自动使用此专用 Proton。"
say "如需撤销 offscreen 补丁：bash $0 --restore-offscreen"
