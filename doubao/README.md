# 豆包 Windows 客户端：Wine 11 排障记录（目标：官方最新版 2.32.6）

更新时间：2026-11-07  
平台：Ubuntu 24.04、海光 C86-4G、砺算 7G100、XWayland

> 约定：本文所有结论以**官方最新版 2.32.6** 为目标。旧版 1.56.6 只作为历史对照，
> 不作为解决方案、回退方案或验收参照。详见 `.codebuddy/rules/豆包客户端版本约定.md`。

## 1. 结论先行

**主线是「Proton 11 对应的原生 Wine 11 + 独立前缀」跑官方 2.32.6。**

当前实测（2026-11-07）：

- 新版 2.32.6 在 **Wine 11（Proton 11.0）** 下已经**越过** Deepin Wine 8.16 的硬门槛
  `RtlGetDeviceFamilyInfoEnum`（不再出现 `FATAL:base\win\win_util.cc ... 找不到过程 (0x7F)`）；
- browser / crashpad / gpu-process / 2×renderer / network service / storage service
  **全部存活**，无 FATAL、无 `CHECK failed`、无 GPU 进程崩溃退出；
- **未解决问题**：窗口始终 `IsUnMapped`，强制 map 后内容纯黑（RGB 0,0,0）。
  网络已排除（winsock 探针 IPv4/IPv6 全部 `CONNECT OK`），
  渲染后端已排除（swiftshader / angle-gl / 禁用 GPU 合成 / gf 硬件 Vulkan 五种组合均为纯黑）。
  下一步定位方向是 Chromium 的 Ozone/X11 后端选择与 viz 合成器首帧产出。

不建议现在投入「把 Windows 豆包拆开后用 Electron 重新打包」。豆包不是只要替换 JavaScript 运行时就能迁移的普通 Electron 应用，主体是定制 Chromium 加大量闭源原生 DLL；完整重打包会持续碰到登录、推送、音视频、系统整合和自动更新等原生依赖。

如果只需要网页版聊天能力，可以另做一个连接豆包官网的 Linux Electron 外壳，这是成本较低的备用方案，但它不是 Windows 客户端的等价重打包，也不能保证客户端特有功能一致。

路线排序：

1. **Wine 11 运行官方 2.32.6**：主线，当前唯一目标。
2. **Electron 外壳加载官方网页版**：可作为功能受限的备用客户端。
3. **Windows 资源 + Electron 全量重打包**：不建议，投入大、维护性差，且无法绕开闭源原生模块。

## 2. 当前实际状态

### 2.1 目标版本（唯一目标）

- 客户端：`2.32.6 / Chromium 147.0.7727.149`（2026-11-07 官方接口确认仍是最新）
- 路径：`AppData/Local/Doubao/Application/app`（新版安装到用户目录，不覆盖 `Program Files`）
- Wine 11 前缀：`~/.deepinwine/doubao-wine11`（Windows 用户名为 `steamuser`）
- 安装入口：`doubao/install-doubao.sh`（2026-11-07 新增，已完整跑过两轮）
- 启动器：`doubao/run-doubao-wine11.sh`
- 状态：**未达到验收标准**。窗口 `1200x800` 始终 `IsUnMapped`；
  且 2026-11-07 重测发现主进程 `CrBrowserMain` 在启动后 10 秒内退出（无 Crashpad 转储），
  只剩 crashpad-handler / gpu-process / 2×renderer 成为孤儿进程。
  早先「browser 全部存活」的结论需要重新验证，黑屏排查要先把主进程退出查掉。

### 2.2 旧版（仅作历史对照，不得作为方案）

- 客户端：`1.56.6 / Chromium 129.0.6668.79`
- 路径：`Program Files/Doubao/app`
- 早期实测：在 Deepin Wine 8.16 + Mesa 软件渲染下能显示完整 `doubao.com/chat/` 界面。
- 这条记录仅用于对比崩溃点差异；**不得**拿它当验收参照，也不得把修复脚本做成自动回退到它。

### 2.3 用户提供的在线安装器

文件（仓库内副本已进 `.gitignore`，`doubao/doubao_client_*.exe`，不入库）：

- `doubao/doubao_client_77737216855800721777_ext.exe`
- `~/下载/doubao_client_77737216855800721777_ext.exe`
- `~/下载/doubao_client_77737216855802805777_ext.exe`（上一轮下载）

信息：

