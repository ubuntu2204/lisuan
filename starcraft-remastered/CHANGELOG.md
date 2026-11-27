# 星际争霸 1 重制版变更记录

## 2026.11.27 — 新增 64 位重制版安装与 Wine 11 启动方案

- 新增独立目录 `starcraft-remastered/`，不复用 32 位 1.08 的旧版前缀。
- 新增 `install.sh`：复制 1.23.10.12409 游戏文件并初始化独立 64 位前缀。
- 新增 `run.sh`：Wine 11 + DXVK 2.3 + lavapipe，支持自定义 Vulkan ICD。
- 修复 `-launch` 启动参数、Wine 11 new WoW64 下 DXVK native 覆盖不生效的问题。
- 实测 gf 原生 ICD 无法创建 Vulkan 1.1 instance，当前默认软件渲染。
- 游戏本体属于厂商专有资产，仅保留本机运行，不纳入 Git。
