# 《随从大师》在 Ubuntu 24.04 + 砺算 7G100 上的攻坚全记录

> 时间跨度：2026-08-28 ~ 08-30
> 目标：让 32 位游戏《Minion Masters》(AppID 489520) 用上砺算 7G100 **硬件渲染**，
> 且**不影响其他游戏**
> 结论：**已达成**。7G100 硬件渲染可用，系统驱动保持原版，改动隔离在 LD_PRELOAD
> （按游戏生效）。
>
> **2026-08-31 更新：已无阻塞项。** 此前记录的唯一剩余阻塞「服务端 404」
> （`fix-minion-masters.md` 游戏坑 3）经实测**不影响游玩** —— 同日完成一局对战
> 并获胜，赛后奖励正常结算，全链路（匹配→对战→判胜负→结算）通过。
> 该 404 降级为已知噪音，详见附录 B。
>
> 本文记录**全部**尝试历程，包括失败的方向与走过的弯路；
> 只想照着做请看 `fix-minion-masters.md`（只保留成功方案）。
>
> **路径说明（2026-08-31 更新）**：文中出现的 `~/gf-compat/...` 是当时的运行时
> 目录，现已并入仓库，对应 `minion-masters/gf-compat/...`（源码与运行时产物同处
> 该目录）。为保留排查记录的真实性，正文里的历史路径未逐一改写，照此对应即可。

**阅读导航**（按时间线排列，越往后越接近最终方案）：

| 章节 | 内容 | 状态 |
|---|---|---|
| 零 | 核心矛盾 + 四个层次的分野 | 结论 |
| 一~二（阶段 1–8） | 早期摸索：识别显卡、定位崩溃、ICD shim、驱动 patch | 部分已被后续取代 |
| 三 | 问题汇总表（显卡坑 1–22） | 查错先看这里 |
| 四~五 | **当前状态与后续** | ✅ 先看 |
| 附录 A：产出物清单 | 早期产出物 + **最终方案产出物** | |
| 六 | 硬件渲染全链路打通（容器运行时适配） | |
| 七 | **隔离方案：去掉全局驱动补丁**（最终解） | ✅ 最终结果 |
| 附录 B：游戏层（非显卡）的教训与未解问题 | 从修复文档移来的非成功方案内容 | |

> 若只关心"最后是怎么解决的"，直接看**第七节**。

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

**两个层次的分野**（整个攻坚最重要的一条经验）：

| 层次 | 做法 | 覆盖面 | 副作用 |
|---|---|---|---|
| Vulkan ICD 层 | 写 shim 拦截 `vkMapMemory` | 只看得到走 `vkMapMemory` 的映射 | 驱动内部其他路径漏网（显卡坑 12、21） |
| 系统 loader 层 | 转发、协商版本垫片 | 受 loader 版本行为影响极大 | gf 协商版本 0 被静默丢弃（显卡坑 15） |
| **驱动文件层** | 给 `libvulkan_gf.so` 打 `MAP_32BIT` | 全路径，但**全局生效** | 连累 64 位原生游戏（显卡坑 18） |
| **mmap 层（最终）** | LD_PRELOAD 按结果地址重映射 | **全路径** | **仅对配了 LD_PRELOAD 的进程生效** ✅ |

结论：**改的层次要足够低以覆盖全路径，又要足够靠上以避免全局副作用——
mmap 层 + LD_PRELOAD 是唯一同时满足两者的位置。**

