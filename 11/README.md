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

- 前缀：`~/.deepinwine/11duizhan-wine11`（与 80 对战平台 Prefix 隔离）。
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

- 本目录**只负责安装**；平台 GUI 启动 / 登录 / 魔兽3 联机尚未在砺算 7G100 上验收。
- 如需运行，参照 `doubao/run-doubao-wine11.sh` 的 wine 进程树隔离方式，不污染 Steam 客户端。
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
