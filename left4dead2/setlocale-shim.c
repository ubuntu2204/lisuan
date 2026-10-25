/*
 * setlocale-shim.c — L4D2（32 位）Steam client locale 覆盖修复
 *
 * 2026-10-25 最终取证：
 *   1) bin/launcher.so::LauncherMain 先执行 setenv("LC_ALL", "en_US.UTF-8", 1)，
 *      随后 setlocale(LC_ALL, "en_US.UTF-8") 成功。
 *   2) Steam 的 linux32/steamclient.so 随后执行 setenv("LC_ALL", "C", 1)。
 *      捕获点在当前构建的 +0x17b9867（返回地址 +0x17b986c）。
 *   3) engine/gameui/client 再以 "" 应用各类别，最终只有部分类别保持 en_US；
 *      engine.so +0x41062a 查询 setlocale(LC_ALL, NULL)，发现返回的是混合名而非
 *      "en_US.UTF-8"，于是弹出模态 Warning 并阻塞地图加载。
 *
 * 本 shim 只改变一种调用：
 *   dladdr 确认直接调用者为 steamclient.so，且 setenv 的名称是 LC_ALL、值是 C/POSIX，
 *   才把该次 value 改为 en_US.UTF-8。其他模块、其他变量、其他 locale、其他调用全部
 *   原样转发。不会拦截或重置 setlocale，不伪造成功，不影响 Steam 客户端进程。
 *
 * 与 2026-10-07 旧 shim 的区别：旧版在 LC_ALL 查询时强制重置所有类别，使用全局
 * 日志缓冲和全局递归 guard，并可能返回调用者拥有的指针；这些风险设计已全部删除。
 *
 * 仅由 left4dead2/fix-l4d2.sh 注入 L4D2 启动链，不能全局注入 Steam。
 * 默认不写日志；需要审计时可设 L4D2_SETLOCALE_LOG=/tmp/l4d2-setlocale.log。
 * 关闭：L4D2_SETLOCALE_ENABLE=0 bash fix-l4d2.sh，或 L4D2_NO_SETLOCALE_SHIM=1。
 *
 * 构建（不需要 libc6-dev-i386；源文件不 include 任何头文件）：
 *   gcc -m32 -fPIC -O2 -fno-stack-protector -fno-builtin -c setlocale-shim.c -o setlocale-shim.o
 *   ld -m elf_i386 -shared -z noexecstack -o setlocale-shim.so setlocale-shim.o
 *
 * 测试专用：编译时加 -DLOCALE_SHIM_TEST 才会包含 L4D2_SETLOCALE_TEST 白名单，
 * 供独立探针模拟目标调用；正常部署不含此旁路。
 */

extern void *dlvsym(void *handle, const char *symbol, const char *version);
extern int dladdr(const void *address, void *info);
extern int openat(int dirfd, const char *path, int flags, ...);
extern int close(int fd);
extern int write(int fd, const void *buf, unsigned len);
extern char *getenv(const char *name);
extern int *__errno_location(void);

#define RTLD_NEXT ((void *)-1)
#define AT_FDCWD (-100)
#define O_WRONLY 01
#define O_CREAT 0100
#define O_APPEND 02000
#define O_CLOEXEC 02000000
#define ENOSYS 38

struct dl_info {
	const char *fname;
	void *fbase;
	const char *sname;
	void *saddr;
};

static int (*real_setenv)(const char *, const char *, int);
static __thread int resolving;

static int (*get_real(void))(const char *, const char *, int)
{
	int (*fn)(const char *, const char *, int) =
		__atomic_load_n(&real_setenv, __ATOMIC_RELAXED);
	if (fn || resolving)
		return fn;
	resolving = 1;
	fn = dlvsym(RTLD_NEXT, "setenv", "GLIBC_2.0");
	resolving = 0;
	if (fn)
		__atomic_store_n(&real_setenv, fn, __ATOMIC_RELAXED);
	return fn;
}

static int eq(const char *a, const char *b)
{
	if (!a)
		return 0;
	while (*a && *a == *b) {
		a++;
		b++;
	}
	return *a == *b;
}

static int has_suffix(const char *text, const char *suffix)
{
	unsigned text_len = 0, suffix_len = 0, i;
	while (text[text_len])
		text_len++;
	while (suffix[suffix_len])
		suffix_len++;
	if (suffix_len > text_len)
		return 0;
	for (i = 0; i < suffix_len; i++)
		if (text[text_len - suffix_len + i] != suffix[i])
			return 0;
	return 1;
}

static int should_redirect(const char *name, const char *value, unsigned long caller)
{
	struct dl_info info;
	if (!eq(name, "LC_ALL") || (!eq(value, "C") && !eq(value, "POSIX")))
		return 0;
#ifdef LOCALE_SHIM_TEST
	if (getenv("L4D2_SETLOCALE_TEST"))
		return 1;
#endif
	if (!dladdr((void *)caller, &info) || !info.fname)
		return 0;
	return eq(info.fname, "steamclient.so") || has_suffix(info.fname, "/steamclient.so");
}

static void put(char *buf, unsigned *used, unsigned cap, const char *s)
{
	while (*s && *used < cap - 1) {
		buf[*used] = *s++;
		(*used)++;
	}
}

static void log_redirect(const char *value)
{
	const char *path = getenv("L4D2_SETLOCALE_LOG");
	char buf[512];
	unsigned used = 0;
	int fd;
	if (!path)
		return;
	put(buf, &used, sizeof(buf), "steamclient locale: LC_ALL=");
	put(buf, &used, sizeof(buf), value);
	put(buf, &used, sizeof(buf), " -> en_US.UTF-8\n");
	fd = openat(AT_FDCWD, path, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
	if (fd >= 0) {
		unsigned offset = 0;
		while (offset < used) {
			int written = write(fd, buf + offset, used - offset);
			if (written <= 0)
				break;
			offset += (unsigned)written;
		}
		close(fd);
	}
}

int setenv(const char *name, const char *value, int overwrite)
{
	int entry_errno = *__errno_location();
	int (*fn)(const char *, const char *, int) = get_real();
	unsigned long caller = (unsigned long)(void *)__builtin_return_address(0);
	int redirect, result, saved_errno;
	if (!fn) {
		*__errno_location() = ENOSYS;
		return -1;
	}
	redirect = should_redirect(name, value, caller);
	*__errno_location() = entry_errno;
	result = fn(name, redirect ? "en_US.UTF-8" : value, overwrite);
	saved_errno = *__errno_location();
	if (redirect && result == 0)
		log_redirect(value);
	*__errno_location() = saved_errno;
	return result;
}
