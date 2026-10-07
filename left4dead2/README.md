# 《求生之路 2》（Left 4 Dead 2, AppID 550）7G100 硬件渲染修复

> 状态：**链路已打通，渲染受 gf 驱动能力限制**（2026-10-06）
> 修复脚本：`./fix-l4d2.sh`（幂等，支持 `--status` / `--uninstall`）
>
> **不使用** `left4dead2.exe`、**不使用 Proton/DXVK**、**不使用软件渲染**。
>
> ⚠️ 当前已知阻塞（详见「至砺算」1~3 条）：游戏能启动、能识别
> `LISUAN 7G100`，但因 gf 驱动无法创建 D16 深度附件而卡死，表现为"无响应"。

## 至砺算：官方实现什么可以让本方案退役

> 本节写给砺算官方驱动 / 图形栈研发。按投入产出比排序；**任意一项落地，本目录 workaround 即可退役**。

1. **补齐 Vulkan 深度格式能力（最关键）**：`vkGetPhysicalDeviceFormatProperties` 报告
   `D16_UNORM` / `D24_UNORM_S8_UINT` / `D32_SFLOAT` **全部没有
   `VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT`**。Vulkan 规范强制要求
   `D16_UNORM` 支持该能力。后果：DXVK 为 L4D2 创建 400x400 手电筒阴影贴图失败
   （`err: D3D9: Cannot create texture / Format: D3D9Format::D16`），游戏随后卡死、
   表现为"无响应"。已实测排除游戏侧原因（`r_flashlightdepthtexture` /
   `r_flashlightdrawdepth` / `r_flashlightupdatedepth` / `r_flashlightculldepth`
   全部置 0 均无效），失败发生在 `vkCreateImage` 本身。
   附注：同一查询对 `R8G8B8A8_UNORM` 也报 `COLOR_ATTACHMENT=False`（规范同样强制要求），
   说明 formatProperties 实现整体不可信。
2. **修 `vkGetPhysicalDeviceFormatProperties` 的返回值**：见上一条，报告与规范不符，
   会让上层（DXVK / Zink / DXVK）按"不支持"处理而放弃可行路径。
3. **加快着色器编译**：走 Zink（`-gl`）路线时，32 位客户端经 Venus 把命令发给服务端，
   gf 驱动编译 GLSL→SPIR-V 过慢，触发 Mesa Venus 客户端硬编码的 8 秒 fence 超时：
   ```
   MESA-VIRTIO: debug: stuck in fence wait with iter at 1024
   MESA-VIRTIO: debug: aborting on expired ring alive status at iter 1024
   ```
   （`MESA_VIRTIO` 无可调超时的环境变量）。编译速度提上来后 Zink 路线才可用。
4. **发布 32 位（i386）用户态驱动**：一旦提供，可跳过 Venus 整层 IPC。
2. 若短期无法出 32 位驱动：按随从大师文档「至砺算」清单加固 64 位栈
   （ICD 规范化、`VK_KHR_load_store_op_none` / `VK_KHR_maintenance5`、去掉 os-release
   白名单、libSC 自解析）—— 本方案的服务端会直接受益（它用的就是 64 位
   `libvulkan_gf.so`）。
3. 注：原生路线曾缺的 32 位 `libopenal.so.1` / `libpng12.so.0` 属发行版依赖，与驱动无关。

---

## 一、TL;DR

```bash
# 1) 完全退出 Steam（vdf 改动会被运行中的 Steam 覆盖）
# 2) 一键部署（幂等）
bash fix-l4d2.sh
# 3) 重启 Steam → 从库里启动《Left 4 Dead 2》
bash fix-l4d2.sh --status     # 随时查看各环节状态
bash fix-l4d2.sh --uninstall  # 停服务 + 还原 Steam 配置
```

游戏内验证 GPU 生效：控制台 `mat_info`，Renderer 应显示 **LISUAN**（而非 llvmpipe）。

---

## 二、方案：原生 32 位 `-vulkan` + Venus 进程外转发

### 2.1 原理

L4D2 的原生 Linux 版自带 **32 位** `bin/shaderapivk.so`（Valve 官方 Vulkan 后端），
所以游戏本体不需要任何 wine/DXVK。

剩下的唯一矛盾是**位数**：32 位进程不能 `dlopen` 64 位 `libvulkan_gf.so`
（同一进程不能混 ABI）。这里不硬加载，而是把渲染指令**转发出进程**：

