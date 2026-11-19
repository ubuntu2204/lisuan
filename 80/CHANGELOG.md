# 80 变更记录

> 从仓库根 `CHANGELOG.md` 拆分而来（2026-11-19）。按时间倒序；历史条目保留当时的命令与路径。

## 2026.11.18 — 80 对战平台：跳过 Wine 下卡住的更新器

- 用户点桌面入口后的日志显示：`03:48:39` 启动后 `update.exe` 运行约 23 秒，
  `03:49:02` 第二次启动停在 `0x80010106`（RPC_E_CHANGED_MODE）并退出，即
  「正在更新」期间重复点击触发单实例冲突后闪退。
- 单次复现：无参数启动会出现 `update.exe`，约 5 秒后退出并派生第二个
  `80launch.exe -debug -noupdate`；直接传 `-noupdate -workdir ...` 则全程只有
  一个进程、不再有更新器，约 4 秒出 1120x623 窗口。
- `80/run-80duizhan.sh` 无参数时默认追加 `-noupdate -workdir <安装目录>`；
  显式传参原样透传，`E80_ALLOW_UPDATE=1` 可恢复更新检查。
- 这是绕过厂商更新器，不是修好它；不加重试/守护/自动重启，需要升级时用
  官网完整安装包重装。详见 `80/README.md` §5。
- 跳过更新器后仍偶发 `c0000005 @0xe2e0a7`，落在 `80launch.exe` 的 `_chkstk`
  栈探测循环；原 PE 栈 1 MiB/4 KiB，`80/fix-80-stack.sh` 改为 8 MiB/64 KiB
  （备份 `.bak-stack`、幂等、可 `--uninstall`）。两次单次启动无崩溃且窗口
  `IsViewable`；这是绕过厂商栈假设，不修 CEF/Wine 栈耗尽。

## 2026.11.17 — 80 对战平台桌面图标与启动修复

- 图标：新增 `80/fix-80-desktop.sh`，从 `80launch.exe` 的 PE 资源提取 8 个尺寸写入
  hicolor；新增 `80/run-80duizhan.sh` 作为桌面入口启动器；修复
  `80/install-80duizhan.sh --status` 因 `find` 收到不存在目录而在 `pipefail` 下误报
  「未安装」。
- 启动：32 位 `80launch.exe` 继承宿主 64 位 `VK_ICD_FILENAMES`（gf ICD），同时
  winex11 探测 `4c54:5006` 的 DRI/OpenGL 失败（`No driver found`），窗口停在
  `IsUnMapped`、再次点击只追加 `Already exist an instance.`。A/B 显示只删 ICD 或
  只加软件 GLX 都不够；`80/run-80duizhan.sh` 现在只在目标 wine 进程树执行
  `env -u VK_ICD_FILENAMES LIBGL_ALWAYS_SOFTWARE=1 __GLX_VENDOR_LIBRARY_NAME=mesa`。
- 验收：单次启动约 6 秒内出现 `80对战平台` 窗口，`Map State: IsViewable` 且
  `wmctrl -l` 可见，启动日志无 `No driver found`。二级 UI 进程的
  `0x80010106`（RPC_E_CHANGED_MODE）在成功出窗的运行中也存在，按非阻塞噪音记录。
  登录 / 房间匹配 / Warcraft III 本体联机未验收；无守护或自动重启。
- 快速 A/B 强杀测试曾触发一次 `wineserver crashed`，已用一次性 `wineserver -k`
  清理前缀状态并复测通过；未加入任何自动恢复逻辑。
