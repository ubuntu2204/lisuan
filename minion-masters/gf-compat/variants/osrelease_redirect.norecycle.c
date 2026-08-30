#define _GNU_SOURCE
#include <stdarg.h>
#include <stddef.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <stdio.h>

/* 将 /etc/os-release 的打开重定向到伪造的 Ubuntu 文件，
   骗过 libvulkan_gf.so 的发行版白名单检查。 */
#define FAKE "/home/ubuntu/project/lisuan/minion-masters/gf-compat/os-release"
#define TARGET "/etc/os-release"

typedef int (*openat_fn)(int, const char *, int, ...);
typedef int (*open_fn)(const char *, int, ...);
typedef FILE *(*fopen_fn)(const char *, const char *);

static int is_target(const char *p) {
    if (!p) return 0;
    const char *t = TARGET;
    while (*t && *p == *t) { p++; t++; }
    return *t == 0 && *p == 0;
}

int openat(int dirfd, const char *path, int flags, ...) {
    static openat_fn real = NULL;
    if (!real) real = (openat_fn)dlsym(RTLD_NEXT, "openat");
    mode_t mode = 0;
    if (flags & (O_CREAT | O_TMPFILE)) {
        va_list ap; va_start(ap, flags);
        mode = va_arg(ap, mode_t);
        va_end(ap);
    }
    if (is_target(path)) {
        fprintf(stderr, "[osr] redirect %s -> %s\n", TARGET, FAKE);
        path = FAKE;
    }
    return real(dirfd, path, flags, mode);
}

int openat64(int dirfd, const char *path, int flags, ...) {
    static openat_fn real = NULL;
    if (!real) real = (openat_fn)dlsym(RTLD_NEXT, "openat64");
    mode_t mode = 0;
    if (flags & (O_CREAT | O_TMPFILE)) {
        va_list ap; va_start(ap, flags);
        mode = va_arg(ap, mode_t);
        va_end(ap);
    }
    if (is_target(path)) {
        fprintf(stderr, "[osr] redirect %s -> %s\n", TARGET, FAKE);
        path = FAKE;
    }
    return real(dirfd, path, flags, mode);
}

int open(const char *path, int flags, ...) {
    static open_fn real = NULL;
    if (!real) real = (open_fn)dlsym(RTLD_NEXT, "open");
    mode_t mode = 0;
    if (flags & (O_CREAT | O_TMPFILE)) {
        va_list ap; va_start(ap, flags);
        mode = va_arg(ap, mode_t);
        va_end(ap);
    }
    if (is_target(path)) {
        fprintf(stderr, "[osr] redirect %s -> %s\n", TARGET, FAKE);
        path = FAKE;
    }
    return real(path, flags, mode);
}

int open64(const char *path, int flags, ...) {
    static open_fn real = NULL;
    if (!real) real = (open_fn)dlsym(RTLD_NEXT, "open64");
    mode_t mode = 0;
    if (flags & (O_CREAT | O_TMPFILE)) {
        va_list ap; va_start(ap, flags);
        mode = va_arg(ap, mode_t);
        va_end(ap);
    }
    if (is_target(path)) {
        fprintf(stderr, "[osr] redirect %s -> %s\n", TARGET, FAKE);
        path = FAKE;
    }
    return real(path, flags, mode);
}

FILE *fopen(const char *path, const char *mode) {
    static fopen_fn real = NULL;
    if (!real) real = (fopen_fn)dlsym(RTLD_NEXT, "fopen");
    if (is_target(path)) {
        fprintf(stderr, "[osr] redirect %s -> %s\n", TARGET, FAKE);
        path = FAKE;
    }
    return real(path, mode);
}

FILE *fopen64(const char *path, const char *mode) {
    static fopen_fn real = NULL;
    if (!real) real = (fopen_fn)dlsym(RTLD_NEXT, "fopen64");
    if (is_target(path)) {
        fprintf(stderr, "[osr] redirect %s -> %s\n", TARGET, FAKE);
        path = FAKE;
    }
    return real(path, mode);
}

