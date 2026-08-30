/* gf Vulkan ICD 协商+扩展垫片
 *
 * 两个问题:
 *  1) 砺算 libvulkan_gf.so (Mesa 22.0.4 分叉) 的协商函数返回接口版本 0,
 *     新版 Vulkan loader (steamrt4, 1.4.x) 对版本 0 驱动走"直接导出符号表"
 *     路径, 而该库只导出 3 个 vk_icd* 符号 -> 被静默丢弃。
 *     -> 本垫片对外协商接口版本 5, 一切调用转发真库。
 *  2) 厂商裁掉了实例扩展 VK_KHR_external_semaphore_capabilities /
 *     VK_KHR_external_fence_capabilities, 而 DXVK 会请求前者; 新 loader
 *     严格校验 "请求的实例扩展必须由某个 ICD 暴露" -> vkCreateInstance 被拒。
 *     -> 垫片在枚举时补报这两个扩展, 在 vkCreateInstance 转发前把真 ICD
 *        不认识的扩展名剔除 (它们只是能力查询扩展, 对应的查询函数本身是
 *        Vulkan 1.1 核心, 真 ICD 支持)。
 */
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>
#include <signal.h>
#include <setjmp.h>
#include <execinfo.h>
#include <fcntl.h>

#define TRACE "/home/ubuntu/gf-compat/hook-trace.log"
typedef void *(*gap_fn)(void *, const char *);
static void *real_h;
static gap_fn real_gap;

static void slog(const char *fmt, ...) {
    FILE *f = fopen(TRACE, "a");
    if (!f) return;
    va_list ap; va_start(ap, fmt);
    fprintf(f, "[shim] pid=%d ", (int)getpid());
    vfprintf(f, fmt, ap);
    fputc('\n', f);
    va_end(ap); fclose(f);
}

static void init_real(void) {
    if (real_gap) return;
    real_h = dlopen("/home/ubuntu/gf-compat/lib/libvulkan_gf.so", RTLD_NOW | RTLD_LOCAL);
    if (real_h) real_gap = (gap_fn)dlsym(real_h, "vk_icdGetInstanceProcAddr");
}

/* ---- 真 ICD 的实例扩展集合 (缓存) ---- */
typedef struct { char name[256]; uint32_t ver; } vk_ext;
static vk_ext *real_exts;
static uint32_t real_ext_count;

typedef int32_t (*enumext_fn)(void *, uint32_t *, void *);
static enumext_fn real_enumext(void) {
    return (enumext_fn)(real_gap ? real_gap(NULL, "vkEnumerateInstanceExtensionProperties") : NULL);
}
static void ensure_real_exts(void) {
    if (real_exts) return;
    enumext_fn f = real_enumext();
    if (!f) return;
    uint32_t n = 0;
    if (f(NULL, &n, NULL) != 0 || n == 0) return;
    vk_ext *p = malloc(n * sizeof(vk_ext));
    if (!p) return;
    if (f(NULL, &n, p) == 0) { real_exts = p; real_ext_count = n; }
    else free(p);
}
static int real_has_ext(const char *name) {
    ensure_real_exts();
    for (uint32_t i = 0; i < real_ext_count; i++)
        if (strcmp(real_exts[i].name, name) == 0) return 1;
    return 0;
}

static const char *FAKE_EXTS[] = {
    "VK_KHR_external_semaphore_capabilities",
    "VK_KHR_external_fence_capabilities",
};
#define N_FAKE_EXTS 2

int32_t vk_icdNegotiateLoaderICDInterfaceVersion(uint32_t *pSupported) {
    if (!pSupported) return -3;
    if (*pSupported > 5) *pSupported = 5;
    return 0;
}

