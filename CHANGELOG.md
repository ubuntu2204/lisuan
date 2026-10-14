# 更新日志（CHANGELOG）

本项目采用日期版本号。所有变更均在 Ubuntu 24.04 LTS + 砺算 7G100（PCI `4c54:5006`）
实机验证。

## 2026.10.14 — Navicat 18 Premium Lite（AppImage）启动闪退修复：内置 fcitx 插件与自带 Qt ABI 冲突

**与 GPU 无关**的应用层修复（AppImage 打包缺陷），新增 `navicat/` 目录
（`fix-navicat-appimage.sh` + `README.md`）。

### 现象

双击/终端运行 `~/development/navicat18-premium-lite-cs-x86_64.AppImage`，
「注册」对话框闪现不到一秒即闪退（核心转储）。

### 根因

AppImage 自带 **Qt 6.8.3**，但内置输入法插件
`libfcitxplatforminputcontextplugin-qt6.so` 是按 **Qt 6.5** 编译的
（动态段要求 `Qt_6.5` 符号版本）。本机会话跑 fcitx5
（`QT_IM_MODULE=fcitx` + `XMODIFIERS=@im=fcitx`）：Qt 6.8.3 仍提供 `Qt_6.5`
版本节点，链接器不报错、插件"加载成功"，但插件内部按 6.5 的 `QObjectPrivate`
布局构造对象 → d 指针错位 → SIGSEGV，崩溃点正是对话框获焦的一瞬。
gdb 栈：`QObject::thread ← QObject 构造 ← fcitx 插件 ← QGuiApplication 焦点事件`。

### 修复

`navicat/fix-navicat-appimage.sh`（幂等、无需 root）**重打包** AppImage：
runtime ELF 原样保留，仅向 squashfs 内的 `AppRun` 注入
`export QT_IM_MODULE=compose`，按原压缩器/块大小 mksquashfs 重打包。
双击/菜单/终端三种启动方式全部生效；原包备份 `.bak`（仅首次），
`--status` / `--test` / `--restore` / `--keep-work` 齐备。

### 验证

模拟双击环境（`QT_IM_MODULE=fcitx XMODIFIERS=@im=fcitx`）下修复版正常弹出
「注册」窗口并稳定运行；原包同环境必现崩溃（`--test` 报 SIGABRT/SIGSEGV）；
脚本幂等、`--restore` 还原后 md5 与原包一致；`tests/` 套件通过。

---

## 2026.10.06 — 全部游戏/应用文档新增「至砺算：」章节

本项目主要写给砺算官方开发人员。6 份游戏/应用文档（steam / underlords /
minion-masters / electron / kingdom-rush / left4dead2）开头统一新增
**「至砺算：官方实现什么可以让本游戏（应用）原生运行」**：

- 按投入产出比排序，每项 = 官方要实现什么 + 已实测的根因证据 + 落地后
  退役的 workaround；
- 共性诉求高频出现：ICD 规范化（协商接口 / 实例扩展）、基于渲染节点的
  容器内设备枚举（不依赖 /sys/bus/pci）、libSC 自解析、去除 os-release
  白名单与 MAP_32BIT 约束、补齐 DXVK v3.x 所需扩展、GLVND 标准集成、
  gf_gbm 与 libgbm 25.x ABI 兼容；
- 32 位用户态驱动（i386）作为 left4dead2 的最彻底解法单独列出。

同批：`left4dead2/README.md` 从已废弃的软渲染方案重写为 Proton WoW64
GPU 方案（2026.10.06 条目），并同步至砺算诉求。

---

## 2026.10.06 — 求生之路 2（L4D2）改走 7G100 硬件渲染：废弃原生 32 位软渲染方案

### 决策

用户定下硬性原则：**任何游戏禁用软件渲染，必须 GPU 硬件加速**（已沉淀为长期记忆）。
原生 Linux 版 L4D2 是 32 位 ToGL/OpenGL，厂商无 32 位 gf_dri.so，原生路线永远只能
软件渲染（llvmpipe，帧率不可用）——整体废弃。

### 新方案

