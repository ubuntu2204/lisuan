# 随从大师（Minion Masters）Windows 启动日志分析报告

> 分析对象：本机 Windows 上最近一次完整启动的日志（2026-08-27 17:06–17:07）。
> 目的：项目目标是让砺算 7G100 在 **Ubuntu** 下跑起随从大师但卡住不知在哪步；
> 本 Windows 运行是一条**已知能进主菜单的“黄金参照”启动链**，用它来标定
> “正常启动应该走到哪、哪些报错可以忽略”，从而反推 Ubuntu 到底卡在哪一步。
>
> 数据来源（均在本机，未连 Ubuntu）：
> - 游戏日志：`C:\Users\godot\AppData\LocalLow\BetaDwarf ApS\Minion Masters\Player.log`
> - Steam 进程：`D:\Program Files (x86)\Steam\logs\console_log.txt`、`gameprocess_log.txt`
> - 设置：`...\Minion Masters\settings\2388710_settings.json`
> - 交叉文档：`docs/fix-minion-masters.md`（Ubuntu 侧的坑 1–8）

---

## 0. 结论速览（TL;DR）

1. **本次 Windows 启动是成功的**：引擎初始化 → GPU 识别（LX 7G100，D3D11）→ 场景/依赖注入
   → Steam 身份验证 → WebSocket 连上游戏服务器 → 拉到商店/配置 → **进入主菜单
   （`BattleModeViewModel` / `MainViewModelDesktop`）** → Vivox 语音登录成功 → 用户在设置里
   切换了画质预设和中文 → **用户主动退出**。整段约 71 秒。
2. **没有致命错误阻断启动**。日志里出现的几条 `InvalidOperationException`（404、贴图未下载完）
   和 `Uploading Crash Report` 都是**可容忍错误**，游戏照样推进到主菜单（见 §3 清单；
   每条的行号、出现次数、堆栈摘要与“后续存活证据”在 **附录 A** 逐条详录）。
3. 进程退出码 `0xC0000005`（访问违规）发生在**用户点退出之后的清理阶段**，属于退出路径
   崩溃，不影响“能否启动/能否进菜单”的结论（见 §4）。
4. **对 Ubuntu 的意义**：把下面 §2 的“里程碑链”当成检查表去比对 Ubuntu 的
   `Player.log`——Ubuntu 停在链条的哪一环，就对应 `fix-minion-masters.md` 里的哪个坑
   （见 §5 定位矩阵）。当前 Ubuntu 侧的历史阻塞点是登录队列 404（坑 3/坑 7）与
   Vivox/证书崩溃（坑 1/坑 2），**不是**本日志里这些可容忍错误。

---

## 1. 运行环境

| 项 | 值 |
| --- | --- |
| 平台 | Windows（本机），Steam 原生，**无 Proton / 无 Wine** |
| GPU | `LX 7G100`（ID=0x5006），Vendor ID `4c54`（“TL”），VRAM 3072 MB |
| 驱动 | `30.0.2260.552`（对应 Downloads 里的 LisuanTechDriver-v30.0.2260.552） |
| 图形 API | Direct3D 11.0（feature level 11.1），**硬件渲染**（非 llvmpipe） |
| 引擎 | Unity `2020.3.44f1` (7f159b6136da) |
| 游戏版本 | `41.0.10631.79001` |
| Steam AppID | `489520` |
| 安装路径 | `D:\Program Files (x86)\Steam\steamapps\common\Minion Masters` |
| 进程 | `MinionMasters.exe` PID 4296 + `UnityCrashHandler32.exe` PID 5040 |

> 说明：Windows 下砺算驱动直接走 D3D11 硬件渲染，`Renderer` 显示为 `LX 7G100`；
> 这与 Ubuntu 侧需要 Proton/pressure-vessel + gf Vulkan 白名单绕过（坑 8）完全不同路。
> 所以本日志验证的是**游戏逻辑链路**，可作为 Ubuntu 排障的“逻辑正确性基线”。

---

## 2. 启动里程碑时间线（“正常启动应到达”的检查表）

### 2.1 Steam 客户端侧（`console_log.txt` / `gameprocess_log.txt`）

