# Steam 在砺算 7G100 上的适配细节

> 适用机型：**砺算 7G100 GPU + Ubuntu 24.04 + Steam（pressure-vessel 容器）**。
> 本文集中收录 Steam 客户端本身的全部坑（容器机制、gf env 污染、CEF 崩溃、无头登录、
> applaunch 状态卡死、Proton 选型、user_settings.py 注入无效、overlay 等）。
> 游戏专属的坑请见 `fix-minion-masters.md` / `fix-underlords-native.md`，本文只讲 Steam 平台层。
> Steam 客户端下载（免费）：<https://store.steampowered.com/about/>

## 至砺算：官方实现什么可以让 Steam 正常运行

> 本节写给砺算官方驱动 / 图形栈研发。按投入产出比排序；**任意一项落地，本仓库对应 workaround 即可退役**。

1. **修复 gf_gbm.so 与系统 libgbm 的 ABI 兼容**（最高优先）：gf_gbm.so 与 Mesa
   libgbm ≥ 25.x 的 `gbm_device` vtable 不兼容，Steam 客户端 CEF GPU 进程
   （steamwebhelper）加载即 SIGSEGV（exit_code=139，见 §3）。另外 `gf_dri.so` /
   `libglapi_gf.so.0` 这套 22.0.4 fork 不能与系统 Mesa 25.2 在容器里混用，
   新建 GL 上下文会 double free（见 §3.1）。要么按当前 libgbm / Mesa ABI
   重编厂商 GBM 与 DRI 驱动，要么随驱动自带完整 libgbm 并做好 soname 隔离。
   → 退役：steam.desktop 的 `GBM_BACKEND=dri` + `LIBGL_ALWAYS_SOFTWARE=1` +
   `__GLX_VENDOR_LIBRARY_NAME=mesa` 注入与 `-cef-disable-gpu` 启动参数
   （install.py 菜单 12，现行四项见 §3 / §3.2）。
2. **GLVND 标准集成**：把 GLX vendor ICD 装到 `/usr/share/glx/vendors/`、EGL ICD
   装到 `/usr/share/egl/egl_vendor.d/`，并保证设备认领**不依赖 `/sys/bus/pci`**
   （pressure-vessel 容器会隐藏它；参考 Mesa 基于 `/dev/dri/renderD*` +
   `drmGetDevice` 的发现方式）。
   → 退役：`__GLX_VENDOR_LIBRARY_NAME` 环境变量注入（含现行 `=mesa` 规避）
   与 drirc kernel_driver 映射依赖。
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

### 最终解法（2026-11-13 更新）：`GBM_BACKEND=dri` + `LIBGL_ALWAYS_SOFTWARE=1` + `__GLX_VENDOR_LIBRARY_NAME=mesa` + `-cef-disable-gpu`

```bash
GBM_BACKEND=dri LIBGL_ALWAYS_SOFTWARE=1 __GLX_VENDOR_LIBRARY_NAME=mesa steam -cef-disable-gpu   # 命令行
```

桌面启动器（菜单 [12] 会统一写用户级菜单项、XDG 桌面图标与历史 `~/Desktop` 入口）：

```ini
Exec=env GBM_BACKEND=dri LIBGL_ALWAYS_SOFTWARE=1 __GLX_VENDOR_LIBRARY_NAME=mesa /usr/bin/steam -cef-disable-gpu %U
```

> **允许叠加自己的环境变量**：Exec 行的 env 段可以按需加入其它变量而不影响
> 修复判定，例如 `Exec=env LC_ALL=en_US.UTF-8 GBM_BACKEND=dri
> LIBGL_ALWAYS_SOFTWARE=1 __GLX_VENDOR_LIBRARY_NAME=mesa
> /usr/bin/steam -cef-disable-gpu %U`。菜单 [12] 按
> 「三个变量都在 Exec 行 env 段、`-cef-disable-gpu` 在 steam 之后、且不含旧 gf
> 注入」判定/注入，只改 Exec 行、保留其余内容。

**为什么需要四项**：

- libgbm 支持 `GBM_BACKEND` / `GBM_BACKENDS_PATH` 两个环境变量（见 `strings libgbm.so.1`）。
  设 `GBM_BACKEND=dri` 后，libgbm 直接加载 Mesa 的 `dri_gbm.so`（56KB），
  **不再扫描加载厂商 `gf_gbm.so`**（106KB），从源头消除 CEF 启动段错误。
