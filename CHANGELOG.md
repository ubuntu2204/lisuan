# 更新日志（CHANGELOG）

本项目采用日期版本号。所有变更均在 Ubuntu 24.04 LTS + 砺算 7G100（PCI `4c54:5006`）
实机验证。

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
