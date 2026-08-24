# Steam 在砺算 7G100 上的适配细节

> 适用机型：**砺算 7G100 GPU + Ubuntu 24.04 + Steam（pressure-vessel 容器）**。
> 本文集中收录 Steam 客户端本身的全部坑（容器机制、gf env 污染、CEF 崩溃、无头登录、
> applaunch 状态卡死、Proton 选型、user_settings.py 注入无效、overlay 等）。
> 游戏专属的坑请见 `fix-minion-masters.md` / `fix-underlords-native.md`，本文只讲 Steam 平台层。

---

## 1. Steam 的容器架构（必须先理解）

Steam 在 Linux 上不是"一个进程"，而是多层容器：

| 组件 | 容器 / 运行环境 | 说明 |
| --- | --- | --- |
| Steam 客户端 UI（CEF / steamwebhelper） | **pressure-vessel `sniper` 容器** | 商店、好友、设置、CEF 登录窗都在这里跑 |
| 原生 Linux 游戏（如刀塔霸业） | **pressure-vessel `scout-on-soldier` 容器** | 与客户端容器是兄弟，共享宿主 `/run/host` |
| Proton / Wine 游戏（如随从大师） | soldier 容器 + Proton 中转 | wine 在 soldier 容器内运行 |

关键事实：

- pressure-vessel 把**宿主根挂到容器内 `/run/host`**，并**自动把宿主 gf Vulkan ICD 合并进容器**（`/usr/lib/pressure-vessel/overrides/` 下已含 `gf_icd.x86_64.json`，容器内 `VK_ICD_FILENAMES` 自动含它）。即 **gf Vulkan 在游戏容器内默认可用，无需手动注入**。
- pressure-vessel 只传**白名单 env** 进容器。自定义显卡变量（`VK_ICD_FILENAMES` / `WINEDLLOVERRIDES` 等）写进 Proton `user_settings.py` **不会进入容器环境**（见 §7）。
- 容器内 PCI sysfs（`/sys/bus/pci`）被隐藏，`/dev/dri` 正常可见。厂商 gf Vulkan ICD 依赖 `/sys/bus/pci` 枚举设备 → 在容器内枚举到 0 设备 → **所有 Vulkan 游戏在 Steam 运行时内默认不可用**（除非驱动改走 `drmGetDevice2()` 枚举，见 `vendor-bug-report-gf-gbm-crash.md` §7 建议 1）。AMD/Intel/NVIDIA 的 ICD 走 `drmGetDevice2()` 故正常。

---

## 2. 致命坑：gf 环境变量不能污染 Steam 客户端

> 这是实机踩出的致命坑，会让人误判为"游戏/驱动问题"。

`__GLX_VENDOR_LIBRARY_NAME=gf`、`VK_ICD_FILENAMES` 指向 gf ICD 这类 gf 图形环境变量，
**只能注入到游戏进程树（Launch Options 或 Proton `user_settings.py` 顶层格式）**，
**绝不能**写进全局 shell profile（`~/.bashrc`、`/etc/environment`）或 Steam 启动环境。

原因：pressure-vessel 把宿主 gf 库挂进容器后，**Steam 客户端自身（CEF/steamwebhelper，
非游戏）也会继承这些 env 并走 gf GLX**，而 Steam 客户端的 GLX 路径在 gf 驱动下会
`double free or corruption` + `failed to load driver: (null)`，Steam 进程直接 `Aborted`。
表现像"Steam 打不开 / 闪退"，与游戏无关，但会连带让所有游戏起不来。

**判定要点**：若 Steam 客户端本身崩溃（非游戏），先检查是否有全局 gf env 注入；去掉后
Steam 客户端即恢复。游戏进程树内的 gf env 注入不受此影响。

（完整论证与修复见 `vendor-bug-report-gf-gbm-crash.md` §6。）

---

## 3. Steam 客户端 CEF 崩溃（gf_gbm / ANGLE）

Steam UI 的 CEF/GPU 进程在 gf 驱动下有两个崩溃点：

1. **`gf_gbm.so` 段错误**（exit_code=139）：现代 `libgbm` 下厂商 `gf_gbm.so` 因 ABI 不兼容崩溃。
2. **容器内 ANGLE 惰性符号解析失败**：CEF 的 ANGLE 指向系统 Mesa EGL（无人把 CEF 指到厂商 EGL），
   而系统 Mesa EGL 对本卡无可用后端，符号解析异常（非崩溃，最终回退软渲染）。

修复（`scripts/fix-steam-cef-lock.sh`，并 `steam-cef-autofix.sh` 开机自愈）：

- 注入 `GBM_BACKEND=dri` → `gf_gbm` 段错误消失；
- `LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libglapi_gf.so.0` → 容器内 ANGLE 符号解析崩溃消失；
- 写入 `steamwebhelper_sniper_wrap.sh` 并 `chattr +i` 锁定（Steam 更新会还原补丁，故需开机自愈重注）。