- `LIBGL_ALWAYS_SOFTWARE=1` 让 Steam 客户端新建 GL 上下文时走 Mesa llvmpipe，
  **不再按 `/usr/share/drirc.d/01-gf_drv.conf` 的 `kernel_driver=gf → dri_driver=gf`
  映射加载 `gf_dri.so` / `libglapi_gf.so.0`**。后者是「帮助→系统信息」弹窗闪退的
  根因，详见 §3.1。
- `__GLX_VENDOR_LIBRARY_NAME=mesa` 让 GLVND 把 Steam 客户端的 GLX 分发到系统
  Mesa vendor：`glXCreateContext` 成功、`glXIsDirect` 通过，消除启动时
  「OpenGL GLX context is not using direct rendering」弹窗与 GLX BadValue
  （2026-11-13 八组消融，详见 §3.2）。**必须是 `mesa` 而不是 `gf`**：
  gf GLX 会让客户端崩溃（§2）。
- `-cef-disable-gpu` 是 **Steam 官方命令行参数**（`strings ubuntu12_32/steam` 可见，
  2026-08 日志里也使用过），**不是**往 `steamwebhelper_sniper_wrap.sh` 注入
  `--disable-gpu`。只加软件 GL 时 CEF 的 ANGLE/EGL GPU 进程初始化失败：
  `eglInitialize ... failed` / `Exiting GPU process due to errors during initialization`，
  界面会一直停在加载动画；显式禁用 CEF GPU 后走 CPU 合成，主界面和系统信息弹窗
  都能正常渲染。该参数只作用于 Steam 客户端 CEF，游戏进程不继承。
- **不修改任何 Steam 文件**（既不动系统库，也不动 `steamwebhelper_sniper_wrap.sh`），因此：
  - 不改变 wrap 脚本大小 → **不触发 Steam 校验回滚**；
  - 无需 `chattr +i` 锁定 → **不会陷入更新回滚循环**（该循环会重置游戏下载状态，
    曾导致刀塔霸业被反复重新下载，见 §10）。
- 不含 `LD_PRELOAD libglapi_gf.so.0` → 不让 Steam 客户端走 gf glapi → **不会 double free**（§2）。
- `LIBGL_ALWAYS_SOFTWARE=1` 只影响 GL，不改 Vulkan：实测 pressure-vessel 游戏容器内
  `VK_ICD_FILENAMES` 仍是 gf ICD，Proton/DXVK/Vulkan 游戏不受影响。

**验证**：`grep -c 'exit_code=139' ~/.local/share/Steam/logs/cef_log.txt` → `0`；
点系统信息后 `console-linux.txt` 无 `double free` / `Aborted`；X11 抓图确认
主界面 1280x800（128463 色）与标题为「系统信息」的 600x500 窗口（5712 色）
均为真实内容，不是加载动画；四项齐全后 `console-linux.txt` 新增行无
`not using direct rendering` / `glXCreateContext failed`（§3.2）；
`python3 install.py --detect` 的 Steam 项为 `ok`，菜单 12 状态为「✓ 已修复」。

> **重要纠正（推翻 2026-08-24 的结论）**：仅移除全局 `lisuan_dri.so` 软链和 wrap 里的
> `LD_PRELOAD` **并不足以让 CEF 稳定**。实测在"干净"状态下 CEF 仍以 exit_code=139 崩溃，
> `steamwebhelper` 无限重启（`startcount` 0→19+）。真正的崩溃源是 `gf_gbm.so`，
> 必须靠 `GBM_BACKEND=dri` 绕开。
>
> 此前把 `GBM_BACKEND=dri` 误判为"有害"，是因为它在旧脚本里与
> `LD_PRELOAD libglapi_gf.so.0` **同时**被注入 wrap——有害的是 PRELOAD（导致 double free），
> 不是 `GBM_BACKEND=dri` 本身。二者必须分开看待。

### 3.1 帮助 → 系统信息闪退（2026-11-09 新增根因）

现象：Steam 客户端能正常打开，但点「帮助 → 系统信息」（`steam://open/systeminfo`）
时整个客户端闪退。现场证据：

```text
# 长会话（11-09 01:55）
steamui_html.txt: PopupHTMLWindow / BrowserReady
console-linux.txt: steam: ../../src/xcb_io.c:626: _XAllocID: Assertion `ret != inval_id' failed.
steam.sh: line 995: ... Aborted (core dumped)

