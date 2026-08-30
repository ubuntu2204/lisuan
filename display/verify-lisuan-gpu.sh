#!/usr/bin/env bash
#
# verify-lisuan-gpu.sh — read-only health check for the Lisuan (砺算) GPU driver.
# Safe to run as a normal user (some lines show more detail under sudo).
#
# License: Apache-2.0
set -uo pipefail

KVER="$(uname -r)"    # 当前运行内核，所有检查都围绕“驱动与它匹配”展开
MODULE="gf"           # 内核模块文件名（gf.ko）；DRM 驱动注册名是 lisuan
ok()  { printf '  \033[1;32m[OK]\033[0m %s\n' "$*"; }
no()  { printf '  \033[1;31m[--]\033[0m %s\n' "$*"; }

# 基本信息：内核版本 + 发行版，供排查时对照
echo "== Environment =="
echo "  kernel : $KVER"
echo "  distro : $(. /etc/os-release 2>/dev/null; echo "${PRETTY_NAME:-unknown}")"

# PCI 层面：砺算卡（厂商号 4c54）是否可见、是否被驱动接管
echo "== GPU (PCI) =="
lspci -nnk | grep -iA3 -E '4c54:|VGA compatible|3D controller' || no "no VGA/3D device seen"

# 模块层面：gf 是否加载；未安装/错配时指向修复脚本
echo "== Kernel module =="
if lsmod | grep -qE "^${MODULE} "; then ok "'${MODULE}' loaded"; else no "'${MODULE}' not loaded"; fi
KO="$(find "/lib/modules/${KVER}" -name "${MODULE}.ko" -o -name "${MODULE}.ko.zst" 2>/dev/null | head -1)"
if [[ -n "$KO" ]]; then
  # vermagic 是本仓库修复的核心指标：必须与运行内核完全一致
  VM="$(modinfo "$KO" 2>/dev/null | awk '/^vermagic:/{print $2}')"
  if [[ "$VM" == "$KVER" ]]; then ok "vermagic matches ($VM)  -> $KO"
  else no "vermagic MISMATCH: module=$VM running=$KVER  -> run display/fix-lisuan-gpu.sh"; fi
else
  no "no ${MODULE}.ko installed for ${KVER}"
fi

# DRM 节点：cardN（primary）/renderD12x 存在才说明驱动已接管设备
echo "== DRM device nodes =="
if ls /dev/dri/card* >/dev/null 2>&1; then ls -l /dev/dri/ | sed 's/^/  /'; else no "no /dev/dri/cardN"; fi

# 持久补丁状态：/usr/src 下的 Makefile 是否已改为认 KERNELVER；
# 未打补丁时下次内核升级会再次黑屏
echo "== Makefile durable patch =="
MK="$(ls -d /usr/src/lisuangpu-dkms-*/Makefile 2>/dev/null | sort -V | tail -1 || true)"
if [[ -n "$MK" ]] && grep -q 'KERNEL_VERSION := $(if $(KERNELVER)' "$MK"; then
  ok "KERNELVER-aware ($MK)"
elif [[ -n "$MK" ]]; then
  no "NOT patched ($MK) — future kernel upgrades may break again"
else
  no "lisuangpu-dkms source not found under /usr/src"
fi

# 内核日志尾部：看 gf/lisuan/drm 相关的最近报错（无 sudo 时 dmesg 可能读不全）
echo "== Recent kernel messages (needs sudo for full dmesg) =="
( dmesg 2>/dev/null || true ) | grep -iE 'gf:|\[gf\]|lisuan|drm' | tail -8 | sed 's/^/  /' \
  || echo "  (run with sudo to read dmesg)"
