# Bug 报告：砺算 7G100 用户态栈在 Ubuntu 24.04 / Steam 容器环境下的三个兼容性问题

| 字段 | 内容 |
|---|---|
| 报告编号 | LISUAN-7G100-UBUNTU2404-001 |
| 报告日期 | 2026-08-05（首次提交），2026-08-05 修订（补充问题三与实测更正） |
| 产品 / 版本 | lisuangpu-dkms / lisuangpu-lib **262.2.0.56**，GPU：砺算 7G100（PCI `4c54:5006`） |
| 操作系统 | Ubuntu 24.04 LTS（Mesa/libgbm 25.2.8），内核 7.0.0-28-generic（HWE） |
| 严重级别 | 问题一：高（Steam 客户端不可用）；问题二：中（UI 无硬件加速）；问题三：高（原生 Linux 游戏不可用） |
| 状态 | 三个问题均已在实机定位，均已验证可用的规避方案；等待厂商修复 |
| 附件 | 崩溃日志、core dump 分析记录、LD_DEBUG 加载链（见各章节内联） |

> 本报告由用户在真实环境完成定位与验证，附完整证据链，可直接用于厂商内部复现与跟进。
>
> 问题一览：
> 1. `gf_gbm.so` 在系统 libgbm 25.2.x 下段错误 → Steam 界面无法启动；
> 2. Steam 自带 ANGLE 在容器内符号解析崩溃（根因在 Steam 侧，但需要厂商
>    `libglapi_gf` 兜底，且暴露了容器内 EGL 分发问题）；
> 3. Steam 运行时容器内 GLX 无法加载厂商驱动（缺 GLX vendor 注册）→
>    原生 Linux 游戏（Dota Underlords 实测）启动即崩。

## 1. 环境

| 项目 | 值 |
|---|---|
| OS | Ubuntu 24.04 LTS |
| 内核 | 7.0.0-28-generic（HWE） |
| GPU | 砺算 7G100，PCI `4c54:5006`（另有音频功能 `4c54:5010`） |
| 厂商包 | lisuangpu-dkms / lisuangpu-lib 262.2.0.56 |
| 系统 Mesa/libgbm | 25.2.8-0ubuntu0.24.04.2 |
| 涉事库 | /usr/lib/x86_64-linux-gnu/gbm/gf_gbm.so（106 KB，2026-04 构建） |
| 宿主 | 海光 CPU + 砺算 7G100 |
| Steam | steam-launcher 1.0.0.87（buildid 1785799196），CEF 为 Chrome 126.0.6478.183，界面运行于 pressure-vessel（sniper）容器；游戏运行于 scout-on-soldier 容器 |
| 测试游戏 | Dota Underlords（appid 1046930，buildid 12606359，原生 Linux / Source 2） |

宿主桌面 GLX/EGL（glxinfo、Chrome、微信等）走厂商 libGLX_gf/libEGL_gf
均正常（OpenGL 4.6）；厂商 Vulkan ICD（libvulkan_gf.so，Vulkan 1.3）
在宿主与容器内均可正常枚举设备。

## 2. 问题一：gf_gbm.so 段错误（Steam 界面无法启动）

### 现象

Steam 界面（CEF/steamwebhelper）运行在 pressure-vessel 容器内，容器使用
系统 libgbm；libgbm 按 DRM 驱动名 `gf` 优先加载 `gf_gbm.so`，进程即崩溃
（`exit_code=139`），Chromium 反复重启 GPU/zygote 进程直至
`FATAL: GPU process isn't usable. Goodbye.`，Steam UI 永远起不来。

### 复现

1. 安装 lisuangpu-dkms/lib 262.2.0.56 与 Steam；
2. 启动 Steam；
3. 观察 `~/.local/share/Steam/logs/cef_log.txt`：
   `GPU process exited unexpectedly: exit_code=139` 反复出现。

最小化复现（不依赖 Steam）：任何进程经**系统 libgbm 25.2.x** 在
primary 节点（cardN）上创建 gbm_device 即触发
（`gbm_create_device()` → dlopen `gf_gbm.so` → 后端初始化崩溃）。
render 节点（renderD128）路径未观察到崩溃。

### 证据（core dump 分析）

对容器内崩溃进程开启 core dump 后用 gdb 分析：

```
Program terminated with signal SIGSEGV, Segmentation fault.
#0  0x0000000000000000 in ?? ()          <- 跳转到空指针
```

回溯中的返回地址全部命中 `gf_gbm.so` 的映射区间（`info proc mappings`
摘录，崩溃帧如 `0x74153b19997c`、`0x74153b1ad290`）：

```
0x74153b195000 - 0x74153b1ad000  /run/host/usr/lib/x86_64-linux-gnu/gbm/gf_gbm.so
```