# 启动后短时间内触发（11-09 02:28 等多次）
console-linux.txt: double free or corruption (!prev)
console-linux.txt: Aborted (core dumped)
```

minidump 里 `steamwebhelper` 映射了 `/run/host/usr/lib/x86_64-linux-gnu/libglapi_gf.so.0.0.0`；
`ldd /usr/lib/x86_64-linux-gnu/dri/gf_dri.so` 证实这个厂商 glapi 只由 `gf_dri.so` 依赖。
也就是说：`GBM_BACKEND=dri` 绕开了 `gf_gbm.so`，但**没有绕开 `dri_gbm.so` 继续按
drirc 映射加载 `gf_dri.so`**。新建系统信息弹窗/GL 上下文时，这套 22.0.4 fork 在
pressure-vessel 容器里与系统 Mesa 25.2 混用，触发 double free 或 Xlib 连接状态损坏。

`steamsysinfo` 的 Vulkan 查询失败（`VK_ERROR_INCOMPATIBLE_DRIVER`，缺 WSI 扩展）是另一层
独立噪音，不是闪退主因；不要为它把 `VK_ICD_FILENAMES` 改成 llvmpipe，那会连累游戏。

**解法**：给 Steam 客户端加 `LIBGL_ALWAYS_SOFTWARE=1`、
`__GLX_VENDOR_LIBRARY_NAME=mesa` 与 `-cef-disable-gpu`（见 §3 Exec），不碰
`VK_ICD_FILENAMES`、不改 Steam 文件、不加看门狗。软件 GL 防 `gf_dri.so` 崩溃，
Mesa GLX vendor 顺带消除启动时的 GLX 非直接渲染弹窗（§3.2），
`-cef-disable-gpu` 让 CEF 在软件 GL 下跳过失败的 GPU 初始化、改用 CPU
合成——**只加软件 GL 虽不闪退，但界面会一直停在加载动画**，
这是 2026-11-09 继续验证时补上的关键一层。

受控 A/B（启动 7 秒后 `steam steam://open/systeminfo`）：

| 启动环境 | 结果 |
|---|---|
| `GBM_BACKEND=dri` | 5 次里 4 次 `double free` / `Aborted` |
| `GBM_BACKEND=dri LIBGL_ALWAYS_SOFTWARE=1` | 0/5 崩溃，但 CEF GPU 进程初始化失败，主界面卡加载动画（抓图约 500–1000 色） |
| `GBM_BACKEND=dri -cef-disable-gpu` | 3 次里 1 次仍崩，单独不够 |
| `GBM_BACKEND=dri MESA_LOADER_DRIVER_OVERRIDE=llvmpipe` | 0/3 崩溃，但重复 `failed to load driver: llvmpipe`，未采用 |
| **三项组合（当时结论，现行四项见 §3 / §3.2）** | 0 崩溃；X11 抓图主界面 128463 色、标题「系统信息」弹窗 5712 色，均为真实内容 |

`MESA_LOADER_DRIVER_OVERRIDE=llvmpipe` 与单独 `-cef-disable-gpu` 都只是中间对照，
不要写回桌面启动器。

另有一处**独立的入口回归**：`~/桌面/steam.desktop`（DING 真正监听的目录）还停留在
2026-08-24 的旧配置 `MESA_LOADER_DRIVER_OVERRIDE=gf /usr/bin/steam -no-cef-sandbox
-cef-force-64bit`，而 `~/.local/share/applications/steam.desktop` 只修了 GBM。
菜单 [12] 现在统一修**用户级菜单项 + XDG 桌面图标 + 历史 `~/Desktop` 入口**，
写入 `GBM_BACKEND=dri` / `LIBGL_ALWAYS_SOFTWARE=1` /
`__GLX_VENDOR_LIBRARY_NAME=mesa` / `-cef-disable-gpu`、剔除旧 gf 注入、
补 `metadata::trusted`，并只改 Exec 行保留用户其它定制。
`/usr/share/applications/steam.desktop` 属于系统包、保持不动；XDG 规范下
`~/.local/share/applications/steam.desktop` 同名优先，菜单/概览启动会走修好的那份。

### 3.2 启动弹「OpenGL GLX context is not using direct rendering」（2026-11-13）

**现象**：Steam 客户端启动弹 KB `9938-EYZB-7457` 文案；同刻
`console-linux.txt` 有 `glXCreateContext failed`、
`X Error ... Major opcode 149 (GLX) / X_GLXCreateContext`、
`CVRStreamThread: Couldn't create context`，紧接 `Using host zenity for message`。
宿主 `glxinfo -B` 仍是 `direct rendering: Yes` / LISUAN 7G100 / OpenGL 4.6——
硬件 GLX 正常，失败的是被三项 workaround 限制、又没指定 GLX vendor 的客户端。

