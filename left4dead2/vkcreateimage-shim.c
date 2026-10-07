/*
 * vkcreateimage-shim.c — 32 位 Venus 客户端侧 vkCreateImage 拦截：D16 失败降级 + 取证
 *
 * 背景（2026-10-07 实测链）：
 *   1. L4D2 日志：err: D3D9: Cannot create texture / Format: D3D9Format::D16 /
 *      Extent 400x400x1 / Levels 1 / Layers 1 —— 全游戏只有手电筒阴影这一处用 D16。
 *   2. gf 原生 64 位：vkGetPhysicalDeviceFormatProperties(D16).optimal=0x1de01 含
 *      DEPTH_STENCIL_ATTACHMENT，且 createImage+alloc+bind 全成功 -> 驱动没缺能力。
 *   3. 32 位经 Venus 查询能力同样正确（optimal=0x1de01, DEPTH_STENCIL=1）-> 转述也没丢。
 *   4. 但解除 `+r_flashlightdepthtexture 0` 规避后，游戏**第一次** D16 创建失败就
 *      SIGSEGV（status=139，core dumped）；带规避时同一错误出现 12 次也不崩。
 *   => D16 创建失败是致命的，必须让它「不失败」。
 *
 * 本 shim 做什么：
 *   拦截 vkCreateImage，对 D16_UNORM 请求做**降级链**，任何一档成功即返回，
 *   全失败也返回最后一个（1x1 RGBA8，游戏主力格式，几乎必然成功）：
 *       D16 原样 -> D24_UNORM_S8_UINT -> D32_SFLOAT -> D16@1x1 -> RGBA8@1x1
 *   最坏情况 = 拿到 1x1 占位图 = 手电筒没有阴影，但游戏不崩（等于旧的规避方案）。
 *   全过程写日志：$L4D2_VKCI_LOG（默认 /tmp/l4d2-vkCreateImage.log）。
 *
 * 为什么必须拦 dlsym：实测 shaderapivk.so 用 dlsym(handle,"vkGetDeviceProcAddr")
 * 取函数指针，而 dlsym(handle,...) **不查 LD_PRELOAD 的库**，只替换
 * vkGetDeviceProcAddr 符号拦不到（本版上线后日志为空证实）。故同时拦 dlsym：
 * 名字命中 vkGetInstanceProcAddr / vkGetDeviceProcAddr / vkCreateImage 时返回我们的包装。
 * dlsym 是全局符号，风险面较大，故可用 L4D2_NO_DLSYM_HOOK=1 单独关掉。
 *
 * 开关：
 *   L4D2_D16_PROBE=1        启用降级（需配合启动器解除 r_flashlightdepthtexture 0）
 *   L4D2_NO_VKCI_SHIM=1     完全关闭本 shim
 *   L4D2_NO_DLSYM_HOOK=1    只关 dlsym 拦截
 *   L4D2_VKCI_ALL=1         记录所有格式（默认只记深度类 124/125/126/129/130）
 *
 * 构建（不依赖 libc6-dev-i386；源文件不 include 任何头文件）：
 *   gcc -m32 -fPIC -O2 -fno-stack-protector -c vkcreateimage-shim.c -o vkci.o
 *   ld -m elf_i386 -shared -o vkcreateimage-shim.so vkci.o
 */

extern void *dlsym(void *h, const char *name);
extern void *dlvsym(void *h, const char *name, const char *ver);
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

/* VkFormat */
#define FMT_R8G8B8A8_UNORM      37
#define FMT_D16_UNORM          124
#define FMT_D16_SFLOAT         125
#define FMT_D32_SFLOAT         126
#define FMT_X8_D24_UNORM_PACK32 129
#define FMT_D24_UNORM_S8_UINT  130
#define FMT_B8G8R8A8_UNORM     44

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
static void *(*real_dlsym)(void *, const char *);
static int in_shim;
static int logged;
static int env_probe;
static int env_nodlsym;
static int env_all;
static int env_inited;