> **关于编号**：本文的"显卡坑 1–22"是**显卡/渲染层**的编号；`fix-minion-masters.md` 里的
> "游戏坑 1–8"是**游戏层**（Vivox、证书、BepInEx）的编号，两者独立，勿混。

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
| DXVK 初始化失败 | Proton 自带 DXVK v3.0.2 硬要求 `VK_KHR_maintenance5`，gf 不支持 | **DXVK 文件**取自 Proton 8.0 自带的 v2.3（注意：只是取 DXVK 文件，运行游戏的仍是 Experimental 系——Proton 8.0 不支持 new WoW64，见 `fix-minion-masters.md` 游戏坑 4） |

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
| 12 | 补丁后 mmap 仍失败 | 映射路径可能是虚函数分发的另一条实现 | **已解决**：mmap 层重映射池覆盖所有路径（显卡坑 17） |
| 13 | 容器内测试 device 创建返回 -1 | 测试脚本缺 wine 提供的环境（非 gf 问题） | 不阻塞，游戏本身能创建 device |
| 18 | 全局驱动补丁连累《刀塔霸业》启动段错误 | `MAP_32BIT` 把 64 位原生游戏也锁进低 2GB | **已解决**：撤掉驱动补丁，改由 LD_PRELOAD 重定位（阶段 9–13） |
| 19 | 撤掉补丁后映射落在 >4GB 无人接管 | 旧重映射池**只在 flags 含 `MAP_32BIT` 时触发**（反汇编 `26eb: and $0x40,%eax`） | **已解决**：改为按"结果地址 ≥4GB + GPU fd + MAP_SHARED"判断（阶段 11） |
| 20 | 4096×4096 大纹理仍创建失败 | 旧重映射池单块上限实际只有 84KB | **已解决**：新池实测可搬 24MB（阶段 11） |
| 21 | 在 Vulkan ICD 层拦截 `vkMapMemory` 做重映射，游戏反而不启动 | ICD 层看不到驱动内部其他映射路径（显卡坑 12 翻版） | 放弃该支路，下沉到 mmap 层（阶段 12） |
| 22 | 多次测试后 `Fontconfig error: out of memory` 等假故障 | 残留 MinionMasters 进程堆积（曾达 10 个）吃光内存，可用仅剩 264MB | 每轮测试后 `pkill -9 -f MinionMasters`；**曾误判 3 次** |

---

## 四、当前状态

> **2026-08-30 更新：目标已达成，隔离方案验证通过。**

**可用（已验证）**：7G100 **硬件渲染**，进主菜单，且**系统驱动保持原版**。

| | 随从大师（32 位 / WoW64） | 刀塔霸业（64 位原生） |
|---|---|---|
| 驱动 | 系统原版 `libvulkan_gf.so` | 系统原版（同一份，md5 `06f19e9a…`） |
| 我们的 hook | 加载（Steam 启动选项配的） | **完全不加载** |
| 结果 | LISUAN 7G100、buffer 错误 0、纹理错误 0、主菜单 29 处标记 | 存活 44 秒无段错误、RSS 991MB |

最终启动选项：
```
LD_PRELOAD=~/gf-compat/lib/osrelease_redirect.so PROTON_USE_WOW64=1 %command%
```
（不再需要 `VK_ICD_FILENAMES`；兼容工具选 `Proton - MM GF`）

**系统驱动状态**：已还原为原版，md5 与 `.orig` 逐字节一致，**无 `MAP_32BIT` 补丁**。
- 校验：`md5sum /usr/lib/x86_64-linux-gnu/libvulkan_gf.so{,.orig}`
- 补丁工具 `patch_gf_map32.py` 保留但**不再使用**，仅作将来调试对照。

**仍存在的阻塞（与显卡无关）**：游戏后端 API 404、进程启动后 ~40–90 秒被移除
（对应 `fix-minion-masters.md` 的**游戏坑 3 / 游戏坑 7**，详见本文附录 B）。

---

## 五、下一步（按优先级）

1. ~~继续方案 B：解析 vtable 定位其他 mmap 路径~~ → **已完成，显卡坑 12 关闭**（mmap 层重映射池全路径覆盖，无需再逐条改驱动）。
2. ~~改 patch 策略为低 4GB hint~~ → **已完成**，且更进一步：不再 patch 驱动，改在 LD_PRELOAD 里重映射。
3. 剩余阻塞已**与显卡无关**，转由 `fix-minion-masters.md` 跟踪：
   - 服务端 API 404 → 该文件**游戏坑 3**（详见本文附录 B）；
   - 进程启动后 ~40–90 秒被移除 → 该文件**游戏坑 7**（详见本文附录 B）。
   > 编号提醒：「游戏坑 N」是**游戏层**编号，「显卡坑 N」是本文**显卡层**编号。
   > 两套数字撞车但含义不同，例如游戏坑 3 = 服务端 404，而显卡坑 3 = Steam 报内容文件损坏。
4. 长期维护：Proton Experimental / 容器运行时更新可能冲掉 DXVK 降级与 libSC 链接，重跑固化脚本即可。

## 附录 A：产出物清单