**根因**：GLVND 在 pressure-vessel 容器内没把客户端 GLX 落到可用 vendor，
建不出上下文 → `glXIsDirect` 判否 → Steam 自检弹窗。
`LIBGL_ALWAYS_SOFTWARE=1` 只决定用软件渲染，不解决 vendor 选择。

**消融**（单实例、单次启动、显式记录，不做自动重启/守护）：

| 组 | 变化 | 弹窗 | `glXCreateContext failed` / GLX BadValue | 结果 |
| --- | --- | --- | --- | --- |
| A0r | 基线三项 | 1 | 7 / 5 | 界面正常，弹窗复现 |
| A1r | + `__GLX_VENDOR_LIBRARY_NAME=mesa` | 0 | 0 / 0 | 弹窗消失，无新增失败 |
| A4 | mesa GLX，去掉 `LIBGL_ALWAYS_SOFTWARE` | 0 | 0 / 0 | 同样消弹窗，系统信息安全性未复验 |
| A2 | + `MESA_LOADER_DRIVER_OVERRIDE=llvmpipe` | 1 | 7 / 5 | 无效，与 §3.1 对照一致 |
| B1 | 去掉 `LIBGL_ALWAYS_SOFTWARE` | 1 | 2 / 3 | 弹窗仍在，登录窗口波动 |
| B2 | 去掉 `-cef-disable-gpu` | 1 | 7 / 5 | GPU 初始化失败 14→66，界面走加载动画 |
| B3 | 去掉 `GBM_BACKEND=dri` | 0 | 0 / 2 | `alive=0`，客户端直接退出 |
| B4 | 三项全无（裸启动） | 0 | 0 / 0 | `alive=0`，客户端直接退出 |

**结论**：只有 `__GLX_VENDOR_LIBRARY_NAME=mesa` 能消弹窗且无新增失败：
A1r / A4 / A1r2 / 最终恢复启动共 4 次弹窗 0、`glXCreateContext failed` 0、
GLX BadValue 0、`double free` 0、CEF `exit_code=139` 0，
`Exiting GPU process ...` 与基线同为 14。A4 说明 `LIBGL_ALWAYS_SOFTWARE`
不是消弹窗的必要条件，但它仍负责系统信息弹窗的 double free 防护（§3.1），
**不得移除**。B3/B4 的「弹窗 0」是客户端直接退出，不是修好。

**禁忌**：`__GLX_VENDOR_LIBRARY_NAME=gf`（§2，客户端崩溃）、
`LD_PRELOAD=libglapi_gf.so.0`、改 `steamwebhelper_sniper_wrap.sh` / `chattr +i`、
改 `VK_ICD_FILENAMES`（连累游戏 Vulkan）、
`MESA_LOADER_DRIVER_OVERRIDE=llvmpipe`（A2 复测弹窗仍在）。

**限制**：反复重启期间登录态波动（几次停在「登录 Steam」窗口），
「帮助 → 系统信息」未能在稳定登录态下复验，需登录后点一次确认；早期 A0/A1
两轮因 `steam://open/systeminfo` 触发窗口与额外进程作废，只采用 A0r 起的
干净口径。证据：`/tmp/steam-glx-ab/results.tsv` 及同目录各组
`*.console.log` / `*.windows.log` / `*.xwd`（临时目录，未入库）。

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
| Steam UI 花屏 / GPU 进程崩 (`exit_code=139`) | CEF gbm（§3），补齐 §3 现行四项命令 |
| 启动弹 `OpenGL GLX context is not using direct rendering` | 缺 `__GLX_VENDOR_LIBRARY_NAME=mesa`（§3.2，菜单 [12]）；若宿主 `glxinfo` 也是间接渲染才去查驱动 |
| 点「帮助 → 系统信息」闪退 / `double free` / `_XAllocID` | 补 `LIBGL_ALWAYS_SOFTWARE=1` + `-cef-disable-gpu`，清掉旧 gf 注入（§3.1，菜单 [12]） |
| Steam 界面一直停在加载动画 | 只加软件 GL 时 CEF GPU 初始化失败，必须再加 `-cef-disable-gpu`（§3） |
| `steamwebhelper` 无限重启（`startcount` 递增） | 同上（§3），四项缺一不可 |
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
