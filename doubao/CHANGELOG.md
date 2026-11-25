# doubao 变更记录

> 从仓库根 `CHANGELOG.md` 拆分而来 （2026-11-19）。按时间倒序；历史条 目保留当时的命令与路径。

## 2026.11.25 — 启用显卡加速：gf 7G100 硬件渲染（DXVK→Vulkan）

### 结论
- 启动器新增 `DOUBAO_GPU`：默认 `auto`（检测到 gf 驱动→`hw`，否则→`sw`），
  可显式 `hw`/`sw`。本机默认即硬件加速；`quit-doubao.sh --status` 新增「渲染后端」行。
- hw 路线：**Chromium ANGLE/D3D11 → DXVK v2.3 → gf Vulkan**。不走 WineD3D/GL：
  容器内 gf GLX/EGL 无法认领屏幕（厂商栈设备发现问题，见
  `dkms/vendor-bug-report-gf-gbm-crash.md` 问题三，无本地 shim 可绕）。
- 实测（17:09–17:19 三轮）：主窗口 `IsViewable`、抓图 1519 色真实界面（已登录态保留）；
  GPU 进程 **0 崩溃**（hw 首轮 DXVK 2.7 时 GPU 反复 0xC0000005，换 2.3 后消失）；
  稳态瞬时总 CPU **7%**（GPU 进程 1%），llvmpipe 时代「窗口出现后 CPU 忙约 23 秒」消除。
- sw 回退无回归：`d3d11,dxgi=b` 强制 builtin，system32 已被 DXVK 覆盖也仍走 WineD3D/GL；
  启动期 3 次 GPU `-2147483645` + 沙箱降级是历史 sw 会话固有行为（旧日志 111 次），非本次引入。

### 容器内 gf Vulkan 四件套（探针 `_work/vk-loader-probe.py` 逐项取证）
1. **os-release 重定向垫片**：gf ICD 有发行版白名单，容器 `/etc/os-release` 是 SteamOS，
   读到即拒绝初始化（`vkEnumeratePhysicalDevices` 返回 -3）；strace 证实开设备后读
   os-release 随即退出。复用随从大师 `osrelease_redirect.c`。
2. **`VK_DRIVER_FILES` 覆盖**：pressure-vessel 注入该变量，新版 loader（1.4.309）里它
   优先于 `VK_ICD_FILENAMES`——只设后者会被无视（loader 调试日志证实仍扫系统 icd.d）。
3. **协商垫片 ICD**：gf 裁掉 `VK_KHR_external_semaphore_capabilities` 且协商接口版本过旧，
   新 loader 拒载/拒建实例；复用 `libvulkan_gf_shim.c`。
4. **`libSC.so` 随部署拷贝 + `LD_LIBRARY_PATH`**：厂商着色器编译器不在 pressure-vessel
   驱动合并列表，容器内缺失时 DXVK 建备失败（`Failed to create D3D11 device`）。

### DXVK 版本
- Proton 11 自带 **v2.7 要求 `VK_KHR_maintenance5`**，gf 22.0.4 没有 →
  `Skipping: Device does not support required feature 'maintenance5'` / `No adapters found`。
- 改用仓库 `minion-masters/dxvk23/`（v2.3-21，随从大师 hw 已验证）；缺失时回退 Proton 自带。
- native dll 不走 `WINEDLLPATH`（只对 builtin 生效），照 Proton 脚本做法拷入前缀
  system32/syswow64，原 WineD3D dll 备份为 `*.wined3d-bak`。

### 部署与开关
- 垫片部署在 `~/.local/share/doubao-wine11/gf-accel`，**首次 hw 启动在宿主侧自动构建**
  （容器无 gcc、/usr/lib 是运行时的）；源码取自 `minion-masters/assets-mm-gf-vulkan`，
  路径经 sed 重写，不依赖 MM 运行时部署。`DOUBAO_GF_ACCEL` 可覆盖部署目录。
- winex11 自身 GL 探测仍留 mesa llvmpipe（`__GLX_VENDOR_LIBRARY_NAME=mesa` +
  `LIBGL_ALWAYS_SOFTWARE=1`），与随从大师 hw 一致，只让 DXVK 用 7G100。
- `tests/test-doubao-launcher.sh` 扩到 24 项（sw/hw 环境注入断言）。

### 追加（同日晚）：修设置弹窗缺块——GPU 合成折中
- 现象：hw 下设置弹窗等叠加层确定性缺块/破线（sw 正常）。责任在 gf 的 DXVK GPU 合成路径。
- 证伪杠杆：DXVK 2.5（仍要 maintenance5）、关 SkiaGraphite、关 GPU 光栅、
  `d3d11.cachedDynamicResources=cv`、scissor 负偏移钳制（垫片临时加后已回退）均无效。
