# 豆包 Windows 客户端：Deepin Wine 排障记录与 Wine 11 / Electron 路线评估

更新时间：2026-10-28  
平台：Ubuntu 24.04、海光 C86-4G、砺算 7G100、XWayland

## 1. 结论先行

**建议优先走「Proton 11 对应的原生 Wine 11 + 独立前缀」路线，保留 Deepin Wine 8.16 下的 1.56.6 作为可回退版本。**

不建议现在投入「把 Windows 豆包拆开后用 Electron 重新打包」。豆包不是只要替换 JavaScript 运行时就能迁移的普通 Electron 应用，主体是定制 Chromium 加大量闭源原生 DLL；完整重打包会持续碰到登录、推送、音视频、系统整合和自动更新等原生依赖。

如果只需要网页版聊天能力，可以另做一个连接豆包官网的 Linux Electron 外壳，这是成本较低的备用方案，但它不是 Windows 客户端的等价重打包，也不能保证客户端特有功能一致。

路线排序：

1. **Wine 11 运行官方 2.32.6**：优先，已有明确证据能解决 Deepin Wine 8 缺失 API 的一道硬门槛。
2. **Electron 外壳加载官方网页版**：可作为功能受限的备用客户端。
3. **Windows 资源 + Electron 全量重打包**：不建议，投入大、维护性差，且无法绕开闭源原生模块。

## 2. 当前实际状态

### 2.1 可回退的旧版

- 软件包：`doubao-wine 1.56.6wine3`
- 客户端：`1.56.6 / Chromium 129.0.6668.79`
- 路径：`~/.deepinwine/doubao-wine/drive_c/Program Files/Doubao/app`
- 已实测可显示主窗口，主进程可持续运行。
- 启动脚本：`doubao/run-doubao-wine.sh`
- 前缀修复脚本：`doubao/fix-doubao-wine.sh`

### 2.2 用户提供的在线安装器

文件：

`/home/ubuntu/下载/doubao_client_77737216855802805777_ext.exe`

信息：

- 64 位 PE 安装器，安装器自身版本 `2.25.16`。
- SHA-256：`865d6bf9bd18e498957314194a6d837607df850d87f0f3ed13a88ff543cf23e9`
- `/S` 可以静默安装，但会联网下载当前客户端，而不是安装安装器自身的版本号。
- 安装日志证明其实际下载的是 `2.32.6 / Chromium 147.0.7727.149`。

新版没有覆盖上述 `Program Files` 旧版，而是安装到了用户目录：

`~/.deepinwine/doubao-wine/drive_c/users/ubuntu/AppData/Local/Doubao/Application/app`

因此目前机器上同时存在两个客户端：

| 客户端 | 版本 | 当前状态 |
|---|---|---|
| `Program Files/Doubao/app` | 1.56.6 | Deepin Wine 8.16 + Mesa 下可用，作为回退 |
| `AppData/Local/Doubao/Application/app` | 2.32.6 | 官方最新安装，Deepin Wine 8.16 下仍崩溃，等待 Wine 11 验证 |

安装器还创建了 `~/Desktop/豆包.lnk`。现有 `run-doubao-wine.sh` 仍指向旧版，不要把它误当成新版启动器。

## 3. Deepin Wine 8.16 已确认的坑

### 坑 1：安装包把 WinSxS DLL 链接写死到打包者 home

最初错误是：

`Failed to load Chrome DLL ... app\Doubao.dll: 找不到模块 (0x7E)`

`Doubao.dll` 文件实际存在。用 `WINEDEBUG=+loaddll,+module` 追踪后，真实依赖链为：

`comctl32.dll → compstui.dll → WINSPOOL.DRV → Doubao.dll`

Wine 实际选择的是 WinSxS 中的 `comctl32.dll`，但安装包把链接写成了：

`/home/zcr/.deepinwine/doubao-wine/dosdevices/c:/...`

在当前用户 `ubuntu` 下这些都是悬空链接。共修复 28 个，改成当前前缀内的相对链接。只重建整个前缀会再次带入相同问题，不能算根因修复。

### 坑 2：缺少 LocalLow 会触发 cryptnet 的 -1/NULL 错误处理缺陷

修完 DLL 链接后，主进程进入 Chromium 初始化，随后在证书处理阶段崩溃。

转储中的关键调用链：

`cryptnet → ucrtbase!fread → ntdll!RtlEnterCriticalSection`

