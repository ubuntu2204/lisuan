#!/usr/bin/env bash
#
# fix-steam-cef-lock.sh — lock the CEF software-rendering patch in place so
# Steam's checksum verification cannot revert it.
#
# Steam overwrites steamwebhelper_sniper_wrap.sh on every launch ("Verifying
# all executable checksums"), undoing a plain edit. This script re-applies the
# --disable-gpu injection AND sets the immutable attribute (chattr +i) so the
# file cannot be overwritten. ext4 only.
#
# Usage:
#   sudo ./fix-steam-cef-lock.sh           # apply + lock
#   sudo ./fix-steam-cef-lock.sh --unlock  # remove the lock (before Steam update)
#
# License: Apache-2.0
set -euo pipefail

[[ $EUID -eq 0 ]] || { echo "需要 root:  sudo $0 $*" >&2; exit 1; }

# Resolve the real user's home (sudo may set HOME=/root)
REAL_USER="${SUDO_USER:-$USER}"
REAL_HOME="$(getent passwd "$REAL_USER" | cut -d: -f6)"
WRAP="${REAL_HOME}/.local/share/Steam/ubuntu12_64/steamwebhelper_sniper_wrap.sh"

[[ -f "$WRAP" ]] || { echo "未找到: $WRAP — Steam 是否已安装？" >&2; exit 1; }

# Always unlock first (idempotent)
chattr -i "$WRAP" 2>/dev/null || true

if [[ "${1:-}" == "--unlock" ]]; then
  echo "==> 已解锁 $WRAP — Steam 更新可正常覆盖。更新后重新运行 sudo $0 重新注入。"
  exit 0
fi

# Inject --disable-gpu if not already present
if ! grep -q -- '--disable-gpu' "$WRAP"; then
  cp -a "$WRAP" "${WRAP}.bak"
  sed -i 's|^exec \./steamwebhelper "\$@"$|exec ./steamwebhelper --disable-gpu --disable-gpu-compositing --in-process-gpu "$@"|' "$WRAP"
fi

if ! grep -q -- '--disable-gpu' "$WRAP"; then
  echo "!! 注入失败 — 文件格式可能已变。请手动检查 $WRAP" >&2
  exit 1
fi

echo "==> 注入成功:"
grep 'exec ./steamwebhelper' "$WRAP"

# Lock the file so Steam's verification cannot overwrite it
chattr +i "$WRAP"
echo "==> 已锁定 (chattr +i) — Steam 校验无法恢复原文件"

cat <<EOF

现在启动 Steam:
  MESA_LOADER_DRIVER_OVERRIDE=gf steam -no-cef-sandbox -cef-force-64bit

验证 CEF 是否还崩:
  grep -c 'exit_code=139' ~/.local/share/Steam/logs/cef_log.txt

Steam 更新前解锁:
  sudo $0 --unlock

更新后重新注入:
  sudo $0
EOF
