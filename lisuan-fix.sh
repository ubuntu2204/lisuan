#!/usr/bin/env bash
#
# lisuan-fix.sh — 砺算 7G100 × Ubuntu 24.04 修复索引（交互式）
#
# 功能：
#   1. 自动检测系统环境（GPU/内核/DKMS/驱动状态/Steam 补丁状态），给出结论；
#   2. 列出编号菜单，选择数字执行对应的单项修复/体检。
#
# 用法：
#   ./lisuan-fix.sh             # 检测 + 交互菜单
#   ./lisuan-fix.sh --detect    # 只做环境检测（可用于排障/脚本化）
#   ./lisuan-fix.sh <编号>      # 直接执行某一项（如 ./lisuan-fix.sh 2）
#
# License: Apache-2.0
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SUB="$SCRIPT_DIR/lisuan-7g100-ubuntu-fix"
S="$SUB/scripts"

# ---------- 颜色 ----------
if [[ -t 1 ]]; then
  G='\033[1;32m'; R='\033[1;31m'; Y='\033[1;33m'; B='\033[1;34m'; N='\033[0m'
else
  G=''; R=''; Y=''; B=''; N=''
fi
ok()   { printf "  ${G}[OK]${N} %s\n" "$*"; }
bad()  { printf "  ${R}[--]${N} %s\n" "$*"; }
warn() { printf "  ${Y}[!!]${N} %s\n" "$*"; }
info() { printf "  ${B}[ii]${N} %s\n" "$*"; }

# ---------- 环境检测 ----------
detect() {
  echo "======================================================"
  echo " 环境检测  $(date '+%F %T')"
  echo "======================================================"

  # GPU
  echo "== GPU =="
  if lspci -nn 2>/dev/null | grep -q '4c54:'; then
    ok "检测到砺算 GPU：$(lspci -nn | grep '4c54:5006' | head -1 | sed 's/^.*: //')"
  else
    bad "未检测到砺算 GPU（PCI 厂商号 4c54）——本工具不适用"
  fi

  # 内核
  echo "== 内核 =="
  info "运行内核：$(uname -r)"

  # DKMS 状态
  echo "== DKMS / 驱动包 =="
  if dpkg -l lisuangpu-dkms >/dev/null 2>&1; then
    ok "lisuangpu-dkms 已安装：$(dpkg -l lisuangpu-dkms | awk '/^ii/{print $3}')"
  else
    bad "lisuangpu-dkms 未安装（请先向厂商/OEM 获取驱动包）"
  fi
  if command -v dkms >/dev/null 2>&1; then
    local dkst
    dkst="$(dkms status 2>/dev/null | grep -i lisuan)"
    if [[ -n "$dkst" ]]; then
      while IFS= read -r line; do info "dkms: $line"; done <<< "$dkst"
    else
      warn "dkms 中无 lisuan 模块记录"
    fi
  fi

  # 模块加载与 vermagic
  echo "== 内核模块 gf =="
  if lsmod | grep -q '^gf '; then
    ok "模块 gf 已加载"
  else
    bad "模块 gf 未加载 —— 优先尝试菜单 [2] 修复 DKMS"
  fi
  local ko verm
  ko="$(find "/lib/modules/$(uname -r)" -name 'gf.ko*' 2>/dev/null | head -1)"
  if [[ -n "$ko" ]]; then
    verm="$(modinfo "$ko" 2>/dev/null | awk '/^vermagic/{print $2}')"
    if [[ "$verm" == "$(uname -r)" ]]; then
      ok "vermagic 与运行内核一致（$verm）"
    else
      bad "vermagic 错配：模块=$verm，运行内核=$(uname -r) —— 菜单 [2]"
    fi
  else
    warn "当前内核模块树下没有 gf.ko —— 菜单 [2]"
  fi

  # DRM 节点与 DRI 驱动名
  echo "== DRM / DRI =="
  if ls /dev/dri/card* >/dev/null 2>&1; then
    ok "DRM 节点：$(ls /dev/dri/ | tr '\n' ' ')"
  else
    bad "无 /dev/dri/cardN"
  fi
  if [[ -e /usr/lib/x86_64-linux-gnu/dri/lisuan_dri.so ]]; then
    ok "lisuan_dri.so 软链存在（DRI 驱动名已修复）"
  elif [[ -e /usr/lib/x86_64-linux-gnu/dri/gf_dri.so ]]; then
    bad "缺 lisuan_dri.so 软链 —— 菜单 [4]"
  else
    warn "未见 gf_dri.so（lisuangpu-lib 未安装？）"
  fi

  # Steam
  echo "== Steam =="
  if command -v steam >/dev/null 2>&1; then
    ok "Steam 已安装"
    local wrap="$HOME/.local/share/Steam/ubuntu12_64/steamwebhelper_sniper_wrap.sh"
    if [[ -f "$wrap" ]]; then
      if grep -q 'GBM_BACKEND=dri' "$wrap" && grep -q 'libglapi_gf' "$wrap"; then
        ok "Steam CEF 补丁已注入（GBM_BACKEND + LD_PRELOAD）"
      else
        bad "Steam CEF 补丁缺失/被客户端还原 —— 菜单 [5]"
      fi
      lsattr "$wrap" 2>/dev/null | awk '{print $1}' | grep -q 'i' \
        && ok "wrap.sh 已锁定（chattr +i）" \
        || warn "wrap.sh 未锁定，可能被 Steam 校验还原 —— 菜单 [5]"
    fi
  else
    info "未安装 Steam（跳过 Steam 相关项）"
  fi

  # 游戏
  local ul="$HOME/.local/share/Steam/steamapps/common/Underlords/game"
  if [[ -x "$ul/play-native.sh" ]]; then
    ok "Dota Underlords 宿主直跑启动器就绪（菜单 [8]）"
  elif [[ -d "$ul" ]]; then
    warn "Dota Underlords 已安装但缺少 play-native.sh（见 README 游戏兼容性一节）"
  fi
  echo
}

