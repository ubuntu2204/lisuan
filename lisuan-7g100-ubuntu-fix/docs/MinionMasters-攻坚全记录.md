# 《随从大师》在 Ubuntu 24.04 + 砺算 7G100 上的攻坚全记录

> 时间跨度：2026-08-28 ~ 08-29
> 目标：让 32 位游戏《Minion Masters》(AppID 489520) 用上砺算 7G100 **硬件渲染**
> 结论：软件渲染可用；硬件渲染已定位到确切机理并给出补丁，仍在收尾验证

---

## 零、一句话概括核心矛盾

```
游戏是 32 位 exe，砺算 gf 只发布 64 位 Vulkan 驱动
        ↓ 必须走 Proton 的 WoW64（32位代码 thunk 调用 64位驱动）
        ↓ gf 的 vkMapMemory 返回的 CPU 指针在高地址（实测 0x740bdfaec000）
        ↓ wine 的 WoW64 thunk 无法把 >4GB 指针交给 32 位代码
        ↓ wine 主动 unmap 并合成 VK_ERROR_OUT_OF_HOST_MEMORY
        ↓ DXVK 分配显存失败 → d3d11: failed to create buffer → 游戏崩溃
```

所有尝试都是围绕"如何让映射地址落进 4GB"展开的。

---

## 一、环境基线

| 项 | 值 |
|---|---|
| 系统 | Ubuntu 24.04.4 LTS，内核 7.0.0-30 |
| 桌面 | **Wayland**（`DISPLAY=:0`，经 XWayland） |
| CPU | Hygon C86-4G，16 核 |
| 显卡 | 砺算 7G100（`4c54:5006`），驱动 `lisuangpu 262.2.0.56`（Vulkan 内部版本 gf 22.0.4） |
| Proton | Experimental（wine 11.0） |
| 游戏 | Unity 2020.3.44f1，32 位 exe |

---

## 二、尝试历程（按时间顺序）

### 阶段 1 — 摸底：确认游戏本身没问题

**做了什么**：扫描磁盘/引导环境，定位游戏安装，抓 `Player.log`。

**发现**：日志只到 11 行就停，最后一行：
```
d3d11: failed to create buffer (target 0x1 mode 0 size 20) [0x80070057]
```
游戏是 **32 位 PE**（`Machine=0x14c`），而 gf 只有 **64 位** `libvulkan_gf.so`。

### 阶段 2 — 让 7G100 被识别（成功）

三个前置障碍，逐个解决：

| 障碍 | 根因 | 解法 |
|---|---|---|
| 枚举到 0 个 GPU | gf 读 `/etc/os-release` 只认 Kylin/uos/openEuler/Deepin/Ubuntu，Steam 容器不在名单 | `LD_PRELOAD` hook 重定向 `/etc/os-release` |
| `vkCreateDevice` 返回 -1 | gf 依赖伴生库 `libSC.so`（着色器编译器），容器找不到 | 软链 `libSC.so` 进 Proton 库目录（容器内路径要带 `/run/host` 前缀） |
| DXVK 初始化失败 | Proton 自带 DXVK v3.0.2 硬要求 `VK_KHR_maintenance5`，gf 不支持 | 换成 Proton 8.0 的 **DXVK v2.3** |

**结果**：`Renderer: LISUAN 7G100 Series Graphics (ID=0x5006)` ✅，但仍在 buffer 创建处崩溃。

> ⚠️ 踩坑：直接改 Proton 自带文件会触发 Steam **"内容文件损坏"**。
> 正解：复制 Proton 为自定义 **`Proton - MM GF`**（Steam 不更新自定义 Proton），在副本里改。

### 阶段 3 — 定位硬件路径的崩溃根因（成功）

开 `DXVK_LOG_LEVEL=debug` 拿到：
```
err: DxvkMemoryAllocator: Mapping memory failed with VK_ERROR_OUT_OF_HOST_MEMORY  (×18)
err: DxvkMemoryAllocator: Memory allocation failed
err:   Size: 4194304   Alignment: 16   Mem types: 0,1,2,3
err: Heap 0: Size 12288 MiB  Allocated 16  Used 0  Reserved 996  Budget 12288
```

**关键判读**：显存 12GB 只用了 16MB，`Used: 0` —— **不是显存不足**，是"映射"这一步失败。

