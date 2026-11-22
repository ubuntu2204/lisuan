# Debug Session: trae-system-lag

> 路径说明：原始调试记录写于 `~/project/trae`；该目录 2026-11-22 已整体迁入
> `lisuan/electron/trae/`。下文工作区与日志路径保留当时原样，不逐处改写。
- **Status**: [OPEN]
- **Issue**: Trae 在当前 Linux 系统上出现明显卡顿，需分析可能原因并整理可上报的缺陷信息。
- **Debug Server**: pending
- **Log File**: .dbg/trae-debug-log-trae-system-lag.ndjson

## Reproduction Steps
1. 启动 Trae。
2. 在日常使用过程中观察界面响应、输入延迟、CPU/内存占用和日志异常。
3. 记录卡顿出现时的系统资源状态与 Trae 进程状态。

## Hypotheses & Verification
| ID | Hypothesis | Likelihood | Effort | Evidence |
|----|------------|------------|--------|----------|
| A | Trae 主进程或渲染进程 CPU 占用异常，导致 UI 卡顿 | High | Low | Confirmed: `ps` 显示多个 Trae 进程持续占用 CPU，zygote 一度约 28.9%，扩展宿主约 5.9%，共享/AI 服务约 3%+ |
| B | 系统内存或 swap 压力过高，Trae 被动变慢 | Medium | Low | Rejected: `free -h` 显示可用内存约 10Gi，swap 0B，`vmstat` 无 si/so |
| C | GPU/图形加速或 Electron 渲染链路异常，引发界面掉帧 | Medium | Medium | Inconclusive: 存在 GPU/WebGPU 缓存文件，但日志未出现明确 GPU crash；仍可作为规避项验证 |
| D | Trae 后台日志/索引/插件任务阻塞主线程或 I/O | Medium | Medium | Confirmed: shared process 未捕获异常后，持续重复扩展扫描；renderer 中 ShellExec/扩展/Git 相关错误密集出现 |
| E | 系统级磁盘 I/O 或文件监控过载，放大 Trae 的响应延迟 | Low | Medium | Rejected/Weak: `iostat` 显示整体 iowait 很低，fileWatcher 有一次被 killed，但不像系统磁盘瓶颈 |

## Log Evidence
- 进程资源：
  - `ps -eo ... --sort=-%cpu` 中 Trae 相关进程持续居高，包含主进程、zygote、extensionHost、shared-process、ai 相关 utility。
- 系统资源：
  - `free -h`：15Gi 内存中可用约 10Gi，swap 未使用。
  - `vmstat` / `iostat`：CPU idle 约 96%，iowait 极低，说明整机未处于资源耗尽。
- 关键日志：
  - `sharedprocess.log` 出现未捕获异常：`TypeError [ERR_INVALID_ARG_TYPE] ... Received undefined`。
  - 同一日志紧随其后反复出现 `Created extension scanner input ...` 与 `get extension gallery manifest ...`，符合异常后重复扩展扫描/重试。
  - `renderer.log` 中出现：
    - `ShellExec initLoginShellSnapshot failed (attempt 1/3...3/3): command 'icube.shellExec.initShell' not found`
    - `checkBeforeSendMessage timeout`
    - `Activating extension 'vscode.git' failed: Git model not found`
    - 多次 `ShellToolCard missing command`
  - `main.log` 中出现：
    - `cue-server ping failed response_timeout`
    - `register user failed ... code:500`
    - `fileWatcher ... crashed with code 15`

## Verification Conclusion
- 根因更接近 **Trae 自身 shared process / extension 管理链路异常**，而不是系统 CPU、内存或磁盘资源不足。
- 高概率触发链路：
  1. shared process 在扩展/扫描相关路径上收到 `undefined` 输入并抛出未捕获异常；
  2. 异常后触发扩展扫描与 gallery manifest 查询的重复重试；
  3. 同时伴随 ShellExec、Git 模型、fileWatcher、部分工具卡片解析失败，造成扩展宿主/共享进程反复忙碌；
  4. 最终表现为 Trae UI 卡顿和多个 Trae 子进程持续占用 CPU。
