# 星际争霸 1.08（Wine 11）全屏启动

> 至砺算：**图标与全屏都不需要改砺算驱动**。游戏是 1998 年的 32 位
> DirectDraw 程序；Spark 包自带的 deepin-wine5（Wine 5.0）在 XWayland 上
> 无法通过 XRandR 切换显示模式，所以 Alt+Enter 也救不了全屏。改用 Proton 11
> 自带的 Wine 11 后，同一份游戏文件在启动时直接创建 1920×1080 全屏窗口。
> 本目录只提供单次前台启动器和桌面入口，不改厂商二进制或原前缀。

## 1. 现状与文件位置

| 项 | 内容 |
| --- | --- |
| 安装包 | Spark 包 `com.starcraft.spark`，`/opt/apps/com.starcraft.spark/` |
| 游戏文件 | `starcraft.exe`（PE32，1.08 中文版，实测可直接进主菜单） |
| 原安装前缀 | `~/.deepinwine/Deepin-starcraft`（Spark 包，Wine 5.0；保留不动） |
| 运行前缀 | `~/.deepinwine/Starcraft-wine11`（首次启动从原前缀复制，约 450 MB） |
| Wine 运行时 | `~/.local/share/Steam/steamapps/common/Proton 11.0/files/bin/wine`（Wine 11） |
| 图标源 | `/opt/apps/com.starcraft.spark/entries/icons/hicolor/scalable/apps/com.starcraft.spark.png`（208×208 PNG） |

**游戏本体、Wine 前缀和图标原文件是第三方专有资产，不入库**：
它们由 Spark 包安装到上表路径，本目录只保存脚本文本。Wine 11 运行前缀与
图标产物写在 `~/.deepinwine/`、`~/.local/share/icons/hicolor/`，同样不入库
（见 `.codebuddy/rules/仓库组织约定.md`）。

## 2. 启动与全屏

```bash
cd ~/project/lisuan
./starcraft/run-starcraft.sh                 # 前台启动；Wine 11 直接全屏
./starcraft/run-starcraft.sh --status        # 只读体检
STARCRAFT_NO_FULLSCREEN=1 ./starcraft/run-starcraft.sh   # 排障时禁用备用按键
```

启动器做四件事：

1. 检查目标前缀（首次运行还会检查原前缀）是否已被占用（`WINEPREFIX` +
   `wineserver` 进程环境），命中即显式报错退出；
2. 首次运行把 Spark 前缀复制到独立的 Wine 11 前缀，原前缀保持不动；
3. 只在游戏进程树注入 Proton 11 的 Wine 运行时与 Mesa 软件 GL 变量，
   不污染 Steam 客户端或全局 shell；
4. 前台运行 `starcraft.exe`；`wmctrl` 找到 `Brood War` 窗口后只检查 fullscreen
   状态。Wine 11 启动时窗口已经是 1920×1080 且带 `_NET_WM_STATE_FULLSCREEN`，
   不按键；只有异常地没进全屏时才发一次备用 `Alt+Enter`，不循环、不常驻。

**根因（2026-11-26 实测）**：deepin-wine5 的 DirectDraw 走 XRandR 切显示模式
（日志：`X11DRV_DisplayDevices_Init via "XRandR 1.4"`、游戏调用
`ddraw1_SetDisplayMode 640x480`）。本机是 GNOME Wayland，`xrandr --output DP-1
--mode 640x480` 直接报 `BadMatch (RRSetScreenSize)`；`Alt+Enter` 确实送达窗口
（`WM_SYSKEYDOWN wp=0x12/0x0d`），但之后再没有显示模式调用，窗口仍是 640×480。
改注册表 `DisplayMode=0/1/2`、用 `wmctrl`/`xprop` 强加 fullscreen 状态、
`xdotool` 改窗口大小，在 Wine 5 下都无效（游戏窗口会忽略尺寸变化）。

**为什么换 Wine 11**：同一份游戏与同一个前缀副本，Proton 11 的 Wine 11 启动后
窗口为 1920×1080、位置 (0,0)，`_NET_WM_STATE_FULLSCREEN` 已置位，活动窗口是
`Brood War`；把 `DisplayMode` 改回原值 2 后仍然全屏，说明起作用的是 Wine 版本
而不是注册表。游戏文件、Spark 原前缀与砺算驱动都没有改动。

**为什么不靠合成抓图判定**：全屏 XWayland 游戏会让 Mutter ScreenCast +
PipeWire 取帧不稳定（多次 25–50 s 超时），按 `.codebuddy/rules/Wayland抓图约定.md`
不能用抓图失败判定渲染故障；本机以窗口几何、`_NET_WM_STATE`、活动窗口作为
全屏判据。

**为什么跳过 HanStar Plus 中文启动器**：原桌面项 `com.starcraft.spark.desktop`
启动 `星际争霸中文启动器.exe`，先弹一个 276×158 的启动器窗口，还要再点一次
才能进游戏；直接运行 `starcraft.exe` 已能显示中文界面并正常进主菜单，启动链更短。

## 3. 桌面图标

```bash
./starcraft/fix-starcraft-desktop.sh             # 生成图标 + 桌面入口（幂等）
./starcraft/fix-starcraft-desktop.sh --status    # 只读体检
./starcraft/fix-starcraft-desktop.sh --uninstall # 恢复备份、移除生成的图标
```

- 入口：`~/.local/share/applications/starcraft.desktop` 与 XDG 桌面目录
  （本机 `~/Desktop`）的 `starcraft.desktop`，均标记 `trusted`。