| 时间 | 事件 |
| --- | --- |
| 17:06:33 | LaunchApp 依次走过 `CheckShaderDepotManifest → RunningInstallScript → SynchronizingCloud → SynchronizingStats → ShowInterstitials → SiteLicenseSeatCheckout → DelayLaunch → CreatingProcess` |
| 17:06:34 | `Game process added : AppID 489520 ... MinionMasters.exe`，ProcID 4296；同时挂上 `UnityCrashHandler32.exe --attach 4296`（PID 5040） |
| 17:06:34 | LaunchApp → `WaitingGameWindow` → `Completed` |
| 17:06:36 | GameOverlay 注入（`gameoverlayui64.exe`） |
| 17:06:42 | `Game process updated`（窗口已出现） |
| 17:07:45 | `Game process removed: AppID 489520`，退出码 `-1073741819`（= `0xC0000005`） |

**存活时长 ≈ 71 秒**，其中大部分是进菜单后的用户交互时间。

### 2.2 游戏进程侧（`Player.log`，按出现顺序）

| # | 里程碑（日志关键字） | 含义 | 状态 |
| --- | --- | --- | --- |
| 1 | `Initialize engine version: 2020.3.44f1` | 引擎拉起 | ✅ |
| 2 | `GfxDevice: creating device client` → `Direct3D ... Renderer: LX 7G100` | **GPU/渲染设备创建成功** | ✅ |
| 3 | `<RI> Input initialized` / `Using XInput` | 输入子系统 | ✅ |
| 4 | `BootstrapInstaller:Start()` / `[ScopeLogger] Addressable scene load` | Addressables + 首场景 | ✅ |
| 5 | `[DefaultModeInstaller] Application session id ...` | 会话装配（Zenject） | ✅ |
| 6 | `[LocalizationService] Initializing language to zh` | 本地化 | ✅ |
| 7 | `[BuildVersionToApiService] ... /v1/buildversion/41.0.10631.79001` → `Received api` | 版本 API 通 | ✅ |
| 8 | `[RemoteConfigurationService] Received remote configuration` | 拉到远端配置（api/wss/vivox） | ✅ |
| 9 | `[SteamService] Initialized.` | Steam 服务 | ✅ |
| 10 | `[BattleVersionService] Initialized? True` | 战斗逻辑版本 | ✅ |
| 11 | `[TransitionViewCommand] Pushing LoginMainViewModel` | 登录界面 | ✅ |
| 12 | `[LoginQueueService] Login queue entered with ticket 00000000-...` | 进登录队列（占位票） | ⚠️ 见 §3-1 |
| 13 | `[SteamAuthenticationService] Ticket 2` | **Steam 身份验证通过** | ✅ |
| 14 | `[WebSocketSharpTransportService] Connection opened` | **连上游戏服务器** | ✅ |
| 15 | `[SteamShopService] Update (48, 58000, JPY) ...` | 商店数据 | ✅ |
| 16 | `[ScopeLogger] Start session took 11335 ms` / `Login flow took 16212 ms` | 会话建立完成 | ✅ |
| 17 | `[BattleModeViewModel] Updating invites 0` | **进入主菜单** | ✅ |
| 18 | `[MetaTutorialStateService] loaded meta tutorial state` | 菜单状态加载 | ✅ |
| 19 | `[VivoxChatServiceAdapter] Initializing Vivox` → `Login started with token ...` → `[ChatService] OnLoginStateChanged: LoggedIn` | **Vivox 语音登录成功（原生，无需补丁）** | ✅ |
| 20 | `[TransitionViewCommand] Pushing MainViewModelDesktop` / `RichPresenceService Set to Online` | 桌面主视图 + 在线状态 | ✅ |
| 21 | `Pushing SettingsViewModel` → `Set Preset to 2` → `Switched language to zh` | 用户改设置 | ✅ |
| 22 | `[ApplicationQuitCommand] Quitting.` | **用户主动退出** | ✅ |

> 结论：**链条 1→22 全绿**。只要 Ubuntu 的 `Player.log` 能走到第 17/18 行（主菜单）
> 就算“能启动”；走不到，就按 §5 的矩阵看它停在哪一环。

---

## 3. 可容忍错误清单（出现但不阻断启动）

这些错误在**成功进菜单的本日志里同样出现**，因此排障时应当**忽略**，不要误当成卡点。

