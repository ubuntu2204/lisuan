/*
 * gf_lowmem_icd.c — 砺算 7G100「低地址显存映射」Vulkan ICD Shim
 *
 * 目的
 *   让 32 位 Windows 游戏（经 Proton WoW64）能用上 7G100 硬件渲染。
 *
 * 问题
 *   32 位游戏的 DXVK 经 wine 的 WoW64 thunk 调用 64 位 gf 驱动。
 *   gf 的 vkMapMemory 返回的 CPU 指针位于高地址（实测 0x7625ebf1a000），
 *   wine 的 thunk 无法把 >4GB 的指针交给 32 位代码，于是主动 unmap 并
 *   合成 VK_ERROR_OUT_OF_HOST_MEMORY，DXVK 随即分配失败、游戏崩溃：
 *       d3d11: failed to create buffer (target 0x1 mode 0 size 20) [0x80070057]
 *
 * 解法（零拷贝 dmabuf 低位重映射）
 *   本 shim 以 ICD 形式插在 winevulkan(64位) 与真实 gf 驱动之间：
 *     1. vkCreateDevice  自动追加 VK_KHR_external_memory_fd 扩展
 *     2. vkAllocateMemory 为 host-visible 内存注入 VkExportMemoryAllocateInfo
 *     3. vkMapMemory      调真实驱动建 GPU VA，再导出 dmabuf fd，
 *                         用 mmap(MAP_SHARED|MAP_32BIT) 映射到 <4GB，把低位指针返回上层
 *     4. vkUnmapMemory / vkFreeMemory 做对应清理
 *   dmabuf 与原始显存共享同一物理页 → 零拷贝，运行时无额外开销。
 *
 * 安全性
 *   全部是用户态标准 Vulkan API 调用 + mmap，不写寄存器、不改驱动、不碰固件。
 *
 * 编译
 *   gcc -O2 -shared -fPIC -o libvulkan_gf_lowmem.so gf_lowmem_icd.c -ldl -lpthread
 *
 * 用法
 *   GFX_REAL_ICD=/usr/lib/x86_64-linux-gnu/libvulkan_gf.so \
 *   VK_ICD_FILENAMES=/path/to/gf_lowmem_icd.json   <启动游戏>
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <dlfcn.h>
#include <pthread.h>
#include <sys/mman.h>

/* ---------------- 精简 Vulkan 定义 ---------------- */
typedef void* VkInstance;
typedef void* VkPhysicalDevice;
typedef void* VkDevice;
typedef void* VkDeviceMemory;
typedef void* VkQueue;
typedef void* VkAllocationCallbacks;   /* 不关心其布局，只透传 */
typedef uint32_t VkResult;
typedef uint32_t VkFlags;
typedef uint32_t VkBool32;
typedef uint64_t VkDeviceSize;
typedef uint32_t VkExternalMemoryHandleTypeFlags;
typedef void (*PFN_vkVoidFunction)(void);

#define VK_SUCCESS 0

#define ST_MEMORY_ALLOCATE_INFO             5
#define ST_EXPORT_MEMORY_ALLOCATE_INFO 1000072000u
#define ST_MEMORY_GET_FD_INFO_KHR      1000072001u

#define PROP_DEVICE_LOCAL   0x1
#define PROP_HOST_VISIBLE   0x2
#define PROP_HOST_COHERENT  0x4

#define HANDLE_TYPE_DMA_BUF 0x00000200

#define VK_MAX_MEMORY_TYPES 32
#define VK_MAX_MEMORY_HEAPS 16

/* VkInstanceCreateInfo：供 shim_CreateInstance 读取 wine 请求的扩展列表 */
typedef struct VkApplicationInfo_ { uint32_t sType; const void* pNext;
                                    const char* pApplicationName; uint32_t applicationVersion;
                                    const char* pEngineName; uint32_t engineVersion;
                                    uint32_t apiVersion; } VkApplicationInfo;
typedef struct { uint32_t sType; const void* pNext; uint32_t flags;
                 const VkApplicationInfo* pApplicationInfo;
                 uint32_t enabledLayerCount; const char* const* ppEnabledLayerNames;
                 uint32_t enabledExtensionCount; const char* const* ppEnabledExtensionNames;
               } VkInstanceCreateInfo;

typedef struct { uint32_t sType; const void* pNext;
                 VkDeviceSize allocationSize; uint32_t memoryTypeIndex;
               } VkMemoryAllocateInfo;

typedef struct { uint32_t sType; const void* pNext;
                 VkExternalMemoryHandleTypeFlags handleTypes;
               } VkExportMemoryAllocateInfo;

typedef struct { uint32_t sType; const void* pNext;
                 VkDeviceMemory memory;
                 VkExternalMemoryHandleTypeFlags handleType;
               } VkMemoryGetFdInfoKHR;

typedef struct { uint32_t propertyFlags; uint32_t heapIndex; } VkMemoryType_;
typedef struct { VkDeviceSize size; VkFlags flags; } VkMemoryHeap_;
typedef struct { uint32_t memoryTypeCount; VkMemoryType_ memoryTypes[VK_MAX_MEMORY_TYPES];
                 uint32_t memoryHeapCount; VkMemoryHeap_ memoryHeaps[VK_MAX_MEMORY_HEAPS];
               } VkPhysicalDeviceMemoryProperties;

typedef struct { uint32_t sType; const void* pNext; uint32_t flags;
                 uint32_t queueCreateInfoCount; const void* pQueueCreateInfos;
                 uint32_t enabledLayerCount; const char* const* ppEnabledLayerNames;
                 uint32_t enabledExtensionCount; const char* const* ppEnabledExtensionNames;
                 const void* pEnabledFeatures;
               } VkDeviceCreateInfo;

/* ---------------- 真实驱动 ---------------- */
static void load_real(void);   /* 定义在文件后部，此处前置声明 */

static void*                        g_real      = NULL;
static void*                        g_loader    = NULL;  /* 方案A：独立命名空间里的标准 loader */
static int                          g_via_loader = 0;    /* 1=经 loader 转发 gf */
static PFN_vkVoidFunction         (*g_real_gipa)(VkInstance, const char*) = NULL;
static PFN_vkVoidFunction         (*g_real_gdpa)(VkDevice,   const char*) = NULL;

