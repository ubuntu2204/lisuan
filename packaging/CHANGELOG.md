# packaging 变更记录

> 从仓库根 `CHANGELOG.md` 拆分而来（2026-11-19）。按时间倒序；历史条目保留当时的命令与路径。

## 2026.10.14 — 修复 install.py 菜单 [12] 误报"需修复"（Steam 桌面启动器检测）

### 现象

Steam 已能正常打开（`GBM_BACKEND=dri` 确实生效，进程环境变量可见），
但 `install.py` 的菜单 [12] 与 `--detect` 报告仍显示 **"✗ 需修复"**。

### 根因

用户的 `~/.local/share/applications/steam.desktop` 在 env 段叠加了自己的
locale 定制：

```ini
Exec=env LC_ALL=en_US.UTF-8 GBM_BACKEND=dri /usr/bin/steam %U
```

而 `lisuan_fix_core.py` 的检测是**整行精确字符串匹配**
（`Exec=env GBM_BACKEND=dri /usr/bin/steam %U`），任何合法定制都会误判。
附带隐患：菜单 [12] 的"修复"动作是**整文件覆写模板**，用户一旦信以为真
去点修复，自己的 `LC_ALL` 定制会被抹掉。

### 修复

`packaging/lisuan_fix_core.py`：

- 新增 `steam_desktop_fixed()` 等 helper：按"**Exec 行 env 段、steam
  可执行文件之前是否存在 `GBM_BACKEND=dri` 赋值**判定（`shlex` 分词），
  容忍 env 段其它变量定制；`detect()` 与 GUI 状态（`collect_statuses`）
  两处判定同源替换。
- `install_steam_fix()` 改为**只改 Exec 行**：注入 `GBM_BACKEND=dri` 到
  env 段最前，保留用户其它变量与注释；文件不存在才写完整模板；重复执行
  幂等（已含则不动作、不重复备份）。
- `remove_steam_fix()` 改为逆操作：只从 Exec 行摘除 `GBM_BACKEND=dri`
  （摘除后 env 段无其它赋值时连 `env` 一起去掉），保留用户定制；
  无 Exec 行的异常文件才回退备份还原/纯净模板。

### 验证

- helper 单元测试：8 组判定用例 + build/strip 往返 + 模板自洽全过；
- 真实环境 `detect()` / `collect_statuses()`：菜单 [12] 由"✗ 需修复"变为
  **"✓ 已修复"**（启动器实际就是 `env LC_ALL=en_US.UTF-8 GBM_BACKEND=dri ...`）；
- 临时文件模拟 6 个场景（缺 GBM 安装 / 重复安装幂等 / 卸载保留 LC_ALL 与
  注释 / 文件缺失写模板 / 无 Exec 行怪文件安装与卸载）全过；
- `py_compile` 三文件通过，无 lint 告警。

---

## 2026.09.05 — 交互界面从 Textual 迁移到 Tkinter；移除第三方 GUI 依赖

### 动机

- Textual 属第三方依赖（pip 安装），0.x 版本 API 变动频繁，升级易碎；
- Tkinter 仅需系统包 `python3-tk`，零第三方依赖，与本项目"仅标准库"策略一致；
- 桌面窗口在鼠标操作 / 多窗口文档查看 / 复制粘贴上体验优于终端 TUI。

### 变更

1. `packaging/install.py`：删除 `_load_tui()` / `text_menu()` 与 Textual 依赖，
   界面改为 Tkinter 桌面窗口（左侧环境检测按级别着色、右侧菜单树按类别
   分组双击执行、底部实时日志与状态栏）；
2. **模块拆分**（均在 `packaging/` 下）：
   - `install.py` — 入口壳：定位核心模块（开发模式同目录 / deb 模式
     `/usr/share/lisuan-fix/`）后转交 `main()`；
   - `lisuan_fix_core.py` — 核心逻辑（无 UI）：检测 / 菜单 / 修复执行 / CLI；
   - `lisuan_fix_gui.py` — Tkinter 界面，提供 `gui_available()` / `run_gui()`；
3. **修复：GUI 修复菜单为空** —— `App.__init__` 此前漏调 `_fill_menu()`，
   右侧菜单树恒为空白；现启动即填充（单测验证 7 组 / 27 项）；