> **判定依据（实证）**：本次运行（2026-08-27 17:06，用户确认**成功进入游戏、没有卡住**）
> 走完了 §2.2 里程碑链 1→22 并正常交互后退出。由此得到一条硬规则：
> **凡是在“进入主菜单”之前出现、且其后日志仍继续推进的错误，均已被本次运行实证为
> 可容忍**——它们没有阻止启动。下表每条的“后续存活证据”见 **附录 A** 的逐条详细记录。

| # | 日志片段 | 触发点 | 影响 | 判定 |
| --- | --- | --- | --- | --- |
| 1 | `InvalidOperationException: Http error code was 404`（`LoginQueueService.Post`，伴随 `Uploading Crash Report`） | 进登录队列时 POST 后端 | 被 `UniTaskScheduler.PublishUnobservedTaskException` 捕获，游戏继续进菜单；但**真正的匹配/排队功能不可用**（服务端 `/v1/*` 404，见 `fix-minion-masters.md` 坑 3） | **可容忍**（不阻断启动，属后端问题） |
| 2 | `Curl error 35: Cert handshake failed. verify result: UNITYTLS_X509VERIFY_FATAL_ERROR` | 新闻/活动图片下载 | 仅新闻图片拉不下来 | 可容忍 |
| 3 | `InvalidOperationException: Texture has not yet finished downloading`（`DownloadSpriteFromUrlAsync`，伴随 `Uploading Crash Report`） | #2 的后果，取贴图时下载未完成 | 被异步异常通道吞掉，主流程继续 | 可容忍 |
| 4 | `ERROR: Shader Standard shader is not supported on this GPU` + 多条 `fallback shader ... not found` / `WARNING: Shader Unsupported` | 菜单场景编译着色器 | 仅画质/外观降级，不退出（对应坑 A3） | 可容忍 |
| 5 | `[LOCALIZATION] No text was found for id=fireball / BlinkBat / 骷髅 / Deal` | 本地化缺 key | 显示回退，不影响逻辑 | 可容忍 |
| 6 | `Multiple PlayerInputManagers in the game` | 输入管理器重复 | 告警，正常跑 | 可容忍 |
| 7 | `Exposed name does not exist: UIVolume`（`SoundMixerBehaviour:Inject`） | AudioMixer 暴露名缺失 | 音量参数取不到，回退默认 | 可容忍 |
| 8 | `Quest id 3000 does not exist and was ignored` | 任务表缺项 | 忽略该任务 | 可容忍 |
| 9 | `Adventure id=0/1/3/4 is deprecated. Skipping.` | 旧冒险模式弃用 | 信息性跳过 | 可容忍 |
| 10 | `[DataBindingBehaviour] ... Converter is null. Using DataBindingDefaultConverter as fallback.` | 数据绑定缺转换器 | 用默认转换器 | 可容忍 |
| 11 | `[AnimationCuePlayerObject] speedMultipliers does not have a value ... Using fallback 1.` | 动画速率表缺项 | 回退 1 | 可容忍 |
| 12 | `Switching audio transmission exclusively to text-only channel -- this is allowed but unusual.`（VivoxUnity） | Vivox 频道 | 信息性 | 可容忍 |
| 13 | `[SettingsService] File exists returned false ... Using defaults.` | 首次运行无设置文件 | 用默认设置 | 可容忍 |
| 14 | `Multiple UIParticleOverlayCamera in scene.` | UI 粒子相机重复 | 告警 | 可容忍 |
| 15 | `Unloading N unused Assets ...`（多处） | 资源卸载 | 正常内存管理 | 可容忍（噪音） |

**Steam 客户端侧的可容忍噪音**（`console_log.txt`，均不影响游戏启动）：

| 日志片段 | 判定 |
| --- | --- |
| `CAPIJobRequestUserStats - Server response failed 2` | 可容忍（坑 7 已确认与退出无时间关联） |
| `ChildProcessQuery: failed with code -2` / `CGpuTopology: failed to run gpu topology query` | 可容忍（Steam 启动自检噪音） |
| `Failed to init SteamVR because it isn't installed` | 可容忍 |
| `LogonFailure No Connection`（15:55–16:00 若干条） | 可容忍（客户端启动早期暂态，随后登录成功） |
| `Error: texture file 'public\steam_cloudsync' does not exist or is invalid` | 可容忍 |
| `Forced create of Win32Event but it already existed` | 可容忍 |

