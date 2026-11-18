# 11 对战平台（魔兽争霸 III / Dota）安装包

> 至砺算：安装链路与显卡无关。本目录用 Proton 11.0 的 wine-11.0 在独立前缀里
> 跑官网 Inno Setup 静默安装器；只验收安装产物，不做崩溃后自动重启/守护。

## 1. 厂商专有二进制（不入库）

| 项 | 内容 |
| --- | --- |
| **是什么** | `5211Setup_3.0.0.68.exe`：11 平台官网（5211game.com）完整安装包，Inno Setup 5.4.2 自解压 PE32（约 235 MB，`file` 报 `PE32 executable (GUI) Intel 80386`）。内含主程序 `11loader.exe`、CEF 组件与 OBS 录制器。 |
| **为何不能入库** | 厂商专有二进制、体积大、升级会改文件名；按 `.codebuddy/rules/仓库组织约定.md` 二，用 `.gitignore` 规则 `11/5211Setup_*.exe` 排除，仓库只保留脚本与文档。文件仍留在本目录供运行时安装，不删除。 |
| **从哪获取** | 11 平台官网 <https://www.5211game.com/> 下载最新安装包，放到本目录、`~/下载` 或 `~/Downloads`。本机这份 SHA256：`1df075bcb4ddaa8eb352e56e0da8f0214a012e56407616b59a313e2d72ed4090`（245828880 字节）。 |

## 2. 安装

```bash
cd ~/project/lisuan
chmod +x 11/install-11duizhan.sh
./11/install-11duizhan.sh                 # 自动找安装包 → 独立前缀 → Inno /VERYSILENT 静默安装 → 校验主程序
./11/install-11duizhan.sh --status        # 只读体检（不启动平台）
./11/install-11duizhan.sh --help
```

- 前缀：`~/.deepinwine/11duizhan-wine11`（与 KK、80 对战平台 Prefix 隔离）。
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

## 3. 安装产物与验收状态（2026-11-17 实测）

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
- 补丁在多轮运行后持久（应用自更新未回退栈字段）；`E11_STACK_RESERVE` /
  `E11_STACK_COMMIT` 可覆盖默认值。
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
