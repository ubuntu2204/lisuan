# 即梦 AI（Dreamina）Windows 桌面客户端在 Ubuntu 24.04 + Wine 11 下的安装记录

> 至砺算：官方客户端是 64 位 Electron/Chromium，Wine 11 下 ANGLE 要求
> `WGL_NV_DX_interop2`，必须走 desktop GL 软件渲染；安装链路与桌面图标问题
> 本身与 7G100 驱动无关，不需要注入 gf 环境、不改驱动、不做崩溃后自动拉起。

安装目标版本：**即梦 AI 1.0.59（Electron 37.10.3 / Chromium 138）**
安装器：`dreamina/Dreamina-Desktop-1.0.59-x64-setup.exe`（NSIS v3.04，electron-builder 产物，
`asInvoker`，不需要管理员权限；**不入库**，见 `.gitignore`）

更新时间：2026-11-09

平台：Ubuntu 24.04、GNOME(Wayland)、砺算 7G100、Steam Runtime sniper 容器 + Proton 11.0 的 wine-11.0

## 1. 结论先行

**安装与桌面集成都已跑通**，安装物完整落在独立前缀里：

| 项 | 实测值 |
|---|---|
| Wine 前缀 | `~/.deepinwine/dreamina-wine11`（Windows 用户名 `steamuser`） |
| 安装目录 | `C:\users\steamuser\AppData\Local\Programs\dreamina-desktop` |
| 主程序 | `即梦 AI.exe`（195 MB 的 Electron 壳，Chromium 138 / Electron 37.10.3） |
| 应用包 | `resources/app.asar`（1.7 MB，Electron + React，主进程 `out/main/index.js`） |
| 资源目录 | `resources/login-carousel/`（登录轮播图，本地资源） |
| 卸载器 | `Uninstall 即梦 AI.exe` |
| Windows 快捷方式 | `C:\users\steamuser\Desktop\即梦 AI.lnk`（安装器自动生成） |
| 桌面图标 | `~/.local/share/icons/hicolor/*/apps/dreamina.png`（官方 exe 提取，16–256 共 7 个尺寸） |
| Linux 桌面入口 | XDG 桌面目录（本机 `~/桌面/`）、`~/.local/share/applications/`，历史 `~/Desktop/` 同步修正 |

启动入口：`./dreamina/run-dreamina.sh`（桌面入口模板 `dreamina/dreamina-wine.desktop`）。

**本项目已把它接入统一菜单**（`install.py`）：

| 菜单编号 | 作用 |
|---|---|
| **28** | 安装/修复：即梦 AI Windows 客户端（Wine 11 + NSIS 静默安装 + 图标/入口）→ 调 `dreamina/install-dreamina.sh` |
| **29** | 说明：即梦 AI 安装与渲染记录（本文件） |

即 `python3 install.py 28` 即可完成一遍安装；官方安装器需自行从官网下载后放进
`dreamina/` 目录（或 `~/下载`、`~/Downloads`），脚本按文件名里的版本号自动选最高
（`.exe` 不入库，见 `.gitignore`）。菜单 28 的状态还会检查图标/桌面入口是否真的生效。

只修图标/入口、不重装 100 MB 客户端时用：

```bash
./dreamina/install-dreamina.sh --desktop-only   # 修「没有图标」
./dreamina/install-dreamina.sh --status          # 只读体检
./dreamina/install-dreamina.sh --uninstall       # 撤销桌面集成，保留前缀与账号数据
```

**安装过程中唯一真正的拦路虎是 Wine 的 powershell 空壳 stub**，详见第 2 节；
**桌面没有图标**是另一个独立问题，详见第 1.1 节。

## 1.1 桌面没有图标：根因与修复（2026-11-09）

现象：客户端能正常启动并显示登录界面，但桌面/应用列表里没有即梦图标。

取证到的三个叠加原因：

1. **模板抄错图标**：`dreamina/dreamina-wine.desktop` 的 `Icon=` 指向
   `/opt/apps/doubao-wine/.../doubao-wine.png`（豆包图标），且从未安装过即梦自己的图标。
