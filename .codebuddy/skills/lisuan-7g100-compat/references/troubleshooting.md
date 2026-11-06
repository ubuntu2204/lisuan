# 砺算 7G100 分层排障与既有结论

所有结论以厂商驱动 **v262.2.0.56** 为验证基准。遇到其它版本先告警，不直接套用字节偏移、专有库路径或游戏 workaround。

## 1. 先取证，后修改

```bash
cd /home/ubuntu/project/lisuan
python3 .codebuddy/skills/lisuan-7g100-compat/scripts/check_driver_baseline.py
python3 install.py --detect --json
bash dkms/verify-lisuan-gpu.sh
bash mesa/verify-system-mesa.sh
lspci -nnk
lsmod | grep -E 'gf|lisuan'
modinfo "$(find /lib/modules/$(uname -r) -name 'gf.ko*' | head -1)" | grep vermagic
dkms status
```

判定顺序：

1. PCI `4c54:5006` 是否存在；
2. `lisuangpu-dkms` / `lisuangpu-lib` 是否均为 v262.2.0.56；
3. `gf.ko` 的 vermagic 是否与 `uname -r` 一致；
4. `dkms status` 的 installed 不等于模块已加载，必须查 `lsmod`；
5. `/dev/dri/card*` 与 DRM 注册名；
6. 用户态 DRI/GLX/Vulkan/GBM；
7. 应用进程树与日志。

## 2. 内核 / DKMS

- 厂商 `Makefile` 以 `KERNEL_VERSION` 决定编译内核，但 DKMS 传入 `KERNELVER`；内核升级后出现 vermagic 错配。
- 使用 `dkms/patches/0001-dkms-makefile-honor-KERNELVER.patch` 与 `dkms/fix-lisuan-gpu.sh`，保持 `vendor-src/Makefile` 原样未补丁，补丁在部署阶段应用。
- 修复后重新体检 `gf.ko` 的 vermagic，不能只看 DKMS installed。

## 3. Mesa / GLX / Vulkan / GBM

- DRM 注册名为 `lisuan`，用户态驱动名为 `gf`；PCI 厂商号 `4c54` 对应字符串 `LT`。
- GLX vendor 环境变量是双下划线 `__GLX_VENDOR_LIBRARY_NAME=gf`；在 Steam Runtime 容器内 gf Vulkan ICD 可自动识别。
- 厂商 `gf_gbm.so` 与系统 libgbm 25.2.8 ABI 不兼容，Steam CEF GPU 进程会 SIGSEGV；GFM 后端用 `GBM_BACKEND=dri` 强制加载 Mesa `dri_gbm.so`。
- 厂商 Mesa 22.0.4 分叉不能被系统 Mesa 25.2.x 直接加载；zink 是已分析的替代路径，详见 `mesa/system-mesa-analysis.md`。
- gf Vulkan ICD 的协商接口版本为旧式版本 0，新版 loader 可能静默丢弃；`minion-masters/gf-compat/` 垫片对外报版本 5。
- gf 缺少 `VK_KHR_maintenance5`，Proton Experimental 自带 DXVK v3.0.2 会跳过设备；随从大师方案使用自定义 Proton 副本或 Proton 11 加固，降级到 DXVK v2.3。
- 禁止创建 `lisuan_dri.so -> gf_dri.so` 全局软链；禁止向 Steam 客户端注入 `LD_PRELOAD=libglapi_gf.so.0`。

## 4. Steam CEF

- 正确启动命令是 `GBM_BACKEND=dri steam`。
- 检查 `~/.local/share/Steam/logs/cef_log.txt` 中 `exit_code=139` 或 `GPU process isn't usable`。
- 当前唯一修复是用户级 `steam.desktop` 的 `Exec` 行注入 `GBM_BACKEND=dri`，由 `install_steam_fix()` 宽松解析并保留用户其它 env 定制。
- 不要修改 `steamwebhelper_sniper_wrap.sh`；不要 `chattr +i`；不要注入 `--disable-gpu`；这些会触发 Steam 校验回滚甚至反复重下游戏。
- Steam 的 `steamid=0` 陈旧态通常重启客户端即可，不需要图形登录界面；首次登录例外。

## 5. Electron / Chromium

- 通用 wrapper：`electron/use-electron-gf.sh`，基础开关 `--no-sandbox --ozone-platform=x11 --ozone-platform-hint=x11 --disable-gpu-compositing`。
- Electron 43 会忽略 `--ozone-platform-hint=x11`，必须同时给显式 `--ozone-platform=x11`。
- gf 硬件路径需 ICD 协商垫片补报 `VK_KHR_external_semaphore_capabilities` / `_fence_capabilities`；默认先用 `--no-hw` 软件渲染建立稳定基线。
- Trae 在 gf EGL 路径下黑屏/闪烁，默认使用 ANGLE/SwiftShader 软件路径。
- 不要把 gf env 写成全局；只注入目标应用进程树。

## 6. 音频

- 真正出声的是海光板载 HDA `1d94:14c9`，不是 7G100 伴生音频 `4c54:5010`。
- 三层根因：PCI ID 不在 `snd_hda_intel` 表中、`snd_intel_dspcfg` 误选 SOF/SST、ALC222 上电慢。
- 解法：`driver_override` + `dsp_driver=1` + `probe_mask=0xff delay=2000`，开机自绑服务。
- 验证：`aplay -l` 出现 `card 0: Intel [HDA Intel], device 0: ALC222 Analog`。

## 7. Proton / Wine 决策

