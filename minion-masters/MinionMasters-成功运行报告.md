# 随从大师 (Minion Masters) Ubuntu 24.04 成功运行报告

> 生成日期：2026-08-28
> 结果：**成功进入游戏主菜单并登录成功**（渲染后端 llvmpipe 软件渲染）
> Steam AppID：489520

---

## 一、结论速览

| 项目 | 状态 |
|---|---|
| 进入主菜单 | ✅ 成功（已进入 `CardCollectionViewModel` 卡牌收藏界面） |
| 账号登录 | ✅ `OnLoginStateChanged: LoggedIn` |
| 语音服务 Vivox | ✅ 正常（拿到 token，登录完成） |
| 渲染后端 | llvmpipe 软件渲染（**不是** 7G100 硬件） |
| 致命错误 | 0 个 |
| 可容忍错误 | 5 类（详见第六节） |

**重要**：本机虽有砺算 7G100 显卡，但因架构限制（32 位游戏 + 仅 64 位 Vulkan 驱动），
硬件加速路径不通，当前使用 CPU 软件渲染 llvmpipe。详见第十节。

---

## 二、运行环境

### 硬件
| 项目 | 值 |
|---|---|
| CPU | Hygon C86-4G (OPN:3450)，16 核 |
| 内存 | 15.2 GB |
| 显卡 | 砺算 7G100（`lspci` 显示 `4c54:5006`，VGA） |
| 显卡音频 | `4c54:5010` |

### 系统
| 项目 | 值 |
|---|---|
| 操作系统 | Ubuntu 24.04.4 LTS |
| 内核 | 7.0.0-30-generic |
| 桌面会话 | **Wayland**（`XDG_SESSION_TYPE=wayland`，`DISPLAY=:0`，经 XWayland） |
| 砺算驱动包 | `lisuangpu-dkms 262.2.0.56`、`lisuangpu-lib 262.2.0.56` |
| 内核模块 | `gf` 已加载（1 个） |
| DRM 节点 | `/dev/dri/card1`、`/dev/dri/renderD128` |

> ⚠️ Wayland 会话是一个关键约束：手动在宿主机直接跑 Proton 时，
> winevulkan 拿不到 `VK_KHR_surface`（报 `Required Vulkan extension VK_KHR_surface not supported`）。
> **必须经由 Steam（Steam Runtime 容器）启动**才有正确的 WSI 环境。

---

## 三、软件栈

| 组件 | 版本 | 说明 |
|---|---|---|
| Proton | `experimental-11.0-20260826-x86_64` | Steam 官方最新版 |
| Wine | **wine-11.0** | Proton Experimental 自带 |
| DXVK | **v3.0.2-58-g9cd6755b** | Proton 官方版，**未做任何降级/替换** |
| Mesa | 25.2.8-0ubuntu0.24.04.2 | amd64 + i386 双架构 |
| llvmpipe | LLVM 20.1.2, 256 bits | Mesa 软件光栅化器 |
| 32 位 llvmpipe ICD | `/usr/lib/i386-linux-gnu/libvulkan_lvp.so` | **关键**：有 32 位驱动是能跑起来的前提 |
| Unity 引擎 | 2020.3.44f1 | 游戏自带 |

### 关于 DXVK 的重要说明
排查过程中曾把 Proton 的 DXVK 从 v3.0.2 **降级**为 Proton 8.0 的 v2.3，
结果 **Steam 报"内容文件损坏"**（`dxgi.dll` 被改动，触发完整性校验）。

现已**全部恢复为 Proton 官方原版 v3.0.2-58**，Steam 不再报错。
**结论：不要再手动降级 DXVK**，llvmpipe 下官方 v3.0.2 完全可用。

---

## 四、关键配置（逐项）

### 4.1 Steam 启动选项（LaunchOptions）

在 Steam 库 → 右键《Minion Masters》→ **属性** → **启动选项** 中填写：

```
VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json PROTON_USE_WOW64=1 LP_NUM_THREADS=16 __GLX_VENDOR_LIBRARY_NAME=mesa %command%
```

逐项解释：

