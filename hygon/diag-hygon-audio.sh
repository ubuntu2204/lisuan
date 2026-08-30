#!/usr/bin/env bash
#
# diag-audio.sh — force snd_hda_intel onto 1d94:14c9 via driver_override, with
# snd-intel-dspcfg forced to native HDA (dsp_driver=1) BEFORE probe, and report
# whether probe succeeds. Run as root:  sudo ./diag-audio.sh
# License: Apache-2.0
set -uo pipefail
DRV=snd_hda_intel
DEV=07:00.6
SYSDEV=/sys/bus/pci/devices/0000:${DEV}

[[ $EUID -eq 0 ]] || { echo "run as root: sudo $0" >&2; exit 1; }

echo "=== fully unload audio stack ==="
modprobe -r "$DRV" snd_hda_codec snd_hda_core snd_intel_dspcfg snd_intel_sdw_acpi 2>/dev/null
sleep 1

echo "=== load snd-intel-dspcfg with dsp_driver=1 (native HDA, no SOF/SST) ==="
modprobe snd-intel-dspcfg dsp_driver=1 2>&1
echo "dsp_driver now: $(cat /sys/module/snd_intel_dspcfg/parameters/dsp_driver 2>/dev/null || echo '?')"

echo "=== load snd_hda_intel (pulls dspcfg as dep, already set) ==="
# probe_mask=0xff : probe all 8 codec addresses (default only probes addr 0-3)
# delay=2000      : give the codec up to 2s to power up before first command
modprobe "$DRV" probe_mask=0xff delay=2000 2>&1

echo "=== driver_override + bind ==="
BEFORE=$(dmesg | wc -l)
echo "$DRV" > "$SYSDEV/driver_override" 2>&1 && echo "driver_override set" || echo "driver_override FAILED"
OUT=$(echo "$DEV" > /sys/bus/pci/drivers/$DRV/bind 2>&1)
echo "bind result: '${OUT:-<none>}'"
ls -l "$SYSDEV/driver" 2>/dev/null && echo ">>> BOUND" || echo ">>> UNBOUND"
sleep 2

echo "=== dmesg DELTA ==="
dmesg | tail -n +$((BEFORE+1)) | tail -40

echo "=== aplay -l ==="
aplay -l 2>/dev/null || echo "(empty)"
