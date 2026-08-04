# 砺算 7G100 GPU · Ubuntu 24.04 内核升级驱动修复

> 修复砺算（Lisuan）7G100 GPU 的 DKMS 驱动在 Ubuntu 24.04.4 / 内核 `7.0.0-28-generic`
> 上**编译“成功”却无法加载、显卡无驱动绑定**的问题。根因是厂商 DKMS 构建脚本的一个
> 变量名不匹配 Bug，本仓库提供一行补丁 + 一键修复脚本 + 排查文档。

[![License](https://img.shields.io/badge/License-Apache_2.0-blue.svg)](./LICENSE)
![Platform](https://img.shields.io/badge/platform-Hygon%20%2B%20Lisuan%207G100-orange)
![OS](https://img.shields.io/badge/Ubuntu-24.04.4%20LTS-E95420)

---

## English (TL;DR)

The Lisuan (砺算) 7G100 GPU driver is shipped as a DKMS package (`lisuangpu-dkms`).
Its `Makefile` selects the build kernel from `KERNEL_VERSION` (default `uname -r`),
but `dkms.conf` passes the **target** kernel as `KERNELVER`. The variable names
differ, so after a kernel upgrade DKMS builds the module against the *running*
kernel and files it under the *target* kernel tree. The result is a module with
the wrong `vermagic` / symbol CRCs that refuses to load — the GPU ends up with
no kernel driver bound (black screen).

**This repo does not contain the vendor driver.** It contains only an
independent, one-line `Makefile` patch, helper scripts, and documentation,
under Apache-2.0. Fix:

```bash
sudo ./scripts/fix-lisuan-gpu.sh      # patches Makefile, rebuilds via DKMS, loads the module
```

No reverse-engineering is required: the single closed-source blob only depends
on stable kernel symbols (`__fentry__`, `sscanf`); everything else is the
open-source glue layer that simply needs recompiling against the right headers.

---

## 目录

- [适用场景](#适用场景)
- [验证环境](#验证环境)
- [问题现象](#问题现象)
- [根因分析](#根因分析)
- [为什么不需要反编译](#为什么不需要反编译)
- [快速修复（推荐）](#快速修复推荐)
- [手动修复步骤](#手动修复步骤)
- [验证结果](#验证结果)
- [关于厂商驱动的获取](#关于厂商驱动的获取)
- [未来内核升级](#未来内核升级)
- [卸载 / 撤销本补丁](#卸载--撤销本补丁)
- [自动化测试](#自动化测试)
- [仓库结构](#仓库结构)
- [常见问题 FAQ](#常见问题-faq)
- [许可证与免责声明](#许可证与免责声明)

---

## 适用场景

只要满足以下**任一**特征，本仓库的分析与修复即适用：

- 砺算 GPU（PCI 厂商号 `4c54`，即 ASCII `"LT"`）在**升级内核后黑屏 / 无显示 / 掉驱动**；
- `dkms status` 显示模块 `installed`，但 `lsmod` 里没有 `gf`，显卡也没有驱动绑定；
- `dmesg` 出现大量 `gf: disagrees about version of symbol ...` 或 `gf: Unknown symbol ...`；
- `modinfo gf.ko` 显示的 `vermagic` 与当前 `uname -r` **不一致**。

> 该类“DKMS 变量名不匹配”缺陷在部分国产信创 GPU 驱动中都可能出现，排查思路通用。

## 验证环境

本修复在如下真实环境完成诊断与验证：

| 项目 | 值 |
| --- | --- |
| CPU 平台 | 海光 Hygon（信创平台） |
| GPU | 砺算 Lisuan **7G100**，PCI `03:00.0` `[4c54:5006]`（子系统 `4c54:5000`） |
| 操作系统 | Ubuntu 24.04.4 LTS (noble) |
| 内核 | `7.0.0-28-generic`（HWE） |
| 编译器 | gcc 13.3.0（与内核构建工具链一致） |
| 驱动包 | `lisuangpu-dkms` / `lisuangpu-lib` 版本 `262.2.0.56` |
| 内核模块 | 文件名 `gf.ko`；DRM 驱动注册名为 `lisuan` |
| Secure Boot | Disabled（如启用需额外签名，见 FAQ） |

## 问题现象

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

## 根因分析

厂商 DKMS 源码 `dkms.conf` 通过如下方式把**目标内核**传给 Make：

```make
MAKE[0]="'make' KERNELVER=$kernelver"
```

但 `Makefile` 却用了**另一个变量名** `KERNEL_VERSION` 来决定编译目标：

```make
ifndef KERNEL_VERSION
KERNEL_VERSION := $(shell uname -r)     # ← 取“当前运行内核”，忽略了 DKMS 传入的 KERNELVER
endif
LINUXDIR ?= /lib/modules/$(KERNEL_VERSION)/build
```

两个变量名对不上，导致 **DKMS 指定的目标内核被无视**：模块总是按“编译当时正在运行的
内核”来构建。于是在“运行着旧内核（6.8）时安装了新内核（7.0）”这一 `apt upgrade`
常见场景下，DKMS 用 6.8 头文件编出模块，却归档到 7.0 的模块树里 —— 版本魔数与符号
CRC 全部错配，模块被内核拒绝加载，显卡因此没有驱动。

**修复**：让 `Makefile` 认 `KERNELVER`（DKMS 的约定变量），仅在手动 `make` 时回退到
`uname -r`。改动仅一行，见 [`patches/0001-dkms-makefile-honor-KERNELVER.patch`](./patches/0001-dkms-makefile-honor-KERNELVER.patch)：

```diff
 ifndef KERNEL_VERSION
-KERNEL_VERSION := $(shell uname -r)
+KERNEL_VERSION := $(if $(KERNELVER),$(KERNELVER),$(shell uname -r))
 endif
```

## 为什么不需要反编译

驱动包里唯一的闭源二进制 `objs/x86_64/lisuan_proprietary.o.hex` 只是两个预编译对象
（`gf_hw_misc.o`、`gf_hw_cmds.o`）的 `ar` 打包，它对外仅依赖 **20 个符号**，其中属于
内核的只有 `__fentry__` 和 `sscanf`——这些跨内核版本**完全稳定**。其余符号（`gf_*`、
`lisuan_*`、`disp_*` 等）全部由**随包附带的开源胶水层**提供。

因此，用正确的内核头文件**重新编译开源部分**即可解决，无需对闭源 blob 做任何逆向。
实测用 `7.0.0-28` 头文件重编：**0 错误**，产物 `vermagic` 正确变为 `7.0.0-28-generic`。

## 快速修复（推荐）

前提：已安装厂商驱动包 `lisuangpu-dkms`（见[下方获取说明](#关于厂商驱动的获取)）、
对应内核头文件与 `dkms`。

```bash
# 修复当前运行内核
sudo ./scripts/fix-lisuan-gpu.sh

# 或：为指定内核构建（例如装了新内核但尚未重启进入）
sudo ./scripts/fix-lisuan-gpu.sh 7.0.0-28-generic
```

脚本会：① 幂等地给 `/usr/src/lisuangpu-dkms-*/Makefile` 打补丁（保留 `.bak`）→
② `dkms remove/build/install` 针对目标内核重编重装 → ③ `depmod` + `modprobe gf` →
④ 打印验证报告。随后可运行只读体检脚本：

```bash
./scripts/verify-lisuan-gpu.sh
```

## 手动修复步骤

若你倾向手动执行：

```bash
# 0) 准备：安装当前内核头文件与 dkms（按需）
sudo apt-get install -y dkms "linux-headers-$(uname -r)"

# 1) 打补丁（从 DKMS 源码根目录）
cd /usr/src/lisuangpu-dkms-262.2.0.56          # 版本号以实际为准
sudo cp -a Makefile Makefile.bak
sudo patch -p1 < /path/to/patches/0001-dkms-makefile-honor-KERNELVER.patch
#   或手动把该行改为：
#   KERNEL_VERSION := $(if $(KERNELVER),$(KERNELVER),$(shell uname -r))

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

## 验证结果

修复成功后应观察到（本环境实测）：

```text
# lsmod
gf                   1081344  24

# lspci -nnk -s 03:00.0
03:00.0 VGA compatible controller [0300]: Device [4c54:5006]
        Subsystem: Device [4c54:5000]
        Kernel driver in use: lisuan          # ← 显卡已被 lisuan 驱动接管

# ls -l /dev/dri/
crw-rw----+ 1 root video  226,   1 card1
crw-rw----+ 1 root render 226, 128 renderD128

# modinfo .../gf.ko.zst
vermagic: 7.0.0-28-generic SMP preempt mod_unload modversions   # ← 与运行内核一致
```

判定成功的标志：出现 `Kernel driver in use: lisuan`，且 `/dev/dri/` 下有 `cardN` 与
`renderD128`。建议随后重启一次，让显示栈以正确驱动干净启动。

## 关于厂商驱动的获取

> **本仓库不分发任何厂商专有文件**（`.deb` 驱动包、闭源 blob、重编出的 `gf.ko`）。
> 这些内容归砺算 / 整机厂商所有，请通过**官方渠道或设备 OEM** 自行获取。

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

> 其它版本的驱动包同样可能存在该 `KERNELVER` 缺陷，补丁思路一致；校验值仅对上述版本有效。

## 未来内核升级

补丁会写入 `/usr/src/lisuangpu-dkms-*/Makefile`，因此之后每次 `apt` 升级内核触发的
`dkms autoinstall` 都会针对**正确的目标内核**构建，问题不再复发。

注意：**若重新安装 / 升级厂商 `.deb`，`/usr/src` 下的 Makefile 会被覆盖**，需再次执行
`scripts/fix-lisuan-gpu.sh`（脚本幂等，可安全重复运行）。

## 卸载 / 撤销本补丁

> **先分清两件事**：本补丁只修改了源码文件 `/usr/src/lisuangpu-dkms-*/Makefile` 的一行，
> **撤销补丁 ≠ 卸载显卡驱动**。撤销后已装好的 `gf.ko` 仍在使用，显卡照常工作；真正的影响是
> **下次 `apt` 升级内核时会退回原厂 Bug 行为（按运行内核而非目标内核编译），很可能再次黑屏**。
> 因此除非确有需要，一般**不建议撤销**。

### 方式一：用备份还原（推荐）

`scripts/fix-lisuan-gpu.sh` 打补丁前会自动把原始 Makefile 备份为 `Makefile.bak`：

```bash
SRC=$(ls -d /usr/src/lisuangpu-dkms-* | sort -V | tail -1)
sudo cp -a "$SRC/Makefile.bak" "$SRC/Makefile"     # 若存在 .bak 则用它还原
```

### 方式二：反向应用补丁（无 .bak 时）

```bash
cd "$(ls -d /usr/src/lisuangpu-dkms-* | sort -V | tail -1)"
sudo patch -p1 -R < /path/to/patches/0001-dkms-makefile-honor-KERNELVER.patch
```

### 验证已还原

第 4 行应变回原始的 `KERNEL_VERSION := $(shell uname -r)`：

```bash
sed -n '4p' "$(ls -d /usr/src/lisuangpu-dkms-* | sort -V | tail -1)/Makefile"
```

> 撤销补丁不会卸载驱动。若要**彻底卸载整个砺算驱动**（会使显卡失去驱动、可能黑屏，请谨慎）：
> `sudo modprobe -r gf && sudo apt-get remove lisuangpu-dkms lisuangpu-lib`。

## 自动化测试

本仓库自带一套**离线、无硬件依赖**的自动化测试，用于验证补丁的正确性与仓库健康度，
在提交/合并前运行，或在 Gitee Go 流水线中自动执行：

```bash
./tests/run-tests.sh        # 运行全部测试
./tests/run-tests.sh -v     # 详细输出
./tests/run-tests.sh -l     # 列出测试单元
./tests/run-tests.sh patch  # 只跑名字含 "patch" 的测试
```

覆盖范围（不需要真实砺算显卡、不需要 root、不需要 dkms）：

| 测试单元 | 验证内容 |
| --- | --- |
| `test-patch.sh` | `patches/` 中的补丁可被 `patch -p1` 干净应用/反向还原；修复结果与预期 `Makefile.fixed` 字节一致；`git apply` 可用 |
| `test-makefile-logic.sh` | 用真实 `make` 展开变量：**修复前**的 Makefile 忽略 `KERNELVER`（复现 bug），**修复后**的 Makefile 优先采用 `KERNELVER`、无 `KERNELVER` 时回退 `uname -r` |
| `test-fix-script.sh` | `fix-lisuan-gpu.sh` 内置的 perl 补丁逻辑能把 buggy Makefile 变成 fixed、幂等（重复执行无变化）、`已打补丁` 检测正确 |
| `test-scripts-syntax.sh` | 对 `scripts/` 与 `tests/` 下所有 shell 脚本做 `bash -n` 语法检查 |
| `test-repo-health.sh` | 开源合规：必需文件存在、脚本可执行、无厂商专有产物（`*.deb` / `*.ko` / `*.o.hex`）混入仓库、README 引用可解析 |

> 测试夹具 `tests/fixtures/` 仅截取厂商 Makefile 头部与 `KERNEL_VERSION` 选择逻辑相关的
> 片段（同补丁文件中的上下文），不含任何专有实现。

## 仓库结构

```text
lisuan-7g100-ubuntu-fix/
├── README.md
├── LICENSE                                        # Apache-2.0
├── NOTICE                                         # 归属与范围声明
├── .workflow/
│   └── gitee-go-test.yml                           # Gitee Go 流水线：自动跑 tests/
├── patches/
│   └── 0001-dkms-makefile-honor-KERNELVER.patch   # 一行根因补丁（可 patch -p1 / git apply）
├── scripts/
│   ├── fix-lisuan-gpu.sh                           # 一键修复（需 root，幂等）
│   └── verify-lisuan-gpu.sh                        # 只读体检
└── tests/
    ├── run-tests.sh                                # 测试运行器（-v / -l / 过滤）
    ├── lib.sh                                      # 共享断言与临时目录管理
    ├── fixtures/
    │   ├── Makefile.buggy                          # 修复前（有 bug）的 Makefile 片段
    │   └── Makefile.fixed                          # 修复后的预期结果
    ├── test-patch.sh                               # 补丁应用 / 反向 / git apply
    ├── test-makefile-logic.sh                      # KERNELVER 行为（真实 make 展开）
    ├── test-fix-script.sh                          # fix 脚本补丁逻辑与幂等性
    ├── test-scripts-syntax.sh                      # bash -n 语法检查
    └── test-repo-health.sh                         # 开源合规静态检查
```

## 常见问题 FAQ

**Q：`dkms status` 明明是 `installed`，为什么还是没驱动？**
A：`installed` 只表示 DKMS 记录了安装动作，不代表模块能被当前内核加载。请用
`modinfo` 看 `vermagic` 是否与 `uname -r` 一致，用 `lsmod | grep gf` 看是否真的加载。

**Q：开启了 Secure Boot 怎么办？**
A：Secure Boot 下模块需用已注册到 MOK 的密钥签名。Ubuntu 的 DKMS 通常会用
`/var/lib/shim-signed/mok/` 自动签名；如仍被拒，请用 `mokutil` 注册密钥，或在固件中
临时关闭 Secure Boot 后再执行修复。

**Q：`gf.ko` 和 `lisuan` 是什么关系？**
A：内核模块文件名为 `gf.ko`，其内部注册的 DRM/PCI 驱动名为 `lisuan`，所以 `lspci`
里显示 `Kernel driver in use: lisuan` 是正常的。

**Q：构建报缺少头文件 / `dkms` 命令？**
A：`sudo apt-get install -y dkms "linux-headers-$(uname -r)"` 后重试。

## 许可证与免责声明

- 本仓库（补丁、脚本、文档）以 **Apache License 2.0** 授权，见 [`LICENSE`](./LICENSE)。
- 本项目与砺算科技（Lisuan Technology）、海光（Hygon）无隶属或背书关系；相关名称、
  商标归各自持有者所有，此处仅用于标识。
- 砺算 GPU 驱动及其用户态库为第三方专有软件，遵循其各自的授权条款，**不包含**在本仓库中。
- 内核模块操作有风险，请自行评估；作者不对任何数据丢失或硬件问题负责。

## 贡献 / 克隆

```bash
# Gitee
git clone https://gitee.com/<your-namespace>/lisuan-7g100-ubuntu-fix.git
```

欢迎通过 Issue / Pull Request 补充其它砺算型号、其它发行版（如 openEuler、麒麟、UOS）
或其它驱动版本的验证结果。
