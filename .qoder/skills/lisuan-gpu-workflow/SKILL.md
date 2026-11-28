---
name: lisuan-gpu-workflow
description: 在 /home/ubuntu/project/lisuan 项目的每次任务开始时调用，用于砺算 7G100 图形栈适配、Steam/Proton/Wine 运行、Vulkan/DXVK 选型、llvmpipe 误用诊断、GPU 合成异常、日志与截图验证及安全回滚。凡是涉及显卡、游戏、Windows 应用、Electron、Steam、渲染、帧率、黑屏、花屏、驱动或兼容层的任务，均按本 Skill 工作。
---

# 砺算 7G100 项目工作流

## 每次任务开始

1. 先判断任务是否涉及图形、运行环境或项目脚本；涉及时读取本 Skill，不要仅凭历史记忆下结论。
2. 确认当前对象：原生 Linux、Electron/Chromium、Steam CEF、Proton/Wine、32 位或 64 位、宿主直跑或 pressure-vessel 容器。
3. 图形故障先取证，再修改。至少收集：进程命令行、关键环境变量、加载库、渲染日志、CPU/内存、窗口截图。
4. 只有文档、菜单或非图形任务可跳过运行时探测，但仍须遵守“环境变量隔离、保留证据、不自动恢复”的项目规则。

## 不可违反的规则

- **不得**把“加载了 `libvulkan_gf.so`”或“打开 `/dev/dri/renderD128`”当作硬件加速证据。
- **不得**把 gf 的 `VK_ICD_FILENAMES`、`__GLX_VENDOR_LIBRARY_NAME=gf`、`LD_PRELOAD` 注入全局 shell、Steam 客户端或其他无关应用。
- **不得**用看门狗、自动重启、重试循环掩盖崩溃；遵守 `.codebuddy/rules/崩溃处理约定.md`。
- **不得**宽泛 `pkill wine`。按 `/proc/<pid>/environ` 中的 `WINEPREFIX` 或 `STEAM_COMPAT_DATA_PATH` 精确识别进程。
- 修改 Steam VDF、Proton 前缀、共享 Proton 目录或游戏目录前先备份；优先做单游戏、单前缀修改。
- 硬件加速失败时允许显式回退软件渲染，但必须记录“原故障未修复”，不能把绕开称为修复。

## 快速决策表

| 现象 | 首选判断 | 处理方向 |
| --- | --- | --- |
| Steam 客户端 CEF 闪退、`gf_gbm.so` 段错误、系统信息弹窗崩溃 | 客户端 GL/GBM 路径 | 客户端专用四项：`GBM_BACKEND=dri`、`LIBGL_ALWAYS_SOFTWARE=1`、`__GLX_VENDOR_LIBRARY_NAME=mesa`、`-cef-disable-gpu` |
| 容器内 Vulkan 枚举 0 设备或返回 `-3` | gf ICD 未适配容器 | 同时处理 `VK_DRIVER_FILES`、os-release 白名单、ICD 协商垫片、`libSC.so` |
| DXVK 日志 `Skipping ... maintenance5` | DXVK 版本过新 | D3D11/DXGI 使用 DXVK 2.3；D3D8 改用 wined3d |
| FPS 低、CPU 多核满载、存在大量 `llvmpipe` 线程 | 误用软件渲染 | 先确认实际 adapter，再修 ICD/容器，不要先降画质 |
| 32 位游戏拿不到 gf | 厂商只有 x86_64 驱动 | new WoW64；不能桥接时使用同架构 llvmpipe，并明确性能代价 |
| Chromium/Electron 缺块、破线、弹窗花屏 | gf GPU 合成缺陷 | `--disable-gpu-compositing --enable-gpu-rasterization`；必要时强制 XWayland |
| 窗口存在但黑屏/无响应 | 不能只看窗口和 DRM fd | 查渲染日志、映射状态、线程等待、截图；new WoW64 检查低 4GB 映射 |
| Wine 进程杀不掉 | 命令行不含前缀 | 扫描 `/proc/*/environ` 精确匹配前缀 |
| 桌面图标无图标或双击无效 | GNOME/XDG 入口问题 | `Icon` 使用 PNG；查询 `xdg-user-dir DESKTOP`；设置 `metadata::trusted` |

## 标准排障流程

### 1. 识别进程与实际渲染器

```bash
pgrep -af 'WorldOfWarships|wine|steam|electron'
pid=<game_pid>
tr '\0' '\n' < /proc/$pid/environ | sort
grep -Ei 'vulkan_gf|vulkan_lvp|libLLVM|libSC|d3d11|dxgi' /proc/$pid/maps
top -b -H -n 1 -p "$pid" | head -30
```

使用项目脚本可快速得到结构化结果：

```bash
python3 .qoder/skills/lisuan-gpu-workflow/scripts/diagnose_process.py <pid>
python3 .qoder/skills/lisuan-gpu-workflow/scripts/vk_probe.py
```

判定优先级：

1. DXVK/Wine 日志中的实际 device/adapter 名称。
2. 是否存在活跃 `llvmpipe` 工作线程及 `libLLVM`。
3. `/proc/<pid>/environ` 中最终生效的 Vulkan 变量。
4. 进程映射的 shim、`libvulkan_gf.so`、`libSC.so`。
5. CPU、内存、FPS 与截图。

### 2. 容器内单独探测 ICD

宿主正常不代表 Steam Runtime 容器正常。必须用同一 runtime 验证：

```bash
RT="$HOME/.local/share/Steam/steamapps/common/SteamLinuxRuntime_4"
PROBE="$PWD/.qoder/skills/lisuan-gpu-workflow/scripts/vk_probe.py"
"$RT/_v2-entry-point" --verb=waitforexitandrun -- \
  /run/host/usr/bin/python3 "$PROBE"
```

