# Trae on Linux (7G100 / Wayland) — 问题归纳与修复

本仓库汇总在 **Ubuntu 24.04 + 砺算 7G100 显卡 + Wayland** 环境下使用 Trae IDE（`3.5.81` → `3.5.91`）时遇到的两类问题：
**卡顿/高 CPU**（Trae 自身逻辑缺陷）与 **显示错误**（gf EGL 渲染栈兼容问题），并给出显示错误的可行修复。

> 路径说明：本目录原为 `~/project/trae`，2026-11-22 整体迁入 `lisuan/electron/trae/`
> （Trae 是 Electron 应用，与 `../use-trae.sh` 同一主题；下文内部相对链接均保持有效）。
>
> 原始报告与调试记录保留在本目录：`trae-bug-report-20260804.md`、`debug-trae-system-lag.md`，日志归档在 `bug-report-artifacts/`。

---

## 一、问题总览

| 类别 | 表现 | 性质 | 状态 |
|------|------|------|------|
| 卡顿 / 高 CPU | 进入工作区后 UI 卡顿，多 Trae 子进程持续占 CPU（zygote ~29%、extHost ~6%） | Trae 自身 shared process 缺陷 | 待官方修复（已上报） |
| 显示错误 | 黑屏 / 花屏 / 内容不刷新 / 闪烁 | 7G100 gf EGL 在 Wayland 下不兼容 | **已修复（软件渲染规避）** |

两条链路相互**独立**：修复显示错误不解决卡顿，反之亦然。

---

## 二、环境

- 产品：Trae（AI 编程 IDE，字节跳动），VS Code base `1.107.1`
- 版本：`3.5.81`（报告初版）→ `3.5.91`（当前安装，build `2.3.73738`）
- OS：Ubuntu 24.04.4 LTS；内核 `7.0.0-28` → `7.0.0-30-generic`
- 会话：**Wayland**（`DISPLAY=:0`，`GDK_BACKEND=x11`）
- 显卡：砺算 7G100（client GLX vendor = `LISUAN`；渲染节点 `/dev/dri/renderD128`）

---

## 三、根因（摘要）

### 3.1 卡顿 / 高 CPU
`shared process` 在扩展扫描/注册表路径上收到 `undefined`，抛
`TypeError [ERR_INVALID_ARG_TYPE] ... Received undefined`，异常后未短路，导致
`Created extension scanner input` / `get extension gallery manifest` 高频重复，叠加 ShellExec / Git model / fileWatcher 失败，使扩展宿主与共享进程反复忙碌 → UI 卡顿。
整机内存/磁盘/CPU 均不紧张，排除系统资源瓶颈。**属 Trae 自身缺陷。**

### 3.2 显示错误
Chromium 自带 EGL 在 Wayland/ANGLE 路径尝试走系统 `libEGL_gf.so`（gf 闭源 RADV fork EGL），其合成实现不完整 → 窗口合成失败。硬件链路（`gf` 模块、`/dev/dri/renderD128`）本身正常，问题纯在 EGL 软件栈兼容性。

---

## 四、显示错误修复（已验证）

当前入口是仓库内 [`../use-trae.sh`](../use-trae.sh)（交互菜单 [10] 可把桌面项切到它）：
默认 X11 + ANGLE/SwiftShader 软件渲染，绕开 gf EGL；`--hw` 用于硬件加速对比（可能复现花屏）。
它复用了通用基座 `../use-electron-gf.sh` 的 `SW_TRAE_FLAGS`，参数源自早期 `~/.local/bin/trae-fix` 的实测结论。

国际版 `trae.desktop` 与国内版 `trae-cn.desktop` **都走同一个修复**：不带 `--app` 的
`--install-desktop` / `--uninstall-desktop` 会同时处理两个变体；只启动国内版用
`bash ../use-trae.sh --app trae-cn`。2026-11-24 实测：国内版系统原入口裸启 25 秒无窗口，
经本 wrapper 软件渲染后正常出现「欢迎使用 TRAE」窗口。

早期原型三件套（`~/.local/bin/trae-fix` + 用户级桌面项 + `~/.bashrc` 别名）保留在
[docs/04-display-error-fix.md](docs/04-display-error-fix.md) 作证据，现以仓库 wrapper 为准。
验证结果：软件渲染启动无 EGL 失败 / GPU 崩溃；`desktop-file-validate` 通过。

> 详见 [docs/04-display-error-fix.md](docs/04-display-error-fix.md)。

---

## 五、卡顿链路处理建议（待官方）

1. `sharedProcessMain.js` 中 `Buffer.from` 上游调用链排查 `undefined` 来源。
2. 用户扩展注册表为空数组 `[]` 时的兼容处理。
3. 异常后重试策略加退避，避免无限重扫扩展。
4. 排查 `ShellExec` / `Git model` / `ShellToolCard` 缺失命令是否由 shared process 异常诱发。

---

## 六、文档索引

| 文档 | 内容 |
|------|------|
| [docs/01-symptom-and-environment.md](docs/01-symptom-and-environment.md) | 症状分类、受影响版本、环境明细 |
| [docs/02-root-cause-analysis.md](docs/02-root-cause-analysis.md) | 两条链路根因分析 |
| [docs/03-key-evidence.md](docs/03-key-evidence.md) | 关键日志证据（行号、ps、EGL 事实） |
| [docs/04-display-error-fix.md](docs/04-display-error-fix.md) | 显示错误修复组件与使用 |
| [docs/05-feedback-and-upstream.md](docs/05-feedback-and-upstream.md) | 上报邮件、建议官方修复点、上游状态 |
| [docs/06-debug-session.md](docs/06-debug-session.md) | 调试会话假设与验证结论 |

---

## 七、原始材料

- `trae-bug-report-20260804.md` — 初版缺陷报告
- `debug-trae-system-lag.md` — 调试会话记录
- `bug-report-artifacts/trae-bug-report-20260804.tar.gz` — 完整 Trae 日志归档
- `bug-report-artifacts/trae-feedback-email.txt` — 上报邮件正文
