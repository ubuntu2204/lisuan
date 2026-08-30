# 随从大师（Minion Masters）修复

> 适用机型：**砺算 7G100 GPU + Ubuntu 24.04 + Proton（Steam）**。
> 现象：Steam 里启动 Minion Masters 先后崩在 Vivox 登录 / 黑屏（Zenject 异常循环）；
> 修复后已能过 Vivox、Steam 验证、连上游戏服务器并**进入主菜单（BattleModeViewModel）**。
> 修复脚本：`./fix-minion-masters-7g100-hw.sh`（7G100 硬件渲染，**当前用这个**）；
> 另有 `./fix-minion-masters.sh`（llvmpipe 软件渲染路线，早期方案，仅存档）；
> 真正的崩溃修复是运行时 IL2CPP patch：**当前为 `MMPatch.dll`**（doorstop 直载，纯 P/Invoke，
> 源码 `assets/mm-gf-vulkan/MMPatch.cs`）。早期的 BepInEx 插件 `MMSkipVivox.dll`
> （源码 `/home/ubuntu/mm_patch/plugin/Patcher.cs`）已于 2026-08-26 退役，见游戏坑 1。
>
> **渲染状态（2026-08-30）**：7G100 **硬件渲染**已打通，见文末「游戏坑 8」。
> 本文只保留**可照做的成功方案**；失败尝试与排查弯路统一收在
> `MinionMasters-攻坚全记录.md`。
>
> **编号约定**：本文的「**游戏坑 N**」指**游戏/应用层**问题（Vivox、证书、BepInEx、Proton 配置等）；
> 显卡/渲染层问题用「**显卡坑 N**」，统一记录在 `MinionMasters-攻坚全记录.md`。
> 两套数字会撞车但含义不同——例如**游戏坑 3 = 服务端 API 404**，
> 而**显卡坑 3 = Steam 报内容文件损坏**，别看混。

---

## TL;DR

随从大师是 Unity / IL2CPP 游戏，坑按排除顺序共五层：

1. **崩在 Vivox 语音登录**（已修复）：游戏在 `VivoxChatServiceAdapter.LoginAsync` 里 fire-and-forget
   调用未处理异常的 async（`UniTask` 的 `PublishUnobservedTaskException`），导致进程崩在 IL2CPP
   机器码（GameAssembly.dll 内），无法按托管名 patch。修复 = 运行时把该函数 prologue 改掉，
   让它立即返回。**当前由 `MMPatch.dll` 经 doorstop 直载完成**（见游戏坑 1）；
   早期的 BepInEx 插件方案已被取代，原因见游戏坑 8 障碍 4。
2. **黑屏 + BepInEx 控制台无限刷 `NotSupportedException`**（已修复）：证书 patch
   （`CertificateHandler.ValidateCertificate`）反而破坏了 IL2CPP runtime 状态，引发 Zenject
   `ProjectContext` 重复创建 assert + `BootstrapInstaller.Start()` NullReference + 异常提取栈
   死循环（见游戏坑 2）。禁用证书 patch、改 patch `DownloadHandlerTexture.InternalGetTexture` 后解决。
3. **图片下载证书错误导致的致命 Texture 异常**（已修复）：`Curl error 60` →
   `Texture has not yet finished downloading` 异常沿 UniTask `TrySetException` 链传播。
   修复 = 把 `InternalGetTexture` patch 成返回 null（见游戏坑 1 的 patch c）。
4. **联网可达性**（外部环境，非配置可解）：游戏后端 `api.prod.mm21.minionmastersthegame.com`
   **所有 `/v1/*` API endpoint 返回 404**（`/health` 却返回 200 "Healthy"）→ `LoginQueueService.Post`
   抛 `Http error code was 404`，登录队列进不去；另有 Azure IP `140.228.24.145:443` 封锁（见游戏坑 3）。
5. **渲染后端**：已能用 **7G100 硬件渲染**（不再是 llvmpipe）。要点是绕过 gf 的发行版白名单、
   处理 `vkCreateDevice` 依赖的 `libSC.so`、把 DXVK 降到 v2.3、以及把落在 >4GB 的显存映射
   重定位到 2~4GB（见游戏坑 8）。**系统驱动保持原版，改动隔离在 LD_PRELOAD**。

当前状态（**2026-08-30**）：游戏能完整走完 IL2CPP 加载 → Steam 身份验证（拿到
Ticket、缓存 SteamID `76561198811581867`）→ WebSocket 连游戏服务器（`Connection opened`）→
拉到 SteamShop 商品列表 → **进入主菜单 `BattleModeViewModel`**（PracticeDifficultyView、
MetaTutorialStateService 均加载），**渲染后端为 7G100 硬件渲染**（2026-08-30 验证：
buffer 错误 0、纹理错误 0、主菜单标记 29）。

但游戏进程会在启动后 **~40–90 秒被移除**（Steam console_log
`Game process removed: AppID 489520`），时间点在登录队列 404 之后——这是**服务端问题**，
与显卡无关，剩余阻塞见游戏坑 3 / 游戏坑 7。

**固化脚本（两个，分工不同——别用错）**：

| 脚本 | 路线 | 当前是否该用 |
|---|---|---|
| `minion-masters/fix-minion-masters-7g100-hw.sh` | **7G100 硬件渲染** | ✅ **用这个** |
| `minion-masters/fix-minion-masters.sh` | llvmpipe 软件渲染（早期路线） | ❌ 仅存档/对比 |

