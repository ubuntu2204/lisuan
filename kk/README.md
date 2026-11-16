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
`WINE_D3D_CONFIG=renderer=gl`，`cd` 到安装目录后 `exec` wine。两个关键项同时
设置是冗余保险：消融显示任一单独即可越过崩溃。日志落在
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

### 已证伪 / 已撤销（排查史料，勿再照抄）

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
