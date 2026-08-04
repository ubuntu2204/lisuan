#!/usr/bin/env bash
#
# verify-lisuan-gpu.sh — read-only health check for the Lisuan (砺算) GPU driver.
# Safe to run as a normal user (some lines show more detail under sudo).
#
# License: Apache-2.0
set -uo pipefail

KVER="$(uname -r)"
MODULE="gf"
ok()  { printf '  \033[1;32m[OK]\033[0m %s\n' "$*"; }
no()  { printf '  \033[1;31m[--]\033[0m %s\n' "$*"; }

echo "== Environment =="
echo "  kernel : $KVER"
echo "  distro : $(. /etc/os-release 2>/dev/null; echo "${PRETTY_NAME:-unknown}")"

echo "== GPU (PCI) =="
lspci -nnk | grep -iA3 -E '4c54:|VGA compatible|3D controller' || no "no VGA/3D device seen"

echo "== Kernel module =="
if lsmod | grep -qE "^${MODULE} "; then ok "'${MODULE}' loaded"; else no "'${MODULE}' not loaded"; fi
KO="$(find "/lib/modules/${KVER}" -name "${MODULE}.ko" -o -name "${MODULE}.ko.zst" 2>/dev/null | head -1)"
if [[ -n "$KO" ]]; then
  VM="$(modinfo "$KO" 2>/dev/null | awk '/^vermagic:/{print $2}')"
  if [[ "$VM" == "$KVER" ]]; then ok "vermagic matches ($VM)  -> $KO"
  else no "vermagic MISMATCH: module=$VM running=$KVER  -> run scripts/fix-lisuan-gpu.sh"; fi
else
  no "no ${MODULE}.ko installed for ${KVER}"
fi

echo "== DRM device nodes =="
if ls /dev/dri/card* >/dev/null 2>&1; then ls -l /dev/dri/ | sed 's/^/  /'; else no "no /dev/dri/cardN"; fi

echo "== Makefile durable patch =="
MK="$(ls -d /usr/src/lisuangpu-dkms-*/Makefile 2>/dev/null | sort -V | tail -1 || true)"
if [[ -n "$MK" ]] && grep -q 'KERNEL_VERSION := $(if $(KERNELVER)' "$MK"; then
  ok "KERNELVER-aware ($MK)"
elif [[ -n "$MK" ]]; then
  no "NOT patched ($MK) — future kernel upgrades may break again"
else
  no "lisuangpu-dkms source not found under /usr/src"
fi

echo "== Recent kernel messages (needs sudo for full dmesg) =="
( dmesg 2>/dev/null || true ) | grep -iE 'gf:|\[gf\]|lisuan|drm' | tail -8 | sed 's/^/  /' \
  || echo "  (run with sudo to read dmesg)"
