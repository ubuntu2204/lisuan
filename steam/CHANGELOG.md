# steam 变更记录

> 从仓库根 `CHANGELOG.md` 拆分而来（2026-11-19）。按时间倒序；历史条目保留当时的命令与路径。

## 2026.11.13 — Steam 启动弹「GLX context is not using direct rendering」修复

### 根因

- 弹窗来自 Steam 客户端启动自检：`console-linux.txt` 里
  `glXCreateContext failed`、`X Error ... Major opcode 149 (GLX) /
  X_GLXCreateContext`、`CVRStreamThread: Couldn't create context`，随后
  `Using host zenity for message` 弹出 KB 9938-EYZB-7457 文案。宿主
  `glxinfo -B` 同时仍是 `direct rendering: Yes`（LISUAN 7G100、OpenGL 4.6），
  硬件 GLX 正常，只有被 CEF workaround 限制的 Steam 客户端建不出 GLX 上下文。
- 原三项修复（`GBM_BACKEND=dri` + `LIBGL_ALWAYS_SOFTWARE=1` +
  `-cef-disable-gpu`）解决了 CEF 段错误与系统信息闪退，但没有指定 GLX vendor；
  GLVND 在 pressure-vessel 容器内落到失败的间接路径，`glXIsDirect` 判否。

### 消融（单实例、单次启动、无自动恢复，8 组）

| 组 | 变化 | 弹窗 | `glXCreateContext failed` / GLX BadValue | 结果 |
| --- | --- | --- | --- | --- |
| A0r | 基线三项 | 1 | 7 / 5 | 界面正常，弹窗复现 |
| A1r | + `__GLX_VENDOR_LIBRARY_NAME=mesa` | 0 | 0 / 0 | 弹窗消失，无新增失败 |
| A4 | mesa GLX，去掉 `LIBGL_ALWAYS_SOFTWARE` | 0 | 0 / 0 | 同样消弹窗，系统信息安全性未复验 |
| A2 | + `MESA_LOADER_DRIVER_OVERRIDE=llvmpipe` | 1 | 7 / 5 | 无效，与文档结论一致 |
| B1 | 去掉 `LIBGL_ALWAYS_SOFTWARE` | 1 | 2 / 3 | 弹窗仍在，登录窗口波动 |
| B2 | 去掉 `-cef-disable-gpu` | 1 | 7 / 5 | GPU 初始化失败 14→66，界面走加载动画 |
| B3 | 去掉 `GBM_BACKEND=dri` | 0 | 0 / 2 | `alive=0`，客户端直接退出 |
| B4 | 三项全无（裸启动） | 0 | 0 / 0 | `alive=0`，客户端直接退出 |

- 唯一有效且无新增失败的是 `__GLX_VENDOR_LIBRARY_NAME=mesa`：A1r / A4 / A1r2 /
  最终恢复启动共 4 次弹窗 0、GLX 失败 0、`double free` 0、CEF `exit_code=139` 0，
  `Exiting GPU process ...` 计数与基线同为 14。B3/B4 的「弹窗 0」是客户端根本
  没起来，不是修好。
- 明确跳过（仓库已证伪或会伤及游戏客户端）：`__GLX_VENDOR_LIBRARY_NAME=gf`、
  `LD_PRELOAD=libglapi_gf.so.0`、改 `steamwebhelper_sniper_wrap.sh` / `chattr +i`、
  改 `VK_ICD_FILENAMES`。

### 固化与验证

- `packaging/lisuan_fix_core.py` 的 `STEAM_FIX_ENV` 增加
  `__GLX_VENDOR_LIBRARY_NAME=mesa`（现为三个变量 + `-cef-disable-gpu`）；
  `steam_desktop_fixed()` / `_build_fixed_exec()` / `_strip_fixed_exec()` /
  `detect()` / 菜单 12 标题与输出同步，卸载仍逐项摘除、可逆。
- 全部 `steam.desktop` 由菜单 12 统一写入四项，保留用户原有
  `LC_ALL=en_US.UTF-8` 等 env 定制；`python3 install.py --detect` 的 Steam 项为 ok。
- 测试：`tests/test-steam-desktop.sh` 三处入口的修复/幂等/回滚断言加入新变量并禁止
  gf 注入；`tests/test-repo-health.sh` 增加「缺 Mesa GLX 判否」与可逆卸载断言；
  完整套件 `bash tests/run-tests.sh -v` 通过。
- 文档：`steam/steam-on-lisuan.md` 新增 §3.2（消融表、结论、禁忌、限制），
  `README.md` / 本 CHANGELOG / 项目技能排障参考同步四项命令。
- 当前 Steam 进程 `VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/gf_icd.x86_64.json`
  未变，游戏 gf Vulkan 不受影响；`LIBGL_ALWAYS_SOFTWARE=1` 保留，
  「帮助 → 系统信息」的 double free 防护仍在。

### 限制

- 反复重启期间 Steam 登录态出现波动（几次停在「登录 Steam」窗口），
  「帮助 → 系统信息」未能在稳定登录态下复验，需登录后点一次确认。
