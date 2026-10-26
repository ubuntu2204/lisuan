# 植物大战僵尸 GOTY（Plants vs. Zombies, AppID 3590）在砺算 7G100 上的启动与渲染修复

> 32 位 Windows 游戏 × 只有 64 位用户态驱动的砺算 7G100。现象是"点了没反应"，
> 实际是**四层独立故障叠加**：容器内图形栈不可用、32 位进程拿不到厂商驱动、
> PopCap 启动器参数被本体拒收、new WoW64 的高地址显存映射接不住（黑屏）。

## 最终结论：默认关闭 3D Acceleration（2026-10-26）

**本机卡顿的根因是 wined3d GL 的硬件渲染路径本身，不是安全模式，也不是帧率限制。**
在已经能正常出画面的 GL 路径上实测：OPTIONS 里一关掉 `3D Acceleration`，游戏立即
流畅；重新打开则恢复卡顿。因此本目录现在把 `Is3D=0`（软件渲染）作为默认配置，
不再把“开启硬件加速”当作修复目标。

| 实测项 | 结果 |
|---|---|
| 硬件 GL（`Is3D=1`） | 能出画面，但卡顿；一次运行 25.17 秒消耗 CPU 47.57 秒，约 **189%** |
| 软件渲染（`Is3D=0`） | 同一台机器、同一版本，**关闭后立即流畅** |
| `--reset-3d-test` 复位安全模式 | 不解决硬件 GL 路径的卡顿 |
| OPTIONS 关闭全屏、窗口化 | 不解决硬件 GL 路径的卡顿 |
| 关闭 `3D Acceleration` | 唯一立即生效的解法 |

PvZ 是 800×600 的 2D 游戏，软件渲染完全够用。保留 new WoW64 + wined3d GL 的启动
环境，是因为它已验证能出画面、不黑屏；软件渲染下 `WINE_D3D_CONFIG` 实际不再决定
游戏的软/硬路径，游戏的 `Is3D` 才是开关。

### 日常使用

```bash
./fix-plants-vs-zombies.sh --launch             # 默认写 Is3D=0 后启动（推荐）
./fix-plants-vs-zombies.sh --software           # 只固化软件渲染，不启动
./fix-plants-vs-zombies.sh --status             # 查看当前软件/硬件渲染模式
./fix-plants-vs-zombies.sh --launch --hardware  # 仅复测硬件路径；本机实测会卡
./fix-plants-vs-zombies.sh --hardware           # 只写 Is3D=1，供以后对照
```

安装脚本和 `--launch` 都会默认固化软件渲染；桌面项与 Steam 入口沿用同一份配置。
切换时会备份 `user.reg.render-mode.bak`，不会移动或删除存档。

### 对旧结论的更正

* 旧文档把“画面正常但非常卡”主要归因于 PopCap 安全模式，并建议必须开启 3D 加速。
  那只适用于已废弃的 Vulkan/shim 黑屏路径和 llvmpipe 背景；**不适用于本次 GL 硬件
  路径卡顿**。现在不要再凭 `FailureReason=""` 推断游戏流畅或硬件加速一定开启。
* 已排除、不再继续叠加的方向：安全模式复位、窗口化、`vblank_mode`/限帧、GL↔Vulkan
  切换、低 4GB shim。它们都没有解决“开 3D 才卡”这一现象。
* 两条无害日志：`amdxcffx64.dll ... No such file or directory` 是 Proton 复制可选
  AMD 文件失败；`KeyboardInterrupt` 是终端里手动 `Ctrl+C` 的正常堆栈。二者都不是
  卡顿原因。

---

## 至砺算：官方实现什么，本目录的 workaround 即可退役