Wine 8.16 上游代码中，`open_cached_revocation_file()` 在 `SHGetKnownFolderPath(FOLDERID_LocalAppDataLow)` 失败时返回 `INVALID_HANDLE_VALUE (-1)`，但调用方只检查 `NULL`：

- `fread(..., file)` 收到 `FILE * = -1`；
- ucrtbase 的文件锁包装给指针加 `0x30`；
- 最终 `RtlEnterCriticalSection` 读到地址 `0x4f`，产生访问违例。

本次前缀里注册表有 LocalAppDataLow 路径，但当前用户目录下没有实际目录。创建：

`drive_c/users/ubuntu/AppData/LocalLow`

后，证书缓存目录能够正常生成，不再发生这一崩溃。

**不要通过全局替换 Wine 的 cryptnet.dll 来掩盖它。目录缺失才是本机触发条件。**

### 坑 3：全局 gf 图形环境会让豆包卡住或反复崩 GPU 子进程

系统环境里有：

- `__GLX_VENDOR_LIBRARY_NAME=gf`
- `LIBGL_ALWAYS_SOFTWARE=0`
- `VK_ICD_FILENAMES=...gf...`

豆包使用 ANGLE/D3D11，再经 WineD3D 到宿主 GL。默认 gf 路径下出现过：

- 主进程长时间存活但没有窗口；
- `skia_output_surface_impl.cc` FATAL；
- GPU 子进程反复退出；
- 主进程随后异常结束。

仅对豆包进程设置以下环境后，旧版能够显示窗口：

- `__GLX_VENDOR_LIBRARY_NAME=mesa`
- `LIBGL_ALWAYS_SOFTWARE=1`
- `GBM_BACKEND=dri`
- `VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json`

注意：直接把 `--disable-gpu` 传给豆包并不适合当前版本，测试中主窗口没有出现。已验证可用的是**原启动器 + Mesa 环境，让客户端自行进入软件回退**，而不是强行禁用整个 GPU 路径。

这些变量只能注入豆包进程，不能污染 Steam、Electron 或其他 Wine 应用。

### 坑 4：桌面入口的裸 `--uri` 会进入非主窗口流程

原桌面项为：

`Exec=".../run.sh" --uri %F`

普通点击、没有文件参数时仍会留下裸 `--uri`。客户端随后进入修复/更新流程，而不是正常主窗口。

当前 `run-doubao-wine.sh` 只过滤“单独一个 `--uri`”的情况，真实 URI 仍原样传递。

### 坑 5：降级不只是替换 app 目录

新版升级后再恢复旧版，直接复制旧程序仍可能崩溃或进入“豆包异常检测”。还需要处理：

- `User Data/Last Version`：从 `147.0.7727.149` 恢复为 `129.0.6668.79`。
- 新版生成的 `ShaderCache`、`GrShaderCache`、`GPUCache`、Dawn 和 Chromium 缓存。
- `repair/record.json` 中的 `running_exception`，否则启动器会进入异常检测。
- `update_data` 中已下载的新版本。

不能只删整个 `User Data`，否则会无谓丢失账号和聊天数据。

### 坑 6：修复器会把“禁用 update.exe”当成文件缺失

曾将 `update.exe` 改名以避免自动更新，结果：

`User Data/repair/missing_files` 中出现 `update.exe`，客户端进入修复流程。

所以更新器文件本身必须保留。后来测试了保留有效 PE、将入口改成 `xor eax,eax; ret` 的方式，但这仍不是长期可靠的版本锁定方案，见坑 7。

### 坑 7：版本屏蔽配置和 patch 更新器都不能保证永不升级

实测以下配置不能阻止 2.32.6 再次安装：

`vulcan_update_block_versions = [ "2.32.6" ]`

而仅 patch `Program Files/Doubao/app/update.exe` 也不够：旧客户端会从下面的目录重新复制原始更新器覆盖它：

`User Data/update_data/Doubao_update/update.exe`

日志明确出现：

`Copy update.exe from ... update_data/Doubao_update ...`

因此当前旧版方案应理解为**可回退的临时兼容方案**，不要把它当作长期更新管理方案。迁移到能运行新版的 Wine 才是根治方向。

### 坑 8：新版在 Deepin Wine 8.16 下缺少 ntdll API

将前缀切到 Windows 11 后，系统构建号已从 18362 变为 22000，但 2.32.6 仍立即报：

