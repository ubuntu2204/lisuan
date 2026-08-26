#define _GNU_SOURCE
#include <stdarg.h>
#include <stddef.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <stdio.h>

/* 将 /etc/os-release 的打开重定向到伪造的 Ubuntu 文件，
   骗过 libvulkan_gf.so 的发行版白名单检查。 */
#define FAKE "/home/ubuntu/gf-compat/os-release"
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

/* ---- 诊断: mmap/ioctl 失败监控 ---- */
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

void *mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off) {
    static void *(*real)(void *, size_t, int, int, int, off_t) = NULL;
    if (!real) real = dlsym(RTLD_NEXT, "mmap");
    void *r = real(addr, len, prot, flags, fd, off);
    if (r == MAP_FAILED)
        fprintf(stderr, "[mm] mmap FAILED addr=%p len=%zu prot=0x%x flags=0x%x fd=%d off=%ld errno=%d(%s)\n",
                addr, len, prot, flags, fd, (long)off, errno, strerror(errno));
    return r;
}

void *mmap64(void *addr, size_t len, int prot, int flags, int fd, off64_t off) {
    static void *(*real)(void *, size_t, int, int, int, off64_t) = NULL;
    if (!real) real = dlsym(RTLD_NEXT, "mmap64");
    void *r = real(addr, len, prot, flags, fd, off);
    if (r == MAP_FAILED)
        fprintf(stderr, "[mm] mmap64 FAILED addr=%p len=%zu prot=0x%x flags=0x%x fd=%d off=%lld errno=%d(%s)\n",
                addr, len, prot, flags, fd, (long long)off, errno, strerror(errno));
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