2. **入口放错目录**：历史手工入口只在 `~/Desktop/`，而 GNOME DING 监听的是
   `xdg-user-dir DESKTOP`（本机 `~/桌面/`）；`~/.local/share/applications/` 也没有入口。
3. **入口不可信**：`gio info` 里没有 `metadata::trusted: true`，DING 不会按应用启动器显示。

修复由 `dreamina/extract-icon.py` + `install-dreamina.sh` 完成：

- 纯标准库解析官方 PE 的 `RT_GROUP_ICON` / `RT_ICON`，取出 16/24/32/48/64/128/256
  共 7 个 PNG 帧，写入 `~/.local/share/icons/hicolor/<尺寸>/apps/dreamina.png`；
  兼容 BMP/DIB 图标，PE 资源缺失时兜底扫描内嵌 PNG。不需要 `icoutils` / `pefile` / `Pillow`。
- 图标**运行时从用户本机的官方 exe 提取**，不把厂商图标资源提交进仓库。
- 桌面项统一写 `Icon=dreamina`、动态 `Exec="<仓库>/run-dreamina.sh" %F`，去掉已废弃的
  `Encoding=`、清掉残留的豆包图标路径，设 `metadata::trusted=true` 并刷新桌面数据库。
- 同时写 XDG 桌面目录、`~/.local/share/applications/`；`~/Desktop` 存在且不是 XDG 桌面目录时
  连历史入口一起修。原文件备份在 `~/.local/share/dreamina-wine11/desktop-backups/`，
  `--uninstall` 可还原。

本机实测：`--desktop-only` 后 `--status` 全项 `[OK]`，三个 `.desktop` 都通过
`desktop-file-validate` 且 `trusted=true`；`256x256` 图标是官方蓝/橙「即梦」图形，
不是豆包图标或空白占位图。`install.py` 菜单 28 的状态也由「桌面图标缺失」变为「✓ 已安装」。


## 2. ★ 最大的坑：Wine 的 powershell 是空壳，导致 electron-builder 安装器直接 Abort

### 2.1 现象

安装器是 NSIS，按惯例直接 `/S` 静默安装，结果：

```
$ wine Dreamina-Desktop-1.0.59-x64-setup.exe /S
wineserver: using server-side synchronization.
$ echo $?
2
```

返回码 **2**，而且：

- `$INSTDIR`（`…/Programs/dreamina-desktop`）**存在但完全是空的**；
- `%TEMP%` 里什么都没留下；
- 前缀里搜不到任何 dreamina 相关文件；
- 不带 `/S` 直接跑 GUI 也一样：进程起来几秒就退出，**连安装窗口都没出现**。

### 2.2 定位过程

`WINEDEBUG=+file` 能看到安装器确实建了 NSIS 的 `$PLUGINSDIR`
（`C:\users\steamuser\AppData\Local\Temp\nswd9f0.tmp`），但解包还没开始就退出了。
加上 `WINEDEBUG=err+all` 后，日志里反复出现：

```
fixme:powershell:wmain argv[1] L"-C"
fixme:powershell:wmain argv[2] L"if (Get-Command Get-CimInstance -ErrorAction SilentlyContinue) { exit 0 } else { exit 1 }"
fixme:powershell:wmain stub.
fixme:powershell:wmain argv[2] L"Get-CimInstance -ClassName Win32_Process | ? {...} | % { Stop-Process -Id $_.ProcessId  }"
fixme:powershell:wmain argv[2] L"if ((Get-CimInstance -ClassName Win32_Process | ? {...}).Count -gt 0) { exit 0 } else { exit 1 }"
fixme:powershell:wmain argv[2] L"Get-CimInstance -ClassName Win32_Process | ? {...} | % { Stop-Process -Id $_.ProcessId -Force }"
```

这 4 条正是 **electron-builder 的 NSIS 模板「检查旧版本进程是否还在跑」**的标准动作：

1. `if (Get-Command Get-CimInstance …) { exit 0 } else { exit 1 }` —— 探测有没有 CIM
2. `Get-CimInstance … | % { Stop-Process … }` —— 尝试停掉旧进程
3. `if ((Get-CimInstance …).Count -gt 0) { exit 0 } else { exit 1 }` —— **判断是否还有进程在跑**
4. `Get-CimInstance … | % { Stop-Process … -Force }` —— 强杀

