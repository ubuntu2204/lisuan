# Steam 在砺算 7G100 上的适配细节

> 适用机型：**砺算 7G100 GPU + Ubuntu 24.04 + Steam（pressure-vessel 容器）**。
> 本文集中收录 Steam 客户端本身的全部坑（容器机制、gf env 污染、CEF 崩溃、无头登录、
> applaunch 状态卡死、Proton 选型、user_settings.py 注入无效、overlay 等）。
> 游戏专属的坑请见 `fix-minion-masters.md` / `fix-underlords-native.md`，本文只讲 Steam 平台层。

## 至砺算：官方实现什么可以让 Steam 正常运行

> 本节写给砺算官方驱动 / 图形栈研发。按投入产出比排序；**任意一项落地，本仓库对应 workaround 即可退役**。

1. **修复 gf_gbm.so 与系统 libgbm 的 ABI 兼容**（最高优先）：gf_gbm.so 与 Mesa
   libgbm ≥ 25.x 的 `gbm_device` vtable 不兼容，Steam 客户端 CEF GPU 进程
   （steamwebhelper）加载即 SIGSEGV（exit_code=139，见 §3）。要么按当前 libgbm
   头文件重编厂商 gbm 后端，要么随驱动自带完整 libgbm 并做好 soname 隔离。
   → 退役：steam.desktop 的 `GBM_BACKEND=dri` 注入（install.py 菜单 15）。
2. **GLVND 标准集成**：把 GLX vendor ICD 装到 `/usr/share/glx/vendors/`、EGL ICD
   装到 `/usr/share/egl/egl_vendor.d/`，并保证设备认领**不依赖 `/sys/bus/pci`**
   （pressure-vessel 容器会隐藏它；参考 Mesa 基于 `/dev/dri/renderD*` +
   `drmGetDevice` 的发现方式）。
   → 退役：`__GLX_VENDOR_LIBRARY_NAME` 环境变量注入与 drirc kernel_driver 映射依赖。
3. **drirc 随包分发确认**：`/usr/share/drirc.d/01-gf_drv.conf`
   （kernel_driver→dri_driver 映射）应由 lisuangpu-lib 包提供、升级时保留——
   它是宿主原生 GL 免软链方案的基础。

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

**真正的根因（2026-08-24 实测）**：不是 env 变量，而是 **全局 DRI 软链**
`/usr/lib/x86_64-linux-gnu/dri/lisuan_dri.so -> gf_dri.so`（由旧版建链脚本创建，
该脚本已随软链方案废弃删除）。它让 Steam 客户端（pressure-vessel sniper
容器内）的 EGL 加载到厂商 **gf 的 22.0.4 fork 驱动**，在无头/XWayland 下该 fork 触发
`double free or corruption` → `Aborted (core dumped)`。移除这两个软链后 Steam 客户端
`double free` 立即消失、转为稳定的 `failed to load driver: (null)`（仅告警、不崩）。

**另一个曾引发同样崩溃的污染源**：`fix-steam-cef-lock.sh` 把
`LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libglapi_gf.so.0` 注入到
`steamwebhelper_sniper_wrap.sh`（即 **Steam 客户端 wrap**，非游戏）→ 同样让 Steam 客户端
走 gf glapi 而 double free。该 PRELOAD 应只注入游戏进程树，绝不能进 Steam 客户端 wrap
（见 §3）。

**判定要点**：若 Steam 客户端本身崩溃（非游戏），先查
`/usr/lib/x86_64-linux-gnu/dri/lisuan_*.so` 软链是否存在、以及 `steamwebhelper_sniper_wrap.sh`
是否被注入了 gf `LD_PRELOAD`；去掉后 Steam 客户端即恢复。游戏进程树内的 gf 注入不受影响。

**修复**：建链脚本已删除，软链方案彻底废弃（若见残留软链直接删除即可）；
`fix-steam-cef-lock.sh` 的 Steam 客户端 PRELOAD 注入应移除（见 §3）。

（完整论证与修复见 `vendor-bug-report-gf-gbm-crash.md` §6。）

---

## 3. Steam 客户端 CEF 崩溃（gf_gbm / ANGLE）

Steam UI 的 CEF/GPU 进程在 gf 驱动下有两个崩溃点：

1. **`gf_gbm.so` 段错误**（exit_code=139）：现代 `libgbm` 下厂商 `gf_gbm.so` 因 ABI 不兼容崩溃。
2. **容器内 ANGLE 惰性符号解析失败**：CEF 的 ANGLE 指向系统 Mesa EGL（无人把 CEF 指到厂商 EGL），
   而系统 Mesa EGL 对本卡无可用后端，符号解析异常（非崩溃，最终回退软渲染）。

