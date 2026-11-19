/* 80 对战平台诊断用 user32 包装层：记录分层窗口 / 区域 API 的调用参数。
 *
 * 目的（2026-11-19）：定位「X11 有主窗口但合成画面不可见」的根因。
 * 80 是 Qt5.15.2 + CEF109 多进程应用；strings 显示导入
 * SetLayeredWindowAttributes，但 wine +win trace 里没有实际调用记录。
 * 本 dll 以 native user32 身份加载（WINEDLLOVERRIDES=user32=n,b，仅 80
 * 进程树），其余导出 PE forwarder 转回 user32_orig.dll；只自实现
 * SetLayeredWindowAttributes / UpdateLayeredWindow / SetWindowRgn，
 * 把参数写到 E80_WRAP_LOG 指定的日志。
 *
 * 编译/部署方式见 80/README.md §8.4。这是诊断工具，不是修复，
 * 不作为默认启动路径。License: Apache-2.0
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

typedef BOOL (WINAPI *SLWA_t)(HWND, COLORREF, BYTE, DWORD);
typedef BOOL (WINAPI *ULW_t)(HWND, HDC, POINT *, SIZE *, HDC, POINT *, COLORREF,
                             BLENDFUNCTION *, DWORD);
typedef int  (WINAPI *SWR_t)(HWND, HRGN, BOOL);

static HMODULE orig_mod;
static SLWA_t orig_slwa;
static ULW_t  orig_ulw;
static SWR_t  orig_swr;

static FILE *wraplogf(void)
{
    static FILE *f;
    static int tried;
    if (tried) return f;
    tried = 1;
    const char *p = getenv("E80_WRAP_LOG");
    if (p) f = fopen(p, "a");
    if (!f) f = stderr;
    return f;
}

#define LOG(...) do { FILE *f = wraplogf(); if (f) { fprintf(f, "[e80-wrap] " __VA_ARGS__); fflush(f); } } while (0)

static void resolve(void)
{
    if (orig_mod) return;
    orig_mod = LoadLibraryA("user32_orig.dll");
    if (!orig_mod) { LOG("LoadLibrary(user32_orig.dll) FAILED\n"); return; }
    orig_slwa = (SLWA_t)GetProcAddress(orig_mod, "SetLayeredWindowAttributes");
    orig_ulw  = (ULW_t)GetProcAddress(orig_mod, "UpdateLayeredWindow");
    orig_swr  = (SWR_t)GetProcAddress(orig_mod, "SetWindowRgn");
}

BOOL WINAPI SetLayeredWindowAttributes(HWND h, COLORREF key, BYTE alpha, DWORD flags)
{
    resolve();
    LOG("SLWA hwnd=%p key=0x%06x alpha=%u flags=0x%x%s%s\n",
        (void*)h, key, (unsigned)alpha, flags,
        (flags & 0x2) ? " LWA_ALPHA" : "", (flags & 0x1) ? " LWA_COLORKEY" : "");
    return orig_slwa ? orig_slwa(h, key, alpha, flags) : FALSE;
}

BOOL WINAPI UpdateLayeredWindow(HWND h, HDC dc, POINT *dst, SIZE *size,
                                HDC src, POINT *srcpt, COLORREF key,
                                BLENDFUNCTION *bf, DWORD flags)
{
    resolve();
    LOG("ULW hwnd=%p dst=%s size=%s alpha=%u flags=0x%x\n", (void*)h,
        dst ? "y" : "n", size ? "y" : "n",
        bf ? (unsigned)bf->SourceConstantAlpha : 255u, flags);
    return orig_ulw ? orig_ulw(h, dc, dst, size, src, srcpt, key, bf, flags) : FALSE;
}

int WINAPI SetWindowRgn(HWND h, HRGN rgn, BOOL redraw)
{
    resolve();
    LOG("SWR hwnd=%p rgn=%p redraw=%d\n", (void*)h, (void*)rgn, (int)redraw);
    return orig_swr ? orig_swr(h, rgn, redraw) : 0;
}