/* ============================================================
 * 显存高位映射重定位
 *
 * 背景：32 位游戏经 Proton WoW64 调 64 位 gf 驱动时，驱动 vkMapMemory
 * 返回的 CPU 指针在高地址（>4GB），wine 的 thunk 交不到 32 位代码，
 * 于是映射"失败"，DXVK 报 VK_ERROR_OUT_OF_HOST_MEMORY。
 *
 * 做法：在 mmap 层把 GPU 相关映射重定位到 2~4GB 区间。
 *
 * 为什么锚定 2~4GB：低 2GB 已被游戏占满（实测 2043/2048MB），
 * 而 2~4GB 基本是空的。MAP_32BIT 只能到低 2GB，那里放不下大纹理。
 *
 * 关键点：这里【不要求调用方带 MAP_32BIT】。
 *   - 旧方案依赖给系统驱动打 MAP_32BIT 补丁 -> 那是全局的，会连累
 *     64 位原生的游戏（如刀塔霸业，它本不需要任何补丁）。
 *   - 本方案按"结果地址 >= 4GB 且是 GPU 映射"来判断，因此驱动可以
 *     保持原版，补丁只作用于配了 LD_PRELOAD 的那个游戏。
 * ============================================================ */
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <stdint.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#define LOW4_START 0x90000000ULL   /* 2.25GB */
#define LOW4_END   0x100000000ULL  /* 4GB    */

static char *low4_pos = (char *)LOW4_START;

/* 递归锁：munmap 钩子回收池内块时会再次进入本锁 */
static pthread_mutex_t low4_lock;   /* 构造期初始化为递归锁，见 osr_resolve_symbols */

/* ============================================================
 * 池内空闲块回收
 *
 * 原实现是纯 bump 分配（只增不减）。实测对战中显存映射被反复申请/释放，
 * 1.75GB 的池被单调吃光后 reloc64 FAILED 频出并崩溃——2026-08-31 一次
 * 运行内 FAILED 达 2299 次（4KB 小块 2270 次，1~16MB 大块各 5 次，
 * 后者是同一次分配在逐级降级重试）。
 *
 * 改为：munmap 命中池内块时归还并合并相邻块；分配优先复用空闲块。
 * ============================================================ */
#define MAX_FREE_BLOCKS 8192

typedef struct {
    uintptr_t start;
    size_t len;
} blk_t;

static blk_t g_free[MAX_FREE_BLOCKS];
static int g_nfree = 0;

static unsigned long g_stat_take, g_stat_bump, g_stat_return, g_stat_fail;

/* 对齐粒度按块大小自适应：>=64KB 用 64KB，小块用 4KB。
   旧代码一律 64KB 对齐，4KB 映射会浪费 16 倍地址空间。 */
static size_t align_up(size_t len) {
    size_t mask = (len >= (1UL << 16)) ? 0xffffUL : 0xfffUL;
    return (len + mask) & ~mask;
}

/* 归还一段池内地址，并与相邻空闲块合并 */
static void pool_return(uintptr_t start, size_t len) {
    if (!len) return;
    for (int i = 0; i < g_nfree; i++) {
        if (g_free[i].start + g_free[i].len == start) {   /* 接在前块之后 */
            g_free[i].len += len;
            len = g_free[i].len;
            start = g_free[i].start;
            for (int j = 0; j < g_nfree; j++) {           /* 合并后可能又接上后块 */
                if (j != i && start + len == g_free[j].start) {
                    len += g_free[j].len;
                    g_free[j] = g_free[--g_nfree];
                    break;
                }
            }
            g_free[i].start = start;
            g_free[i].len = len;
            return;
        }
        if (start + len == g_free[i].start) {             /* 接在后块之前 */
            g_free[i].start = start;
            g_free[i].len += len;
            return;
        }
    }
    if (g_nfree < MAX_FREE_BLOCKS) {
        g_free[g_nfree].start = start;
        g_free[g_nfree].len = len;
        g_nfree++;
    }
    /* 池满则丢弃该块：退化成不回收的旧行为，但不至于崩 */
}

