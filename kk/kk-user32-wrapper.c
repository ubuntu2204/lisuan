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
 * 只自实现 UpdateLayeredWindow / UpdateLayeredWindowIndirect：子窗口的
 * pptDst（父客户区坐标，含内容层 (0,0) 与弹窗关闭按钮等非零相对坐标）换算
 * 成屏幕坐标再转交内置；「保持位置」调用（pptDst == 当前全局矩形原点且非
 * (0,0)）原样透传（一律换算会二次偏移振荡）。
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
#include <stdio.h>
/* 本 dll 就是 user32：自实现的两个导出覆盖头文件原型（dllimport 重声明
   警告属预期）；其余 user32 调用保持 dllimport，经 mingw 导入库链接，运行时
   由本 dll 的 forwarder 转回内置 user32_orig。 */

/* 诊断开关（KK_WRAP_DEBUG=1，日志进启动日志）：打印每次 ULW 的 hwnd / 父窗口 /
   原始 pptDst / 当前全局矩形 / 判读决策 / 换算后 dst，供坐标语义取证。 */
static int wrap_dbg(void)
{
    static int on = -1;
    if (on < 0) { const char *e = getenv("KK_WRAP_DEBUG"); on = e && atoi(e); }
    return on;
}
#define WLOG(...) do { if (wrap_dbg()) fprintf(stderr, "[kk-wrap] " __VA_ARGS__); } while (0)

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

/* 子窗口的 pptDst 是父客户区坐标（KK 的内容层原点调用 (0,0)、弹窗关闭按钮
   的 (popup_width-42,0) 等都是），换算成屏幕坐标再交给内置实现。
   例外：KK 的「保持位置」调用传的是当前全局矩形（pptDst == GetWindowRect
   原点且非 (0,0)），原样透传；若一律换算会把保持位置调用二次偏移
   （(294,33)->(588,66) 振荡，2026-11-14 trace 取证）。 */
static POINT map_dst(HWND hwnd, const POINT *src, POINT *buf)
{
    HWND parent = GetParent(hwnd);
    RECT cur;
    *buf = *src;
    if (!parent) { WLOG("map hwnd=%p top-level src=(%ld,%ld) passthrough\n", hwnd, src->x, src->y); return *buf; }
    if (!(src->x == 0 && src->y == 0) &&
        GetWindowRect(hwnd, &cur) && cur.left == src->x && cur.top == src->y) {
        WLOG("map hwnd=%p parent=%p src=(%ld,%ld) keep-position\n", hwnd, parent, src->x, src->y);
        return *buf; /* 保持位置：已是屏幕坐标 */
    }
    GetWindowRect(hwnd, &cur);
    MapWindowPoints(parent, HWND_DESKTOP, buf, 1);
    WLOG("map hwnd=%p parent=%p src=(%ld,%ld) cur=(%ld,%ld) -> dst=(%ld,%ld)\n",
         hwnd, parent, src->x, src->y, cur.left, cur.top, buf->x, buf->y);
    return *buf;
}

/* 内容层（1332x946 分层子窗口）在 Windows 上恒位于父客户区原点；KK 的
   ULW 调用语义混杂（原点 / 保持位置 / 屏幕坐标），逐一判读仍会振荡出
   (588,66) 等错误矩形 → 命中区右偏（用户报障：点右面一点才激发）。
   故对内容层直接钉死在父客户区原点，与 Windows 实际表现一致，
   win32k 矩形与 X 窗口原点恒等 → 命中零偏移。 */
static int is_content_layer(HWND hwnd)
{
    RECT r;
    return GetWindowRect(hwnd, &r) && (r.right - r.left) == 1332 &&
           (r.bottom - r.top) == 946 && GetParent(hwnd) != NULL;
}
static POINT layer_origin(HWND hwnd)
{
    POINT pt = { 0, 0 };
    MapWindowPoints(GetParent(hwnd), HWND_DESKTOP, &pt, 1);
    return pt;
}