游戏目录自带 Windows 版（left4dead2.exe），走与随从大师同源的 Proton new WoW64
路线（`PROTON_USE_WOW64=1`，注意不是 ENABLE）：32 位游戏进程经 wine wow64 thunk
调用 64 位 gf Vulkan ICD，DXVK 把 DX9 翻译成 Vulkan -> 7G100 硬件渲染。

`left4dead2/fix-l4d2.sh` 全部重写：

- 部署自持运行时 `left4dead2/gf-compat/`（osrelease_redirect hook + 协商垫片 ICD +
  驱动库副本；源码复用 `minion-masters/assets-mm-gf-vulkan/`，gcc 现场编译）；
- libSC.so 软链进目标 Proton 库目录（vkCreateDevice 依赖）；
- 目标 Proton 的 DXVK 为 v3.x 时自动降级为 Proton 8.0 的 v2.3（gf 22.0.4 缺
  VK_KHR_load_store_op_none / VK_KHR_maintenance5；实测 Experimental
  v3.1.1 -> v2.3，Proton 11.0 当前已是 v2.3）；
- CompatToolMapping 550 -> proton_experimental + LaunchOptions 写入（保留用户
  参数如 `-language schinese`）；`--status` / `--uninstall` 齐备；
- 卸载只回收指向本 DEPLOY 的 libSC 链接，不误伤随从大师的加固。

### 状态

- 实测：安装流程 exit 0、二次运行全走幂等分支、vdf 写入确认；
- 待重启 Steam 实测游戏内 `mat_info` 显示 LISUAN（而非 llvmpipe）。

---

## 2026.10.03 — 王国保卫战（Kingdom Rush）启动崩溃修复：容器内 GLXBadFBConfig

### 现象

Steam 点"开始游戏"后弹窗、无法进入：

```
Unable to create OpenGL window
OpenGL context creation error: Could not create GL context: GLXBadFBConfig
```

### 根因

Kingdom Rush（AppID 246420）是原生 64 位 LÖVE（SDL2 + OpenGL）游戏。Steam 默认
把它放进 pressure-vessel 容器运行，容器隐藏 `/sys/bus/pci`，砺算厂商 GLX/EGL 在无
设备发现路径下无法认领屏幕，回退的 mesa 又给不出游戏请求的 GLXFBConfig →
`GLXBadFBConfig`。宿主的厂商栈完全正常（glxinfo：LISUAN 7G100、OpenGL 4.6）。
另：宿主缺 64 位 `libopenal.so.1`（Ubuntu 24.04 默认只装 i386），LÖVE 需要它。

### 修复

新增 `kingdom-rush/fix-kingdom-rush.sh`（幂等，与 Dota Underlords 同一套容器→宿主
委托方案）：

1. `apt install libopenal1` 补齐 64 位 OpenAL；
2. 给游戏自带 `run.sh` 打补丁：检测到 `/usr/lib/pressure-vessel` 时经宿主
   `systemd-run --user` 委托到宿主直跑，阻塞至游戏退出以保持 Steam 跟踪；
3. 生成 `kr-host-run.sh` 委托包装与桌面项；
4. 登记进交互式修复索引：新增分组"王国保卫战"（菜单 [18] 安装/修复、[19] 文档），
   含环境检测、状态展示与卸载（`--uninstall`）；`build-deb.sh` 打包目录加入 `kingdom-rush`。

### 验证

宿主直跑 `run.sh`（干净环境）：窗口正常创建、无 GL/音频报错（timeout 杀，rc=124）。
`python3 install.py --detect` 游戏节显示 `[OK] Kingdom Rush 修复已安装`；
`tests/run-tests.sh` 7/7 全过（含 shellcheck / repo-health）。

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

## 2026.09.06 — 删除 mesa/use-system-mesa.sh，配方内联进文档

### 理由

- 无菜单入口、无脚本依赖、无实际案例——纯边缘实验/应急工具；
- 实测能力上限为 **GL 3.0**（zink 受厂商 Vulkan ICD 限制），低于厂商栈的
  GL 4.6，重 GL 应用不适用；
- zink 建立在 `libvulkan_gf.so` 上，而该驱动刚发现 GlobPool0 GPF 崩溃
  bug（见同日刀塔霸业条目），地基本身不稳。

