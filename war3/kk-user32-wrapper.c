/* KK 官方对战平台 user32 包装层：修 UpdateLayeredWindow 子窗口坐标语义。
 *
 * 根因（2026-11-14 取证）：KK 的 Qt UI 把内容层 SetParent 挂到主框架下后，用
 * UpdateLayeredWindow(pptDst=(0,0)) 定位。Windows 对子窗口把 pptDst 解释为
 * 父客户区坐标，Wine/Proton 11 的 win32u 却按屏幕坐标处理，于是内容层 win32k
 * 矩形永远停在屏幕 (0,0)：与主框架错开整个框架原点（UI 错位），且 win32k 的
 * 子窗口命中测试裁剪到父矩形，内容层几乎整片在父矩形外，侧栏 / 导航的鼠标
 * 输入被直接丢弃（按键点不动）。
 *
 * 本包装层以 native user32 身份加载（WINEDLLOVERRIDES=user32=n,b，仅 KK 进程
 * 树），其余 1300 余个导出全部 PE forwarder 转回重命名的 Wine 内置
 * user32_orig.dll（内置 PE 的 unixlib 名字嵌在 PE 里，改名加载已实测可行），
 * 只自实现 UpdateLayeredWindow / UpdateLayeredWindowIndirect：子窗口且
 * pptDst==(0,0)（KK 的“父客户区原点”调用）时换算成屏幕坐标再转交内置；
 * KK 另有“保持位置”调用传当前全局矩形，原样透传（一律换算会二次偏移振荡）。
 *
 * 编译（def 由 gen-kk-user32-def.py 从内置 user32 生成；启动器自动做）：
 *   i686-w64-mingw32-dlltool --def user32.def --dllname user32.dll \
 *       --output-lib libkkuser32wrap.a
 *   i686-w64-mingw32-gcc -shared -o user32.dll kk-user32-wrapper.c \
 *       libkkuser32wrap.a -luser32 -lkernel32
 * 部署：前缀 drive_c/windows/syswow64/{user32.dll,user32_orig.dll}（启动器做）。
 * License: Apache-2.0
 */
#include <windows.h>
/* 本 dll 就是 user32：自实现的两个导出覆盖头文件原型（dllimport 重声明
   警告属预期）；其余 user32 调用保持 dllimport，经 mingw 导入库链接，运行时
   由本 dll 的 forwarder 转回内置 user32_orig。 */

typedef BOOL (WINAPI *ULW_t)(HWND, HDC, POINT *, SIZE *, HDC, POINT *, COLORREF,
                             BLENDFUNCTION *, DWORD);
typedef BOOL (WINAPI *ULWI_t)(HWND, const UPDATELAYEREDWINDOWINFO *);

static HMODULE orig_mod;
static ULW_t orig_ulw;
static ULWI_t orig_ulwi;

static void resolve_orig(void)
{
    if (orig_mod) return;
    orig_mod = LoadLibraryA("user32_orig.dll");
    if (!orig_mod) return;
    orig_ulw = (ULW_t)GetProcAddress(orig_mod, "UpdateLayeredWindow");
    orig_ulwi = (ULWI_t)GetProcAddress(orig_mod, "UpdateLayeredWindowIndirect");
}

/* 子窗口且 pptDst==(0,0)：KK 的“放到父客户区原点”调用（Windows 语义），
   换算成屏幕坐标。其余调用（KK 用 GetWindowRect 的当前全局矩形做“保持位置”
   更新）原样透传；若一律换算会把保持位置调用二次偏移（(294,33)->(588,66)
   振荡，2026-11-14 trace 取证）。 */
static POINT map_dst(HWND hwnd, const POINT *src, POINT *buf)
{
    HWND parent = GetParent(hwnd);
    *buf = *src;
    if (parent && src->x == 0 && src->y == 0)
        MapWindowPoints(parent, HWND_DESKTOP, buf, 1);
    return *buf;
}

BOOL WINAPI UpdateLayeredWindow(HWND hwnd, HDC hdcDst, POINT *pptDst, SIZE *psize,
                                HDC hdcSrc, POINT *pptSrc, COLORREF crKey,
                                BLENDFUNCTION *pblend, DWORD dwFlags)
{
    POINT buf;
    POINT *dst = pptDst;
    resolve_orig();
    if (!orig_ulw) return FALSE;
    if (pptDst) { map_dst(hwnd, pptDst, &buf); dst = &buf; }
    return orig_ulw(hwnd, hdcDst, dst, psize, hdcSrc, pptSrc, crKey, pblend, dwFlags);
}

BOOL WINAPI UpdateLayeredWindowIndirect(HWND hwnd, const UPDATELAYEREDWINDOWINFO *info)
{
    UPDATELAYEREDWINDOWINFO local;
    POINT buf;
    resolve_orig();
    if (!orig_ulwi) return FALSE;
    if (info && info->pptDst) {
        local = *info;
        map_dst(hwnd, info->pptDst, &buf);
        local.pptDst = &buf;
        return orig_ulwi(hwnd, &local);
    }
    return orig_ulwi(hwnd, info);
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    (void)inst; (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) DisableThreadLibraryCalls(inst);
    return TRUE;
}