4. **无降级**：无法启动图形界面（无显示环境 / 未装 python3-tk）时直接报错退出，
   非交互场景用 `--detect` / `--detect --json` / 菜单编号；
5. **菜单项显示修复状态**：每个修复/卸载项标题后缀状态（✓ 已修复 / ✗ 需修复 /
   有残留 / 未配置 / 可选 等）并整行染色；状态与检测共用一次 `detect()`
   结果（`lisuan_fix_core.collect_statuses()`，15 项有状态），执行完任务
   自动重测刷新；说明与纯操作项不显示状态；
6. **说明弹窗 Markdown 渲染**：新增 `lisuan_fix_md.py` 轻量渲染器
   （标题 / 代码块 / 表格 / 列表 / 引用 / 粗体 / 行内代码 / 链接 / 水平线），
   此前说明文档按纯文本原样显示；
7. 需要 root 的菜单项从 GUI 打开终端窗口，在真实终端里 `sudo` 重跑本工具
   （密码输入与脚本交互提示均发生在真实终端）；
8. 删除 `packaging/pyrightconfig.json`（其唯一用途是把 pip 安装的 textual
   加进 basedpyright 搜索路径，已无意义）；
9. `README.md` / `packaging/build-deb.sh` 同步更新（deb 打包四个模块、
   Recommends 增加 `python3-tk`）；修 `build-deb.sh` 尾部 `| head` 在
   pipefail 下的 SIGPIPE 假失败（exit 141）；
10. **入口壳上移到仓库根**：`packaging/install.py` → `install.py`
   （与旧版 `lisuan-fix.sh` 并列，`python3 install.py` 直达）；
   核心模块仍在 `packaging/`，入口引导顺序：`<根>/packaging/` →
   `<根>/` → `/usr/share/lisuan-fix/`；deb 打包改从仓库根取入口壳；
11. **删除菜单 26「查看修复状态」**——各菜单项已带修复状态后缀（变更 5），
    单独的状态查看项冗余；原 27「Underlords 说明文档」顺移为 26，
    `detect()` 的 Underlords 提示编号同步改为菜单 [24]/[25]；
12. **删除菜单 7「安装/修复：DRI 驱动名软链」**——实机验证宿主 Mesa 已由
    drirc（`/usr/share/drirc.d/01-gf_drv.conf` 的 `kernel_driver="gf"` →
    `dri_driver="gf"` 映射）天然找到 gf 驱动（glxinfo 显示 LISUAN），
    Steam 走 GBM_BACKEND=dri，全局软链反而会使 Steam 客户端 double free；
    保留残留清理项（新 7）与 `mesa/fix-lisuan-mesa-driver-name.sh`
    （高级手动方案，文档保留）。全部编号重排（7-25 各 -1），共 25 项；
13. **删除软链清理菜单项**（原 7「卸载：DRI 驱动名软链」）——残留仅存在于
    曾运行过旧版工具的机器，且 `mesa/uninstall-lisuan-mesa-driver-name.sh`
    保留在仓库可手动执行（幂等、带安全校验）；`detect()` 对残留的警告
    改为提示手动命令。全部编号重排，共 24 项；
14. **删除 7 个无用脚本**（从磁盘移除，git 历史保留）：
    `lisuan-fix.sh`（旧版 bash 索引，install.py 已替代）、
    `mesa/fix-lisuan-mesa-driver-name.sh`（建链方案本体，有害已废弃）、
    `mesa/uninstall-lisuan-mesa-driver-name.sh`（仅清理上一行的产物）、
    `steam/steam-cef-autofix.sh`（开机自愈历史方案，卸载菜单为纯 Python 实现）、
    `minion-masters/fix-minion-masters.sh`（llvmpipe 早期方案，文档标注"仅存档"）、
    `minion-masters/mm-render-switch.sh`、`minion-masters/run-mm.sh`
    （早期手动渲染切换/启动器，仅历史记录文档提及）。
    `mesa/use-system-mesa.sh`（系统 Mesa 包装器）、`gf-compat/` 构建配套、
    `dkms/` `electron/` `underlords/` 各脚本均仍在用，保留。
    README / steam-on-lisuan.md 的相关引用已清理；
    `detect()` 对软链残留的警告改为手动 `rm` 提示；
