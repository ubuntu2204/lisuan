/*
 * gf_isolated_icd.c — 砺算 7G100 的【隔离式】Vulkan ICD Shim
 *
 * 设计目标：让 32 位游戏（随从大师）能用 7G100 硬件渲染，同时【不影响其他游戏】。
 *
 * 为什么需要隔离：
 *   直接给系统驱动 /usr/lib/x86_64-linux-gnu/libvulkan_gf.so 打 MAP_32BIT 补丁，
 *   会作用于所有使用 gf 的程序。而像《刀塔霸业》是 Linux 原生 64 位，
 *   本可直接用高位地址，不需要任何补丁 —— 补丁对它只有害处（把映射锁进低 2GB）。
 *
 * 隔离做法：
 *   本 shim 以 ICD 形式只被 VK_ICD_FILENAMES 指向它的那个游戏加载，
 *   并且【直接 dlopen 驱动副本】，完全不经过系统 Vulkan loader，
 *   因此不会循环、也不会影响其他游戏（它们仍用系统原版驱动）。
 *
 * 三项职责：
 *   1) 转发：所有 Vulkan 调用转发到驱动副本
 *   2) 扩展修复：伪造 VK_KHR_load_store_op_none（gf 只有 EXT 版，DXVK 要 KHR 版）
 *   3) 映射锚定：vkMapMemory 经 dmabuf 重新映射到 2~4GB 区间
 *      （MAP_32BIT 只能到低 2GB，而游戏进程低 2GB 已被占满 2043/2048MB）
 *
 * 编译：
 *   gcc -O2 -shared -fPIC -o libvulkan_gf_isolated.so gf_isolated_icd.c -ldl -lpthread
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
typedef void* VkAllocationCallbacks;
typedef uint32_t VkResult;
typedef uint32_t VkFlags;
typedef uint64_t VkDeviceSize;
typedef uint32_t VkExternalMemoryHandleTypeFlags;
typedef void (*PFN_vkVoidFunction)(void);

#define VK_SUCCESS 0

#define ST_MEMORY_ALLOCATE_INFO             5
#define ST_EXPORT_MEMORY_ALLOCATE_INFO 1000072000u
#define ST_MEMORY_GET_FD_INFO_KHR      1000072001u

#define PROP_HOST_VISIBLE   0x2
#define HANDLE_TYPE_DMA_BUF 0x00000200
#define VK_MAX_MEMORY_TYPES 32
#define VK_MAX_MEMORY_HEAPS 16

/* 2~4GB 区间：低 2GB 已被游戏占满，而这里基本是空的 */
#define LOW4_START 0x80000000ULL
#define LOW4_END   0x100000000ULL

typedef struct { uint32_t sType; const void* pNext;
                 VkDeviceSize allocationSize; uint32_t memoryTypeIndex;
               } VkMemoryAllocateInfo;
typedef struct { uint32_t sType; const void* pNext;
                 VkExternalMemoryHandleTypeFlags handleTypes; } VkExportMemoryAllocateInfo;
typedef struct { uint32_t sType; const void* pNext;
                 VkDeviceMemory memory; VkExternalMemoryHandleTypeFlags handleType;
               } VkMemoryGetFdInfoKHR;
typedef struct { uint32_t propertyFlags; uint32_t heapIndex; } VkMemoryType_;
typedef struct { VkDeviceSize size; VkFlags flags; } VkMemoryHeap_;
typedef struct { uint32_t memoryTypeCount; VkMemoryType_ memoryTypes[VK_MAX_MEMORY_TYPES];
                 uint32_t memoryHeapCount; VkMemoryHeap_ memoryHeaps[VK_MAX_MEMORY_HEAPS];
               } VkPhysicalDeviceMemoryProperties;
typedef struct { char extensionName[256]; uint32_t specVersion; } VkExtensionProperties;
typedef struct { uint32_t sType; const void* pNext; uint32_t flags;
                 uint32_t queueCreateInfoCount; const void* pQueueCreateInfos;
                 uint32_t enabledLayerCount; const char* const* ppEnabledLayerNames;
                 uint32_t enabledExtensionCount; const char* const* ppEnabledExtensionNames;
                 const void* pEnabledFeatures; } VkDeviceCreateInfo;

