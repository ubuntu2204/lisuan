#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <dlfcn.h>

/* vkfix: 向 DXVK 伪造 VK_KHR_load_store_op_none（gf 只有 EXT 版，枚举值相同） */

static void *lib = NULL;
static void *rsym(const char *n) {
    if (!lib) lib = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_GLOBAL);
    return lib ? dlsym(lib, n) : NULL;
}

#define KHR_LSOP "VK_KHR_load_store_op_none"

/* ---- vkEnumerateDeviceExtensionProperties: 追加 KHR 扩展 ---- */
typedef struct { char extensionName[256]; uint32_t specVersion; } VkExtProp;
typedef int (*vkEnumDevExt_t)(void *, const char *, uint32_t *, void *);

int vkEnumerateDeviceExtensionProperties(void *pdev, const char *layer,
                                         uint32_t *count, void *props) {
    static vkEnumDevExt_t real = NULL;
    if (!real) real = (vkEnumDevExt_t)rsym("vkEnumerateDeviceExtensionProperties");
    int r = real(pdev, layer, count, props);
    if (r != 0 && r != 1 /* VK_INCOMPLETE */) return r;
    static int appended_reported = 0;
    if (props == NULL) {
        /* count 查询：+1 */
        (*count)++;
        return r;
    }
    /* 数据查询：末尾追加 KHR（数组容量是 count 查询时返回的，含我们 +1） */
    VkExtProp *arr = (VkExtProp *)props;
    VkExtProp *e = &arr[*count - 1];
    memset(e, 0, sizeof(*e));
    strncpy(e->extensionName, KHR_LSOP, 255);
    e->specVersion = 1;
    if (!appended_reported) { fprintf(stderr, "[vkfix] appended %s\n", KHR_LSOP); appended_reported = 1; }
    return r;
}

/* ---- vkGetPhysicalDeviceFeatures2: 填 LoadStoreOpNoneFeaturesKHR=1 ---- */
struct hdr { int32_t sType; void *pNext; };
typedef void (*vkGetFeat2_t)(void *, void *);
void vkGetPhysicalDeviceFeatures2(void *pdev, void *feats) {
    static vkGetFeat2_t real = NULL;
    if (!real) real = (vkGetFeat2_t)rsym("vkGetPhysicalDeviceFeatures2");
    real(pdev, feats);
    static int dumped = 0;
    struct hdr *p = ((struct hdr *)feats)->pNext;
    for (; p; p = p->pNext) {
        if (!dumped) fprintf(stderr, "[vkfix] feat pNext sType=%d\n", p->sType);
        /* VkPhysicalDeviceLoadStoreOpNoneFeaturesKHR: sType 猜测 1000504000,
           结构 = { sType, pNext, VkBool32 loadStoreOpNone }，填第一个 u32=1 */
        if (p->sType == 1000504000) {
            *(uint32_t *)(p + 1) = 1;  /* p+1 跳过 sType+pNext(16B 64位) 不对，需按 64 位布局 */
        }
    }
    dumped = 1;
    /* 修正：64 位下结构头 = sType(4) + pad(4) + pNext(8) = 16 字节，bool 在 offset 16 */
    p = ((struct hdr *)feats)->pNext;
    for (; p; p = p->pNext) {
        if (p->sType == 1000504000) {
            *(uint32_t *)((char *)p + 16) = 1;
            fprintf(stderr, "[vkfix] set loadStoreOpNone feature = 1\n");
        }
    }
}

/* ---- vkCreateDevice: 移除 KHR 扩展（gf 不认识）---- */
typedef int (*vkCreateDevice_t)(void *, const void *, const void *, void **);
struct DCI {
    int32_t sType; const void *pNext; uint32_t flags;
    uint32_t queueCount; const void *pQueues;
    uint32_t layerCount; const void *ppLayers;
    uint32_t extCount; const char *const *ppExts;
    const void *pFeatures;
};
int vkCreateDevice(void *pdev, const void *ci_, const void *alloc, void **out) {
    static vkCreateDevice_t real = NULL;
    if (!real) real = (vkCreateDevice_t)rsym("vkCreateDevice");
    const struct DCI *ci = (const struct DCI *)ci_;
    struct DCI copy = *ci;
    const char **exts = NULL;
    if (ci->extCount > 0 && ci->ppExts) {
        exts = malloc(sizeof(char *) * ci->extCount);
        uint32_t n = 0;
        for (uint32_t i = 0; i < ci->extCount; i++) {
            const char *e = ci->ppExts[i];
            if (e && strcmp(e, KHR_LSOP) == 0) {
                fprintf(stderr, "[vkfix] vkCreateDevice: strip %s\n", KHR_LSOP);
                continue;
            }
            exts[n++] = e;
        }
        copy.extCount = n;
        copy.ppExts = exts;
    }
    int r = real(pdev, &copy, alloc, out);
    if (exts) free(exts);
    return r;
}
