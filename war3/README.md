# 魔兽争霸 III（1.24 冰封王座）在砺算 7G100 上的运行方案

> 至砺算：**启动链路的唯一硬约束是「32 位游戏不能用本机 64 位 Vulkan 驱动」**。
> 本目录用 Proton 11.0 的 wine-11.0 在独立前缀里跑 `/opt/apps/war3/files` 的
> 32 位 War3.exe，图形栈固定走 32 位 llvmpipe（软件渲染）。只做单次前台启动，
> 不做崩溃后自动重启/守护。

## 1. 环境与文件位置

| 项 | 内容 |
| --- | --- |
| 游戏安装 | `/opt/apps/war3/files`（约 1.1 GB，204 个文件）：`War3.exe`（PE32）、`Game.dll`（1.24a，12.1 MB）、`Storm.dll`、`WAR3.MPQ`(632 MB)、`War3x.mpq`(342 MB)、`war3patch.mpq`(37 MB)、`War3xlocal.mpq`(45 MB)、`Maps/`、`Save/`、`Replay/`、`Font/`、`redist/miles/` |
| 主程序 | `War3.exe`，PE32 GUI，依赖 `Storm.dll`/`mss32.dll`/`MSVCR80.dll` 等，均随游戏自带 |
| Wine 前缀 | `~/.deepinwine/war3-wine11`（与 11 对战平台、豆包等前缀隔离） |
| Proton | `~/.local/share/Steam/steamapps/common/Proton 11.0`（wine-11.0，new WoW64） |
| Vulkan ICD | `/usr/share/vulkan/icd.d/lvp_icd.json` → `libvulkan_lvp.so`（32 位版在 `/usr/lib/i386-linux-gnu/`） |

**游戏本体是第三方专有二进制，不在仓库内**：`/opt/apps/war3/files` 由用户自行放置，
本目录只保留脚本与文档（符合 `.codebuddy/rules/仓库组织约定.md` 一、二）。

## 2. 根因：继承 64 位 gf ICD 导致启动即崩

### 现象

直接用 wine 启动 `War3.exe`，窗口一闪即灭，进程以 `EXCEPTION_ACCESS_VIOLATION` 结束。

### 取证

```bash
cd /opt/apps/war3/files
WINEDEBUG=-all,+seh,+loaddll wine ./War3.exe
```

关键日志：

```
trace:loaddll:build_module Loaded L"C:\\windows\\system32\\d3d8.dll" : builtin
trace:loaddll:build_module Loaded L"C:\\windows\\system32\\wined3d.dll" : builtin
trace:loaddll:build_module Loaded L"C:\\windows\\system32\\libvkd3d-1.dll" : builtin
warn:seh:dispatch_exception backtrace: --- Exception 0xc0000005.
trace:seh:dispatch_exception code=c0000005 addr=35800000
trace:seh:dispatch_exception eip=35800000            ← 跳到野地址执行
warn:seh:dispatch_exception "Storm Error : memory never released -- .?AV?$TSExplicitList@UMapInfoHeaderHashData..."
```

即：War3 走 **D3D8 → vkd3d → Vulkan**，而宿主继承的 `VK_ICD_FILENAMES` 指向
**64 位** 的 `gf_icd.x86_64.json`（砺算 7G100 只有 64 位 Vulkan 驱动），
32 位进程拿到的是不可用的设备指针，最终跳到固定野地址 `0x35800000`。

### 修复

把该进程树的 Vulkan ICD 换成**同架构的 32 位 llvmpipe**：

```bash
VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json
```

`/usr/lib/i386-linux-gnu/libvulkan_lvp.so` 存在，与 32 位进程同架构，无需地址 thunk，
这与本仓库既有结论一致（32 位游戏在 7G100 上走 llvmpipe，见 `minion-masters/`）。

`run-war3.sh` 已把该变量、`PROTON_USE_WOW64=1`、Mesa 软件 GLX 三件套固定注入
**该进程树**，不污染 Steam 客户端与全局 shell。

### 第二个根因：同一 wine 前缀上并发实例 → "Warcraft III was unable to initialize"

现象：窗口出现，但弹出英文模态框 `Warcraft III was unable to initialize.`，
点不掉、看起来像"卡死"（该框是 wine 的 MessageBox，会一直等用户输入）。

取证（受控消融，2026-11-18）：

| 场景 | 结果 |
| --- | --- |
| 干净启动（无残留 wineserver） | 正常，主菜单完整渲染 |
| 紧接上一个实例（或其 wineserver 尚未退出）再启动 | 弹 `was unable to initialize` 并挂住 |
| 注册表有无 `Video` 配置、`lockfb` 取 0/1 | **均不影响**，三种配置都能正常启动 |

即注册表里的视频配置（`HKCU\Software\Blizzard Entertainment\Warcraft III\Video`）
**不是**根因；真正的触发条件是**同一 `WINEPREFIX` 上存在前一个实例**（双击桌面图标
最常见）。`run-war3.sh` 因此在启动前检查该前缀是否被占用：