/* ---------------- 驱动副本 ---------------- */
static void* g_drv = NULL;
static PFN_vkVoidFunction (*g_gipa)(VkInstance, const char*) = NULL;
static PFN_vkVoidFunction (*g_gdpa)(VkDevice,   const char*) = NULL;

typedef VkResult (*PFN_vkCreateInstance)(const void*, const void*, VkInstance*);
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
typedef VkResult (*PFN_vkEnumDevExt)(VkPhysicalDevice, const char*, uint32_t*, void*);
typedef void     (*PFN_vkGetFeat2)(VkPhysicalDevice, void*);

static PFN_vkCreateInstance                real_CreateInstance = NULL;
static PFN_vkCreateDevice                  real_CreateDevice   = NULL;
static PFN_vkGetPhysicalDeviceMemoryProperties real_GetMemProps = NULL;
static PFN_vkAllocateMemory                real_AllocMem   = NULL;
static PFN_vkMapMemory                     real_MapMem     = NULL;
static PFN_vkUnmapMemory                   real_UnmapMem   = NULL;
static PFN_vkFreeMemory                    real_FreeMem    = NULL;
static PFN_vkGetMemoryFdKHR                real_GetFd      = NULL;
static PFN_vkEnumDevExt                    real_EnumDevExt = NULL;
static PFN_vkGetFeat2                      real_GetFeat2   = NULL;

#define KHR_LSOP "VK_KHR_load_store_op_none"
#define ST_LOAD_STORE_OP_NONE_FEATURES 1000504000

/* 映射记录表 */
typedef struct MapEntry {
    VkDevice dev; VkDeviceMemory mem;
    void* real_ptr; void* low_ptr; size_t size; int fd; struct MapEntry* next;
} MapEntry;
static MapEntry* g_maps = NULL;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static uint32_t g_type_flags[VK_MAX_MEMORY_TYPES];
static int g_type_flags_valid = 0;

/* ---------------- 底层：经系统 Vulkan loader 转发 ----------------
 *
 * 重要：必须走系统 loader，不能 dlopen 驱动副本。
 * 实测直连驱动副本时，wine 虽能查询到 vkCreateInstance，却从不调用它
 * （wine 的 init_vulkan() 只查询指针，真正创建走 wine 图形驱动，
 *   它依赖标准 loader 的调度行为）。
 * 而 vkfix 这类 shim 通过 dlopen("libvulkan.so.1") 转发是可行的。
 *
 * 隔离性如何保证？
 *   - 系统驱动回滚为原版 → 其他游戏（如刀塔霸业）走系统原版驱动，完全不受影响
 *   - 只有 VK_ICD_FILENAMES 指向本 shim 的游戏才会加载它
 *   - 映射重定位在【本 shim 内】完成，不改动任何驱动文件
 */
static void load_drv(void)
{
    if (g_drv) return;
    const char* p = getenv("GF_ISOLATED_LOADER");
    if (!p || !*p) p = "libvulkan.so.1";

    g_drv = dlopen(p, RTLD_NOW | RTLD_GLOBAL);
    if (!g_drv) { fprintf(stderr, "[gf-iso] dlopen loader 失败: %s (%s)\n", p, dlerror()); return; }

    /* loader 导出的是标准 Vulkan 符号，直接取用 */
    *(void**)&g_gipa = dlsym(g_drv, "vkGetInstanceProcAddr");
    *(void**)&g_gdpa = dlsym(g_drv, "vkGetDeviceProcAddr");
    *(void**)&real_CreateInstance = dlsym(g_drv, "vkCreateInstance");
    *(void**)&real_GetMemProps    = dlsym(g_drv, "vkGetPhysicalDeviceMemoryProperties");
    *(void**)&real_CreateDevice   = dlsym(g_drv, "vkCreateDevice");

    fprintf(stderr, "[gf-iso] loader 就绪: %s (gipa=%s createInst=%s dev=%s)\n", p,
            g_gipa ? "ok" : "null", real_CreateInstance ? "ok" : "null",
            real_CreateDevice ? "ok" : "null");
}