```bash
sudo ./minion-masters/fix-minion-masters-7g100-hw.sh
```

> 早期笔记里提到的宿主启动器 `mm.sh`（先 kill 残留再 `valve-run` → `steam -applaunch 489520`）
> **已不存在**，2026-08-30 核实无此文件。现在直接从 Steam 启动即可。

---

## 游戏坑 1：Vivox 登录崩溃 + Texture 异常（已修复，插件三件套）

### 根因
游戏崩在 IL2CPP 机器码 `VivoxChatServiceAdapter.LoginAsync`（位于 GameAssembly.dll 内）。
这是 fire-and-forget async 未处理异常（UniTask 的 `PublishUnobservedTaskException`）。
该类型/方法**不在** Il2CppInterop 生成的 interop 程序集里，所以常规的 Harmony 按托管名 patch 行不通。

### 修复方案（当前部署：`MMPatch.dll` 10752 B）

> **2026-08-30 更正**：本节原写的是 BepInEx 6 插件 `MMSkipVivox.dll`（v1.1.1 / 15360 B）。
> 该方案在 new WoW64 下**已不加载**（BepInEx Chainloader 停在初始化，见游戏坑 8 障碍 4），
> 已被 `MMPatch.dll` 取代。旧方案移记为「历史方案」保留在文末。

**当前生效的注入链**：`winhttp.dll`（doorstop）→ `MMPatch.dll` → P/Invoke 直接打 patch。

| 项 | 值 |
| --- | --- |
| 注入器 | `winhttp.dll`（doorstop 4，`/run/host` 无关），配置 `doorstop_config.ini` |
| `target_assembly` | **`MMPatch.dll`**（`~/.local/share/Steam/steamapps/common/Minion Masters/`） |
| 源码 | `assets/mm-gf-vulkan/MMPatch.cs`（netstandard2.1，纯 P/Invoke） |
| 依赖 | **不依赖** BepInEx.Core / Il2CppInterop |
| 触发时机 | `Doorstop.Entrypoint.Start` 起线程轮询 `il2cpp_domain_get()` 就绪后打 patch |

之所以改用 doorstop 直载：new WoW64 下 Il2CppInterop 的 runtime-invoke hook 不触发，
`Chainloader.Start()` 永不执行 → 插件不加载 → Vivox 崩溃回归。

三个 patch **目标与地址不变**（32 位 wine 运行时绝对 VA，实测跨启动稳定）：

| # | 目标 | 地址 | 改法 | 目的 |
| --- | --- | --- | --- | --- |
| a | `VivoxChatServiceAdapter.LoginAsync` / `LogoutAsync` | `0x7983BD0` / `0x7983C90` | prologue → `E9 rel32` 跳 xor trampoline（全零 UniTask） | 跳过 Vivox 语音登录 |
| b | `NewsItemViewModel+<DownloadSpriteFromUrlAsync>d__56.MoveNext` | `0x6630500` | prologue → `E9 rel32` 跳 `ret` | 跳过新闻图片下载状态机 |
| c | `UnityEngine.Networking.DownloadHandlerTexture.InternalGetTexture` | `0x97CE7B0` | prologue → `31 C0 C3`（`xor eax,eax; ret` 返回 null） | **cert 失败时不再抛 `Texture has not yet finished downloading`** |

定位方式（Il2CppInterop 运行时 API）：`il2cpp_domain_get()` →
`il2cpp_domain_get_assemblies(domain, ref uint count)`（`IntPtr*`，unsafe 遍历）→
`il2cpp_assembly_get_image(asm)` → `il2cpp_class_from_name(image, ns, name)` 定位
Il2CppClass* → `il2cpp_class_get_method_from_name(klass, methodName, -1)` 得到
Il2CppMethodInfo* → 读 `methodInfo + 0` = methodPointer（**注意**：wine 下是 32 位运行时绝对
VA，**已被重定位**，不是 PE 镜像基址，不要套 PE 基址）→ `kernel32.VirtualProtect` 改页 R/W/E
改 prologue。

patch a 的 trampoline 演进（**重要教训**，源自 `MMSkipVivox` 时期，`MMPatch` 沿用其最终版）：

- **v1.0.0（旧，有隐患）**：prologue 直接覆盖 `31 C0 31 D2 C3`（xor eax,eax; xor edx,edx; ret）
  返回**全零 struct**。游戏 **8/23 更新前**能跑；更新后 `ChatService.LoginAsync` 改为 `await`
  Vivox 返回，全零 UniTask 的 `GetAwaiter()` 访问 `null` runner → `NullReferenceException` →
  退出（e0434352）。
- **v1.1.0 尝试**：优先 `get_CompletedTask` getter 跳转 / 运行时读静态字段地址构造 trampoline；
  本机 IL2CPP 没生成该 getter，**回退 xor 方案**（`31 C0 31 D2 C3` 的远跳版）。
  实测 8/24 版游戏下全零 UniTask 已不再致命（配合 patch b/c），故当前即为 xor 回退版在跑。

> 完整演进（两次尝试的细节、`MMSkipVivox` 退役过程与现场文件痕迹）见
> `MinionMasters-攻坚全记录.md` → 附录 B「已废弃方案：BepInEx 6 + `MMSkipVivox.dll`」。

