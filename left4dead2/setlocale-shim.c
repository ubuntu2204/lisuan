/*
 * setlocale-shim.c — L4D2（32 位）locale 自检误报修复
 *
 * 现象：bin/engine.so 用 GTK 弹
 *   SetLocale('en_US.UTF-8') failed. Using 'LC_CTYPE=C;LC_NUMERIC=en_US.UTF-8;...'.
 *   You may have limited glyph support. Please install 'en_US.UTF-8' locale.
 *
 * 根因（2026-10-07 实证）：
 *   该警告的条件不是「setlocale 失败」，而是【setlocale(LC_ALL,NULL) 的返回值不等于
 *   "en_US.UTF-8"】。glibc 在 LC_ALL 处于「混合态」（各类别分属不同 locale）时返回的
 *   是复合名 "LC_CTYPE=C;LC_NUMERIC=en_US.UTF-8;LC_TIME=..."，已用 32 位探针 locmix.c
 *   复现，与弹窗文本逐字一致。
 *   即：游戏里有代码单独执行了 setlocale(LC_CTYPE,"C")（该调用成功，所以「失败日志」
 *   为空），把 CTYPE 从 en_US.UTF-8 降到 C，LC_ALL 因此变混合，引擎自检判定失败 → 弹框。
 *   系统侧完全正常，已逐条排除：locale-archive 含 en_US.utf8（localedef --list-archive）；
 *   32 位探针 setlocale(LC_ALL,"en_US.UTF-8") 成功；进程 env 全为 en_US.UTF-8；
 *   engine.so/gameui.so/libcef.so 的 setlocale 全是 UND（真 libc，无自带实现覆盖）；
 *   低 4GB 仅映射 1275MB/4096MB（31%，非地址空间耗尽）。
 *
 * 本 shim 做两件事（只影响游戏进程）：
 *   1) 归一化：任何 setlocale(LC_ALL,NULL) 查询若返回复合名（以 "LC_CTYPE=" 开头），
 *      就重新 setlocale(LC_ALL,"en_US.UTF-8") 把混合态拉回一致，并返回规范名。
 *      引擎自检因此通过，弹框消失；真实 locale 也确实被修正，不是单纯骗过检查。
 *   2) 取证：把「LC_CTYPE 被改成 C/POSIX」「归一化动作」「显式设置失败」写进
 *      $L4D2_SETLOCALE_LOG（默认 /tmp/l4d2-setlocale.log），带调用返回地址，
 *      便于日后定位到底哪个模块在动 locale。
 *
 * 不做的事：不改写别人主动设置的非 en_US locale（不改变其它模块本意），
 *   只在 LC_ALL 查询这个「引擎自检点」上做归一。
 *
 * 关闭：L4D2_NO_SETLOCALE_SHIM=1（或不注入 LD_PRELOAD）。
 *
 * 构建（不需要 libc6-dev-i386；源文件不 include 任何头文件）：
 *   gcc -m32 -fPIC -O2 -fno-stack-protector -c setlocale-shim.c -o setlocale-shim.o
 *   ld -m elf_i386 -shared -o setlocale-shim.so setlocale-shim.o
 */

extern void *dlvsym(void *handle, const char *symbol, const char *version);
extern int openat(int dirfd, const char *path, int flags, ...);
extern int close(int fd);
extern int write(int fd, const void *buf, unsigned len);
extern char *getenv(const char *name);

#define RTLD_NEXT ((void *)-1)
#define AT_FDCWD (-100)
#define O_WRONLY 01
#define O_CREAT 0100
#define O_APPEND 02000
#define O_CLOEXEC 02000000

#define LC_ALL 6
#define LC_CTYPE 0

static char *(*real_setlocale)(int, const char *);
static int in_shim;

/* ---- 极简日志（不引 stdio，减小依赖面） ---- */
static char logbuf[1200];
static unsigned logn;

static void lput(const char *s)
{
	while (*s && logn < sizeof(logbuf) - 1)
		logbuf[logn++] = *s++;
}
/* 注意：lput 依赖 NUL 终止。所有临时缓冲都必须显式补 '\0'，
 * 否则 lput 会越过有效长度一直读到栈垃圾（日志里表现为乱码）。 */