### 变更

1. 删除 `mesa/use-system-mesa.sh`（从磁盘移除，git 历史保留可找回）；
2. **环境变量配方内联**进 `mesa/system-mesa-analysis.md` §7 与 `README.md`
   （三个变量：`__GLX_VENDOR_LIBRARY_NAME=mesa`、
   `__EGL_VENDOR_LIBRARY_FILENAMES=/usr/share/glvnd/egl_vendor.d/50_mesa.json`、
   `GBM_BACKEND=dri`）——文档不再依赖脚本即可复现实验；
3. README 4 处引用清理；`mesa/verify-system-mesa.sh`（体检）与
   `system-mesa-analysis.md`（菜单 8 说明）保留。

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

## 2026.09.06 — 随从大师三份文档合并为「一篇通」

原 `MinionMasters-攻坚全记录.md`（研发日志，741 行）与 `MinionMasters-成功运行
报告.md`（llvmpipe 时代实测快照，516 行）内容去重后并入 `fix-minion-masters.md`
（保留文件名，`install.py` 菜单 14 引用、deb 打包、README 链接均无需改动）：

- 新增章：环境与架构矛盾（含 Wayland 必须经 Steam 启动的约束）、启动流程与健康
  判读（阶段时间线 + **5 类可容忍错误** + 健康/异常速查三命令）、排障速查表
  （症状 → 根因 → 处置）、历史方案与已废弃路线（9 条，含已排除嫌疑清单）、
  产出物清单、文档沿革；
- 头部标注合并说明与阅读顺序建议；`gf-compat/README.md`、README 两处旧链接同步
  指向合并后的文档；
- 两份旧文档从仓库删除（git 历史保留可查）。

---

## 2026.09.06 — 随从大师加固目标自动跟随 Steam 中的实际选择

### 背景

本次"玩不了"复发的间接诱因：裸跑修复脚本默认加固 Proton - Experimental，
而游戏实际用 Steam 里选择的 Proton 11.0——加固与运行对象脱节，且
CompatToolMapping 被打回 experimental（覆盖用户选择）。

### 变更

`fix-minion-masters-7g100-hw.sh` 的加固目标解析改为三级优先：

1. `MM_PROTON_DIR` 显式指定（沿用，且不改写 CompatToolMapping）；
2. **自动跟随**：读 `config.vdf` 的 CompatToolMapping 489520，把 Steam 里
   游戏当前选择的 Proton（如 `proton_11` → `Proton 11.0`）解析为目录并
   加固它——**裸跑 `bash fix-...sh` 即可加固正确对象**，不再依赖记得带
   环境变量；
3. 兜底默认 Proton - Experimental（无记录/目录不存在时）。

`--status` 新增"加固目标"行，可直接核对加固对象与 Steam 选择是否一致；
`--uninstall` 亦作用于解析后的目标（还原正确的 Proton）。

### 验证

- 裸跑实测：自动检测 `proton_11 → Proton 11.0` 并输出加固目标；
- `MM_PROTON_DIR=...Proton - Experimental` 显式覆盖仍优先；
- 语法、`--status`、测试套件 7/7 通过。

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

## 2026.08.31 — Proton 更新致加固失效；改用 Proton 11.0 稳定版

### 症状

Steam 自动更新 Proton Experimental 至 `experimental-11.0-20260826` 后，
《Minion Masters》无法启动：

```
Failed to initialize player
Details: Failed to initialize graphics ... InitializeEngineGraphics failed
```

### 根因

Proton **Experimental**（版本 `experimental-11.0-20260826`。注意它虽含 "11.0"
字样，但与独立安装的稳定版《Proton 11.0》**不是同一个东西**）将库目录由
`files/lib64/` 改为 `files/lib/`，三处加固**静默失效**：

| 加固项 | 失效表现 |
|---|---|
| `libSC.so` 软链 | 悬空（仍指向已删除的旧路径）→ gf `vkCreateDevice` 失败 |
| DXVK 降级 | 还原为新版（要求 `VK_KHR_load_store_op_none`，gf 22.0.4 不支持）→ DXVK 跳过 7G100 |
| hook 部署路径 | 指向失效 |