- 64 位原生应用与 64 位 Wine 不需要 32 位低 4GB 重映射。
- 32 位游戏经 new WoW64 调 64 位 gf 驱动时，`vkMapMemory` 可能返回高于 4GB 的地址，WoW64 thunk 无法交给 32 位代码，DXVK 报 `VK_ERROR_OUT_OF_HOST_MEMORY`。随从大师的隔离方案用 `LD_PRELOAD` 对目标游戏做低 4GB 重定位，不全局补丁驱动。
- 原生直跑 Proton 必须提供：
  - `WINEDLLPATH=<Proton>/files/lib/vkd3d:<Proton>/files/lib/wine`
  - `LD_LIBRARY_PATH=<Proton>/files/lib/x86_64-linux-gnu:<Proton>/files/lib/i386-linux-gnu`
  - 独立 `WINEPREFIX`
  - `WINEDLLOVERRIDES=mscoree,mshtml=`
  - Mesa/llvmpipe 或经确认的 gf 渲染环境
- Wayland 下手动直跑 Wine 可能缺 `VK_KHR_surface`，经 Steam Runtime 容器可正常。
- 用 llvmpipe 建立稳定基线：`VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json`、`PROTON_USE_WOW64=1`、`__GLX_VENDOR_LIBRARY_NAME=mesa`。
- 不要手动降级 Proton 自带 DXVK；修改官方 DXVK 文件会触发 Steam“内容文件损坏”。

## 8. 应用 / 游戏既有结论

### 随从大师（Minion Masters, 489520）

- Proton 11.0 稳定版 + 加固脚本已实测完整可玩；加固三要素：`libSC.so` 软链、DXVK v2.3、doorstop 直载 `MMPatch.dll`。
- 真正生效源码是 `assets-mm-gf-vulkan/MMPatch.cs`；Vivox patch 必须停用，否则拿不到 token 并卡登录。
- Vivox 在 Wine 下本身可正常登录；证书 patch 曾破坏 IL2CPP runtime 导致黑屏，必须禁用。
- 使用 `il2cpp_class_get_method_from_name`，不要用 `il2cpp_class_get_methods`（binding 签名错）。
- `LoginQueueService` HTTP 404 是非阻塞噪音。
- llvmpipe 软件渲染方案需从 Steam 启动，命令行直跑约 90 秒后因缺 Steam 认证上下文退出。

### 刀塔霸业（Underlords）

- 原生 64 位 `play-native.sh`；必须保持 `--no-autorestart`，崩溃后让用户感知，不自动拉起。
- 系统原版 `libvulkan_gf.so` 与 `osrelease_redirect` 隔离验证通过，不影响其他游戏。

### 求生之路 2（L4D2, 550）

- 原生 32 位 `hl2_linux -vulkan` → DXVK v2.0 → 32 位 Mesa Venus ICD → Unix socket → 私有 64 位 `virgl_test_server --venus` → `libvulkan_gf.so`。
- D16/MatQueue0 闪退由 gf 错误上报 `VK_EXT_attachment_feedback_loop_layout` 引起，Venus 补丁 0002 屏蔽扩展。
- locale 模态 Warning 由 `launcher.so` 与 `steamclient.so` 设置冲突引起；`setlocale-shim.c` 仅在确认调用者为 `steamclient.so` 时改写 `LC_ALL`。
- `fix-l4d2.sh` 使用 `systemd-run --wait` 传真实退出码，不做隐式重启。

### 植物大战僵尸 GOTY

- 32 位 Windows 游戏；new WoW64 + `PROTON_USE_WINED3D=1` + `WINE_D3D_CONFIG=renderer=gl` + 宿主软件渲染。
- 默认 `Is3D=0` 软件路径；wined3d Vulkan 后端不上报 alpha blend 能力，DXVK 又要求 gf 不支持的 `maintenance5`。
- 容器内无图形栈时 `winex11.drv` 加载失败，`explorer.exe` 建窗失败；不要靠自动重启掩盖。

### 豆包 Windows 客户端

- Deepin Wine 8.16 的已知坑：WinSxS 绝对软链指向 `/home/zcr`、`AppData/LocalLow` 缺失触发 cryptnet `FILE* = -1`、gf 图形环境卡住、桌面裸 `--uri` 进入修复、降级后 Chromium 缓存和 `Last Version` 不兼容、自动更新到 2.32.6。
- 旧版 1.56.6 / Chromium 129 在修复前缀 + Mesa 软件渲染下可用；新版 2.32.6 / Chromium 147 在 Deepin Wine 8.16 缺 `RtlGetDeviceFamilyInfoEnum`，Proton 11 已导出该函数。
- Wine 11 独立前缀目前只越过缺失 API，尚未完成“窗口 + 主进程 + 重复启动”验收；详见 `doubao/README.md`。
- 禁止把 `update.exe` 改名，豆包修复器会把它列入 `missing_files`；版本屏蔽配置也不能阻止 `update_data` 覆盖更新器。
- Windows 客户端全量 Electron 重打包不推荐；网页版 Electron 外壳是功能受限的备用方案。

## 9. 日志与验收

- `Player.log` 被 `NotSupportedException` 刷屏时先 `grep -vE` 滤噪，再看最初 20 行。
- Unity 启动会轮转 `Player.log` 为 `Player-prev.log`，用 mtime 区分本次运行。
- 崩溃取证可用 `coredumpctl`、`ulimit -c unlimited`、`dmesg`、`journalctl --user`、Crashpad 转储；先定位根因，不自动重启。
- 验收至少包括：目标窗口出现、主进程持续存活、关键错误计数为 0 或确认为既有非阻塞噪音、重复启动稳定、用户数据未被清除。