static void resolve_dev(VkDevice dev)
{
    if (!dev || !g_gdpa) return;
#define GET(x, n) if (!x) *(void**)&x = (void*)g_gdpa(dev, n)
    GET(real_AllocMem,   "vkAllocateMemory");
    GET(real_MapMem,     "vkMapMemory");
    GET(real_UnmapMem,   "vkUnmapMemory");
    GET(real_FreeMem,    "vkFreeMemory");
    GET(real_GetFd,      "vkGetMemoryFdKHR");
    GET(real_EnumDevExt, "vkEnumerateDeviceExtensionProperties");
    GET(real_GetFeat2,   "vkGetPhysicalDeviceFeatures2");
#undef GET
}

/* 记录每次分配的大小，供 vkMapMemory 传 VK_WHOLE_SIZE(~0) 时查用 */
#define SZSLOT 4096
typedef struct { VkDeviceMemory mem; VkDeviceSize size; } SzRec;
static SzRec g_sizes[SZSLOT];
static pthread_mutex_t g_szlock = PTHREAD_MUTEX_INITIALIZER;

static void size_put(VkDeviceMemory m, VkDeviceSize s)
{
    pthread_mutex_lock(&g_szlock);
    size_t h = (((uintptr_t)m) >> 4) % SZSLOT;
    g_sizes[h].mem = m; g_sizes[h].size = s;
    pthread_mutex_unlock(&g_szlock);
}
static VkDeviceSize size_get(VkDeviceMemory m)
{
    pthread_mutex_lock(&g_szlock);
    size_t h = (((uintptr_t)m) >> 4) % SZSLOT;
    VkDeviceSize s = (g_sizes[h].mem == m) ? g_sizes[h].size : 0;
    pthread_mutex_unlock(&g_szlock);
    return s;
}

/* ---------------- 映射表 ---------------- */
static void map_add(VkDevice d, VkDeviceMemory m, void* rp, void* lp, size_t s, int fd)
{
    MapEntry* e = (MapEntry*)calloc(1, sizeof(*e));
    e->dev = d; e->mem = m; e->real_ptr = rp; e->low_ptr = lp; e->size = s; e->fd = fd;
    pthread_mutex_lock(&g_lock); e->next = g_maps; g_maps = e; pthread_mutex_unlock(&g_lock);
}
static MapEntry* map_find(VkDevice d, VkDeviceMemory m)
{
    pthread_mutex_lock(&g_lock);
    for (MapEntry* e = g_maps; e; e = e->next)
        if (e->dev == d && e->mem == m) { pthread_mutex_unlock(&g_lock); return e; }
    pthread_mutex_unlock(&g_lock); return NULL;
}
static void map_del(MapEntry* t)
{
    pthread_mutex_lock(&g_lock);
    for (MapEntry** pp = &g_maps; *pp; pp = &(*pp)->next)
        if (*pp == t) { *pp = t->next; break; }
    pthread_mutex_unlock(&g_lock);
    free(t);
}

/* 在 2~4GB 区间内映射（多组 hint 依次尝试） */
static void* map_into_low4(size_t size, int prot, int fd, off_t offset)
{
    for (int i = 0; i < 10; i++) {
        void* hint = (void*)(LOW4_START + (uint64_t)i * 0x8000000ULL);   /* 每档 +128MB */
        void* p = mmap(hint, size, prot, MAP_SHARED, fd, offset);
        if (p == MAP_FAILED) continue;
        if ((uintptr_t)p < LOW4_END) return p;      /* 落在 4GB 以内，成功 */
        munmap(p, size);                            /* 漂到高位，换一档重试 */
    }
    return MAP_FAILED;
}

/* ---------------- 拦截实现 ---------------- */