`libSC.so` 是 gf 驱动的着色器编译器伴侣库，悬空是本例的直接致命原因。

### 修复

重跑 `minion-masters/fix-minion-masters-7g100-hw.sh`（幂等，已适配 `files/lib/` 新路径）。
Steam 运行时改 vdf 会被覆盖，需**退出 Steam 后执行，跑完重启**。

验证结果：`Renderer: LISUAN 7G100`，D3D11 设备创建失败 **0 次**（修复前每次运行多次失败），
低 4GB 地址峰值 2.92 GiB / 4 GiB（72.9%）。

### 工程改进

- 新增 `minion-masters/gf-compat/variants/` 与 `build.sh`：hook 实验变体
  （`baseline-64k` / `norecycle` / `main`）纳入版本管理并提供可复现构建，
  不再在 `/tmp` 下临时编译——`/tmp` 会被清理、不进 git、构建不可复现。
- `MinionMasters-成功运行报告.md` 第 5 条原为**软件渲染时期**的描述
  （称 libSC「不需要」、DXVK 降级「无影响」），已按硬件渲染现状重写，
  并补入 `experimental-11.0` 的目录变化点与三步诊断命令；同时标注该条
  **仅适用于硬件渲染**（llvmpipe 软件渲染不涉及这两项）。
- **删除 `minion-masters/mm-patch/`**（`git rm`，历史保留）。它是补丁开发的历史
  工作目录，随 BepInEx 方案退役而整体失效：

  | 子目录 | 失效原因 |
  |---|---|
  | `plugin/`（`MMSkipVivox`） | 从未运行过——游戏日志中 `BepInEx` **0 命中** |
  | `doorstop/` | MMPatch 的**旧版副本**（08-26），Vivox patch 仍启用，误部署会卡登录 |
  | `apicheck/` `metalib/` `typelist/` | 一次性诊断工具，目标 DLL 已不在位 |

  连带清理 231MB 第三方依赖（`tools/` 97M + `backup/` 134M，原被 `.gitignore`
  排除、**不在 git 内**）。`minion-masters/` 体积由约 295MB 降至 **64MB**。

  删前已把关键知识迁至 `assets-mm-gf-vulkan/README.md`（新建）与 `MMPatch.cs`
  头部：① **`MMPatch.cs` 以 `assets-mm-gf-vulkan/` 那份为唯一权威**；
  ② **Vivox patch 必须停用** —— 日志出现 `PATCHED 2/2 Vivox async` 即中招，
  症状是卡在登录，与显卡故障相似，极易误判。

### 改用 Proton 11.0 稳定版（已验证，推荐）

Experimental 更新频繁，每次更新都会打断加固。已验证稳定版《Proton 11.0》
（`proton-11.0-2`）同样可跑通 7G100 硬件渲染，且更新频率低得多，更省心。

用法：在 Steam 里为游戏手动选择《Proton 11.0》，再对该目录加固：

```bash
cd ~/project/lisuan/minion-masters
MM_PROTON_DIR="$HOME/.local/share/Steam/steamapps/common/Proton 11.0" \
  bash ./fix-minion-masters-7g100-hw.sh
```

脚本相应改动：

- 新增 `MM_PROTON_DIR`：指定要加固的 Proton 目录，默认仍是 Experimental
- 指定该变量时**跳过 `config_steam`** —— 该函数会硬写 `proton_experimental`，
  不跳过就会把用户在 Steam 界面里的手动选择打回去
- 不指定时行为与原先完全一致（兼容既有用法）

另注：仓库内的 `Proton - MM GF`（自定义副本）**已失效** —— 其 `libSC.so` 指向
容器内不可见的 `/run/host/usr/lib`，建议删除，或用 `MM_PROTON_DIR` 指向它重跑修复。

### 里程碑：完整对战验证通过（可玩性闭环）

2026-08-31 在 Proton 11.0 + 7G100 硬件渲染下**完整打完一局对战并获胜**，
赛后奖励正常结算。`Player.log` 证据：

```
[BattleNetworkStateActive] Game ended with Completed
[GameUnitService] Not removing winner master 'WitchQueen' to let win animation play
[PostMatchViewModel] Displaying Battlepass reward card
```

