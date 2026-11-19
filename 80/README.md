# 80 对战平台（魔兽争霸 III）安装包

> 至砺算：安装链路与显卡无关。本目录用 Proton 11.0 的 wine-11.0 在独立前缀里
> 跑官网 NSIS 安装器；只验收安装产物，不做崩溃后自动重启/守护。

## 路径说明（2026-11-19）

本文档第 1–5 节记录 2026-11-17/18 原始路径与结论；本版新增第 6 节记录
2026-11-19 重装时的下载源、安装器行为与 Wayland 可见性坑。

## 1. 厂商专有二进制（不入库）

| 项 | 内容 |
| --- | --- |
| **是什么** | `80duizhan_1.9.9.60_guanwang.exe`：80 对战平台官网完整安装包，NSIS 自解压 PE32（约 180 MB，`file` 报 `Nullsoft Installer self-extracting archive`）。内含主程序 `80launch.exe` 与基于 OBS 27.2.4 的录制器组件。 |
| **为何不能入库** | 厂商专有二进制、体积大、升级会改文件名；按 `.codebuddy/rules/仓库组织约定.md` 二，用 `.gitignore` 规则 `80/80duizhan_*.exe` 排除，仓库只保留脚本与文档。文件仍留在本目录供运行时安装，不删除。 |
| **从哪获取** | 80 对战平台官网下载"官网完整安装包"，放到本目录、`~/下载` 或 `~/Downloads`。本机这份 SHA256：`dc3e6a5506055b534d87c745f92dad5f1b04248eec60c7cc6b78b1992b952d2b`。 |

## 2. 安装

```bash
cd ~/project/lisuan
./80/install-80duizhan.sh                 # 自动找安装包 → 独立前缀 → NSIS /S 静默安装 → 校验主程序
./80/install-80duizhan.sh --status        # 只读体检（不启动平台）
```

- 前缀：`~/.deepinwine/80duizhan-wine11`（与其它应用前缀隔离）。Proton 11.0 的
  wine，只在 wine 进程树上设置 `WINEPREFIX` / `WINEDLLPATH` / `LD_LIBRARY_PATH` /
  `WINEDLLOVERRIDES=mscoree,mshtml=`，不污染全局 shell。
- 安装器查找顺序：命令行参数 → `80/` → `~/下载` → `~/Downloads`。
- 安装器返回码 `2`（Abort）、`124`（超时）或其它非零均明确报错并保留日志，
  **不自动重试**；NSIS 已装时只体检不重装。
- 安装目录由脚本在 `drive_c/Program Files*` 下查找 `80launch.exe` 自动判定，
  可用 `E80_INSTALL_DIR` 覆盖（相对前缀 drive_c 的路径）。

## 3. 运行与验收状态

- 安装与「启动到可见窗口」已于 2026-11-17 验收；平台登录 / 魔兽3 联机仍未在砺算
  7G100 上验收，见第 5 节。
- 运行请用 `./80/run-80duizhan.sh`（桌面入口已指向它）：只在 wine 进程树上隔离环境，
  不污染 Steam 客户端或全局 shell。
- 崩溃处理：不加剧、不掩盖。采集日志 / dump / journal 后再定位，不加看门狗或自动重启
  （见 `.codebuddy/rules/崩溃处理约定.md`）。

## 4. 桌面图标与启动入口（2026-11-17 实测）

安装器只在 Wine 前缀里留了 Windows `.lnk`，Linux 桌面没有图标；`install-80duizhan.sh`
的 `--status` 曾因 `find` 收到不存在的候选目录，在 `pipefail` 下把已安装的主程序
误报为「未找到」，现已修复。

```bash
cd ~/project/lisuan
./80/fix-80-desktop.sh              # 提取官方图标 + 生成桌面入口（幂等）
./80/fix-80-desktop.sh --status     # 只读体检
./80/fix-80-desktop.sh --uninstall  # 恢复备份、删除生成的图标与入口
```

- 从 `80launch.exe` 的 PE 资源提取 16/24/32/48/64/96/128/256 共 8 个尺寸，
  写入 `~/.local/share/icons/hicolor`，图标名 `80duizhan`；复用
  `dreamina/extract-icon.py`，**厂商图标产物不入库**。
