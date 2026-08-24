# 随从大师（Minion Masters）修复

> 适用机型：**砺算 7G100 GPU + Ubuntu 24.04 + Proton 8.0（Steam）**。
> 现象：Steam 里启动 Minion Masters 先后崩在 Vivox 登录 / 黑屏（Zenject 异常循环）；
> 修复后已能过 Vivox、Steam 验证、连上游戏服务器并**进入主菜单（BattleModeViewModel）**。
> 修复脚本：`lisuan-7g100-ubuntu-fix/scripts/fix-minion-masters.sh`（Launcher 注入）；
> 真正的崩溃修复是 BepInEx 插件 `MMSkipVivox.dll`（源码 `/home/ubuntu/mm_patch/plugin/Patcher.cs`）。

---

## TL;DR

随从大师是 Unity / IL2CPP 游戏，坑按排除顺序共四层：

1. **崩在 Vivox 语音登录**（已修复）：游戏在 `VivoxChatServiceAdapter.LoginAsync` 里 fire-and-forget
   调用未处理异常的 async（`UniTask` 的 `PublishUnobservedTaskException`），导致进程崩在 IL2CPP
   机器码（GameAssembly.dll 内），无法按托管名 patch。修复 = BepInEx 6 插件，运行时把该函数的
   prologue 改掉，让它立即返回（见坑 1）。
2. **黑屏 + BepInEx 控制台无限刷 `NotSupportedException`**（已修复）：证书 patch
   （`CertificateHandler.ValidateCertificate`）反而破坏了 IL2CPP runtime 状态，引发 Zenject
   `ProjectContext` 重复创建 assert + `BootstrapInstaller.Start()` NullReference + 异常提取栈
   死循环（见坑 2）。禁用证书 patch、改 patch `DownloadHandlerTexture.InternalGetTexture` 后解决。
3. **图片下载证书错误导致的致命 Texture 异常**（已修复）：`Curl error 60` →
   `Texture has not yet finished downloading` 异常沿 UniTask `TrySetException` 链传播。
   修复 = 把 `InternalGetTexture` patch 成返回 null（见坑 1 的 patch c）。
4. **联网可达性**（外部环境，非配置可解）：游戏后端 `api.prod.mm21.minionmastersthegame.com`
   **所有 `/v1/*` API endpoint 返回 404**（`/health` 却返回 200 "Healthy"）→ `LoginQueueService.Post`
   抛 `Http error code was 404`，登录队列进不去；另有 Azure IP `140.228.24.145:443` 封锁（见坑 3）。

当前状态（**2026-08-25 00:38 排查后**）：游戏能完整走完 IL2CPP 加载 → Steam 身份验证（拿到
Ticket、缓存 SteamID `76561198811581867`）→ WebSocket 连游戏服务器（`Connection opened`）→
拉到 SteamShop 商品列表 → **进入主菜单 `BattleModeViewModel`**（PracticeDifficultyView、
MetaTutorialStateService 均加载）。但游戏进程会在启动后 **~40–90 秒被移除**（Steam console_log
`Game process removed: AppID 489520`），时间点在登录队列 404 之后——剩余阻塞见坑 3 / 坑 7。

```bash
sudo ./scripts/fix-minion-masters.sh            # 注入 Launcher（gate 逻辑）→ 宿主启动 mm.sh
sudo ./scripts/fix-minion-masters.sh --uninstall
```

`mm.sh` 才是宿主启动器：先 kill 残留（Steam 进程残留），再 valve-run → 宿主 `steam -applaunch 489520`。

---

## 坑 1：Vivox 登录崩溃 + Texture 异常（已修复，插件三件套）

### 根因
游戏崩在 IL2CPP 机器码 `VivoxChatServiceAdapter.LoginAsync`（位于 GameAssembly.dll 内）。
这是 fire-and-forget async 未处理异常（UniTask 的 `PublishUnobservedTaskException`）。
该类型/方法**不在** Il2CppInterop 生成的 interop 程序集里，所以常规的 Harmony 按托管名 patch 行不通。

### 修复方案（已验证成功，v1.1.1 = 当前部署 15360 bytes）
BepInEx 6 + 自定义插件 `MMSkipVivox.dll`（源码 `/home/ubuntu/mm_patch/plugin/Patcher.cs`，
编译目标 net6.0，引用 tools/bepinex 的 BepInEx.Core / BepInEx.Unity.IL2CPP / Il2CppInterop.Runtime）。

插件在 `Load()` 中共做**三个 patch**（地址为 32 位 wine 运行时绝对 VA，实测跨启动稳定）：

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

