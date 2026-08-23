#!/usr/bin/env bash
#
# fix-lisuan-audio.sh — make Ubuntu's audio stack see a REAL sound card instead
# of only "Dummy Output" / virtual sink on a 砺算 7G100 + 海光 (Hygon) machine.
#
# Root cause it fixes:
#   On this board Ubuntu shows only a virtual sink because NO ALSA sound card is
#   registered. Two audio PCI functions exist:
#     - 05:00.1  4c54:5010  Lisuan 7G100 GPU companion HD-Audio.
#                 This is a *transparent* codec driven entirely inside the vendor
#                 `gf` display driver (disp_codec.c / display_hda_handle / ELD /
#                 infoframe logic). It does NOT create an ALSA card and is NOT a
#                 standards-compliant Intel HD-A controller, so snd_hda_intel
#                 cannot bind to it — binding it is a dead end.
#     - 07:00.6  1d94:14c9  Hygon (海光) on-board HD-Audio controller.
#                 This IS a standard HD-A controller (Hygon derives from AMD
#                 Zen), but its vendor id 1d94 is missing from snd_hda_intel's
#                 built-in PCI id table, so the kernel never auto-probes it and
#                 the BAR stays disabled -> no /dev/snd/card* -> virtual output.
#
#   => The real fix is to hot-bind the Hygon HDA (1d94:14c9) to snd_hda_intel
#      via its new_id interface. (The 7G100 HDMI audio currently has no
#      kernel-side ALSA path and is out of scope for this script.)
#
# What it does (idempotent):
#   1. Loads snd_hda_intel and hot-binds 1d94:14c9 via the driver's new_id
#      interface (enables the BAR, allocates a real IRQ, registers an ALSA card).
#   2. Drops a modprobe.d + systemd oneshot so the binding survives reboot.
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

AUDIO_VID="1d94"
AUDIO_DID="14c9"
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
[[ -n "$DEV" ]] || { warn "Hygon HDA ${AUDIO_VID}:${AUDIO_DID} not found via lspci — is the board audio controller present?"; [[ $STATUS -eq 1 || $UNINSTALL -eq 1 ]] && exit 0 || exit 1; }
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
  # 清除 driver_override，避免重启后仍强制绑定
  [[ -e "${SYSDEV}/driver_override" ]] && echo "" > "${SYSDEV}/driver_override" 2>/dev/null || true
  modprobe -r "$DRV" snd_hda_codec snd_hda_core snd_intel_dspcfg snd_intel_sdw_acpi 2>/dev/null || true
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

# ---- 修复：加载驱动并热绑定海光 HDA (1d94:14c9) -------------------------
log "Audio function: ${DEV} (${AUDIO_VID}:${AUDIO_DID})"

# 若已被别的驱动绑定，先解绑
if [[ -e "${SYSDEV}/driver" ]]; then
  BOUND="$(readlink -f "${SYSDEV}/driver" | xargs basename)"
  if [[ "$BOUND" != "$DRV" ]]; then
    log "Device already bound to '${BOUND}' — unbinding first"
    echo "${DEV}" > "/sys/bus/pci/drivers/${BOUND}/unbind" 2>/dev/null || true
  else
    log "Already bound to ${DRV} — will reload to apply options"
  fi
fi

# 关键：模块若已在内存，其 modprobe options (dsp_driver=1 等) 不会被重读。
# 先干净卸载再加载，确保新 options 生效。
if [[ -n "$(lsmod | grep -o "^${DRV} ")" ]]; then
  log "Unloading ${DRV} (so new modprobe.d options take effect)"
  modprobe -r "$DRV" 2>/dev/null || true
  # 顺带卸掉依赖的 codec/core 模块
  modprobe -r snd_hda_codec snd_hda_core 2>/dev/null || true
fi

# 关键：snd_hda_intel 的 probe 内部会调用 snd_intel_dspcfg 决定最终用哪条
# 路径（原生 HDA=1 / SST=2 / SOF=3）。对未知 vendor 1d94，dspcfg 默认可能选
# SOF/SST 而本机无对应固件 -> azx_probe 静默失败（dmesg 无日志）。因此必须
# 在加载 snd_hda_intel 之前，先以 dsp_driver=1 加载 snd-intel-dspcfg。
log "Loading snd-intel-dspcfg with dsp_driver=1 (native HDA, no SOF/SST)"
modprobe snd-intel-dspcfg dsp_driver=1 2>/dev/null \
  || warn "snd-intel-dspcfg load/param failed — inspect dmesg"

log "Loading ${DRV} (probe_mask=0xff to find codec on any addr, delay=2000 for slow power-up)"
modprobe "$DRV" probe_mask=0xff delay=2000 \
  || warn "modprobe ${DRV} failed — inspect: dmesg | tail -30"

# 关键：1d94 不在 snd_hda_intel 的 PCI id 表里，new_id 对其返回 -EINVAL，
# 直接 bind 又会因 id 不匹配被 driver_probe_device 静默拒绝（dmesg 无日志）。
# 正确机制是 driver_override：写入驱动名后，内核忽略 id 表、强制用该驱动
# probe 此设备；probe 时内核自动 pci_enable_device，启用 [disabled] 的 BAR
# 并分配 IRQ。海光 Dhyana 源于 AMD Zen，HDA 控制器硬件与 snd_hda_intel 兼容。
if [[ -e "${SYSDEV}/driver_override" ]]; then
  log "Setting driver_override = ${DRV} (forces match, bypasses id table)"
  echo "${DRV}" > "${SYSDEV}/driver_override" 2>/dev/null \
    || warn "driver_override write failed"