- 入口位置：`~/.local/share/applications/80duizhan.desktop`、XDG 桌面目录
  （本机为 `~/桌面`）以及 `~/Desktop`，均已标记 `trusted`。
- 桌面项 `Exec` 指向 `80/run-80duizhan.sh`：单次前台启动、隔离前缀、
  参数透传、退出码原样透传；不做存活监控或崩溃后自动重启
  （见 `.codebuddy/rules/崩溃处理约定.md`）。
- 覆盖已有 `.desktop` 前先备份到
  `~/.local/share/80duizhan-wine11/desktop-backups/`；`--uninstall` 可恢复。
- 图标生成成功不代表平台已启动；启动问题见第 5 节。

## 5. 启动修复（2026-11-17 实测）

### 现象与根因

- 点桌面入口后 `80launch.exe` 进程仍在、CPU 有占用，但 X11 只有
  `80对战平台` 648x514 窗口且 `Map State: IsUnMapped`，`wmctrl -l` 看不到；
  再次点击只在 `Log/*/ferry_*.txt` 里追加 `Already exist an instance.`。
- 启动日志关键证据：
  - `failed to get driver name for fd ... pci id 4c54:5006, driver (null)` →
    `No driver found` → `failed to load driver: (null)`：winex11 探测 OpenGL 时
    DRI 驱动没起来，窗口停在未映射状态；
  - 宿主继承给 32 位 `80launch.exe` 的 `VK_ICD_FILENAMES` 指向 **64 位**
    `gf_icd.x86_64.json`，架构不匹配；
  - 二级 UI 进程里可见 `0x80010106`（RPC_E_CHANGED_MODE，COM apartment 冲突），
    但在成功出窗的运行中也出现过，A/B 显示它与「是否出窗」无稳定因果，按非阻塞噪音处理。
- A/B 结论：**只**去掉 `VK_ICD_FILENAMES` 或**只**加软件 GLX 都不能稳定出窗；
  三项同时生效才稳定（约 6 秒内 `IsViewable`，日志不再有 `No driver found`）。

### 修复与验收

- `80/run-80duizhan.sh` 只在目标 wine 进程树执行
  `env -u VK_ICD_FILENAMES LIBGL_ALWAYS_SOFTWARE=1 __GLX_VENDOR_LIBRARY_NAME=mesa`；
  不改驱动文件、不污染 Steam 客户端或全局 shell。
- 单次启动实测：X11 窗口 `Map State: IsViewable`，`wmctrl -l` 能看到
  `80对战平台`，启动日志无 `No driver found`；仍是单次前台启动、无守护/自动重启。
- 登录、房间匹配、Warcraft III 本体联机未验收；不要为这些场景加看门狗。

### 更新阶段闪退（2026-11-18）

- 用户点击桌面入口后的日志：`03:48:39` 第一次启动正常写 `records saved`，
  `update.exe` 随后运行约 23 秒；`03:49:02` 又启动一次，日志停在
  `[WindowTool] 初始化com组件失败. 错误码:0x80010106`，没有 `records saved`，
  进程随即退出。即「正在更新」阶段较长，重复点击触发单实例冲突后闪退。
- 单次复现确认：无参数启动时 `t=5s` 可见 `update.exe`，`t=10s` 它退出并
  派生带 `-debug -noupdate` 的第二个 `80launch.exe`；显式传
  `-noupdate -workdir 'C:/Program Files (x86)/80duizhan/'` 则全程只有**一个**
  `80launch.exe`，不再出现 `update.exe`，约 4 秒即出 1120x623 窗口。
- `80/run-80duizhan.sh` 现在在**无参数且未设 `E80_ALLOW_UPDATE=1`** 时默认
  追加 `-noupdate -workdir <安装目录>`；显式传参原样透传。
  `E80_ALLOW_UPDATE=1 ./80/run-80duizhan.sh` 可恢复更新检查。
- 这是**绕过**厂商更新器在 Wine 下的卡住/闪退，不是修好更新器本身；
  不加重试、守护或自动重启。真正需要升级时用官网完整安装包重新安装。

