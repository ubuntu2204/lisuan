# 5. 上报邮件与上游状态

## 5.1 已发出反馈（2026-08-04）
上报邮件模板见 `bug-report-artifacts/trae-feedback-email.txt`，要点：

- 主题：`[Bug][Linux][Trae 3.5.81] UI lag and high CPU caused by shared process exception`
- 环境：Trae `3.5.81` / VS Code base `1.107.1` / Ubuntu 24.04.4 LTS / kernel `7.0.0-28-generic`
- 核心证据：
  - `sharedprocess.log`：`TypeError [ERR_INVALID_ARG_TYPE] ... Received undefined`
  - 异常后重复：`Created extension scanner input ...` / `get extension gallery manifest ...`
  - `main.log` / `renderer.log`：`cue-server ping failed response_timeout`、`fileWatcher crashed`、`ShellExec initLoginShellSnapshot failed`、`Activating extension 'vscode.git' failed: Git model not found`
- 诉求：排查 Linux 下 shared process / 扩展扫描路径的异常与重试策略。

## 5.2 建议官方修复点
1. `sharedProcessMain.js` 中 `Buffer.from` 上游调用链，确认何处可能传入 `undefined`。
2. 用户扩展注册表为空数组 `[]` 时的兼容行为。
3. shared process 异常后的重试策略，避免无限 / 高频重新扫描扩展。
4. `ShellExec` / `Git model` / `ShellToolCard` 缺失命令是否由 shared process 异常诱发。

## 5.3 上游状态
- 截至 2026-08-26：本机 Trae 已自动更新至 `3.5.91`，但 shared process / 扩展扫描结构未变，卡顿链路根因大概率仍存在。
- 显示错误（gf EGL）属本机显卡栈兼容问题，非 Trae 上游可单方面修复；本仓库以软件渲染规避（见 `04-display-error-fix.md`）。

## 5.4 附带材料
- `bug-report-artifacts/trae-bug-report-20260804.tar.gz`：完整 Trae 日志归档。
- `bug-report-artifacts/trae-feedback-email.txt`：上报邮件正文。