/* 活动弹窗的独立关闭按钮窗（Qt5152QWindowToolSaveBits）。两个并发缺陷
   （2026-11-16 trace 定案）：
   1) 活锁：Qt 建 94x60 窗口但只 ULW 60x60 surface（pptDst=NULL → Wine 居中
      放置，bounds (17,0)-(77,60) ≠ 窗口）→ Wine winex11 对 bounds≠窗口 的
      OR 分层窗陷入 Expose→重绘→surface 重建死循环（主线程 ~100% CPU、
      全 UI 输入冻死，用户报障「漂移」的根源）。把 win32k 窗口缩到 60x60
      与 surface 一致 → bounds==窗口 → 循环收敛（不能扩拷贝：源 DC 只有
      60x60，越界拷贝触发 GraphicsExpose 风暴，已证伪）。
   2) 漂移：按钮 win32k 落在弹窗中心附近（KK 坐标混算），与视觉 X（弹窗
      右上角）错开 → X 点不动、模态遮罩解除不了。钉死到弹窗右上角：
      41x41 锚点 (W-42,0)；60x60 按 glyph 中心对齐 (W-55,-6)。 */
static int close_button_size(HWND hwnd, RECT *r)
{
    char cls[64];
    int w, h;
    if (!GetWindowRect(hwnd, r)) return 0;
    w = r->right - r->left; h = r->bottom - r->top;
    if (!((w == 41 && h == 41) || (w == 94 && h == 60) || (w == 60 && h == 60)))
        return 0;
    if (!GetClassNameA(hwnd, cls, sizeof(cls)) ||
        strcmp(cls, "Qt5152QWindowToolSaveBits")) return 0;
    return 1;
}
struct popup_find { HWND hwnd; };
static BOOL CALLBACK popup_enum(HWND hwnd, LPARAM lp)
{
    struct popup_find *f = (struct popup_find *)lp;
    RECT r;
    int w, h;
    if (!IsWindowVisible(hwnd)) return TRUE;
    if (!(GetWindowLongW(hwnd, GWL_EXSTYLE) & WS_EX_LAYERED)) return TRUE;
    if (!GetWindowRect(hwnd, &r)) return TRUE;
    w = r.right - r.left; h = r.bottom - r.top;
    if ((w == 920 && h == 560) || (w == 440 && h == 360)) { f->hwnd = hwnd; return FALSE; }
    return TRUE;
}
static POINT close_button_origin(HWND hwnd, const RECT *br)
{
    struct popup_find f = { NULL };
    RECT pr;
    POINT pt;
    int bw = br->right - br->left;
    EnumWindows(popup_enum, (LPARAM)&f);
    if (!f.hwnd || !GetWindowRect(f.hwnd, &pr)) { pt.x = br->left; pt.y = br->top; return pt; }
    if (bw == 41) { pt.x = pr.left + (pr.right - pr.left) - 42; pt.y = pr.top; }
    else          { pt.x = pr.left + (pr.right - pr.left) - 55; pt.y = pr.top - 6; }
    return pt;
}

/* 按钮的 ULW 早于弹窗创建，之后可能不再调 → 借弹窗本体的 ULW 作时机补钉。
   按钮在 win32k 树里的父窗不定（顶层 / 弹窗 / 其它 helper 都见过，枚举不可靠），
   故在其自身 ULW 触发时登记 hwnd，补钉直接遍历登记表。 */
