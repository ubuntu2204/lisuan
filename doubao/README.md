# 豆包 Windows 客户端：Wine 11 排障记录（目标：官方最新版本）

更新时间：2026-11-08

平台：Ubuntu 24.04、海光 C86-4G、砺算 7G100、XWayland

> 约定：本文所有结论以**官方最新版本**为目标。官方版本有**两个口径**：
> 安装器渠道（2026-11-08 实测 2.32.6）与客户端运行版本（自更新后可更高，实测 2.35.8），
> 区别见 **2.1.2**，不要混用。旧版 1.56.6 只作为历史对照，
> 不作为解决方案、回退方案或验收参照。详见 `.codebuddy/rules/豆包客户端版本约定.md`。

**统一菜单入口**（`install.py`）：**26** = 安装/修复（`doubao/install-doubao.sh`），
**27** = 本说明文档。官方安装器需自行从官网下载后放入 `doubao/` 目录（或 `~/下载`），
脚本会自动查找（`.exe` 不入库，见 `.gitignore`）。

## 1. 结论先行

**主线是「Steam Runtime 容器 + Proton 11 对应的原生 Wine 11 + 独立前缀」跑官方最新版本。**
（安装器渠道当前给 2.32.6，客户端自更新后可以是更高版本，见 2.1.2。）

2026-11-08 实测结论（**已能启动并显示真实界面**，但根因要按会话区分）：

- **旧图形会话（11-07 22:00 – 11-08 02:53）**：裸跑宿主 `wine` 时主窗口一直
  `IsUnMapped`（强制 XMap 仍纯黑 1 色）、主进程 `exit_code 0` 主动退出；
  经 `SteamLinuxRuntime_sniper` 容器启动后 1200x800 窗口 `IsViewable`、抓图 546 色
  （白/浅灰为主），日志出现 `chrome://doubao-chat/`、`chrome://doubao-launcher/`，
  主进程存活 65 秒以上。**容器是那一会话里唯一验证成功的路径。**
- **机器 07:50:20 重启后（新会话，GNOME/XWayland/Steam 07:51 起）**：裸 wine 与容器
  **都能**出界面；甚至换成全新空 `User Data` 后两者也都成功（405 色）。
  所以容器**不是唯一必要条件**，黑屏与**宿主图形会话状态**相关；具体是哪一项
  （Steam 客户端 / CEF、gf GBM/Mesa、XWayland 累积状态、残留孤儿进程）尚未单独定论。
- 结论与《随从大师》「图形环境只注入目标进程树、绝不污染全局」「宿主 GL 栈异常会让
  应用建窗失败」的经验一致：容器提供隔离的 Mesa/GL 栈，绕开宿主污染状态，因此
  **保留为稳健默认**；`DOUBAO_NO_STEAM_RUNTIME=1` 可在干净会话里走轻量裸 wine 路径。
- `doubao/run-doubao-wine11.sh` 已默认用 `run-in-sniper` 包装自身，桌面入口无需改动。
- **点右上角 X 不会退出豆包**：那是「隐入通知栏」，而 XWayland/GNOME 下通知栏
  图标建不出来也看不见，于是看起来「关不掉」。已取证并给出显式入口
  `doubao/quit-doubao.sh`（`--quit` / `--show` / `--status`，见 2.6）。
- Deepin Wine 8.16 的 `RtlGetDeviceFamilyInfoEnum` 硬门槛仍在，主运行时必须是 Wine 11；
  旧版 1.56.6 不作方案、不作回退、不作验收参照。

不建议现在投入「把 Windows 豆包拆开后用 Electron 重新打包」。豆包不是只要替换 JavaScript 运行时就能迁移的普通 Electron 应用，主体是定制 Chromium 加大量闭源原生 DLL；完整重打包会持续碰到登录、推送、音视频、系统整合和自动更新等原生依赖。

如果只需要网页版聊天能力，可以另做一个连接豆包官网的 Linux Electron 外壳，这是成本较低的备用方案，但它不是 Windows 客户端的等价重打包，也不能保证客户端特有功能一致。

路线排序：

1. **Steam Runtime 容器 + Wine 11 运行官方 2.32.6**：主线，当前唯一目标，已出真实界面。
2. **Electron 外壳加载官方网页版**：可作为功能受限的备用客户端。
3. **Windows 资源 + Electron 全量重打包**：不建议，投入大、维护性差，且无法绕开闭源原生模块。

### 1.1 顶层定案：顶栏托盘菜单「关一次即永久失效」（2026-11-08 反编译 + 受控对照实验）

这是目前所有图形栈排障里**唯一被彻底定案**的问题，完整证据链在 **2.6.3**，此处只放结论。

**规律（可复现、可计数）**：

> 客户端每次启动后，**第一次**右键托盘图标必弹菜单；
> 菜单**一旦被关闭过一次**（按 ESC / 点菜单外 / 点菜单项，三者等价），
> 之后**永久**不再弹出，直到重启客户端。
> 反过来：菜单一直开着不关，连续右键都正常。

**责任方：客户端自身，与 Wine / GNOME / appindicator 无关。** 依据：

| 层次 | 证据 | 结论 |
|---|---|---|
| 事件投递 | Wine systray 日志：8 次右键 = 8 次 `WM_RBUTTONDOWN(0x204)` → `notify_owner relaying 0x204` → `send_inter_thread_message hwnd 0xc0184 msg 8001`，目标窗口始终是客户端的 `Doubao_StatusTrayWindow` | 右键**完整到达客户端**，不是图形栈吞掉 |
| 客户端内部 | 日志门控实测开着（`[0x192c788f0]=0`、`[0x192a40990]=1`），但整个会话日志**从未出现** `OnStatusIconClicked`；`_work/watch_menu.py` 以 0.03s 粒度轮询，坏态下右键**一次也没有** 165x284 菜单窗口闪现 | 不是「建了又被毁」，而是**根本没进创建流程** |
| 静态反编译 | `Doubao.dll`（ImageBase `0x180000000`）：`OnStatusIconClicked` @ `0x184856700`；`BuildMainStatusIconContextMenu` 引用点 `0x184855b86/0x184855c20/0x184855ce4`，`menu_ is null` 判断在 `0x184855c4a`；`main_status_icon_` 成员偏移 `this+0xb0` | 客户端自己的菜单对象状态机坏死 |
| 坏态/好态对比 | 两种状态下 Wine 的 systray 日志**逐字相同** | 责任在客户端，不在 Wine |
| 交叉验证 | 坏态下 Chromium 自己的气泡菜单（右上角 `⋯`）能正常弹出并关闭 → 全局 MenuController 健康 | 托盘是独立的一套状态 |
| 外部复位 | 激活/最小化主窗口、重广播 `TaskbarCreated`、左键单击、单实例再拉、Wine 内部注入 `WM_APP+1`、开关另一个 Chromium 菜单 —— **六种全部无效** | 无法从外部复位 |

同时**推翻了两个曾经的假设**：

- 「强制更新守卫吞掉点击」—— 进程内存直读该守卫字节 = `0x00`（从未置位），作废；
- 「`main_status_icon_ is null` 是根因」—— 精确串匹配后确认所有该日志**都在启动阶段**，与好坏态无关。

**唯一有效的恢复手段：重启客户端**（没有别的）。已交付诊断/修复工具：

```bash
./doubao/tray-doctor.sh --status   # 只读：进程数、主窗口、托盘图标、提醒
./doubao/tray-doctor.sh --probe    # 判定好坏态（会真弹菜单，不替你关）
./doubao/tray-doctor.sh --fix      # 探测 → 坏则重启客户端 → 等托盘图标注册 → 复测
```

实测闭环（同一台机器、同一客户端）：

```
好态：--probe → GOOD（菜单窗口 0x5a0000e 出现）
      → ESC 关闭菜单
坏态：--probe → BAD（6 秒内 0 个菜单窗口）
      → --fix 重启客户端 → 等托盘图标就位 → --probe → GOOD
```

**给使用者的结论**：托盘右键不弹时**不要折腾图形栈**，直接 `--fix`；
并且记住**别随手把菜单关掉就期待它还能再弹** —— 想用的时候就开着，
要关就等这一轮用完再重启客户端。

> 注：这是**客户端的设计缺陷**，不是本项目引入的，也不是 Wine 的问题。
> 豆包在 Windows 上的行为是否相同未验证（Windows 上托盘走 Win32 原生通知区，
> 菜单生命周期由 Shell_NotifyIcon 与 Chromium 菜单系统共同管理，机制不同）。

## 2. 当前实际状态

### 2.1 目标版本（唯一目标）

- 客户端：安装器渠道 `2.32.6 / Chromium 147.0.7727.149`；客户端自更新后可到更高版本
  （2026-11-08 实测运行中为 `2.35.8`）。两个口径的区别见 **2.1.2**，不要混用。
