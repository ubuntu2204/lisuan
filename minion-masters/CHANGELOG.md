# minion-masters 变更记录

> 从仓库根 `CHANGELOG.md` 拆分而来（2026-11-19）。按时间倒序；历史条目保留当时的命令与路径。

## 2026.09.06 — 随从大师三份文档合并为「一篇通」

原 `MinionMasters-攻坚全记录.md`（研发日志，741 行）与 `MinionMasters-成功运行
报告.md`（llvmpipe 时代实测快照，516 行）内容去重后并入 `fix-minion-masters.md`
（保留文件名，`install.py` 菜单 14 引用、deb 打包、README 链接均无需改动）：

- 新增章：环境与架构矛盾（含 Wayland 必须经 Steam 启动的约束）、启动流程与健康
  判读（阶段时间线 + **5 类可容忍错误** + 健康/异常速查三命令）、排障速查表
  （症状 → 根因 → 处置）、历史方案与已废弃路线（9 条，含已排除嫌疑清单）、
  产出物清单、文档沿革；
- 头部标注合并说明与阅读顺序建议；`gf-compat/README.md`、README 两处旧链接同步
  指向合并后的文档；
- 两份旧文档从仓库删除（git 历史保留可查）。

---

## 2026.09.06 — 随从大师加固目标自动跟随 Steam 中的实际选择

### 背景

本次"玩不了"复发的间接诱因：裸跑修复脚本默认加固 Proton - Experimental，
而游戏实际用 Steam 里选择的 Proton 11.0——加固与运行对象脱节，且
CompatToolMapping 被打回 experimental（覆盖用户选择）。

### 变更

`fix-minion-masters-7g100-hw.sh` 的加固目标解析改为三级优先：

1. `MM_PROTON_DIR` 显式指定（沿用，且不改写 CompatToolMapping）；
2. **自动跟随**：读 `config.vdf` 的 CompatToolMapping 489520，把 Steam 里
   游戏当前选择的 Proton（如 `proton_11` → `Proton 11.0`）解析为目录并
   加固它——**裸跑 `bash fix-...sh` 即可加固正确对象**，不再依赖记得带
   环境变量；
3. 兜底默认 Proton - Experimental（无记录/目录不存在时）。

`--status` 新增"加固目标"行，可直接核对加固对象与 Steam 选择是否一致；
`--uninstall` 亦作用于解析后的目标（还原正确的 Proton）。

### 验证

- 裸跑实测：自动检测 `proton_11 → Proton 11.0` 并输出加固目标；
- `MM_PROTON_DIR=...Proton - Experimental` 显式覆盖仍优先；
- 语法、`--status`、测试套件 7/7 通过。

---

## 2026.08.31 — Proton 更新致加固失效；改用 Proton 11.0 稳定版

### 症状

Steam 自动更新 Proton Experimental 至 `experimental-11.0-20260826` 后，
《Minion Masters》无法启动：

```
Failed to initialize player
Details: Failed to initialize graphics ... InitializeEngineGraphics failed
```

### 根因

Proton **Experimental**（版本 `experimental-11.0-20260826`。注意它虽含 "11.0"
字样，但与独立安装的稳定版《Proton 11.0》**不是同一个东西**）将库目录由
`files/lib64/` 改为 `files/lib/`，三处加固**静默失效**：

| 加固项 | 失效表现 |
|---|---|
| `libSC.so` 软链 | 悬空（仍指向已删除的旧路径）→ gf `vkCreateDevice` 失败 |
| DXVK 降级 | 还原为新版（要求 `VK_KHR_load_store_op_none`，gf 22.0.4 不支持）→ DXVK 跳过 7G100 |
| hook 部署路径 | 指向失效 |

`libSC.so` 是 gf 驱动的着色器编译器伴侣库，悬空是本例的直接致命原因。

### 修复

重跑 `minion-masters/fix-minion-masters-7g100-hw.sh`（幂等，已适配 `files/lib/` 新路径）。
Steam 运行时改 vdf 会被覆盖，需**退出 Steam 后执行，跑完重启**。

验证结果：`Renderer: LISUAN 7G100`，D3D11 设备创建失败 **0 次**（修复前每次运行多次失败），
低 4GB 地址峰值 2.92 GiB / 4 GiB（72.9%）。

### 工程改进

- 新增 `minion-masters/gf-compat/variants/` 与 `build.sh`：hook 实验变体
  （`baseline-64k` / `norecycle` / `main`）纳入版本管理并提供可复现构建，
  不再在 `/tmp` 下临时编译——`/tmp` 会被清理、不进 git、构建不可复现。
- `MinionMasters-成功运行报告.md` 第 5 条原为**软件渲染时期**的描述
  （称 libSC「不需要」、DXVK 降级「无影响」），已按硬件渲染现状重写，
  并补入 `experimental-11.0` 的目录变化点与三步诊断命令；同时标注该条
  **仅适用于硬件渲染**（llvmpipe 软件渲染不涉及这两项）。