15. **删除旧方案清理菜单项 17/18**（新方案下对新用户永远"干净"，与变更 13
    同理，一次性清理需求改由脚本/命令手动完成）：
    - 17「清理 wrap.sh 注入」→ `sudo steam/fix-steam-cef-lock.sh --uninstall`；
    - 18「卸载开机自愈服务」→ `systemctl disable --now` + 删 unit 文件
      （`remove_autofix()` 与 `AUTOFIX_UNIT` 死代码一并移除）；
    保留 16「卸载 Steam 修复」（现行方案配套卸载 + `chattr -i` 解锁入口，
    锁定警告仍指向它）。全部编号重排，共 22 项；
16. **删除菜单「启动游戏」**——补丁自带的桌面项（`dota-underlords-native.desktop`）
    与 Steam 内"开始游戏"（自动委托宿主）已覆盖全部启动场景，GUI 手动启动入口
    冗余；`launch_underlords()` 死代码一并移除。共 21 项。

## 2026.09.06 — deb 补齐随从大师；状态判定改为"实际生效标志"

### 问题

CI 打包的 deb 安装后：菜单 13（随从大师）恒显"需修复"、14 说明文档提示
"文档不存在 /usr/share/lisuan-fix/minion-masters/fix-minion-masters.md"，
且实测游戏玩不了（Proton 11.0 的 DXVK 降级被 Steam 更新覆盖回 v2.7.1）。

### 根因

1. `build-deb.sh` 的目录循环不含 `minion-masters/`（此前设计"体积大不打包"）——
   状态判定与说明文档都指向该目录，deb 版必然缺失；
2. 13 的"✓ 已部署"判定依赖 `S/minion-masters/gf-compat/lib/osrelease_redirect.so`
   ——这是**安装侧**产物（34M 驱动库副本，部署时由脚本从宿主拷贝），deb 模式
   下不存在，判定必然失败。

### 修复

1. `build-deb.sh` 按**运行时最小集**打包 minion-masters（约 300K）：
   主脚本 + `fix-minion-masters.md` + `assets-mm-gf-vulkan/`（部署源：
   MMPatch.dll、hook 源码/产物、gf_icd.json、os-release）；不打打包
   `gf-compat/lib`（34M 驱动库副本——目标机由脚本从宿主重建）、`dxvk23/`、
   `gf-lowmem-shim/`、历史记录 md；
2. 13 的判定改为 **Steam 配置层的"实际生效标志"**（与修复安装位置解耦，
   deb/源码两模式通用）：a) 任一 Steam 用户的 localconfig.vdf 含
   `osrelease_redirect`（LD_PRELOAD 生效）；b) 游戏目录
   `doorstop_config.ini` 的 target_assembly = MMPatch.dll（hook 生效）；
   遍历所有用户目录（含 anonymous）避免误判；
3. 本机随从大师同步修复：Proton 11.0 更新覆盖了 DXVK 降级（v2.3 → v2.7.1），
   `MM_PROTON_DIR` 指向 Proton 11.0 重跑加固，DXVK 回到 v2.3-21、
   CompatToolMapping 保留 proton_11 选择。

### 验证

- deb 内容含 minion-masters 主脚本/说明/assets（dpkg-deb -c 核对）；
- 本机判定 13 = "✓ 已部署"；DXVK 版本 v2.3-21-g1b31aa5d；
- 编译、测试套件 7/7 通过；
- **用户实测能进游戏**（DXVK 重降生效）。

### 附：加固组件消融实测（文件系统级逐个禁用，2026-09-06）

- 必需：LD_PRELOAD hook（禁 → 白名单拒绝 0 设备）、垫片 ICD（禁 → GF
  适配器建不起来）、DXVK v2.3（禁/覆盖 → 玩不了，天然实验）；