static int32_t my_enumext(void *inst, uint32_t *cnt, void *props) {
    enumext_fn real = real_enumext();
    if (!real || !cnt) return -1;
    uint32_t n = 0;
    int32_t r = real(inst, &n, NULL);
    if (r != 0) { *cnt = n; return r; }
    uint32_t total = n + N_FAKE_EXTS;
    if (!props) { *cnt = total; return 0; }
    uint32_t cap = *cnt;
    *cnt = total;
    if (cap < total) return 5; /* VK_INCOMPLETE */
    vk_ext *p = props;
    r = real(inst, &n, p);
    if (r != 0) return r;
    for (uint32_t i = 0; i < N_FAKE_EXTS; i++) {
        strncpy(p[n + i].name, FAKE_EXTS[i], 255);
        p[n + i].name[255] = 0;
        p[n + i].ver = 1;
    }
    return 0;
}

typedef int32_t (*createinstance_fn)(const void *, const void *, void *);
static void *last_instance;
static int32_t my_createinstance(const void *ci_, const void *alloc, void *pinst) {
    /* VkInstanceCreateInfo 布局: sType, pNext, flags, pAppInfo,
       enabledLayerCount, ppEnabledLayerNames, enabledExtCount, ppEnabledExtNames */
    struct ci { uint32_t stype; const void *next; uint32_t flags; const void *app;
                uint32_t lc; const char *const *ln; uint32_t ec; const char *const *en; } *ci = (struct ci *)ci_;
    createinstance_fn real = (createinstance_fn)(real_gap ? real_gap(NULL, "vkCreateInstance") : NULL);
    if (!real) return -1;
    ensure_real_exts();
    const char **kept = NULL;
    struct ci ci2 = *ci;
    if (ci->ec && ci->en) {
        kept = malloc(ci->ec * sizeof(char *));
        if (kept) {
            uint32_t k = 0;
            for (uint32_t i = 0; i < ci->ec; i++) {
                if (real_has_ext(ci->en[i])) kept[k++] = ci->en[i];
                else slog("strip unsupported instance ext: %s", ci->en[i]);
            }
            ci2.ec = k;
            ci2.en = kept;
        }
    }
    int32_t r = real(&ci2, alloc, pinst);
    if (r == 0 && pinst) last_instance = *(void **)pinst;
    free(kept);
    return r;
}

/* ---- vkCreateDevice 崩溃守卫: 段错误时抓回溯 ---- */
static sigjmp_buf cd_jb;
static volatile sig_atomic_t cd_guarding;
static void cd_segv(int sig) {
    if (cd_guarding) siglongjmp(cd_jb, 1);
}
/* ---- 真 ICD 的设备扩展集合 (缓存, 用于剔除应用请求中不支持的) ---- */
static vk_ext *dev_exts;
static uint32_t dev_ext_count;
typedef int32_t (*enumdevext_fn)(void *, const char *, uint32_t *, void *);
static void ensure_dev_exts(void *physdev) {
    if (dev_exts) return;
    enumdevext_fn f = (enumdevext_fn)(real_gap ? real_gap(last_instance, "vkEnumerateDeviceExtensionProperties") : NULL);
    if (!f) f = (enumdevext_fn)(real_gap ? real_gap(NULL, "vkEnumerateDeviceExtensionProperties") : NULL);
    if (!f) { slog("DE no-fn"); return; }
    uint32_t n = 0;
    int32_t r = f(physdev, NULL, &n, NULL);
    slog("DE enum ret=%d n=%u", r, n);
    if (r != 0 || n == 0) return;
    vk_ext *p = malloc(n * sizeof(vk_ext));
    if (!p) return;
    if (f(physdev, NULL, &n, p) == 0) { dev_exts = p; dev_ext_count = n; }
    else free(p);
}
static int dev_has_ext(const char *name) {
    for (uint32_t i = 0; i < dev_ext_count; i++)
        if (strcmp(dev_exts[i].name, name) == 0) return 1;
    return 0;
}

