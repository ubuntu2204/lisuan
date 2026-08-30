#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <dlfcn.h>

/* 拦截 vulkan loader 关键调用，打印失败返回值，定位 DXVK 失败点 */

static void *real_lib = NULL;
static void *rsym(const char *n) {
    if (!real_lib) real_lib = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_GLOBAL);
    return real_lib ? dlsym(real_lib, n) : NULL;
}

#define VKFN(ret, name, ...) \
    static ret (*real_##name)(__VA_ARGS__) = NULL;

/* vkCreateBuffer */
typedef int (*vkCreateBuffer_t)(void *, const void *, const void *, void **);
int vkCreateBuffer(void *dev, const void *ci, const void *alloc, void **out) {
    static vkCreateBuffer_t real = NULL;
    if (!real) real = (vkCreateBuffer_t)rsym("vkCreateBuffer");
    int r = real(dev, ci, alloc, out);
    fprintf(stderr, "[vkspy] vkCreateBuffer -> %d\n", r);
    return r;
}

typedef int (*vkAllocateMemory_t)(void *, const void *, const void *, void **);
int vkAllocateMemory(void *dev, const void *ci, const void *alloc, void **out) {
    static vkAllocateMemory_t real = NULL;
    if (!real) real = (vkAllocateMemory_t)rsym("vkAllocateMemory");
    int r = real(dev, ci, alloc, out);
    if (r != 0) fprintf(stderr, "[vkspy] vkAllocateMemory -> %d\n", r);
    return r;
}

typedef int (*vkMapMemory_t)(void *, void *, uint64_t, uint64_t, uint32_t, void **);
int vkMapMemory(void *dev, void *mem, uint64_t off, uint64_t size, uint32_t flags, void **out) {
    static vkMapMemory_t real = NULL;
    if (!real) real = (vkMapMemory_t)rsym("vkMapMemory");
    int r = real(dev, mem, off, size, flags, out);
    if (r != 0) fprintf(stderr, "[vkspy] vkMapMemory off=%llu size=%llu -> %d\n",
                        (unsigned long long)off, (unsigned long long)size, r);
    return r;
}

typedef int (*vkBindBufferMemory_t)(void *, void *, void *, uint64_t);
int vkBindBufferMemory(void *dev, void *buf, void *mem, uint64_t off) {
    static vkBindBufferMemory_t real = NULL;
    if (!real) real = (vkBindBufferMemory_t)rsym("vkBindBufferMemory");
    int r = real(dev, buf, mem, off);
    if (r != 0) fprintf(stderr, "[vkspy] vkBindBufferMemory -> %d\n", r);
    return r;
}

typedef int (*vkCreateDevice_t)(void *, const void *, const void *, void **);
int vkCreateDevice(void *pdev, const void *ci, const void *alloc, void **out) {
    static vkCreateDevice_t real = NULL;
    if (!real) real = (vkCreateDevice_t)rsym("vkCreateDevice");
    int r = real(pdev, ci, alloc, out);
    fprintf(stderr, "[vkspy] vkCreateDevice -> %d\n", r);
    return r;
}

typedef void (*vkGetPhysicalDeviceProperties2_t)(void *, void *);
void vkGetPhysicalDeviceProperties2(void *pdev, void *props) {
    static vkGetPhysicalDeviceProperties2_t real = NULL;
    if (!real) real = (vkGetPhysicalDeviceProperties2_t)rsym("vkGetPhysicalDeviceProperties2");
    real(pdev, props);
    /* dump maxBufferSize from VkPhysicalDeviceVulkan13Properties in pNext chain */
    struct hdr { int sType; void *pNext; };
    struct hdr *p = (struct hdr *)props;
    p = (struct hdr *)p->pNext;
    while (p) {
        if (p->sType == 53) { /* VkPhysicalDeviceVulkan13Properties */
            /* maxBufferSize 在该结构中的 offset 需要探测, 打印前 400 字节中非零 u64 */
            uint64_t *q = (uint64_t *)p;
            for (int i = 2; i < 50; i++) {
                if (q[i] == 0x80000000ULL || q[i] == 0xffffffffULL)
                    fprintf(stderr, "[vkspy] v13props q[%d]=0x%llx (candidate maxBufferSize)\n",
                            i, (unsigned long long)q[i]);
            }
        }
        p = (struct hdr *)p->pNext;
    }
}
