# Bug 报告：Qoder 在原生 Wayland 会话下点击行为异常（X11/XWayland 正常）

## 一句话概述

Qoder（Electron 应用）在 **GNOME 原生 Wayland 会话**下出现"点击不正常"（错位 / 无响应 / 需多次点击 / 焦点异常）；切换到 **X11 / XWayland** 后一切正常。经排查，这是 **Qoder 应用层对原生 Wayland 的兼容性问题**，**与砺算（Lisuan）7G100 GPU 及其内核驱动无关**。

---

## 环境信息

| 项目 | 值 |
| --- | --- |
| 操作系统 | Ubuntu 24.04.4 LTS (Noble Numbat) |
| 内核 | `7.0.0-28-generic` |
| 桌面 / 合成器 | GNOME (Unity 会话名) / Mutter，`XDG_SESSION_TYPE=wayland` |
| GPU | 砺算 Lisuan **7G100**，PCI `03:00.0` `[4c54:5006]` |
| GPU 内核驱动 | `gf.ko`（DRM 注册名 `lisuan`），**已正常加载**（`Kernel driver in use: lisuan`） |
| 用户态 GL | `libEGL_gf.so` / `libgbm_gf.so` / `gf_dri.so`（砺算专有栈） |
| Qoder | Electron **42.2.0** / Chromium **148.0.7778.97** |
| 复现方式 | 直接从 GNOME 应用列表启动 Qoder（`.desktop` 未指定后端，默认按 Electron 策略选择） |

---

## 问题现象

- **原生 Wayland 会话**：Qoder 窗口内点击表现异常——点击位置与实际命中区域错位、部分点击无响应、悬浮/拖拽/click-through 类交互失效。
- **X11 / XWayland 会话**：完全正常。

---

## 复现步骤

1. 以 GNOME **Wayland** 会话登录（`XDG_SESSION_TYPE=wayland`）。
2. 从应用列表启动 Qoder（等价于 `/usr/share/qoder/qoder %F`）。
3. 在窗口内进行点击 / 悬浮 / 拖拽等交互 → 出现异常。
4. 对比：注销后以 **Ubuntu on Xorg** 会话登录，或强制 Qoder 走 X11（见下方规避方案），交互恢复正常。

---

## 根因分析

### 决定性证据（Qoder 自身日志）

无论从桌面启动还是命令行 `qoder --version`，主进程都会打印：

```text
[main] [AuraBuddyWindow] native Wayland does not support global pointer polling;
precise click-through Aura Buddy input requires X11/XWayland
```

日志来源确认为 Qoder 主进程（`file:///usr/share/qoder/resources/app/out/main.js`），
即 **Qoder 应用代码自己声明**：其 "Aura Buddy" 悬浮/click-through 特性依赖
**全局指针轮询（global pointer polling）**，而该能力在**原生 Wayland 下不被支持**，
必须依赖 X11 / XWayland。

### 为什么 Wayland 下会出问题（技术背景）

- Wayland 安全模型下，客户端**无法全局轮询指针坐标**，也不能任意读取/注入全局输入或做
  跨窗口 click-through；指针事件仅在合成器认为该 surface 应接收时才下发。
- Qoder 的 Aura Buddy 交互依赖 X11 时代的"全局指针位置查询 + click-through"范式；
  在原生 Wayland 下这些调用无效，导致命中测试/坐标错位、点击落空。
- 该特性在 XWayland 下能工作，是因为 XWayland 提供了传统 X11 输入语义。

### 与 GPU / 驱动无关的证据

以下均已在本机确认，排除砺算 7G100 GPU 及驱动因素：

- `lspci -nnk -s 03:00.0` → `Kernel driver in use: lisuan`，驱动**已正确绑定**。
- `lsmod | grep gf` → `gf` 模块已加载（引用计数正常）。
- `/dev/dri/` 下 `card1`、`renderD128` 均存在，DRM 节点正常。
- `qoder --version` 启动过程**无任何 EGL/GBM/GL/DRM 渲染错误**；异常仅围绕
  "指针/点击"这一 **输入** 维度，与渲染无关。
- 问题现象随**会话类型**（Wayland vs X11）切换而出现/消失，与显卡型号、驱动版本无关联。
- 输入本身在系统层由 libinput/evdev 提供，不经过 GPU 驱动路径。

> 结论：这是 **Qoder（Electron 应用）在原生 Wayland 下的输入/交互兼容性缺陷**，
> **不是** 砺算 7G100 GPU 或其驱动（`gf.ko` / lisuan）的问题。

---

## 规避方案（Workaround）

在**修复前**，可让 Qoder 走 X11/XWayland 以恢复正常点击（任选其一）：

**方案 A：临时命令行启动（强制 X11 后端）**

```bash
/usr/share/qoder/qoder --ozone-platform=x11
# 或
ELECTRON_OZONE_PLATFORM_HINT=x11 /usr/share/qoder/qoder
```

**方案 B：永久修改桌面快捷方式**

编辑 `~/.local/share/applications/qoder.desktop`（从 `/usr/share/applications/qoder.desktop`
复制一份，避免被包更新覆盖），把 `Exec` 改为：

```ini
Exec=/usr/share/qoder/qoder --ozone-platform=x11 %F
```

**方案 C：整机改用 X11 会话**

登录界面选择 "Ubuntu on Xorg" 会话。

> 注：`--ozone-platform-hint=auto` 会让 Electron 尽量走原生 Wayland，**会触发本问题**，
> 应避免；此处需显式指定 `x11`。

---

## 建议给 Qoder 官方的修复方向

1. **不要在原生 Wayland 下依赖全局指针轮询 / click-through**；改用 Wayland 合适协议
   （如 `wlr-layer-shell`、`xdg-shell` 的输入区域 `wl_surface.set_input_region`、
   `pointer-constraints` / `relative-pointer` 等）实现 Aura Buddy，或在原生 Wayland 下
   **自动降级/禁用**该特性而非让普通点击一并异常。
2. 当检测到原生 Wayland 且该特性不可用时，**默认回退到 XWayland**（或提示用户），
   而不是保持在一个点击会错位的状态。
3. 修复 Aura Buddy 窗口的**命中测试/坐标换算**，确保在 Wayland 合成器坐标系下正确。

---

## 附：现场采集命令与关键输出

```text
$ echo $XDG_SESSION_TYPE
wayland

$ lspci -nnk -s 03:00.0
03:00.0 VGA compatible controller [0300]: Device [4c54:5006]
        Subsystem: Device [4c54:5000]
        Kernel driver in use: lisuan

$ lsmod | grep gf
gf                   1081344  62

$ ls /dev/dri/
by-path  card1  renderD128

$ /usr/share/qoder/qoder --version   # 摘录
... Chrome/148.0.7778.97 Electron/42.2.0
[AuraBuddyWindow] native Wayland does not support global pointer polling;
precise click-through Aura Buddy input requires X11/XWayland
（全程无 EGL/GBM/GL/DRM 错误）
```