static void lputhex(unsigned long v)
{
	static const char h[] = "0123456789abcdef";
	int i;
	char t[2];
	lput("0x");
	for (i = 28; i >= 0; i -= 4) {
		t[0] = h[(v >> i) & 15];
		t[1] = 0;
		lput(t);
	}
}
static void lputnum(long v)
{
	char t[26];
	int n = 0;
	unsigned long u = v < 0 ? (unsigned long)-v : (unsigned long)v;
	do {
		t[n++] = (char)('0' + u % 10);
		u /= 10;
	} while (u);
	if (v < 0)
		lput("-");
	t[n] = 0;
	lput(t);
}
static void logline(const char *what, int cat, const char *name, const char *r,
		   const char *r2, unsigned long ra)
{
	const char *p = getenv("L4D2_SETLOCALE_LOG");
	int fd;
	lput(what);
	lput(" cat=");
	lputnum(cat);
	lput(" name=");
	lput(name ? name : "(query)");
	lput(" ret=");
	lput(r ? r : "(NULL)");
	if (r2) {
		lput(" ret2=");
		lput(r2);
	}
	lput(" ra=");
	lputhex(ra);
	lput("\n");
	fd = openat(AT_FDCWD, p ? p : "/tmp/l4d2-setlocale.log",
		    O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
	if (fd >= 0) {
		write(fd, logbuf, logn);
		close(fd);
	}
	logn = 0;
}

/* glibc 复合名形如 "LC_CTYPE=C;LC_NUMERIC=en_US.UTF-8;..." */
static int is_composite(const char *s)
{
	return s && s[0] == 'L' && s[1] == 'C' && s[2] == '_';
}
static int is_en_us(const char *s)
{
	return s && s[0] == 'e' && s[1] == 'n' && s[2] == '_' && s[3] == 'U'
	    && s[4] == 'S';
}
static int eq(const char *a, const char *b)
{
	while (*a && *a == *b) {
		a++;
		b++;
	}
	return *a == *b;
}

char *setlocale(int category, const char *locale)
{
	char *r, *fix;
	unsigned long ra;

	if (in_shim)
		return real_setlocale ? real_setlocale(category, locale)
				      : (char *)0;
	if (!real_setlocale)
		real_setlocale = dlvsym(RTLD_NEXT, "setlocale", "GLIBC_2.0");
	if (!real_setlocale)
		return (char *)0;

	ra = (unsigned long)(void *)__builtin_return_address(0);
	r = real_setlocale(category, locale);

	if (!locale) {
		/* 查询：只在 LC_ALL 这个引擎自检点上做归一化 */
		if (category == LC_ALL && is_composite(r)) {
			char before[400];
			int i = 0;
			/* 先拷贝：归一化会复用 glibc 的静态缓冲，原指针随即失效 */
			while (r[i] && i < (int)sizeof(before) - 1) {
				before[i] = r[i];
				i++;
			}
			before[i] = 0;
			in_shim = 1;
			fix = real_setlocale(LC_ALL, "en_US.UTF-8");
			logline(is_composite(fix) ? "normalise-FAILED" : "normalise",
				category, locale, fix, before, ra);
			in_shim = 0;
			if (fix && !is_composite(fix))
				r = fix;
		}
		return r;
	}

	/* 显式设置失败：只有请求 en_US 时才兜底，避免掩盖真的缺 locale */
	if (!r && is_en_us(locale)) {
		in_shim = 1;
		fix = real_setlocale(category, "en_US.utf8");
		logline("set-failed-retried", category, locale, r, fix, ra);
		in_shim = 0;
		if (fix)
			return fix;
		return (char *)locale;
	}

	/* 取证：谁把 LC_CTYPE 降成了 C / POSIX */
	if (category == LC_CTYPE && r && (eq(locale, "C") || eq(locale, "POSIX")))
		logline("LC_CTYPE-downgraded", category, locale, r, (char *)0, ra);

	return r;
}
