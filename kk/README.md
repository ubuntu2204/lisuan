# KK 官方对战平台（魔兽争霸 III）在砺算 7G100 上的安装与运行

> 至砺算：安装链路与显卡无关；运行期的崩溃也不是 gf 驱动缺陷，而是 QtWebEngine
> 依赖的 wined3d / Vulkan 后端在 Wine 11 下初始化失败。给目标进程显式指定
> Mesa llvmpipe Vulkan ICD 与 wined3d 的 GL 渲染器即可稳定显示界面。若官方
> 让 32 位 WoW64 场景下的 Vulkan/GL 设备发现完整可用，可再评估硬件渲染路径。

## 1. 厂商专有二进制（不入库）

| 项 | 内容 |
| --- | --- |
| **是什么** | `4_pf_files_KKDuiZhanDownloader-v1.0.2.454.exe`：KK 官方对战平台的 NSIS v3.06.1 PE32 下载器安装器（7.9 MB，`requireAdministrator`）。`/S` 静默安装后落地 `C:\Program Files (x86)\kkduizhan\`（本机实测 1.1 GB / 539 个文件），含 `Platform.exe`（48 MB 主程序）、`PlatformDownloader.exe`、`KKEmulator.exe`、`Uninstall.exe` 以及 Warcraft III 1.28/1.29 补丁文件。 |
| **为何不能入库** | 厂商专有二进制、升级会改文件名；按 `.codebuddy/rules/仓库组织约定.md` 二，用 `.gitignore` 规则 `kk/4_pf_files_KKDuiZhanDownloader-*.exe` 排除，仓库只保留脚本与文档。文件仍留在本目录供运行时使用，不删除。生成的图标写入 `~/.local/share/icons/hicolor/`，同样不入库。 |
| **从哪获取** | KK 官方对战平台官网/官方分发页下载 `4_pf_files_KKDuiZhanDownloader-v<版本>.exe`，放到本目录、`~/下载` 或 `~/Downloads`，脚本会自动查找。本机这份 SHA256：`418354d8023686a8d1580c18499fc380b471980eb471f59c33ce6d7566875659`。安装产物在 `~/.deepinwine/kkduizhan-wine11/drive_c/Program Files (x86)/kkduizhan/`，同样不入库。 |

### 1.1 反编译产物（不入库）

| 项 | 内容 |
| --- | --- |
| **是什么** | `kk/reverse/` 下 12 个 KK 自有 PE 模块的静态逆向产物，约 190 MB。每个模块一个子目录：`meta.txt`（PE 头 / 节表 / 导入导出表）、`strings-ascii.txt` 与 `strings-utf16.txt`（字符串）、`disasm.txt`（Intel 语法反汇编）。超过 `KK_MAX_DISASM` 的大模块（`Platform.exe`、`NEP2.dll`）只生成元数据与字符串，附 `DISASM-SKIPPED.txt`。由 `kk/decompile-kk.sh` 生成。 |
| **为何不能入库** | 厂商专有二进制的派生物，含 KK 实现细节与机器相关绝对路径；按 `.codebuddy/rules/仓库组织约定.md` 二，用 `.gitignore` 规则 `kk/reverse/` 整体排除，仓库只保留生成脚本与本文档。本机无 Ghidra / IDA / radare2，产物是**反汇编 + 元数据**级别，不是伪代码级反编译。 |
| **从哪获取** | 先跑 `./kk/install-kk-platform.sh` 安装到 `~/.deepinwine/kkduizhan-wine11/drive_c/Program Files (x86)/kkduizhan/`，再跑 `./kk/decompile-kk.sh` 生成；`--status` 只读查看清单，`--clean` 删除产物，`--force` 覆盖。依赖 binutils 的 `i686-w64-mingw32-objdump`（32 位 PE32）与 `objdump`（PE32+ 的 `KernelDumpAnalyzer.exe`）。 |

用途：理解 KK 对 `UpdateLayeredWindow` / `SetParent` / CEF 等 API 的调用方式，辅助排查 Wine 下的窗口与 web 内容兼容性问题（§5 未修复项）。不修改软件本身，不联网，无自动重启 / 守护逻辑。

## 2. 安装（2026-11-14 实测）

```bash
cd ~/project/lisuan
./kk/install-kk-platform.sh                 # 自动找安装器，NSIS /S 静默安装
./kk/install-kk-platform.sh --status       # 只读体检
```

- 前缀 `~/.deepinwine/kkduizhan-wine11`，Wine 取自
  `~/.local/share/Steam/steamapps/common/Proton 11.0/files/bin/wine`（wine-11.0）。
  只在该 wine 进程树上设置 `WINEPREFIX` / `WINEDLLPATH` / `LD_LIBRARY_PATH` /
  `WINEDLLOVERRIDES=mscoree,mshtml=`，不污染全局 shell 或 Steam 客户端。
- 实测：安装器 `/S` 返回 0；`Platform.exe`、`Uninstall.exe` 均为有效 PE；
  安装目录 1.1 GB / 539 文件；`user.reg` 写入 `Platform.exe "%1"` 文件关联；
  安装器自带 Wine 菜单项与 `drive_c/users/Public/Desktop/KK官方对战平台.lnk`
  （只在 Wine 前缀里，Linux 桌面看不到，见 §4）。
- 幂等：检测到有效 `Platform.exe` 时只体检；返回码 `2`/`124`/其它分别报错并
  保留日志，不做自动重试或守护。环境变量：`KK_PROTON`、
  `KK_WINE11_PREFIX`、`KK_INSTALL_TIMEOUT`、`KK_INSTALL_LOG`、`KK_WINEDEBUG`。

## 3. 运行问题与解法（2026-11-14 实测）

**现象**：从裸 Proton wine 启动 `Platform.exe`，无窗口、约 3 秒后退出（rc=5）。
应用日志 `log/<时间戳>/platform.log` 停在 `[PlatformManager:1117] clear cache`；
Wine `+seh,+loaddll` 日志显示 `EXCEPTION_ACCESS_VIOLATION`（读地址 8）发生在
`Qt5WebEngineCore.dll` 基址 `0x71C20000` + `0x82FA45`，应用的 CrashHunter 捕获后
静默退出。

**根因**：客户端是 32 位 Qt5 + QtWebEngine，应用自己强制 `QT_OPENGL=angle`。
默认 wined3d / Vulkan 后端在 Wine 11 下没能把 GL 设备初始化出来，ANGLE 拿到空
对象 → QtWebEngineCore 空指针读。与砺算显卡硬件无关，也不是安装不完整
（`resources/*.pak`、`icudtl.dat`、`plugins/qt/platforms/qwindows.dll` 都齐全）。

**消融**（每组单次启动、显式收尾，均带 `GBM_BACKEND=dri` +
`LIBGL_ALWAYS_SOFTWARE=1` + `__GLX_VENDOR_LIBRARY_NAME=mesa` 共同基底）：

| 组 | 变化 | 结果 |
| --- | --- | --- |
| A | 基线（无下列两项） | rc=5、无窗口、`c0000005` |
| B / C | `QT_OPENGL=software` / `desktop` | 无效；应用日志仍打印 `QT_OPENGL = angle`（代码强制覆盖） |
| D / E | `QTWEBENGINE_CHROMIUM_FLAGS=--disable-gpu` / `--use-gl=desktop ...` | 无效 |
| F | 工作目录改为安装目录 | 无效（排除 QtWebEngine 资源路径） |
| G / H | 命令行 `--disable-gpu ...` / `--use-gl=desktop ...` | 无效 |
| **I** | **+ `VK_ICD_FILENAMES=lvp_icd.json` + `WINE_D3D_CONFIG=renderer=gl`** | **两个窗口、主进程持续存活** |
| **K** | **只加 `VK_ICD_FILENAMES=lvp_icd.json`** | **成功** |
| **L** | **只加 `WINE_D3D_CONFIG=renderer=gl`** | **成功** |

I 之后未再跑 Steam Runtime 容器组：裸 wine 已稳定，容器非必要（与豆包客户端
「容器是稳健默认而非唯一必要条件」的结论一致）。共同基底三个 GL 变量未单独
消融，保留它们是为了与项目其它 Wine 应用基线一致、且不污染全局。

**现行启动器**：`kk/run-kk-platform.sh`，在应用进程树内设置
`WINEPREFIX` / `WINEDLLPATH` / `LD_LIBRARY_PATH` / `WINEDLLOVERRIDES`、
`GBM_BACKEND=dri`、`LIBGL_ALWAYS_SOFTWARE=1`、`__GLX_VENDOR_LIBRARY_NAME=mesa`、
`VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json`、
`WINE_D3D_CONFIG=renderer=vulkan`，`cd` 到安装目录后 `exec` wine。
历史消融（11-14）显示 lvp / renderer=gl 任一单独即可越过原崩溃，曾同设作冗余
保险；**11-16 夜服务端上线寻宝活动 web 后 renderer=gl 的 llvmpipe GL 路径忙等
活锁（~100% CPU、登录/大厅冻死），vulkan 路径同内容正常推进，故撤 GL 单列
vulkan**（裸 Wine 对照取证）。日志落在
`~/.local/share/kkduizhan-wine11/launch.log`，可用 `KK_LAUNCH_LOG` 覆盖。
**没有任何自动重启 / 重试 / 守护逻辑**。

**验收**（3 次独立启动：I 组、复验、固化脚本）：
- 两个窗口标题「KK官方对战平台」，`Map State: IsViewable`，
  1334×947 与 920×560；X11 抓图颜色数 5674–11113 / 116–3370（真实界面，
  不是纯黑或加载动画）；窗口 PID 归属当前 `Platform.exe`，收尾后窗口消失。
- 应用日志越过原崩溃点，出现 `ClientConfig` / `WndLobby*` / `CouponLib`，
  并拿到 `status:200` 的网络响应；无 `c0000005`。
- 主进程持续存活到我们用 `timeout` / `wineserver -k` 显式收尾（rc=124 表示
  主动结束，不是崩溃）；无残留进程。

## 3.1 硬件渲染方案（7G100，2026-11-17，大厅黑块已消除）

**结果**：大厅 web 区（左侧游戏列表、顶部导航、活动弹窗、背景海报）**完整显示，
黑块消失**；UI 位置与点击命中由移植过来的 wrapper+shim 修正（内容层 `(294,33)`
= 主框架客户区原点）。合成图颜色数从补丁前 58537 → 补丁后 182282。

**根因（纠正早期判断）**：黑块**不是** CEF GPU 进程崩溃造成（gf 下 GPU 进程稳定、
登录页 web 完整，但大厅仍黑），而是 Wine `winex11.drv` 的
`needs_offscreen_rendering()` 把 KK 主框架客户面判为 `offscreen=TRUE`：分层子窗
`SetParent` 后 SYSRGN 复杂 → `needs_client_window_clipping()` 返回 TRUE →
client surface 被 `detach_client_window`、渲染转投 offscreen pixmap 从不 present
到可见 X 窗口。`+x11drv` 取证：大厅状态 363 次 `offscreen 1`，主框架 `0x300ce`
被 detach。

**修复**：对专用 Proton 副本的 `winex11.so` 打 5 字节二进制补丁，把
`needs_offscreen_rendering` 的 TRUE 出口（`0x1ddc2` 处 `mov eax,1`）改为
`xor eax,eax;nop×3`，强制恒返回 FALSE → 所有 client surface 直接绘到可见窗口。
补丁点绑定 md5 白名单（版本变化即跳过并告警），备份 `.orig-nooffscreen`，
`setup-kk-hw-proton.sh --restore-offscreen` 可撤销。**只改专用 Proton 副本，
不改全局 Proton、不触发 Steam 校验。**

**用法**（独立环境，不影响 §3 软件方案）：

```bash
./kk/setup-kk-hw-proton.sh                 # 一次性：专用 Proton + DXVK v2.3 + offscreen 补丁
bash kk/run-kk-platform-hw.sh --reset      # 首次：建独立 new WoW64 前缀 + 植入 KK/登录态
bash kk/run-kk-platform-hw.sh --keep       # 启动后保留进程（手动登录用）
bash kk/run-kk-platform-hw.sh --status     # 只读体检（含补丁状态）
bash kk/run-kk-platform-hw.sh              # 常规启动（单次、显式收尾、无守护）
```

首次在新前缀里自动登录 `token=` 为空（KK token 服务端动态签发、绑定会话），
在登录页手动登录一次后，会话保存在该前缀，后续可自动进大厅。

关键实现（与 `minion-masters/fix-minion-masters-7g100-hw.sh` 同源）：
- **独立前缀** `compatdata/481234`：必须让 proton 自己初始化成 new WoW64；
  直接复制传统 WoW64 前缀会让 32 位 CEF 无法经 thunk 调到 64 位 gf ICD。
- **专用 Proton `- KK HW`**：DXVK v2.7.1 → v2.3（gf 缺 `VK_KHR_maintenance5`，
  v2.7.1 会 `Skipping device` → `Failed to initialize DXVK` → KK 两行退出）+
  offscreen 补丁（本节核心）。
- **gf-compat**：`osrelease_redirect.so`（发行版白名单 + 低 4GB 重映射）、垫片
  ICD `gf_icd.json`、`libSC.so` 软链；`VK_LOADER_LAYERS_DISABLE` 关 Steam overlay。
- **几何修复层**：复用 `kk-user32-wrapper.c` + `kk-geometry-shim.c` 修 UI/按键错位；
  `KK_GEOMETRY_SHIM=0` 可关。实测 wrapper 在 new WoW64 下可用，之前"启动即崩"
  的真凶是部署 wrapper 时 `cp` **跟随软链**覆盖了 Proton 自带的
  `i386-windows/user32.dll`（→ forwarder 找不到 `user32_orig` → `LoadStringW`
  未实现 → 启动退出）。修法：先 `rm -f` 前缀 user32.dll 软链再 `cp`，只替换
  前缀内文件、不动 Proton 源。若曾污染，从全局 Proton 11 复制同名 dll 恢复。
- 启动用 `proton waitforexitandrun`（不是裸 `wine`：32 位 loader 会拒载 64 位
  hook，报 `wrong ELF class`），由 proton 提供容器（`VK_KHR_surface`）。

**不做**：不监控、不守护、不崩溃后自动重启（见崩溃处理约定）。

## 4. 图标与桌面入口

安装器只在 Wine 前缀里留了 `KK官方对战平台.lnk`，Linux 桌面/应用列表既没有
入口也没有图标（豆包客户端曾遇到同类的 `.lnk` 被 DING 交给 deepin-wine 的坑）。

```bash
./kk/fix-kk-desktop.sh                 # 提取图标 + 写入口（幂等）
./kk/fix-kk-desktop.sh --status       # 只读体检
./kk/fix-kk-desktop.sh --uninstall   # 恢复备份、删除图标与新建入口
```

- `kk/extract-icon.py`（与 `dreamina/extract-icon.py` 同源，纯标准库解析 PE
  `RT_ICON` / `RT_GROUP_ICON`，不依赖 icoutils / Pillow）从本机 `Platform.exe`
  提取官方图标，写入 `~/.local/share/icons/hicolor/{16..256}.../apps/kkduizhan.png`
  共 10 个尺寸，最大 256×256；厂商图标产物不入库。
- 桌面项写入 `~/.local/share/applications/kkduizhan.desktop`、XDG 桌面
  (`~/桌面/`) 与历史 `~/Desktop/`，统一 `Icon=kkduizhan`、
  `Exec="<仓库>/kk/run-kk-platform.sh"`、`Categories=Game;`，并打
  `metadata::trusted=true`。只改 `Icon` / `Exec` 两行、保留用户其它定制；
  改文件前在 `~/.local/share/kkduizhan-wine11/desktop-backups/` 生成 `.bak`。
  硬件方案用 `--hw` 切换，`Exec` 必须是
  `"<仓库>/kk/run-kk-platform-hw.sh" --keep`：硬件启动器默认取证后显式收尾
  （杀掉 KK），不带 `--keep` 会出现「双击图标闪一下就没了」。桌面项同时补
  `u+x`（否则 GIO `access::can-execute=FALSE`、双击不启动），并在生成图标后
  刷新用户级 hicolor 缓存（否则桌面仍显示旧/通用图标）；均非看门狗或自动重启。
- 实测 `--status` 全项 `[OK]`，`desktop-file-validate` 无警告；`--uninstall`
  从备份还原（新建的直接删除）并移除生成的图标。

## 5. 界面现象：弹窗遮罩（非 bug）与 UI 错位（2026-11-14 纠正）

**结论（据用户纠正）**：启动时盖住主界面的半透明深灰层是活动弹窗自带的
**模态遮罩**，点弹窗的 `X` 关闭即消失，是正常 UI，**不是渲染 bug**，不需要任何
ARGB / 合成层干预。此前把它误判为「32 位 ARGB 窗口重影」并注入
`XLIB_SKIP_ARGB_VISUALS=1`；Wayland 合成画面显示窗口虽变不透明，却让
QtWebEngine 主内容（游戏列表 / 商城导航）整片空白，代价大于收益，**已撤销**。
详见 `CHANGELOG.md` 的 `2026.11.14 — KK 界面现象纠正` 条目。

### 真正的缺陷：Wine 对分层子窗口的坐标语义误处理（2026-11-14 深夜定案）

关掉遮罩后界面元素整体偏左上、按键点不动。`WINEDEBUG=+x11drv,+win,+event`
取证链：

1. KK 的 Qt UI 先以 `WS_POPUP` 在 `(0,0)` 创建内容层（Depth 32、1332×946、
   `WS_EX_LAYERED|TOPMOST`），再 `NtUserSetParent` 挂到主框架（Depth 24、
   1334×947）下，随后用 `UpdateLayeredWindow(pptDst=(0,0))` 定位。
   **Windows 对子窗口把 pptDst 解释为父客户区坐标，Wine/Proton 11 的 win32u
   按屏幕坐标处理** → 内容层 win32k 矩形永远停在屏幕 `(0,0)`。
2. 视觉：内容层 X 窗口画在 `(0,0)`，与主框架 `(293,32)` 错开整个框架原点。
3. 输入：win32k 的子窗口命中测试裁剪到父矩形，内容层几乎整片在父矩形外，
   侧栏 / 导航的 `ButtonPress` 在 win32k 直接 `hittest 0` 丢弃（trace 实证）。
4. 堆叠：内容层是 override-redirect + TOPMOST，盖住活动弹窗的原生窗口，
   弹窗的 `X` / 按钮永远收不到点击；mutter 又把受管窗口恒排在 OR 窗口之下
   且忽略客户端 raise 请求。
5. 渲染：`SetParent(web 子窗口)` 期间主框架客户面被误判 offscreen，
   QtWebEngine 客户窗口被 `detach_client_window` 停到 1×1 dummy 父窗口下，
   游戏列表整片不可见。

另取证：Wine 的 managed / unmanaged 两条路径都**不把 X 几何回写 win32k**
（`window_update_client_config` 对 unmanaged 直接返回 0，managed 等 WM_STATE
而 OR 窗口永远等不到），所以任何「外部移主框架」的方案都修不了父子矩形。

#### 源码级复核：`+win,+user32` 取证 + Wine 11.0 win32u 源码（2026-11-16）

用 `KK_WINEDEBUG='+win,+user32'` 单次启动 50 秒（timeout 显式收尾、rc=124
为主动结束非崩溃、无残留），日志落
`~/.local/share/kkduizhan-wine11/win-user32-trace.log`（50438 行；不入库）。
对照 Wine 11.0 `dlls/win32u/window.c`（上游开源、本机临时下载核对，不入库）：

- `NtUserUpdateLayeredWindow`（`window.c:2550-2607`）先以
  `get_window_rects(hwnd, COORDS_PARENT, &new_rects, ...)` 取矩形，再用
  `pts_dst - new_rects.window.left/top` 算 offset 平移三块矩形。
- `get_window_rects` 的 `COORDS_PARENT` 分支（`window.c:1799-1823`）在父窗口
  存在且非 RTL 时**只校验、不做父客户区原点平移**，故 `new_rects` 仍是含
  父偏移的窗口矩形。KK 按 Windows 语义传 `pptDst=(0,0)`（父客户区原点）
  时，offset 等于减掉父原点 → win32k 矩形算回屏幕 `(0,0)`，与上面第 1 条
  的现象同源。这是 Wine 的坐标语义缺陷，不是 KK 或显卡驱动的问题。
- `NtUserSetParent`（`window.c:569-668`）用 `COORDS_PARENT` 取子窗口矩形、
  `COORDS_SCREEN` 取新旧屏幕矩形，再以
  `set_window_pos(..., new_screen_rect.left - old_screen_rect.left, ...)`
  保持视觉位置——这正是主框架从 `(293,17)` 挪到 `(293,32)` 时内容层不跟着
  重算 win32k 父矩形的原因。

本次日志实证（wrapper + shim 均已加载）：

- 主框架 `0x300e2`：1332×945 创建于 `(0,0)` → `MoveWindow(293,17)` →
  `SetWindowPos(293,32)`，客户区原点 `(294,33)`。
- **内容层 `0x10172`（1332×946）**：创建于 `(0,0)`，挂到 `0x300e2` 后 17 次
  `NtUserUpdateLayeredWindow` 的 `new_rects` 全部稳定为
  `(294,33)-(1626,979)`——`kk-user32-wrapper.c` 的「钉死父客户区原点」
  已生效，这是侧栏命中零偏移的直接证据。
- `0x10100`（1156×882）是**登录页 web 子窗口、不是内容层**：它 `SetParent`
  到弹窗再到主框架，最终 `calc_winpos` 矩形 `(0,59)-(1330,943)`、
  `client (0,59)-(0,59)`（1×1 dummy）。排查时曾一度把它误认作内容层，
  据此得出「内容层仍在 (0,0)、按键偏移未修」的错误结论；实际内容层是
  `0x10172`（精确匹配 wrapper 的 1332×946 判据），偏移早已闭环，勿再照抄
  该误判。`0x10100` 的 1×1 dummy 对应下方「web 内容面不呈现」的既有缺陷。

**结论**：按键偏移已由双层修复闭环，`ui-autotest.py` 本轮实测仍 PASS
（真实点击魔兽争霸3 视觉中心即命中选中、命中零偏移）。源码对照用于解释
根因链条，不构成新增修复项；反编译 KK 的 PE 对此问题无用（根因在 Wine
win32u，KK 侧的 `pptDst=(0,0)` 由 trace 已直接可见）。

### 现行解法：双层修复（`kk/run-kk-platform.sh`，`KK_GEOMETRY_SHIM=0` 可关）

- **根因层 `kk/kk-user32-wrapper.c`**：native user32 包装层（mingw 编译，
  `WINEDLLOVERRIDES=user32=n,b` 仅 KK 进程树；64 位进程回退内置）。除
  `UpdateLayeredWindow` / `UpdateLayeredWindowIndirect` 外全部导出以 PE
  forwarder 转回 `user32_orig.dll`（内置 user32 的重命名副本，unixlib 名字嵌
  在 PE 里、改名加载已实测）。自实现换算子窗口的父客户区坐标 pptDst
  （含内容层 `(0,0)` 与弹窗关闭按钮的 `(popup_width-42,0)` 等非零相对坐标）
  → `MapWindowPoints` 换屏幕坐标；「保持位置」调用（pptDst == 当前全局矩形
  原点且非 `(0,0)`）原样透传（一律换算会二次偏移成 `(588,66)` 振荡，trace
  取证）。**内容层（1332×946）另钉死在父客户区原点**：KK 的 ULW 语义混杂
  （原点 / 保持位置 / 屏幕坐标），逐一判读仍振荡出 `(588,66)` → 真实鼠标
  命中区右偏（用户报障「点右面一点才激发」）；钉死后 win32k 矩形与 X 窗口
  原点恒等，命中零偏移。def 由 `kk/gen-kk-user32-def.py` 从内置
  user32 导出表生成（纯标准库；dlltool 不支持纯序号 forwarder，476 个序号
  导出跳过，KK/Qt/Wine 均按名字导入）。
- **堆叠 / 挂载层 `kk/kk-geometry-shim.c`**：LD_PRELOAD X11 垫片（32 位为主、
  64 位兼容新 WoW64；免系统头文件自声明 X11 类型，本机无 multilib 头文件）。
  剥夺内容层抬升（`XRaiseWindow` / `XMapRaised` / `CWStackMode`）；Depth 32
  弹窗 / 通知保持 OR 并在映射后抬到内容层之上；拒绝主框架客户面被 detach
  到 1×1 dummy（保持原生 UI 镜像可见）；主框架 / 内容层销毁时清空识别
  （登录→大厅切换）。识别靠创建时精确尺寸 / 深度 / OR 组合且只认
  第一个匹配者，厂商改尺寸自动退化透传，不误伤 Warcraft III 本体。
- **web 宿主左移对齐（用户报障「按键无效，真正点击有效在大黑框」）**：
  `kk-geometry-shim.c` 识别 web 宿主 X 窗（创建时 1330×884 / 1158×884，
  Depth24，取自 `+win` trace），在创建 / 移动时把它的左边缘对齐到主框架
  客户区左边缘（程序最左，实测 `x=293`），y 不变。根因是 winex11 把
  win32k 的父客户区坐标 `(172,59)` 当屏幕坐标 → 黑框被摆到 `x≈466`，
  比程序最右偏整框架原点，视觉按键落在黑框外点不动。默认开启，
  `KK_WEB_ALIGN_LEFT=0` 可回退原行为；只纠几何、不改厂商二进制，
  不做守护。此项是几何层缓解，若 web 内容面不呈现（见 §5 限制）仍在，
  属于 winex11/QtWebEngine surface 缺陷，不由本开关修。
- 两层都**不监控、不守护、不自动重启**；启动器按需编译（gcc / mingw /
  dlltool），产物落 `~/.local/share/kkduizhan-wine11/` 与前缀 syswow64，不入库。

**验收**（2026-11-14 夜 + 11-15/16 复验，合成画面 + XSendEvent 合成点击）：
内容层 `(294,33)` = 主框架客户区原点且不再振荡；登录窗 / 模态 / 活动弹窗几何
全部正确；模态「确定」、登录页「密码登录」tab（Web 内容点击）、活动弹窗 `X`
（含大活动弹窗独立的 41×41 关闭按钮窗）均点击生效、遮罩可解除；登录页 Web
渲染完整。侧栏选中已可用（命中零偏移，见自动化验收）；**但 web 内容面仍不呈现**（游戏列表空白）：见下方限制中的既有缺陷。

### 自动化验收（`kk/ui-autotest.py` + `tests/test-kk-ui.sh`）

用户要求「核心是你要能自动测试」。注入与断言通道：

- **真实指针注入**：`org.gnome.Mutter.RemoteDesktop`（compositor 级 →
  wl_pointer → XWayland → Wine）。绝对坐标注入需预注册 screencast 会话
  （"No screen cast active"），故用**相对位移**（查 X 指针位置→差量）；
  XTEST / XSendEvent 均不走真实命中路径，不用。
- **画面断言**：同一条 Mutter ScreenCast 流抓帧（嵌入式光标 + 指针微动保证
  有帧）；断言区域掩掉光标像素。侧栏行用**图标颜色定位**（魔兽争霸3 黄 W /
  DotA 红 D），不依赖固定行偏移（导航存在与否 / 展开子行 / 窗口最大化都会改偏移）。
- **强制断言**：活动弹窗 X 真实点击关窗；悬停魔兽争霸3 行 UI 变化；真实点击
  视觉中心即选中（命中零偏移，未中则扫描偏移并判 FAIL）。选中高亮画在主框架
  客户面、会被其它窗口遮挡，故辅以应用日志反应（`WndWar3RPGLobby|url->|context create`）。
- **降级**：DotA 导航闭环受遮挡/日志口径影响记 WARN 不判 FAIL；环境不具备
  （无 Wayland / 未装 KK / 缺依赖）退出 77，`tests/test-kk-ui.sh` 转 SKIP。
- 运行：`python3 kk/ui-autotest.py`（或 `bash tests/test-kk-ui.sh`）；单次启动、
  显式收尾（自启的 KK 用 wineserver -k 收），不监控不守护。
- **退出机制验收**（`tests/test-kk-shim-exit.sh` + `tests/fixtures/kk-shim-exit-test.c`）：
  LD_PRELOAD 垫片到独立 X11 客户端按特征尺寸建窗，确定性断言内容层随主框架
  隐藏/恢复、非退出路径不误杀、退出路径 watchdog ~5s 推导同目录 wineserver -k。
  不依赖 KK 安装 / Wayland 会话，仅需 gcc + X11 dev + DISPLAY。
- **注入通道退化史料（2026-11-17）**：Mutter RemoteDesktop 虚拟设备事件在 KK
  模态弹窗开着时会被吞（指针/键盘均无反应，真实鼠标不受影响），多次会话后
  甚至常驻退化。退出链路验收改走不依赖 RD 的通道：托盘图标窗 XSendEvent
  右键 → 菜单窗 XSendEvent 点退出项 → 挽留弹窗 XSendEvent 点「下次再说」
  （合成按钮事件直达窗口，绕过 grab；标题栏 X 在 Wine 下点击死，分层命中缺陷）。

### 魔兽争霸3 漂移：活动弹窗关闭按钮窗（2026-11-16 夜定案）

用户报障「魔兽争霸3 漂移」，怀疑与已修 UI 漂移同源——确同族。取证链：

1. 活动弹窗（920×560 寻宝 / 440×360 奖励）的关闭 X 由**独立按钮窗**承载
   （`Qt5152QWindowToolSaveBits`，41×41 / 94×60 两变体）。其 win32k 矩形落在
   弹窗中心附近（创建 (913,493)、ULW (912,461)，KK 坐标混算），而视觉 X 在
   弹窗右上角 → 点视觉 X 无反应（真实点击扫描 11 个偏移均不关窗），点弹窗
   中心反而关窗（命中区与视觉错开的直接实证）。
2. X 点不动 → 模态遮罩解除不了 → 魔兽3 侧栏 / 导航点击全被遮罩吞掉
   （`+event` 取证 ButtonPress 落到内容层、应用日志零反应）——用户感知的
   「漂移 / 点不动」。
3. 并发活锁：94×60 按钮窗 Qt 只 ULW 60×60 surface（pptDst=NULL → Wine 居中，
   bounds (17,0)-(77,60) ≠ 窗口）→ winex11 Expose→重绘→surface 重建循环；
   叠加服务端寻宝 web 内容在 renderer=gl 路径忙等 → 主线程 ~100% CPU、
   登录/大厅冻死（基线无 wrapper/shim 同样复现，非本仓库层引入）。

**修复**（双层同策，仅 KK 进程树）：

- `kk-user32-wrapper.c`：按钮窗自身 ULW 触发时登记 hwnd（其在 win32k 树里父窗
  不定，顶层/子窗枚举均不可靠）；**94×60 登记即缩到 60×60**（bounds≠窗口的
  Expose 活锁在登录期就空转，缩尺寸不依赖弹窗）；借弹窗本体 ULW 时机
  `SetWindowPos` 钉死到弹窗右上角（41×41 锚点 `(W-42,0)`；60×60 按 glyph 中心
  `(W-55,-6)`）；弹窗未出现不做锚点钉死。`KK_WRAP_DEBUG=1` 打印判读决策。
- `kk-geometry-shim.c`：按钮窗（41×41/94×60/60×60 OR Depth32）跟踪并随弹窗
  抬升序列最后抬（高于内容层/通知）；通知 315×145 纳入弹窗跟踪（h 阈值
  150→100）。**detach 拒绝收窄到主框架客户面（1332×945）**：web 子窗
  （1156×882）的 detach/re-attach 放行，避免阻 vulkan 路径 web surface 重挂。
- `run-kk-platform.sh`：`renderer=gl` → `renderer=vulkan`（见 §3）。

**验证状态**：响应式会话里视觉 X 真实点击关窗多轮复现；用户实鼠确认**点击黑框
即选中真实魔兽争霸3**（命中漂移已修）。**黑框本身=渲染堆叠限制**：web 渲染窗是
嵌套 X 子窗，内容层是顶层 OR 窗恒压其上，仅左侧 ~125px 透明缝露出 web；抬升
子窗无法越过顶层 OR（需 reparent 到 root，属 Wine 层 win32u/winex11 缺陷，与
大厅 offscreen 锁存同源），脚本层不强行 reparent。**2026-11-17 进一步证伪**：
web 宿主 win32k 尺寸实为满幅 1158×884，位置 (172,59) 系父客户区坐标被当屏幕
（漂移同源）；活体移 web X 窗到 (466,92)、 lowered 内容层、map+抬顶 web 窗，
绘制区仍只 ~125px 宽 → 黑框是 winex11/QtWebEngine surface 绘制宽度缺陷，非几何/
堆叠，脚本层无解。**硬件/Wine 层评估（11-17）**：gf ICD 仅 64 位
（`gf_icd.x86_64.json`→x86_64 `libvulkan_gf.so`），32 位目录无 gf 库；Proton 11
新 WoW64 unix 侧虽 64 位，但纯硬件 gf 启动在 QtWebEngine 初始化即崩（原始 rc=5
缺陷，`hw-test.log`）→ 硬件路径不可行。web 位置由 `NtUserSetParent`(syscall) 设置，
user32 层 SetWindowPos/MoveWindow hook 截不到（KK 仅 SWP_NOMOVE）→ 坐标换算修不了
黑框。真正修复需改 Wine win32u/winex11 源码（重编 Wine）或厂商补 gf 32 位/修
Vulkan 初始化，超出脚本层。**随从大师交叉参照（11-17）**：其同矛盾（32 位+仅 64
位 gf）的硬件解法 = new WoW64 + gf-compat LD_PRELOAD（os-release 绕过+低4GB重映射）
+ 垫片 ICD + DXVK v2.3 + libSC 软链，但**必须经 Steam Runtime 容器**启动才有
`VK_KHR_surface`；KK 直跑 wine/proton run 报 `VK_KHR_surface not supported`→DXVK
建不了 device。故 KK 要用硬件需改为经 Steam（非 Steam 游戏+ gf-compat LaunchOptions）
启动，属启动方式重构，待用户确认。魔兽3 侧栏选中闭环待新登录页
问题解除后复验。`ui-autotest.py` 已适配两弹窗尺寸、War3 行搜索限上半段、登录
守卫退 77。

**已证伪 / 未执行杠杆（勿再照抄）**：扩拷贝规范化 psize=94×60/pptSrc=(0,0)
（源 DC 仅 60×60，越界拷贝触发 GraphicsExpose 风暴）；登录期强制 XMapWindow
按钮、ULWI 空 pptDst 路径补钉（扰 Qt 初始化；加载卡死主因后经裸 Wine 对照
证明是环境，仍保留保守触发）；`--host-resolver-rules` 屏蔽活动域名（与登录
同域 `kkdzpt.com`，风险大未执行）；`KK_WEBENGINE_SOFTWARE=1` 对 GL 活锁无效。

**未修复（vendor/Wine 层，11-16 夜裸 Wine 对照定案）**：服务端新登录页
（1095×558）在软件渲染（lvp GL/vulkan）下不完成自动登录——无 wrapper/shim
同样卡；登录表单已渲染、可手动登录。与大厅 web offscreen 锁存同类，需硬件
渲染路径评估或厂商修复。证据：`~/.local/share/kkduizhan-wine11/drift-trace.log`
（多会话追加，按 `=== 2026` 分段）、`orig-trace.log`、`plain-vk.log`、`vk-test.log`。

### 退出挽留弹窗「下次再说」（2026-11-17）

点退出弹出的 440×360 挽留弹窗是**受管 d24 or=0** 窗，被顶层 OR 内容层恒压其下
（mutter 受管窗恒排 OR 之下、忽略客户端 raise），「下次再说」/「立即前往」点击全落
内容层。与关闭按钮漂移同族、不同变体。`kk-geometry-shim.c` 按尺寸 440×360 跟踪，
映射时强制 `override_redirect=1` 并随抬升序列置于内容层之上。**不含 920×560**
（登录窗，强制 OR 扰手动登录；寻宝 920×560 本就 OR d32 走原有跟踪）。合成点击不
复现该弹窗（需实鼠点退出），待实鼠复验。

### 退出残框与退出卡死兜底（2026-11-17）

**现象**：点退出后 KK 完成 logout、隐藏主框架，但进程不死（取证：主线程
futex 等待、web renderer/gpu 仍活、wineserver ~18% CPU）；顶层 OR 内容层不随
父隐藏同步 unmap → 桌面留一个空框。该退出路径此前被挽留弹窗不可点挡住，
11-17 弹窗修复后首次暴露。

**修复**（`kk-geometry-shim.c` C 项）：
- 主框架 unmap 时同步 unmap 内容层 / 弹窗 / 按钮，remap 时同步恢复并重建
  堆叠（最小化 / 恢复同受益）；
- 以挽留弹窗（440×360）销毁为退出意图信号：5s 内主框架隐藏、再 5s 进程仍
  活 → 一次性 watchdog 线程从 `/proc/self/exe` 同目录推导 wineserver 跑 `-k`
  清整前缀；主框架恢复映射即清零放弃。仅退出路径触发，非常驻守护。

**限制**：退出卡死根因是 Wine/CEF shutdown 死锁，脚本层只做清理兜底；厂商改
挽留弹窗尺寸时意图信号退化失效（残框视觉修复仍生效）。机制验收见
`tests/test-kk-shim-exit.sh`（8 断言全绿，2026-11-17）。

### 已证伪 / 已撤销（排查史料，勿再照抄）

- **`win32u.dll` PE 包装层（2026-11-20 实验，默认关闭）**：尝试用 native
  `win32u.dll` + `WINEDLLOVERRIDES=win32u=n,b` 拦 `NtUserSetParent`，把
  `1158x884` 的 CEF web 子窗从 `(172,59)` 平移父框架原点 `(+294,+33)`。
  结果：包装 DLL 被加载、导出生成正常（1541 个），但 hook 从未命中
  （`KK_WIN32U_DEBUG` 无日志）——Wine 的 user32 经内部 syscall dispatcher
  调 win32u，不走 PE 导出；而且仅覆盖该文件就让主框架掉到 `(0,0)`、内容层
  消失。`run-kk-platform-hw.sh` 的 `KK_WIN32U_WRAPPER` 默认 `0`，源码与生成
  脚本保留仅为 A/B 记录，**不能作为修复**。下一步只能走 Unix 侧 syscall/
  驱动层拦截（LD_PRELOAD `winex11` 或改专用 Proton 二进制），需单独评估。
- `XLIB_SKIP_ARGB_VISUALS=1`：窗口不透明，但游戏列表 / 商城导航整片空白。
- 一次性 / 多次 `xdotool windowmove` 对齐内容层：只修视觉不修输入（win32k
  矩形不变，点击偏移整个框架原点），且应用事件会把内容层重置回 `(0,0)`。
- 外部移主框架到 `(0,0)` / `(-1,-1)` + OR 化 + `Managed=N` + serial 错开：
  Wine 两条路径都不把 X 几何回写 win32k，父子矩形修不了（本轮 trace 定案）。
- Wine 虚拟桌面 `explorer /desktop=KK,1920x1080` 与 `1332x945`：前者主框架被
  居中 / 被 mutter 钳制依旧错开；后者应用仍按真实屏幕尺寸居中主框架，桌面内
  父子矩形再次错开。
- `KK_WEBENGINE_SOFTWARE` 的软件绘制 / DPR=1 flags、
  `QT_ANGLE_PLATFORM=swiftshader`、Steam Runtime 容器、`LIBGL_DRI3_DISABLE=1`：
  对错位 / 遮罩均无收益或不稳定；对大厅 web 不呈现同样无效（11-16 复验）。
  `KK_WEBENGINE_SOFTWARE=1` 仅保留为 A/B 诊断开关。
- 大厅 web 不呈现的清除尝试（11-16，均无效）：客户面 dummy→frame 往返重挂、
  同父微移触发 ConfigureNotify、WM 移动主框架 1px / resize 1px 触发 win32k
  位置变更、`KK_WEBENGINE_SOFTWARE=1`、`QTWEBENGINE_DISABLE_SANDBOX=1` +
  `--no-sandbox --disable-gpu-sandbox --in-process-gpu`（后者启动后稍晚崩溃）、
  包装层保持 Chrome_WidgetWin_* 顶层（启动即 rc=5 崩溃）、包装层把 web 链
  SetParent 改挂内容层（无收益）。offscreen 锁存仍在（trace 持续
  `client_surface_update_offscreen ... offscreen 1`）；侧栏镜像点击零应用日志
  反应，确认交互面只在死掉的 web surface 内。
- `_NET_WM_WINDOW_OPACITY`：曾准备给内容层强制不透明，命令被用户中止、
  **未执行**；既然遮罩不是 bug，无需再做这类透明层干预。
- 早期 X11 抓图全黑曾被误判为「合成层脱节」：`xwd` / `ffmpeg x11grab` /
  PIL `ImageGrab` 连 1920×1080 全屏都纯黑，是 XWayland 下 `XGetImage` 拿不到
  合成内容的全局限制；CDP（`QtWebEngine/5.15.2 Chrome/83`）证实 Web 层渲染与
  DPR（`devicePixelRatio=1`）都正常。
- XTEST 合成点击（`xdotool click`）在本机 XWayland 会话不投递 ButtonPress 给
  X 客户端（只有 motion 生效）；点击验收改用 `XSendEvent` 合成事件
  （`_work/xsend-click.py`，不入库）或真实鼠标。
- `wmctrl` 与 `xwininfo` 坐标差 2 倍是本机所有应用共有的 XWayland 全局映射，
  不是 KK 独有。宿主泄漏的 `QT_ACCESSIBILITY=1` / `QT_IM_MODULE=fcitx` 仍被
  启动器清掉（无害保留）。

### 限制

- **未修复（既有缺陷，基线同样存在，11-16 二分定案）**：大厅的 QtWebEngine
  交互面（侧栏 / 导航 / 游戏列表都在其中）从不呈现：web 子窗口 win32k 矩形
  卡在屏外 `(880,502)-(2210,1386)`，主框架客户面被持续锁存 offscreen，渲染
  被转投到 offscreen pixmap；可见的侧栏 / 导航只是原生非交互镜像。因此
  **选游戏 / 切页签仍不可用**，与遮罩 X 修复无关；登录页 web 面正常（其客户面
  未经历 offscreen 锁存）。需 Wine 层修复（win32u 子窗口 SetParent/ULW 矩形
  语义 + winex11 surface offscreen 锁存），脚本层无解；证据见
  `~/.local/share/kkduizhan-wine11/trace-web.log` / `bisect-*.log`。
- 房间匹配 / Warcraft III 本体对战未验收；不对该应用加自动重启 / 重试 / 守护
  （见 `.codebuddy/rules/崩溃处理约定.md`）。
- **新登录页自动登录在软件渲染下不完成**（11-16 夜，裸 Wine 对照同样卡）：
  表单可手动登录；登录后大厅/弹窗链路按上方漂移修复生效。需硬件渲染路径
  评估或厂商修复，脚本层无解。
- 包装层 / 垫片的窗口识别绑定厂商当前尺寸（1332×945 / 1332×946 / Depth 组合），
  厂商改尺寸时自动退化透传（回到未修复行为），需按新尺寸更新识别常量。
- 内容层 / 弹窗的 X 窗口 ID 会在运行中重建（视觉切换 / ULW recreate），
  取证与合成点击必须实时查询窗口 ID，不得缓存旧 ID（垫片靠 XDestroyWindow
  清空识别自动适应）。
- `KK_GEOMETRY_SHIM=0` 回到未修复原始行为；syswow64 里的包装层 dll 在没有
  `user32=n,b` override 时不被加载（Wine 默认内置优先），无副作用。

## 6. 已知噪音与限制

- 安装/启动日志里 `failed to get driver name ... 4c54:5006, driver (null)` /
  `No driver found` 是 wine 探测 GPU 的既有噪音，与安装和运行结果无关，
  不需要改 drirc 或注入 gf 环境。
- 应用日志里的 `qt.qpa.fonts: Unable to enumerate family ...`、
  `WSALookupServiceBeginW Stub!`、`Y3SharedLib: Not a directory`、
  `GetCurrentPackageId stub` 均为非阻塞噪音；QtWebEngine 已能正常渲染。
- **未验收**：账号登录交互、房间/匹配、Warcraft III 本体启动与对战
  （本次只验收客户端界面渲染与网络响应）。如需继续，按同样「单次启动 + 显式
  收尾」方式取证，不得加看门狗/自动重启（见
  `.codebuddy/rules/崩溃处理约定.md`）。
- 卸载客户端用安装目录自带 `Uninstall.exe /S`（未实测）；桌面集成用
  `fix-kk-desktop.sh --uninstall` 撤销，脚本不删除 Wine 前缀。
- 本主题未接入 `install.py` 菜单；deb 打包脚本与文档（不含厂商 exe 与图标）。