- 路径：`AppData/Local/Doubao/Application/app`（新版安装到用户目录，不覆盖 `Program Files`）
- Wine 11 前缀：`~/.deepinwine/doubao-wine11`（Windows 用户名为 `steamuser`）
- 安装入口：`doubao/install-doubao.sh`（2026-11-07 新增，已完整跑过两轮）
- 启动器：`doubao/run-doubao-wine11.sh`（自动套 Steam Runtime 容器）
- 状态：**窗口与主进程已通过验证**（2026-11-08）。`1200x800` 窗口
  `Map State: IsViewable`、抓图 405–546 色（白/浅灰为主），`CrBrowserMain` 连续存活
  65 秒以上，日志出现真实页面资源；桌面入口连续 3 次启动通过。
  登录、文字对话、10 分钟长时存活仍按第 8 节待验收。

#### 2.1.1 根因判定与消融（2026-11-08，含会话边界）

**必须区分图形会话**：本机 `uptime -s` 为 `2026-11-08 07:50:20`，GNOME/XWayland/Steam
均在 07:51 重新启动。此前全部黑屏实验在旧会话，此后全部实验在新会话。

| 会话 | 启动方式 | 主窗口 | 抓图 | 主进程 |
|---|---|---|---|---|
| 旧（重启前） | 宿主裸跑 `Proton 11.0/files/bin/wine` | `IsUnMapped` | 强制 XMap 后只有 `(0,0,0)` 1 色 | 约 10–20 秒 `NtTerminateProcess exit_code 0` 主动退出 |
| 旧（重启前） | `run-in-sniper -- bash run-doubao-wine11.sh` | `IsViewable` | 546 色，白/浅灰为主 | 稳定 65 秒以上 |
| 新（重启后） | 裸 wine（`DOUBAO_NO_STEAM_RUNTIME=1`） | `IsViewable` | 544 色 | 存活 |
| 新（重启后） | 桌面入口（容器） | `IsViewable` | 405–546 色 | 存活 |
| 新（重启后） | **全新空 `User Data` + 裸 wine / 容器** | 两者均 `IsViewable` | 均 405 色 | 均存活 |

消融结论（避免以后重复走弯路）：

- **容器是稳健默认，但不是唯一必要条件**：旧会话里它是唯一成功路径；新会话里
  裸 wine 也能跑。差异指向**宿主图形会话状态**（Steam 客户端 / CEF、gf GBM/Mesa、
  XWayland 累积状态或残留孤儿进程），具体是哪一项未单独定论，不要写成「缺 API」。
  容器提供隔离的 Mesa/GL 栈，能绕开宿主污染，因此保留为默认启动方式。
- **User Data 不是变量**：把 `Local State` 还原成 `info_cache={}`、删掉
  `Default/Preferences` / `Remote_State` / `saman_update_info`，甚至整体换成全新空
  `User Data`，容器内仍映射渲染；客户端会自行写全 profile。
- **网络已排除**：WinINet 探针在 Wine 8.16 与 Wine 11 前缀里访问
  `https://www.doubao.com/` 都拿到 HTTP 200（宿主 curl 302）。
- **Wine 8.16 补 API 不是解法**：按 `osrelease_redirect.so` 同款办法做
  `LD_PRELOAD` 打开重定向 + 给 `ntdll.dll` 追加 `RtlGetDeviceFamilyInfoEnum` 导出，
  严格对照下 `FATAL:base\win\win_util.cc(544) ... 0x7F` 确实消失（日志 75 → 369 行），
  但启动链随后进「豆包修复程序」流程，仍无主窗口。此路已证伪，不要重做。
- **渲染后端不是根因**：Wine 的 `DCompositionCreateDevice3` 两个版本都返回
  `E_NOTIMPL`，Chromium 本就会回退；下列开关在旧会话裸 wine 下全部仍为纯黑。

旧会话裸跑 wine 的历史证据（保留供以后对照）：

- `WINEDEBUG=+win` 显示主窗口类 `Chrome_WidgetWin_1`（HWND `0x1009e`，1200x800）
  以 `style=0x06cf0000` 创建，**不含 `WS_VISIBLE`**；此后**从未收到 `SW_SHOW`**，
  仅有的两条 `show_window` 都是其它小窗口的 `cmd=0`（`SW_HIDE`）。
  所以问题在「客户端自身没有调用显示窗口」，不是 X11 窗口没建出来。
- 从 X11 侧 `XMapWindow` 强制映射后 `Map State: IsViewable`，但抓图只有
  1 种颜色：`(0,0,0)` × 960000 像素（纯黑）。Wine 仍认为 HWND 隐藏，
  不会派发 `WM_PAINT`；从 Wine 内部用 32 位 PE 调 `ShowWindow`/`SetWindowPos`
  后 `IsWindowVisible` 仍为 0，跨进程强显不成立。
- 以下开关单次 A/B 后**仍为纯黑**：`--disable-direct-composition`、
  `--use-gl=angle --use-angle=swiftshader --enable-unsafe-swiftshader`、
  `--in-process-gpu`、`--disable-gpu --enable-unsafe-swiftshader`、
  `--disable-gpu-compositing --disable-gpu`、`--use-angle=gl`、
  `--disable-features=CalculateNativeWinOcclusion`、
  `--no-first-run --profile-directory=Default`。GPU 进程无 FATAL，日志里
  只有云配置缺失（`saman_app_state` 不存在 / `frame_context is null` /
  `user_id not ready`）这类非致命噪音。
- 主进程以 `NtTerminateProcess ... exit_code 0` 主动退出，不是段错误；
  残留孤儿进程用 `WINEPREFIX=~/.deepinwine/doubao-wine11 <Proton>/files/bin/wineserver -k`
  清理，**不做自动重启**。

#### 2.1.2 两条版本通道：安装器渠道 ≠ 客户端运行版本（2026-11-08 21:08 实测）

「官方最新版」在本项目里有**两个口径**，不写清楚就会得出互相矛盾的结论：

| 通道 | 取证方式 | 2026-11-08 实测值 |
|---|---|---|
| **安装器渠道** | `settings/v3?update_type=downloader` 返回 `doubao_pc/<ver>/Doubao_win_X64_<ver>-signed.zip` | **2.32.6** |
| **客户端运行版本** | 客户端 crashpad 记录里的 `app_version` 字段 | **2.35.8** |

- 安装器自己请求的也是同一个接口、也是 2.32.6：

  ```
  21:07:45  [install] host: www.doubao.com, path: service/settings/v3/?...&update_type=downloader&...
  21:07:45  [install] host: lf-flow-web-cdn.doubao.com, path: obj/flow-doubao/doubao_pc/2.32.6/Doubao_win_X64_2.32.6-signed.zip
  ```

- 但客户端进程自己报的是更高版本：

  ```
  [Parfait] Record crash {"aid":"7777","app_version":"2.35.8","channel":"webdaoliu_doudi_online_installer_win",...}
  ```

  （`saman_crashpad_handler_2026.1108.log`，`client_timestamp=1794098970771` ≈ 2026-11-08 08:49 CST）

**由此产生的实测行为**（2026-11-08 21:07 走 `install.py` 菜单 26 实跑）：

```
安装前版本：2.35.8     ← 客户端已自更新到 2.35.8
安装后版本：2.32.6     ← 安装器渠道的包把客户端拉回了 2.32.6
```

即**跑一次官方安装器 = 先把客户端落回安装器渠道版本**，之后客户端自己的更新器
再决定最终跑到哪个版本。因此：

- 「官方接口返回 2.32.6」**不能**用来断言「本机装的客户端就是 2.32.6」，
  也不能断言「2.32.6 是客户端侧最新」；
- 判断本机实际版本要看 `Application/app/manifest.json` 的 `version` 字段，
  运行期看 crashpad 的 `app_version`；
- 本项目**不做任何版本锁定或回退**（见 `.codebuddy/rules/豆包客户端版本约定.md`），
  安装器给什么版本就装什么版本。

### 2.2 旧版（仅作历史对照，不得作为方案）

- 客户端：`1.56.6 / Chromium 129.0.6668.79`
- 路径：`Program Files/Doubao/app`
- 早期实测：在 Deepin Wine 8.16 + Mesa 软件渲染下能显示完整 `doubao.com/chat/` 界面。
- 这条记录仅用于对比崩溃点差异；**不得**拿它当验收参照，也不得把修复脚本做成自动回退到它。

### 2.3 用户提供的在线安装器

文件（仓库内副本已进 `.gitignore`，`doubao/doubao_client_*.exe`，不入库）：

- `doubao/doubao_client_77737216855800721777_ext.exe`
- `~/下载/doubao_client_77737216855800721777_ext.exe`
- `~/下载/doubao_client_77737216855802805777_ext.exe`（上一轮下载）

信息：

- 64 位 PE 安装器（字节跳动 Saman 安装器），安装器自身版本 `2.25.16`，
  内嵌 `chromium_version 147.0.7727.89`、`product_app_id 582478`。
- SHA-256：`865d6bf9bd18e498957314194a6d837607df850d87f0f3ed13a88ff543cf23e9`
  —— 上面三个文件字节相同，文件名里那串数字只是下载地址编号，**不代表客户端版本**。