| 参数 | 作用 |
|---|---|
| `VK_ICD_FILENAMES=.../lvp_icd.json` | **核心**：强制 Vulkan 走 llvmpipe，绕开有问题的 gf 驱动 |
| `PROTON_USE_WOW64=1` | 启用 Proton 的 **new WoW64**，让 32 位游戏经 thunk 调用 64 位驱动 |
| `LP_NUM_THREADS=16` | llvmpipe 多线程渲染线程数（按 CPU 16 核设置，显著影响帧率） |
| `__GLX_VENDOR_LIBRARY_NAME=mesa` | 让 winex11 的 OpenGL 探测走 Mesa llvmpipe，避免 winex11 初始化崩溃 |
| `%command%` | Steam 占位符，必须保留在最后 |

### 4.2 游戏目录配置

游戏目录：`/home/ubuntu/.local/share/Steam/steamapps/common/Minion Masters`

**`doorstop_config.ini`**（Doorstop 4.3.0 注入器）：
```ini
[General]
enabled = true                      # 必须为 true，否则 MMPatch 不加载
target_assembly = MMPatch.dll       # 载入我们自研的补丁
redirect_output_log = false
ignore_disable_switch = false

[Il2Cpp]
coreclr_path = dotnet\coreclr.dll   # IL2CPP 游戏走 CoreCLR
corlib_dir = dotnet
```

**`dxvk.conf`**：
```
dxvk.logLevel = info
```
（保持最简单即可，不要加 `dxvk.maxChunkSize` 之类参数——实测无效）

### 4.3 MMPatch（自研 IL2CPP 补丁）

- 源码：`~/project/lisuan/minion-masters/assets-mm-gf-vulkan/MMPatch.cs`
- 产物：`bin/MMPatch.dll` → 部署到游戏目录
- 运行日志：`Minion Masters/BepInEx/MMPatch.log`
- 编译：`dotnet build mmpatch.csproj -c Release`（netstandard2.1）

**当前启用的补丁（只有 1 个）**：

| 补丁 | 目标 | 作用 |
|---|---|---|
| `InternalGetTexture -> null` | `UnityEngine.Networking.DownloadHandlerTexture.InternalGetTexture` | 网络纹理下载返回 null，避免主菜单加载新闻/活动图片时崩溃 |

**已停用的补丁（关键！）**：

| 补丁 | 原作用 | 停用原因 |
|---|---|---|
| ~~Vivox `LoginAsync`/`LogoutAsync` → 空操作~~ | 跳过语音登录 | **这是闪退的真凶**。实测 Vivox 在 wine 下能正常工作；patch 成空操作后游戏等不到登录回调，直接退出 |

本次运行补丁日志：
```
[20:16:01] [MMPatch] waiting for IL2CPP domain...
[20:16:01] [MMPatch] domain ready @0x79C3FE0, applying patches
[20:16:01] [MMPatch] Vivox patch disabled (Vivox works natively under wine)
[20:16:01] [MMPatch]   patched InternalGetTexture->null 0x777AE7B0 [55 8B EC 6A FF] -> [E9 ...]
[20:16:01] [MMPatch] done
```

### 4.4 关键路径

| 用途 | 路径 |
|---|---|
| 游戏目录 | `/home/ubuntu/.local/share/Steam/steamapps/common/Minion Masters` |
| Player.log | `~/.local/share/Steam/steamapps/compatdata/489520/pfx/drive_c/users/steamuser/AppData/LocalLow/BetaDwarf ApS/Minion Masters/Player.log` |
| Proton 前缀 | `~/.local/share/Steam/steamapps/compatdata/489520/pfx` |
| MMPatch 日志 | `Minion Masters/BepInEx/MMPatch.log` |
| 崩溃转储 | `.../Temp/BetaDwarf ApS/Minion Masters/Crashes/` |
| 切换脚本 | `~/project/lisuan/minion-masters/mm-render-switch.sh` |

---

## 五、启动流程详解（本次成功运行）

日志总行数 **1108 行**。以下按日志行号与实测时间对照。

### 阶段 1：引擎与图形初始化（第 1–15 行，约 0–10 秒）

