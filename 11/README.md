# 11 对战平台（魔兽争霸 III / Dota）安装包

> 至砺算：安装链路与显卡无关。本目录用 Proton 11.0 的 wine-11.0 在独立前缀里
> 跑官网 Inno Setup 静默安装器；只验收安装产物，不做崩溃后自动重启/守护。

## 0. 现行推荐方案：官网正式版 2.6.3.48（2026-11-19 实测）

**结论（2026-11-19 最终）：官网正式版 `2.6.3.48`（`11Loader.exe` + `bins/11Client.exe`）
已可登录并进入大厅。** 根因是 Wine 前缀的系统字体为 Tahoma、本机没有该字体：CEF 105.3.38
的 `SystemFonts::Initialize()` 在 `AddFont()` 期间创建 SkTypeface 失败，回退调用
`GetDefaultSystemFont()` 时 `is_initialized_` 尚未置位，于是再次 `Initialize()`，无限重入；
每轮取 DC/建字体，先耗尽 GDI 表，再触发 CEF 断言或栈溢出。修复：`./11/fix-11-fonts.sh`
把五类系统字体改为本机存在的 Arial（可用 `E11_SYSTEM_FONT` 指定），幂等、备份、可卸载。
实测登录后主窗口 `1243×720` 完整渲染、服务器测速完成且可确认，GDI/栈/异常均为 0。

- 获取：`https://go.5211game.com/?id=10` →
  `https://download.5211game.com/11/download/5211Setup_2.6.3.48.exe`
  （需带 `Referer: https://www.5211game.com/`，否则 403）。
  本机文件 `11/5211Setup_2.6.3.48.exe`，SHA256
  `20066c97cfcf25b80cbf7b48d89f2d1508a11783978a3a201bd6fabff46a23b8`，503283232 字节。
- 用法：

```bash
cd ~/project/lisuan
./11/install-11duizhan.sh 11/5211Setup_2.6.3.48.exe   # 默认新前缀 ~/.deepinwine/11duizhan-263
./11/install-11duizhan.sh --status
./11/fix-11-desktop.sh                                # 从 11Loader.exe 提取图标并生成桌面入口
./11/fix-11-fonts.sh                                  # 修复 Wine 系统字体（登录后卡死的根因）
./11/fix-11-fonts.sh --status                         # 只读体检
./11/run-11duizhan.sh                                 # 单次前台启动，无守护
```

- **2026-11-19 取证链（历史；已由系统字体修复解决）**。点击登录后应用日志停在
  `CLoginLogic::onLoginSucceed` / `DoWebPageLogin: 1`，Wine stderr 出现
  `stack overflow ... addr 0x4836d2`。反汇编确认 `0x4836b0` 每帧 `sub $0xfc4,%esp`，
  但它是注册给异常处理链的回调（内部加载 `dbghelp`），不是普通业务函数；把 PE 栈从
  4MB 加到 16MB/64MB 只能把溢出推迟，进程存活更久后仍在 64MB 栈上溢出。
  `11/fix-11-stack.sh` 保留为诊断/缓解工具，但不是修复。
- **GDI 泄漏调用点已抓到**：新增 `11/redirect-11-gdi-iat.sh` 把 `11Client.exe` 和
  `bins/cef/libcef.dll` 的 `USER32.dll` 导入名等长替换为 `e11gdi.dll`，让
  `11/user32-gdi-wrapper.c` 进入进程。日志（`11/_work/gdi-iat-both-*`）显示：
  `11Client.exe` 只有 1 次 `GetDC` 且成对释放；`libcef.dll` 的 `GetDC(NULL)` 固定来自
  `+0x300060a`，约 2.1 万次，`ReleaseDC` 固定来自 `+0x30087ed`，约 1.05 万次，
  且 HDC 不配对。这解释了 Wine GDI 句柄表耗尽和 `libcef.dll` `0x80000003`。
  包装层的屏幕 DC 缓存可把 GDI 耗尽降为 0，但登录仍停在“正在验证账号…”并最终栈溢出；
  提前回收 DC 同样卡在验证页。两者均为历史证伪路径，真正修复见 `11/fix-11-fonts.sh`。