> **重要（2026-08-24 修正）**：原 `fix-steam-cef-lock.sh` 把
> `LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libglapi_gf.so.0` + `GBM_BACKEND=dri`
> 注入到 **`steamwebhelper_sniper_wrap.sh`（Steam 客户端 wrap）** 并 `chattr +i` 锁定。
> 这会让 **Steam 客户端自身** 走 gf glapi → 在无头/XWayland 下 `double free or corruption`
> 崩溃（见 §2）。该 PRELOAD **绝不能进 Steam 客户端 wrap**，否则 Steam 打不开、所有游戏
> 起不来。正确做法是把这些变量只注入**游戏进程树**（游戏 Launch Options 或 Proton
> `user_settings.py`，且仅当游戏渲染确实需要时）。
>
> 实测：移除 Steam 客户端 wrap 里的 PRELOAD + 全局 `lisuan_dri.so` 软链后，Steam 客户端
> 稳定（仅 `failed to load driver: (null)` 告警、不崩），游戏(Proton/wine) 渲染走 gf Vulkan
> 不受影响（不依赖这些软链）。故 `fix-steam-cef-lock.sh` 应**停止向 Steam 客户端 wrap 注入**
> gf PRELOAD（保留为可选、仅供原生游戏排障用，且明确警告其会害 Steam 客户端）。

### 最终解法（2026-09-01 实测确认）：`GBM_BACKEND=dri` 启动环境变量

```bash
GBM_BACKEND=dri steam          # 命令行
```

桌面启动器 `~/.local/share/applications/steam.desktop`（已配置，菜单启动同样生效）：

```ini
Exec=env GBM_BACKEND=dri /usr/bin/steam %U
```

**为什么这是正确解法**：

- libgbm 支持 `GBM_BACKEND` / `GBM_BACKENDS_PATH` 两个环境变量（见 `strings libgbm.so.1`）。
  设 `GBM_BACKEND=dri` 后，libgbm 直接加载 Mesa 的 `dri_gbm.so`（56KB），
  **不再扫描加载厂商 `gf_gbm.so`**（106KB），从源头消除段错误。
- **不修改任何文件**（既不动系统库，也不动 Steam 的 wrap 脚本），因此：
  - 不改变 `steamwebhelper_sniper_wrap.sh` 大小 → **不触发 Steam 校验回滚**；
  - 无需 `chattr +i` 锁定 → **不会陷入更新回滚循环**（该循环会重置游戏下载状态，
    曾导致刀塔霸业被反复重新下载，见 §10）。
- 不含 `LD_PRELOAD libglapi_gf.so.0` → 不让 Steam 客户端走 gf glapi → **不会 double free**（§2）。

**验证**：`grep -c 'exit_code=139' ~/.local/share/Steam/logs/cef_log.txt` → `0`。

> **重要纠正（推翻 2026-08-24 的结论）**：仅移除全局 `lisuan_dri.so` 软链和 wrap 里的
> `LD_PRELOAD` **并不足以让 CEF 稳定**。实测在"干净"状态下 CEF 仍以 exit_code=139 崩溃，
> `steamwebhelper` 无限重启（`startcount` 0→19+）。真正的崩溃源是 `gf_gbm.so`，
> 必须靠 `GBM_BACKEND=dri` 绕开。
>
> 此前把 `GBM_BACKEND=dri` 误判为"有害"，是因为它在旧脚本里与
> `LD_PRELOAD libglapi_gf.so.0` **同时**被注入 wrap——有害的是 PRELOAD（导致 double free），
> 不是 `GBM_BACKEND=dri` 本身。二者必须分开看待。

### 备用方案

若 `GBM_BACKEND` 不生效，可用 `GBM_BACKENDS_PATH` 把搜索路径限制到只含 dri 后端的目录：

```bash
mkdir -p ~/.local/share/gbm-dri-only
cp /usr/lib/x86_64-linux-gnu/gbm/dri_gbm.so ~/.local/share/gbm-dri-only/
GBM_BACKENDS_PATH=$HOME/.local/share/gbm-dri-only steam
```

### 游戏进程树

游戏（Proton/wine）渲染走 gf Vulkan，不依赖 gbm 后端，无需额外注入；
仅当原生 Linux 游戏确需 gf GL 时，才在**游戏 Launch Options** 内单独设置（不要写全局）。

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

| 游戏 | 当前使用版本 | 原因 |
| --- | --- | --- |
| 随从大师（489520） | **Proton 11.0 稳定版** | 见下方说明 |

### 选用 Proton 11.0 稳定版的理由（2026-08-31 起）

- ✅ **推荐：Proton 11.0 稳定版**。更新频率低、目录结构稳定，加固脚本
  `minion-masters/fix-minion-masters-7g100-hw.sh` 已适配其 `files/lib/` 路径。
  实测 `Renderer: LISUAN 7G100`，D3D11 设备创建失败 **0 次**。