1. **提供 32 位用户态驱动（i386 的 GL/Vulkan）**——这是根因。本机 `lisuangpu-lib`
   只有 x86_64：`/usr/share/vulkan/icd.d/gf_icd.x86_64.json`、
   `/usr/lib/x86_64-linux-gnu/dri/gf_dri.so`；`/usr/lib/i386-linux-gnu/` 下既无
   `libvulkan_gf.so` 也无 `gf_dri.so`。本目录靠 **Proton new WoW64** 把 32 位 PE 的
   Vulkan 调用经 thunk 转给 64 位 ICD 来绕过；厂商若直接给 i386 驱动，
   这一层就不再需要。
2. **在 pressure-vessel 容器内让 gf 图形栈可用**：提供 gf 的 glvnd GLX/EGL vendor
   JSON 与 drirc 映射（`kernel_driver="gf" → dri_driver="gf"`）。容器内的 Mesa 只有
   `steamrt4_platform/.../share/drirc.d/00-mesa-defaults.conf`，没有厂商那份
   `01-gf_drv.conf`，于是 Mesa 找 `lisuan_dri.so` → 找不到 → `driver=(null)`。
   补齐后可不必"委托宿主直跑"。
3. **`libvulkan_gf.so` 补 `VK_KHR_maintenance5`**，并把 ICD 的 **loader interface
   version 升到 5**。这样 DXVK ≥2.4（含原生 d3d8 实现）就能直接用；当前 gf 会被
   DXVK 跳过，只能用 wined3d 的 Vulkan 后端。
4. **wined3d 的 Vulkan 后端补齐光栅/混合能力上报**（至少 `D3DPBCAPS_SRCALPHA`）：
   否则依赖老 D3D8 能力检测的游戏（如本作）会判定 Vulkan 后端不合格、只能退回 GL 后端。
5. **CPU 可见映射不要默认落在 ≥4GB**：new WoW64 下 `vkMapMemory` 返回高地址指针，
   wine 的 thunk 交不给 32 位代码，直接表现为黑屏 + “无响应”。驱动若优先在低 4GB
   分配，本目录的“低 4GB 重映射池”LD_PRELOAD 即可退役（与 minion-masters 同一诉求）。

---

## 症状

| 场景 | 表现 |
|---|---|
| Steam 里点"开始游戏" | 没有任何游戏画面；Steam 认为游戏在运行，3 分钟后被手动停止 |
| 宿主直跑启动器 | PopCap 弹窗：`FATAL ERROR / Invalid command line parameter: -changedir` |
| 宿主直跑本体（传统 wow64） | 能进游戏，但 **llvmpipe 软件渲染**：1080p 全屏 450–510% CPU |
| new WoW64 但未加重映射池 | **全屏黑屏** + GNOME 弹“steam_app_3590 无响应”，窗口存在、CPU 占满一核 |

复现时 `PROTON_LOG=1` 的日志在容器分支涨到 **1.7 GB**（`trace:seh:handle_syscall_fault` 刷屏）。

## 根因（四层，均有实证）

### 1. 容器内没有可用图形栈 → Wine 的 explorer 直接崩

容器分支的日志在加载 `winex11.drv` 后立刻出现：

```text
libGL error: No driver found
libGL error: failed to load driver: (null)
err:winediag:nodrv_CreateWindow Application tried to create a window, but no driver could be loaded.
err:winediag:nodrv_CreateWindow L"The explorer process failed to start."
```

容器 Mesa 缺 `lisuan→gf` 映射、也没有 gf 的 glvnd vendor JSON →
`libGL` 找不到 DRI 驱动 → `winex11.drv` 卸载 → Wine 建不了窗，游戏进程根本没起来。
（与仓库既有结论一致：`steam/steam-on-lisuan.md`、`underlords/`。）

### 2. 32 位进程拿不到厂商驱动 → 传统 wow64 下只能软渲染

PvZ 是 32 位 PE。传统 wow64 下它的 Vulkan/GL 由 i386 库承担，而厂商只装了 x86_64：