- **minidump 最终证据**：用 `winedbg <.../11client_*.dmp>` 重载崩溃转储，调用栈不是普通
  业务递归，而是 `libcef.dll` 内一个固定回调环反复压栈：
  `+0x30096f8 → +0x3bbb771 → +0x3bbbc70 → +0x3bbbb48 → +0x3bbcd80 →
  +0x3009c90 → +0x3009dbe → +0x300981c → +0x30096f8`，崩溃点是断言
  `+0x3adc9dd` 的 `int3`。这解释了为何加大栈、缓存 DC 都只能延后崩溃；
  需要在 CEF/Wine 交互层打断该重入，而不是继续加栈。
- **定点二进制 A/B 已证伪**：对 `libcef.dll` 的 `+0x30096f8` thunk 试过返回 0 / 1 / `EDI`，
  并把下游解引用调用 `+0x3bbb773` NOP 掉。GDI 耗尽和栈溢出确实都降为 0，但登录仍停在
  `onLoginSucceed` 后，崩溃变成 CEF 的 `0xc0000005` 空指针/非法地址访问或转移到别处；
  该 thunk 是对象构造链的一环，不能单点跳过。`libcef.dll` 已按备份还原。
- 以上 IAT/栈/崩溃回调实验均可通过 `./11/redirect-11-gdi-iat.sh --uninstall`、
  `./11/fix-11-stack.sh --uninstall` 还原；本机当前已全部还原为原始厂商二进制。
  最终修复是 `./11/fix-11-fonts.sh` 改 Wine 系统字体，不动任何厂商二进制。
  禁止看门狗/自动重启；不把绕开包装成修复。
- 主程序文件名大小写与 3.0 不同：正式版是 `11Loader.exe`，公测版是 `11loader.exe`；
  安装/启动/桌面脚本已同时兼容两者。
- 启动阶段验收（未登录，默认启动器 + `+err,+gdi,+seh`，单次运行）：
  `wmctrl -lpG` 在 t=10s / t=25s 均见 `11对战平台` 窗口；`stack overflow` 0、
  `out of GDI object handles` 0、`Unhandled exception` 0；GDI 句柄峰值 **142**。
- 登录后验收（2026-11-19，Arial 系统字体）：窗口变为 `11对战平台 - 2.6.3.48`
  （`1243×720`），服务器测速完成并可确认，大厅完整渲染；Wine 日志
  `stack overflow` 0、`out of GDI object handles` 0、`0x80000003` 0。
  证据在 `11/_work/font-fix-arial-20261119-1925/`（`.gitignore` 已排除 `_work/`）。
- 对照结论：纯 32 位前缀、winetricks 安装 `mfc42/msxml3/vcrun2019` 都没能消除 3.0
  公测版的 GDI 泄漏；真正差异是**正式版客户端本身**。旧 3.0 内容保留在下文作为历史排障记录。

## 1. 厂商专有二进制（不入库，含 3.0 公测版历史）

| 项 | 内容 |
| --- | --- |
| **推荐（正式版）** | `5211Setup_2.6.3.48.exe`：官网客户端 2.0 正式版，Inno Setup 自解压 PE32，503283232 字节；主程序 `11Loader.exe` + `bins/11Client.exe`。见第 0 节，现行默认。 |
| **是什么（历史）** | `5211Setup_3.0.0.68.exe`：11 平台官网 3.0 公测版完整安装包，Inno Setup 5.4.2 自解压 PE32（约 235 MB）。内含主程序 `11loader.exe`、CEF 组件与 OBS 录制器；该版本有栈溢出、自更新与 GDI 泄漏问题。 |
| **为何不能入库** | 厂商专有二进制、体积大、升级会改文件名；按 `.codebuddy/rules/仓库组织约定.md` 二，用 `.gitignore` 规则 `11/5211Setup_*.exe` 排除，仓库只保留脚本与文档。文件仍留在本目录供运行时安装，不删除。 |
| **从哪获取** | 11 平台官网 <https://www.5211game.com/>：正式版走 `go.5211game.com/?id=10`（本机 SHA256 见第 0 节）；3.0 公测版走 `?id=5171`，本机这份 SHA256：`1df075bcb4ddaa8eb352e56e0da8f0214a012e56407616b59a313e2d72ed4090`（245828880 字节）。放到本目录、`~/下载` 或 `~/Downloads`。 |