```
1  Loading player data from .../MinionMasters_Data/data.unity3d
2  Initialize engine version: 2020.3.44f1 (7f159b6136da)
3  [Subsystems] Discovering subsystems at path .../UnitySubsystems
4  GfxDevice: creating device client; threaded=1
5  Direct3D:
7      Renderer: llvmpipe (LLVM 20.1.2, 256 bits) (ID=0x0)   ← ✅ 渲染器就绪
8      Vendor:   Unknown (ID=10005)
9      VRAM:     3072 MB
10 <RI> Initializing input.
12 Using XInput
13 <RI> Input initialized.
14 <RI> Initialized touch support.
```

> 关键点：第 7 行必须是 `llvmpipe`。若显示 `LISUAN 7G100`，说明走的是硬件路径，会崩。

### 阶段 2：资源与场景加载（第 16–100 行，约 10–30 秒）

```
16 [ScopeLogger] Loading runtime key 27fefc73abf97444abab64ddacce447b.
19 [ScopeLogger] Addressable scene load.
20 Play asset delivery initialization starting.
46 [ScopeLogger] Addressable scene load took 639 ms.
69 [ScopeLogger] Loading runtime key ... took 886 ms.
```

### 阶段 3：进入登录界面（第 195–236 行，约 20–30 秒）

```
195 [TransitionViewCommand] Pushing LoginMainViewModel with parameter .
196 [PushPopStackViewModel] Push LoginMainViewModel.
205 [LoginQueueService] Login queue entered with ticket 00000000-0000-0000-0000-000000000000.
```

> 登录队列 ticket 是全零（`00000000-...`），属正常，游戏会继续推进。

### 阶段 4：进入主菜单内部（第 499 行起，约 40–45 秒）

```
499 Assets...CardCollectionViewModel:UpdateCollectionCards()
```

`CardCollectionViewModel`（卡牌收藏）是主菜单内部的界面，
**出现这行即代表已经进入游戏主体，不再是登录画面**。

### 阶段 5：Vivox 语音登录（第 826–927 行，约 55–60 秒）

```
826 [ChatService] Starting LogoutAsync
827 [VivoxChatServiceAdapter] Starting Vivox LogoutAsync
828 [ChatService] Starting LoginAsync
829 [VivoxChatServiceAdapter] Starting Vivox LoginAsync
838 [VivoxChatServiceAdapter] Initializing Vivox.
840 [VivoxChatServiceAdapter] Login started with token e30.eyJpc3MiOiI4MjgxNC1taW5pby01NDEzMy11ZGFzaCIs...
841 [ChatService] OnLoginStateChanged: LoggingIn
927 [ChatService] OnLoginStateChanged: LoggedIn          ← ✅ 登录完成
```

> 第 840 行的 `token e30...` 是 Vivox 的真实登录令牌（JWT）。
> **如果看不到这几行，说明 Vivox 被错误 patch 掉了，游戏会闪退。**

### 实测时间线

| 时间 | 日志行数 | 事件 |
|---|---|---|
| 10s | 105 | 引擎初始化完成 |
| 20s | 211 | 登录界面、登录队列 |
| 30s | 474 | 资源加载 |
| 40s | 841 | 主菜单/卡牌界面 |
| 50s | 867 | — |
| 60s | 927 | **Vivox LoggedIn** |
| 90s | 1062 | 稳定运行 |
| — | 1108 | 最终行数（进程退出） |

> ⚠️ **必须从 Steam 启动**：命令行直跑时，游戏虽能走到上述全部流程，
> 但因缺少 Steam 的认证上下文，约 90–100 秒后会自行退出。
> 从 Steam 启动时进程可长期驻留。

---

## 六、可容忍的错误与警告（5 类）

以下错误在**成功运行**的日志中同样存在，**不影响进入游戏**，可放心忽略。

### 1. Curl 证书错误（2 种，共 3 次）
```
Curl error 35: Cert handshake failed. verify result: UNITYTLS_X509VERIFY_FATAL_ERROR. error state: 7   (1 次)
Curl error 42: Callback aborted                                                                        (2 次)
```
- **原因**：wine 前缀内缺少完整 CA 证书链，Unity 的 TLS 校验失败。
- **影响**：仅影响部分 HTTPS 请求（如新闻图片、遥测），**登录走的是另一条链路，不受影响**。
- **是否需要修**：否。历史成功记录（8/26）同样有此错误。

