# 星际争霸目录变更记录

## 2026.11.26 — 鼠标不跟手：关闭前缀指针加速

- 根因：Spark 原前缀 `HKCU\Control Panel\Mouse` 带 Windows 指针加速
  （`MouseSpeed=1`、`MouseThreshold1=6`、`MouseThreshold2=10`）；Wine 11
  全屏 XWayland 下与指针 warp/回中叠加，光标位移被放大/压缩，表现为
  “不跟手”、点选和边缘卷屏错位。与 7G100 显卡驱动无关。
- 新增 `starcraft/fix-starcraft-mouse.sh`：只把独立前缀三项 REG_SZ 改为 0，
  改前导出 `Mouse.reg.bak`，幂等、提供 `--status` / `--uninstall`；
  不改宿主 libinput、系统驱动或 Steam 全局配置，不做守护/自动重启。
- 验证：修复前 status 三项均非 0；修复后 3/3=0；40 秒受控启动出现
  `Brood War` 全屏窗口，无崩溃、无残留 `starcraft.exe`。

## 2026.11.26 — 全屏改用 Proton 11 的 Wine 11

- 根因：Spark 包 deepin-wine5（Wine 5.0）的 DirectDraw 全屏依赖 XRandR 切
  分辨率；本机 GNOME Wayland/XWayland 下 `xrandr --mode 640x480` 报
  `BadMatch (RRSetScreenSize)`。`Alt+Enter` 消息能送达游戏窗口，但不再触发
  `SetDisplayMode`；改注册表 `DisplayMode`、WM fullscreen 属性与
  `xdotool` 改窗口大小均无效。
- `run-starcraft.sh` 默认改用 Proton 11 自带 Wine 11（`PROTON_USE_WOW64=1`
  + `WINEDLLPATH`/`LD_LIBRARY_PATH`），首次运行从 Spark 前缀复制到独立
  `~/.deepinwine/Starcraft-wine11`，原前缀不动。
- Wine 11 启动时直接创建 1920×1080 全屏窗口，`_NET_WM_STATE_FULLSCREEN`
  已置位；启动器只在异常未全屏时发一次备用 `Alt+Enter`，不循环、不守护。
- 实测：`DisplayMode` 保持原值 2 仍全屏；活动窗口为 `Brood War`。
- 桌面入口脚本无需改动（`Exec` 仍指向 `run-starcraft.sh`）。