patch c 的异常传播链（为什么必须 patch 这里）：`Curl error 60`（证书校验失败）→
`DownloadHandlerTexture.InternalGetTexture()` 抛 `InvalidOperationException: Texture has not
yet finished downloading` → `NewsItemViewModel+<DownloadSpriteFromUrlAsync>d__56.MoveNext()` →
`SimpleCoroutineAwaiter.Complete(T, Exception)` → `UniTaskCompletionSourceCore.TrySetException` →
`UniTaskScheduler.PublishUnobservedTaskException` →（fire-and-forget 的
`HandleSafeFireAndForget`）→ Unity `Uploading Crash Report`。只 patch MoveNext 拦不住
awaiter 链上的二次传播，**在异常源头（InternalGetTexture）返回 null 才彻底**。

### 部署现状（2026-08-30 核实）

| 内容 | 位置 / 状态 |
| --- | --- |
| `MMPatch.dll`（10752 B） | 游戏根目录 ✅ **当前生效** |
| `MMPatch.dll.disabled-20260826`（10240 B） | 同目录，旧版已禁用 |
| `doorstop_config.ini` | `target_assembly = MMPatch.dll` ✅ |
| `doorstop_config.ini.bak-bepinex` | BepInEx 时代的备份 |
| `winhttp.dll`（22016 B） | 游戏根目录，doorstop 注入器 ✅ |
| `winhttp=n,b` 覆盖 | `pfx/user.reg` 第 982 行 ✅ |
| `BepInEx/` | **已重命名为 `BepInEx.disabled-20260826/`**，plugins 为空 → 不再参与启动 |
| `MMSkipVivox.dll` | 仅存于 `BepInEx.disabled-*/plugins/` 与 `~/mm_patch/backup/` → **已退役** |
| dxgi / d3d11 | 已从游戏目录移除（避免 pressure-vessel 路径冲突） |

验证：`Player.log` 出现 `BattleModeViewModel`、`MetaTutorialStateService loaded`；
**不再出现** `Texture has not yet finished downloading`。
（new WoW64 下不再有 `BepInEx/LogOutput.log`，插件日志改由 MMPatch 自身输出到 wine  stderr。）

> 已排除的弯路：禁用 Vivox DLL 反而让游戏更早崩在登录队列；Vivox 必须"跳过"而非"禁用"。

---

## 游戏坑 2：证书 patch 回归——黑屏 + NotSupportedException 无限刷屏（已修复，重大教训）

**现象**：BepInEx 控制台持续刷 `NotSupportedException: Derived classes must provide an
implementation`，主窗口黑屏，Player.log 停在异常循环（几万行），游戏进程最终消失。

**根因**：曾给插件加过 `PatchCertVerification()`——patch
`UnityEngine.Networking.CertificateHandler.ValidateCertificate`（`0x75D98C0`）让它永远返回
true。实测该 patch **破坏 IL2CPP runtime 状态**：

1. `ZenjectException: Assert hit! Tried to create multiple instances of ProjectContext!`
   （`SceneContext.Awake()` → `ProjectContext.get_Instance()`）；
2. `NullReferenceException at Assets.Core.Platform.Common.Installers.BootstrapInstaller.Start()`；
3. 之后每个异常走 `StackTraceUtility.ExtractStringFromExceptionInternal` → `MethodBase.GetGenericArguments()`
   → IL2CPP 未实现 → 再抛 `NotSupportedException` → 循环刷屏，**把真正的原始异常淹没**。

> 注：单独的 `Tried to create multiple instances of ProjectContext` assert 在**正常成功运行的
> Player.log 里也出现**（每个场景的 SceneContext.Awake 都会触发一次），是良性告警，不是杀手；
> 杀手是它后面跟着的 BootstrapInstaller NullReference 循环。

**修复**：**彻底禁用 `PatchCertVerification`**（源码里只留注释）。证书问题的正确处理 =
游戏坑 1 的 patch b/c（跳过下载 + 异常源头返回 null）。经验：**patch Unity 内部 native 方法前先
确认它不是共享 helper**——`0x75D98C0` 原字节就是 `32 C0 C3 CC CC`（一个已被优化成
"return false" 的共享 stub），patch 它会波及所有调用方。

**排障技巧**：看到 NotSupportedException 刷屏时，用
`grep -vE "NotSupportedException|Couldn't extract|Derived classes|at System.Reflection|..."` 过滤
噪音，才能看到最初 20 行里的**真正异常**（Zenject + BootstrapInstaller）。

---

## 游戏坑 3：联网可达性（外部环境，非配置可解）

两层问题（2026-08-25 实测 curl 复核）：

1. **游戏后端 API endpoint 404（当前主要阻塞）**：`LoginQueueService.Post` 抛
   `InvalidOperationException: Http error code was 404`。直接 curl 验证：
   - `https://api.prod.mm21.minionmastersthegame.com/health` → **200 "Healthy"**（服务器活着）；
   - `https://api.prod.mm21.minionmastersthegame.com/` → 200（HTML 页面）；
   - `/v1/loginQueue`、`/v1/buildversion/...` 等**所有 `/v1/*` API 路径 → 404**。

   即服务器健康但后端 API 已下线/迁移（BetaDwarf 服务端问题，本地无解）。游戏内
   `BuildVersionToApiService` / `RemoteConfigurationService` 的请求却能成功（走的是仍存在的
   endpoint），说明是**部分路径**缺失。修复方向：等厂商恢复后端，或抓包确认新 API 路径。
