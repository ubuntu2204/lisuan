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
		 u32 qfCount; u32 *pQf; int initialLayout; } ImageCreateInfo;
typedef struct { int sType; void *pNext; char *app; u32 appVer; char *eng;
		 u32 engVer; u32 apiVer; } AppInfo;
typedef struct { int sType; void *pNext; u32 flags; AppInfo *pApp; u32 layerCount;
		 void *pLayers; u32 extCount; void *pExts; } InstanceCreateInfo;
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

int main(void)
{
	void *vk, *inst = 0, *devl = 0, *img, *mem, *devs[4];
	void *phys;
	int (*CreateInstance)(const InstanceCreateInfo *, const void *, void **);
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

	setbuf(stdout, 0);	/* 崩溃时不丢输出 */
	vk = dlopen("libvulkan.so.1", 2 /*RTLD_NOW*/);
	if (!vk) { puts("dlopen libvulkan.so.1 失败"); return 1; }

	SYM(CreateInstance, "vkCreateInstance");
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
	InstanceCreateInfo ci;
	memset(&ci, 0, sizeof(ci));
	ci.sType = 1; ci.pApp = &ai;
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
	DeviceCreateInfo dci;
	memset(&dci, 0, sizeof(dci));
	dci.sType = 3; dci.qciCount = 1; dci.pQcis = &qci;
	rc = CreateDevice(phys, &dci, 0, &devl);
	if (rc) { printf("vkCreateDevice rc=%d\n", rc); return 1; }
	printf("logical device 已建 (graphics queue family #%d)\n\n", qidx);

	MemProps mp;
	memset(&mp, 0, sizeof(mp));
	GetPhysicalDeviceMemoryProperties(phys, &mp);

	for (f = 0; f < MAXF; f++) {
		FormatProps fp;
		memset(&fp, 0, sizeof(fp));
		GetPhysicalDeviceFormatProperties(phys, FORMATS[f].fmt, &fp);
		printf("== %s (VkFormat=%d) ==\n", FORMATS[f].name, FORMATS[f].fmt);
		printf("   optimal=0x%x  DEPTH_STENCIL=%d COLOR=%d\n", fp.optimal,
		       (fp.optimal & F_DEPTH_STENCIL_ATTACHMENT) ? 1 : 0,
		       (fp.optimal & F_COLOR_ATTACHMENT) ? 1 : 0);
		for (u = 0; u < 4; u++) {
			ImageCreateInfo ici;
			memset(&ici, 0, sizeof(ici));
			ici.sType = 9; ici.imageType = 2; ici.format = FORMATS[f].fmt;
			ici.extent.w = 400; ici.extent.h = 400; ici.extent.d = 1;
			ici.tiling = 1; ici.usage = USAGES[u];
			img = 0;
			rc = CreateImage(devl, &ici, 0, &img);
			if (rc) {
				printf("   %-42s createImage 失败 rc=%d\n", UNAMES[u], rc);
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
				printf("   %-42s createImage 成功，无匹配 memoryType\n", UNAMES[u]);
			} else {
				MemAllocateInfo mai;
				memset(&mai, 0, sizeof(mai));
				mai.sType = 5; mai.allocSize = req.size; mai.typeIndex = (u32)mi;
				mem = 0;
				rc = AllocateMemory(devl, &mai, 0, &mem);
				if (rc) {
					printf("   %-42s createImage 成功，allocMemory 失败 rc=%d\n",
					       UNAMES[u], rc);
				} else {
					rc = BindImageMemory(devl, img, mem, 0);
					printf("   %-42s %s (size=%llu mtype=%d)\n", UNAMES[u],
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
