# Bug 报告：gf_gbm.so 在系统 libgbm 25.2.x 下段错误（Steam CEF 无法启动）

> 状态：已在实机定位并验证规避方案（2026-08-05）。本报告附完整证据链，
> 可直接提交厂商支持渠道。

## 1. 摘要

砺算 7G100 的用户态库 `gf_gbm.so`（GBM 后端插件，随 `lisuangpu-lib`
262.2.0.56 分发，构建于 Mesa 22.0.4 分叉）在 **Ubuntu 24.04 系统
libgbm 25.2.8** 下被加载时发生**段错误（跳转到空函数指针）**。

该崩溃直接导致 **Steam 无法启动**：Steam 界面（CEF/steamwebhelper）
运行在 pressure-vessel 容器内，容器使用系统 libgbm；libgbm 按 DRM
驱动名 `gf` 优先加载 `gf_gbm.so`，进程即崩溃（`exit_code=139`），
Chromium 反复重启 GPU/zygote 进程直至 `FATAL: GPU process isn't
usable. Goodbye.`，Steam UI 永远起不来。

规避方案（已验证）：`GBM_BACKEND=dri` 强制使用发行版自带的 dri 后端。

## 2. 环境

| 项目 | 值 |
|---|---|
| OS | Ubuntu 24.04 LTS |
| 内核 | 7.0.0-28-generic（HWE） |
| GPU | 砺算 7G100，PCI `4c54:5006`（另有音频功能 `4c54:5010`） |
| 厂商包 | lisuangpu-dkms / lisuangpu-lib 262.2.0.56 |
| 系统 Mesa/libgbm | 25.2.8-0ubuntu0.24.04.2 |
| 崩溃库 | /usr/lib/x86_64-linux-gnu/gbm/gf_gbm.so（106 KB，2026-04 构建） |
| 宿主 | 海光 CPU + 砺算 7G100 |
| 触发场景 | Steam（steam-launcher 1.0.0.87，buildid 1785799196），其 CEF 为 Chrome 126.0.6478.183，运行于 pressure-vessel（sniper）容器 |

桌面 GLX/EGL（宿主进程，如 glxinfo、Chrome、微信）走厂商
libGLX_gf/libEGL_gf 均正常（OpenGL 4.6）；**仅 GBM 路径崩溃**。

## 3. 复现步骤

1. 安装 lisuangpu-dkms/lib 262.2.0.56 与 Steam；
2. `MESA_LOADER_DRIVER_OVERRIDE=gf steam -no-cef-sandbox -cef-force-64bit`；
3. 观察 `~/.local/share/Steam/logs/cef_log.txt`：
   `GPU process exited unexpectedly: exit_code=139` 反复出现，
   最终 `FATAL ... GPU process isn't usable. Goodbye.`。

最小化复现（不依赖 Steam）：任何进程经**系统 libgbm 25.2.x** 在
primary 节点（cardN）上创建 gbm_device 即触发
（`gbm_create_device()` → dlopen `gf_gbm.so` → 后端初始化崩溃）。

## 4. 证据链（core dump 分析）

对容器内崩溃进程开启 core dump 后用 gdb 分析（`ulimit -c unlimited`，
core_pattern 指向 /tmp）：

```
Program terminated with signal SIGSEGV, Segmentation fault.
#0  0x0000000000000000 in ?? ()          <- 跳转到空指针
```

回溯中的返回地址落在以下映射区间（`info proc mappings` 摘录）：

```
0x74153b195000 - 0x74153b197000  /run/host/usr/lib/x86_64-linux-gnu/gbm/gf_gbm.so  (只读段)
0x74153b197000 - 0x74153b19f000  gf_gbm.so (text)
0x74153b19f000 - 0x74153b1ab000  gf_gbm.so (text)
0x74153b1ab000 - 0x74153b1ad000  gf_gbm.so (rodata/data)
0x74153b1ad000 - 0x74153b1ae000  gf_gbm.so (data)
```

崩溃帧地址（如 `0x74153b19997c`、`0x74153b1ad290`、`0x74153b19a5ea`）
全部命中 **gf_gbm.so** 的代码/数据段——崩溃发生在厂商 GBM 后端内部，
而非系统 libgbm 或 Chromium 代码。

