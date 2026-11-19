# 11 变更记录

> 从仓库根 `CHANGELOG.md` 拆分而来（2026-11-19）。按时间倒序；历史条目保留当时的命令与路径。

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
