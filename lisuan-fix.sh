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

# 路径锪定：本脚本在仓库根，所有单项脚本在 lisuan-7g100-ubuntu-fix/scripts/
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SUB="$SCRIPT_DIR/lisuan-7g100-ubuntu-fix"
S="$SUB/scripts"

# ---------- 颜色（非终端输出时置空，避免日志里混入转义码） ----------
if [[ -t 1 ]]; then
  G='\033[1;32m'; R='\033[1;31m'; Y='\033[1;33m'; B='\033[1;34m'; N='\033[0m'
else
  G=''; R=''; Y=''; B=''; N=''
fi
ok()   { printf "  ${G}[OK]${N} %s\n" "$*"; }    # 正常
bad()  { printf "  ${R}[--]${N} %s\n" "$*"; }   # 异常，通常对应某个菜单项
warn() { printf "  ${Y}[!!]${N} %s\n" "$*"; }   # 提醒，不一定需要处理
info() { printf "  ${B}[ii]${N} %s\n" "$*"; }   # 中性信息

# ---------- 环境检测：只读，逐项给出结论并指向对应菜单项 ----------
detect() {
  echo "======================================================"
  echo " 环境检测  $(date '+%F %T')"
  echo "======================================================"

  # GPU：砺算的 PCI 厂商号是 4c54（ASCII "LT"），没有它本仓库不适用
  echo "== GPU =="
  if lspci -nn 2>/dev/null | grep -q '4c54:'; then
    ok "检测到砺算 GPU：$(lspci -nn | grep '4c54:5006' | head -1 | sed 's/^.*: //')"
  else
    bad "未检测到砺算 GPU（PCI 厂商号 4c54）——本工具不适用"
  fi

  # 内核：vermagic 比对的目标
  echo "== 内核 =="
  info "运行内核：$(uname -r)"

  # DKMS 状态：驱动包是否安装、dkms 里有哪些构建记录
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

  # 模块加载与 vermagic：本仓库核心修复对象的直接指标
  echo "== 内核模块 gf =="
  if lsmod | grep -q '^gf '; then
    ok "模块 gf 已加载"
  else
    bad "模块 gf 未加载 —— 优先尝试菜单 [2] 修复 DKMS"
  fi
  # 找当前内核模块树下的 gf.ko(.zst)，比对 vermagic
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

  # DRM 节点与 DRI 驱动名：lisuan_dri.so 软链是菜单 [4] 的产物
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

  # Steam：补丁注入状态 + 锁定状态，两项都绿才算修复到位
  echo "== Steam =="
  if command -v steam >/dev/null 2>&1; then
    ok "Steam 已安装"
    local wrap="$HOME/.local/share/Steam/ubuntu12_64/steamwebhelper_sniper_wrap.sh"
    if [[ -f "$wrap" ]]; then
      # 两个注入标记：GBM_BACKEND=dri（绕 gf_gbm 段错误）+ libglapi_gf（修 ANGLE 崩溃）
      if grep -q 'GBM_BACKEND=dri' "$wrap" && grep -q 'libglapi_gf' "$wrap"; then
        ok "Steam CEF 补丁已注入（GBM_BACKEND + LD_PRELOAD）"
      else
        bad "Steam CEF 补丁缺失/被客户端还原 —— 菜单 [5]"
      fi
      # chattr +i 锁定才能抵御 Steam 的启动校验还原
      lsattr "$wrap" 2>/dev/null | awk '{print $1}' | grep -q 'i' \
        && ok "wrap.sh 已锁定（chattr +i）" \
        || warn "wrap.sh 未锁定，可能被 Steam 校验还原 —— 菜单 [5]"
    fi
  else
    info "未安装 Steam（跳过 Steam 相关项）"
  fi

  # 开机自愈服务
  echo "== 开机自愈 =="
  if systemctl is-enabled lisuan-steam-autofix.service >/dev/null 2>&1; then
    ok "Steam 补丁开机自愈服务已安装（Steam 更新后自动重新注入）"
  else
    warn "未安装开机自愈服务 —— 菜单 [9]（Steam 更新后需手动跑菜单 [5]）"
  fi

  # 游戏
  local ul="$HOME/.local/share/Steam/steamapps/common/Underlords/game"
  if grep -q 'Lisuan 7G100 fix (injected)' "$ul/underlords.sh" 2>/dev/null \
     && [[ -x "$ul/play-native.sh" ]]; then
    ok "Dota Underlords 修复已安装（Steam 内直接点开始游戏；菜单 [8]/[15]）"
  elif [[ -d "$ul" ]]; then
    warn "Dota Underlords 修复未安装 —— 菜单 [14]"
  fi
  echo
}

# ---------- 单项执行 ----------
need_root() {  # need_root <cmd...>：非 root 时自动 sudo，菜单项不用关心权限
  if [[ $EUID -eq 0 ]]; then "$@"; else sudo "$@"; fi
}

AUTOFIX_UNIT=/etc/systemd/system/lisuan-steam-autofix.service   # 自愈服务单元路径

