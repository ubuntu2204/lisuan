# 刀塔霸业（Dota Underlords）修复：Steam 内一键启动

> 适用机型：**砺算 7G100 GPU + Ubuntu 24.04 + Steam（pressure-vessel 容器）**。
> 现象：Steam 里点"开始游戏"能正常进入并游玩（已实测）。
> 修复脚本：`lisuan-7g100-ubuntu-fix/scripts/fix-underlords-native.sh`

---

## TL;DR

刀塔霸业是 Source 2 原生 Linux 游戏，但它运行在 Steam 的 **pressure-vessel 容器**里，
而容器内厂商图形栈完全不可用（与 Steam 界面 GPU 进程崩溃同源）：

1. 沙箱隐藏 `/sys/bus/pci` → 厂商 Vulkan ICD 枚举不到 GPU（0 设备）；新版 Vulkan loader 还会以
   "loader interface version" 为由拒绝厂商 ICD；
2. 容器内 glvnd 无法注册厂商 GLX vendor（无 vendor JSON 目录）；
3. 厂商 EGL 在容器内拿不到 X11 visual。

**规避方案**：把游戏弹出沙箱、在**宿主直跑**（宿主图形栈完全正常）。
实现上 `underlords.sh` 检测到自己在容器内时，通过共享的会话 D-Bus 调用宿主
`systemd-run --user` 在宿主启动游戏，并阻塞到游戏退出，Steam 的运行时长/退出检测依然有效。

```bash
sudo ./scripts/fix-underlords-native.sh                  # 安装 / 修复（默认开启自动重启）
sudo ./scripts/fix-underlords-native.sh --no-autorestart # 安装并彻底关闭自动重启
sudo ./scripts/fix-underlords-native.sh --uninstall
./scripts/fix-underlords-native.sh --status
```

部署后从 Steam 库里点"开始游戏"即可正常玩；也可从桌面项
`dota-underlords-native.desktop` 不依赖 Steam 启动链路直接玩。

---

## 根因与规避原理

完整证据链见 [`vendor-bug-report-gf-gbm-crash.md`](../docs/vendor-bug-report-gf-gbm-crash.md) §4。
核心矛盾：pressure-vessel 容器内的图形栈三根支柱（Vulkan ICD 枚举、GLX vendor 注册、EGL/X11
visual）在厂商驱动下全部失效，而宿主侧图形栈（X11 + 系统 Mesa / gf）完全正常。因此把游戏进程
"委托给宿主" 是唯一干净路径 —— 这也是 README「Steam 适配 → 游戏兼容性」里记录的结论。

---

## 修复安装的内容（均幂等）

| 文件 | 作用 |
| --- | --- |
| `underlords.sh`（游戏目录下，注入补丁） | 容器分支 → exec `ul-host-run.sh`（宿主委托）；智能崩溃重试；退出检测 |
| `ul-host-run.sh` | 容器 → 宿主委托器（经会话 D-Bus / `systemd-run --user`） |
| `play-native.sh` | 宿主启动器（含残留 `LS_SHARE_MEMORY_*` 清理），桌面项也用它 |
| `bin/linuxsteamrt64/libpng12.so.0` | Ubuntu 24.04 已移除该库，Source 2 的 pango 文本模块需要；缺失时自动下载补齐 |
| `dota-underlords-native.desktop` | 桌面项（放 `~/.local/share/applications/`） |

---

## 两个已知坑与对策

### 1) 首次启动缺 `libpng12.so.0`
Ubuntu 24.04 已移除该库，Source 2 的 pango 模块会因找不到它崩溃。脚本会把
`libpng12.so.0` 放进 `bin/linuxsteamrt64/`（可从 18.04 归档取），缺失即自动补齐。

### 2) 游戏中途随机崩溃（`libpanorama_text_pango.so` 固定偏移段错误）
这是 Source 2 panorama 文本模块自身的内存损坏（虚表调用失败），**与显卡无关**。
实验：把游戏自带 scout pango 换成宿主新版反而变成"启动即崩"（确定性），已回滚到
游戏自带 pango（备份在 `pango-scout-bak/`，勿删）。

当前对策（`underlords.sh` 智能重试）：

- 段错误后自动清理残留 `LS_SHARE_MEMORY_*` 并重启（最多 10 次）；
- 180 秒内连崩 3 次判定为崩溃循环，停止重试避免无限循环；
- **退出检测**：退出时 panorama 析构也会段错误。脚本用日志里的
  `CSteam3Client::Shutdown` 标记区分"退出时崩"与"游戏中崩"——前者不重启
  （否则会出现"关一次游戏自动重开"的现象）；
- 启动参数加了 `-nobreakpad`：游戏自带 breakpad 会吞掉 SIGSEGV，关掉后崩溃才会产生
  内核 core（`/tmp/core.underlords.*`，需 core_pattern 指向 /tmp），可供 gdb 定位。

**彻底关闭自动重启**：`sudo ./scripts/fix-underlords-native.sh --no-autorestart`
会把游戏进程结束（正常退出或崩溃）即直接退出，不再拉起。副作用：panorama 随机段错误
会让游戏直接闪退不可玩，按需选择。

---

## 验证

```bash
./scripts/fix-underlords-native.sh --status
# 期望: underlords.sh 已打补丁（容器委托 + 智能重试 + 退出检测）
#       ul-host-run.sh / play-native.sh 就绪；libpng12.so.0 就绪；桌面项已安装
```

然后从 Steam 点"开始游戏"，应正常进入主菜单与对局。被 `kill -9` 强杀后残留的
`LS_SHARE_MEMORY_*` 会在下次启动自动清理；段错误由智能重试兜底（仅默认开启自动重启时生效）。