typedef VkResult (*PFN_vkCreateDevice)(VkPhysicalDevice, const VkDeviceCreateInfo*,
                                       const VkAllocationCallbacks*, VkDevice*);
typedef void     (*PFN_vkGetPhysicalDeviceMemoryProperties)(VkPhysicalDevice,
                                                            VkPhysicalDeviceMemoryProperties*);
typedef VkResult (*PFN_vkAllocateMemory)(VkDevice, const VkMemoryAllocateInfo*,
                                         const VkAllocationCallbacks*, VkDeviceMemory*);
typedef VkResult (*PFN_vkMapMemory)(VkDevice, VkDeviceMemory, VkDeviceSize, VkDeviceSize,
                                    VkFlags, void**);
typedef void     (*PFN_vkUnmapMemory)(VkDevice, VkDeviceMemory);
typedef void     (*PFN_vkFreeMemory)(VkDevice, VkDeviceMemory, const VkAllocationCallbacks*);
typedef VkResult (*PFN_vkGetMemoryFdKHR)(VkDevice, const VkMemoryGetFdInfoKHR*, int*);

static PFN_vkCreateDevice                      real_CreateDevice      = NULL;
static PFN_vkGetPhysicalDeviceMemoryProperties real_GetMemProps       = NULL;
static PFN_vkAllocateMemory                    real_AllocateMemory    = NULL;
static PFN_vkMapMemory                         real_MapMemory         = NULL;
static PFN_vkUnmapMemory                       real_UnmapMemory       = NULL;
static PFN_vkFreeMemory                        real_FreeMemory        = NULL;
static PFN_vkGetMemoryFdKHR                    real_GetMemoryFdKHR    = NULL;

typedef VkResult (*PFN_vkCreateInstance)(const void*, const void*, VkInstance*);
static PFN_vkCreateInstance                    real_CreateInstance    = NULL;

/* 用【真实 instance】解析 instance 级函数（传 NULL 只能拿到全局函数） */
static void resolve_instance_funcs(VkInstance inst) {
    if (!g_real_gipa || !inst) return;
    if (!g_real_gdpa)
        *(void**)&g_real_gdpa = (void*)g_real_gipa(inst, "vkGetDeviceProcAddr");
    if (!real_GetMemProps)
        real_GetMemProps = (PFN_vkGetPhysicalDeviceMemoryProperties)(void*)
            g_real_gipa(inst, "vkGetPhysicalDeviceMemoryProperties");
    if (!real_CreateDevice)
        real_CreateDevice = (PFN_vkCreateDevice)(void*)
            g_real_gipa(inst, "vkCreateDevice");
    fprintf(stderr, "[gf-shim]   instance 级函数: gdpa=%s getMemProps=%s createDevice=%s\n",
            g_real_gdpa ? "ok" : "null", real_GetMemProps ? "ok" : "null",
            real_CreateDevice ? "ok" : "null");
}

typedef struct { char extensionName[256]; uint32_t specVersion; } VkExtensionProperties;

typedef VkResult (*PFN_vkEnumerateInstanceExtensionProperties)(const char*, uint32_t*,
                                                               VkExtensionProperties*);
static PFN_vkEnumerateInstanceExtensionProperties real_EnumInstExtProps = NULL;

/* 实例扩展缓存（只取一次，保证"查数量"与"取列表"两次调用结果一致） */
static VkExtensionProperties* g_ext_cache = NULL;
static uint32_t               g_ext_cache_count = 0;

/*
 * gf 的矛盾行为：vkEnumerateInstanceExtensionProperties 报告支持 surface/display 系列，
 * 但 vkCreateInstance 传入这些扩展时返回 -7 (VK_ERROR_FEATURE_NOT_PRESENT)。
 * 实测直连 gf 时 wine 只启用 3 个扩展即成功，多出的 surface 三个会失败。
 * 故在转发 vkCreateInstance 前把这些剔除（枚举仍保留，以免 wine 不构建 WSI）。
 */
static int gf_shim_drop_inst_ext(const char *n)
{
    static const char *drop_exact[] = {
        "VK_KHR_surface", "VK_KHR_xlib_surface", "VK_KHR_xcb_surface",
        "VK_KHR_wayland_surface", "VK_KHR_win32_surface",
        "VK_KHR_get_surface_capabilities2", "VK_KHR_surface_protected_capabilities",
        "VK_KHR_display", "VK_KHR_get_display_properties2",
        "VK_EXT_direct_mode_display", "VK_EXT_acquire_xlib_display",
        "VK_EXT_display_surface_counter", "VK_EXT_acquire_drm_display",
        "VK_EXT_swapchain_colorspace",
    };
    static const char *drop_prefix[] = { "VK_EXT_surface_maintenance" };
    if (!n) return 0;
    for (unsigned i = 0; i < sizeof(drop_exact)/sizeof(drop_exact[0]); i++)
        if (!strcmp(n, drop_exact[i])) return 1;
    for (unsigned i = 0; i < sizeof(drop_prefix)/sizeof(drop_prefix[0]); i++)
        if (!strncmp(n, drop_prefix[i], strlen(drop_prefix[i]))) return 1;
    return 0;
}

