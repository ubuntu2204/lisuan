# 1. 症状与环境

## 1.1 受影响版本

| 项 | 值 |
|----|----|
| 产品 | Trae（AI 编程 IDE，字节跳动） |
| 原始报告版本 | `3.5.81`（VS Code base `1.107.1`，build `2.3.61406`） |
| 当前安装版本 | `3.5.91`（build `2.3.73738`，product date `2026-08-18`） |
| 操作系统 | Ubuntu 24.04.4 LTS |
| 内核 | `7.0.0-28-generic` → `7.0.0-30-generic`（后续更新） |
| 会话类型 | **Wayland**（`XDG_SESSION_TYPE=wayland`，`WAYLAND_DISPLAY=wayland-0`；`DISPLAY=:0`，`GDK_BACKEND=x11`） |
| 显卡 | 砺算 7G100（client GLX vendor = `LISUAN`；DRM 渲染节点 `/dev/dri/renderD128`） |

> 注意：本报告初版基于 `3.5.81`，但本机后续已自动更新到 `3.5.91`。两版的 shared process / 扩展扫描链路结构一致，根因未变。

## 1.2 两类症状

### A. 卡顿 / 高 CPU（2026-08-04 报告）
- IDE 进入工作区后出现明显卡顿、输入延迟。
- 多个 Trae 子进程持续占用 CPU：
  - zygote 一度约 `28.9%`
  - extension host 约 `5.9%`
  - shared process / ai / ckg 相关进程均有持续占用。
- 内存充足（`free -h` 可用约 `10Gi`，swap `0B`），`iowait` 很低，整机未资源耗尽。

### B. 显示错误（后续复现，7G100 + Wayland 环境）
- 在 Wayland 会话下，Trae 窗口出现黑屏 / 花屏 / 内容不刷新 / 闪烁等渲染异常。
- 根因见 `02-root-cause-analysis.md`，修复见 `04-display-error-fix.md`。

## 1.3 关联功能异常（renderer / main 日志）
- `ShellExec initLoginShellSnapshot failed: command 'icube.shellExec.initShell' not found`
- `checkBeforeSendMessage timeout`
- `Activating extension 'vscode.git' failed: Git model not found`
- `ShellToolCard missing command`
- `cue-server ping failed response_timeout`
- `fileWatcher ... crashed with code 15`
- `register user failed ... code:500`（login 态相关）