- 唯一有效：`--disable-gpu-compositing`。hw 固化为 **软件合成+强制 GPU 光栅**
  （`--disable-gpu-compositing --enable-gpu-rasterization`），显示正确且保留大部分加速。
- 滚动负载 CPU：sw 89% / 折中 78% / 完整 hw 72%；稳态均 ~7%。
- 新增 `DOUBAO_DXVK_DIR` 覆盖 DXVK 来源（诊断旋钮）。
- 复现/度量脚本：`_work/dialog-shot.sh`、`_work/scroll-bench.sh`。

## 2026.11.24 — 启动加速：跳过 Proton 的 start.exe 中转

### 实测分解（冷启动到主窗口，基线 7.0–7.3 秒）
| 阶段 | 耗时 | 处理 |
|---|---|---|
| Steam Runtime 容器启动（含 `capsule-capture-libs`） | 1.2–1.8 秒 | 保留为稳健默认；`DOUBAO_FAST=1` 可跳过 |
| `wineboot --init` | 约 1.5 秒 | 未动（见下） |
| `start.exe /exec` 中转 | 约 1.06 秒 | **已消除** |
| 豆包初始化到窗口可见 | 约 2.5 秒 | 客户端自身 |

### 根因与修复
- `files/bin/wine` 壳把 64 位 `Doubao.exe` 交给 **32 位 loader**（`i386-unix/wine`）；32 位侧
  加载不了 64 位 PE，改走 `C:\windows\syswow64\start.exe /exec` 再 `CreateProcess` 给 64 位。
  `start.exe` 加载 `shell32.dll` 后到创建豆包进程空转 1.04 秒（`+loaddll` 追踪证实）。
  Proton 脚本自己就是直接用 `wine-preloader` + `x86_64-unix/wine` 并设 `WINELOADERNOEXEC=1`，
  注释写明 “avoid restart through start.exe”。启动器现按 PE 头 Machine 字段判断：
  64 位走直连并把参数换成 `C:\users\...`（保持豆包看到的自身路径不变），否则回退旧路径。
- 改传 Windows 路径、设置 `STEAM_COMPAT_CLIENT_INSTALL_PATH` 均无效（已证伪，不要重做）。

### 其它
- 默认 `WINEDEBUG=fixme-all`（`err`/`warn` 保留，外部值优先）；`launch.log` 超过
  `DOUBAO_LOG_MAX_BYTES`（默认 20 MiB）轮转为 `.1`，原 93 MB 日志保留为 `launch.log.1`。
- `DOUBAO_FAST=1` 是 `DOUBAO_NO_STEAM_RUNTIME=1` 的别名，不做默认。
- 新增 `tests/test-doubao-launcher.sh`（假 Proton + 假前缀，17 项）。

### 结果与未做项
- 默认路径 6.2–6.5 秒，`DOUBAO_FAST=1` 约 4.3 秒；主窗口 `IsViewable`、无新增错误。
- `wineboot` 阶段 `plugplay.exe` 单核满载 0.87 秒（设备枚举，上游行为），未禁用
  `PlugPlay` 服务；窗口出现后 CPU 仍忙约 23 秒（llvmpipe 软件渲染），留待硬件渲染一轮。

---

## 2026.11.24 — 界面字体过细：专用 fontconfig 替换

### 根因
- 豆包 `local_webcontents` 中 CSS 高频使用 `font-family:PingFang SC` 与
  `font-weight:300`（统计 2309 处 PingFang SC）。本机没有 PingFang SC，
  `fc-match 'PingFang SC:weight=light'` 回退到 `Noto Sans CJK SC Light`，
  所以中文正文笔画过细、看不清。

### 变更
1. 新增 `doubao/fonts.conf`：把 PingFang SC / Microsoft YaHei Light /
   微软雅黑 Light / DengXian Light / Segoe UI Light 映射到对应 Regular 家族，
   并把字重抬到 medium；
2. `doubao/run-doubao-wine11.sh` 在容器包装前导出 `FONTCONFIG_FILE`，
   只对豆包进程树生效；`DOUBAO_FONT_DISABLE=1` 可关闭，
   `DOUBAO_FONT_CONF` 可指定自定义配置；
3. 新增 `tests/test-doubao-font.sh`，验证配置注入与 `fc-match` 结果；
   `doubao/README.md` 同步说明重启方式与回退开关。