#define MAXLOG 400

/* 极简日志；lbuf 全局零初始化，lp() 始终补 NUL */
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
static const char *fmt_name(i32 f)
{
	switch (f) {
	case FMT_R8G8B8A8_UNORM: return "RGBA8";
	case FMT_B8G8R8A8_UNORM: return "BGRA8";
	case FMT_D16_UNORM: return "D16";
	case FMT_D16_SFLOAT: return "D16F";
	case FMT_D32_SFLOAT: return "D32F";
	case FMT_X8_D24_UNORM_PACK32: return "X8D24";
	case FMT_D24_UNORM_S8_UINT: return "D24S8";
	}
	return "?";
}
static void log_try(const char *tag, const VkImageCreateInfo *ci, VkResult r,
		    unsigned long ra)
{
	lp(tag);
	lp(" fmt=");
	lp(fmt_name(ci->format));
	lp("(");
	lnum(ci->format);
	lp(")");
	lp(" tiling=");
	lp(ci->tiling == 0 ? "LINEAR" : ci->tiling == 1 ? "OPTIMAL" : "OTHER");
	lp(" usage=");
	lhex(ci->usage);
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
static void init_env(void)
{
	const char *e;
	if (env_inited)
		return;
	env_inited = 1;
	e = getenv("L4D2_D16_PROBE");
	env_probe = (e && *e == '1') ? 1 : 0;
	e = getenv("L4D2_NO_DLSYM_HOOK");
	env_nodlsym = (e && *e == '1') ? 1 : 0;
	e = getenv("L4D2_VKCI_ALL");
	env_all = (e && *e == '1') ? 1 : 0;
}
static int want_log(i32 fmt)
{
	if (env_all)
		return 1;
	return fmt == FMT_D16_UNORM || fmt == FMT_D16_SFLOAT
	    || fmt == FMT_D32_SFLOAT || fmt == FMT_X8_D24_UNORM_PACK32
	    || fmt == FMT_D24_UNORM_S8_UINT;
}
/* 关键：shim 自己定义了 dlsym，这里【必须】用已解析的真 dlsym（real_dlsym），
 * 否则 shim 内部对 dlsym 的调用会解析到自己 → 无限递归 → 爆栈。
 * 同理 real_dlsym 的首次解析用 dlvsym（它不会走我们自己的 dlsym）。 */
static void load_real(void)
{
	if (!real_dlsym) {
		real_dlsym = dlvsym(RTLD_NEXT, "dlsym", "GLIBC_2.2.5");
		if (!real_dlsym)
			real_dlsym = dlvsym(RTLD_NEXT, "dlsym", "GLIBC_2.0");
	}
	if (!real_dlsym)
		return;
	if (!real_create)
		real_create = real_dlsym(RTLD_NEXT, "vkCreateImage");
	if (!real_gipa)
		real_gipa = real_dlsym(RTLD_NEXT, "vkGetInstanceProcAddr");
	if (!real_gdpa)
		real_gdpa = real_dlsym(RTLD_NEXT, "vkGetDeviceProcAddr");
}

/* ---- D16 降级链 ----
 * 依次尝试，每档成功即返回；全失败也返回最后一档（1x1 RGBA8，游戏主力格式，
 * 几乎必然成功），保证游戏拿到的永远是一个可用的 image —— 这就是「不崩溃」的关键。 */
struct fallback {
	i32 fmt;
	u32 w, h;
	const char *tag;
};
static const struct fallback FALLBACKS[] = {
	{ FMT_D24_UNORM_S8_UINT, 400, 400, "fb-d24s8" },
	{ FMT_D32_SFLOAT, 400, 400, "fb-d32f" },
	{ FMT_D16_UNORM, 1, 1, "fb-d16-1x1" },
	{ FMT_R8G8B8A8_UNORM, 1, 1, "fb-rgba8-1x1" },
};
#define NFALLBACK ((int)(sizeof(FALLBACKS) / sizeof(FALLBACKS[0])))

static VkResult my_create(VkDevice dev, const VkImageCreateInfo *ci,
			  const void *alloc, void **img)
{
	VkResult r;
	unsigned long ra = (unsigned long)(void *)__builtin_return_address(0);
	VkImageCreateInfo mod;
	int i;

	init_env();
	load_real();
	if (in_shim)
		return real_create(dev, ci, alloc, img);

	r = real_create(dev, ci, alloc, img);
	if (want_log(ci->format) && logged < MAXLOG)
		logged++;
	if (want_log(ci->format))
		log_try("try-orig", ci, r, ra);

	/* 只对「游戏请求 D16」且失败时降级，其它格式一律照实返回错误 */
	if (r != 0 && env_probe && ci->format == FMT_D16_UNORM) {
		for (i = 0; i < NFALLBACK; i++) {
			mod = *ci;
			mod.format = FALLBACKS[i].fmt;
			mod.width = FALLBACKS[i].w;
			mod.height = FALLBACKS[i].h;
			/* usage 原样保留：SAMPLED_IMAGE 对所有格式都合法，
			 * 驱动自己会按 format 判定 */
			r = real_create(dev, &mod, alloc, img);
			if (want_log(ci->format))
				log_try(FALLBACKS[i].tag, &mod, r, ra);
			if (r == 0)
				break;
		}
	}
	return r;
}

/* ---- gipa/gdpa 包装：把 vkCreateImage 指向我们 ---- */
void *vkGetInstanceProcAddr(void *inst, const char *name);
void *vkGetDeviceProcAddr(VkDevice dev, const char *name);

void *vkGetInstanceProcAddr(void *inst, const char *name)
{
	void *f;
	init_env();
	load_real();
	f = real_gipa ? real_gipa(inst, name) : 0;
	if (f && name && seq(name, "vkCreateImage"))
		return (void *)my_create;
	return f;
}

void *vkGetDeviceProcAddr(VkDevice dev, const char *name)
{
	void *f;
	init_env();
	load_real();
	f = real_gdpa ? real_gdpa(dev, name) : 0;
	if (f && name && seq(name, "vkCreateImage"))
		return (void *)my_create;
	return f;
}

/* 走 PLT 直接调用 vkCreateImage 的程序也命中这里 */
VkResult vkCreateImage(VkDevice dev, const VkImageCreateInfo *ci,
		       const void *alloc, void **img);
VkResult vkCreateImage(VkDevice dev, const VkImageCreateInfo *ci,
		       const void *alloc, void **img)
{
	if (in_shim) {
		load_real();
		return real_create(dev, ci, alloc, img);
	}
	return my_create(dev, ci, alloc, img);
}

/* ---- dlsym 拦截 ----
 * 实测 shaderapivk.so 用 dlsym(handle, "vkGetDeviceProcAddr") 取函数指针，
 * 而 dlsym(handle,...) 不查 LD_PRELOAD 的库，所以只替换 gipa 符号拦不到。
 * 这里拦 dlsym：名字命中时返回我们的包装，其余一律原样转发。
 * 首次解析用 dlvsym(RTLD_NEXT,...) 避免与自身递归。 */
void *dlsym(void *h, const char *n);
void *dlsym(void *h, const char *n)
{
	void *r;
	init_env();
	load_real();		/* 内部用 dlvsym 解析真 dlsym，不会递归 */
	if (!real_dlsym)
		return 0;
	r = real_dlsym(h, n);
	if (in_shim || env_nodlsym || !n)
		return r;
	if (seq(n, "vkCreateImage"))
		return (void *)my_create;
	if (seq(n, "vkGetDeviceProcAddr"))
		return (void *)vkGetDeviceProcAddr;
	if (seq(n, "vkGetInstanceProcAddr"))
		return (void *)vkGetInstanceProcAddr;
	return r;
}
