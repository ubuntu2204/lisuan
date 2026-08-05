#!/usr/bin/env bash
#
# steam-cef-autofix.sh — 开机自愈：为每个本机 Steam 安装重新注入并锁定
# CEF 补丁（GBM_BACKEND=dri + LD_PRELOAD glapi）。
#
# 用途：Steam 客户端更新会还原 steamwebhelper_sniper_wrap.sh，导致砺算
# 7G100 上 Steam 界面再次无法启动。本脚本由 systemd 服务在每次开机时
# 运行（见 lisuan-fix.sh 菜单 9），发现补丁丢失即自动修复，用户无感。
#
# 需 root 运行（chattr +i）。幂等：补丁完好时不做任何事。
#
# Usage:
#   sudo ./steam-cef-autofix.sh               # scan + auto-repair (systemd calls this)
#   sudo ./steam-cef-autofix.sh --uninstall   # remove the boot-time systemd service
#
# License: Apache-2.0
set -uo pipefail

# 需要 chattr +i，必须 root（systemd 服务以 root 运行，手工执行要 sudo）
[[ $EUID -eq 0 ]] || { echo "需要 root（通常由 systemd 服务调用）" >&2; exit 1; }

# --uninstall：卸载开机自愈服务（与 lisuan-fix.sh 菜单 10 等价，幂等）
if [[ "${1:-}" == "--uninstall" ]]; then
  UNIT=/etc/systemd/system/lisuan-steam-autofix.service
  if [[ -f "$UNIT" ]]; then
    systemctl disable --now lisuan-steam-autofix.service 2>/dev/null || true
    rm -f "$UNIT"
    systemctl daemon-reload
    echo "[lisuan-autofix] 已卸载开机自愈服务（$UNIT）"
  else
    echo "[lisuan-autofix] 开机自愈服务未安装，无需卸载"
  fi
  exit 0
fi

# 注入逻辑全部复用 fix-steam-cef-lock.sh，本脚本只负责“判断要不要修”
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LOCK_SCRIPT="$HERE/fix-steam-cef-lock.sh"
[[ -x "$LOCK_SCRIPT" ]] || { echo "找不到 $LOCK_SCRIPT" >&2; exit 1; }

# 逐个扫描本机用户的 Steam 安装（支持多用户）
found=0
for home in /home/*; do
  wrap="$home/.local/share/Steam/ubuntu12_64/steamwebhelper_sniper_wrap.sh"
  [[ -f "$wrap" ]] || continue      # 该用户没装 Steam，跳过
  found=1
  user="$(stat -c %U "$wrap")"      # 以文件属主为准，避免家目录软链等歧义
  if grep -q 'GBM_BACKEND=dri' "$wrap" && grep -q 'libglapi_gf' "$wrap"; then
    # 两个注入标记都在 = 补丁完好（通常还被 chattr +i 锁着），无需操作
    echo "[lisuan-autofix] $user: 补丁完好，跳过"
  else
    # 标记缺失 = 被 Steam 客户端更新还原了，重新注入并锁定。
    # 通过 SUDO_USER 告知锁定脚本真实用户（它靠这个定位家目录）
    echo "[lisuan-autofix] $user: 补丁丢失（Steam 更新?），重新注入并锁定"
    SUDO_USER="$user" bash "$LOCK_SCRIPT" || \
      echo "[lisuan-autofix] $user: 注入失败，请手动运行 fix-steam-cef-lock.sh" >&2
  fi
done

# 机器上没有任何 Steam 安装也属正常（服务静默放行）
[[ $found -eq 0 ]] && echo "[lisuan-autofix] 未找到 Steam 安装，无事可做"
exit 0