---

## 4. 退出阶段的错误（不是启动卡点）

用户点退出（`[ApplicationQuitCommand] Quitting.`）**之后**，清理/析构阶段抛出并最终以
访问违规收尾。这些只影响“退出是否干净”，不影响“能否启动/进菜单”。

| 日志片段 | 阶段 | 判定 |
| --- | --- | --- |
| `InvalidOperationException: Steamworks is not initialized`（`SteamAuthenticationService.Dispose` → `ZenjectException`） | 退出时 Dispose | 退出路径错误 |
| `InvalidOperationException: Cannot unregister Consumer 'BattleModeViewModel' when it still has loaded resources e.g. 'MayhemIcon'` | 退出时 Dispose | 退出路径错误 |
| `Curl error 42: Callback aborted`（×2） | 退出中断下载 | 退出噪音 |
| `Setting up 8 worker threads for Enlighten` | 退出时 Enlighten 收尾 | 正常 |
| **进程退出码 `0xC0000005`（ACCESS_VIOLATION）** | 退出清理后崩溃 | 退出路径崩溃，**非启动阻断**；`backtrace`/`Crashes` 目录为空（Backtrace 已 Disabled，未落盘） |

> 排障提示：**不要**把退出码 `0xC0000005` 误判为“游戏启动崩溃”。判断是否真的崩在启动，
> 要看 `Player.log` 是否到达 §2.2 的第 17/18 行（主菜单）。本次是到达后才由用户退出的。

---

## 5. 反推 Ubuntu：卡点定位矩阵

把 Ubuntu 的 `Player.log` 与 §2.2 里程碑逐行比对，停在哪一环就对应哪个已知坑
（详见 `docs/fix-minion-masters.md`）：

| Ubuntu 的 `Player.log` 停在 | 症状 | 对应坑 | 处理 |
| --- | --- | --- | --- |
| 第 2 行前（无 `Renderer:`，或停在 `GfxDevice: creating device client`） | 渲染设备创建失败/卡死 | 坑 A1 / 坑 4 | `PROTON_USE_WINED3D=1`，或让 pressure-vessel 注入 gf Vulkan；别手塞 `VK_ICD_FILENAMES` |
| 第 2 行有 `Renderer:` 但随后黑屏、控制台刷 `NotSupportedException` | IL2CPP runtime 被破坏 | 坑 2 | 确认用的是**禁用证书 patch**的插件版本 |
| 崩在 `VivoxChatServiceAdapter.LoginAsync`（无 `PATCHED` 日志） | Vivox 语音登录崩溃 | 坑 1 | 确认 `MMSkipVivox.dll`/`MMPatch.dll` 已加载、三条 patch 生效 |
| 出现 `Texture has not yet finished downloading` 且无 patch | 证书/贴图异常 | 坑 1c | 用 `InternalGetTexture` 返回 null 的 patch |
| 走到第 17/18 行（`BattleModeViewModel`/`MetaTutorialStateService`）**之后** 40–90 秒进程被移除 | 进菜单后被踢 | 坑 3 / 坑 7（登录队列 `/v1/*` 404、Azure IP 封锁） | 后端问题，本地无解；等厂商恢复或抓新 API |
| `steamid=0` / `e0434352` | Steam 未登录 | 坑 A2 | 重启 Steam 客户端触发自动登录；首次需图形界面登录一次 |

**本 Windows 日志给出的“应到达”基线**：引擎初始化 → `Renderer: LX 7G100` →
`SteamAuthenticationService Ticket` → `Connection opened` → `BattleModeViewModel` →
`MetaTutorialStateService loaded`。Ubuntu 侧只要补齐对应坑，就应能复现这条链。

---

## 6. 建议

1. **以 §2.2 里程碑链为检查表**比对 Ubuntu 的 `Player.log`（注意先看 `ls -la` 的 mtime，
   别拿轮转的 `Player-prev.log` 下结论）。
2. 排障时先用 `grep -vE` 过滤 §3 的可容忍噪音，聚焦**最早出现的真正异常**。
3. 本次确认登录队列 404 与贴图/证书错误**不会阻止进菜单**，因此若 Ubuntu 连菜单都进不去，
   优先怀疑渲染（坑 A1/坑 4）、Vivox（坑 1）、证书 patch 回归（坑 2），而非这些可容忍错误。
