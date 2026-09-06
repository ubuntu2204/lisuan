# 砺算 7G100 与 Ubuntu 24.04 系统 Mesa(25.2.x)兼容性分析

> 环境:Ubuntu 24.04.4 LTS,内核 7.0.0-28-generic,Mesa 25.2.8-0ubuntu0.24.04.2,
> 厂商包 lisuangpu-dkms/lib 262.2.0.56,砺算 7G100(PCI `4c54:5006`)。
> 结论验证日期:2026-08-04,基于 Mesa 25.2.8 官方源码逐行核对 + 实机实验。

## 1. 结论(TL;DR)

- **系统 Mesa 25.2.8 无法原生加载厂商的 GL 驱动**(`gf_dri.so` / `pipe_gf.so`):
  不是配置问题,是**架构上不存在该机制**(见 §3,源码级证据)。
- **可用的"系统 Mesa"路径是 zink**:系统 Mesa 的 GL 实现(zink,GL-on-Vulkan)
  架在厂商 Vulkan ICD(`libvulkan_gf.so`,Vulkan 1.3)之上,**已实测可用**:
  - GLX:自动回退 zink,直接可用;
  - GBM:`GBM_BACKEND=dri` 时可用(主节点 card*);
  - EGL:需显式选择 Mesa vendor;其 GBM 平台目前会因厂商 `gf_gbm.so` 崩溃(见 §5)。
- zink 路径当前仅报告 **OpenGL 3.0**(厂商 Vulkan ICD 能力所致),
  厂商原生栈报告 OpenGL 4.6。要完整 GL 能力仍需厂商栈。
- **真正的"原生支持"= 厂商把 Gallium 驱动源码移植/上游化到新版 Mesa**;
  反编译 `pipe_gf.so` 重建驱动不现实(见 §6)。

## 2. 现状架构

厂商走的是标准插件式分发,但 GL 部分是一套**自包含的 Mesa 22.0.4 分叉**:

```
应用
 ├─ libGL/libEGL (glvnd 分发)
 │    ├─ 10_gf.json   -> libGLX_gf / libEGL_gf   (厂商,内置 Mesa 22.0.4,自带 libglapi_gf)
 │    │                    └─ 自行 dlopen dri/gf_dri.so (22.0.4 megadriver, 自带 Gallium 栈)
 │    └─ 50_mesa.json -> libGLX_mesa / libEGL_mesa (系统 Mesa 25.2.8)
 │                         └─ 静态 Gallium pipe-loader -> 无 gf 描述符 -> zink 回退(仅 GLX)
 ├─ libgbm (系统 25.2.8)
 │    ├─ gbm/gf_gbm.so   (厂商 22.0.4 后端;render 节点可用,primary 节点段错误!)
 │    └─ gbm/dri_gbm.so  (系统后端 -> zink/kms)
 └─ Vulkan: libvulkan_gf.so (厂商 ICD, 1.3)   ← zink 的底座,工作正常
```

关键事实:`gf_dri.so` 的 `NEEDED` 里只有 `libglapi_gf`/libdrm 等,
**不依赖系统 libgallium** —— 它是 22.0.4 时代的自包含 megadriver,
导出 `__driDriverGetExtensions_gf`,扩展表仅 4 项
(`DRI_Core v2, DRI_IMAGE_DRIVER v1, DRI_DRI2 v4, DRI_ConfigOptions v2`)。

## 3. 为什么 25.2.8 加载不了它(源码级证据)

对照 Mesa 25.2.8 源码(与本机二进制版本完全一致):

1. GLX/EGL/GBM 各自**内嵌** Gallium DRI 前端(`src/gallium/frontends/dri/dri_util.c`
   的 `driCreateNewScreen3` 被静态链入各目标),**不再有"dlopen 外部 megadriver"
   这一步**。`gf_dri.so` 这种 megadriver 格式在 25.2 没有任何加载者。
2. pipe 驱动由 `src/gallium/auxiliary/pipe-loader/pipe_loader_drm.c` 解析,
   其 `driver_descriptors[]` 是**编译期静态表**(i915/iris/radeonsi/…/zink,
   兜底 kmsro),25.2 已无 `u_dl`/dlopen 外部 `pipe_*.so` 的代码路径
   (动态 pipe 目标随 XvMC/VDPAU 等在旧版本移除)。
3. `src/loader/pci_id_driver_map.h` 无 `4c54` 条目;DRM 名来自
   `drmGetVersion()` = `gf`(注意:`/sys/.../uevent` 的 `DRIVER=lisuan` 是模块名,
   二者不一致 —— 这也是已删除的历史建链脚本存在的原因)。
4. GLX 有 zink 回退(`src/glx/dri3_glx.c:497` 对驱动名 `zink` 的特殊处理 +
   kopper);EGL **没有**(`src/egl/drivers/dri2/egl_dri2.c:749 dri2_create_screen`
   失败即返回,实测 device/GBM 平台报 "failed to create dri2 screen")。

实验佐证:强制 mesa GLX 加载时输出 `using driver gf for 4` →
`glx: failed to create dri3 screen` → `failed to load driver: gf` →
回退 `zink Vulkan 1.3(LISUAN 7G100 ... (MESA_RADV))`。

## 4. 实测矩阵(2026-08-04)

