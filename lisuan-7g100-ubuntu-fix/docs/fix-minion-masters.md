# 随从大师（Minion Masters）修复

> 适用机型：**砺算 7G100 GPU + Ubuntu 24.04 + Proton 8.0（Steam）**。
> 现象：Steam 里启动 Minion Masters 会崩在 Vivox 登录 / 登录流程；修复后可过 Vivox 与 Steam 验证、连上游戏服务器，
> 但完整联网对局仍受 Azure IP 封锁限制。
> 修复脚本：`lisuan-7g100-ubuntu-fix/scripts/fix-minion-masters.sh`（Launcher 注入）；
> 真正的崩溃修复是 BepInEx 插件 `MMSkipVivox.dll`（源码 `/home/ubuntu/mm_patch/plugin/Patcher.cs`）。

---

## TL;DR

随从大师是 Unity / IL2CPP 游戏，有两个独立坑：

1. **崩在 Vivox 语音登录**（已修复）：游戏在 `VivoxChatServiceAdapter.LoginAsync` 里 fire-and-forget
   调用未处理异常的 async（`UniTask` 的 `PublishUnobservedTaskException`），导致进程崩在 IL2CPP
   机器码（GameAssembly.dll 内），无法按托管名 patch。修复 = BepInEx 6 插件，运行时把该函数的
   prologue 改掉，让它立即返回"已完成的 UniTask"。
2. **联网可达性**（外部网络，非配置可解）：游戏需连 Azure 服务器 `140.228.24.145:443`（及 `:33798`），
   该 IP 在当前网络环境被阻断（SYN_SENT 卡住超时），会导致登录队列/对战场景再次崩。**修复方向是
   等网络恢复或用 VPN/代理绕过封锁**，不是改 Proton/游戏配置。

当前状态（2026-08-24 排查后）：Vivox patch（v1.1.0）已修复，游戏在 **llvmpipe 软件渲染**下能完成
IL2CPP 加载 → Steam 身份验证（`Authenticate` 成功拿到 Ticket 4）→ WebSocket 连游戏服务器
（`Connection opened`）→ 推 `LoginMainViewModel` 登录界面；但随后在 `ChatService.LoginAsync` 之后的
登录流程里退出（无 crash dump、无 .NET 异常日志、进程干净退出）。Player.log 停在
`[ChatService] Starting LoginAsync`。剩余阻塞见坑 2（Azure IP 网络封锁）。

```bash
sudo ./scripts/fix-minion-masters.sh            # 注入 Launcher（gate 逻辑）→ 宿主启动 mm.sh
sudo ./scripts/fix-minion-masters.sh --uninstall
```

`mm.sh` 才是宿主启动器：先 kill 残留（Steam 进程残留），再 valve-run → 宿主 `steam -applaunch 489520`。

---

## 坑 1：Vivox 登录崩溃（已修复）

### 根因
游戏崩在 IL2CPP 机器码 `VivoxChatServiceAdapter.LoginAsync`（位于 GameAssembly.dll 内）。
这是 fire-and-forget async 未处理异常（UniTask 的 `PublishUnobservedTaskException`）。
该类型/方法**不在** Il2CppInterop 生成的 interop 程序集里，所以常规的 Harmony 按托管名 patch 行不通。

### 修复方案（已验证成功）
BepInEx 6 + 自定义插件 `MMSkipVivox.dll`（源码 `/home/ubuntu/mm_patch/plugin/Patcher.cs`，
编译目标 net6.0，引用 tools/bepinex 的 BepInEx.Core / BepInEx.Unity.IL2CPP / Il2CppInterop.Runtime）。

插件在 `Load()` 中：

1. 用 Il2CppInterop 运行时 API：`il2cpp_domain_get()` →
   `il2cpp_domain_get_assemblies(domain, ref uint count)`（`IntPtr*`，unsafe 遍历）→
   `il2cpp_assembly_get_image(asm)` →
   `il2cpp_class_from_name(image, "Assets.Rework.MinionMasters.Services", "VivoxChatServiceAdapter")`
   定位 Il2CppClass*（实际在 assembly #107）。
2. `il2cpp_class_get_method_from_name(klass, "LoginAsync", -1)` 得到 Il2CppMethodInfo*。
3. 读 `methodInfo + 0` = methodPointer。**注意**：wine 下是 32 位运行时绝对 VA，**已被重定位**
   （不是 PE 镜像基址 0x10000000；本机 ≈ 0x7BE04E0），不要套 PE 基址。
4. `kernel32.VirtualProtect` 改页为 R/W/E，把函数 prologue 改掉让它返回"已完成的 UniTask"。

   **v1.0.0（旧，有隐患）**：prologue 覆盖为 `31 C0 31 D2 C3`（xor eax,eax; xor edx,edx; ret）
   返回**全零 struct**。Unity 旧版下调用方 `await` 能立即完成不崩；但游戏 **8/23 更新后**
   `ChatService.LoginAsync` 改为 `await` Vivox 的返回，全零 UniTask 的 `GetAwaiter()` 访问
   `null` runner → `NullReferenceException` → 进程退出（e0434352），表现就是"闪退"。

   **v1.1.0（当前部署）**：不再返回全零 struct，而是返回**真正的 `UniTask.CompletedTask`**。
   做法：先 `il2cpp_class_from_name(image, "", "UniTask")`（或 `Cysharp.Threading.Tasks` 命名空间）
   找到 UniTask 类；优先 `il2cpp_class_get_method_from_name(ut, "get_CompletedTask", -1)` 跳转到该
   getter（若 IL2CPP 生成了它）；IL2CPP 没生成 getter（CompletedTask 是纯 `readonly static` 字段）
   时，用 `DllImport("GameAssembly") il2cpp_class_get_static_field_data(ut)` 拿到静态字段块基址，
   加 `il2cpp_field_get_offset(field)`（本机 off=8）得到 `CompletedTask` 字段的**运行时地址**，
   再写 trampoline `A1 <addr>`（`mov eax,[addr]`）让 `LoginAsync` 在每次被调用时**实时读取**该字段
   —— 此时 UniTask 类的 `.cctor` 早已运行，字段是真正的 completed task（runner 非 null），而非
   BepInEx 加载期读到的全零。最后 `LoginAsync` prologue 写 `E9 rel32` 跳到 trampoline。
   这样无论调用方是否 `await`，都拿到合法 completed UniTask，彻底消除旧版隐患。