**决定性验证**：写 Python ctypes + C 程序在**原生 64 位**环境直接调 gf：
- `vkMapMemory`（type 0/2/3，4MiB）→ **全部成功**
- `vkCreateBuffer`（20 字节 vertex buffer，各种 usage 组合）→ **全部成功**
- 返回指针 `0x7625ebf1a000` —— **高位地址**

→ 驱动无辜，是"地址宽度不匹配"。

### 阶段 4 — 修复软件渲染下的闪退（成功，重要副产品）

用 llvmpipe 时游戏能启动但会闪退。对比 8/26 的历史成功日志发现：

```
成功时： [VivoxChatServiceAdapter] Login started with token e30...
         [ChatService] OnLoginStateChanged: LoggedIn
闪退时： （完全没有 VivoxChatServiceAdapter 的日志）
```

**根因**：自研补丁 `MMPatch.dll` 把 `VivoxChatServiceAdapter.LoginAsync/LogoutAsync`
patch 成了空操作，游戏拿不到 token、登录回调永不触发，卡死后退出。

**修复**：注释掉 `PatchVivox()`，只保留新闻图片 patch，重新 `dotnet build`。

**结果**：llvmpipe 下稳定进游戏，走到 `CardCollectionViewModel`（主菜单内）+ `LoggedIn`。

### 阶段 5 — Vulkan ICD Shim（方案 A，失败但有重要收获）

**思路**：参考 `vulkan-adreno-shim`（同样问题的先例），做**零拷贝 dmabuf 低位重映射**：
拦截 `vkMapMemory` → 导出 dmabuf fd → `mmap(MAP_SHARED|MAP_32BIT)` → 返回低位指针。

**原生验证：完全成功** ✅
```
type[0] vkMapMemory OK ptr=0x40eb7000
type[2] vkMapMemory OK ptr=0x40c1a000
type[3] vkMapMemory OK ptr=0x4053c000      ← 含 DEVICE_LOCAL+HOST_VISIBLE
```
gf 支持 `VK_KHR_external_memory_fd` / `VK_EXT_external_memory_dma_buf` /
`VK_KHR_external_memory` / `VK_KHR_dedicated_allocation`（102 个扩展中确认）；
不支持 `VK_EXT_map_memory_placed`、`VK_EXT_external_memory_host`。

**wine 集成：失败** ❌

### 阶段 6 — 读 wine 源码搞清"为何不启用 shim"（成功，关键认知修正）

读 `wine-11.0` 的 `dlls/winevulkan/loader.c` + `vulkan.c`，纠正了此前的误解：

```c
/* loader.c */
#define WINE_VULKAN_ICD_VERSION 4
void * WINAPI vk_icdGetInstanceProcAddr(VkInstance instance, const char *name)
{
    return vkGetInstanceProcAddr(instance, name);   /* winevulkan.dll 自己就是 ICD */
}
```

```c
/* vulkan.c — init_vulkan() */
vk_funcs = __wine_get_vulkan_driver(WINE_VULKAN_DRIVER_VERSION);
p_vkCreateInstance = (PFN_vkCreateInstance)vk_funcs->p_vkGetInstanceProcAddr(NULL, "vkCreateInstance");
```

真正的调用链是**三层**：
```
DXVK(d3d11.dll) → vulkan-1.dll
  → winevulkan.dll              【第1层：对系统 loader 而言的 ICD】
    → UNIX_CALL(...)            【转入 Unix 侧 winevulkan.so】
      → __wine_get_vulkan_driver()  【第2层：wine 自己的图形驱动 vk_funcs】
        → 物理 ICD (gf / 我们的 shim)  【第3层】
```

**"查询过 vkCreateInstance 却从不调用"的真相**：
`init_vulkan()` 只是**查询**函数指针存起来，真正创建 instance 走的是
`vk_funcs->p_vkCreateInstance`（wine 驱动自己的实现）。

后续加 `WINEDEBUG=+vulkan` 抓到真正错误：
```
win32u_vkCreateInstance Failed to create vulkan instance, res -7
```
（`-7` = `VK_ERROR_FEATURE_NOT_PRESENT`）

### 阶段 7 — 方案 A 深入：经系统 loader 转发（三条路全部堵死）

因为"直连 gf 成功、经 shim 失败"，判断 gf 依赖标准 loader 的加载上下文，于是尝试让 shim 改用标准 loader：

