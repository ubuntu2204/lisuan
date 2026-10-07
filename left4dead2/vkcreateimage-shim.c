/*
 * vkcreateimage-shim.c — 32 位 Venus 客户端侧 vkCreateImage 取证 / 改写 shim
 *
 * 背景：L4D2 日志稳定出现
 *   err: D3D9: Cannot create texture: ... Format: D3D9Format::D16  400x400
 * （L4D2 手电筒阴影贴图；日志统计：整局只缺这 1 个特性，其余特性均正常）。
 * 已知：gf 原生 64 位能正确上报 D16 的 DEPTH_STENCIL_ATTACHMENT（optimal=0x1de01），
 *       vkCreateImage+AllocateMemory+BindImageMemory 全部成功；
 *       32 位经 Venus 查询能力也正确（optimal=0x1de01, DEPTH_STENCIL=1）。
 * 未知：游戏到底用什么 format/tiling/usage 请求 D16，失败返回码又是什么。
 *
 * 本 shim 做两件事：
 *   1) 取证：把每次 vkCreateImage 的 format/tiling/usage/flags/extent/
 *      mipLevels/arrayLayers/samples/返回码/调用者地址写入 $L4D2_VKCI_LOG
 *      （默认 /tmp/l4d2-vkCreateImage.log）。默认只记深度类格式
 *      （124/125/126/129/130），设 L4D2_VKCI_ALL=1 记全部。
 *   2) 改写（可选，默认关）：L4D2_VKCI_TILING=<0|1> 强制改 tiling；
 *      L4D2_VKCI_USAGE_MASK=<hex> 把 usage 按位与掩码（剥离可疑 usage 位）。
 *
 * 为什么连 gipa/gdpa 一起拦：现代 Vulkan 程序（Valve 的 shaderapivk.so）通常用
 * vkGetDeviceProcAddr 取函数指针，单纯 LD_PRELOAD 定义 vkCreateImage 拦不到。
 *
 * 关闭：L4D2_NO_VKCI_SHIM=1（或不注入 LD_PRELOAD）
 *
 * 构建（不依赖 libc6-dev-i386；源文件不 include 任何头文件）：
 *   gcc -m32 -fPIC -O2 -fno-stack-protector -c vkcreateimage-shim.c -o vkci.o
 *   ld -m elf_i386 -shared -o vkcreateimage-shim.so vkci.o
 */

extern void *dlsym(void *h, const char *name);
extern char *getenv(const char *name);
extern int openat(int dirfd, const char *path, int flags, ...);
extern int close(int fd);
extern int write(int fd, const void *buf, unsigned len);
extern unsigned long strtoul(const char *s, char **end, int base);

#define RTLD_NEXT ((void *)-1)
#define AT_FDCWD (-100)
#define O_WRONLY 01
#define O_CREAT 0100
#define O_APPEND 02000
#define O_CLOEXEC 02000000

typedef unsigned int u32;
typedef int i32;
typedef void *VkDevice;
typedef i32 VkResult;

typedef struct {
	i32 sType;
	const void *pNext;
	u32 flags;
	i32 imageType;
	i32 format;
	u32 width, height, depth;
	i32 tiling;
	u32 usage;
	i32 sharingMode;
	u32 mipLevels;
	u32 arrayLayers;
	i32 samples;
	u32 qfCount;
	const u32 *pQf;
	i32 initialLayout;
} VkImageCreateInfo;

static VkResult (*real_create)(VkDevice, const VkImageCreateInfo *, const void *, void **);
static void *(*real_gipa)(void *, const char *);
static void *(*real_gdpa)(VkDevice, const char *);
static int in_shim;
static int logged;

#define MAXLOG 400

/* 极简日志；lbuf 全局零初始化，lp() 里始终补 NUL 终止 */
static char lbuf[1200];
static unsigned ln;