2. **Azure IP 封锁**：游戏需连 Azure 服务器 **`140.228.24.145:443`**（及 `:33798`），该 IP 的
   TCP 443 在当前网络环境被阻断（`SYN_SENT` 卡住、超时）。修复方向是等网络恢复或用 VPN/代理
   绕过封锁，**不是**改 Proton/游戏配置。

---

## 游戏坑 4：Proton 版本

> **⚠️ 本节结论于 2026-08-30 更正。** 原文"必须用 Proton 8.0"是 llvmpipe 软件渲染时期的结论，
> 在 7G100 硬件渲染方案下**已不成立，且正好相反**。旧结论保留在下方"历史"小节备查。

**当前结论：必须用支持 new WoW64 的 Proton（Experimental / wine 11.0 系）。**

实测依据（`proton` 脚本中 `PROTON_USE_WOW64` 的出现次数）：

| Proton | `PROTON_USE_WOW64` 次数 | 能否用于硬件渲染 |
|---|---|---|
| `Proton 8.0` | **0** | ❌ **不支持 new WoW64** |
| `Proton - Experimental` | 3 | ✅ |
| `Proton - MM GF`（本项目自定义副本） | 3 | ✅ |

**原因**：32 位游戏要调 64 位 gf 驱动，必须经 new WoW64 thunk。Proton 8.0 只有传统 wow64
（32 位 winevulkan），会去找**并不存在的 32 位 Vulkan ICD**（砺算无 i386 版，且 gf 闭源无法自编）。
因此硬件渲染路径下 Proton 8.0 直接出局。

**当前部署**：

- CompatToolMapping `489520` → `proton_experimental`；
- 或使用 `Proton - MM GF`（`version` = `experimental-11.0-20260826-x86_64`），
  它是 Experimental 的**独立副本**（2026-08-28 复制，inode 不同），内置 DXVK v2.3 降级与
  `libSC.so` 软链，**不会被 Experimental 的自动更新冲掉**——长期用更省心（见游戏坑 8）。
- 启动选项须带 `PROTON_USE_WOW64=1`（Experimental 默认只对白名单游戏开 new WoW64）。

> **注意用哪一份（2026-08-30 核实）**：CompatToolMapping 当前指向 `proton_experimental`，
> 所以**实际生效的是 Experimental 那份 DXVK**，不是 `Proton - MM GF`。
> 好消息是两份目前**内容一致**（`dxgi.dll` md5 均为 `fb6cf30d…`，版本串均含 `v2.3`），
> 即降级在两处都已落地。若日后 Experimental 被 Steam 更新覆盖，游戏会退回 v3.x 而失败
> （v3.0.2 硬要求 `VK_KHR_maintenance5`，gf 不支持）——届时把 CompatToolMapping 切到
> `Proton - MM GF`，或重跑固化脚本即可。

### 历史：为什么曾经推荐 Proton 8.0（旧结论，已失效）

llvmpipe 软件渲染时期记录为："Proton Experimental / wine 11.0 在 7.0.0-30 内核下有
32 位地址空间回归，游戏崩在 `0x01160000`，Proton 8.0 绕过。"

**该归因后来被推翻**：那个崩溃的真正原因是 **winex11 的 64 位 OpenGL 初始化失败**
（`libGL error: No driver found`）后崩进 `handle_syscall_fault` 死循环，与 Proton 版本无关。
用 `__GLX_VENDOR_LIBRARY_NAME=mesa LIBGL_ALWAYS_SOFTWARE=1` 让 winex11 走 llvmpipe GL 即可绕过，
且**只影响 winex11 的 GL 探测，不影响 DXVK 走 7G100**。详见游戏坑 8「附带」小节。

保留此段是为了解释"为什么旧笔记里到处写着要用 Proton 8.0"——**那些笔记已过期**。

---

## 游戏坑 5：user_settings.py 注入无效（教训，不要再踩）

**结论：不要用 `user_settings.py` 注入显卡相关的环境变量。**

- 格式是顶层键值对，嵌套 `session_environment` 会 `TypeError` 导致游戏不启动；
- 即便格式正确，自定义变量**不会进入 pressure-vessel 容器**（只传白名单 env），
  注入 `VK_ICD_FILENAMES` / `WINEDLLOVERRIDES` 均无效。

当前部署：**已移除自定义 user_settings.py**（保持默认）。显卡相关变量一律走
Steam 启动选项（LaunchOptions）。

> Steam 平台层的通用说明（容器白名单、格式陷阱）见
> [`docs/steam-on-lisuan.md` §7](../steam/steam-on-lisuan.md)。

---

## 游戏坑 6：Steam "游戏运行中" 状态卡死

> 平台层通用说明见 [`docs/steam-on-lisuan.md` §5](../steam/steam-on-lisuan.md)。

每次 `pkill` 强杀游戏后，Steam 会卡在"游戏运行中"状态，后续 `steam -applaunch 489520` 被静默忽略，
游戏起不来。必须 `steam -shutdown` 彻底重启 Steam 才能再次启动。调试时若发现 applaunch 拉不起游戏，
先怀疑这个，而不是游戏本身。