- 真正的客户端包由接口下发，安装前可先用 curl 预查（无需 Wine、无需登录）：

  ```bash
  curl -s 'https://www.doubao.com/service/settings/v3/?device_platform=pc&brand=doubao&aid=582478&os=windows&update_type=downloader&version_code=2.25.16&os_version=10.0.22000' \
    | grep -oE 'doubao_pc/[0-9.]+/Doubao_win_X64_[0-9.]+-signed\.zip'
  ```

  2026-11-07 21:44 与 2026-11-08 21:07 两次实测均返回
  `doubao_pc/2.32.6/Doubao_win_X64_2.32.6-signed.zip`，即**安装器渠道当前仍是 2.32.6**。
  注意这个接口只代表安装器渠道，**不代表客户端运行版本**（本机运行中一度是 2.35.8，
  详见 2.1.2）；换 `update_type=update/updater/hotupdate` 或留空，接口一律回 2.32.6。

#### `/S` 不是静默安装（2026-11-07 实测推翻旧结论）

早期记录写的「`/S` 可以静默安装」不成立。安装器日志明确显示：

```
[install] CmdLine:/S
[install] saman command line install mode is      ← 空，/S 没被当成命令行安装模式
[install] CBaseWindow::OnSize to 718x478           ← 弹出 GUI 后就此停住
```

窗口停在「立即安装」按钮上**空等人工点击**（实测空等 4 分钟无进展），
不点击就什么都不会发生。因此安装入口已固定为 `doubao/install-doubao.sh`：
它用 `_NET_WM_PID` 反查确认安装器窗口、抓窗口位图按按钮主题色 `#3370FF`
定位「立即安装」实心块（不写死坐标，兼容 DPI 缩放），点下去后以安装日志
本轮新增行里的 `_OnInstallSuccess` 为完成判据。

抓图与点击这一段有四个必须处理的坑（2026-11-08 21:07 通过 `install.py` 菜单实跑才暴露）：

- **窗口枚举要按 PID 过滤**：豆包客户端自身也注册名为「豆包」的窗口，
  混进来会抓到没有安装界面的窗口。
- **必须等映射**：安装器窗口刚建出来时 `map_state != IsViewable`，
  此时 `XGetImage` 直接抛 `Xlib.error.BadMatch`（`code=8`）。脚本改为轮询
  `map_state` 并重试抓图，最多等 60 秒。
- **点击要走 XTEST**：`xdotool click --window` 用的是 `XSendEvent` 合成事件，
  Wine 侧会忽略；改成 `mousemove --window` + `xdotool click` 的真实指针点击。
- **安装前必须关掉同前缀的客户端**：Wine 进程的命令行里**不含** `WINEPREFIX`
  （只有 `C:\...` 路径），`pgrep -f "$PREFIX"` 匹配不到；前缀只在
  `/proc/<pid>/environ` 里。脚本改用 `environ` 精确匹配，否则运行中的豆包关不掉，
  安装器会因文件被占用而卡住。

#### 一次完整安装的实际行为（2026-11-07 21:46 日志）

```
21:46:04  _StartInstallThread → GET service/settings/v3/
21:46:04  host: lf-flow-web-cdn.doubao.com, path: obj/flow-doubao/doubao_pc/2.32.6/Doubao_win_X64_2.32.6-signed.zip
          下载到 %LOCALAPPDATA%\Temp\Doubao\2.32.6.zip，共 948,603,770 字节（≈904 MB）
21:46:43  wait uninstall → WaitForExit done（先走旧实例卸载）
21:47:00  Replace uninstaller / Replace launcher → launcher/uninstaller extracted successfully
21:47:00  write version → create desktop icon → Add Install/Uninstall Reg
21:47:00  _OnInstallSuccess → _LaunchAgent（自动拉起客户端）
```

两条 `ERRO` 是无害的：`GetHttpTotalSize::WinHttpQueryHeaders error: 12150`（HEAD 取长度失败，
不影响 GET 下载）和 `UnzipDoctorApp not found Doctor.zip`（包里本来就没带 Doctor.zip）。
临时包自删，`app` 目录 1.8 GB，`豆包.lnk` 重建。整个流程在本机约 60 秒。

新版没有覆盖 `Program Files` 旧版，而是安装到了用户目录：

`~/.deepinwine/doubao-wine/drive_c/users/ubuntu/AppData/Local/Doubao/Application/app`

因此目前机器上同时存在两个客户端：

| 客户端 | 版本 | 当前状态 |
|---|---|---|
| `Program Files/Doubao/app` | 1.56.6 | 早期在 Deepin Wine 8.16 + Mesa 下可显示，**仅作历史对照** |
| `AppData/Local/Doubao/Application/app` | 2.32.6（安装器渠道；客户端自更新后可为 2.35.8） | **唯一目标版本**；经 Steam Runtime 容器启动已显示真实界面（见 2.1.1、2.1.2） |

安装器还创建了 `~/Desktop/豆包.lnk`。`run-doubao-wine.sh` 仍指向旧版，
`run-doubao-wine11.sh` 才是新版启动器；桌面入口已于 2026-11-07 切到后者。

### 2.4 桌面入口回归（2026-11-07 22:08 实测，已修）

症状：双击桌面「豆包」没反应 / 秒退。`journalctl --user` 里的现场是：

```
DING: [DEBUG]: Run doubao-wine 1.56.6wine3 c:/Program Files/Doubao/Doubao.exe
DING: wine version: 8.16
DING: [424:580:FATAL:base\win\win_util.cc(544)] Check failed: . : 找不到过程。 (0x7F)
```

即点击的是安装器生成的 `~/Desktop/豆包.lnk`：DING 把它交给
`spark-dwine-helper` → `/opt/apps/doubao-wine/files/run.sh` → **Deepin Wine 8.16
旧前缀**，于是撞上坑 8 的 `RtlGetDeviceFamilyInfoEnum` 缺失。
同时 `~/Desktop/doubao-wine.desktop` 没有 `metadata::trusted=true`，
DING 会把它当成不可信启动器，双击不一定执行。

本机共有 5 个「豆包」入口（4 个 `.desktop` + 1 个 `.lnk`），修复前只有桌面
`.desktop` 指向 Wine 11：

| 入口 | 修复前 Exec | 修复后 |
|---|---|---|
| `/usr/share/applications/doubao-wine.desktop` | deb 的 `run.sh --uri %F` | 指向 `run-doubao-wine11.sh`（08:12 补充） |
| `/opt/apps/doubao-wine/entries/applications/doubao-wine.desktop` | deb 的 `run.sh --uri %F` | 指向 `run-doubao-wine11.sh`（08:12 补充） |
| `~/.local/share/applications/doubao-wine.desktop` | 旧版 `run-doubao-wine.sh --uri %F` | 指向 `run-doubao-wine11.sh` |
| `~/Desktop/doubao-wine.desktop` | `run-doubao-wine11.sh` | 去 shebang、补 `trusted=true` |
| `~/Desktop/豆包.lnk` | Windows 快捷方式（无默认处理器，DING 转交旧 Wine） | 移出桌面，可恢复 |

修复入口：`doubao/fix-doubao-desktop.sh`（`--help` / `--status` / `--uninstall`）。
它幂等地统一**四个** `.desktop`（桌面 / 用户级 / 系统级 / deb entries）的 `Exec`，
去掉裸 `--uri` 与非标准 shebang、打 `metadata::trusted=true`、把 `.lnk` 移到
`~/.local/share/doubao-wine11/disabled-desktop-shortcuts/`；原文件与旧前缀
`repair/record.json` 备份在 `~/.local/share/doubao-wine11/desktop-backups/`
（不留在桌面产生多余图标），`--uninstall` 可完整还原。
`install-doubao.sh` 安装成功后会自动跑一次，防止安装器重建 `.lnk` 后复发。
`run-doubao-wine11.sh` 把启动输出同时写入
`~/.local/share/doubao-wine11/launch.log`，方便排查没有终端的桌面双击。
系统级与 deb entries 文件属主是当前用户，可写；不可写时脚本明确提示而不静默跳过。

**边界**：这只修「点错运行时」这一层，与旧会话里宿主图形栈导致的黑屏
是两个独立问题（见 2.1.1）。当前桌面入口指向的 `run-doubao-wine11.sh`
已默认套容器，双击可正常出界面；全程不改回 1.56.6、不加看门狗或自动重启。

### 2.5 「当前版本启动发生异常」修复弹窗（2026-11-08 08:12，已修）

现象：启动后弹出「豆包修复程序：当前版本启动发生异常，可能导致无法正常使用，
是否进行修复？」（可勾选「该版本不再提醒」）。

取证结论：

- 弹窗进程是 `repair_2026_11_08_08_12_27.exe`，参数指向
  `C:\Program Files\Doubao\app` 与 `C:\users\ubuntu\...\User Data`，
  wineserver 是 `deepin-wine8-stable` —— **来自旧 Deepin Wine 8.16 前缀**，
  不是 Wine 11 前缀（那边 `running_exception` 一直是 0）。
- 08:12 的 journalctl 链路：`完成：修复 0 个软链接`（只出自
  `doubao/fix-doubao-wine.sh`）→ `Run doubao-wine 1.56.6wine3` →
  `wine version: 8.16`，说明点击落在旧 `run-doubao-wine.sh` / deb `run.sh` 链上。
