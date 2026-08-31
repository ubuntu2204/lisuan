# 砺算 7G100 GPU · Ubuntu 24.04 修复与适配

> 砺算（Lisuan）7G100 GPU（PCI `4c54:5006`）在 **Ubuntu 24.04 LTS** 上的一系列
> 修复与适配方案，覆盖三个层面：
>
> 1. **内核层**：DKMS 驱动在内核升级后编译/加载失败（`vermagic` 错配）——一行补丁修复；
> 2. **用户态图形层**：Mesa DRI 驱动名不一致（`lisuan` vs `gf`）、与系统 Mesa 25.2.x
>    的兼容性（厂商 Mesa 22.0.4 分叉无法被新 Mesa 加载，zink 为可用替代路径）；
> 3. **应用层**：Steam（CEF/pressure-vessel 容器）GPU 进程崩溃导致界面无法启动——
>    已定位双层根因并提供稳定规避方案。
>
> 目标环境：Ubuntu 24.04.4 LTS、HWE 内核 `7.0.0-28-generic`、海光 CPU + 砺算 7G100、
> 厂商包 `lisuangpu-dkms` / `lisuangpu-lib` 262.2.0.56。

[![License](https://img.shields.io/badge/License-Apache_2.0-blue.svg)](./LICENSE)
![Platform](https://img.shields.io/badge/platform-Hygon%20%2B%20Lisuan%207G100-orange)
![OS](https://img.shields.io/badge/Ubuntu-24.04.4%20LTS-E95420)

---

## English (TL;DR)

A set of independent, field-verified fixes for running the **Lisuan (砺算) 7G100**
GPU (PCI `4c54:5006`) on **Ubuntu 24.04 LTS** with vendor driver packages
`lisuangpu-dkms` / `lisuangpu-lib` 262.2.0.56. Three problem layers are covered:

1. **Kernel (DKMS)** — after a kernel upgrade the module is built against the
   *running* kernel but filed under the *target* kernel tree (the vendor
   `Makefile` reads `KERNEL_VERSION` while `dkms.conf` passes `KERNELVER`).
   Wrong `vermagic`/symbol CRCs → module refuses to load → black screen.
   Fixed by a one-line Makefile patch (`patches/`) plus an idempotent repair
   script; no reverse-engineering involved.
2. **Userspace graphics (Mesa)** — the DRM driver registers as `lisuan` while
   the userspace driver is named `gf`, so Mesa finds no DRI driver
   (`driver=(null)`); fixed with symlinks. A source-level analysis of the
   vendor Mesa 22.0.4 fork vs system Mesa 25.2.x is in `mesa/system-mesa-analysis.md` (zink is the
   viable fallback path).
3. **Applications (Steam)** — Steam's CEF crashes in its pressure-vessel
   container (two root causes: vendor `gf_gbm.so` segfaults under modern
   libgbm; Steam's bundled ANGLE fails lazy symbol resolution). Fixed by an
   injected, `chattr +i`-locked environment in `steamwebhelper_sniper_wrap.sh`,
   with a boot-time auto-repair systemd service. Native Linux games (verified:
   Dota Underlords) run via a documented host-native launch path.

**This repo does not contain the vendor driver** — only independent patches,
scripts, documentation and tests, under Apache-2.0. It also includes a
fully-evidenced vendor bug report (`dkms/vendor-bug-report-gf-gbm-crash.md`)
covering the three container/ABI issues, intended for Lisuan's driver team.

Quick entry point (interactive: environment detection + numbered menu):

```bash
./lisuan-fix.sh
```

No reverse-engineering is required for the kernel fix: the single closed-source
blob only depends on stable kernel symbols (`__fentry__`, `sscanf`); everything
else is the open-source glue layer that simply needs recompiling against the
right headers.

---

## 目录

- [仓库结构](#仓库结构)
- [快速开始](#快速开始)
- [一、内核 DKMS 驱动修复](#一内核-dkms-驱动修复)
- [二、Mesa / 图形栈适配](#二mesa--图形栈适配)
- [三、Steam 适配](#三steam-适配)
- [四、Electron 类应用适配（ZCode / CodeBuddy / Qoder）](#四electron-类应用适配zcode--codebuddy--qoder)
  - [4.0 ZCode（软件渲染稳定优先）](#40-zcode软件渲染稳定优先)
  - [4.1 通用 Electron 硬件加速 wrapper（CodeBuddy / Qoder 共用）](#41-通用-electron-硬件加速-wrappercodebuddy--qoder-共用)
  - [4.2 CodeBuddy 国内版](#42-codebuddy-国内版)
  - [4.3 Qoder 国内版](#43-qoder-国内版)
- [五、游戏修复文档](#五游戏修复文档)
  - [6.1 刀塔霸业（Dota Underlords）修复](#61-刀塔霸业dota-underlords-修复)
  - [6.2 随从大师（Minion Masters）修复](#62-随从大师minion-masters-修复)
- [Steam 平台适配细节（容器 / CEF / 登录 / Proton）](#steam-平台适配细节容器--cef--登录--proton)
- [六、踩坑经验总览与砺算技术要点](#六踩坑经验总览与砺算技术要点)
  - [7.1 砺算 7G100 技术要点（必读）](#71-砺算-7g100-技术要点必读)
  - [7.2 踩坑清单（按层分类）](#72-踩坑清单按层分类)
  - [7.3 环境/网络类坑](#73-环境网络类坑)
  - [7.4 排查方法论](#74-排查方法论)
- [关于厂商驱动的获取](#关于厂商驱动的获取)
- [自动化测试](#自动化测试)
- [常见问题 FAQ](#常见问题-faq)
- [许可证与免责声明](#许可证与免责声明)

---

## 仓库结构

按**主题**分为 6 个目录（每个目录内自带脚本与文档，不再区分 `scripts/` `docs/`）：

| 目录 | 主题 | 内容 |
|---|---|---|
| `steam/` | Steam 平台 | 容器 CEF 崩溃修复、开机自愈、容器架构说明 |
| `minion-masters/` | 随从大师 | 7G100 硬件渲染、Vivox/Texture patch、低 4GB 映射 shim、DXVK 2.3。其中 `gf-compat/` 是 Vulkan 兼容层（源码与运行时产物同处该目录，两个厂商专有 `.so` 不入库、获取方式见其 README），`assets-mm-gf-vulkan/` 是 C# 补丁工程（MMPatch 唯一权威源码所在；原开发工作目录 `mm-patch/` 已于 2026-08-31 删除，git 历史可查） |
| `underlords/` | 刀塔霸业 | 容器委托宿主直跑、libpng12 补齐、崩溃智能重试 |
| `electron/` | Electron 类应用 | 通用加速 wrapper（`use-electron-gf.sh`）+ ZCode / CodeBuddy / Qoder 三个应用入口。注意事项（gf 不支持的扩展、显示异常三件套等）见 [`electron/README.md`](./electron/README.md) |
| `dkms/` | DKMS 内核驱动 | 驱动构建/装载修复与卸载、体检、DKMS Makefile 补丁、厂商源码与 bug 报告 |
| `mesa/` | Mesa 图形栈 | 驱动名修复（`lisuan` vs `gf`）、系统 Mesa 兼容、切换与体检、源码级分析 |

其余：`tests/`（离线测试）、`_work/`（本机诊断草稿，**非交付物**）、`lisuan-fix.sh`（交互式索引）。

> `electron/` 内部分工：`use-electron-gf.sh` 是**通用基座**（通用 Electron/Chromium 修复
> + gf 硬件加速），三个应用 wrapper（`use-zcode.sh` / `use-codebuddy.sh` / `use-qoder.sh`）
> 均通过 `exec "$DIR/use-electron-gf.sh" --app <name>` 转调它。同目录引用，无跨目录耦合。

**修复类（fix）**——在目标机器上执行，会修改系统状态（均幂等）：

| 路径 | 说明 |
|---|---|
| `dkms/patches/0001-dkms-makefile-honor-KERNELVER.patch` | 厂商 DKMS Makefile 的 `KERNEL_VERSION`/`KERNELVER` 变量名错配修复（一行补丁） |
| `dkms/fix-lisuan-gpu.sh` | 一键应用补丁并重编/重装/加载 `gf.ko`（幂等、自动备份） |
| `mesa/fix-lisuan-mesa-driver-name.sh` | 修复 DRM 名与用户态驱动名不一致（`lisuan` vs `gf`）导致的 DRI 加载失败 |
| `steam/fix-steam-cef-lock.sh` | **Steam 修复主入口**：向容器内注入 `GBM_BACKEND=dri` + `LD_PRELOAD` glapi 并 `chattr +i` 锁定（详见[三](#三steam-适配)） |
| `underlords/fix-underlords-native.sh` | **Dota Underlords 修复安装器**（幂等）：容器委托宿主直跑 + 智能重试/退出检测 + libpng12 补齐 + 桌面项；`--status` 查状态、`--uninstall` 卸载、`--no-autorestart` 彻底关闭自动重启（游戏中途崩溃也直接退出，菜单 14/15） |
| `steam/steam-cef-autofix.sh` | 开机自愈（root/systemd 调用）：发现 Steam 更新还原了补丁就自动重新注入锁定，由 `./lisuan-fix.sh` 菜单 9 安装为系统服务；`--uninstall` 卸载服务 |
| `dkms/uninstall-lisuan-gpu.sh` | 卸载：撤销 DKMS Makefile 补丁（优先从 `.bak` 还原，不影响已装驱动） |
| `mesa/uninstall-lisuan-mesa-driver-name.sh` | 卸载：删除 `lisuan_*.so` 软链（只删指向 `gf_*` 的软链，不碰真实文件） |

**体检类（verify）**——只读诊断，不修改任何状态，需要有砺算显卡的真机：

| 路径 | 说明 |
|---|---|
| `dkms/verify-lisuan-gpu.sh` | 驱动体检：lspci/lsmod/modinfo/DRM 节点 |
| `mesa/verify-system-mesa.sh` | 系统 Mesa 兼容性体检（zink 回退、GBM dri 后端等） |

**工具类（use）**——按需包装单个应用的环境：

| 路径 | 说明 |
|---|---|
| `mesa/use-system-mesa.sh` | 让指定应用运行在系统 Mesa（zink）之上，绕开厂商 Mesa 22.0.4 分叉 |
| `electron/use-zcode.sh` | **ZCode（Electron AppImage）显示异常修复**：`--no-sandbox` + `--ozone-platform-hint=x11` + 禁用 GPU 合成；`--install-desktop` 把桌面项切到本 wrapper |
| `electron/use-electron-gf.sh` | **通用 Electron / Chromium 应用 gf 硬件加速 wrapper**：显示异常三件套 + gf 驱动 GPU 加速（默认开），`--no-hw` 回退软件渲染；支持 `--app codebuddy\|qoder\|zcode` / `--cmd` / `--appimage` |
| `electron/use-codebuddy.sh` | **CodeBuddy 国内版**加速启动 wrapper（复用上面通用脚本，默认 gf 加速） |
| `electron/use-qoder.sh` | **Qoder 国内版**加速启动 wrapper（复用上面通用脚本，默认 gf 加速） |

**文档（docs）**：

| 路径 | 说明 |
|---|---|
| `mesa/system-mesa-analysis.md` | 7G100 与系统 Mesa 25.2.x 的源码级兼容性分析（为什么原生加载不可行、zink 路径、已知地雷） |
| `dkms/vendor-bug-report-gf-gbm-crash.md` | 提交厂商的 bug 报告（gf_gbm 段错误、容器内 EGL 分发、GLX vendor 注册缺失三个问题，附完整证据链） |
| `underlords/fix-underlords-native.md` | **刀塔霸业（Dota Underlords）修复文档**：Steam 容器内崩溃根因、容器委托宿主直跑、libpng12 补齐、panorama 随机段错误智能重试与退出检测 |
| `minion-masters/fix-minion-masters.md` | **随从大师（Minion Masters）修复文档**：Vivox 登录崩溃与 Texture 异常的 BepInEx 运行时 patch 三件套、证书 patch 回归（黑屏 Zenject 循环）教训、游戏后端 API 404、Proton 选型（须支持 new WoW64，Proton 8.0 不可用于硬件渲染；**推荐 Proton 11.0 稳定版**，更新频率低于 Experimental）、user_settings.py 注入无效教训、Steam 状态卡死坑 |
| `dkms/vendor-src/` | 厂商 DKMS 内核源码原样转录（GPLv2/Dual MIT/GPLv2 授权部分，69 个 .c + 99 个 .h；闭源 blob 不在其中，见其 README） |

> 注意区分：`scripts/` 下的 verify-* 是**面向真机的体检工具**，与仓库根的
> `tests/`（离线、无硬件依赖的 CI 自检）不是一回事。

其余顶层内容：

| 路径 | 说明 |
|---|---|
| `CHANGELOG.md` | 更新日志：每个版本的新增/修复/实测更正，均实机验证 |
| `lisuan_fix.py` | **交互式修复索引（推荐入口）**：Python 版，检测环境并给出结论，编号菜单执行单项修复/体检。装了 Textual 则为 TUI 界面，否则自动降级为纯文本菜单 |
| `tui.py` | Textual TUI 实现（可选增强，被 `lisuan_fix.py` 自动加载） |
| `pyrightconfig.json` | basedpyright 配置：把 `--user` 安装的 `textual` 加进搜索路径。缺它时 `tui.py` 会报 `reportMissingImports` 并级联出几十条 Unknown 类型警告。换机器后路径见文件内注释 |
| `lisuan-fix.sh` | 旧版 bash 交互索引，功能等价，作为 Python 不可用时的回退保留 |
| `tests/` | 离线自动化测试（补丁、脚本幂等性、语法、仓库合规），检验对象为各主题目录（`steam/` `minion-masters/` `underlords/` `electron/` `dkms/` `mesa/` 等） |
| `.workflow/gitee-go-test.yml` | Gitee Go 流水线：push/PR 时自动跑 `tests/` |
| `_work/` | 本地诊断草稿区（构建日志、探针程序、解包的厂商 DKMS 源码树、Mesa 官方源码等）。仅本机排查用，**不是交付物**；其中的厂商文件不得再分发 |
| `lisuangpu-*.deb` | 厂商驱动包原件（已被 `.gitignore` 排除，不入库），仅本机留存 |

## 快速开始

不熟悉各脚本的话，直接用交互式索引（自动检测环境 + 编号菜单）：

```bash
python3 lisuan_fix.py              # 交互界面（有 Textual 用 TUI，否则纯文本菜单）
python3 lisuan_fix.py --detect     # 只做环境检测并打印报告
python3 lisuan_fix.py --detect --json   # JSON 输出，便于脚本化/排障
python3 lisuan_fix.py 2            # 直接执行第 2 项（修复 DKMS）
```

菜单项编号与旧版 `lisuan-fix.sh` **完全一致**，本文件各处提到的"菜单 [N]"对两个版本通用。

**启用 TUI**（可选，需联网）：

```bash
python3 -m pip install --user --break-system-packages textual
```

未安装时自动降级为纯文本菜单，功能不变。

> 注：Ubuntu 仓库里的 `python3-textual` 是 0.1.13（2021 年版本），API 与现在不兼容，
> 请用 pip 安装。若提示 PEP 668，加 `--break-system-packages`（只装到用户目录，不碰系统 Python）。

或手动执行单项：

```bash
# 1) 内核升级后驱动加载失败（vermagic 错配 / 黑屏）
sudo ./fix-lisuan-gpu.sh
./dkms/verify-lisuan-gpu.sh

# 2) GL/EGL 找不到驱动（driver=(null)）、DRI 加载失败
sudo ./mesa/fix-lisuan-mesa-driver-name.sh

# 3) Steam 界面起不来（cef_log.txt 大量 exit_code=139/11）
sudo ./mesa/fix-lisuan-mesa-driver-name.sh
sudo ./steam/fix-steam-cef-lock.sh
MESA_LOADER_DRIVER_OVERRIDE=gf steam -no-cef-sandbox -cef-force-64bit

# 4) 想让某个应用走系统 Mesa（25.2.x，zink）而非厂商 Mesa 22.0.4
./mesa/use-system-mesa.sh --print-env
./mesa/use-system-mesa.sh glxinfo -B
./mesa/verify-system-mesa.sh

# 5) Electron 类应用 (ZCode) 显示异常: 界面空白 / 花屏 / 黑块
./electron/use-zcode.sh                       # 启动 (含 --no-sandbox 等修复)
./electron/use-zcode.sh --install-desktop     # 把应用菜单的 Exec 切到本 wrapper

# 5b) Electron 类应用 gf 硬件加速 (CodeBuddy / Qoder 国内版等)
./electron/use-codebuddy.sh                   # CodeBuddy 国内版 (gf 加速, 默认)
./electron/use-codebuddy.sh --no-hw           # 回退软件渲染(白屏/花屏时)
./electron/use-codebuddy.sh --install-desktop # 应用菜单直启即带加速
./electron/use-qoder.sh                        # Qoder 国内版 (gf 加速, 默认)
./electron/use-qoder.sh --no-hw                # 回退软件渲染
./electron/use-qoder.sh --install-desktop      # 应用菜单直启即带加速

```

---

## 一、内核 DKMS 驱动修复

### 适用场景

只要满足以下**任一**特征，本仓库的分析与修复即适用：

- 砺算 GPU（PCI 厂商号 `4c54`，即 ASCII `"LT"`）在**升级内核后黑屏 / 无显示 / 掉驱动**；
- `dkms status` 显示模块 `installed`，但 `lsmod` 里没有 `gf`，显卡也没有驱动绑定；
- `dmesg` 出现大量 `gf: disagrees about version of symbol ...` 或 `gf: Unknown symbol ...`；
- `modinfo gf.ko` 显示的 `vermagic` 与当前 `uname -r` **不一致**。

> 该类"DKMS 变量名不匹配"缺陷在部分国产信创 GPU 驱动中都可能出现，排查思路通用。

### 验证环境

| 项目 | 值 |
| --- | --- |
| CPU 平台 | 海光 Hygon（信创平台） |
| GPU | 砺算 Lisuan **7G100**，PCI `03:00.0` `[4c54:5006]`（子系统 `4c54:5000`） |
| 操作系统 | Ubuntu 24.04.4 LTS (noble) |
| 内核 | `7.0.0-28-generic`（HWE） |
| 编译器 | gcc 13.3.0（与内核构建工具链一致） |
| 驱动包 | `lisuangpu-dkms` / `lisuangpu-lib` 版本 `262.2.0.56` |
| 内核模块 | 文件名 `gf.ko`；DRM 驱动注册名为 `lisuan` |
| Secure Boot | Disabled（如启用需要额外签名，见 FAQ） |

### 问题现象

内核日志（`dmesg` / `journalctl -k`）中可见符号版本与未知符号错误：

```text
gf: disagrees about version of symbol drm_crtc_vblank_on
gf: Unknown symbol drm_crtc_vblank_on (err -22)      # -22 EINVAL: 符号 CRC 不匹配
gf: Unknown symbol kmalloc_large (err -2)            # -2 ENOENT: 新内核中该符号已改名/移除
gf: Unknown symbol kmalloc_trace (err -2)
...
```

`modinfo` 暴露出关键矛盾——模块被放进 `7.0.0-28` 目录，`vermagic` 却是 `6.8.0-31`：

```text
filename: /lib/modules/7.0.0-28-generic/updates/dkms/gf.ko
vermagic: 6.8.0-31-generic SMP preempt mod_unload modversions   # ← 内核版本错配！
```

同时 `lspci -nnk -s 03:00.0` 没有 `Kernel driver in use:` 一行，即**显卡未被任何驱动接管**。

### 根因分析

厂商 DKMS 源码 `dkms.conf` 通过如下方式把**目标内核**传给 Make：

```make
MAKE[0]="'make' KERNELVER=$kernelver"
```

但 `Makefile` 却用了**另一个变量名** `KERNEL_VERSION` 来决定编译目标：

```make
ifndef KERNEL_VERSION
KERNEL_VERSION := $(shell uname -r)     # ← 取"当前运行内核"，忽略了 DKMS 传入的 KERNELVER
endif
LINUXDIR ?= /lib/modules/$(KERNEL_VERSION)/build
```

两个变量名对不上，导致 **DKMS 指定的目标内核被无视**：模块总是按"编译当时正在运行的
内核"来构建。于是在"运行着旧内核（6.8）时安装了新内核（7.0）"这一 `apt upgrade`
常见场景下，DKMS 用 6.8 头文件编出模块，却归档到 7.0 的模块树里 —— 版本魔数与符号
CRC 全部错配，模块被内核拒绝加载，显卡因此没有驱动。

**修复**：让 `Makefile` 认 `KERNELVER`（DKMS 的约定变量），仅在手动 `make` 时回退到
`uname -r`。改动仅一行，见 [`dkms/patches/0001-dkms-makefile-honor-KERNELVER.patch`](./dkms/patches/0001-dkms-makefile-honor-KERNELVER.patch)：

```diff
 ifndef KERNEL_VERSION
-KERNEL_VERSION := $(shell uname -r)
+KERNEL_VERSION := $(if $(KERNELVER),$(KERNELVER),$(shell uname -r))
 endif
```

### 为什么不需要反编译

驱动包里唯一的闭源二进制 `objs/x86_64/lisuan_proprietary.o.hex` 只是两个预编译对象
（`gf_hw_misc.o`、`gf_hw_cmds.o`）的 `ar` 打包，它对外仅依赖 **20 个符号**，其中属于
内核的只有 `__fentry__` 和 `sscanf`——这些跨内核版本**完全稳定**。其余符号（`gf_*`、
`lisuan_*`、`disp_*` 等）全部由**随包附带的开源胶水层**提供。

因此，用正确的内核头文件**重新编译开源部分**即可解决，无需对闭源 blob 做任何逆向。
实测用 `7.0.0-28` 头文件重编：**0 错误**，产物 `vermagic` 正确变为 `7.0.0-28-generic`。

### 快速修复（推荐）

前提：已安装厂商驱动包 `lisuangpu-dkms`（见[获取说明](#关于厂商驱动的获取)）、
对应内核头文件与 `dkms`。

```bash
# 修复当前运行内核
sudo ./dkms/fix-lisuan-gpu.sh

# 或：为指定内核构建（例如装了新内核但尚未重启进入）
sudo ./dkms/fix-lisuan-gpu.sh 7.0.0-28-generic
```

脚本会：① 幂等地给 `/usr/src/lisuangpu-dkms-*/Makefile` 打补丁（保留 `.bak`）→
② `dkms remove/build/install` 针对目标内核重编重装 → ③ `depmod` + `modprobe gf` →
④ 打印验证报告。随后可运行只读体检脚本 `dkms/verify-lisuan-gpu.sh`。

### 手动修复步骤

```bash
# 0) 准备：安装当前内核头文件与 dkms（按需）
sudo apt-get install -y dkms "linux-headers-$(uname -r)"

# 1) 打补丁（从 DKMS 源码根目录）
cd /usr/src/lisuangpu-dkms-262.2.0.56          # 版本号以实际为准
sudo cp -a Makefile Makefile.bak
sudo patch -p1 < /path/to/dkms/patches/0001-dkms-makefile-honor-KERNELVER.patch

# 2) 针对当前内核重编重装
sudo dkms remove  -m lisuangpu-dkms -v 262.2.0.56 -k "$(uname -r)" || true
sudo dkms build   -m lisuangpu-dkms -v 262.2.0.56 -k "$(uname -r)"
sudo dkms install -m lisuangpu-dkms -v 262.2.0.56 -k "$(uname -r)" --force

# 3) 刷新依赖并加载
sudo depmod -a "$(uname -r)"
sudo modprobe gf

# 4) 验证 vermagic 与运行内核一致
modinfo "$(find /lib/modules/$(uname -r) -name 'gf.ko*' | head -1)" | grep vermagic
```

### 验证结果

修复成功后应观察到（本环境实测）：

```text
# lsmod
gf                   1081344  24

# lspci -nnk -s 03:00.0
03:00.0 VGA compatible controller [0300]: Device [4c54:5006]
        Kernel driver in use: lisuan          # ← 显卡已被 lisuan 驱动接管

# ls -l /dev/dri/
crw-rw----+ 1 root video  226,   1 card1
crw-rw----+ 1 root render 226, 128 renderD128

# modinfo .../gf.ko.zst
vermagic: 7.0.0-28-generic SMP preempt mod_unload modversions   # ← 与运行内核一致
```

建议随后重启一次，让显示栈以正确驱动干净启动。

### 未来内核升级

补丁写入 `/usr/src/lisuangpu-dkms-*/Makefile`，之后每次 `apt` 升级内核触发的
`dkms autoinstall` 都会针对**正确的目标内核**构建，问题不再复发。
注意：**重装/升级厂商 `.deb` 会覆盖 `/usr/src` 下的 Makefile**，需再次执行
`fix-lisuan-gpu.sh`（幂等，可安全重复运行）。

### 卸载 / 撤销补丁

> 撤销补丁 ≠ 卸载显卡驱动。撤销后已装好的 `gf.ko` 仍在使用；真正的影响是
> **下次内核升级会退回原厂 Bug 行为**。除非确有需要，一般不建议撤销。

每一项注入都有对应的卸载入口（均幂等，也都在 `./lisuan-fix.sh` 菜单里）：

| 注入 | 卸载方式 |
|---|---|
| DKMS Makefile 补丁 | `sudo dkms/uninstall-lisuan-gpu.sh`（优先从 `.bak` 还原，无备份时反向替换并留 `.uninstall.bak`；菜单 11） |
| DRI 驱动名软链 | `sudo mesa/uninstall-lisuan-mesa-driver-name.sh`（只删指向 `gf_*` 的软链；菜单 12） |
| Steam 容器补丁 | `sudo steam/fix-steam-cef-lock.sh --uninstall`（还原 wrap.sh 并解锁；`--unlock` 只解锁不还原；菜单 13） |
| 开机自愈服务 | `sudo steam/steam-cef-autofix.sh --uninstall`（菜单 10） |

验证 DKMS 补丁已还原（第 4 行应变回原厂行）：

```bash
sed -n '4p' "$(ls -d /usr/src/lisuangpu-dkms-* | sort -V | tail -1)/Makefile"
# 预期：KERNEL_VERSION := $(shell uname -r)
```

> 若要彻底卸载整个砺算驱动（显卡将失去驱动，请谨慎）：
> `sudo modprobe -r gf && sudo apt-get remove lisuangpu-dkms lisuangpu-lib`。

---

## 二、Mesa / 图形栈适配

### 驱动名不一致（`lisuan` vs `gf`）

内核 DRM 驱动注册名为 `lisuan`，而厂商用户态驱动叫 `gf_dri.so`。Mesa 的 DRI 加载器
按 `drmGetVersion()` 返回的名字找驱动 → 找 `lisuan_dri.so` → 找不到 → `driver=(null)`。

修复：`sudo ./mesa/fix-lisuan-mesa-driver-name.sh`
创建 `lisuan_dri.so → gf_dri.so` 等符号链接（幂等；重装厂商 lib 包后建议重跑）。

### 与系统 Mesa 25.2.x 的关系（要点）

完整源码级分析见 [`mesa/system-mesa-analysis.md`](./mesa/system-mesa-analysis.md)：

- 厂商 GL 栈是**自包含的 Mesa 22.0.4 分叉**（`libGLX_gf`/`libEGL_gf` + `gf_dri.so`
  megadriver），系统 Mesa 25.2.x **架构上无法加载它**（无 megadriver 加载机制、
  pipe 描述符静态表无 `gf`）；
- 可用的系统 Mesa 路径是 **zink**（GL-on-Vulkan，架在厂商 Vulkan ICD
  `libvulkan_gf.so` 之上）：GLX 自动回退可用（GL 3.0 上限）；
- **地雷**：厂商 `gf_gbm.so` 在系统 libgbm 25.x 下段错误（ABI 漂移）——任何走
  系统 libgbm 默认后端解析的场景需 `GBM_BACKEND=dri` 规避；
- 想让某应用走系统 Mesa：`./mesa/use-system-mesa.sh -- <cmd>`。

---

## 三、Steam 适配

**症状**：Steam 界面无法启动，`~/.local/share/Steam/logs/cef_log.txt` 大量
`GPU process exited unexpectedly: exit_code=139`（或 11），最终
`FATAL: GPU process isn't usable. Goodbye.`。桌面 GL（glxinfo）完全正常——
**与系统 Mesa 版本高低无关**。

### 根因（双层，均有实证）

1. **`gf_gbm.so` 段错误**：Steam 界面（CEF）运行在 pressure-vessel 容器内，容器用
   系统 libgbm，其按 DRM 名 `gf` 加载厂商 `gf_gbm.so`（22.0.4 分叉）→ 与新 libgbm
   ABI 不兼容 → 跳转空指针 SIGSEGV。证据：core dump 的 gdb 回溯帧全部落在
   `gf_gbm.so` 地址段。
2. **Steam 自带 ANGLE 符号劫持**：修好 GBM 后，Steam 自带的
   `ubuntu12_64/libEGL.so`（ANGLE）被 `LD_LIBRARY_PATH=.` 优先加载，其延迟绑定的
   GL 符号在容器内找不到 glapi 提供者 → `symbol lookup error (fatal)` → SIGSEGV。
   证据：`LD_DEBUG=libs` 追踪到 GPU 进程崩溃前的完整加载链。

### 修复（已实测稳定）

```bash
sudo ./mesa/fix-lisuan-mesa-driver-name.sh   # 先修驱动名
sudo ./steam/fix-steam-cef-lock.sh            # 注入并 chattr +i 锁定
MESA_LOADER_DRIVER_OVERRIDE=gf steam -no-cef-sandbox -cef-force-64bit
```

`fix-steam-cef-lock.sh` 向 `steamwebhelper_sniper_wrap.sh` 注入：

```bash
export GBM_BACKEND=dri                                        # 绕开 gf_gbm.so 段错误
export LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libglapi_gf.so.0  # 修 ANGLE 符号解析崩溃
```

效果：GPU 进程不再崩溃；它尝试硬件加速失败后（容器内 glvnd 的 EGL 分发
拿不到厂商 EGL，见
[`dkms/vendor-bug-report-gf-gbm-crash.md`](./dkms/vendor-bug-report-gf-gbm-crash.md) §3）
自动回退软渲染，Steam 界面正常。注意：容器内的原生 GLX 游戏仍无法启动
（见[游戏兼容性](#游戏兼容性dota-underlords-实测)与报告 §4）。

### 注意事项

- **日常开机无需任何操作**：补丁写入 `wrap.sh` 并 `chattr +i` 锁定，重启不丢；
  游戏用桌面菜单图标启动。
- 建议安装开机自愈：`./lisuan-fix.sh` 选 **9**（或
  `sudo steam/steam-cef-autofix.sh` 手动验证）。
  之后 Steam 客户端更新还原补丁时，下次开机自动重新注入，菜单 5/6 不再需要。
- 不装自愈服务时：Steam 客户端更新前先 `sudo ./steam/fix-steam-cef-lock.sh --unlock`，
  更新后重跑注入。
- 手动编辑 wrap.sh 后务必 `chmod +x`（`sed -i` 会丢执行位，容器内 pv-adverb 会报
  Permission denied）。
- 三个问题的根因与修复建议已整理成厂商 bug 报告：
  [`dkms/vendor-bug-report-gf-gbm-crash.md`](./dkms/vendor-bug-report-gf-gbm-crash.md)。

### 游戏兼容性（Dota Underlords 实测）

**在 Steam 里点"开始游戏"即可直接玩**（已实测）。原理与细节：

- 容器内无法用厂商图形栈，根因有三层（均有实证）：
  1. 沙箱隐藏 `/sys/bus/pci` → 厂商 Vulkan ICD 枚举不到 GPU（0 设备）；
     新版 Vulkan loader 还会以 "loader interface version" 为由拒绝厂商 ICD；
  2. 容器内 glvnd 无法注册厂商 GLX vendor（无 vendor JSON 目录）；
  3. 厂商 EGL 在容器内拿不到 X11 visual。
- 规避方案：把游戏弹出沙箱、在宿主直跑（宿主图形栈完全正常）。
  实现：`underlords.sh` 检测到自己在容器内时，通过共享的会话 D-Bus 调用
  宿主 systemd（`systemd-run --user`）在宿主启动游戏，并阻塞到游戏退出，
  Steam 的运行时长/退出检测依然有效。相关文件（游戏目录下）：
  `ul-host-run.sh`（委托器）、`play-native.sh`（宿主启动器，含残留
  `LS_SHARE_MEMORY_*` 清理）、`underlords.sh`（容器分支 + 启动崩溃重试）。
  整套文件由 `underlords/fix-underlords-native.sh`（菜单 14）安装/修复/卸载，
  Steam 更新游戏后可重跑修复。
- 首次部署需要把 `libpng12.so.0` 放进游戏 `bin/linuxsteamrt64/` 目录
  （Ubuntu 24.04 已移除该库，可从 18.04 归档取），否则 Source 2 的
  pango 文本模块崩溃。
- **游戏中途随机崩溃（`libpanorama_text_pango.so` 固定偏移段错误）**：这是
  Source 2 panorama 文本模块自身的内存损坏（虚表调用失败），与显卡无关。
  实验：把游戏目录的 pango 换成宿主新版反而把崩溃变成**启动即崩**（确定性），
  已回滚到游戏自带 scout pango（备份在 `pango-scout-bak/`，勿删）。
  当前对策：`underlords.sh` 智能重试——段错误后自动清理残留 shm 并重启
  （最多 10 次；180 秒内连崩 3 次判定为崩溃循环，停止重试避免无限循环）。
  退出检测：退出时 panorama 析构也会段错误，脚本用日志中的
  `CSteam3Client::Shutdown` 标记区分"退出时崩"与"游戏中崩"，前者不重启
  （否则会出现"关一次游戏自动重开"的现象）。
  另外启动参数加了 `-nobreakpad`：游戏自带 breakpad 会吞掉 SIGSEGV，关掉后
  崩溃才会产生内核 core（`/tmp/core.underlords.*`，需 core_pattern 指向 /tmp），
  可供 gdb 定位。
- **彻底关闭自动重启**：若不想保留"游戏中途崩溃自动重开"（例如遇到退出误重开
  又不想区分），用 `sudo underlords/fix-underlords-native.sh --no-autorestart`
  重打补丁——游戏进程结束（正常退出或崩溃）即直接退出，不再拉起。副作用：
  panorama 随机段错误会让游戏**直接闪退不可玩**，按需选择（菜单外手动执行）。
- 已知限制：宿主直跑的游戏没有 Steam 游戏内覆盖（Shift+Tab）；被 `kill -9`
  强杀后残留的 `LS_SHARE_MEMORY_*` 会在下次启动时自动清理；段错误由
  `underlords.sh` 智能重试兜底（见上条，仅在默认自动重启开启时生效）。
- 备用：桌面项 `dota-underlords-native.desktop` 可不依赖 Steam 启动链路直接玩。

---

## 四、Electron 类应用适配（ZCode / CodeBuddy / Qoder）

> 同类问题也适用于其它 Electron / Chromium 系桌面应用（VSCode、网易云音乐、微信等）：
> 在国产 GPU + Ubuntu 24.04 上同样可能表现为"界面空白 / 花屏 / 黑块"。根因与修复
> 完全一致，本节的 `--no-sandbox` / `--ozone-platform-hint=x11` / `--disable-gpu`
> 三件套可直接套用。

> 本节已实测适配 **ZCode**（Foxit 国产 AI 编码助手，Electron AppImage）、
> **CodeBuddy 国内版** 与 **Qoder 国内版**（均为国产 AI 编码助手 Electron 应用）：
> ZCode 走 `use-zcode.sh`（软件渲染稳定优先）；CodeBuddy / Qoder 走 `use-electron-gf.sh`
> 通用 wrapper（默认启用 gf 驱动 GPU 硬件加速，详见 4.1–4.3）。

### 4.0 ZCode（软件渲染稳定优先）

**症状**：ZCode（福昕同系的国产 AI 编码助手，Electron AppImage，3.7.7.4926）启动后
窗口弹出但内容区空白（或拖拽/缩放时花屏、窗口残缺），无法使用。

### 根因（与 Steam / Foxit 同源的三层叠加）

1. **Chromium 沙箱剥离渲染进程**：Ubuntu 24.04 内核限制了 unprivileged user
   namespace，Electron 的渲染进程在默认沙箱里无法启动。ZCode 自带的 `AppRun` 原本
   带了 `--no-sandbox`，但很多用户/桌面环境实际走的是**另一份 `Exec`**（如应用菜单
   项直接指向 AppImage 本体、漏了 `--no-sandbox`）→ 渲染进程起不来 → 整窗白屏。
2. **Wayland 原生后端异常**：当前会话是 Wayland（`XDG_SESSION_TYPE=wayland`），
   Electron 的 wayland 后端在国产驱动上窗口渲染残缺 → 切回 XWayland 即可稳定。
3. **GPU 合成白屏**：砾算 G100 + Mesa 驱动 GL 特性不全，Chromium 的 GPU 合成层会
   白屏/花屏 → 禁用 GPU 与 GPU 合成，回退软渲染（与 Foxit WebEngine 修复同源）。

> 与 Foxit 的区别：Foxit 的 WebEngine 还有"环境变量到不了子进程需 `/opt` 软链兜底"
> 这一环；Electron 的渲染进程直接吃命令行，因此只需在启动时传入开关即可，无需改系统。

### 修复（已实测）

```bash
cd zcode
bash use-zcode.sh                       # 启动 ZCode (带完整修复开关)
bash use-zcode.sh --install-desktop     # 把系统 zcode.desktop 的 Exec 切到本 wrapper
```

`use-zcode.sh` 实际执行的开关（与 `use-system-mesa.sh` 同级，属"按需包装单应用环境"）：

```bash
"$APPIMAGE" \
  --no-sandbox \                 # 修 Ubuntu 24.04 沙箱导致的渲染进程起不来
  --ozone-platform-hint=x11 \    # 经 XWayland 渲染, 规避 wayland 原生后端残缺
  --disable-gpu --disable-gpu-compositing   # 规避国产驱动 GPU 合成白屏
  ${ZCODE_EXTRA_FLAGS:-}         # 可追加自定义开关, 如 ZCODE_EXTRA_FLAGS=--enable-logging
```

修复后从应用菜单 / 双击启动均正常（桌面项 `Exec` 已指向 wrapper）。

### 验证

```bash
ZCODE_EXTRA_FLAGS="--enable-logging=stderr --v=1" bash electron/use-zcode.sh
# 正常应看到 GPU 进程 fallback 到软件渲染, 无 "GPU process isn't usable" 致命错误
```

### 注意事项

- 若换回纯 X11 会话（`XDG_SESSION_TYPE=x11`），`--ozone-platform-hint=x11` 仍无害，
  可保留。
- 想试恢复硬件加速流畅度：`ZCODE_EXTRA_FLAGS="--ignore-gpu-blocklist" bash electron/use-zcode.sh`
  （若出现花屏/白屏，去掉该标志回退默认即可）。
- 本脚本只改"启动环境"，不触碰系统其它文件；撤销只需把桌面项 `Exec` 改回 AppImage
  本体，或删除 `use-zcode.sh`。

### 4.1 通用 Electron 硬件加速 wrapper（CodeBuddy / Qoder 共用）

`electron/use-electron-gf.sh` 是上一节 ZCode 修复思路的**通用化 + gf 硬件加速版**：
同一套"显示异常三件套"（`--no-sandbox` / `--ozone-platform-hint=x11` /
`--disable-gpu-compositing`）保证能开，另提供 **gf 驱动 GPU 加速**替代默认软件渲染，
让 AI 编码助手这类重 UI 应用更流畅。

**gf 加速原理**（与第三节方案 A 探明的机制一致）：

- 宿主 gf GLX 已验证可用（`GLX_VENDOR_LIBRARY_NAME=gf glxinfo` 可枚举
  `LISUAN 7G100 Series Graphics`）。
- 只给**本应用进程**注入 gf env（`__GLX_VENDOR_LIBRARY_NAME=gf` +
  `VK_ICD_FILENAMES` 指向 gf ICD）+ 让 Chromium 走桌面 GL
  （`--use-gl=angle --use-angle=gl`），由 gf 驱动承担渲染 / 栅格化。
- **关键**：env 只注入到 Electron 进程树，**不污染 Steam 等其它进程**（全局注入 gf
  会让 Steam 客户端 GLX 崩溃，见第三节）—— 本脚本天然只作用于单应用，安全。

**默认启用 gf 加速**；若个别版本出现白屏 / 花屏，加 `--no-hw` 一键回退软件渲染。

```bash
cd SCRIPTS

# CodeBuddy 国内版（未安装时先装好，或 CODEBUDDY_CMD 指定路径）
bash use-codebuddy.sh                      # gf 加速启动
bash use-codebuddy.sh --no-hw              # 白屏/花屏时回退软件渲染
bash use-codebuddy.sh --install-desktop    # 应用菜单 Exec 切到本 wrapper（从菜单直启即带加速）

# Qoder 国内版（已装: /usr/bin/qoder）
bash use-qoder.sh                          # gf 加速启动
bash use-qoder.sh --no-hw                  # 回退软件渲染
bash use-qoder.sh --install-desktop        # 桌面项改写到 ~/.local/share/applications（无需 sudo）

# 任意其它 Electron 应用
bash use-electron-gf.sh --cmd /path/to/electron-app
bash use-electron-gf.sh --appimage /path/to.appimage
```

> **硬件加速失效排查（`vkCreateInstance() failed: -7`）**
>
> 症状：`--hw` 模式下 Electron 报
> `vkCreateInstance() failed: -7`（`VK_ERROR_INCOMPATIBLE_DRIVER`），界面白屏/花屏。
>
> 根因（与随从大师 `MinionMasters-攻坚全记录.md` 显卡坑 15 同源）：
> gf 厂商**裁掉了**实例扩展 `VK_KHR_external_semaphore_capabilities` /
> `VK_KHR_external_fence_capabilities`，而新版 Vulkan loader 严格校验
> "请求的实例扩展必须由某个 ICD 暴露"；Electron/Chromium 会请求前者 → 直接被拒。
> 用 `VK_LOADER_DEBUG=error,warn` 可看到：
> `loader_validate_instance_extensions: Instance extension VK_KHR_external_semaphore_capabilities not supported`。
>
> 解法：使用**协商垫片** `minion-masters/gf-compat/gf_icd.json`（→ `libvulkan_gf_shim.so`），
> 它对外报协商版本 5 并在枚举时补报被裁扩展。脚本已**自动优先选用垫片**
> （存在时优先于原版 `gf_icd.x86_64.json`），外部 `VK_ICD_FILENAMES` 若已设置则不被覆盖。
>
> 另注：早期版本脚本会**无条件覆盖** `VK_ICD_FILENAMES` 为原版 ICD，
> 导致外部指定垫片无效——现已改为"外部优先 + 垫片次之 + 原版兜底"三级回退。

**加速模式实际开关**（`use-electron-gf.sh`，默认 `--hw`）：

```bash
# 软件渲染(稳定, 默认 --no-hw)
"$CMD" --no-sandbox --ozone-platform=x11 --ozone-platform-hint=x11 --disable-gpu-compositing --disable-gpu

# gf 硬件加速(默认)
__GLX_VENDOR_LIBRARY_NAME=gf LIBGL_ALWAYS_SOFTWARE=0 VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/gf_icd.x86_64.json \
"$CMD" --no-sandbox --ozone-platform=x11 --ozone-platform-hint=x11 --disable-gpu-compositing \
        --ignore-gpu-blocklist --enable-gpu-rasterization \
        --use-gl=angle --use-angle=gl --enable-features=Vulkan
```

> 注：**Electron 43（Qoder）会忽略 `--ozone-platform-hint=x11`**，应用仍走 Wayland 原生
> （子进程 `--ozone-platform=wayland`，拖拽/缩放窗口残缺），必须再加显式
> `--ozone-platform=x11` 强制 XWayland；两个并存以兼容新旧 Chromium（未知开关被安全忽略）。
> 判定方法：看 `gpu-process` 子进程命令行里的 `--ozone-platform=` 实际取值。
> 另外从另一个 Qoder 变体的内置终端启动时，需先清掉宿主注入的 `QODER_*`/`QODERCN_*`
> 身份/鉴权变量，否则报"运行身份与产物身份不一致"或 `job_token_invalid`
> （`use-electron-gf.sh` 对 `qoder|qoder-cn` 已自动清理；桌面图标启动走干净会话，不受影响）。

**验证 gf 是否真正生效**：启动后在应用内打开 DevTools（`F12` 或 `Ctrl+Shift+I`）→
`chrome://gpu`，确认 `Graphics Feature Status` 下 `Canvas` / `WebGL` 显示
`Hardware accelerated`，且 `GL_VENDOR` / `GL_RENDERER` 含 `LISUAN` / `gf`；或用：

```bash
GF_EXTRA_FLAGS="--enable-logging=stderr --v=1" bash use-qoder.sh 2>&1 | grep -iE "gl_renderer|gpu|angle"
# 应看到 ANGLE 走 GL + gf 设备, 而非 SwiftShader/软件
```

**桌面项改写说明**：`--install-desktop` 会把系统级 `.desktop`
（`/usr/share/applications`）**复制**到用户级（`~/.local/share/applications`）再改写
`Exec`，用户级优先级更高且无需 sudo；撤销只需删除用户级那份 `.desktop`。

### 4.2 CodeBuddy 国内版

**症状**：同 4 节通用表现——窗口弹出但内容区空白 / 花屏 / 黑块，或卡在加载页。

**部署**：

```bash
cd SCRIPTS
# 1) 直接加速启动（推荐先试 gf 加速）
bash use-codebuddy.sh
# 2) 若白屏/花屏，回退软件渲染：
bash use-codebuddy.sh --no-hw
# 3) 安装后从应用菜单直启即带修复+加速：
bash use-codebuddy.sh --install-desktop
```

- 若 CodeBuddy 不在默认 `PATH`（`codebuddy` / `codebuddy-ai`），用环境变量指定：
  `CODEBUDDY_CMD=/opt/codebuddy/codebuddy bash use-codebuddy.sh`。
- 其它开关可追加：`GF_EXTRA_FLAGS="--enable-logging=stderr --v=1" bash use-codebuddy.sh`。

### 4.3 Qoder 国内版

**症状**：同 4 节通用表现——界面空白 / 花屏，GPU 合成层异常。

**部署**：

```bash
cd SCRIPTS
bash use-qoder.sh                       # gf 加速启动（默认）
bash use-qoder.sh --no-hw               # 白屏/花屏时回退软件渲染
bash use-qoder.sh --install-desktop     # 桌面项改写到用户级, 从菜单直启即带加速
```

- 已安装判定：`/usr/bin/qoder` + `/usr/share/applications/qoder.desktop`（含 `qoder-cn`）。
- 加速验证同上（`chrome://gpu` 看 `GL_RENDERER` 是否为 gf 设备）。
- 撤销：删除 `~/.local/share/applications/qoder*.desktop` 即可恢复系统原桌面项。

---


## 五、游戏修复文档

针对具体游戏的完整修复文档（根因、规避方案、部署、验证）独立成篇：

- [刀塔霸业（Dota Underlords）修复](./underlords/fix-underlords-native.md)
  —— Source 2 原生 Linux 游戏在 Steam pressure-vessel 容器内崩溃，容器委托宿主直跑；libpng12 补齐、panorama 随机段错误智能重试与退出检测。
- [随从大师（Minion Masters）修复](./minion-masters/fix-minion-masters.md)
  —— Unity/IL2CPP 游戏：Vivox 登录崩溃 + 图片下载 Texture 异常的 BepInEx 运行时 patch 三件套（Vivox `LoginAsync`、新闻下载状态机 `MoveNext`、`DownloadHandlerTexture.InternalGetTexture`）；证书 patch 反致黑屏（Zenject 异常循环）的回归教训；游戏后端 API 全 404 与 Azure IP 封锁；Proton 选型（须支持 new WoW64，Proton 8.0 不可用于硬件渲染；**推荐 Proton 11.0 稳定版**，更新频率低于 Experimental）、user_settings.py 注入无效教训、Steam 状态卡死坑。
- [随从大师攻坚全记录](./minion-masters/MinionMasters-攻坚全记录.md)
  —— 32 位游戏 × 只有 64 位 gf 驱动的完整攻坚：WoW64 路线、发行版白名单绕过、libSC 依赖、DXVK 降级、`vkMapMemory` 高地址 vs 32 位 thunk 的根因与驱动 `MAP_32BIT` 补丁。**2026-08-29 硬件渲染链路已端到端打通**（协商垫片 + 低 4GB 重映射池，`Renderer: LISUAN 7G100` + 登录流程）；剩余阻塞仅服务端 API 404。

---

## Steam 平台适配细节（容器 / CEF / 登录 / Proton）

> Steam 本身的全部坑集中在这里，游戏专属问题见上面的 6.1 / 6.2。

- [Steam 在砺算 7G100 上的适配细节](./steam/steam-on-lisuan.md)
  —— pressure-vessel 容器架构（gf Vulkan 容器内自动可用、PCI sysfs 被隐藏）；**gf env 不能污染 Steam 客户端**（致命坑）；CEF `gf_gbm` 段错误 + ANGLE 崩溃修复（`fix-steam-cef-lock.sh`）；无头环境 Steam 登录窗 GLX 崩溃需图形界面生成 `ssfn`；`applaunch` "游戏运行中"状态卡死；Proton 选型（须支持 new WoW64，Proton 8.0 不可用于硬件渲染；**推荐 Proton 11.0 稳定版**，更新频率低于 Experimental）；`user_settings.py` 注入不进容器白名单；overlay 排障；排障速查表。

---

## 六、踩坑经验总览与砺算技术要点

> 本章把前面各章分散的"坑"与"砺算技术背景"汇成一份速查表，便于**排障时直接对照**，
> 也便于向砺算驱动团队反馈。逐项的完整论证见对应章节与各主题目录下的文档。

### 7.1 砺算 7G100 技术要点（必读）

| 项 | 值 / 说明 |
| --- | --- |
| GPU | 砺算 Lisuan **7G100**，PCI `03:00.0` `[4c54:5006]`（子系统 `4c54:5000`） |
| PCI 厂商号 | `4c54`（ASCII `"LT"`）—— 一切驱动匹配、日志过滤的起点 |
| 内核模块 | 文件名 `gf.ko`；DRM 驱动**注册名**为 `lisuan`（与用户态驱动名 `gf` 不一致，见 7.2） |
| 用户态驱动名 | `gf`（Mesa DRI driver / Vulkan ICD 均以此命名）；DRM 名却叫 `lisuan` → 经典坑 |
| Vulkan ICD | `gf_icd.x86_64.json`（gf Vulkan）；另有 lavapipe `lvp_icd.json`（软件兜底） |
| GLX vendor | 双下划线变量 `__GLX_VENDOR_LIBRARY_NAME=gf`（pressure-vessel 白名单只认双下划线形式） |
| 伴生音频 | `4c54:5010` `05:00.1` 是 `gf` 显示驱动内部的**透明 codec**（`disp_codec.c`），**不创建 ALSA 卡**，不能作为声卡使用 |
| 真正出声 | 海光板载 HDA `1d94:14c9` `07:00.6`（ALC222）—— 与 7G100 GPU **无关** |
| 验证环境 | Ubuntu 24.04.4 LTS (noble) / 内核 `7.0.0-28-generic` (HWE) / gcc 13.3.0 / 驱动包 `lisuangpu-dkms` `262.2.0.56` |

**图形栈层次（从上到下）**：应用 → (Electron/Chromium / Steam CEF / Wine+DXVK) → Mesa(`gf` DRI) / ANGLE → Vulkan(`gf_icd`) 或 GLX(`__GLX_VENDOR_LIBRARY_NAME=gf`) → 内核 `gf.ko`(DRM 名 `lisuan`) → 硬件。每一层都曾踩过坑（见下表）。

### 7.2 踩坑清单（按层索引）

> 每条细节与修法见对应主题目录下的文档（链接内为文档内小节）。本表仅为速查索引。

| 层 | 坑 | 修法 / 细节文档 |
| --- | --- | --- |
| 内核 | DKMS 变量名错配（`KERNEL_VERSION` vs `KERNELVER`）→ vermagic 错配 | 一行 Makefile 补丁 + `fix-lisuan-gpu.sh`（[一、内核模块](#一内核模块-gfko-编译与加载) / `patches/`） |
| Mesa | 驱动名不一致（DRM `lisuan` vs 用户态 `gf`） | `fix-lisuan-mesa-driver-name.sh`（[三、Mesa/用户态](#三mesa用户态适配)） |
| Mesa | 厂商 fork 22.0.4 vs 系统 25.2.x 不兼容 | `zink` 回退（`mesa/system-mesa-analysis.md`） |
| Steam/CEF | `gf_gbm.so` 段错误 + ANGLE 惰性符号失败 | 仅游戏进程树注入（`dkms/vendor-bug-report-gf-gbm-crash.md`） |
| Steam/CEF | **全局 dri 软链 `lisuan_dri.so->gf_dri.so` / Steam 客户端 PRELOAD 使 Steam 客户端 double free**（致命坑） | 默认不建软链、不向 Steam wrap 注入 gf（`steam/steam-on-lisuan.md` §2/§3） |
| Wine/DXVK | `d3d11: failed to create factory` 渲染卡死 | `PROTON_USE_WINED3D=1`（`minion-masters/fix-minion-masters.md` 附录 A1） |
| Wine/DXVK | 手动 `run-mm.sh` 硬塞 env 破坏 Steam 注入 | 清空 LaunchOptions 恢复默认（`minion-masters/fix-minion-masters.md` 附录 A1） |
| 游戏逻辑 | Vivox 登录崩溃（IL2CPP 机器码） | `MMSkipVivox` 插件（`minion-masters/fix-minion-masters.md` 游戏坑 1） |
| 游戏逻辑 | **证书 patch（`CertificateHandler.ValidateCertificate`）反致黑屏**：Zenject 循环 + BootstrapInstaller NullReference + `NotSupportedException` 刷屏 | 禁用证书 patch，改 patch `DownloadHandlerTexture.InternalGetTexture` 返回 null（`minion-masters/fix-minion-masters.md` 游戏坑 1 / 游戏坑 2） |
| 游戏逻辑 | 图片下载 `Curl error 60` → `Texture has not yet finished downloading` 沿 UniTask `TrySetException` 链传播 | patch `InternalGetTexture` 返回 null（`minion-masters/fix-minion-masters.md` 游戏坑 1c） |
| IL2CPP 插件 | **`il2cpp_class_get_methods` binding 签名错**（iterator 被当数组）→ count 垃圾值 → 0xC0000005 | 只用 `il2cpp_class_get_method_from_name`（`minion-masters/fix-minion-masters.md` 附录 A4） |
| 游戏逻辑 | `user_settings.py` 注入无效 | 用 Launch Options（`minion-masters/fix-minion-masters.md` 游戏坑 5） |

### 7.3 环境 / 网络类坑（索引）

- **Steam 客户端登录态丢失（`steamid=0` 陈旧态）** → 登录过一次后（`AutoLoginUser` + `RememberPassword` 已写入）**重启 Steam 客户端即自动重登**，无需图形界面；首次登录仍需图形界面生成 ssfn（`minion-masters/fix-minion-masters.md` 附录 A2 / `steam/steam-on-lisuan.md` §4）
- **随从大师后端 API 全 404**（`api.prod.mm21.minionmastersthegame.com` 的 `/v1/*` 全 404、`/health` 却 200）→ `LoginQueueService.Post` 抛 404，登录队列进不去；服务端问题本地无解（`minion-masters/fix-minion-masters.md` 游戏坑 3）
- **Azure IP `140.228.24.145:443`(及 `:33798`) 网络封锁** → 代理/VPN（`minion-masters/fix-minion-masters.md` 游戏坑 3）
- **随从大师进程启动后 ~40–90 秒被移除**（进主菜单后消失，当前未解；疑似登录队列 404 后游戏自身退出）→ `minion-masters/fix-minion-masters.md` 游戏坑 7
- **Standard shader 不支持（画质告警，不致命）** → 忽略（`minion-masters/fix-minion-masters.md` 附录 A3）

### 7.4 排查方法论（速查）

1. **先看 Player.log / cef_log.txt 停在哪一行**——区分卡渲染 / 卡登录 / 卡网络。
2. **Player.log 被 `NotSupportedException` 刷屏时**，先 `grep -vE "NotSupportedException|Couldn't extract|Derived classes|at System.Reflection|..."` 滤噪再看最初 20 行——IL2CPP 的 `GetGenericArguments()` 未实现，异常提取栈本身会再抛异常，把真正的原始异常淹没。
3. **卡渲染** → 优先 `PROTON_USE_WINED3D=1`，再查 `VK_ICD_FILENAMES` 指向有效 ICD。
4. **卡登录/崩溃** → 优先看 Player.log 最后一行 + 进程退出码（wine seh 经 Steam 多层重定向常抓不到 stderr）。
5. **进程干净退出（无 dump、无 .NET 异常）** → 多为网络/服务端问题（维护、IP 封锁、API 下线），用 curl 直测后端 endpoint 复核，别只盯本地日志。
6. **改 env 只注入目标进程树，绝不污染 Steam 客户端 / 全局 shell**。
7. **IL2CPP 崩溃无法按托管名 patch** → 走 Il2CppInterop 运行时 API（`il2cpp_class_from_name` + `il2cpp_class_get_method_from_name`，读 MethodInfo+0 拿 methodPointer）定位 native 方法改机器码；**不要用 `il2cpp_class_get_methods`**（binding 签名错，必崩，见 7.2）。
8. **patch Unity 内部 native 方法前先反汇编确认它不是共享 stub**——原字节已是 `32 C0 C3 CC`（优化过的 return-false stub）说明有多个调用方，patch 它会波及无辜路径（证书 patch 黑屏的根因）。

---

## 关于厂商驱动的获取

> **本仓库不分发任何厂商专有文件**（`.deb` 驱动包、闭源 blob、重编出的 `gf.ko`），
> 这些内容归砺算 / 整机厂商所有，请通过**官方渠道或设备 OEM** 自行获取。
>
> 例外：厂商 DKMS 包内的**内核源码**本身以 GPLv2 / Dual MIT/GPLv2 授权
> （包内 `copyright` 与文件头声明），已按授权原样转录至
> [`dkms/vendor-src/`](./dkms/vendor-src/README.md)
> （含来源包 SHA-256；闭源 blob 与一切二进制产物不在其中）。

本修复针对并验证于以下版本的驱动包，供你核对完整性（`sha256sum`）：

| 文件 | 大小 | SHA-256 |
| --- | --- | --- |
| `lisuangpu-dkms-262.2.0.56_amd64.deb` | 472 KiB (481116 B) | `8277dc9c8975add6182e6487415529ea7850e045eed0cd144b54c13f9663807c` |
| `lisuangpu-lib-262.2.0.56_amd64.deb` | 32 MiB (33213680 B) | `8397604732e8bb2b21257153bd6dd3013bc225f2f52741e2e4fc4e0d21c7adc1` |

```bash
# 拿到 .deb 后先校验，再安装
sha256sum lisuangpu-dkms-262.2.0.56_amd64.deb lisuangpu-lib-262.2.0.56_amd64.deb
sudo apt-get install -y ./lisuangpu-dkms-262.2.0.56_amd64.deb ./lisuangpu-lib-262.2.0.56_amd64.deb
```

---

## 自动化测试

仓库根目录的 `tests/` 自带一套**离线、无硬件依赖**的测试
（Gitee Go 流水线 `.workflow/gitee-go-test.yml` 自动执行），
检验对象是各主题目录的交付物（`steam/` `minion-masters/` `underlords/` `electron/` `dkms/` `mesa/`）：

```bash
./tests/run-tests.sh        # 运行全部
./tests/run-tests.sh -v     # 详细输出
./tests/run-tests.sh -l     # 列出测试单元
```

| 测试单元 | 验证内容 |
| --- | --- |
| `test-patch.sh` | 补丁可干净应用/反向还原；结果与 `Makefile.fixed` 字节一致 |
| `test-makefile-logic.sh` | 真实 `make` 展开：修复前忽略 `KERNELVER`（复现 bug），修复后优先 `KERNELVER` |
| `test-fix-script.sh` | `fix-lisuan-gpu.sh` 内置补丁逻辑正确且幂等 |
| `test-scripts-syntax.sh` | 所有 shell 脚本 `bash -n` 语法检查 |
| `test-repo-health.sh` | 必需文件存在、脚本可执行、无厂商专有产物混入、README 引用可解析 |

---

## 常见问题 FAQ

**Q：`dkms status` 明明是 `installed`，为什么还是没驱动？**
A：`installed` 只表示 DKMS 记录了安装动作。用 `modinfo` 看 `vermagic` 是否与
`uname -r` 一致，用 `lsmod | grep gf` 看是否真的加载。

**Q：开启了 Secure Boot 怎么办？**
A：模块需用已注册到 MOK 的密钥签名；如仍被拒，用 `mokutil` 注册密钥，或临时
关闭 Secure Boot 后再执行修复。

**Q：`gf.ko` 和 `lisuan` 是什么关系？**
A：内核模块文件名为 `gf.ko`，其注册的 DRM/PCI 驱动名为 `lisuan`，所以 `lspci`
显示 `Kernel driver in use: lisuan` 是正常的。

**Q：Steam 能恢复界面硬件加速吗？**
A：暂不能。厂商 EGL 本身在宿主可用（X11 等平台实测正常），卡点在容器内的
 glvnd 分发拿不到厂商栈，已在 bug 报告中提请厂商提供容器适配；在此之前
界面软渲染、游戏宿主直跑用 GPU 是当前最优解。

---

## 许可证与免责声明

- 本仓库（补丁、脚本、文档）以 **Apache License 2.0** 授权，见 [`LICENSE`](./LICENSE)
  与 [`NOTICE`](./NOTICE)。
- 本项目与砺算科技（Lisuan Technology）、海光（Hygon）无隶属或背书关系；相关名称、
  商标归各自持有者所有，此处仅用于标识。
- 砺算 GPU 驱动及其用户态库为第三方专有软件，遵循其各自的授权条款，**不包含**在本仓库中。
- 内核模块操作有风险，请自行评估；作者不对任何数据丢失或硬件问题负责。

欢迎通过 Issue / Pull Request 补充其它砺算型号、其它发行版（如 openEuler、麒麟、UOS）
或其它驱动版本的验证结果。