```
hl2_linux -vulkan
  -> bin/shaderapivk.so          (ELF32)
  -> libvulkan_virtio.so         (ELF32, Mesa Venus 客户端 ICD)
  == Unix socket ==
  -> virgl_test_server --venus   (ELF64, 私有构建)
  -> libvulkan_gf.so             (ELF64, 厂商)
  -> 砺算 7G100
```

32 位侧只做命令序列化，像素由 gf 驱动在 GPU 上算 —— 因此**不是软件渲染**。
客户端 ICD 用的是 Ubuntu 自带的 i386 Mesa（`libvulkan_virtio.so`），无需自编译。

### 2.2 部署内容

| # | 组件 | 位置 | 作用 |
|---|---|---|---|
| 1 | 64 位 Venus 服务端 | `~/.local/share/l4d2-venus/build/` | 加载 gf ICD 真正渲染；由 `vtest-forward/build-venus-bridge.sh` 私有构建，不动系统 |
| 2 | systemd --user 单元 | `~/.config/systemd/user/l4d2-venus.service` | 随登录常驻服务端（`WantedBy=default.target`） |
| 3 | 宿主启动器 | `~/.local/share/l4d2-venus/l4d2-host-run.sh` | 真正跑游戏：确认 socket 就绪 → 注入环境变量 → `exec hl2.sh` |
| 4 | 32 位 setlocale shim | `~/.local/share/l4d2-venus/setlocale-shim.so`（源码 `setlocale-shim.c`） | 消除引擎的 `SetLocale failed` 误报弹框（见 §3.1）；`LD_PRELOAD` 注入，可关 |
| 5 | Steam 启动包装 | `~/.local/share/l4d2-venus/l4d2-venus-launch.sh` | Steam 链入口；检测到容器则委托宿主，否则直接跑宿主启动器 |
| 6 | Steam 启动选项 | localconfig.vdf | `<包装器> %command% -vulkan` |

三个关键环境变量（由宿主启动器注入，只影响游戏进程）：

- `VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/virtio_icd.json`
  → 该 JSON 的 `library_path` 是裸文件名，loader 按进程位数解析，32 位进程自动取 i386 版本。
- `VTEST_SOCKET_NAME=$XDG_RUNTIME_DIR/l4d2-venus.sock`
- **`VN_DEBUG=vtest`**（最关键，漏了就起不来）
  → Mesa 的 Venus ICD 默认走 virtio-gpu 内核路径（本机没有该设备），只有显式指定
  `vtest` 才会经 socket 连到我们的 64 位服务端。缺失时
  `vkEnumeratePhysicalDevices` 直接返回 `ERROR_INITIALIZATION_FAILED`，
  游戏表现为 `terminate called after throwing an instance of 'dxvk::DxvkError'`。
- `VK_LOADER_LAYERS_DISABLE=...steam_overlay_32,_64`：overlay 的 Vulkan 层会绕过 Venus ICD 直连，禁用。
- **整套 locale 显式统一为 `en_US.UTF-8`**（`LC_ALL`+`LANG`+`LC_CTYPE`/`LC_COLLATE`/`LC_TIME`/`LC_MONETARY`/`LC_NUMERIC`/`LC_MESSAGES`，**不设 `LOCPATH`**）：
  Source 引擎启动时硬调 `setlocale(LC_ALL,"en_US.UTF-8")`，失败则弹「Warning」框。委托到
  宿主时，游戏会从 systemd 用户管理器继承 `LANG=zh_CN.UTF-8`，与 `LC_ALL=en_US.UTF-8` 不一致；
  实测游戏会直接 `getenv` 读个别 `LC_*`/`LANG` 来判断（弹窗串里 `LC_CTYPE=C` 等混搭就是证据），
  歧义环境会触发误报。把整套显式设为 `en_US.UTF-8` 可消除歧义。glibc 层本身能正常读
  系统归档（实测 32 位探针可完整解析所有类别，strace 也见 `locale-archive` 打开成功）。
  ⚠️ 该弹窗**具间歇性**（同配置多次启动可能弹也可能不弹），需真实 Steam 启动多次验证。

### 2.3 为什么必须绕开 pressure-vessel 容器