匹配 → 对战 → 判定胜负 → 结算奖励，全链路通过。两项随之重新定性：

- **游戏坑 3（后端 API 404）降级为已知噪音** —— 已非「全 404」：实测单次会话
  仅 `LoginQueueService.Post` 一处 404，而 Steam 认证、WebSocket、商店数据、
  档案数据、语音聊天（`ChatService` → `LoggedIn`）**全部正常**，且不影响对战。
- **游戏坑 7（进程 40–90 秒后消失）已解决** —— 随 Proton 11.0 + 完整加固消失；
  原「疑似登录队列 404 导致游戏退出」的猜测**排除**（404 依旧存在，但游戏不再退出）。

---

## 2026.08.30 — 目录重构与海光音频迁出

### 目录重构
原 `lisuan-7g100-ubuntu-fix/{scripts,docs,patches,assets}` 扁平结构，改为**按主题分目录**：

`steam/` `minion-masters/` `underlords/` `electron/` `electron/` `electron/` `display/`。

每个主题目录自带脚本与文档，不再区分 `scripts/` 与 `docs/`。
README 的全部交叉链接已同步修正（断链校验 0 处）。

### 海光音频 → 独立项目
`hygon/` 下的海光板载音频修复**与砺算 7G100 GPU 无关**，已迁出为独立仓库：

```
/home/ubuntu/project/hygon/
```

lisuan 仓库中相关脚本、菜单项、文档章节与测试引用已同步移除。
下方 2026.08.09 等历史条目中提到的 `hygon/…` 路径为**迁出前的记录**，请以上述新仓库为准。

---

## 2026.08.29 — 随从大师硬件渲染链路打通（容器运行时更新适配）

### 背景

08-26 的 WoW64 + gf 硬件渲染方案在 Steam 容器运行时（steamrt4）与 Vulkan loader
更新后全部回归（游戏 9 秒内必崩）。本轮逐层定位并修复，硬件渲染链路首次端到端打通。

### 修复

- **容器内 `/run/host/usr/lib` 不再可见**：gf ICD 库与 `libSC.so` 从该路径消失，
  loader 静默丢弃驱动、`vkCreateDevice` 段错误。改为把两库复制到 `~/gf-compat/lib/`
  （home 在容器内一定可见），自定义 `gf_icd.json` 经 `VK_ICD_FILENAMES` 注入，
  libSC 软链改指 home 副本。
- **新 Vulkan loader（1.4.309）丢弃老式 ICD + 严格校验实例扩展**：gf 协商版本 0、
  只导出 3 个 `vk_icd*` 符号，被走"直接导出符号表"路径静默丢弃；厂商又裁掉了
  `VK_KHR_external_semaphore_capabilities`，新 loader 严格校验直接拒绝
  `vkCreateInstance`。新增**协商垫片 `libvulkan_gf_shim.so`**：对外报协商版本 5、
  全程 `vk_icdGetInstanceProcAddr` 转发；枚举时补报被裁扩展，转发
  `vkCreateInstance`/`vkCreateDevice` 前剔除真 ICD 不支持的实例/设备扩展。
- **Steam overlay Vulkan 层绕过垫片直连真 ICD**（崩在 libc）→
  `VK_LOADER_LAYERS_DISABLE` 禁用（容器内 overlay 本就不可用）。
- **32 位低地址映射（显卡坑 12 终结）**：实测游戏进程低 2GB 被 wine 占用 ~1.7GB
  （`MAP_32BIT` 只剩 ~321MB），而 2~4GB 完全空闲。`osrelease_redirect.so` 内置
  **低 4GB 重映射池**：`MAP_32BIT` 失败或映射落 >4GB 时用 `MAP_FIXED_NOREPLACE`
  重映射到 2.25~4GB 空闲区。覆盖**所有** mmap 路径（含虚函数分发的未补丁路径），
  实测 1300 次重定向全部成功；驱动单字节 `MAP_32BIT` 补丁由此变为可选优化。