typedef int32_t (*createdev_fn)(void *, const void *, const void *, void *);
static int32_t my_createdevice(void *physdev, const void *ci, const void *alloc, void *pdev) {
    slog("CD step0 physdev=%p ci=%p inst=%p", physdev, ci, last_instance);
    struct sigaction sa, old;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = cd_segv;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGSEGV, &sa, &old);
    cd_guarding = 1;
    createdev_fn real = (createdev_fn)(real_gap ? real_gap(last_instance, "vkCreateDevice") : NULL);
    if (!real) real = (createdev_fn)(real_gap ? real_gap(NULL, "vkCreateDevice") : NULL);
    if (!real) { cd_guarding = 0; sigaction(SIGSEGV, &old, NULL); slog("CD no-real"); return -1; }
    ensure_dev_exts(physdev);
    slog("CD step2 dev_exts=%u", dev_ext_count);
    /* VkDeviceCreateInfo: 剔除真 ICD 不支持的设备扩展 */
    struct dci { uint32_t stype; const void *next; uint32_t flags; uint32_t qcic;
                 const void *qcis; uint32_t lc; const char *const *ln;
                 uint32_t ec; const char *const *en; const void *feat; } *dci = (struct dci *)ci;
    slog("CD step3 ec=%u en=%p", dci->ec, (void *)dci->en);
    const char **dkept = NULL;
    struct dci dci2 = *dci;
    if (dev_exts && dci->ec && dci->en) {
        dkept = malloc((dci->ec + 1) * sizeof(char *));   /* +1 留给 dmabuf 扩展 */
        if (dkept) {
            uint32_t k = 0;
            for (uint32_t i = 0; i < dci->ec; i++) {
                if (dev_has_ext(dci->en[i])) dkept[k++] = dci->en[i];
                else slog("strip unsupported DEVICE ext: %s", dci->en[i]);
            }
            /* 应用未主动请求时补上 dmabuf 导出能力（gf 支持），供后续低位映射 */
            if (dev_has_ext("VK_KHR_external_memory_fd")) {
                int has = 0;
                for (uint32_t i = 0; i < k; i++)
                    if (!strcmp(dkept[i], "VK_KHR_external_memory_fd")) { has = 1; break; }
                if (!has) {
                    dkept[k++] = "VK_KHR_external_memory_fd";
                    slog("add VK_KHR_external_memory_fd for low-4G mapping");
                }
            }
            dci2.ec = k;
            dci2.en = dkept;
        }
    }
    slog("vkCreateDevice enter (exts %u -> %u)", dci->ec, dci2.ec);
    if (sigsetjmp(cd_jb, 1)) {
        cd_guarding = 0;
        sigaction(SIGSEGV, &old, NULL);
        slog("vkCreateDevice CRASHED, backtrace:");
        void *bt[64];
        int n = backtrace(bt, 64);
        char **sy = backtrace_symbols(bt, n);
        if (sy) { for (int i = 0; i < n; i++) slog("  #%d %s", i, sy[i]); free(sy); }
        return -4; /* VK_ERROR_DEVICE_LOST */
    }
    int32_t r = real(physdev, &dci2, alloc, pdev);
    cd_guarding = 0;
    sigaction(SIGSEGV, &old, NULL);
    free(dkept);
    slog("vkCreateDevice exit ret=%d", r);
    return r;
}

/* loader 会通过 gap(NULL,"vkGetInstanceProcAddr") 拿 ICD 自己的分发函数,
   之后所有实例/设备函数查询都走它 -> 必须在这里也拦截 */
/* 映射拦截函数定义在文件后部，此处前置声明 */
static int32_t my_allocmem(void *dev, const void *ai, const void *alloc, void **out);
static int32_t my_mapmem(void *dev, void *mem, uint64_t off, uint64_t size, uint32_t flags, void **pp);
static void    my_unmapmem(void *dev, void *mem);
static void    my_freemem(void *dev, void *mem, const void *alloc);

static void *my_gap_dispatch(void *instance, const char *name) {
    if (!name) return NULL;
    if (instance == NULL || strcmp(name, "vkCreateDevice") == 0 || strcmp(name, "vkGetInstanceProcAddr") == 0)
        slog("gapq inst=%s name=%s", instance ? "I" : "NULL", name);
    if (strcmp(name, "vkCreateDevice") == 0) return (void *)my_createdevice;
    if (strcmp(name, "vkCreateInstance") == 0) return (void *)my_createinstance;
    if (strcmp(name, "vkEnumerateInstanceExtensionProperties") == 0) return (void *)my_enumext;
    /* 显存映射重定位（32 位游戏需要低 4GB 指针） */
    if (strcmp(name, "vkAllocateMemory") == 0) return (void *)my_allocmem;
    if (strcmp(name, "vkMapMemory")      == 0) return (void *)my_mapmem;
    if (strcmp(name, "vkUnmapMemory")    == 0) return (void *)my_unmapmem;
    if (strcmp(name, "vkFreeMemory")     == 0) return (void *)my_freemem;
    return real_gap ? real_gap(instance, name) : NULL;
}