Steam 会**把 Linux 原生游戏也包进容器**（实测启动命令含
`SteamLinuxRuntime_soldier/_v2-entry-point` + `scout-on-soldier-entry-point-v2`），
而 **`STEAM_RUNTIME=0` 拦不住**。容器内是 steamrt 自带的 i386 栈：

- 没有 `libvulkan_virtio.so`；
- 且其 glibc 太旧，无法加载宿主编译的 i386 ICD。

所以启动包装 `l4d2-venus-launch.sh` 检测到容器就**委托宿主**运行游戏：经共享会话总线
调宿主的 `systemd-run --user` 启动 `l4d2-host-run.sh`，并轮询服务状态阻塞到游戏退出，
让 Steam 的时长/退出检测依然有效。机制与 `../underlords/ul-host-run.sh` 同源。

> ⚠️ **容器判定不能只看 `/usr/lib/pressure-vessel`**（2026-10-06 实测踩坑）。
> 本游戏走的是 `SteamLinuxRuntime_soldier` + `scout-on-soldier`，这个容器里
> `/usr/lib/pressure-vessel`、`/run/pressure-vessel`、`/run/host` **三个都不存在**，
> 宿主根实际挂载在 **`/host`**。误判为宿主的后果是：游戏直接在容器内启动 →
> 32 位栈不完整 → **挂起成"无响应"**。
> 现在按 `/run/host`、`/host` 自动探测宿主前缀，并检查多个标志。
>
> 另一个坑：容器内的 `XDG_RUNTIME_DIR=/run/user/<uid>` 是**空目录**（看不到宿主的
> `bus`），`systemd-run --user` 会连不上用户 systemd。必须显式设
> `DBUS_SESSION_BUS_ADDRESS=unix:path=<hostpfx>/run/user/<uid>/bus`。
>
> 调试：每次启动会把判定结果写到 `/tmp/l4d2-venus-launch.log`
> （`in_container` / `hostpfx` / `bus` / 分支）。

> ⚠️ **vdf 改动必须在 Steam 完全退出后进行**：Steam 退出时会用内存中的配置覆盖
> `config.vdf` / `localconfig.vdf`。`steam -shutdown` 可优雅退出。

### 2.4 游戏语言（默认简体中文）

脚本会同时写两处，二者需保持一致：

- LaunchOptions 里的 `-language schinese`（游戏进程参数）
- localconfig.vdf 中 550 段的 `Language` = `schinese`（Steam 侧，决定使用/下载哪套语言包）

改语言用 `L4D2_LANG=english bash fix-l4d2.sh`（默认 `schinese`）。

中文字幕资源游戏已自带（`left4dead2/resource/closecaption_schinese.dat`、
`subtitles_schinese.dat`），字体用游戏自带的 `platform/vgui/fonts/xarialuni.ttf`
（含 CJK 字形），无需额外装字体。

> 若 Steam 属性里的语言与之不一致，Steam 可能在下次启动时重新下载语言包——
> 保持两边都是「简体中文」即可。

> ⚠️ **服务端必须常驻**：单元里的 `virgl_test_server` **不能**加 `--no-loop-or-fork`
> —— 该选项服务完第一个客户端就退出，会导致第二次启动游戏连不上 socket。
> 单元已用 `ExecStartPre=/bin/rm -f <socket>` 清理残留 socket 再 bind。

---

## 三、验证与排障

```bash
# 32 位冒烟测试（复用常驻服务端）
bash vtest-forward/run-l4d2-venus.sh --test
# 期望: Selected GPU 0: Virtio-GPU Venus (LISUAN 7G100 Series Graphics) → PASS

bash fix-l4d2.sh --status
# 期望: shaderapivk.so ✅ ELF 32-bit / systemd: active / socket ✅ /
#       CompatTool 未绑定 ✅ / LaunchOpts 含 -vulkan
```