| 路径 | 说明 |
|---|---|
| `~/project/lisuan/minion-masters/gf-lowmem-shim/gf_lowmem_icd.c` | Vulkan ICD shim（方案 A 实现，原生验证通过） |
| `~/project/lisuan/minion-masters/gf-lowmem-shim/patch_gf_map32.py` | **驱动补丁工具**（带备份/回滚，方案 B；现已不再使用，保留作调试对照） |
| `~/project/lisuan/minion-masters/gf-lowmem-shim/patch_soname.py` | 纯 Python 改 ELF SONAME |
| `~/project/lisuan/minion-masters/gf-lowmem-shim/run-test.sh` | 多模式对照测试脚本 |
| `~/project/lisuan/minion-masters/gf-lowmem-shim/analyze_lowmem.py` | 进程低地址空间占用分析 |
| `~/project/lisuan/minion-masters/gf-lowmem-shim/verify_patch.py` | 容器内校验驱动补丁字节 |
| ~~`/tmp/vk_dmabuf_32bit_test.c`~~ | dmabuf 低位映射可行性验证程序。**已丢失且不再需要** —— 该文件当时置于 `/tmp`，已被系统清理且未归档；其验证的 dmabuf 路线最终未被采用（现方案为 `osrelease_redirect.so` 的低 4GB 重映射池，不依赖 dmabuf），故未重建。教训：验证类程序也应落盘到项目内（参见 `gf-compat/variants/` 的做法） |
| `~/project/lisuan/minion-masters/MinionMasters-成功运行报告.md` | llvmpipe 成功运行报告 |
| `~/.local/share/Steam/steamapps/common/Proton - MM GF/` | 自定义 Proton（DXVK v2.3 + libSC） |

### 最终方案使用的产出物（2026-08-30）

| 路径 | 说明 |
|---|---|
| `~/gf-compat/lib/osrelease_redirect.so` | **核心**：os-release 重定向 + 低 4GB 重映射池（按结果地址判断，不依赖 MAP_32BIT） |
| `~/gf-compat/lib/osrelease_redirect.so.bak-08292140` | 旧版 hook 备份（依赖 MAP_32BIT 的那一版） |
| `~/gf-compat/lib/libvulkan_gf_shim.so` | 协商垫片 ICD（报版本 5 + 扩展补报/剔除） |
| `~/gf-compat/gf_icd.json` | 垫片 ICD 清单（**现可不用**，系统驱动即可） |
| `~/project/lisuan/minion-masters/gf-lowmem-shim/gf_icd_shim_orig.c` | **shim 源码**（原在 `/tmp/gf_icd_shim.c`，已归档；头部注释解释了版本 0 被 loader 丢弃的坑） |
| `~/project/lisuan/minion-masters/gf-lowmem-shim/gf_shim_low4.so` | 已放弃的支路：ICD 层 `vkMapMemory` 拦截（见阶段 12） |
| `~/project/lisuan/minion-masters/gf-lowmem-shim/gf_low4_icd.json` | 上述支路的 ICD 清单 |

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
| 17 | `MAP_32BIT` 补丁生效后仍 `VK_ERROR_MEMORY_MAP_FAILED`（显卡坑 12 的后续） | 游戏进程**低 2GB 被 wine 占用 ~1.7GB**（实测 `0x1000a000-0x68000000` 1408MB 保留块，`MAP_32BIT` 只剩 ~321MB），而 **2~4GB 完全空闲** | `osrelease_redirect.so` 内置**低 4GB 重映射池**：`MAP_32BIT` 失败或映射落 >4GB 时，`munmap` 后用 `MAP_FIXED_NOREPLACE` 重映射到 2.25~4GB 区间（WoW64 thunk 可接 <4GB 任意指针）。**覆盖所有 mmap 路径**——包括虚函数分发的未补丁路径，显卡坑 12 一并解决，无需再逐条改驱动 |

### 实测结果（2026-08-29 19:10）

```
Renderer: LISUAN 7G100 Series Graphics (ID=0x5006)   ← D3D11 11.1 硬件渲染
[ScopeLogger] Addressable scene load took 2088 ms      ← 场景加载
[ScopeLogger] Login flow took 8214 ms                  ← 登录流程
[LoginQueueService] Login queue entered ...            ← 登录队列
InvalidOperationException: Http error code was 404     ← 已知服务端阻塞（显卡坑 3，见附录 B）
```

