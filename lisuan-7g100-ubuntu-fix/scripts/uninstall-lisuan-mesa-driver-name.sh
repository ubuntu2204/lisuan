#!/usr/bin/env bash
#
# uninstall-lisuan-mesa-driver-name.sh — remove the lisuan_*.so compatibility
# symlinks created by fix-lisuan-mesa-driver-name.sh.
#
# 安全保证：只删“我们创建的软链”，绝不删除真实驱动文件——
#   删除前逐一校验：必须是软链（-L）且指向 gf_* 目标，否则跳过。
#
# 撤销后果：Mesa 按 DRM 名（lisuan）找不到 DRI 驱动，GL/EGL 回到
# driver=(null) 状态；Steam CEF 的容器崩溃问题会重新出现。
#
# Usage:   sudo ./uninstall-lisuan-mesa-driver-name.sh
# License: Apache-2.0
set -euo pipefail

[[ $EUID -eq 0 ]] || { echo "Must run as root:  sudo $0" >&2; exit 1; }

DRI64=/usr/lib/x86_64-linux-gnu/dri
DRI32=/usr/lib/i386-linux-gnu/dri

log()  { printf '\033[1;32m==>\033[0m %s\n' "$*"; }
warn() { printf '\033[1;33m!!\033[0m %s\n'  "$*" >&2; }

# remove_link <路径>：仅当它是“指向 gf_* 的软链”时才删除
remove_link() {
  local f="$1"
  if [[ ! -e "$f" && ! -L "$f" ]]; then
    return 0                      # 不存在，无事可做（幂等）
  fi
  if [[ -L "$f" ]] && [[ "$(readlink "$f")" == gf_* ]]; then
    rm -f "$f"
    log "removed symlink: $f"
  else
    # 不是软链或指向别处 = 不是我们建的，坚决不碰
    warn "skipped (not our symlink): $f -> $(readlink "$f" 2>/dev/null || echo '<regular file>')"
  fi
}

# ---- 64 位：fix 脚本建的两个软链 -------------------------------------------
remove_link "$DRI64/lisuan_dri.so"
remove_link "$DRI64/lisuan_drv_video.so"

# ---- 32 位：厂商通常未提供 32 位驱动，fix 也没建链；有则一并清理 ----------
remove_link "$DRI32/lisuan_dri.so"
remove_link "$DRI32/lisuan_drv_video.so"

# 刷新动态链接缓存（与 fix 脚本对称）
ldconfig 2>/dev/null || true

log "Done. Verify:"
ls -l "$DRI64"/lisuan_*.so 2>/dev/null || log "  no lisuan_*.so left in $DRI64 (expected)"