- 64 位 PE 安装器（字节跳动 Saman 安装器），安装器自身版本 `2.25.16`，
  内嵌 `chromium_version 147.0.7727.89`、`product_app_id 582478`。
- SHA-256：`865d6bf9bd18e498957314194a6d837607df850d87f0f3ed13a88ff543cf23e9`
  —— 上面三个文件字节相同，文件名里那串数字只是下载地址编号，**不代表客户端版本**。
- 真正的客户端包由接口下发，安装前可先用 curl 预查（无需 Wine、无需登录）：

  ```bash
  curl -s 'https://www.doubao.com/service/settings/v3/?device_platform=pc&brand=doubao&aid=582478&os=windows&update_type=downloader&version_code=2.25.16&os_version=10.0.22000' \
    | grep -oE 'doubao_pc/[0-9.]+/Doubao_win_X64_[0-9.]+-signed\.zip'
  ```

  2026-11-07 21:44 实测返回 `doubao_pc/2.32.6/Doubao_win_X64_2.32.6-signed.zip`，
  即**官方 Windows x64 最新版仍是 2.32.6**，与本机已装版本一致。

#### `/S` 不是静默安装（2026-11-07 实测推翻旧结论）

早期记录写的「`/S` 可以静默安装」不成立。安装器日志明确显示：

```
[install] CmdLine:/S
[install] saman command line install mode is      ← 空，/S 没被当成命令行安装模式
[install] CBaseWindow::OnSize to 718x478           ← 弹出 GUI 后就此停住
```

窗口停在「立即安装」按钮上**空等人工点击**（实测空等 4 分钟无进展），
不点击就什么都不会发生。因此安装入口已固定为 `doubao/install-doubao.sh`：
它用 `_NET_WM_PID` 反查确认安装器窗口、抓窗口位图按按钮主题色 `#3370FF`
定位「立即安装」实心块（不写死坐标，兼容 DPI 缩放），点下去后以安装日志
本轮新增行里的 `_OnInstallSuccess` 为完成判据。

#### 一次完整安装的实际行为（2026-11-07 21:46 日志）

```
21:46:04  _StartInstallThread → GET service/settings/v3/
21:46:04  host: lf-flow-web-cdn.doubao.com, path: obj/flow-doubao/doubao_pc/2.32.6/Doubao_win_X64_2.32.6-signed.zip
          下载到 %LOCALAPPDATA%\Temp\Doubao\2.32.6.zip，共 948,603,770 字节（≈904 MB）
21:46:43  wait uninstall → WaitForExit done（先走旧实例卸载）
21:47:00  Replace uninstaller / Replace launcher → launcher/uninstaller extracted successfully
21:47:00  write version → create desktop icon → Add Install/Uninstall Reg
21:47:00  _OnInstallSuccess → _LaunchAgent（自动拉起客户端）
```

两条 `ERRO` 是无害的：`GetHttpTotalSize::WinHttpQueryHeaders error: 12150`（HEAD 取长度失败，
不影响 GET 下载）和 `UnzipDoctorApp not found Doctor.zip`（包里本来就没带 Doctor.zip）。
临时包自删，`app` 目录 1.8 GB，`豆包.lnk` 重建。整个流程在本机约 60 秒。

新版没有覆盖 `Program Files` 旧版，而是安装到了用户目录：

`~/.deepinwine/doubao-wine/drive_c/users/ubuntu/AppData/Local/Doubao/Application/app`

因此目前机器上同时存在两个客户端：

| 客户端 | 版本 | 当前状态 |
|---|---|---|
| `Program Files/Doubao/app` | 1.56.6 | 早期在 Deepin Wine 8.16 + Mesa 下可显示，**仅作历史对照** |
| `AppData/Local/Doubao/Application/app` | 2.32.6 | 官方最新，**唯一目标版本**；Wine 11 下进程稳定，窗口待修 |

安装器还创建了 `~/Desktop/豆包.lnk`。`run-doubao-wine.sh` 仍指向旧版，
`run-doubao-wine11.sh` 才是新版启动器；桌面入口需切换到后者。

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

### 坑 5：降级不只是替换 app 目录（历史做法，已废弃）

新版升级后再恢复旧版，直接复制旧程序仍可能崩溃或进入“豆包异常检测”。还需要处理：