### 2. Unity Standard Shader 不受支持（3 条 WARNING，各 2 次）
```
WARNING: Shader Unsupported: 'Standard' - All subshaders removed
WARNING: Shader If subshaders removal was intentional
WARNING: Shader Did you use #pragma only_renderers and omit this platform?
```
- **原因**：llvmpipe 对某些 shader 变体支持不完整。
- **影响**：个别材质可能显示异常（偏色/丢失），不影响流程。
- **是否需要修**：否。这是软件渲染的固有局限。

### 3. UniTask 未观察异常（1 次）
```
Cysharp.Threading.Tasks.UniTaskScheduler:PublishUnobservedTaskException(Exception)
Cysharp.Threading.Tasks.ExceptionHolder:Finalize()
```
- **原因**：某个异步任务抛异常但未被 await 捕获，被 UniTask 调度器兜底记录。
- **影响**：无，游戏继续运行。

### 4. 登录队列 Post 异常（4 次）
```
at Assets.Core.Platform.Common.Services.LoginQueueService+<Post>d__15`2[TRequest,TResponse].MoveNext ()
at Assets.Core.Platform.Common.Services.LoginQueueService+<EnterQueueAndWaitForTurn>d__13.MoveNext ()
```
- **原因**：登录队列的 HTTP 请求失败（与第 1 类证书错误同源）。
- **影响**：**无**。游戏随后照常拿到 Vivox token 并 `LoggedIn`。
- **是否需要修**：否。

### 5. 容器 locale 警告（启动期，无害）
```
setlocale "zh_CN.UTF-8": No such file or directory
pv-locale-gen: Generating locale zh_CN.UTF-8...
pv-adverb: W: Container startup will be faster if missing locales are created at OS level
```
- **原因**：Steam Runtime 容器内 locale 未预生成，容器自动补建。
- **影响**：仅拖慢启动约 1 秒。

### 汇总

| 类别 | 次数 | 是否致命 | 需处理 |
|---|---|---|---|
| Curl 证书 (35/42) | 3 | 否 | 否 |
| Standard Shader 警告 | 6 | 否 | 否 |
| UniTask 未观察异常 | 1 | 否 | 否 |
| LoginQueue Post 异常 | 4 | 否 | 否 |
| 容器 locale 警告 | 若干 | 否 | 否 |

---

## 七、致命错误（本次已解决）

### 错误 1：闪退（本次修复的核心问题）

**现象**：进入登录界面后约 35–40 秒进程消失。

**根因**：MMPatch 把 `VivoxChatServiceAdapter.LoginAsync/LogoutAsync`
patch 成了空操作（`xor eax,eax; xor edx,edx; ret`），导致：

- 日志里**完全没有** `VivoxChatServiceAdapter` 相关输出
- 拿不到 Vivox token
- `OnLoginStateChanged` 永远不触发
- 游戏卡在 `[ChatService] Starting LoginAsync` 后退出

**证据对比**：

| | 旧 MMPatch（闪退） | 新 MMPatch（正常） |
|---|---|---|
| `[VivoxChatServiceAdapter] Starting Vivox LoginAsync` | ❌ 无 | ✅ 有（第 829 行） |
| `Login started with token` | ❌ 无 | ✅ 有（第 840 行） |
| `OnLoginStateChanged: LoggedIn` | ❌ 无 | ✅ 有（第 927 行） |
| 进程存活 | ~40 秒后退出 | 90 秒+ 稳定 |

**修复**：注释掉 `PatchVivox()`，重新编译 `MMPatch.dll`。
Vivox 在 wine 下本来就能正常工作，不需要 patch。

### 错误 2：Steam "内容文件损坏"

**现象**：启动游戏时 Steam 弹窗提示
`内容文件损坏 - .../Proton - Experimental/files/lib/wine/dxvk/i386-windows/dxgi.dll`

**根因**：此前为适配 gf 驱动，手动把 Proton 的 DXVK v3.0.2 替换成 Proton 8.0 的 v2.3，
Steam 完整性校验检测到 `dxgi.dll` 被改动。

**修复**：从备份 `dxvk.bak-v3` 恢复全部原始文件，
`dxgi.dll` 的 MD5 恢复为 `99430a8f...`(i386) / `1661ea48...`(x86_64)，与官方一致。

**教训**：不要手动替换 Proton 自带文件；Steam 更新还会再次覆盖。

### 错误 3：7G100 硬件路径崩溃（未解决，已规避）

**现象**（走 gf 驱动时）：
```
DxvkMemoryAllocator: Mapping memory failed with VK_ERROR_OUT_OF_HOST_MEMORY   (×18)
DxvkMemoryAllocator: Memory allocation failed
    Size: 4194304   Alignment: 16   Mem types: 0,1,2,3