static void lp(const char *s)
{
	while (*s && ln < sizeof(lbuf) - 1)
		lbuf[ln++] = *s++;
	lbuf[ln] = 0;
}
static void lnum(long v)
{
	char t[24];
	int n = 0, i;
	unsigned long u = v < 0 ? (unsigned long)-v : (unsigned long)v;
	do {
		t[n++] = (char)('0' + u % 10);
		u /= 10;
	} while (u);
	t[n] = 0;
	if (v < 0)
		lp("-");
	for (i = n; i > 0; i--) {
		char c = t[i - 1];
		lp(&c);
	}
}
static void lhex(unsigned long v)
{
	static const char h[] = "0123456789abcdef";
	char t[20];
	int n = 0, i;
	do {
		t[n++] = h[v & 15];
		v >>= 4;
	} while (v);
	t[n] = 0;
	lp("0x");
	for (i = n; i > 0; i--) {
		char c = t[i - 1];
		lp(&c);
	}
}
static int seq(const char *a, const char *b)
{
	while (*a && *a == *b) {
		a++;
		b++;
	}
	return *a == *b;
}
static void flush_log(void)
{
	const char *p = getenv("L4D2_VKCI_LOG");
	int fd = openat(AT_FDCWD, p ? p : "/tmp/l4d2-vkCreateImage.log",
			O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
	if (fd >= 0) {
		write(fd, lbuf, ln);
		close(fd);
	}
	ln = 0;
	lbuf[0] = 0;
}
static void logline(const VkImageCreateInfo *ci, VkResult r, unsigned long ra)
{
	lp("createImage fmt=");
	lnum(ci->format);
	lp(" tiling=");
	lp(ci->tiling == 0 ? "LINEAR" : ci->tiling == 1 ? "OPTIMAL" : "OTHER");
	lp(" usage=");
	lhex(ci->usage);
	lp(" flags=");
	lhex(ci->flags);
	lp(" type=");
	lnum(ci->imageType);
	lp(" ext=");
	lnum(ci->width);
	lp("x");
	lnum(ci->height);
	lp(" mip=");
	lnum(ci->mipLevels);
	lp(" arr=");
	lnum(ci->arrayLayers);
	lp(" samples=");
	lnum(ci->samples);
	lp(" rc=");
	lnum(r);
	lp(" ra=");
	lhex(ra);
	lp("\n");
	flush_log();
}
static void load_real(void)
{
	if (!real_create)
		real_create = dlsym(RTLD_NEXT, "vkCreateImage");
	if (!real_gipa)
		real_gipa = dlsym(RTLD_NEXT, "vkGetInstanceProcAddr");
	if (!real_gdpa)
		real_gdpa = dlsym(RTLD_NEXT, "vkGetDeviceProcAddr");
}
static void apply_mod(VkImageCreateInfo *mod, const VkImageCreateInfo *ci)
{
	const char *e;
	*mod = *ci;
	e = getenv("L4D2_VKCI_TILING");
	if (e && *e)
		mod->tiling = (i32)strtoul(e, 0, 0);
	e = getenv("L4D2_VKCI_USAGE_MASK");
	if (e && *e)
		mod->usage &= (u32)strtoul(e, 0, 0);
}

/* 深度类格式：124 D16_UNORM / 125 D16_SFLOAT / 126 D32_SFLOAT
 *              129 X8_D24_UNORM_PACK32 / 130 D24_UNORM_S8_UINT */
static int want_log(i32 fmt)
{
	const char *a = getenv("L4D2_VKCI_ALL");
	if (a && *a == '1')
		return 1;
	return fmt == 124 || fmt == 125 || fmt == 126 || fmt == 129 || fmt == 130;
}

static int need_mod(void)
{
	const char *e;
	e = getenv("L4D2_VKCI_TILING");
	if (e && *e)
		return 1;
	e = getenv("L4D2_VKCI_USAGE_MASK");
	if (e && *e)
		return 1;
	return 0;
}

static VkResult my_create(VkDevice dev, const VkImageCreateInfo *ci,
			  const void *alloc, void **img)
{
	VkResult r;
	unsigned long ra = (unsigned long)(void *)__builtin_return_address(0);
	VkImageCreateInfo mod;
	int nm;

	load_real();
	if (in_shim)
		return real_create(dev, ci, alloc, img);

	nm = need_mod();
	if (nm) {
		apply_mod(&mod, ci);
		r = real_create(dev, &mod, alloc, img);
	} else {
		r = real_create(dev, ci, alloc, img);
	}
	if (want_log(ci->format) && logged < MAXLOG) {
		logged++;
		logline(nm ? &mod : ci, r, ra);
	}
	return r;
}

/* 用 gipa/gdpa 取到 vkCreateImage 的调用方，改拿我们的包装 */
void *vkGetInstanceProcAddr(void *inst, const char *name);
void *vkGetDeviceProcAddr(VkDevice dev, const char *name);

void *vkGetInstanceProcAddr(void *inst, const char *name)
{
	void *f;
	load_real();
	f = real_gipa(inst, name);
	if (f && name && !seq(name, "vkGetInstanceProcAddr")) {
		const char *t = name;
		if (t[0] == 'v' && t[1] == 'k' && seq(t + 2, "CreateImage"))
			return (void *)my_create;
	}
	return f;
}

void *vkGetDeviceProcAddr(VkDevice dev, const char *name)
{
	void *f;
	load_real();
	f = real_gdpa(dev, name);
	if (f && name && !seq(name, "vkGetDeviceProcAddr")) {
		const char *t = name;
		if (t[0] == 'v' && t[1] == 'k' && seq(t + 2, "CreateImage"))
			return (void *)my_create;
	}
	return f;
}

/* 走 PLT 直接调用 vkCreateImage 的程序也会命中这里 */
VkResult vkCreateImage(VkDevice dev, const VkImageCreateInfo *ci,
		       const void *alloc, void **img);
VkResult vkCreateImage(VkDevice dev, const VkImageCreateInfo *ci,
		       const void *alloc, void **img)
{
	load_real();
	if (in_shim)
		return real_create(dev, ci, alloc, img);
	return my_create(dev, ci, alloc, img);
}
