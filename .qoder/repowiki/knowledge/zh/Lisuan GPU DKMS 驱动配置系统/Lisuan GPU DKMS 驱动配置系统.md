---
kind: configuration_system
name: Lisuan GPU DKMS 驱动配置系统
category: configuration_system
scope:
    - '**'
source_files:
    - _work/src/usr/src/lisuangpu-dkms-262.2.0.56/lisuan_drv.c
    - _work/dkms/DEBIAN/postinst
    - _work/dkms/DEBIAN/prerm
    - _work/src/usr/src/lisuangpu-dkms-262.2.0.56/Makefile
    - _work/src/usr/src/lisuangpu-dkms-262.2.0.56/dkms.conf
    - _work/src/usr/src/lisuangpu-dkms-262.2.0.56/log_control.c
    - _work/src/usr/src/lisuangpu-dkms-262.2.0.56/include/driver_settings.h
    - _work/src/usr/src/lisuangpu-dkms-262.2.0.56/VERSION
---

该仓库的 Lisuan GPU 驱动采用内核模块参数（module_param）作为核心运行时配置机制，配合 Debian 包管理器的 postinst/prerm 脚本实现安装时默认配置注入，以及通过 procfs 提供动态日志开关控制。具体架构如下：

**1. 内核模块参数（module_param）**
所有可配置的运行时选项均通过 `module_param()` 宏在 `lisuan_drv.c` 中集中声明，包括打印级别、fbdev 启用、bypass_only、mhb_disable_mask、cluster_slice_mask、recovery_enable、enable_clock_gating、no_gfx/no_gce/no_vem/no_video 等硬件功能开关，以及 lisuan_debug、endebug、dp_cts、display_cts、vram_only、pz1、mode_8k 等调试与特性标志。每个参数都配有 `MODULE_PARM_DESC` 描述，权限统一设为 0600（仅 root 可读写）。

**2. 安装时默认配置注入**
Debian 包的 `postinst` 脚本在安装后自动写入两个配置文件：
- `/etc/modules-load.d/gf.conf`：确保模块 gf 被自动加载
- `/etc/modprobe.d/gf.conf`：写入默认模块参数 `options gf dump_rb_cmd=0 cpu_update_pte=0 paging_rb=1 mhb_disable_mask=0x0 cluster_slice_mask=0xFFFFFF recovery_enable=1 no_vem=0 no_video=0 bypass_only=0 print_level=2 validation_chip=0`
这些参数随内核启动时由 modprobe 传递给模块，形成持久化配置。

**3. 运行时动态配置（procfs）**
`log_control.c` 通过 `proc_create("log_control", ...)` 暴露 `/proc/log_control` 接口，支持写入 '1' 或 '0' 动态启用/禁用日志输出，变量 `log_enabled` 供其他模块读取。该接口使用 `proc_ops`（Linux 5.6+）或兼容 `file_operations` 结构。

**4. 构建期配置（Makefile）**
`Makefile` 通过 `EXTRA_CFLAGS` 和编译宏控制行为：`DEBUG=1` 开启调试符号与 `-D_DEBUG_`，`FAKE_CHIP=1` 启用仿真芯片模式，`HANTROVCMD_ENABLE_IP_SUPPORT`、`HANTROAXIFE_SUPPORT` 等宏控制视频编解码子系统的功能开关。DKMS 通过 `dkms.conf` 指定模块名 `gf`、安装路径 `/kernel/drivers/gpu/drm/gf`，并传入 `$kernelver` 目标内核版本。

**5. 配置数据结构**
`include/driver_settings.h` 定义 `driver_settings_s` 结构体，包含 null_rendering、force_local/nonlocal、cmd_buffer_no_overlap、per_draw_kickoff、pcnt_*、chip_image_dump、dump_path 等运行时可调设置字段，为上层应用或工具预留扩展点。

**约束与约定：**
- 所有 module_param 权限固定为 0600，禁止非 root 用户修改
- 模块参数命名采用小写蛇形，布尔型使用 int 类型（0/1）
- 默认配置集中在 postinst 脚本中硬编码，未使用外部配置文件格式（如 .yaml/.toml/.env）
- 无环境变量或命令行参数解析逻辑（C 代码中未见 getopt/argv 处理）
- 版本信息通过 `VERSION` 文件中的 `PACKAGE_VERSION` 宏与 `dkms.conf` 中的版本号保持一致