```text
/usr/share/vulkan/icd.d/gf_icd.x86_64.json     # 只有 64 位 ICD
/usr/lib/x86_64-linux-gnu/dri/gf_dri.so        # 只有 64 位 DRI
/usr/lib/i386-linux-gnu/  → 无 libvulkan_gf.so、dri/ 无 gf_dri.so
```

于是 32 位 DXVK 无适配器，wined3d 落到 Mesa llvmpipe。证据是 32 位进程自己的日志：

```text
fixme:d3d:wined3d_guess_card No card selector available for card vendor 0000
      (using GL_RENDERER "llvmpipe (LLVM 20.1.2, 256 bits)").
```

> **别被 64 位辅助进程的日志误导**：Proton 会为可访问性拉起 `xalia.exe`（64 位），
> 它加载 dxgi 时走 64 位 Vulkan，日志里因此出现
> `Game: xalia.exe` → `Found device: LISUAN 7G100 Series Graphics` →
> `Skipping: Device does not support required feature 'maintenance5'`。
> 这段属于 xalia，不是游戏本体。

### 3. PopCap 启动器传的参数，游戏本体不认

`PlantsVsZombies.exe`（Steam 版启动器）先把本体解到
`%ProgramData%\PopCap Games\PlantsVsZombies\popcapgame1.exe`，再以
`popcapgame1.exe -changedir="Z:\...\Plants Vs Zombies"` 拉起本体。本体对该参数
**任何形式都判非法**（`-changedir=<dir>` 与两段式 `-changedir <dir>` 实测都弹同一个框，
与路径是否含空格无关——复制到 `/tmp/pvz_nospace` 仍失败）。截图取证：窗口标题
`FATAL ERROR`，正文 `Invalid command line parameter: -changedir`。

### 4. 走 Vulkan 会遇到两个坑（最终选 GL 后端的原因）

**坑 4a：`vkMapMemory` 返回高地址，32 位代码接不住 → 黑屏**

new WoW64 下显存映射由 64 位宿主进程完成，返回的 CPU 指针在高位（如
`0x7fd188000000`），wine 的 thunk 无法把它交给 32 位代码，wined3d 建不出缓冲：

```text
fixme:vulkan:win32u_vkMapMemory2KHR returned mapping 0x7fd188000000 does not fit 32-bit pointer
err:d3d:wined3d_allocator_chunk_vk_map Failed to map chunk memory, vr VK_ERROR_OUT_OF_HOST_MEMORY.
```

现象：**全屏黑屏 + GNOME 弹“steam_app_3590 无响应”**（主线程等一个永远建不出的
缓冲，不再泵 X 事件循环），而窗口存在、CPU 占满一核。
解法是沿用 `minion-masters/gf-compat/` 的**低 4GB 重映射池** LD_PRELOAD（在 `mmap`
层把 ≥4GB 的 GPU 映射重定位到 2.25–4GB），实测可让画面出来。

**坑 4b：wined3d 的 Vulkan 后端不上报 alpha 混合能力 → 游戏自己关掉 3D 加速 → 卡**

PvZ 启动时自己跑一遍 3D 能力检测（字符串表里可见它查 `A4R4G4B4`/`A8R8G8B8`
纹理与 alpha/additive 混合），结论写进注册表，可直接读：

```text
renderer=vulkan → FailureReason = "Alpha blend Not Supported"   ← 不通过
renderer=gl     → FailureReason = ""                            ← 通过
```

不通过时游戏进 **PopCap 安全模式**：自动关掉 3D 加速、把存档隔离到
`userdata_backup/` → 变成软件渲染 → 用户看到的就是“画面正常但非常卡”。

**结论**：两个坑都是“Vulkan 路径”自带的；而 GL 路径在 wow64 下用 64 位
`libGL` → 厂商 GLX（`Using the OpenGL renderer` + `GL_VENDOR "LISUAN"`），既能硬件
渲染、又能过检测，而且根本不需要 shim（给 GL 加 shim 反而会让 gf 驱动在
`reloc64` 上死循环）。这就是最终配置。

