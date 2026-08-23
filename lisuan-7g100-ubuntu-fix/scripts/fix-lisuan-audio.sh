#!/usr/bin/env bash
#
# fix-lisuan-audio.sh — make Ubuntu's audio stack see the Lisuan (砺算) 7G100
# HDMI sound device instead of only "Dummy Output" / virtual sink.
#
# Root cause it fixes:
#   The 7G100 GPU (PCI 4c54:5006) exposes a companion HD-Audio function at
#   05:00.1 (PCI 4c54:5010). The vendor display driver (gf -> "lisuan") binds
#   the VGA function, but the audio function is left with NO driver bound:
#     - lspci shows "Memory at ... [disabled]" and "IRQ 255" (never enabled)
#     - `aplay -l` is empty  ->  PipeWire/WirePlumber only expose a virtual sink
#   Standard snd_hda_intel supports this controller, but 4c54:5010 is NOT in its
#   built-in PCI id table, so the kernel never auto-probes it.
#
# What it does (idempotent):
#   1. Loads snd_hda_intel and hot-binds 4c54:5010 via the driver's new_id
#      interface (enables the BAR, allocates a real IRQ, registers an ALSA card).
#   2. Drops a modprobe.d + systemd oneshot so the binding survives reboot
#      (the GPU audio function re-appears unbound after every boot).
#   3. Restarts PipeWire/WirePlumber so they pick up the new ALSA card.
#   4. Prints a verification report.
#
# Usage:
#   sudo ./fix-lisuan-audio.sh            # fix now + persist
#   sudo ./fix-lisuan-audio.sh --status   # only report, no changes
#   sudo ./fix-lisuan-audio.sh --uninstall
#
# License: Apache-2.0
set -euo pipefail

AUDIO_VID="4c54"
AUDIO_DID="5010"
DRV="snd_hda_intel"
MODCONF="/etc/modprobe.d/lisuan-audio.conf"
ONESHOT="/etc/systemd/system/lisuan-audio-bind.service"
STATUS=0; UNINSTALL=0
[[ "${1:-}" == "--status"   ]] && STATUS=1
[[ "${1:-}" == "--uninstall" ]] && UNINSTALL=1

log()  { printf '\033[1;32m==>\033[0m %s\n' "$*"; }
warn() { printf '\033[1;33m!!\033[0m %s\n'  "$*" >&2; }
die()  { printf '\033[1;31mXX\033[0m %s\n'  "$*" >&2; exit 1; }

[[ $EUID -eq 0 ]] || die "Must run as root:  sudo $0 ${1:-}"

# ---- 定位音频功能设备 (04 = multimedia audio controller) -------------------
DEV="$(lspci -n -d ${AUDIO_VID}:${AUDIO_DID} 2>/dev/null | head -1 | cut -d' ' -f1 || true)"
[[ -n "$DEV" ]] || { warn "7G100 audio function ${AUDIO_VID}:${AUDIO_DID} not found via lspci — is the GPU seated?"; [[ $STATUS -eq 1 || $UNINSTALL -eq 1 ]] && exit 0 || exit 1; }
SYSDEV="/sys/bus/pci/devices/0000:${DEV}"

# ---- 卸载分支 --------------------------------------------------------------
if [[ $UNINSTALL -eq 1 ]]; then
  log "Uninstalling Lisuan audio binding"
  systemctl disable --now lisuan-audio-bind.service 2>/dev/null || true
  rm -f "$ONESHOT"
  rm -f "$MODCONF"
  # 若已绑定，解绑并卸载
  if [[ -e "${SYSDEV}/driver" ]]; then
    DRVNAME="$(readlink -f "${SYSDEV}/driver" | xargs basename)"
    log "Unbinding ${DEV} from ${DRVNAME}"
    echo "${DEV}" > "/sys/bus/pci/drivers/${DRVNAME}/unbind" 2>/dev/null || true
  fi
  modprobe -r "$DRV" 2>/dev/null || true
  log "Removed. A reboot is recommended."
  exit 0
fi

# ---- 状态分支 --------------------------------------------------------------
report() {
  echo
  echo "==================== AUDIO STATUS ===================="
  echo "-- PCI audio function --"
  lspci -nnk -s "$DEV" 2>/dev/null || lspci -nn -s "$DEV"
  echo "-- ALSA cards (aplay -l) --"
  aplay -l 2>/dev/null || echo "(aplay not available)"
  echo "-- PipeWire sinks --"
  if command -v pw-cli >/dev/null; then
    pw-cli list-objects Node 2>/dev/null | grep -iE "node.name|device" | head -20 || true
  else
    echo "(pw-cli not available)"
  fi
  echo "======================================================="
}