| 路径 | 结果 | 原因 |
|---|---|---|
| `dlmopen(LM_ID_NEWLM)` | ❌ -9 | 新命名空间重载 libc → `cannot allocate memory in static TLS block`；即便成功，gf 依赖（libSC 等）在新命名空间加载不了 |
| 同命名空间 dlopen 改过 SONAME 的副本 | ❌ 崩溃 | 两份 loader 符号冲突；且 loader 惰性初始化，环境变量一恢复就去加载 shim 自身 → `Syscall stack overrun`（gipa 被调 190 次） |
| gf 运行时自检环境变量假设 | ❌ 证伪 | 转发前把 `VK_ICD_FILENAMES` 改回 gf，仍返回 -7 |

**结论**：方案 A 需要"独立于 wine 的第二个 loader 实例"，而容器内要么隔离过度（依赖加载不了），要么隔离不足（符号冲突/递归）——这个窗口不存在。

顺带产出：
- `patch_soname.py`：纯 Python 改 ELF 的 `DT_SONAME`（patchelf 未装），`libvulkan.so.1` → `libvk_gfs.so.1` 验证成功
- 修掉 clang 报错 `function_definition_not_allowed`：原因是函数内写了 GCC 嵌套函数 `is_drop()`，改为文件级函数

### 阶段 8 — 方案 B：patch 驱动（进行中，已见成效）

**定位**（`nm` + `objdump`）：

gf 用**动态链接的 `mmap64`**（`U mmap64@GLIBC_2.2.5`），全库仅 6 处调用。
其中 `0x17a07a` 落在 `pil::device_icdlinux_c::map_buffer_cb()`（0x17a000）内：

```asm
17a060: mov 0x8(%rbp),%r9      ; offset
17a064: mov 0x78(%rbx),%r8d    ; fd
17a068: xor %edi,%edi          ; addr = NULL
17a06a: b9 01 00 00 00  mov $0x1,%ecx    ; ← flags = MAP_SHARED   【patch 点】
17a06f: ba 03 00 00 00  mov $0x3,%edx    ; prot = PROT_READ|PROT_WRITE
17a07a: e8 61 73 f0 ff  call mmap64@plt
```

**补丁**：`0x17A06B` 的 `0x01` → `0x41`（`MAP_SHARED|MAP_32BIT`）—— **单字节**。
（该 PT_LOAD 段 vaddr == offset，故文件偏移 == 虚拟地址）

**宿主机实测**（`patch_gf_map32.py --apply` 后）：

| | `vkMapMemory` 返回 |
|---|---|
| 打补丁前 | `0x740bdfaec000` ❌ |
| 打补丁后 | **`0x43f68000`** ✅ |

4MB / 16MB / 64MB / 128MB / 256MB 全部成功落进低 4GB。

**应用到系统驱动后跑游戏**：
- ❌ 仍 11 行、buffer 错误 1
- ✅ 但错误码变了：`VK_ERROR_OUT_OF_HOST_MEMORY` → **`VK_ERROR_MEMORY_MAP_FAILED`**
  → 说明补丁**确实生效了**（走到了真实 mmap），但 mmap 本身失败

**当前正在排查**：`vkMapMemory` 的调用链是
```
pil_gf_map_memory (0x1b8e70)
  → pil::gpu_memory_c::map() (0x180920)
    → pil::gpu_memory_mgt_c::mgt_map() (0x1808e0)   ← 虚函数分发层
      → call *0x20(%rax)   ← 虚调用，实际实现取决于对象类型
```
即**真正做 mmap 的可能不止 `map_buffer_cb` 一条路径**，需要确认游戏实际走哪个
`gpu_memory_mgt_c` 子类（已找到 `pil::gpu_memory_icdlinux_c` 的 vtable @ `0x6e1650`）。

---

## 三、遇到的问题汇总

