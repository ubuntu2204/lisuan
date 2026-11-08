/* 豆包托盘诊断探针：广播 TaskbarCreated，逼客户端重新注册托盘图标。
 *
 * 背景（详见 doubao/README.md 2.6.2）：
 *   豆包的托盘菜单只在启动阶段构建一次，失败时打
 *   「[Tray] BuildMainStatusIconContextMenu main_status_icon_ is null」，
 *   之后整个会话都不再重建，右键就是没反应。Windows 的约定是 shell 重建时
 *   向所有顶层窗口广播注册消息 "TaskbarCreated"，应用收到后重新 Shell_NotifyIcon。
 *   本探针就是手工触发这一次广播，用来看应用会不会重新注册、以及重建后菜单能否用。
 *
 * 实测结论（2026-11-08）：应用确实响应（NIM_DELETE + NIM_ADD，Wine 侧
 *   add_icon + X11DRV_SystrayDockInsert，且不再报 null），但菜单只有第一次
 *   重建后弹出过一次，之后仍失效 —— 只能作诊断，不是可靠修复。
 *
 * 编译（Wine 11 前缀是 win64 + new WoW64，32 位 PE 可以跑）：
 *   i686-w64-mingw32-gcc -O2 -o probe-trayrefresh.exe probe-trayrefresh.c -luser32
 *
 * 运行（必须与客户端同一前缀，且套容器保持环境一致）：
 *   RT=~/.local/share/Steam/steamapps/common/SteamLinuxRuntime_sniper/run-in-sniper
 *   PR="$HOME/.local/share/Steam/steamapps/common/Proton 11.0"
 *   "$RT" -- env WINEPREFIX="$HOME/.deepinwine/doubao-wine11" \
 *       WINEDLLPATH="$PR/files/lib/vkd3d:$PR/files/lib/wine" \
 *       LD_LIBRARY_PATH="$PR/files/lib/x86_64-linux-gnu:$PR/files/lib/i386-linux-gnu" \
 *       "$PR/files/bin/wine" ./probe-trayrefresh.exe
 */
#include <windows.h>
#include <stdio.h>

int main(void)
{
    UINT msg = RegisterWindowMessageA("TaskbarCreated");
    if (!msg) {
        printf("RegisterWindowMessage failed: %lu\n", (unsigned long)GetLastError());
        return 1;
    }

    printf("TaskbarCreated=0x%x, broadcasting to top-level windows...\n", msg);

    DWORD_PTR result = 0;
    LRESULT ok = SendMessageTimeoutA(HWND_BROADCAST, msg, 0, 0,
                                     SMTO_ABORTIFHUNG | SMTO_NORMAL,
                                     5000, &result);
    /* ERR_TIMEOUT(1460) 属正常：豆包是 Chromium，UI 线程可能不回消息循环 */
    printf("SendMessageTimeout=%ld err=%lu\n", (long)ok, (unsigned long)GetLastError());
    return 0;
}