- `User Data/Last Version`：从 `147.0.7727.149` 恢复为 `129.0.6668.79`。
- 新版生成的 `ShaderCache`、`GrShaderCache`、`GPUCache`、Dawn 和 Chromium 缓存。
- `repair/record.json` 中的 `running_exception`，否则启动器会进入异常检测。
- `update_data` 中已下载的新版本。

不能只删整个 `User Data`，否则会无谓丢失账号和聊天数据。

### 坑 6：修复器会把“禁用 update.exe”当成文件缺失（历史做法，已废弃）

曾将 `update.exe` 改名以避免自动更新，结果：

`User Data/repair/missing_files` 中出现 `update.exe`，客户端进入修复流程。

所以更新器文件本身必须保留。后来测试了保留有效 PE、将入口改成 `xor eax,eax; ret` 的方式，但这仍不是长期可靠的版本锁定方案，见坑 7。

### 坑 7：版本屏蔽配置和 patch 更新器都不能保证永不升级（历史做法，已废弃）

实测以下配置不能阻止 2.32.6 再次安装：

`vulcan_update_block_versions = [ "2.32.6" ]`

而仅 patch `Program Files/Doubao/app/update.exe` 也不够：旧客户端会从下面的目录重新复制原始更新器覆盖它：

`User Data/update_data/Doubao_update/update.exe`

日志明确出现：

`Copy update.exe from ... update_data/Doubao_update ...`

因此旧版方案只能理解为**临时兼容方案**。按现行约定，这些回退/锁版本手段**一律不再使用**，
修复脚本已清理为纯前缀层修复。迁移到能运行新版的 Wine 才是根治方向。

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

### 4.3 实际进展（2026-11-07）

在 `~/.deepinwine/doubao-wine11` 前缀中用 Proton 11.0 启动官方 2.32.6，实测：

| 检查项 | 结果 |
|---|---|
| `RtlGetDeviceFamilyInfoEnum` | **通过**（日志出现 `fixme:ntdll:RtlGetDeviceFamilyInfoEnum ... stub`，不再是 FATAL） |
| 进程存活 | browser / crashpad-handler / gpu-process / 2×renderer / network service / storage service 全部在跑 |
| FATAL / `CHECK failed` | 无 |
| GPU 进程异常退出 | 无（1.56.6 在 Deepin Wine 下会反复 `-2147483645` 退出） |
| 网络 | winsock 探针对 `www.doubao.com` 的 IPv4/IPv6 **全部 `CONNECT OK`** |
| 窗口 | 「豆包」1200x800 与一个无标题 1440x704 窗口均 `IsUnMapped`；强制 map 后内容纯黑 |

结论：**Wine 11 路线已经走通 API 兼容段**，剩下的黑屏属于渲染/窗口映射问题，
不是 Wine API 缺失，也不是网络问题。

排查方法论沉淀（避免重走弯路）：

- **运行时证据优先于静态符号表**。`objdump -T ntdll.so | grep RtlGetDeviceFamilyInfo`
  在 Proton 的 `ntdll.so` 上查不到导出（该 so 只导出 11 个 Rtl 符号），
  但运行时确实存在（stub 形式），只看符号表会得出「Wine 11 也没有该 API」的错误结论。
- **进程类型识别不要用一次性的 `ps | grep -oE`**，容易漏掉 `--utility-sub-type=` 参数。
  逐秒轮询 `/proc/<pid>/cmdline` 才看清 network service 与 storage service 实际存在。
- **抓图用 `xwd -id <窗口ID>`**，Wayland 下 `ffmpeg x11grab` 抓 root 全黑；
  `import`/`scrot`/`grim` 均不存在。未映射窗口 `xwd` 会报 `BadMatch`（X_GetImage 失败）。
- **Wine 网络探针自己编译**：`wine cmd /c curl.exe`、`certutil -urlcache`、`powershell`
  在 Wine 下都是 stub，不可用。用 `i686-w64-mingw32-gcc -lws2_32` 编译 32 位 PE，
  `getaddrinfo` + `connect` 逐地址测试才有效。
- `WSALookupServiceBegin failed with: 8` 在新旧版都出现，是 Chromium 在 Wine 下的正常现象，非根因。

## 5. Wine 11 启动环境（已落地为脚本）

