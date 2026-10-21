#!/usr/bin/env bash
#
# fix-lisuan-hdmi-audio.sh — 砺算 7G100 显卡 HDMI/DP 音频修复（幂等）
#
# 症状：
#   * 声音设置里只有"模拟输出"，看不到显卡的 HDMI/DP 输出；
#   * `aplay -l` 只有板载 ALC222，没有 "HDMI 0"；
#   * `lspci -nnk -s 05:00.1` 里 `4c54:5010` 后面没有 "Kernel driver in use"。
#
# 根因（2026-10-21 本机实测，逐条有证据）：
#   1) `05:00.1` 是**标准 HD-Audio 控制器**，不是"厂商透明 codec"：
#      BAR0 16 KiB，GCAP=0x4001（64 位地址、4 个输出流）、VMIN/VMAJ=0/1，
#      INTCTL/INTSTS/CORB/RIRB/流描述符全部是按 HDA 规范排布的寄存器；
#      其后的 codec 是标准 HDMI codec（Vendor Id 0x4c545020，4 组
#      converter+pin），`snd_hda_codec_hdmi` 无需任何 quirk 即可驱动。
#   2) 厂商号 `4c54` 不在 `snd_hda_intel` 的静态 PCI id 表里，内核永不 probe；
#      本内核（7.0.0-38-generic）上向 `new_id` 写入直接返回 EIO，
#      只能用 `driver_override` 绕过 id 表强制匹配。
#   3) 该功能的 **MSI 是坏的**：绑定后 MSI 一条也进不来
#      （/proc/interrupts 里计数恒为 0），INTSTS 的 CIS/GIS 一直置位，
#      内核刷 "Spurious interrupt (vector 0xef)"，PCM 一开就卡住、不出声。
#      把 `msi_bus` 置 0 改用 INTx（ACPI _PRT 把 INB 路由到 GSI 31）后，
#      中断计数正常增长、播放正常结束。
#
# 修复：
#   安装 `lisuan-hdmi-audio-bind.service` 开机自绑（driver_override +
#   msi_bus=0 + bind），并立即执行一次。海光板载声卡（07:00.6）的既有
#   修复不受影响。
#
# Usage:
#   sudo ./fix-lisuan-hdmi-audio.sh              # 安装/修复（立即生效 + 开机自绑）
#   ./fix-lisuan-hdmi-audio.sh --status          # 查看状态（普通用户即可）
#   sudo ./fix-lisuan-hdmi-audio.sh --uninstall  # 撤销（解绑、恢复 msi_bus、删服务）
#
# License: Apache-2.0
set -euo pipefail

UNIT_NAME="lisuan-hdmi-audio-bind.service"
UNIT_PATH="/etc/systemd/system/${UNIT_NAME}"
HELPER="/usr/local/sbin/lisuan-hdmi-audio-bind"
LISUAN_VENDOR="0x4c54"   # 砺算 PCI 厂商号（ASCII "LT"）
AUDIO_CLASS="0x040300"   # PCI class: Multimedia audio controller

log()  { printf '\033[1;32m==>\033[0m %s\n' "$*"; }
warn() { printf '\033[1;33m!!\033[0m %s\n'  "$*" >&2; }
die()  { printf '\033[1;31mXX\033[0m %s\n'  "$*" >&2; exit 1; }

# 读 sysfs 属性并去掉换行；文件不存在时返回空串（不触发 set -e）
read_attr() {
  [[ -r "$1" ]] || return 0
  tr -d '\n' < "$1"
}

# 列出所有"砺算 + 音频类"的 PCI 功能（通常只有一个：05:00.1）
find_audio_bdfs() {
  local d
  for d in /sys/bus/pci/devices/*; do
    [[ -r "$d/vendor" && -r "$d/class" ]] || continue
    [[ "$(read_attr "$d/vendor")" == "$LISUAN_VENDOR" ]] || continue
    [[ "$(read_attr "$d/class")" == "$AUDIO_CLASS" ]] || continue
    printf '%s\n' "${d##*/}"
  done
}