---

## 游戏坑 7：游戏进程启动后 ~40–90 秒被移除（当前未解，与显卡无关）

**现象**：游戏已能进入主菜单（`BattleModeViewModel` 出现在 Player.log），但 Steam
console_log 在启动后 38–90 秒不等出现 `Game process removed: AppID 489520`，进程消失。
无 crash dump、无 .NET 致命异常。

**主因怀疑**：登录队列 404（游戏坑 3）之后游戏自身超时退出——
`LoginQueueService.EnterQueueAndWaitForTurn` 的 404 异常让流程走不下去。属服务端问题，
本地无解，需等后端恢复后复测。

**已排除**：`CAPIJobRequestUserStats` 报错、Zenject assert（良性，游戏坑 2）、Texture 异常（已 patch，游戏坑 1c）。

> 排查细节（排除过程、观察退出路径的 hook 思路）见
> `MinionMasters-攻坚全记录.md` → 附录 B「游戏层（非显卡）的教训与未解问题」。

---

## 验证

```bash
# 1) Launcher 已注入（指向宿主 mm.sh）
cat ~/.steam/root/steamapps/common/Minion\ Masters/MinionMasters.sh | grep -A3 gate

# 2) 从 Steam 启动后（BepInEx/LogOutput.log）应看到三条 patch：
#    - [Cert] Fallback PATCHED 1 MoveNext methods          (NewsItem 下载跳过)
#    - [Cert] DownloadHandlerTexture.InternalGetTexture PATCHED -> return null
#    - [MMSkipVivox] PATCHED 2/2 Vivox async methods
# 3) Player.log（~/.steam/steam/steamapps/compatdata/489520/pfx/drive_c/users/steamuser/
#    AppData/LocalLow/BetaDwarf ApS/Minion Masters/Player.log）里程碑：
#    - SteamAuthenticationService: Ticket N（Steam 验证通过）
#    - WebSocketSharpTransportService] Connection opened（连上游戏服务器）
#    - [BattleModeViewModel] Updating invites 0（进入主菜单）
#    - [MetaTutorialStateService] loaded meta tutorial state
# 4) 渲染后端（grep "Renderer:"）—— 应为硬件渲染：
#      Renderer: LISUAN 7G100 Series Graphics (ID=0x5006)
#    若为 llvmpipe / 其他，说明 7G100 未生效，按「游戏坑 8」逐项排查：
#      - Steam 启动选项是否含 LD_PRELOAD=<仓库>/minion-masters/gf-compat/lib/osrelease_redirect.so
#      - 是否 PROTON_USE_WOW64=1
#      - libSC.so 软链是否被 Proton 更新冲掉（重跑固化脚本）
```

若启动后崩在 Vivox（无 PATCHED 日志）：确认 `MMSkipVivox.dll` 在 `BepInEx/plugins/` 且 winhttp 覆盖生效。
若黑屏 + BepInEx 控制台刷 NotSupportedException：确认插件是**禁用证书 patch 的版本**（见游戏坑 2）。
若进主菜单后 ~1 分钟进程消失：属游戏坑 7（当前未解，先确认游戏坑 3 的 API 404 状态）。

**Player.log 排障技巧**：Unity 每次启动会把上次的 `Player.log` 轮转为 `Player-prev.log`；看
`ls -la` 的 mtime 区分本次/上次运行，别拿旧日志下结论。日志被 NotSupportedException 刷屏时先
用 grep -vE 过滤噪音（见游戏坑 2 的排障技巧）。

---

## 附录 A：渲染层与 Steam 登录层额外坑（与 Vivox 无关）

> 让 7G100 **硬件渲染**的突破在文末**「游戏坑 8」**（因其为最后追加，位置在附录之后），
> 若你要找的是显卡相关配置，请直接跳到那里。

> 这些坑在排障时容易和 Vivox 崩溃混淆，单独列出。细节亦见 README 第七章 7.2 / 7.3 / 7.4。

### A1. `d3d11: failed to create factory`（渲染设备创建卡死）—— **已过时，仅存档**

> **2026-08-30 核实：本节在 7G100 硬件渲染方案下已不适用。**
> 当前日志中 `failed to create factory` 出现 **0 次**，且本节建议的
> `PROTON_USE_WINED3D=1`（wined3d 软件渲染）与硬件渲染目标**背道而驰**——照做会退回 llvmpipe。
> 原始内容已移存 `MinionMasters-攻坚全记录.md` 附录 B，此处不再作为操作建议。

### A2. 无头环境 Steam 客户端登录（更新 2026-08-25：自动登录可自愈）

> Steam 平台层完整说明见 [`docs/steam-on-lisuan.md` §4](../steam/steam-on-lisuan.md)。

本机缺失 `ssfn` 登录令牌（只有 16 字节 `steam.token` 不足）时，首次登录仍需图形界面。
但**登录过一次后**（`config.vdf` 已有 `AutoLoginUser=godot0413301` + `RememberPassword=1`），
**重启 Steam 客户端即可自动重新登录**，无需再次交互：

```bash
pkill -9 -f "ubuntu12_32/steam"; pkill -9 -f webhelper; sleep 5
DISPLAY=:0 nohup steam -silent > /tmp/steam.log 2>&1 &
# ~30 秒后 webhelper_js.txt 应出现:
#   Login: OnLoginStateChange godot0413301 5 1 0 0   ← state 5 = 完全登录
# loopback HTTPS server 自动监听 127.0.0.1:57343
```