- **删除 `minion-masters/mm-patch/`**（`git rm`，历史保留）。它是补丁开发的历史
  工作目录，随 BepInEx 方案退役而整体失效：

  | 子目录 | 失效原因 |
  |---|---|
  | `plugin/`（`MMSkipVivox`） | 从未运行过——游戏日志中 `BepInEx` **0 命中** |
  | `doorstop/` | MMPatch 的**旧版副本**（08-26），Vivox patch 仍启用，误部署会卡登录 |
  | `apicheck/` `metalib/` `typelist/` | 一次性诊断工具，目标 DLL 已不在位 |

  连带清理 231MB 第三方依赖（`tools/` 97M + `backup/` 134M，原被 `.gitignore`
  排除、**不在 git 内**）。`minion-masters/` 体积由约 295MB 降至 **64MB**。

  删前已把关键知识迁至 `assets-mm-gf-vulkan/README.md`（新建）与 `MMPatch.cs`
  头部：① **`MMPatch.cs` 以 `assets-mm-gf-vulkan/` 那份为唯一权威**；
  ② **Vivox patch 必须停用** —— 日志出现 `PATCHED 2/2 Vivox async` 即中招，
  症状是卡在登录，与显卡故障相似，极易误判。

### 改用 Proton 11.0 稳定版（已验证，推荐）

Experimental 更新频繁，每次更新都会打断加固。已验证稳定版《Proton 11.0》
（`proton-11.0-2`）同样可跑通 7G100 硬件渲染，且更新频率低得多，更省心。

用法：在 Steam 里为游戏手动选择《Proton 11.0》，再对该目录加固：

```bash
cd ~/project/lisuan/minion-masters
MM_PROTON_DIR="$HOME/.local/share/Steam/steamapps/common/Proton 11.0" \
  bash ./fix-minion-masters-7g100-hw.sh
```

脚本相应改动：

- 新增 `MM_PROTON_DIR`：指定要加固的 Proton 目录，默认仍是 Experimental
- 指定该变量时**跳过 `config_steam`** —— 该函数会硬写 `proton_experimental`，
  不跳过就会把用户在 Steam 界面里的手动选择打回去
- 不指定时行为与原先完全一致（兼容既有用法）

另注：仓库内的 `Proton - MM GF`（自定义副本）**已失效** —— 其 `libSC.so` 指向
容器内不可见的 `/run/host/usr/lib`，建议删除，或用 `MM_PROTON_DIR` 指向它重跑修复。

### 里程碑：完整对战验证通过（可玩性闭环）

2026-08-31 在 Proton 11.0 + 7G100 硬件渲染下**完整打完一局对战并获胜**，
赛后奖励正常结算。`Player.log` 证据：

```
[BattleNetworkStateActive] Game ended with Completed
[GameUnitService] Not removing winner master 'WitchQueen' to let win animation play
[PostMatchViewModel] Displaying Battlepass reward card
```

匹配 → 对战 → 判定胜负 → 结算奖励，全链路通过。两项随之重新定性：

- **游戏坑 3（后端 API 404）降级为已知噪音** —— 已非「全 404」：实测单次会话
  仅 `LoginQueueService.Post` 一处 404，而 Steam 认证、WebSocket、商店数据、
  档案数据、语音聊天（`ChatService` → `LoggedIn`）**全部正常**，且不影响对战。
- **游戏坑 7（进程 40–90 秒后消失）已解决** —— 随 Proton 11.0 + 完整加固消失；
  原「疑似登录队列 404 导致游戏退出」的猜测**排除**（404 依旧存在，但游戏不再退出）。

---

## 2026.08.29 — 随从大师硬件渲染链路打通（容器运行时更新适配）

### 背景

08-26 的 WoW64 + gf 硬件渲染方案在 Steam 容器运行时（steamrt4）与 Vulkan loader
更新后全部回归（游戏 9 秒内必崩）。本轮逐层定位并修复，硬件渲染链路首次端到端打通。

### 修复

- **容器内 `/run/host/usr/lib` 不再可见**：gf ICD 库与 `libSC.so` 从该路径消失，
  loader 静默丢弃驱动、`vkCreateDevice` 段错误。改为把两库复制到 `~/gf-compat/lib/`
  （home 在容器内一定可见），自定义 `gf_icd.json` 经 `VK_ICD_FILENAMES` 注入，
  libSC 软链改指 home 副本。
- **新 Vulkan loader（1.4.309）丢弃老式 ICD + 严格校验实例扩展**：gf 协商版本 0、
  只导出 3 个 `vk_icd*` 符号，被走"直接导出符号表"路径静默丢弃；厂商又裁掉了
  `VK_KHR_external_semaphore_capabilities`，新 loader 严格校验直接拒绝
  `vkCreateInstance`。新增**协商垫片 `libvulkan_gf_shim.so`**：对外报协商版本 5、
  全程 `vk_icdGetInstanceProcAddr` 转发；枚举时补报被裁扩展，转发
  `vkCreateInstance`/`vkCreateDevice` 前剔除真 ICD 不支持的实例/设备扩展。
- **Steam overlay Vulkan 层绕过垫片直连真 ICD**（崩在 libc）→
  `VK_LOADER_LAYERS_DISABLE` 禁用（容器内 overlay 本就不可用）。