低地址重定向统计：`MAP_32BIT` 失败回退 1228 次 + 高地址重定位 72 次，全部成功。
游戏存活 8 分钟以上（此前各轮均 9 秒内崩溃）。**剩余阻塞仅服务端 404**。

### 固化产物

| 路径 | 说明 |
|---|---|
| `assets/mm-gf-vulkan/libvulkan_gf_shim.c/.so` | 协商垫片 ICD（显卡坑 15） |
| `assets/mm-gf-vulkan/gf_icd.json` | 垫片 ICD 清单（相对路径，部署时随 `VK_ICD_FILENAMES` 指向） |
| `assets/mm-gf-vulkan/osrelease_redirect.c/.so` | hook 更新版：os-release 重定向 + 低 4GB 重映射池（显卡坑 17） |
| `minion-masters/fix-minion-masters-7g100-hw.sh` | 固化脚本升级：部署垫片/库副本、新启动选项、状态项扩充（幂等） |

启动选项（固化脚本写入）：

```
PROTON_USE_WOW64=1 LD_PRELOAD=~/gf-compat/lib/osrelease_redirect.so
__GLX_VENDOR_LIBRARY_NAME=mesa LIBGL_ALWAYS_SOFTWARE=1
VK_ICD_FILENAMES=~/gf-compat/gf_icd.json
VK_LOADER_LAYERS_DISABLE=VK_LAYER_VALVE_steam_overlay_64,VK_LAYER_VALVE_steam_overlay_32
%command%
```

> 显卡坑 12 状态更新为**已解决**（低 4GB 重映射池）。方案 B 的驱动单字节补丁仍有效但变为**可选**：
> 即使不打，hook 的高地址重定位也能兜住全部映射。
> 注意：Proton Experimental / 容器运行时每次更新都可能再次冲掉 DXVK 降级与 libSC 链接，重跑固化脚本即可。

---

## 七、2026-08-29 晚 ~ 08-30 补记：隔离方案——彻底去掉全局驱动补丁

### 背景：上一阶段的方案有个致命副作用

第六节打通了硬件渲染，但留下一个隐患：**给系统驱动 `libvulkan_gf.so` 打了 `MAP_32BIT`
单字节补丁**。这是全局的——所有用砺算显卡的程序都受影响。

直接后果是 **64 位原生的《刀塔霸业》一启动就段错误**（`underlords.sh` 第 118 行）：
`MAP_32BIT` 语义是"进程前 2GB"，会把一个本不需要任何补丁的 64 位游戏强行锁进低 2GB
地址空间。为了一个 32 位游戏去破坏另一个游戏，这个方案不能算完成。

所以目标明确：**保住随从大师的硬件渲染，同时让系统驱动回到原版**。

---

### 阶段 9 — 找到 shim 源码（转折点）

在 `/tmp/gf_icd_shim.c` 找到了一直在用的 `libvulkan_gf_shim.so` 的完整源码
（此前只找到过零散的变体，一直以为没有源码）。定位方法值得一提：先用
`nm` 确认 shim **没有 strip**，再拿反汇编里出现的两个独特函数名
`init_real` / `my_gap_dispatch` 去全盘 grep，一击命中。

源码已备份为 `~/project/lisuan/minion-masters/gf-lowmem-shim/gf_icd_shim_orig.c`。

其头部注释直接解释了我此前**所有** shim 尝试失败的根因：

```c
/* 1) 砺算 libvulkan_gf.so (Mesa 22.0.4 分叉) 的协商函数返回接口版本 0,
 *    新版 Vulkan loader (steamrt4, 1.4.x) 对版本 0 驱动走"直接导出符号表"
 *    路径, 而该库只导出 3 个 vk_icd* 符号 -> 被静默丢弃。
 *    -> 本垫片对外协商接口版本 5, 一切调用转发真库。 */
```

**认知修正（重要）**：我此前判断"shim 必须经系统 loader 转发"，绕了很久的弯路。
源码第 45 行显示它恰恰相反——**直接 `dlopen` 驱动副本**：

```c
real_h = dlopen("/home/ubuntu/gf-compat/lib/libvulkan_gf.so", RTLD_NOW | RTLD_LOCAL);
real_gap = (gap_fn)dlsym(real_h, "vk_icdGetInstanceProcAddr");
```