### 2.3 根因

**Wine 自带的 `powershell.exe` 是空壳**：`fixme:powershell:wmain stub.` —— 它的 `wmain`
什么都不做，**无条件 exit 0**。

实测确认：

```
$ wine cmd /c 'powershell -Command "exit 1"'          # 期望 1，实际 0
$ wine cmd /c 'powershell -Command "if (Get-Command Get-CimInstance …) { exit 0 } else { exit 1 }"'
$ echo $?    # 0   <- 恒为 0
```

于是第 3 步「有没有进程在跑」被解读成 **exit 0 = 有进程在跑**（明明一个进程都没有），
安装器转去走「应用正在运行，是否强制关闭」的分支，再弹框要人点 —— 静默模式下直接
`Abort`，所以返回码 2、什么都没装。

> 顺带一个测试上的坑：想用 `cmd /c '… & echo RC=%ERRORLEVEL%'` 验证退出码是**无效的**，
> cmd 会在执行整条命令之前就展开 `%ERRORLEVEL%`。要直接看 wine 进程的返回码。

### 2.4 解决：换掉 powershell stub

`dreamina/powershell-stub.c`：一个 32 位 PE，逻辑只有一句 ——
**命令行里只要出现 `Get-CimInstance` 就返回 1**（= 没有进程在跑），其余调用仍返回 0
（与 wine 原 stub 行为一致，不影响安装器后续用 powershell 做别的事）。

```bash
i686-w64-mingw32-gcc -O2 -m32 -o powershell.exe dreamina/powershell-stub.c
# 装到两个位置（32 位 NSIS 与 64 位宿主都可能走到）：
cp powershell.exe "$PREFIX/drive_c/windows/system32/WindowsPowerShell/v1.0/powershell.exe"
cp powershell.exe "$PREFIX/drive_c/windows/syswow64/WindowsPowerShell/v1.0/powershell.exe"
```

（原件备份为 `powershell.exe.wine-stub`，随时可回退。）

装上替身后同一个安装器：

```
$ wine Dreamina-Desktop-1.0.59-x64-setup.exe /S
$ echo $?
0
```

安装目录立刻有内容 —— **安装成功**。

`dreamina/install-dreamina.sh` 已经把这一步固化进去了（第 2/4 步「替换 powershell stub」），
以后升级版本直接重跑脚本即可。

## 3. 启动

```bash
./dreamina/run-dreamina.sh          # 默认走 Steam Runtime 容器
DREAMINA_NO_STEAM_RUNTIME=1 ./dreamina/run-dreamina.sh   # 裸 wine（诊断用）
```

启动日志：`~/.local/share/dreamina-wine11/launch.log`
（可用 `DREAMINA_WINE11_LOG` 覆盖）。

默认渲染开关（2026-11-08 实测，理由见第 4 节）：

| 开关 | 作用 |
|---|---|
| `--use-gl=desktop` | **最关键的一个**。绕开 ANGLE，改走 Wine 的 `opengl32` → mesa llvmpipe，详见第 4 节 |
| `--ignore-gpu-blocklist` | 允许在软件渲染环境下继续用 GL |
| `--enable-unsafe-swiftshader` | Chromium 137+ 默认禁用 SwiftShader 光栅化 |
| `--no-sandbox --disable-gpu-sandbox` | Wine 下 Chromium 沙箱起不来 |

额外参数可在命令行末尾追加覆盖默认，例如：

```bash
./dreamina/run-dreamina.sh --disable-features=Vulkan
```

## 4. 渲染：黑屏的根因与解法（已解决）

### 4.1 现象

窗口能正常建出来，但**内容区整块全黑**（Electron 窗口只剩默认底色），
`--use-gl=angle --use-angle=gl` 时连窗口都直接消失（`IsUnMapped`，屏幕上什么都没有）。

### 4.2 根因：ANGLE 在 Wine 下要求 `WGL_NV_DX_interop2`

Electron 默认走 ANGLE。Wine 下 ANGLE 的 GL 后端初始化时报：

