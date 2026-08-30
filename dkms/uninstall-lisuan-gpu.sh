#!/usr/bin/env bash
#
# uninstall-lisuan-gpu.sh — undo the Makefile patch applied by
# fix-lisuan-gpu.sh (restore the vendor-original KERNEL_VERSION line).
#
# 重要说明（先看清楚再执行）：
#   * 撤销补丁 ≠ 卸载显卡驱动：已装好的 gf.ko 继续工作，显卡不受影响；
#   * 真正后果是**下次内核升级**时退回原厂 Bug 行为（按运行内核而非
#     目标内核编译），很可能再次黑屏。除非确有需要，不建议执行。
#
# 还原方式（自动选择）：
#   1. 有 Makefile.bak（fix 脚本打的备份）→ 直接从备份还原（推荐）；
#   2. 无备份但检测到补丁行 → 反向替换回原厂行；
#   3. 两者皆无 → 说明本来就是原厂状态，不做任何事。
#
# Usage:   sudo ./uninstall-lisuan-gpu.sh
# License: Apache-2.0
set -euo pipefail

PKG="lisuangpu-dkms"
PATCHED='KERNEL_VERSION := $(if $(KERNELVER)'     # 补丁后的特征行
ORIGINAL='KERNEL_VERSION := $(shell uname -r)'     # 原厂行

log()  { printf '\033[1;32m==>\033[0m %s\n' "$*"; }
warn() { printf '\033[1;33m!!\033[0m %s\n'  "$*" >&2; }
die()  { printf '\033[1;31mXX\033[0m %s\n'  "$*" >&2; exit 1; }

# 改 /usr/src 需要 root
[[ $EUID -eq 0 ]] || die "Must run as root:  sudo $0"

# ---- 定位 DKMS 源码目录（与 fix-lisuan-gpu.sh 相同的规则） ---------------
SRC_DIR="$(ls -d /usr/src/${PKG}-* 2>/dev/null | sort -V | tail -1 || true)"
[[ -n "$SRC_DIR" && -d "$SRC_DIR" ]] || die "$PKG source not found under /usr/src — nothing to revert."
MK="${SRC_DIR}/Makefile"
[[ -f "$MK" ]] || die "Makefile not found: $MK"
log "Target: $MK"

if [[ -f "${MK}.bak" ]]; then
  # 方式一：备份还原最可靠——完整回到打补丁前的文件内容
  log "Restoring from backup ${MK}.bak"
  cp -a "${MK}.bak" "$MK"
elif grep -q "$PATCHED" "$MK"; then
  # 方式二：无备份时做反向替换（与 fix 脚本的 perl 替换互逆）
  log "No backup found — reverse-patching in place"
  cp -a "$MK" "${MK}.uninstall.bak"    # 反向操作前也留一份，防手滑
  perl -0pi -e \
    's/^KERNEL_VERSION := \$\(if \$\(KERNELVER\),\$\(KERNELVER\),\$\(shell uname -r\)\)$/KERNEL_VERSION := \$(shell uname -r)/m' \
    "$MK"
else
  # 方式三：既无补丁行也无备份，本来就是原厂状态
  log "Makefile is already vendor-original — nothing to do."
  exit 0
fi

# ---- 验证：必须回到原厂行，且补丁特征行消失 ---------------------------------
if grep -q "$ORIGINAL" "$MK" && ! grep -q "$PATCHED" "$MK"; then
  log "Reverted OK:"
  printf '    now: '; grep -n 'KERNEL_VERSION :=' "$MK"
else
  die "Revert verification failed — inspect $MK manually (uninstall backup: ${MK}.uninstall.bak if present)."
fi

warn "提醒：补丁已撤销，但已安装的 gf.ko 不受影响；下次内核升级可能再次黑屏，"
warn "届时重新运行 sudo dkms/fix-lisuan-gpu.sh 即可。"