static HWND btn_hwnds[4];
static int n_btn;
static void remember_button(HWND hwnd)
{
    int i;
    for (i = 0; i < n_btn; i++) if (btn_hwnds[i] == hwnd) return;
    if (n_btn < 4) btn_hwnds[n_btn++] = hwnd;
}
static void fix_one_button(HWND hwnd)
{
    RECT br;
    POINT pt;
    HWND parent;
    int cx, cy;
    struct popup_find f = { NULL };
    if (!IsWindow(hwnd) || !close_button_size(hwnd, &br)) return;
    EnumWindows(popup_enum, (LPARAM)&f);
    if (!f.hwnd) return;  /* 弹窗未出现（登录阶段）不动按钮，免扰 Qt 初始化 */
    pt = close_button_origin(hwnd, &br);
    cx = br.right - br.left; cy = br.bottom - br.top;
    if (cx == 94) { cx = 60; cy = 60; }  /* 缩到 surface 尺寸破活锁 */
    if (br.left == pt.x && br.top == pt.y &&
        br.right - br.left == cx && br.bottom - br.top == cy) return;
    parent = GetParent(hwnd);
    if (parent) MapWindowPoints(HWND_DESKTOP, parent, &pt, 1);
    WLOG("fix-btn hwnd=%p -> (%ld,%ld) %dx%d\n", hwnd, pt.x, pt.y, cx, cy);
    SetWindowPos(hwnd, NULL, pt.x, pt.y, cx, cy, SWP_NOZORDER | SWP_NOACTIVATE);
}
static void fix_close_buttons(HWND popup)
{
    RECT pr;
    int w, h, i;
    if (!GetWindowRect(popup, &pr)) return;
    w = pr.right - pr.left; h = pr.bottom - pr.top;
    if (!((w == 920 && h == 560) || (w == 440 && h == 360))) return;
    for (i = 0; i < n_btn; i++) fix_one_button(btn_hwnds[i]);
}

BOOL WINAPI UpdateLayeredWindow(HWND hwnd, HDC hdcDst, POINT *pptDst, SIZE *psize,
                                HDC hdcSrc, POINT *pptSrc, COLORREF crKey,
                                BLENDFUNCTION *pblend, DWORD dwFlags)
{
    POINT buf;
    POINT *dst = pptDst;
    RECT br;
    resolve_orig();
    if (!orig_ulw) return FALSE;
    if (is_content_layer(hwnd)) { buf = layer_origin(hwnd); dst = &buf;
        WLOG("ulw hwnd=%p content-layer pinned=(%ld,%ld)\n", hwnd, buf.x, buf.y); }
    else if (close_button_size(hwnd, &br)) {
        remember_button(hwnd);
        /* 94x60 与 60x60 surface 的 bounds≠窗口 Expose 活锁在登录期就空转，
           缩尺寸不依赖弹窗、登记即做；锚点钉死仍等弹窗出现。 */
        if (br.right - br.left == 94)
            SetWindowPos(hwnd, NULL, 0, 0, 60, 60,
                         SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        buf = close_button_origin(hwnd, &br); dst = &buf;
        WLOG("ulw-btn hwnd=%p pinned=(%ld,%ld)\n", hwnd, buf.x, buf.y);
    }
    else if (pptDst) { map_dst(hwnd, pptDst, &buf); dst = &buf; }
    fix_close_buttons(hwnd);
    return orig_ulw(hwnd, hdcDst, dst, psize, hdcSrc, pptSrc, crKey, pblend, dwFlags);
}

BOOL WINAPI UpdateLayeredWindowIndirect(HWND hwnd, const UPDATELAYEREDWINDOWINFO *info)
{
    UPDATELAYEREDWINDOWINFO local;
    POINT buf;
    RECT br;
    resolve_orig();
    if (!orig_ulwi) return FALSE;
    if (info && info->pptDst) {
        local = *info;
        if (is_content_layer(hwnd)) buf = layer_origin(hwnd);
        else if (close_button_size(hwnd, &br)) {
            remember_button(hwnd);
            if (br.right - br.left == 94)
                SetWindowPos(hwnd, NULL, 0, 0, 60, 60,
                             SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
            buf = close_button_origin(hwnd, &br);
        }
        else map_dst(hwnd, info->pptDst, &buf);
        local.pptDst = &buf;
        fix_close_buttons(hwnd);
        return orig_ulwi(hwnd, &local);
    }
    fix_close_buttons(hwnd);
    return orig_ulwi(hwnd, info);
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    (void)inst; (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) DisableThreadLibraryCalls(inst);
    return TRUE;
}