# 该 PCI 功能当前绑定的驱动名（未绑定则输出空串）
bdf_driver() {
  local link="/sys/bus/pci/devices/$1/driver"
  [[ -L "$link" ]] || return 0
  basename "$(readlink -f "$link")"
}

# 该 PCI 功能对应的 ALSA 卡号（没有声卡则输出空串）
bdf_card() {
  local s card=""
  for s in "/sys/bus/pci/devices/$1"/sound/card*; do
    [[ -e "$s" ]] || continue
    card="${s##*card}"
  done
  printf '%s' "$card"
}

# 等待 ALSA 卡出现：$1=BDF $2=超时秒数
wait_card() {
  local bdf="$1" timeout="$2" i card
  for ((i = 0; i < timeout * 10; i++)); do
    card="$(bdf_card "$bdf")"
    if [[ -n "$card" ]]; then
      printf '%s' "$card"
      return 0
    fi
    sleep 0.1
  done
  return 0
}

# 从 /proc/asound/cardN/eld#* 里挑一个有效的 ELD 做摘要
eld_summary() {
  local card="$1" f name conn ch
  for f in "/proc/asound/card${card}"/eld#*; do
    [[ -r "$f" ]] || continue
    grep -q '^eld_valid[[:space:]]*1' "$f" 2>/dev/null || continue
    name="$(awk '$1 == "monitor_name" { sub(/^[^ \t]+[ \t]+/, ""); print; exit }' "$f")"
    conn="$(awk '$1 == "connection_type" { print $2; exit }' "$f")"
    ch="$(awk '$1 == "sad0_channels" { print $2; exit }' "$f")"
    printf '%s / %s / %s 声道' "${name:-未知显示器}" "${conn:-未知}" "${ch:-?}"
    return 0
  done
  printf '无有效 ELD（显示器未连接或无音频能力）'
}

status() {
  local bdfs bdf card drv msi
  echo "== 砺算 7G100 HDMI/DP 音频状态 =="
  bdfs="$(find_audio_bdfs)"
  if [[ -z "$bdfs" ]]; then
    echo "  [--] 未找到砺算伴生音频功能（vendor ${LISUAN_VENDOR} / class ${AUDIO_CLASS}）"
    return 0
  fi
  for bdf in $bdfs; do
    drv="$(bdf_driver "$bdf")"
    card="$(bdf_card "$bdf")"
    msi="$(read_attr "/sys/bus/pci/devices/${bdf}/msi_bus")"
    printf '  PCI 功能   : %s [%s:%s]\n' \
      "$bdf" "$(read_attr "/sys/bus/pci/devices/${bdf}/vendor")" \
      "$(read_attr "/sys/bus/pci/devices/${bdf}/device")"
    if [[ "$drv" == "snd_hda_intel" ]]; then
      printf '  驱动       : %s（已绑定）\n' "$drv"
    elif [[ -n "$drv" ]]; then
      printf '  驱动       : %s（非 snd_hda_intel，本脚本不会抢占）\n' "$drv"
    else
      printf '  驱动       : 未绑定（HDMI/DP 音频不可用）\n'
    fi
    if [[ "$msi" == "0" ]]; then
      printf '  中断       : msi_bus=0 → INTx（该功能 MSI 不可用，必须走 INTx）\n'
    else
      printf '  中断       : msi_bus=%s → MSI（该功能 MSI 不可用，会静默丢中断）\n' \
        "${msi:-未知}"
    fi
    if [[ -n "$card" ]]; then
      printf '  ALSA 卡    : card%s\n' "$card"
      printf '  显示器 ELD : %s\n' "$(eld_summary "$card")"
      if [[ -e "/proc/asound/card${card}/pcm3p" ]]; then
        printf '  HDMI 0     : 就绪（hw:%s,3）\n' "$card"
      else
        printf '  HDMI 0     : 缺失（pcm3p 不存在）\n'
      fi
    else
      printf '  ALSA 卡    : 无（声音设置里不会出现 HDMI/DP 输出）\n'
    fi
  done
  if systemctl is-enabled --quiet "$UNIT_NAME" 2>/dev/null; then
    printf '  开机自绑   : 已启用（%s）\n' \
      "$(systemctl is-active "$UNIT_NAME" 2>/dev/null || true)"
  elif [[ -f "$UNIT_PATH" ]]; then
    printf '  开机自绑   : 已安装但未启用（systemctl enable %s）\n' "$UNIT_NAME"
  else
    printf '  开机自绑   : 未安装\n'
  fi
}

