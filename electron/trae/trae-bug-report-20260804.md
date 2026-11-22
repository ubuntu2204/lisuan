# Trae Bug Report: Linux 卡顿 / 高 CPU / 扩展扫描重复

> 路径说明：原始报告写于 `~/project/trae`（2026-08-04）；该目录 2026-11-22 已整体迁入
> `lisuan/electron/trae/`。下文工作区路径与日志路径保留当时原样，不逐处改写。

## Summary
在 Ubuntu 24.04.4 LTS 上，Trae `3.5.81` 启动并进入工作区后出现明显卡顿。排查显示并非系统资源不足，而是 Trae 自身 `shared process`、扩展扫描和若干扩展/工具链路异常，导致多个 Trae 子进程持续占用 CPU。

## Environment
- Product: Trae
- App version: `3.5.81`
- VS Code base version: `1.107.1`
- Build version: `2.3.61406`
- OS: Ubuntu 24.04.4 LTS
- Kernel: `7.0.0-28-generic`
- Session type: Wayland
- User settings: `/home/ubuntu/.config/Trae/User/settings.json` 当前为空对象 `{}` 
- User extension registry: `/home/ubuntu/.trae/extensions/extensions.json` 当前为空数组 `[]`

## Symptom
- IDE 出现明显卡顿
- Trae 多个子进程持续占用 CPU
- AI / Shell / Git 相关功能伴随超时、命令缺失或初始化失败

## Reproduction
1. 启动 Trae。
2. 打开工作区 `/home/ubuntu/project/trae`。
3. 在 Agent/AI、Shell 工具、工作区扫描相关路径活跃后，观察到界面卡顿。
4. 同时查看 Trae 日志与系统进程，可见异常和高 CPU 现象。

## Expected
- Trae 在常规工作区中保持流畅。
- shared process 不应因无效输入抛出未捕获异常。
- 扩展扫描不应在异常后进入高频重复执行。
- ShellExec / Git / 工具卡片链路不应连续报错并拖慢 UI。

## Actual
- `sharedprocess.log` 出现未捕获异常：
  - `TypeError [ERR_INVALID_ARG_TYPE]: The first argument must be of type string ... Received undefined`
- 异常后 shared process 持续重复：
  - `Created extension scanner input ...`
  - `get extension gallery manifest ...`
- renderer 同时出现：
  - `ShellExec initLoginShellSnapshot failed ... command 'icube.shellExec.initShell' not found`
  - `checkBeforeSendMessage timeout`
  - `Activating extension 'vscode.git' failed: Git model not found`
  - `ShellToolCard missing command`
- `main.log` 中还有：
  - `cue-server ping failed response_timeout`
  - `fileWatcher ... crashed with code 15`

## Why this looks like a Trae bug, not a machine issue
- 内存充足：`free -h` 显示可用内存约 `10Gi`，swap `0B`
- 无明显磁盘瓶颈：`iostat` / `vmstat` 显示 `iowait` 很低
- 但 Trae 相关进程持续占用 CPU：
  - zygote 进程一度约 `28.9%`
  - extension host 约 `5.9%`
  - shared process / ai / ckg 相关进程均有持续占用

## Key Evidence
- Shared process uncaught exception:
  - `/home/ubuntu/.config/Trae/logs/20260804T022015/sharedprocess.log:7`
- Repeated extension scanning after exception:
  - `/home/ubuntu/.config/Trae/logs/20260804T022015/sharedprocess.log:42-60`
  - `/home/ubuntu/.config/Trae/logs/20260804T022015/sharedprocess.log:72-80`
- ShellExec repeated failures:
  - `/home/ubuntu/.config/Trae/logs/20260804T022015/window1/renderer.log` around line `2167`
- Git activation failure:
  - `/home/ubuntu/.config/Trae/logs/20260804T022015/window1/renderer.log` around line `3230`
- cue-server timeout / fileWatcher crash:
  - `/home/ubuntu/.config/Trae/logs/20260804T022015/main.log:160`
  - `/home/ubuntu/.config/Trae/logs/20260804T022015/main.log:217`

## Suspected Root Cause
`shared process` 在处理扩展扫描或扩展注册表相关输入时拿到了 `undefined`，触发 `Buffer.from(undefined)` 类异常。异常后未完全短路，导致扩展扫描、gallery manifest 查询以及若干依赖 shared process/extension host 的功能反复重试，最终造成 CPU 持续占用和 UI 卡顿。

## Suggested Investigation
1. 检查 `sharedProcessMain.js` 中 `Buffer.from` 上游调用链，确认何处可能传入 `undefined`。
2. 检查用户扩展注册表为空数组 `[]` 时的兼容行为。
3. 检查 shared process 异常后的重试策略，避免无限/高频重新扫描扩展。
4. 检查 `ShellExec`、`Git model`、`ShellToolCard` 缺失命令是否由 shared process 异常诱发。

## Attachments Recommended
- `/home/ubuntu/.config/Trae/logs/20260804T022015/sharedprocess.log`
- `/home/ubuntu/.config/Trae/logs/20260804T022015/main.log`
- `/home/ubuntu/.config/Trae/logs/20260804T022015/window1/renderer.log`
- 进程资源截图或 `ps` 结果
