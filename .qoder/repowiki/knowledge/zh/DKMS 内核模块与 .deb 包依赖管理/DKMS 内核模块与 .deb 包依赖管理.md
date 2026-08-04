---
kind: dependency_management
name: DKMS 内核模块与 .deb 包依赖管理
category: dependency_management
scope:
    - '**'
source_files:
    - _work/src/usr/src/lisuangpu-dkms-262.2.0.56/dkms.conf
    - _work/src/usr/src/lisuangpu-dkms-262.2.0.56/Makefile
    - _work/pristine/Makefile
    - _work/dkms/DEBIAN/control
    - _work/dkms/DEBIAN/postinst
    - _work/dkms/DEBIAN/postrm
    - _work/dkms/DEBIAN/prerm
    - lisuangpu-dkms-262.2.0.56_amd64.deb
    - lisuangpu-lib-262.2.0.56_amd64.deb
---

本仓库不包含 Go/Node.js/Python/Cargo 等用户态语言包的依赖声明文件（如 go.mod、package.json、requirements.txt、Cargo.toml）。项目的“依赖管理”完全围绕 Linux 内核 DKMS（Dynamic Kernel Module Support）机制展开，通过 Makefile 和 dkms.conf 将 Lisuan GPU 驱动源码按目标内核版本动态编译为 gf.ko 模块，并打包为 lisuangpu-dkms 与 lisuangpu-lib 两个 .deb 包。

**系统与工具**
- 构建系统：纯 Makefile + DKMS。dkms.conf 声明 PACKAGE_NAME、PACKAGE_VERSION、AUTOINSTALL 以及 BUILT_MODULE_NAME/DEST_MODULE_LOCATION，由 DKMS 在目标系统上根据运行内核自动触发 make 构建。
- 包格式：最终产物为 lisuangpu-dkms-262.2.0.56_amd64.deb 与 lisuangpu-lib-262.2.0.56_amd64.deb，分别承载内核模块与配套用户态库。
- 外部依赖：仅依赖目标系统的内核头文件（/lib/modules/$KERNEL_VERSION/build），Makefile 通过探测 LINUXDIR 与 RHEL_DRM_* 变量适配不同发行版的 DRM 版本。

**关键文件与位置**
- _work/src/usr/src/lisuangpu-dkms-262.2.0.56/dkms.conf：DKMS 元数据，指定模块名 gf、安装路径 /kernel/drivers/gpu/drm/gf、MAKE 命令传入 KERNELVER。
- _work/src/usr/src/lisuangpu-dkms-262.2.0.56/Makefile：主构建脚本，解析 KERNELVER/uname -r 确定内核目录，收集 display/kms/video_codec 等子树源文件到 $(DRM_DRIVER_NAME)-objs，生成 gf.ko。
- _work/pristine/Makefile：与上述 Makefile 几乎一致，用于 pristine 构建或独立编译。
- _work/dkms/DEBIAN/：Debian 包控制目录（当前为空，实际 control/md5sums/postinst/postrm/prerm 位于顶层 _work/_work/dkms/DEBIAN/ 的对应文件中）。
- 根目录 lisuangpu-dkms-262.2.0.56_amd64.deb、lisuangpu-lib-262.2.0.56_amd64.deb：已构建好的二进制包。

**架构与约定**
- 单模块多子系统：所有 C 源文件（lisuan_*.c、gf_*.c、display/*、kms/*、video_codec/*、objs/x86_64/*.hex）统一由一个 Makefile 汇总为单一 gf.ko 内核模块，无分层包管理器。
- 内核头依赖：通过 `LINUXDIR ?= /lib/modules/$(KERNEL_VERSION)/build` 指向目标内核 build 目录，依赖发行版提供的 linux-headers-* 包。
- 版本耦合：dkms.conf 中 PACKAGE_VERSION="262.2.0.56" 与 .deb 文件名中的版本号保持一致；Makefile 通过 sed 读取内核 Makefile 中的 RHEL_DRM_VERSION/PATCH/SUBLEVEL 计算 DRM_CODE，以条件编译兼容不同内核 DRM API。
- 构建隔离：clean 目标会递归删除各子目录下的 .o、.cmd、*.mod* 等中间产物，确保每次构建从干净状态开始。

**约束与规则**
- 构建必须使用 root 权限：install 目标显式检查 whoami，非 root 直接 exit 1。
- DKMS 自动安装：AUTOINSTALL="yes" 使 dkms autoinstall 在系统启动时自动重建匹配内核的模块。
- 目标内核选择优先级：DKMS 调用时通过 KERNELVER 传入目标内核，Makefile 优先使用该值而非 uname -r，保证跨内核构建正确性。
- 二进制固件内嵌：objs/x86_64/lisuan_proprietary.o.hex 通过 xxd 反转为 .o 后链接进 gf.ko，不依赖外部固件加载。
- 无第三方包管理器：仓库未使用任何语言级依赖管理工具，所有“依赖”均为系统内核头文件与标准 C 工具链（gcc、make、xxd）。