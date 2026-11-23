# 更新日志（CHANGELOG）

本项目采用日期版本号。所有变更均在 Ubuntu 24.04 LTS + 砺算 7G100（PCI `4c54:5006`）
实机验证。

> 各项目的详细变更记录已按主题拆分到对应项目目录的 `CHANGELOG.md`；
> 本文件只保留仓库级条目与项目索引。历史条目保留当时的命令与路径。

## 项目变更日志索引

| 项目 | 变更记录 |
|---|---|
| `.codebuddy/skills/lisuan-7g100-compat/` | [`.codebuddy/skills/lisuan-7g100-compat/CHANGELOG.md`](.codebuddy/skills/lisuan-7g100-compat/CHANGELOG.md) |
| `11/` | [`11/CHANGELOG.md`](11/CHANGELOG.md) |
| `Plants-vs-Zombies/` | [`Plants-vs-Zombies/CHANGELOG.md`](Plants-vs-Zombies/CHANGELOG.md) |
| `audio/` | [`audio/CHANGELOG.md`](audio/CHANGELOG.md) |
| `doubao/` | [`doubao/CHANGELOG.md`](doubao/CHANGELOG.md) |
| `electron/` | [`electron/CHANGELOG.md`](electron/CHANGELOG.md) |
| `kingdom-rush/` | [`kingdom-rush/CHANGELOG.md`](kingdom-rush/CHANGELOG.md) |
| `left4dead2/` | [`left4dead2/CHANGELOG.md`](left4dead2/CHANGELOG.md) |
| `mesa/` | [`mesa/CHANGELOG.md`](mesa/CHANGELOG.md) |
| `minion-masters/` | [`minion-masters/CHANGELOG.md`](minion-masters/CHANGELOG.md) |
| `packaging/` | [`packaging/CHANGELOG.md`](packaging/CHANGELOG.md) |
| `steam/` | [`steam/CHANGELOG.md`](steam/CHANGELOG.md) |
| `underlords/` | [`underlords/CHANGELOG.md`](underlords/CHANGELOG.md) |
| `war3/` | [`war3/CHANGELOG.md`](war3/CHANGELOG.md) |

> 2026.10.14 Navicat 18 Premium Lite（AppImage）启动闪退条目已迁至 hygon 仓库
> `CHANGELOG.md`（脚本与文档在 `~/project/hygon/navicat/`）；本仓库不再保留该条目。
>
> 2026.11.23 即梦 AI（Dreamina）1.0.59 补丁与文档已迁至 hygon 仓库
> `Dreamina/`（`~/project/hygon/Dreamina/`）；菜单 28/29、deb 打包目录、体检函数与
> `tests/test-dreamina-install.sh` 一并撤销。`11/` 复用的图标提取器改为同目录副本。

---

## 2026.10.06 — 全部游戏/应用文档新增「至砺算：」章节

本项目主要写给砺算官方开发人员。6 份游戏/应用文档（steam / underlords /
minion-masters / electron / kingdom-rush / left4dead2）开头统一新增
**「至砺算：官方实现什么可以让本游戏（应用）原生运行」**：

- 按投入产出比排序，每项 = 官方要实现什么 + 已实测的根因证据 + 落地后
  退役的 workaround；
- 共性诉求高频出现：ICD 规范化（协商接口 / 实例扩展）、基于渲染节点的
  容器内设备枚举（不依赖 /sys/bus/pci）、libSC 自解析、去除 os-release
  白名单与 MAP_32BIT 约束、补齐 DXVK v3.x 所需扩展、GLVND 标准集成、
  gf_gbm 与 libgbm 25.x ABI 兼容；
- 32 位用户态驱动（i386）作为 left4dead2 的最彻底解法单独列出。

同批：`left4dead2/README.md` 从已废弃的软渲染方案重写为 Proton WoW64
GPU 方案（2026.10.06 条目），并同步至砺算诉求。


---

## 2026.08.05 — 完整修复栈与运维工具链

### 新增

- **交互式修复索引** `lisuan-fix.sh`（仓库根）：自动检测系统环境并给出结论
  （GPU/内核/DKMS/vermagic/DRI/Steam 补丁状态），编号菜单执行单项修复、体检、
  卸载；支持 `--detect` 与直接传编号的非交互模式。
