/* kk-shim-exit-test.c — 垫片退出残框清理（C 项）的确定性机制验收客户端。
 *
 * 以 LD_PRELOAD 加载 kk-geometry-shim 后按特征尺寸建窗：
 *   主框架 1332x945 d24 根之子；内容层 1332x946 d32 OR；挽留弹窗 440x360。
 * 默认（非退出路径）：隐藏主框架 → 断言内容层同步隐藏；恢复主框架 →
 *   断言内容层同步恢复；再隐藏后睡 7s，watchdog 不得杀进程（打印 no-watchdog）。
 * arm（退出路径）：先销毁挽留弹窗（退出意图）再隐藏主框架 → 断言内容层
 *   同步隐藏；watchdog 应在 ~5s 后从 /proc/self/exe 同目录推导 wineserver
 *   跑 -k（测试用假 wineserver 记录调用）并 _exit(0)。若 8s 后仍能打印
 *   FAIL-watchdog 说明兜底未触发。
 * 窗口建在屏外负坐标，避免打扰真实桌面。License: Apache-2.0
 */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int xerr;
static int err_handler(Display *d, XErrorEvent *e)
{
    (void)d; (void)e;
    xerr = 1;
    return 0;
}

int main(int argc, char **argv)
{
    int arm = argc > 1 && !strcmp(argv[1], "arm");
    Display *dpy = XOpenDisplay(NULL);
    Window root, frame, layer, dialog;
    XSetWindowAttributes attr;
    XWindowAttributes wa;
    XVisualInfo vi;

    setvbuf(stdout, NULL, _IONBF, 0);
    if (!dpy) { fprintf(stderr, "no display\n"); return 77; }
    XSetErrorHandler(err_handler);
    root = DefaultRootWindow(dpy);

    frame = XCreateWindow(dpy, root, -3000, -3000, 1332, 945, 0, 24,
                          InputOutput, CopyFromParent, 0, NULL);
    XMapWindow(dpy, frame);
    if (!XMatchVisualInfo(dpy, DefaultScreen(dpy), 32, TrueColor, &vi)) {
        fprintf(stderr, "no depth32 visual\n");
        return 77;
    }
    attr.override_redirect = True;
    /* d32 窗必须显式给 background/border pixel：默认继承父窗 24 深度
       pixmap 会 BadMatch（XWayland 实测）。 */
    attr.background_pixel = 0;
    attr.border_pixel = 0;
    attr.colormap = XCreateColormap(dpy, root, vi.visual, AllocNone);
    layer = XCreateWindow(dpy, root, -3000, -3000, 1332, 946, 0, 32,
                          InputOutput, vi.visual,
                          CWOverrideRedirect | CWBackPixel | CWBorderPixel | CWColormap,
                          &attr);
    XSync(dpy, False);
    if (xerr) { fprintf(stderr, "d32 create rejected\n"); return 77; }
    XMapWindow(dpy, layer);
    dialog = XCreateWindow(dpy, root, -3000, -3000, 440, 360, 0, 24,
                           InputOutput, CopyFromParent, 0, NULL);
    XMapWindow(dpy, dialog);
    XSync(dpy, False);

    if (arm) XDestroyWindow(dpy, dialog);
    XUnmapWindow(dpy, frame);
    XSync(dpy, False);
    XGetWindowAttributes(dpy, layer, &wa);
    if (wa.map_state != IsUnmapped) { fprintf(stderr, "FAIL layer not hidden\n"); return 1; }
    printf("layer-hidden\n");

    if (arm) {
        sleep(8); /* watchdog 应 ~5s 终止本进程；能走到这里即失败 */
        fprintf(stderr, "FAIL-watchdog\n");
        return 1;
    }

    XMapWindow(dpy, frame);
    XSync(dpy, False);
    XGetWindowAttributes(dpy, layer, &wa);
    if (wa.map_state != IsViewable) { fprintf(stderr, "FAIL layer not restored\n"); return 1; }
    printf("layer-restored\n");
    XUnmapWindow(dpy, frame);
    XSync(dpy, False);
    sleep(7); /* 非退出路径：watchdog 不得触发 */
    printf("no-watchdog\n");
    return 0;
}