`FATAL:base\win\win_util.cc(544) Check failed: . : 找不到过程。 (0x7F)`

豆包使用的定制 Chromium 与上游行号不完全一致，不能直接拿上游第 544 行机械对号入座；但同文件中有明确的候选：

- `GetProcAddress(ntdll, "RtlGetDeviceFamilyInfoEnum")`
- 随后 `PCHECK(get_device_family_info_fn)`，找不到函数就触发检查失败。

实测导出：

| 运行时 | `RtlGetDeviceFamilyInfoEnum` |
|---|---|
| Deepin Wine 8.16 | **没有导出** |
| Proton 11 / Wine 11 | **有导出**（RVA `0x27510`） |

这是优先尝试 Wine 11 的直接技术依据，而不是单纯“版本越新越好”的猜测。

## 4. 为什么优先考虑 Proton 11 对应的 Wine 11

### 4.1 已有优势

1. 已具备新版需要的 `RtlGetDeviceFamilyInfoEnum`。
2. 本机已有 Proton 11.0，`wine --version` 为 `wine-11.0`。
3. 项目已经积累 Proton 11 的 WoW64、宿主直跑、gf Vulkan、DXVK 和低地址映射经验。
4. 豆包是 64 位程序，不涉及《随从大师》那种 32 位进程接不住高地址显存映射的问题，不应套用那些低位重映射 hack。
5. 可给新版单独前缀，不破坏旧版回退路径。

### 4.2 已经踩过的 Proton 直跑坑

直接执行 `Proton 11.0/files/bin/wine` 时，不能只给 `WINEPREFIX`：

- 缺少 `WINEDLLPATH` 会报 `libvkd3d-1.dll`、`libvkd3d-shader-1.dll`、`libvkd3d-utils-1.dll` 找不到。
- `LD_LIBRARY_PATH` 也要包含 Proton 自带宿主库目录。
- Wine 拒绝在 `/tmp` 下创建前缀，因为 `/tmp` 不属于当前用户。
- 原生 Proton 前缀的 Windows 用户可能是 `steamuser`，不要假设一定是 `ubuntu`。

Proton 自己的 `proton` 脚本使用：

- `WINEDLLPATH=files/lib/vkd3d:files/lib/wine`
- `LD_LIBRARY_PATH` 包含 `x86_64-linux-gnu`、`i386-linux-gnu` 等目录。

### 4.3 当前尚未完成的部分

早前在临时前缀中使用 Proton 11 + 完整运行库环境启动 2.32.6 时：

- GPU 和 renderer 子进程能初始化；
- 但 `CrBrowserMain` 随后退出；
- 没有形成可用主窗口，未达到验收标准。

那次测试发生在本轮 Windows 11 版本模式和详细 FATAL 定位之前。因此，**Wine 11 是有明确依据的下一步，不等于已经迁移成功。**

下一步应在新的 Wine 11 前缀中设置 `win11`，再启动官方 2.32.6，确认它是否越过 `RtlGetDeviceFamilyInfoEnum` 后继续运行。如果还有第二个错误，应继续按日志定位，而不是退回盲目换包。

## 5. 推荐的 Wine 11 验证环境

下面是环境模板，不是已经验证成功的最终启动器：

```bash
PROTON="$HOME/.local/share/Steam/steamapps/common/Proton 11.0"
export WINEPREFIX="$HOME/.deepinwine/doubao-wine11"

export WINEDLLPATH="$PROTON/files/lib/vkd3d:$PROTON/files/lib/wine"
export LD_LIBRARY_PATH="$PROTON/files/lib/x86_64-linux-gnu:$PROTON/files/lib/i386-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export WINEDLLOVERRIDES='mscoree,mshtml='

# 先稳定显示，再单独评估 gf 硬件加速。
export __GLX_VENDOR_LIBRARY_NAME=mesa
export LIBGL_ALWAYS_SOFTWARE=1
export GBM_BACKEND=dri
export VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json
export WINE_D3D_CONFIG='renderer=gl'

"$PROTON/files/bin/wine" winecfg -v win11
```

安装时可使用用户提供的 `/S` 安装器。安装后要先用 `find` 确认实际落点，不要预设 Windows 用户名：

```bash
find "$WINEPREFIX/drive_c/users" -path '*/Doubao/Application/app/Doubao.exe' -print
```