| 症状 | 检查 |
|---|---|
| `terminate called ... 'dxvk::DxvkError'` | 几乎总是缺 `VN_DEBUG=vtest`（见 §2.2）；确认宿主启动器里有没有导出 |
| `Failed to detect any valid GPUs` | 同上没有走 vtest socket，或服务端没起（32 位 `vulkaninfo --summary` 一测便知） |
| 游戏起不来 / 无画面 | `systemctl --user status l4d2-venus`；socket 是否存在 |
| 委托后游戏没起来 | `journalctl --user -u l4d2-host-game`；确认 `XAUTHORITY` 解析成功 |
| 服务端崩 | `journalctl --user -u l4d2-venus` |
| `mat_info` 显示 llvmpipe | LaunchOptions 未生效（Steam 覆盖）或走了 `-gl` |
| 仍弹 `SetLocale failed` 框 | shim 没生效：查 `fix-l4d2.sh --status` 里 shim 是否 ✅；游戏进程 `tr '\0' '\n' < /proc/<pid>/environ | grep LD_PRELOAD` 应含 `setlocale-shim.so`；真实失败记录在 `/tmp/l4d2-setlocale.log` |
| 手动直跑调试 | `bash vtest-forward/run-l4d2-venus.sh --game` |

手动直跑脚本会复用已运行的常驻服务端（不会重复拉起，也不会删它的 socket）；
若服务端没起，它会临时拉起一个并在脚本退出时清理。

---

### 3.1 已知告警清单（哪些有害、哪些不用管）

