# 刀塔霸业（Dota Underlords）修复：Steam 内一键启动

> 适用机型：**砺算 7G100 GPU + Ubuntu 24.04 + Steam（pressure-vessel 容器）**。
> 现象：Steam 里点"开始游戏"能正常进入并游玩（已实测）。
> 修复脚本：`./fix-underlords-native.sh`

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
sudo ./underlords/fix-underlords-native.sh                  # ★ 默认即关闭自动重启
sudo ./underlords/fix-underlords-native.sh --autorestart    # 显式开启（需明确接受）
sudo ./underlords/fix-underlords-native.sh --no-autorestart # 同默认（兼容旧用法）
sudo ./underlords/fix-underlords-native.sh --uninstall
./underlords/fix-underlords-native.sh --status
```

> **自动重启默认已关闭**（2026-08-31 起），依据
> [`.codebuddy/rules/崩溃处理约定.md`](../.codebuddy/rules/崩溃处理约定.md)：
> 自动重启会掩盖崩溃、阻碍根因定位，且可能导致「关不掉」。
>
> ⚠️ **不要**因为「游戏会闪退」就用 `--autorestart` 重新开启 ——
> panorama 随机段错误导致的闪退在此策略下是**预期行为**，应转而定位根因。
>
> 本环境当前即为关闭状态（`--status` 显示「自动重启已关闭」）。

部署后从 Steam 库里点"开始游戏"即可正常玩；也可从桌面项
`dota-underlords-native.desktop` 不依赖 Steam 启动链路直接玩。

---

## 根因与规避原理

完整证据链见 [`vendor-bug-report-gf-gbm-crash.md`](../dkms/vendor-bug-report-gf-gbm-crash.md) §4。
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

`underlords.sh` 内建的处理逻辑：

- 段错误后清理 stale `/dev/shm/LS_SHARE_MEMORY_*`；
- 启动参数加了 `-nobreakpad`：游戏自带 breakpad 会吞掉 SIGSEGV，关掉后崩溃才会产生
  内核 core（`/tmp/core.underlords.*`，需 core_pattern 指向 /tmp），可供 gdb 定位；
- **退出检测**（仅 `--autorestart` 模式下生效）：退出时 panorama 析构也会段错误。
  脚本用日志里的 `CSteam3Client::Shutdown` 标记区分"退出时崩"与"游戏中崩"——
  前者不重启（否则会出现"关一次游戏自动重开"的现象）。

### 自动重启：**默认关闭**（2026-08-31 起）

按 [`.codebuddy/rules/崩溃处理约定.md`](../.codebuddy/rules/崩溃处理约定.md)，
游戏进程结束（正常退出或崩溃）即直接退出，**不再拉起**。
因此 panorama 随机段错误表现为**直接闪退** —— 这在此策略下是**预期行为**，
应转而定位根因，而不是重新开启自动重启兜底。

若明确接受"崩溃自动拉起"，用 `--autorestart` 安装：段错误后自动重启
（最多 10 次；180 秒内连崩 3 次判定为崩溃循环则停止）。

---

## 验证

```bash
./underlords/fix-underlords-native.sh --status
# 期望: underlords.sh 已打补丁（容器委托 + 智能重试 + 退出检测）
#       ul-host-run.sh / play-native.sh 就绪；libpng12.so.0 就绪；桌面项已安装
```

然后从 Steam 点"开始游戏"，应正常进入主菜单与对局。被 `kill -9` 强杀后残留的
`LS_SHARE_MEMORY_*` 会在下次启动自动清理；段错误由智能重试兜底（仅默认开启自动重启时生效）。

## 排障：加载期段错误（GlobPool0 / libvulkan_gf.so）

**症状**：宿主直跑约 70-90 秒（shader 编译高峰）段错误退出，`journalctl -k` 出现：

```
traps: GlobPool0 general protection fault ... in libvulkan_gf.so
```

**根因**：Steam/Fossilize 的 pipeline cache（`steam_pipeline_cache.foz`）里
上次运行写入的管线在驱动加载/replay 时触发 `libvulkan_gf.so` 用户态 bug
（2026-09-06 实测，详见 `dkms/vendor-bug-report-gf-gbm-crash.md` 附录）。

**对策**：清掉该游戏的 shader 缓存，重启游戏即可（Fossilize 自动重建，
首次进游戏加载稍久属正常）：

```bash
rm -rf ~/.local/share/Steam/steamapps/shadercache/1046930/fozpipelinesv6
```

**不要改走 `-gl`**：Source 2 Linux 的 GL 渲染系统虽能加载（需先补
`libudev.so.0`，补丁已自动处理），但实测进游戏黑屏，渲染不可用。