```bash
for pid in $(pgrep -x wineserver); do
    tr '\0' '\n' < /proc/$pid/environ | grep -qxF "WINEPREFIX=$PREFIX"
done
```

命中即**显式报错退出**（提示先关闭已打开的 War3），不加看门狗、不自动重启。

## 3. 使用

```bash
cd ~/project/lisuan
./war3/run-war3.sh                    # 前台启动 War3（单次，退出码原样透传）
./war3/run-war3.sh --status           # 只读体检
./war3/fix-war3-desktop.sh            # 生成桌面图标与桌面入口（幂等）
./war3/fix-war3-desktop.sh --status    # 只读体检
./war3/fix-war3-desktop.sh --uninstall # 恢复备份、移除图标与入口
./war3/fix-war3-writable.sh           # 授权 Save/Replay 写权限（需 sudo）
./war3/fix-war3-writable.sh --status
./war3/fix-war3-writable.sh --uninstall
```

- 桌面入口：`~/.local/share/applications/war3.desktop`、XDG 桌面目录（本机 `~/桌面`）
  与 `~/Desktop`，均标记 `trusted`；图标取自 `War3.exe` 的 PE 资源，并用安装目录自带的
  128×128 `war3.png` 缩放补齐 16/24/32/48/64/96/128/256 尺寸（**图标产物不入库**）。
- 桌面项 `Exec` 指向 `war3/run-war3.sh`：单次前台启动、参数透传、退出码透传；
  **不做存活监控或崩溃后自动重启**（见 `.codebuddy/rules/崩溃处理约定.md`）。
- `WAR3_DIR` 可覆盖游戏目录，`WAR3_ICD` 可覆盖 ICD，`WAR3_RESET_CONFIG=1` 可在启动前
  删除 Blizzard 写入的视频配置（仅在配置异常时用）。

## 4. 验收状态（2026-11-18 实测）

| 项 | 结果 |
| --- | --- |
| 窗口 | 出现 `Warcraft III` 窗口（1920×1080，`wmctrl -l` 可见、`Map State: IsViewable`） |
| 画面 | 主菜单完整渲染：Frozen Throne logo、插画背景、`单人模式 / Battle.net / 局域网 / 选项 / 谢谢 / 退出` 按钮、版本号 `1.24.6.387`（XWayland 单窗口抓帧：1920×1080、12.6 万色、非黑像素 72.9%） |
| 崩溃 | 修复 ICD 后，连续 9 次受控启动（清/不清注册表、`lockfb` 0/1、带/不带抓图）均 **0 次** `c0000005`、**0 次**错误框 |
| 音频 | PipeWire 出现 `Warcraft III` 音频流节点（游戏已过初始化阶段） |
| 联机 / 单局对战 | **未验收**（本次未登录、未建房） |

### 未解决与注意事项

- **存档/录像不可写**：`/opt/apps/war3/files` 属主 root，`Save/`、`Replay/` 不可写，
  游戏进度与录像无法保存；执行 `./war3/fix-war3-writable.sh`（需 sudo）修复。
- **游戏目录只读**：除上述三个目录外整个安装目录只读，`Maps/` 本身已可写。
- **单机与联机**、**画质与分辨率**（游戏把设置写进
  `HKCU\Software\Blizzard Entertainment\Warcraft III\Video`）尚未逐项验收。
- **Wayland 合成抓图通道失效**：`capture-wayland-screen.py` 建流成功但取不到帧
  （`gst-launch` 25 秒超时），期间 War3 全屏 llvmpipe 窗口持续存在；改用 XWayland
  单窗口抓帧取证。按规则，xwd 结果只作补充证据，不单独用于判定渲染故障。
- 如再遇到崩溃，按崩溃处理约定取证：`WINEDEBUG=-all,+seh,+loaddll ./war3/run-war3.sh`，
  不要用看门狗/自动重启掩盖。

## 5. 排障速查

```bash
# 只看图形栈与异常
WINEDEBUG=-all,+seh,+loaddll ./war3/run-war3.sh

# 只看窗口是否出现（Wayland 合成取证用）
python3 .codebuddy/skills/lisuan-7g100-compat/scripts/capture-wayland-screen.py -o /tmp/war3.png

# 清空厂商写入的视频配置后重启
WAR3_RESET_CONFIG=1 ./war3/run-war3.sh
```

### 常见现象对照

| 现象 | 原因 | 处理 |
| --- | --- | --- |
| 弹 `Warcraft III was unable to initialize.` 并挂住 | 同一 wine 前缀上已有实例 | 关闭已开的 War3 窗口再启动；脚本已前置检查并显式报错 |
| 窗口一闪即灭，日志 `eip=0x35800000` / `c0000005` | 继承了 64 位 gf ICD | 用 `war3/run-war3.sh`（已固定 32 位 llvmpipe ICD） |
| 无窗口、日志停在 Storm 加载 | 前缀未初始化 | 首次启动会自动 `wineboot`；或 `WAR3_PREFIX=<路径> ./war3/run-war3.sh` |
