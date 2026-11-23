# 砺算 7G100 分层排障与既有结论

所有结论以厂商驱动 **v262.2.0.56** 为验证基准。遇到其它版本先告警，不直接套用字节偏移、专有库路径或游戏 workaround。

## 1. 先取证，后修改

```bash
cd /home/ubuntu/project/lisuan
python3 .codebuddy/skills/lisuan-7g100-compat/scripts/check_driver_baseline.py
python3 install.py --detect --json
bash dkms/verify-lisuan-gpu.sh
bash mesa/verify-system-mesa.sh
lspci -nnk
lsmod | grep -E 'gf|lisuan'
modinfo "$(find /lib/modules/$(uname -r) -name 'gf.ko*' | head -1)" | grep vermagic
dkms status
```

判定顺序：

1. PCI `4c54:5006` 是否存在；
2. `lisuangpu-dkms` / `lisuangpu-lib` 是否均为 v262.2.0.56；
3. `gf.ko` 的 vermagic 是否与 `uname -r` 一致；
4. `dkms status` 的 installed 不等于模块已加载，必须查 `lsmod`；
5. `/dev/dri/card*` 与 DRM 注册名；
6. 用户态 DRI/GLX/Vulkan/GBM；
7. 应用进程树与日志。

## 2. 内核 / DKMS

- 厂商 `Makefile` 以 `KERNEL_VERSION` 决定编译内核，但 DKMS 传入 `KERNELVER`；内核升级后出现 vermagic 错配。
- 使用 `dkms/patches/0001-dkms-makefile-honor-KERNELVER.patch` 与 `dkms/fix-lisuan-gpu.sh`，保持 `vendor-src/Makefile` 原样未补丁，补丁在部署阶段应用。
- 修复后重新体检 `gf.ko` 的 vermagic，不能只看 DKMS installed。

## 3. Mesa / GLX / Vulkan / GBM

- DRM 注册名为 `lisuan`，用户态驱动名为 `gf`；PCI 厂商号 `4c54` 对应字符串 `LT`。
- GLX vendor 环境变量是双下划线 `__GLX_VENDOR_LIBRARY_NAME=gf`；在 Steam Runtime 容器内 gf Vulkan ICD 可自动识别。
- 厂商 `gf_gbm.so` 与系统 libgbm 25.2.8 ABI 不兼容，Steam CEF GPU 进程会 SIGSEGV；GFM 后端用 `GBM_BACKEND=dri` 强制加载 Mesa `dri_gbm.so`。
- 厂商 Mesa 22.0.4 分叉不能被系统 Mesa 25.2.x 直接加载；zink 是已分析的替代路径，详见 `mesa/system-mesa-analysis.md`。
- gf Vulkan ICD 的协商接口版本为旧式版本 0，新版 loader 可能静默丢弃；`minion-masters/gf-compat/` 垫片对外报版本 5。
- gf 缺少 `VK_KHR_maintenance5`，Proton Experimental 自带 DXVK v3.0.2 会跳过设备；随从大师方案使用自定义 Proton 副本或 Proton 11 加固，降级到 DXVK v2.3。
- 禁止创建 `lisuan_dri.so -> gf_dri.so` 全局软链；禁止向 Steam 客户端注入 `LD_PRELOAD=libglapi_gf.so.0`。

## 4. Steam CEF