- **开机自愈服务** `steam/steam-cef-autofix.sh`：systemd 单元
  `lisuan-steam-autofix.service`，Steam 客户端更新还原 CEF 补丁后开机自动
  重新注入并锁定；`--uninstall` 卸载。
- **卸载能力全覆盖**：每项注入均有对称、幂等的卸载入口——
  - `dkms/uninstall-lisuan-gpu.sh`（撤销 DKMS Makefile 补丁）
  - `mesa/uninstall-lisuan-mesa-driver-name.sh`（删除 DRI 驱动名软链）
  - `steam/fix-steam-cef-lock.sh --uninstall`（还原 Steam wrap.sh）
  - `steam/steam-cef-autofix.sh --uninstall`（卸载自愈服务）
- **厂商 bug 报告** `docs/vendor-bug-report-gf-gbm-crash.md`：三个问题
  （gf_gbm 段错误、容器内 EGL 分发、GLX vendor 注册缺失）的完整证据链、
  复现步骤、规避方案与修复建议，含规范化报告头（编号/日期/严重级别/状态）。
- `docs/system-mesa-analysis.md`：7G100 与系统 Mesa 25.2.x 的源码级兼容性分析。
- 测试套件（`tests/`）：补丁应用/反向、Makefile 逻辑、fix 脚本幂等性、
  shell 语法（含仓库根入口脚本）、仓库合规（无厂商产物混入）；
  Gitee Go 流水线（`.workflow/gitee-go-test.yml`）自动执行。
- Dota Underlords 宿主直跑方案：`play-native.sh` + 桌面启动项
  （绕开容器内 GLX 无解问题；补 libpng12 依赖）。

### 修复

- **Steam 界面无法启动**（`cef_log.txt` 大量 `exit_code=139`）：
  - 根因一：容器内系统 libgbm 加载厂商 `gf_gbm.so`（Mesa 22.0.4 时代后端）
    段错误 → 注入 `GBM_BACKEND=dri` 规避；
  - 根因二：Steam 自带 ANGLE 被 `LD_LIBRARY_PATH=.` 优先加载，延迟绑定的
    GL 符号在容器内无 glapi 提供者 → `LD_PRELOAD` 厂商 `libglapi_gf.so.0` 规避；
  - 载体：`steam/fix-steam-cef-lock.sh` 注入 `steamwebhelper_sniper_wrap.sh`
    并 `chattr +i` 锁定（抵御 Steam 启动校验还原；补 `chmod +x` 防丢执行位）。
- **DRI 驱动加载失败**（`driver=(null)`）：DRM 注册名 `lisuan` 与用户态驱动名
  `gf` 不一致 → `mesa/fix-lisuan-mesa-driver-name.sh` 建立
  `lisuan_dri.so → gf_dri.so` 等软链。
- **内核升级后驱动无法加载**（vermagic 错配/黑屏）：厂商 DKMS Makefile
  `KERNEL_VERSION`/`KERNELVER` 变量名错配 → 一行补丁
  （`patches/0001-dkms-makefile-honor-KERNELVER.patch`）+
  `dkms/fix-lisuan-gpu.sh` 一键重编重装。

### 实测更正（相对早期分析记录）

- 厂商 **EGL 在宿主可用**（X11/XCB/Wayland/GBM/surfaceless 平台，EGL 1.5）；
  Steam 容器内 UI 无硬件加速的真正卡点是容器 glvnd 分发拿不到厂商 EGL，
  而非"厂商 EGL 缺失"。
- 厂商 **Vulkan ICD 在容器内工作正常**（设备枚举/创建无崩溃）；
  容器内原生游戏崩溃的根因是 **GLX vendor 注册缺失**，而非 Vulkan 问题。

## 2026.07.31 — 初始修复

- DKMS Makefile `KERNELVER` 错配的一行补丁与一键修复脚本
  （`fix-lisuan-gpu.sh`）、只读体检（`verify-lisuan-gpu.sh`）。
- Apache-2.0 授权，LICENSE/NOTICE 完备；明确不收录任何厂商专有产物。
