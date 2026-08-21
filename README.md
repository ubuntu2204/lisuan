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
   vendor Mesa 22.0.4 fork vs system Mesa 25.2.x is in `docs/` (zink is the
   viable fallback path).
3. **Applications (Steam)** — Steam's CEF crashes in its pressure-vessel
   container (two root causes: vendor `gf_gbm.so` segfaults under modern
   libgbm; Steam's bundled ANGLE fails lazy symbol resolution). Fixed by an
   injected, `chattr +i`-locked environment in `steamwebhelper_sniper_wrap.sh`,
   with a boot-time auto-repair systemd service. Native Linux games (verified:
   Dota Underlords) run via a documented host-native launch path.

**This repo does not contain the vendor driver** — only independent patches,
scripts, documentation and tests, under Apache-2.0. It also includes a
fully-evidenced vendor bug report (`docs/vendor-bug-report-gf-gbm-crash.md`)
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
- [四、Electron 类应用（ZCode）适配](#四electron-类应用zcode-适配)
- [关于厂商驱动的获取](#关于厂商驱动的获取)
- [自动化测试](#自动化测试)
- [常见问题 FAQ](#常见问题-faq)
- [许可证与免责声明](#许可证与免责声明)

---

## 仓库结构

全部修复脚本、补丁与文档集中在 `lisuan-7g100-ubuntu-fix/`，按用途分为四类：

**修复类（fix）**——在目标机器上执行，会修改系统状态（均幂等）：

| 路径 | 说明 |
|---|---|
| `lisuan-7g100-ubuntu-fix/patches/0001-dkms-makefile-honor-KERNELVER.patch` | 厂商 DKMS Makefile 的 `KERNEL_VERSION`/`KERNELVER` 变量名错配修复（一行补丁） |
| `lisuan-7g100-ubuntu-fix/scripts/fix-lisuan-gpu.sh` | 一键应用补丁并重编/重装/加载 `gf.ko`（幂等、自动备份） |
| `lisuan-7g100-ubuntu-fix/scripts/fix-lisuan-mesa-driver-name.sh` | 修复 DRM 名与用户态驱动名不一致（`lisuan` vs `gf`）导致的 DRI 加载失败 |
| `lisuan-7g100-ubuntu-fix/scripts/fix-steam-cef-lock.sh` | **Steam 修复主入口**：向容器内注入 `GBM_BACKEND=dri` + `LD_PRELOAD` glapi 并 `chattr +i` 锁定（详见[三](#三steam-适配)） |
| `lisuan-7g100-ubuntu-fix/scripts/fix-underlords-native.sh` | **Dota Underlords 修复安装器**（幂等）：容器委托宿主直跑 + 智能重试/退出检测 + libpng12 补齐 + 桌面项；`--status` 查状态、`--uninstall` 卸载、`--no-autorestart` 彻底关闭自动重启（游戏中途崩溃也直接退出，菜单 14/15） |
| `lisuan-7g100-ubuntu-fix/scripts/steam-cef-autofix.sh` | 开机自愈（root/systemd 调用）：发现 Steam 更新还原了补丁就自动重新注入锁定，由 `./lisuan-fix.sh` 菜单 9 安装为系统服务；`--uninstall` 卸载服务 |
| `lisuan-7g100-ubuntu-fix/scripts/uninstall-lisuan-gpu.sh` | 卸载：撤销 DKMS Makefile 补丁（优先从 `.bak` 还原，不影响已装驱动） |
| `lisuan-7g100-ubuntu-fix/scripts/uninstall-lisuan-mesa-driver-name.sh` | 卸载：删除 `lisuan_*.so` 软链（只删指向 `gf_*` 的软链，不碰真实文件） |

**体检类（verify）**——只读诊断，不修改任何状态，需要有砺算显卡的真机：

| 路径 | 说明 |
|---|---|
| `lisuan-7g100-ubuntu-fix/scripts/verify-lisuan-gpu.sh` | 驱动体检：lspci/lsmod/modinfo/DRM 节点 |
| `lisuan-7g100-ubuntu-fix/scripts/verify-system-mesa.sh` | 系统 Mesa 兼容性体检（zink 回退、GBM dri 后端等） |

**工具类（use）**——按需包装单个应用的环境：

| 路径 | 说明 |
|---|---|
| `lisuan-7g100-ubuntu-fix/scripts/use-system-mesa.sh` | 让指定应用运行在系统 Mesa（zink）之上，绕开厂商 Mesa 22.0.4 分叉 |
| `lisuan-7g100-ubuntu-fix/scripts/use-zcode.sh` | **ZCode（Electron AppImage）显示异常修复**：`--no-sandbox` + `--ozone-platform-hint=x11` + 禁用 GPU 合成；`--install-desktop` 把桌面项切到本 wrapper |

**文档（docs）**：

| 路径 | 说明 |
|---|---|
| `lisuan-7g100-ubuntu-fix/docs/system-mesa-analysis.md` | 7G100 与系统 Mesa 25.2.x 的源码级兼容性分析（为什么原生加载不可行、zink 路径、已知地雷） |
| `lisuan-7g100-ubuntu-fix/docs/vendor-bug-report-gf-gbm-crash.md` | 提交厂商的 bug 报告（gf_gbm 段错误、容器内 EGL 分发、GLX vendor 注册缺失三个问题，附完整证据链） |
| `lisuan-7g100-ubuntu-fix/vendor-src/` | 厂商 DKMS 内核源码原样转录（GPLv2/Dual MIT/GPLv2 授权部分，69 个 .c + 99 个 .h；闭源 blob 不在其中，见其 README） |

> 注意区分：`scripts/` 下的 verify-* 是**面向真机的体检工具**，与仓库根的
> `tests/`（离线、无硬件依赖的 CI 自检）不是一回事。

其余顶层内容：

| 路径 | 说明 |
|---|---|
| `CHANGELOG.md` | 更新日志：每个版本的新增/修复/实测更正，均实机验证 |
| `lisuan-fix.sh` | **交互式修复索引**：自动检测系统环境并给出结论，输入编号执行单项修复/体检（`./lisuan-fix.sh --detect` 仅检测） |
| `tests/` | 离线自动化测试（补丁、脚本幂等性、语法、仓库合规），检验对象为 `lisuan-7g100-ubuntu-fix/` 子树 |
| `.workflow/gitee-go-test.yml` | Gitee Go 流水线：push/PR 时自动跑 `tests/` |
| `_work/` | 本地诊断草稿区（构建日志、探针程序、解包的厂商 DKMS 源码树、Mesa 官方源码等）。仅本机排查用，**不是交付物**；其中的厂商文件不得再分发 |
| `lisuangpu-*.deb` | 厂商驱动包原件（已被 `.gitignore` 排除，不入库），仅本机留存 |

## 快速开始

不熟悉各脚本的话，直接用交互式索引（自动检测环境 + 编号菜单）：

```bash
./lisuan-fix.sh             # 检测 + 菜单
./lisuan-fix.sh --detect    # 只做环境检测
./lisuan-fix.sh 2           # 直接执行第 2 项（修复 DKMS）
```

或手动执行单项：

```bash
cd lisuan-7g100-ubuntu-fix

# 1) 内核升级后驱动加载失败（vermagic 错配 / 黑屏）
sudo ./scripts/fix-lisuan-gpu.sh
./scripts/verify-lisuan-gpu.sh

# 2) GL/EGL 找不到驱动（driver=(null)）、DRI 加载失败
sudo ./scripts/fix-lisuan-mesa-driver-name.sh

# 3) Steam 界面起不来（cef_log.txt 大量 exit_code=139/11）
sudo ./scripts/fix-lisuan-mesa-driver-name.sh
sudo ./scripts/fix-steam-cef-lock.sh
MESA_LOADER_DRIVER_OVERRIDE=gf steam -no-cef-sandbox -cef-force-64bit

# 4) 想让某个应用走系统 Mesa（25.2.x，zink）而非厂商 Mesa 22.0.4
./scripts/use-system-mesa.sh --print-env
./scripts/use-system-mesa.sh glxinfo -B
./scripts/verify-system-mesa.sh

# 5) Electron 类应用 (ZCode) 显示异常: 界面空白 / 花屏 / 黑块
./scripts/use-zcode.sh                       # 启动 (含 --no-sandbox 等修复)
./scripts/use-zcode.sh --install-desktop     # 把应用菜单的 Exec 切到本 wrapper
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
`uname -r`。改动仅一行，见 [`lisuan-7g100-ubuntu-fix/patches/0001-dkms-makefile-honor-KERNELVER.patch`](./lisuan-7g100-ubuntu-fix/patches/0001-dkms-makefile-honor-KERNELVER.patch)：

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
sudo ./lisuan-7g100-ubuntu-fix/scripts/fix-lisuan-gpu.sh

# 或：为指定内核构建（例如装了新内核但尚未重启进入）
sudo ./lisuan-7g100-ubuntu-fix/scripts/fix-lisuan-gpu.sh 7.0.0-28-generic
```

脚本会：① 幂等地给 `/usr/src/lisuangpu-dkms-*/Makefile` 打补丁（保留 `.bak`）→
② `dkms remove/build/install` 针对目标内核重编重装 → ③ `depmod` + `modprobe gf` →
④ 打印验证报告。随后可运行只读体检脚本 `lisuan-7g100-ubuntu-fix/scripts/verify-lisuan-gpu.sh`。

### 手动修复步骤

```bash
# 0) 准备：安装当前内核头文件与 dkms（按需）
sudo apt-get install -y dkms "linux-headers-$(uname -r)"

# 1) 打补丁（从 DKMS 源码根目录）
cd /usr/src/lisuangpu-dkms-262.2.0.56          # 版本号以实际为准
sudo cp -a Makefile Makefile.bak
sudo patch -p1 < /path/to/lisuan-7g100-ubuntu-fix/patches/0001-dkms-makefile-honor-KERNELVER.patch

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
| DKMS Makefile 补丁 | `sudo lisuan-7g100-ubuntu-fix/scripts/uninstall-lisuan-gpu.sh`（优先从 `.bak` 还原，无备份时反向替换并留 `.uninstall.bak`；菜单 11） |
| DRI 驱动名软链 | `sudo lisuan-7g100-ubuntu-fix/scripts/uninstall-lisuan-mesa-driver-name.sh`（只删指向 `gf_*` 的软链；菜单 12） |
| Steam 容器补丁 | `sudo lisuan-7g100-ubuntu-fix/scripts/fix-steam-cef-lock.sh --uninstall`（还原 wrap.sh 并解锁；`--unlock` 只解锁不还原；菜单 13） |
| 开机自愈服务 | `sudo lisuan-7g100-ubuntu-fix/scripts/steam-cef-autofix.sh --uninstall`（菜单 10） |

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

修复：`sudo ./lisuan-7g100-ubuntu-fix/scripts/fix-lisuan-mesa-driver-name.sh`
创建 `lisuan_dri.so → gf_dri.so` 等符号链接（幂等；重装厂商 lib 包后建议重跑）。

### 与系统 Mesa 25.2.x 的关系（要点）

完整源码级分析见 [`docs/system-mesa-analysis.md`](./lisuan-7g100-ubuntu-fix/docs/system-mesa-analysis.md)：

- 厂商 GL 栈是**自包含的 Mesa 22.0.4 分叉**（`libGLX_gf`/`libEGL_gf` + `gf_dri.so`
  megadriver），系统 Mesa 25.2.x **架构上无法加载它**（无 megadriver 加载机制、
  pipe 描述符静态表无 `gf`）；
- 可用的系统 Mesa 路径是 **zink**（GL-on-Vulkan，架在厂商 Vulkan ICD
  `libvulkan_gf.so` 之上）：GLX 自动回退可用（GL 3.0 上限）；
- **地雷**：厂商 `gf_gbm.so` 在系统 libgbm 25.x 下段错误（ABI 漂移）——任何走
  系统 libgbm 默认后端解析的场景需 `GBM_BACKEND=dri` 规避；
- 想让某应用走系统 Mesa：`./lisuan-7g100-ubuntu-fix/scripts/use-system-mesa.sh -- <cmd>`。

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
sudo ./scripts/fix-lisuan-mesa-driver-name.sh   # 先修驱动名
sudo ./scripts/fix-steam-cef-lock.sh            # 注入并 chattr +i 锁定
MESA_LOADER_DRIVER_OVERRIDE=gf steam -no-cef-sandbox -cef-force-64bit
```

`fix-steam-cef-lock.sh` 向 `steamwebhelper_sniper_wrap.sh` 注入：

```bash
export GBM_BACKEND=dri                                        # 绕开 gf_gbm.so 段错误
export LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libglapi_gf.so.0  # 修 ANGLE 符号解析崩溃
```

效果：GPU 进程不再崩溃；它尝试硬件加速失败后（容器内 glvnd 的 EGL 分发
拿不到厂商 EGL，见
[`docs/vendor-bug-report-gf-gbm-crash.md`](./lisuan-7g100-ubuntu-fix/docs/vendor-bug-report-gf-gbm-crash.md) §3）
自动回退软渲染，Steam 界面正常。注意：容器内的原生 GLX 游戏仍无法启动
（见[游戏兼容性](#游戏兼容性dota-underlords-实测)与报告 §4）。

### 注意事项

- **日常开机无需任何操作**：补丁写入 `wrap.sh` 并 `chattr +i` 锁定，重启不丢；
  游戏用桌面菜单图标启动。
- 建议安装开机自愈：`./lisuan-fix.sh` 选 **9**（或
  `sudo lisuan-7g100-ubuntu-fix/scripts/steam-cef-autofix.sh` 手动验证）。
  之后 Steam 客户端更新还原补丁时，下次开机自动重新注入，菜单 5/6 不再需要。
- 不装自愈服务时：Steam 客户端更新前先 `sudo ./scripts/fix-steam-cef-lock.sh --unlock`，
  更新后重跑注入。
- 手动编辑 wrap.sh 后务必 `chmod +x`（`sed -i` 会丢执行位，容器内 pv-adverb 会报
  Permission denied）。
- 三个问题的根因与修复建议已整理成厂商 bug 报告：
  [`docs/vendor-bug-report-gf-gbm-crash.md`](./lisuan-7g100-ubuntu-fix/docs/vendor-bug-report-gf-gbm-crash.md)。

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
  整套文件由 `scripts/fix-underlords-native.sh`（菜单 14）安装/修复/卸载，
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
  又不想区分），用 `sudo scripts/fix-underlords-native.sh --no-autorestart`
  重打补丁——游戏进程结束（正常退出或崩溃）即直接退出，不再拉起。副作用：
  panorama 随机段错误会让游戏**直接闪退不可玩**，按需选择（菜单外手动执行）。
- 已知限制：宿主直跑的游戏没有 Steam 游戏内覆盖（Shift+Tab）；被 `kill -9`
  强杀后残留的 `LS_SHARE_MEMORY_*` 会在下次启动时自动清理；段错误由
  `underlords.sh` 智能重试兜底（见上条，仅在默认自动重启开启时生效）。
- 备用：桌面项 `dota-underlords-native.desktop` 可不依赖 Steam 启动链路直接玩。

---

## 四、Electron 类应用（ZCode）适配

> 同类问题也适用于其它 Electron / Chromium 系桌面应用（VSCode、网易云音乐、微信等）：
> 在国产 GPU + Ubuntu 24.04 上同样可能表现为"界面空白 / 花屏 / 黑块"。根因与修复
> 完全一致，本节的 `--no-sandbox` / `--ozone-platform-hint=x11` / `--disable-gpu`
> 三件套可直接套用。

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
cd lisuan-7g100-ubuntu-fix
bash scripts/use-zcode.sh                       # 启动 ZCode (带完整修复开关)
bash scripts/use-zcode.sh --install-desktop     # 把系统 zcode.desktop 的 Exec 切到本 wrapper
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
ZCODE_EXTRA_FLAGS="--enable-logging=stderr --v=1" bash scripts/use-zcode.sh
# 正常应看到 GPU 进程 fallback 到软件渲染, 无 "GPU process isn't usable" 致命错误
```

### 注意事项

- 若换回纯 X11 会话（`XDG_SESSION_TYPE=x11`），`--ozone-platform-hint=x11` 仍无害，
  可保留。
- 想试恢复硬件加速流畅度：`ZCODE_EXTRA_FLAGS="--ignore-gpu-blocklist" bash scripts/use-zcode.sh`
  （若出现花屏/白屏，去掉该标志回退默认即可）。
- 本脚本只改"启动环境"，不触碰系统其它文件；撤销只需把桌面项 `Exec` 改回 AppImage
  本体，或删除 `use-zcode.sh`。

---

## 关于厂商驱动的获取

> **本仓库不分发任何厂商专有文件**（`.deb` 驱动包、闭源 blob、重编出的 `gf.ko`），
> 这些内容归砺算 / 整机厂商所有，请通过**官方渠道或设备 OEM** 自行获取。
>
> 例外：厂商 DKMS 包内的**内核源码**本身以 GPLv2 / Dual MIT/GPLv2 授权
> （包内 `copyright` 与文件头声明），已按授权原样转录至
> [`lisuan-7g100-ubuntu-fix/vendor-src/`](./lisuan-7g100-ubuntu-fix/vendor-src/README.md)
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
检验对象是 `lisuan-7g100-ubuntu-fix/` 交付物子树：

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
