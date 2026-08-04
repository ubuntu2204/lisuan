#!/usr/bin/env bash
#
# fix-lisuan-gpu.sh — repair Lisuan (砺算) GPU DKMS module that won't load after a
# kernel upgrade on Ubuntu / Debian-based 信创 systems.
#
# Root cause it fixes:
#   The lisuangpu-dkms Makefile keys its build kernel off KERNEL_VERSION
#   (default `uname -r`), but dkms.conf passes the target kernel as KERNELVER.
#   The names differ, so DKMS builds the module against the *running* kernel and
#   files it under the *target* kernel tree -> wrong vermagic / symbol-CRC ->
#   the module refuses to load and the GPU has no driver bound.
#
# What it does (idempotent):
#   1. Patch the DKMS Makefile so it honors KERNELVER (durable: fixes future
#      kernel upgrades too). Keeps a .bak.
#   2. Rebuild + reinstall the module via DKMS for the target kernel.
#   3. depmod + modprobe, then print a verification report.
#
# Usage:
#   sudo ./fix-lisuan-gpu.sh                # fix the currently running kernel
#   sudo ./fix-lisuan-gpu.sh 7.0.0-28-generic   # fix a specific kernel
#
# License: Apache-2.0
set -euo pipefail

PKG="lisuangpu-dkms"
KVER="${1:-$(uname -r)}"
MODULE="gf"          # kernel module file name (gf.ko); DRM driver registers as "lisuan"
GPU_VENDOR="4c54"    # Lisuan ("LT")

log()  { printf '\033[1;32m==>\033[0m %s\n' "$*"; }
warn() { printf '\033[1;33m!!\033[0m %s\n'  "$*" >&2; }
die()  { printf '\033[1;31mXX\033[0m %s\n'  "$*" >&2; exit 1; }

[[ $EUID -eq 0 ]] || die "Must run as root:  sudo $0 ${1:-}"

# ---- locate the installed DKMS source & version -----------------------------
SRC_DIR="$(ls -d /usr/src/${PKG}-* 2>/dev/null | sort -V | tail -1 || true)"
[[ -n "$SRC_DIR" && -d "$SRC_DIR" ]] || die "$PKG source not found under /usr/src. Install the vendor lisuangpu-dkms package first (see README)."
VER="${SRC_DIR##*/${PKG}-}"
MK="${SRC_DIR}/Makefile"
[[ -f "$MK" ]] || die "Makefile not found: $MK"

log "Package      : ${PKG}/${VER}"
log "Target kernel: ${KVER}"
log "Source dir   : ${SRC_DIR}"

command -v dkms >/dev/null || die "dkms is not installed:  apt-get install dkms"
[[ -d "/lib/modules/${KVER}/build" ]] || die "Kernel headers for ${KVER} missing:  apt-get install linux-headers-${KVER}"

# ---- step 1: durable root-cause patch (honor KERNELVER) ----------------------
if grep -q 'KERNEL_VERSION := $(if $(KERNELVER)' "$MK"; then
  log "Makefile already honors KERNELVER — skipping patch."
else
  log "Patching ${MK} (backup -> ${MK}.bak)"
  cp -a "$MK" "${MK}.bak"
  perl -0pi -e \
    's/^KERNEL_VERSION := \$\(shell uname -r\)$/KERNEL_VERSION := \$(if \$(KERNELVER),\$(KERNELVER),\$(shell uname -r))/m' \
    "$MK"
  grep -q 'KERNEL_VERSION := $(if $(KERNELVER)' "$MK" \
    || die "Patch did not apply — inspect $MK manually (expected line: KERNEL_VERSION := \$(shell uname -r))."
  printf '    now: '; grep -n 'KERNEL_VERSION :=' "$MK"
fi

# ---- step 2: rebuild + reinstall through DKMS for the target kernel ----------
log "Removing stale DKMS build for ${KVER} (if any)"
dkms remove  -m "$PKG" -v "$VER" -k "$KVER" 2>/dev/null || true
log "Building against ${KVER} headers"
dkms build   -m "$PKG" -v "$VER" -k "$KVER"
log "Installing"
dkms install -m "$PKG" -v "$VER" -k "$KVER" --force
log "Refreshing module dependency data"
depmod -a "$KVER"

# ---- step 3: verify the freshly installed module -----------------------------
KO="$(find "/lib/modules/${KVER}" -name "${MODULE}.ko" -o -name "${MODULE}.ko.zst" 2>/dev/null | head -1)"
[[ -n "$KO" ]] || die "Built module not found in /lib/modules/${KVER} after install."
VMAGIC="$(modinfo "$KO" 2>/dev/null | awk '/^vermagic:/{print $2}')"
log "Installed: $KO"
log "vermagic : $VMAGIC  (expected: ${KVER})"
[[ "$VMAGIC" == "$KVER" ]] || warn "vermagic still mismatched — do NOT continue; re-check headers."

# ---- step 4: load (only meaningful when fixing the running kernel) -----------
if [[ "$KVER" == "$(uname -r)" ]]; then
  log "Loading module '${MODULE}'"
  modprobe "$MODULE" || warn "modprobe failed — inspect: dmesg | tail -30"
  echo
  echo "==================== RESULT ===================="
  lsmod | grep -E "^${MODULE} " || echo "(${MODULE} not loaded)"
  echo "-- GPU binding --"
  lspci -nnk | grep -iA3 "${GPU_VENDOR}:" || lspci -nnk | grep -iA3 'VGA compatible'
  echo "-- DRM nodes --"
  ls -l /dev/dri/ 2>/dev/null || echo "(no /dev/dri)"
  echo "================================================"
  echo "Success looks like:  'Kernel driver in use: lisuan'  +  /dev/dri/cardN present."
  echo "A reboot is recommended so the display stack starts cleanly."
else
  log "Built for non-running kernel ${KVER}. Reboot into it to load the driver."
fi