- 早期 A0/A1 两轮因 `steam://open/systeminfo` 触发窗口/额外进程而作废，
  只采用 A0r 起的干净口径。证据留在 `/tmp/steam-glx-ab/results.tsv` 及同目录
  各组 `*.console.log` / `*.windows.log` / `*.xwd`（临时目录，未入库）。

## 2026.11.09 — Steam「帮助 → 系统信息」闪退修复

### 根因

- `GBM_BACKEND=dri` 只绕开了 `gf_gbm.so` 与系统 libgbm 的 ABI 不兼容（CEF 启动
  `exit_code=139`）。点系统信息弹窗时，Mesa 仍按
  `/usr/share/drirc.d/01-gf_drv.conf` 的 `kernel_driver=gf → dri_driver=gf` 映射
  加载 `gf_dri.so` → `libglapi_gf.so.0`；这套 22.0.4 fork 与系统 Mesa 25.2 在
  pressure-vessel 容器里混用，触发 `double free or corruption (!prev)` 或
  `xcb_io.c:626 _XAllocID` 断言，Steam 主进程 `Aborted (core dumped)`。
- 只加 `LIBGL_ALWAYS_SOFTWARE=1` 虽然不再闪退，但 CEF 的 ANGLE/EGL GPU 进程
  初始化失败（`eglInitialize ... failed` / `Exiting GPU process ...`），界面会
  一直停在加载动画——必须用 Steam 官方命令行参数 `-cef-disable-gpu` 走 CPU 合成。
- 另有独立的入口回归：DING 真正监听的 `~/桌面/steam.desktop` 还带
  `MESA_LOADER_DRIVER_OVERRIDE=gf -no-cef-sandbox -cef-force-64bit`，而
  `~/.local/share/applications/steam.desktop` 只修了 GBM，两处不一致。

### 修复与验证

- 菜单 12 的 Steam 桌面启动器 Exec 最终改为
  `env GBM_BACKEND=dri LIBGL_ALWAYS_SOFTWARE=1 /usr/bin/steam -cef-disable-gpu %U`；
  `-cef-disable-gpu` 是命令行参数，不改 `steamwebhelper_sniper_wrap.sh`，不触发
  Steam 校验回滚。只影响 Steam 客户端 CEF/GL，实测游戏容器内
  `VK_ICD_FILENAMES` 仍是 gf ICD。
- `steam_desktop_files()` 统一修用户级菜单项、XDG 桌面图标与历史 `~/Desktop`
  入口；`steam_desktop_fixed()` 要求两个变量 + `-cef-disable-gpu` 齐全且不含旧
  gf 注入；安装时只改 Exec 行、备份原文件、补 `metadata::trusted`，卸载逐项摘除。
- 受控 A/B（启动 7 秒后 `steam steam://open/systeminfo`）：基线 `GBM_BACKEND=dri`
  5 次里 4 次崩溃；只加软件 GL 0/5 崩溃但界面卡加载动画；单独
  `-cef-disable-gpu` 3 次里 1 次仍崩；`MESA_LOADER_DRIVER_OVERRIDE=llvmpipe`
  0/3 崩溃但重复 `failed to load driver: llvmpipe`，均未采用。
- 最终三项组合 0 崩溃，X11 抓图确认主界面 1280x800（128463 色）与标题为
  「系统信息」的 600x500 窗口（5712 色）均渲染真实内容，不是加载动画。
- 文档 `steam/steam-on-lisuan.md` 新增 §3.1 并细化 §3；测试新增
  `tests/test-steam-desktop.sh`（临时 HOME 验证三处入口的修复/幂等/回滚），
  `tests/test-repo-health.sh` 加入 `-cef-disable-gpu` 必需性与旧 gf 注入清理断言。

## 2026.09.01 — Steam CEF 崩溃定解法 `GBM_BACKEND=dri`；修正 `chattr` 锁定引发的回滚循环

### 症状

1. Steam UI 起不来：`cef_log.txt` 中大量 `GPU process exited unexpectedly: exit_code=139`，
   最终 `GPU process isn't usable. Goodbye.`；`steamwebhelper` 无限重启
   （`startcount` 0→19+）。
2. 每次打开 Steam，《Dota Underlords》（刀塔霸业）都被重新排队下载，进度反复重置。

### 根因

两个独立问题：

**1）CEF 崩溃** —— 厂商 `/usr/lib/x86_64-linux-gnu/gbm/gf_gbm.so`（106KB）与系统
`libgbm 25.2.8` ABI 不兼容，pressure-vessel 容器内 CEF GPU 进程加载它即段错误。

> 纠正 2026-08-24 的结论：仅移除全局 `lisuan_dri.so` 软链与 wrap 内 `LD_PRELOAD`
> **并不足以**让 CEF 稳定——实测"干净"状态下仍崩。此前把 `GBM_BACKEND=dri` 误判为
> 有害，是因为它与 `LD_PRELOAD libglapi_gf.so.0` 同时被注入；有害的是 PRELOAD
> （导致 `double free`），不是 `GBM_BACKEND=dri` 本身。