- 旧前缀 `repair/record.json` 的 `running_exception` 已累到
  `{"1.56.6":1,"2.32.6":8}`，超过阈值触发修复提示。这些计数来自排查期间反复
  `wineserver -k` / `timeout` 强杀进程，**不是真实崩溃**；但旧前缀本来就跑不了
  2.32.6（缺 `RtlGetDeviceFamilyInfoEnum`），不应再被任何入口触发。

修复（全部幂等、可还原）：

1. 四个 `.desktop` 入口统一指向 `run-doubao-wine11.sh`，并去掉会让部分文件管理器
   把 `.desktop` 当脚本执行的 `#!/usr/bin/env xdg-open` 首行。
2. `doubao/run-doubao-wine.sh` 改为**安全转发**到 Wine 11 启动器：残留旧快捷方式
   也不会再进 8.16；不再跑 `fix-doubao-wine.sh`、不再注入旧 Mesa 环境、不再调 deb `run.sh`。
   需要 1.56.6 历史对照时直接用 `deepin-wine8-stable` 手动启动旧前缀。
3. 备份后把旧前缀 `running_exception` 清零（不删账号数据、不禁用修复器、不锁版本）。

验证：旧启动器 `--uri`、系统级 desktop、桌面图标三条路径实测都进 Steam Runtime
容器 / Wine 11，`map=2`、405 色真实界面，`main_browser=1`，无 8.16 旧链进程；
旧前缀异常计数保持为空。

### 2.6 点右上角 X 关不掉豆包（2026-11-08 09:20，已给显式入口）

现象：点主窗口右上角 X，窗口消失但豆包并没退出；顶栏图标右键有时出菜单、
有时没反应，看起来就是「关不掉」。

取证（三条都可在本机复现）：

1. **X 的语义是「隐入通知栏」，不是退出**。点 X 后主窗口 `WM_STATE` 变成
   `Withdrawn`、`Map State: IsUnMapped`，`User Data/saman_app_state` 写成
   `chat_app_state.visible=false`，而 17 个 `Doubao.exe` 进程照常存活。
2. **托盘菜单在本机建不出来**。豆包的托盘图标确实上了顶栏（见 2.6.1），但
   客户端侧的 StatusIcon 对象是空的：每次启动都报
   `[Tray] BuildMainStatusIconContextMenu main_status_icon_ is null`（11-08 累计 34 次），
   于是「打开豆包窗口 / 退出豆包」这两条菜单项点不出来。
3. **客户端没有「关闭主窗口时退出程序」这类开关**。`Doubao.dll` 里的
   `saman.general_settings.*` 只有 `double_click_close_tab`、`file_open_method`、
   `link_open_method`、`primary_browser` 四个键；桌面悬浮球右键菜单也只有
   打开豆包窗口 / 打开对话浮窗 / 打开豆包浏览器 / 截图提问 / 开启录音转写 /
   实时双语字幕 / 隐藏直到下次重启豆包 / 停用豆包桌面悬浮球，**没有退出项**。

处理：`doubao/quit-doubao.sh` 提供三个显式动作（不做守护、不做自动重启、
不改客户端版本，进程筛选按 `WINEPREFIX` 精确限定在本前缀）：

| 动作 | 行为 | 2026-11-08 实测 |
|---|---|---|
| `--quit`（默认） | SIGTERM 主浏览器进程，必要时本前缀 `wineserver -k` 收尾 | 1 秒内 17 个进程全退，`repair/record.json` 的 `running_exception` 仍为 `{"2.32.6":0,"2.35.8":0}`，不会引出「启动异常」修复弹窗 |
| `--show` | 再拉一次启动器；单实例会自己把窗口显示出来，没在跑时等于正常启动 | 隐入通知栏后 5 秒内 `IsViewable`；`--quit` 后重启 6 秒出界面 |
| `--status` | 只读报告进程数、主窗口 Map State、XEmbed 托盘 owner 与嵌入图标数 | 能直接把「窗口已隐入通知栏、进程仍在」和崩溃区分开 |

注意一个边界：用窗口管理器标题栏的关闭（`WM_DELETE_WINDOW`，例如
`xdotool windowclose`）与点页面内 X 不完全等价 —— 前者会把主窗口**销毁**，
此时再拉一次启动器并不会重建它（实测 60 秒不出窗口）。这种情况下用
悬浮球右键「打开豆包窗口」，或 `--quit` 后重新启动即可。

另外：托盘图标的**渲染不是障碍**（Ubuntu 的 appindicator 扩展自带 legacy XEmbed
托盘支持，见 2.6.1），障碍在客户端侧的 StatusIcon 与云端配置链路；所以不需要
`xembedsniproxy`、`stalonetray`、`trayer` 这类额外东西（本机仓库也没有 `xembedsniproxy`），
**不引入新依赖**，改用上面的显式入口；豆包在 Windows 上点 X 同样是隐入托盘，行为并不特殊。

#### 2.6.1 顶部状态栏的「退出」为什么时好时坏（2026-11-08 10:15 更正并补测）

> ⚠️ **本小节的最终结论已被 2.6.3 推翻**：当时归因于「云配置 / 登录态拉不到 → 建不出菜单」，
> 实测证明那是**红鲱鱼** —— 真因是**菜单一旦被关闭过就永久失效**（见 1.1 / 2.6.3）。
> 本小节保留是因为它的取证（托盘图标本体、XEmbed 状态、6 种输入尝试）
> 仍是有效的，且记录了「如何证伪一个看似合理的假设」。

**先更正**：本小节上一版写「顶栏里根本没有豆包图标」是错的。当时只查了
SNI 注册表和 `_NET_SYSTEM_TRAY_S0` owner 的子窗口数，漏了一个事实：
Ubuntu 的 `ubuntu-appindicators` 扩展**自带 legacy XEmbed 托盘**：

```bash
gsettings get org.gnome.shell.extensions.appindicator legacy-tray-enabled  # true
gsettings get org.gnome.shell.extensions.appindicator tray-pos             # 'right'
# trayIconsManager.js 里 new Shell.TrayManager() + connect('tray-icon-added')
```

实测到的图标本体：

| 顶栏图标现场 | 实测值 |
|---|---|
| 图标窗口 | `0xe0000e`，16x16，绝对位置 1556,8（就在顶栏右侧） |
| XEmbed 状态 | `_XEMBED_INFO = 0x0, 0x1`（XEMBED_MAPPED，已正常嵌入） |
| 父窗口 | mutter 为 legacy tray 单独建的容器 `0x6000b8`（不在 owner `0x600009` 的子窗口里，这是之前漏判的原因） |
| 窗口属主 | Wine 的 `explorer.exe /desktop`（pid 230927），不是豆包主进程 —— Wine 把 `Shell_NotifyIcon` 代理给了 shell |

**为什么点了没反应**：图标只是「显示在那里」，客户端侧能接菜单的对象是空的。
今天全部会话里 `[Tray] BuildMainStatusIconContextMenu main_status_icon_ is null`
共 34 次，每次启动都刷；同一时间还有一堆云配置/账号链路失败：
`Cloud config not exists or invalid` 300 次、`user_id not ready` 23 次、
`frame_context is null` 31 次，本会话还有 `account_token_manager_initial_fetch_error`
与 `auth_get_oauth_token_error`。托盘菜单的项与命令就靠这套云端配置，
拉不到就建不出菜单。

**输入侧试了 6 种，全部既无菜单也无 app 侧 tray 日志**：真实指针右键、左键、
`xdotool click --window`（XSendEvent 直接投递）、`windowfocus` + Menu 键、
`windowraise` 后再点、新鲜启动后点、以及销毁主窗口后点。

**另外两个已证实的事实**（推翻上一版的两个说法）：

- WM 级「退出」（顶栏应用菜单 / 任务栏 / Alt+F4 / `xdotool windowclose`）确实
  只是 `WM_DELETE_WINDOW`：只关主窗口、隐藏悬浮球后再关、主窗口与悬浮球都关，
  三种组合下 17 个进程全部存活 —— 这条路从来没退过进程。
- 顶栏那个图标不是「抢不到」的 XEmbed 托盘：Ubuntu 已经 legacy-tray 把
  Wine 的图标接住了，所以**不需要** `xembedsniproxy`/`stalonetray`/`trayer`。

**结论**：顶部「退出」时好时坏的变量在**客户端自己的云配置/登录态**，
不在图形栈，本机无法从外部修好。可用动作只有两个：

1. 先登录豆包、等云端配置拉下来（看日志里 `Cloud config not exists or invalid`
   不再刷），顶栏右键菜单才有内容；
2. 不折腾托盘，直接 `./doubao/quit-doubao.sh --quit`（实测 1–2 秒清干净）。

判据只看一个：`--status` 的「豆包进程」是不是 0；窗口消失不等于退出。
想要一个不用打字的退出按钮，可以加个 GNOME 快捷键（可逆，不改客户端）：

  ```bash
  gsettings set org.gnome.settings-daemon.plugins.media-keys custom-keybindings \
    "['/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/doubao-quit/']"
  gsettings set org.gnome.settings-daemon.plugins.media-keys.custom-keybinding:/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/doubao-quit/ \
    name '彻底退出豆包'
  gsettings set org.gnome.settings-daemon.plugins.media-keys.custom-keybinding:/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/doubao-quit/ \
    command '/home/ubuntu/project/lisuan/doubao/quit-doubao.sh --quit'
  gsettings set org.gnome.settings-daemon.plugins.media-keys.custom-keybinding:/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/doubao-quit/ \
    binding '<Super><Alt>q'
  ```

