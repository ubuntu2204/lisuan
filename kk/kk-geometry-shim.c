/* KK 官方对战平台 Wine 分层子窗口几何垫片（LD_PRELOAD，仅注入 KK 的 wine 进程树）。
 *
 * 根因（2026-11-14 取证，WINEDEBUG=+x11drv,+win,+event）：KK 的 Qt UI 把承载
 * 侧栏 / 遮罩的内容层先以 WS_POPUP 建在 (0,0)，再 SetParent 挂到主框架下，并
 * 用 UpdateLayeredWindow(pptDst=(0,0)) 定位——Windows 对子窗口把 pptDst 解释为
 * 父客户区坐标，Wine/Proton 11 却按屏幕坐标处理，于是 win32k 里内容层矩形永远
 * 停在屏幕 (0,0)：
 *   1) 内容层 X 窗口画在屏幕 (0,0)，与主框架错开整个框架原点（UI 错位）；
 *   2) win32k 的子窗口命中测试裁剪到父矩形，内容层几乎整片在父矩形外，
 *      侧栏 / 导航的鼠标输入被直接丢弃（按键点不动）；
 *   3) 内容层是 override-redirect 且 WS_EX_TOPMOST，盖住活动弹窗的原生窗口，
 *      弹窗的 X / 按钮永远收不到点击；
 *   4) SetParent(web 子窗口) 期间主框架客户面被误判 offscreen，客户窗口被
 *      detach 到 1x1 dummy 父窗口下，QtWebEngine 游戏列表整片不可见。
 *
 * 坐标语义根因由 war3/kk-user32-wrapper.c（native user32 包装层）修复：子窗口
 * ULW 的 pptDst 换算成屏幕坐标后，内容层 win32k 矩形 = 主框架客户区原点，
 * 视觉与输入自然重合。本垫片只修剩下两个 X11 堆叠 / 挂载缺陷（不监控、
 * 不守护、不重启进程）：
 *   A. 剥夺内容层的抬升操作（XRaiseWindow / XMapRaised / CWStackMode），
 *      并把弹窗 / 通知等 Depth32 窗口保持 OR、映射后抬到内容层之上
 *      （mutter 把受管窗口排在 OR 窗口之下且忽略客户端 raise 请求），
 *      弹窗按键恢复可点。
 *   B. 拒绝把主框架客户面 detach 到 1x1 dummy：Wine 在 SetParent(web 子窗口)
 *      期间误判 offscreen 后不再挂回，拒绝 detach 保持原生 UI 镜像可见。
 *      注意：lobby 的 QtWebEngine 交互面仍因 Wine surface offscreen 锁存不
 *      呈现（既有缺陷，基线同样存在，见 README §5 限制），垫片不掩盖它。
 *
 * 识别靠创建时的精确尺寸 / 深度 / OR 组合（主框架 1332x945 Depth24、内容层
 * 1332x946 Depth32 OR），且只认第一个匹配者；厂商改尺寸时垫片自动退化为
 * 透传，不会误伤 Warcraft III 本体等其它窗口。
 *
 * Platform.exe 走 i386-unix wine-preloader（32 位 unix 进程），垫片必须编 32
 * 位；本机无 multilib 头文件，故本文件不依赖任何系统头文件（X11 类型 / 结构
 * 布局按 Xlib 自声明，X 函数全部 dlsym(RTLD_NEXT) 解析），同一源码可编两种
 * 架构：
 *   64 位：gcc -shared -fPIC -O2 -o kk-geometry-shim.so kk-geometry-shim.c -ldl
 *   32 位：gcc -m32 -shared -fPIC -O2 -nostartfiles -nostdlib -fno-stack-protector \
 *           -fno-asynchronous-unwind-tables \
 *           -o kk-geometry-shim32.so kk-geometry-shim.c /usr/lib/i386-linux-gnu/libc.so.6
 * 调试：KK_SHIM_DEBUG=1（日志进启动日志）。License: Apache-2.0
 */
#define _GNU_SOURCE

typedef unsigned long Window;
typedef unsigned long Pixmap;
typedef unsigned long Colormap;
typedef unsigned long Cursor;
typedef int Bool;
typedef int Status;
typedef struct _XDisplay Display;
typedef struct _XVisual Visual;

typedef struct {
    int x, y;
    int width, height;
    int border_width;
    Window sibling;
    int stack_mode;
} XWindowChanges;

typedef struct {
    Pixmap background_pixmap;
    unsigned long background_pixel;
    Pixmap border_pixmap;
    unsigned long border_pixel;
    int bit_gravity;
    int win_gravity;
    int backing_store;
    unsigned long backing_planes;
    unsigned long backing_pixel;
    Bool save_under;
    long event_mask;
    long do_not_propagate_mask;
    Bool override_redirect;
    Colormap colormap;
    Cursor cursor;
} XSetWindowAttributes;

