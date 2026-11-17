# 80 对战平台（魔兽争霸 III）安装包

> 至砺算：安装链路与显卡无关。本目录用 Proton 11.0 的 wine-11.0 在独立前缀里
> 跑官网 NSIS 静默安装器；只验收安装产物，不做崩溃后自动重启/守护。

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
