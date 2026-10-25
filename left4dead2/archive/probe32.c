/*
 * probe32.c — 32 位客户端 GL 链路探测（L4D2 原生版转发方案用）
 *
 * 目的：验证“32 位进程把 GL 渲染转发给 64 位 gf 驱动执行”这条链路是否成立。
 *
 * 原理：
 *   32 位进程 -> Mesa 客户端 GL 栈（libGL/libEGL，i386）
 *             -> virgl 驱动（virtio_gpu_dri.so）+ vtest winsys
 *             -> Unix socket（VTEST_SOCKET_NAME）
 *             -> 64 位 virgl_test_server（host，用 gf 驱动真正渲染）
 *
 * 期望：GL_RENDERER 不是 llvmpipe/softpipe，而是 host 侧驱动的名字（如 LISUAN ...）。
 *
 * 编译：gcc -m32 -O2 -o probe32 probe32.c -ldl
 * 运行：VTEST_SOCKET_NAME=/tmp/vtest.sock MESA_LOADER_DRIVER_OVERRIDE=virtio_gpu ./probe32
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <stdint.h>

typedef void *EGLDisplay, *EGLContext, *EGLSurface, *EGLConfig, *EGLDeviceEXT;
typedef unsigned int EGLenum, EGLBoolean;
typedef int32_t EGLint;

#define EGL_PLATFORM_SURFACELESS_MESA 0x31DD
#define EGL_OPENGL_API 0x30A2
#define EGL_NO_SURFACE ((EGLSurface)0)
#define EGL_NO_CONTEXT ((EGLContext)0)
#define EGL_SURFACE_TYPE 0x3033
#define EGL_PBUFFER_BIT 0x0001
#define EGL_RENDERABLE_TYPE 0x3040
#define EGL_OPENGL_BIT 0x0008
#define EGL_NONE 0x3038
#define GL_VENDOR 0x1F00
#define GL_RENDERER 0x1F01
#define GL_VERSION 0x1F02

static void *H;

static void *sym(const char *name)
{
    void *p = dlsym(H, name);
    if (!p)
        p = ((void *(*)(const char *))dlsym(H, "eglGetProcAddress"))(name);
    return p;
}

int main(void)
{
    H = dlopen("libEGL.so.1", RTLD_NOW);
    if (!H) { fprintf(stderr, "dlopen libEGL failed: %s\n", dlerror()); return 1; }

    EGLDisplay (*eglGetPlatformDisplayEXT)(EGLenum, void *, const EGLint *);
    EGLDisplay (*eglGetDisplay)(void *);
    EGLBoolean (*eglInitialize)(EGLDisplay, EGLint *, EGLint *);
    EGLBoolean (*eglBindAPI)(EGLenum);
    EGLBoolean (*eglChooseConfig)(EGLDisplay, const EGLint *, EGLConfig *, EGLint, EGLint *);
    EGLContext (*eglCreateContext)(EGLDisplay, EGLConfig, EGLContext, const EGLint *);
    EGLBoolean (*eglMakeCurrent)(EGLDisplay, EGLSurface, EGLSurface, EGLContext);
    EGLint (*eglGetError)(void);

    eglGetDisplay = sym("eglGetDisplay");
    eglInitialize = sym("eglInitialize");
    eglBindAPI = sym("eglBindAPI");
    eglChooseConfig = sym("eglChooseConfig");
    eglCreateContext = sym("eglCreateContext");
    eglMakeCurrent = sym("eglMakeCurrent");
    eglGetError = sym("eglGetError");
    eglGetPlatformDisplayEXT = sym("eglGetPlatformDisplayEXT");

    EGLDisplay dpy = NULL;
    if (eglGetPlatformDisplayEXT) {
        dpy = eglGetPlatformDisplayEXT(EGL_PLATFORM_SURFACELESS_MESA, NULL, NULL);
        printf("platform = surfaceless (EGL_MESA_platform_surfaceless)\n");
    }
    if (!dpy && eglGetDisplay) {
        dpy = eglGetDisplay((void *)0);
        printf("platform = default\n");
    }

    EGLint maj, min;
    if (!eglInitialize(dpy, &maj, &min)) {
        fprintf(stderr, "eglInitialize failed err=0x%x\n", eglGetError());
        return 1;
    }
    printf("EGL %d.%d\n", maj, min);

    eglBindAPI(EGL_OPENGL_API);

    EGLint cfg_attr[] = { EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
                          EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT, EGL_NONE };
    EGLConfig cfg; EGLint n = 0;
    if (!eglChooseConfig(dpy, cfg_attr, &cfg, 1, &n) || n < 1) {
        fprintf(stderr, "eglChooseConfig failed err=0x%x\n", eglGetError());
        return 1;
    }

    EGLContext ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, NULL);
    if (!ctx) { fprintf(stderr, "eglCreateContext failed err=0x%x\n", eglGetError()); return 1; }
    if (!eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx)) {
        fprintf(stderr, "eglMakeCurrent failed err=0x%x\n", eglGetError());
        return 1;
    }

    void *GL = dlopen("libGL.so.1", RTLD_NOW);
    const unsigned char *(*glGetString)(unsigned int) =
        GL ? dlsym(GL, "glGetString") : NULL;
    if (!glGetString) glGetString = sym("glGetString");
    if (!glGetString) { fprintf(stderr, "no glGetString\n"); return 1; }

    const char *vend = (const char *)glGetString(GL_VENDOR);
    const char *rend = (const char *)glGetString(GL_RENDERER);
    const char *vers = (const char *)glGetString(GL_VERSION);
    const char *needle = "llvmpipe|softpipe|swrast";
    printf("GL_VENDOR   = %s\n", vend ? vend : "(null)");
    printf("GL_RENDERER = %s\n", rend ? rend : "(null)");
    printf("GL_VERSION  = %s\n", vers ? vers : "(null)");
    printf("VERDICT     = %s\n",
           (rend && !strstr(rend, "llvmpipe") && !strstr(rend, "softpipe") &&
            !strstr(rend, "swrast")) ? "HARDWARE-FORWARDED" : "SOFTWARE");
    (void)needle;
    return 0;
}
