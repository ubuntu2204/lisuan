# Plants-vs-Zombies 变更记录

> 从仓库根 `CHANGELOG.md` 拆分而来（2026-11-19）。按时间倒序；历史条目保留当时的命令与路径。

## 2026.10.25 — 植物大战僵尸 GOTY（AppID 3590）四层根因与 7G100 硬件渲染修复

### 新增

- **`Plants-vs-Zombies/`**：《植物大战僵尸 GOTY》修复集——`fix-plants-vs-zombies.sh`
  （安装/修复/状态/卸载/前缀迁移/启动选项）、`patch_launch_options.py`（只改目标 AppID
  的 `LaunchOptions`，写入前做括号平衡自检）、`README.md`（根因与证据）。运行时产物落在
  `~/.local/share/lisuan-fix/plants-vs-zombies/`，不进 Steam 安装目录，避免"验证文件完整性"误删。
- 交互索引新增 **24（安装/修复）/ 25（说明）**，状态检测覆盖已安装/未安装，卸载接入 `UNDO`。

### 根因（四层，均有日志/截图实证）

1. **容器内无可用图形栈**：容器 Mesa 缺厂商 `01-gf_drv.conf` 的 `lisuan→gf` 映射，也无 gf 的
   glvnd vendor → `libGL error: No driver found` → `winex11.drv` 卸载、Wine 建不了窗；
   `PROTON_LOG=1` 在该分支刷出 1.7 GB 日志。
2. **传统 wow64 下 32 位进程拿不到厂商驱动**：厂商只装 x86_64（`gf_icd.x86_64.json`、
   `gf_dri.so`），i386 下无 `libvulkan_gf.so`/`gf_dri.so` → 32 位 DXVK 无 ICD、
   wined3d 落到 Mesa llvmpipe（1080p 全屏 450–510% CPU）。**不接受软件渲染**。
3. **PopCap 启动器传的 `-changedir` 被本体判非法**：启动器把本体解到 `%ProgramData%` 后带
   `-changedir="<游戏目录>"` 启动，本体对任何形式的该参数都弹
   `FATAL ERROR: Invalid command line parameter: -changedir`（与路径是否含空格无关）。
4. **new WoW64 的高地址显存映射接不住（黑屏真因）**：`vkMapMemory` 返回 ≥4GB 指针，
   wine 的 thunk 交不给 32 位代码 → `wined3d_allocator_chunk_vk_map Failed to map chunk
   memory` → 全屏黑屏 + GNOME 弹“steam_app_3590 无响应”（窗口存在、CPU 占满一核）。

### 黑屏回归与修正（2026-10-25 晚）

首版只做到 new WoW64 + wined3d Vulkan，**把“窗口存在 + 持有 `/dev/dri/renderD128` +
CPU 占用”当成了渲染正常的证据**，实际是黑屏。补上 minion-masters 的同一个修复后：

- 宿主启动器默认 `LD_PRELOAD` 低 4GB 重映射池
  （`minion-masters/gf-compat/lib/osrelease_redirect.so`，安装时解析路径并校验为 64 位，
  可用 `PVZ_SHIM` 覆盖；`--status` 单列一项）。
- 实测：`does not fit 32-bit pointer` / `Failed to map chunk` 计数归零，shim 日志
  `reloc` 3179 次；`_NET_WM_PING` 从“无回应”变为“8s 内回应”；GPU fd 从 2 变 4。
- **画面已由用户确认正常**（Steam 里点“开始游戏”，标题画面正常显示）。

### 第2次修正：“画面正常但非常卡”——游戏自己的 3D 检测否决了 Vulkan 后端

上一步只验证到“标题画面能出”，用户反馈**非常卡**，并补图了墓碑框
`Not Supported / Hardware Acceleration cannot be enabled on this computer`。这次不再靠猜：
游戏自己把检测结论写进了注册表，可直接读。

- 本体字符串表暴露检测项：`A4R4G4B4`/`A8R8G8B8` 纹理与 alpha/additive 混合。
- 对比两个后端的实测结论（`HKCU\Software\SteamPopCap\PlantsVsZombies\Test3D\FailureReason`）：
  `renderer=vulkan` → `"Alpha blend Not Supported"`（不通过）；`renderer=gl` → `""`（通过）。
- 不通过时游戏进 **PopCap 安全模式**：自动关掉 3D 加速、把存档隔离到
  `userdata_backup/` → 退回软件渲染 → 这就是“卡”的来源。之前把卡归因到
  “GL 后端比 Vulkan 慢”是错的。
- 因此最终配置改为 **wined3d GL 后端**（wow64 下用 64 位 `libGL` → 厂商 GLX，
  `GL_VENDOR "LISUAN"`，仍是硬件渲染），且 **默认不加载低 4GB shim**：
  shim 会把 GL 的 DRI 映射也搬进低 4GB，gf 驱动会陷入 `reloc64` 死循环。
  shim 仅在 `PVZ_RENDERER=vulkan` 时才预加载。