| # | 问题 | 根因 | 解决 |
|---|---|---|---|
| 1 | gf 枚举到 0 个 GPU | 发行版白名单 | LD_PRELOAD 重定向 os-release |
| 2 | `vkCreateDevice` 返回 -1 | 缺 `libSC.so` | 软链进 Proton 库目录（容器内用 `/run/host` 前缀） |
| 3 | Steam 报"内容文件损坏" | 改了 Proton 自带 DXVK | 改用自定义 Proton 副本 |
| 4 | DXVK 初始化失败 | v3.0.2 要求 maintenance5 | 换 v2.3 |
| 5 | 游戏闪退 | MMPatch 的 Vivox patch | 去掉该 patch 重编译 |
| 6 | 手动跑 Proton 报 `VK_KHR_surface not supported` | Wayland 下缺 WSI | 必须经 Steam Runtime 容器启动 |
| 7 | 容器内 `dlopen` 失败（gf 路径） | 容器内宿主机根挂载在 `/run/host` | 自动探测两种布局 |
| 8 | shim 未被游戏进程加载 | wine 三层架构，物理 ICD 由图形驱动层加载 | 提供 negotiate/gdpa 后部分解决 |
| 9 | `vkCreateInstance` 返回 -7 | 未做 ICD 版本协商 | shim 自行提供 negotiate |
| 10 | `dlmopen` 失败 | 静态 TLS 块上限 | 改用同命名空间 + 改 SONAME |
| 11 | 改 SONAME 后游戏 0 行崩溃 | 两份 loader 符号冲突 + 惰性初始化导致递归 | 预触发 loader 初始化 |
| 12 | 补丁后 mmap 仍失败 | 映射路径可能是虚函数分发的另一条实现 | **排查中** |
| 13 | 容器内测试 device 创建返回 -1 | 测试脚本缺 wine 提供的环境（非 gf 问题） | 不阻塞，游戏本身能创建 device |

---

## 四、当前状态

**可用（已验证）**：llvmpipe 软件渲染，能进游戏到主菜单并登录。
```
VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json PROTON_USE_WOW64=1 \
LP_NUM_THREADS=16 __GLX_VENDOR_LIBRARY_NAME=mesa %command%
```
兼容工具选 `Proton - MM GF`。

**已打补丁（系统驱动）**：`libvulkan_gf.so` 偏移 `0x17A06B` = `0x41`，宿主机验证映射成功，容器内也确认读到的是补丁版。
- 还原：`sudo python3 ~/project/lisuan/gf-lowmem-shim/patch_gf_map32.py --restore`

**待收尾**：确认 `vkMapMemory` 实际走哪条映射路径，必要时对其余路径同样加 `MAP_32BIT`。

---

## 五、下一步（按优先级）

1. **继续方案 B**：解析 `pil::gpu_memory_icdlinux_c` 的 vtable（0x6e1650），
   定位其 `mgt_map` 实现，确认是否也调用 `map_buffer_cb`；若走别的路径，对那处 mmap 同样打补丁。
2. 备选：用 gdb 在游戏进程里对 `mmap64@plt` 下断点，直接看失败的调用来自哪个函数、errno 是什么。
3. 备选：改 patch 策略——`MAP_32BIT` 限制在低 **2GB**，若空间紧张可改用低 4GB 的 hint 地址
   （需更大改动的 patch，因为 `xor %edi,%edi` 只有 2 字节，放不下 5 字节的 mov 立即数）。

---

## 附录：产出物清单

| 路径 | 说明 |
|---|---|
| `~/project/lisuan/gf-lowmem-shim/gf_lowmem_icd.c` | Vulkan ICD shim（方案 A 实现，原生验证通过） |
| `~/project/lisuan/gf-lowmem-shim/patch_gf_map32.py` | **驱动补丁工具**（带备份/回滚，方案 B） |
| `~/project/lisuan/gf-lowmem-shim/patch_soname.py` | 纯 Python 改 ELF SONAME |
| `~/project/lisuan/gf-lowmem-shim/run-test.sh` | 多模式对照测试脚本 |
| `~/project/lisuan/gf-lowmem-shim/analyze_lowmem.py` | 进程低地址空间占用分析 |
| `~/project/lisuan/gf-lowmem-shim/verify_patch.py` | 容器内校验驱动补丁字节 |
| `/tmp/vk_dmabuf_32bit_test.c` | dmabuf 低位映射可行性验证程序 |
| `~/project/lisuan/lisuan-7g100-ubuntu-fix/MinionMasters-成功运行报告.md` | llvmpipe 成功运行报告 |
| `~/.local/share/Steam/steamapps/common/Proton - MM GF/` | 自定义 Proton（DXVK v2.3 + libSC） |

---

## 六、2026-08-29 补记：硬件渲染全链路打通（容器运行时更新适配）

### 背景

方案 B 之后，Steam 容器运行时（steamrt4）与 Vulkan loader 更新，硬件路径出现**新的一批回归**，
全部定位并解决，硬件渲染链路首次端到端打通。

### 新发现的四个回归及解法