- **32 位低地址映射（显卡坑 12 终结）**：实测游戏进程低 2GB 被 wine 占用 ~1.7GB
  （`MAP_32BIT` 只剩 ~321MB），而 2~4GB 完全空闲。`osrelease_redirect.so` 内置
  **低 4GB 重映射池**：`MAP_32BIT` 失败或映射落 >4GB 时用 `MAP_FIXED_NOREPLACE`
  重映射到 2.25~4GB 空闲区。覆盖**所有** mmap 路径（含虚函数分发的未补丁路径），
  实测 1300 次重定向全部成功；驱动单字节 `MAP_32BIT` 补丁由此变为可选优化。
- 固化脚本 `fix-minion-masters-7g100-hw.sh` 升级：部署垫片/库副本、新启动选项、
  状态项扩充（幂等）；资产 `assets/mm-gf-vulkan/` 新增 `libvulkan_gf_shim.c/.so`、
  `gf_icd.json`，`osrelease_redirect.c/.so` 更新。

### 实测

- `Renderer: LISUAN 7G100 Series Graphics (ID=0x5006)`（D3D11 11.1 硬件渲染）+
  场景加载 + 登录流程（8.2s）+ 进入登录队列；游戏存活 8 分钟以上（此前各轮 9 秒内崩溃）。
- **剩余阻塞仅服务端**：`api.prod.mm21.../v1/*` 404（BetaDwarf 问题，本地无解，游戏坑 3）。

## 2026.08.25 — 随从大师（Minion Masters）进入主菜单

### 修复（文档固化，插件源码在 `/home/ubuntu/mm_patch/plugin/Patcher.cs`）

- **随从大师黑屏 + BepInEx 控制台无限刷 `NotSupportedException`**：定位为证书 patch
  （`CertificateHandler.ValidateCertificate` @0x75D98C0）破坏 IL2CPP runtime 状态——引发
  Zenject `ProjectContext` 重复创建 assert + `BootstrapInstaller.Start()` NullReference +
  异常提取栈死循环（IL2CPP `GetGenericArguments()` 未实现，每个异常的栈提取又抛新异常）。
  **修复 = 禁用证书 patch**。
- **致命 Texture 异常**（`Texture has not yet finished downloading`）：图片下载
  `Curl error 60`（证书校验失败）→ `DownloadHandlerTexture.InternalGetTexture()` 抛异常 →
  沿 UniTask `SimpleCoroutineAwaiter.Complete` / `TrySetException` 链二次传播（只 patch
  `MoveNext` 拦不住）。**修复 = patch `InternalGetTexture` @0x97CE7B0 → `xor eax,eax; ret`
  返回 null**。
- **插件最终形态**（`MMSkipVivox.dll` 15360 B，三件套）：Vivox `LoginAsync`/`LogoutAsync`
  （0x7983BD0/0x7983C90 → xor trampoline）+ 新闻下载状态机 `MoveNext`（0x6630500 → ret）+
  `InternalGetTexture`（0x97CE7B0 → null）。游戏实测到达 **`BattleModeViewModel`（主菜单）**，
  Steam ID 缓存（76561198811581867）、Ticket、WebSocket 连接、SteamShop 商品列表均成功。
- **Steam 客户端登录态自愈**：登录过一次后（`AutoLoginUser` + `RememberPassword` 已写入
  config.vdf），`steamid=0` 陈旧态只需**重启 Steam 客户端**即自动重登（state 5），无需再
  进图形界面；loopback `127.0.0.1:57343` 自动起。

### 新坑记录（防再踩）

- `il2cpp_class_get_methods` 在 BepInEx/Il2CppInterop binding 下签名错（iterator 协议被当
  数组返回），count 读出垃圾值（实测 621846768）→ 0xC0000005 闪退；枚举方法只能用
  `il2cpp_class_get_method_from_name`。
- Zenject "Tried to create multiple instances of ProjectContext" 是良性 assert（成功运行里
  也有），不要误判为崩溃根因。
- BepInEx 日志显示的插件版本是 `AssemblyVersion`（非 `BepInPlugin` 属性），源码需显式
  `[assembly: AssemblyVersion("1.1.0.0")]`。
- Unity 启动会把 `Player.log` 轮转为 `Player-prev.log`，排障时看 mtime 区分本次/上次运行。
- patch Unity 内部 native 方法前先反汇编原字节——已是 `32 C0 C3 CC`（优化过的共享
  return-false stub）说明有多个调用方，patch 它会波及无辜路径。

### 剩余阻塞（外部环境，本地无解）

- 游戏后端 `api.prod.mm21.minionmastersthegame.com` 所有 `/v1/*` API endpoint 返回 404
  （`/health` 却 200 "Healthy"）→ `LoginQueueService.Post` 抛 404，登录队列进不去（疑似
  BetaDwarf 后端下线/迁移）。
- 游戏进程启动后 ~40–90 秒被移除（进主菜单后消失，疑似登录队列 404 后游戏自身退出）。
- Azure IP `140.228.24.145:443`（及 `:33798`）网络封锁仍在。