/* 诊断：观察实例扩展枚举结果（wine 依赖它判断 ICD 可用性） */
static VkResult shim_EnumerateInstanceExtensionProperties(const char* pLayerName,
                                                          uint32_t* pCount,
                                                          VkExtensionProperties* pProps) {
    load_real();
    if (!real_EnumInstExtProps) {
        real_EnumInstExtProps = (PFN_vkEnumerateInstanceExtensionProperties)
            dlsym(g_real, "vkEnumerateInstanceExtensionProperties");
        if (!real_EnumInstExtProps && g_real_gipa)
            real_EnumInstExtProps = (PFN_vkEnumerateInstanceExtensionProperties)(void*)
                g_real_gipa(NULL, "vkEnumerateInstanceExtensionProperties");
    }
    if (!real_EnumInstExtProps) {
        fprintf(stderr, "[gf-shim] !! 无法解析 vkEnumerateInstanceExtensionProperties\n");
        if (pCount) *pCount = 0;
        return VK_SUCCESS;
    }
    /*
     * 关键修复：
     * gf 的 vkEnumerateInstanceExtensionProperties 会报告支持 surface 系列扩展，
     * 但 vkCreateInstance 时却拒收（返回 -7 VK_ERROR_FEATURE_NOT_PRESENT）。
     * 实测直连 gf 时 wine 只启用 3 个扩展并成功；
     * 而经 shim 时 wine 会启用 6 个（多出 surface 三个）从而失败。
     * 故在此把 surface / display 系列从枚举结果中剔除，对齐直连行为。
     */
    static const char *extra[] = { "VK_KHR_external_semaphore_capabilities" };
    const unsigned nextra = sizeof(extra) / sizeof(extra[0]);

    /*
     * 剔除判定统一用文件级函数 gf_shim_drop_inst_ext()。
     * （原先在此处写的是 GCC 嵌套函数 is_drop()，属于 GCC 扩展，
     *   clang 不接受，会报 function_definition_not_allowed。）
     */

    /*
     * 只向真实驱动取一次完整列表并缓存。
     * 不能"按需传数组容量再回来过滤"——那样第一次返回的数量与第二次实际
     * 填充的内容不一致，会导致上层（wine）反复重试（实测被调用 56 万次）。
     */
    if (!g_ext_cache) {
        uint32_t n = 0;
        if (real_EnumInstExtProps(NULL, &n, NULL) == VK_SUCCESS && n) {
            g_ext_cache = (VkExtensionProperties*)calloc(n, sizeof(VkExtensionProperties));
            uint32_t m = n;
            if (real_EnumInstExtProps(NULL, &m, g_ext_cache) == VK_SUCCESS)
                g_ext_cache_count = m;
        }
        fprintf(stderr, "[gf-shim] 缓存实例扩展: %u 个\n", g_ext_cache_count);
    }

    /*
     * 注意：这里【不再剔除】surface 扩展——wine 需要看到它们才会构建 WSI。
     * 剔除动作改在 vkCreateInstance 转发给 gf 之前进行（见 shim_CreateInstance），
     * 因为 gf 的矛盾行为是：枚举报支持、但创建时拒收。
     */
    if (!pProps) {                        /* 仅查询数量 */
        *pCount = g_ext_cache_count + nextra;
        fprintf(stderr, "[gf-shim] EnumInstExtProps(计数) %u -> %u\n",
                g_ext_cache_count, *pCount);
        return VK_SUCCESS;
    }

    uint32_t cap = *pCount;               /* 数组容量 */
    uint32_t w = 0;
    for (uint32_t i = 0; i < g_ext_cache_count && w < cap; i++)
        pProps[w++] = g_ext_cache[i];
    for (unsigned i = 0; i < nextra && w < cap; i++) {
        int dup = 0;
        for (uint32_t j = 0; j < w; j++)
            if (!strncmp(pProps[j].extensionName, extra[i], 255)) { dup = 1; break; }
        if (!dup) {
            snprintf(pProps[w].extensionName, sizeof(pProps[w].extensionName),
                     "%s", extra[i]);
            pProps[w].specVersion = 1;
            w++;
            fprintf(stderr, "[gf-shim] 补齐实例扩展: %s\n", extra[i]);
        }
    }
    *pCount = w;
    fprintf(stderr, "[gf-shim] EnumInstExtProps 返回 %u 个\n", w);
    return VK_SUCCESS;
}

/* 拦截 vkCreateInstance：拿到真实 instance 后才能解析 instance 级函数 */
static VkResult shim_CreateInstance(const void* pCI, const void* pAlloc, VkInstance* pInst) {
    fprintf(stderr, "[gf-shim] >>> shim_CreateInstance 被调用\n");
    if (pCI) {
        const VkInstanceCreateInfo *ci = (const VkInstanceCreateInfo *)pCI;
        fprintf(stderr, "[gf-shim] createinfo: sType=%u flags=%u pNext=%p appInfo=%p layers=%u exts=%u\n",
                ci->sType, ci->flags, (void*)ci->pNext, (void*)ci->pApplicationInfo,
                ci->enabledLayerCount, ci->enabledExtensionCount);
        for (uint32_t i = 0; i < ci->enabledExtensionCount; i++)
            fprintf(stderr, "[gf-shim]   req: %s\n",
                    ci->ppEnabledExtensionNames ? ci->ppEnabledExtensionNames[i] : "(null)");
        /* 遍历 pNext 链，看 wine 是否塞了 ICD 需要的结构 */
        {
            const void *pn = ci->pNext;
            int depth = 0;
            while (pn && depth < 6) {
                uint32_t st = 0;
                memcpy(&st, pn, sizeof(st));
                fprintf(stderr, "[gf-shim]   pNext[%d] sType=%u\n", depth, st);
                pn = *((const void * const *)((const char *)pn + 8));
                depth++;
            }
        }
    }
    load_real();
    if (!real_CreateInstance) {
        real_CreateInstance = (PFN_vkCreateInstance)dlsym(g_real, "vkCreateInstance");
        if (!real_CreateInstance && g_real_gipa)
            real_CreateInstance = (PFN_vkCreateInstance)(void*)
                g_real_gipa(NULL, "vkCreateInstance");
    }
    if (!real_CreateInstance) { fprintf(stderr, "[gf-shim] vkCreateInstance 解析失败\n");
                                return (VkResult)-9; }
    VkResult r;
    if (pCI && ((const VkInstanceCreateInfo *)pCI)->enabledExtensionCount) {
        const VkInstanceCreateInfo *ci = (const VkInstanceCreateInfo *)pCI;
        const char **keep = (const char **)calloc(ci->enabledExtensionCount, sizeof(char *));
        uint32_t kn = 0;
        for (uint32_t i = 0; i < ci->enabledExtensionCount; i++) {
            if (gf_shim_drop_inst_ext(ci->ppEnabledExtensionNames[i])) {
                fprintf(stderr, "[gf-shim] 转发时剔除扩展: %s\n",
                        ci->ppEnabledExtensionNames[i]);
                continue;
            }
            keep[kn++] = ci->ppEnabledExtensionNames[i];
        }
        VkInstanceCreateInfo ci2 = *ci;
        ci2.enabledExtensionCount = kn;
        ci2.ppEnabledExtensionNames = keep;
        fprintf(stderr, "[gf-shim] 实际转发给 gf 的扩展(%u):\n", kn);
        for (uint32_t i = 0; i < kn; i++)
            fprintf(stderr, "[gf-shim]   keep: %s\n", keep[i]);

        /*
         * 假设：gf 可能在运行时自行读取 VK_ICD_FILENAMES 做自检。
         * 直连 gf 时该变量指向 gf 自身；经 shim 时它指向 shim，
         * 若 gf 校验这一点，就会拒绝（表现为 -7）。
         * 故在真正转发前临时把它改回 gf，调用结束再还原。
         */
        const char *gf_json2 = getenv("GF_SHIM_GF_ICD");
        if (!gf_json2 || !*gf_json2) gf_json2 = "/usr/share/vulkan/icd.d/gf_icd.x86_64.json";
        char *saved2 = NULL;
        {
            const char *c2 = getenv("VK_ICD_FILENAMES");
            if (c2) saved2 = strdup(c2);
        }
        setenv("VK_ICD_FILENAMES", gf_json2, 1);
        r = real_CreateInstance(&ci2, pAlloc, pInst);
        if (saved2) { setenv("VK_ICD_FILENAMES", saved2, 1); free(saved2); }
        else        { unsetenv("VK_ICD_FILENAMES"); }
        free(keep);
    } else {
        r = real_CreateInstance(pCI, pAlloc, pInst);
    }
    fprintf(stderr, "[gf-shim] <<< shim_CreateInstance 返回 r=%d inst=%p\n",
            (int)r, (void*)(pInst ? *pInst : NULL));
    if (r == VK_SUCCESS && *pInst) resolve_instance_funcs(*pInst);
    return r;
}