- 固化脚本 `fix-minion-masters-7g100-hw.sh` 升级：部署垫片/库副本、新启动选项、
  状态项扩充（幂等）；资产 `assets/mm-gf-vulkan/` 新增 `libvulkan_gf_shim.c/.so`、
  `gf_icd.json`，`osrelease_redirect.c/.so` 更新。

### 实测

- `Renderer: LISUAN 7G100 Series Graphics (ID=0x5006)`（D3D11 11.1 硬件渲染）+
  场景加载 + 登录流程（8.2s）+ 进入登录队列；游戏存活 8 分钟以上（此前各轮 9 秒内崩溃）。
- **剩余阻塞仅服务端**：`api.prod.mm21.../v1/*` 404（BetaDwarf 问题，本地无解，游戏坑 3）。

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

## 2026.08.25 — 随从大师（Minion Masters）进入主菜单

### 修复（文档固化，插件源码在 `/home/ubuntu/mm_patch/plugin/Patcher.cs`）

- **随从大师黑屏 + BepInEx 控制台无限刷 `NotSupportedException`**：定位为证书 patch
  （`CertificateHandler.ValidateCertificate` @0x75D98C0）破坏 IL2CPP runtime 状态——引发
  Zenject `ProjectContext` 重复创建 assert + `BootstrapInstaller.Start()` NullReference +
  异常提取栈死循环（IL2CPP `GetGenericArguments()` 未实现，每个异常的栈提取又抛新异常）。
  **修复 = 禁用证书 patch**。
- **致命 Texture 异常**（`Texture has not yet finished downloading`）：图片下载
  `Curl error 60`（证书校验失败）→ `DownloadHandlerTexture.InternalGetTexture()` 抛异常 →
  沿 UniTask `SimpleCoroutineAwaiter.Complete` / `TrySetException` 链二次传播（只 patch
  `MoveNext` 拦不住）。**修复 = patch `InternalGetTexture` @0x97CE7B0 → `xor eax,eax; ret`
  返回 null**。
- **插件最终形态**（`MMSkipVivox.dll` 15360 B，三件套）：Vivox `LoginAsync`/`LogoutAsync`
  （0x7983BD0/0x7983C90 → xor trampoline）+ 新闻下载状态机 `MoveNext`（0x6630500 → ret）+
  `InternalGetTexture`（0x97CE7B0 → null）。游戏实测到达 **`BattleModeViewModel`（主菜单）**，
  Steam ID 缓存（76561198811581867）、Ticket、WebSocket 连接、SteamShop 商品列表均成功。
- **Steam 客户端登录态自愈**：登录过一次后（`AutoLoginUser` + `RememberPassword` 已写入
  config.vdf），`steamid=0` 陈旧态只需**重启 Steam 客户端**即自动重登（state 5），无需再
  进图形界面；loopback `127.0.0.1:57343` 自动起。

### 新坑记录（防再踩）

- `il2cpp_class_get_methods` 在 BepInEx/Il2CppInterop binding 下签名错（iterator 协议被当
  数组返回），count 读出垃圾值（实测 621846768）→ 0xC0000005 闪退；枚举方法只能用
  `il2cpp_class_get_method_from_name`。
- Zenject "Tried to create multiple instances of ProjectContext" 是良性 assert（成功运行里
  也有），不要误判为崩溃根因。
- BepInEx 日志显示的插件版本是 `AssemblyVersion`（非 `BepInPlugin` 属性），源码需显式
  `[assembly: AssemblyVersion("1.1.0.0")]`。
- Unity 启动会把 `Player.log` 轮转为 `Player-prev.log`，排障时看 mtime 区分本次/上次运行。
- patch Unity 内部 native 方法前先反汇编原字节——已是 `32 C0 C3 CC`（优化过的共享
  return-false stub）说明有多个调用方，patch 它会波及无辜路径。

### 剩余阻塞（外部环境，本地无解）

- 游戏后端 `api.prod.mm21.minionmastersthegame.com` 所有 `/v1/*` API endpoint 返回 404
  （`/health` 却 200 "Healthy"）→ `LoginQueueService.Post` 抛 404，登录队列进不去（疑似
  BetaDwarf 后端下线/迁移）。