patch a 的 trampoline 演进（**重要教训**）：

- **v1.0.0（旧，有隐患）**：prologue 直接覆盖 `31 C0 31 D2 C3`（xor eax,eax; xor edx,edx; ret）
  返回**全零 struct**。游戏 **8/23 更新前**能跑；更新后 `ChatService.LoginAsync` 改为 `await`
  Vivox 返回，全零 UniTask 的 `GetAwaiter()` 访问 `null` runner → `NullReferenceException` →
  退出（e0434352）。
- **v1.1.0 尝试**：优先 `get_CompletedTask` getter 跳转 / 运行时读静态字段地址构造 trampoline；
  本机 IL2CPP 没生成该 getter，**回退 xor 方案**（`31 C0 31 D2 C3` 的远跳版）。
  实测 8/24 版游戏下全零 UniTask 已不再致命（配合 patch b/c），故当前即为 xor 回退版在跑。

patch c 的异常传播链（为什么必须 patch 这里）：`Curl error 60`（证书校验失败）→
`DownloadHandlerTexture.InternalGetTexture()` 抛 `InvalidOperationException: Texture has not
yet finished downloading` → `NewsItemViewModel+<DownloadSpriteFromUrlAsync>d__56.MoveNext()` →
`SimpleCoroutineAwaiter.Complete(T, Exception)` → `UniTaskCompletionSourceCore.TrySetException` →
`UniTaskScheduler.PublishUnobservedTaskException` →（fire-and-forget 的
`HandleSafeFireAndForget`）→ Unity `Uploading Crash Report`。只 patch MoveNext 拦不住
awaiter 链上的二次传播，**在异常源头（InternalGetTexture）返回 null 才彻底**。

### 部署现状
| 内容 | 位置 |
| --- | --- |
| `MMSkipVivox.dll`（15360 B） | 游戏 `BepInEx/plugins/`（备份 `.bak-1002` = v1.0.0 8704 B） |
| doorstop 注入 | winhttp.dll（已注入） |
| `winhttp=n,b` 覆盖 | 游戏 `user.reg` |
| dxgi / d3d11 | 已从游戏目录移除（避免 pressure-vessel 路径冲突） |

验证：`BepInEx/LogOutput.log` 出现 3 条 `patched/ PATCHED`；Player.log 出现
`BattleModeViewModel`、`MetaTutorialStateService loaded`；**不再出现**
`Texture has not yet finished downloading`。

> 已排除的弯路：禁用 Vivox DLL 反而让游戏更早崩在登录队列；Vivox 必须"跳过"而非"禁用"。

---

## 坑 2：证书 patch 回归——黑屏 + NotSupportedException 无限刷屏（已修复，重大教训）

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
坑 1 的 patch b/c（跳过下载 + 异常源头返回 null）。经验：**patch Unity 内部 native 方法前先
确认它不是共享 helper**——`0x75D98C0` 原字节就是 `32 C0 C3 CC CC`（一个已被优化成
"return false" 的共享 stub），patch 它会波及所有调用方。

**排障技巧**：看到 NotSupportedException 刷屏时，用
`grep -vE "NotSupportedException|Couldn't extract|Derived classes|at System.Reflection|..."` 过滤
噪音，才能看到最初 20 行里的**真正异常**（Zenject + BootstrapInstaller）。

---

## 坑 3：联网可达性（外部环境，非配置可解）

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

## 坑 4：Proton 版本

必须用 **Proton 8.0**（经 steamcmd 账号登录下载 AppID 218010，安装位置
`~/.local/share/Steam/steamapps/common/Proton 8.0`）。Proton Experimental / wine 11.0 在
7.0.0-30 内核下有 32 位地址空间回归（游戏崩在 `0x01160000`），Proton 8.0 绕过。

---

## 坑 5：user_settings.py 注入无效（教训，不要再踩）

> Steam 平台层的通用说明（容器白名单、格式陷阱）见 [`docs/steam-on-lisuan.md` §7](./steam-on-lisuan.md)。

`~/.local/share/Steam/steamapps/common/Proton 8.0/user_settings.py` 的正确格式是**顶层键值对**：

```python
user_settings = {
    "VK_ICD_FILENAMES": "/usr/share/vulkan/icd.d/gf_icd.x86_64.json",
    "WINEDLLOVERRIDES": "winhttp=n,b",
}
```