- 正确启动命令是 `GBM_BACKEND=dri LIBGL_ALWAYS_SOFTWARE=1 __GLX_VENDOR_LIBRARY_NAME=mesa steam -cef-disable-gpu`。
- 检查 `~/.local/share/Steam/logs/cef_log.txt` 中 `exit_code=139` 或 `GPU process isn't usable`；点系统信息崩溃查 `console-linux.txt` 的 `double free` / `_XAllocID`；界面卡加载动画查 `webhelper-linux.txt` 的 `Exiting GPU process due to errors during initialization`。
- 当前唯一修复是**全部** `steam.desktop`（用户级菜单项 + XDG 桌面图标 + 历史 `~/Desktop`）的 `Exec` 行注入 `GBM_BACKEND=dri LIBGL_ALWAYS_SOFTWARE=1 __GLX_VENDOR_LIBRARY_NAME=mesa` 与 `-cef-disable-gpu`，由 `install_steam_fix()` 宽松解析并保留用户其它 env 定制。
- 启动时弹 `OpenGL GLX context is not using direct rendering`（KB 9938-EYZB-7457）是 GLX 上下文建不起来（`glXCreateContext failed` / GLX BadValue）的自检结果，加 `__GLX_VENDOR_LIBRARY_NAME=mesa` 即可消除；2026-11-13 八组消融显示只有该变量有效且无新增崩溃，`MESA_LOADER_DRIVER_OVERRIDE=llvmpipe` 无效。B3/B4 类「弹窗为 0」是客户端根本没起来，不是修好。详见 `steam/steam-on-lisuan.md` §3.2。
- 点「帮助→系统信息」闪退是第二层：`drirc` 把 gf 设备映射到 `gf_dri.so` / `libglapi_gf.so.0`，新 GL 上下文在容器里 double free 或 Xlib 断言。`LIBGL_ALWAYS_SOFTWARE=1` 防崩溃，但它会让 CEF ANGLE/EGL GPU 进程初始化失败、界面停在加载动画，所以必须再加 Steam 官方命令行参数 `-cef-disable-gpu` 走 CPU 合成。单独用该参数仍会崩；不要改 `VK_ICD_FILENAMES`（会连累游戏 Vulkan），也不要用 `MESA_LOADER_DRIVER_OVERRIDE=llvmpipe`（重复 `failed to load driver`）。
- 不要修改 `steamwebhelper_sniper_wrap.sh`；不要 `chattr +i`；不要**往 wrap.sh 注入** `--disable-gpu`（会改文件大小触发校验回滚甚至反复重下游戏）。`-cef-disable-gpu` 是传给 `/usr/bin/steam` 的官方命令行参数，与注入 wrap.sh 不是一回事。详见 `steam/steam-on-lisuan.md` §3 / §3.1。
- Steam 的 `steamid=0` 陈旧态通常重启客户端即可，不需要图形登录界面；首次登录例外。

## 5. Electron / Chromium

- 通用 wrapper：`electron/use-electron-gf.sh`，基础开关 `--no-sandbox --ozone-platform=x11 --ozone-platform-hint=x11 --disable-gpu-compositing`。
- Electron 43 会忽略 `--ozone-platform-hint=x11`，必须同时给显式 `--ozone-platform=x11`。
- gf 硬件路径需 ICD 协商垫片补报 `VK_KHR_external_semaphore_capabilities` / `_fence_capabilities`；默认先用 `--no-hw` 软件渲染建立稳定基线。
- Trae 在 gf EGL 路径下黑屏/闪烁，默认使用 ANGLE/SwiftShader 软件路径。
- 不要把 gf env 写成全局；只注入目标应用进程树。

## 6. 音频

- 真正出声的是海光板载 HDA `1d94:14c9`，不是 7G100 伴生音频 `4c54:5010`。
- 三层根因：PCI ID 不在 `snd_hda_intel` 表中、`snd_intel_dspcfg` 误选 SOF/SST、ALC222 上电慢。
- 解法：`driver_override` + `dsp_driver=1` + `probe_mask=0xff delay=2000`，开机自绑服务。
- 验证：`aplay -l` 出现 `card 0: Intel [HDA Intel], device 0: ALC222 Analog`。

## 7. Proton / Wine 决策

- 64 位原生应用与 64 位 Wine 不需要 32 位低 4GB 重映射。
- 32 位游戏经 new WoW64 调 64 位 gf 驱动时，`vkMapMemory` 可能返回高于 4GB 的地址，WoW64 thunk 无法交给 32 位代码，DXVK 报 `VK_ERROR_OUT_OF_HOST_MEMORY`。随从大师的隔离方案用 `LD_PRELOAD` 对目标游戏做低 4GB 重定位，不全局补丁驱动。
- 原生直跑 Proton 必须提供：
  - `WINEDLLPATH=<Proton>/files/lib/vkd3d:<Proton>/files/lib/wine`
  - `LD_LIBRARY_PATH=<Proton>/files/lib/x86_64-linux-gnu:<Proton>/files/lib/i386-linux-gnu`
  - 独立 `WINEPREFIX`
  - `WINEDLLOVERRIDES=mscoree,mshtml=`
  - Mesa/llvmpipe 或经确认的 gf 渲染环境
