# 项目地图与当前代码事实

记录时间：2026-11-06。以代码为准；README 中部分数量描述已落后，见文末“文档漂移”。

## 版本基线

| 项目 | 值 |
|---|---|
| 厂商驱动要求 / 验证基准 | `lisuangpu-dkms` / `lisuangpu-lib` **v262.2.0.56** |
| 核心常量 | `packaging/lisuan_fix_core.py::REQUIRED_LISUAN_DRIVER_VERSION` |
| 安装器说明 | `install.py` 顶层 docstring 与 `python3 install.py -h` |
| 检测输出 | `detect()` 的“驱动要求 / 驱动包 / 用户态库” |
| 项目工具版本 | 日期版本，如 `2026.10.25`；deb 版本由构建日期生成，与驱动版本无关 |
| 已验证环境 | Ubuntu 24.04.4、HWE 内核、海光 CPU、砺算 7G100 `4c54:5006` |

## 四层入口

- `install.py`：轻量入口壳；`_bootstrap()` 将 `packaging/`、`here` 或 `/usr/share/lisuan-fix` 加入 `sys.path`，随后调用 `lisuan_fix_core.main()`。
- `packaging/lisuan_fix_core.py`：环境检测、菜单、状态、执行、回滚和 CLI。`_find_base()` 要求资源根同时存在 `dkms/` 与 `steam/`。
- `packaging/lisuan_fix_gui.py`：Tkinter GUI；`gui_available()` 会实际创建一次 `tk.Tk()`。
- `packaging/lisuan_fix_md.py`：说明弹窗的轻量 Markdown 渲染。

常用 CLI：

```bash
python3 install.py                  # GUI
python3 install.py --detect
python3 install.py --detect --json
python3 install.py <菜单编号> [参数]
python3 install.py <菜单编号> --undo
python3 install.py -h
```

## 菜单与状态

- `ITEMS`：25 项，编号 1–25；`GROUP_ORDER`：11 组。
- `ITEMS_BY_ID` 从 `ITEMS` 自动构建；`UNDO` 有 14 个可卸载项。
- 分组顺序：环境检测 → DKMS → Mesa → Electron → Steam → 随从大师 → 刀塔霸业 → 王国保卫战 → 植物大战僵尸 → 音频 → 求生之路 2。实际末组以 `GROUP_ORDER[-1]` 为准，测试断言为“求生之路 2”。
- `detect()` 覆盖 GPU、内核、驱动包、DKMS、`gf` 模块及 vermagic、`/dev/dri`、有害 DRI 软链、Steam 桌面项与旧注入残留、自愈服务、游戏注入标记；`hint` 可指向菜单编号。
- `collect_statuses()` 用实际生效标志着色 GUI；不得用脚本文件是否存在替代。

## 主题目录

| 目录 | 作用 |
|---|---|
| `dkms/` | 内核模块修复、只读体检、KERNELVER 补丁、厂商源码转录与厂商缺陷报告 |
| `mesa/` | 系统 Mesa、DRI/GBM/Vulkan ICD 分析与体检 |
| `steam/` | Steam CEF 根因文档和历史方案清理工具 |
| `electron/` | 通用 Electron/Chromium wrapper 与应用入口 |
| `audio/` | 海光板载 HDA 音频修复和 BAR0 探针源码 |
| `minion-masters/` | 随从大师硬件渲染、低 4GB 重映射、MMPatch、DXVK 2.3 |
| `underlords/` | 刀塔霸业原生 64 位启动修复 |
| `kingdom-rush/` | 王国保卫战启动修复 |
| `left4dead2/` | 原生 L4D2 + Venus 转发、locale shim、历史归档 |
| `Plants-vs-Zombies/` | 32 位 PVZ、wined3d GL、Steam 启动选项修复 |
| `doubao/` | 豆包 2.32.6 的 Steam Runtime 容器启动器、桌面入口修复、安装器与排障记录；当前无菜单项 |
| `packaging/` | 核心模块、GUI、deb 构建脚本 |
| `tests/` | 离线回归套件与 fixtures |
| `.codebuddy/rules/` | 仓库组织约定、崩溃处理约定 |
| `.codebuddy/skills/` | 项目级技能（本技能所在目录） |

## 关键脚本

- 体检：`dkms/verify-lisuan-gpu.sh`、`mesa/verify-system-mesa.sh`、`python3 install.py --detect`。
- 修复：`dkms/fix-lisuan-gpu.sh`、`audio/fix-lisuan-hdmi-audio.sh`、`underlords/fix-underlords-native.sh --no-autorestart`、`left4dead2/fix-l4d2.sh`、`Plants-vs-Zombies/fix-plants-vs-zombies.sh`、`minion-masters/fix-minion-masters-7g100-hw.sh`。
- 单应用 wrapper：`electron/use-electron-gf.sh --app <name> --install-desktop`。
- 技能驱动检查：`.codebuddy/skills/lisuan-7g100-compat/scripts/check_driver_baseline.py`。

## 测试与构建

`tests/run-tests.sh` 收集 9 个 `test-*.sh`：

1. `test-patch.sh`
2. `test-makefile-logic.sh`
3. `test-fix-script.sh`
4. `test-scripts-syntax.sh`
5. `test-shellcheck.sh`
6. `test-audio-status.sh`
7. `test-repo-health.sh`
8. `test-vendor-src.sh`
9. `test-plants-vs-zombies.sh`

`test-repo-health.sh` 断言 25 个菜单项、L4D2/PVZ 注册、UNDO、驱动基线常量、项目技能存在且未被 `.gitignore` 排除。

deb 构建：`packaging/build-deb.sh <VER>`，输出 `packaging/dist/lisuan-fix_<VER>_all.deb`。CI 是 `.workflow/gitee-go-build-deb.yml`，Gitee Go 原生 schema，固定产物 `lisuan-fix.deb`，Tag `deb-latest`。

## 文档漂移（以代码为准）

| 项 | README/注释旧描述 | 代码实际 |
|---|---|---|
| 菜单 | 10 组 23 项 | 25 项 11 组，`tests/test-repo-health.sh` 断言 25 |
| 主题目录 | 9 个 | 11 个，`kingdom-rush/`、`doubao/` 未列入旧表 |
| 测试套件 | 6 / 7 个 | 9 个 |
| deb 覆盖 | core 注释称不含 `minion-masters/` | `build-deb.sh` 已精简打包；真正未打包的是 `Plants-vs-Zombies/` 与 `doubao/` |
| Steam 方案 | 旧脚本头仍描述注入和 `chattr +i` | 当前只改桌面 `Exec` 注入 `GBM_BACKEND=dri`；旧脚本仅用于清理残留 |

## 关键文档

- `README.md`：总索引；重点搜索 `驱动包|坑|GBM_BACKEND|Vulkan|Player.log|测试`。
- `CHANGELOG.md`：按日期记录每次验证结论。
- `.codebuddy/rules/仓库组织约定.md`、`.codebuddy/rules/崩溃处理约定.md`。
- `dkms/vendor-src/README.md`、`dkms/vendor-bug-report-gf-gbm-crash.md`。
- `mesa/system-mesa-analysis.md`、`steam/steam-on-lisuan.md`。
- 各主题目录 README；`doubao/README.md` 记录 2.32.6 经 Steam Runtime 容器跑通的
  根因、消融证据与桌面入口回归修复。
