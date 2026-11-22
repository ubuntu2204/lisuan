# 4. 显示错误修复方案

针对 7G100 + Wayland 环境下 Trae 的黑屏 / 花屏 / 不刷新 / 闪烁，采用**强制软件渲染**绕开 gf EGL。

**现行入口（2026-09 起）**：`electron/use-trae.sh`（交互菜单 [10] 装桌面项）。

```bash
bash electron/use-trae.sh                 # 默认软件渲染稳定模式
bash electron/use-trae.sh --install-desktop # 桌面图标直启即带修复
bash electron/use-trae.sh --hw            # 硬件加速对比，可能复现花屏
```

该 wrapper 转调 `use-electron-gf.sh`，其 `SW_TRAE_FLAGS` 已吸收下面 4.1 原型中经实测的
X11 + ANGLE/SwiftShader 参数；4.1–4.4 保留为当时的部署记录与证据。

## 4.1 历史原型组件（2026-08-26，已被仓库 wrapper 取代）

### (1) 包装脚本 `~/.local/bin/trae-fix`
```bash
#!/usr/bin/env bash
set -e
TRAE_BIN="/usr/share/trae/trae"
if [ "${TRAE_HW_GPU:-0}" = "1" ]; then
    # 硬件加速对比模式（可能复现显示错误）
    exec "${TRAE_BIN}" "$@"
else
    export GDK_BACKEND=x11
    export EGL_PLATFORM=surfaceless
    exec "${TRAE_BIN}" "$@" \
        --ozone-platform=x11 \
        --use-gl=angle \
        --use-angle=swiftshader \
        --enable-unsafe-swiftshader \
        --disable-gpu-sandbox
fi
```

### (2) 用户级桌面项 `~/.local/share/applications/trae.desktop`
- `Name=Trae (fixed)`，`Exec=/home/ubuntu/.local/bin/trae-fix %F`（覆盖系统级 `/usr/share/applications/trae.desktop`）。
- `desktop-file-validate` 通过。

### (3) CLI 别名 `~/.bashrc`
```bash
trae() {
    if [ "${TRAE_HW_GPU:-0}" = "1" ]; then
        command trae "$@"
    else
        GDK_BACKEND=x11 EGL_PLATFORM=surfaceless command trae "$@" \
            --ozone-platform=x11 --use-gl=angle --use-angle=swiftshader \
            --enable-unsafe-swiftshader --disable-gpu-sandbox
    fi
}
```

## 4.2 历史原型使用方式
- **桌面**：应用菜单选 **"Trae (fixed)"**（勿用系统原 "Trae"）。
- **命令行**：直接 `trae`（已自动套用修复参数）。
- **硬件加速对比**：`TRAE_HW_GPU=1 trae-fix` 或 `TRAE_HW_GPU=1 trae`。

现使用 `bash electron/use-trae.sh` 或菜单 [10] 安装的桌面项（见文首）。

## 4.3 验证结果
- 软件渲染启动后进程正常，参数生效。
- 日志中无 EGL 初始化失败 / GPU 崩溃 / 上下文丢失。
- `desktop-file-validate` 通过（仅一个非致命 Categories 提示）。

## 4.4 说明
- 软件渲染对 IDE 性能足够且最稳定，不依赖 7G100 图形栈。
- 此为渲染层规避，不解决 `02-root-cause-analysis.md` 中的 shared process 卡顿链路。
- 原型组件为用户级，不受系统升级影响；现行 wrapper 的桌面项若被 Trae 升级覆盖，重跑菜单 [10] / `use-trae.sh --install-desktop` 即可。
