#!/usr/bin/env bash
#
# fix-steam-cef-lock.sh — lock the CEF software-rendering patch in place so
# Steam's checksum verification cannot revert it.
#
# Steam overwrites steamwebhelper_sniper_wrap.sh on every launch ("Verifying
# all executable checksums"), undoing a plain edit. This script re-applies the
# CEF injection AND sets the immutable attribute (chattr +i) so the
# file cannot be overwritten. ext4 only.
#
# What gets injected (verified working config, 2026-08-05):
#   1. `export GBM_BACKEND=dri` — the REAL root cause: inside the
#      pressure-vessel container, system libgbm resolves DRM name "gf" to the
#      vendor backend gbm/gf_gbm.so (22.0.4 fork), which segfaults (jump to
#      NULL) under the newer libgbm ABI. Core dumps of the crashing processes
#      show the crash frames inside gf_gbm.so. Forcing the distro "dri"
#      backend sidesteps it (see docs/system-mesa-analysis.md §5 landmine #1).
#   2. `export LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libglapi_gf.so.0` —
#      Steam bundles its own ANGLE (libEGL.so/libGLESv2.so) in ubuntu12_64/,
#      which LD_LIBRARY_PATH=. puts first. That ANGLE lazily resolves GL API
#      symbols (glVertexAttribIPointer etc.) against the global scope; inside
#      the container no glapi provider is loaded yet, so ld.so aborts the GPU
#      process with "symbol lookup error ... (fatal)" -> SIGSEGV (seen as
#      exit_code=139/11). Preloading the vendor libglapi provides the symbols.
#      (Verified via LD_DEBUG=libs traces of the crashing gpu-process.)
#   3. Plain `exec ./steamwebhelper "$@"` — with 1+2 in place the GPU process
#      no longer crashes; it tries ANGLE EGL (fails cleanly: the container's
#      glvnd EGL dispatch never reaches the vendor EGL, and system mesa EGL
#      has no usable backend for this card — vendor EGL itself works fine on
#      the host), then falls back to software rendering. The UI works.
#      Full hardware compositing for the UI needs vendor-side container/EGL
#      dispatch support, and in-container GLX for native games needs a GLX
#      vendor registration (see docs/vendor-bug-report-gf-gbm-crash.md §3/§4).
#
# Note: sed -i recreates the file, so the executable bit is restored with
# chmod +x afterwards — without it pv-adverb fails with "Permission denied".
#
# Usage:
#   sudo ./fix-steam-cef-lock.sh           # apply + lock
#   sudo ./fix-steam-cef-lock.sh --unlock  # remove the lock (before Steam update)
#
# License: Apache-2.0
set -euo pipefail

[[ $EUID -eq 0 ]] || { echo "需要 root:  sudo $0 $*" >&2; exit 1; }

# Resolve the real user's home (sudo may set HOME=/root)
REAL_USER="${SUDO_USER:-$USER}"
REAL_HOME="$(getent passwd "$REAL_USER" | cut -d: -f6)"
WRAP="${REAL_HOME}/.local/share/Steam/ubuntu12_64/steamwebhelper_sniper_wrap.sh"

[[ -f "$WRAP" ]] || { echo "未找到: $WRAP — Steam 是否已安装？" >&2; exit 1; }

# Always unlock first (idempotent)
chattr -i "$WRAP" 2>/dev/null || true

if [[ "${1:-}" == "--unlock" ]]; then
  echo "==> 已解锁 $WRAP — Steam 更新可正常覆盖。更新后重新运行 sudo $0 重新注入。"
  exit 0
fi

# Inject GBM_BACKEND=dri + LD_PRELOAD glapi if not already present
MARK_OK=1
grep -q -- 'GBM_BACKEND=dri' "$WRAP" || MARK_OK=0
grep -q -- 'libglapi_gf' "$WRAP" || MARK_OK=0
if [[ "$MARK_OK" == "0" ]]; then
  cp -a "$WRAP" "${WRAP}.bak"
  sed -i 's|^exec \./steamwebhelper "\$@"$|export GBM_BACKEND=dri\nexport LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libglapi_gf.so.0\nexec ./steamwebhelper "$@"|' "$WRAP"
fi

# sed -i drops the executable bit; pv-adverb needs it to exec the script
chmod +x "$WRAP"

if ! grep -q -- 'GBM_BACKEND=dri' "$WRAP" || ! grep -q -- 'libglapi_gf' "$WRAP"; then
  echo "!! 注入失败 — 文件格式可能已变。请手动检查 $WRAP" >&2
  exit 1
fi

echo "==> 注入成功:"
grep -E 'GBM_BACKEND=dri|exec ./steamwebhelper' "$WRAP"

# Lock the file so Steam's verification cannot overwrite it
chattr +i "$WRAP"
echo "==> 已锁定 (chattr +i) — Steam 校验无法恢复原文件"

cat <<EOF

现在启动 Steam:
  MESA_LOADER_DRIVER_OVERRIDE=gf steam -no-cef-sandbox -cef-force-64bit

验证 CEF 是否还崩:
  grep -c 'exit_code=139' ~/.local/share/Steam/logs/cef_log.txt

Steam 更新前解锁:
  sudo $0 --unlock

更新后重新注入:
  sudo $0
EOF
