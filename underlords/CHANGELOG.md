# underlords 变更记录

> 从仓库根 `CHANGELOG.md` 拆分而来（2026-11-19）。按时间倒序；历史条目保留当时的命令与路径。

## 2026.09.06 — 刀塔霸业闪退修复：Fossilize pipeline cache 触发厂商 Vulkan 驱动 GPF

### 现象

游戏在宿主直跑 ~73 秒后段错误（status 139，自动重启关闭故直接退出）。

### 根因（journalctl -k 定位）

```
traps: GlobPool0 general protection fault ... in libvulkan_gf.so
```

厂商 Vulkan 驱动 `libvulkan_gf.so` 的 GlobPool0 线程 GPF。排查排除游戏更新
（二进制 08-05 未变）后锁定触发源：**Steam/Fossilize 的 pipeline cache**——
上次游玩（09-05 22:12）写入的 50M `steam_pipeline_cache.foz` 在下次启动被
驱动加载/replay 时触发驱动 bug。崩溃那次运行（00:13）Fossilize 又写入了
replay_cache，形成"越崩越写、越写越崩"的循环。

### 修复

1. 清空 Steam 的 `steamapps/shadercache/1046930/fozpipelinesv6/`（游戏重启后
   Fossilize 自动重建，代价是下次进游戏 shader 重编译、加载稍久）；
2. 保持 `-vulkan` 渲染（实测清缓存后 120s+ 稳定、无内核异常）；
3. 补丁模板 `fix-underlords-native.sh` 顺带补齐 `libudev.so.0 → libudev.so.1`
   软链（Source 2 GL 渲染系统依赖，Ubuntu 24.04 缺失——为将来 GL 后端备用）；
4. 厂商 bug 报告追加 `libvulkan_gf.so` GlobPool0 GPF 案例（附内核 traps 原文）。

### 已尝试并放弃的路径

- **`-gl` 渲染**：libudev 补齐后 GL 渲染系统可加载（运行时确认走
  `gf_dri.so` + `libGLX_gf`），但 Source 2 Linux 的 GL 后端实测**进游戏黑屏**
  （渲染不输出），不可用；
- 由此确认厂商 Vulkan 路线仍是唯一可用渲染路径，排障重点是缓存状态。

### 验证

- 修复前：`-vulkan` 实测 00:13:24 启动 → 00:14:37 GPF（journal 证据）；
- 清缓存后 `-vulkan` 120 秒存活测试通过，stderr 无 GPU 报错、内核无 traps；
- **用户实测佐证**（2026-09-06）：清缓存后进游戏时 Steam 提示
  **"正在编译顶点着色器"**（Fossilize 重新编译而非 replay 坏缓存），
  编译完成后正常进入游戏——与"崩溃发生在 replay 旧缓存阶段"的根因判断吻合。

### 复发排障

再遇 `GlobPool0 ... general protection fault ... libvulkan_gf.so`：
删除 `~/.local/share/Steam/steamapps/shadercache/1046930/fozpipelinesv6/`
后重启游戏即可（详见 fix-underlords-native.md 排障节）。

---

## 2026.08.06 — Dota Underlords 可玩性修复固化

### 新增

- **厂商 DKMS 内核源码开源转录** `dkms/vendor-src/`：
  厂商包 lisuangpu-dkms 262.2.0.56 内的内核源码（69 个 .c + 99 个 .h）本身
  以 GPLv2 / Dual MIT/GPLv2 授权（包内 copyright 与文件头声明），按授权
  原样转录（Makefile 保持出厂未补丁状态，附来源包 SHA-256）；
  闭源 blob（lisuan_proprietary.o.hex）与一切二进制产物不在其中。
  `patches/0001` 可在其上干净应用（新增 `tests/test-vendor-src.sh` 守护）。
- **游戏修复安装器** `underlords/fix-underlords-native.sh`（幂等，菜单 14/15）：
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