# ---- 开机服务调用的自绑脚本（安装到 /usr/local/sbin） ----------------------
write_helper() {
  cat > "$HELPER" <<'HELPER_EOF'
#!/bin/sh
# lisuan-hdmi-audio-bind — 把砺算 7G100 伴生 HD-Audio 功能（4c54:5010）
# 绑到 snd_hda_intel 上，并强制使用 INTx。
#
# 由 audio/fix-lisuan-hdmi-audio.sh 安装，lisuan-hdmi-audio-bind.service
# 开机调用；也可以随时手工执行（幂等）。
set -eu

VENDOR=0x4c54
CLASS=0x040300
DRIVER=/sys/bus/pci/drivers/snd_hda_intel

say() { printf 'lisuan-hdmi-audio: %s\n' "$*"; }

find_bdfs() {
    for d in /sys/bus/pci/devices/*; do
        [ -r "$d/vendor" ] || continue
        [ "$(cat "$d/vendor")" = "$VENDOR" ] || continue
        [ -r "$d/class" ] || continue
        [ "$(cat "$d/class")" = "$CLASS" ] || continue
        printf '%s\n' "${d##*/}"
    done
}

bdfs=$(find_bdfs)
if [ -z "$bdfs" ]; then
    say "no Lisuan (4c54) HD-Audio function found — nothing to do"
    exit 0
fi

# 板载声卡的既有修复通常已经加载了 snd_hda_intel；这里兜底。
modprobe snd_hda_intel 2>/dev/null || true
[ -d "$DRIVER" ] || { say "snd_hda_intel driver directory missing"; exit 1; }

for bdf in $bdfs; do
    dev=/sys/bus/pci/devices/$bdf
    drv=
    if [ -L "$dev/driver" ]; then
        drv=$(basename "$(readlink -f "$dev/driver")")
    fi

    if [ -n "$drv" ] && [ "$drv" != snd_hda_intel ]; then
        say "$bdf is owned by $drv — skip (vendor audio driver takes precedence)"
        continue
    fi

    msi_bus=1
    if [ -r "$dev/msi_bus" ]; then
        msi_bus=$(cat "$dev/msi_bus")
    fi
    if [ "$drv" = snd_hda_intel ] && [ "$msi_bus" = 0 ]; then
        say "$bdf already bound to snd_hda_intel with INTx"
        continue
    fi

    # 先解绑再改 msi_bus：驱动已按 MSI 初始化过，必须重新 probe 才会走 INTx。
    if [ "$drv" = snd_hda_intel ]; then
        if ! printf '%s\n' "$bdf" > "$DRIVER/unbind" 2>/dev/null; then
            say "$bdf: unbind failed"
            exit 1
        fi
    fi

    # 厂商号不在 azx_ids 里，new_id 在本内核返回 EIO；driver_override 绕过 id 表。
    printf 'snd_hda_intel\n' > "$dev/driver_override"
    printf '0\n' > "$dev/msi_bus"
    if ! printf '%s\n' "$bdf" > "$DRIVER/bind"; then
        say "$bdf: bind failed"
        printf '\n' > "$dev/driver_override" || true
        exit 1
    fi
    say "$bdf bound to snd_hda_intel with INTx (msi_bus=0)"
done
HELPER_EOF
  chmod 0755 "$HELPER"
}

