# vendor-src — 砺算 DKMS 驱动源码（原样转录 + 根因补丁）

本目录是厂商 DKMS 驱动包 **lisuangpu-dkms 262.2.0.56** 中内核源码的
**原样转录**（未做改动，`Makefile` 保持出厂状态），供存档、审计与补丁
验证使用。根因修复以补丁形式独立存放：
[`../patches/0001-dkms-makefile-honor-KERNELVER.patch`](../patches/0001-dkms-makefile-honor-KERNELVER.patch)
（在本目录 `patch -p1` 可干净应用，见 `tests/test-patch.sh`）。

## 来源与校验

| 项目 | 值 |
|---|---|
| 源包 | `lisuangpu-dkms-262.2.0.56_amd64.deb` |
| SHA-256 | `8277dc9c8975add6182e6487415529ea7850e045eed0cd144b54c13f9663807c` |
| 转录路径 | 包内 `usr/src/lisuangpu-dkms-262.2.0.56/` |
| 内容 | 69 个 `.c` + 99 个 `.h` + `Makefile`/`dkms.conf`/`VERSION`/`copyright` |

## 授权

- 包 `copyright` 声明：*"This program is free software; you can redistribute
  it and/or modify it under the terms of the GNU General Public License …
  version 2"*（GPLv2，见本目录 `copyright`）。
- 各源文件头标注：`@License Dual MIT/GPLv2`。
- 据此本转录以 **GPLv2** 随仓库分发。

## 不包含的内容

厂商包内的闭源二进制 **`objs/x86_64/lisuan_proprietary.o.hex`**（两个预编译
硬件对象 `gf_hw_misc.o`/`gf_hw_cmds.o` 的 ar 打包）及其一切二进制产物
（`*.o`/`*.ko`/构建日志）**不在**本目录与仓库中，其权利仍归砺算科技所有。
该 blob 仅依赖稳定内核符号（`__fentry__`/`sscanf`），重编开源部分即可在
新内核上构建模块，无需逆向（详见根 README "为什么不需要反编译"节）。

## 已知缺陷（出厂状态）

`Makefile` 读 `KERNEL_VERSION`（缺省 `uname -r`），而 `dkms.conf` 传的是
`KERNELVER`——内核升级后 DKMS 会按运行内核编译、归档到目标内核目录，
导致 vermagic 错配、模块无法加载。修复见上述补丁与 `scripts/fix-lisuan-gpu.sh`。
