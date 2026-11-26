# 星际争霸目录变更记录

## 2026.11.26 — 开启 7G100 硬件渲染（zink + Venus -> gf）

- 现行默认：桌面三处入口已切到 `run-starcraft-hw.sh`；`run-starcraft.sh`
  保留为软件回退，`fix-starcraft-desktop.sh --soft` 可切回。
- 渲染链：原 32 位前缀不变，wined3d -> Mesa GLX zink -> 32 位 virtio/Venus
  ICD -> `/run/user/1000/l4d2-venus.sock` -> 64 位 `virgl_test_server --venus`
  -> `libvulkan_gf.so` -> 7G100。复用 `left4dead2/fix-l4d2.sh` 已建立的服务，
  启动器只检查 active/socket，不自动拉起、不做看门狗。
- 启动器显式清除继承的 `LIBGL_ALWAYS_SOFTWARE`，设
  `MESA_LOADER_DRIVER_OVERRIDE=zink` / `VTEST_SOCKET_NAME` / `VN_DEBUG=vtest`；
  不污染 Steam 客户端或系统驱动。
- 验证：`run-starcraft-hw.sh --status` 全项 OK；35 秒受控启动出现
  `Brood War` 1920×1080 全屏窗口，loader 日志明确
  `Using "Virtio-GPU Venus (LISUAN 7G100 Series Graphics)" with driver: "libvulkan_virtio.so"`，
  无崩溃、无残留游戏进程。
- 证伪并清理：new WoW64 + 原生 gf ICD/gf GLX 在 1998 年 DirectDraw 上弹
  `DirectDraw was unable to be acquired / Data File Error`（软件渲染同样失败），
  已移除临时前缀 `~/.deepinwine/Starcraft-wine11-hw`；这条路径不入库。

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
