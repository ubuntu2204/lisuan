# 《求生之路 2》（Left 4 Dead 2, AppID 550）无响应修复

> 状态：**诊断完成，依赖待安装后验证**（2026-09-15）
> 修复脚本：`./fix-l4d2.sh`（幂等，支持 `--status` / `--launch` / `--uninstall`）

---

## 一、TL;DR

```bash
# 1) 补 32 位依赖（缺 32 位 OpenAL → engine.so 加载失败）
sudo dpkg --add-architecture i386 && sudo apt update
sudo apt install -y libopenal1:i386

# 2) 写入宿主直跑启动选项（保留你原有的参数，如 -language schinese）
bash fix-l4d2.sh

# 3) 重启 Steam → 从库里启动《Left 4 Dead 2》
bash fix-l4d2.sh --status     # 随时查看依赖/启动选项/进程
```

---

## 二、症状

- 从 Steam 启动后**窗口卡死 / 无响应**，数分钟后 Steam 移除进程：
  ```
  [2026-09-15 05:22:25] Game process added : AppID 550
  [2026-09-15 05:28:25] Game process removed: AppID 550
  ```
- **内核无 traps / segfault**（`journalctl -k` 干净）——不是崩溃，是**挂起**。

## 三、根因（三层，逐层实测定位）

| # | 事实 | 证据 |
|---|---|---|
| 1 | L4D2 是**原生 Linux 32 位**游戏（不经过 Proton/Wine） | `file hl2_linux` → `ELF 32-bit LSB executable`；启动命令走 `hl2.sh -game left4dead2` |
| 2 | Steam 默认把它放进 **pressure-vessel 容器**运行（SteamLinuxRuntime_soldier + scout-on-soldier） | console_log 里的启动命令行 |
| 3 | **宿主直跑暴露真正错误**：`Unable to load module engine.so` | 手动 `STEAM_RUNTIME=0 ./hl2.sh …` 的 stderr |
| 4 | `engine.so` 缺 **32 位 `libopenal.so.1`**（OpenAL 音频库） | `ldd bin/engine.so`（带 `LD_LIBRARY_PATH=.`）→ `libopenal.so.1 => not found`；系统 `/usr/lib/i386-linux-gnu/libopenal.so.1` 不存在 |
| 5 | 厂商 **只有 64 位** gf 驱动，无 32 位 `gf_dri.so` | 64 位 `/usr/lib/x86_64-linux-gnu/dri/gf_dri.so` 有；32 位 `dri/` 里没有 gf |
| 6 | 宿主有 **32 位 Mesa 软渲染**兜底 | `/usr/lib/i386-linux-gnu/dri/swrast_dri.so`、`kms_swrast_dri.so` 存在 |

**所以**：容器内 32 位图形/音频栈不完整 → 挂起；宿主直跑则因缺 32 位 OpenAL 加载失败。

## 四、修复设计

1. **宿主直跑**（`STEAM_RUNTIME=0`）：绕开 pressure-vessel 容器，直接用宿主 32 位 Mesa。
2. **32 位软渲染**（`__GLX_VENDOR_LIBRARY_NAME=mesa LIBGL_ALWAYS_SOFTWARE=1`）：
   厂商无 32 位驱动，只能用 swrast；对 L4D2（Source 引擎、低画质可跑）够用。
3. **补 32 位依赖**：至少 `libopenal1:i386`（脚本会自动检测并给出缺失清单与安装命令）。

脚本写入的启动选项（幂等，且**保留你原有参数**）：

```
STEAM_RUNTIME=0 __GLX_VENDOR_LIBRARY_NAME=mesa LIBGL_ALWAYS_SOFTWARE=1 <原有参数> %command%
```

> ⚠️ Steam 运行时会把自己内存里的值写回 `localconfig.vdf`——**建议先退出 Steam 再改**，
> 改完重启 Steam。

## 五、验证（待依赖安装完成后执行）

```bash
bash fix-l4d2.sh --status        # 依赖应"齐全"、启动选项应含 STEAM_RUNTIME=0
bash fix-l4d2.sh --launch        # 宿主直跑一次，观察窗口是否出现、能否进主菜单
```

观察点：
- 无 `Unable to load module engine.so`
- 出现 GL 初始化日志（`This system supports the OpenGL extension …`）
- 能进主菜单；游戏内把分辨率/画质调低以适配软渲染

## 六、已知限制

- **只能软渲染**（无 32 位 gf 驱动）：帧率依赖 CPU，建议 720p + 低画质。
- 32 位 Vulkan / 硬件加速：需厂商发布 32 位驱动才可行（与随从大师的
  "32 位游戏 × 64 位驱动"是同一类架构限制，见 `minion-masters/fix-minion-masters.md`）。

## 七、相关

- 刀塔霸业（Source 2 原生 Linux，容器委托宿主直跑）：`underlords/`
- 随从大师（Proton + WoW64 + 低 4GB 重映射）：`minion-masters/`
