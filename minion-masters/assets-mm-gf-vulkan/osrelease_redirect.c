#define _GNU_SOURCE
#include <stdarg.h>
#include <stddef.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>

#define FAKE "/home/ubuntu/gf-compat/os-release"
#define TARGET "/etc/os-release"
#define TRACE "/home/ubuntu/gf-compat/hook-trace.log"

typedef int (*openat_fn)(int, const char *, int, ...);
typedef int (*open_fn)(const char *, int, ...);
typedef FILE *(*fopen_fn)(const char *, const char *);

static void trace(const char *what, const char *p) {
    FILE *f = fopen(TRACE, "a");
    if (!f) return;
    char comm[64] = "?";
    FILE *c = fopen("/proc/self/comm", "r");
    if (c) { if (fgets(comm, sizeof comm, c)) comm[strcspn(comm,"\n")] = 0; fclose(c); }
    fprintf(f, "%s pid=%d comm=%s path=%s\n", what, (int)getpid(), comm, p ? p : "(null)");
    fclose(f);
}

static int in_game(void);
static void gtrace(const char *fmt, ...);

static int is_target(const char *p) {
    if (!p) return 0;
    return strcmp(p, TARGET) == 0;
}
static int has_osrelease(const char *p) {
    return p && (strstr(p, "os-release") || strstr(p, "libvulkan") || strstr(p, "vulkan_gf") || strstr(p, "/run/host/usr/lib"));
}

__attribute__((constructor)) static void probe_init(void) {
    trace("[osr] HOOK LOADED", "-");
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
    if (has_osrelease(path)) trace("[osr] openat", path);
    if (path && (strstr(path, "/etc/") || strstr(path, "/run/host") || strstr(path, "/dev/dri") || strstr(path, "renderD"))) gtrace("openat %s", path);
    if (is_target(path)) path = FAKE;
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
    if (has_osrelease(path)) trace("[osr] openat64", path);
    if (is_target(path)) path = FAKE;
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
    if (has_osrelease(path)) trace("[osr] open", path);
    if (is_target(path)) path = FAKE;
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
    if (has_osrelease(path)) trace("[osr] open64", path);
    if (is_target(path)) path = FAKE;
    return real(path, flags, mode);
}

FILE *fopen(const char *path, const char *mode) {
    static fopen_fn real = NULL;
    if (!real) real = (fopen_fn)dlsym(RTLD_NEXT, "fopen");
    if (has_osrelease(path)) trace("[osr] fopen", path);
    if (is_target(path)) path = FAKE;
    return real(path, mode);
}

FILE *fopen64(const char *path, const char *mode) {
    static fopen_fn real = NULL;
    if (!real) real = (fopen_fn)dlsym(RTLD_NEXT, "fopen64");
    if (has_osrelease(path)) trace("[osr] fopen64", path);
    if (is_target(path)) path = FAKE;
    return real(path, mode);
}

#include <sys/stat.h>
#include <errno.h>
typedef int (*xstat_fn)(const char *, struct stat *);
int stat(const char *p, struct stat *b) {
    static xstat_fn real = NULL;
    if (!real) real = (xstat_fn)dlsym(RTLD_NEXT, "stat");
    int r = real(p, b);
    if (p && (strstr(p,"libvulkan") || strstr(p,"/run/host/usr") || strstr(p,"os-release"))) {
        FILE *f = fopen(TRACE, "a");
        if (f) { fprintf(f, "[osr] stat pid=%d path=%s ret=%d errno=%d\n", (int)getpid(), p, r, r<0?errno:0); fclose(f); }
    }
    return r;
}
typedef int (*access_fn)(const char *, int);
int access(const char *p, int m) {
    static access_fn real = NULL;
    if (!real) real = (access_fn)dlsym(RTLD_NEXT, "access");
    int r = real(p, m);
    if (p && (strstr(p,"libvulkan") || strstr(p,"/run/host/usr") || strstr(p,"os-release"))) {
        FILE *f = fopen(TRACE, "a");
        if (f) { fprintf(f, "[osr] access pid=%d path=%s ret=%d errno=%d\n", (int)getpid(), p, r, r<0?errno:0); fclose(f); }
    }
    return r;
}

typedef void *(*dlopen_fn)(const char *, int);
void *dlopen(const char *file, int mode) {
    static dlopen_fn real = NULL;
    static char *(*dlerr)(void) = NULL;
    if (!real) { real = (dlopen_fn)dlsym(RTLD_NEXT, "dlopen"); dlerr = (char *(*)(void))dlsym(RTLD_NEXT, "dlerror"); }
    void *r = real(file, mode);
    if (file && (strstr(file, "vulkan_gf") || strstr(file, "libSC") || strstr(file, "libvulkan"))) {
        FILE *f = fopen(TRACE, "a");
        if (f) { fprintf(f, "[osr] dlopen pid=%d file=%s ret=%p err=%s\n", (int)getpid(), file, r, r ? "-" : (dlerr ? dlerr() : "?")); fclose(f); }
    }
    return r;
}

