# 11 变更记录

> 从仓库根 `CHANGELOG.md` 拆分而来（2026-11-19）。按时间倒序；历史条目保留当时的命令与路径。

## 2026.11.20 — 11 对战平台：还原发生在进房间时，gp_* 目录加只读锁

- 19:25 按「又拉起失败」重新取证：最新会话仍是 `2026-11-20 19_18_07.537067`，
  19:22 的 War3Shell 字符串补丁之后没有新的拉起日志（平台窗口和 wineserver
  从 19:18 一直存活，无 War3 进程）。该会话证据：19:18:30 进房间 171，
  19:18:31 `gp_1292` 的 `assocket.dll` / `vpndll.dll` / `xp.dll` 及
  `DxRender.dll` / `DxWorker.dll` 被平台重新落盘（ctime 19:18:31、mtime 保持
  11-04）；19:18:37 `Main_...txt` 显示 War3Shell 从 `gp_1292` 加载
  DxRender/DxWorker 成功、`m_fnDxWorkerInitialit success`，随后停在
  `war3shell prepare finish`，只有每分钟一次的 CodeStatist，无窗口——与
  11-19 定的「渲染替换层挂死」完全同型。
- 结论：改名 + War3Shell 字符串补丁都挡不住「进房间时按缺失文件重新部署」。
  `11/fix-11-render.sh` 在补丁完成后把每个 `gp_*` 目录置为 `555`，原权限记在
  `.e11-render-dirmode`（`--uninstall` 按记录还原），阻止平台再把渲染 DLL
  写回；`--status` 增加「目录=只读/可写」检查，可写即报失败。新版本 `gp_*`
  由启动器下次启动时自动补丁并置只读。只影响本机前缀内这几个补丁目录，
  不动 `W3L/core`，无守护/看门狗/自动重启。
- 离线往返验证：临时前缀 apply→status→uninstall 全通过，权限记录 775→555→775。
  实际 6 个 `gp_*` 已置 555，状态全绿。**尚未由用户重新进房间验收**，
  若平台因目录只读弹错，应如实记录并 `--uninstall` 回退，不包装成已修好。

## 2026.11.20 — 11 对战平台：平台还原 DLL 导致二次复发，改补丁 War3Shell 名称

- 19:18 用户在已禁用全部 gp_* 的 DxRender/DxWorker 后再次拉起失败。
  取证：`gp_1292/DxRender.dll` 与 `DxWorker.dll` 在 19:18:30 被重新创建
  （ctime 当天、mtime 仍为 11-04、md5 与 `.e11-disabled` 相同），
  `Main_...txt` 显示 `LoadLib ...\gp_1292\DxRender.dll` / `DxWorker.dll`
  成功、`window-check` 又一次 `not-found`。说明平台进房间时会按缺失文件
  重新部署 DLL，单纯改名不可靠。
- 修复：`11/fix-11-render.sh` 除改名外，还把每个 gp_* 下
  `War3Shell.dll` 本机副本中的 UTF-16/ASCII 名称 `DxRender.dll` /
  `DxWorker.dll` 改为同长度不存在的 `DxRendex.dll` / `DxWorkex.dll`；
  平台即使还原 DLL 本体也 LoadLibrary 失败，游戏回退原生 D3D8→DXVK(lvp)。
  首次修改前备份 `War3Shell.dll.e11-render-orig`，`--uninstall` 还原全部。
  `--status` 以 shell 补丁为准报告，不再被 DLL 还原误判。
- 当前 6 个 gp_* 目录均已改名并补丁；启动器每次启动自动调用。无守护/自动重启。

## 2026.11.20 — 11 对战平台：War3 拉起修复固化为覆盖所有 gp_* 补丁目录

- 现象：11-19 验收通过后，11-20 19:04 再次从平台拉起 War3 失败。
  取证：Assocket 日志 `war3shell path=...\W3L\Patch\War3\gp_1292`、
  `injectedDll=...\gp_1292\assocket.dll`，同目录 `DxWorker.dll` 日志有
  `dxrender init`；`window-check` 10 秒 `not-found`。11-19 的修复只改了
  `gp_1290`，平台按地图/版本切换到 `gp_1292` 后渲染替换层重新生效。
