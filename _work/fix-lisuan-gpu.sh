#!/usr/bin/env bash
#
# Fix: Lisuan (砺算) 7G100 GPU [4c54:5006] not working on Ubuntu 24.04.4 / kernel 7.0.0-28-generic
#
# Root cause:
#   The lisuangpu-dkms Makefile keys its build kernel off KERNEL_VERSION (default `uname -r`),
#   but dkms.conf passes the *target* kernel as KERNELVER. The names don't match, so DKMS
#   silently built the module against whatever kernel was running at build time (6.8.0-31)
#   and filed it under the 7.0.0-28 tree. Result: vermagic/symbol-CRC mismatch, module
#   refuses to load ("disagrees about version of symbol ...", "Unknown symbol ..."), and the
#   GPU has no driver bound.
#
# This script:
#   1. Patches the root-owned DKMS Makefile so it honors KERNELVER (durable: fixes future
#      kernel upgrades too). Idempotent; keeps a .bak.
#   2. Rebuilds + reinstalls the module through DKMS for the running kernel.
#   3. Loads the module and verifies the GPU is bound.
#
# Run:  sudo bash fix-lisuan-gpu.sh
set -euo pipefail

PKG=lisuangpu-dkms
VER=262.2.0.56
KVER="$(uname -r)"
SRC="/usr/src/${PKG}-${VER}"
MK="${SRC}/Makefile"
GPU_BDF="0000:03:00.0"

echo "==> Target kernel : ${KVER}"
echo "==> DKMS source   : ${SRC}"

if [[ $EUID -ne 0 ]]; then
  echo "!! Must run as root:  sudo bash $0" >&2
  exit 1
fi

if [[ ! -f "${MK}" ]]; then
  echo "!! ${MK} not found. Is the ${PKG} package installed?" >&2
  exit 1
fi

# ---- Step 1: durable root-cause patch (make the Makefile honor KERNELVER) ----
if grep -q 'KERNEL_VERSION := $(if $(KERNELVER)' "${MK}"; then
  echo "==> Makefile already patched (honors KERNELVER). Skipping patch."
else
  echo "==> Patching ${MK} (backup -> Makefile.bak)"
  cp -a "${MK}" "${MK}.bak"
  # Replace the single line `KERNEL_VERSION := $(shell uname -r)` with a KERNELVER-aware form.
  perl -0pi -e 's/^KERNEL_VERSION := \$\(shell uname -r\)$/KERNEL_VERSION := \$(if \$(KERNELVER),\$(KERNELVER),\$(shell uname -r))/m' "${MK}"
  echo "    New line:"
  grep -n 'KERNEL_VERSION :=' "${MK}" | sed 's/^/      /'
fi

# ---- Step 2: rebuild + reinstall via DKMS for the running kernel ----
echo "==> Removing stale DKMS build for ${KVER} (if any)"
dkms remove  -m "${PKG}" -v "${VER}" -k "${KVER}" 2>/dev/null || true

echo "==> Building against ${KVER} headers"
dkms build   -m "${PKG}" -v "${VER}" -k "${KVER}"

echo "==> Installing"
dkms install -m "${PKG}" -v "${VER}" -k "${KVER}" --force

echo "==> Refreshing module dependencies"
depmod -a "${KVER}"

# ---- Step 3: sanity-check the freshly installed module ----
INSTALLED="/lib/modules/${KVER}/updates/dkms/gf.ko"
[[ -f "${INSTALLED}.zst" ]] && INSTALLED="${INSTALLED}.zst"
echo "==> Installed module vermagic:"
modinfo "${INSTALLED}" 2>/dev/null | grep -E 'filename|vermagic' | sed 's/^/      /'

# ---- Step 4: load the driver ----
echo "==> Loading module 'gf'"
modprobe gf

echo
echo "==================== RESULT ===================="
echo "-- lsmod --"
lsmod | grep -E '^gf ' || echo "   (gf not listed -- check: dmesg | tail)"
echo "-- GPU driver binding --"
lspci -nnk -s "${GPU_BDF}" || lspci -nnk | grep -iA3 'VGA compatible'
echo "-- DRM device nodes --"
ls -l /dev/dri/ 2>/dev/null || echo "   (no /dev/dri yet)"
echo "-- recent kernel messages --"
dmesg | grep -iE 'gf:|\[gf\]|lisuan|drm' | tail -15
echo "================================================"
echo "If 'Kernel driver in use: gf' appears above, the GPU is now driven."
echo "A reboot is recommended so the display stack starts cleanly on the fixed driver."