/* 1) 记录内存类型属性 */
static void shim_GetMemProps(VkPhysicalDevice pd, VkPhysicalDeviceMemoryProperties* p)
{
    load_drv();
    if (!real_GetMemProps) *(void**)&real_GetMemProps = (void*)g_gipa(NULL, "vkGetPhysicalDeviceMemoryProperties");
    real_GetMemProps(pd, p);
    uint32_t n = p->memoryTypeCount < VK_MAX_MEMORY_TYPES ? p->memoryTypeCount : VK_MAX_MEMORY_TYPES;
    for (uint32_t i = 0; i < n; i++) g_type_flags[i] = p->memoryTypes[i].propertyFlags;
    g_type_flags_valid = 1;
}

/* 2) 设备扩展：追加 KHR_load_store_op_none（gf 只有 EXT 版） */
static VkResult shim_EnumDevExt(VkPhysicalDevice pd, const char* layer, uint32_t* cnt, void* props)
{
    load_drv(); resolve_dev(NULL);
    if (!real_EnumDevExt) return -2;
    VkResult r = real_EnumDevExt(pd, layer, cnt, props);
    if (r != 0 && r != 1) return r;
    if (!props) { (*cnt)++; return r; }
    VkExtensionProperties* arr = (VkExtensionProperties*)props;
    VkExtensionProperties* e = &arr[*cnt - 1];
    memset(e, 0, sizeof(*e));
    snprintf(e->extensionName, sizeof(e->extensionName), "%s", KHR_LSOP);
    e->specVersion = 1;
    return r;
}

/* 3) 特性：填 loadStoreOpNone = 1 */
static void shim_GetFeat2(VkPhysicalDevice pd, void* feats)
{
    load_drv();
    if (!real_GetFeat2) { if (g_gipa) *(void**)&real_GetFeat2 = (void*)g_gipa(NULL, "vkGetPhysicalDeviceFeatures2"); }
    if (!real_GetFeat2) return;
    real_GetFeat2(pd, feats);
    /* VkBaseInStructure 链：sType(4)+pad(4)+pNext(8) = 16 字节，bool 数据在偏移 16 */
    for (void* p = ((void**)feats)[1]; p; p = ((void**)p)[1]) {
        int32_t st = 0; memcpy(&st, p, sizeof(st));
        if (st == ST_LOAD_STORE_OP_NONE_FEATURES) { *(uint32_t*)((char*)p + 16) = 1; }
    }
}

/* 4) 创建设备：剔除 gf 不认识的 KHR 扩展，并启用 dmabuf 导出能力 */
static VkResult shim_CreateDevice(VkPhysicalDevice pd, const VkDeviceCreateInfo* ci,
                                  const VkAllocationCallbacks* ac, VkDevice* out)
{
    load_drv();
    if (!real_CreateDevice) *(void**)&real_CreateDevice = (void*)g_gipa(NULL, "vkCreateDevice");

    uint32_t orig = ci->enabledExtensionCount;
    const char** exts = (const char**)calloc(orig + 2, sizeof(char*));
    uint32_t n = 0;
    for (uint32_t i = 0; i < orig; i++) {
        const char* e = ci->ppEnabledExtensionNames ? ci->ppEnabledExtensionNames[i] : NULL;
        if (e && !strcmp(e, KHR_LSOP)) continue;                       /* 剔除伪造的 KHR */
        if (e && !strcmp(e, "VK_KHR_external_memory_fd")) continue;    /* 去重 */
        exts[n++] = e;
    }
    exts[n++] = "VK_KHR_external_memory_fd";                           /* 供 dmabuf 低位映射 */
    VkDeviceCreateInfo c2 = *ci;
    c2.enabledExtensionCount = n;
    c2.ppEnabledExtensionNames = exts;

    VkResult r = real_CreateDevice(pd, &c2, ac, out);
    free(exts);
    if (r == VK_SUCCESS && out && *out) { resolve_dev(*out); fprintf(stderr, "[gf-iso] device ok (dmabuf 已启用)\n"); }
    return r;
}

