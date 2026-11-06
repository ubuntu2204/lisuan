---
name: lisuan-7g100-compat
description: This skill should be used when working on the lisuan 7G100 Ubuntu compatibility repository, including driver baseline checks, DKMS and Mesa fixes, Steam CEF, Electron, audio, Proton/Wine, game launchers, install.py menu changes, deb packaging, tests, or troubleshooting. It encodes the required vendor driver v262.2.0.56 and the project-specific rules that must not be bypassed.
---

# 砺算 7G100 兼容性修复

## 适用范围

在 `/home/ubuntu/project/lisuan` 仓库内处理以下任务时加载本技能：

- 砺算 7G100（`PCI 4c54:5006`）驱动、DKMS、Mesa、GBM、Vulkan、音频问题；
- Steam、Electron、Proton/Wine 和具体游戏/桌面应用的启动、渲染或崩溃问题；
- `install.py` / `packaging/lisuan_fix_core.py` 菜单、检测、状态、回滚与 deb 打包；
- 新增 `fix-*` / `verify-*` / `use-*` 脚本、文档、测试或 CI 流程；
- 需要判断项目约定、已知禁忌或历史方案是否仍有效。

## 必守基线

- 将厂商包 `lisuangpu-dkms` / `lisuangpu-lib` **v262.2.0.56** 视为本项目的要求版本和验证基准。任何二进制偏移、驱动补丁、Mesa 兼容性结论与游戏 workaround 都只对这一版本负责。
- 开始改动前先执行 `python3 scripts/check_driver_baseline.py`；发现版本不匹配时明确报告，不擅自套用旧结论。
- 将项目工具版本（日期版本号）与厂商驱动版本严格区分；deb 包版本由构建日期生成，不代表驱动版本。
- 崩溃后禁止自动重启、看门狗、supervisor 或 `Restart=always`。优先定位根因；`underlords` 必须保持 `--no-autorestart`。
- 将 gf 环境变量仅注入目标应用进程树，绝不污染 Steam 客户端或全局 shell。
- 不修改 Steam 自身文件；当前 Steam 修复只改用户桌面启动器的 `Exec` 行，注入 `GBM_BACKEND=dri`。
- 不把 `.codebuddy/` 当作临时缓存删除；项目规则和项目技能位于其中并随仓库分发。

## 首次响应流程

1. 进入仓库根并检查 `git status --short`，确认用户未提交的改动。
2. 执行 `python3 .codebuddy/skills/lisuan-7g100-compat/scripts/check_driver_baseline.py`，记录驱动包实际版本。
3. 执行 `python3 install.py --detect` 或 `python3 install.py --detect --json`，按 GPU、内核、驱动包、DKMS、模块、DRM/DRI、Steam、应用注入标记分类读取结果。
4. 先读取 `references/project-map.md` 定位入口、菜单、脚本、测试和文档；再按问题类型读取 `references/troubleshooting.md`；修改前读取 `references/conventions.md`。
5. 以只读体检和日志证据确定根因，再调用相应 `verify-*` / `fix-*` 脚本；修复后重新执行 `--detect` 和该修复器的 `--status`。
6. 仅在主进程持续存活、目标窗口出现、关键日志健康且重复启动稳定时判定成功。不得把“残留子进程存在”“GPU 进程重启”“renderD128 存在”当作成功证据。
7. 改动完成后执行 `bash tests/run-tests.sh -v`、`git diff --check`，必要时执行 `bash packaging/build-deb.sh <日期版本>` 和 `dpkg-deb --info` 自检。

## 按任务加载参考资料

- 目录结构、菜单规模、双路径入口、测试套件、CI/deb 范围和代码/文档漂移：读取 `references/project-map.md`。
- 分层排障命令、日志、图形栈、Proton 11、Steam CEF、Electron、音频和各游戏既有结论：读取 `references/troubleshooting.md`。
- 仓库组织、开源合规、命名、幂等、回滚、崩溃处理、路径与文档约定：读取 `references/conventions.md`。
- 需要精确行号时优先使用 LSP 或 `search_content`，不要将 65 KB 的 `README.md` 整体读入上下文。大文档推荐搜索模式：`坑|GBM_BACKEND|Vulkan|Player.log|驱动包|测试`。

## 常见修改工作流

### 环境检测或菜单新增

- 在 `packaging/lisuan_fix_core.py` 同步维护 `ITEMS`、`GROUP_ORDER`、`UNDO`、`collect_statuses()`；新增项必须符合“安装/修复 → 卸载 → 说明”顺序。
- 在 `tests/test-repo-health.sh` 同步更新菜单数量、末组、root 权限、doc 与 UNDO 注册断言。
- 保持 `install.py` 为轻量入口壳；资源根始终通过 `_find_base()` 解析，不写死仓库绝对路径。
- 保持 GUI 不可用时明确报错，不静默降级为其他界面。

### 新增修复脚本

- 按用途命名 `fix-*`、`verify-*` 或 `use-*`，放入对应主题目录；提供 `--help`、幂等执行、状态检查和可逆卸载。
- 修改系统文件前生成 `.bak`；检测实际生效标志而非脚本是否曾经运行；容忍用户合法定制。
- 对 root 项由菜单统一加 `sudo`；L4D2 与 PVZ 必须以当前用户运行，不得 sudo。
- 每个游戏/应用文档开头写「至砺算：」；操作指南路径保持最新，排查记录保留原始路径并补路径对照。

### Proton / Wine 任务

- 先区分 64 位原生、32 位 Windows、传统 WoW64 与 new WoW64；不得把 32 位游戏的低 4GB 显存重映射方案套到 64 位应用。
- 使用 Proton 11 的原生 Wine 11 时，显式提供 `WINEDLLPATH`、`LD_LIBRARY_PATH`、独立 `WINEPREFIX` 和 `WINEDLLOVERRIDES`；缺少 vkd3d 路径会造成 `wined3d.dll` 导入失败。
- 新版豆包 2.32.6 在 Wine 8.16 上因缺少 `RtlGetDeviceFamilyInfoEnum` 崩溃；Wine 11 仅越过这一道门槛，仍须完成窗口、主进程和重复启动验收。
- 优先用 Mesa/llvmpipe 软件渲染建立稳定基线，再评估 gf 硬件路径；不要直接降级 Proton 自带 DXVK 或全局添加 `LD_PRELOAD`。

### 打包与发布

- 先运行完整测试，再构建 deb；CI 使用 `.workflow/gitee-go-build-deb.yml` 的 Gitee Go 原生 schema。
- 核对 `packaging/build-deb.sh` 是否包含新增主题目录；`Plants-vs-Zombies/` 与 `doubao/` 当前未进入 deb，相关菜单项在 deb 模式应返回明确的“资源未打包”错误。
- 厂商专有二进制、`.deb`、`.ko`、反编译产物不得入库；保留在本地时，必须在对应目录 README 写明“是什么、为何不能入库、从哪获取”。

## 验证命令

```bash
cd /home/ubuntu/project/lisuan
python3 .codebuddy/skills/lisuan-7g100-compat/scripts/check_driver_baseline.py
python3 install.py --detect
python3 install.py --detect --json
python3 install.py -h
bash tests/run-tests.sh -v
git diff --check
```

技能压缩包输出在仓库根的 `dist/lisuan-7g100-compat.zip`；`dist/` 已被 `.gitignore` 排除，项目技能本体位于 `.codebuddy/skills/lisuan-7g100-compat/` 并随仓库版本控制。
