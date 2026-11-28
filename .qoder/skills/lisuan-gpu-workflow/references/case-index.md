# 项目案例索引

优先复用已有脚本和文档，不重复造轮子。以下路径均相对于项目根目录。

## 总览与规则

| 文件 | 用途 |
| --- | --- |
| `README.md` §7.2 | 按层索引的踩坑清单与图形栈层次 |
| `.codebuddy/rules/崩溃处理约定.md` | 禁止自动重启/看门狗，保留崩溃证据 |
| `packaging/lisuan_fix_core.py` | 菜单、状态检测、Steam 桌面入口修复的统一实现 |
| `install.py` | 项目安装与体检入口 |

## Steam / Proton / Vulkan

| 文件 | 已验证经验 |
| --- | --- |
| `steam/steam-on-lisuan.md` | pressure-vessel 架构、CEF 崩溃、GLX/GBM 四项修复、环境隔离 |
| `minion-masters/fix-minion-masters-7g100-hw.sh` | gf 容器四件套、DXVK 2.3、Steam LaunchOptions、低 4GB shim |
| `minion-masters/assets-mm-gf-vulkan/libvulkan_gf_shim.c` | ICD 协商与扩展过滤垫片源码 |
| `minion-masters/assets-mm-gf-vulkan/osrelease_redirect.c` | os-release 与低地址映射 hook 源码 |
| `minion-masters/gf-compat/` | 已部署运行时与源码；厂商 `.so` 不入库 |
| `doubao/run-doubao-wine11.sh` | 独立部署 gf-accel、DXVK 2.3、Chromium 合成折中 |
| `_work/vk-loader-probe.py` | 历史 Vulkan loader 探针；新任务优先用 Skill 自带脚本 |

## 32 位与 Wine

| 文件 | 已验证经验 |
| --- | --- |
| `war3/run-war3.sh`、`war3/README.md` | 32 位 War3 使用 32 位 llvmpipe；同前缀单实例 |
| `Plants-vs-Zombies/README.md` | new WoW64、wined3d GL、低 4GB 映射、软件渲染回退 |
| `Plants-vs-Zombies/fix-plants-vs-zombies.sh` | PvZ 单实例、渲染模式切换、前缀迁移 |
| `left4dead2/fix-l4d2.sh` | 原生 32 位 Vulkan → Venus → gf 的容器/宿主桥接 |
| `underlords/fix-underlords-native.sh` | 原生 Linux 游戏与 Steam 容器边界 |
| `11/fix-11-render.sh`、`11/fix-11-war3path.sh` | 平台注入件禁用、注册表路径、游戏目录可写 |
| `kk/` | Wine user32 wrapper、X11 geometry shim、Web 子窗层级修复 |

## Chromium / Electron

| 文件 | 已验证经验 |
| --- | --- |
| `electron/use-electron-gf.sh` | 通用 Electron 基座，gf env 仅单应用注入 |
| `electron/README.md` | 实例扩展 `-7`、XWayland 三件套、合成破线、桌面入口 |
| `electron/use-zcode.sh` | ZCode 稳定软件模式 |
| `electron/use-qoder.sh` | Qoder 入口与失效会话清理 |
| `electron/trae/` | Trae 卡顿与 gf EGL/ANGLE 显示错误分开处理 |

## Mesa / 内核 / 验证

| 文件 | 已验证经验 |
| --- | --- |
| `mesa/system-mesa-analysis.md` | 厂商 Mesa 22.0.4 fork 与系统 25.2 不兼容、zink 路径 |
| `dkms/fix-lisuan-gpu.sh` | DKMS vermagic 与驱动加载修复 |
| `dkms/patches/` | 内核构建变量补丁 |
| `_work/shot.py`、`_work/xwd2png.py` | X11 根窗口/窗口截图与 XWD 转 PNG |
| `_work/list_windows.py` | 窗口枚举与映射状态检查 |
| `tests/` | 仓库健康、脚本语法与状态检测测试 |

## World of Warships（AppID 552990）

| 项目 | 实测结果 |
| --- | --- |
| 运行时 | `~/.local/share/worldofwarships-gf-accel/` |
| 项目脚本 | `world-of-warships/setup-wows-gf.sh`、`run-wows-gf.sh`、`uninstall-wows-gf.sh` |
| 源码 | `world-of-warships/assets/`，不含游戏、DXVK DLL 或厂商 `.so` |
| DXVK | 2.3 DLL 放在 `World of Warships/bin/13533378/bin64/`，避免 Proton 覆盖前缀副本 |
| libSC | shim `init_real()` 先 `dlopen(libSC.so, RTLD_GLOBAL)`，再加载真实 gf ICD |
| Steam | `localconfig.vdf` 已持久化 gf LaunchOptions |
| 验收 | 港口约 77 FPS；CPU 约 134%；llvmpipe 线程 0；DXVK 识别 7G100 |
| 边界 | 已验证港口，未逐项验收战斗场景 |

## 桌面快捷方式

- 使用 `xdg-user-dir DESKTOP` 查询真实桌面目录。
- `Icon=` 使用 PNG 等桌面环境可直接读取的格式，不依赖 `.exe` 内嵌图标。
- GNOME/DING 需要 `metadata::trusted=true` 和可执行位。
- 修改 wrapper 路径后同步更新 `~/.local/share/applications` 与桌面副本。

## 经验适用边界

- 驱动包版本、用户态 Vulkan 版本、内核模块版本是三层事实，不能互相替代。
- “窗口存在”“打开 render node”“加载 gf 库”均不能单独证明硬件渲染。
- 同一方案在 32 位、64 位、传统 wow64、new WoW64、容器内、宿主直跑下可能完全不同。
- 历史结论必须用当前日志复验；文档过时时以代码、配置和实测证据为准。