enum {
    CWX = 1L << 0, CWY = 1L << 1, CWStackMode = 1L << 6,
    CWOverrideRedirect = 1L << 9
};

extern void *stderr;
extern int fprintf(void *, const char *, ...);
extern char *getenv(const char *);
extern int atoi(const char *);
extern void *dlsym(void *, const char *);
#define RTLD_NEXT ((void *)-1)

static Window w_frame; /* 主框架：Depth24 1332x945 */
static Window w_layer; /* 内容层：Depth32 1332x946 override-redirect */
static Window dummies[4]; /* 1x1 OR dummy 父窗口（detach 客户窗口的停车场） */
static int n_dummies;
static Window big_clients[8]; /* 创建时 >=1000x700 的客户窗口（渲染面等） */
static int n_big;
static Window popups[8]; /* Depth32 受管弹窗 / 通知：OR 化并保持在内容层之上 */
static int n_popups;

static int debug_on = -1;
static int dbg(void)
{
    if (debug_on < 0) {
        const char *e = getenv("KK_SHIM_DEBUG");
        debug_on = e && atoi(e);
    }
    return debug_on;
}
#define LOG(...) do { if (dbg()) fprintf(stderr, "[kk-shim] " __VA_ARGS__); } while (0)

#define REAL(name) static name##_t real_##name; \
    static name##_t get_##name(void) { \
        name##_t p = __atomic_load_n(&real_##name, __ATOMIC_RELAXED); \
        if (!p) { p = (name##_t)dlsym(RTLD_NEXT, #name); \
                  __atomic_store_n(&real_##name, p, __ATOMIC_RELAXED); } \
        return p; }

typedef Window (*XCreateWindow_t)(Display *, Window, int, int, unsigned int, unsigned int,
                                  unsigned int, int, unsigned int, Visual *, unsigned long,
                                  XSetWindowAttributes *);
typedef Window (*XDefaultRootWindow_t)(Display *);
typedef int (*XConfigureWindow_t)(Display *, Window, unsigned int, XWindowChanges *);
typedef int (*XReconfigureWMWindow_t)(Display *, Window, int, unsigned int, XWindowChanges *);
typedef int (*XMoveWindow_t)(Display *, Window, int, int);
typedef int (*XMoveResizeWindow_t)(Display *, Window, int, int, unsigned int, unsigned int);
typedef int (*XReparentWindow_t)(Display *, Window, Window, int, int);
typedef int (*XChangeWindowAttributes_t)(Display *, Window, unsigned long, XSetWindowAttributes *);
typedef int (*XMapWindow_t)(Display *, Window);
typedef int (*XDestroyWindow_t)(Display *, Window);
typedef int (*XMapRaised_t)(Display *, Window);
typedef int (*XRaiseWindow_t)(Display *, Window);
REAL(XCreateWindow)
REAL(XDefaultRootWindow)
REAL(XConfigureWindow)
REAL(XReconfigureWMWindow)
REAL(XMoveWindow)
REAL(XMoveResizeWindow)
REAL(XReparentWindow)
REAL(XChangeWindowAttributes)
REAL(XMapWindow)
REAL(XDestroyWindow)
REAL(XMapRaised)
REAL(XRaiseWindow)

/* 主框架识别：Depth24、根窗口之子、1332x945（managed 创建尺寸）；只作 web
   客户窗口的重挂目标，不改其几何（几何由 user32 包装层在 win32k 侧修复）。 */
static int is_frame_create(Display *dpy, Window parent, int depth, unsigned int w, unsigned int h)
{
    return !w_frame && depth == 24 && parent == get_XDefaultRootWindow()(dpy) &&
           w == 1332 && h == 945;
}
static int is_layer_create(int depth, unsigned int w, unsigned int h, int or_flag)
{
    return !w_layer && or_flag && depth == 32 && w == 1332 && h == 946;
}
static int is_popup_create(int depth, unsigned int w, unsigned int h, int or_flag)
{
    /* 弹窗 / 通知：Depth32 OR、中等尺寸；排除内容层与小_tooltip_ */
    return depth == 32 && or_flag && w >= 300 && h >= 150 && !(w == 1332 && h == 946);
}
static int is_dummy(Window w)
{
    int i;
    for (i = 0; i < n_dummies; i++) if (dummies[i] == w) return 1;
    return 0;
}
static int is_big_client(Window w)
{
    int i;
    for (i = 0; i < n_big; i++) if (big_clients[i] == w) return 1;
    return 0;
}
static int is_popup(Window w)
{
    int i;
    for (i = 0; i < n_popups; i++) if (popups[i] == w) return 1;
    return 0;
}
static void raise_popups(Display *dpy)
{
    int i;
    for (i = 0; i < n_popups; i++) get_XRaiseWindow()(dpy, popups[i]);
}

Window XCreateWindow(Display *dpy, Window parent, int x, int y, unsigned int w, unsigned int h,
                     unsigned int border, int depth, unsigned int class, Visual *visual,
                     unsigned long mask, XSetWindowAttributes *attr)
{
    XCreateWindow_t real = get_XCreateWindow();
    int or_flag = attr && (mask & CWOverrideRedirect) && attr->override_redirect;
    Window ret;
    if (dbg() && depth == 32)
        LOG("create32 parent=%lx %ux%u or=%d attr=%p mask=%lx\n", parent, w, h, or_flag, (void *)attr, mask);
    if (is_frame_create(dpy, parent, depth, w, h)) {
        ret = real(dpy, parent, x, y, w, h, border, depth, class, visual, mask, attr);
        w_frame = ret;
        LOG("frame %lx identified\n", ret);
        return ret;
    }
    ret = real(dpy, parent, x, y, w, h, border, depth, class, visual, mask, attr);
    if (is_layer_create(depth, w, h, or_flag)) {
        w_layer = ret;
        LOG("layer %lx identified\n", ret);
    } else if (is_popup_create(depth, w, h, or_flag) && n_popups < 8) {
        /* 弹窗 / 通知：保持 OR 才能堆到内容层之上（mutter 受管窗口恒在 OR 之下） */
        popups[n_popups++] = ret;
        LOG("popup %lx tracked\n", ret);
    } else if (or_flag && w == 1 && h == 1 && n_dummies < 4) {
        dummies[n_dummies++] = ret;
    } else if (w >= 1000 && h >= 700 && n_big < 8) {
        big_clients[n_big++] = ret;
    }
    return ret;
}

int XConfigureWindow(Display *dpy, Window w, unsigned int mask, XWindowChanges *ch)
{
    if (w == w_layer) mask &= ~CWStackMode; /* 不许抬升：保持在弹窗之下 */
    return get_XConfigureWindow()(dpy, w, mask, ch);
}

int XReconfigureWMWindow(Display *dpy, Window w, int screen, unsigned int mask, XWindowChanges *ch)
{
    if (w == w_layer) {
        mask &= ~CWStackMode;
        return get_XConfigureWindow()(dpy, w, mask, ch);
    }
    if (is_popup(w)) {
        /* OR 弹窗没有 WM 应答 _NET_WM_MOVERESIZE，直连配置 */
        return get_XConfigureWindow()(dpy, w, mask, ch);
    }
    return get_XReconfigureWMWindow()(dpy, w, screen, mask, ch);
}

int XMoveWindow(Display *dpy, Window w, int x, int y)
{
    return get_XMoveWindow()(dpy, w, x, y);
}

int XMoveResizeWindow(Display *dpy, Window w, int x, int y, unsigned int cw, unsigned int ch)
{
    return get_XMoveResizeWindow()(dpy, w, x, y, cw, ch);
}

int XChangeWindowAttributes(Display *dpy, Window w, unsigned long mask, XSetWindowAttributes *attr)
{
    /* Wine 会在 managed 状态变化时改 OR；内容层 / 弹窗锁定 OR 以保持堆叠可控。 */
    if ((mask & CWOverrideRedirect) && attr && (w == w_layer || is_popup(w)))
        attr->override_redirect = 1;
    return get_XChangeWindowAttributes()(dpy, w, mask, attr);
}

int XReparentWindow(Display *dpy, Window w, Window parent, int x, int y)
{
    /* Wine 在 SetParent(web 子窗口) / 视觉切换期间把主框架客户面误判
       offscreen 并 detach 到 1x1 dummy，之后不再挂回 → 原生 UI / 游戏列表
       不可见。大客户窗口（渲染面）直接拒绝 detach：留在创建时的 whole
       窗口下（attach_client_window 后续的正常重挂不受影响）。 */
    if (is_dummy(parent) && is_big_client(w) && w != w_frame && w != w_layer) {
        LOG("block detach of client %lx to dummy %lx\n", w, parent);
        return 1;
    }
    return get_XReparentWindow()(dpy, w, parent, x, y);
}

int XDestroyWindow(Display *dpy, Window w)
{
    /* 登录框架→大厅框架切换时清空识别，避免重定向指向已销毁 XID */
    if (w == w_frame) w_frame = 0;
    if (w == w_layer) w_layer = 0;
    return get_XDestroyWindow()(dpy, w);
}

int XMapWindow(Display *dpy, Window w)
{
    int ret = get_XMapWindow()(dpy, w);
    if (is_popup(w)) get_XRaiseWindow()(dpy, w);
    else if (w == w_layer) raise_popups(dpy);
    return ret;
}

int XMapRaised(Display *dpy, Window w)
{
    if (w == w_layer) { int ret = get_XMapWindow()(dpy, w); raise_popups(dpy); return ret; }
    return get_XMapRaised()(dpy, w);
}

int XRaiseWindow(Display *dpy, Window w)
{
    if (w == w_layer) { LOG("suppress raise of layer %lx\n", w); return 1; }
    return get_XRaiseWindow()(dpy, w);
}