日志位置：`~/.local/share/Steam/logs/cef_log.txt`（exit_code=139 段）。
（完整论证见 `vendor-bug-report-gf-gbm-crash.md` 问题一/二。）

---

## 4. 无头环境 Steam 客户端无法登录（需图形界面生成 ssfn）

若环境缺失 `ssfn` 登录令牌（只有 16 字节 `steam.token` 不足），Steam 无法静默自动登录。

在无头 XWayland 下，Steam CEF 登录窗会 `X Error: BadValue (opcode 149 = GLX)` 崩溃
（`webhelper.txt` 累积数千次），登录窗被 "safe browser shutdown workaround" 关闭 →
永远 `steamid=0`。

后果：游戏 Steamworks 初始化失败（`SteamInternal_SetMinidumpSteamID ... API loaded no`），
wine 反复抛 `unknown exception (code=e0434352)`，游戏在 BepInEx Chainloader 之后、Unity 写
Player.log 之前就退出（**无 Player.log**）。

`steam -offlinemode -silent` 能绕过 CEF 窗拉起 Steam 与游戏，但 Steam 仍无 steamid，游戏同样
`e0434352` 退出。**必须在图形界面登录 Steam 一次生成 `ssfn`**，游戏才能正常启动到渲染阶段。

> 这也是为什么"游戏跑不到渲染"往往不是 gf 驱动问题，而是 Steam 登录会话缺失。

---

## 5. `steam -applaunch` 与"游戏运行中"状态卡死

每次 `pkill` 强杀游戏后，Steam 会卡在"游戏运行中"状态，后续 `steam -applaunch <appid>` 被
静默忽略，游戏起不来。必须 `steam -shutdown` 彻底重启 Steam 才能再次启动。

调试时若发现 `applaunch` 拉不起游戏，**先怀疑这个状态卡死**，而不是游戏本身。

宿主启动器（`mm.sh` 之类）正确顺序：先 `kill` 残留 Steam 进程 → `steam -shutdown` →
`valve-run` → 宿主 `steam -applaunch <appid>`。

---

## 6. Proton 版本选型

| 游戏 | 必须版本 | 原因 |
| --- | --- | --- |
| 随从大师（489520） | **Proton 8.0** | Proton Experimental / wine 11.0 在 7.0.0-30 内核下有 32 位地址空间回归（游戏崩在 `0x01160000`），Proton 8.0 绕过。经 steamcmd 账号登录下载 AppID 218010 安装到 `~/.local/share/Steam/steamapps/common/Proton 8.0` |

> 注：Proton 版本仅影响 Wine 类游戏；原生 Linux 游戏（刀塔霸业）不走 Proton。

---

## 7. Proton `user_settings.py` 注入无效（教训）

`~/.local/share/Steam/steamapps/common/Proton 8.0/user_settings.py` 的正确格式是
**顶层键值对**：

```python
user_settings = {
    "VK_ICD_FILENAMES": "/usr/share/vulkan/icd.d/gf_icd.x86_64.json",
}
```

坑：

- ❌ 不能用嵌套 `session_environment`：嵌套 dict 会被 proton 当成 env 值 →
  `TypeError: expected str... not dict` → 游戏根本不启动。
- ⚠️ 即便格式正确，`user_settings.py` 注入的自定义变量（`VK_ICD_FILENAMES` /
  `WINEDLLOVERRIDES`）**不会进入 pressure-vessel 容器环境**（容器只传白名单 env）。
  所以 `user_settings.py` 不是注入显卡驱动的可靠途径。

当前部署：**已移除自定义 `user_settings.py`**（保持顶层默认），游戏用 llvmpipe 软件渲染，
或依赖 Steam 默认启动下 pv 自动注入的 gf Vulkan。

---

## 8. Steam Overlay（游戏叠加层）

游戏叠加层（gameoverlayrenderer）会注入游戏进程。在某些崩溃场景（如登录流程崩）可尝试
在游戏属性取消"启用 Steam 叠加层"以排查是否由 overlay 注入引起。本机未确认 overlay 为
随从大师崩溃根因（崩溃定位在 Vivox 登录逻辑，与 overlay 无关），但作为排障手段保留。

---

## 9. 排障速查

| 现象 | 先查 |
| --- | --- |
| Steam 客户端打不开 / 闪退 | 全局 gf env 污染（§2），去掉后恢复 |
| Steam UI 花屏 / GPU 进程崩 | CEF gbm/ANGLE（§3），跑 `fix-steam-cef-lock.sh` |
| 游戏 e0434352 退出、无 Player.log | Steam 未登录（§4，缺 ssfn），图形界面登录一次 |
| `applaunch` 拉不起游戏 | "游戏运行中"状态卡死（§5），`steam -shutdown` 重启 |
| 游戏崩在 `0x01160000` | Proton 版本（§6），换 Proton 8.0 |
| 注入的显卡 env 不生效 | user_settings.py 不进容器（§7），改用 Launch Options |
