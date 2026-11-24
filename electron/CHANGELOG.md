# electron 变更记录

> 从仓库根 `CHANGELOG.md` 拆分而来（2026-11-19）。按时间倒序；历史条目保留当时的命令与路径。

## 2026.11.24 — Trae CN（国内版）纳入同一 wrapper 修复

### 根因与证据
- 菜单 [10] 与 `use-trae.sh` 此前只把 `trae.desktop` 切到 wrapper；系统级
  `trae-cn.desktop` 的 `Exec=/usr/share/trae-cn/trae-cn %F` 仍裸启，而菜单状态用
  `any()` 检查两个变体，国际版已修就显示「✓ 已配置」，把 CN 漏掉。
- 受控复现：裸启 `/usr/share/trae-cn/trae-cn` 25 秒无 Trae CN 窗口，子进程持续占 CPU，
  apport 介入；同一环境用 `use-electron-gf.sh --app trae-cn`（X11 + ANGLE/SwiftShader）
  启动则出现「欢迎使用 TRAE」窗口，合成抓图确认正常渲染。

### 变更
1. `electron/use-trae.sh` 支持 `--app trae-cn` 透传；不带 `--app` 的
   `--install-desktop` / `--uninstall-desktop` 同时处理国际版和国内版；
2. `packaging/lisuan_fix_core.py` 菜单 10 标题注明含 Trae CN，状态改为「所有已安装变体
   都切到 wrapper」才算已配置；
3. `use-electron-gf.sh` 未知 `--app` 提示补上 `trae|trae-cn`；
4. 新增 `tests/test-trae-wrapper.sh`，在临时 HOME 里验证双桌面项安装/回滚、显式
   `--app trae-cn` 透传与软件渲染参数；`electron/README.md`、`electron/trae/README.md`、
   `electron/trae/docs/04-display-error-fix.md` 与总 README 同步更新。

---

## 2026.11.22 — Trae 问题归档迁入 `electron/trae/`

### 变更
1. `~/project/trae/`（README + 6 篇分析 + 原始报告/调试记录 + 日志归档）整体迁入
   `electron/trae/`，纳入版本控制；Trae 是 Electron 应用，与 `use-trae.sh` 同主题，
   原有 wrapper 与菜单 [10] 不变；
2. `electron/README.md` 增加 Trae 专题索引与排障入口；仓库根 `README.md` 的
   工具表/文档表挂上 `electron/trae/README.md`；
3. `electron/trae/README.md` 与 `docs/04-display-error-fix.md`（操作指南）改用仓库
   wrapper 作为现行入口，早期 `~/.local/bin/trae-fix` 三件套降级为历史证据；
   原始报告与调试记录只在文首加新旧路径对照，正文保持当时原样。

---

## 2026.09.07 — 新增 Trae（字节 AI IDE）Electron 启动 wrapper

### 背景

本机已装 Trae（`/usr/bin/trae` 国际版 + `/usr/bin/trae-cn` 国内版），且用户此前
自制过 `~/.local/bin/trae-fix`：Trae 在 **gf EGL（Wayland/ANGLE 合成）路径下有
显示错误**——黑屏/花屏/窗口不刷新，需强制 X11 + ANGLE/SwiftShader 软件渲染
（与走 gf 加速的 CodeBuddy/Qoder 相反，更接近 ZCode 的"软件渲染稳定优先"）。

### 变更

1. `electron/use-trae.sh`：Trae 启动 wrapper（转调通用脚本 `--app trae`）；
2. `use-electron-gf.sh` 支持 Trae：
   - `--app trae|trae-cn`：命令解析为 `/usr/share/trae/trae`（**ELF 真身，也是
     桌面项 Exec 指向的路径**；`bin/trae` 是 sh 启动脚本，用它会让桌面项
     改写被过滤跳过），桌面项列表含国际/国内两版；
   - 新增 `SW_TRAE_FLAGS` + `GDK_BACKEND=x11` / `EGL_PLATFORM=surfaceless`：
     合并本机验证过的 `trae-fix` 开关；
   - `--app trae` 默认软件渲染（除非显式 `--hw`）：新增 `ACCEL_EXPLICIT`
     区分"用户显式指定"与"应用默认值"；
   - 桌面项改写在 Trae 上实测通过（主入口切 wrapper，`--new-window` 动作保留，
     `trae-cn.desktop` 按 Exec 过滤正确跳过）。

### 菜单

Electron 组新增 **10 Trae：点图标即用**（装桌面项；默认软件渲染）：
编号 7 ZCode / 8 CodeBuddy / 9 Qoder / **10 Trae** / 11 说明，后续各 +1，共 17 项。

---

## 2026.09.06 — Electron 三项合并为"点图标即用"

### 变更

1. wrapper 新增 `--start`：`--install-desktop --start` = **装完桌面项后继续
   启动应用**（原先 `--install-desktop` 装完即 `exit 0`）。自身标志从下游
   参数中剔除，不污染应用启动参数；