else
  warn "No driver_override attr — falling back to new_id/bind"
  printf '%s %s\n' "$AUDIO_VID" "$AUDIO_DID" > "/sys/bus/pci/drivers/${DRV}/new_id" 2>/dev/null \
    || true
fi

if [[ ! -e "${SYSDEV}/driver" ]]; then
  log "Forcing bind ${DEV} -> ${DRV} (enables BAR/IRQ, runs probe)"
  ERR="$(echo "${DEV}" > "/sys/bus/pci/drivers/${DRV}/bind" 2>&1)"
  if [[ -e "${SYSDEV}/driver" ]]; then
    log "bind succeeded"
  else
    warn "bind failed: ${ERR:-<no message>}"
    warn "--- dmesg probe errors ---"
    dmesg 2>/dev/null | grep -iE "hda|azx|snd|codec|1d94|error|fail|invalid|enodev" | tail -25 \
      || echo "(dmesg not readable)"
  fi
else
  log "Device already bound"
fi

# ---- 持久化：开机自动绑定（每次启动音频功能都是 unbound 状态） -----------
log "Persisting binding across reboots"

# modprobe.d：强制加载 snd_hda_intel（并给通用 codec 兜底，避免某些固件
# 枚举不出具体 codec 型号时整卡哑掉）。
# 关键选项：
#   - dsp_driver=1  强制 snd_hda_intel 走原生 HDA 路径，避免 snd_intel_dspcfg
#     把未知 vendor (1d94) 默认选去 SOF/DSP 而本机无 SOF 固件导致 probe 失败。
#   - probe_mask=0xff  探测所有 codec 地址（默认只探 0-3）。
#   - delay=2000      给 codec (ALC222) 上电时间，否则 azx_get_response 超时。
cat > "$MODCONF" <<EOF
# Hygon (海光) on-board HD-Audio (1d94:14c9) — a standard HDA controller but
# its vendor id is absent from snd_hda_intel's built-in PCI id table, so bind it
# via driver_override (see lisuan-audio-bind.service); new_id is also tried as a
# best-effort fallback.
# (The Lisuan 7G100 GPU companion codec 4c54:5010 is a transparent codec inside
#  the vendor gf display driver and does NOT register an ALSA card — out of scope.)
install ${DRV} /sbin/modprobe --ignore-install ${DRV} probe_mask=0xff delay=2000 ; \\
  echo ${AUDIO_VID} ${AUDIO_DID} > /sys/bus/pci/drivers/${DRV}/new_id 2>/dev/null ; true
# dsp_driver=1 强制 snd_hda_intel 走原生 HDA 路径（snd-intel-dspcfg 的模块参数），
# 避免未知 vendor (1d94) 被默认选去 SOF/DSP 而本机无 SOF 固件导致 probe 失败。
options snd-intel-dspcfg dsp_driver=1
options snd-hda-intel probe_mask=0xff delay=2000
EOF

# systemd oneshot：在显示驱动绑定 VGA 之后、音频服务之前，确保绑定完成
cat > "$ONESHOT" <<EOF
[Unit]
Description=Hygon (海光) on-board HD-Audio (1d94:14c9) HDA bind
After=local-fs.target
Before=pipewire.service pipewire-pulse.service

[Service]
Type=oneshot
RemainAfterExit=yes
ExecStart=/bin/sh -c 'modprobe -r ${DRV} snd_intel_dspcfg 2>/dev/null; modprobe snd-intel-dspcfg dsp_driver=1 2>/dev/null; modprobe ${DRV} probe_mask=0xff delay=2000 2>/dev/null; echo ${DRV} > /sys/bus/pci/devices/0000:${DEV}/driver_override 2>/dev/null; echo ${AUDIO_VID} ${AUDIO_DID} > /sys/bus/pci/drivers/${DRV}/new_id 2>/dev/null; echo 0000:${DEV} > /sys/bus/pci/drivers/${DRV}/bind 2>/dev/null; true'

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
  warn "Audio function STILL unbound."
  warn "--- kernel probe log (most recent) ---"
  dmesg 2>/dev/null | grep -iE "hda|snd|codec|azx|1d94|audio" | tail -20 \
    || echo "(dmesg not readable without root / dmesg_restrict=1)"
  warn "--- device resources ---"
  lspci -v -s "$DEV" 2>/dev/null | grep -iE "Memory|IRQ|Control" | head
  warn "If 'Memory ... [disabled]' / 'IRQ 255': the function is not enabled by"
  warn "ACPI/firmware — binding a driver alone won't help; check BIOS/ACPI."
fi

report
echo
echo "Success looks like:  'Kernel driver in use: ${DRV}'  +  a real card in 'aplay -l'"
echo "and a non-virtual sink in the Ubuntu sound settings (Hygon on-board HDMI/analog)."
echo "If the card appears but is silent, re-check HDMI cable + sink profile in pavucontrol."