- Wayland 下手动直跑 Wine 可能缺 `VK_KHR_surface`，经 Steam Runtime 容器可正常。
- 用 llvmpipe 建立稳定基线：`VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json`、`PROTON_USE_WOW64=1`、`__GLX_VENDOR_LIBRARY_NAME=mesa`。
- 不要手动降级 Proton 自带 DXVK；修改官方 DXVK 文件会触发 Steam“内容文件损坏”。

## 8. 应用 / 游戏既有结论

### 随从大师（Minion Masters, 489520）

- Proton 11.0 稳定版 + 加固脚本已实测完整可玩；加固三要素：`libSC.so` 软链、DXVK v2.3、doorstop 直载 `MMPatch.dll`。
- 真正生效源码是 `assets-mm-gf-vulkan/MMPatch.cs`；Vivox patch 必须停用，否则拿不到 token 并卡登录。
- Vivox 在 Wine 下本身可正常登录；证书 patch 曾破坏 IL2CPP runtime 导致黑屏，必须禁用。
- 使用 `il2cpp_class_get_method_from_name`，不要用 `il2cpp_class_get_methods`（binding 签名错）。
- `LoginQueueService` HTTP 404 是非阻塞噪音。
- llvmpipe 软件渲染方案需从 Steam 启动，命令行直跑约 90 秒后因缺 Steam 认证上下文退出。

### 刀塔霸业（Underlords）

- 原生 64 位 `play-native.sh`；必须保持 `--no-autorestart`，崩溃后让用户感知，不自动拉起。
- 系统原版 `libvulkan_gf.so` 与 `osrelease_redirect` 隔离验证通过，不影响其他游戏。

### 求生之路 2（L4D2, 550）

- 原生 32 位 `hl2_linux -vulkan` → DXVK v2.0 → 32 位 Mesa Venus ICD → Unix socket → 私有 64 位 `virgl_test_server --venus` → `libvulkan_gf.so`。
- D16/MatQueue0 闪退由 gf 错误上报 `VK_EXT_attachment_feedback_loop_layout` 引起，Venus 补丁 0002 屏蔽扩展。
- locale 模态 Warning 由 `launcher.so` 与 `steamclient.so` 设置冲突引起；`setlocale-shim.c` 仅在确认调用者为 `steamclient.so` 时改写 `LC_ALL`。
- `fix-l4d2.sh` 使用 `systemd-run --wait` 传真实退出码，不做隐式重启。

### 植物大战僵尸 GOTY

- 32 位 Windows 游戏；new WoW64 + `PROTON_USE_WINED3D=1` + `WINE_D3D_CONFIG=renderer=gl` + 宿主软件渲染。
- 默认 `Is3D=0` 软件路径；wined3d Vulkan 后端不上报 alpha blend 能力，DXVK 又要求 gf 不支持的 `maintenance5`。
- 容器内无图形栈时 `winex11.drv` 加载失败，`explorer.exe` 建窗失败；不要靠自动重启掩盖。

### 豆包 Windows 客户端