## 2. 安装

```bash
cd ~/project/lisuan
chmod +x 11/install-11duizhan.sh
./11/install-11duizhan.sh                 # 自动找安装包 → 独立前缀 → Inno /VERYSILENT 静默安装 → 校验主程序
./11/install-11duizhan.sh --status        # 只读体检（不启动平台）
./11/install-11duizhan.sh --help
```

- 默认前缀：`~/.deepinwine/11duizhan-263`（正式版 2.6.3.48，与 KK、80 对战平台隔离）。
  历史 3.0 公测版前缀 `~/.deepinwine/11duizhan-wine11` 需显式用 `E11_WINE11_PREFIX` 指定。
  Proton 11.0 的 wine，只在 wine 进程树上设置 `WINEPREFIX` / `WINEDLLPATH` /
  `LD_LIBRARY_PATH` / `WINEDLLOVERRIDES=mscoree,mshtml=`，不污染全局 shell。
- **安装包是 Inno Setup 5.4.2，不是 NSIS**：静默参数为
  `/VERYSILENT /SUPPRESSMSGBOXES /NORESTART /DIR=C:\11duizhan /LOG=...`，
  不能用 80 平台脚本的 `/S`。
- 安装器查找顺序：命令行参数 → `11/` → `~/下载` → `~/Downloads`。
- 安装目录默认 `C:\11duizhan`（前缀内 `drive_c/11duizhan`），可用
  `E11_INSTALL_DIR` 覆盖（相对 `drive_c` 的路径）。
- 其它环境变量：`E11_PROTON`、`E11_WINE11_PREFIX`、`E11_INSTALL_TIMEOUT`、
  `E11_INSTALL_LOG`、`E11_WINEDEBUG`。
- 安装器返回码非 0（含超时 124）均明确报错并保留日志，**不自动重试**；
  已装时只体检不重装。

## 3. 3.0 公测版安装产物与验收状态（2026-11-17 实测，历史）

| 项 | 结果 |
| --- | --- |
| Inno 安装日志 | `Installation process succeeded.`，`Need to restart Windows? No` |
| 安装目录 | `~/.deepinwine/11duizhan-wine11/drive_c/11duizhan`（约 733 MB） |
| 主程序 | `11loader.exe`，PE32，2311848 字节 |
| 卸载器 | `unins000.exe`（Inno Setup 标准卸载器） |
| 其它组件 | `yybins/YYClient.exe`、CEF 渲染器、Config / VerComp.dat |
| 安装脚本 | `./11/install-11duizhan.sh --status` 全项 `[OK]` |

- 本目录负责安装与启动修复；平台登录 / 魔兽3 联机尚未在砺算 7G100 上验收。
- 启动需先运行 `./11/fix-11-stack.sh`（修复 YYClient.exe 栈溢出，见第 7 节）；
  桌面入口由 `./11/fix-11-desktop.sh` 生成，指向隔离启动器 `run-11duizhan.sh`。
- 栈修复后仍有 GDI 句柄泄漏导致运行约 12 秒退出（见第 8 节，尚未修复）。
- 崩溃处理：不加剧、不掩盖。先采集日志 / dump / journal 再定位根因，不加看门狗或自动重启
  （见 `.codebuddy/rules/崩溃处理约定.md`）。
- 本目录未接入 `install.py` 菜单，与 `80/` 的处理方式一致。

## 4. 桌面图标与启动入口（2026-11-17 实测）

安装器只在 Wine 前缀里留了 Windows `.lnk`，Linux 桌面没有图标；用下面的脚本
从 `11loader.exe` 的 PE 资源提取官方图标（复用 `dreamina/extract-icon.py`），
写入 `~/.local/share/icons/hicolor`，并生成指向隔离启动器的桌面项。

