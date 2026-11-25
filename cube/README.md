# 方块游戏平台（Cube Game 4.1.2.1）在砺算 7G100 上的运行方案

至砺算：本平台是 Windows 客户端，图形栈问题与 7G100 驱动本身无关。现行方案只依赖
Proton 11 的 wine-11.0、系统 Mesa/llvmpipe 与两个目标进程树内的环境变量，
不改系统驱动、不改 Steam、不改厂商二进制。2026-11-25 实测登录窗口完整渲染，
连续多次启动稳定；不做看门狗、自动重启或守护。

## 现行方案

```bash
cd ~/project/lisuan
bash cube/fix-cube-fonts.sh        # 一次性：五类系统字体 Tahoma -> Arial（幂等、可还原）
bash cube/fix-cube-desktop.sh      # 可选：提取官方图标并生成桌面/应用菜单快捷方式
bash cube/run-cube.sh              # 前台启动，参数透传给 Cube.exe
```

启动后应出现标题为「方块游戏」的 1208×684 窗口（`Map State: IsViewable`），
显示手机号/密码输入框与「登录」按钮。`run-cube.sh --status` 检查 wine 与主程序；
`fix-cube-fonts.sh --status` 检查字体。未接入 `install.py` 菜单，与 `11/`、`war3/` 一样
作为独立主题目录维护。

## 根因与对应措施

1. **CEF 字体初始化重入 → GDI 句柄耗尽**。CEF 43.3 用 Wine 前缀的 Tahoma 调
   `AddFont()` 失败，回退 `GetDefaultSystemFont()` 时 `is_initialized_` 尚未置位，
   无限重入并耗尽 Wine GDI 表；日志为
   `err:gdi:alloc_gdi_handle out of GDI object handles, expect a crash`。
   与 11 对战平台同根因，用 `fix-cube-fonts.sh` 改为 Arial 后归零。
2. **Wine 的 UISettings stub 递归**。内置 `windows.ui.dll` 的
   `IUISettings2::GetTextScaleFactor` 是 stub，CEF 会递归调用上万次，每层约 1.6KB，
   8MB 默认栈直接溢出。启动器默认 `WINEDLLOVERRIDES=windows.ui=n`，
   让 WinRT 激活失败、CEF 走非 WinRT 回退；实测 stub 计数 0，PE 无需改栈。
3. **Depth 32/ARGB 窗口在 Wayland/XWayland 下不可见**：`XLIB_SKIP_ARGB_VISUALS=1`
   让其使用 Depth 24。
4. **32 位 CEF + 只有 64 位 gf ICD**：进程树内
   `VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json`（lavapipe），
   不污染 Steam 客户端。
5. **GPU 子进程崩溃是非阻塞噪音**。应用日志会出现
   `(ElectronEvent) App#child-process-gone: type: 'GPU', reason: 'crashed', exitCode: -2147483391`，
   启动阶段 1–3 次；浏览器/渲染进程仍在，登录界面正常绘制。
   对应 Wine 日志里的 `err:seh:NtRaiseException Exception frame is not in stack limits`
   与 `call_seh_handlers invalid frame`，均来自 GPU 子进程，不是浏览器主线程。
   已证伪的“修复”：`--disable-gpu` 传给 `Cube.exe` 不生效、注入
   `__GLX_VENDOR_LIBRARY_NAME=mesa LIBGL_ALWAYS_SOFTWARE=1` 也不改变次数。
   其它启动噪音（`addrinfo EBUSY`、`com_get_class_object ... not registered`）
   同样不影响界面。不要为此加看门狗或自动拉起。