/* 5) 分配内存：为 host-visible 注入可导出属性 */
static VkResult shim_AllocMem(VkDevice dev, const VkMemoryAllocateInfo* ai,
                              const VkAllocationCallbacks* ac, VkDeviceMemory* out)
{
    load_drv(); resolve_dev(dev);
    if (!real_AllocMem) return -2;
    int host_vis = 1;
    if (g_type_flags_valid && ai->memoryTypeIndex < VK_MAX_MEMORY_TYPES)
        host_vis = !!(g_type_flags[ai->memoryTypeIndex] & PROP_HOST_VISIBLE);
    if (host_vis) {
        VkExportMemoryAllocateInfo exp;
        memset(&exp, 0, sizeof(exp));
        exp.sType = ST_EXPORT_MEMORY_ALLOCATE_INFO;
        exp.pNext = ai->pNext;
        exp.handleTypes = HANDLE_TYPE_DMA_BUF;
        VkMemoryAllocateInfo a2 = *ai;
        a2.pNext = &exp;
        VkResult r = real_AllocMem(dev, &a2, ac, out);
        if (r == VK_SUCCESS && out && *out) size_put(*out, ai->allocationSize);
        return r;
    }
    VkResult r = real_AllocMem(dev, ai, ac, out);
    if (r == VK_SUCCESS && out && *out) size_put(*out, ai->allocationSize);
    return r;
}

/* 6) 映射：优先用驱动原生结果；若高位或失败，则经 dmabuf 落到 2~4GB */
static VkResult shim_MapMem(VkDevice dev, VkDeviceMemory mem, VkDeviceSize off,
                            VkDeviceSize size, VkFlags flags, void** pp)
{
    load_drv(); resolve_dev(dev);
    if (!real_MapMem) return -2;

    void* rp = NULL;
    VkResult r = real_MapMem(dev, mem, off, size, flags, &rp);
    if (r != VK_SUCCESS) return r;

    /* 驱动已经给了低 4GB 地址，直接用 */
    if (rp && (uintptr_t)rp < LOW4_END) { *pp = rp; return VK_SUCCESS; }

    /* 否则经 dmabuf 重新映射 */
    if (!real_GetFd) { *pp = rp; return VK_SUCCESS; }
    VkMemoryGetFdInfoKHR fi;
    memset(&fi, 0, sizeof(fi));
    fi.sType = ST_MEMORY_GET_FD_INFO_KHR;
    fi.memory = mem;
    fi.handleType = HANDLE_TYPE_DMA_BUF;
    int fd = -1;
    if (real_GetFd(dev, &fi, &fd) != VK_SUCCESS || fd < 0) { *pp = rp; return VK_SUCCESS; }

    VkDeviceSize sz = (size == ~(VkDeviceSize)0) ? 0 : size;
    if (sz == 0) sz = size_get(mem);                       /* VK_WHOLE_SIZE：查分配时登记的大小 */
    if (sz == 0) { close(fd); *pp = rp; return VK_SUCCESS; }
    void* lp = map_into_low4((size_t)sz, PROT_READ | PROT_WRITE, fd, (off_t)off);
    if (lp == MAP_FAILED) {
        close(fd);
        *pp = rp;                       /* 回退：让上层按原样处理（32 位下可能失败，但保留信息） */
        return VK_SUCCESS;
    }
    map_add(dev, mem, rp, lp, (size_t)sz, fd);
    fprintf(stderr, "[gf-iso] map 重定位: 0x%p -> 0x%p (%zu B)\n", rp, lp, (size_t)sz);
    *pp = lp;
    return VK_SUCCESS;
}

/* 7) 解除映射 / 释放 */
static void shim_UnmapMem(VkDevice dev, VkDeviceMemory mem)
{
    load_drv(); resolve_dev(dev);
    MapEntry* e = map_find(dev, mem);
    if (e) { munmap(e->low_ptr, e->size); close(e->fd); map_del(e); }
    if (real_UnmapMem) real_UnmapMem(dev, mem);
}
static void shim_FreeMem(VkDevice dev, VkDeviceMemory mem, const VkAllocationCallbacks* ac)
{
    load_drv(); resolve_dev(dev);
    MapEntry* e = map_find(dev, mem);
    if (e) { munmap(e->low_ptr, e->size); close(e->fd); map_del(e); }
    if (real_FreeMem) real_FreeMem(dev, mem, ac);
}