```
ANGLE Display::initialize error 12289: WGL_NV_DX_interop2 is required but not present
Exiting GPU process due to errors during initialization
```

`WGL_NV_DX_interop2` 是 NVIDIA 私有扩展，Wine 的 `wined3d` 不提供 →
ANGLE 初始化失败 → **GPU 进程直接退出** → 没有 GPU 进程就没有合成器，
窗口要么全黑、要么停在 `IsUnMapped`。

这也解释了早先那些「GPU 进程反复 `exit_code=-2147483645`（0x80000003）」的现象。

### 4.3 解法：改用 desktop GL

```bash
--use-gl=desktop
```

绕开 ANGLE，直接走 Wine 的 `opengl32` → mesa llvmpipe 软件渲染，
GPU 进程**零崩溃**，界面正常绘制。

最终默认参数（已固化进 `dreamina/run-dreamina.sh`）：

```
--use-gl=desktop --ignore-gpu-blocklist --enable-unsafe-swiftshader --no-sandbox --disable-gpu-sandbox
```

### 4.4 实测结果（2026-11-08，默认参数复验）

| 项 | 实测值 |
|---|---|
| 主窗口 | `0xd800007 "即梦 AI" 1280x800+320+123`，`map_state=IsViewable` |
| 登录子窗口 | `0xd800009 "即梦AI - 注册登录" 620x700+650+173` |
| 抓图颜色数 | **101467**（对比黑屏时的 1～375 色） |
| GPU 进程 | 存活，崩溃计数 0 |

界面完整可见：左侧「即梦 Seedream 5.0 Pro 支持智能改图」轮播图、
右侧「欢迎登录即梦」手机号 / 验证码输入框、登录按钮、抖音与即梦第三方登录图标、
底部用户协议勾选框、顶部菜单栏（文件 / 编辑 / 帮助）。

![即梦 AI 登录界面渲染成功](screenshot-login.png)

### 4.5 参数对照（失败 vs 成功）

| 参数组合 | GPU 崩溃 | 结果 |
|---|---|---|
| `--use-gl=angle --use-angle=gl` | 初始化即退出 | 窗口消失，屏幕上什么都没有 |
| `--use-angle=opengl` | 退出 | 全黑 |
| 纯 `--enable-unsafe-swiftshader`（不加 `--use-gl`） | 3 次 | 全黑 |
| `--use-gl=desktop`（其余同） | **0** | **完整渲染** |

### 4.6 定位过程中排除的假设

| 假设 | 验证方式 | 结果 |
|---|---|---|
| 网络不通 | 宿主 `curl https://jimeng.jianying.com/ai-tool/home` → HTTP 200 / 1.1s；Wine 内 DNS 也能解析出 `120.201.104.152` | 排除 |
| 页面是本地资源加载失败 | `app.asar` 里 `out/renderer/index.html` 只是壳，真正首屏是远程 `loadURL(DREAMINA_HOME_URL)` → `https://jimeng.jianying.com/ai-tool/home` | 首屏依赖网络 |
| renderer 报 JS 错 | `--enable-logging=stderr --v=1` 全量日志里没有任何 `CONSOLE` / `Uncaught` | 排除 |
| 单纯加载慢 | 20s → 140s 每 15s 抓一次，颜色数恒定 375（= 标题栏+灰底），不随时间增长 | 排除 |

**判定方法学上的坑**：用 `xwd -id <窗口>` 抓图统计颜色数来判「有没有画面」并不可靠 ——
同一组参数在不同轮次分别得到 1 / 125 / 375 色，且 Wayland 下 `xwd -root` 直接
`BadMatch`、`org.gnome.Shell.Screenshot` 报 `AccessDenied`。
本记录里的「375 色」只代表**窗口装饰画出来了、内容区还是空白**，不能据此认为已可用。
可靠的判据是**「GPU 进程崩溃次数」+ 抓图颜色数大幅上升 + 用户肉眼确认**三者同时成立。

后续可查的方向（未做）：

- CDP 远程调试端口 `--remote-debugging-port=9222` 能起来（`DevTools listening on
  ws://127.0.0.1:9222/...`），但 HTTP 请求在 Wine 的 socket 上一直超时，拿不到页面状态；
