# 仓库约定与禁止事项

权威规则在 `.codebuddy/rules/仓库组织约定.md` 与 `.codebuddy/rules/崩溃处理约定.md`。本文件是供技能快速加载的提炼版；冲突时以规则原文和当前代码为准。

## 组织与路径

- 项目相关脚本、源码、配置、工具一律放入仓库对应主题目录：`dkms/`、`mesa/`、`steam/`、`electron/`、`audio/`、`minion-masters/`、`underlords/`、`kingdom-rush/`、`left4dead2/`、`Plants-vs-Zombies/`、`doubao/`。
- 不把关键资产留在 home 或 `/tmp`；历史教训是 `~/mm_patch` 与 `~/gf-compat` 曾完全未入库。
- `.codebuddy/` 不是临时缓存；不得删除。`.codebuddy/rules/` 与 `.codebuddy/skills/` 随仓库分发，其余个人配置保持忽略。
- 脚本命名：`fix-*`（改系统、幂等）、`verify-*`（只读体检）、`use-*`（单应用包装）。
- 操作指南类文档必须保持路径最新；排查记录类文档保留当时原始路径，只在文首加“路径说明”对照。
- 每个游戏/应用文档开头写「至砺算：」，说明官方驱动/图形栈需要修复什么。
- Git 不保留 mtime；禁止写依赖文件新旧比较的逻辑。

## 开源合规

以下文件不得提交：

- 厂商 `.deb`、`.ko`、`.o`、`lisuan_proprietary*`；
- `libSC.so`、`libvulkan_gf.so` 等厂商专有运行时；
- 解包的厂商 DKMS 树（除已合规转录的 `dkms/vendor-src/`）；
- 反编译程序集、IL2CPP 互操作 DLL、dump、大体积第三方运行时。

“排除 ≠ 丢弃”：本地保留供运行时加载时，必须在对应主题目录 README 写清三件事：

1. 是什么、做什么用；
2. 为什么不能入库；
3. 从哪获取（具体路径、包名或官方来源）。

`dkms/vendor-src/Makefile` 保持厂商出厂原样；补丁在部署阶段应用，测试会反向验证。

## 修改与测试

- 所有 `fix-*` 提供 `--help`、`--status` / `--uninstall`，重复执行不得改变结果；改系统文件前生成 `.bak`。
- 菜单新增功能同步改 `ITEMS`、`GROUP_ORDER`、`UNDO`、`collect_statuses()` 和 `tests/test-repo-health.sh`。
- 状态检查看实际生效标志，并容忍用户合法定制；不为消除误报把宽松解析改成整行精确匹配。
- L4D2 与 PVZ 修复以当前用户运行，不得 sudo；root 项由菜单/GUI 统一提权。
- GUI 不可用时明确报错退出，不静默降级。
- 改动后自查：全局搜索旧路径 → 检查 `.gitignore` 是否误伤 → `bash tests/run-tests.sh -v` → `git diff --check`。
- CI 先测试后打包；测试失败不得发布。deb 文件名固定 `lisuan-fix.deb`，Tag `deb-latest`。

## 崩溃处理

- 任何游戏或应用崩溃后禁止自动重启、重试、supervisor、看门狗或 `Restart=always`。
- `underlords/fix-underlords-native.sh` 默认自动重启与用户要求冲突，必须保持 `--no-autorestart`。
- 用 `systemd-run --wait` 传真实退出码，不 `reset-failed`，不隐式重启。
- 优先定位根因；若需兜底，采用用户可感知、可控制的方式，并先征求同意。

## 图形环境隔离

- gf 环境变量只作用于目标游戏/应用进程树；绝不污染 Steam 客户端或全局 shell。
- 全局 `LD_PRELOAD=libglapi_gf.so.0` 会让 Steam CEF double free；`lisuan_dri.so -> gf_dri.so` 全局软链会让 Mesa 加载 gf 驱动崩溃。
- Steam 启动只使用 `GBM_BACKEND=dri`；不要修改 Steam 的包装脚本或给文件加 `chattr +i`。
- 修复完成后验证系统驱动文件 md5 与原始备份一致，确保没有全局补丁残留。

## 图形取证（Wayland）

- 判定合成后画面（重影、黑块、坐标错位）只能用 `.codebuddy/rules/Wayland抓图约定.md` 规定的 Mutter ScreenCast + PipeWire 通道；`xwd` / `ffmpeg x11grab` / PIL `ImageGrab` 在 XWayland 下全是黑的，不得作为渲染故障证据。
- 抓帧必须在同一条 D-Bus 连接内建会话并显式 Stop，不留后台录屏；不做自动重试或守护。

## 技能自身维护

- 驱动基线变化时，同步更新 `install.py`、`packaging/lisuan_fix_core.py`、技能 `SKILL.md`、参考文档、`scripts/check_driver_baseline.py` 与 `tests/test-repo-health.sh`。
- 技能本体位于 `.codebuddy/skills/lisuan-7g100-compat/`，需保持被 git 跟踪；分发压缩包输出到 `dist/`，由现有 `.gitignore` 排除。
- 新增参考文档后重新执行：

```bash
python3 /usr/share/buddycn/resources/app/extensions/genie/out/extension/builtin/skill-creator/scripts/quick_validate.py \
  .codebuddy/skills/lisuan-7g100-compat
python3 /usr/share/buddycn/resources/app/extensions/genie/out/extension/builtin/skill-creator/scripts/package_skill.py \
  .codebuddy/skills/lisuan-7g100-compat dist
```