- 当前可省：无——**深度实测推翻初判**：libSC 禁用虽能过登录期，但
  `vkCreateDevice` 实际失败（弹 `Failed to initialize graphics`，此前 70s+
  存活是失败弹窗挂住进程的假象）；MMPatch 禁用后登录/对局/胜利全程正常，
  但**点退出后崩溃**（退出流程触发未观察 async → IL2CPP 机器码 GPF，正是
  该补丁修复的场景）；
- **最终结论：5 个组件全部必需，不存在可精简项**，部署保持全量；矩阵已
  固化至 `minion-masters/fix-minion-masters.md`。

---

## 2026.09.06 — CI 产物改为 deb 直出（发布到 Gitee Releases）

### 背景

之前用 `publish@general_artifacts`（上传制品插件），下载时被 Gitee 打包为
tar.gz——用户要的是 deb 直出。

### 变更

1. 流水线改用 **`release@gitee`** 插件（2026-01 上线）：deb 以**原样附件**
   发布到仓库 Releases 页（不再经过制品库 tar.gz）；
2. deb 固定重命名为 `lisuan-fix.deb`（Release 附件路径是静态声明，无法
   引用动态日期版本号）；
3. Tag 固定 `deb-latest` + `allowUpdate: true`——每次 push 自动覆盖更新，
   Release 页始终是最新包；构建号保留在 Release 名称中；
4. 测试与打包合并为一个 stage（`test-build`），消除跨 stage 制品传递；
5. 安装命令统一为 `sudo dpkg -i lisuan-fix.deb`。

---

## 2026.09.06 — 新增 Gitee Go 流水线：push 自动打包 deb

- `.workflow/gitee-go-build-deb.yml`（Gitee Go 原生 schema）：push 到 master/main 时依次
  检出代码 → 跑测试套件（不打包坏代码）→ `packaging/build-deb.sh` 构建
  `lisuan-fix_<当天日期>_all.deb`（dpkg-deb 自检 + sha256 记录到日志）→
  上传为 CI 制品 `lisuan-fix-deb`；
- 构建机为 CentOS 8 系官方镜像（内置阿里源），commands 内临时安装
  dpkg/make/patch/perl/ShellCheck 等打包与测试依赖；
- `.workflow/gitee-go-test.yml`（独立测试流水线）已删除——打包流水线内已含
  同一套测试，独立运行仅多耗构建时长；测试仍可随时本地运行
  `bash tests/run-tests.sh`；
- `packaging/dist/` 本就被 .gitignore 排除，deb 仅作为 CI 制品分发，不入库；
- 本地等价命令不变：`bash packaging/build-deb.sh`；
- README「自动化测试」章节追加「自动打包 deb」说明与文件表条目。

---

## 2026.09.07 — GUI 执行日志显示脚本/文档完整路径

- 执行脚本类菜单项：日志增加一行实际命令（含脚本绝对路径），便于核对与手动复现：
  ```
  $ bash /home/ubuntu/project/lisuan/electron/use-zcode.sh --install-desktop
  ```
- 打开说明文档：日志由"已打开说明：\<标题\>"改为**文档绝对路径** + 标题；
  文档不存在时的报错本来就带路径，现保持一致；
- 内部实现项（11 安装 / 13 还原 Steam）：日志列出实际操作的文件路径
  （`steam.desktop`、`steamwebhelper_sniper_wrap.sh`），不再是"黑盒内部实现"；
- 卸载（操作列按钮）同样输出还原所执行的命令与脚本路径。

---

## 2026.09.06 — 删除独立卸载菜单项（功能由"卸载"按钮承担）

### 变更

1. **删除菜单 5「卸载：DKMS Makefile 补丁」**——还原只是退回厂商的坏
   Makefile（下次内核升级又会编译失败），无人会主动做；且 `dkms/uninstall
   -lisuan-gpu.sh` 幂等，已由菜单 4 的**卸载按钮**（恒显示）承担；
2. **删除菜单 13「卸载：移除 GBM_BACKEND 并解锁」**——功能 = 菜单 11 的
   `remove_steam_fix`，已由 11 的**卸载按钮**承担；原"有残留/干净"状态与
   11 的已修复/需修复判定同源，合并进 11（文件被 `chattr +i` 锁定时显示
   "! 文件被锁定"）；
