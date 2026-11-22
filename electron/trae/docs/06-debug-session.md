# 6. 调试会话记录

> 路径说明：本目录原为 `~/project/trae`，2026-11-22 整体迁入 `lisuan/electron/trae/`；
> 第 6.1 节的工作区路径保留当时原样。

来源：`debug-trae-system-lag.md`（Status: 曾 OPEN）。

## 6.1 复盘步骤
1. 启动 Trae，进入工作区 `/home/ubuntu/project/trae`。
2. 观察界面响应、输入延迟、CPU/内存占用与日志异常。
3. 记录卡顿出现时的系统与 Trae 进程状态。

## 6.2 假设与验证

| ID | 假设 | 可能性 | 结果 |
|----|------|--------|------|
| A | 主/渲染进程 CPU 异常导致 UI 卡顿 | High | 确认：`ps` 显示多 Trae 进程持续占 CPU（zygote ~28.9%、extHost ~5.9%、shared/ai ~3%+） |
| B | 内存/swap 压力致变慢 | Medium | 排除：`free -h` 可用 ~10Gi，swap 0，`vmstat` 无 si/so |
| C | GPU/图形加速/Electron 渲染链路异常致掉帧 | Medium |  inconclusive：有 GPU/WebGPU 缓存文件但无明确 GPU crash（后于显示错误修复中确认 gf EGL 为根因） |
| D | 后台日志/索引/插件任务阻塞主线程或 I/O | Medium | 确认：shared process 异常后持续重复扩展扫描；renderer 中 ShellExec/扩展/Git 错误密集 |
| E | 系统磁盘 I/O / 文件监控过载 | Low | 弱/排除：`iostat` iowait 极低，fileWatcher 仅一次被 killed |

## 6.3 验证结论
- 根因更接近 **Trae 自身 shared process / 扩展管理链路异常**，而非系统 CPU、内存或磁盘资源不足。
- 高概率触发链路：
  1. shared process 收到 `undefined` 输入并抛未捕获异常；
  2. 触发扩展扫描与 gallery manifest 查询的重复重试；
  3. 伴随 ShellExec / Git model / fileWatcher / 工具卡片解析失败，扩展宿主/共享进程反复忙碌；
  4. 最终表现为 UI 卡顿与多子进程持续占 CPU。

## 6.4 后续补充
- 2026-08-26 新增"显示错误"独立链路（假设 C 的延伸），根因确认为 7G100 gf EGL 在 Wayland 下不兼容，修复见 `04-display-error-fix.md`。