- 游戏进程启动后 ~40–90 秒被移除（进主菜单后消失，疑似登录队列 404 后游戏自身退出）。
- Azure IP `140.228.24.145:443`（及 `:33798`）网络封锁仍在。

## 2026.08.23 — HDMI 音频（HDAudio codec）修复

### 修复

- **7G100 在本系统（Ubuntu 24.04）HDMI 音频不正常**（converter 3 无声 / 音频流
  重配置后丢声）：根因在 `vendor-src/display/hdaudio/disp_codec.c` 的
  `disp_codec_irq_prepare`。`Reg_Hau_Reg105` 的 `set_cov2_strm_fmt` 与
  `set_cov3_strm_fmt` 是**两个相互独立**的 bit（可同时置位），原代码却用
  `if/else` 互斥处理——当两者同时置位（HDMI 音频同时重配置 converter 2、3
  是常见场景）时，converter[3] 的 `stream_format`（采样率/位深）永远不更新、
  停留在 0（无效），导致该 converter 输出的音频无声或异常。改为两个独立 `if`。
  顺带修正其 `disp_print` 把 `pconverter[3]` 错打印为 `pconverter[1]` 的复制笔误。
  - 载体：补丁 `patches/0002-audio-fix-hdmi-converter-23-stream-format.patch`
    （在转录源码上干净应用）。

- **声音设置只显示"虚拟输出"（无真实声卡）**：这是**用户态音频服务没枚举到
  任何 ALSA 声卡**，与上面的内核 codec 逻辑是两层问题。本机有两张音频 PCI
  功能：
  - `05:00.1` `4c54:5010`：7G100 GPU 伴生 HD-Audio，是 `gf` 显示驱动**内部
    的透明 codec**（`disp_codec.c` / `display_hda_handle` / ELD / infoframe
    全自管），**不创建 ALSA 声卡、也不是标准 Intel HD-A 控制器**，绑给
    `snd_hda_intel` 是死路——已排除。
  - `07:00.6` `1d94:14c9`：海光（Hygon）板载 **标准 HD-A 控制器**（源于 AMD
    Zen），才是本机真实可出声的声卡；但其 vendor id `1d94` 不在
    `snd_hda_intel` 内置 PCI id 表，内核不自动 probe → BAR 不启用 → 无
    `/dev/snd/card*`。
  - 修复：`hygon/fix-hygon-audio.sh`（幂等，需 `sudo`）。完整根因链：
    1. `1d94` 不在 `snd_hda_intel` 的 PCI id 表 → `new_id` 返回 `-EINVAL`、
       `bind` 因 id 不匹配被静默拒绝。用 `driver_override` 绕过 id 表强制匹配。
    2. `snd_intel_dspcfg` 对未知 vendor 默认选 SOF/SST 路径，本机无固件 →
       `azx_probe` 静默失败（dmesg 无日志）。须在加载 `snd_hda_intel` **之前**
       以 `dsp_driver=1` 加载 `snd-intel-dspcfg` 强制原生 HDA。
    3. 绑定成功后 codec (ALC222) 上电慢，`azx_get_response` 超时。加
       `probe_mask=0xff delay=2000` 探测全部 codec 地址并等待上电。
    最终 `aplay -l` 出现 `card 0: Intel [HDA Intel], device 0: ALC222 Analog`。
  - 持久化：`/etc/modprobe.d/hygon-audio.conf`（`install` 指令带
    `probe_mask=0xff delay=2000` + `options snd-intel-dspcfg dsp_driver=1`）
    与 `hygon-audio-bind.service` 开机自绑（driver_override + bind）；
    重启 PipeWire/WirePlumber 接管新声卡。`--status` 看报告、`--uninstall` 撤销。
  - 注：7G100 的 HDMI 边带音频（`4c54:5010`）是 `gf` 显示驱动内部透明 codec，
    不创建 ALSA 卡，需厂商驱动支持，超出本脚本范围；板载出声走海光 HDA。

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

## 2026.08.05 — 完整修复栈与运维工具链

### 新增

- **交互式修复索引** `lisuan-fix.sh`（仓库根）：自动检测系统环境并给出结论
  （GPU/内核/DKMS/vermagic/DRI/Steam 补丁状态），编号菜单执行单项修复、体检、
  卸载；支持 `--detect` 与直接传编号的非交互模式。