### 验证
- `FONTCONFIG_FILE=doubao/fonts.conf fc-match 'PingFang SC:weight=light'`
  由 `Noto Sans CJK SC Light` 变为 `微软雅黑 | Regular`。

---

## 2026.11.08 — 豆包 2.32.6 主窗口跑通（Steam Runtime 容器为稳健默认）

### 根因（按图形会话区分）

- **旧会话（11-07 22:00 – 11-08 02:53）**：裸跑宿主 `Proton 11.0/files/bin/wine`
  时 Chromium 147 主窗口以不含 `WS_VISIBLE` 的 style 创建、此后从未收到 `SW_SHOW`，
  `Map State: IsUnMapped`；强制 XMap 后抓图仍只有 `(0,0,0)` 1 色，主进程以
  `NtTerminateProcess ... exit_code 0` 主动退出。经 `SteamLinuxRuntime_sniper`
  容器启动则窗口 `IsViewable`、546 色真实界面 —— 容器是旧会话里唯一成功路径。
- **机器 07:50:20 重启后（GNOME/XWayland/Steam 07:51 起）**：裸 wine 与容器
  **都能**出界面，换成全新空 `User Data` 后两者也都成功（405 色）。
  所以容器**不是唯一必要条件**，黑屏指向**宿主图形会话状态**
  （Steam 客户端 / CEF、gf GBM/Mesa、XWayland 累积状态或残留孤儿进程），
  具体哪一项未单独定论；容器靠隔离 Mesa/GL 栈绕开污染，因此保留为稳健默认。
- 已排除：Wine API 缺失、网络、profile 状态、渲染后端。与《随从大师》
  「图形环境只注入目标进程树、绝不污染全局」的经验一致。

### 修复

- `doubao/run-doubao-wine11.sh` 默认用 `run-in-sniper` 包装自身
  （`DOUBAO_STEAM_RUNTIME_CHILD` 防递归，sniper → soldier 回退，都没有则明确报错，
  不静默降级）；`DOUBAO_NO_STEAM_RUNTIME=1` 走轻量裸 wine 路径，干净会话下可用。
- `doubao/fix-doubao-desktop.sh` 统一桌面与用户级 `.desktop` 的 Exec、打
  `metadata::trusted`、把会被 DING 交给 deepin-wine8-stable 的 `豆包.lnk` 移出桌面；
  `install-doubao.sh` 安装成功后自动跑一次。
- 启动输出落盘到 `~/.local/share/doubao-wine11/launch.log`，便于排查桌面双击。

### 补充修复：08:12「当前版本启动发生异常」弹窗

- 弹窗来自**旧 Deepin Wine 8.16 前缀**（`repair_2026_11_08_08_12_27.exe`，
  `C:\Program Files\Doubao\app`）；Wine 11 前缀 `running_exception` 一直是 0。
  journalctl 链路「完成：修复 0 个软链接 → wine version: 8.16」证实点击落在旧
  `run-doubao-wine.sh` / deb `run.sh` 链上。
- 旧前缀 `running_exception` 累计 `{"1.56.6":1,"2.32.6":8}`，来自排查期间反复
  `wineserver -k` / `timeout` 强杀，不是真实崩溃；旧前缀本就跑不了 2.32.6。
- `doubao/fix-doubao-desktop.sh` 扩展为统一**四个** `.desktop`（桌面 / 用户级 /
  系统级 / deb entries）的 Exec、去掉非标准 shebang、备份并清零旧前缀异常计数；
  `doubao/run-doubao-wine.sh` 改为安全转发到 Wine 11 启动器。
  不删账号数据、不禁用修复器、不锁版本、不自动重启。
- 三条路径实证（旧启动器 `--uri` / 系统级 desktop / 桌面图标）都进 Steam Runtime
  容器 / Wine 11：`map=2`、405 色真实界面、`main_browser=1`、无 8.16 旧链进程。

### 补充修复：点右上角 X 关不掉豆包（09:20）

- 根因三条实证：① 点 X 后主窗口 `WM_STATE=Withdrawn`、`saman_app_state` 的
  `chat_app_state.visible=false`，17 个 `Doubao.exe` 进程照常在跑 —— X 是「隐入
  通知栏」而不是退出；② Wine 11 `winex11.so` 只实现 XEmbed 托盘，XWayland 上
  `_NET_SYSTEM_TRAY_S0` 被 mutter 持有但不渲染（owner `0x600009`，嵌入图标 0 个），
  豆包同时报 `[Tray] BuildMainStatusIconContextMenu main_status_icon_ is null`，
  所以既没有「退出豆包」菜单也点不回窗口；③ 客户端没有关闭行为开关
  （`saman.general_settings.*` 只有四个键，悬浮球右键菜单也无退出项）。