> 另外，在两个后端之间来回切换会让 `Test3D\DisplayGUID` 缓存与实际能力集不一致，
> 旧结论把游戏锁在安全模式里——这就是 `--reset-3d-test` 存在的理由。

## 修复（硬件渲染路线，参考 minion-masters）

1. **Proton new WoW64**：`PROTON_USE_WOW64=1`，让 32 位 PE 经 wow64 thunk 调
   **64 位** gf Vulkan——不需要厂商提供 i386 驱动。前提是**前缀必须是 wow64 布局**：
   旧前缀沿用经典布局，必须移走重建。本目录用 `$COMPAT_DATA/.lisuan-pvz-wow64`
   标记位保证只迁移一次（`--reset-prefix` 可强制重来）。
2. **渲染后端用 wined3d 的 GL 实现**：`PROTON_USE_WINED3D=1` +
   `WINE_D3D_CONFIG=renderer=gl`（启动器里用 `PVZ_RENDERER` 可改）。
   wow64 下 GL 走的也是 64 位 `libGL` → 厂商 GLX，日志为
   `Using the OpenGL renderer` + `GL_VENDOR "LISUAN"`，仍是**硬件渲染**。

   **为什么不是 Vulkan 后端**：wined3d 的 Vulkan 后端不上报 `D3DPBCAPS_SRCALPHA`，
   而 PvZ 启动时会自己跑一次 3D 能力检测，结论写进注册表
   `HKCU\Software\SteamPopCap\PlantsVsZombies\Test3D\FailureReason`：

   ```text
   renderer=vulkan → "FailureReason"="Alpha blend Not Supported"   ← 不通过
   renderer=gl     → "FailureReason"=""                            ← 通过
   ```

   不通过时游戏进 **PopCap 安全模式**：自动关掉 3D 加速 + 把存档隔离到
   `userdata_backup/` → 变成软件渲染 → 表现就是“非常卡”。因此本目录提供
   `--check-3d`（读游戏自己的结论，无需看画面）与 `--reset-3d-test`（跳出安全模式）。

   **为什么不用 DXVK**：PvZ 走 **DirectDraw/D3D8**（二进制里 `Direct3DCreate8`、
   `DirectDrawCreateEx`）。DXVK **2.3 没有 d3d8**（仓库里的 `minion-masters/dxvk23`
   就是 2.3，只有 d3d9/d3d11/dxgi），而 DXVK ≥2.4 硬要求 `VK_KHR_maintenance5`
   （gf 缺），所以 DXVK 两条路都堵死——只能走 wined3d，而 wined3d 里只有 GL
   后端能过游戏的 alpha 混合检测。
3. **抽本体常驻**：启动一次启动器，趁 `popcapgame1.exe` 存在时复制到
   `~/.local/share/lisuan-fix/plants-vs-zombies/pvz-game.exe`（启动器退出会删掉它）。
   放在 Steam 安装目录之外，避免"验证文件完整性"把产物删掉。
   启动时 cwd = 游戏目录（本体按 cwd 找 `main.pak`），**不带任何参数**。
4. **容器委托**：Steam 内点"开始游戏"时，启动选项把整条命令交给
   `plants-vs-zombies-steam.sh`；它在容器内用 `systemd-run --user --wait` 委托宿主拉起
   宿主启动器并回传退出码（同 `left4dead2/`、`kingdom-rush/`、`underlords/`）。
   委托能否成功取决于三件容易踩错的事，见下面的「委托链路的三个坑」。
5. **低 4GB 重映射池（只对 vulkan 后端需要）**：`minion-masters/gf-compat/` 的
   `osrelease_redirect.so` 在 `mmap` 层把“结果地址 ≥ 4GB 且 fd 是 GPU 设备”的映射
   搬到 2.25–4GB。启动器仅在 `PVZ_RENDERER=vulkan` 时预加载它；**默认的 GL 路径
   不加载**，因为 shim 会把 GL 的 DRI 映射也搬走，gf 驱动会陷入 `reloc64` 死循环
   （实测 200% CPU 不推进）。