首次启动建议带 `--enable-logging=stderr`，并单独记录日志。不要使用旧的 `fix-doubao-wine.sh` 去修新的 Wine 11 前缀，它包含针对旧版 Deepin 前缀的回退和更新器处理。

## 6. Electron 路线评估

### 6.1 全量重打包 Windows 客户端：不建议

从安装目录看，豆包包含：

- 约 308 MB 的 `Doubao.dll`；
- `doubao_elf.dll`、`logifier_retrieval.dll`、`ffmpeg.dll`；
- `dxcompiler.dll`、`cudnn_graph64_9.dll`；
- RTCSDK、模型、屏幕共享、推送、全局快捷键等原生模块；
- `local_webcontents` 中的网页资源。

这意味着它不能只按“HTML/CSS/JS + Electron”处理。即使网页资源能够抽出，也仍需替换或重写：

1. 登录态、设备 ID、加密存储；
2. WebSocket/推送与遥测；
3. 音视频、RTC、模型和本地推理；
4. 全局快捷键、剪贴板、文件拖拽、屏幕共享；
5. 自更新、修复器、协议注册和桌面集成。

这些恰好是最难维护、最容易随服务端变化失效的部分。最终会变成长期逆向维护一个非官方客户端，而不是一次性重打包。

### 6.2 做一个加载官方网页版的 Electron 外壳：可行但功能受限

这条路线不碰 Windows 原生 DLL，客户端只负责显示官方 Web 页面，服务端功能由网页本身提供。优点是：

- 使用原生 Linux Chromium，不再受 Windows API 缺失影响；
- 可复用项目 `electron/` 中已验证的 X11、sandbox、GPU 合成和 gf 加速经验；
- 升级主要发生在网页服务端，本地不用追 Windows 安装器。

限制：

- 不保证拥有 Windows 客户端的模型、RTC、全局快捷键和系统整合能力；
- 登录方式、文件上传下载和通知要逐项验证；
- 它是新的 Web 外壳，不是官方桌面客户端的等价迁移。

如果走这条路，默认先用软件渲染；稳定后再参考 `electron/use-electron-gf.sh` 评估 gf，不要第一天就叠加硬件加速变量。

## 7. 建议实施顺序

1. **冻结旧版回退路径**：保留 `Program Files` 的 1.56.6、原安装包和现有修复脚本。
2. **建立独立 Wine 11 前缀**：不要覆盖 `doubao-wine`。
3. **在 Wine 11 中安装并首次运行 2.32.6**：先 Mesa、Windows 11 模式、详细日志。
4. **先空配置验证，再迁移数据**：避免把 Deepin 前缀中的版本状态、修复记录和 GPU 缓存一起带过去。
5. **逐项验收**：窗口、登录、文字对话、文件上传、音频/RTC、重复启动。
6. **稳定后再切换桌面入口**：旧入口保留到新版连续手动启动验证通过。
7. **最后才评估 gf 加速**：Wine API 兼容与 gf 渲染是两个问题，不能混在一起排查。

账号数据不要直接整目录覆盖。日志里已出现 `AppBoundEncryption` 错误，跨 Wine/前缀迁移后应有重新登录的心理准备。

## 8. 验收与回退标准

新版至少满足以下条件，才替换旧启动器：

- 60 秒内出现主窗口；
- `CrBrowserMain` 连续存活 10 分钟以上；
- 没有新的 browser 主进程 Crashpad 转储；
- 登录和文字对话可用；
- 连续进行 3 次手动启动均正常；
- 版本保持官方 2.32.6，不需要禁用更新器才能运行。

如果 Wine 11 仍失败：

- 保留 `Program Files` 旧版和 `run-doubao-wine.sh`；
- 用对应 Proton 的 `wineserver -k` 只关闭 Wine 11 测试前缀；
- 不做崩溃后自动重启，不用 supervisor 掩盖主进程退出；
- 再决定是继续修下一个 Wine API，还是退到功能明确的网页版 Electron 外壳。

## 9. 相关文件

- `doubao/fix-doubao-wine.sh`：Deepin 前缀链接、LocalLow、旧版回退及更新处理。
- `doubao/run-doubao-wine.sh`：旧版的 Mesa 启动器和空 URI 过滤。
- `electron/README.md`、`electron/use-electron-gf.sh`：原生 Electron/Chromium 在 7G100 上的现有适配经验。
- Proton 的 `proton` 脚本：官方运行库环境变量来源。
