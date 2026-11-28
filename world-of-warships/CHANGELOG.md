# 变更记录

## 2026-11-28 — 战舰世界 7G100 硬件加速港口验收

- 定位低帧率根因为 DXVK 回退 llvmpipe 软件渲染，而非游戏画质限制。
- 在 Steam Runtime 容器内验证原版 gf ICD 返回 `-3`、设备数 0，llvmpipe 可用。
- 部署 gf 容器兼容四件套：os-release 重定向、ICD 协商垫片、gf 驱动副本、`libSC.so`。
- 在 shim `init_real()` 中先加载 `libSC.so`，再加载真实 gf ICD，解决设备创建失败。
- 将 DXVK 2.3 的 `d3d11.dll`、`dxgi.dll` 放入游戏主 exe 目录，避免 Proton 覆盖前缀副本。
- 游戏进入亚洲服港口，DXVK HUD 约 77.7 FPS。
- 游戏 CPU 从软件渲染时约 1180% 降至约 134%，llvmpipe 活跃线程为 0。
- 保存港口截图 `images/wows-gf-port.png`。
- 保留 shim、DXVK 和 Steam 配置备份，文档记录回滚步骤。
- 未验收战斗、录像和排位场景。

## 2026-11-28 — 脚本与源码归档

- 新增 `setup-wows-gf.sh`：幂等部署 gf 运行时、编译 shim、部署 DXVK 2.3，可离线写入 Steam LaunchOptions。
- 新增 `run-wows-gf.sh`：单次前台启动，支持直接运行与 `--steam` 两种入口。
- 新增 `uninstall-wows-gf.sh`：停用游戏目录内 DXVK、恢复 shim 备份，不删除游戏或存档。
- 新增 `assets/`：shim 源码、os-release hook、ICD 清单和 Ubuntu 标识。
- shim 源码改为可通过 `GF_RUNTIME_DIR` 编译期注入路径，并默认预加载 `libSC.so`。
- 游戏本体、DXVK DLL 和厂商 `.so` 不纳入目录。
