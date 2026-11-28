# Steam / Proton / pressure-vessel 详细流程

## 1. 先区分三条路径

| 路径 | 故障表现 | 不能混淆的点 |
| --- | --- | --- |
| Steam 客户端 CEF | 启动闪退、界面卡加载、系统信息弹窗崩溃 | 客户端的软件 GL 修复不代表游戏软件渲染 |
| Proton 游戏容器 | Vulkan 0 设备、DXVK fallback、黑屏、低 FPS | 宿主 probe 正常不代表容器正常 |
| Wine/Windows 应用 | GLX/EGL/Vulkan 初始化失败、窗口异常 | 32/64 位、前缀布局、DLL 覆盖都会改变结果 |

## 2. Steam 客户端专用修复

桌面入口或命令行使用：

```bash
env GBM_BACKEND=dri LIBGL_ALWAYS_SOFTWARE=1 \
  __GLX_VENDOR_LIBRARY_NAME=mesa \
  /usr/bin/steam -cef-disable-gpu
```

作用边界：

- `GBM_BACKEND=dri`：绕开 `gf_gbm.so` 与系统 libgbm 25.x 的 ABI 不兼容。
- `LIBGL_ALWAYS_SOFTWARE=1`：只让客户端 GL 走 Mesa llvmpipe，避免加载容器内不稳定的 gf GL。
- `__GLX_VENDOR_LIBRARY_NAME=mesa`：避免 GLVND 选错 vendor 导致 GLX 上下文创建失败。
- `-cef-disable-gpu`：只作用于 Steam CEF；不修改游戏容器 Vulkan 配置。

禁止：

- 向 `steamwebhelper_sniper_wrap.sh` 注入 gf `LD_PRELOAD`。
- 建全局 `lisuan_dri.so -> gf_dri.so` 软链。
- 把 `__GLX_VENDOR_LIBRARY_NAME=gf` 写入全局 shell 或 Steam 客户端环境。

## 3. 容器内 Vulkan 探测

使用 Skill 自带 `scripts/vk_probe.py`。宿主对照：

```bash
VK_DRIVER_FILES=/usr/share/vulkan/icd.d/gf_icd.x86_64.json \
VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/gf_icd.x86_64.json \
python3 scripts/vk_probe.py
```

容器对照：

```bash
RT="$HOME/.local/share/Steam/steamapps/common/SteamLinuxRuntime_4"
PROBE="$PWD/scripts/vk_probe.py"
VK_DRIVER_FILES=/usr/share/vulkan/icd.d/gf_icd.x86_64.json \
VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/gf_icd.x86_64.json \
"$RT/_v2-entry-point" --verb=waitforexitandrun -- \
  /run/host/usr/bin/python3 "$PROBE"
```

典型结果：

- 宿主 gf：`LISUAN 7G100 Series Graphics`，1 个设备。
- 容器原版 gf：`vkEnumeratePhysicalDevices -> -3 count=0`。
- 容器 lvp：`llvmpipe (LLVM ...)`，1 个设备。

这证明“默认多 ICD 环境”下 DXVK 可能选 lvp，即使进程也映射了 gf 库。

## 4. gf 容器兼容四件套

### 4.1 os-release 重定向

gf 驱动读取 `/etc/os-release` 做发行版白名单。容器显示 Steam Runtime 时返回 `-3`。

生成宿主侧 `.so`，把 `/etc/os-release` 打开重定向到用户目录中的 Ubuntu `os-release` 文件，通过 `LD_PRELOAD` 注入游戏进程树。

### 4.2 ICD 协商与扩展垫片

新版 loader 对旧式 ICD 的协商版本和实例扩展校验更严格。使用：

- `gf_icd.json`：`library_path` 指向 `libvulkan_gf_shim.so`。
- `libvulkan_gf_shim.so`：对外协商 interface version 5，转发真实 ICD，补报/过滤被裁剪扩展。

编译前重写源码中的 `/home/ubuntu/gf-compat` 等硬编码路径。运行时若不需要逐调用 trace，应禁用 `slog`，避免每个 `vk_icdGetInstanceProcAddr` 都写文件。

### 4.3 驱动与着色器编译器

复制到容器可见的用户目录：

- `libvulkan_gf.so`
- `libSC.so`

`libSC.so` 不在 pressure-vessel 默认驱动合并列表中。Proton 可能重写 `LD_LIBRARY_PATH`，因此必须检查最终进程：