| # | 现象 | 根因 | 解法 |
|---|---|---|---|
| 14 | loader 找到 gf ICD 清单却从不加载库；`vkCreateDevice` 段错误 | 容器内 `/run/host/usr/lib` 不再可见，`libvulkan_gf.so`/`libSC.so` 从该路径消失 | 两库**复制到 `~/gf-compat/lib/`**（home 在容器内一定可见），自定义 `gf_icd.json` + `VK_ICD_FILENAMES` 指过去，libSC 软链改指 home 副本 |
| 15 | `vkCreateInstance` 被拒：`VK_KHR_external_semaphore_capabilities not supported by available ICDs` | ① 新 loader（1.4.309）对 gf 这种**协商版本 0、只导出 3 个 `vk_icd*` 符号**的老 ICD 走"直接导出符号表"路径，静默丢弃；② 厂商裁掉了该实例扩展，新 loader 严格校验 | **协商垫片 `libvulkan_gf_shim.so`**：对外报协商版本 5、全程 `vk_icdGetInstanceProcAddr` 转发；枚举时补报被裁扩展，`vkCreateInstance`/`vkCreateDevice` 转发前剔除真 ICD 不支持的实例/设备扩展 |
| 16 | 垫片生效后仍在 libc 段错误 | **Steam overlay Vulkan 层**经 `vk_icdGetPhysicalDeviceProcAddr` 直连真 ICD，绕过垫片 | `VK_LOADER_LAYERS_DISABLE=VK_LAYER_VALVE_steam_overlay_64,...`（容器内 overlay 本就不可用） |
| 17 | `MAP_32BIT` 补丁生效后仍 `VK_ERROR_MEMORY_MAP_FAILED`（坑 12 的后续） | 游戏进程**低 2GB 被 wine 占用 ~1.7GB**（实测 `0x1000a000-0x68000000` 1408MB 保留块，`MAP_32BIT` 只剩 ~321MB），而 **2~4GB 完全空闲** | `osrelease_redirect.so` 内置**低 4GB 重映射池**：`MAP_32BIT` 失败或映射落 >4GB 时，`munmap` 后用 `MAP_FIXED_NOREPLACE` 重映射到 2.25~4GB 区间（WoW64 thunk 可接 <4GB 任意指针）。**覆盖所有 mmap 路径**——包括虚函数分发的未补丁路径，坑 12 一并解决，无需再逐条改驱动 |

### 实测结果（2026-08-29 19:10）

```
Renderer: LISUAN 7G100 Series Graphics (ID=0x5006)   ← D3D11 11.1 硬件渲染
[ScopeLogger] Addressable scene load took 2088 ms      ← 场景加载
[ScopeLogger] Login flow took 8214 ms                  ← 登录流程
[LoginQueueService] Login queue entered ...            ← 登录队列
InvalidOperationException: Http error code was 404     ← 已知服务端阻塞（坑 3）
```

低地址重定向统计：`MAP_32BIT` 失败回退 1228 次 + 高地址重定位 72 次，全部成功。
游戏存活 8 分钟以上（此前各轮均 9 秒内崩溃）。**剩余阻塞仅服务端 404**。

### 固化产物

| 路径 | 说明 |
|---|---|
| `assets/mm-gf-vulkan/libvulkan_gf_shim.c/.so` | 协商垫片 ICD（坑 15） |
| `assets/mm-gf-vulkan/gf_icd.json` | 垫片 ICD 清单（相对路径，部署时随 `VK_ICD_FILENAMES` 指向） |
| `assets/mm-gf-vulkan/osrelease_redirect.c/.so` | hook 更新版：os-release 重定向 + 低 4GB 重映射池（坑 17） |
| `scripts/fix-minion-masters-7g100-hw.sh` | 固化脚本升级：部署垫片/库副本、新启动选项、状态项扩充（幂等） |

启动选项（固化脚本写入）：

```
PROTON_USE_WOW64=1 LD_PRELOAD=~/gf-compat/lib/osrelease_redirect.so
__GLX_VENDOR_LIBRARY_NAME=mesa LIBGL_ALWAYS_SOFTWARE=1
VK_ICD_FILENAMES=~/gf-compat/gf_icd.json
VK_LOADER_LAYERS_DISABLE=VK_LAYER_VALVE_steam_overlay_64,VK_LAYER_VALVE_steam_overlay_32
%command%
```

> 坑 12 状态更新为**已解决**（低 4GB 重映射池）。方案 B 的驱动单字节补丁仍有效但变为**可选**：
> 即使不打，hook 的高地址重定位也能兜住全部映射。
> 注意：Proton Experimental / 容器运行时每次更新都可能再次冲掉 DXVK 降级与 libSC 链接，重跑固化脚本即可。
