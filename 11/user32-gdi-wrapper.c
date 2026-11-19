/* 11 对战平台 GDI 句柄泄漏诊断包装层（native user32，仅 32 位客户端）。
 *
 * Wine 11 的 GDI 句柄表上限 65536；11 平台客户端每次窗口消息循环调用
 * GetDC/GetDCEx 取显示 DC 后不释放，最终关键 GDI 对象创建失败，libcef.dll
 * 抛 0x80000003。正式版由 11/redirect-11-gdi-iat.sh 把目标 PE 的 USER32.dll
 * 导入名等长替换为 e11gdi.dll，绕过 Wine known DLL 让包装层真正进入客户端；
 * 其余导出由 gen-11-user32-def.py 转发到 user32_orig.dll。
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
    if (on < 0) {
        const char *e = getenv("E11_GDI_DEBUG");
        on = e ? atoi(e) : 1;  /* 默认打开；11Loader 可能不向客户端传递环境变量 */
    }
    return on;
}

static int reclaim_on(void)
{
    static int on = -1;
    if (on < 0) { const char *e = getenv("E11_GDI_RECLAIM"); on = e && atoi(e); }
    return on;
}

/* 模拟 Windows 的显示 DC 缓存：同一 HWND 的 GetDC 返回同一 HDC 并计引用，
 * 只有最后一次 ReleaseDC 才真正交还。Wine 11 对每次 GetDC 都新建 DC，
 * CEF 的 ReleaseDC 又传入旧 HDC，导致净泄漏约一半。 */
typedef struct { HWND hwnd; HDC hdc; LONG refs; } dc_cache_t;
#define DC_CACHE_SLOTS 128
static dc_cache_t dc_cache[DC_CACHE_SLOTS];
static CRITICAL_SECTION dc_cache_cs;

static int cache_on(void)
{
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("E11_GDI_CACHE");
        on = e ? atoi(e) : 1;  /* 默认开启；子进程可能继承不到环境变量 */
    }
    return on;
}

static HDC cached_get_dc(HWND hwnd, int *from_cache)
{
    int i;
    HDC dc;
    HWND key = hwnd ? hwnd : (HWND)1;  /* NULL = 屏幕 DC，用哨兵键缓存 */
    *from_cache = 0;
    if (!cache_on()) return NULL;
    EnterCriticalSection(&dc_cache_cs);
    for (i = 0; i < DC_CACHE_SLOTS; i++) {
        if (dc_cache[i].hwnd == key && dc_cache[i].hdc) {
            InterlockedIncrement(&dc_cache[i].refs);
            dc = dc_cache[i].hdc;
            LeaveCriticalSection(&dc_cache_cs);
            *from_cache = 1;
            return dc;
        }
    }
    LeaveCriticalSection(&dc_cache_cs);
    return NULL;
}

static HDC cache_store_dc(HWND hwnd, HDC dc)
{
    int i, free_slot = -1;
    HWND key = hwnd ? hwnd : (HWND)1;
    if (!cache_on() || !dc) return dc;
    EnterCriticalSection(&dc_cache_cs);
    for (i = 0; i < DC_CACHE_SLOTS; i++) {
        if (dc_cache[i].hwnd == key && dc_cache[i].hdc) {
            HDC old = dc_cache[i].hdc;
            dc_cache[i].hdc = dc;
            dc_cache[i].refs = 1;
            LeaveCriticalSection(&dc_cache_cs);
            if (old && old != dc) {
                if (p_release_dc) p_release_dc(hwnd, old);
                else ReleaseDC(hwnd, old);
            }
            return dc;
        }
        if (free_slot < 0 && !dc_cache[i].hdc) free_slot = i;
    }
    if (free_slot >= 0) {
        dc_cache[free_slot].hwnd = key;
        dc_cache[free_slot].hdc = dc;
        dc_cache[free_slot].refs = 1;
    }
    LeaveCriticalSection(&dc_cache_cs);
    return dc;
}