```bash
tr '\0' '\n' < /proc/<pid>/environ | grep -E 'LD_LIBRARY_PATH|VK_'
grep -E 'libSC|libvulkan_gf|gf_shim' /proc/<pid>/maps
```

若 `libSC.so` 未加载，在真实 ICD 初始化前显式 `dlopen(..., RTLD_GLOBAL)` 或通过 `LD_PRELOAD` 预加载。仅设置一个可能被覆盖的 `LD_LIBRARY_PATH` 不算完成。

### 4.4 环境变量

同时覆盖：

```bash
VK_ICD_FILENAMES=/path/to/gf_icd.json
VK_DRIVER_FILES=/path/to/gf_icd.json
```

按需禁用 Steam overlay Vulkan layer。不要只设置 `VK_ICD_FILENAMES`。

## 5. DXVK 选择

gf 当前缺少 `VK_KHR_maintenance5`。日志出现：

```text
Found device: LISUAN 7G100 Series Graphics
Skipping: Device does not support required feature 'maintenance5'
DXVK: No adapters found
```

说明不是“没有显卡”，而是 DXVK 版本与驱动能力不匹配。

策略：

- D3D11/DXGI：使用 DXVK 2.3。
- D3D8：DXVK 2.3 无原生实现，改用 wined3d。
- 多 ICD 环境可能回退 llvmpipe；必须检查最终日志和线程，不能只看到 gf 枚举成功。

## 6. DXVK DLL 的安装位置

Proton 启动时会同步自带 DXVK 到前缀 `drive_c/windows/system32`。实测先改前缀、再启动，文件会被还原为 Proton 11 的 v2.7.1。

对 64 位游戏，可靠做法：

1. 备份前缀原 DLL。
2. 从 Proton 8.0 的 DXVK 2.3 取 `d3d11.dll`、`dxgi.dll`。
3. 放到游戏主 exe 同目录，例如 `bin/<build>/bin64/`。
4. 启动后确认：
   - 日志 `Game: WorldOfWarships64.exe`，不是 `xalia.exe`。
   - `DXVK: v2.3-...`。
   - 设备未被 maintenance5 跳过。
   - `/proc/<pid>/maps` 指向游戏目录的 DLL。

注意：这只解决“DXVK 能选 gf”。若 `libSC.so` 未加载，仍可能在 `vkCreateDevice` 失败。

## 7. 低 FPS 诊断

```bash
python3 scripts/diagnose_process.py <pid>
pidstat -p <pid> -u -r -d 1 5
mpstat 1 5
vmstat 1 5
free -h
```

强软件渲染信号：

- 多个名为 `llvmpipe` 的线程。
- 每个线程持续占用一个逻辑核。
- 映射 `libvulkan_lvp.so`、`libLLVM.so`。
- 游戏进程长期占用 10+ 个 CPU 核心。
- 宿主/容器 probe 显示 lvp 可用而 gf 不可用。

临时启用 FPS HUD：

```text
DXVK_HUD=fps
DXVK_LOG_LEVEL=info
DXVK_LOG_PATH=/home/ubuntu/.local/share/<app>-gpu
```

验证后移除日志/HUD 环境，避免额外开销。

## 8. Steam 配置安全

- `localconfig.vdf` 运行中直接修改可能被 Steam 内存配置覆盖。
- 需要改 LaunchOptions 时，优先通过 Steam GUI；否则完全停止客户端后离线修改并立即启动验证。
- 启动 Steam 客户端使用干净环境，不能带 `SteamAppId`，否则客户端可能被当成游戏进程并出现 `WaitingPrevProcess`。
- Steam 正常退出卡住时先 SIGTERM；仍不退出才在用户同意后对明确 PID 使用 SIGKILL。
- 不修改共享 Proton 目录，除非已评估对所有 Proton 游戏的影响。

## 9. 验收清单

- [ ] 容器内 gf probe 返回 7G100。
- [ ] DXVK 日志为游戏本体且版本合适。
- [ ] 没有 `No adapters found`、`Skipping maintenance5`。
- [ ] `libSC.so` 已加载或确认不需要。
- [ ] 没有活跃 llvmpipe 渲染线程。
- [ ] CPU/内存/FPS 有对照数据。
- [ ] 截图显示真实内容。
- [ ] 变更、备份、回滚位置已记录。