启动器：`doubao/run-doubao-wine11.sh`。其环境变量与 Proton 自带 `proton` 脚本保持一致：

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
```

脚本会自动 `find` 出实际落点的 `Doubao.exe`，不预设 Windows 用户名。

### 5.1 安装 / 升级入口

```bash
./doubao/install-doubao.sh                    # 自动找仓库 doubao/ 与 ~/下载 里最新的安装器
./doubao/install-doubao.sh /路径/安装器.exe   # 显式指定
DOUBAO_WINE11_PREFIX="$HOME/.deepinwine/doubao-wine11" ./doubao/install-doubao.sh
```

脚本做的事：先 curl 预查官方下发版本 → 只杀本前缀的 Wine 进程 → 拉起安装器并
自动点「立即安装」（`/S` 不静默，必须点）→ 等安装日志 `_OnInstallSuccess` →
收尾杀掉安装器自动拉起的黑屏实例 → 跑一次 `fix-doubao-wine.sh`。
依赖 `xdotool` / `xprop` / `python3-xlib` / `Pillow`；缺依赖时会退成“请人工点按钮”，
脚本继续等日志结果。安装器日志落在 `User Data/sdk_storage/log/Doubao-installer.log`，
完成与否以它为准，不看版本号是否变化（同版本重装时版本号不变）。

安装后要先用 `find` 确认实际落点：

```bash
find "$WINEPREFIX/drive_c/users" -path '*/Doubao/Application/app/Doubao.exe' -print
```

首次启动建议带 `--enable-logging=stderr`，并单独记录日志。
`fix-doubao-wine.sh` 已清理为**纯前缀层修复**（WinSxS 链接 + LocalLow 目录），
不再包含回退、锁版本与更新器禁用逻辑，可以安全地在 Wine 11 前缀上跑：

```bash
DOUBAO_WINE_PREFIX="$HOME/.deepinwine/doubao-wine11" ./doubao/fix-doubao-wine.sh
```

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

1. **修黑屏**：定位并解决 Wine 11 下窗口不映射 / 内容纯黑的问题。
2. **确认页面真实内容**：窗口内渲染的必须是豆包真实界面，不是白屏或崩溃页。
3. **逐项验收**：登录、文字对话、文件上传、音频/RTC、重复启动。
4. **切换桌面入口**：新版连续手动启动验证通过后，把 `~/Desktop/doubao-wine.desktop`
   从 `run-doubao-wine.sh`（旧版）改到 `run-doubao-wine11.sh`（新版）。
5. **最后才评估 gf 加速**：Wine API 兼容与 gf 渲染是两个问题，不能混在一起排查。

账号数据不要直接整目录覆盖。日志里已出现 `AppBoundEncryption` 错误，跨 Wine/前缀迁移后应有重新登录的心理准备。

## 8. 验收标准（新版 2.32.6）

新版至少满足以下条件，才算修完：

- 60 秒内出现主窗口；
- `CrBrowserMain` 连续存活 10 分钟以上；
- 没有新的 browser 主进程 Crashpad 转储；
- 登录和文字对话可用；
- 连续进行 3 次手动启动均正常；
- 版本保持官方 2.32.6，不需要禁用更新器或屏蔽版本号才能运行。

若 Wine 11 仍失败：

- 用对应 Proton 的 `wineserver -k` 只关闭 Wine 11 测试前缀；
- 不做崩溃后自动重启，不用 supervisor 掩盖主进程退出；
- 继续按日志定位下一个 Wine API 或渲染问题，
  **不退到旧版客户端**；若要换方向，只能整体转向功能明确的网页版 Electron 外壳。

## 9. 相关文件

- `doubao/install-doubao.sh`：**官方在线安装器的安装/升级入口**；预查下发版本、
  自动点「立即安装」、以 `_OnInstallSuccess` 为完成判据，装完自动跑前缀层修复。
- `doubao/run-doubao-wine11.sh`：**新版 2.32.6 的 Wine 11 启动器（主线）**。
- `doubao/fix-doubao-wine.sh`：与版本无关的前缀层修复（WinSxS 软链接、LocalLow 目录）；
  不再做版本回退、不锁版本、不禁用更新器。
- `doubao/run-doubao-wine.sh`：旧版 1.56.6 的 Mesa 启动器和空 URI 过滤，仅作历史对照。
- `.codebuddy/rules/豆包客户端版本约定.md`：只认官方最新版、不做旧版回退的项目约定。
- `electron/README.md`、`electron/use-electron-gf.sh`：原生 Electron/Chromium 在 7G100 上的现有适配经验。
- Proton 的 `proton` 脚本：官方运行库环境变量来源。