### 部署现状
| 内容 | 位置 |
| --- | --- |
| `MMSkipVivox.dll` | 游戏 `BepInEx/plugins/` |
| doorstop 注入 | winhttp.dll（已注入） |
| `winhttp=n,b` 覆盖 | 游戏 `user.reg` |
| dxgi / d3d11 | 已从游戏目录移除（避免 pressure-vessel 路径冲突） |

验证：`BepInEx/LogOutput.log` 出现 `MMSkipVivox` + `PATCHED`；进程稳定存活 300+ 秒并进入场景加载；
`LoginQueue` WebSocket 连接成功（`Connection opened`）。

> 已排除的弯路：禁用 Vivox DLL 反而让游戏更早崩在登录队列；Vivox 必须"跳过"而非"禁用"。

---

## 坑 2：联网可达性（外部网络，非配置可解）

游戏需连接 Azure 服务器 **`140.228.24.145:443`**（及 `:33798`）。该 IP 的 TCP 443 在当前网络环境
被阻断（`SYN_SENT` 卡住、超时），导致登录队列/对战场景 C# 未处理异常崩。游戏其它连接（Steam CDN 等）正常。

**这是外部网络问题，Proton / 游戏配置无法解决。** 修复方向：
- 等网络恢复使该 IP 可达；或
- 用 VPN / 代理绕过封锁。

网络恢复后游戏应回到"进 Arena 后退出"状态（届时可能还需解决 Arena/Vivox 兼容性或 llvmpipe 性能问题）。

---

## 坑 3：Proton 版本

必须用 **Proton 8.0**（经 steamcmd 账号登录下载 AppID 218010，安装位置
`~/.local/share/Steam/steamapps/common/Proton 8.0`）。Proton Experimental / wine 11.0 在
7.0.0-30 内核下有 32 位地址空间回归（游戏崩在 `0x01160000`），Proton 8.0 绕过。

---

## 坑 4：user_settings.py 注入无效（教训，不要再踩）

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

## 坑 5：Steam "游戏运行中" 状态卡死

> 平台层通用说明见 [`docs/steam-on-lisuan.md` §5](./steam-on-lisuan.md)。

每次 `pkill` 强杀游戏后，Steam 会卡在"游戏运行中"状态，后续 `steam -applaunch 489520` 被静默忽略，
游戏起不来。必须 `steam -shutdown` 彻底重启 Steam 才能再次启动。调试时若发现 applaunch 拉不起游戏，
先怀疑这个，而不是游戏本身。

---

## 验证

```bash
# 1) Launcher 已注入（指向宿主 mm.sh）
cat ~/.steam/root/steamapps/common/Minion\ Masters/MinionMasters.sh | grep -A3 gate

# 2) 从 Steam 启动后：
#    - BepInEx/LogOutput.log 出现 MMSkipVivox PATCHED
#    - 进程稳定存活、进入场景
#    - Player.log 渲染后端为 llvmpipe（软件，已知限制）
```

若启动后崩在 Vivox（无 PATCHED 日志）：确认 `MMSkipVivox.dll` 在 `BepInEx/plugins/` 且 winhttp 覆盖生效。
若进场景后崩在登录队列：检查 `140.228.24.145:443` 是否可达（见坑 2）。

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

### A2. 无头环境 Steam 客户端无法登录（需图形界面生成 ssfn）

> Steam 平台层完整说明见 [`docs/steam-on-lisuan.md` §4](./steam-on-lisuan.md)。

本机环境若缺失 `ssfn` 登录令牌（只有 16 字节 `steam.token` 不足），Steam 无法静默自动登录。
在无头 XWayland 下，Steam CEF 登录窗会 `X Error: BadValue (opcode 149 = GLX)` 崩溃
（`webhelper.txt` 累积数千次），登录窗被 "safe browser shutdown workaround" 关闭 →
永远 `steamid=0`。后果：游戏 Steamworks 初始化失败
（`SteamInternal_SetMinidumpSteamID ... API loaded no`），wine 反复抛
`unknown exception (code=e0434352)`，游戏在 BepInEx Chainloader 之后、Unity 写 Player.log
之前就退出（无 Player.log）。`steam -offlinemode -silent` 能绕过 CEF 窗拉起 Steam 与游戏，
但 Steam 仍无 steamid，游戏同样 `e0434352` 退出。**必须在图形界面登录 Steam 一次生成
`ssfn`**，游戏才能正常启动到渲染阶段。

### A3. Standard shader 不支持（画质告警，不致命）

Player.log 偶见 `ERROR: Shader Standard shader is not supported on this GPU`
（gf / llvmpipe 不支持 Unity Standard shader）。这通常**不导致进程退出**，仅影响画质，
无需处理。