### 阶段 10 — 反汇编发现：旧重映射池依赖 `MAP_32BIT`

顺着这条线索回头反汇编已在用的 `osrelease_redirect.so`，看到重定位的触发条件：

```asm
26eb: and $0x40,%eax     ; 检查 flags & 0x40 (MAP_32BIT)
26f0: je  27b6           ; 没有 MAP_32BIT 就跳过重定位
```

**这就是死结**：重映射池只在调用方带 `MAP_32BIT` 时才工作，而 `MAP_32BIT`
恰恰来自那个全局驱动补丁。两者绑死，所以补丁"可选"的说法在当时并不成立——
不打补丁，映射落在高位（`>4GB`）就没人管。

顺带发现旧池的另一个隐性缺陷：统计 `hook-trace.log` 可见 19802 次重定位**全部成功**，
但**最大单块只有 86016 字节（84KB）**。这就解释了第六节里 4096×4096 大纹理仍然失败——
它需要的块远大于 84KB。

### 阶段 11 — 重写 hook：改为按"结果地址"判断

把触发条件从"看 flags"改为"看结果"：

> 映射返回值 **≥ 4GB** 且 **fd 是 GPU**（DRM 字符设备主设备号 226，或 dma-buf 匿名文件）
> 且 **`MAP_SHARED`** → 把它搬到 2~4GB

```c
#define LOW4_START 0x90000000ULL   /* 2.25GB */
#define LOW4_END   0x100000000ULL  /* 4GB    */

static int is_gpu_fd(int fd) {
    struct stat st;
    if (fd < 0 || fstat(fd, &st) != 0) return 0;
    if (S_ISCHR(st.st_mode) && major(st.st_rdev) == 226) return 1;  /* /dev/dri/card* */
    if (S_ISREG(st.st_mode) && st.st_nlink == 0) return 1;          /* dma-buf */
    return 0;
}

void *mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off) {
    void *r = real_mmap(addr, len, prot, flags, fd, off);
    if (r == MAP_FAILED) return r;
    if ((uintptr_t)r >= LOW4_END && fd >= 0 && (flags & MAP_SHARED) && is_gpu_fd(fd)) {
        void *p = low4_map(len, prot, flags, fd, off);   /* 区内 MAP_FIXED bump 分配 */
        if (p != MAP_FAILED) { munmap(r, len); return p; }
    }
    return r;
}
```

这样一来**驱动是否需要 `MAP_32BIT` 补丁就完全无关了**——补丁可以撤掉。

**为什么是 2~4GB**：`MAP_32BIT` 只能到低 2GB，而游戏进程低 2GB 已被 wine 占满
（实测 2043/2048MB）；2~4GB 区间基本空闲。low4 区取 `0x90000000`~`0x100000000`
（约 1.9GB），bump 分配不做回收——显存映射次数有限，实测够用。

**误伤防护**：用 `is_gpu_fd()` 限定只搬 GPU 相关映射，普通文件映射
（加载 .so 等，都是 `MAP_PRIVATE`）和匿名内存不受影响。

### 阶段 12 — 一条被放弃的支路：在 Vulkan ICD 层做映射拦截

在找到 mmap 层方案之前，曾尝试在 shim 里拦截 `vkMapMemory`：先给
`vkAllocateMemory` 注入 `VK_KHR_external_memory_fd` 声明，拿到 dmabuf fd 后
重新 `mmap` 到低 4GB。编译通过（`gf_shim_low4.so`），但实测**游戏根本没起来**
（无 `Player.log`），比原方案更差。

**放弃原因**：Vulkan ICD 层只能看到走 `vkMapMemory` 的映射，看不到驱动内部
其他映射路径（这正是显卡坑 12 "sf 虚函数分发的另一条实现"的翻版）；而 mmap 层在
更底下，**天然覆盖所有路径**。这次经历再次印证：**改的层次越低，覆盖面越全**。

### 阶段 13 — 验证（2026-08-30）

**第一步：确认系统驱动干净**

```
当前: 06f19e9a9dda61012db18aea7690bb8d  /usr/lib/x86_64-linux-gnu/libvulkan_gf.so
原版: 06f19e9a9dda61012db18aea7690bb8d  /usr/lib/x86_64-linux-gnu/libvulkan_gf.so.orig
✅ 逐字节一致，无 MAP_32BIT 补丁
```