6. **下载目录必须落在可写路径**（2026-11-25 实测）： Wine 前缀的 `Z:` 指向
   Linux 根目录 `/`。若把下载目录设成 `Z:\CubeLibrary\`（实际是 `/CubeLibrary`），
   平台会在启动时卡在该目录上：进程还活着但主窗口 `IsUnMapped`，屏幕上看不到
   任何方块窗口，表现为“设置完下载目录就卡死、之后进不去”。正确写法是
   `Z:\home\ubuntu\CubeLibrary\`（即 `~/CubeLibrary`）或 `C:\CubeLibrary\`（前缀内）。
   修正方法（先停掉卡死进程，不做自动拉起）：

   ```bash
   CFG="$HOME/.deepinwine/cube-4121/drive_c/users/steamuser/AppData/Local/Cube/userdata/setting.json"
   cp -a "$CFG" "$CFG.bak-$(date +%Y%m%d-%H%M%S)"
   mkdir -p "$HOME/CubeLibrary/download"
   python3 - "$CFG" <<'PY'
   import json, sys
   p = sys.argv[1]
   d = json.load(open(p, encoding='utf-8'))
   d['download']['downloadPath'] = ['Z:\\home\\ubuntu\\CubeLibrary\\']
   d['download']['tempDownloadPath'] = 'Z:\\home\\ubuntu\\CubeLibrary\\download\\'
   json.dump(d, open(p, 'w', encoding='utf-8'), ensure_ascii=False, indent=4)
   PY
   bash cube/run-cube.sh
   ```

   本次备份在 `~/.local/share/cube-4121/download-path-fix/`；卡死现场日志与截图
   保留在 `/tmp/cube-fixed-20261125.log`、`/tmp/cube-stuck-20261125.png`、
   `/tmp/cube-fixed2.png`。不要再在设置里选根目录下的目录。

## 历史路径（不要再执行）

- `fix-cube-stack.sh` 把 `cubelauncher.exe` / `Cube.exe` 的 PE 栈改成 64MB，
  曾缓解第 2 条造成的溢出，但治标不治本。字体修复 + `windows.ui=n` 生效后，
  两个 EXE 已用 `--uninstall` 还原为原厂栈（8192KB / 1024KB），
  栈溢出与 GDI 错误均为 0。脚本仅保留作历史取证与 A/B 对照，默认不要运行。

## 验收与取证

```bash
# 启动时另开终端观察（窗口 ID 每次不同）
DISPLAY=:0 xwininfo -root -tree | grep 方块游戏
DISPLAY=:0 xwininfo -id <窗口ID> | grep 'Map State'   # 应为 IsViewable

# 关键计数应为 0
grep -c uisettings2_get_TextScaleFactor /tmp/<启动日志>
grep -c 'out of GDI object handles' /tmp/<启动日志>
grep -c 'stack overflow' /tmp/<启动日志>
```

合成画面取证必须用 Mutter ScreenCast 通道；**注意先清掉 wine 启动器导出的
`LD_LIBRARY_PATH`**，否则 GStreamer 插件加载失败、脚本会误报“缺少依赖”：

```bash
env -u LD_LIBRARY_PATH -u WINEDLLPATH -u WINEDLLOVERRIDES -u WINEDEBUG \
    python3 .codebuddy/skills/lisuan-7g100-compat/scripts/capture-wayland-screen.py \
    -o /tmp/cube-shot.png
```

2026-11-25 实测截图 `/tmp/cube-font4.png`：登录框、活动海报、按钮均完整绘制。
调试日志放在 `/tmp/cube-*.log`，不入库。

## 不入库的厂商文件

| 文件 | 是什么 | 为何不入库 | 如何获取 |
|---|---|---|---|
| `CubeSetup_v4.1.2.1.exe`（114MB） | 官网 PE 安装器，安装到 Wine 前缀 `~/.deepinwine/cube-4121` | 厂商专有二进制，体积大、随版本换名 | 从方块游戏官网下载；`.gitignore` 已排除 |
| `E92FDE8/` | 安装器解包出的完整客户端（EXE/DLL/PAK 等），仅用于本机运行/排查 | 厂商专有文件，无源码 | 由上述安装器解包得到；`.gitignore` 已排除 |

仓库只保留脚本与本文；安装后运行资源在用户 prefixes 内，不在版本控制中。