6. `flock /tmp/plants-vs-zombies-3590.lock` 保证单实例。

### 委托链路的三个坑（本轮实测抓出来）

| 坑 | 现象 | 正确做法 |
|---|---|---|
| 用**容器内**的 `ld.so` 跑宿主的 `systemd-run` | `symbol lookup error: …/libc.so.6: undefined symbol: __nptl_change_stack_perm, version GLIBC_PRIVATE` | 必须用**宿主**的动态链接器 `$HOSTPFX/lib64/ld-linux-x86-64.so.2`（或 `$HOSTPFX/lib/x86_64-linux-gnu/…`），再 `--library-path` 指宿主库目录 |
| 硬写总线路径 `$HOSTPFX/run/user/<uid>/bus` | `Failed to connect to bus: No such file or directory`——steamrt4 **不挂载宿主 /run** | 逐个探测候选：容器默认的 `$DBUS_SESSION_BUS_ADDRESS`（pressure-vessel 代理到 `/run/pressure-vessel/bus`）→ `$HOSTPFX/run/user/<uid>/bus` → `/run/user/<uid>/bus`，用 `systemctl --user show-environment` 验活；全连不上就**报错退出**，绝不在容器内直接跑游戏 |
| 只认 `/run/host` 一种宿主挂载点 | 手工跑 `_v2-entry-point` 时是 `/run/host`，而 **Steam 客户端实际启动游戏时是 `/host`** | `/run/host` 与 `/host` 都探测，`$HOSTPFX` 贯穿 ld.so / 库路径 / 总线 / XAUTH |

## 部署

```bash
./fix-plants-vs-zombies.sh                 # 安装/修复（前缀迁移 + 抽本体 + 部署 + 默认软件渲染）
./fix-plants-vs-zombies.sh --status        # 状态（含当前软件/硬件渲染模式）
./fix-plants-vs-zombies.sh --launch        # 默认关闭 3D 加速后宿主直跑（推荐）
./fix-plants-vs-zombies.sh --software      # 只固化软件渲染（Is3D=0）
./fix-plants-vs-zombies.sh --launch --hardware  # 开启 3D 加速复测（本机实测会卡）
./fix-plants-vs-zombies.sh --hardware      # 只固化硬件渲染（Is3D=1）
./fix-plants-vs-zombies.sh --force         # 游戏更新后重抽本体
./fix-plants-vs-zombies.sh --reset-prefix   # 强制把前缀移走重建为 wow64
./fix-plants-vs-zombies.sh --set-launch-options   # 写入 Steam 启动选项（需先退出 Steam）
./fix-plants-vs-zombies.sh --clear-launch-options
./fix-plants-vs-zombies.sh --uninstall
```

安装后的布局：

```text
~/.local/share/lisuan-fix/plants-vs-zombies/
├── pvz-game.exe                     # 从启动器抽出的本体（3.55 MB）
├── plants-vs-zombies-host-run.sh    # 宿主启动器（new WoW64 + wined3d GL；默认 Is3D=0 软件渲染）
└── plants-vs-zombies-steam.sh       # Steam 入口（容器内委托宿主）
~/.local/share/applications/plants-vs-zombies-lisuan.desktop
```

Steam 启动选项（也可直接在 Steam 属性里粘贴）：

```text
"/home/ubuntu/.local/share/lisuan-fix/plants-vs-zombies/plants-vs-zombies-steam.sh" %command%
```

## 验证（2026-10-25 本机实测）