3. `UNDO` 映射表与 `install.py <编号> --undo` 均保留——**卸载能力未减少**，
   只是入口从独立菜单项变为所属安装项的操作列按钮；
4. 编号重排（5/13 删除后各前移），共 16 项。

### 验证

- 16 项编号连续；UNDO 键 {4,7,8,9,11,15}；实测 7/8/9/11/15 均"✓ 已配置/
  已修复"且卸载按钮在位；
- 编译、GUI 存活、deb、测试套件 7/7 通过。

---

## 2026.09.06 — 安装项增加"卸载"按钮（已配置后显示）

### 变更

1. 菜单树新增**操作列**：安装项在"已配置/已修复/已部署/有残留"状态下，
   标题后显示 **卸载** 按钮，点击即还原该配置；还原后状态变"未配置/干净"、
   按钮自动消失（执行完自动重测刷新）；
2. wrapper 新增 `--uninstall-desktop`（`use-zcode.sh` / `use-electron-gf.sh`）：
   - 把桌面项 `Exec` 还原为应用本体；
   - **优先复用系统原版桌面项的 Exec**，以保留原始占位符（如 CodeBuddy CN
     的 `%F`）；
   - 只还原第一个 `Exec=`，不动 Desktop Action；
3. `lisuan_fix_core` 新增卸载映射表 `UNDO` 与 `undo_item()`：
   4（DKMS，脚本幂等故恒显示）/ 8 / 9 / 10（Electron）/ 12（Steam，内部
   `remove_steam_fix`）/ 17（Underlords `--uninstall`）；
   需要 root 的项在 GUI 内仍走终端窗口 `sudo python3 install.py <id> --undo`；
4. 命令行同步支持 `python3 install.py <编号> --undo`（非交互还原）；
5. **随从大师（13）补齐卸载**：`fix-minion-masters-7g100-hw.sh` 新增
   `--uninstall`——依序还原 Steam CompatToolMapping 489520 条目、
   localconfig LaunchOptions、pfx user.reg 的 winhttp=n,b、DXVK（从
   `.bak-v3` 备份恢复原版 v3）、MMPatch.dll/doorstop_config.ini、libSC
   软链，并清理 `gf-compat` 运行时产物（保留源码与构建脚本）；
   存在性检查仅约束安装模式（卸载允许环境残缺时部分还原）；
6. 每个安装项现均有卸载途径（操作列按钮 / `--undo`）。

### 验证

- 按钮按状态正确显示：8 / 9 / 10 / 12 / 17（+ 4 恒显示）；
- 往返实测：`install.py 8 --undo` → 状态转"未配置"、`Exec` 还原为 AppImage
  真身 → `install.py 8` 重装 → 回到"✓ 已配置"；
- 编译、GUI 存活、deb、测试套件 7/7 通过。

---

## 2026.09.05 — 菜单顺序再调整：Electron 提前；每类严格"安装 → 卸载 → 说明"

### 变更（在 2026-09-03 分组基础上的两处修正）

1. **类别顺序**改为 `DKMS → Mesa → **Electron** → Steam → 随从大师 → 刀塔霸业`
   （此前 Electron 被排在最后，不符合项目习惯的浏览顺序）。
2. **每类严格三项**：`安装/修复 → 卸载 → 说明`。
   原先夹在中间的"体检"项已**移入环境检测一节**——体检本质是检测，
   归入检测节后各主题类目才保持整齐的三段式。
3. 新增 Electron 类的说明项 `electron/README.md`（此前该目录有 md 但未接入菜单）。

### 新编号表

```
 1-3)   环境检测  重新检测 · GPU 体检 · Mesa 体检
 4-5)   DKMS      安装/修复 · 说明（卸载走 4 的"卸载"按钮，见操作列）
 6)     Mesa      说明（软链方案已整体退出菜单，脚本留作手动工具）
 7-10)  Electron  ZCode / CodeBuddy / Qoder（各一项：装桌面项）· 说明
11-12)  Steam     安装/修复 · 说明（卸载走 11 的"卸载"按钮）
13-14)  随从大师   安装/修复 · 说明
15-16)  刀塔霸业   安装/修复 · 说明（启动走 Steam"开始游戏"或补丁自带的桌面项）
```

