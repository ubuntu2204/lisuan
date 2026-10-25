/*
 * vk32.c — 32 位客户端「经 Venus socket」实测深度附件能力，定位 D16 失败究竟发生在
 *          gf 驱动 / Venus 服务端 / 上层中的哪一层。
 *
 * 为什么不用 include：系统没装 libc6-dev-i386，结构体与函数全部自己声明。
 *
 * 用法（必须与 l4d2-host-run.sh 相同的 Venus 环境）：
 *   VN_DEBUG=vtest VTEST_SOCKET_NAME=$XDG_RUNTIME_DIR/l4d2-venus.sock \
 *   VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/virtio_icd.json ./vk32
 *
 * 构建：
 *   gcc -m32 -fno-stack-protector -O2 -c vk32.c -o vk32.o
 *   ld -m elf_i386 -dynamic-linker /lib/ld-linux.so.2 -e main vk32.o \
 *      /usr/lib/i386-linux-gnu/libc.so.6 -o vk32
 */

extern void *dlopen(const char *path, int flags);
extern void *dlsym(void *h, const char *name);
extern int printf(const char *fmt, ...);
extern int puts(const char *s);
extern void *memset(void *s, int c, unsigned long n);
extern void *stdout;
extern int setbuf(void *stream, int mode);
extern unsigned long strtoul(const char *s, char **end, int base);
extern void *malloc(unsigned long n);
extern void free(void *p);
extern char *strncpy(char *d, const char *s, unsigned long n);

/* VkFormatFeatureFlagBits（规范取值，勿错位：漏掉 VERTEX_BUFFER=0x40 会整体偏移一位） */
#define F_COLOR_ATTACHMENT         0x80
#define F_DEPTH_STENCIL_ATTACHMENT 0x200
#define F_BLIT_SRC                 0x400
#define F_BLIT_DST                 0x800

typedef unsigned int u32;
typedef unsigned long long u64;

typedef struct { u32 linear, optimal, buffer; } FormatProps;
typedef struct { u32 w, h, d; } Extent3D;
typedef struct { int sType; void *pNext; u32 flags; int imageType; int format;
		 Extent3D extent; int tiling; u32 usage; int sharingMode;
		 u32 mipLevels; u32 arrayLayers; int samples;
		 u32 qfCount; u32 *pQf; int initialLayout; } ImageCreateInfo;
typedef struct { int sType; void *pNext; char *app; u32 appVer; char *eng;
		 u32 engVer; u32 apiVer; } AppInfo;
typedef struct { int sType; void *pNext; u32 flags; AppInfo *pApp; u32 layerCount;
		 void *pLayers; u32 extCount; void *pExts; } InstanceCreateInfo;
typedef struct { char name[256]; u32 specVersion; } ExtProps;
typedef struct { u32 qFlags, qCount, tsBits; Extent3D gran; } QueueFamilyProps;
typedef struct { int sType; void *pNext; u32 flags; u32 qfIndex; u32 qCount;
		 float *pPrio; } DeviceQueueCreateInfo;
typedef struct { int sType; void *pNext; u32 flags; u32 qciCount; void *pQcis;
		 u32 layerCount; void *pLayers; u32 extCount; void *pExts;
		 void *pFeat; } DeviceCreateInfo;
typedef struct { u64 size, alignment; u32 typeBits; } MemRequirements;
typedef struct { int sType; void *pNext; u64 allocSize; u32 typeIndex; } MemAllocateInfo;
typedef struct { u32 propFlags, heapIndex; } MemType;
typedef struct { u32 typeCount; MemType types[32]; u32 heapCount;
		 unsigned char heaps[256]; } MemProps;