可分别设置 `VK_DRIVER_FILES` 和 `VK_ICD_FILENAMES` 为 gf、lvp 做对照。注意：

- 新 loader 中 `VK_DRIVER_FILES` 优先级高于 `VK_ICD_FILENAMES`，两者都要覆盖。
- 容器 `/etc/os-release` 常为 Steam Runtime，gf 白名单会拒绝初始化。
- 不要用 `--verb=run` 做需要读取输出的诊断；本机实测它不转发脚本 stdin/输出，使用 `waitforexitandrun`。

### 3. Steam/Proton 硬件加速四件套

适用于 pressure-vessel 内 gf Vulkan 不可用：

1. `osrelease_redirect.so`：把 `/etc/os-release` 重定向到 Ubuntu 文件。
2. `libvulkan_gf_shim.so` + `gf_icd.json`：协商 loader interface version，补报/过滤被裁剪扩展。
3. `libvulkan_gf.so` 与 `libSC.so`：从宿主复制到容器可见的用户目录。
4. 同时设置 `VK_ICD_FILENAMES` 与 `VK_DRIVER_FILES`；按需禁用 Steam Vulkan overlay layer。

编译必须在宿主侧完成。生成 shim 时重写硬编码路径；性能敏感场景关闭逐调用 `slog` 文件日志。

不要假设 `LD_LIBRARY_PATH` 一定进入最终游戏进程：Proton 会重写它。启动后检查 `/proc/<pid>/environ` 与 `/proc/<pid>/maps`；若 `libSC.so` 未加载，应在驱动初始化前显式预加载。

### 4. DXVK 版本与安装位置

- gf 当前用户态报告 `gf 22.0.4`，缺少 `VK_KHR_maintenance5`。
- DXVK 2.5/2.7/3.x 可能直接跳过 gf；D3D11/DXGI 使用仓库验证过的 DXVK 2.3。
- Proton 启动时会用自带 DXVK 覆盖前缀 `drive_c/windows/system32` 的副本；只改前缀可能在下次启动被还原。
- 对 64 位游戏，优先把 DXVK 2.3 的 `d3d11.dll`、`dxgi.dll` 放在游戏主 exe 同目录，并确认日志中的 `Game:` 是游戏本体而非 `xalia.exe`。
- DXVK 2.3 没有 D3D8；老 D3D8 游戏走 wined3d，不要强行替换不存在的 DLL。

### 5. 32 位与 new WoW64

- 32 位 PE + 仅 x86_64 gf：传统 wow64 通常只能 llvmpipe。
- `PROTON_USE_WOW64=1` 可让 32 位调用经 thunk 到 64 位 ICD，但可能需要新建 wow64 布局前缀。
- 若 `vkMapMemory` 返回 ≥4GB 地址，使用项目低 4GB 重映射池；不要给所有 GL 路径无条件加载该 shim。
- ELF class 必须匹配：64 位 `LD_PRELOAD` 无法作用于 32 位 Wine 进程。

### 6. Chromium/Electron

- 显示异常优先强制 XWayland：`--ozone-platform=x11 --ozone-platform-hint=x11`。
- gf 合成破线使用软件合成 + GPU 光栅：`--disable-gpu-compositing --enable-gpu-rasterization`。
- 沙箱启动失败按项目 wrapper 处理 `--no-sandbox`。
- 加速失败时显式使用 `--no-hw`，不做自动切换。

## 验证门槛

只有同时满足以下条件才可声明“硬件加速正常”：

- 容器内 probe 能枚举 `LISUAN 7G100 Series Graphics`。
- DXVK/Wine 日志确认游戏本体使用 gf，且没有 `Skipping ... maintenance5` 或 `No adapters found`。
- 游戏进程加载 gf shim/driver；需要时加载 `libSC.so`；没有活跃 llvmpipe 渲染线程。
- 与软件渲染对照，CPU 占用和帧率有可测量改善。
- 截图显示真实游戏内容；窗口 `map_state == IsViewable`。

截图工具：

```bash
python3 .qoder/skills/lisuan-gpu-workflow/scripts/capture_x11.py --title '《战舰世界》' --output /tmp/wows.png
```

截图脚本使用 `xwd`，支持 `_NET_WM_NAME`。若目标窗口返回 `BadMatch`/`BadColor`，说明该 X11 窗口或根窗口当前不可抓取；改用 GNOME ScreenCast/Portal 或等待可抓取的游戏窗口，不能把失败截图当作验证。

首次启动着色器预编译可能耗时数分钟，不能仅因 CPU 高就判崩溃；保留日志并等待明确错误或超时。

## 安全变更与回滚

- Steam `localconfig.vdf` 尽量在客户端完全停止后修改；运行中直接编辑可能被内存配置覆盖。
- Steam 正常退出卡住时先发 SIGTERM；仍不退出才在用户同意后对明确 PID 使用 SIGKILL。禁止宽泛杀进程。
- 修改前缀 DLL 前备份到同目录隐藏备份；不要删除原文件。
- 修改共享 Proton 前评估对所有游戏的影响；优先单游戏目录或独立 compat prefix。
- 回滚必须恢复 VDF、DLL、启动选项和环境变量，并复测原故障是否仍可复现。

## 输出要求

向用户汇报时区分：

- **已验证事实**：日志、probe、线程、截图、资源数据。
- **根因判断**：由证据链支持，不把相关当因果。
- **临时绕开**：明确哪些问题仍未修复。
- **变更与回滚**：列出修改位置、备份位置、恢复方法。

## 参考

- [Steam/Proton 详细流程](references/steam-proton.md)
- [项目案例索引](references/case-index.md)
