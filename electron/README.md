# Electron 类应用适配（砺算 7G100 + Ubuntu 24.04）

> 解决 Electron / Chromium 系桌面应用在国产 GPU 上的两类问题：
> **① 显示异常**（界面空白 / 花屏 / 黑块 / 经 XWayland 渲染残缺）
> **② 硬件加速**（用 gf 驱动替代默认软件渲染）
>
> 已内置适配：ZCode、CodeBuddy 国内版、Qoder 国内版。其它 Electron 应用可用 `--cmd` / `--appimage` 复用。

---

## 文件

| 文件 | 说明 |
|---|---|
| `use-electron-gf.sh` | **通用基座**：显示异常修复 + gf 硬件加速。支持 `--app` / `--cmd` / `--appimage` |
| `use-zcode.sh` | ZCode（Electron AppImage）入口 |
| `use-codebuddy.sh` | CodeBuddy 国内版入口 |
| `use-qoder.sh` | Qoder 国内版入口 |

三个应用 wrapper 均通过 `exec "$DIR/use-electron-gf.sh" --app <name>` 转调基座，同目录引用。

## 快速开始

```bash
bash use-electron-gf.sh --app qoder                 # 默认 gf 硬件加速
bash use-electron-gf.sh --app qoder --no-hw         # 白屏/花屏时回退软件渲染
bash use-electron-gf.sh --app codebuddy
bash use-electron-gf.sh --app qoder --install-desktop  # 把应用菜单 Exec 切到本 wrapper
bash use-electron-gf.sh --cmd /path/to/electron-app    # 任意 Electron 应用
bash use-electron-gf.sh --appimage /path/to.appimage

# 调试
GF_EXTRA_FLAGS="--enable-logging=stderr --v=1" bash use-electron-gf.sh --app qoder
```

---

## 注意事项

### 1. 砺算 gf 裁掉了两个实例扩展 → 硬件加速报 -7（**最常见**）

**症状**：`--hw` 模式下应用报

```
vkCreateInstance() failed: -7        ← VK_ERROR_INCOMPATIBLE_DRIVER
Failed to create and initialize Vulkan implementation.
```

界面白屏或花屏，但加 `--no-hw` 就正常。

**根因**：gf 厂商裁掉了实例扩展

- `VK_KHR_external_semaphore_capabilities`
- `VK_KHR_external_fence_capabilities`

而新版 Vulkan loader 严格校验**"请求的实例扩展必须由某个 ICD 暴露"**。
Electron/Chromium 会请求前者 → loader 直接拒绝创建实例。

用 `VK_LOADER_DEBUG=error,warn` 可看到决定性证据：

```
[Vulkan Loader] ERROR: loader_validate_instance_extensions:
Instance extension VK_KHR_external_semaphore_capabilities not supported by available ICDs
```

**解法：用协商垫片。** `minion-masters/gf-compat/gf_icd.json` → `libvulkan_gf_shim.so`，
它对外报协商版本 5，并在枚举时补报被裁的两个扩展，令校验通过。

脚本已**自动优先选用垫片**：

```bash
if [ -z "${VK_ICD_FILENAMES:-}" ]; then
  if   [ -e "$GF_VK_ICD_SHIM" ]; then export VK_ICD_FILENAMES="$GF_VK_ICD_SHIM"  # 垫片优先
  elif [ -e "$GF_VK_ICD" ];      then export VK_ICD_FILENAMES="$GF_VK_ICD"        # 原版兜底
  fi
fi
```

> **坑**：早期版本无条件 `export VK_ICD_FILENAMES="$GF_VK_ICD"`，会覆盖外部指定的垫片，
> 导致手动设 `VK_ICD_FILENAMES` 无效。现已是"外部优先 → 垫片 → 原版"三级回退。

> 同一根因也影响《随从大师》（DXVK 路径），详见
> `../minion-masters/MinionMasters-攻坚全记录.md` 的「显卡坑 15」。
> 本目录的 shim 与那边是同一个。

### 2. 另一条 loader 警告（目前无害）

