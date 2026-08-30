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
static pthread_mutex_t low4_lock = PTHREAD_MUTEX_INITIALIZER;

static void *(*real_mmap)(void *, size_t, int, int, int, off_t) = NULL;
static void *(*real_mmap64)(void *, size_t, int, int, int, off64_t) = NULL;

/* 只处理 GPU 相关的 fd：DRM 字符设备(主设备号 226) 或 dma-buf 匿名文件 */
static int is_gpu_fd(int fd) {
    struct stat st;
    if (fd < 0) return 0;
    if (fstat(fd, &st) != 0) return 0;
    if (S_ISCHR(st.st_mode) && major(st.st_rdev) == 226) return 1;
    if (S_ISREG(st.st_mode) && st.st_nlink == 0) return 1;   /* dma-buf */
    return 0;
}

/* 在 low4 区内做 MAP_FIXED 映射（bump 分配，不回收——显存映射次数有限） */
static void *low4_map(size_t len, int prot, int flags, int fd, off64_t off) {
    size_t need = (len + 0xffffUL) & ~(size_t)0xffffUL;
    void *p = MAP_FAILED;
    pthread_mutex_lock(&low4_lock);
    char *base = low4_pos;
    if ((uintptr_t)base + need <= LOW4_END) {
        p = real_mmap64(base, len, prot, (flags & ~MAP_FIXED) | MAP_FIXED, fd, off);
        if (p != MAP_FAILED) {
            if ((uintptr_t)p + need <= LOW4_END) low4_pos = (char *)p + need;
            else { munmap(p, len); p = MAP_FAILED; }
        }
    }
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
