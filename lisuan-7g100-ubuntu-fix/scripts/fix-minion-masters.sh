#!/bin/bash
# fix-minion-masters.sh - 修复 Minion Masters 在 砾算 7G100 + Ubuntu 24.04 上
#                         "一直显示正在启动"无法进入游戏的问题
#
# 游戏: Minion Masters (Steam AppID 489520) — 纯 Windows Unity, 走 Proton
#
# 根因 (7G100 + Ubuntu 24.04 Wayland 下的 Proton 显示问题):
#   1) 会话是 Wayland, 但 Proton Experimental 默认用 xalia 桥把 wine 的 X11 窗口
#      翻译成 Wayland surface —— 在 7G100 上这个翻译过程窗口直接丢失 (进程在跑但
#      无可见窗口, Steam 一直显示"正在启动")。清空 WAYLAND_DISPLAY 强制 Proton 走
#      DISPLAY=:0 的 XWayland (纯 X11) 即可正常弹出。
#   2) Vulkan/DXVK 初始化失败: 容器内 winevulkan 默认加载的 Vulkan loader 会拒绝
#      7G100 厂商 ICD libvulkan_gf.so (interface v4, 而 loader 1.3.275 要求 v5,
#      报 Policy #LDP_DRIVER_7 并跳过 -> "Failed to create Vulkan instance")。
#      但 host 的系统 loader 实测 vkCreateInstance 成功且能枚举到 7G100 设备(1 个)。
#      故强制容器内 wine 用 host 的 Vulkan: LD_LIBRARY_PATH 指向 /run/host 的
#      libvulkan.so.1, VK_ICD_FILENAMES 指向 /run/host 的 gf_icd (pressure-vessel
#      把 host 根挂到容器内 /run/host)。注意: wined3d(OpenGL) 路径在 7G100 上也
#      失败 (Mesa GLX 拿不到驱动名 "failed to get driver name"), 因此只用 DXVK。
#
# 修复 (强制 DXVK + XWayland, 无 wined3d 选项):
#   唯一有效模式 --dxvk (默认): LD_LIBRARY_PATH + VK_ICD_FILENAMES 指向 /run/host 的
#   系统 Vulkan loader/ICD, 清空 WAYLAND_DISPLAY 走 XWayland。两步同时解决窗口丢失
#   与 Vulkan 初始化失败。wined3d 在 7G100 上同样失败, 故不提供。
#
# 实现: 写入 Proton 的 user_settings.py (官方支持, 不经 vdf, 不被 Steam 覆盖),
#       不修改游戏文件; 卸载即删除该文件并清 vdf LaunchOptions。
#
# Usage:
#   ./fix-minion-masters.sh              # 安装 (默认 DXVK 修复)
#   ./fix-minion-masters.sh --uninstall  # 还原
#   ./fix-minion-masters.sh --status     # 查看当前 user_settings.py 状态

