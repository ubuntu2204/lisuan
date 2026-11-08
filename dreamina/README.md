# 即梦 AI（Dreamina）Windows 桌面客户端在 Ubuntu 24.04 + Wine 11 下的安装记录

安装目标版本：**即梦 AI 1.0.59（Electron 37.10.3 / Chromium 138）**
安装器：`dreamina/Dreamina-Desktop-1.0.59-x64-setup.exe`（NSIS v3.04，electron-builder 产物，
`asInvoker`，不需要管理员权限；**不入库**，见 `.gitignore`）

更新时间：2026-11-08

平台：Ubuntu 24.04、GNOME(Wayland)、砺算 7G100、Steam Runtime sniper 容器 + Proton 11.0 的 wine-11.0

## 1. 结论先行

**安装已经跑通**，安装物完整落在独立前缀里：

| 项 | 实测值 |
|---|---|
| Wine 前缀 | `~/.deepinwine/dreamina-wine11`（Windows 用户名 `steamuser`） |
| 安装目录 | `C:\users\steamuser\AppData\Local\Programs\dreamina-desktop` |
| 主程序 | `即梦 AI.exe`（195 MB 的 Electron 壳，Chromium 138 / Electron 37.10.3） |
| 应用包 | `resources/app.asar`（1.7 MB，Electron + React，主进程 `out/main/index.js`） |
| 资源目录 | `resources/login-carousel/`（登录轮播图，本地资源） |
| 卸载器 | `Uninstall 即梦 AI.exe` |
| Windows 快捷方式 | `C:\users\steamuser\Desktop\即梦 AI.lnk`（安装器自动生成） |

启动入口：`./dreamina/run-dreamina.sh`（桌面入口 `dreamina/dreamina-wine.desktop`）。

**本项目已把它接入统一菜单**（`install.py`）：

| 菜单编号 | 作用 |
|---|---|
| **28** | 安装/修复：即梦 AI Windows 客户端（Wine 11 + NSIS 静默安装）→ 调 `dreamina/install-dreamina.sh` |
| **29** | 说明：即梦 AI 安装与渲染记录（本文件） |

即 `python3 install.py 28` 即可完成一遍安装；官方安装器需自行从官网下载后放进
`dreamina/` 目录（或 `~/下载`），脚本会自动查找（`.exe` 不入库，见 `.gitignore`）。

**安装过程中唯一真正的拦路虎是 Wine 的 powershell 空壳 stub**，详见第 2 节。

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

## 6. 文件清单

| 文件 | 作用 |
|---|---|
| `dreamina/install-dreamina.sh` | 完整安装流程：建前缀 → 换 powershell stub → 跑 `/S` → 校验（对应 `install.py` 菜单 **28**） |
| `dreamina/powershell-stub.c` | powershell 替身源码（凡见 `Get-CimInstance` 返回 1） |
| `dreamina/run-dreamina.sh` | 启动器（Steam Runtime 容器 + Wine 11 + 默认渲染开关） |
| `dreamina/dreamina-wine.desktop` | 桌面入口模板（已复制到 `~/Desktop/`） |
| `dreamina/screenshot-login.png` | 登录界面渲染成功的证据截图 |
| `dreamina/Dreamina-Desktop-*-x64-setup.exe` | 官方安装器（**不入库**，见 `.gitignore`） |
