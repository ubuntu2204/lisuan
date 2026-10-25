/* dlsym-hook-test.c — 验证 shim 的 dlsym 拦截能否改写这条链：
 *   dlsym(handle,"vkGetDeviceProcAddr") -> gdpa(dev,"vkCreateImage")
 * 这正是 shaderapivk.so 的取址方式（上一版纯符号替换拦不到，日志为空）。
 * 注意：不实际调用返回的函数指针，避免 32 位 dlopen libvulkan 的退出期崩溃。
 * 构建：
 *   gcc -m32 -fno-stack-protector -O2 -c dlsym-hook-test.c -o t.o
 *   ld -m elf_i386 -dynamic-linker /lib/ld-linux.so.2 -e main t.o \
 *      /usr/lib/i386-linux-gnu/libc.so.6 -o dlsym-hook-test
 */
extern void *dlopen(const char *path, int flags);
extern void *dlsym(void *h, const char *name);
extern int printf(const char *fmt, ...);
extern void *stdout;
extern int setbuf(void *s, int mode);

int main(void)
{
	void *h, *a, *b, *c, *d;
	setbuf(stdout, 0);
	h = dlopen("libvulkan.so.1", 2);
	if (!h) { printf("dlopen failed\n"); return 1; }
	a = dlsym(h, "vkGetDeviceProcAddr");
	b = dlsym(0, "vkGetDeviceProcAddr");
	c = dlsym(h, "vkCreateImage");
	d = dlsym(0, "vkCreateImage");
	printf("gdpa:   handle=%p DEFAULT=%p\n", a, b);
	printf("Create: handle=%p DEFAULT=%p\n", c, d);
	/* libvulkan.so.1 的符号是版本化的（VK_1_0），不带版本的 dlsym(RTLD_DEFAULT)
	 * 原本取不到；现在 DEFAULT 与 handle 都拿到同一个地址，说明两条路都被改写
	 * 到了 shim 提供的非版本化符号。 */
	if (b != 0 && a == b && d != 0 && c == d) {
		printf("RESULT: dlsym hook active OK（handle 与 DEFAULT 都被改写）\n");
		return 0;
	}
	printf("RESULT: hook NOT active\n");
	return 2;
}
