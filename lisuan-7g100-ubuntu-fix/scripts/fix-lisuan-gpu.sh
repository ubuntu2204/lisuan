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

# ---------------- 常量与入参 ----------------
PKG="lisuangpu-dkms"            # 厂商 DKMS 驱动包名
KVER="${1:-$(uname -r)}"        # 目标内核：缺省修复当前运行内核，也可传参指定
MODULE="gf"          # kernel module file name (gf.ko); DRM driver registers as "lisuan"
GPU_VENDOR="4c54"    # Lisuan ("LT")，用于 lspci 定位显卡

log()  { printf '\033[1;32m==>\033[0m %s\n' "$*"; }
warn() { printf '\033[1;33m!!\033[0m %s\n'  "$*" >&2; }
die()  { printf '\033[1;31mXX\033[0m %s\n'  "$*" >&2; exit 1; }

# 打内核模块/改 /usr/src 均需 root
[[ $EUID -eq 0 ]] || die "Must run as root:  sudo $0 ${1:-}"

# ---- 定位已安装的 DKMS 源码与版本号 -----------------------------
# DKMS 包解包在 /usr/src/<包名>-<版本>/；同名多版本时取最新
SRC_DIR="$(ls -d /usr/src/${PKG}-* 2>/dev/null | sort -V | tail -1 || true)"
[[ -n "$SRC_DIR" && -d "$SRC_DIR" ]] || die "$PKG source not found under /usr/src. Install the vendor lisuangpu-dkms package first (see README)."
VER="${SRC_DIR##*/${PKG}-}"     # 从目录名截出版本号，如 262.2.0.56
MK="${SRC_DIR}/Makefile"
[[ -f "$MK" ]] || die "Makefile not found: $MK"

log "Package      : ${PKG}/${VER}"
log "Target kernel: ${KVER}"
log "Source dir   : ${SRC_DIR}"

command -v dkms >/dev/null || die "dkms is not installed:  apt-get install dkms"
# 编译模块必须有目标内核的头文件（/lib/modules/<kver>/build 软链）
[[ -d "/lib/modules/${KVER}/build" ]] || die "Kernel headers for ${KVER} missing:  apt-get install linux-headers-${KVER}"

# ---- 第 1 步：根因补丁（让 Makefile 认 KERNELVER）——永久生效 ----------
# 原厂 Makefile 只认 KERNEL_VERSION（缺省取 uname -r），忽略 dkms.conf 传入的
# KERNELVER，导致“拿运行内核的头文件编、归档到目标内核目录”。改为：
# 有 KERNELVER 用之，否则才回退 uname -r。补丁写入 /usr/src，今后每次
# 内核升级触发的 dkms autoinstall 都会正确构建。
if grep -q 'KERNEL_VERSION := $(if $(KERNELVER)' "$MK"; then
  log "Makefile already honors KERNELVER — skipping patch."
else
  log "Patching ${MK} (backup -> ${MK}.bak)"
  cp -a "$MK" "${MK}.bak"          # 备份原件，供撤销（见 README）
  # 用 perl 做整行替换（sed 处理 $( ) 转义繁琐，perl 更稳）
  perl -0pi -e \
    's/^KERNEL_VERSION := \$\(shell uname -r\)$/KERNEL_VERSION := \$(if \$(KERNELVER),\$(KERNELVER),\$(shell uname -r))/m' \
    "$MK"
  # 替换后必须能 grep 到新行，否则 Makefile 格式可能变了，宁死不继续
  grep -q 'KERNEL_VERSION := $(if $(KERNELVER)' "$MK" \
    || die "Patch did not apply — inspect $MK manually (expected line: KERNEL_VERSION := \$(shell uname -r))."
  printf '    now: '; grep -n 'KERNEL_VERSION :=' "$MK"
fi

# ---- 第 2 步：针对目标内核重编重装（remove 旧构建 → build → install）----
log "Removing stale DKMS build for ${KVER} (if any)"
dkms remove  -m "$PKG" -v "$VER" -k "$KVER" 2>/dev/null || true   # 无旧构建时忽略报错
dkms build   -m "$PKG" -v "$VER" -k "$KVER"                        # 用目标内核头文件编译
log "Installing"
dkms install -m "$PKG" -v "$VER" -k "$KVER" --force                # --force：覆盖同版本残留
log "Refreshing module dependency data"
depmod -a "$KVER"                                                   # 刷新 modules.dep，modprobe 才能找到

# ---- 第 3 步：验证新装模块的 vermagic 与目标内核一致 ----------------
KO="$(find "/lib/modules/${KVER}" -name "${MODULE}.ko" -o -name "${MODULE}.ko.zst" 2>/dev/null | head -1)"
[[ -n "$KO" ]] || die "Built module not found in /lib/modules/${KVER} after install."
VMAGIC="$(modinfo "$KO" 2>/dev/null | awk '/^vermagic:/{print $2}')"
log "Installed: $KO"
log "vermagic : $VMAGIC  (expected: ${KVER})"
# vermagic 不一致 = 补丁未生效或头文件错，此时加载必失败，提醒人工检查
[[ "$VMAGIC" == "$KVER" ]] || warn "vermagic still mismatched — do NOT continue; re-check headers."

# ---- 第 4 步：加载模块（仅当修复的就是当前运行内核时才有意义）-------
if [[ "$KVER" == "$(uname -r)" ]]; then
  log "Loading module '${MODULE}'"
  modprobe "$MODULE" || warn "modprobe failed — inspect: dmesg | tail -30"
  echo
  echo "==================== RESULT ===================="
  lsmod | grep -E "^${MODULE} " || echo "(${MODULE} not loaded)"
  echo "-- GPU binding --"    # 成功标志：Kernel driver in use: lisuan
  lspci -nnk | grep -iA3 "${GPU_VENDOR}:" || lspci -nnk | grep -iA3 'VGA compatible'
  echo "-- DRM nodes --"       # 成功标志：出现 cardN / renderD12x
  ls -l /dev/dri/ 2>/dev/null || echo "(no /dev/dri)"
  echo "================================================"
  echo "Success looks like:  'Kernel driver in use: lisuan'  +  /dev/dri/cardN present."
  echo "A reboot is recommended so the display stack starts cleanly."
else
  log "Built for non-running kernel ${KVER}. Reboot into it to load the driver."
fi