内部实现项随之改绑：`17 install_steam_fix`、`18 remove_steam_fix`、
`20 remove_autofix`、`25 launch_underlords`；`detect()` 全部 hint 编号同步。
`README.md` 中 5 处"菜单 [N]"引用（120/188-200/424/425/543 行）一并更新。

---

## 2026.09.03 — 打包相关文件归入 `packaging/`；菜单按类别重排（安装 → 体检 → 卸载 → 说明）

### 文件归位

把三个与"分发/构建"相关的条目从仓库根移入 `packaging/`，根目录只留主题目录：

| 原位置 | 新位置 |
|---|---|
| `install.py` | `packaging/install.py` |
| `pyrightconfig.json` | `packaging/pyrightconfig.json` |
| `dist/` | `packaging/dist/` |

连带更新：
- `build-deb.sh`：新增 `HERE`（脚本所在目录）与 `ROOT`（仓库根）两个变量，
  主程序与产物路径改用 `HERE`，不再依赖"脚本在根目录"的假设。
- `install.py`：`_find_base()` 注释与用法示例同步（开发模式下仓库根是
  本文件所在目录的**父目录**，探测逻辑无需改动即可命中）；TUI 内的
  "请在命令行执行"提示文本改为 `python3 packaging/install.py N`。
- `README.md`：脚本表、快速开始、deb 安装三处的路径引用更新。
- `.gitignore`：补 `__pycache__/`、`*.py[cod]`、`packaging/dist/`、`dist/`
  （此前缺 Python 产物规则，编译会生成未跟踪的 `__pycache__`）。

---

## 2026.09.03 — `install.py` 菜单按类别重排（安装 → 体检 → 卸载 → 说明）

### 变更

旧菜单按"检测/修复/卸载/体检/游戏/Electron"六类平铺，同类项目的安装与卸载
被分散在不同分组，找起来要跨区跳转。改为**按类别（目录）分组**，类内统一排列：

| 编号 | 类别 | 类内顺序 |
|---|---|---|
| 1 | 环境检测 | （单独一节） |
| 2–5 | DKMS | 安装/修复 · 体检 · 卸载 · 说明 |
| 6–9 | Mesa | 安装/修复 · 体检 · 卸载 · 说明 |
| 10–14 | Steam | 安装/修复 · 卸载 · 清理残留 · 卸载自愈 · 说明 |
| 15–16 | 随从大师 | 安装/修复 · 说明 |
| 17–20 | 刀塔霸业 | 安装/修复 · 启动 · 查看状态 · 说明 |
| 21–26 | Electron | ZCode / CodeBuddy / Qoder（各：启动 + 安装桌面项） |

### 实现要点

- `Item` 新增 `doc` 字段："说明"项携带该类别目录下的 md 路径，
  由新增的 `show_doc()` 渲染（交互终端走 `less -R`，非交互直接打印）。
- **说明项优先于"内部实现"分支判断**：说明项的 `cmd` 也是 `None`，
  若在 `cmd is None` 之后处理会被误判为"请在命令行执行"（已修正顺序）。
- TUI 的 `_exec()` 对说明项特殊处理：读文件内容写入 RichLog 预览
  （前 200 行，超出提示用命令行看全文），避免在全屏 TUI 内调用 `less`。
- 移除了"安装开机自愈"菜单项（旧方案遗留、新方案下无用），保留卸载入口。
- 新增脚本存在性检查：deb 安装版不含 `minion-masters/`（64MB），
  对应菜单项缺失脚本时返回 127 并给出可读提示，而非 bash 的 `No such file`。
- `detect()` 中所有 `hint` 菜单编号同步到新编号；`README.md` 中
  "菜单 [N]" 引用（120/187/405/406/408/409 行）一并更新。

### 验证

`py_compile` 通过；新分组结构渲染正确；说明项 [5]/[9]/[14]/[16]/[20] 文档路径
均可达且能正确输出；TUI headless 渲染 33 条目（26 项 + 7 分组标题）；
`tests/run-tests.sh` 7/7 通过。

> 注意：编号已**重新编排**，与旧版 `lisuan-fix.sh` 不再一一对应。

---