if [[ $STATUS -eq 1 ]]; then
  report
  exit 0
fi

# ---- 修复：加载驱动并热绑定 4c54:5010 -------------------------------------
log "Audio function: ${DEV} (${AUDIO_VID}:${AUDIO_DID})"

# 若已被别的驱动绑定，先解绑
if [[ -e "${SYSDEV}/driver" ]]; then
  BOUND="$(readlink -f "${SYSDEV}/driver" | xargs basename)"
  if [[ "$BOUND" != "$DRV" ]]; then
    log "Device already bound to '${BOUND}' — unbinding first"
    echo "${DEV}" > "/sys/bus/pci/drivers/${BOUND}/unbind"
  else
    log "Already bound to ${DRV} — reloading to apply options"
  fi
fi

log "Loading ${DRV}"
modprobe "$DRV" || warn "modprobe ${DRV} failed — inspect: dmesg | tail -30"

# 驱动加载后才有 new_id 接口；写入 PCI id 触发 probe
if [[ -e "/sys/bus/pci/drivers/${DRV}/new_id" ]]; then
  log "Hot-binding ${AUDIO_VID}:${AUDIO_DID} to ${DRV}"
  # 已绑定时写 new_id 会报 EEXIST，忽略即可
  echo "${AUDIO_VID} ${AUDIO_DID}" > "/sys/bus/pci/drivers/${DRV}/new_id" 2>/dev/null \
    || warn "new_id write returned non-zero (often benign if already bound)"
else
  die "/sys/bus/pci/drivers/${DRV}/new_id missing — is ${DRV} really loaded?"
fi

# 若新绑定，强制 PCI 层启用设备资源（内核有时需要 rescan）
if [[ ! -e "${SYSDEV}/driver" ]]; then
  warn "Still no driver bound after new_id — forcing pci rescan"
  echo 1 > /sys/bus/pci/rescan 2>/dev/null || true
  echo "${AUDIO_VID} ${AUDIO_DID}" > "/sys/bus/pci/drivers/${DRV}/new_id" 2>/dev/null || true
fi

# ---- 持久化：开机自动绑定（每次启动音频功能都是 unbound 状态） -----------
log "Persisting binding across reboots"

# modprobe.d：强制加载 snd_hda_intel（并给通用 codec 兜底，避免某些固件
# 枚举不出具体 codec 型号时整卡哑掉）
cat > "$MODCONF" <<EOF
# Lisuan 7G100 (4c54:5010) HDMI audio — not in snd_hda_intel's built-in PCI
# id table, so bind it via new_id at load time. See lisuan-audio-bind.service.
install ${DRV} /sbin/modprobe --ignore-install ${DRV} ; \\
  echo ${AUDIO_VID} ${AUDIO_DID} > /sys/bus/pci/drivers/${DRV}/new_id 2>/dev/null ; true
options snd-hda-intel probe_mask=0xfff
EOF

# systemd oneshot：在显示驱动绑定 VGA 之后、音频服务之前，确保绑定完成
cat > "$ONESHOT" <<EOF
[Unit]
Description=Lisuan 7G100 HDMI audio (4c54:5010) HDA bind
After=local-fs.target
Before=pipewire.service pipewire-pulse.service

[Service]
Type=oneshot
RemainAfterExit=yes
ExecStart=/bin/sh -c 'modprobe ${DRV} 2>/dev/null; echo ${AUDIO_VID} ${AUDIO_DID} > /sys/bus/pci/drivers/${DRV}/new_id 2>/dev/null; true'

[Install]
WantedBy=multi-user.target
EOF

systemctl daemon-reload
systemctl enable --now lisuan-audio-bind.service 2>/dev/null \
  || warn "Could not enable systemd unit (non-fatal; manual modprobe still works)"

# ---- 重启用户态音频服务，使其发现新声卡 -----------------------------------
log "Restarting PipeWire / WirePlumber to pick up the new ALSA card"
for u in pipewire pipewire-pulse wireplumber; do
  systemctl --user restart "$u" 2>/dev/null \
    || systemctl restart "$u" 2>/dev/null \
    || true
done

# ---- 验证 -----------------------------------------------------------------
sleep 1
if [[ -e "${SYSDEV}/driver" ]]; then
  log "Driver bound: $(readlink -f "${SYSDEV}/driver" | xargs basename)"
else
  warn "Audio function STILL unbound — check dmesg for HDA codec probe errors"
fi

report
echo
echo "Success looks like:  'Kernel driver in use: ${DRV}'  +  a real card in 'aplay -l'"
echo "and a non-virtual sink in the Ubuntu sound settings (HDMI / 7G100)."
echo "If the card appears but is silent, re-check HDMI cable + sink profile in pavucontrol."