void *vk_icdGetInstanceProcAddr(void *instance, const char *name) {
    init_real();
    if (!name) return NULL;
    if (strcmp(name, "vkGetInstanceProcAddr") == 0) return (void *)my_gap_dispatch;
    return my_gap_dispatch(instance, name);
}

/* ============================================================
 * 显存映射重定位：把 vkMapMemory 的结果搬到 2~4GB
 *
 * 为什么：32 位游戏经 WoW64 调 64 位 gf 驱动时，驱动返回的 CPU 指针在高地址
 * (>4GB)，wine 的 thunk 交不到 32 位代码。gf 支持 dmabuf 导出，
 * 我们把同一块显存重新 mmap 到低 4GB 即可。
 *
 * 为什么锚定 2~4GB 而不是用 MAP_32BIT：
 *   MAP_32BIT 语义是"进程前 2GB"，实测游戏进程低 2GB 已被占满
 *   (2043/2048MB)，大纹理一映射就 ENOMEM；而 2~4GB 区间基本是空的。
 *
 * 好处：不改动任何驱动文件，因此其他游戏（如 64 位原生的刀塔霸业）
 * 仍走系统原版驱动，完全不受影响。
 * ============================================================ */
#include <sys/mman.h>
#include <pthread.h>

#define LOW4_START      0x80000000ULL
#define LOW4_END        0x100000000ULL
#define ST_EXPORT_ALLOC 1000072000u
#define ST_GET_FD       1000072001u
#define HANDLE_DMA_BUF  0x00000200

typedef struct { int32_t sType; const void *pNext; uint64_t size; uint32_t typeIdx; } MA_t;
typedef struct { int32_t sType; const void *pNext; uint32_t handleTypes; } ExpMA_t;
typedef struct { int32_t sType; const void *pNext; void *mem; uint32_t handleType; } GetFd_t;

typedef int32_t (*allocmem_fn)(void *, const void *, const void *, void **);
typedef int32_t (*mapmem_fn)  (void *, void *, uint64_t, uint64_t, uint32_t, void **);
typedef void    (*unmapmem_fn)(void *, void *);
typedef void    (*freemem_fn) (void *, void *, const void *);
typedef int32_t (*getfd_fn)   (void *, const void *, int *);

static allocmem_fn R_alloc(void) { return (allocmem_fn)(real_gap ? real_gap(last_instance, "vkAllocateMemory") : NULL); }
static mapmem_fn   R_map(void)   { return (mapmem_fn)  (real_gap ? real_gap(last_instance, "vkMapMemory")      : NULL); }
static unmapmem_fn R_unmap(void) { return (unmapmem_fn)(real_gap ? real_gap(last_instance, "vkUnmapMemory")    : NULL); }
static freemem_fn  R_free(void)  { return (freemem_fn) (real_gap ? real_gap(last_instance, "vkFreeMemory")     : NULL); }
static getfd_fn    R_getfd(void) { return (getfd_fn)   (real_gap ? real_gap(last_instance, "vkGetMemoryFdKHR") : NULL); }

typedef struct MapRec { void *mem; void *low; size_t size; int fd; struct MapRec *next; } MapRec;
static MapRec *g_recs;
static pthread_mutex_t g_mlk = PTHREAD_MUTEX_INITIALIZER;