4. 若 Ubuntu 已能进菜单但随后被移除，属坑 3/坑 7（后端 404），需等厂商或抓包确认新 API 路径。
5. 退出码 `0xC0000005` 属退出路径问题，当前可忽略；如需根治可在退出前先释放
   `BattleModeViewModel` 资源并保证 Steamworks 释放顺序。

---

## 附录 A：可容忍错误逐条详细记录

> 基准日志：`Player.log`（`C:\Users\godot\AppData\LocalLow\BetaDwarf ApS\Minion Masters\Player.log`，
> 2026-08-27 17:07，75 KB）；Steam 侧：`D:\Program Files (x86)\Steam\logs\console_log.txt`。
> 行号为该次日志内的实际行号。**“后续存活证据”= 该错误出现之后日志仍继续推进到的里程碑**，
> 这是它被判定为可容忍的直接实证（本次运行最终成功进主菜单并交互）。

### A1. `Http error code was 404` —— 登录队列 POST 失败（Player.log:218–247）

- **原文**：`Uploading Crash Report` + `InvalidOperationException: Http error code was 404.`
- **次数**：1 次
- **堆栈摘要**：`LoginQueueService+<Post>d__15` → `UniTaskCompletionSourceCore.TrySetException`
  → `LoginQueueService+<EnterQueueAndWaitForTurn>d__13` → `DefaultModeInstaller..ctor`
  → `UniTaskScheduler.PublishUnobservedTaskException` → `ExceptionHolder:Finalize()`。
  即这是一个 **fire-and-forget 异步任务的未观察异常**，只被日志通道捕获，未向主流程抛出。
- **触发原因**：向游戏后端 `https://api.prod.mm21.minionmastersthegame.com/v1/loginQueue`
  POST 时服务端返回 404（后端 API 路径下线/迁移）。本报告生成时实测复核：`/health` 200、
  `/v1/buildversion` 200、`/v1/loginQueue` **404**。
- **后续存活证据**：行 249 起 `[ScopeLogger] Update value repository` → 行 313
  `Process profile data took 100 ms` → …… → 行 825 `[BattleModeViewModel] Updating invites 0`
  （进主菜单）。**404 之后游戏完整走完了登录、装配并进菜单。**
- **判定**：**可容忍（不阻断启动）**。但它使**匹配/排队功能不可用**——这是目前唯一影响
  实际游玩的功能性阻塞（服务端问题，本地无解；`fix-minion-masters.md` 坑 3/坑 7）。

### A2. `Curl error 35` —— 新闻图片证书握手失败（Player.log:826）

- **原文**：`Curl error 35: Cert handshake failed. verify result: UNITYTLS_X509VERIFY_FATAL_ERROR. error state: 7`
- **次数**：1 次
- **触发原因**：拉取新闻/活动横幅图片时，UnityWebRequest 的 TLS 证书校验失败
  （证书链不被信任或过期）。只影响这一条下载。
- **后续存活证据**：行 827 起继续 `[DataBindingBehaviour]` 注入 → 行 853 `Bindings took 3822 ms`
  → 行 916 `MetaTutorialStateService loaded`（菜单状态加载完成）。
- **判定**：**可容忍**。新闻图缺失不影响任何玩法逻辑。

### A3. `Texture has not yet finished downloading` —— 贴图未下载完（Player.log:860–901）

- **原文**：`Uploading Crash Report` + `InvalidOperationException: Texture has not yet finished downloading`
- **次数**：1 次
- **堆栈摘要**：`DownloadHandlerTexture.InternalGetTexture` →
  `NewsItemViewModel+<DownloadSpriteFromUrlAsync>d__56.MoveNext` →
  `SimpleCoroutineAwaiter.Complete(T, Exception)` → `UniTaskCompletionSourceCore.TrySetException`
  → `PublishUnobservedTaskException`。这是 **A2 的直接后果**：证书失败导致下载未完成，
  随后取贴图抛出异常，仍走未观察异常通道被吞掉。
- **后续存活证据**：行 903 起 `[LOCALIZATION] No text ... id=Deal` → 行 916
  `MetaTutorialStateService loaded` → 行 930+ Vivox 登录 → 行 983 `MainViewModelDesktop`。