| 告警 | 性质 |
|---|---|
| `Couldn't find custom font file 'resource/linux_fonts/*.ttf'` | **已修**：Valve 的 Linux 版没随包带字体，`fix-l4d2.sh` 会软链系统 DejaVu/Liberation 补齐（也影响中文 VGUI 文本） |
| `err: D3D9: Cannot create texture / Format: D3D9Format::D16 / 400x400` | **真问题**：Venus 链路下创建 D16 深度纹理失败（L4D2 的手电筒阴影贴图）。会让游戏以**退出码 0 静默退出**（启动约 26 秒后）。默认已规避：启动器自动加 `+r_flashlightdepthtexture 0`；要保留原画质用 `L4D2_NO_D16_WORKAROUND=1`（代价是随时可能退出） |
| `CMaterialReference::Init got error material for particle\*.vmt` | 粒子材质在已下载资源里查不到（不在 `pak01_dir.vpk` 索引中），游戏退回 error material。表现为部分粒子特效缺失，**不致命** |
| `failed to get driver name for fd 7 / No driver found` | SDL 启动时探测 OpenGL DRI（`4c54:5006` 在 Mesa 无驱动）失败。走 `-vulkan` 不需要 GL，**无害噪音** |
| `SetLocale('en_US.UTF-8') failed. Using 'LC_CTYPE=C;…'. You may have limited glyph support. Please install 'en_US.UTF-8' locale.`（弹「Warning」框 + stdout 一行 `WARNING: setlocale('en_US.UTF-8') failed, using locale:'C'.`） | **仅当游戏被「容器内直跑」时出现**：soldier 容器自带的 locale 归档不完整（约 3MB，缺 `LC_CTYPE/LC_COLLATE/LC_TIME/LC_MONETARY` 数据），Source 引擎 `setlocale('en_US.UTF-8')` 返回 NULL → 回落部分类别到 C 并弹框、随后挂起；宿主归档（约 6MB）完整，委托到宿主则不弹。**复发根因（2026-10-07 实测）**：Steam 运行中会用内存配置回写 `localconfig.vdf`，把 `LaunchOptions`/`Language` 刷掉（`--status` 里 `Language=未设置` 即信号），一旦启动项被清空，Steam 会绕过包装器直接跑 `hl2.sh` → 落回容器 → 弹框。**正确修法**：完全退出 Steam（`steam -shutdown`）后重跑 `fix-l4d2.sh`，让 `LaunchOptions`+`Language` 持久写回。**纵深护栏**：`l4d2-host-run.sh` 现用「容器独有、宿主必无」的标志（`PRESSURE_VESSEL_RUNTIME`/`PRESSURE_VESSEL_PREFIX`/`container=pressure-vessel`/`/run/pressure-vessel`）自检，若被误在容器内执行则 `exit 42` 报错而非静默弹框挂起（注意：`/host` 在部分沙箱视图里也存在，不作判据）。宿主 `locale-gen` 与 Steam 客户端自身启动时的同名告警是两回事，后者要重启 Steam 才消失。**最终定位（2026-10-07、多次迭代后的准确结论）**：①用 `xdotool`/`xprop` 抓到 `Warning` 窗口（`WM_TRANSIENT_FOR` 指向游戏窗口），确认是**委托到宿主的游戏进程**弹的，而它 env 里 `LC_ALL=en_US.UTF-8` 俱在却仍弹。②用**免 root 解包 `libc6-dev-i386`+`lib32gcc-13-dev` 编出真·32 位探针**实测：32 位 glibc **能完整读**系统归档和 64 位 `localedef` 编的 `LOCPATH` 目录的所有类别——所以“32 位读不了 64 位 locale”是错的（曾一度误判为元凶），设不设 LOCPATH 都不是根因；strace 也见游戏 `openat locale-archive` 成功。③真诱因（高置信、但弹窗**具间歇性**）：委托环境从 systemd 用户管理器继承 `LANG=zh_CN.UTF-8`，与 `LC_ALL=en_US.UTF-8` 不一致，游戏直接 `getenv` 读个别 `LC_*`/`LANG` 判断时产生命搭歧义→误报。**正解：HOST_RUN 把整套 locale 显式统一为 `en_US.UTF-8`**（`LC_ALL`+`LANG`+各 `LC_*`）、不设 `LOCPATH`；实测委托启动两次均无弹窗。`steam.desktop` 也只注 `LC_ALL=en_US.UTF-8`。（因弹窗非确定性，单次不弹不能 100% 定论，需真实 Steam 启动多次验证）**最终定性与处置（2026-10-07）**：把整套 locale 显式统一为 `en_US.UTF-8` 后游戏**仍会弹**，且弹窗串里 `LC_CTYPE=C` 与 env 的 `LC_CTYPE=en_US.UTF-8` 直接矛盾、strace 又确认 `locale-archive` 加载成功 → **这不是 locale 环境问题，而是 Valve Source 引擎自身的已知 bug**（见 `ValveSoftware/halflife#3006`）：跳过开场 movie 时引擎会报这个 `SetLocale failed` 警告。我们改不了游戏二进制。**确定性触发（用户提供）**：开场动画播放时按 ESC 跳过。**根因级缓解**：`l4d2-host-run.sh` 默认加 **`-novid`** 不放开场动画→消除触发源（且玩家本就会手动跳过）；想看动画设 `L4D2_KEEPVID=1`。曾加过一个 `l4d2-dismiss-warning.sh` 自动点0守护，但**已删除**——那是掩盖症状而非解决问题。彻底修复需等 Valve 修引擎。 |
| 上面那条 locale 框 | **已加 shim 兜住（2026-10-07 二次取证后定案）**：`left4dead2/setlocale-shim.c`。本轮把「环境/系统 locale 有问题」的可能性逐条实测排除：①`localedef --list-archive` 显示 `/usr/lib/locale/locale-archive`（6.2MB）含 `en_US.utf8`；②用免 dev 包的 32 位探针（`gcc -m32 -c` + `ld -m elf_i386 -e main … libc.so.6`）在同一台机器上 `setlocale(LC_ALL,"en_US.UTF-8")` 返回正常；③`/proc/<pid>/environ` 显示游戏进程 `LC_ALL/LANG/各 LC_*` 全为 `en_US.UTF-8`；④`readelf -sW` 确认 `engine.so/gameui.so/libcef.so` 的 `setlocale` 全是 **UND**（真 libc，无库自带实现覆盖）；⑤低 4GB 只映射 1275MB/4096MB（31%），排除 32 位地址空间耗尽。`xprop` 也确认弹框是游戏进程（`WM_TRANSIENT_FOR` → hl2_linux 窗口）的 GTK DIALOG/MODAL。**第二次取证推翻「引擎侧异常」，拿到准确根因（2026-10-07）**：反汇编 `engine.so` 的 `0x4105a0` 段发现，弹框条件**不是 `setlocale` 失败**，而是「`setlocale(LC_ALL,NULL)` 的返回值 ≠ `"en_US.UTF-8"`」。而 glibc 在 **LC_ALL 处于混合态**（各类别分属不同 locale）时返回的是复合名 —— 用 32 位探针 `locmix.c` 复现出的字符串与弹窗文本**逐字一致**（`LC_CTYPE=C;LC_NUMERIC=en_US.UTF-8;…`）。即：**游戏里有代码单独执行了 `setlocale(LC_CTYPE,"C")`**（该调用成功，所以 shim 的「失败日志」为空），把 CTYPE 从 `en_US.UTF-8` 降到 `C`，`LC_ALL` 因此变混合 → 引擎自检判失败 → 弹框。shim 改为：**任何 `setlocale(LC_ALL,NULL)` 查询若返回复合名（以 `LC_CTYPE=` 开头），就重新 `setlocale(LC_ALL,"en_US.UTF-8")` 把混合态拉回一致并返回规范名**（真修，不是骗检查）；同时把「LC_CTYPE 被降级」「归一化动作」「显式设置失败」连同调用返回地址写进 `/tmp/l4d2-setlocale.log` 用于定位是哪个模块在动 locale。`setlocale-shim.c` 内注释提醒：`lput()` 依赖 NUL 终止，所有临时缓冲必须补 `'\0'`（否则日志乱码）。关闭：`L4D2_NO_SETLOCALE_SHIM=1`。 |
| `warn: D3D9Cursor::ShowCursor: Not supported` | DXVK 在 Linux 原生的已知限制，无害 |
| `File ...pak01_0xx.vpk was never closed` | Source 引擎退出时未关闭 VPK 句柄，正常噪音 |
| `ld.so: gameoverlayrenderer.so ... ELFCLASS64: ignored` | 64 位 overlay 注入 32 位进程被 ld.so 拒绝，无害 |