#### 2.6.2 带 Wine systray 调试的深挖：菜单为什么「时有时无」（2026-11-08 13:20）

> ⚠️ **本小节结论已被 2.6.3 补充与修正**：
> 「菜单只在启动阶段建一次」方向对，但**触发条件说错了** —— 不是「构建失败」，
> 而是「**构建成功后被关闭过一次**」；因此 2.6.2 里「广播 `TaskbarCreated` 后第一次点击
> 能弹、之后再不弹」的观测，真实原因是那次「能弹」恰好是**本轮启动的第一次**。
> 本小节的 Wine systray 调试方法与消息链取证仍然有效。

方法：`WINEDEBUG=+systray` 重启客户端，Wine 会打印托盘图标的完整消息链
（`add_icon` / `Shell_NotifyIconW` / `X11DRV_SystrayDockInsert` / `tray_icon_wndproc` /
`notify_owner relaying`）。注意一个方法论：GNOME 面板是 Wayland actor，
**`xdotool` 的 XTEST 模拟点击到不了面板** —— 只有真实鼠标点击有效，
这就是之前 6 种「模拟点击」全部无效的原因，不是应用不理人。

已证实：

1. **真实右键确实到了豆包**。一次点击在 Wine 侧产生 `WM_MOUSEMOVE(0x200)` →
   `WM_RBUTTONDOWN(0x204)` → `notify_owner relaying 0x204` → `WM_RBUTTONUP(0x205)`
   → relaying（双击还有 `0x206`），另外每次点击前还会有 `WM_CONTEXTMENU(0x7b)`。
2. **豆包收到后不建任何菜单窗口**。100ms 粒度的窗口轮询覆盖 3 次真实点击，
   豆包进程 0 个新窗口，日志里也 0 条 `[Tray]`。
3. **菜单只在启动阶段建一次**。每个会话 2–4 条
   `[Tray] BuildMainStatusIconContextMenu main_status_icon_ is null`（容器启动约 15s、
   裸 wine 约 60s 出现），之后既不重建也不报错 —— 这一会话里托盘菜单能不能用，
   在启动那一刻就定了。
4. **广播 `TaskbarCreated` 能让客户端重新注册**：应用先 `Shell_NotifyIconW(2)=NIM_DELETE`
   再 `(0)=NIM_ADD`，Wine 重新 `add_icon` + `X11DRV_SystrayDockInsert`，这次**不再报 null**。
   探针源码：`doubao/probe-trayrefresh.c`（`i686-w64-mingw32-gcc` 编成 PE32，
   在容器内跑）。实测第一次重建后用户右键**弹出过一次菜单**，但之后又不响应；
   再广播一次重建后第一次点击也不再弹。→ 只能算诊断手段，**不构成可靠修复**。
5. **不要杀 Wine 的 explorer.exe**：托盘图标随它一起消失，应用不会自己补注册
   （实测新 explorer 里 0 个 16x16 图标窗口），只能重启客户端恢复。

结论：这是客户端托盘模块自己的状态问题（菜单构建一次性、失败后不自愈），
本机只能诊断、不能从外部修好。可用动作仍是 `--quit` / `--show`；
`--status` 已改为**只看本次启动**的日志段来判断托盘菜单能否用，不再读历史会话。

#### 2.6.3 反编译定案：菜单「关一次就永久失效」（2026-11-08 19:00）

> 本节的**顶层结论**（规律 + 责任方 + 唯一恢复手段 + 给使用者的建议）已提到
> **第 1.1 节**；本节是支撑该结论的完整证据链。两处内容一致，以本节为准。

上一小节的结论「菜单构建一次性、失败后不自愈」方向对，但**触发条件说错了**。
经过反编译 `Doubao.dll` + 受控对照实验，真实规律是：

> **客户端每次启动后第一次右键必弹；菜单一旦被关闭过一次（按 ESC / 点菜单外 /
> 点菜单项都一样），之后**永久**不再弹出，直到重启客户端。
> 反过来，菜单一直开着不关的话，连续右键都正常。**

**这就把「时有时无」完全解释清楚了**：用户看到的「有时不弹」，实际是
「上次关过菜单 → 本次永远不弹 → 重启后第一次能弹 → 一关又废」的循环。
所谓「有时候又能弹了」，是因为中间重启过客户端。

##### 证据一：受控对照实验（每组都重启客户端，保证起点一致）

实验脚本 `_work/exp_close.sh`，判定方式是「是否存在 165x284 的菜单窗口」：

| 组 | 操作 | 第一次右键 | 第二次右键 |
|---|---|---|---|
| A | 菜单保持打开不关，直接第二次右键 | 菜单窗口=1 | **菜单窗口=1（正常）** |
| B | 按 ESC 关闭菜单后再右键 | 菜单窗口=1 | **菜单窗口=0（失效）** |
| C | 点菜单外关闭后再右键 | 菜单窗口=1 | **菜单窗口=0（失效）** |

##### 证据二：责任在客户端，不在 Wine / GNOME / appindicator

坏态与好态下，Wine 的 systray 日志**逐字相同**：

```
tray_icon_wndproc hwnd 0000000000010198, msg 0x204, wparam 0x2, lparam 0x80008
notify_owner relaying 0x204
send_inter_thread_message hwnd 0xc0184 msg 8001 (8001) wp 8 lp 204
```

目标窗口句柄始终是 `0xc0184`（`Doubao_StatusTrayWindow`），
8 次右键 = 8 次 relaying。**右键事件完整地交到了客户端手里**，
是客户端收到后自己不再建菜单窗口。

配套的两条反证：

- **菜单窗口 0 次闪现**：`_work/watch_menu.py` 以 0.03s 高频轮询 165x284 窗口的
  出现/消失，坏态下右键一次也没有任何事件 —— 不是「创建后立刻销毁」，
  而是根本没进入创建流程。
- **客户端自己的日志从未触发**：`Doubao.dll` 里的日志门控开关实测为
  `[0x192c788f0]=0`、`[0x192a40990]=1`（日志是开着的），但整个会话日志里
  **从未出现 `OnStatusIconClicked`** —— 那个函数根本没被调用到。

##### 证据三：反编译定位（`Doubao.dll`，ImageBase=0x180000000）

用 `objdump -M intel --section=.text` 静态反汇编 + `/proc/PID/mem` 直读内存比对：

| 符号 | 地址 | 说明 |
|---|---|---|
| `OnStatusIconClicked` | `0x184856700` | 托盘点击入口；开头是日志门控，正常路径 `mov rcx,[rsi-0x8]; call 0x183ee61d0` 取 `menu_`，再 `call 0x183c40430` 显示菜单 |
| `BuildMainStatusIconContextMenu` | 引用点 `0x184855b86 / 0x184855c20 / 0x184855ce4` | `menu_ is null` 的判断在 `0x184855c4a` |
| `main_status_icon_` | 成员偏移 `this+0xb0` | 赋值在 `0x184857950`；引用点 `0x184855263 / 0x184855d0e / 0x1848566b1 / 0x1848579fb` |
| `ExecuteCommand` | `0x1848569c0` | `esi==0xd6ed` 走正常路径 `0x184856bda → call 0x184cfc150` |

##### 推翻了上一轮的两个红鲱鱼

1. **强制更新守卫不是根因**。上一轮怀疑云配置键 `saman.need_force_update_version`
   的守卫字节 `0x192c65550` 吞掉了点击。进程内存直读实测该字节 = **0x00**（从未置位），
   日志里 `GetCloudMinVersion GetValue(kForceUpdateConfig) returned nullptr`
   也印证守卫没生效 → 该假设作废。
2. **`main_status_icon_ is null` 不是根因**。早期用 `grep 'is null'` 宽匹配统计，
   误以为它在坏态会新增；改用精确串 `'main_status_icon_ is null'` 后确认：
   **所有出现都在启动阶段**（如 `184439.758 / 184439.845 / 184440.551`），与好坏态无关。
   （另：`grep` 宽匹配导致的一次计数误判 3→22，实际是别的 `...is null` 行。）

##### 还排除了：Chromium MenuController 全局卡死

坏态下点豆包主窗口右上角的 `⋯`，Chromium 的圆角气泡菜单（开启自动捕获 /
在对话浮窗中打开）**能正常弹出并关闭** → 全局菜单控制器是健康的；
但打开再关闭它之后，托盘右键**仍然坏** → 托盘是独立的一套状态。

##### 六种外部复位手段全部无效

| 手段 | 结果 |
|---|---|
| 激活主窗口 / 最小化再恢复主窗口 | 无效 |
| 广播 `TaskbarCreated` 让客户端重注册托盘图标 | 无效 |
| 左键单击托盘图标 | 无效 |
| 单实例再拉一次客户端 | 无效 |
| Wine 内部注入 `WM_APP+1`（绕过 GNOME 直接喂消息） | 无效 |
| 打开再关闭另一个 Chromium 菜单 | 无效 |

##### 交付物