- 试 Chromium 138 的 `--disable-features=CalculateNativeWinOcclusion`（Wayland 下窗口遮挡判定）；
- 抓真屏：改用 `org.gnome.portal.Screenshot`（会弹授权框）而不是 `org.gnome.Shell.Screenshot`。

## 5. 方法论备注

- **取 Wine 前缀里的模块基址/查内存** 时，别用 `pgrep -f "xxx.exe" | head -1`：
  会取到 crashpad-handler。要遍历并排除带 `--type=` 的子进程。
- **`cmd /c '… & echo %ERRORLEVEL%'` 测不出子进程退出码**：
  cmd 会在执行前展开 `%ERRORLEVEL%`。直接看 wine 的返回码。
- **Wine 的 TCP 监听会真的落在宿主上**（`ss -ltnp` 能看到
  `127.0.0.1:9222 users:(("wineserver",…))`），但 HTTP 请求会超时，别指望能用它当代理。
- **electron-builder 的 NSIS 安装器在 Wine 上大概率装不上**，
  除非处理掉 powershell stub 这一个问题（见第 2 节）。

## 5.1 安装脚本健壮化（2026-11-09）

`dreamina/install-dreamina.sh` 从「一次性四步脚本」改成可体检、可回滚的安装器：

| 旧问题 | 现在的行为 |
|---|---|
| `set -uo pipefail`，中途失败仍继续 | `set -euo pipefail`；Wine 环境只经 `wine_env` 注入 wine 进程树，不污染当前 shell |
| 安装器路径写死 `1.0.59` | 命令行参数 > `dreamina/` > `~/下载` > `~/Downloads`；多版本按**文件名版本号**取最高（不依赖 mtime） |
| powershell 替身判断写反：含 `Get-CimInstance` 才替换，全新前缀反而跳过 → 安装 Abort | 用唯一标记 `DREAMINA_PS_STUB_V1` 判定「是否已就位」；兼容 11-08 的旧替身；原件只备份一次 |
| 只看安装目录存在就报「安装完成」 | 要求返回码 0 + 主程序是有效 PE + `resources/app.asar` 非空；区分超时(124)、Abort(2) 与其它失败 |
| 装到一半进程占用、前缀初始化失败都无提示 | 每次写带时间戳的日志；前缀初始化检查返回码；按 `/proc/<pid>/environ` 精确找本前缀进程，安装前一次性 `wineserver -k`（`DREAMINA_NO_STOP=1` 可拒绝关闭） |
| 无状态、无回滚、无法单独修桌面 | 新增 `--help` / `--status` / `--uninstall` / `--desktop-only`；`--uninstall` 恢复备份、移除图标与替身，但**保留 Wine 前缀和账号数据** |
| 主程序名写死在启动器里 | `run-dreamina.sh` 先按 `AppData/Local/Programs/dreamina-desktop` 找目录，再优先 `即梦 AI.exe`、回退任意非卸载器 exe |

崩溃/退出处理遵守项目约定：安装时只做一次显式关闭，不做看门狗、自动重启或守护式拉起。

## 6. 文件清单

| 文件 | 作用 |
|---|---|
| `dreamina/install-dreamina.sh` | 安装/升级 + 图标与桌面入口；`--desktop-only` / `--status` / `--uninstall`（菜单 **28**） |
| `dreamina/extract-icon.py` | 纯标准库 PE 图标提取：RT_ICON → hicolor 多尺寸 PNG，不依赖第三方库 |
| `dreamina/powershell-stub.c` | powershell 替身源码（凡见 `Get-CimInstance` 返回 1；带唯一识别标记） |
| `dreamina/run-dreamina.sh` | 启动器（Steam Runtime 容器 + Wine 11 + 默认渲染开关；按安装目录找主程序） |
| `dreamina/dreamina-wine.desktop` | 桌面入口模板：`Icon=dreamina`，Exec 由安装脚本按仓库路径动态生成 |
| `dreamina/screenshot-login.png` | 登录界面渲染成功的证据截图 |
| `dreamina/Dreamina-Desktop-*-x64-setup.exe` | 官方安装器（**不入库**，见 `.gitignore`） |
