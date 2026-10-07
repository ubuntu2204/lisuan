/* locale-mix-probe.c — 32 位复现/验证探针（可保留在仓库以便复查）
 * 编译：gcc -m32 -fPIC -c -fno-stack-protector locale-mix-probe.c -o p.o \
 *       ld -m elf_i386 -dynamic-linker /lib/ld-linux.so.2 -e main p.o \
 *          /usr/lib/i386-linux-gnu/libc.so.6 -o probe
 * 作用：证明 glibc 在 LC_ALL 处于「各类别分属不同 locale」的混合态时，
 *       setlocale(LC_ALL,NULL) 返回复合名 "LC_CTYPE=C;LC_NUMERIC=en_US.UTF-8;…"
 *       ——与 L4D2 弹窗文本逐字一致，是该弹窗的真根因（见 README §3.1）。
 */
/* locmix.c — 验证 glibc 在「LC_ALL 为混合 locale」时 setlocale(LC_ALL,NULL) 的返回形式 */
extern char *setlocale(int, const char *);
extern int write(int, const void *, unsigned);
extern void _exit(int);

static char buf[2048];
static unsigned n;
static void ps(const char *s) { while (*s && n < sizeof(buf) - 1) buf[n++] = *s++; }
static void show(const char *tag)
{
	char *r = setlocale(6 /*LC_ALL*/, (const char *)0);
	ps(tag); ps(" -> "); ps(r ? r : "(NULL)"); ps("\n");
}

void main(void)
{
	ps("1) 初始查询: "); show("");
	ps("2) setlocale(LC_ALL,\"en_US.UTF-8\") = ");
	ps(setlocale(6, "en_US.UTF-8") ? "ok" : "(NULL)"); ps("\n");
	show("   之后查询");
	ps("3) 单独把 LC_CTYPE 设成 C（模拟只有 CTYPE 失败/被改）\n");
	ps("   setlocale(LC_CTYPE,\"C\") = ");
	ps(setlocale(0, "C") ? "ok" : "(NULL)"); ps("\n");
	show("   之后查询");
	ps("4) 恢复：setlocale(LC_ALL,\"en_US.UTF-8\")\n");
	ps(setlocale(6, "en_US.UTF-8") ? "ok" : "(NULL)"); ps("\n");
	show("   之后查询");
	write(1, buf, n);
	_exit(0);
}