```
Driver libvulkan_gf.so supports Vulkan 1.3, but only supports loader interface version 4.
Interface version 5 or newer required (Policy #LDP_DRIVER_7)
```

目前只是 warning，不影响运行。若将来 loader 收紧为错误，需升级厂商驱动。

### 3. 显示异常三件套（与 Steam CEF / Foxit WebEngine 同源）

本质都是 Chromium 渲染进程的问题：

| 开关 | 解决 |
|---|---|
| `--no-sandbox` | Ubuntu 24.04 收紧 unprivileged user namespace，渲染进程起不来 |
| `--ozone-platform=x11` + `--ozone-platform-hint=x11` | Wayland 原生后端残缺（拖拽/缩放窗口残缺），强制走 XWayland |
| `--disable-gpu-compositing` | 国产驱动 GPU 合成白屏 |

> **坑**：Electron 43（Qoder）会**忽略** `--ozone-platform-hint=x11`，子进程仍走
> `--ozone-platform=wayland`。因此必须**同时**给显式的 `--ozone-platform=x11`。
> 两个并存以兼容新旧 Chromium（未知开关会被安全忽略）。

### 4. 不要把 gf 环境变量做成全局

只给**本应用**注入 gf env：

```bash
export __GLX_VENDOR_LIBRARY_NAME=gf
export LIBGL_ALWAYS_SOFTWARE=0
```

**全局注入会让 Steam 客户端的 GLX 崩溃**（CEF 走 gf_gbm 段错误），
详见 `../steam/steam-on-lisuan.md` §「gf env 不能污染 Steam 客户端」。

### 5. 硬件加速失效时的回退

脚本**没有自动回退**——`--hw` 失败不会自动切 `--no-hw`，表现为白屏。
遇到异常时手动加 `--no-hw`：

```bash
bash use-electron-gf.sh --app qoder --no-hw
```

软件渲染稳定优先，性能略低但一定能用。

### 6. 桌面图标路径

`--install-desktop` 会把 `~/.local/share/applications/*.desktop` 的 `Exec` 改写成
**本 wrapper 的绝对路径**。因此：

- **移动/重命名本目录后，图标会失效**（Exec 仍指向旧路径），需重新执行 `--install-desktop`；
- 桌面上的副本（`~/桌面/*.desktop`）也要同步更新；
- GNOME 下还需 `gio set <file> metadata::trusted true`，否则双击不执行；
- 改完跑 `update-desktop-database ~/.local/share/applications`。

### 7. Qoder 特例

`use-electron-gf.sh` 对 `qoder` / `qoder-cn` 会**自动清理**失效的会话状态，
桌面图标启动时走干净会话，不受影响。

---

## 硬件加速原理

1. 宿主 gf GLX 可用（`GLX_VENDOR_LIBRARY_NAME=gf glxinfo` 能枚举 `LISUAN 7G100`）；
2. 注入 `__GLX_VENDOR_LIBRARY_NAME=gf` + `VK_ICD_FILENAMES` 指向垫片；
3. 让 Chromium 走**桌面 GL**（`--use-gl=angle --use-angle=gl`），
   由 gf 驱动承担渲染与栅格化。

即：不是让 Chromium 直接跑 gf Vulkan（那条路会因缺扩展被拒），
而是走 ANGLE-on-GL，由 gf 的 GL 栈承接。

## 故障排查速查

| 现象 | 检查 |
|---|---|
| 启动报 `-7` / 白屏 | ICD 是否为 `gf-compat/gf_icd.json`（垫片）？`minion-masters/gf-compat/` 是否被清理？ |
| 界面残缺/拖拽异常 | 是否同时给了 `--ozone-platform=x11`？（Electron 43 会忽略 hint） |
| 双击图标无反应 | Exec 路径是否有效？是否 `gio set ... trusted`？ |
| Steam 客户端 GLX 崩 | 是否把 gf env 设成了全局？应只作用于单个应用 |
| 一切正常但想要更高性能 | 确认 `ICD=` 提示为垫片，且日志无 `vkCreateInstance` 错误 |

诊断命令：

```bash
VK_LOADER_DEBUG=error,warn bash use-electron-gf.sh --app qoder --hw
```