| 路径 | 结果 |
|---|---|
| 厂商栈 GLX/EGL(默认 glvnd 顺序) | ✅ OpenGL 4.6(Mesa 22.0.4 分叉) |
| 系统 Mesa GLX(`__GLX_VENDOR_LIBRARY_NAME=mesa`) | ✅ zink,**OpenGL 3.0** |
| 系统 Mesa EGL + Wayland/X11 平台(mesa vendor) | ⚠️ 可初始化但默认落 llvmpipe,需显式设备选择 |
| 系统 Mesa EGL device/GBM 平台 | ❌ "failed to create dri2 screen";GBM 平台触发 §5 崩溃 |
| GBM `GBM_BACKEND=dri`(card1) | ✅ 建 bo 成功 |
| GBM 厂商 `gf` 后端(renderD128) | ✅ 建 bo 成功 |
| GBM 厂商 `gf` 后端(card1/primary) | ❌ **段错误**(厂商驱动内部 `pil::gpu_memory_c::init_from_segment_info`) |
| Vulkan(厂商 ICD) | ✅ Vulkan 1.3 |

## 5. 已知地雷(勿动,但要知道)

1. **`/usr/lib/x86_64-linux-gnu/gbm/gf_gbm.so`**:任何进程经系统 libgbm
   默认链(DRM 名 `gf` → `gf_gbm.so`)在 **primary 节点**上会段错误。
   GNOME 合成器目前安然(gnome-shell 持有 card1 但未踩崩溃路径;
   mutter-x11-frames 走 renderD128)。**不要**为了"清理"而删除它:
   合成器内存里已经映射该库,且其 render 节点路径仍被依赖;
   规避方式是按需 `GBM_BACKEND=dri`(配方见 §7)。
2. **`MESA_LOADER_DRIVER_OVERRIDE` 救不了 EGL**:override 只影响驱动名解析,
   改变不了 §3.4 的"EGL 无 zink 回退"与 §3.2 的静态描述符表。
3. `drirc` 告警 `/usr/share/drirc.d/01-gf_drv.conf:<option> should be
   inside <application>`:厂商 XML 写法不合规,无害但会刷屏 debug 输出。

## 6. 为什么"反编译 pipe_gf.so"不是可行路线

`pipe_gf.so` 是 Gallium pipe 驱动二进制。要让系统 Mesa 用它,必须让它与
25.2.8 的 `pipe_screen`/`pipe_context` 等**内部 ABI** 对接 —— 这些结构体
在 22.0.4 → 25.2.8 之间多次增删字段(无稳定 ABI 承诺)。逆向一个完整
Gallium 驱动再对齐新 ABI,等于重写驱动,工程量以人年计,且无法验证正确性。
同理,给 25.2.8 的 `libgallium` 打二进制补丁加一个 "gf 描述符" 也不成立:
描述符指向的 create_screen 实现必须与 25.2 的 Gallium 内部一致。

**现实路径(按优先级)**:
1. 推动厂商发布 Gallium 驱动源码并移植到当前 Mesa(京微先耕 JM9、
   Moore Threads 等国产 GPU 均走上游化路线);zink  meanwhile 兜底。
2. 短期:应用按需切到系统 Mesa zink(配方见 §7,接受 GL 3.0 上限);
   重 GL 应用继续走厂商栈。
3. 厂商提升 Vulkan ICD 的特性覆盖(更多 VK features/extensions),
   zink 的 GL 版本上限会随之自动提高 —— 这是**不改 Mesa 就能受益**的方向。

## 7. 配方与验证工具

### 切到系统 Mesa zink 的环境变量配方

> 原包装脚本 `mesa/use-system-mesa.sh` 已删除(2026-09-06,无菜单入口、
> 无脚本依赖、无实际案例;配方内联于此,git 历史可找回)。三个变量按需组合:

```bash
export __GLX_VENDOR_LIBRARY_NAME=mesa
export __EGL_VENDOR_LIBRARY_FILENAMES=/usr/share/glvnd/egl_vendor.d/50_mesa.json
export GBM_BACKEND=dri        # 可选: 规避 gf_gbm.so 在 libgbm 25.2 下的段错误
```

- 前两个强制 glvnd 把 GLX/EGL 分发给 Mesa 实现(glvnd 按 JSON 文件名排序,
  `10_gf.json` 优先于 `50_mesa.json`,故必须显式指定);
- `GBM_BACKEND=dri` 让系统 libgbm 走发行版 dri 后端(zink/kms)。

对单个应用生效:

```bash
__GLX_VENDOR_LIBRARY_NAME=mesa \
__EGL_VENDOR_LIBRARY_FILENAMES=/usr/share/glvnd/egl_vendor.d/50_mesa.json \
GBM_BACKEND=dri  some-app --args
```

### 验证工具

| 文件 | 作用 |
|---|---|
| `mesa/verify-system-mesa.sh` | 只读体检:DRM 名、glvnd 配置、zink 回退、GBM dri 后端 |
| 本文档 | 源码级根因与实验记录 |

复现关键实验(无需改系统):

```bash
__GLX_VENDOR_LIBRARY_NAME=mesa glxinfo -B                      # zink 回退
GBM_BACKEND=dri ./gbm-zink-probe                                # dri 后端(见 _work/)
__EGL_VENDOR_LIBRARY_FILENAMES=/usr/share/glvnd/egl_vendor.d/50_mesa.json eglinfo -B
```
