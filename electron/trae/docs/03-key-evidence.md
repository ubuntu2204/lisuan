# 3. 关键日志证据

所有证据来自 `/home/ubuntu/.config/Trae/logs/20260804T022015/`（初版报告），以及本机 2026-08-26 复现启动日志。

## 3.1 shared process 未捕获异常
- 文件：`sharedprocess.log:7`
- 内容：
  ```
  TypeError [ERR_INVALID_ARG_TYPE]: The first argument must be of type string ...
      Received undefined
  ```

## 3.2 异常后的高频重复扫描
- 文件：`sharedprocess.log:42-60`、`sharedprocess.log:72-80`
- 重复打印：
  ```
  Created extension scanner input ...
  get extension gallery manifest ...
  ```

## 3.3 renderer 中工具链失败
- 文件：`window1/renderer.log`（约 line 2167 附近）
- 内容：
  ```
  ShellExec initLoginShellSnapshot failed (attempt 1/3 ... 3/3): command 'icube.shellExec.initShell' not found
  checkBeforeSendMessage timeout
  ShellToolCard missing command
  ```
- Git 激活失败（约 line 3230 附近）：
  ```
  Activivating extension 'vscode.git' failed: Git model not found
  ```

## 3.4 main 进程级异常
- 文件：`main.log:160`、`main.log:217`
- 内容：
  ```
  cue-server ping failed response_timeout
  fileWatcher ... crashed with code 15
  register user failed ... code:500
  ```

## 3.5 进程资源证据（ps）
```
zygote            ~28.9% CPU
extension host    ~ 5.9% CPU
shared-process   持续占用
ai / ckg 进程     持续占用
```
整机：`free -h` 可用 ~10Gi，`vmstat`/`iostat` 显示 idle ~96%、`iowait` 极低 → 非系统资源瓶颈。

## 3.6 显示错误相关（2026-08-26 复现）
- `glxinfo`：`client glx vendor string: LISUAN`
- Trae GPU 子进程参数含：`--render-node-override=/dev/dri/renderD128`
- 系统 EGL vendor：`/usr/share/glvnd/egl_vendor.d/10_gf.json` → `libEGL_gf.so.0`
- 软件渲染启动（`--ozone-platform=x11 --use-gl=angle --use-angle=swiftshader`）后，日志中**无** EGL 初始化失败 / GPU 崩溃 / context lost，验证规避有效。

## 3.7 原始日志归档
- `bug-report-artifacts/trae-bug-report-20260804.tar.gz`：初版报告的完整 Trae 日志打包（建议上报时附带）。