> 验证技巧：不需要 `sudo`，`md5sum` 对比同目录 `.orig` 即可。

**第二步：随从大师——改用系统原版驱动**

刻意**不再**用 `gf-compat/lib/` 下的驱动副本，也不再指定 `VK_ICD_FILENAMES`，
直接吃系统原版驱动，只保留 LD_PRELOAD：

```
Renderer: LISUAN 7G100 Series Graphics (ID=0x5006)
buffer 错误 = 0     纹理错误 = 0     主菜单标记 = 29     日志 1564 行
重定位 10005 次
```

**第三步：刀塔霸业——确认不再被牵连**

```
进程存活 44 秒，无段错误（打补丁时必崩）
加载的驱动：libvulkan_gf.so（系统原版）
osrelease_redirect hook 加载次数 = 0   ← 完全未加载，隔离成立
VmRSS: 991196 kB
```

### 本轮解决效果对照

| | 旧方案（全局驱动补丁） | 新方案（LD_PRELOAD 重定位） |
|---|---|---|
| Renderer | LISUAN 7G100 | **LISUAN 7G100** |
| buffer 错误 | 0 | **0** |
| 纹理错误 | **1**（4096² 卡死） | **0** ← 顺带解决 |
| 主菜单 | 卡在加载中 | **进主菜单** |
| 单块最大重定位 | 84 KB | **24 MB** |
| 需改系统驱动 | 是（全局，连累他游） | **否** |
| 刀塔霸业 | 启动段错误 | **正常** |

2574 次重定位、0 次失败（另一次采样 10005 次）。

### 固化产物（本轮新增/更新）

| 路径 | 说明 |
|---|---|
| `~/project/lisuan/minion-masters/gf-lowmem-shim/gf_icd_shim_orig.c` | **shim 源码**（原在 `/tmp`，已归档，含关键注释） |
| `~/gf-compat/lib/osrelease_redirect.so` | **重写版 hook**：os-release 重定向 + 按结果地址判断的重映射池 |
| `~/gf-compat/lib/osrelease_redirect.so.bak-08292140` | 旧版 hook 备份（依赖 MAP_32BIT 的那一版） |
| `~/project/lisuan/minion-masters/gf-lowmem-shim/gf_shim_low4.so` | 已放弃的 ICD 层映射拦截尝试（保留供参考） |

**最终启动选项**（相比第六节可再精简——不再需要 `VK_ICD_FILENAMES`）：

```
LD_PRELOAD=~/gf-compat/lib/osrelease_redirect.so PROTON_USE_WOW64=1 %command%
```

### 为什么这样就隔离了

关键在于 **LD_PRELOAD 是每游戏配置的**：

```
系统驱动 libvulkan_gf.so（原版，未打补丁）
  ├── 随从大师 → Steam 启动选项配了 LD_PRELOAD → 加载 hook → 重定位到 2~4GB → 7G100 渲染
  └── 刀塔霸业 → 不配 LD_PRELOAD → 完全不加载 hook → 原样使用驱动
```

不是靠"补丁做得更小心"，而是**换了个层次**：把改动从全局的驱动文件，
下沉到按进程生效的 LD_PRELOAD。

---

## 附录 B：游戏层（非显卡）的教训与未解问题

> 以下内容与显卡无关，从 `fix-minion-masters.md` 移来（该文只保留可操作的成功方案）。
> 编号沿用该文的「游戏坑 N」，与本文显卡层的"显卡坑 1–22"无关。

### 游戏坑 3：`LoginQueueService.Post` 404（仍存在，但**不阻塞** —— 2026-08-31 复测）

游戏后端 `api.prod.mm21.minionmastersthegame.com`：

- `/health` 返回 **200 "Healthy"** —— 服务活着；
- 早期排查时**所有 `/v1/*` endpoint 返回 404**，`LoginQueueService.Post` 抛
  `Http error code was 404`，登录队列进不去。

> **2026-08-31 复测：「全 404」的描述已不准确，且本项不再是阻塞项。**

在 Proton 11.0 + 7G100 硬件渲染下，单次会话中 404 **仅出现 1 次**，且只来自
`LoginQueueService.Post`。与此同时以下功能**全部正常**：

