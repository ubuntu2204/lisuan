# .codebuddy/skills/lisuan-7g100-compat 变更记录

> 从仓库根 `CHANGELOG.md` 拆分而来（2026-11-19）。按时间倒序；历史条目保留当时的命令与路径。

## 2026.11.14 — 打通 Wayland 合成画面的非交互抓图通道

### 问题与误判纠正

- X11 抓图全黑曾被误判为「QtWebEngine 合成层没画进 X11 窗口」。
  按 `xwininfo` 真实坐标重抓后确认：`xwd`（窗口/root，报 `BadMatch`）、
  `ffmpeg -f x11grab`（窗口与 1920×1080 全屏）、PIL `ImageGrab` 全部拿不到
  合成后内容，连全屏都是纯黑——这是 XWayland 下 `XGetImage` 的全局限制，
  **不是应用渲染故障**。
- 已证伪通道：GNOME Shell `org.gnome.Shell.Screenshot` D-Bus
  （`AccessDenied: Screenshot is not allowed`）、`xdotool key shift+Print`
  （XTEST 到不了 Wayland 合成器，不落盘）、逐条 `gdbus call` 操作 ScreenCast
  （连接断开后 session 被 Mutter 回收）。

### 通道与工具

- 新增 `.codebuddy/skills/lisuan-7g100-compat/scripts/capture-wayland-screen.py`：
  在**同一条 D-Bus 连接**里完成 `org.gnome.Mutter.ScreenCast` 的
  `CreateSession → RecordMonitor('DP-1')` → 订阅 `PipeWireStreamAdded` →
  `Start` → `gst-launch-1.0 pipewiresrc path=<node_id> num-buffers=1 !
  videoconvert ! pngenc ! filesink` → `finally Stop`；`--check` 只读查依赖，
  `--connector` / `--timeout` 可覆盖，不留后台录屏、不自动重试。
- 实测首帧 1920×1080 / 34486 色，能看到桌面、任务栏与各窗口。
  抓图通道本身有效。
- 新增规则 `.codebuddy/rules/Wayland抓图约定.md`（禁用证伪通道、
  规定唯一通道与验收标准）；技能 `SKILL.md` 验证命令、
  `references/troubleshooting.md` §10、`references/conventions.md` 同步；
  `tests/test-repo-health.sh` 增加脚本与规则文件断言。
