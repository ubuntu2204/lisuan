# KK 官方对战平台（魔兽争霸 III）在砺算 7G100 上的安装与运行

> 至砺算：安装链路与显卡无关；运行期的崩溃也不是 gf 驱动缺陷，而是 QtWebEngine
> 依赖的 wined3d / Vulkan 后端在 Wine 11 下初始化失败。给目标进程显式指定
> Mesa llvmpipe Vulkan ICD 与 wined3d 的 GL 渲染器即可稳定显示界面。若官方
> 让 32 位 WoW64 场景下的 Vulkan/GL 设备发现完整可用，可再评估硬件渲染路径。

## 1. 厂商专有二进制（不入库）

| 项 | 内容 |
| --- | --- |
| **是什么** | `4_pf_files_KKDuiZhanDownloader-v1.0.2.454.exe`：KK 官方对战平台的 NSIS v3.06.1 PE32 下载器安装器（7.9 MB，`requireAdministrator`）。`/S` 静默安装后落地 `C:\Program Files (x86)\kkduizhan\`（本机实测 1.1 GB / 539 个文件），含 `Platform.exe`（48 MB 主程序）、`PlatformDownloader.exe`、`KKEmulator.exe`、`Uninstall.exe` 以及 Warcraft III 1.28/1.29 补丁文件。 |
| **为何不能入库** | 厂商专有二进制、升级会改文件名；按 `.codebuddy/rules/仓库组织约定.md` 二，用 `.gitignore` 规则 `war3/4_pf_files_KKDuiZhanDownloader-*.exe` 排除，仓库只保留脚本与文档。文件仍留在本目录供运行时使用，不删除。生成的图标写入 `~/.local/share/icons/hicolor/`，同样不入库。 |
| **从哪获取** | KK 官方对战平台官网/官方分发页下载 `4_pf_files_KKDuiZhanDownloader-v<版本>.exe`，放到本目录、`~/下载` 或 `~/Downloads`，脚本会自动查找。本机这份 SHA256：`418354d8023686a8d1580c18499fc380b471980eb471f59c33ce6d7566875659`。安装产物在 `~/.deepinwine/kkduizhan-wine11/drive_c/Program Files (x86)/kkduizhan/`，同样不入库。 |

## 2. 安装（2026-11-14 实测）

```bash
cd ~/project/lisuan
./war3/install-kk-platform.sh                 # 自动找安装器，NSIS /S 静默安装
./war3/install-kk-platform.sh --status       # 只读体检
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

**现行启动器**：`war3/run-kk-platform.sh`，在应用进程树内设置
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
./war3/fix-kk-desktop.sh                 # 提取图标 + 写入口（幂等）
./war3/fix-kk-desktop.sh --status       # 只读体检
./war3/fix-kk-desktop.sh --uninstall   # 恢复备份、删除图标与新建入口
```

- `war3/extract-icon.py`（与 `dreamina/extract-icon.py` 同源，纯标准库解析 PE
  `RT_ICON` / `RT_GROUP_ICON`，不依赖 icoutils / Pillow）从本机 `Platform.exe`
  提取官方图标，写入 `~/.local/share/icons/hicolor/{16..256}.../apps/kkduizhan.png`
  共 10 个尺寸，最大 256×256；厂商图标产物不入库。
- 桌面项写入 `~/.local/share/applications/kkduizhan.desktop`、XDG 桌面
  (`~/桌面/`) 与历史 `~/Desktop/`，统一 `Icon=kkduizhan`、
  `Exec="<仓库>/war3/run-kk-platform.sh"`、`Categories=Game;`，并打
  `metadata::trusted=true`。只改 `Icon` / `Exec` 两行、保留用户其它定制；
  改文件前在 `~/.local/share/kkduizhan-wine11/desktop-backups/` 生成 `.bak`。
- 实测 `--status` 全项 `[OK]`，`desktop-file-validate` 无警告；`--uninstall`
  从备份还原（新建的直接删除）并移除生成的图标。

## 5. 已知噪音与限制

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
