# dreamina 变更记录

> 从仓库根 `CHANGELOG.md` 拆分而来（2026-11-19）。按时间倒序；历史条目保留当时的命令与路径。

## 2026.11.09 — 即梦 AI 桌面图标修复与安装脚本健壮化

### 根因

- 桌面没有即梦图标不是渲染问题：旧 `dreamina-wine.desktop` 的 `Icon` 指向
  **豆包**的 `/opt/apps/doubao-wine/.../doubao-wine.png`；官方图标从未装进
  hicolor 主题；入口只放在 `~/Desktop/`，而 DING 监听 `xdg-user-dir DESKTOP`
  （本机 `~/桌面/`），且没有 `metadata::trusted`。
- 安装脚本另有一处会让**全新前缀装不上**：powershell 替身的判断写反了
  （目标含 `Get-CimInstance` 才替换，而 Wine 原 stub 不含），于是跳过替换，
  electron-builder 安装器把「无进程」误判成「有进程」，`/S` 返回 2 直接 Abort。

### 修复与验证

- 新增 `dreamina/extract-icon.py`：纯标准库解析 PE 的 `RT_GROUP_ICON` / `RT_ICON`，
  从本机官方安装器或已装主程序提取 16–256 共 7 个 PNG 尺寸，写入
  `~/.local/share/icons/hicolor/*/apps/dreamina.png`；兼容 DIB、支持 PNG 兜底扫描，
  不入库厂商图标、不依赖 `icoutils` / `pefile` / `Pillow`。
- `dreamina/install-dreamina.sh` 重写：`set -euo pipefail`；按版本号自动查找安装器；
  wine 环境只注入 wine 进程树；严格校验返回码、主程序 PE 头与 `app.asar`；
  区分超时/Abort；按 `/proc/<pid>/environ` 精确关闭本前缀进程（不做自动拉起）；
  新增 `--desktop-only` / `--status` / `--uninstall` / `--help` 与备份还原。
- `dreamina/run-dreamina.sh` 不再写死主程序名，按安装目录定位非卸载器 exe。
- `packaging/lisuan_fix_core.py` 菜单 28 增加图标/桌面入口的实际生效检查；
  测试套件新增 `tests/test-dreamina-install.sh`（临时 HOME 下验证图标提取、入口生成与卸载）。
- 本机实测 `--desktop-only` 后三个 `.desktop` 均通过 `desktop-file-validate`、
  `trusted=true`，`--status` 全项 `[OK]`，256x256 图标为官方蓝/橙即梦图形。
- 另在全新临时前缀里实跑一遍完整安装：`/S` 返回 0、两个 `powershell.exe` 都带上
  新替身标记、主程序与 `app.asar` 完整、隔离体检全项 `[OK]`；验证了旧脚本
  「全新前缀反而跳过替身替换」的回归已消除。