d3d11: failed to create buffer (target 0x1 mode 0 size 20) [0x80070057]
```
Player.log 停在 11 行或 135 行，进程崩溃。

**规避方法**：使用 llvmpipe（本报告方案）。此时 `failed to create buffer` 次数为 **0**。

---

## 八、已知限制

1. **性能受限于软件渲染**
   llvmpipe 用 CPU 渲染，随从大师这类 3D 游戏帧率较低。
   建议进游戏后把画质/分辨率调到最低。已设 `LP_NUM_THREADS=16`（16 核全用上）。

2. **Standard Shader 部分材质可能显示异常**
   llvmpipe 对某些 shader 变体支持不完整（见第六节第 2 条）。

3. **证书错误导致部分网络内容加载失败**
   如新闻图片、活动横幅可能空白（见第六节第 1 条）。不影响对战与登录。

4. **必须从 Steam 启动**
   命令行直跑会因缺少 Steam 认证上下文，约 90 秒后自行退出。

5. **Proton 更新会重置部分状态**（**硬件渲染下必看**）
   Proton Experimental 更新时会覆盖：
   - `libSC.so` 软链 —— gf 驱动的**着色器编译器伴侣库**，`vkCreateDevice` 依赖它。
     悬空 → `Failed to initialize player` / `Failed to initialize graphics` /
     `InitializeEngineGraphics failed`。
   - **DXVK 降级** —— 被还原为新版（要求 `VK_KHR_load_store_op_none` 与
     `VK_KHR_maintenance5`，gf 22.0.4 均不支持）→ DXVK 直接跳过 7G100。

   > 2026-08-31 新增变化点：Proton **`experimental-11.0-20260826`** 起，库目录由
   > `files/lib64/` 改为 `files/lib/`。旧加固若仍写死 `lib64` 会**全部静默失效**
   > ——包括 `libSC.so` 软链与 DXVK 降级，症状就是上面两条。

   **诊断**（更新后打不开游戏时，按顺序看这三项）：
   ```bash
   PE="$HOME/.local/share/Steam/steamapps/common/Proton - Experimental"
   cat "$PE/version"                                              # 版本号
   ls -l "$PE/files/lib/x86_64-linux-gnu/libSC.so"                # 是否悬空
   head -1 "$PE/files/lib/wine/dxvk/version"                      # 应为 v2.3
   ```

   **修复**：重跑 `fix-minion-masters-7g100-hw.sh`（幂等，已适配 `files/lib/` 新路径）。
   Steam 运行时改 vdf 会被覆盖，**建议先退出 Steam 再跑，跑完重启**。

---

## 九、日常使用与维护

### 启动
从 **Steam 库** 启动《Minion Masters》即可（不要命令行直跑）。

### 查看/切换渲染后端
```bash
~/project/lisuan/minion-masters/mm-render-switch.sh status   # 查看当前
~/project/lisuan/minion-masters/mm-render-switch.sh soft     # 切到 llvmpipe（当前推荐）
~/project/lisuan/minion-masters/mm-render-switch.sh gf       # 切到 7G100（会崩，仅备用）
```

> ⚠️ **Steam 运行时会把自己内存中的值写回 `localconfig.vdf`，覆盖脚本改动。**
> 用脚本切换前必须**完全退出 Steam**。
>
> 更安全的方式：直接在 Steam 客户端「属性 → 启动选项」里粘贴 4.1 的命令，不会被覆盖。

### 排障：如何判断是否健康

```bash
LOG=~/.local/share/Steam/steamapps/compatdata/489520/pfx/drive_c/users/steamuser/AppData/LocalLow/BetaDwarf\ ApS/Minion\ Masters/Player.log