- **判定**：**可容忍**。（注：在 Ubuntu/Proton 环境此异常曾是坑 1c 的崩溃源，需用
  `InternalGetTexture` 返回 null 的 patch；Windows 原生下它只是被日志吞掉，不致命。）

### A4. Shader 不受支持/回退缺失（Player.log:107、526–535）

- **原文（多条）**：
  - `Shader 'Minion Masters/Lit': fallback shader 'Minion Masters/Simple Lit' not found`（107）
  - `Shader 'VFX_Shader_Additive': fallback shader 'False' not found`（526）
  - `Shader 'Standard': fallback shader 'StandardMobileFallback' not found`（527）
  - `WARNING: Shader Unsupported: 'Standard' - All subshaders removed`（528、532）
  - `ERROR: Shader Standard shader is not supported on this GPU (none of subshaders/fallbacks are suitable)`（531）
  - `Shader 'VFX_Shader': fallback shader 'False' not found`（535）
- **次数**：`Standard not supported` 1 次；`Unsupported/All subshaders removed` 2 次；fallback 缺失 4 处
- **触发原因**：砺算 GPU 驱动不支持 Unity Standard shader 的 subshader，且声明的
  fallback 也不存在，Unity 移除这些 subshader。仅影响用到这些着色器的材质外观。
- **后续存活证据**：行 536 起场景继续加载 → 行 542 `Addressable scene load took 1731 ms`
  → 行 825 进主菜单。着色器告警之后菜单照常渲染。
- **判定**：**可容忍（画质降级，非致命）**。与 `fix-minion-masters.md` 附录 A3 记录一致。

### A5. 本地化缺 key（Player.log:550、621、692、903）

- **原文**：`[LocalizationService] [LOCALIZATION] No text was found for the id=fireball / BlinkBat / 骷髅 / Deal`
- **次数**：4 次（各 1 次）
- **触发原因**：卡牌/按钮的本地化键在当前语言包里不存在，回退显示原键名。
- **后续存活证据**：每条之后卡片集合（`CardCollectionViewModel`）继续构建，最终进菜单。
- **判定**：**可容忍（显示回退）**，不影响逻辑。

### A6. 输入/场景结构告警（Player.log:41、536）

- **原文**：`Multiple PlayerInputManagers in the game. There should only be one PlayerInputManager`（41）；
  `Multiple UIParticleOverlayCamera in scene.`（536）
- **次数**：各 1 次
- **触发原因**：场景中存在重复的输入管理器/粒子叠加相机，Unity 提示但不中断。
- **后续存活证据**：行 47 起 `[ScopeLogger] Addressable scene load took 533 ms` 继续；行 542 起继续。
- **判定**：**可容忍（告警）**。

### A7. 音频暴露名缺失（Player.log:117）

- **原文**：`Exposed name does not exist: UIVolume`（`AudioMixer:GetFloat` ← `SoundMixerBehaviour:Inject`）
- **次数**：1 次
- **触发原因**：AudioMixer 中未暴露名为 `UIVolume` 的参数，Zenject 注入时取不到，回退默认值。
- **后续存活证据**：行 139 起 `Bindings took 47 ms` → `Kernel initialization` 继续。
- **判定**：**可容忍（音量参数回退默认）**。

### A8. 任务/冒险数据告警（Player.log:263、498–502）

- **原文**：`[QuestManager] Quest id 3000 does not exist and was ignored.`（263）；
  `[AdventureContext] No wins for adventure. id=2`（498）；
  `[AdventureContext] Adventure id=0/1/3/4 is deprecated. Skipping.`（499–502）
- **次数**：Quest 1 次；Adventure deprecated 4 次；No wins 1 次
- **触发原因**：服务端下发的任务/冒险条目在客户端不存在或已弃用，按设计忽略/跳过。
- **后续存活证据**：行 313 `Process profile data took 100 ms`；行 503
  `Session parallel took 2329 ms`，会话继续。
- **判定**：**可容忍（数据性跳过）**。

### A9. 设置文件不存在用默认（Player.log:314–315、494）

- **原文**：`[SettingsService] Loading settings+2388710_settings.json...`
  `File exists returned false ... Using defaults.`（314–315）；
  `[SettingsService] Loaded settings+2388710_settings.json? False.`（494）