### 3.2 闪退排查

游戏自带 breakpad，且 `ulimit -c` 常为 0，段错误往往拿不到现场。
宿主启动器已加 `ulimit -c unlimited`，下次闪退后：

```bash
ls -lt /var/crash/ | head          # apport 落盘（core_pattern 指向 apport）
dmesg | tail -20                   # 内核记录
journalctl --user -u l4d2-host-game   # 委托实例的游戏输出
journalctl --user -u l4d2-venus        # Venus 服务端
```

已知现象：32 位 `vulkaninfo` 在这条链路上会段错误（枚举设备能力时，仅输出 104 行），
说明 Venus + gf 在**设备能力查询**上不稳定；游戏本体在 gdb 下可稳定运行数分钟。

---

## 四、未采用的备选方案

### 4.1 Zink（OpenGL-on-Vulkan）—— 可用但非首选

系统 i386 Mesa 带 `zink_dri.so`，可让原生版走默认 ToGL/OpenGL 路径：

```
hl2_linux（默认 -gl） -> libGLX_mesa(32) -> zink_dri.so(32)
  -> libvulkan_virtio.so(32) -> socket -> 64 位服务端 -> gf
```

未采用的原因：比 `-vulkan` 多一层 GL→Vulkan 转换；且系统 Mesa zink 架在 gf 上
只报 **OpenGL 3.0**（见 `../mesa/system-mesa-analysis.md` §1），Source 1 的 ToGL
可能不够用。`-vulkan` 是 Valve 官方后端，无此限制。

### 4.2 Proton new WoW64 + DXVK —— 已删除（2026-10-06）

历史方案：用游戏目录自带的 Windows 版 `left4dead2.exe`，`PROTON_USE_WOW64=1` 让
32 位游戏经 wine wow64 thunk 调 64 位 gf Vulkan，DXVK 把 DX9 转 Vulkan。
五层配方（os-release hook / 协商垫片 ICD / overlay 层禁用 / libSC 软链 / DXVK 降 v2.3）
与随从大师同源，实测可用，但：

- 引入 wine + DXVK 翻译层，链路最长；
- 需要改动 Proton 目录（DXVK 降级、libSC 链接），Steam 更新 Proton 后会失效、需重跑；
- 用户明确要求走原生、不走 exe。

已从 `fix-l4d2.sh` 与本文档中彻底移除。若需回退，见 git 历史。
相关的 `gf-compat/` 部署目录（hook / 垫片 ICD / 驱动副本）不再生成，
已存在者可安全删除。

### 4.3 原生 32 位 + llvmpipe —— 已废弃

更早的方案（宿主直跑 + Mesa 软渲染），按「游戏禁用软件渲染」原则废弃。
为其安装的 i386 库（`libopenal.so.1`）与 `bin/libpng12.so.0` 保留无害，
本方案的原生路径同样需要它们。

---

## 五、相关

- 随从大师（32 位 Windows，Proton WoW64 + IL2CPP patch）：`../minion-masters/`
- 刀塔霸业（Source 2 原生 Linux 64 位，容器委托宿主直跑）：`../underlords/`
- 系统 Mesa / zink 能力分析：`../mesa/system-mesa-analysis.md`