2. 菜单合并（原 8+9 / 10+11 / 12+13 → 新 8/9/10）：每个应用一项，名为
   "XXX：点图标即用（装桌面项）"——执行后桌面图标的 Exec 切到 wrapper，
   **以后像平时一样点图标就自动带修复参数（ZCode）或 gf 硬件加速
   （CodeBuddy/Qoder）**。**只装桌面项、不启动应用**（菜单不传 `--start`）；
3. 菜单状态（✓ 已配置 / 未配置）改绑到 8/9/10，并修正两处检测错误：
   - CodeBuddy/Qoder 的桌面项 Exec 实际指向通用 wrapper
     （`use-electron-gf.sh --app <name>`，因脚本内 SELF 解析为自身），
     检测标记改为 `use-electron-gf.sh`，此前恒误报"未配置"；
   - **CodeBuddy 国内版识别**：本机以 "CodeBuddy CN" 安装，命令名 `buddycn`、
     桌面项 `buddycn.desktop`，而 wrapper 只认 `codebuddy` → 报"未解析到启动
     命令"。现按 codebuddy → buddycn → 常见路径顺序探测，桌面项列表加入
     buddycn.desktop；
4. 桌面项改写只替换**第一个 `Exec=`**（主入口）：CodeBuddy CN 等桌面项含
   Desktop Action 的第二个 Exec（`--new-window`），全文替换会丢动作参数；
5. 编号重排：Electron 7 项 → 4 项，后段依次前移，共 18 项；
   `README.md` 编号表与快速开始示例同步。

### 验证

- 三项实测：`[8] ✓ 已配置`、`[9] ✓ 已配置`、`[10] ✓ 已配置`；
- `~/.local/share/applications/buddycn.desktop`：主入口走 wrapper，
  `New Empty Window` 动作保留 `--new-window`，功能不丢；
- 编译、GUI 存活、deb、测试套件 7/7 均通过。

### 验证

- `python3 -m py_compile` 三文件通过；`--detect` / `-h` / GUI 报错路径
  （exit 1）实测正常；
- Tk 8.6 下菜单树单测（7 组 / 27 项）、窗口 3 秒存活测试正常；
- deb 构建 exit 0，产物含 `/usr/bin/lisuan-fix` + 三个模块 + `python3-tk`
  依赖声明。

---

## 2026.08.28 — Qoder（国际版/国内版）显示修复落地与 Electron 43 新坑

### 修复

- **`--ozone-platform-hint=x11` 在 Electron 43（Qoder）上被忽略**：应用仍走 Wayland
  原生（子进程命令行仍为 `--ozone-platform=wayland`），拖拽/缩放窗口残缺、花屏依旧。
  `use-electron-gf.sh` 的 BASE_FLAGS 追加显式 `--ozone-platform=x11` 强制 XWayland
  （与 hint 并存，兼容新旧 Chromium）。实测：Qoder CN 子进程变为 `ozone-platform=x11`，
  窗口经 XWayland 正常渲染（`wmctrl -l` 可见）。
- **`use-electron-gf.sh` 新增 `--app qoder-cn`**（解析国内版二进制与桌面项）；
  `--app qoder` 未安装国际版时自动回退 `qoder-cn` 二进制；`--install-desktop` 按
  桌面项 Exec 实际指向（`readlink -f` 精确比较）过滤，国际版/国内版不再互相误改。
  已实测把 `qoder.desktop` / `qoder-cn.desktop` 用户级条目与桌面图标均切到 wrapper。
- **`--install-desktop` 生成的桌面项被 GNOME 应用菜单隐藏**：从 `/usr/share` 复制的
  条目无执行位、`sed -i` 也会丢执行位，脚本型桌面项无执行位时 GNOME 直接隐藏。
  两处改写路径均补 `chmod +x`（幂等跳过分支也补，可自愈）。
- **跨变体启动崩溃**：从另一个 Qoder 变体的内置终端启动时，环境带宿主注入的
  `QODER_*`/`QODERCN_*`/`ELECTRON_PA_APP_NAME`（身份 `QODER_PRODUCT_ID`、鉴权
  `QODER_SDK_AUTH_PAYLOAD_FILE` 等），新实例报"运行身份与产物身份不一致"崩溃或
  `job_token_invalid`。wrapper 对 `qoder|qoder-cn` 启动前清理全部此类变量
  （桌面图标启动走干净会话，本不受影响；此修覆盖跨终端场景）。

### 新坑记录（防再踩）

- 判定 Electron 应用实际走 X11 还是 Wayland：看 `--type=gpu-process` 子进程命令行的
  `--ozone-platform=` 取值（hint 可能被忽略）；X11 窗口应出现在 `wmctrl -l`。
- GNOME 对无执行位的脚本型 `.desktop` 直接从菜单隐藏；验证用
  `desktop-file-validate` + Gio `should_show()`。
- Qoder 内置终端环境含身份变量，启动另一变体前必须清理。