**2）游戏反复重下** —— 旧版 `fix-steam-cef-lock.sh` 用 `chattr +i` 锁定了
`steamwebhelper_sniper_wrap.sh`。Steam 校验发现大小不符（680 vs 623 字节）→ 下载更新 →
`BCommitUpdatedFiles: failed to rename ... (error 1)` → `Failed to apply update, reverting...`
→ 回滚循环，该循环会重置游戏下载状态。

### 修复

```bash
# 1) 解锁，停止回滚循环（一次性）
sudo chattr -i ~/.local/share/Steam/ubuntu12_64/steamwebhelper_sniper_wrap.sh
# 启动一次 Steam，让它把文件恢复成原厂 623 字节

# 2) 用环境变量绕开 gf_gbm 段错误（日常启动方式）
GBM_BACKEND=dri steam
```

桌面启动器 `~/.local/share/applications/steam.desktop` 已改为
`Exec=env GBM_BACKEND=dri /usr/bin/steam %U`，菜单启动同样生效。

**验证**：`grep -c 'exit_code=139' ~/.local/share/Steam/logs/cef_log.txt` → `0`。

### 教训

- **不要用 `chattr +i` 保护对 Steam 文件的修改**：只会触发回滚循环，并连带破坏
  游戏下载状态。持久化配置一律走「环境变量 + 桌面启动器」，不碰 Steam 自己的文件。
- **全局 `lisuan_dri.so -> gf_dri.so` 软链有害**：让 Steam 客户端加载 gf 驱动而崩溃，
  已移除；`fix-lisuan-mesa-driver-name.sh` 仅在 `--native-gl` 下按需创建。
- **`LD_PRELOAD libglapi_gf.so.0` 绝不能进 Steam 客户端 wrap**：会让 Steam 客户端走
  gf glapi，在无头/XWayland 下 `double free`。只可用于**游戏进程树**。
- 游戏（Proton/wine）渲染走 gf Vulkan，不依赖 gbm 后端，因此 `GBM_BACKEND=dri`
  不影响游戏。

详见 `steam/steam-on-lisuan.md` §3（最终解法）与 §10（回滚循环）。

### 工程改进：Python 工具合并为单文件 `install.py`

- `lisuan_fix.py` + `tui.py` **合并为单文件 `install.py`**（两旧文件已删除），
  单文件分发，无需项目内相对导入。
- 原先两文件通过 importlib 动态互导以断开 `lisuan_fix <-> tui` 导入环；
  合并后 TUI 相关类下沉到 `_load_tui()` 内部**延迟定义**——未安装 textual 时
  `from textual.app import App` 不在模块导入期执行，主程序正常降级为纯文本菜单。
- 菜单编号与旧版完全兼容；`detect()` 检测逻辑同步修正为新方案口径：
  - DRI 软链：检测到全局 `lisuan_dri.so` 由"ok"改为"**warn**（有害残留，建议移除）"；
  - Steam：检查**桌面启动器 Exec 行**是否含 `GBM_BACKEND=dri`（新方案），
    并新增旧方案残留检查（wrap.sh 注入、`chattr +i` 锁定 → 报 bad 并指向清理菜单）。
- 新增菜单 [5]（写入启动器修复）/[6]（还原启动器+解锁）的纯 Python 内部实现，
  `remove` 采用"干净备份才还原、否则重写纯净模板"策略——避免把含修复的备份
  原样复制回来导致"移除无效"的坑（合并测试中发现并修复）。
- `README.md`、`pyrightconfig.json` 中的引用同步更新为 `install.py`。
- 验证：`py_compile` 通过；`--detect`/`--detect --json`/`-h` 正常；TUI headless
  渲染测试通过（27 条目）；`install`/`remove` 全状态转换断言通过；
  `tests/run-tests.sh` 7/7 通过；从 `/tmp` 运行验证单文件自足。

### 工程改进：deb 打包（`packaging/build-deb.sh`）

- 新增 `packaging/build-deb.sh`：一键构建 `dist/lisuan-fix_<版本>_all.deb`（~52KB）。
- 安装布局：
  - `/usr/bin/lisuan-fix` — 可执行入口（install.py 副本，无扩展名，PATH 直达）；
  - `/usr/share/lisuan-fix/{dkms,mesa,steam,underlords,electron}/` — 修复脚本与文档
    （排除 `dkms/vendor-src/` 厂商源码副本与 64MB 的 `minion-masters/` 游戏专用件）；
  - `/usr/share/applications/lisuan-fix.desktop` — 终端菜单项；
  - `DEBIAN/`：`control`（Depends: python3/bash；Recommends: pciutils/kmod/dkms）、
    `postinst`（刷新桌面数据库）、`md5sums`。
- `install.py` 资源根目录自适应（`_find_base()`）：开发仓库 → `/usr/bin`（资源在
  `/usr/share/lisuan-fix`）→ `/usr/share/lisuan-fix/install.py` 直跑，三处均可正确定位。
- 验证：`dpkg-deb --info/-c` 结构正确；md5sums 逐项校验一致；解包树内
  `/usr/bin/lisuan-fix --detect` 可运行；postinst 语法通过；脚本权限 755 保留。

---
