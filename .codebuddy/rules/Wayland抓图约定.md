# Wayland 合成画面取证约定

> 适用：本机 Ubuntu 24.04 + GNOME Wayland（XWayland）+ 砺算 7G100。
> 2026-11-14 实测固化。权威工具与排障细节见
> `.codebuddy/skills/lisuan-7g100-compat/scripts/capture-wayland-screen.py`
> 与 `references/troubleshooting.md` §10。

## 一、为什么要有这条约定

排查 KK 对战平台「错位 / 黑块 / 鼠标点不了」时，曾把 X11 抓图全黑误判为
「QtWebEngine 合成层没画进 X11 窗口」，据此推出错误的修复方向。按 `xwininfo`
真实坐标重抓后确认：**连 1920×1080 全屏都是纯黑**，这是 XWayland 下
`XGetImage` 拿不到任何合成后内容的全局限制，不是应用渲染故障。

## 二、已证伪的通道（不得用于判定 Wayland 应用渲染）

| 通道 | 结果 |
| --- | --- |
| `xwd -id <wid>` / `xwd -root` | `BadMatch`；偶尔抓到也是纯黑 |
| `ffmpeg -f x11grab`（窗口或全屏） | 全黑（本机全屏 207 万像素纯黑） |
| PIL `ImageGrab.grab()` | `OSError: X get_image failed` |
| GNOME Shell `org.gnome.Shell.Screenshot` D-Bus | `AccessDenied: Screenshot is not allowed` |
| `xdotool key shift+Print`（XTEST 模拟全局快捷键） | 事件到不了 Wayland 合成器，不落盘；不能依赖交互确认 |

**禁止**用上述任一结果判定应用黑屏、错位、未刷新或渲染故障；也禁止把
「X11 全黑」写进结论当作证据。

## 三、唯一可用通道（必须遵守）

`org.gnome.Mutter.ScreenCast`（D-Bus）+ PipeWire + GStreamer `pipewiresrc`：

```bash
python3 .codebuddy/skills/lisuan-7g100-compat/scripts/capture-wayland-screen.py -o /tmp/shot.png
python3 .codebuddy/skills/lisuan-7g100-compat/scripts/capture-wayland-screen.py --check
```

- **必须在同一条 D-Bus 连接内**完成 `CreateSession → RecordMonitor →` 订阅
  `PipeWireStreamAdded` → `Start` → `pipewiresrc` 抓一帧 → `Stop`。
  用 `gdbus call` 逐条调用会因连接断开被 Mutter 回收 session，随后报
  「对象在路径 … 处不存在」。
- 会话**必须显式 Stop**，不留后台录屏；脚本用 `finally` 保证。
- 抓图工具不得常驻、守护或自动重试（遵循 `.codebuddy/rules/崩溃处理约定.md`）。
- 依赖：`python3-dbus` / `python3-gi`、`gstreamer1.0-tools`、
  `pngenc`、`pipewiresrc`、PipeWire；缺失时用 `--check` 明确报错，不静默降级。

## 四、验收标准

- PNG 必须是 1920×1080、颜色数远超纯黑（本机首帧实测 34486 色），
  并能看到桌面、任务栏与目标窗口等合成后内容。
- 只报「窗口出现 / 进程存活 / CDP 页面正常」不算视觉验收；
  涉及重影、黑块、坐标错位时必须用本通道的图或用户提供的合成后截图确认。
- 应用内 Web 层可用 `QTWEBENGINE_REMOTE_DEBUGGING` + CDP 单独取证，
  但它只覆盖 WebView，不替代整屏合成画面。

## 五、沉淀位置

- 技能：`SKILL.md` 验证命令、`references/troubleshooting.md` §10、
  `references/conventions.md` 图形取证条目、脚本本体。
- 规则：本文件；测试：`tests/test-repo-health.sh` 的通道断言；
  变更记录：`CHANGELOG.md` 2026.11.14 条目。