- `Exec` 指向 `run-starcraft.sh`；`Icon=starcraft` 指向从 Spark 包官方 PNG
  缩放出的 16/24/32/48/64/96/128/256 尺寸图标。
- 脚本只改已有 `.desktop` 的 `Icon` / `Exec` 两行，改前生成 `.bak`；
  `--uninstall` 恢复备份或删除新建项，并移除生成的图标。

## 4. 鼠标不跟手（2026-11-26）

Spark 原前缀的 `HKCU\Control Panel\Mouse` 带 Windows 指针加速
（`MouseSpeed=1`、`MouseThreshold1=6`、`MouseThreshold2=10`）。Wine 11
全屏 XWayland 下，游戏收到的鼠标位移先经过 Wine 的指针加速，再叠加
指针 warp/回中，表现为光标跟不上手、点选与边缘卷屏错位。

修复只作用于独立前缀 `~/.deepinwine/Starcraft-wine11`：

```bash
./starcraft/fix-starcraft-mouse.sh             # 幂等关闭指针加速
./starcraft/fix-starcraft-mouse.sh --status    # 只读体检
./starcraft/fix-starcraft-mouse.sh --uninstall # 从 Mouse.reg.bak 还原
```

备份在 `~/.local/share/starcraft/mouse-fix/Mouse.reg.bak`；不改宿主
libinput、系统驱动、Steam 全局配置，也不做看门狗/自动重启。

> 该修复解决的是“光标位移与手不一致”。如果连菜单箭头和画面都整体卡顿，
> 那更可能是 `llvmpipe` 软件渲染帧率问题，不在本脚本范围内。

## 5. 验收状态（2026-11-26 实测）

- Wine 5.0（旧方案）：窗口 640×480，`Alt+Enter` 送达但无效；
  `DisplayMode=0/1/2`、WM fullscreen、改窗口尺寸全部无效 → 判定为
  XWayland 无法切分辨率，不是按键发送问题。
- Wine 11（现行方案）：`Brood War` 窗口 1920×1080、位置 (0,0)，
  `_NET_WM_STATE(ATOM) = _NET_WM_STATE_FULLSCREEN, _NET_WM_STATE_FOCUSED`，
  `xdotool getactivewindow getwindowname` 为 `Brood War`；启动器日志显示
  「Wine 11 已直接创建全屏窗口，无需 Alt+Enter」。`DisplayMode` 保持原值 2。
- 隔离：Wine 11 前缀与原 Spark 前缀分开；砺算驱动与 Steam 客户端未改动。
- 桌面入口与图标：由 `./starcraft/fix-starcraft-desktop.sh --status` 自检。
- 鼠标（2026-11-26）：独立前缀的指针加速三项已设为 0，`fix-starcraft-mouse.sh --status` 为 3/3 OK；修复后 40 秒受控启动仍直接出现 `Brood War` 全屏窗口，无崩溃、无残留游戏进程。
- 限制：全屏时 ScreenCast 抓帧可能超时，不作为失败判据（见第 2 节）。

## 6. 排障速查

```bash
# 只读体检（Wine 运行时、主程序、前缀、窗口工具）
./starcraft/run-starcraft.sh --status

# 看 Wine 日志定位启动问题
STARCRAFT_WINEDEBUG=err+all ./starcraft/run-starcraft.sh

# 临时禁用备用 Alt+Enter（游戏自身仍会全屏）
STARCRAFT_NO_FULLSCREEN=1 ./starcraft/run-starcraft.sh --status

# 指定其它 Proton/Wine 或前缀
STARCRAFT_PROTON_DIR="$HOME/.local/share/Steam/steamapps/common/Proton 11.0" \
  STARCRAFT_PREFIX="$HOME/.deepinwine/Starcraft-wine11" ./starcraft/run-starcraft.sh

# 图标或桌面项异常
./starcraft/fix-starcraft-desktop.sh --status
```

常见现象：

| 现象 | 原因 | 处理 |
| --- | --- | --- |
| 提示前缀已有实例 | 同一 `WINEPREFIX` 上还有 wineserver/游戏进程 | 关闭已开的星际争霸窗口再启动；不做自动拉起 |
| 提示找不到 Wine | Proton 11 路径变化或未安装 | 检查 `STARCRAFT_PROTON_DIR`，确认 `files/bin/wine` 存在 |
| 首次启动复制前缀很慢 | 从 Spark 前缀复制约 450 MB，只做一次 | 等待完成；不要重复点击桌面图标 |
| 窗口仍是 640×480 | 实际用的还是 deepin-wine5，或 `STARCRAFT_WINE` 覆盖了默认 | 检查 `--status` 里的 Wine 运行时；不要用 WM 强改状态掩盖 |
| 桌面图标显示为空白 | 图标缓存未刷新或 `Icon=` 被覆盖 | 重跑 `fix-starcraft-desktop.sh`，检查 `gio info` 的 trusted 标记 |
| 鼠标不跟手 / 点选错位 | Spark 前缀带 Windows 指针加速，叠加 Wine 全屏 XWayland 指针 warp | 关闭游戏后跑 `./starcraft/fix-starcraft-mouse.sh`，再启动对比；`--uninstall` 可还原 |
| 游戏崩溃 | 不要用看门狗掩盖 | 按 `.codebuddy/rules/崩溃处理约定.md` 取证（保留日志、core、journalctl） |