grep -m1 "Renderer:" "$LOG"                  # 应为 llvmpipe
grep -c  "failed to create buffer" "$LOG"    # 应为 0
grep -c  "OnLoginStateChanged: LoggedIn" "$LOG"  # 应 >= 1
```

| 检查项 | 健康值 | 异常处理 |
|---|---|---|
| `Renderer:` | `llvmpipe` | 若是 `LISUAN 7G100`，说明启动选项未生效 |
| `failed to create buffer` | `0` | 非 0 说明走了 gf 硬件路径 |
| `LoggedIn` | `>= 1` | 为 0 说明 Vivox 被错误 patch |

### 重新编译 MMPatch（如需改补丁）
```bash
cd ~/project/lisuan/minion-masters/assets-mm-gf-vulkan/
# 编辑 MMPatch.cs 后：
dotnet build mmpatch.csproj -c Release
cp bin/MMPatch.dll "/home/ubuntu/.local/share/Steam/steamapps/common/Minion Masters/MMPatch.dll"
```

---

## 十、附录：7G100 硬件路径为何不通

### 架构矛盾

| 事实 | 说明 |
|---|---|
| 游戏是 **32 位** exe | `MinionMasters.exe`，PE Machine=0x14c (i386) |
| 砺算 gf 只发布 **64 位** Vulkan 驱动 | 仅有 `/usr/lib/x86_64-linux-gnu/libvulkan_gf.so`，**无 32 位版本** |
| 32 位游戏要用 64 位驱动 | 必须走 Proton 的 **WoW64**（`PROTON_USE_WOW64=1`） |

### 失败点

WoW64 下，32 位 DXVK 经 wine thunk 调用 64 位 gf 驱动时，
**显存映射失败**：

```
DxvkMemoryAllocator: Mapping memory failed with VK_ERROR_OUT_OF_HOST_MEMORY  (×18)
d3d11: failed to create buffer (target 0x1 mode 0 size 20) [0x80070057]
```

即 32 位 DXVK 无法访问 64 位驱动映射出的高位地址。

### 驱动本身是无辜的（已实测）

用 Python ctypes 直接调用 Vulkan，**原生 64 位环境下 gf 一切正常**：

| 测试 | 结果 |
|---|---|
| `vkMapMemory` (type 0/2/3，4MiB) | ✅ 全部成功 |
| `vkCreateBuffer` 20 字节 vertex buffer（各种 usage 组合） | ✅ 全部成功 |
| `vkAllocateMemory` + `vkBindBufferMemory` | ✅ 成功 |

### 已排除的其他嫌疑

| 嫌疑 | 结论 |
|---|---|
| 系统内存不足 | ❌ 可用 9.5 GB |
| `vm.max_map_count` 过小 | ❌ 已是 1048576（`/etc/sysctl.d/10-map-count.conf`） |
| 2GB 用户地址空间限制 | ❌ exe 已启用 `LARGE_ADDRESS_AWARE`（Characteristics=0x122） |
| `overcommit_memory` / `ulimit -v` | ❌ 分别为 0 / unlimited |
| MMPatch 干扰 | ❌ 禁用后映射失败依旧 18 次 |
| `dxvk.maxChunkSize=16` | ❌ 无效，失败次数不变 |
| DXVK 版本 v3.0.2 过新 | ❌ 换成 v2.3 后仍然失败（且触发 Steam 文件损坏） |

### 对比：llvmpipe 为何可以

`mesa-vulkan-drivers:i386` 提供了 **32 位** `libvulkan_lvp.so`。
32 位 DXVK 可直接调用同架构驱动，**无需经过 WoW64 thunk**，因此没有映射问题。

### 未来可尝试的方向

1. **砺算发布 32 位 Vulkan 驱动**（最干净，但闭源驱动希望不大）
2. **wine 修复 WoW64 的 Vulkan 内存映射**（可携本报告实测数据向上游报 bug，已证明驱动侧无辜）
3. 驱动更新后支持在 32 位地址空间映射显存

在此之前，使用 llvmpipe 软件渲染（本报告方案）是唯一可用路径。

---

*报告结束。配置以第五节"启动流程详解"与第四节"关键配置"为准。*
