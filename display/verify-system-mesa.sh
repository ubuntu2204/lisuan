#!/usr/bin/env bash
#
# verify-system-mesa.sh — report how the Lisuan (砺算) 7G100 is exposed to
# the SYSTEM Mesa stack on this machine, and flag known hazards.
#
# Checks:
#   1. kernel DRM driver name + DRI nodes
#   2. Mesa / glvnd stack versions and vendor configs
#   3. vendor stack components (DRI megadriver, GBM backend, Vulkan ICD)
#   4. GLX via system Mesa  -> must report the zink fallback renderer
#   5. GBM 'dri' backend    -> buffer object creation (requires libgbm-dev)
#
# This script only READS system state (plus running glxinfo / a small GBM
# probe); it changes nothing.
#
# Usage:   ./verify-system-mesa.sh
# License: Apache-2.0
set -uo pipefail

log()  { printf '\033[1;32m==>\033[0m %s\n' "$*"; }
warn() { printf '\033[1;33m!!\033[0m %s\n'  "$*" >&2; }
fail() { printf '\033[1;31mXX\033[0m %s\n'  "$*" >&2; }

RC=0   # 总结退出码：任一项 fail 置 1，便于脚本化判断

# --- 1. 内核侧：DRM 驱动注册名与设备节点 -------------------------------
# DRIVER= 就是 Mesa 用来拼 DRI 驱动文件名的名字（本卡是 lisuan，
# 与用户态驱动名 gf 不一致，靠 fix-lisuan-mesa-driver-name.sh 建软链解决）
log "Kernel DRM driver"
for c in /sys/class/drm/card[0-9]; do
  drv=$(grep '^DRIVER=' "$c/device/uevent" 2>/dev/null | cut -d= -f2)
  [[ -n "$drv" ]] && echo "  $c -> DRIVER=$drv"
done
ls /dev/dri 2>/dev/null | sed 's/^/  node: /'
lsmod | awk '$1=="gf"{print "  module gf loaded (refcount "$3")"}'

# --- 2. Mesa 栈：系统包版本、glvnd 厂商配置、Vulkan ICD -------------------
log "Mesa stack"
dpkg-query -W -f '  ${Package} ${Version}\n' libgbm1 libgl1-mesa-dri mesa-libgallium 2>/dev/null
for j in /usr/share/glvnd/egl_vendor.d/*.json; do echo "  EGL vendor: $j"; done
# zink 路径的前提：厂商 Vulkan ICD 的 JSON 必须在位
[[ -f /usr/share/vulkan/icd.d/gf_icd.x86_64.json ]] \
  && echo "  Vulkan ICD: gf_icd.x86_64.json present" \
  || warn "no gf Vulkan ICD — zink path cannot work"

# --- 3. 厂商组件清点：DRI megadriver / GBM 后端 / pipe 驱动 / Vulkan ICD ---
log "Vendor components"
for f in /usr/lib/x86_64-linux-gnu/dri/gf_dri.so \
         /usr/lib/x86_64-linux-gnu/gbm/gf_gbm.so \
         /usr/lib/x86_64-linux-gnu/gallium-pipe/pipe_gf.so \
         /usr/lib/x86_64-linux-gnu/libvulkan_gf.so; do
  [[ -f $f ]] && echo "  present: $f" || warn "missing: $f"
done
# 地雷提示：gf_gbm.so 与新 libgbm ABI 不兼容（见 bug 报告 §2）
echo "  NOTE: gf_gbm.so is a Mesa 22.0.4-era backend; under libgbm >= 24.x it"
echo "        SEGFAULTS on primary nodes (card*) — see docs/system-mesa-analysis.md"

# --- 4. 系统 Mesa 的 GLX：预期自动回退到 zink（GL-on-Vulkan）------------
# 厂商 Mesa 22.0.4 分叉无法被 25.x 加载，zink 是系统 Mesa 唯一可用路径；
# 落到 llvmpipe 说明 Vulkan ICD 不可用
log "GLX on system Mesa (expect: zink on LISUAN Vulkan)"
if command -v glxinfo >/dev/null 2>&1; then
  # 强制走 mesa GLX（缺省 glvnd 会优先厂商栈）
  rend=$(__GLX_VENDOR_LIBRARY_NAME=mesa glxinfo -B 2>/dev/null | awk -F': ' '/^OpenGL renderer/{print $2; exit}')
  echo "  renderer: ${rend:-<none>}"
  if [[ "$rend" == *zink*LISUAN* ]]; then
    echo "  OK: system Mesa renders through zink on the 7G100 Vulkan ICD"
  elif [[ "$rend" == *llvmpipe* ]]; then
    fail "fell back to software rendering — check the Vulkan ICD"
    RC=1
  else
    warn "unexpected renderer: $rend"
  fi
else
  warn "glxinfo not installed (mesa-utils); skipping"
fi

# --- 5. GBM dri 后端实测：现场编译一个小探针验证能否建 buffer object -----
# 这是 Steam 容器规避方案（GBM_BACKEND=dri）可行性的直接证据
log "GBM 'dri' backend (buffer creation)"
if [[ -f /usr/include/gbm.h ]] && command -v cc >/dev/null 2>&1; then
  tmpd=$(mktemp -d)
  trap 'rm -rf "$tmpd"' EXIT    # 退出时清理临时目录
  cat > "$tmpd/probe.c" <<'EOF'
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <glob.h>
#include <gbm.h>
int main(void){
  glob_t g; const char *node = NULL;
  /* Prefer a PRIMARY node: the dri backend allocates dumb buffers, which
     render nodes refuse (master-only ioctl). Safe to touch card* here
     because the runner forces GBM_BACKEND=dri, so the broken vendor
     gf_gbm backend is never loaded. */
  if (glob("/dev/dri/card*", 0, NULL, &g) == 0 && g.gl_pathc > 0) node = g.gl_pathv[0];
  if (!node) node = "/dev/dri/renderD128";
  int fd = open(node, O_RDWR);
  if (fd < 0) { perror("open"); return 4; }
  struct gbm_device *d = gbm_create_device(fd);
  if (!d) { fprintf(stderr, "gbm_create_device failed\n"); return 2; }
  struct gbm_bo *bo = gbm_bo_create(d, 64, 64, GBM_FORMAT_XRGB8888, GBM_BO_USE_RENDERING);
  printf("  node=%s backend=%s bo=%s\n", node, gbm_device_get_backend_name(d), bo?"OK":"FAIL");
  return bo ? 0 : 3;
}
EOF
  if cc -O2 -o "$tmpd/probe" "$tmpd/probe.c" -lgbm 2>/dev/null; then
    # GBM_BACKEND=dri 避开厂商 gf_gbm 后端（详见探针源码内注释）
    if GBM_BACKEND=dri "$tmpd/probe"; then
      echo "  OK"
    else
      warn "GBM dri probe failed (check /dev/dri permissions: user must be in the video/render group)"
    fi
  else
    warn "could not compile GBM probe"
  fi
else
  warn "libgbm-dev or cc missing; skipping GBM probe"
fi

log "Done"
exit "$RC"