- Deepin Wine 8.16 的已知坑：WinSxS 绝对软链指向 `/home/zcr`、`AppData/LocalLow` 缺失触发 cryptnet `FILE* = -1`、gf 图形环境卡住、桌面裸 `--uri` 进入修复、降级后 Chromium 缓存和 `Last Version` 不兼容、自动更新到 2.32.6。
- 旧版 1.56.6 / Chromium 129 在修复前缀 + Mesa 软件渲染下可用；新版 2.32.6 / Chromium 147 在 Deepin Wine 8.16 缺 `RtlGetDeviceFamilyInfoEnum`，Proton 11 已导出该函数。
- 2026-11-08 已能启动真实界面，**结论要按图形会话区分**：旧会话
  （11-07 22:00 – 11-08 02:53）里裸跑宿主 `wine` 主窗口 `IsUnMapped`（强制 XMap 仍纯黑
  1 色）、约 10–20 秒 `exit_code 0` 主动退出；用
  `SteamLinuxRuntime_sniper/run-in-sniper -- bash doubao/run-doubao-wine11.sh` 则
  `IsViewable`、546 色（`chrome://doubao-chat/`）、主进程 65 秒以上存活，是当时唯一成功路径。
  机器 07:50:20 重启后，裸 wine 与容器**都能**出界面，全新空 `User Data` 亦如此。
  故容器是**稳健默认（隔离宿主 Mesa/GL 栈）而非唯一必要条件**，黑屏指向宿主图形会话
  状态（具体项未定论）。启动器已默认包装容器，`DOUBAO_NO_STEAM_RUNTIME=1` 走轻量裸 wine，
  找不到 `run-in-sniper` 时明确报错而非静默降级。
- 消融已排除：profile 状态（`Local State` / `Preferences`）、云配置 `Remote_State`、
  8 组渲染开关、网络（WinINet 在两个前缀访问 `https://www.doubao.com/` 均 HTTP 200）、
  `DCompositionCreateDevice3` stub（两个 Wine 都返回 `E_NOTIMPL`，Chromium 本就回退）。
  给 Wine 8.16 补 `RtlGetDeviceFamilyInfoEnum` 导出只消除 `win_util.cc(544)` FATAL，
  随后仍进修复流程且无主窗口，此路已证伪，不要重做。详见 `doubao/README.md` §2.1.1。
- 桌面入口另有回归：`~/Desktop/豆包.lnk` 会被 DING 交给 deepin-wine8-stable 并撞 `0x7F` FATAL；
  `doubao/fix-doubao-desktop.sh` 统一**四个** `.desktop`（桌面 / 用户级 / 系统级 /
  deb entries）的 Exec、去掉非标准 shebang、打 `metadata::trusted`、把 `.lnk` 移出桌面；
  `doubao/run-doubao-wine.sh` 已改为安全转发到 Wine 11（可 `--uninstall` 还原）。
- 「当前版本启动发生异常，是否进行修复？」弹窗来自**旧 Deepin 前缀**的
  `repair_*.exe`：`repair/record.json` 的 `running_exception` 被排查期间反复
  `wineserver -k` / `timeout` 累到阈值（实测 `2.32.6: 8`）。修复是备份后清零该计数
  **并堵死旧入口**，不是禁用修复器 / 改名 `update.exe` / 删账号数据；
  Wine 11 前缀的计数一直是 0。详见 `doubao/README.md` §2.5。
- 禁止把 `update.exe` 改名，豆包修复器会把它列入 `missing_files`；版本屏蔽配置也不能阻止 `update_data` 覆盖更新器。
- Windows 客户端全量 Electron 重打包不推荐；网页版 Electron 外壳是功能受限的备用方案。

### 即梦 AI Windows 客户端（1.0.59；已迁至 hygon 仓库）

- 补丁、脚本与完整排障记录已于 2026-11-23 迁至 `~/project/hygon/Dreamina/`（见其 `README.md`）；本仓库不再提供 `install.py` 菜单 28/29。
- 要点：electron-builder NSIS 安装器需替换 Wine 的 powershell 空壳 stub（凡含 `Get-CimInstance` 返回 1）；渲染黑屏用 `--use-gl=desktop` 绕开 ANGLE 的 `WGL_NV_DX_interop2`，与 gf 驱动无关；桌面图标由官方 PE 的 `RT_ICON` 提取，不入库。

### 魔兽争霸 III 1.24 本体（`/opt/apps/war3/files`，32 位）

