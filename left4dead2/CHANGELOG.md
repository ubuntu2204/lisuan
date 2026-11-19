# left4dead2 变更记录

> 从仓库根 `CHANGELOG.md` 拆分而来（2026-11-19）。按时间倒序；历史条目保留当时的命令与路径。

## 2026.10.25 — L4D2 D16 闪退、locale 模态框与启动链诊断修复

### 修复

- **《求生之路 2》D16 / `MatQueue0` 闪退已根治**：gf 虽上报
  `VK_EXT_attachment_feedback_loop_layout`，却拒绝带对应 usage 的深度图；私有 Venus
  服务端现屏蔽该扩展，使 DXVK 回退到合法 usage。启动器不再强制关闭
  `r_flashlightdepthtexture`，系统 gf 驱动和其他游戏不受影响。
- **locale 模态 Warning 已根治**：最终取证确认 `launcher.so` 先设置
  `LC_ALL=en_US.UTF-8`，随后 `linux32/steamclient.so` 调用 `setenv("LC_ALL","C",1)`，
  使引擎读到混合 locale。新版 `setlocale-shim.c` 只重定向 steamclient 的这一次
  赋值，其他调用原样通过；默认启用、默认不写日志，不修改 Steam 二进制。
- **Steam 容器委托不再吞掉真实故障**：`fix-l4d2.sh` 改用 `systemd-run --wait`
  等待并传回游戏退出码（含 `128+signal`），不再 `reset-failed` 或把异常统一写成
  `exit 0`。Venus 未就绪时明确报错，不隐式重启；重复委托直接拒绝。
- **32 位 Venus 冒烟测试假失败已修**：`run-l4d2-venus.sh --test` 复用常驻服务端时，
  清理函数不再把空判断的 1 带成脚本退出码。

### 验证

- 32 位 vkcube 经 Venus 使用 `LISUAN 7G100`，PASS 且退出码为 0。
- `-condebug +map c1m1_hotel +r_flashlightdepthtexture 1` 单次回归：`console.log`
  明确记录 `Map: c1m1_hotel`；同一实例运行 4 分 11 秒后仍活跃，D16 创建错误、
  SIGSEGV、Vulkan 错误和新转储均为 0，随后主动结束测试。
- locale 定点版在真实 `steamclient.so` 调用上成功记录 `LC_ALL=C -> en_US.UTF-8`：
  地图实例约 7 分钟后以 `success/0` 结束；正式部署版地图回归连续 2 分 38 秒
  Warning/D16/致命错误均为 0，不带 `+map` 的普通启动也通过。未自动点击或重启。

### 目录整理与菜单接入

- `left4dead2/` 根目录只保留当前修复、文档和 Venus 构建链；历史探针、旧诊断源码
  与验证截图集中到 `left4dead2/archive/`，附归档说明和新旧路径对照。无引用的
  `.l4d2-window-test.log` 已删除，有效结论保留在文档中。
- 交互索引新增 **22（安装/修复 L4D2）/ 23（L4D2 说明文档）**，分组为
  “求生之路 2”，均不使用 sudo；卸载由 22 的操作列承担。状态检测覆盖 Venus 服务、
  socket、启动器、Steam 启动配置和 locale shim，不再仅看文件或脚本返回码。
- deb 同步打包 L4D2 当前资源与归档源码（排除截图/日志）；Shell 语法、ShellCheck
  和可执行位测试扩展到三级目录，并新增菜单资源、编号与 UNDO 离线检查。
- 卸载不再用旧备份覆盖整个 `steam.desktop`，只撤销 L4D2 注入的 locale，保留
  用户后续的 `GBM_BACKEND=dri` 或其他启动器修改。

---

## 2026.10.06 — 求生之路 2（L4D2）改走 7G100 硬件渲染：废弃原生 32 位软渲染方案

### 决策

用户定下硬性原则：**任何游戏禁用软件渲染，必须 GPU 硬件加速**（已沉淀为长期记忆）。
原生 Linux 版 L4D2 是 32 位 ToGL/OpenGL，厂商无 32 位 gf_dri.so，原生路线永远只能
软件渲染（llvmpipe，帧率不可用）——整体废弃。

### 新方案

游戏目录自带 Windows 版（left4dead2.exe），走与随从大师同源的 Proton new WoW64
路线（`PROTON_USE_WOW64=1`，注意不是 ENABLE）：32 位游戏进程经 wine wow64 thunk
调用 64 位 gf Vulkan ICD，DXVK 把 DX9 翻译成 Vulkan -> 7G100 硬件渲染。

`left4dead2/fix-l4d2.sh` 全部重写：

- 部署自持运行时 `left4dead2/gf-compat/`（osrelease_redirect hook + 协商垫片 ICD +
  驱动库副本；源码复用 `minion-masters/assets-mm-gf-vulkan/`，gcc 现场编译）；
- libSC.so 软链进目标 Proton 库目录（vkCreateDevice 依赖）；
- 目标 Proton 的 DXVK 为 v3.x 时自动降级为 Proton 8.0 的 v2.3（gf 22.0.4 缺
  VK_KHR_load_store_op_none / VK_KHR_maintenance5；实测 Experimental
  v3.1.1 -> v2.3，Proton 11.0 当前已是 v2.3）；
- CompatToolMapping 550 -> proton_experimental + LaunchOptions 写入（保留用户
  参数如 `-language schinese`）；`--status` / `--uninstall` 齐备；
- 卸载只回收指向本 DEPLOY 的 libSC 链接，不误伤随从大师的加固。

### 状态

- 实测：安装流程 exit 0、二次运行全走幂等分支、vdf 写入确认；
- 待重启 Steam 实测游戏内 `mat_info` 显示 LISUAN（而非 llvmpipe）。

---