**排障要点**：游戏拿不到 Steam 身份（`steamid=0`）时先看
`~/.local/share/Steam/logs/webhelper_js.txt` 里 `OnLoginStateChange` 的最新状态；发现陈旧的
`steamid=0`（登录态丢失）只需重启 Steam 客户端。Steam 启动初期 CEF 对 `steamloopback.host`
的 `handshake failed` 是 loopback server 还没起来的暂态错误，等几十秒即消失，不要误诊为网络问题。

首次登录（无任何凭证）仍受下述限制：无头 XWayland 下 CEF 登录窗 `X Error: BadValue
(opcode 149 = GLX)` 崩溃 → 永远 `steamid=0` → 游戏 `e0434352` 退出。**必须在图形界面登录一次
生成 `ssfn`**。

### A3. Standard shader 不支持（画质告警，不致命）

Player.log 偶见 `ERROR: Shader Standard shader is not supported on this GPU`
（gf / llvmpipe 不支持 Unity Standard shader）。这通常**不导致进程退出**，仅影响画质，
无需处理。

### A4. `il2cpp_class_get_methods` 陷阱（不要用）

想枚举一个 IL2CPP 类的**全部**同名方法（如多个 async 状态机的 `MoveNext`）时，
`IL2CPP.il2cpp_class_get_methods(klass, ref IntPtr)` **不可用**：

- BepInEx/Il2CppInterop 的 binding 把第二个参数声明成 `ref IntPtr`，而 native 真实签名是
  **iterator 协议**（`void* il2cpp_class_get_methods(Il2CppClass*, void** iterator)`，每次调用
  返回下一个 MethodInfo*，iterator 由调用方保存）；
- 按"返回数组首地址 + count"解读会得到垃圾 count（实测 621846768），继续按数组遍历
  → `Marshal.PtrToStringAnsi` 访问野指针 → **0xC0000005 Access Violation，游戏闪退**。

**结论**：枚举方法一律用 `il2cpp_class_get_method_from_name(klass, name, -1)`（拿第一个匹配）。
若确实需要"所有同名方法"，只能扫描 GameAssembly 的导出/metadata，别走这个 API。

### A5. BepInEx 插件版本号显示的是 AssemblyVersion

BepInEx 日志里 `Loading [MM Skip Vivox 1.0.0]` 显示的是**程序集版本**
（`AssemblyVersion`），不是 `[BepInPlugin(Version = "1.1.0")]` 属性。插件源码需要显式：

```csharp
[assembly: AssemblyVersion("1.1.0.0")]
[assembly: AssemblyFileVersion("1.1.0.0")]
```

否则 BepInPlugin 写 1.1.0、日志仍显示旧 assembly 版本，排障时容易拿错"当前跑的是哪个 dll"。
（csproj 若用默认规则，会一直生成 1.0.0.0。）

---

## 游戏坑 8（2026-08-26 突破）：让 7G100【硬件渲染】——new WoW64 + gf 发行版白名单绕过

此前路线（游戏坑 1-7）游戏只能用 **llvmpipe 软件渲染**。本章记录让游戏真正用 **7G100 显卡渲染 D3D11** 的完整方案（固化脚本 `minion-masters/fix-minion-masters-7g100-hw.sh`，资产 `assets/mm-gf-vulkan/`）。

### 根本矛盾：32 位游戏 vs 只有 64 位 gf 驱动

- Minion Masters 是 **32 位** Windows Unity 游戏（UnityCrashHandler32.exe 可证）。
- Proton 8.0（传统 wow64）下 32 位游戏 → 32 位 winevulkan → **需要 32 位 Vulkan ICD**；
  而砺算只为 amd64 发布 `libvulkan_gf.so`（`lisuangpu-lib` 无 i386 版），**32 位 ICD 不存在**，
  且 gf 是闭源 RADV fork（无源码可自编 32 位）。
- 唯一出路：**Proton Experimental 的 new WoW64**（`PROTON_USE_WOW64=1`）——32 位游戏代码
  经 wine 的 wow64 thunk 调用 **64 位** vulkan ICD。**注意**：Experimental 的 new WoW64 默认
  只对白名单游戏开，其他游戏必须显式 `PROTON_USE_WOW64=1`（proton 脚本 1543 行逻辑）。
- 另：前缀必须是 new WoW64 布局——旧前缀沿用传统布局，**把 pfx 移走让 proton 重建**才生效。
  （重建后 `user.reg` 里的 `winhttp=n,b` 会丢，需重注入，否则 doorstop 不加载。）

### 障碍 1：gf 驱动的「发行版白名单」

容器内 DXVK 永远只看到 llvmpipe。逐层排查（strace/LD_DEBUG/hook strcmp）定位：
`libvulkan_gf.so` 初始化时读 `/etc/os-release`，与内置名单
（`Kylin/uos/openEuler/TencentOS Server/Deepin/Ubuntu`）比对，**不在名单直接返回 0 个物理设备**
（vulkan loader 日志：`Removing driver libvulkan_gf.so due to not having any physical devices`）。
宿主机 NAME=Ubuntu 通过；Steam Runtime 容器 NAME="Steam Runtime" 被拒。