- 新增 `doubao/quit-doubao.sh`：`--quit`（SIGTERM 主浏览器，必要时本前缀
  `wineserver -k` 收尾）、`--show`（单实例再拉以唤回主窗口）、`--status`（只读
  区分隐入通知栏 / 崩溃 / 托盘可用性）。进程按 `WINEPREFIX` 精确限定，
  不加守护、不做自动重启、不引入新依赖（xembedsniproxy 仓库无包，且
  selection 已被 mutter 占用，装 stalonetray/trayer 也抢不到）。
- 实测：`--quit` 1 秒内 17 个进程全退且 `running_exception` 仍为 0（不引发
  「启动异常」修复弹窗）；`--show` 在隐入通知栏后 5 秒、重启后 6 秒出主窗口。
  边界：`WM_DELETE_WINDOW`（如 `xdotool windowclose`）会销毁主窗口而不是隐入，
  这时单实例再拉不重建窗口，需用悬浮球右键「打开豆包窗口」或 `--quit` 后重启。
- 追加实测（10:15，含对 09:50 结论的更正）：顶部状态栏的「退出」时好时坏。
  **更正**：之前写「顶栏里没有豆包图标」是错的 —— Ubuntu 的
  `ubuntu-appindicators` 扩展自带 legacy XEmbed 托盘（`legacy-tray-enabled=true`，
  `trayIconsManager.js` 用 `Shell.TrayManager`），豆包图标确实显示在顶栏：
  窗口 `0xe0000e` 16x16 在 1556,8，`_XEMBED_INFO` flags=1，父窗口是 mutter 为
  legacy tray 建的容器 `0x6000b8`（不在 `_NET_SYSTEM_TRAY_S0` owner 的子窗口里，
  所以只查那里会漏判），窗口属主是 Wine 的 `explorer.exe /desktop`。
  图标是空壳：客户端侧 `main_status_icon_ is null`（11-08 累计 34 次），同时
  `Cloud config not exists or invalid` 300 次、`user_id not ready` 23 次、
  `account_token_manager_initial_fetch_error` / `auth_get_oauth_token_error` ——
  托盘菜单依赖这套云端配置/登录链路，拉不到就建不出菜单。
  输入侧试了 6 种（真实右键/左键、`click --window` XSendEvent、`windowfocus`+Menu、
  `windowraise` 后点、新鲜启动后点、销毁主窗口后点）全部无菜单、无 app 侧 tray 日志。
  另外 WM 级退出就是 `WM_DELETE_WINDOW`：只关主窗口 / 隐藏悬浮球后再关 /
  主窗口与悬浮球都关，三种组合下 17 个进程全部存活 —— 这条路从来没退过进程。
  所以「好使」只是窗口全消失了。判定统一用 `--status` 的进程数（现在会同时报
  顶栏图标窗口与菜单可用性），真要退只能 `--quit`；README 2.6.1 给了可选的
  GNOME 快捷键做法。不需要也不会引入 `xembedsniproxy`/`stalonetray`/`trayer`。
- 深挖（13:20，`WINEDEBUG=+systray` + 100ms 窗口轮询）：真实右键**确实到了豆包**
  （`WM_RBUTTONDOWN/UP` → `notify_owner relaying`），但豆包不建任何菜单窗口、
  也不打日志；它的托盘菜单只在启动阶段建一次，失败就是 `main_status_icon_ is null`
  （容器约 15s / 裸 wine 约 60s），之后不自愈。新增探针 `doubao/probe-trayrefresh.c`：
  广播 `TaskbarCreated` 能让客户端 `NIM_DELETE+NIM_ADD` 重注册（不再报 null），
  但菜单只有第一次重建后弹出过一次，再重建也不重现 —— 只作诊断、不作修复。
  另两条纪律：GNOME 面板是 Wayland actor，`xdotool` 的 XTEST 模拟点击**到不了面板**
  （真实点击才行）；**不要杀 Wine 的 explorer.exe**，托盘图标会随之消失且应用不补注册。
  `--status` 同步修正为只看本次启动的日志段，不再读历史会话。