/* ---------------- 实例创建：过滤 gf 不支持的扩展 ---------------- */
typedef struct VkApplicationInfo_ { uint32_t sType; const void* pNext;
                                    const char* pApplicationName; uint32_t applicationVersion;
                                    const char* pEngineName; uint32_t engineVersion;
                                    uint32_t apiVersion; } VkApplicationInfo;
typedef struct { uint32_t sType; const void* pNext; uint32_t flags;
                 const VkApplicationInfo* pApplicationInfo;
                 uint32_t enabledLayerCount; const char* const* ppEnabledLayerNames;
                 uint32_t enabledExtensionCount; const char* const* ppEnabledExtensionNames;
               } VkInstanceCreateInfo;

static VkResult shim_CreateInstance(const void* pCI, const void* pAlloc, VkInstance* pInst)
{
    load_drv();
    if (!real_CreateInstance) return -9;
    if (!pCI) return real_CreateInstance(pCI, pAlloc, pInst);

    const VkInstanceCreateInfo* ci = (const VkInstanceCreateInfo*)pCI;
    /* 先取 gf 真正支持的实例扩展 */
    uint32_t navail = 0;
    if (g_gipa) {
        PFN_vkVoidFunction f = g_gipa(NULL, "vkEnumerateInstanceExtensionProperties");
        if (f) ((VkResult(*)(const char*, uint32_t*, void*))f)(NULL, &navail, NULL);
    }
    VkExtensionProperties* avail = NULL;
    if (navail) {
        avail = (VkExtensionProperties*)calloc(navail, sizeof(*avail));
        PFN_vkVoidFunction f = g_gipa ? g_gipa(NULL, "vkEnumerateInstanceExtensionProperties") : NULL;
        if (f) ((VkResult(*)(const char*, uint32_t*, void*))f)(NULL, &navail, avail);
    }
    /* 只保留 gf 支持的（缺了这步，gf 会对不认识的扩展返回 -7 FEATURE_NOT_PRESENT） */
    const char** keep = (const char**)calloc(ci->enabledExtensionCount + 1, sizeof(char*));
    uint32_t n = 0;
    for (uint32_t i = 0; i < ci->enabledExtensionCount; i++) {
        const char* e = ci->ppEnabledExtensionNames ? ci->ppEnabledExtensionNames[i] : NULL;
        if (!e) continue;
        int supported = 0;
        for (uint32_t j = 0; j < navail; j++)
            if (!strcmp(e, avail[j].extensionName)) { supported = 1; break; }
        if (supported) keep[n++] = e;
        else fprintf(stderr, "[gf-iso] 剔除 gf 不支持的实例扩展: %s\n", e);
    }
    VkInstanceCreateInfo c2 = *ci;
    c2.enabledExtensionCount = n;
    c2.ppEnabledExtensionNames = keep;
    fprintf(stderr, "[gf-iso] vkCreateInstance: 请求 %u -> 保留 %u\n", ci->enabledExtensionCount, n);
    VkResult r = real_CreateInstance(&c2, pAlloc, pInst);
    free(keep); free(avail);
    fprintf(stderr, "[gf-iso] vkCreateInstance -> r=%d inst=%p\n", r, (void*)(pInst ? *pInst : NULL));
    return r;
}

/* ---------------- ICD 入口 ---------------- */
/*
 * 注意：这里【不拦截】vkCreateInstance。
 * vkfix（可用的参考实现）也不拦截它。拦截后 wine 虽能取到指针却不调用，
 * 反而使设备创建流程不再推进；实例扩展的兼容性问题交由 loader 处理即可。
 */
/* GF_ISO_MAP_FIX=0 可关掉映射相关的拦截，用于二分定位问题 */
static int mapfix_on(void)
{
    const char* e = getenv("GF_ISO_MAP_FIX");
    return !(e && !strcmp(e, "0"));
}