**绕过**：`LD_PRELOAD` hook 把 `/etc/os-release` 的 `open/openat/fopen` 重定向到伪造的 Ubuntu
文件（`assets/mm-gf-vulkan/osrelease_redirect.c` + `os-release`）。
→ 容器内 `vkEnumeratePhysicalDevices` 立即拿到 `LISUAN 7G100`（"TL"，vendor 0x4c54）。

### 障碍 2：vkCreateDevice 依赖伴生库 libSC.so

枚举通过后 `vkCreateDevice` 返回 `-1`（OUT_OF_HOST_MEMORY）。`libvulkan_gf.so` 运行期
`dlopen("libSC.so")`（砺算着色器编译器）。它只存在于 `/run/host/usr/lib/x86_64-linux-gnu/`，
**不在容器 ld 搜索路径**（pressure-vessel 重新生成 ld.so.cache，不含 /run/host；proton 又会用
自己的 `LD_LIBRARY_PATH` 覆盖 LaunchOptions 传的）。

**修复**：把 `libSC.so` 软链进 `Proton - Experimental/files/lib/x86_64-linux-gnu/`
（proton 设置的 `LD_LIBRARY_PATH` 首位就是它）。链接目标用容器内路径
`/run/host/usr/lib/x86_64-linux-gnu/libSC.so`。

### 障碍 3：DXVK 版本 vs gf 扩展

- Experimental 自带 **DXVK v3.0.2** 硬要求 `VK_KHR_load_store_op_none` + `VK_KHR_maintenance5`，
  gf（driver 22.0.4）都没有 → `Skipping: Device does not support required feature ...` → 无适配器。
  （maintenance5 含 gf 没有的新函数，LD_PRELOAD 伪造扩展会导致调用空指针，**不可伪造**。）
- **修复**：降级为 **Proton 8.0 的 DXVK v2.3-21**（不要求上述扩展）。注意 Proton 8.0 的目录布局：
  32 位 DXVK 在 `dist/lib/wine/dxvk/`（proton 脚本把它同步到 syswow64），64 位在 `dist/lib64/wine/dxvk/`；
  别误用 `dist/lib/wine/i386-windows/`（那是 wine **builtin** d3d11，import wined3d）。
- 已知非致命噪音：v2.3 下 Unity 会报一次 `d3d11: failed to create buffer (size 20) [0x80070057]`，
  **不影响继续运行**（实测游戏照常推进，属 v2.3 + wow64 的良性报错）。

### 障碍 4：wine11 wow64 下 BepInEx 插件不加载

new WoW64 下 BepInEx 停在 `Chainloader initialized`——Il2CppInterop 的 runtime-invoke hook
在 wine11 wow64 下不触发，Chainloader.Start 永不执行，MMSkipVivox 插件不加载 → Vivox 崩回归。

**修复**：放弃 BepInEx Chainloader，改用 **doorstop 直载的纯 P/Invoke 补丁**
`assets/mm-gf-vulkan/MMPatch.cs`（netstandard2.1，`Doorstop.Entrypoint.Start` 起线程轮询
`il2cpp_domain_get()` 就绪后，用 `il2cpp_class_get_method_from_name` + VirtualProtect 打同样三个
patch），`doorstop_config.ini` 的 `target_assembly` 指向 `MMPatch.dll`。**完全不依赖**
BepInEx.Core / Il2CppInterop。实测 wow64 下 patch 地址与传统模式一致（如 LoginAsync @0x75963BD0）。

### 附带：winex11 GL 初始化崩溃（游戏坑 4"0x01160000"的真面目）

wine11 的 `winex11.drv` 在容器内初始化 64 位 OpenGL 失败（`libGL error: No driver found`）
后会直接崩进 `handle_syscall_fault` 死循环（这就是游戏坑 4记录的"32 位地址空间回归"表象）。
`__GLX_VENDOR_LIBRARY_NAME=mesa` + `LIBGL_ALWAYS_SOFTWARE=1` 让 winex11 走 llvmpipe GL
即可绕过；**只影响 winex11 的 GL 探测，不影响 DXVK 走 7G100**。

### 障碍 5（2026-08-29 突破）：显存映射落在 >4GB —— 最终解法

**现象**：枚举、建 device 都过了，DXVK 一分配显存就失败：

```
err: D3D11: Failed to map memory ...
d3d11: failed to create buffer (size 20) [0x80070057]
d3d11: failed to create 2D texture width=4096 height=4096 ...
```

**根因**：gf 的 `vkMapMemory` 返回的 CPU 指针在高地址（实测 `0x740bdfaec000`）。
wine 的 WoW64 thunk 无法把 `>4GB` 指针交给 32 位代码，于是主动 `unmap` 并合成
`VK_ERROR_OUT_OF_HOST_MEMORY`。

**解法**：在 `mmap` 层把 GPU 映射搬到 **2~4GB**。