- **开机自愈服务** `steam/steam-cef-autofix.sh`：systemd 单元
  `lisuan-steam-autofix.service`，Steam 客户端更新还原 CEF 补丁后开机自动
  重新注入并锁定；`--uninstall` 卸载。
- **卸载能力全覆盖**：每项注入均有对称、幂等的卸载入口——
  - `dkms/uninstall-lisuan-gpu.sh`（撤销 DKMS Makefile 补丁）
  - `mesa/uninstall-lisuan-mesa-driver-name.sh`（删除 DRI 驱动名软链）
  - `steam/fix-steam-cef-lock.sh --uninstall`（还原 Steam wrap.sh）
  - `steam/steam-cef-autofix.sh --uninstall`（卸载自愈服务）
- **厂商 bug 报告** `docs/vendor-bug-report-gf-gbm-crash.md`：三个问题
  （gf_gbm 段错误、容器内 EGL 分发、GLX vendor 注册缺失）的完整证据链、
  复现步骤、规避方案与修复建议，含规范化报告头（编号/日期/严重级别/状态）。
- `docs/system-mesa-analysis.md`：7G100 与系统 Mesa 25.2.x 的源码级兼容性分析。
- 测试套件（`tests/`）：补丁应用/反向、Makefile 逻辑、fix 脚本幂等性、
  shell 语法（含仓库根入口脚本）、仓库合规（无厂商产物混入）；
  Gitee Go 流水线（`.workflow/gitee-go-test.yml`）自动执行。
- Dota Underlords 宿主直跑方案：`play-native.sh` + 桌面启动项
  （绕开容器内 GLX 无解问题；补 libpng12 依赖）。

### 修复

- **Steam 界面无法启动**（`cef_log.txt` 大量 `exit_code=139`）：
  - 根因一：容器内系统 libgbm 加载厂商 `gf_gbm.so`（Mesa 22.0.4 时代后端）
    段错误 → 注入 `GBM_BACKEND=dri` 规避；
  - 根因二：Steam 自带 ANGLE 被 `LD_LIBRARY_PATH=.` 优先加载，延迟绑定的
    GL 符号在容器内无 glapi 提供者 → `LD_PRELOAD` 厂商 `libglapi_gf.so.0` 规避；
  - 载体：`steam/fix-steam-cef-lock.sh` 注入 `steamwebhelper_sniper_wrap.sh`
    并 `chattr +i` 锁定（抵御 Steam 启动校验还原；补 `chmod +x` 防丢执行位）。
- **DRI 驱动加载失败**（`driver=(null)`）：DRM 注册名 `lisuan` 与用户态驱动名
  `gf` 不一致 → `mesa/fix-lisuan-mesa-driver-name.sh` 建立
  `lisuan_dri.so → gf_dri.so` 等软链。
- **内核升级后驱动无法加载**（vermagic 错配/黑屏）：厂商 DKMS Makefile
  `KERNEL_VERSION`/`KERNELVER` 变量名错配 → 一行补丁
  （`patches/0001-dkms-makefile-honor-KERNELVER.patch`）+
  `dkms/fix-lisuan-gpu.sh` 一键重编重装。

### 实测更正（相对早期分析记录）

- 厂商 **EGL 在宿主可用**（X11/XCB/Wayland/GBM/surfaceless 平台，EGL 1.5）；
  Steam 容器内 UI 无硬件加速的真正卡点是容器 glvnd 分发拿不到厂商 EGL，
  而非"厂商 EGL 缺失"。
- 厂商 **Vulkan ICD 在容器内工作正常**（设备枚举/创建无崩溃）；
  容器内原生游戏崩溃的根因是 **GLX vendor 注册缺失**，而非 Vulkan 问题。

## 2026.07.31 — 初始修复

- DKMS Makefile `KERNELVER` 错配的一行补丁与一键修复脚本
  （`fix-lisuan-gpu.sh`）、只读体检（`verify-lisuan-gpu.sh`）。
- Apache-2.0 授权，LICENSE/NOTICE 完备；明确不收录任何厂商专有产物。