/* 只做 dlsym（device 级函数在 vkCreateDevice 之后再解析，见 shim_CreateDevice） */
#define REAL(name) do { \
    if (!real_##name) \
        real_##name = (PFN_vk##name)dlsym(g_real, "vk" #name); \
} while (0)

/* vkGetPhysicalDeviceMemoryProperties 的变量名与函数名不同，单独解析 */
#define LOAD_MEMPROPS() do { \
    if (!real_GetMemProps) \
        real_GetMemProps = (PFN_vkGetPhysicalDeviceMemoryProperties) \
            dlsym(g_real, "vkGetPhysicalDeviceMemoryProperties"); \
} while (0)

/* ---------------- 映射记录表 ---------------- */
typedef struct MapEntry {
    VkDevice            dev;
    VkDeviceMemory      mem;
    void*               real_ptr;   /* 真实驱动返回的高位指针 */
    void*               low_ptr;    /* 我们提供的低位(<4GB)映射 */
    size_t              size;
    int                 fd;
    struct MapEntry*    next;
} MapEntry;

static MapEntry*   g_maps   = NULL;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

static void map_add(VkDevice d, VkDeviceMemory m, void* rp, void* lp, size_t sz, int fd) {
    MapEntry* e = (MapEntry*)calloc(1, sizeof(MapEntry));
    e->dev = d; e->mem = m; e->real_ptr = rp; e->low_ptr = lp; e->size = sz; e->fd = fd;
    pthread_mutex_lock(&g_lock);
    e->next = g_maps; g_maps = e;
    pthread_mutex_unlock(&g_lock);
}

static MapEntry* map_find(VkDevice d, VkDeviceMemory m) {
    pthread_mutex_lock(&g_lock);
    for (MapEntry* e = g_maps; e; e = e->next)
        if (e->dev == d && e->mem == m) { pthread_mutex_unlock(&g_lock); return e; }
    pthread_mutex_unlock(&g_lock);
    return NULL;
}

static void map_remove(MapEntry* target) {
    pthread_mutex_lock(&g_lock);
    MapEntry** pp = &g_maps;
    while (*pp) {
        if (*pp == target) { *pp = target->next; break; }
        pp = &((*pp)->next);
    }
    pthread_mutex_unlock(&g_lock);
    free(target);
}

/* 内存类型属性缓存：判断某 type 是否 host-visible */
static uint32_t g_type_flags[VK_MAX_MEMORY_TYPES];
static int      g_type_flags_valid = 0;

/* ---------------- 拦截实现 ---------------- */

/* 1) 记录内存类型属性 */
static void shim_GetPhysicalDeviceMemoryProperties(VkPhysicalDevice pd,
                                                   VkPhysicalDeviceMemoryProperties* p) {
    real_GetMemProps(pd, p);
    uint32_t n = p->memoryTypeCount < VK_MAX_MEMORY_TYPES ? p->memoryTypeCount
                                                          : VK_MAX_MEMORY_TYPES;
    for (uint32_t i = 0; i < n; i++)
        g_type_flags[i] = p->memoryTypes[i].propertyFlags;
    g_type_flags_valid = 1;
}

/* A/B 开关：GF_SHIM_NO_EXPORT=1 时完全不介入（行为等同直连真实驱动） */
static int export_enabled(void) {
    const char* e = getenv("GF_SHIM_NO_EXPORT");
    return !(e && *e && strcmp(e, "0") != 0);
}

/* 解析 device 级函数（device 创建成功后调用） */
static void resolve_dev_funcs(VkDevice dev) {
    if (!dev) return;
    __typeof__(g_real_gdpa) gdpa =
        (__typeof__(g_real_gdpa))dlsym(g_real, "vkGetDeviceProcAddr");
    if (!gdpa) gdpa = g_real_gdpa;
    if (!gdpa) return;
    if (!real_AllocateMemory)
        real_AllocateMemory = (PFN_vkAllocateMemory)(void*)gdpa(dev, "vkAllocateMemory");
    if (!real_MapMemory)
        real_MapMemory = (PFN_vkMapMemory)(void*)gdpa(dev, "vkMapMemory");
    if (!real_UnmapMemory)
        real_UnmapMemory = (PFN_vkUnmapMemory)(void*)gdpa(dev, "vkUnmapMemory");
    if (!real_FreeMemory)
        real_FreeMemory = (PFN_vkFreeMemory)(void*)gdpa(dev, "vkFreeMemory");
    if (!real_GetMemoryFdKHR)
        real_GetMemoryFdKHR = (PFN_vkGetMemoryFdKHR)(void*)gdpa(dev, "vkGetMemoryFdKHR");
}

