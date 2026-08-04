---
kind: logging_system
name: Lisuan GPU 内核驱动日志系统
category: logging_system
scope:
    - '**'
source_files:
    - _work/src/usr/src/lisuangpu-dkms-262.2.0.56/log_control.c
    - _work/src/usr/src/lisuangpu-dkms-262.2.0.56/log_control.h
    - _work/src/usr/src/lisuangpu-dkms-262.2.0.56/gf_os_interface.c
    - _work/src/usr/src/lisuangpu-dkms-262.2.0.56/gf_os_interface.h
    - _work/src/usr/src/lisuangpu-dkms-262.2.0.56/display/disp_callback.h
---

该仓库的 Lisuan GPU DKMS 驱动采用两套并行的日志子系统：一套面向显示子系统的 disp_print 宏体系，另一套面向通用驱动层的 gf_printk 体系，均通过 procfs 提供运行时开关控制。

显示子系统日志（disp_print）
- 级别定义：在 display/disp_callback.h 中定义 DDM_ERROR/DDM_WARNING/DDM_KEY/DDM_INFO/DDM_TRACE 五级，数值越小优先级越高。
- 模块掩码：同一头文件定义了 DISP_PRINT_MASK_* 位掩码（CRTC、PLANE、PIPE、ENCODER、CONNECTOR、EDID、DP_AUX、PWM、HDAUDIO、VBT、INTR、DEVICE、COMMON 等），每个显示子模块通过 #define disp_print(level, log, ...) disp_print_ex(MASK, level, log, ##__VA_ARGS__) 绑定自身掩码。
- 输出函数：disp_print_ex / disp_print_pure_ex / disp_print_to_file_ex 三个变体，支持普通打印与额外写入文件。
- 使用方式：各子模块（如 disp_connector.c、disp_dp_aux.c、disp_edid.c 等）统一用 disp_print(log_level, "...") 调用，其中 log_level 多为 DDM_TRACE 或 DDM_INFO。

通用驱动层日志（gf_printk）
- 级别定义：在 gf_os_interface.h 中定义 GF_DRV_EMERG(0x00) < GF_DRV_ERROR(0x01) < GF_DRV_INFO(0x02) < GF_DRV_WARNING(0x03) < GF_DRV_DEBUG(0x04)，数值越大优先级越低。
- 封装实现：gf_os_interface.c 中的 gf_printk() 根据 msgtype 映射到内核 printk(KERN_*)，并在消息前加 LS 前缀；所有调用需满足 msgtype <= print_level 才输出。
- 便捷宏：gf_emerg/gf_error/gf_warning/gf_info/gf_debug 五个宏直接调用 gf_printk，被 kms、scheduler、ringbuffer 等广泛使用。

运行时控制（procfs）
- log_control 接口：log_control.c 通过 proc_create("log_control", ...) 暴露 /proc/log_control，写 1 启用日志、写 0 禁用，读返回当前状态。内部维护全局 log_enabled 变量。
- 初始化/退出：通过 gf_kmdEvent_log_control_init() 和 gf_kmdEvent_log_control_exit() 注册/注销 proc 节点，适配不同内核版本的 proc_ops / file_operations。
- 双重入口：gf_kmd_event.c 中也存在对 /proc/log_control 的直接读写处理，形成重复实现。

其他调试输出
- 命令/缓冲区转储：gf_dump_commands() 和 gf_dump_buffer_to_file() 将 GPU 命令或内存缓冲区以十六进制形式打印到内核日志或 /tmp/*.txt 文件。
- assert：gf_assert() 失败时直接 BUG()

架构特点与约束
- 两套日志体系并存但互不依赖：显示子系统走 disp_print，核心驱动走 gf_printk。
- 日志级别按“数值越小越紧急”（disp）或“数值越大越详细”（gf）两种约定混用，缺乏统一规范。
- 没有结构化日志字段或集中式 sink，所有输出最终落入内核 dmesg。
- 运行时开关仅支持全局 on/off，不支持按模块掩码动态过滤。