### 随机 c0000005 闪退（2026-11-18）

- 跳过更新器后仍偶发闪退：WINEDEBUG=+seh 捕获
  Unhandled exception code c0000005 addr 0xe2e0a7；反汇编落在
  80launch.exe 的 _chkstk 栈探测循环（test DWORD PTR [eax],eax）。
- PE 头原为栈保留 1 MiB / 提交 4 KiB，改为 8 MiB / 64 KiB 后，
  两次单次启动均无 c0000005，窗口 IsViewable。
- 应用：./80/fix-80-stack.sh（幂等，先备份 .bak-stack）
  体检：./80/fix-80-stack.sh --status
  回滚：./80/fix-80-stack.sh --uninstall
- 只改本地前缀内的 80launch.exe，厂商二进制不入库；
  本机原版 MD5 be7ce1b16033f59f5252364a0dd9ec94，
  修复后 MD5 8a6a7235c057874a840aaab349e27cad。
- 这是绕过厂商栈假设，不是修好 CEF/Wine 栈耗尽；升级重装后需重新应用。
- 2026-11-18 晚复测：**栈补丁并非必需**。前缀内 80launch.exe 已回滚为原版
  （MD5 `be7ce1b16033f59f5252364a0dd9ec94`），`./80/fix-80-stack.sh` 保留备查。

### 现行方案：默认 +seh 绕过渲染竞态（2026-11-18 晚）

- 现象：默认 `WINEDEBUG=-all` 时约 4 秒退出（wine 退出码 5）、窗口从未出现。
  `E80_WINEDEBUG=+err,+fixme` 抓到决定性证据：
  `0128:err:seh:NtRaiseException Unhandled exception code c0000005 flags 0 addr 0xe2e0a7`。
  反汇编 `0xe2e0a7`（RVA `0xa2e0a7`）落在 80launch.exe `.text` 的 int3 填充区。
- 已排除（A/B 均无稳定效果）：改 wine `combase.dll` 让 `RPC_E_CHANGED_MODE`
  返回 S_OK（ferry 日志的 `0x80010106` 确实消失，但仍退出码 5）；PE 栈提交
  改 8 MiB、`ulimit -s 65536`；CEF `--disable-gpu`、`--use-angle=swiftshader`、
  `--use-gl=swiftshader`；显式 32 位 llvmpipe `VK_ICD_FILENAMES=lvp_icd.json`；
  清理残留 wine 服务进程；新建前缀重装；回滚 exe 版本。
- 有效项只有 `WINEDEBUG=+seh`：它改变初始化时序。实测原版 exe + 系统
  Proton 11.0 连续 2/2 出窗、无 `Unhandled exception`、存活至超时；专用
  Proton 副本（含 combase 补丁）下 3/3 同样成立。
- 实现：`80/run-80duizhan.sh` 默认 `WINEDEBUG=+seh`，trace 写入
  `~/.local/state/80duizhan/wine.log`，不刷桌面/终端；
  `E80_WINEDEBUG=-all ./80/run-80duizhan.sh` 可关闭做 A/B。
- 验收：`wmctrl -l | grep 80对战平台` 能看到窗口，进程稳定存活，
  `wine.log` 无 `Unhandled exception`。
- **这是绕过，不是修好根因**：CEF/ANGLE 在 wine 下的渲染初始化竞态/内存
  破坏仍未定位；没有看门狗、自动重启或重试。登录与联机未验收。

## 6. 2026-11-19 重装与可见性排障（踩坑记录）

### 6.1 安装包获取

- 官网 `https://www.80dzgame.com/` 首页“下载客户端”由 JS 绑定，静态 HTML 没有直链；
  直链写在 `static/js/http.js`：`DL_DOMAIN_URL`。当前该文件仍写死旧版本
  `80duizhan_1.9.9.15_guanwang.exe`，但 CDN 上已有 `1.9.9.60`：
  `https://dl.80dzgame.com/war3online/80duizhan_1.9.9.60_guanwang.exe`。
