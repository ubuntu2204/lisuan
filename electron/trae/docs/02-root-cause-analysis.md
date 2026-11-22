# 2. 根因分析

Trae 在本机的问题可拆分为**两条独立链路**：一条是 Trae 自身逻辑缺陷（卡顿/高 CPU），一条是显卡渲染栈兼容问题（显示错误）。

## 2.1 卡顿 / 高 CPU：shared process 异常后无限重试

### 触发链路（高概率）
1. `shared process` 在扩展扫描 / 扩展注册表相关输入路径上收到 `undefined`，触发
   `TypeError [ERR_INVALID_ARG_TYPE]: The first argument must be of type string ... Received undefined`
   （典型为 `Buffer.from(undefined)` 类异常）。
2. 异常后未完全短路，导致扩展扫描与 gallery manifest 查询**反复重试**：
   - `Created extension scanner input ...`
   - `get extension gallery manifest ...`
3. 同时 ShellExec / Git model / fileWatcher / 工具卡片解析失败，使 extension host / shared process 反复忙碌。
4. 最终表现为 UI 卡顿与多子进程持续占用 CPU。

### 佐证事实
- 用户扩展注册表 `/home/ubuntu/.trae/extensions/extensions.json` 为空数组 `[]`。
- 用户设置 `/home/ubuntu/.config/Trae/User/settings.json` 为空对象 `{}`。
- 整机内存/磁盘/CPU 均不紧张，排除系统资源瓶颈。

### 性质
属 **Trae 自身缺陷**（shared process 对空/异常输入缺乏防御 + 重试策略无退避），需官方修复。本仓库仅做规避性建议，见 `05-feedback-and-upstream.md`。

## 2.2 显示错误：gf EGL 在 Wayland 下不兼容

### 环境事实
- 本机为 **Wayland** session；client GLX vendor = `LISUAN`（7G100 显卡）。
- Trae 自带 `libEGL.so` / `libGLESv2.so`（Chromium 内置），并注册系统 EGL vendor：
  `/usr/share/glvnd/egl_vendor.d/10_gf.json` → `libEGL_gf.so.0`（gf 闭源 RADV fork EGL）。
- Trae GPU 子进程带 `--render-node-override=/dev/dri/renderD128`（7G100 渲染节点）。
- 硬件链路本身正常：`gf` 内核模块已加载，`/dev/dri/renderD128` 存在且属主 `render` 组。

### 失败机理
Chromium 自带 EGL 在 Wayland/ANGLE 合成路径上尝试走 `libEGL_gf.so` 时，gf 的 EGL 实现（闭源、针对特定合成器）不完整，导致窗口合成失败 → 黑屏 / 花屏 / 内容不刷新 / 闪烁。

### 修复思路
强制 **X11 ozone 后端 + ANGLE/SwiftShader 纯软件渲染**，完全绕开 gf EGL。对代码编辑器类应用性能足够且最稳定。详见 `04-display-error-fix.md`。

## 2.3 两条链路的关系
两条链路**相互独立**：卡顿即使修复显示错误仍可能存在（因 shared process 缺陷与渲染无关）；显示错误即使卡顿消失也会出现（因 gf EGL 兼容问题独立于业务逻辑）。因此修复需分别处理。