- Wine 来源核对（13:50）：豆包跑的是 Valve 官方 `Proton 11.0`（`proton-11.0-2e-x86_64`，
  `wine-11.0`），主进程 `exe → …/Proton 11.0/files/lib/wine/x86_64-unix/wine64-preloader`，
  `WINEDLLPATH`/`LD_LIBRARY_PATH` 全指向 Proton 11.0，`LD_PRELOAD` 为空；该目录无
  `.bak/.orig/.patch`、也无晚于安装日 1 小时的文件 → **未修改**。另有一套改过的
  `Proton - MM GF`（Experimental 11.0-20260826 改名，改 `proton` 脚本 +
  DXVK 降至 v2.3-21 + `ntdll.so`/`winex11.so`/`win32u.so` 替换），但豆包不引用它；
  全盘无 wine 源码树（无 `dlls/winex11.drv`），故无需拷源码。
  `quit-doubao.sh --status` 新增 `wine 来源` 一行（自动判定是否被本地改过）。

### 验证

- 容器内 1200x800 主窗口 `IsViewable`，抓图 546 色真实界面（「有什么我能帮你的吗？」、
  登录按钮、输入框与对话 / PPT 生成等入口），日志出现 `chrome://doubao-chat/`。
- 桌面入口连续 3 次启动全部通过：`map=2`、546–547 色、`CrBrowserMain` 存活、容器在跑。
- 会话对照与消融：还原实验前空 profile 状态、删除 `Preferences` / `Remote_State` /
  `saman_update_info`，甚至整体换成全新空 `User Data`，容器内（新会话里裸 wine 亦然）
  仍正常出界面；WinINet 在两个前缀访问 `https://www.doubao.com/` 均 HTTP 200，
  排除网络；8 组渲染开关在旧会话裸 wine 下全部无效；给 Wine 8.16 补
  `RtlGetDeviceFamilyInfoEnum` 只消除 `win_util.cc(544)` FATAL 却仍进修复流程且
  无主窗口，此路已证伪。
- `bash tests/run-tests.sh -v`：9/9 通过；厂商驱动基线 v262.2.0.56 一致。
- 未做自动重启 / 看门狗；长时存活（10 分钟）、登录与文字对话仍列为待用户验收项。

### 续查：顶栏托盘右键菜单「有时不弹」（11-08 晚间）

- 客户端经安装器落回 **2.32.6** 后重新验证「关一次即永久失效」：3 次独立启动
  首次右键均弹、关闭后均不弹；菜单窗口 `165x284 @(1540,32)`、`IsViewable`、
  `_NET_WM_STATE_ABOVE/FOCUSED`、堆叠最上层，抓图 252 色（7 个菜单项），
  坏态下 Win32 层枚举也是 0 个菜单窗口 → 排除「建了但看不见」。
- 严谨复位矩阵（每步重取 16x16 图标窗口、用 `relaying 0x204` 确认右键送达）再证伪
  7 种外部复位手段，特别是 13:20 遗留的「广播 `TaskbarCreated` 后能弹一次」；
  真实 XTEST 右键、Win32 8 种托盘消息、XEmbed 焦点序列、单实例再拉全部无效。
- 内存对照：同一进程三次 `Doubao.dll .data` 快照只有 12 字节变化（计数器与两个
  `0→1` 标志），回写标志不能恢复；日志门控实测开着、坏态无新增 `[Tray]` 行
  → 状态不在模块全局数据段，**重启客户端仍是唯一可靠恢复手段**。
- 方法学陷阱：合成右键弹出的菜单 ESC 关不掉（只有真实点菜单外能关）、
  `TaskbarCreated` 会重建图标窗口使旧 id 失效、启动日志 `tee` 缓冲让
  `relaying` 计数滞后。
- ⚠️ 版本漂移：2.6.3 记录的反编译地址、PE 段表、日志行号与门控地址属于当时的
  2.35.8 DLL；当前 2.32.6 的段表（`.text` 0x10499a00 / `.data` RVA 0x12789000 /
  `SizeOfImage` 0x1350c000）、字符串区 0x1918fc8xx、代码 0x18458xxxx、日志行号 505
  与门控 0x192a5e940/0x192a5e948/0x1928281e0 已全部改变，详见 `doubao/README.md` 2.6.4。
- 修复 `doubao/tray-probe.py`：只统计 `IsViewable` 菜单窗口；菜单已在屏幕上时直接判
  GOOD 且不再点击，避免 `tray-doctor.sh --fix` 误重启客户端。闭环实测：坏态 `--fix`
  → 重启 → GOOD，菜单留屏再 `--probe` → GOOD（未重复点击）。
- 未引入看门狗/自动重启；实验脚本在 `_work/`（不入库），结论与交付工具在 `doubao/`。