```bash
cd ~/project/lisuan
./11/fix-11-desktop.sh              # 提取官方图标 + 生成桌面入口（幂等）
./11/fix-11-desktop.sh --status     # 只读体检
./11/fix-11-desktop.sh --uninstall  # 恢复备份、删除生成的图标与入口
```

- 入口位置：`~/.local/share/applications/11duizhan.desktop`、XDG 桌面目录
  （本机为 `~/桌面`）以及 `~/Desktop`，均已标记 `trusted`。
- 图标名 `11duizhan`，16/20/24/32/40/48/60/64/72/80/96/128/256 共 13 个尺寸；
  **厂商图标产物不入库**，只写在 `~/.local/share/icons`。
- 桌面项 `Exec` 指向 `11/run-11duizhan.sh`：前台单次启动、隔离前缀、
  参数透传、退出码原样透传；不做存活监控或崩溃后自动重启
  （见 `.codebuddy/rules/崩溃处理约定.md`）。
- 覆盖已有 `.desktop` 前会先备份到
  `~/.local/share/11duizhan-wine11/desktop-backups/`；`--uninstall` 可恢复。
- 平台 GUI 启动 / 登录尚未验收；图标生成成功不代表联机可用。

## 5. 桌面图标点击后“无法启动”的排查（2026-11-17）

**第一层根因已修复：Depth 32/ARGB 窗口在 Wayland 合成下完全不可见。**
证据：点击图标 / 直接启动后 `wmctrl -l` 能看到 `11对战平台` 窗口，
`xwininfo` 显示 `Map State: IsViewable`、尺寸 1168×533，但 Mutter ScreenCast
合成图里没有窗口；`xwininfo` 显示该窗口 `Depth: 32`。
`run-11duizhan.sh` 现在默认在 wine 进程树注入 `XLIB_SKIP_ARGB_VISUALS=1`，
窗口变为 `Depth: 24` 且合成图里出现窗口（可用 `E11_SKIP_ARGB_VISUALS=0` 做 A/B）。

**第二层问题仍未解决：窗口出现但客户区全黑，随后异常退出。**
保留的证据（不做自动重启/守护）：

| 证据 | 内容 |
| --- | --- |
| CEF 日志 | `gpu_init.cc(486) Passthrough is not supported, GL is disabled, ANGLE is` |
| Wine 日志 | `err:virtual:virtual_setup_exception stack overflow 3008 bytes addr 0x48de11`（崩溃 RVA ≈ `0x8de11`，主程序 ImageBase `0x400000`，附近代码是 protobuf 序列化循环） |
| 应用状态 | `yybins/log/<时间>/platform.ini`：`normal_exit=0`、`error_code=2`；`uplog.log` 记录 `ErrType=2 /Abnormal exit` |
| 合成图 | `XLIB_SKIP_ARGB_VISUALS=1` 后窗口可见但内容全黑（`/tmp/11-argb.png`、`/tmp/11-fullenv.png`） |

已做消融、**均不能解决黑屏/栈溢出**：移除 `mscoree,mshtml` DLL 覆盖、把主程序 PE
栈保留改 16MB/提交 64KB（已回滚）、Mesa 软件 GL 三件套（`GBM_BACKEND=dri`、
`LIBGL_ALWAYS_SOFTWARE=1`、`__GLX_VENDOR_LIBRARY_NAME=mesa`）、`QT_OPENGL=software`、
CEF `--disable-gpu` / SwiftShader 参数、Wine 虚拟桌面、全新前缀重装、Proton 9。
另：平台下载站 `download.5211game.com` 返回 403，官网 200，但这不是黑屏的直接原因。

排查复现命令（单次启动，不自动恢复）：

```bash
cd ~/project/lisuan
E11_WINEDEBUG='+err+seh' ./11/run-11duizhan.sh        # 日志写到终端
./11/fix-11-desktop.sh --status                       # 图标/入口体检
```