- 32 位 `War3.exe`（1.24.6.387）在本机只能配同架构的 32 位 Vulkan 实现。继承宿主 `VK_ICD_FILENAMES`（`gf_icd.x86_64.json`，64 位）时，D3D8 → vkd3d → Vulkan 链初始化失败，进程跳到野地址 `0x35800000`（`c0000005`，伴随 `Storm Error : memory never released`），窗口一闪即灭；改用 `/usr/share/vulkan/icd.d/lvp_icd.json`（32 位 llvmpipe）后正常出窗。固化在 `war3/run-war3.sh`（变量只注入目标进程树）。
- **同一 wine 前缀并发实例** → 后启动者弹 `Warcraft III was unable to initialize.` 并挂住（双击桌面图标最常见）。受控消融证明注册表 `HKCU\Software\Blizzard Entertainment\Warcraft III\Video`（含 `lockfb`）**不是**根因。启动前用 `/proc/<wineserver-pid>/environ` 里的 `WINEPREFIX` 判占用并显式报错退出；不监控、不守护、不自动重启。
- 验收：干净启动出现 1920×1080 窗口（`IsViewable`），XWayland 单窗口抓帧显示 Frozen Throne 主菜单完整渲染（12.6 万色、非黑像素 72.9%）；9 次受控启动 0 次崩溃、0 次错误框。联机与单局对战未验收。
- 该场景下 Wayland 合成抓图（`capture-wayland-screen.py`）建流成功但取不到帧（`gst-launch` 25 秒超时）；改用 XWayland 单窗口抓帧作补充证据。

### 11 对战平台（Proton 11 / wine-11.0）

- **现行方案：官网正式版 2.6.3.48**，默认前缀 `~/.deepinwine/11duizhan-263`，主程序 `11Loader.exe` + `bins/11Client.exe`。安装包走 `go.5211game.com/?id=10`（需 Referer）；安装/桌面/启动脚本已默认指向它。**登录后卡死已修复**：先运行 `./11/fix-11-fonts.sh`（把 Wine 五类系统字体从 Tahoma 改为 Arial，幂等、备份、可 `--uninstall`），再 `./11/run-11duizhan.sh`。根因是 Wine 返回本机不存在的 Tahoma，CEF 105 的 `SystemFonts::Initialize()` 在 `AddFont()` 期间创建 SkTypeface 失败，回退调用 `GetDefaultSystemFont()` 时 `is_initialized_` 仍为 false，导致无限重入、GDI 耗尽和断言；用官方 CEF 105 PDB + Chromium 105 源码确认。实测登录后主窗口 `1243×720`、服务器测速完成、大厅完整渲染，GDI/栈/异常均为 0。`fix-11-stack.sh`（栈）、`redirect-11-gdi-iat.sh`（IAT 抓调用点）、CEF thunk 定点 A/B 均为历史取证/证伪路径，不是修复；厂商二进制已还原。不做看门狗/自动重启。
- 下文全是 `3.0.0.x` 公测版（历史前缀 `~/.deepinwine/11duizhan-wine11`）的遗留问题，不要拿它当正式版结论。
- 安装是 Inno Setup（`/VERYSILENT`，不是 NSIS `/S`）；桌面图标从主程序 PE 资源提取。窗口 `Depth 32` 在 Wayland 合成下不可见，`XLIB_SKIP_ARGB_VISUALS=1` 可修可见性，但不代表平台可用。
- 启动崩溃真凶是 `yybins/YYClient.exe`（不是拉起器 `11loader.exe`）：频道序列化递归帧约 4KB，PE 栈仅 1MB，`stack overflow ... addr 0x48de11`（3.0.0.68）/ `0x48e441`（3.0.0.79）。`11/fix-11-stack.sh` 改 16MB/64KB，按忽略栈字段的 SHA256 轮转备份。
- 平台自更新会在 3.0.0.68 ↔ 3.0.0.79 间来回切换并冲掉栈补丁；`11/run-11duizhan.sh` 无参数默认 `-updatemode=0` 冻结版本，`E11_ALLOW_UPDATE=1` 可恢复。**这是绕过更新器，不是修好**；冻结后仍需重跑栈补丁。
- 当前阻塞是 Wine 11 GDI 句柄表 65536 耗尽：thread 0144 每轮泄漏 1 显示 DC + 1 FONT + 4 REGION，计数 REGION 138927/95302、FONT 48169/37248、DC 10910/2；随后 libcef `0x80000003`（EIP `0x719fc9dd`）。冻结版本后仍复现 `out of GDI object handles` 5 次，不能说已修好。
- 已证伪：归错到 11loader、只改 11loader 栈、CEF/QT/虚拟桌面/Proton 8/全新前缀等表面开关、把 GDI 配额降到 10000（更早崩）、`LD_PRELOAD` 拦截 `NtUserGetDC`（0 命中，Wine dispatcher 用内部函数指针）、`user32` PE wrapper + `WINEDLLOVERRIDES`（known DLL 使客户端仍加载内置 user32；只拦到 loader）、`gdb` 跨 `__wine_syscall_dispatcher` 的 `bt`（栈帧不可靠）。`11/fix-11-gdi.sh` 等目前只是诊断件。
- 完整证据、失败方向与下一步（IAT 重定向 `USER32.dll`→等长 `e11gdi.dll`）见 `.codebuddy/rules/11平台启动排障约定.md` 与 `11/README.md` §5–§9。禁止看门狗/自动重启。

