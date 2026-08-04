---
kind: error_handling
name: Lisuan GPU 驱动错误处理机制
category: error_handling
scope:
    - '**'
source_files:
    - _work/src/usr/src/lisuangpu-dkms-262.2.0.56/display/display.h
    - _work/src/usr/src/lisuangpu-dkms-262.2.0.56/display/disp_callback.h
    - _work/src/usr/src/lisuangpu-dkms-262.2.0.56/log_control.c
    - _work/src/usr/src/lisuangpu-dkms-262.2.0.56/gf_os_interface.c
    - _work/src/usr/src/lisuangpu-dkms-262.2.0.56/display/connector/disp_connector.c
    - _work/src/usr/src/lisuangpu-dkms-262.2.0.56/display/connector/disp_dp_aux.c
---

该仓库的 Lisuan GPU DKMS 内核模块采用**自定义 DISP_STATUS 枚举 + 负 errno 值返回**的错误处理模式，结合分层日志系统实现错误传播与诊断。

## 1. 错误码体系
- **DISP_STATUS 枚举**（定义于 `display/display.h`）：包含标准 POSIX 风格错误码（如 `DISP_EIO`、`DISP_ENOMEM`、`DISP_EINVAL`、`DISP_ETIMEDOUT`、`DISP_EBUSY`、`DISP_ENODEV`、`DISP_ERANGE`、`DISP_ENREADY`、`DISP_ENSUPPORT` 等），以及私有错误码（`DISP_EPRIV_BEGIN=100` 起，含 `DISP_EPRIV_UNKNOWN`、`DISP_EPRIV_DEVICE_ID`、`DISP_EPRIV_INEDID`、`DISP_EPRIV_INTERNAL`、`DISP_EREMOTEIO`、`DISP_ETIMEDOUT`、`DISP_EPROTO`）。
- **成功/失败约定**：函数返回 `DISP_OK=0` 表示成功，负值表示错误；调用方通过检查返回值 `< 0` 判断失败。
- **POSIX 兼容**：错误码命名遵循 Linux 内核惯例（如 `-EIO`、`-ENOMEM`），便于与内核子系统交互。

## 2. 错误传播模式
- **逐层返回**：显示子系统各层（connector → encoder → crtc → pipe）函数均返回 `DISP_STATUS`，上层调用者检查返回值并向上层传递。
- **早期返回**：参数校验失败时立即返回 `-DISP_EINVAL`，避免无效操作。
- **资源清理**：失败路径中需释放已分配资源（如内存、锁），但代码中未见统一的 goto cleanup 模式，多为直接 return。

## 3. 日志与调试
- **分层日志宏**：每个源文件通过 `#define disp_print(level, log, ...)` 绑定到 `disp_print_ex()`，支持 `DDM_ERROR`、`DDM_WARNING`、`DDM_INFO`、`DDM_TRACE` 四级日志。
- **可插拔日志后端**：通过 `disp_callback_t` 结构体注入 `pdisp_fn_print`、`pdisp_fn_print_to_file` 等回调，允许运行时替换日志输出目标。
- **内核日志桥接**：`gf_os_interface.c` 中的 `gf_printk()` 将内部日志映射到 `printk(KERN_ERR|KERN_WARNING|KERN_INFO|KERN_DEBUG|KERN_EMERG)`。
- **运行时开关**：通过 `/proc/log_control` 接口动态启用/禁用日志（`log_enabled` 全局变量）。

## 4. 硬件错误处理
- **I²C/AUX 通信错误**：DP AUX 通道读写失败返回 `-DISP_EIO`，超时返回 `-DISP_ETIMEDOUT`，忙状态返回 `-DISP_EBUSY`。
- **设备状态检查**：未就绪设备返回 `-DISP_ENREADY`，不支持功能返回 `-DISP_ENSUPPORT`。
- **中断处理**：`disp_intr.c` 处理硬件中断，错误通过日志记录而非异常抛出。

## 5. 关键约束与约定
- **禁止 panic/recover**：内核模块中未发现 `panic()` 或 `recover()` 使用，所有错误通过返回值传播。
- **无统一错误类型**：不使用 C++ 异常或结构化错误对象，仅依赖整数错误码。
- **日志级别强制**：错误路径必须配合 `DDM_ERROR` 日志，警告路径使用 `DDM_WARNING`，便于问题定位。
- **用户态交互**：DKMS 包不包含用户态库的错误处理代码（用户态库在独立 .deb 包中），内核态错误通过 DRM/KMS 框架向用户态传递。

## 6. 典型错误场景
- EDID 读取失败：`disp_connector_probe_edid()` 返回 `-DISP_EIO` 并记录 `DDM_WARNING`。
- DP AUX 传输失败：`disp_dp_aux_read/write()` 返回 `-DISP_EIO` 并打印详细寄存器状态。
- 内存分配失败：多处检查 `kmalloc` 返回值，失败时返回 `-DISP_ENOMEM`。
- 参数非法：入口函数验证输入参数，非法时返回 `-DISP_EINVAL`。