| 文件 | 作用 |
|---|---|
| `doubao/tray-probe.py` | 探针：像 GNOME Shell 那样用 `XSendEvent` 向 16x16 的 systray 图标窗口投递 ButtonPress/Release（等价 XEmbed 转发），再轮询判断 165x284 菜单窗口是否出现。**绝不主动关闭菜单**（关菜单正是触发坏态的动作）。退出码 `0`=好 / `1`=坏 / `3`=没找到托盘图标 |
| `doubao/tray-doctor.sh` | 入口：`--status`（只读）/ `--probe`（会真弹菜单，不替你关）/ `--fix`（探测 → 坏则重启客户端 → 等托盘图标重新注册 → 复测） |

实测闭环（同一台机器、同一客户端）：

```
好态：--probe → GOOD（菜单窗口 0x5a0000e 出现）
      → ESC 关闭菜单
坏态：--probe → BAD（6 秒内 0 个菜单窗口）
      → --fix 重启客户端 → 等托盘图标就位 → --probe → GOOD
```

**给使用者的结论**：托盘右键不弹时不要折腾图形栈，
直接 `./doubao/tray-doctor.sh --fix`；并且记住
**别随手把菜单关掉就期待它还能再弹** —— 想用的时候就开着，
要关就等这一轮用完再重启客户端。

##### 方法论备注（给后续排障用）

- **gdb 附加 Wine 进程下不了断点**：Wine 的 fake PE header 让 gdb 以为目标是 32 位，
  地址被截断（`0x6fffef266700` → `0xef266700`），报 `Cannot insert breakpoint`。
  改用 `/proc/PID/maps` 找模块基址 + `/proc/PID/mem` 的 `os.pread/os.pwrite` 直读直写。
- **取基址时别用 `pgrep -f "Doubao.exe" | head -1`**：会取到 crashpad-handler。
  要遍历并排除带 `--type=` 的子进程。
- **PE 段表**（`/proc/PID/maps` + `_work/pe_seg.py` 校正后的值）：
  `.text` RVA `0x1000`/文件偏移 `0x600`、`.rdata` `0x10684000`/`0x10683200`、
  `.data` `0x1299a000`/`0x12998400`、`.tls` `0x134ab000`/`0x13336000`；
  VMA = ImageBase + RVA。

## 2.7 豆包用的 Wine 来源与完整性（2026-11-08 核对）

**结论：豆包用的是 Valve 官方 Proton 11.0 里的 wine-11.0，没有为本项目改过。**

| 项 | 实测值 |
|---|---|
| Proton 目录 | `~/.local/share/Steam/steamapps/common/Proton 11.0` |
| Steam 记录的构建 | `proton-11.0-2e-x86_64`；`wine --version` = `wine-11.0` |
| 运行时本体 | 主进程 `/proc/<pid>/exe → …/Proton 11.0/files/lib/wine/x86_64-unix/wine64-preloader` |
| 库搜索路径 | `WINEDLLPATH` / `LD_LIBRARY_PATH` 全部指向 `Proton 11.0/files/…` |
| 注入 | `LD_PRELOAD` **为空**（豆包不加载任何 gf 兼容垫片）；只有 `WINEDLLOVERRIDES=mscoree,mshtml=` |
| 完整性 | 根目录无 `*.bak*` / `*modified*` / `*.orig` / `*.patch`；`files/` 下无晚于安装时间（2026-10-18 09:14）1 小时以上的文件 |

`--status` 现在会直接报这一行：`wine 来源 : Proton 11.0（未见本地改动）`。

机器上确实另外存在一套**改过的 Proton**：`Proton - MM GF`（实际是 Proton
Experimental `11.0-20260826` 改名），改动包括：

- `proton` 脚本被改（94439B，旁边留有 `proton.mymodified.bak`）；
- `user_settings.py` 多个变体（`.bak-wrongfmt` / `.bak` / `.disabled`）；
- DXVK 从 v3.0.2 降到 v2.3-21（备份在 `files/lib/wine/dxvk.bak-v3`），vkd3d-proton 替换；
- wine 核心 `.so` 动过：`ntdll.so` / `winex11.so` / `win32u.so`（x86_64 与 i386 各一套）。

但它是给《随从大师》/gf 用的：**豆包不引用它**（`DOUBAO_PROTON` 未设置，启动器
默认 Proton 11.0；也没有 `compatibilitytools.d` 覆盖）。另外全盘没有 wine 源码树
（找不到 `dlls/winex11.drv`），MM GF 的改动也只有二进制与脚本层面的替换 ——
**所以既不存在「豆包专用改版 wine」，也没有源码可拷到 `doubao/`**。

## 3. Deepin Wine 8.16 已确认的坑

### 坑 1：安装包把 WinSxS DLL 链接写死到打包者 home

最初错误是：

`Failed to load Chrome DLL ... app\Doubao.dll: 找不到模块 (0x7E)`

`Doubao.dll` 文件实际存在。用 `WINEDEBUG=+loaddll,+module` 追踪后，真实依赖链为：

`comctl32.dll → compstui.dll → WINSPOOL.DRV → Doubao.dll`

Wine 实际选择的是 WinSxS 中的 `comctl32.dll`，但安装包把链接写成了：

`/home/zcr/.deepinwine/doubao-wine/dosdevices/c:/...`

在当前用户 `ubuntu` 下这些都是悬空链接。共修复 28 个，改成当前前缀内的相对链接。只重建整个前缀会再次带入相同问题，不能算根因修复。

### 坑 2：缺少 LocalLow 会触发 cryptnet 的 -1/NULL 错误处理缺陷

修完 DLL 链接后，主进程进入 Chromium 初始化，随后在证书处理阶段崩溃。

转储中的关键调用链：

`cryptnet → ucrtbase!fread → ntdll!RtlEnterCriticalSection`

Wine 8.16 上游代码中，`open_cached_revocation_file()` 在 `SHGetKnownFolderPath(FOLDERID_LocalAppDataLow)` 失败时返回 `INVALID_HANDLE_VALUE (-1)`，但调用方只检查 `NULL`：

- `fread(..., file)` 收到 `FILE * = -1`；
- ucrtbase 的文件锁包装给指针加 `0x30`；
- 最终 `RtlEnterCriticalSection` 读到地址 `0x4f`，产生访问违例。

本次前缀里注册表有 LocalAppDataLow 路径，但当前用户目录下没有实际目录。创建：

`drive_c/users/ubuntu/AppData/LocalLow`

后，证书缓存目录能够正常生成，不再发生这一崩溃。

**不要通过全局替换 Wine 的 cryptnet.dll 来掩盖它。目录缺失才是本机触发条件。**

### 坑 3：全局 gf 图形环境会让豆包卡住或反复崩 GPU 子进程

系统环境里有：

- `__GLX_VENDOR_LIBRARY_NAME=gf`
- `LIBGL_ALWAYS_SOFTWARE=0`
- `VK_ICD_FILENAMES=...gf...`

豆包使用 ANGLE/D3D11，再经 WineD3D 到宿主 GL。默认 gf 路径下出现过：

- 主进程长时间存活但没有窗口；
- `skia_output_surface_impl.cc` FATAL；
- GPU 子进程反复退出；
- 主进程随后异常结束。

仅对豆包进程设置以下环境后，旧版能够显示窗口：

- `__GLX_VENDOR_LIBRARY_NAME=mesa`
- `LIBGL_ALWAYS_SOFTWARE=1`
- `GBM_BACKEND=dri`
- `VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json`

注意：直接把 `--disable-gpu` 传给豆包并不适合当前版本，测试中主窗口没有出现。已验证可用的是**原启动器 + Mesa 环境，让客户端自行进入软件回退**，而不是强行禁用整个 GPU 路径。

这些变量只能注入豆包进程，不能污染 Steam、Electron 或其他 Wine 应用。

### 坑 4：桌面入口的裸 `--uri` 会进入非主窗口流程

原桌面项为：

`Exec=".../run.sh" --uri %F`

普通点击、没有文件参数时仍会留下裸 `--uri`。客户端随后进入修复/更新流程，而不是正常主窗口。

当前 `run-doubao-wine.sh` 只过滤“单独一个 `--uri`”的情况，真实 URI 仍原样传递。

### 坑 5：降级不只是替换 app 目录（历史做法，已废弃）

新版升级后再恢复旧版，直接复制旧程序仍可能崩溃或进入“豆包异常检测”。还需要处理：

- `User Data/Last Version`：从 `147.0.7727.149` 恢复为 `129.0.6668.79`。
- 新版生成的 `ShaderCache`、`GrShaderCache`、`GPUCache`、Dawn 和 Chromium 缓存。
- `repair/record.json` 中的 `running_exception`，否则启动器会进入异常检测。
- `update_data` 中已下载的新版本。

不能只删整个 `User Data`，否则会无谓丢失账号和聊天数据。

### 坑 6：修复器会把“禁用 update.exe”当成文件缺失（历史做法，已废弃）

曾将 `update.exe` 改名以避免自动更新，结果：

`User Data/repair/missing_files` 中出现 `update.exe`，客户端进入修复流程。

所以更新器文件本身必须保留。后来测试了保留有效 PE、将入口改成 `xor eax,eax; ret` 的方式，但这仍不是长期可靠的版本锁定方案，见坑 7。