/* 2) vkCreateDevice：自动追加 VK_KHR_external_memory_fd */
static VkResult shim_CreateDevice(VkPhysicalDevice pd, const VkDeviceCreateInfo* ci,
                                  const VkAllocationCallbacks* ac, VkDevice* pDev) {
    if (!real_CreateDevice) REAL(CreateDevice);
    if (!export_enabled()) {
        fprintf(stderr, "[gf-shim] GF_SHIM_NO_EXPORT=1，不注入扩展（对照模式）\n");
        VkResult rr = real_CreateDevice(pd, ci, ac, pDev);
        if (rr == VK_SUCCESS && *pDev) resolve_dev_funcs(*pDev);
        return rr;
    }

    uint32_t orig = ci->enabledExtensionCount;
    int already = 0;
    for (uint32_t i = 0; i < orig; i++)
        if (ci->ppEnabledExtensionNames[i] &&
            !strcmp(ci->ppEnabledExtensionNames[i], "VK_KHR_external_memory_fd")) {
            already = 1; break;
        }
    if (already) return real_CreateDevice(pd, ci, ac, pDev);

    const char** exts = (const char**)calloc(orig + 1, sizeof(char*));
    for (uint32_t i = 0; i < orig; i++) exts[i] = ci->ppEnabledExtensionNames[i];
    exts[orig] = "VK_KHR_external_memory_fd";

    VkDeviceCreateInfo ci2 = *ci;
    ci2.enabledExtensionCount   = orig + 1;
    ci2.ppEnabledExtensionNames = exts;

    VkResult r = real_CreateDevice(pd, &ci2, ac, pDev);
    free(exts);

    if (r == VK_SUCCESS && *pDev) {
        resolve_dev_funcs(*pDev);
        fprintf(stderr, "[gf-shim] device created, external_memory_fd 已启用\n");
    }
    return r;
}

/* 3) vkAllocateMemory：为 host-visible 内存注入导出信息 */
static VkResult shim_AllocateMemory(VkDevice dev, const VkMemoryAllocateInfo* ai,
                                    const VkAllocationCallbacks* ac, VkDeviceMemory* pMem) {
    if (!real_AllocateMemory) REAL(AllocateMemory);

    int host_visible = 1;   /* 默认按可导出处理，最稳妥 */
    if (g_type_flags_valid && ai->memoryTypeIndex < VK_MAX_MEMORY_TYPES)
        host_visible = !!(g_type_flags[ai->memoryTypeIndex] & PROP_HOST_VISIBLE);

    if (host_visible) {
        VkExportMemoryAllocateInfo exp;
        memset(&exp, 0, sizeof(exp));
        exp.sType       = ST_EXPORT_MEMORY_ALLOCATE_INFO;
        exp.pNext       = ai->pNext;        /* 保留调用方原有链 */
        exp.handleTypes = HANDLE_TYPE_DMA_BUF;

        VkMemoryAllocateInfo ai2 = *ai;
        ai2.pNext = &exp;
        return real_AllocateMemory(dev, &ai2, ac, pMem);
    }
    return real_AllocateMemory(dev, ai, ac, pMem);
}

/* 4) vkMapMemory：核心 —— 用 dmabuf 映射到低 4GB */
static VkResult shim_MapMemory(VkDevice dev, VkDeviceMemory mem, VkDeviceSize offset,
                               VkDeviceSize size, VkFlags flags, void** ppData) {
    if (!real_MapMemory)      REAL(MapMemory);
    if (!real_GetMemoryFdKHR) REAL(GetMemoryFdKHR);

    /* 先让真实驱动建立 GPU VA 映射（我们不用它的指针，但状态需要正确） */
    void* real_ptr = NULL;
    VkResult r = real_MapMemory(dev, mem, offset, size, flags, &real_ptr);
    if (r != VK_SUCCESS) return r;

    if (!real_GetMemoryFdKHR) {
        fprintf(stderr, "[gf-shim] 无 vkGetMemoryFdKHR，回退高位指针 0x%p\n", real_ptr);
        *ppData = real_ptr;
        return VK_SUCCESS;
    }

    VkMemoryGetFdInfoKHR fi;
    memset(&fi, 0, sizeof(fi));
    fi.sType      = ST_MEMORY_GET_FD_INFO_KHR;
    fi.memory     = mem;
    fi.handleType = HANDLE_TYPE_DMA_BUF;
    int fd = -1;
    r = real_GetMemoryFdKHR(dev, &fi, &fd);
    if (r != VK_SUCCESS || fd < 0) {
        fprintf(stderr, "[gf-shim] dmabuf 导出失败(%u)，回退高位指针\n", r);
        *ppData = real_ptr;
        return VK_SUCCESS;
    }

    VkDeviceSize map_size = (size == ~(VkDeviceSize)0) ? 0 : size;
    if (map_size == 0) {
        /* VK_WHOLE_SIZE：用我们记录的分配大小（此处以 4MB 兜底，实际应查分配记录） */
        map_size = 4u * 1024 * 1024;
    }

    void* low = mmap(NULL, map_size, PROT_READ | PROT_WRITE,
                     MAP_SHARED | MAP_32BIT, fd, (off_t)offset);
    if (low == MAP_FAILED) {
        fprintf(stderr, "[gf-shim] mmap(MAP_32BIT) 失败，回退高位指针 0x%p\n", real_ptr);
        close(fd);
        *ppData = real_ptr;
        return VK_SUCCESS;
    }

    if (((uintptr_t)low >> 32) != 0) {
        fprintf(stderr, "[gf-shim] 警告: 映射仍 >=4GB (0x%p)，回退\n", low);
        munmap(low, map_size);
        close(fd);
        *ppData = real_ptr;
        return VK_SUCCESS;
    }

    map_add(dev, mem, real_ptr, low, (size_t)map_size, fd);
    fprintf(stderr, "[gf-shim] map: 高位 0x%p -> 低位 0x%p (%zu 字节, fd=%d)\n",
            real_ptr, low, (size_t)map_size, fd);
    *ppData = low;
    return VK_SUCCESS;
}