结论：**桌面图标已能显示并真正拉起窗口**。本节当时未定位的“客户区全黑 + 异常退出”
根因已于 2026-11-18 查明并修复（YYClient.exe 栈溢出，见第 7 节）；栈修复后又暴露出
GDI 句柄泄漏这一次生问题（见第 8 节，尚未修复）。不要把“窗口已出现”当成“平台可用”，
也不要加入看门狗/自动重启掩盖退出。

## 6. “开启卡死”复测（2026-11-18）

用户点击图标后 GNOME 弹出“steam_proton 无响应”。现场取证确认这是**应用级阻塞**：

| 证据 | 内容 |
| --- | --- |
| 线程状态 | 主进程 `STAT=Sl`、`WCHAN=anon_pipe_read`；3 秒内 CPU tick 保持 92 不变，不是死循环，而是在等 IPC 管道 |
| 管道 | `/proc/<pid>/fd` 有多对匿名管道且两端都在同一进程；`pgrep -P` 无子进程，说明对端线程已退出或卡住 |
| 应用日志 | 停在 `net game task` / `CMD_AGAIN_REQUEST_CHANNEL`；`platform.ini` 为 `normal_exit=0`、`error_code=2` |
| CEF / Wine | 同第 5 节：`gpu_init.cc` GL 禁用；`stack overflow ... addr 0x48de11` |

本轮新增消融，**均无效**：Wine Windows 版本临时改 Win7（已恢复）、把 `0x48de4b` 的
`je` 改成 `jmp` 跳过 protobuf CHECK 分支（已回滚）、DXVK v3 + llvmpipe ICD、
`strace -ff` 全程跟踪（进程仍在 20 秒内退出，未见阻塞中的 read）、Proton 8.0
新前缀（因 vkd3d/DXGI 加载失败未完成有效对照，临时前缀已删除）。

结论：本节当时的“卡死”实为栈溢出后主线程阻塞在 IPC 管道的余波，根因已查明并修复
（见第 7 节）。`XLIB_SKIP_ARGB_VISUALS=1` 只解决“窗口不可见”；栈补丁解决“序列化递归
溢出”；其后仍有 GDI 句柄泄漏导致退出（见第 8 节）。继续排查不要加看门狗或自动拉起。

## 7. 栈溢出根因定位与修复（2026-11-18，已修复）

**真正的客户端是 `yybins/YYClient.exe`，不是 `11loader.exe`。** `11loader.exe` 只是
拉起器，它创建的客户端进程命令行带 `-lnchwnd=... -qtype=platform`（argv 被清空，
`/proc/<pid>/exe` 指向 wine-preloader，ELF class=1 即 32 位）。第 5/6 节把崩溃 RVA
`0x8de11` 归到 `11loader.exe`（ImageBase `0x400000`）是误判——两个 PE 同 ImageBase，
但崩溃实际发生在 `YYClient.exe` 进程（其主线程栈 `0xe00000-0xf00000` 仅 1MB）。

| 证据 | 内容 |
| --- | --- |
| 崩溃指令 | `YYClient.exe` VMA `0x48de11` 所在函数 `sub $0xf80,%esp`（递归帧约 4KB），是频道序列化递归 |
| 栈配置 | `YYClient.exe` PE `SizeOfStackReserve=0x100000`（1MB）、`SizeOfStackCommit=0x1000` |
| 溢出日志 | `err:virtual:virtual_setup_exception stack overflow 3008 bytes addr 0x48de11 stack 0xe00440 (0xe00000-0xe01000-0xf00000)` |
| 进程取证 | 客户端主线程栈保留区恰为 1MB；`11loader.exe` 改 16MB 后客户端仍溢出（证明崩溃不在拉起器） |

约 250 层递归（1MB / 4KB）即触守护页。第 5 节“改 16MB 无效”的旧结论是因为改错了
文件（改了 `11loader.exe` 而非 `YYClient.exe`）。