static int intercepted(const char* n)
{
    /*
     * 注意：不要拦截 vkGetPhysicalDeviceMemoryProperties。
     * 它是实例级函数，经 gipa 拦截后实测会导致设备创建流程不再推进
     * （日志表现为只查询 vkCreateInstance 却从不调用）。
     */
    if (!strcmp(n, "vkEnumerateDeviceExtensionProperties")) return 1;
    if (!strcmp(n, "vkGetPhysicalDeviceFeatures2"))         return 1;
    if (!strcmp(n, "vkCreateDevice"))                       return 1;
    if (mapfix_on() && (!strcmp(n, "vkAllocateMemory") ||
                        !strcmp(n, "vkMapMemory")      ||
                        !strcmp(n, "vkUnmapMemory")    ||
                        !strcmp(n, "vkFreeMemory")))        return 1;
    return 0;
}
static PFN_vkVoidFunction pick(const char* n)
{
    if (!strcmp(n, "vkCreateInstance"))                     return (PFN_vkVoidFunction)shim_CreateInstance;
    
    if (!strcmp(n, "vkEnumerateDeviceExtensionProperties")) return (PFN_vkVoidFunction)shim_EnumDevExt;
    if (!strcmp(n, "vkGetPhysicalDeviceFeatures2"))         return (PFN_vkVoidFunction)shim_GetFeat2;
    if (!strcmp(n, "vkCreateDevice"))                       return (PFN_vkVoidFunction)shim_CreateDevice;
    if (!strcmp(n, "vkAllocateMemory"))                     return (PFN_vkVoidFunction)shim_AllocMem;
    if (!strcmp(n, "vkMapMemory"))                          return (PFN_vkVoidFunction)shim_MapMem;
    if (!strcmp(n, "vkUnmapMemory"))                        return (PFN_vkVoidFunction)shim_UnmapMem;
    if (!strcmp(n, "vkFreeMemory"))                         return (PFN_vkVoidFunction)shim_FreeMem;
    return NULL;
}

__attribute__((visibility("default")))
PFN_vkVoidFunction vk_icdGetInstanceProcAddr(VkInstance inst, const char* pName)
{
    load_drv();
    if (intercepted(pName)) {
        fprintf(stderr, "[gf-iso] gipa(%s) -> 拦截\n", pName);
        return pick(pName);
    }
    /*
     * 关键：未拦截的函数必须返回 NULL，让 loader 走它自己的默认实现。
     * 参考 vkfix 的 my_gap_dispatch：它对不关心的名字一律返回 0。
     * 若此处把 loader 的 vkGetInstanceProcAddr 结果再转发出去，
     * 会打乱 loader 的调度，导致设备创建流程停摆（只查询、不调用）。
     */
    fprintf(stderr, "[gf-iso] gipa(inst=%p, %s) -> NULL(交由 loader)\n", (void*)inst, pName);
    return NULL;
}
__attribute__((visibility("default")))
PFN_vkVoidFunction vk_icdGetPhysicalDeviceProcAddr(VkInstance inst, const char* pName)
{
    load_drv();
    if (intercepted(pName)) return pick(pName);
    if (g_gipa) return g_gipa(inst, pName);
    return NULL;
}
__attribute__((visibility("default")))
VkResult vk_icdNegotiateLoaderICDInterfaceVersion(uint32_t* v)
{
    if (v && *v > 6) *v = 6;
    return VK_SUCCESS;
}
/*
 * 注意：这里【不要导出】vkGetDeviceProcAddr。
 * loader 自身也导出同名符号，ICD 再导出一个会造成符号冲突，
 * 实测会让设备创建流程停摆。该函数仅作内部使用即可。
 */
__attribute__((visibility("hidden")))
PFN_vkVoidFunction shim_internal_GetDeviceProcAddr(VkDevice dev, const char* pName)
{
    load_drv(); resolve_dev(dev);
    if (intercepted(pName)) return pick(pName);
    if (g_gdpa) return g_gdpa(dev, pName);
    return NULL;
}