- ❌ **不要用 `Proton Experimental`**。2026-08-26 的 `experimental-11.0-20260826`
  把库目录由 `files/lib64/` 改为 `files/lib/`，导致三处加固**静默失效**：

  | 加固项 | 失效表现 |
  | --- | --- |
  | `libSC.so` 软链 | 悬空（指向已删除旧路径）→ gf `vkCreateDevice` 失败 |
  | DXVK 降级 | 还原为新版（要求 `VK_KHR_load_store_op_none`，gf 22.0.4 不支持）→ DXVK 跳过 7G100 |
  | hook 部署路径 | 指向失效 |

  `libSC.so`（gf 着色器编译器伴侣库）悬空是直接致命原因，表现为
  `Failed to initialize graphics ... InitializeEngineGraphics failed`。
  详见 CHANGELOG `2026.08.31`。

- ⚠️ **版本号易混淆**：`Proton Experimental` 版本号含 "11.0" 字样
  （如 `experimental-11.0-20260826`），但与独立安装的稳定版《**Proton 11.0**》
  **不是同一个东西**。选型时以 Steam 库中的条目名为准。

- 📌 **历史**：早期曾用 **Proton 8.0** 绕过 7.0.0-30 内核下的 32 位地址空间回归
  （游戏崩在 `0x01160000`）。现已被 Proton 11.0 稳定版取代。

> 注：Proton 版本仅影响 Wine 类游戏；原生 Linux 游戏（刀塔霸业）不走 Proton。

**切换 Proton 后必做**：重跑加固脚本（改 vdf 需退出 Steam 后执行，跑完重启 Steam）：

```bash
./minion-masters/fix-minion-masters-7g100-hw.sh
```

**配置核对**（排查"到底跑在哪个 Proton 上"时用）：

```bash
grep -A4 '"489520"' ~/.local/share/Steam/config/config.vdf
```

期望输出（随从大师单独覆盖全局默认值）：

```
"489520"
{
    "name"      "proton_11"      # ← 随从大师
    "config"    ""
    "priority"  "250"
}
```

注意全局条目 `"0"` 通常是 `proton_experimental`，属于 Steam 默认，
**不影响**已单独指定 `proton_11` 的随从大师。

---

## 7. Proton `user_settings.py` 注入无效（教训）

`~/.local/share/Steam/steamapps/common/Proton 11.0/user_settings.py` 的正确格式是
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
| Steam UI 花屏 / GPU 进程崩 (`exit_code=139`) | CEF gbm（§3），用 `GBM_BACKEND=dri steam` 启动 |
| `steamwebhelper` 无限重启（`startcount` 递增） | 同上（§3），`GBM_BACKEND=dri` |
| **游戏反复自动重新下载** | Steam 更新回滚循环（§10），`lsattr` 查是否被 `chattr +i` 锁死 |
| 游戏 e0434352 退出、无 Player.log | Steam 未登录（§4，缺 ssfn），图形界面登录一次 |
| `applaunch` 拉不起游戏 | "游戏运行中"状态卡死（§5），`steam -shutdown` 重启 |
| 游戏崩在 `0x01160000` | Proton 版本（§6），用 Proton 11.0 稳定版 |
| `Failed to initialize graphics` / `InitializeEngineGraphics failed` | Proton Experimental 改了库目录致加固失效（§6），改用 Proton 11.0 稳定版并重跑加固脚本 |
| 注入的显卡 env 不生效 | user_settings.py 不进容器（§7），改用 Launch Options |

---

## 10. 游戏反复自动重新下载（Steam 更新回滚循环）

现象：每次打开 Steam，某个游戏（如刀塔霸业 / Dota Underlords）都被重新排队下载，
进度反复重置，永远装不完。

根因：`steamwebhelper_sniper_wrap.sh` 被 `chattr +i` 设为不可变（旧版
`fix-steam-cef-lock.sh` 的行为）。Steam 启动时校验发现文件大小不符，于是走
"下载更新 → `failed to rename` → `Failed to apply update, reverting...`" 的**回滚循环**。
该循环会重置游戏的下载状态，游戏因此被反复重下。

日志特征：

```
BVerifyInstalledFiles: ubuntu12_64/steamwebhelper_sniper_wrap.sh is 680 bytes, expected 623
BCommitUpdatedFiles: failed to rename ./ubuntu12_64/steamwebhelper_sniper_wrap.sh -> ...
Failed to apply update, reverting...
```

**修复**：

```bash
sudo chattr -i ~/.local/share/Steam/ubuntu12_64/steamwebhelper_sniper_wrap.sh
```

解锁后启动一次 Steam，让它正常完成更新、把文件恢复成原厂大小（623 字节）。
用 `lsattr` 确认 `i` 标志已消失：

```bash
lsattr ~/.local/share/Steam/ubuntu12_64/steamwebhelper_sniper_wrap.sh
# 正常应为：--------------e------- （无 i）
```

**教训**：**不要**用 `chattr +i` 去"保护"对 Steam 文件的修改。Steam 有严格的自我校验
机制，锁定文件只会让它陷入回滚循环，连带破坏游戏下载状态。需要持久化配置时，
一律走**环境变量 + 桌面启动器**（如 §3 的 `GBM_BACKEND=dri`），不碰 Steam 自己的文件。