**修复**：`./11/fix-11-stack.sh` 把 `YYClient.exe` 与 `11loader.exe` 的 PE 栈保留改
16MB、提交改 64KB（只改可选头两个 DWORD，不动代码段）。

```bash
cd ~/project/lisuan
./11/fix-11-stack.sh              # 备份并打栈补丁（幂等）
./11/fix-11-stack.sh --status     # 只读体检
./11/fix-11-stack.sh --uninstall  # 从备份还原原始 exe
```

- 备份目录：`~/.local/share/11duizhan-wine11/stack-backups/{YYClient,11loader}.exe.orig`。
- 实测：补丁后 `stack overflow` 计数从 1 降为 0，`root.log` 走到
  `case 3 success PostNotifyMsg`（频道序列化完成），越过原 4 秒崩溃点。
- **2026-11-19 新发现：平台自更新会替换 exe 并冲掉栈补丁。** 根目录下
  `gamever.ini` / `Config/yyclientx.xml` 在两次启动间从 `3.0.0.68` 变为
  `3.0.0.79` 又变回 `3.0.0.68`，`YYUpdate.log` 每次都走完
  `Check → Download → Install → Final`。3.0.0.79 的栈溢出仍在
  （`YYClient.exe+0x48e441`，默认栈 1MB），不是新版本修好了。
- `run-11duizhan.sh` 无参数启动默认追加 `-updatemode=0` 冻结当前版本，避免
  更新器来回换版本；`E11_ALLOW_UPDATE=1` 可恢复更新做 A/B。这是**绕过**
  Wine 下失效的厂商更新器，不是修好更新器；真正升级请用官网完整安装包重装。
- `fix-11-stack.sh` 现在按「忽略栈字段的 SHA256」识别版本：发现 exe 变化时把
  旧备份转存为 `.orig.<hash8>`，再备份当前版本，避免 `--uninstall` 错还原旧版本。
  自更新/重装后需重跑本脚本；`E11_STACK_RESERVE` / `E11_STACK_COMMIT` 可覆盖默认值。
- 厂商二进制不入库；补丁只作用于本机前缀内的安装产物。

## 8. GDI 句柄泄漏（栈修复后暴露的次生问题，尚未修复）

栈修复让客户端越过原崩溃点后，运行约 12 秒仍会异常退出
（`platform.ini` `error_code=1` / `Unhandled exception`）。取证确认是 **GDI 句柄表耗尽**：

| 证据 | 内容 |
| --- | --- |
| Wine 日志 | `err:gdi:alloc_gdi_handle out of GDI object handles, expect a crash`（出现 5 次） |
| 句柄 dump | 耗尽时 65536 表中：`NTGDI_OBJ_REGION` 43625 + `NTGDI_OBJ_FONT` 10921 + `NTGDI_OBJ_DC` 10908，均 `deleted=0` |
| 泄漏模式 | 渲染循环每轮创建 1 DC + 1 FONT + 4 REGION，删除 FONT/REGION 但**从不删除 DC**；约 10908 轮 |
| 机制 | Wine 11.0 未实现 Windows 的 `GDIProcessHandleQuota`（默认 10000），句柄表硬上限 65536（`alloc_gdi_handle`），泄漏累积到顶 → 关键对象创建返回 0 → 崩溃 |

**已尝试且证伪的修复**：用 LD_PRELOAD 垫片在内存把 `win32u.so` 的
`alloc_gdi_handle` 上限常量从 65536 改回 10000（模拟 Windows 配额）。补丁确实生效
（i386-unix/win32u.so VMA `0x9fc0c` 的 `add $0x180000,%eax`），但客户端**更早崩溃**
（约 10 秒）——说明应用并不能容忍句柄表满后的 NULL 返回，配额模拟不是正确修复。
该实验垫片已移除，不作为修复交付。

**待查方向**（不加看门狗/自动重启）：
- 泄漏的 DC 是 `NTGDI_OBJ_DC`（窗口/显示 DC，来自 `GetDC`/`CreateDC`，非
  `CreateCompatibleDC` 的 MEMDC）。需确认创建 API 与 Wine 的 DC 缓存/释放行为
  是否与 Windows 不同（Windows 上同代码不泄漏，疑为 Wine DC 缓存差异）。