- **坑 1**：`dl.80dzgame.com` 对 `curl`/无浏览器指纹请求返回 WAF
  「Page Verification」HTML（`X-Tengine-Error: denied by custom_acl`），
  带 `Referer`/UA 也一样；`HEAD` 看似正常（Content-Length 187779288），
  `GET` 只拿到 10 KB HTML。若只校验 HTTP 200/Content-Length 会把 HTML 当安装包。
  官网备用域名 `https://dl.hfnuola.com/war3online/80duizhan_1.9.9.60_guanwang.exe`
  可直接下载完整 NSIS PE，SHA256 与第 1 节一致（`dc3e6a55…952d2b`）。
- **坑 2**：`80/` 里同时存在 `80duizhan_1.9.9.60_guanwang3_VOID_…exe`（哈希不同、
  180 MB），`install-80duizhan.sh` 无参数时按文件名排序会优先选中它；该安装包
  在当前 wine 下 `/S` 返回 0 但不落盘。重装务必显式传安装包路径，或先清理多余文件。

### 6.2 安装器 `/S` 返回 0 但不落盘

- `./80/install-80duizhan.sh [安装包]` 使用 NSIS `/S`，实测约 3 秒返回 0，
  日志只有 `wineserver: using server-side synchronization.`，前缀里没有任何
  `80launch.exe`；脚本正确报「返回码 0 但未找到 80launch.exe」。
- `/S /NCRC /D=...` 同样无效。交互式启动 `wine <安装包>`（不带 `/S`）
  约 16 秒自动完成安装并退出，得到 515 MB 安装目录和 36 MB 主程序。
- 结论：该安装包在 Proton 11.0 wine-11.0 下静默参数不可靠。**不要**为“返回 0”
  加自动重试或守护；`--status` 校验主程序 PE 头才是安装成功判据。

### 6.3 Wayland 可见性：X11 有窗口但屏幕看不到

- `xwininfo -root -children` 能看到 `80对战平台` 1120×623 `IsViewable`，
  `xprop` 也显示 `_NET_WM_STATE` 正常、`_NET_WM_DESKTOP=0`；
  但用 `capture-wayland-screen.py` 抓合成画面时完全看不到窗口。
- **坑 3（显示模式相关）**：排障当时本机是双屏扩展，X11 root 3840×1080
  （`xrandr`：HDMI-1 左 +*，DP-1 右），抓图脚本默认 connector 取到 DP-1，
  抓到的是空桌面；当时必须显式 `--connector HDMI-1` 才能抓到主屏。
  2026-11-19 晚用户已改为**双屏镜像**（`xrandr` 两个输出都 1920x1080+0+0），
  两个 connector 内容相同，看任一屏即可，不必再指定 connector；
  但若改回扩展模式，必须重新显式指定。
- 显式抓主屏后仍看不到 80 窗口；随后把窗口移到另一块屏、抬到堆叠顶层、
  设为 `_NET_ACTIVE_WINDOW` 仍不可见。对照实验：同前缀的 `notepad` 窗口
  Depth 24 正常显示，80 窗口 Depth 32（ARGB visual），最初怀疑
  alpha 通道全 0 被合成器透掉。
- **假说已证伪（2026-11-19）**：用 `E80_WINEDEBUG=+seh,+x11drv` 短时启动抓日志，
  主窗口 `X11DRV_WindowPosChanging ... shaped 0`，各子窗口
  `needs_offscreen_rendering ... surface->alpha_mask 0`，`init_visuals`
  显示默认 visual 25 / ARGB visual 72；即窗口不是 layered/shaped，
  alpha mask 也没被设成 0，Depth 32 只是 Wine 的默认 visual 选择。
- 两次注册表尝试均无效且已撤销：`UseShape=N`（驱动里并无此键）与
  `ShapeLayeredWindows=N`（驱动确实读取，但实测窗口仍不可见）。
  不要再试这两个偏方；这不是安装或分层窗口问题。
- 当前验收只能到“X11 窗口存在 + 进程存活 + 日志无未处理异常”，
  **合成后视觉可见尚未通过**；需要进一步核对 Wine/XWayland 与 CEF 的
  OpenGL swapchain 呈现路径。

### 6.4 命令超时