- **次数**：1 组
- **触发原因**：首次运行无个人设置文件，使用默认画质/键位。（本次运行结束时已生成
  `settings\2388710_settings.json`。）
- **后续存活证据**：行 365 起 `[QualityConfigurationPresetService] Setting Preset Medium` 继续。
- **判定**：**可容忍（首启正常行为）**。

### A10. 数据绑定/动画回退（Player.log:827、967）

- **原文**：`[DataBindingBehaviour] ...HardYellowButton.OnInitialize Converter is null. Using DataBindingDefaultConverter as fallback.`（827）；
  `[AnimationCuePlayerObject] speedMultipliers does not have a value for animationCue BrotherHero_Relaxed ... Using fallback 1.`（967）
- **次数**：各 1 次
- **触发原因**：绑定缺转换器、动画速率表缺项，均回退默认。
- **后续存活证据**：行 853 `Bindings took 3822 ms` 继续；行 980+ 商店/语音继续。
- **判定**：**可容忍（回退默认）**。

### A11. Vivox 文本频道提示（Player.log:993）

- **原文**：`Switching audio transmission exclusively to text-only channel -- this is allowed but unusual.`
- **次数**：1 次
- **触发原因**：Vivox 加入频道时仅订阅文本（未开语音发送），SDK 信息性提示。
- **后续存活证据**：行 991 `[ChatService] OnLoginStateChanged: LoggedIn`（**Vivox 登录成功**），
  其后菜单照常。
- **判定**：**可容忍（信息性）**。

### A12. 日志噪音/非错误（Player.log:116、137、206、919 等）

- **原文**：`[BacktraceAdapterService] Backtrace is Disabled`（116，崩溃上报服务关闭，故
  `Crashes`/`backtrace` 目录为空）；`[ line 13561136]`（137，堆栈转储残留的噪音行）；
  `[LoginQueueService] Login queue entered with ticket 00000000-0000-0000-0000-000000000000`（206，
  占位零票，随后即 A1 的 404）；`Unloading N unused Assets ...`（多处，正常内存管理）。
- **判定**：**非错误/噪音**，排障时直接忽略。

### A13. Steam 客户端侧噪音（console_log.txt，均不影响游戏进程）

- **原文与次数**：
  - `CAPIJobRequestUserStats - Server response failed 2` ×4（16:05 三条 + 17:06:37 一条）
  - `LogonFailure No Connection` ×7（15:55–16:00，客户端启动早期暂态，随后登录成功）
  - `ChildProcessQuery: failed with code -2` ×1、`CGpuTopology: failed to run gpu topology query` ×1（启动自检）
  - `Failed to init SteamVR because it isn't installed` ×1
  - `Error: texture file 'public\steam_cloudsync' does not exist or is invalid` ×1
  - `Forced create of Win32Event but it already existed` ×2
- **判定**：**全部可容忍**。`CAPIJobRequestUserStats` 与游戏退出无时间关联
  （`fix-minion-masters.md` 坑 7 已实证）；其余为客户端启动噪音。

---

## 附录 B：退出阶段错误（非启动卡点，单列以免误判）

| 日志片段 | 位置 | 阶段 | 判定 |
| --- | --- | --- | --- |
| `InvalidOperationException: Steamworks is not initialized`（→ `ZenjectException`） | Player.log:1077–1085 | 用户退出后 `SteamAuthenticationService.Dispose` | 退出路径错误 |
| `InvalidOperationException: Cannot unregister Consumer 'BattleModeViewModel' ... 'MayhemIcon'` | Player.log:1087–1098 | 退出后资源释放顺序问题 | 退出路径错误 |
| `Curl error 42: Callback aborted` ×2 | Player.log:1102–1103 | 退出中断未完成下载 | 退出噪音 |
| 进程退出码 `0xC0000005`（ACCESS_VIOLATION） | gameprocess_log.txt:10 | 退出清理后崩溃 | **非启动阻断** |

> 判别口径：这些错误都出现在 `[ApplicationQuitCommand] Quitting.`（Player.log:1066）**之后**。
> 只要日志在退出命令前已到达主菜单（§2.2 第 17/18 行），就**不构成启动卡点**。

---

*报告生成：2026-08-27。分析未连接 Ubuntu，全部基于本机 Windows 的日志与仓库文档交叉验证。*