### 坑 7：版本屏蔽配置和 patch 更新器都不能保证永不升级（历史做法，已废弃）

实测以下配置不能阻止 2.32.6 再次安装：

`vulcan_update_block_versions = [ "2.32.6" ]`

而仅 patch `Program Files/Doubao/app/update.exe` 也不够：旧客户端会从下面的目录重新复制原始更新器覆盖它：

`User Data/update_data/Doubao_update/update.exe`

日志明确出现：

`Copy update.exe from ... update_data/Doubao_update ...`

因此旧版方案只能理解为**临时兼容方案**。按现行约定，这些回退/锁版本手段**一律不再使用**，
修复脚本已清理为纯前缀层修复。迁移到能运行新版的 Wine 才是根治方向。

### 坑 8：新版在 Deepin Wine 8.16 下缺少 ntdll API

将前缀切到 Windows 11 后，系统构建号已从 18362 变为 22000，但 2.32.6 仍立即报：

`FATAL:base\win\win_util.cc(544) Check failed: . : 找不到过程。 (0x7F)`

豆包使用的定制 Chromium 与上游行号不完全一致，不能直接拿上游第 544 行机械对号入座；但同文件中有明确的候选：

- `GetProcAddress(ntdll, "RtlGetDeviceFamilyInfoEnum")`
- 随后 `PCHECK(get_device_family_info_fn)`，找不到函数就触发检查失败。

实测导出：

| 运行时 | `RtlGetDeviceFamilyInfoEnum` |
|---|---|
| Deepin Wine 8.16 | **没有导出** |
| Proton 11 / Wine 11 | **有导出**（RVA `0x27510`） |

这是优先尝试 Wine 11 的直接技术依据，而不是单纯“版本越新越好”的猜测。

## 4. 为什么优先考虑 Proton 11 对应的 Wine 11

### 4.1 已有优势

1. 已具备新版需要的 `RtlGetDeviceFamilyInfoEnum`。
2. 本机已有 Proton 11.0，`wine --version` 为 `wine-11.0`。
3. 项目已经积累 Proton 11 的 WoW64、宿主直跑、gf Vulkan、DXVK 和低地址映射经验。
4. 豆包是 64 位程序，不涉及《随从大师》那种 32 位进程接不住高地址显存映射的问题，不应套用那些低位重映射 hack。
5. 可给新版单独前缀，不破坏旧版回退路径。

### 4.2 已经踩过的 Proton 直跑坑

直接执行 `Proton 11.0/files/bin/wine` 时，不能只给 `WINEPREFIX`：

- 缺少 `WINEDLLPATH` 会报 `libvkd3d-1.dll`、`libvkd3d-shader-1.dll`、`libvkd3d-utils-1.dll` 找不到。
- `LD_LIBRARY_PATH` 也要包含 Proton 自带宿主库目录。
- Wine 拒绝在 `/tmp` 下创建前缀，因为 `/tmp` 不属于当前用户。
- 原生 Proton 前缀的 Windows 用户可能是 `steamuser`，不要假设一定是 `ubuntu`。

Proton 自己的 `proton` 脚本使用：

- `WINEDLLPATH=files/lib/vkd3d:files/lib/wine`
- `LD_LIBRARY_PATH` 包含 `x86_64-linux-gnu`、`i386-linux-gnu` 等目录。

### 4.3 实际进展（2026-11-07 → 2026-11-08 已闭环）

在 `~/.deepinwine/doubao-wine11` 前缀中用 Proton 11.0 启动官方 2.32.6，实测：

| 检查项 | 结果 |
|---|---|
| `RtlGetDeviceFamilyInfoEnum` | **通过**（日志出现 `fixme:ntdll:RtlGetDeviceFamilyInfoEnum ... stub`，不再是 FATAL） |
| 进程存活 | browser / crashpad-handler / gpu-process / 2×renderer / network service / storage service 全部在跑 |
| FATAL / `CHECK failed` | 无 |
| GPU 进程异常退出 | 无（1.56.6 在 Deepin Wine 下会反复 `-2147483645` 退出） |
| 网络 | winsock 探针对 `www.doubao.com` 的 IPv4/IPv6 **全部 `CONNECT OK`**；WinINet HTTPS 200 |
| 窗口（旧会话裸 wine） | 「豆包」1200x800 与一个无标题 1440x704 窗口均 `IsUnMapped`；强制 map 后内容纯黑 |
| 窗口（旧会话容器） | **`IsViewable`，546 色真实界面，主进程 65 秒以上存活** |
| 窗口（07:50 重启后的新会话） | 裸 wine 与容器**都** `IsViewable`（405–546 色），全新空 `User Data` 亦如此 |

结论：Wine 11 早已走通 API 兼容段，黑屏既不是 Wine API 缺失、网络、profile 状态，
也不是渲染后端，而是**旧宿主图形会话状态**（具体项未定论）让裸 wine 建不出可见窗口；
Steam Runtime 容器提供隔离 Mesa/GL 栈，在旧会话里是唯一成功路径，故设为稳健默认。
重启后裸 wine 也可运行，详见 2.1.1 的会话对照表。

排查方法论沉淀（避免重走弯路）：

- **运行时证据优先于静态符号表**。`objdump -T ntdll.so | grep RtlGetDeviceFamilyInfo`
  在 Proton 的 `ntdll.so` 上查不到导出（该 so 只导出 11 个 Rtl 符号），
  但运行时确实存在（stub 形式），只看符号表会得出「Wine 11 也没有该 API」的错误结论。
- **进程类型识别不要用一次性的 `ps | grep -oE`**，容易漏掉 `--utility-sub-type=` 参数。
  逐秒轮询 `/proc/<pid>/cmdline` 才看清 network service 与 storage service 实际存在。
- **抓图用 `xwd -id <窗口ID>`**，Wayland 下 `ffmpeg x11grab` 抓 root 全黑；
  `import`/`scrot`/`grim` 均不存在。未映射窗口 `xwd` 会报 `BadMatch`（X_GetImage 失败）。
- **Wine 网络探针自己编译**：`wine cmd /c curl.exe`、`certutil -urlcache`、`powershell`
  在 Wine 下都是 stub，不可用。用 `i686-w64-mingw32-gcc -lws2_32` 编译 32 位 PE，
  `getaddrinfo` + `connect` 逐地址测试才有效。
- `WSALookupServiceBegin failed with: 8` 在新旧版都出现，是 Chromium 在 Wine 下的正常现象，非根因。

## 5. Wine 11 启动环境（已落地为脚本）

启动器：`doubao/run-doubao-wine11.sh`。**第一件事是把自身包进 Steam Runtime 容器**
（隔离宿主图形栈，避免旧会话状态下建窗失败；干净会话下裸 wine 也能跑，
但容器是更稳健的默认）。容器里的环境变量才是与 Proton 自带 `proton` 脚本保持一致的那一套：

```bash
RT="$HOME/.local/share/Steam/steamapps/common/SteamLinuxRuntime_sniper/run-in-sniper"
exec env DOUBAO_STEAM_RUNTIME_CHILD=1 "$RT" -- bash "$SELF" "$@"
```

`DOUBAO_STEAM_RUNTIME_CHILD` 防止容器内二次包装；找不到 sniper 时回退 soldier，
两者都没有就明确报错退出，不静默改变启动路径。
`DOUBAO_NO_STEAM_RUNTIME=1` 走轻量裸 wine 路径（干净图形会话下可用，旧污染会话下会黑屏）。

容器内实际生效的变量：

```bash
PROTON="$HOME/.local/share/Steam/steamapps/common/Proton 11.0"
export WINEPREFIX="$HOME/.deepinwine/doubao-wine11"

export WINEDLLPATH="$PROTON/files/lib/vkd3d:$PROTON/files/lib/wine"
export LD_LIBRARY_PATH="$PROTON/files/lib/x86_64-linux-gnu:$PROTON/files/lib/i386-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export WINEDLLOVERRIDES='mscoree,mshtml='

# 先稳定显示，再单独评估 gf 硬件加速。
export __GLX_VENDOR_LIBRARY_NAME=mesa
export LIBGL_ALWAYS_SOFTWARE=1
export GBM_BACKEND=dri
export VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json
export WINE_D3D_CONFIG='renderer=gl'
```

脚本会自动 `find` 出实际落点的 `Doubao.exe`，不预设 Windows 用户名。

### 5.1 安装 / 升级入口

```bash
./doubao/install-doubao.sh                    # 自动找仓库 doubao/ 与 ~/下载 里最新的安装器
./doubao/install-doubao.sh /路径/安装器.exe   # 显式指定
DOUBAO_WINE11_PREFIX="$HOME/.deepinwine/doubao-wine11" ./doubao/install-doubao.sh
```

