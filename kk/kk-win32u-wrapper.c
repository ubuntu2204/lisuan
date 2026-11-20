/* KK 官方对战平台 win32u 包装层：修正 CEF web 子窗的父客户区坐标。
 *
 * 根因（2026-11-20 取证 + Wine 11.0 dlls/win32u/window.c 源码复核）：
 * KK 把 1158x884 的 CEF web 子窗 SetParent 到主框架后，按 Windows 语义传
 * 父客户区坐标 (172,59)。Wine 的 NtUserSetParent 取 COORDS_PARENT 矩形时不
 * 平移父客户区原点，结果 web 窗被放到屏幕 (172,59)，而正确位置是
 * (172+294, 59+33) = (466,92)。视觉侧栏在主框架客户面有原生镜像，点镜像
 * 被内容层 HWND 吃掉；点右下红框才落到 CEF 子窗，正是这个原点偏移。
 *
 * user32 包装层截不到：KK 对 web 窗只用 SWP_NOMOVE，位置由 win32u 的
 * NtUserSetParent 系统调用决定。本文件是一次**已证伪的实验**：2026-11-20
 * 实测 Wine 的 user32 经内部 syscall dispatcher 调 win32u，不走 PE 导出，
 * 本 hook 从未命中；覆盖 win32u.dll 还会让主框架掉到 (0,0)、内容层消失。
 * 因此 run-kk-platform-hw.sh 的 KK_WIN32U_WRAPPER 默认 0，此源码仅作 A/B
 * 记录，不是修复。其余导出全部 PE forwarder 转回 win32u_orig.dll。
 *
 * 只修识别到的 web 窗（默认 1158x884 且落在 (172,59)），一次性平移
 * KK_WEB_SHIFT_X/Y（默认 294/33）；其它窗口与进程完全透传。
 * KK_WIN32U_WRAPPER=0 可在启动器关闭；KK_WIN32U_DEBUG=1 打印决策。
 * License: Apache-2.0
 */
#include <windows.h>
#include <stdio.h>

typedef HWND (WINAPI *set_parent_t)(HWND, HWND);
typedef BOOL (WINAPI *set_window_pos_t)(HWND, HWND, INT, INT, INT, INT, UINT);
typedef BOOL (WINAPI *get_window_rect_t)(HWND, RECT *);

static HMODULE win32u_mod;
static HMODULE user32_mod;
static set_parent_t orig_set_parent;
static set_window_pos_t orig_set_window_pos;
static get_window_rect_t user32_get_rect;
static int web_shifted;

static int dbg(void)
{
    static int on = -1;
    if (on < 0) { const char *e = getenv("KK_WIN32U_DEBUG"); on = e && atoi(e); }
    return on;
}
#define WLOG(...) do { if (dbg()) fprintf(stderr, "[kk-win32u] " __VA_ARGS__); } while (0)

static void resolve(void)
{
    if (win32u_mod) return;
    win32u_mod = LoadLibraryA("win32u_orig.dll");
    user32_mod = LoadLibraryA("user32.dll");
    if (win32u_mod) {
        orig_set_parent = (set_parent_t)GetProcAddress(win32u_mod, "NtUserSetParent");
        orig_set_window_pos = (set_window_pos_t)GetProcAddress(win32u_mod, "NtUserSetWindowPos");
    }
    if (user32_mod)
        user32_get_rect = (get_window_rect_t)GetProcAddress(user32_mod, "GetWindowRect");
    WLOG("orig setparent=%p setpos=%p getrect=%p\n",
         (void *)orig_set_parent, (void *)orig_set_window_pos, (void *)user32_get_rect);
}

static int env_int(const char *name, int def)
{
    const char *e = getenv(name);
    return e ? atoi(e) : def;
}

static int is_web_window(const RECT *r)
{
    int w = r->right - r->left, h = r->bottom - r->top;
    return (w == 1158 && h == 884) || (w == 1156 && h == 882);
}

HWND WINAPI NtUserSetParent(HWND hwnd, HWND parent)
{
    HWND ret;
    RECT r;
    resolve();
    if (!orig_set_parent) return NULL;
    ret = orig_set_parent(hwnd, parent);
    if (ret && !web_shifted && user32_get_rect && user32_get_rect(hwnd, &r) &&
        is_web_window(&r) && r.left == 172 && r.top == 59) {
        int dx = env_int("KK_WEB_SHIFT_X", 294);
        int dy = env_int("KK_WEB_SHIFT_Y", 33);
        WLOG("web hwnd=%p (%ld,%ld) %ldx%ld -> shift (%d,%d)\n", hwnd,
             r.left, r.top, r.right - r.left, r.bottom - r.top, dx, dy);
        if (orig_set_window_pos)
            orig_set_window_pos(hwnd, NULL, r.left + dx, r.top + dy, 0, 0,
                                SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        web_shifted = 1;
    }
    return ret;
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    (void)inst; (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) DisableThreadLibraryCalls(inst);
    return TRUE;
}