- GDI 代码在 unix 侧 `i386-unix/win32u.so`（传统 WoW64，客户端 32 位）；
  `NtGdiGetDC`/`NtGdiCreateDC`/`NtGdiReleaseDC` 非动态导出，不能按名 interpose。
- PE 侧 gdi32/user32 wrapper 需放对搜索路径（前缀为 win64，system32 是 64 位 DLL，
  32 位客户端走 syswow64；把 32 位 wrapper 放 system32 会令 64 位进程报 c000007b）。

结论：启动栈溢出（第 7 节）已修复；GDI 句柄泄漏是独立的深层 Wine 兼容问题，
根因证据已保留，修复尚未完成。不得用自动重启掩盖该退出。

2026-11-19 补记：当时取证尚未发现自更新会在 3.0.0.68 ↔ 3.0.0.79 间来回切换；
`run-11duizhan.sh` 现在默认冻结版本。冻结后单次验收 `stack overflow` 与
`Unhandled exception` 计数降为 0，但 `+gdi` 日志仍有 **5 条**
`out of GDI object handles`，句柄峰值 65503/65536，`libcef.dll` 随后 `0x80000003`。
因此 GDI 泄漏在冻结版本上仍然复现，不能宣称已修复。完整证据与失败方向见第 9 节
及 `.codebuddy/rules/11平台启动排障约定.md`。

## 9. 已证伪方向与未完成项（2026-11-19）

| 方向 | 结果 | 结论 |
|---|---|---|
| 把崩溃归给 `11loader.exe`，只改其栈 | 客户端 `YYClient.exe` 仍溢出（`0x48de11` / `0x48e441`） | 已证伪；必须改 YYClient |
| CEF `--disable-gpu` / SwiftShader、`QT_OPENGL=software|desktop`、虚拟桌面、Proton 8、全新前缀、软件 GL 三件套 | 对黑屏/栈溢出/GDI 泄漏均无效 | 不要再作为主攻方向 |
| 把 `win32u.so` 的 GDI 句柄上限从 65536 降到 10000 | 补丁生效但客户端更早崩（约 10 秒） | 已证伪；不是修复，垫片已移除 |
| `LD_PRELOAD` 拦截 `NtUserGetDC` | 0 次命中；Wine syscall dispatcher 用内部函数指针 | 按名 interpose 无效 |
| `user32` PE wrapper + `WINEDLLOVERRIDES=user32=n[,b]` | 只拦到 `11loader.exe`；客户端因 known DLL 仍加载内置 user32；另需 `-static-libgcc` 避免缺 `libgcc_s_dw2-1.dll` | 当前未拿到客户端 GetDC 调用点 |
| gdb attach 已运行进程 | `ptrace_scope` 拒绝 | 改为 gdb 直接启动，但必须忽略 `SIGUSR1/2/CHLD/PIPE/ALRM`；跨 dispatcher 的 `bt` 不可靠 |
| `-updatemode=0` 冻结版本 | 解决反复换 exe/冲栈补丁 | **不修 GDI 泄漏**，冻结后仍耗尽 |

**未完成**：`11/fix-11-gdi.sh`、`11/gen-11-user32-def.py`、`11/user32-gdi-wrapper.c`
目前只是诊断件（`E11_GDI_WRAP=1` 为 A/B 开关），不是修复。下一步是改写
`YYClient.exe` 的导入表：本次解析其 `USER32.dll\0` 位于文件 offset `0x7319ea`，
可等长替换为 `e11gdi.dll\0`（版本变化后必须重新解析导入表），并在 `yybins/` 放
`e11gdi.dll` + `user32_orig.dll`，让包装层真正进入客户端再抓 GetDC 调用者。
`gen-11-user32-def.py` 已支持第三个参数指定库名，`fix-11-gdi.sh` 尚仍生成
`user32.dll` 包装层；IAT 方案未实现，勿误认为已交付。全程禁止看门狗/自动重启。
