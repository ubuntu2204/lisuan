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
# Fix: create the expected symlinks so the loader can find the vendor driver.
# This is durable across reboots; redo it only if the vendor .deb is
# reinstalled (it overwrites the dri/ directory? no — it only ships gf_*.so,
# so the symlinks survive, but re-run is harmless / idempotent).
#
# Usage:
#   sudo ./fix-lisuan-mesa-driver-name.sh
#
# License: Apache-2.0
set -euo pipefail

[[ $EUID -eq 0 ]] || { echo "Must run as root:  sudo $0" >&2; exit 1; }

DRI64=/usr/lib/x86_64-linux-gnu/dri
DRI32=/usr/lib/i386-linux-gnu/dri

log()  { printf '\033[1;32m==>\033[0m %s\n' "$*"; }
warn() { printf '\033[1;33m!!\033[0m %s\n'  "$*" >&2; }

# ---- 64 位：lisuan_dri.so -> gf_dri.so -----------------------------------
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

Done. Verify with:
  glxinfo -B | grep -iE 'renderer|vendor'        # should show LISUAN, not llvmpipe
  MESA_LOADER_DRIVER_OVERRIDE=gf steam -no-cef-sandbox -cef-force-64bit

Notes:
  * This is independent of the KERNELVER DKMS fix (fix-lisuan-gpu.sh); apply both.
  * Re-running this script is safe (idempotent).
EOF