| 功能 | 日志证据 | 状态 |
|---|---|---|
| Steam 认证 | `[SteamAuthenticationService] Ticket 2` | ✅ |
| WebSocket | `[WebSocketSharpTransportService] Connection opened` | ✅ |
| 商店数据 | `[SteamShopService] Update (48, 58000, JPY)…` | ✅ |
| 档案数据 | `[ScopeLogger] Profile data request took 1209 ms` | ✅ |
| 语音聊天 | `[ChatService] OnLoginStateChanged: LoggedIn` | ✅ |

**关键结论：该 404 不影响实际游玩。** 同日实测完成一局对战并获胜，赛后奖励正常结算：

```
[BattleNetworkStateActive] Game ended with Completed
[GameUnitService] Not removing winner master 'WitchQueen' to let win animation play
[PostMatchViewModel] Displaying Battlepass reward card
```

匹配 → 对战 → 判定胜负 → 结算奖励，全链路通过。

因此本项**降级为已知噪音**。日志中仍可见
`Login queue entered with ticket 00000000-0000-…`（ticket 全零），看似异常但不影响对战。

另有 Azure IP `140.228.24.145:443` 被封锁。二者均属 BetaDwarf 服务端问题，本地无解，
但已证实**不构成游玩阻塞**。原先「是显卡坑 7（进程 40–90 秒后消失）主因」的怀疑，
现已排除——进程消失另有原因（见显卡坑相关条目）。

### 游戏坑 5：`user_settings.py` 注入无效（教训）

`Proton 8.0/user_settings.py` 的正确格式是顶层键值对：

```python
user_settings = {
    "VK_ICD_FILENAMES": "/usr/share/vulkan/icd.d/gf_icd.x86_64.json",
    "WINEDLLOVERRIDES": "winhttp=n,b",
}
```

两个坑：

- ❌ 不能用嵌套 `session_environment`：嵌套 dict 会被 proton 当成 env 值 →
  `TypeError: expected str... not dict` → 游戏根本不启动。**曾在这个格式问题上卡过**。
- ⚠️ 即便格式正确，自定义变量**不会进入 pressure-vessel 容器**（只传白名单 env），
  注入 `VK_ICD_FILENAMES` / `WINEDLLOVERRIDES` 均无效。

**教训**：显卡相关环境变量一律走 Steam 启动选项（LaunchOptions），不要走 `user_settings.py`。

### 游戏坑 7：进程启动后 ~40–90 秒被移除（未解）

**现象**：已进入主菜单（`BattleModeViewModel` 出现在 Player.log），但 Steam console_log
在 38–90 秒不等出现 `Game process removed: AppID 489520`。Player.log 末尾停在
`[ChatService] Starting LoginAsync`（patch 跳过后的 Vivox 调用），**无 crash dump、
无 .NET 致命异常**——是游戏主动退出，不是崩溃。

**已排除**（这些排查都白做了）：

- 不是 `CAPIJobRequestUserStats - Server response failed 2`：该错误从 8/24 13:34 起持续
  出现 23 次，与退出时刻**无固定时间关联**，且成功进主菜单的运行里同样存在；
- 不是 Zenject assert（良性）；
- 不是 Texture 异常（已 patch）。

**怀疑方向**：登录队列 404 后游戏自身超时退出，`DefaultModeInstaller` 构造里
`LoginQueueService.EnterQueueAndWaitForTurn` 的 404 异常让流程走不下去。
需等后端恢复后复测，或进一步 hook `LoginQueueService` 观察退出路径。

### 游戏层其他教训

| # | 教训 | 说明 |
|---|---|---|
| A4 | **不要用 `il2cpp_class_get_methods`** | 在 wine11 wow64 下行为不可靠，改用 `il2cpp_class_get_method_from_name` |
| A5 | BepInEx 插件版本号显示的是 `AssemblyVersion` | 改了版本号但显示没变时，先确认改的是不是 AssemblyVersion |
| A2 | 无头环境 Steam 登录可自愈 | 约 30 秒后 `webhelper_js.txt` 出现 `state 5`（完全登录），不必手动干预 |

---

### 已废弃方案：BepInEx 6 + `MMSkipVivox.dll`（2026-08-26 起退役）

> 2026-08-30 核实：该方案**已不在部署中**，被 `MMPatch.dll`（doorstop 直载）取代。
> 保留在此，供理解旧笔记与 `~/mm_patch/` 下的历史产物。

