#!/usr/bin/env bash
#
# fix-lisuan-mesa-driver-name.sh — fix the Mesa DRI driver-name mismatch that
# breaks GPU acceleration (and Steam) on Lisuan (砺算) 7G100.
#
# Root cause this fixes:
#   The Lisuan kernel DRM driver registers under the name "lisuan"
#   (/sys/class/drm/cardN/device/uevent -> DRIVER=lisuan), and that is what
#   `drmGetVersion()` reports. Mesa's DRI loader then looks for a user-space
#   driver named  lisuan_dri.so.  But the vendor ships it as  gf_dri.so
#   (its internal descriptor is "LISUAN"). The names differ, so every GL/EGL
#   client gets  driver=(null)  -> Steam's CEF GPU process segfaults
#   (exit_code=139, "GPU process isn't usable. Goodbye.").
#
# 重要（2026-08-24 实测）: 在 /usr/lib/x86_64-linux-gnu/dri/ 建全局
#   lisuan_dri.so -> gf_dri.so 软链会让 **Steam 客户端自身** 的 EGL 加载到
#   gf 的 22.0.4 fork 驱动, 在无头/XWayland 下触发
#   `double free or corruption` + `Aborted (core dumped)` -> Steam 客户端崩 ->
#   steamid=0 -> 所有游戏拿不到 ticket 退出。详见 README 七/§2 与 docs/steam-on-lisuan.md §2。
#   因此默认 **不再建全局软链**; 仅当确有原生 Linux 游戏(如刀塔霸业)需要 gf GL 时,
#   才用 --native-gl 显式开启, 且要知道这会让 Steam 客户端崩溃(需先在图形
#   界面登录好 Steam 再启动游戏, 不要依赖本机 Steam 自动登录)。
#
# Fix: create the expected symlinks so the loader can find the vendor driver.
# This is durable across reboots; redo it only if the vendor .deb is
# reinstalled (it overwrites the dri/ directory? no — it only ships gf_*.so,
# so the symlinks survive, but re-run is harmless / idempotent).
#
# Usage:
#   sudo ./fix-lisuan-mesa-driver-name.sh            # 默认: 不建全局软链(避免 Steam 崩)
#   sudo ./fix-lisuan-mesa-driver-name.sh --native-gl # 仅原生游戏需要 gf GL 时开启
#
# License: Apache-2.0
set -euo pipefail

[[ $EUID -eq 0 ]] || { echo "Must run as root:  sudo $0" >&2; exit 1; }

DRI64=/usr/lib/x86_64-linux-gnu/dri
DRI32=/usr/lib/i386-linux-gnu/dri

log()  { printf '\033[1;32m==>\033[0m %s\n' "$*"; }
warn() { printf '\033[1;33m!!\033[0m %s\n'  "$*" >&2; }

# ---- 默认不建全局软链 (避免 Steam 客户端 double free) ----------------------
if [[ "${1:-}" != "--native-gl" ]]; then
  # 若存在旧软链 (之前版本建的), 这里顺手清理, 保证 Steam 客户端稳定
  if [[ -L "$DRI64/lisuan_dri.so" || -L "$DRI64/lisuan_drv_video.so" ]]; then
    rm -f "$DRI64/lisuan_dri.so" "$DRI64/lisuan_drv_video.so"
    log "removed legacy lisuan_*.so symlinks (they crash Steam client via gf fork)"
  fi
  log "默认模式: 不建全局软链 (避免 Steam 客户端 double free)。"
  log "需要原生 Linux 游戏用 gf GL 时请加 --native-gl (并注意其会使 Steam 客户端崩溃)。"
  ldconfig 2>/dev/null || true
  exit 0
fi

# ---- 仅 --native-gl: 建 64 位软链 (原生游戏需要, 但会害 Steam 客户端) -----
# Mesa 的 DRI 加载器按 drmGetVersion() 返回的名字（lisuan）找驱动文件，
# 而厂商只提供 gf_dri.so；建同名软链即可，不需要拷贝/改二进制。
if [[ -f "$DRI64/gf_dri.so" ]]; then
  ln -sf gf_dri.so        "$DRI64/lisuan_dri.so"   # -f：重复执行时覆盖旧链（幂等）
  log "linked $DRI64/lisuan_dri.so -> gf_dri.so"
else
  warn "gf_dri.so not found in $DRI64 — install lisuangpu-lib first."
fi
# VA-API 视频驱动同理：libva 按驱动名找 <name>_drv_video.so
if [[ -f "$DRI64/gf_drv_video.so" ]]; then
  ln -sf gf_drv_video.so  "$DRI64/lisuan_drv_video.so"
  log "linked $DRI64/lisuan_drv_video.so -> gf_drv_video.so"
fi

# ---- 32 位：厂商未提供 32 位 gf 驱动，无链可建 ----------------------------
# Steam 的 CEF 是 64 位，不受影响；32 位 GL 应用会落到 swrast
if [[ -f "$DRI32/gf_dri.so" ]]; then
  ln -sf gf_dri.so        "$DRI32/lisuan_dri.so"
  log "linked $DRI32/lisuan_dri.so -> gf_dri.so"
else
  warn "No 32-bit gf_dri.so in $DRI32 (vendor only ships 64-bit)."
  warn "32-bit GL clients will fall back to swrast; Steam's 64-bit CEF is unaffected."
fi

# ---- 刷新动态链接缓存（软链本身不强制需要，无害）
ldconfig 2>/dev/null || true

# --- 验证：打印新建的软链 -------------------------------------------------
log "Result:"
ls -l "$DRI64"/lisuan_*.so 2>/dev/null || true

cat <<'EOF'

Done (--native-gl). Warning: 全局软链会让 Steam 客户端 double free 崩溃,
请务必先在图形界面登录好 Steam 再启动原生游戏。随从大师(Proton)无需此软链。

Verify with:
  glxinfo -B | grep -iE 'renderer|vendor'        # should show LISUAN, not llvmpipe

Notes:
  * This is independent of the KERNELVER DKMS fix (fix-lisuan-gpu.sh); apply both.
  * 默认模式(无参数)不建软链, 避免 Steam 客户端崩溃; re-run is safe.
EOF

