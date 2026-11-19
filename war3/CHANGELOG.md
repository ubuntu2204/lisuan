# war3 变更记录

> 从仓库根 `CHANGELOG.md` 拆分而来（2026-11-19）。按时间倒序；历史条目保留当时的命令与路径。

## 2026.11.18 — 魔兽争霸 III（1.24 冰封王座）在 7G100 上跑通

- 新增 `war3/` 主题目录：`run-war3.sh`（单应用启动器，Proton 11 / wine-11.0，独立前缀
  `~/.deepinwine/war3-wine11`）、`fix-war3-desktop.sh`（从 `War3.exe` 提取官方图标 +
  生成桌面入口）、`fix-war3-writable.sh`（授权 `Save/`、`Replay/` 写权限）、`war3/README.md`。
- **根因一（启动即崩）**：32 位 War3 的 D3D8 → vkd3d → Vulkan 链继承了宿主的
  **64 位 gf ICD**（`gf_icd.x86_64.json`），设备初始化失败后进程跳到野地址
  `0x35800000`（`EXCEPTION_ACCESS_VIOLATION`，伴随 `Storm Error : memory never released`）。
  改用同架构的 **32 位 llvmpipe ICD**（`/usr/share/vulkan/icd.d/lvp_icd.json`）后正常出窗；
  该变量只注入目标进程树，不污染 Steam 客户端与全局 shell。
- **根因二（弹框卡死）**：同一 wine 前缀上存在前一个实例时，新实例会弹出
  `Warcraft III was unable to initialize.` 并挂住（双击桌面图标最常见）。受控消融证明
  注册表 `Warcraft III\Video`（含 `lockfb`）**不是**根因。启动器改为启动前检查前缀占用
  并**显式报错退出**，不加看门狗/自动重启。
- 验收：干净启动出现 1920×1080 `Warcraft III` 窗口（`IsViewable`），XWayland 单窗口抓帧
  显示 Frozen Throne 主菜单完整渲染（12.6 万色、非黑像素 72.9%、版本 `1.24.6.387`），
  PipeWire 出现 `Warcraft III` 音频节点；9 次受控启动 0 次崩溃、0 次错误框。
  联机与单局对战未验收。