- 修复：`11/fix-11-render.sh` 改为遍历 `W3L/Patch/War3/gp_*` 下**全部**
  DxRender/DxWorker 改名禁用（`--status` 逐目录显示、`--uninstall` 全量还原），
  本次新禁用 10 个 DLL（gp_1252/1289/1291/1292/1293）。
  `11/run-11duizhan.sh` 每次启动前自动调用该脚本（`E11_RENDER_LAYER=keep`
  可关做 A/B），新下载的 gp_* 版本也能覆盖，不再依赖一次性手工操作。
- 性质仍是**绕过**平台渲染替换层（War3Shell/assocket/IPCProxyEx 保留、联机 IPC
  保留），不是修好它；联机对战细节仍未逐项验收。无看门狗/自动重启。

## 2026.11.19 — 11 对战平台：War3 拉起验收通过（禁用 DxRender/DxWorker 渲染替换层）

- 验收：禁用 `gp_1290/DxRender.dll`/`DxWorker.dll` 后，平台 20:47:17 拉起
  `C:\war3\YY\war3.exe`（win32_error=0），出窗进入游戏，游玩约 18 分钟后
  21:05:08 正常 gameexit，无崩溃 dump；用户确认「可以从 11 平台正常进魔兽争霸3」。
- 根因链（本日取证）：① 注册表 `InstallPath` 缺失 → `fix-11-war3path.sh` 写入；
  ② 游戏目录 root 不可写、平台 `YY\` 补丁部署失败 → 复制到前缀 `drive_c/war3`
  指 `C:\war3\`；③ 注入的渲染替换层 DxRender/DxWorker 在 Wine 下挂死游戏
  （存活、IPC 握手成功、无窗、主线程 futex 等待）→ `fix-11-render.sh` 改名禁用，
  游戏回退原生 D3D8→DXVK(lvp)。对照：YY exe+补丁 Game.dll 不注入 shell 单独启动
  正常出窗；原装 War3 独立启动器正常。
- 证伪记录：导入独立前缀的 Video/Misc 键值（含 seenintromovie=1）单独不能解决挂死；
  `FollowMapRenderModeV2=false` 后 `GAME_PARAM_USE_VULKAN` 日志仍打印（该日志无条件
  打印，不反映 flag 值），均非根因。
- 性质：绕过平台渲染替换层，不是修好它；War3Shell/assocket/IPCProxyEx 仍注入，
  联机 IPC 保留。联机对战逐项细节未验收。`fix-11-render.sh --uninstall` 可还原。
- 配套：`run-11duizhan.sh` 默认注入 `VK_ICD_FILENAMES=lvp_icd.json`（`E11_VK_ICD`
  覆盖 / `=skip` 关），32 位 War3 不再继承 gf 64 位-only ICD。详见 `11/README.md` §10。

## 2026.11.19 — 11 对战平台：War3 拉不起根因（游戏目录不可写）与修复

- 现象：注册表 `InstallPath` 已指 `Z:\opt\apps\war3\files\` 后平台仍拉不起 War3。
  取证：平台自存 `YiYi/11client/game.ini` 的 `War3ExePath` 已从注册表生成，但
  `PureWar3ExePath=<游戏目录>\YY\war3.exe` 指向的 `YY\` 子目录不存在——平台拉起前
  要在游戏目录部署补丁版 war3.exe 与注入件，而 `/opt/apps/war3/files` 属主 root、
  本机无 sudo，部署永远失败。只改注册表不够。
- 修复：游戏复制到前缀内 `drive_c/war3`（用户可写）；`fix-11-war3path.sh` 增加
  drive_c → `C:` 盘映射，注册表改指 `C:\war3\`；平台退出时同步 `game.ini`
  （备份 `.bak-zpath`）。重启平台后 `YY\` 立即部署成功（补丁版 War3.exe + MPQ）。
- 渲染配套：`run-11duizhan.sh` 默认注入 `VK_ICD_FILENAMES=lvp_icd.json`
  （`E11_VK_ICD` 覆盖 / `=skip` 关），平台拉起的 32 位 War3 不再继承 gf 64 位-only
  ICD 环境；根因同 `war3/run-war3.sh`（2026-11-18 实测）。
- 验收状态：`YY\` 部署成功已确认；进房间点「启动游戏」实际拉起 War3 待用户点击验收，
  不写成「联机可用」。详见 `11/README.md` §10。

## 2026.11.19 — 11 对战平台：设置魔兽争霸 III 路径

- 机制取证：`bins/11Client.exe` 与 `W3L/core/War3Lobby.exe` 的 PE 字符串均含
  `Software\Blizzard Entertainment\Warcraft III` + `InstallPath`（另有
  `AutoSearchWar3Enabled` / `IPC2HFPlat::saveWar3PathToClient`），即平台走官方
  暴雪注册表键定位 War3；11 前缀默认无该键。
- 新增 `11/fix-11-war3path.sh`：把本机 War3 目录 `/opt/apps/war3/files`（Wine 路径
  `Z:\opt\apps\war3\files\`）写入 11 前缀的 `InstallPath` / `InstallPathX`，
  HKCU 与 HKLM 32 位视图（Wow6432Node）都写；幂等、备份原值、`--status` /
  `--uninstall` 齐全。平台客户端占用前缀时拒写；仅剩 CEF 渲染残留时取证后
  可 `E11_FORCE=1`。只改注册表，不动厂商二进制。
- 验证：四个值已写入并落盘（`user.reg` / `system.reg`）；幂等重跑不重写备份；
  卸载→体检报缺失→再写入的往返通过。平台内识别 War3 / 联机尚未验收。
- 遗留提醒：平台拉起 War3 不带 `war3/run-war3.sh` 的 32 位 llvmpipe ICD，
  渲染崩溃风险见 `war3/README.md`；`Save/` `Replay/` 不可写时先跑
  `./war3/fix-war3-writable.sh`。详见 `11/README.md` §10。

## 2026.11.19 — 11 对战平台：正式版登录后卡死已修复（Wine 系统字体）

- 最终根因：前缀 `NONCLIENTMETRICS` 系统字体为 Tahoma（本机未安装）。下载 CEF
  `105.3.38+g2a6a54b` 官方 release PDB 后，用 `llvm-pdbutil` 把崩溃环映射到源码：
  `gfx::win::GetDefaultSystemFont / GetSystemFont / AdjustFontSize`、
  `PlatformFontSkia::EnsuresDefaultFontIsInitialized / InitFromDetails`、
  `PlatformFont::CreateFromNameAndSize`，断言点为 `base::debug::CollectGDIUsageAndDie`。
  Chromium 105 源码显示 `SystemFonts::Initialize()` 在 `AddFont()` 全部完成后才置
  `is_initialized_=true`；期间 Skia 用 Tahoma 创建字体失败会回调
  `GetDefaultSystemFont()`，看到“未初始化”后再次 `Initialize()`，无限重入，
  每轮取 DC/建字体，先耗尽 GDI 表再触发断言/栈溢出。
- 修复：新增 `11/fix-11-fonts.sh` + `11/system-font-helper.c`，用
  `SPI_SETNONCLIENTMETRICS` 把 caption / smcaption / menu / message / status 五类系统字体
  改为本机存在的 Arial（可用 `E11_SYSTEM_FONT` 覆盖）；脚本幂等，先 `reg export`
  备份 `HKCU\Control Panel\Desktop\WindowMetrics`，`--status` 只读体检，
  `--uninstall` 用 `.reg` 备份还原。只改当前 WINEPREFIX 注册表，不动厂商二进制。
- 验证：登录后主窗口从登录框变为 `11对战平台 - 2.6.3.48`（`1243×720`）；
  资源更新、服务器测速完成并可确认；大厅完整渲染；Wine 日志
  `out of GDI object handles` 0、`stack overflow` 0、`0x80000003` 0，
  进程稳定存活。证据 `11/_work/font-fix-arial-20261119-1925/`（`.gitignore` 排除 `_work/`）。
- 官方 PDB 用于取证，保存在 `11/_work/cef-official/`（约 4 GB，未入库）；本机
  `libcef.dll` 与官方同版本所有 PE 节区 SHA1 完全一致，差异仅在文件尾 overlay，
  故 PDB 地址映射有效。未下载/构建 Chromium 全量源码。

## 2026.11.19 — 11 对战平台：正式版登录后卡死取证（未修复）

- 复现：正式版 `2.6.3.48` 启动可见，但点击登录后窗口无响应。带 `E11_WINEDEBUG='+err+seh+process+thread'` 单次取证，Wine stderr 出现 `stack overflow ... addr 0x4836d2`，栈范围 `0xdc0000-0x11b0000`；应用日志停在 `CLoginLogic::onLoginSucceed` / `DoWebPageLogin: 1`，`11Client.exe` 主线程退出，窗口 PID 变僵尸 CEF 进程，三个 CEF 子进程阻塞在 `anon_pipe_read`。
- 栈证据：`bins/11Client.exe` 的函数 `0x4836b0` 每帧 `sub $0xfc4,%esp`；进一步反汇编确认它是注册到异常处理链的崩溃回调（内部加载 `dbghelp`），不是普通业务递归。把 PE 栈从 4MB 加到 16MB/64MB 只能推迟溢出；进程存活更久后仍在 64MB 栈上溢出。临时把回调注册改成 NULL 后，暴露的是 Wine 内部 `Unhandled stack overflow at address 7BC59450`，仍未修复。栈与回调补丁均已还原。
- GDI 根因：新增 `11/redirect-11-gdi-iat.sh` 把 `11Client.exe` 与 `bins/cef/libcef.dll` 的 `USER32.dll` 导入名等长替换为 `e11gdi.dll`，让 `user32-gdi-wrapper.c` 进入进程。`11Client.exe` 仅 1 次 `GetDC` 且成对释放；`libcef.dll` 的 `GetDC(NULL)` 固定来自 `+0x300060a`，约 2.1 万次，`ReleaseDC` 固定来自 `+0x30087ed`，约 1.05 万次，HDC 不配对 → Wine GDI 句柄表耗尽、`libcef.dll` `0x80000003`。
- minidump 最终证据：`winedbg <.../11client_*.dmp>` 重载显示崩溃调用栈是 `libcef.dll` 内固定回调环反复压栈（`+0x30096f8 → +0x3bbb771 → +0x3bbbc70 → +0x3bbbb48 → +0x3bbcd80 → +0x3009c90 → +0x3009dbe → +0x300981c → +0x30096f8`），端点为断言 `+0x3adc9dd` 的 `int3`。加大栈、缓存 DC 只能延后崩溃；需在 CEF/Wine 交互层打断重入。
- 定点二进制 A/B 已证伪：对 `libcef.dll` 重入环 thunk `+0x30096f8`（文件偏移 `0x3008af3`）分别试过 `xor eax,eax`、`mov eax,1`、`mov eax,edi`，以及把下游解引用调用 `+0x3bbb773`（文件偏移 `0x3bbab73`）整体 NOP。五组中 GDI/栈溢出均降为 0，但登录仍停在 `onLoginSucceed` 后，并分别转为 `0xc0000005` 空指针/非法地址写入或转移到新的 CEF 访问违规；说明该 thunk 是对象构造链的一环，不能单点跳过。`libcef.dll` 已按 `11/_work/cef-loop-ab/libcef.dll.orig` 还原，SHA256 `beb7a9e0f1f3e525e949aa83c14938600caf510e6d825b1fa6712512b3a73eed`，两处补丁偏移均恢复原字节。
- 失败方向：Wine 9、Wine 11、`QT_ACCESSIBILITY=0`、`XLIB_SKIP_ARGB_VISUALS=0` 均不能消除 GDI 耗尽。包装层的「回收上一枚 DC」和「屏幕 DC 缓存」都能把 `out of GDI object handles` 降为 0，但登录仍停在“正在验证账号…”并最终栈溢出，**不是可用修复**。当前所有厂商二进制已还原；不加看门狗/自动重启。证据在 `11/_work/incident-20261119-1713/`、`11/_work/verify-20261119-1745/`、`11/_work/gdi-ab-*`、`11/_work/gdi-iat-*`、`11/_work/no-crashhandler-*`（`.gitignore` 已排除 `_work/`）。

## 2026.11.19 — 11 对战平台：改用官网正式版 2.6.3.48，启动问题定案

- 网上成功案例指向官网**正式版 2.6.3.x**；本机此前装的 `3.0.0.68` 是公测版，两者进程结构完全不同：正式版为 `11Loader.exe` + `bins/11Client.exe`，公测版为 `11loader.exe` + `yybins/YYClient.exe`。
- 下载 `5211Setup_2.6.3.48.exe`（`go.5211game.com/?id=10`，需 Referer，SHA256 `20066c97cfcf25b80cbf7b48d89f2d1508a11783978a3a201bd6fabff46a23b8`），在独立前缀 `~/.deepinwine/11duizhan-263` 静默安装成功。
- `install-11duizhan.sh` / `run-11duizhan.sh` / `fix-11-desktop.sh` 默认切到新前缀，并同时兼容 `11Loader.exe` 与 `11loader.exe` 的大小写差异；桌面入口已重新生成。
- 单次验收：窗口在 t=10s / 25s 均可见；`stack overflow` 0、`out of GDI object handles` 0、`Unhandled exception` 0，GDI 句柄峰值 142。正式版不需要 3.0 的栈补丁、GDI 垫片或 winetricks。
- 对照实验证伪：纯 `WINEARCH=win32` 前缀、winetricks 装 `mfc42/msxml3/vcrun2019` 都不能消除 3.0 公测版 GDI 泄漏；关键差异是正式版客户端本身。3.0 公测版的全部排障记录保留在 `11/README.md` §1–§9 与 `.codebuddy/rules/11平台启动排障约定.md` 作为历史。
- 完整测试套件 14/14 通过；登录/进房间/War3 联机仍未验收，不写成「平台完全可用」。

## 2026.11.19 — 11 对战平台：自更新冲掉栈补丁、GDI 句柄泄漏仍未修复

- 定位并修复「修完又失效」：平台自更新在 `3.0.0.68` ↔ `3.0.0.79` 间来回切换，
  替换 `11loader.exe` / `YYClient.exe` 后把 PE 栈补丁打回 1MB。3.0.0.79 的栈溢出仍在
  （`YYClient.exe+0x48e441`），不是新版本修好了。`11/run-11duizhan.sh` 无参数默认追加
  `-updatemode=0` 冻结版本（`E11_ALLOW_UPDATE=1` 可恢复 A/B）；这是绕过更新器，不是修好。
- `11/fix-11-stack.sh` 改为按「忽略两个栈 DWORD 的 SHA256」识别版本：exe 变化时旧备份
  转存为 `.orig.<hash8>` 再备份当前版本，避免 `--uninstall` 错还原旧版本；
  `tests/test-11-stack.sh` 增加自更新回归，完整测试套件 14/14 通过。
- GDI 泄漏复现并固化证据：thread 0144 每轮泄漏 1 显示 DC + 1 FONT + 4 REGION，
  分配/释放 REGION 138927/95302、FONT 48169/37248、DC 10910/2；句柄峰值 65503/65536，
  `out of GDI object handles` 5 次，随后 `libcef.dll` `0x80000003`（EIP `0x719fc9dd`）。
  **冻结版本后仍复现**，不得把「无栈溢出」写成「平台可用」。
- 已证伪并记录：归错进程（真凶是 YYClient）、只改 11loader 栈、CEF/QT/虚拟桌面等表面
  开关、把 GDI 配额降到 10000（更早崩）、`LD_PRELOAD` 拦截 `NtUserGetDC`（0 命中）、
  `user32` wrapper + `WINEDLLOVERRIDES`（known DLL 使客户端仍加载内置 user32）、
  gdb 跨 `__wine_syscall_dispatcher` 的栈帧。`11/fix-11-gdi.sh` 等目前只是诊断件。
- 新增 `.codebuddy/rules/11平台启动排障约定.md`，并在技能排障参考、`11/README.md` §9
  记录完整失败方向与下一步（IAT 重定向 `USER32.dll`→等长 `e11gdi.dll`）。禁止看门狗/自动重启。
