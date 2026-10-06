# 《求生之路 2》（Left 4 Dead 2, AppID 550）7G100 硬件渲染修复

> 状态：**GPU 硬件渲染方案已部署**（2026-10-06，替代已废弃的原生 32 位软渲染方案）
> 修复脚本：`./fix-l4d2.sh`（幂等，支持 `--status` / `--uninstall`）

## 至砺算：官方实现什么可以让本游戏原生运行

> 本节写给砺算官方驱动 / 图形栈研发。按投入产出比排序；**任意一项落地，本目录对应 workaround 即可退役**。

1. **发布 32 位（i386）用户态驱动**：`gf_dri.so (i386)` + 32 位 Vulkan ICD。
   一旦提供，L4D2 **原生 Linux 32 位版**（ToGL/OpenGL）可直接 GPU 硬件渲染——
   不需要 Proton WoW64 + DXVK 整套 workaround，帧率与兼容性都更好；同时解锁
   所有 32 位原生 Linux 游戏。这是本游戏最彻底的解法。
2. 若短期无法出 32 位驱动：按随从大师文档「至砺算」清单加固 64 位栈（ICD
   规范化、`VK_KHR_load_store_op_none` / `VK_KHR_maintenance5`、去掉 os-release
   白名单、libSC 自解析、去除 MAP_32BIT 约束）——本目录的 Proton WoW64 方案
   自动受益（DXVK 升回 v3.x、垫片与 hook 全部退役）。
3. 注：原生路线曾缺的 32 位 `libopenal.so.1` / `libpng12.so.0` 属发行版依赖，
   与驱动无关（且 Proton 路线根本不需要它们）。

---

## 一、TL;DR

```bash
# 1) 完全退出 Steam（vdf 改动会被运行中的 Steam 覆盖）
# 2) 一键部署（幂等）
bash fix-l4d2.sh
# 3) 重启 Steam → 从库里启动《Left 4 Dead 2》
bash fix-l4d2.sh --status     # 随时查看各环节状态
```

游戏内验证 GPU 生效：控制台 `mat_info`，Renderer 应显示 **LISUAN**（而非 llvmpipe）。

## 二、方案

L4D2 目录自带 Windows 版（`left4dead2.exe`）。走 **Proton new WoW64**：
`PROTON_USE_WOW64=1` 让 32 位游戏进程经 wine wow64 thunk 调用【64 位】gf Vulkan
ICD，DXVK 把 DX9 翻译成 Vulkan → 7G100 硬件渲染。

脚本自动完成（与随从大师 `fix-minion-masters-7g100-hw.sh` 同源五层配方）：

| # | 步骤 | 解决的障碍 |
|---|---|---|
| 1 | 部署 os-release 重定向 hook | gf 白名单不认 Steam Runtime 容器 → 0 设备 |
| 2 | 部署协商垫片 ICD（`VK_ICD_FILENAMES`） | Vulkan loader 1.4.x 丢弃老式 ICD |
| 3 | `VK_LOADER_LAYERS_DISABLE` 禁 Steam overlay 层 | overlay 绕过垫片直连真 ICD → 崩 |
| 4 | libSC.so 软链进 Proton 库目录 | `vkCreateDevice` 依赖伴生库 |
| 5 | 目标 Proton 的 DXVK 为 v3.x 时降级 v2.3 | gf 缺 `load_store_op_none`/`maintenance5` |

写入：`CompatToolMapping 550 → proton_11`（推荐稳定版，与随从大师同款；脚本默认
跟随 Steam 中 550 的选择）+ LaunchOptions（保留用户参数如 `-language schinese`）。

> ⚠️ **vdf 改动必须在 Steam 完全退出后进行**：Steam 退出时会用内存中的配置覆盖
> `config.vdf` / `localconfig.vdf`——在 Steam 运行中跑本脚本，改动会在下次退出时
> 被抹掉，游戏随之回退到原生 32 位路径（pressure-vessel 容器内挂起 =「无响应」，
> 2026-10-06 实测踩坑）。`steam -shutdown` 可优雅退出。

## 三、实验方案：原生 32 位 + Venus 进程外垫片

`vtest-forward/` 提供不修改系统驱动的实验链路：

```
L4D2 Linux i386（-vulkan）
  -> Mesa Venus i386（libvulkan_virtio.so）
  -> Unix socket
  -> virglrenderer/Venus x86_64
  -> libvulkan_gf.so x86_64
  -> 7G100
```

这不是在 32 位进程内加载 64 位 `.so`，而是把 Vulkan 命令通过进程外 RPC
交给 64 位服务端。2026-10-06 已用 32 位 `vkcube` 端到端验证，设备显示为
`Virtio-GPU Venus (LISUAN 7G100 Series Graphics)`；L4D2 自带
`shaderapivk.so`，可通过 `-vulkan` 使用同一链路。

```bash
# 构建私有服务端到 ~/.local/share/l4d2-venus，不需要 root
bash vtest-forward/build-venus-bridge.sh

# 先跑 8 秒硬件渲染冒烟测试
bash vtest-forward/run-l4d2-venus.sh --test

# 启动原生 Linux 32 位版
bash vtest-forward/run-l4d2-venus.sh --game
```

该方案目前标记为实验性：Venus/virglrenderer 增加了一层 IPC，性能和完整游戏
稳定性仍需实测。脚本只向目标进程设置 `VK_ICD_FILENAMES` 和
`VTEST_SOCKET_NAME`，不会替换系统 ICD；游戏退出后 64 位服务端随即清理。
稳定游玩仍优先使用上一节已验证的 Proton WoW64 方案。

## 四、历史方案（已废弃，仅存档）

原方案 = 原生 Linux 32 位 + `STEAM_RUNTIME=0` 宿主直跑 + Mesa llvmpipe 软渲染
（曾依次修过：容器挂起 → 32 位 `libopenal` 缺失致 `engine.so` 加载失败 →
32 位 `libpng12` 缺失致 `vguimatsurface.so` 加载失败进游戏黑屏）。2026-10-06
按「游戏禁用软件渲染」原则废弃：厂商无 32 位 gf 驱动，原生路线永远到不了 GPU。
为旧方案装的 i386 库与 `bin/libpng12.so.0` 保留无害。

## 五、相关

- 随从大师（32 位 Windows，同一 WoW64 配方 + IL2CPP patch）：`../minion-masters/`
- 刀塔霸业（Source 2 原生 Linux，容器委托宿主直跑）：`../underlords/`
