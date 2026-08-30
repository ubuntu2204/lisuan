#!/bin/bash
# run-mm.sh - Minion Masters (Steam AppID 489520) 启动 wrapper (诊断/代理用)
#
# 作为 Steam Launch Options 的 %command% 前置 wrapper。
# 注意: 渲染相关 env (Vulkan/GBM/WAYLAND) 已移除以避免 d3d11 factory 创建失败;
# 当前环境用默认 proton 渲染即可。
#
# 用法 (Steam 启动选项):
#   bash /home/ubuntu/project/lisuan/minion-masters/run-mm.sh %command%

# 代理: 游戏登录 REST/HTTPS 后端 140.228.24.145:443 在网络被封时走代理。
# 仅作环境注入, 实测 wine 下游戏可能不读 http_proxy (改用 user.reg 的 WinHTTP 代理)。
export http_proxy="http://127.0.0.1:7897"
export https_proxy="http://127.0.0.1:7897"
export all_proxy="http://127.0.0.1:7897"
export HTTP_PROXY="http://127.0.0.1:7897"
export HTTPS_PROXY="http://127.0.0.1:7897"
export ALL_PROXY="http://127.0.0.1:7897"

# 渲染回退: dxvk(vulkan) 在某些环境下枚举 Vulkan 设备卡住导致 GfxDevice 创建失败 -> 游戏退出。
# 强制 wine 用内建 wined3d 的 d3d11 (OpenGL/Mesa/llvmpipe), 绕开 Vulkan。
export PROTON_USE_WINED3D=1
export WINEDLLOVERRIDES="d3d11=b,wined3d"

# 执行 Steam 注入的原始命令 (%command%)
exec "$@"
