# 星际争霸 1 重制版 1.23.10.12409（Wine 11）

本目录独立于 `starcraft/`（32 位 1.08）：重制版是 PE32+ 64 位程序，使用
独立游戏目录与 Wine 前缀，不复用旧版 DirectDraw 渲染链。

## 安装与启动

```bash
cd ~/project/lisuan
./starcraft-remastered/install.sh
./starcraft-remastered/run.sh
./starcraft-remastered/run.sh --status
```

默认源目录为 `/home/ubuntu/development/starcraft1.23.10.12409`，也可通过首个参数
或 `SC_REMASTERED_SOURCE` 指定。安装脚本校验 64 位 `StarCraft.exe` 与游戏数据，
优先使用 reflink 复制，不改源目录；已安装时直接退出。

| 项 | 默认值 |
| --- | --- |
| 游戏目录 | `starcraft-remastered/game/` |
| Wine 前缀 | `~/.deepinwine/StarCraft-Remastered-wine11` |
| Wine | Proton 11.0 自带的 Wine 11 |
| DXVK | `minion-masters/dxvk23/x86_64-windows`（2.3） |
| Vulkan | `/usr/share/vulkan/icd.d/lvp_icd.json`（lavapipe 软件渲染） |

DXVK 2.3 用于兼容本机 gf 驱动缺少 `VK_KHR_maintenance5` 的限制。安装脚本会把
DXVK 的 `d3d9/d3d10core/d3d11/dxgi` 部署到游戏目录与独立前缀，并在前缀注册表
设置 native 覆盖；仅设 `WINEDLLOVERRIDES` 在 Wine 11 new WoW64 下对
`dxgi/d3d11` 不生效。启动器只对游戏进程树注入环境变量，单次前台运行，不做自动
重启、看门狗或守护。

实测 gf 原生 ICD 无法创建 Vulkan 1.1 instance，32 位 Venus 链路也不适用于该
64 位程序，因此当前默认使用 lavapipe 保证可运行。可用 `SC_REMASTERED_VK_ICD`
指定其它 ICD 做后续硬件验证。

## 第三方资产与不入库说明

`game/` 是暴雪《星际争霸 1 重制版》厂商专有游戏文件，受版权保护，不能开源，
因此不纳入 Git。其获取来源为用户本机目录
`/home/ubuntu/development/starcraft1.23.10.12409`，版本为 `1.23.10.12409`；
仓库只保存安装/启动脚本和说明。`.gitignore` 排除整个运行目录。

## 验证记录（2026-11-27）

- `install.sh` 完成约 12 GB 游戏文件复制、Wine 11 64 位前缀初始化和 DXVK 部署。
- 按随包 `.bat` 传入 `-launch` 后，成功创建 `Brood War` 窗口、播放开场动画，
  并进入 1920×1080 主菜单/选项界面。
- DXVK 日志确认加载 native `dxgi.dll`、`d3d11.dll`，使用
  `D3D_FEATURE_LEVEL_11_1`。
- gf 原生 Vulkan 与 Venus 均未通过设备枚举，当前交付为 lavapipe 软件渲染方案。