#include <stdint.h>
static int in_game(void) {
    static int cached = -1;
    if (cached < 0) {
        char buf[4096]; FILE *f = fopen("/proc/self/cmdline", "r");
        cached = 0;
        if (f) { size_t n = fread(buf, 1, sizeof buf - 1, f); fclose(f);
                 for (size_t i = 0; i < n; i++) if (buf[i] == 0) buf[i] = ' ';
                 buf[n] = 0;
                 if (strstr(buf, "MinionMasters")) cached = 1; }
    }
    return cached;
}
static void gtrace(const char *fmt, ...) {
    if (!in_game()) return;
    FILE *f = fopen(TRACE, "a");
    if (!f) return;
    va_list ap; va_start(ap, fmt);
    fprintf(f, "[osr][game] pid=%d ", (int)getpid());
    vfprintf(f, fmt, ap);
    fputc('\n', f);
    va_end(ap); fclose(f);
}

__attribute__((constructor(200))) static void icd_selftest(void) {
    if (!in_game()) return;
    void *h = dlopen("/home/ubuntu/gf-compat/lib/libvulkan_gf.so", 1 /*RTLD_LAZY*/);
    if (!h) { gtrace("selftest dlopen FAILED: %s", dlerror()); return; }
    int32_t (*neg)(uint32_t *) = (int32_t (*)(uint32_t *))dlsym(h, "vk_icdNegotiateLoaderICDInterfaceVersion");
    uint32_t ver = 0;
    if (neg) neg(&ver);
    void *(*gap)(void *, const char *) = (void *(*)(void *, const char *))dlsym(h, "vk_icdGetInstanceProcAddr");
    gtrace("selftest negotiate=%s ver=%u gap=%p", neg ? "ok" : "MISSING", ver, (void *)gap);
    if (!gap) return;
    int32_t (*enumext)(void *, uint32_t *, void *) = (int32_t (*)(void *, uint32_t *, void *))gap(NULL, "vkEnumerateInstanceExtensionProperties");
    if (!enumext) { gtrace("selftest enumext lookup FAILED"); return; }
    uint32_t cnt = 0;
    int32_t r = enumext(NULL, &cnt, NULL);
    gtrace("selftest enumext ret=%d count=%u", r, cnt);
}

/* ==== 低 4GB 重映射: 游戏是 32 位 (WoW64 thunk 只能接 <4GB 指针),
   而 gf 的 vkMapMemory 落在 >4GB; MAP_32BIT (<2GB) 只剩 ~321MB 会失败。
   2~4GB 完全空闲 -> 把失败/高位的设备映射重定向到这里。 ==== */
#ifndef MAP_32BIT
#define MAP_32BIT 0x40
#endif
#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif
#include <sched.h>
#include <sys/mman.h>

typedef void *(*mmap64_fn)(void *, size_t, int, int, int, off64_t);
typedef int (*munmap_fn)(void *, size_t);
static unsigned long low4_pos = 0x90000000UL;
static volatile int low4_lock;

static void *low4_alloc(size_t len) {
    len = (len + 0xFFFFUL) & ~0xFFFFUL;
    while (__sync_lock_test_and_set(&low4_lock, 1)) sched_yield();
    unsigned long a = low4_pos;
    if (a + len >= 0xFF000000UL) { __sync_lock_release(&low4_lock); return NULL; }
    low4_pos = a + len;
    __sync_lock_release(&low4_lock);
    return (void *)a;
}

static void *mmap_common(void *(*real)(void *, size_t, int, int, int, off64_t),
                         void *addr, size_t len, int prot, int flags, int fd, off64_t off) {
    if (!in_game()) return real(addr, len, prot, flags, fd, off);
    if (flags & MAP_32BIT) {
        void *r = real(addr, len, prot, flags, fd, off);
        if (r != MAP_FAILED) return r;
        void *h = low4_alloc(len);
        if (h) {
            void *r2 = real(h, len, prot, ((flags & ~MAP_32BIT) | MAP_FIXED_NOREPLACE), fd, off);
            if (r2 != MAP_FAILED) { gtrace("low4 map32bit-fallback %zu -> %p", len, r2); return r2; }
        }
        return MAP_FAILED;
    }
    if (addr == NULL && fd >= 0 && (flags & MAP_SHARED)) {
        void *r = real(addr, len, prot, flags, fd, off);
        if (r != MAP_FAILED && (unsigned long)r >= 0x100000000UL) {
            static munmap_fn un = NULL;
            if (!un) un = (munmap_fn)dlsym(RTLD_NEXT, "munmap");
            un(r, len);
            void *h = low4_alloc(len);
            if (h) {
                void *r2 = real(h, len, prot, flags | MAP_FIXED_NOREPLACE, fd, off);
                if (r2 != MAP_FAILED) { gtrace("low4 highmap-reloc %zu %p -> %p", len, r, r2); return r2; }
            }
            return real(NULL, len, prot, flags, fd, off);
        }
        return r;
    }
    return real(addr, len, prot, flags, fd, off);
}

void *mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off) {
    static mmap64_fn real = NULL;
    if (!real) real = (mmap64_fn)dlsym(RTLD_NEXT, "mmap");
    return mmap_common(real, addr, len, prot, flags, fd, (off64_t)off);
}
void *mmap64(void *addr, size_t len, int prot, int flags, int fd, off64_t off) {
    static mmap64_fn real = NULL;
    if (!real) real = (mmap64_fn)dlsym(RTLD_NEXT, "mmap64");
    return mmap_common(real, addr, len, prot, flags, fd, off);
}