write_unit() {
  cat > "$UNIT_PATH" <<UNIT_EOF
[Unit]
Description=Lisuan 7G100 HDMI/DP audio (4c54:5010) bind to snd_hda_intel
# 海光板载声卡的服务会在启动时 modprobe -r snd_hda_intel 再重新加载，
# 必须排在它之后，否则绑定会被那次卸载冲掉。
After=systemd-udevd.service hygon-audio-bind.service
Before=graphical.target

[Service]
Type=oneshot
RemainAfterExit=yes
ExecStart=${HELPER}

[Install]
WantedBy=multi-user.target
UNIT_EOF
  chmod 0644 "$UNIT_PATH"
}

install_fix() {
  local bdfs bdf card
  [[ $EUID -eq 0 ]] || die "需要 root：sudo $0"
  bdfs="$(find_audio_bdfs)"
  [[ -n "$bdfs" ]] || die "未找到砺算伴生音频功能（vendor ${LISUAN_VENDOR} / class ${AUDIO_CLASS}），本机可能不是 7G100 或显卡未上电"

  log "安装自绑脚本：$HELPER"
  write_helper
  log "安装开机服务：$UNIT_PATH"
  write_unit
  systemctl daemon-reload
  systemctl enable "$UNIT_NAME" >/dev/null 2>&1 || warn "enable 失败（服务已写入，可手动 systemctl enable $UNIT_NAME）"
  log "立即执行绑定（driver_override + msi_bus=0 + bind）"
  systemctl restart "$UNIT_NAME"

  for bdf in $bdfs; do
    card="$(wait_card "$bdf" 10)"
    if [[ -z "$card" ]]; then
      warn "$bdf 绑定后未出现 ALSA 卡"
      journalctl -u "$UNIT_NAME" -n 20 --no-pager 2>/dev/null || true
      die "修复失败：请把上面的服务日志与 \`dmesg | tail -30\` 一并反馈"
    fi
    log "$bdf → card${card}；ELD：$(eld_summary "$card")"
    if [[ -e "/proc/asound/card${card}/pcm3p" ]]; then
      log "HDMI 0 PCM 就绪：hw:${card},3"
    else
      warn "card${card} 未见 pcm3p（HDMI 0），请检查 /proc/asound/card${card}/"
    fi
  done

  cat <<'DONE'

完成。在「设置 → 声音 → 输出」里选择 "HDA Intel 数字立体声 (HDMI)"
（或 pavucontrol / wpctl 里的 HDMI/DP 输出）即可。

自检：
  audio/fix-lisuan-hdmi-audio.sh --status
  speaker-test -D hw:<card>,3 -c 2 -r 48000 -t sine -l 1
DONE
}

uninstall_fix() {
  local bdf
  [[ $EUID -eq 0 ]] || die "需要 root：sudo $0 --uninstall"
  if systemctl list-unit-files "$UNIT_NAME" >/dev/null 2>&1; then
    systemctl disable --now "$UNIT_NAME" >/dev/null 2>&1 || true
  fi
  rm -f "$UNIT_PATH" "$HELPER"
  systemctl daemon-reload
  for bdf in $(find_audio_bdfs); do
    if [[ "$(bdf_driver "$bdf")" == "snd_hda_intel" ]]; then
      printf '%s\n' "$bdf" > "/sys/bus/pci/drivers/snd_hda_intel/unbind" 2>/dev/null || true
    fi
    [[ -w "/sys/bus/pci/devices/${bdf}/msi_bus" ]] \
      && printf '1\n' > "/sys/bus/pci/devices/${bdf}/msi_bus" || true
    [[ -w "/sys/bus/pci/devices/${bdf}/driver_override" ]] \
      && printf '\n' > "/sys/bus/pci/devices/${bdf}/driver_override" || true
  done
  log "已撤销 HDMI/DP 音频修复（海光板载声卡不受影响）"
}

case "${1:-}" in
  ""|--install) install_fix ;;
  --status)     status ;;
  --uninstall)  uninstall_fix ;;
  -h|--help)
    sed -n '2,40p' "$0" | sed 's/^# \{0,1\}//'
    ;;
  *)
    echo "未知参数：$1（可用：--status / --uninstall / --help）" >&2
    exit 2
    ;;
esac