脚本做的事：先 curl 预查官方下发版本 → 只杀本前缀的 Wine 进程 → 拉起安装器并
自动点「立即安装」（`/S` 不静默，必须点）→ 等安装日志 `_OnInstallSuccess` →
收尾杀掉安装器自动拉起的实例（安装器是裸 wine 拉起，会黑屏）→ 跑一次
`fix-doubao-wine.sh` 与 `fix-doubao-desktop.sh`。正式启动一律走套了容器的
`run-doubao-wine11.sh`。
依赖 `xdotool` / `xprop` / `python3-xlib` / `Pillow`；缺依赖时会退成“请人工点按钮”，
脚本继续等日志结果。安装器日志落在 `User Data/sdk_storage/log/Doubao-installer.log`，
完成与否以它为准，不看版本号是否变化（同版本重装时版本号不变）。

安装后要先用 `find` 确认实际落点：

```bash
find "$WINEPREFIX/drive_c/users" -path '*/Doubao/Application/app/Doubao.exe' -print
```

首次启动建议带 `--enable-logging=stderr`，并单独记录日志。
`fix-doubao-wine.sh` 已清理为**纯前缀层修复**（WinSxS 链接 + LocalLow 目录），
不再包含回退、锁版本与更新器禁用逻辑，可以安全地在 Wine 11 前缀上跑：

```bash
DOUBAO_WINE_PREFIX="$HOME/.deepinwine/doubao-wine11" ./doubao/fix-doubao-wine.sh
```

## 6. Electron 路线评估

### 6.1 全量重打包 Windows 客户端：不建议

从安装目录看，豆包包含：

- 约 308 MB 的 `Doubao.dll`；
- `doubao_elf.dll`、`logifier_retrieval.dll`、`ffmpeg.dll`；
- `dxcompiler.dll`、`cudnn_graph64_9.dll`；
- RTCSDK、模型、屏幕共享、推送、全局快捷键等原生模块；
- `local_webcontents` 中的网页资源。

这意味着它不能只按“HTML/CSS/JS + Electron”处理。即使网页资源能够抽出，也仍需替换或重写：

1. 登录态、设备 ID、加密存储；
2. WebSocket/推送与遥测；
3. 音视频、RTC、模型和本地推理；
4. 全局快捷键、剪贴板、文件拖拽、屏幕共享；
5. 自更新、修复器、协议注册和桌面集成。

这些恰好是最难维护、最容易随服务端变化失效的部分。最终会变成长期逆向维护一个非官方客户端，而不是一次性重打包。

### 6.2 做一个加载官方网页版的 Electron 外壳：可行但功能受限

这条路线不碰 Windows 原生 DLL，客户端只负责显示官方 Web 页面，服务端功能由网页本身提供。优点是：

- 使用原生 Linux Chromium，不再受 Windows API 缺失影响；
- 可复用项目 `electron/` 中已验证的 X11、sandbox、GPU 合成和 gf 加速经验；
- 升级主要发生在网页服务端，本地不用追 Windows 安装器。

限制：

- 不保证拥有 Windows 客户端的模型、RTC、全局快捷键和系统整合能力；
- 登录方式、文件上传下载和通知要逐项验证；
- 它是新的 Web 外壳，不是官方桌面客户端的等价迁移。

如果走这条路，默认先用软件渲染；稳定后再参考 `electron/use-electron-gf.sh` 评估 gf，不要第一天就叠加硬件加速变量。

## 7. 实施顺序（1–2 已完成）

1. ✅ **修黑屏**：旧宿主图形会话下裸 wine 不出窗口；启动器默认套 Steam Runtime
   容器隔离 Mesa/GL 栈后稳定出界面（07:50 重启后的新会话里裸 wine 也能跑，见 2.1.1）。
2. ✅ **确认页面真实内容**：窗口抓图 546 色（白/浅灰为主），日志出现
   `chrome://doubao-chat/`、`chrome://doubao-launcher/`，不是白屏或纯黑。
3. **逐项验收**：登录、文字对话、文件上传、音频/RTC（需用户交互）；重复启动已 3/3 通过。
4. ✅ **桌面入口**：`~/Desktop/doubao-wine.desktop` 已指向
   `run-doubao-wine11.sh` 并标记 trusted（见 2.4）。
5. **最后才评估 gf 加速**：容器路线先用 Mesa 软件渲染稳定，再单独一轮评估 gf。

账号数据不要直接整目录覆盖。日志里已出现 `AppBoundEncryption` 错误，跨 Wine/前缀迁移后应有重新登录的心理准备。

## 8. 验收标准（官方最新版本）

新版至少满足以下条件，才算修完（2026-11-08 实测进度）：

- ✅ 60 秒内出现主窗口（容器内约 20 秒内 `IsViewable`）；
- ⬜ `CrBrowserMain` 连续存活 10 分钟以上（已验证 65 秒以上，长时验证待做）；
- ✅ 没有新的 browser 主进程 Crashpad 转储；
- ⬜ 登录和文字对话可用（需用户交互确认）；
- ✅ 连续 3 次从 `~/Desktop/doubao-wine.desktop` 启动均正常：三次都
  `map=2`、546–547 色、`main_browser=1`、`pressure_vessel` 在跑；
- ✅ 版本跟随官方（安装器渠道或客户端自更新），不需要禁用更新器或屏蔽版本号才能运行。
- ✅ 安装/升级链路已跑通：`install.py` **菜单 26** 实跑一轮完整安装，
  自动定位并点中「立即安装」（窗口 88080391，按钮中心 358,309），
  安装日志出现 `_OnInstallSuccess`，安装结果：成功。
- ✅ 托盘菜单：**已彻底定案**（关一次即永久失效，责任在客户端，唯一恢复是重启），
  `--fix` 闭环实测通过；详见 1.1 / 2.6.3。此项**不再作为待排查项**。

2026-11-08 抓图人工确认：窗口内是真实豆包主界面（「有什么我能帮你的吗？」、
左侧「新工作任务 / 新对话 / API 服务 / 更多」、左下「登录」、底部输入框与
对话 / 项目 / PPT 生成 / 图像生成 / 视频生成入口），不是白屏或崩溃页。

若后续仍失败：

- 用对应 Proton 的 `wineserver -k` 只关闭 Wine 11 测试前缀；
- 不做崩溃后自动重启，不用 supervisor 掩盖主进程退出；
- 先查 Steam Runtime 容器是否还在（Steam 更新会替换 `sniper_platform_*` 目录，
  顶层 `run-in-sniper` 会随之重生成），再按 `~/.local/share/doubao-wine11/launch.log`
  定位下一层问题；
- **不退到旧版客户端**；若要换方向，只能整体转向功能明确的网页版 Electron 外壳。

## 9. 相关文件

- `doubao/install-doubao.sh`：**官方在线安装器的安装/升级入口**（对应 `install.py` 菜单 **26**）；
  预查安装器渠道下发版本、关掉同前缀进程（按 `/proc/<pid>/environ` 精确匹配 `WINEPREFIX`）、
  自动点「立即安装」（枚举窗口 + 等 `IsViewable` + 重试抓图，XTEST 真实点击）、
  以 `_OnInstallSuccess` 为完成判据，装完自动跑前缀层修复与桌面入口修复。
- `doubao/fix-doubao-desktop.sh`：**入口与异常状态修复**（幂等、`--status`、`--uninstall`）：
  统一四个 `.desktop` 的 Exec 到 Wine 11、去 shebang、打 trusted 标记、移走 `豆包.lnk`、
  备份并清零旧前缀 `running_exception`。
- `doubao/quit-doubao.sh`：**退出 / 唤回 / 状态**（见 2.6）。点 X 只是隐入通知栏，
  而本机通知栏不可用，所以用 `--quit` 真正退出、`--show` 唤回主窗口、
  `--status` 区分「隐入通知栏」与「崩溃」；只作用于 `doubao-wine11` 前缀，无守护。
- `doubao/tray-doctor.sh`：**托盘菜单诊断与修复**（见 1.1 / 2.6.3）：`--status` 只读、
  `--probe` 判定好坏态、`--fix` 重启客户端复位菜单。
- `doubao/tray-probe.py`：托盘菜单探针（用 `XSendEvent` 模拟 XEmbed 转发，
  轮询 165x284 菜单窗口出现；**绝不主动关闭菜单**，因为关闭正是触发坏态的动作）。
  退出码 `0`=好 / `1`=坏 / `3`=没找到托盘图标。
- `doubao/run-doubao-wine11.sh`：**新版 2.32.6 的主线启动器**，自动套
  `SteamLinuxRuntime_sniper` 容器；启动日志落在 `~/.local/share/doubao-wine11/launch.log`。
  `DOUBAO_NO_STEAM_RUNTIME=1` 可回到裸 wine 做对照诊断。
- `doubao/fix-doubao-wine.sh`：与版本无关的前缀层修复（WinSxS 软链接、LocalLow 目录）；
  不再做版本回退、不锁版本、不禁用更新器。
- `doubao/run-doubao-wine.sh`：**历史入口已停用**，过滤裸 `--uri` 后安全转发到
  `run-doubao-wine11.sh`；1.56.6 历史对照改为直接用 `deepin-wine8-stable` 手动启动。
- `.codebuddy/rules/豆包客户端版本约定.md`：只认官方最新版、不做旧版回退的项目约定。
- `electron/README.md`、`electron/use-electron-gf.sh`：原生 Electron/Chromium 在 7G100 上的现有适配经验。
- Proton 的 `proton` 脚本：官方运行库环境变量来源。