/* 5) vkUnmapMemory / vkFreeMemory：清理 */
static void shim_UnmapMemory(VkDevice dev, VkDeviceMemory mem) {
    if (!real_UnmapMemory) REAL(UnmapMemory);
    MapEntry* e = map_find(dev, mem);
    if (e) {
        munmap(e->low_ptr, e->size);
        close(e->fd);
        map_remove(e);
    }
    real_UnmapMemory(dev, mem);
}

static void shim_FreeMemory(VkDevice dev, VkDeviceMemory mem,
                            const VkAllocationCallbacks* ac) {
    if (!real_FreeMemory) REAL(FreeMemory);
    MapEntry* e = map_find(dev, mem);
    if (e) {
        munmap(e->low_ptr, e->size);
        close(e->fd);
        map_remove(e);
    }
    real_FreeMemory(dev, mem, ac);
}

/* ---------------- 驱动加载 ---------------- */
static void load_real(void) {
    if (g_real) return;
    const char* path = getenv("GFX_REAL_ICD");
    if (!path || !*path) {
        /*
         * 自动探测真实驱动位置。
         * 注意：Steam Runtime（pressure-vessel）容器内，宿主机根目录挂载在 /run/host 下，
         * 因此 /usr/lib/x86_64-linux-gnu/libvulkan_gf.so 在容器内并不存在。
         */
        static const char* cands[] = {
            "/run/host/usr/lib/x86_64-linux-gnu/libvulkan_gf.so",  /* 容器内 */
            "/usr/lib/x86_64-linux-gnu/libvulkan_gf.so",           /* 宿主机 */
            "libvulkan_gf.so",                                     /* 交给动态链接器 */
            NULL
        };
        for (int i = 0; cands[i]; i++) {
            if (access(cands[i], R_OK) == 0) { path = cands[i]; break; }
        }
        if (!path) path = cands[0];
    }

    /*
     * 方案 A：不直接 dlopen gf，而是让【标准 Vulkan loader】去加载 gf。
     *
     * 原因：实测同样的实例扩展，标准 loader 调 gf 的 vkCreateInstance 成功，
     * 而 shim 自己 dlopen gf 后调用返回 -7（VK_ERROR_FEATURE_NOT_PRESENT）——
     * gf 依赖 loader 建立的加载上下文。
     *
     * 两个要点：
     * 1) 用 dlmopen(LM_ID_NEWLM) 在【独立命名空间】加载一份 loader，
     *    否则会拿到进程内已加载的那个实例，它会把调用 dispatch 回本 shim → 递归。
     * 2) dlmopen 之前把 VK_ICD_FILENAMES 指向【gf 原始 json】，
     *    绝不能指回 shim 自身，否则新 loader 又去加载 shim → 递归。
     *    随后恢复原值，避免影响进程内其他代码。
     */
    const char *gf_json = getenv("GF_SHIM_GF_ICD");
    if (!gf_json || !*gf_json) {
        /*
         * 容器内宿主机根挂载在 /run/host 下，系统 json 里的
         * /usr/lib/x86_64-linux-gnu/libvulkan_gf.so 在容器内并不存在，
         * 会让新 loader 找不到驱动而返回 -9 (VK_ERROR_INCOMPATIBLE_DRIVER)。
         * 故按实际可访问路径选择对应 json。
         */
        if (access("/run/host/usr/lib/x86_64-linux-gnu/libvulkan_gf.so", R_OK) == 0)
            gf_json = "/home/ubuntu/project/lisuan/gf-lowmem-shim/gf_real_container.json";
        else
            gf_json = "/usr/share/vulkan/icd.d/gf_icd.x86_64.json";
    }

    char *saved_icd = NULL;
    const char *cur_icd = getenv("VK_ICD_FILENAMES");
    if (cur_icd) saved_icd = strdup(cur_icd);

    /*
     * 加载第二份 loader。
     * 不能用 dlmopen(LM_ID_NEWLM)：新命名空间会把 libc 也重新加载一遍，
     * 触发 "cannot allocate memory in static TLS block"（静态 TLS 块有上限）。
     * 改为在【同一命名空间】dlopen 一份改过 SONAME 的副本：
     *   - 同命名空间 → libc/libm 直接复用已加载的，不额外占静态 TLS
     *   - SONAME 不同 → 动态链接器视其为新库，会重新初始化，拥有独立的 ICD 列表
     * 副本由 patch_soname.py 生成（libvk_gfs.so.1，SONAME 同名）。
     */
    {
        const char *use_loader = getenv("GF_SHIM_USE_LOADER");
        int want_loader = use_loader && *use_loader && strcmp(use_loader, "0") != 0;

        if (want_loader) {
            const char *copy = getenv("GF_SHIM_LOADER_COPY");
            if (!copy || !*copy)
                copy = "/home/ubuntu/project/lisuan/gf-lowmem-shim/libvk_gfs.so.1";
            setenv("VK_ICD_FILENAMES", gf_json, 1);   /* 让 loader 副本加载 gf，而非 shim 自身 */
            g_loader = dlopen(copy, RTLD_NOW | RTLD_LOCAL);
            if (!g_loader) {
                g_loader = dlmopen(LM_ID_NEWLM, "libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
            }

            if (g_loader) {
                /*
                 * 关键：Vulkan loader 是【惰性初始化】的——它在首次调用时才去读
                 * VK_ICD_FILENAMES 并加载 ICD。若在此处就把环境变量恢复成 shim，
                 * loader 副本随后会去加载 shim 自身 → 无限递归
                 * （表现为 gipa 被调用上百次、Syscall stack overrun）。
                 * 所以必须在恢复环境变量【之前】先触发它完成初始化。
                 */
                void *fn = dlsym(g_loader, "vkEnumerateInstanceExtensionProperties");
                if (fn) {
                    uint32_t n = 0;
                    ((VkResult (*)(const char *, uint32_t *, void *))fn)(NULL, &n, NULL);
                    fprintf(stderr, "[gf-shim] loader 副本已预初始化（ext=%u）\n", n);
                }
            }
        }
    }

    if (saved_icd) { setenv("VK_ICD_FILENAMES", saved_icd, 1); free(saved_icd); }
    else           { unsetenv("VK_ICD_FILENAMES"); }

    if (g_loader) {
        g_real = g_loader;                 /* 后续统一经 loader 调用 */
        g_via_loader = 1;
        fprintf(stderr, "[gf-shim] 已通过标准 loader(新命名空间)加载 gf: %s\n", gf_json);
    } else {
        fprintf(stderr, "[gf-shim] dlmopen loader 失败(%s)，回退为直接 dlopen gf\n", dlerror());
        g_real = dlopen(path, RTLD_NOW | RTLD_LOCAL);
        if (!g_real) {
            fprintf(stderr, "[gf-shim] dlopen 失败: %s (%s)\n", path, dlerror());
            return;
        }
        g_via_loader = 0;
    }
    /*
     * 关键：ICD 形态的驱动通常【只导出 vk_icdGetInstanceProcAddr】，
     * 不导出 vkCreateDevice / vkMapMemory 等标准符号，必须经由它获取。
     */
    if (g_via_loader) {
        /*
         * 方案 A：经标准 loader 转发。
         * loader 导出标准 Vulkan 符号，直接取即可；
         * ICD 协商由 loader 内部完成，这里不再（也不能）调用 negotiate。
         */
        *(void**)&g_real_gipa = dlsym(g_loader, "vkGetInstanceProcAddr");
        *(void**)&g_real_gdpa = dlsym(g_loader, "vkGetDeviceProcAddr");
        real_CreateInstance   = (PFN_vkCreateInstance)dlsym(g_loader, "vkCreateInstance");
        real_GetMemProps      = (PFN_vkGetPhysicalDeviceMemoryProperties)
                                  dlsym(g_loader, "vkGetPhysicalDeviceMemoryProperties");
        real_EnumInstExtProps = (PFN_vkEnumerateInstanceExtensionProperties)
                                  dlsym(g_loader, "vkEnumerateInstanceExtensionProperties");
        fprintf(stderr, "[gf-shim]   loader 符号: gipa=%s gdpa=%s createInstance=%s extProps=%s memProps=%s\n",
                g_real_gipa ? "ok" : "null", g_real_gdpa ? "ok" : "null",
                real_CreateInstance ? "ok" : "null",
                real_EnumInstExtProps ? "ok" : "null", real_GetMemProps ? "ok" : "null");
    } else {
        /* ICD 形态驱动只导出 vk_icdGetInstanceProcAddr，须经由它获取 */
        *(void**)&g_real_gipa = dlsym(g_real, "vk_icdGetInstanceProcAddr");
        *(void**)&g_real_gdpa = dlsym(g_real, "vkGetDeviceProcAddr");

        {
            typedef VkResult (*PFN_negotiate)(uint32_t *);
            PFN_negotiate neg = (PFN_negotiate)dlsym(g_real,
                                "vk_icdNegotiateLoaderICDInterfaceVersion");
            if (neg) {
                uint32_t ver = 7;
                VkResult nr = neg(&ver);
                fprintf(stderr, "[gf-shim] 与 gf 协商 ICD 接口: 请求=7 -> 支持=%u (r=%d)\n",
                        ver, (int)nr);
            }
        }

        if (g_real_gipa) {
            if (!g_real_gdpa)
                *(void**)&g_real_gdpa = (void*)g_real_gipa(NULL, "vkGetDeviceProcAddr");
            if (!real_GetMemProps)
                real_GetMemProps = (PFN_vkGetPhysicalDeviceMemoryProperties)(void*)
                    g_real_gipa(NULL, "vkGetPhysicalDeviceMemoryProperties");
            if (!real_CreateDevice)
                real_CreateDevice = (PFN_vkCreateDevice)(void*)
                    g_real_gipa(NULL, "vkCreateDevice");
        }
    }

    /* device 级函数必须等 device 创建后通过 gdpa 取，见 shim_CreateDevice */

    extern char *program_invocation_short_name;
    fprintf(stderr, "[gf-shim][proc=%s pid=%d] 已加载真实驱动: %s\n",
            program_invocation_short_name ? program_invocation_short_name : "?",
            (int)getpid(), path);
    fprintf(stderr, "[gf-shim]   gipa=%s gdpa=%s getMemProps=%s createDevice=%s\n",
            g_real_gipa ? "ok" : "null", g_real_gdpa ? "ok" : "null",
            real_GetMemProps ? "ok" : "null", real_CreateDevice ? "ok" : "null");
}

/* ---------------- ICD / loader 入口 ---------------- */
/*
 * 关键：必须连 vkGetDeviceProcAddr 一起拦截。
 * loader 是通过 gipa(instance,"vkGetDeviceProcAddr") 拿到 gdpa 的，
 * 若不拦截，loader 会拿到真实驱动的 gdpa，从而绕过我们对 vkMapMemory 等的包装。
 */
static PFN_vkVoidFunction shim_GetDeviceProcAddr(VkDevice dev, const char* pName);
static VkResult gf_shim_negotiate(uint32_t* pSupportedVersion);   /* 定义在文件后部 */
PFN_vkVoidFunction vk_icdGetPhysicalDeviceProcAddr(VkInstance inst, const char* pName);

/* 诊断用：GF_SHIM_PASS=1 时纯转发，不拦截任何函数（用于定位是框架问题还是某拦截函数问题） */
static int pass_through(void) {
    const char* e = getenv("GF_SHIM_PASS");
    return (e && *e && strcmp(e, "0") != 0);
}

static int is_intercepted(const char* n) {
    if (pass_through()) return 0;
    return !strcmp(n, "vkCreateInstance") ||
           !strcmp(n, "vkEnumerateInstanceExtensionProperties") ||
           !strcmp(n, "vkGetDeviceProcAddr") ||
           !strcmp(n, "vkCreateDevice") ||
           !strcmp(n, "vkGetPhysicalDeviceMemoryProperties") ||
           !strcmp(n, "vkAllocateMemory") ||
           !strcmp(n, "vkMapMemory") ||
           !strcmp(n, "vkUnmapMemory") ||
           !strcmp(n, "vkFreeMemory");
}

static PFN_vkVoidFunction pick(const char* n) {
    if (!strcmp(n, "vkGetDeviceProcAddr"))
        return (PFN_vkVoidFunction)shim_GetDeviceProcAddr;
    if (!strcmp(n, "vkCreateInstance"))
        return (PFN_vkVoidFunction)shim_CreateInstance;
    if (!strcmp(n, "vkEnumerateInstanceExtensionProperties"))
        return (PFN_vkVoidFunction)shim_EnumerateInstanceExtensionProperties;
    if (!strcmp(n, "vkCreateDevice"))
        return (PFN_vkVoidFunction)shim_CreateDevice;
    if (!strcmp(n, "vkGetPhysicalDeviceMemoryProperties"))
        return (PFN_vkVoidFunction)shim_GetPhysicalDeviceMemoryProperties;
    if (!strcmp(n, "vkAllocateMemory"))
        return (PFN_vkVoidFunction)shim_AllocateMemory;
    if (!strcmp(n, "vkMapMemory"))
        return (PFN_vkVoidFunction)shim_MapMemory;
    if (!strcmp(n, "vkUnmapMemory"))
        return (PFN_vkVoidFunction)shim_UnmapMemory;
    if (!strcmp(n, "vkFreeMemory"))
        return (PFN_vkVoidFunction)shim_FreeMemory;
    return NULL;
}

/* Linux ICD 必需入口 */
__attribute__((visibility("default")))
PFN_vkVoidFunction vk_icdGetInstanceProcAddr(VkInstance inst, const char* pName) {
    load_real();
    fprintf(stderr, "[gf-shim] gipa(inst=%p, \"%s\") intercepted=%d\n",
            (void*)inst, pName, is_intercepted(pName));
    /*
     * 关键：wine 是【经 gipa】查询 negotiate 函数的，而不是 dlsym。
     * 真实 gf ICD 不提供它（转发会返回 NULL），wine 随之不再启用本 ICD
     * —— 表现就是"查询过 vkCreateInstance 却从不调用"。
     * 因此这里必须由 shim 自己应答。
     */
    /*
     * 模拟真实 gf 的"老式 ICD"特征：不提供 negotiate / gdpa 入口。
     * 提供它们会让 wine 的 loader 改走新版初始化路径，而 gf 并不真正支持，
     * 结果是 ICD 被探测后不被启用（查询过 vkCreateInstance 却从不调用）。
     * GF_SHIM_ICD_MODERN=1 可恢复"提供新版入口"的行为用于对比。
     */
    {
        const char* modern = getenv("GF_SHIM_ICD_MODERN");
        if (!(modern && *modern && strcmp(modern, "0") != 0)) {
            if (!strcmp(pName, "vk_icdNegotiateLoaderICDInterfaceVersion") ||
                !strcmp(pName, "vk_icdGetPhysicalDeviceProcAddr"))
                return NULL;
        } else {
            if (!strcmp(pName, "vk_icdNegotiateLoaderICDInterfaceVersion"))
                return (PFN_vkVoidFunction)gf_shim_negotiate;
            if (!strcmp(pName, "vk_icdGetPhysicalDeviceProcAddr"))
                return (PFN_vkVoidFunction)vk_icdGetPhysicalDeviceProcAddr;
        }
    }
    if (is_intercepted(pName)) return pick(pName);
    PFN_vkVoidFunction f = g_real_gipa ? g_real_gipa(inst, pName) : NULL;
    if (!f) fprintf(stderr, "[gf-shim]   !! 真实驱动对 \"%s\" 返回 NULL\n", pName);
    return f;
}

__attribute__((visibility("default")))
PFN_vkVoidFunction vk_icdGetPhysicalDeviceProcAddr(VkInstance inst, const char* pName) {
    load_real();
    if (is_intercepted(pName)) return pick(pName);
    if (g_real_gipa) {
        /* 某些 loader 版本用它取 physical device 级函数，转发即可 */
        PFN_vkVoidFunction f =
            (PFN_vkVoidFunction)dlsym(g_real, "vk_icdGetPhysicalDeviceProcAddr");
        if (f) return ((PFN_vkVoidFunction(*)(VkInstance, const char*))f)(inst, pName);
        return g_real_gipa(inst, pName);
    }
    return NULL;
}

/*
 * 只声明支持 interface version 3（最经典形态，loader 仅要求 vk_icdGetInstanceProcAddr）。
 * 声明更高的版本会让 loader 额外要求 vk_icdGetPhysicalDeviceProcAddr /
 * vk_icdEnumerateAdapterPhysicalDevices 等，本项目未完整实现，
 * 声称支持反而导致 loader 初始化路径异常（表现为 CreateDXGIFactory 失败）。
 */
/*
 * 重要：真实 gf ICD 是老式 ICD，很可能【不导出】本函数，loader 遂按最老接口(version 0)处理。
 * 而 shim 一旦导出并声明高版本，loader 会改走新版初始化路径，导致 ICD 未被真正启用
 * （表现为：查询过 vkCreateInstance 却从不调用）。
 * 故此处【故意不导出】，改名以免被 dlsym 命中，从而复刻直连 gf 时的行为。
 */
static VkResult gf_shim_negotiate(uint32_t* pSupportedVersion) {
    if (!pSupportedVersion) return VK_SUCCESS;
    fprintf(stderr, "[gf-shim] negotiate(由 shim 提供): 请求=%u，原样接受\n", *pSupportedVersion);
    return VK_SUCCESS;   /* 不改值 = 接受 loader 请求的最高版本 */
}

/* 拦截 gdpa：DXVK / loader 都通过它取设备级函数指针 */
static PFN_vkVoidFunction shim_GetDeviceProcAddr(VkDevice dev, const char* pName) {
    load_real();
    if (is_intercepted(pName)) return pick(pName);
    if (g_real_gdpa) return g_real_gdpa(dev, pName);
    return NULL;
}

/* 部分 loader 会直接 dlsym ICD 的该符号，也导出一份 */
__attribute__((visibility("default")))
PFN_vkVoidFunction vkGetDeviceProcAddr(VkDevice dev, const char* pName) {
    return shim_GetDeviceProcAddr(dev, pName);
}
