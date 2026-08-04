---
kind: build_system
name: Lisuan GPU DKMS 驱动与用户态库打包构建系统
category: build_system
scope:
    - '**'
source_files:
    - _work/src/usr/src/lisuangpu-dkms-262.2.0.56/Makefile
    - _work/src/usr/src/lisuangpu-dkms-262.2.0.56/dkms.conf
    - _work/dkms/DEBIAN/control
    - _work/dkms/DEBIAN/postinst
    - _work/dkms/DEBIAN/prerm
    - _work/dkms/DEBIAN/postrm
    - _work/fix-lisuan-gpu.sh
    - _work/pristine/Makefile
---

该仓库采用 DKMS（Dynamic Kernel Module Support）+ Debian 包的混合构建体系，将 Lisuan GPU 内核模块（gf.ko）与配套用户态库分别打包为 lisuangpu-dkms 和 lisuangpu-lib 两个 .deb 包，供目标系统按内核版本动态编译安装。

## 1. 构建系统与工具链
- 内核模块构建：基于 Linux 内核 Kbuild 子系统，通过顶层 Makefile 调用 make -C $(LINUXDIR) M=`pwd` modules 完成编译。
- DKMS 集成：使用 dkms.conf 声明模块名、安装路径与构建命令，支持多内核版本自动构建。
- Debian 打包：通过 DEBIAN 控制文件（control、postinst、prerm、postrm、md5sums）构建标准 .deb 包。
- 辅助工具：xxd 用于将 .o.hex 二进制数据嵌入内核模块；perl 脚本用于修复 Makefile 中的 KERNELVER 兼容性问题。

## 2. 核心构建文件
- _work/src/usr/src/lisuangpu-dkms-262.2.0.56/Makefile：主构建脚本，定义 DRM 驱动名称、源码路径、编译器标志、依赖对象列表（包含显示子系统、KMS 层、视频编解码等），并处理 x86_64/i386 架构差异。
- _work/src/usr/src/lisuangpu-dkms-262.2.0.56/dkms.conf：DKMS 配置，指定包名、版本号、自动安装开关、模块名（gf）及安装位置（/kernel/drivers/gpu/drm/gf）。
- _work/dkms/DEBIAN/control：Debian 包元数据，声明依赖 dkms 和 xxd。
- _work/dkms/DEBIAN/postinst：安装后脚本，配置模块加载参数、检测发行版并提示重启显示管理器。
- _work/dkms/DEBIAN/prerm/postrm：卸载清理脚本，移除 DKMS 构建产物和配置文件。
- _work/fix-lisuan-gpu.sh：运行时修复脚本，自动修补 Makefile 以正确识别 KERNELVER，重新构建并加载模块。

## 3. 构建架构与约定
- 模块化源码组织：源码按功能划分为 display（显示管线）、kms（KMS 管理层）、video_codec（VC9000 编解码）、include（公共头文件）等目录，Makefile 中通过 -I 路径和对象列表精确管理依赖。
- 内核版本适配：Makefile 通过 KERNEL_VERSION := $(if $(KERNELVER),$(KERNELVER),$(shell uname -r)) 优先使用 DKMS 传入的目标内核版本，否则回退到运行内核。
- 调试与发布模式：通过 DEBUG=1 启用调试符号和优化降级，FAKE_CHIP=1 启用模拟芯片模式。
- 二进制嵌入：使用 objs/x86_64/lisuan_proprietary.o.hex 通过 xxd 转换为 .o 文件嵌入内核模块。

## 4. 约束与规则
- 权限要求：install 目标强制检查 root 权限（whoami != root），防止非特权安装。
- 依赖约束：DKMS 包依赖 dkms 和 xxd 工具，确保构建环境完整。
- 内核兼容性：通过解析内核 Makefile 中的 RHEL_DRM_VERSION 等变量计算 DRM 版本代码，实现跨内核版本兼容。
- 清理规范：clean 目标递归删除各子目录的 .o、.cmd、.mod* 等中间文件，保持源码树整洁。
- 模块安装路径：固定安装到 /lib/modules/$(KERNEL_VERSION)/kernel/drivers/gpu/drm/gf，遵循内核 DRM 子系统命名规范。