- 新增两个能力：`--check-3d`（读游戏自己的结论，不需看画面即可判定硬件加速
  是否真开着）与 `--reset-3d-test`（清 `Test3D` 缓存 + 把被隔离的存档放回
  `userdata/`，将游戏从安全模式拉出来）；`--status` 现在也显示这一项。
- 现场已复位：删掉失效的 `Test3D` 键、`user1.dat` 放回 `userdata/`、`user.reg` 先备份。
- 本轮走过的弯路也记录在此：曾在 Vulkan/GL 之间反复切换，使 `Test3D\DisplayGUID`
  与实际能力集不一致，把游戏锁在安全模式里——这正是需要 `--reset-3d-test` 的原因。

### 修复（沿用 minion-masters 的 32 位硬件渲染路线）

- **Proton new WoW64**（`PROTON_USE_WOW64=1`）：32 位 PE 经 wow64 thunk 调 **64 位** gf Vulkan；
  前缀必须重建为 wow64 布局，脚本用 `$COMPAT_DATA/.lisuan-pvz-wow64` 标记位只迁移一次
  （旧前缀保留为 `pfx.pre-wow64-<ts>`，`--reset-prefix` 可强制重来）。
- **wined3d + Vulkan 后端**（`PROTON_USE_WINED3D=1` + `WINE_D3D_CONFIG=renderer=vulkan`）：
  PvZ 是 DirectDraw/D3D8 游戏，DXVK 2.3 无 d3d8、DXVK ≥2.4 需 `VK_KHR_maintenance5`（gf 缺），
  故走 wined3d 的 Vulkan 实现。
- 抽本体常驻 + 宿主直跑（cwd = 游戏目录、不带参数）；`flock` 单实例。
- Steam 内"开始游戏"：启动选项指向 `plants-vs-zombies-steam.sh`，容器内经会话总线
  `systemd-run --user --wait` 委托宿主拉起并回传退出码（同 `kingdom-rush/`、`underlords/`）。

### 验证

- wined3d 日志确认 `Using the Vulkan renderer` + 设备 `LISUAN 7G100 Series Graphics` `4c54:5006`。
- 游戏进程为 64 位 `wine-preloader`（wow64 生效），持有 `/dev/dri/renderD128`（2 个 fd）。
- CPU：1080p 全屏 450–510%（llvmpipe）→ **~100%**（单核）；窗口连续存活 90s+，无崩溃弹窗。
- 音频：进程持有 PulseAudio memfd，bass 链路已建立。
- **Steam 按钮端到端**（已写入启动选项并重启 Steam 实测）：库里点"开始游戏"→ Steam 命令行
  变为 `plants-vs-zombies-steam.sh` → 容器内 `in_container=1 hostpfx=/host`、总线走代理 →
  宿主单元 `pvz-plantsvszombies.service` 拉起游戏（GPU fd 2、CPU ~100%）→ Steam `App Running`；
  游戏结束后单元 `inactive`、Steam 状态回到 `Fully Installed`。
- 离线测试：新增 `tests/test-plants-vs-zombies.sh`（19 项，覆盖脚本语法、wow64/Vulkan 后端断言、
  委托链路断言、`LaunchOptions` 的 VDF 结构/幂等/清除/未知 AppID）；全量 `tests/run-tests.sh` 9/9 通过。

### 委托链路本轮修掉的坑（均有日志原文）

- 用容器内 ld.so 跑宿主 `systemd-run` → `undefined symbol: __nptl_change_stack_perm (GLIBC_PRIVATE)`；
  改用宿主 `$HOSTPFX/lib64/ld-linux-x86-64.so.2` + `--library-path`。
- steamrt4 **不挂载宿主 /run**，硬写的 `$HOSTPFX/run/user/<uid>/bus` 不存在 →
  `Failed to connect to bus`；改为逐个探测总线候选并用 `systemctl --user show-environment` 验活
  （pressure-vessel 会把会话总线代理到 `/run/pressure-vessel/bus`）。
- 宿主挂载点不唯一：手跑 `_v2-entry-point` 时是 `/run/host`，**Steam 客户端真启动游戏时是 `/host`**；
  两者都探测后 Steam 按钮路径才能稳定委托。
- 另记一坑（排查代价最大）：从带 `SteamAppId=3590` 的 shell 重启 Steam 客户端，客户端被
  当成游戏进程本身，导致新启动卡在 `WaitingPrevProcess` 后直接 `Completed`（“点了没反应”）。

### 仍未覆盖

- Xephyr（无 DRI3）下硬件路径会崩在 wined3d，属嵌套 X 的 Vulkan WSI 限制；真实显示（XWayland）下窗口稳定。