| 验证项 | 结果 |
|---|---|
| wined3d 后端 | `Using the Vulkan renderer` + 设备 `LISUAN 7G100 Series Graphics` `4c54:5006` |
| GPU 占用 | 游戏进程持有 `/dev/dri/renderD128`（2 个 fd） |
| CPU | 1080p 全屏从 450–510%（llvmpipe）降到 **~100%**（单核） |
| 进程架构 | 游戏进程是 64 位 `wine-preloader`（wow64 生效） |
| 稳定性 | 窗口 `Plants vs. Zombies` 连续存活 90s+，无崩溃弹窗 |
| 音频 | 进程持有 PulseAudio memfd，bass 音频链路已建立 |
| 渲染后端 | `Using the OpenGL renderer` + `GL_VENDOR "LISUAN"`（wow64 下走 64 位厂商 GLX，硬件） |
| **游戏自己的 3D 检测** | `--check-3d` 读到 `FailureReason=""` → **通过**，3D 加速保持开启（Vulkan 后端下同一字段为 `Alpha blend Not Supported`） |
| 安全模式 | 存档留在 `userdata/`，未被隔离到 `userdata_backup/` |
| 活性 | `_NET_WM_PING` 发向游戏窗口并在 8s 内收到回复 → 事件循环正常（修复前收不到，GNOME 因此弹“无响应”） |
| 显存映射 | 加 shim 后 `does not fit 32-bit pointer` / `Failed to map chunk` 均为 **0**（修复前必现），shim 日志 `reloc` 3179 次 |
| **Steam 按钮端到端** | 库里点"开始游戏"→ Steam 命令行变为 `"…/plants-vs-zombies-steam.sh" …`→ 容器内 `in_container=1 hostpfx=/host`、总线走代理 → 宿主单元 `pvz-plantsvszombies.service` 拉起游戏（GPU fd 4、CPU ~150%）→ Steam 状态 `App Running` |
| 画面 | **已人眼确认正常**（2026-10-25，Steam 里点"开始游戏"后标题画面正常显示） |
| 退出回传 | 游戏结束后单元 `inactive`、包装进程退出、Steam 状态回到 `Fully Installed`（时长统计有效） |
| 前缀迁移 | `--reset-prefix` 移走旧前缀 → Proton 重建 → 仍是硬件渲染 |
| **卡顿最终结论（2026-10-26）** | 同一 GL 启动环境下，开 `3D Acceleration` 卡顿、关掉立即流畅；安全模式复位与窗口化均无效 |
| 本机默认配置 | 已写入 `Is3D=dword:00000000`；`--status` 显示“软件渲染” |
| 离线测试 | `tests/test-plants-vs-zombies.sh` 42/42；全量 `tests/run-tests.sh` 9/9 |

> **验证方法的教训**：本轮一度把“窗口存在 + 持有 `/dev/dri/renderD128` + CPU 占用”
> 当作渲染正常的证据，实际是黑屏。现在改用 `_NET_WM_PING` 活性 + 映射失败计数
> 作最低门槛，**画面本身必须让人看一眼**（Wayland 会话下无法自行截图）。
>
> 截图验证的边界：本机是 Wayland 会话，XWayland 下 `xwd` 抓不到 GPU 直出窗口；
> 改用 Xephyr（嵌套 X，**没有 DRI3**）则无论有无 shim 都会崩在 `wined3d.dll` 同一
> 地址（`0001:000F07A2`），属嵌套 X 的 Vulkan WSI 限制，不能用于验证本路径。

## 已知限制

* **跨目录依赖**：宿主启动器预加载的 shim 在
  `minion-masters/gf-compat/lib/osrelease_redirect.so`（与随从大师共用同一个产物）。
  它内部把伪造的 `/etc/os-release` 路径写死为仓库内绝对路径，所以仓库移动位置后需
  重跑 `minion-masters/gf-compat/deploy.sh` 重编；本目录的 `find_shim()` 只校
  验存在与“是 64 位”，可用 `PVZ_SHIM=<路径>` 改指。
* **硬件 GL 路径仍会卡**：若手动开了 `3D Acceleration`，本机会回到约 190–200% CPU
  的卡顿状态；执行 `--software` 或在 OPTIONS 里关掉即可。不要再为此调
  `WaitForVSync`/`vblank_mode`——2026-10-26 已确认它们不是这次卡顿的解法。