static int cached_release_dc(HWND hwnd, HDC dc, int *handled)
{
    int i;
    HWND key = hwnd ? hwnd : (HWND)1;
    *handled = 0;
    if (!cache_on()) return 0;
    EnterCriticalSection(&dc_cache_cs);
    for (i = 0; i < DC_CACHE_SLOTS; i++) {
        if (dc_cache[i].hwnd == key && dc_cache[i].hdc == dc && dc) {
            if (dc_cache[i].refs > 1) {
                InterlockedDecrement(&dc_cache[i].refs);
                LeaveCriticalSection(&dc_cache_cs);
                *handled = 1;
                return 1;
            }
            dc_cache[i].hdc = NULL;
            dc_cache[i].hwnd = NULL;
            dc_cache[i].refs = 0;
            LeaveCriticalSection(&dc_cache_cs);
            *handled = 1;
            return 0;
        }
    }
    LeaveCriticalSection(&dc_cache_cs);
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    (void)inst; (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        InitializeCriticalSection(&dc_cache_cs);
        if (dbg_on()) {
            fprintf(stderr, "[11-u32wrap] loaded\n");
            fflush(stderr);
            FILE *f = fopen("e11gdi-load.marker", "a");
            if (f) { fputs("loaded\n", f); fclose(f); }
        }
    }
    return TRUE;
}

static void log_call(const char *kind, HWND hwnd, HDC dc, void *ret)
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
    fprintf(stderr, "[11-u32wrap] %s #%ld hwnd=%p dc=%p caller=%p module=%s\n",
            kind, n, hwnd, dc, ret, mod);
    fflush(stderr);
    FILE *f = fopen("e11gdi-calls.log", "a");
    if (f) {
        fprintf(f, "%s #%ld hwnd=%p dc=%p caller=%p module=%s\n", kind, n, hwnd, dc, ret, mod);
        fclose(f);
    }
}

/* 每线程只保留最近一枚未释放的显示 DC；下一次 GetDC 前先交还旧 DC。
 * 仅 E11_GDI_RECLAIM=1 且未启用 DC 缓存时使用；默认取证不改语义。 */
static __thread HDC last_dc;

static HDC track(HWND hwnd, HDC dc, void *ret)
{
    if (!dc) return dc;
    log_call("GetDC", hwnd, dc, ret);
    if (!reclaim_on() || cache_on()) return dc;
    if (last_dc && last_dc != dc) {
        if (p_release_dc) p_release_dc(hwnd, last_dc);
        else ReleaseDC(hwnd, last_dc);
    }
    last_dc = dc;
    return dc;
}

static HDC WINAPI get_dc_ex_as_getdc(HWND hwnd)
{
    return p_get_dc_ex ? p_get_dc_ex(hwnd, NULL, 0) : NULL;
}

static HDC get_dc_cached_or_real(HWND hwnd, HDC (WINAPI *real)(HWND), void *ret)
{
    int from_cache = 0;
    HDC dc = cached_get_dc(hwnd, &from_cache);
    if (dc) {
        log_call("GetDC", hwnd, dc, ret);
        return dc;
    }
    dc = real ? real(hwnd) : NULL;
    if (cache_on()) cache_store_dc(hwnd, dc);
    return track(hwnd, dc, ret);
}

HDC WINAPI GetDC(HWND hwnd)
{
    resolve_orig();
    return get_dc_cached_or_real(hwnd, p_get_dc, __builtin_return_address(0));
}

HDC WINAPI GetDCEx(HWND hwnd, HRGN hrgn, DWORD flags)
{
    resolve_orig();
    if (flags == 0 && hrgn == NULL)
        return get_dc_cached_or_real(hwnd, get_dc_ex_as_getdc,
                                     __builtin_return_address(0));
    HDC dc = p_get_dc_ex ? p_get_dc_ex(hwnd, hrgn, flags) : NULL;
    return track(hwnd, dc, __builtin_return_address(0));
}

int WINAPI ReleaseDC(HWND hwnd, HDC dc)
{
    int handled = 0;
    if (cache_on()) {
        int rc = cached_release_dc(hwnd, dc, &handled);
        if (handled) {
            log_call("ReleaseDC", hwnd, dc, __builtin_return_address(0));
            return rc;
        }
    }
    resolve_orig();
    if (last_dc == dc) last_dc = NULL;
    log_call("ReleaseDC", hwnd, dc, __builtin_return_address(0));
    return p_release_dc ? p_release_dc(hwnd, dc) : 0;
}