APPID=489520
GAME="Minion Masters"
STEAM_USERCFG=$(ls ~/.local/share/Steam/userdata/*/config/localconfig.vdf 2>/dev/null | head -1)
[ -n "$STEAM_USERCFG" ] || { echo "!! 找不到 localconfig.vdf" >&2; exit 1; }

MODE="dxvk"
[[ "${1:-}" == "--status" ]] && {
  US=$(ls ~/.local/share/Steam/steamapps/common/Proton\ -\ Experimental/files/share/user_settings.py 2>/dev/null)
  if [ -f "$US" ]; then echo "  user_settings.py 已存在:"; grep -E 'WAYLAND_DISPLAY|LD_LIBRARY_PATH|VK_ICD_FILENAMES|GBM_BACKEND' "$US"; else echo "  未安装 (无 user_settings.py)"; fi
  exit 0; }

# 仅当目标 vdf 非当前用户可写时才需要 root (家目录下的 Steam 配置通常无需 sudo)
if [ ! -w "$STEAM_USERCFG" ]; then
  [[ $EUID -eq 0 ]] || { echo "!! 需要 root (sudo) 写入 $STEAM_USERCFG" >&2; exit 1; }
fi

# 双重修复 (两层根因必须同时解决):
#  1) WAYLAND_DISPLAY= 清空 -> Proton 回退 DISPLAY=:0 的 XWayland, 绕开 xalia 桥窗口丢失
#  2) Vulkan 路径: 容器内 winevulkan 默认加载的 loader 会拒绝 7G100 旧版 gf ICD
#     (interface v4, 系统 loader 1.3.275 要求 v5 -> "Failed to create Vulkan instance")。
#     但 host 的系统 loader 实测 vkCreateInstance 成功且能识别 7G100 设备(count=1)。
#     故强制容器内 wine 用 host 的 Vulkan: LD_LIBRARY_PATH 指向 /run/host 的 libvulkan,
#     VK_ICD_FILENAMES 指向 /run/host 的 gf_icd (pressure-vessel 把 host 挂到 /run/host)。
#
# 注入方式: 写入 Proton 的 user_settings.py (官方支持, 不经 vdf, 不被 Steam 覆盖)。
#   已实测 Steam 会在退出时把 localconfig.vdf 的 LaunchOptions 覆盖回默认值, 因此
#   写 vdf 不可靠; user_settings.py 由 Proton 每次启动时加载, 是稳定注入点。
PROTON_DIR=$(ls -d ~/.local/share/Steam/steamapps/common/Proton\ -\ Experimental 2>/dev/null)
# 注意: Proton 实际加载 user_settings.py 的位置是 Proton 安装根目录 (与 proton 脚本同级),
#       而非 files/share/ (那里只是 user_settings.sample.py 的存放处, 文档有歧义)。
USER_SETTINGS="$PROTON_DIR/user_settings.py"
case "$MODE" in
  dxvk)
    note="写入 user_settings.py (DXVK + XWayland, 强制容器内 wine 用 host 的 Vulkan loader/ICD)"
    ;;
esac

if [[ "${1:-}" == "--uninstall" ]]; then
  # 还原: 删 user_settings.py (若有备份则恢复), 并清掉 vdf 的 LaunchOptions
  if [ -f "$USER_SETTINGS" ]; then rm -f "$USER_SETTINGS"; echo "已删除 $USER_SETTINGS"; fi
  # 同时清 vdf LaunchOptions, 避免残留 PROTON_USE_WINED3D 干扰
  python3 - "$STEAM_USERCFG" <<'PY'
import sys
p=sys.argv[1]
d=open(p,'rb').read()
i=d.find(b'489520')
bo=d.find(b'{\n', i)
depth=0; j=bo; be=None
while j<len(d):
    if d[j:j+1]==b'{': depth+=1
    elif d[j:j+1]==b'}':
        depth-=1
        if depth==0: be=j; break
    j+=1
import re
seg=d[i:be]
seg2=re.sub(rb'\t*"LaunchOptions"\s*"[^"]*"\n', b'', seg)
open(p,'wb').write(d[:i]+seg2+d[be:])
PY
  echo "卸载完成。重启 Steam 后生效。"
  exit 0
fi

# 安装: 写入 user_settings.py
mkdir -p "$(dirname "$USER_SETTINGS")"
cat > "$USER_SETTINGS" <<'PYEOF'
# 由 fix-minion-masters.sh 生成 - 7G100 + Ubuntu 24.04 (Wayland) Proton 全局修复
# 见脚本注释了解根因。Steam 不覆盖本文件。
user_settings = {
    "WAYLAND_DISPLAY": "",
    "LD_LIBRARY_PATH": "/run/host/usr/lib/x86_64-linux-gnu",
    "VK_ICD_FILENAMES": "/run/host/usr/share/vulkan/icd.d/gf_icd.x86_64.json",
    "GBM_BACKEND": "dri",
}
PYEOF
echo "已写入 $USER_SETTINGS"
echo "  (WAYLAND_DISPLAY= 走 XWayland; LD_LIBRARY_PATH/VK_ICD_FILENAMES 指向 /run/host 的 Vulkan)"
echo "重启 Steam 后从库里点 '开始游戏' 即可验证。"
