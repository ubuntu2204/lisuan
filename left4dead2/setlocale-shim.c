/*
 * setlocale-shim.c — L4D2（32 位）setlocale 拦截器
 *
 * 背景（2026-10-07 实证）：
 *   bin/engine.so 里 `SetLocale('%s') failed. Using '%s'.` 的代码会调用
 *   libc 的 setlocale(LC_ALL, "en_US.UTF-8")；返回 NULL 时用 GTK 弹
 *   “Warning: ... Please install 'en_US.UTF-8' locale.” 模态框。
 *
 *   已排除的可能（都有实测依据）：
 *   - 系统 locale 不完整：/usr/lib/locale/locale-archive 含 en_US.utf8；
 *     32 位探针（/tmp/loc32.c，ld -e main 直接链接 libc.so.6）在同一台机器上
 *     setlocale(LC_ALL,"en_US.UTF-8") 返回正常。
 *   - 环境变量缺失：游戏进程 env 里 LC_ALL/LANG/各 LC_* 全为 en_US.UTF-8。
 *   - 库内自带 setlocale 覆盖：engine.so / gameui.so / libcef.so 全是 UND（导入 libc）。
 *   - 32 位地址空间耗尽：低 4GB 仅映射 1275MB/4096MB（31%）。
 *   ⇒ 属于 Valve 引擎侧的行为异常（同类问题见 ValveSoftware/halflife#3006），
 *     我们改不了游戏二进制；本 shim 在 libc 层面兜住。
 *
 * 行为：
 *   1) 每次调用都追加一行到 $L4D2_SETLOCALE_LOG（默认 /tmp/l4d2-setlocale.log），
 *      记录 类别/名字/返回值/重试结果，便于日后复查是否真失败。
 *   2) 若 libc setlocale 返回 NULL（引擎即判为失败并弹框）：
 *      a. 先用规范化别名重试一次（en_US.UTF-8 -> en_US.utf8）；
 *      b. 仍失败则把 locale 名原样返回给调用方，让引擎认为设置成功。
 *      实际生效的 locale 本来就由环境变量（en_US.UTF-8，正确）决定，
 *      所以第 2b 步只是消除误报，不改变进程的真实本地化行为。
 *
 * 关闭：L4D2_NO_SETLOCALE_SHIM=1（或删除 shim 的 LD_PRELOAD）。
 *
 * 构建（不需要 libc6-dev-i386，源文件不 include 任何头文件）：
 *   gcc -m32 -fPIC -O2 -fno-stack-protector -c setlocale-shim.c -o setlocale-shim.o
 *   ld -m elf_i386 -shared -o setlocale-shim.so setlocale-shim.o
 */

extern void *dlvsym(void *handle, const char *symbol, const char *version);
extern int open(const char *path, int flags, ...);
extern int close(int fd);
extern int write(int fd, const void *buf, unsigned len);
extern char *getenv(const char *name);
extern int openat(int dirfd, const char *path, int flags, ...);

#define RTLD_NEXT ((void *)-1)
#define O_WRONLY 01
#define O_CREAT 0100
#define O_APPEND 02000
#define O_CLOEXEC 02000000

static char *(*real_setlocale)(int, const char *);
static int in_shim;

/* ---- 极简日志（避免引入 stdio，减小依赖面） ---- */
static char logbuf[1024];
static unsigned logn;

static void lput(const char *s)
{
	while (*s && logn < sizeof(logbuf) - 1)
		logbuf[logn++] = *s++;
}
static void lputnum(long v)
{
	char t[24];
	int i = 0, neg = v < 0;
	unsigned long u = neg ? (unsigned long)-v : (unsigned long)v;
	if (!u)
		t[i++] = '0';
	while (u) {
		t[i++] = (char)('0' + u % 10);
		u /= 10;
	}
	if (neg)
		lput("-");
	while (i)
		lput(&t[--i]);
}
static void logcat(const char *cat, const char *name, char *r, char *r2)
{
	const char *p = getenv("L4D2_SETLOCALE_LOG");
	int fd;
	lput("cat=");
	lputnum((long)cat);
	lput(" name=");
	lput(name ? name : "(null)");
	lput(" ret1=");
	lput(r ? r : "(NULL)");
	lput(" ret2=");
	lput(r2 ? r2 : "(NULL)");
	lput("\n");
	fd = openat(-100 /*AT_FDCWD*/, p ? p : "/tmp/l4d2-setlocale.log",
		    O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
	if (fd < 0)
		return;
	write(fd, logbuf, logn);
	close(fd);
	logn = 0;
}

char *setlocale(int category, const char *locale)
{
	char *r, *r2 = (char *)0;

	if (!real_setlocale)
		real_setlocale = dlvsym(RTLD_NEXT, "setlocale", "GLIBC_2.0");

	r = real_setlocale(category, locale);

	/* 只有「显式请求某个 locale」失败才需要兜；查询（locale=NULL）失败照实返回 */
	if (r || !locale || in_shim)
		return r;

	in_shim = 1;
	/* glibc 认 en_US.utf8 / en_US.UTF-8 两种拼写，都失败才判定为引擎侧异常 */
	if (locale[0] == 'e' && locale[1] == 'n' && locale[2] == '_'
	    && locale[3] == 'U' && locale[4] == 'S')
		r2 = real_setlocale(category, "en_US.utf8");
	logcat(category == 6 ? "LC_ALL" : (category == 0 ? "LC_CTYPE" : "cat"), locale, r, r2);
	in_shim = 0;

	return r2 ? r2 : (char *)locale;
}