- `wineserver -w`、注册表操作后跟裸 `wineserver -w`、GUI 安装器探测均会挂住会话；
  三次卡死均由缺少 `timeout` 引起。已新增
  `.codebuddy/rules/命令超时约定.md`，所有易卡命令必须带 `timeout` 并保留退出码。

## 7. 网上资料检索（2026-11-19）

### 7.1 结论：没有 80 平台的 Wine/Proton 专项资料

用中英文多组关键词检索（80对战平台 + Wine/Proton/Linux/deepin/统信/麒麟，
`80duizhan` + `80launch.exe` + Wine/Proton，以及
`site:github.com` / `site:protondb.com` / `site:lutris.net` / Wine AppDB），
**没有找到任何专门讲 80 对战平台在 Wine 或 Proton 下运行的资料**：
GitHub/Gitee、ProtonDB、Lutris 均无 80 平台条目；Wine AppDB 只有暴雪
「魔兽争霸官方对战平台」（`iId=37606`，版本 1.7.85，Wine 4.0，评分 Garbage）
和 Warcraft III 本体条目，都不是 80 平台，且结果过于陈旧。
网上能搜到的“80 对战平台”内容全是下载站和游戏资讯，无 Linux 适配记录。

### 7.2 可借鉴的同类平台资料（都不是 80 平台）

1. **11 对战平台 / War3（CSDN，2026-05-09，Ubuntu 26.04）**
   `https://blog.csdn.net/east4ming/article/details/160912889`
   - 11 平台需要 `winetricks corefonts vcrun2019 mfc42 msxml3 ie8`；
     其中 `mfc42` 是平台 UI 框架、`msxml3` 用于加载对战列表、`ie8` 供部分 HTML 内容。
   - 11 平台 UI 开 DXVK 会严重闪烁（登录界面、房间列表），关闭 DXVK 可解；
     作者最终没关，表示“不影响正常使用”。
   - 原文未给出 Wine 版本号、独立 prefix 路径前后矛盾、无 DLL override 细节。
   - 对我们的意义：可尝试**补齐运行库**（`mfc42/msxml3/ie8`）作为可见性/UI 下一步；
     但“关 DXVK 防闪”不适用于我们——我们的问题不是闪烁，是窗口根本没合成出来。

2. **Eurobattle.net（Wine 11.0 Staging + Debian 13，2026-01 修订）**
   `https://wiki.eurobattle.net/index.php?title=Linux_guide&oldid=2538`
   - 安装时接受 wine-mono，且**不要**勾 “I already have Visual C++”，让安装器装 VC++。
   - `HKCU\Software\Blizzard Entertainment\Warcraft III\Misc` 下
     `seenintromovie=1` 禁用开场动画；卡 “checking for updates” 时
     `HKCU\Software\Eurobattle.net` 下 `patch=15`（十进制）。
   - 已知问题：News 标签页（Qt WebEngine）不工作，其它功能正常。
   - 对我们的意义：同类对战平台的**内嵌 Web 页在 Wine 下不工作**是已知痛点，
     但表现为“主窗口在、某个标签页废”，和我们“整个窗口不可见”不同，不能直接套用。

3. **Wine 原生 Wayland 驱动的 CEF 修复（新闻，2026-11-18）**
   Wine 11.23 合入 remote OpenGL swapchain presentation，修复 CEF 应用在
   Wine 原生 Wayland 驱动下画空白窗口的问题。本机 Proton 11.0（wine-11.0）
   的 `files/` 下没有任何 `wayland` 驱动文件，走的是 XWayland 路径，
   这个修复**当前不可用**；若要验证需换带该驱动的 wine 版本做 A/B。

### 7.3 结论与下一步

- 网上没有现成配置可抄；80 平台在 Wine 下目前属于本项目自己的探索。
- 可低成本尝试：在 80 前缀补 `mfc42/msxml3/vcrun2019/corefonts`（`ie8` 待定，
  80 用 CEF 而非 IE 内核）；若仍不可见，再考虑带 Wayland 驱动的新 wine 做 A/B。
- 不要照搬 11 平台的 DXVK 开关或 Eurobattle 的注册表键到 80 前缀。