/* 从空闲链表取一块（best-fit，减小碎片） */
static int pool_take(size_t need, uintptr_t *out) {
    int best = -1;
    for (int i = 0; i < g_nfree; i++) {
        if (g_free[i].len >= need && (best < 0 || g_free[i].len < g_free[best].len))
            best = i;
    }
    if (best < 0) return 0;
    *out = g_free[best].start;
    g_free[best].start += need;
    g_free[best].len -= need;
    if (g_free[best].len == 0)
        g_free[best] = g_free[--g_nfree];
    return 1;
}

/* ---- 已分配块登记表 ----
 * 记录每次池内分配实际占用的地址范围。munmap 时只有【起始地址精确匹配】
 * 才回收，避免"部分释放"把仍在使用的地址误标成空闲。 */
#define MAX_ALLOC_BLOCKS 8192

static blk_t g_alloc[MAX_ALLOC_BLOCKS];
static int g_nalloc = 0;

static void alloc_add(uintptr_t start, size_t need) {
    if (g_nalloc < MAX_ALLOC_BLOCKS) {
        g_alloc[g_nalloc].start = start;
        g_alloc[g_nalloc].len = need;
        g_nalloc++;
    }
}

static int alloc_del(uintptr_t start, size_t *need_out) {
    for (int i = 0; i < g_nalloc; i++) {
        if (g_alloc[i].start == start) {
            *need_out = g_alloc[i].len;
            g_alloc[i] = g_alloc[--g_nalloc];
            return 1;
        }
    }
    return 0;
}

static void *(*real_mmap)(void *, size_t, int, int, int, off_t) = NULL;
static void *(*real_mmap64)(void *, size_t, int, int, int, off64_t) = NULL;
static int (*real_munmap)(void *, size_t) = NULL;

/* 构造期解析符号：避免在 munmap 钩子里调 dlsym——它的内部结构可能走
   malloc/munmap，存在递归风险。 */
__attribute__((constructor))
static void osr_resolve_symbols(void) {
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&low4_lock, &attr);
    pthread_mutexattr_destroy(&attr);

    real_mmap   = dlsym(RTLD_NEXT, "mmap");
    real_mmap64 = dlsym(RTLD_NEXT, "mmap64");
    real_munmap = dlsym(RTLD_NEXT, "munmap");
}

/* 只处理 GPU 相关的 fd：DRM 字符设备(主设备号 226) 或 dma-buf 匿名文件 */
static int is_gpu_fd(int fd) {
    struct stat st;
    if (fd < 0) return 0;
    if (fstat(fd, &st) != 0) return 0;
    if (S_ISCHR(st.st_mode) && major(st.st_rdev) == 226) return 1;
    if (S_ISREG(st.st_mode) && st.st_nlink == 0) return 1;   /* dma-buf */
    return 0;
}

#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif

