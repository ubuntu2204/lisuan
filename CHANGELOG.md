# 更新日志（CHANGELOG）

本项目采用日期版本号。所有变更均在 Ubuntu 24.04 LTS + 砺算 7G100（PCI `4c54:5006`）
实机验证。

## 2026.08.06 — Dota Underlords 可玩性修复固化

### 新增

- **厂商 DKMS 内核源码开源转录** `lisuan-7g100-ubuntu-fix/vendor-src/`：
  厂商包 lisuangpu-dkms 262.2.0.56 内的内核源码（69 个 .c + 99 个 .h）本身
  以 GPLv2 / Dual MIT/GPLv2 授权（包内 copyright 与文件头声明），按授权
  原样转录（Makefile 保持出厂未补丁状态，附来源包 SHA-256）；
  闭源 blob（lisuan_proprietary.o.hex）与一切二进制产物不在其中。
  `patches/0001` 可在其上干净应用（新增 `tests/test-vendor-src.sh` 守护）。
- **游戏修复安装器** `scripts/fix-underlords-native.sh`（幂等，菜单 14/15）：
  把此前散落在游戏目录的手工修复固化为可复现脚本，一键安装/修复/卸载：
  - `underlords.sh` 补丁：容器分支委托宿主 systemd 直跑（沙箱隐藏 PCI sysfs，
    厂商 Vulkan ICD 在容器内枚举 0 设备，shim 无法绕过——见 bug 报告 §4）；
    段错误智能重试（上限 10 次，180 秒内 3 连崩判定崩溃循环则停止）；
    退出检测（退出时 panorama 析构也段错误，靠 `CSteam3Client::Shutdown`
    日志标记区分"退出时崩"不重启，修复"关一次游戏自动重开"问题）；
    `-nobreakpad` 使崩溃产生内核 core（游戏自带 breakpad 吞 SIGSEGV）；
  - 自动下载补齐 `libpng12.so.0`（Ubuntu 24.04 已移除，Source 2 pango 需要）；
  - 安装桌面项 `dota-underlords-native.desktop`；`--status` / `--uninstall`。

### 实测更正

- 曾把游戏目录的 scout 老 pango 换成宿主新版，崩溃从"玩 30–90 分钟随机出现"
  恶化为"启动即确定性连崩"，已回滚；pango 混搭假设证伪，崩溃确认为
  Source 2 panorama 文本模块自身内存损坏（与显卡/驱动无关），只能靠重试兜底。

## 2026.08.05 — 完整修复栈与运维工具链

### 新增

- **交互式修复索引** `lisuan-fix.sh`（仓库根）：自动检测系统环境并给出结论
  （GPU/内核/DKMS/vermagic/DRI/Steam 补丁状态），编号菜单执行单项修复、体检、
  卸载；支持 `--detect` 与直接传编号的非交互模式。
- **开机自愈服务** `scripts/steam-cef-autofix.sh`：systemd 单元
  `lisuan-steam-autofix.service`，Steam 客户端更新还原 CEF 补丁后开机自动
  重新注入并锁定；`--uninstall` 卸载。
- **卸载能力全覆盖**：每项注入均有对称、幂等的卸载入口——
  - `scripts/uninstall-lisuan-gpu.sh`（撤销 DKMS Makefile 补丁）
  - `scripts/uninstall-lisuan-mesa-driver-name.sh`（删除 DRI 驱动名软链）
  - `scripts/fix-steam-cef-lock.sh --uninstall`（还原 Steam wrap.sh）
  - `scripts/steam-cef-autofix.sh --uninstall`（卸载自愈服务）
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
  - 载体：`scripts/fix-steam-cef-lock.sh` 注入 `steamwebhelper_sniper_wrap.sh`
    并 `chattr +i` 锁定（抵御 Steam 启动校验还原；补 `chmod +x` 防丢执行位）。
- **DRI 驱动加载失败**（`driver=(null)`）：DRM 注册名 `lisuan` 与用户态驱动名
  `gf` 不一致 → `scripts/fix-lisuan-mesa-driver-name.sh` 建立
  `lisuan_dri.so → gf_dri.so` 等软链。
- **内核升级后驱动无法加载**（vermagic 错配/黑屏）：厂商 DKMS Makefile
  `KERNEL_VERSION`/`KERNELVER` 变量名错配 → 一行补丁
  （`patches/0001-dkms-makefile-honor-KERNELVER.patch`）+
  `scripts/fix-lisuan-gpu.sh` 一键重编重装。

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
