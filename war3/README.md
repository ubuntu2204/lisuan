# KK 官方对战平台（魔兽争霸 III）在砺算 7G100 上的安装

> 至砺算：本主题只涉及 Windows 应用的安装链路，与 gf 驱动 / Mesa / Vulkan 无关；
> 用 Proton 11.0 自带的 wine-11.0 在独立前缀里跑官方 NSIS 安装器即可完成安装。
> 平台 GUI 的启动、登录与渲染另属应用层适配，本次未验收。

## 1. 厂商专有二进制（不入库）

| 项 | 内容 |
| --- | --- |
| **是什么** | `4_pf_files_KKDuiZhanDownloader-v1.0.2.454.exe`：KK 官方对战平台的 NSIS v3.06.1 PE32 下载器安装器（7.9 MB，`requireAdministrator`）。`/S` 静默安装后落地 `C:\Program Files (x86)\kkduizhan\`（本机实测 1.1 GB / 539 个文件），含 `Platform.exe`（48 MB 主程序）、`PlatformDownloader.exe`、`KKEmulator.exe`、`Uninstall.exe` 以及 Warcraft III 1.28/1.29 补丁文件。 |
| **为何不能入库** | 厂商专有二进制、升级会改文件名；按 `.codebuddy/rules/仓库组织约定.md` 二，用 `.gitignore` 第 43 行规则 `war3/4_pf_files_KKDuiZhanDownloader-*.exe` 排除，仓库只保留安装脚本与本文。文件仍留在本目录供运行时使用，不删除。 |
| **从哪获取** | KK 官方对战平台官网/官方分发页下载 `4_pf_files_KKDuiZhanDownloader-v<版本>.exe`，放到本目录、`~/下载` 或 `~/Downloads`，脚本会自动查找。本机这份 SHA256：`418354d8023686a8d1580c18499fc380b471980eb471f59c33ce6d7566875659`。安装产物在 `~/.deepinwine/kkduizhan-wine11/drive_c/Program Files (x86)/kkduizhan/`，同样不入库。 |

## 2. 安装（2026-11-14 实测）

```bash
cd ~/project/lisuan
chmod +x war3/install-kk-platform.sh
./war3/install-kk-platform.sh                 # 自动找安装器，NSIS /S 静默安装
./war3/install-kk-platform.sh --status       # 只读体检
```

- 实现：`war3/install-kk-platform.sh`，前缀 `~/.deepinwine/kkduizhan-wine11`，
  Wine 取自 `~/.local/share/Steam/steamapps/common/Proton 11.0/files/bin/wine`
  （wine-11.0）。只在该 wine 进程树上设置 `WINEPREFIX` / `WINEDLLPATH` /
  `LD_LIBRARY_PATH` / `WINEDLLOVERRIDES=mscoree,mshtml=`，不污染全局 shell 或
  Steam 客户端，不注入 gf 环境变量。
- 实测结果：安装器 `/S` 返回码 `0`；`Platform.exe`、`Uninstall.exe` 均为有效
  PE（MZ 头）；安装目录 1.1 GB / 539 文件；`user.reg` 写入了 `Platform.exe "%1"`
  文件关联；安装器自带 Wine 菜单项
  `~/.local/share/applications/wine/Programs/KK官方对战平台` 与
  `drive_c/users/Public/Desktop/KK官方对战平台.lnk`，脚本不改动它们。
- 脚本幂等：检测到有效 `Platform.exe` 时只做体检，不重复跑安装器；返回码
  `2`（Abort）/`124`（超时）/其它分别报错并保留日志，不做自动重试或守护。
- 环境变量：`KK_PROTON`、`KK_WINE11_PREFIX`、`KK_INSTALL_TIMEOUT`、
  `KK_INSTALL_LOG`、`KK_WINEDEBUG`；日志默认写在 `/tmp/kk-platform-install-*.log`。

## 3. 已知噪音与限制

- 安装日志里的 `failed to get driver name for fd ... 4c54:5006, driver (null)` /
  `No driver found` / `failed to load driver: (null)` 是 wine 初始化时探测 GPU
  的已知噪音（项目其它 Wine 应用同样出现）。安装器返回 0 且产物完整，与砺算
  显卡无关，不需要为此改 drirc 或注入 gf 环境。
- **本次只验收「安装完成」**：没有启动 `Platform.exe`，GUI 窗口、登录、渲染、
  对战功能均未验收；如需验证，应做一次用户可感知的单次冒烟启动并显式收尾，
  不得加看门狗/自动重启（见 `.codebuddy/rules/崩溃处理约定.md`）。
- 卸载用安装目录自带的 `Uninstall.exe /S`（未实测）；脚本不代跑卸载、
  不删除前缀，避免误删用户数据。
- 本主题未接入 `install.py` 菜单；deb 只打包脚本与本文，不含厂商 exe。