# ---------- 单项执行 ----------
need_root() {  # need_root <cmd...>：非 root 时自动 sudo
  if [[ $EUID -eq 0 ]]; then "$@"; else sudo "$@"; fi
}

run_item() {
  case "$1" in
    1) detect ;;
    2) need_root bash "$S/fix-lisuan-gpu.sh" "${2:-}" ;;
    3) bash "$S/verify-lisuan-gpu.sh" ;;
    4) need_root bash "$S/fix-lisuan-mesa-driver-name.sh" ;;
    5) need_root bash "$S/fix-steam-cef-lock.sh" ;;
    6) need_root bash "$S/fix-steam-cef-lock.sh" --unlock ;;
    7) bash "$S/verify-system-mesa.sh" ;;
    8)
      local ul="$HOME/.local/share/Steam/steamapps/common/Underlords/game/play-native.sh"
      if [[ -x "$ul" ]]; then
        pgrep -f 'ubuntu12_32/steam' >/dev/null || warn "Steam 客户端未在运行，游戏可能无法完成授权"
        "$ul"
      else
        bad "未找到 $ul"
      fi ;;
    *) bad "无效选项：$1"; return 1 ;;
  esac
}

menu() {
  cat <<EOF
======================================================
 修复菜单（输入编号回车执行，Ctrl-C 退出）
======================================================
  ${B}检测${N}
  1) 重新检测系统环境
  ${B}修复${N}
  2) 修复 DKMS 驱动构建/加载（vermagic 错配、黑屏）   [需要 sudo]
  4) 修复 DRI 驱动名（lisuan_dri.so 软链）            [需要 sudo]
  5) 修复 Steam 界面崩溃（注入容器补丁并锁定）        [需要 sudo]
  6) 解锁 Steam 补丁（Steam 客户端更新前执行）        [需要 sudo]
  ${B}体检${N}
  3) GPU 驱动体检（lspci/lsmod/modinfo/DRM）
  7) 系统 Mesa 兼容性体检（zink/GBM dri 后端）
  ${B}游戏${N}
  8) 启动 Dota Underlords（宿主直跑，需 Steam 在运行）
  0) 退出
======================================================
EOF
}

# ---------- 入口 ----------
[[ -d "$SUB" ]] || { echo "错误：找不到 $SUB" >&2; exit 1; }

if [[ "${1:-}" == "--detect" ]]; then
  detect; exit 0
fi

if [[ -n "${1:-}" ]]; then        # 非交互：./lisuan-fix.sh <编号>
  detect
  run_item "$1" "${2:-}"
  exit $?
fi

detect
while true; do
  menu
  if [[ -t 0 ]]; then
    read -rp "请选择 [0-8]: " choice
  else
    read -rp "请选择 [0-8]: " choice </dev/tty
  fi
  [[ "$choice" == "0" ]] && { echo "再见。"; break; }
  run_item "$choice" || true
  echo
  read -rp "按回车继续..." _ </dev/tty 2>/dev/null || read -rp "按回车继续..." _
done