## 9. 日志与验收

- `Player.log` 被 `NotSupportedException` 刷屏时先 `grep -vE` 滤噪，再看最初 20 行。
- Unity 启动会轮转 `Player.log` 为 `Player-prev.log`，用 mtime 区分本次运行。
- 崩溃取证可用 `coredumpctl`、`ulimit -c unlimited`、`dmesg`、`journalctl --user`、Crashpad 转储；先定位根因，不自动重启。
- 验收至少包括：目标窗口出现、主进程持续存活、关键错误计数为 0 或确认为既有非阻塞噪音、重复启动稳定、用户数据未被清除。

## 10. Wayland 合成画面取证（2026-11-14）

- **不要把 X11 抓图当证据**：本机 GNOME Wayland + XWayland 下，`xwd`（窗口与 root）、`ffmpeg -f x11grab`（窗口与 1920×1080 全屏）、PIL `ImageGrab` 都拿不到合成后内容，全屏实测 207 万像素纯黑、`xwd` 报 `BadMatch`。这是 `XGetImage` 的全局限制，不是应用渲染故障。
- 已证伪通道：GNOME Shell `org.gnome.Shell.Screenshot` D-Bus 返回 `AccessDenied: Screenshot is not allowed`；`xdotool key shift+Print`（XTEST）到不了 Wayland 合成器，不落盘；portal Screenshot 会走交互授权，不适合自动取证。
- **可用通道**：`org.gnome.Mutter.ScreenCast` D-Bus + PipeWire + GStreamer。工具 `.codebuddy/skills/lisuan-7g100-compat/scripts/capture-wayland-screen.py`，用法 `python3 <脚本> -o /tmp/shot.png`，`--check` 只读查依赖。首帧实测 1920×1080 / 34486 色，能看到桌面、任务栏与各窗口。
- **关键坑**：ScreenCast session 与创建它的 D-Bus 连接绑定；`gdbus call` 逐条调用会在连接断开后被 Mutter 回收（再调 `RecordMonitor` 报「对象不存在」）。必须在同一条连接里完成 `CreateSession → RecordMonitor('DP-1') →` 订阅 `PipeWireStreamAdded(u node_id)` → `Session.Start()` → `gst-launch-1.0 pipewiresrc path=<node_id> num-buffers=1 ! videoconvert ! pngenc ! filesink` → `Session.Stop()`。
- 会话必须显式 Stop，不留后台录屏；不做常驻/守护/自动重试。规则见 `.codebuddy/rules/Wayland抓图约定.md`。
- 应用内 WebView 另可用 `QTWEBENGINE_REMOTE_DEBUGGING=9222` + CDP 读 `devicePixelRatio` / `Page.captureScreenshot`（只读 Web 层，不替代整屏合成画面）。