崩溃发生在厂商 GBM 后端内部，而非系统 libgbm 或 Chromium 代码。

### 根因

GBM 后端以 dlopen 插件形式分发，`gf_gbm.so` 编译于 **Mesa 22.0.4**
时代的 `gbm_backend` 接口。Mesa 上游在 22.x → 25.x 之间多次调整后端
接口与内部结构（无稳定 ABI 承诺）。系统 libgbm 25.2.8 加载 22.0.4
时代的 `gf_gbm.so` 时，后端描述表/回调指针布局错位，初始化路径上
通过空函数指针跳转 → SIGSEGV。

与厂商 GLX/EGL 栈不同（libGLX_gf/libEGL_gf 自带完整 Mesa 22.0.4 分叉、
自成体系），`gf_gbm.so` 作为**插件**必须与宿主 libgbm 版本匹配，因此
唯独它在新的系统 Mesa 下失效。

### 规避（已验证）

`export GBM_BACKEND=dri`——强制系统 libgbm 使用发行版自带的 dri 后端，
崩溃消失。

## 3. 问题二：容器内 CEF 的 EGL/GL 不可用（Steam 界面无硬件加速）

修复问题一后，Steam 容器的 GPU 进程不再段错误，但硬件合成仍不可用，
期间还踩到一层 Steam 侧崩溃。逐层结论：

1. **Steam 自带 ANGLE 符号解析崩溃（Steam 侧问题，厂商库可兜底）**：
   Steam 在 `ubuntu12_64/` 自带 ANGLE（`libEGL.so`/`libGLESv2.so`），
   被 `LD_LIBRARY_PATH=.` 优先加载；其延迟绑定的 GL API 符号
   （`glVertexAttribIPointer` 等数百个）在容器内找不到 glapi 提供者，
   ld.so 报 "symbol lookup error ... (fatal)" → SIGSEGV（exit_code=139/11）。
   规避：`LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libglapi_gf.so.0`
   （`LD_DEBUG=libs` 追踪证实了完整加载链）。
2. **ANGLE 无法创建后端 GL 上下文（容器内 EGL 分发问题）**：
   ANGLE 报 `Could not create a backing OpenGL context`
   （EGL_NOT_INITIALIZED）。注意：**厂商 EGL 本身是好的**——在宿主实测
   `eglGetPlatformDisplayEXT(EGL_PLATFORM_X11)` + `eglInitialize` 成功
   （EGL 1.5，vendor=Lisuan；支持 X11/XCB/Wayland/GBM/surfaceless/device
   平台）。容器内失败的真正原因是 glvnd 的 EGL 分发默认落到**系统 Mesa
   EGL**（无人把 CEF 指到厂商 EGL），而系统 Mesa EGL 对本卡无可用后端
   （无 zink 回退，见 `docs/system-mesa-analysis.md` §3.4）。
3. **ANGLE Vulkan 后端可枚举设备但初始化不完整**：`libvulkan_gf.so` 在
   容器内工作正常（设备枚举成功），Steam 的旧版 ANGLE 对 `4c54` 架构
   报 `initFeatures: Unknown GPU architecture` 警告后 GPU 进程干净退出
   （非崩溃），CEF 最终回退软渲染。

当前规避：问题一的 `GBM_BACKEND=dri` + 上述 `LD_PRELOAD`，Steam UI 以
软渲染稳定运行。

## 4. 问题三：容器内 GLX 无厂商 vendor 注册（原生游戏无法启动）

### 现象

原生 Linux 游戏（Dota Underlords，Source 2，GLX 建上下文）在 Steam
运行时容器（scout-on-soldier）内启动即崩：

```
libGL error: No driver found
libGL error: failed to load driver: (null)
X Error of failed request:  BadValue ... X_GLXCreateContext
```

### 定位过程与证据

容器内环境诊断（游戏进程内实测）：

- `/dev/dri/card1` 可访问，`drmGetVersion()` 返回 `name=gf`；
- pressure-vessel 已把厂商库正确合并进容器：
  `/usr/lib/pressure-vessel/overrides/lib/x86_64-linux-gnu/` 下有
  `libGLX_gf.so.0`、`libEGL_gf.so.0`、`libglapi_gf.so.0`、`dri/gf_dri.so`，
  `share/glvnd/egl_vendor.d/` 下有 `10_gf.json`/`50_mesa.json`；
- 但游戏拿到的 GLX 始终分发到系统 Mesa（容器内 mesa 不认识 `gf`，
  报 `failed to load driver: (null)`）。

根因：**glvnd 的 GLX vendor 注册机制对厂商栈失效**：

1. glvnd 从 `/usr/share/glvnd/egl_vendor.d/*.json` 枚举 vendor，该目录
   **在容器内不存在**；pressure-vessel 合并的 JSON 在
   `/usr/lib/pressure-vessel/overrides/share/glvnd/egl_vendor.d/`，
   glvnd 并不读取该路径；