/* 在 low4 区内映射：优先复用回收块，其次 bump。 */
static void *low4_map(size_t len, int prot, int flags, int fd, off64_t off) {
    size_t need = align_up(len);
    uintptr_t base = 0;
    void *p;

    pthread_mutex_lock(&low4_lock);

    if (0 && pool_take(need, &base)) {
        g_stat_take++;
    } else if ((uintptr_t)low4_pos + need <= LOW4_END) {
        base = (uintptr_t)low4_pos;
        low4_pos += need;
        g_stat_bump++;
    } else {
        g_stat_fail++;
        pthread_mutex_unlock(&low4_lock);
        return MAP_FAILED;
    }

    /* MAP_FIXED_NOREPLACE：目标地址已被占用时返回 EEXIST 而不是静默覆盖。
       旧代码用 MAP_FIXED 会覆盖同地址的既有映射（wine 堆等），是堆损坏隐患。 */
    p = real_mmap64((void *)base, len, prot,
                    (flags & ~MAP_FIXED) | MAP_FIXED_NOREPLACE, fd, off);

    if (p == MAP_FAILED || (uintptr_t)p != base) {
        if (p != MAP_FAILED) munmap(p, len);
        pool_return(base, need); /* 退回去，别把地址空间漏掉 */
        g_stat_fail++;
        pthread_mutex_unlock(&low4_lock);
        return MAP_FAILED;
    }

    alloc_add(base, need);
    pthread_mutex_unlock(&low4_lock);
    return p;
}

void *mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off) {
    if (!real_mmap)   real_mmap   = dlsym(RTLD_NEXT, "mmap");
    if (!real_mmap64) real_mmap64 = dlsym(RTLD_NEXT, "mmap64");
    void *r = real_mmap(addr, len, prot, flags, fd, off);
    if (r == MAP_FAILED) return r;
    if ((uintptr_t)r >= LOW4_END && fd >= 0 && (flags & MAP_SHARED) && is_gpu_fd(fd)) {
        void *p = low4_map(len, prot, flags, fd, off);
        if (p != MAP_FAILED) {
            munmap(r, len);
            fprintf(stderr, "[osr][low4] reloc %zu %p -> %p\n", len, r, p);
            return p;
        }
        fprintf(stderr, "[osr][low4] reloc FAILED len=%zu (低4GB空间不足?)\n", len);
    }
    return r;
}

void *mmap64(void *addr, size_t len, int prot, int flags, int fd, off64_t off) {
    if (!real_mmap64) real_mmap64 = dlsym(RTLD_NEXT, "mmap64");
    void *r = real_mmap64(addr, len, prot, flags, fd, off);
    if (r == MAP_FAILED) return r;
    if ((uintptr_t)r >= LOW4_END && fd >= 0 && (flags & MAP_SHARED) && is_gpu_fd(fd)) {
        void *p = low4_map(len, prot, flags, fd, off);
        if (p != MAP_FAILED) {
            munmap(r, len);
            fprintf(stderr, "[osr][low4] reloc64 %zu %p -> %p\n", len, r, p);
            return p;
        }
        fprintf(stderr, "[osr][low4] reloc64 FAILED len=%zu (低4GB空间不足?)\n", len);
    }
    return r;
}

/* 回收：释放落在池内的映射时，把地址还给空闲链表。
   只回收起始地址与登记记录精确匹配的块，部分释放一律不回收。 */
int munmap(void *addr, size_t len) {
    uintptr_t s = (uintptr_t)addr;

    if (real_munmap && s >= LOW4_START && s + len <= LOW4_END) {
        size_t need = 0;
        pthread_mutex_lock(&low4_lock);
        if (alloc_del(s, &need)) {
            pool_return(s, need);
            g_stat_return++;
        }
        pthread_mutex_unlock(&low4_lock);
    }
    return real_munmap ? real_munmap(addr, len) : 0;
}

int ioctl(int fd, unsigned long req, ...) {
    static int (*real)(int, unsigned long, ...) = NULL;
    if (!real) real = dlsym(RTLD_NEXT, "ioctl");
    va_list ap; va_start(ap, req);
    void *arg = va_arg(ap, void *);
    va_end(ap);
    int r = real(fd, req, arg);
    if (r < 0 && ((req >> 8) & 0xff) == 0x64)  /* 'd' = DRM */
        fprintf(stderr, "[mm] ioctl FAILED fd=%d req=0x%lx(nr=%lu,size=%lu) errno=%d(%s)\n",
                fd, req, req & 0xff, (req >> 16) & 0x3fff, errno, strerror(errno));
    return r;
}