* **首次安装要重建前缀**（约 30–60 秒）：wow64 布局与旧前缀不兼容；旧前缀会保留为
  `compatdata/3590/pfx.pre-wow64-<时间>`，其中的本地存档/PopCap userdata 不自动迁移
  （Steam 云存档不受影响）。
* **wined3d Vulkan 的 D3D8 噪音**：日志有若干
  `err:d3d:validate_state_table State STATE_RENDER(WINED3D_RS_*) should have a representative`，
  是 D3D8 老状态在 Vulkan 后端没有对应实现，非致命。
* **Steam 内启动需要一次启动选项设置**：`localconfig.vdf` 由 Steam 自己回写，
  只能"退出 Steam → `--set-launch-options` → 再启动"；不想动配置就直接用桌面项。
  本机已写入并实测通过。
* **异常杀进程后 Steam 的"运行中"会滞留**：强杀 `pvz-game.exe` 会留下 `winedevice.exe`，
  宿主单元进入 `deactivating (stop-sigterm)` 直到 systemd 停止超时（约 90 秒），
  期间 `systemd-run --wait` 不返回、Steam 仍显示运行中。游戏内正常退出无此问题。
* **别在带 `SteamAppId` 的 shell 里启动 Steam 客户端**：客户端会继承该变量而被
  当成游戏进程本身，表现为新启动卡在 `WaitingPrevProcess` 后直接 `Completed`（“点了没反应”）。
  排查先验：`tr '\0' '\n' < /proc/<steam 主进程>/environ | grep SteamAppId`。
* **Steam 覆盖层**：容器注入的是 32 位 `gameoverlayrenderer.so`，而委托后的游戏进程是
  64 位，日志会有一行 `wrong ELF class: ELFCLASS32 … ignored`（仅噪音，不影响运行）。
* **游戏更新后**本体可能变化：`--force` 重抽一次。
* 若渲染异常（花屏/黑屏），先确认状态里"前缀已标记为 new WoW64 布局"为 [OK]，
  再考虑 `--reset-prefix` 重建。

## 排障速查

| 现象 | 先看哪里 |
|---|---|
| **画面正常但非常卡** | 先看 `--status`：若是“硬件渲染”，执行 `--software` 或在 OPTIONS 关掉 `3D Acceleration`；这是 2026-10-26 验证有效的解法 |
| 日志 `amdxcffx64.dll ... No such file or directory` | Proton 可选 AMD 文件缺失，可忽略，与卡顿无关 |
| 日志 `KeyboardInterrupt` | 终端里手动 `Ctrl+C` 的正常堆栈，不是游戏崩溃 |
| **全屏黑屏 + “无响应”** | 只在 `PVZ_RENDERER=vulkan` 下会发生（低 4GB 重映射池没生效）；用默认 gl 后端不涉及此路径 |
| Steam 里点了仍没反应 | `~/.local/share/Steam/logs/console_log.txt` 里该 AppID 的命令行是否已变成 `plants-vs-zombies-steam.sh`；没变 = 启动选项未生效 |
| 启动了但秒退 | `/tmp/plants-vs-zombies-hostrun.log`（含 `in_container` / `hostpfx` / `bus` / 退出码） |
| 日志 `undefined symbol: __nptl_change_stack_perm` | 用了容器 ld.so → 重跑安装脚本拿新版委托逻辑 |
| 日志 `Failed to connect to bus` | 总线候选全失败，查 `ls -l /run/pressure-vessel/bus` 与用户 systemd 是否在跑 |
| 游戏弹 `-changedir` | 启动选项丢了，Steam 又去跑 PopCap 启动器了 |
| CPU 又回到 400%+ | 前缀不是 wow64 布局 → `--reset-prefix` 重建 |
| Steam 长时间显示"运行中" | 单元 `deactivating`，`systemctl --user status pvz-plantsvszombies.service` 看残留 wine 子进程 |