install_autofix() {  # 安装开机自愈服务：Steam 更新还原补丁后自动重新注入
  # 写入 systemd 单元：每次开机跑一次 steam-cef-autofix.sh（oneshot）
  need_root tee "$AUTOFIX_UNIT" >/dev/null <<EOF
[Unit]
Description=Lisuan 7G100 Steam CEF patch auto-repair

[Service]
Type=oneshot
ExecStart=$S/steam-cef-autofix.sh

[Install]
WantedBy=multi-user.target
EOF
  need_root systemctl daemon-reload
  # enable = 开机自启；--now = 立即跑一次验证可用性
  need_root systemctl enable --now lisuan-steam-autofix.service
  ok "已安装开机自愈服务（$AUTOFIX_UNIT）"
  info "今后 Steam 客户端更新还原补丁后，下次开机自动重新注入，无需手动操作"
}

remove_autofix() {   # 卸载自愈服务：停用并删除单元文件（幂等）
  if [[ -f "$AUTOFIX_UNIT" ]]; then
    need_root systemctl disable --now lisuan-steam-autofix.service 2>/dev/null || true
    need_root rm -f "$AUTOFIX_UNIT"
    need_root systemctl daemon-reload
    ok "已卸载开机自愈服务"
  else
    info "开机自愈服务未安装，无需卸载"
  fi
}

# 菜单分发：编号 → 对应脚本/函数；无效编号返回 1 由主循环提示
run_item() {
  case "$1" in
    1) detect ;;                                              # 重新检测
    2) need_root bash "$S/fix-lisuan-gpu.sh" "${2:-}" ;;     # 可透传目标内核参数
    3) bash "$S/verify-lisuan-gpu.sh" ;;                     # 只读体检
    4) need_root bash "$S/fix-lisuan-mesa-driver-name.sh" ;; # DRI 驱动名软链
    5) need_root bash "$S/fix-steam-cef-lock.sh" ;;          # Steam 补丁注入+锁定
    6) need_root bash "$S/fix-steam-cef-lock.sh" --unlock ;; # Steam 更新前解锁
    7) bash "$S/verify-system-mesa.sh" ;;                    # Mesa 兼容性体检
    8)
      # 宿主直跑 Underlords（容器内 GLX 无解，见 bug 报告 §4）
      local ul="$HOME/.local/share/Steam/steamapps/common/Underlords/game/play-native.sh"
      if [[ -x "$ul" ]]; then
        # 直跑模式依赖 Steam 客户端提供授权，先提醒
        pgrep -f 'ubuntu12_32/steam' >/dev/null || warn "Steam 客户端未在运行，游戏可能无法完成授权"
        "$ul"
      else
        bad "未找到 $ul"
      fi ;;
    9) install_autofix ;;    # 安装开机自愈
    10) remove_autofix ;;    # 卸载开机自愈
    11) need_root bash "$S/uninstall-lisuan-gpu.sh" ;;                 # 撤销 DKMS 补丁
    12) need_root bash "$S/uninstall-lisuan-mesa-driver-name.sh" ;;    # 删 DRI 软链
    13) need_root bash "$S/fix-steam-cef-lock.sh" --uninstall ;;       # 还原 Steam wrap.sh
    14) need_root bash "$S/fix-underlords-native.sh" ;;                # 安装/修复游戏补丁集
    15) bash "$S/fix-underlords-native.sh" --status ;;                 # 游戏修复状态
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
  9) 安装开机自愈（Steam 更新后自动重新注入补丁）     [需要 sudo]
  ${B}卸载${N}（每项注入都有对应的撤销，均幂等）
  10) 卸载开机自愈服务                              [需要 sudo]
  11) 卸载 DKMS Makefile 补丁（还原原厂，不影响已装驱动） [需要 sudo]
  12) 卸载 DRI 驱动名软链（只删我们建的软链）        [需要 sudo]
  13) 卸载 Steam 补丁（还原 wrap.sh，界面将回到无法启动状态） [需要 sudo]
  ${B}体检${N}
  3) GPU 驱动体检（lspci/lsmod/modinfo/DRM）
  7) 系统 Mesa 兼容性体检（zink/GBM dri 后端）
  ${B}游戏${N}
  8) 启动 Dota Underlords（宿主直跑，需 Steam 在运行）
  14) 安装/修复 Dota Underlords 补丁集（容器委托+重试+libpng12+桌面项） [需要 sudo]
  15) 查看 Dota Underlords 修复状态
  0) 退出
======================================================
EOF
}

# ---------- 入口：三种模式 ----------
[[ -d "$SUB" ]] || { echo "错误：找不到 $SUB" >&2; exit 1; }

if [[ "${1:-}" == "--detect" ]]; then   # 模式一：只检测，供排障/脚本化使用
  detect; exit 0
fi

if [[ -n "${1:-}" ]]; then              # 模式二：非交互直执行某项（./lisuan-fix.sh 2）
  detect
  run_item "$1" "${2:-}"
  exit $?
fi

# 模式三：交互菜单循环；stdin 非终端（如 sudo 重定向）时从 /dev/tty 读
detect
while true; do
  menu
  if [[ -t 0 ]]; then
    read -rp "请选择 [0-15]: " choice
  else
    read -rp "请选择 [0-15]: " choice </dev/tty
  fi
  [[ "$choice" == "0" ]] && { echo "再见。"; break; }
  run_item "$choice" || true             # 单项失败不退出菜单，方便换一项重试
  echo
  read -rp "按回车继续..." _ </dev/tty 2>/dev/null || read -rp "按回车继续..." _
done
