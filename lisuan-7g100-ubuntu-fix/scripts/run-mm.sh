#!/bin/bash
# run-mm.sh - Minion Masters (Steam AppID 489520) Proton 启动 wrapper
#
# 用途: 作为 Steam Launch Options 的 %command% 前置 wrapper, 在游戏进程启动前
#       注入 7G100 + Ubuntu 24.04 (Wayland) 下必需的 Vulkan/显示修复环境变量。
#       用 wrapper 而非直接写 LaunchOptions 字符串, 可避免 Steam 重写 vdf 时
#       丢失修复变量。
#
# 双层根因修复:
#   1) WAYLAND_DISPLAY= 清空 -> Proton 回退 DISPLAY=:0 的 XWayland, 绕开
#      Proton Experimental 的 xalia 桥在 7G100 Wayland 下窗口丢失。
#   2) 容器内 winevulkan 默认 loader 拒绝 7G100 厂商 ICD libvulkan_gf.so
#      (interface v4, loader 1.3.275 要求 v5 -> "Failed to create Vulkan instance")。
#      但 host 系统 loader 实测 vkCreateInstance 成功且能枚举 7G100 设备(1个)。
#      故强制容器内 wine 用 host 的 Vulkan: LD_LIBRARY_PATH + VK_ICD_FILENAMES
#      指向 /run/host (pressure-vessel 把 host 根挂到容器内 /run/host)。
#
# 用法 (Steam 启动选项):
#   bash /home/ubuntu/project/lisuan/lisuan-7g100-ubuntu-fix/scripts/run-mm.sh %command%

# 1) 绕开 xalia 桥, 走 XWayland
export WAYLAND_DISPLAY=
# 2) 强制容器内 wine 使用 host 的 Vulkan loader 与 7G100 厂商 ICD
export LD_LIBRARY_PATH="/run/host/usr/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH}"
export VK_ICD_FILENAMES="/run/host/usr/share/vulkan/icd.d/gf_icd.x86_64.json"
# 3) 厂商 Mesa 22.0.4 与系统 libgbm ABI 漂移的兜底 (同类 7G100 问题的通用防御)
export GBM_BACKEND=dri

# 可选调试: 设为 0 关闭。默认开以便首次抓 ~/steam-489520.log 定位容器内 Vulkan 行为。
if [ "${MM_DEBUG:-1}" = "1" ]; then
  export PROTON_LOG=1
  export DXVK_LOG_LEVEL=info
  export DXVK_LOG_PATH=/tmp/dxvk-mm
  export VK_LOADER_DEBUG=all
  mkdir -p /tmp/dxvk-mm
fi

# 执行 Steam 注入的原始命令 (%command%)
exec "$@"