**方案本体**：BepInEx 6 + 自定义插件 `MMSkipVivox.dll`（v1.1.1，**15360 B**），
源码 `~/mm_patch/plugin/Patcher.cs`（17214 B，编译目标 net6.0，引用 BepInEx.Core /
BepInEx.Unity.IL2CPP / Il2CppInterop.Runtime）。插件在 `Load()` 中打三个 patch。

**退役原因**：切到 new WoW64（7G100 硬件渲染必需）后，Il2CppInterop 的
runtime-invoke hook 不触发，BepInEx 停在 `Chainloader initialized`，
`Chainloader.Start()` 永不执行 → `MMSkipVivox` 不加载 → Vivox 崩溃回归。
遂改用 doorstop 直载的 `MMPatch.dll`（纯 P/Invoke，不依赖 BepInEx）。
详见 `fix-minion-masters.md` 游戏坑 8 障碍 4。

**现场痕迹**（2026-08-30 核实）：

| 路径 | 状态 |
|---|---|
| `.../Minion Masters/BepInEx.disabled-20260826/plugins/MMSkipVivox.dll` | 目录被改名禁用 |
| `.../Minion Masters/BepInEx/plugins/` | **空**（目录已不存在） |
| `~/mm_patch/backup/BepInEx/plugins/MMSkipVivox.dll` | 备份 |
| `~/mm_patch/plugin/bin/net6.0/MMSkipVivox.dll` | 编译产物 |
| `.../Minion Masters/doorstop_config.ini.bak-bepinex` | BepInEx 时代配置备份 |

**patch a 的 trampoline 演进**（这段经验被 `MMPatch` 继承，故仍值得记）：

- **v1.0.0（旧，有隐患）**：prologue 直接覆盖 `31 C0 31 D2 C3`
  （`xor eax,eax; xor edx,edx; ret`）返回**全零 struct**。游戏 **8/23 更新前**能跑；
  更新后 `ChatService.LoginAsync` 改为 `await` Vivox 返回，全零 UniTask 的 `GetAwaiter()`
  访问 `null` runner → `NullReferenceException` → 退出（`e0434352`）。
- **v1.1.0 尝试**：优先 `get_CompletedTask` getter 跳转 / 运行时读静态字段地址构造 trampoline；
  本机 IL2CPP **没生成该 getter**，回退 xor 方案。实测 8/24 版游戏下全零 UniTask 已不再致命
  （配合 patch b/c），故最终为 xor 回退版在跑。

**顺带排除的弯路**：禁用 Vivox DLL 反而让游戏更早崩在登录队列——
Vivox 必须"跳过"而非"禁用"。

---

### 已废弃建议：A1「`d3d11: failed to create factory` 用 wined3d 绕过」

> 2026-08-30 从 `fix-minion-masters.md` 附录 A1 移来。当时是有效解，
> 但在 7G100 硬件渲染方案下**会起反作用**，故不再作为操作建议。

**当年的症状**：DXVK 创建 D3D11 factory 失败 → 游戏卡在
`GfxDevice: creating device client` 然后退出（无 Player.log 或停在该行）。

**当年的修法**：

- **优先 `PROTON_USE_WINED3D=1`**：wined3d 走软件渲染（llvmpipe），能稳定过渲染关，
  游戏可到 `Direct3D 11 llvmpipe` + 推 `LoginMainViewModel`。
- 或在 Steam 默认启动下，让 pressure-vessel 自动注入 gf Vulkan
  （`VK_ICD_FILENAMES` 已含 `gf_icd.x86_64.json`），DXVK 才能拿到有效 Vulkan 后端。

**当年的注意事项**（这条至今仍对）：不要手动在启动 wrapper（`run-mm.sh` 之类）里硬塞
`VK_ICD_FILENAMES` / `WINEDEBUG` 等 env——会破坏 Steam 注入的完整 env，反而让渲染卡死。
清空 LaunchOptions、恢复 Steam 默认启动后通常就正常。

**为何废弃**：`PROTON_USE_WINED3D=1` 会把渲染强行拉回 llvmpipe 软件渲染，
与「用 7G100 硬件渲染」的目标直接冲突。2026-08-30 核实当前 `Player.log` 中
`failed to create factory` 出现 **0 次**，该问题已随硬件渲染链路打通而消失。