对照日志：同一机器上宿主 Chromium 系应用（Chrome、QQ 等）的 GPU 进程
全部正常，只有 Steam 容器内崩溃——差异仅在于容器进程经由系统
libgbm 默认后端解析链（DRM 名 `gf` → `gf_gbm.so`）踩中该库。

## 5. 根因分析

GBM 后端以 dlopen 插件形式分发，`gf_gbm.so` 编译于 **Mesa 22.0.4**
时代的 `gbm_backend` 接口。Mesa 上游在 22.x → 25.x 之间多次调整
后端接口与内部结构（无稳定 ABI 承诺）。系统 libgbm 25.2.8 加载
22.0.4 时代的 `gf_gbm.so` 时，后端描述表/回调指针布局错位，
初始化路径上通过空函数指针跳转 → SIGSEGV。

与厂商 GLX/EGL 栈不同（libGLX_gf/libEGL_gf 自带完整 Mesa 22.0.4
分叉、自成体系），`gf_gbm.so` 作为**插件**必须与宿主 libgbm 版本
匹配，因此唯独它在新的系统 Mesa 下失效。

## 6. 已验证的规避方案

| 措施 | 效果 |
|---|---|
| `GBM_BACKEND=dri`（让系统 libgbm 走发行版 dri_gbm.so → zink/kms） | ✅ gf_gbm 崩溃消失 |
| `LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libglapi_gf.so.0` | ✅ 容器内 ANGLE 符号解析崩溃消失 |
| 两者叠加 + 不加任何 Chromium 标志 | ✅ Steam UI 稳定（GPU 进程干净失败后自动软渲染；游戏仍用 GPU） |

## 7. 附带发现（同环境第二个问题，已定位）

Steam 容器内 CEF GPU 进程崩溃（exit_code=139/11），经 `LD_DEBUG=libs`
追踪定位到两层原因：

1. **Steam 自带 ANGLE 符号解析失败（直接崩溃原因，非厂商库问题）**：
   Steam 在 `ubuntu12_64/` 自带 ANGLE（`libEGL.so`/`libGLESv2.so`），
   被 `LD_LIBRARY_PATH=.` 优先加载；其延迟绑定的 GL API 符号
   （`glVertexAttribIPointer` 等数百个）在容器内找不到 glapi 提供者，
   ld.so 报 "symbol lookup error ... (fatal)" → 进程 SIGSEGV。
   规避：`LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libglapi_gf.so.0`。
2. **修复后不再崩溃，但硬件合成仍不可用（厂商相关）**：
   ANGLE 的 EGL 后端报 `Could not create a backing OpenGL context`
   （EGL_NOT_INITIALIZED）——厂商 EGL 无可用 X11 平台，系统 Mesa EGL
   又无 zink 回退；ANGLE Vulkan 后端能枚举到设备（`libvulkan_gf.so`
   在容器内工作正常），但旧版 ANGLE 不认识 `4c54` 架构
   （`initFeatures: Unknown GPU architecture`）后 GPU 进程退出，
   CEF 最终回退软渲染。

对厂商的附加建议：**提供可用的 EGL（至少 X11/surfaceless 平台）**，
这是 Steam UI/各类容器化应用获得硬件加速的前提；同时建议提升
Vulkan ICD 对通用特性/扩展的覆盖，使 ANGLE/zink 等 GL-on-Vulkan
方案可用。

## 8. 对厂商的建议

1. **针对当前系统 Mesa 重新构建 `gf_gbm.so`**，或改为静态/自带
   libgbm 的方案（与厂商 GLX/EGL 栈同思路），消除对宿主 libgbm
   ABI 的依赖；
2. 在 `gf_gbm.so` 初始化入口做版本探测，不匹配时返回错误码而非
   崩溃（libgbm 会回退下一后端），避免拖垮整个进程；
3. 排查 §7 的容器内 Vulkan ICD 初始化崩溃；
4. 长期：将 Gallium 驱动上游化到当前 Mesa（同行已有先例），
   从根上消除"分叉 Mesa 插件 vs 系统 Mesa"的 ABI 漂移。

## 9. 附件清单

- 崩溃日志样本：`~/.local/share/Steam/logs/cef_log.txt`（exit_code=139 段）
- gdb 分析记录（回溯 + 映射）：见本文 §4
- 兼容性背景分析：`docs/system-mesa-analysis.md`
- 修复脚本（含复现/规避步骤）：`scripts/fix-steam-cef-lock.sh`