```c
#define LOW4_START 0x90000000ULL   /* 2.25GB */
#define LOW4_END   0x100000000ULL  /* 4GB    */

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

两个关键点：

1. **为什么锚定 2~4GB，而不是用 `MAP_32BIT`**：`MAP_32BIT` 的语义是"进程前 **2GB**"，
   而游戏进程低 2GB 已被 wine 占满（实测 2043/2048MB），大纹理一映射就 `ENOMEM`；
   2~4GB 区间基本空闲，实测单块可搬 **24MB**。
2. **为什么判断条件是"结果地址 ≥4GB"，而不是"flags 含 MAP_32BIT"**：早期版本按 flags
   判断（反汇编 `26eb: and $0x40,%eax`），导致必须给**系统驱动**打 `MAP_32BIT` 补丁才能触发
   重定位——那是全局的，会让 64 位原生的《刀塔霸业》一启动就段错误。改成看结果地址后，
   **驱动完全不需要改动**。

`is_gpu_fd()` 用于避免误伤：只认 DRM 字符设备（主设备号 226）和 dma-buf 匿名文件，
普通文件映射（加载 .so，都是 `MAP_PRIVATE`）和匿名内存不受影响。

> 完整尝试历程（含在 Vulkan ICD 层拦截 `vkMapMemory` 等被放弃的支路）见
> `MinionMasters-攻坚全记录.md` 阶段 9–13。

### 验证与最终配置

- `Player.log`: `Renderer: LISUAN 7G100 Series Graphics (ID=0x5006)`，连续多次运行稳定。
- 2026-08-30 验证（系统驱动为原版、未打补丁）：buffer 错误 0、纹理错误 0、主菜单标记 29、
  日志 1564 行、低 4GB 重定位 10005 次。

**LaunchOptions（最终，固化脚本写入）**：

```
LD_PRELOAD=<仓库>/minion-masters/gf-compat/lib/osrelease_redirect.so PROTON_USE_WOW64=1 %command%
```

- CompatToolMapping: 489520 → `proton_experimental`（或自定义 `Proton - MM GF`）。
- `VK_ICD_FILENAMES` **已不再需要**——直接用系统原版驱动。
- **注意**：Proton Experimental 每次更新都会覆盖 libSC 链接与 DXVK 降级，更新后需重跑固化脚本。

**关于 Steam 里的实际 LaunchOptions（2026-08-30 核实）**：

当前 Steam 存的是固化脚本早期写入的版本，比上面的最简形式多两个变量：

```
PROTON_USE_WOW64=1 LD_PRELOAD=<仓库>/minion-masters/gf-compat/lib/osrelease_redirect.so
__GLX_VENDOR_LIBRARY_NAME=mesa LIBGL_ALWAYS_SOFTWARE=1
VK_ICD_FILENAMES=<仓库>/minion-masters/gf-compat/gf_icd.json
VK_LOADER_LAYERS_DISABLE=VK_LAYER_VALVE_steam_overlay_64,VK_LAYER_VALVE_steam_overlay_32
%command%
```

| 变量 | 是否仍需要 | 说明 |
|---|---|---|
| `PROTON_USE_WOW64=1` | ✅ 必需 | 32 位游戏调 64 位驱动的唯一途径 |
| `LD_PRELOAD=…osrelease_redirect.so` | ✅ 必需 | 发行版白名单绕过 + 低 4GB 重定位 |
| `__GLX_VENDOR_LIBRARY_NAME=mesa`<br>`LIBGL_ALWAYS_SOFTWARE=1` | ✅ 需要 | 让 **winex11** 的 GL 探测走 llvmpipe，避免其崩进 `handle_syscall_fault`；**不影响 DXVK 走 7G100** |
| `VK_ICD_FILENAMES` | ⚠️ **冗余** | 2026-08-30 实测不带它也能用系统原版驱动跑通（见上文验证）。留着无害（指向 `gf-compat/gf_icd.json`），但精简配置时可去掉 |
| `VK_LOADER_LAYERS_DISABLE` | ✅ 建议保留 | 禁用 Steam overlay 的 Vulkan 层——它会经 `vk_icdGetPhysicalDeviceProcAddr` 直连真 ICD 绕过垫片导致段错误（显卡坑 16） |

> 若按最简形式改写后出现异常，把 `VK_ICD_FILENAMES` 与 `VK_LOADER_LAYERS_DISABLE` 加回去即可——
> 这两个是"去掉可能出问题、留着绝不出问题"的类型。

### 隔离性（重要）

改动全部收敛在 LD_PRELOAD，而 LD_PRELOAD 是**按游戏配置**的，因此不影响其他游戏：

```
系统驱动 libvulkan_gf.so（原版，未打补丁）
  ├── 随从大师 → 配了 LD_PRELOAD → 加载 hook → 重定位到 2~4GB → 7G100 渲染
  └── 刀塔霸业 → 不配 LD_PRELOAD → 完全不加载 hook → 原样使用驱动
```

已验证：刀塔霸业进程内 `osrelease_redirect` 加载次数为 **0**，存活 44 秒无段错误
（打全局驱动补丁时必崩）。

- 系统驱动校验：`md5sum /usr/lib/x86_64-linux-gnu/libvulkan_gf.so{,.orig}` 应一致。
- 补丁工具 `patch_gf_map32.py` 保留但**不再使用**，仅作调试对照。

### 剩余阻塞（与显卡无关）

游戏后端 `api.prod.mm21.minionmastersthegame.com` 的 `/v1/*` API 仍 **404**（`/health` 200 Healthy，
BetaDwarf 服务器端问题，游戏坑 7 已记录）。游戏用 7G100 渲染推进到登录队列后，因 404 自行退出
（无 crash report 的主动退出）。**7G100 硬件渲染链路本身已完全打通并验证**。