2. 厂商只提供了 **EGL** 的 vendor 配置（`10_gf.json` →
   `libEGL_gf.so.0`），没有 **GLX** 的 vendor 配置；该库不导出
   `__glx_Main`，即使被读取也无法注册 GLX vendor（`libGLX_gf.so.0`
   才导出 `__glx_Main`，需要独立的 JSON 条目）；
3. 宿主上 `__GLX_VENDOR_LIBRARY_NAME=gf` 可用，是因为 glvnd 的名称回退
   会在默认链接路径 dlopen `libGLX_gf.so.0`；容器内 overrides 目录不在
   默认 dlopen 搜索路径，回退同样失败。

已尝试并排除的手段（全部无效，GLX 一律回退系统 Mesa）：
`__GLX_VENDOR_LIBRARY_NAME=gf`、`__GLX_FORCE_VENDOR_LIBRARY_0=gf`、
`__EGL_VENDOR_LIBRARY_DIRS` 指向 overrides 合并目录并自建指向
`libGLX_gf.so.0` 的 GLX vendor JSON、Source 2 的 `-egl` 参数、
`SDL_VIDEO_X11_FORCE_EGL=1`、改用纯 soldier 运行时。

### 规避（已验证）

绕过 Steam 运行时容器，在宿主直跑（宿主厂商 GLX/Vulkan 均正常，Steam
客户端保持运行以提供授权）：

```bash
STEAM_RUNTIME=0 ~/.local/share/Steam/steamapps/common/Underlords/game/underlords.sh -steam -vulkan
```

注：Ubuntu 24.04 已移除 libpng12，宿主直跑需把 `libpng12.so.0`（可取自
18.04 归档）放入游戏 `bin/linuxsteamrt64/` 目录，否则 Source 2 的
pango 文本模块初始化失败。

## 5. 已验证的规避方案汇总

| 措施 | 效果 |
|---|---|
| `GBM_BACKEND=dri` | ✅ gf_gbm 段错误消失（问题一） |
| `LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libglapi_gf.so.0` | ✅ 容器内 ANGLE 符号解析崩溃消失（问题二第 1 层） |
| 两者叠加（注入 `steamwebhelper_sniper_wrap.sh` 并 `chattr +i` 锁定） | ✅ Steam UI 稳定（GPU 进程干净失败后自动软渲染） |
| `STEAM_RUNTIME=0` 宿主直跑游戏 + 补 libpng12 | ✅ Dota Underlords 正常运行（问题三） |

## 6. 对厂商的建议

1. **针对当前系统 Mesa 重新构建 `gf_gbm.so`**，或改为自带 libgbm 的
   方案（与厂商 GLX/EGL 栈同思路），消除对宿主 libgbm ABI 的依赖；
   至少应在初始化入口做版本探测，不匹配时返回错误码而非崩溃
   （libgbm 会回退下一后端），避免拖垮整个进程；
2. **补齐 glvnd vendor 配置**：随包提供 GLX 的 vendor JSON
   （指向 `libGLX_gf.so.0`，与现有 EGL JSON 并列）；并考虑与
   pressure-vessel 的合并路径（`/usr/lib/pressure-vessel/overrides/
   share/glvnd/egl_vendor.d/`）兼容的分发方式，使容器化应用
   （Steam UI、原生游戏）能注册厂商 GLX/EGL vendor；
3. **容器/沙箱场景的 EGL 可用性**：厂商 EGL（X11 等平台）在宿主实测
   可用，但容器内的应用（CEF/ANGLE、SDL）默认分发到系统 Mesa EGL 而
   拿不到厂商 EGL；建议在文档中给出官方推荐的环境变量组合
   （如 `__EGL_VENDOR_LIBRARY_FILENAMES`），或提供统一的容器适配脚本；
4. 持续提升 Vulkan ICD 的特性/扩展覆盖（容器内设备枚举与创建已验证
   正常），使 zink/ANGLE 等 GL-on-Vulkan 方案的可用性随之提高；
5. 长期：将 Gallium 驱动上游化到当前 Mesa（同行已有先例），从根上
   消除"分叉 Mesa 插件 vs 系统 Mesa"的 ABI 漂移。

## 7. 附件清单

- Steam CEF 崩溃日志样本：`~/.local/share/Steam/logs/cef_log.txt`
  （exit_code=139 段）
- 游戏崩溃 stderr 样本（GLX 失败 + X Error）与 gdb 分析记录：见 §2/§4
- 兼容性背景分析：`docs/system-mesa-analysis.md`
- 修复脚本：`scripts/fix-steam-cef-lock.sh`（问题一、二的注入与锁定）
- 游戏宿主直跑命令：见 §4"规避"