static void rec_add(void *m, void *low, size_t s, int fd) {
    MapRec *r = calloc(1, sizeof(*r));
    if (!r) return;
    r->mem = m; r->low = low; r->size = s; r->fd = fd;
    pthread_mutex_lock(&g_mlk); r->next = g_recs; g_recs = r; pthread_mutex_unlock(&g_mlk);
}
static MapRec *rec_find(void *m) {
    pthread_mutex_lock(&g_mlk);
    for (MapRec *r = g_recs; r; r = r->next) if (r->mem == m) { pthread_mutex_unlock(&g_mlk); return r; }
    pthread_mutex_unlock(&g_mlk); return NULL;
}
static void rec_del(MapRec *t) {
    pthread_mutex_lock(&g_mlk);
    for (MapRec **pp = &g_recs; *pp; pp = &(*pp)->next)
        if (*pp == t) { *pp = t->next; break; }
    pthread_mutex_unlock(&g_mlk);
    free(t);
}

/* 依次尝试多组 hint，把映射落到 4GB 以内 */
static void *map_low4(size_t size, int prot, int fd, off_t off) {
    for (int i = 0; i < 10; i++) {
        void *hint = (void *)(LOW4_START + (uint64_t)i * 0x8000000ULL);
        void *p = mmap(hint, size, prot, MAP_SHARED, fd, off);
        if (p == MAP_FAILED) continue;
        if ((uintptr_t)p < LOW4_END) return p;
        munmap(p, size);
    }
    return MAP_FAILED;
}

static int32_t my_allocmem(void *dev, const void *ai_, const void *alloc, void **out) {
    allocmem_fn real = R_alloc();
    if (!real) return -1;
    const MA_t *ai = (const MA_t *)ai_;
    /* 声明"可导出"，为后续 dmabuf 重映射做准备 */
    ExpMA_t exp;
    memset(&exp, 0, sizeof(exp));
    exp.sType = ST_EXPORT_ALLOC;
    exp.pNext = ai->pNext;
    exp.handleTypes = HANDLE_DMA_BUF;
    MA_t a2 = *ai;
    a2.pNext = &exp;
    return real(dev, &a2, alloc, out);
}

static int32_t my_mapmem(void *dev, void *mem, uint64_t off, uint64_t size, uint32_t flags, void **pp) {
    mapmem_fn real = R_map();
    if (!real) return -1;
    void *rp = NULL;
    int32_t r = real(dev, mem, off, size, flags, &rp);
    if (r != 0) return r;
    if (rp && (uintptr_t)rp < LOW4_END) { *pp = rp; return r; }   /* 已在低 4GB，无需处理 */

    getfd_fn gfd = R_getfd();
    if (!gfd) { *pp = rp; return r; }
    GetFd_t gi;
    memset(&gi, 0, sizeof(gi));
    gi.sType = ST_GET_FD;
    gi.mem = mem;
    gi.handleType = HANDLE_DMA_BUF;
    int fd = -1;
    if (gfd(dev, &gi, &fd) != 0 || fd < 0) { *pp = rp; return r; }

    uint64_t sz = (size == (uint64_t)-1) ? 0 : size;
    if (sz == 0) { close(fd); *pp = rp; return r; }

    void *lp = map_low4((size_t)sz, PROT_READ | PROT_WRITE, fd, (off_t)off);
    if (lp == MAP_FAILED) { close(fd); *pp = rp; return r; }

    rec_add(mem, lp, (size_t)sz, fd);
    slog("map 重定位 0x%p -> 0x%p (%llu B)", rp, lp, (unsigned long long)sz);
    *pp = lp;
    return r;
}

static void my_unmapmem(void *dev, void *mem) {
    MapRec *r = rec_find(mem);
    if (r) { munmap(r->low, r->size); close(r->fd); rec_del(r); }
    unmapmem_fn real = R_unmap();
    if (real) real(dev, mem);
}

static void my_freemem(void *dev, void *mem, const void *alloc) {
    MapRec *r = rec_find(mem);
    if (r) { munmap(r->low, r->size); close(r->fd); rec_del(r); }
    freemem_fn real = R_free();
    if (real) real(dev, mem, alloc);
}

void *vk_icdGetPhysicalDeviceProcAddr(void *dev, const char *name) {
    init_real();
    if (!real_h) return NULL;
    void *(*f)(void *, const char *) = (void *(*)(void *, const char *))dlsym(real_h, "vk_icdGetPhysicalDeviceProcAddr");
    return f ? f(dev, name) : NULL;
}