#define MAXF 3
static struct { const char *name; int fmt; } FORMATS[] = {
	{ "D16_UNORM", 124 },
	{ "D24_UNORM_S8_UINT", 130 },
	{ "R8G8B8A8_UNORM", 37 },
};
static const u32 USAGES[] = { 0x8, 0x8 | 0x1, 0x10, 0x1000 };
static const char *UNAMES[] = { "DEPTH_STENCIL_ATTACHMENT",
				"DEPTH_STENCIL_ATTACHMENT|SAMPLED_IMAGE",
				"COLOR_ATTACHMENT", "TRANSFER_DST" };

#define SYM(v, nm) do { *(void **)(&v) = dlsym(vk, nm); \
			if (!v) { printf("缺符号 %s\n", nm); return 1; } } while (0)

int main(int argc, char **argv)
{
	void *vk, *inst = 0, *devl = 0, *img, *mem, *devs[4];
	void *phys;
	int (*CreateInstance)(const InstanceCreateInfo *, const void *, void **);
	int (*EnumerateInstanceExtensionProperties)(const char *, u32 *, ExtProps *);
	int (*EnumerateDeviceExtensionProperties)(void *, const char *, u32 *, ExtProps *);
	int (*EnumeratePhysicalDevices)(void *, u32 *, void **);
	void (*GetPhysicalDeviceProperties)(void *, void *);
	void (*GetPhysicalDeviceFormatProperties)(void *, int, FormatProps *);
	void (*GetPhysicalDeviceMemoryProperties)(void *, MemProps *);
	void (*GetPhysicalDeviceQueueFamilyProperties)(void *, u32 *, QueueFamilyProps *);
	int (*CreateDevice)(void *, const DeviceCreateInfo *, const void *, void **);
	int (*CreateImage)(void *, const ImageCreateInfo *, const void *, void **);
	void (*GetImageMemoryRequirements)(void *, void *, MemRequirements *);
	int (*AllocateMemory)(void *, const MemAllocateInfo *, const void *, void **);
	int (*BindImageMemory)(void *, void *, void *, u64);
	void (*DestroyImage)(void *, void *, const void *);
	void (*FreeMemory)(void *, void *, const void *);
	u32 n = 0, i;
	int qidx = -1, rc, f, u;
	const char *tn = "OPTIMAL";
	/* 可选参数：<format> <tiling> <usage>；缺省则全量扫描 */
	int selFmt = -1, selTiling = -1;
	u32 selUsage = 0;
	int haveUsage = 0;

	setbuf(stdout, 0);	/* 崩溃时不丢输出 */
	if (argc >= 2) selFmt = (int)strtoul(argv[1], 0, 0);
	if (argc >= 3) selTiling = (int)strtoul(argv[2], 0, 0);
	if (argc >= 4) { selUsage = (u32)strtoul(argv[3], 0, 0); haveUsage = 1; }
	vk = dlopen("libvulkan.so.1", 2 /*RTLD_NOW*/);
	if (!vk) { puts("dlopen libvulkan.so.1 失败"); return 1; }

	SYM(CreateInstance, "vkCreateInstance");
	SYM(EnumerateInstanceExtensionProperties, "vkEnumerateInstanceExtensionProperties");
	SYM(EnumerateDeviceExtensionProperties, "vkEnumerateDeviceExtensionProperties");
	SYM(EnumeratePhysicalDevices, "vkEnumeratePhysicalDevices");
	SYM(GetPhysicalDeviceProperties, "vkGetPhysicalDeviceProperties");
	SYM(GetPhysicalDeviceFormatProperties, "vkGetPhysicalDeviceFormatProperties");
	SYM(GetPhysicalDeviceMemoryProperties, "vkGetPhysicalDeviceMemoryProperties");
	SYM(GetPhysicalDeviceQueueFamilyProperties, "vkGetPhysicalDeviceQueueFamilyProperties");
	SYM(CreateDevice, "vkCreateDevice");
	SYM(CreateImage, "vkCreateImage");
	SYM(GetImageMemoryRequirements, "vkGetImageMemoryRequirements");
	SYM(AllocateMemory, "vkAllocateMemory");
	SYM(BindImageMemory, "vkBindImageMemory");
	SYM(DestroyImage, "vkDestroyImage");
	SYM(FreeMemory, "vkFreeMemory");

	AppInfo ai = { 0, 0, (char *)"vk32", 0, (char *)"vk32", 0, (1u << 22) | (1u << 12) };
	/* 必须启用实例扩展：Venus 客户端的 image 资源导入路径依赖
	 * VK_KHR_external_memory_fd / VK_EXT_external_memory_dma_buf 等，
	 * 一个都不启用时 vkCreateImage 会走到未初始化状态而 SEGV。 */
	char *exts[128];
	char extbuf[128][256];
	u32 ec = 0, ei = 0;
	if (EnumerateInstanceExtensionProperties(0, &ec, 0) == 0 && ec > 0) {
		ExtProps *props = (ExtProps *)malloc(ec * sizeof(ExtProps));
		if (props) {
			EnumerateInstanceExtensionProperties(0, &ec, props);
			for (ei = 0; ei < ec && ei < 128; ei++) {
				strncpy(extbuf[ei], props[ei].name, 255);
				extbuf[ei][255] = 0;
				exts[ei] = extbuf[ei];
			}
			free(props);
		}
	}
	printf("启用实例扩展 %u 个\n", ei);
	InstanceCreateInfo ci;
	memset(&ci, 0, sizeof(ci));
	ci.sType = 1; ci.pApp = &ai;
	ci.extCount = ei; ci.pExts = ei ? (void *)exts : 0;
	rc = CreateInstance(&ci, 0, &inst);
	if (rc) { printf("vkCreateInstance rc=%d\n", rc); return 1; }

	EnumeratePhysicalDevices(inst, &n, 0);
	printf("物理设备数 %u\n", n);
	if (!n) { puts("枚举不到设备（VN_DEBUG/socket 不对？）"); return 1; }
	n = n > 4 ? 4 : n;
	EnumeratePhysicalDevices(inst, &n, devs);
	phys = devs[0];

	unsigned char props[1024];
	memset(props, 0, sizeof(props));
	GetPhysicalDeviceProperties(phys, props);
	printf("设备名: %s\n", props + 560);

	u32 nq = 0;
	GetPhysicalDeviceQueueFamilyProperties(phys, &nq, 0);
	QueueFamilyProps qf[8];
	if (nq > 8) nq = 8;
	GetPhysicalDeviceQueueFamilyProperties(phys, &nq, qf);
	for (i = 0; i < nq; i++)
		if (qf[i].qFlags & 0x1) { qidx = (int)i; break; }
	if (qidx < 0) { puts("无 graphics 队列族"); return 1; }

	float prio = 1.0f;
	DeviceQueueCreateInfo qci;
	memset(&qci, 0, sizeof(qci));
	qci.sType = 2; qci.qfIndex = (u32)qidx; qci.qCount = 1; qci.pPrio = &prio;
	/* device 扩展同样要全量启用：Venus 的 image 资源导入依赖
	 * VK_KHR_external_memory_fd / VK_EXT_external_memory_dma_buf / VK_KHR_dedicated_allocation，
	 * 不启用时 vkCreateImage 会 SEGV。 */
	char *dexts[128];
	char dextbuf[128][256];
	u32 dec = 0, dei = 0;
	if (EnumerateDeviceExtensionProperties(phys, 0, &dec, 0) == 0 && dec > 0) {
		ExtProps *dprops = (ExtProps *)malloc(dec * sizeof(ExtProps));
		if (dprops) {
			EnumerateDeviceExtensionProperties(phys, 0, &dec, dprops);
			for (dei = 0; dei < dec && dei < 128; dei++) {
				strncpy(dextbuf[dei], dprops[dei].name, 255);
				dextbuf[dei][255] = 0;
				dexts[dei] = dextbuf[dei];
			}
			free(dprops);
		}
	}
	printf("启用 device 扩展 %u 个\n", dei);
	DeviceCreateInfo dci;
	memset(&dci, 0, sizeof(dci));
	dci.sType = 3; dci.qciCount = 1; dci.pQcis = &qci;
	dci.extCount = dei; dci.pExts = dei ? (void *)dexts : 0;
	rc = CreateDevice(phys, &dci, 0, &devl);
	if (rc) { printf("vkCreateDevice rc=%d\n", rc); return 1; }
	printf("logical device 已建 (graphics queue family #%d)\n\n", qidx);

	MemProps mp;
	memset(&mp, 0, sizeof(mp));
	GetPhysicalDeviceMemoryProperties(phys, &mp);

	for (f = 0; f < MAXF; f++) {
		FormatProps fp;
		if (selFmt >= 0 && FORMATS[f].fmt != selFmt)
			continue;
		memset(&fp, 0, sizeof(fp));
		GetPhysicalDeviceFormatProperties(phys, FORMATS[f].fmt, &fp);
		printf("== %s (VkFormat=%d) ==\n", FORMATS[f].name, FORMATS[f].fmt);
		printf("   optimal=0x%x  DEPTH_STENCIL=%d COLOR=%d\n", fp.optimal,
		       (fp.optimal & F_DEPTH_STENCIL_ATTACHMENT) ? 1 : 0,
		       (fp.optimal & F_COLOR_ATTACHMENT) ? 1 : 0);
		for (u = 0; u < 4; u++) {
			ImageCreateInfo ici;
			u32 usage = USAGES[u];
			if (haveUsage && usage != selUsage)
				continue;
			memset(&ici, 0, sizeof(ici));
			ici.sType = 9; ici.imageType = 2; ici.format = FORMATS[f].fmt;
			ici.extent.w = 400; ici.extent.h = 400; ici.extent.d = 1;
			ici.tiling = selTiling >= 0 ? selTiling : 1;
			ici.usage = usage;
			/* 这三个字段漏掉会传 0（mipLevels=0 非法），Venus 客户端会野指针崩 */
			ici.mipLevels = 1;
			ici.arrayLayers = 1;
			ici.samples = 1;	/* VK_SAMPLE_COUNT_1_BIT */
			tn = ici.tiling == 0 ? "LINEAR  " : "OPTIMAL";
			img = 0;
			rc = CreateImage(devl, &ici, 0, &img);
			if (rc) {
				printf("   %-42s tiling=%s createImage 失败 rc=%d\n", UNAMES[u], tn, rc);
				continue;
			}
			MemRequirements req;
			memset(&req, 0, sizeof(req));
			GetImageMemoryRequirements(devl, img, &req);
			int mi = -1;
			for (i = 0; i < mp.typeCount; i++)
				if ((req.typeBits & (1u << i)) && (mp.types[i].propFlags & 1)) {
					mi = (int)i;
					break;
				}
			if (mi < 0) {
				printf("   %-42s tiling=%s createImage 成功，无匹配 memoryType\n", UNAMES[u], tn);
			} else {
				MemAllocateInfo mai;
				memset(&mai, 0, sizeof(mai));
				mai.sType = 5; mai.allocSize = req.size; mai.typeIndex = (u32)mi;
				mem = 0;
				rc = AllocateMemory(devl, &mai, 0, &mem);
				if (rc) {
					printf("   %-42s tiling=%s createImage 成功，allocMemory 失败 rc=%d\n",
					       UNAMES[u], tn, rc);
				} else {
					rc = BindImageMemory(devl, img, mem, 0);
					printf("   %-42s tiling=%s %s (size=%llu mtype=%d)\n", UNAMES[u], tn,
					       rc ? "bind 失败" : "完整可用",
					       (unsigned long long)req.size, mi);
					FreeMemory(devl, mem, 0);
				}
			}
			DestroyImage(devl, img, 0);
		}
	}
	return 0;
}
