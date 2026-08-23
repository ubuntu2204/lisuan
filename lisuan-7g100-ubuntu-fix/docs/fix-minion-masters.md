# 随从大师（Minion Masters）修复

> 适用机型：**砺算 7G100 GPU + Ubuntu 24.04 + Proton 8.0（Steam）**。
> 现象：Steam 里启动 Minion Masters 会崩在 Vivox 登录；修复后可进入场景、不崩。
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

当前状态：Vivox 崩溃已修复，游戏在 **llvmpipe 软件渲染**下能进场景、稳定不崩；能否真正联网对局
取决于上面那个 Azure IP 是否可达。

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
4. `kernel32.VirtualProtect` 改页为 R/W/E，把函数 prologue 覆盖为
   `31 C0 31 D2 C3`（xor eax,eax; xor edx,edx; ret）—— 返回全零结构 = 已完成的 UniTask，
   调用方 `await` 立即完成，不抛异常，不崩。

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
