/* 11 对战平台 GDI 句柄泄漏诊断包装层（native user32，仅 32 位客户端）。
 *
 * Wine 11 的 GDI 句柄表上限 65536；11 平台客户端每次窗口消息循环调用
 * GetDC/GetDCEx 取显示 DC 后不释放，约 10908 轮后关键 GDI 对象创建失败，
 * libcef.dll 在 0x719fc9dd 触发 0x80000003。此包装层以 native user32
 * 身份加载，其余导出由 gen-11-user32-def.py 转发到 user32_orig.dll。
 *
 * 当前版本只做取证：每 500 次 GetDC/ReleaseDC 打印 HDC、返回地址和调用模块，
 * 用于确认泄漏调用点。E11_GDI_RECLAIM=1 时额外回收「上一枚未释放的 DC」，
 * 把每线程的未释放显示 DC 控制在 1 枚以内（只作用于本进程）。
 *
 * 编译：
 *   i686-w64-mingw32-dlltool --def user32.def --dllname user32.dll --output-exp exp.o
 *   i686-w64-mingw32-gcc -shared -O2 -o user32.dll user32-gdi-wrapper.c exp.o -luser32 -lkernel32
 * License: Apache-2.0
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

typedef HDC (WINAPI *get_dc_t)(HWND);
typedef HDC (WINAPI *get_dc_ex_t)(HWND, HRGN, DWORD);
typedef int (WINAPI *release_dc_t)(HWND, HDC);

static HMODULE orig_mod;
static get_dc_t p_get_dc;
static get_dc_ex_t p_get_dc_ex;
static release_dc_t p_release_dc;

static void resolve_orig(void)
{
    if (orig_mod) return;
    orig_mod = LoadLibraryA("user32_orig.dll");
    if (!orig_mod) return;
    p_get_dc = (get_dc_t)GetProcAddress(orig_mod, "GetDC");
    p_get_dc_ex = (get_dc_ex_t)GetProcAddress(orig_mod, "GetDCEx");
    p_release_dc = (release_dc_t)GetProcAddress(orig_mod, "ReleaseDC");
}

static int dbg_on(void)
{
    static int on = -1;
    if (on < 0) { const char *e = getenv("E11_GDI_DEBUG"); on = e && atoi(e); }
    return on;
}

static int reclaim_on(void)
{
    static int on = -1;
    if (on < 0) { const char *e = getenv("E11_GDI_RECLAIM"); on = e && atoi(e); }
    return on;
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    (void)inst; (void)reserved;
    if (reason == DLL_PROCESS_ATTACH && dbg_on())
        fprintf(stderr, "[11-u32wrap] loaded\n");
    return TRUE;
}

static void log_call(const char *kind, HDC dc, void *ret)
{
    static volatile LONG get_n, rel_n;
    LONG n = strcmp(kind, "GetDC") == 0 ?
        InterlockedIncrement(&get_n) : InterlockedIncrement(&rel_n);
    if (!dbg_on() || (n > 5 && n % 500 != 0)) return;
    char mod[MAX_PATH] = "?";
    HMODULE m = NULL;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCSTR)ret, &m) && m)
        GetModuleFileNameA(m, mod, sizeof(mod));
    fprintf(stderr, "[11-u32wrap] %s #%ld dc=%p caller=%p module=%s\n",
            kind, n, dc, ret, mod);
}

/* 每线程只保留最近一枚未释放的显示 DC；下一次 GetDC 前先交还旧 DC。
 * 客户端的泄漏模式是每个窗口消息循环取一枚、函数返回后不再使用。 */
static __thread HDC last_dc;

static HDC track(HWND hwnd, HDC dc, void *ret)
{
    if (!dc) return dc;
    log_call("GetDC", dc, ret);
    if (!reclaim_on()) return dc;
    if (last_dc && last_dc != dc) {
        if (p_release_dc) p_release_dc(hwnd, last_dc);
        else ReleaseDC(hwnd, last_dc);
    }
    last_dc = dc;
    return dc;
}

HDC WINAPI GetDC(HWND hwnd)
{
    resolve_orig();
    HDC dc = p_get_dc ? p_get_dc(hwnd) : NULL;
    return track(hwnd, dc, __builtin_return_address(0));
}

HDC WINAPI GetDCEx(HWND hwnd, HRGN hrgn, DWORD flags)
{
    resolve_orig();
    HDC dc = p_get_dc_ex ? p_get_dc_ex(hwnd, hrgn, flags) : NULL;
    return track(hwnd, dc, __builtin_return_address(0));
}

int WINAPI ReleaseDC(HWND hwnd, HDC dc)
{
    resolve_orig();
    if (last_dc == dc) last_dc = NULL;
    log_call("ReleaseDC", dc, __builtin_return_address(0));
    return p_release_dc ? p_release_dc(hwnd, dc) : 0;
}
