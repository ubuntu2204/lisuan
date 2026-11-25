# 星际争霸 1.08 桌面图标与 Wine 全屏启动

> 至砺算：**图标与全屏启动都不需要改砺算驱动**。游戏是 1998 年的 32 位
> DirectDraw 程序，已装在本机 Spark 包的 deepin-wine5 前缀里；本目录只提供
> 一个单次前台启动器和桌面入口，按《星际争霸》自己的 Alt+Enter 全屏快捷键
> 让它进入全屏，和本机暗黑 2 在 Wine 里进入全屏是同一条路径。

## 1. 现状与文件位置

| 项 | 内容 |
| --- | --- |
| 安装包 | Spark 包 `com.starcraft.spark`，`/opt/apps/com.starcraft.spark/` |
| 游戏目录 | `~/.deepinwine/Deepin-starcraft/drive_c/Program Files/starcraft/` |
| 主程序 | `starcraft.exe`（PE32，1.08 中文版，实测可直接进主菜单） |
| Wine 前缀 | `~/.deepinwine/Deepin-starcraft`（PACKAGE_VERSION `1.08.spark0`，Wine 5.0） |
| Wine 命令 | `/usr/bin/deepin-wine5-stable` |
| 图标源 | `/opt/apps/com.starcraft.spark/entries/icons/hicolor/scalable/apps/com.starcraft.spark.png`（208×208 PNG） |

**游戏本体、Wine 前缀和图标原文件是第三方专有资产，不入库**：
它们由 Spark 包安装到上表路径，本目录只保存脚本文本。图标产物写入
`~/.local/share/icons/hicolor/`，同样不入库（见 `.codebuddy/rules/仓库组织约定.md`）。

## 2. 启动与全屏

```bash
cd ~/project/lisuan
./starcraft/run-starcraft.sh                 # 前台启动，窗口出现后自动按 Alt+Enter
./starcraft/run-starcraft.sh --status        # 只读体检
STARCRAFT_NO_FULLSCREEN=1 ./starcraft/run-starcraft.sh   # 排障时禁用自动全屏
```

启动器做三件事：

1. 检查前缀是否已被占用（`WINEPREFIX` + `wineserver` 进程环境），命中即显式报错退出；
2. 只在游戏进程树注入 `LIBGL_ALWAYS_SOFTWARE=1` 与
   `__GLX_VENDOR_LIBRARY_NAME=mesa`，不污染 Steam 客户端或全局 shell；
3. 前台运行 `starcraft.exe`；`wmctrl` 发现 `Brood War` 窗口后，
   用 `xdotool key --window <wid> alt+Return` 发送一次按键，由游戏自己切全屏。

**为什么不用窗口管理器硬改状态**：`Alt+Enter` 是《星际争霸》内建的全屏切换，
实测把 640×480 窗口变成 1920×1080、位置 (0,0)；这与 Wine 下的暗黑 2 一样，
是游戏/DirectDraw 层的行为。`wmctrl` 只负责找到窗口，`xdotool` 只负责模拟按键，
都不改游戏文件、不强制改 `_NET_WM_STATE`。

**为什么每次启动都要按**：1.08 不把全屏状态写进注册表；实测 `Alt+Enter` 后
即使干净退出，`HKCU\Software\Blizzard Entertainment\Starcraft` 也没有新增
Video/Display 键，再启动仍是 640×480。因此由启动器每次发一次按键，
不做后台常驻，也不做崩溃后自动重启（见 `.codebuddy/rules/崩溃处理约定.md`）。

**为什么跳过 HanStar Plus 中文启动器**：原桌面项 `com.starcraft.spark.desktop`
启动 `星际争霸中文启动器.exe`，先弹一个 276×158 的启动器窗口，还要再点一次
才能进游戏；直接运行 `starcraft.exe` 已能显示中文界面并正常进主菜单，
启动链更短，也便于在窗口出现时立刻发全屏按键。

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

## 4. 验收状态（2026-11-25 实测）

- 直接启动 `starcraft.exe`：约 4 秒出现 `Brood War` 窗口，640×480；
  合成抓图可见完整主菜单（单人模式、战役编辑器、多人游戏、退出等）。
- 向该窗口发送一次 `Alt+Enter`：窗口变为 1920×1080、位置 (0,0)，
  持续渲染不再变化；未观察到崩溃。
- 干净退出与再启动：无残留 wineserver；启动器会在窗口出现后再次发按键。
- 桌面入口与图标：由 `fix-starcraft-desktop.sh --status` 自检（见脚本输出）。

## 5. 排障速查

```bash
# 只看启动器和全屏按键是否生效
STARCRAFT_WINEDEBUG=err+all ./starcraft/run-starcraft.sh

# 窗口没出现/全屏没生效时，临时禁用自动全屏排查游戏本身
STARCRAFT_NO_FULLSCREEN=1 ./starcraft/run-starcraft.sh --status

# 图标或桌面项异常
./starcraft/fix-starcraft-desktop.sh --status
```

常见现象：

| 现象 | 原因 | 处理 |
| --- | --- | --- |
| 提示前缀已有实例 | 同一 `WINEPREFIX` 上还有 wineserver/游戏进程 | 关闭已开的星际争霸窗口再启动；不做自动拉起 |
| 窗口仍是 640×480 | `xdotool` 不在 PATH、窗口未找到或按键被拦截 | 检查 `run-starcraft.sh --status` 与 `STARCRAFT_WINDOW_WAIT`；不要改成强制改窗口状态的守护 |
| 桌面图标显示为空白 | 图标缓存未刷新或 `Icon=` 被覆盖 | 重跑 `fix-starcraft-desktop.sh`，检查 `gio info` 的 trusted 标记 |
| 游戏崩溃 | 不要用看门狗掩盖 | 按 `.codebuddy/rules/崩溃处理约定.md` 取证（保留日志、core、journalctl） |