**两个坑**：
- ❌ 不能用嵌套 `session_environment`：嵌套 dict 会被 proton 当成 env 值 → `TypeError: expected str... not dict` → 游戏根本不启动。
- ⚠️ 即便格式正确，`user_settings.py` 注入的自定义变量（VK_ICD_FILENAMES / WINEDLLOVERRIDES）**不会进入
  pressure-vessel 容器环境**（pressure-vessel 只传白名单 env）。所以 user_settings.py 不是注入显卡驱动的可靠途径。

当前部署：**已移除自定义 user_settings.py**（保持顶层默认），游戏用 llvmpipe 软件渲染。

---

## 坑 6：Steam "游戏运行中" 状态卡死

> 平台层通用说明见 [`docs/steam-on-lisuan.md` §5](./steam-on-lisuan.md)。

每次 `pkill` 强杀游戏后，Steam 会卡在"游戏运行中"状态，后续 `steam -applaunch 489520` 被静默忽略，
游戏起不来。必须 `steam -shutdown` 彻底重启 Steam 才能再次启动。调试时若发现 applaunch 拉不起游戏，
先怀疑这个，而不是游戏本身。

---

## 坑 7：游戏进程启动后 ~40–90 秒被移除（当前未解）

**现象**：游戏已能进入主菜单（`BattleModeViewModel` 出现在 Player.log），但 Steam
console_log 在启动后 38–90 秒不等出现 `Game process removed: AppID 489520`，进程消失。
Player.log 末尾停在 `[ChatService] Starting LoginAsync`（patch 跳过后的 Vivox 调用），
无 crash dump、无 .NET 致命异常。

**已排除**：
- 不是 `CAPIJobRequestUserStats - Server response failed 2`——该错误从 8/24 13:34 起持续出现
  （23 次），与游戏退出时刻无固定时间关联，且成功进入主菜单的运行里同样存在；
- 不是 Zenject assert（良性，见坑 2）；
- 不是 Texture 异常（已 patch，见坑 1c）。

**怀疑方向**：登录队列 404（坑 3.1）之后游戏自身超时退出，`DefaultModeInstaller` 构造里
`LoginQueueService.EnterQueueAndWaitForTurn` 的 404 异常可能最终让流程走不下去。需等后端
恢复后复测，或进一步 hook `LoginQueueService` 观察退出路径。

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
# 4) 渲染后端为 llvmpipe（软件，已知限制）
```

若启动后崩在 Vivox（无 PATCHED 日志）：确认 `MMSkipVivox.dll` 在 `BepInEx/plugins/` 且 winhttp 覆盖生效。
若黑屏 + BepInEx 控制台刷 NotSupportedException：确认插件是**禁用证书 patch 的版本**（见坑 2）。
若进主菜单后 ~1 分钟进程消失：属坑 7（当前未解，先确认坑 3 的 API 404 状态）。

**Player.log 排障技巧**：Unity 每次启动会把上次的 `Player.log` 轮转为 `Player-prev.log`；看
`ls -la` 的 mtime 区分本次/上次运行，别拿旧日志下结论。日志被 NotSupportedException 刷屏时先
用 grep -vE 过滤噪音（见坑 2 的排障技巧）。

---

## 附录：渲染层与 Steam 登录层额外坑（与 Vivox 无关）

> 这些坑在排障时容易和 Vivox 崩溃混淆，单独列出。细节亦见 README 第七章 7.2 / 7.3 / 7.4。

### A1. `d3d11: failed to create factory`（渲染设备创建卡死）

DXVK 创建 D3D11 factory 失败时，游戏会卡在 `GfxDevice: creating device client` 然后退出
（无 Player.log 或停在 creating device client）。修法：

- **优先 `PROTON_USE_WINED3D=1`**：wined3d 走软件渲染（llvmpipe），能稳定过渲染关，
  游戏可到 `Direct3D 11 llvmpipe` + 推 `LoginMainViewModel`。
- 或在 Steam 默认启动下，让 pressure-vessel 自动注入 gf Vulkan（`VK_ICD_FILENAMES`
  已含 `gf_icd.x86_64.json`），DXVK 才能拿到有效 Vulkan 后端。

**注意**：不要手动在启动 wrapper（`run-mm.sh` 之类）里硬塞 `VK_ICD_FILENAMES` /
`WINEDEBUG` 等 env——会破坏 Steam 注入的完整 env，反而让渲染卡死。清空 LaunchOptions、
恢复 Steam 默认启动后通常就正常。

### A2. 无头环境 Steam 客户端登录（更新 2026-08-25：自动登录可自愈）

> Steam 平台层完整说明见 [`docs/steam-on-lisuan.md` §4](./steam-on-lisuan.md)。

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
