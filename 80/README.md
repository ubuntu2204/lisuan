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

- 本目录**只负责安装**；平台 GUI 启动 / 登录 / 魔兽3 联机尚未在砺算 7G100 上验收。
- 如需运行，参照 `doubao/run-doubao-wine11.sh` 的 wine 进程树隔离方式，不污染 Steam 客户端。
- 崩溃处理：不加剧、不掩盖。采集日志 / dump / journal 后再定位，不加看门狗或自动重启
  （见 `.codebuddy/rules/崩溃处理约定.md`）。
