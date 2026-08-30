#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <dlfcn.h>

typedef int (*cmp_fn)(const char *, const char *);
typedef int (*ncmp_fn)(const char *, const char *, size_t);

static const char *safe(const char *s) { return s ? s : "(null)"; }

static void esc(const char *s, char *out, size_t n) {
    size_t j = 0;
    for (size_t i = 0; s && s[i] && j < n - 2; i++) {
        if (s[i] == '\r') { out[j++]='\\'; out[j++]='r'; }
        else if (s[i] == '\n') { out[j++]='\\'; out[j++]='n'; }
        else out[j++] = s[i];
    }
    out[j] = 0;
}

#include <execinfo.h>

int strcmp(const char *a, const char *b) {
    static cmp_fn real = NULL;
    if (!real) real = (cmp_fn)dlsym(RTLD_NEXT, "strcmp");
    int r = real(a, b);
    int interesting = (a && b) && (__builtin_strstr(a, "Ubuntu") || __builtin_strstr(b, "Ubuntu") ||
                      __builtin_strstr(a, "Steam") || __builtin_strstr(b, "Steam"));
    if (interesting) {
        void *bt[16];
        int n = backtrace(bt, 16);
        fprintf(stderr, "[HOOK] strcmp(\"%s\", \"%s\") = %d\n", safe(a), safe(b), r);
        backtrace_symbols_fd(bt + 1, n - 1, 2);
    }
    return r;
}

int strncmp(const char *a, const char *b, size_t n) {
    static ncmp_fn real = NULL;
    if (!real) real = (ncmp_fn)dlsym(RTLD_NEXT, "strncmp");
    int r = real(a, b, n);
    char ea[512], eb[512];
    esc(a, ea, sizeof ea); esc(b, eb, sizeof eb);
    fprintf(stderr, "[HOOK] strncmp(\"%s\", \"%s\", %zu) = %d\n", ea, eb, n, r);
    return r;
}

void *dlopen(const char *path, int flags) {
    static void *(*real)(const char *, int) = NULL;
    if (!real) real = dlsym(RTLD_NEXT, "dlopen");
    void *r = real(path, flags);
    fprintf(stderr, "[HOOK] dlopen(\"%s\") = %p\n", safe(path), r);
    return r;
}

char *getenv(const char *name) {
    static char *(*real)(const char *) = NULL;
    if (!real) real = dlsym(RTLD_NEXT, "getenv");
    char *r = real(name);
    fprintf(stderr, "[HOOK] getenv(\"%s\") = %s\n", safe(name), r ? r : "(null)");
    return r;
}
