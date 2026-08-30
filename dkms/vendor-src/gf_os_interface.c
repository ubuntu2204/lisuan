/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#include "gf_os_interface.h"
#include "lisuan_scheduler.h"


#define  debug_memleak 0

void gf_assert(int match)
{
	if (!match) {
		BUG();
	}
}

extern unsigned int print_level;

int gf_vsnprintf(char *buf, unsigned long size, const char *fmt, ...)
{
    va_list args;
    int ret;

    va_start(args, fmt);

    ret = vsnprintf(buf, size, fmt, args);

    va_end(args);

    return ret;
}

void gf_printk(unsigned int msgtype, const char *fmt, ...)
{
	char buffer[256];
	va_list marker;

	if (msgtype <= print_level) {
		va_start(marker, fmt);
		vsnprintf(buffer, 256, fmt, marker);
		va_end(marker);

		if (msgtype == GF_DRV_DEBUG)
			printk(KERN_DEBUG "%s: %s", "LS", buffer);
		if (msgtype == GF_DRV_WARNING)
			printk(KERN_WARNING "%s: %s", "LS", buffer);
		if (msgtype == GF_DRV_INFO)
			printk(KERN_INFO "%s: %s", "LS", buffer);
		if (msgtype == GF_DRV_ERROR)
			printk(KERN_ERR "%s: %s", "LS error", buffer);
		if (msgtype == GF_DRV_EMERG)
			printk(KERN_EMERG "%s: %s", "LS emerg", buffer);
	}
}

unsigned int gf_read_mmio32(void *addr)
{
#ifdef FAKE_CHIP
	return 0;
#else
#ifndef __BIG_ENDIAN__
	return readl(addr);
#else
	register unsigned int val;
	val = *(volatile unsigned int *)(addr);
	return val;
#endif
#endif
}

void gf_write_mmio32(void *addr, unsigned int val)
{
#ifndef FAKE_CHIP
#ifndef __BIG_ENDIAN__
	writel(val, addr);
#else
	*(volatile unsigned int *)(addr) = val;
#endif
#endif
}

unsigned short gf_read_mmio16(void *addr)
{
#ifdef FAKE_CHIP
	return 0;
#else
#ifndef __BIG_ENDIAN__
	return readw(addr);
#else
	register unsigned short val;
	val = *(volatile unsigned short *)(addr);
	return val;
#endif
#endif
}

void gf_write_mmio16(void *addr, unsigned short val)
{
#ifndef FAKE_CHIP
#ifndef __BIG_ENDIAN__
	writew(val, addr);
#else
	*(volatile unsigned short *)(addr) = val;
#endif
#endif
}

unsigned char gf_read_mmio8(void *addr)
{
#ifdef FAKE_CHIP
	return 0;
#else
#ifndef __BIG_ENDIAN__
	return readb(addr);
#else
	register unsigned char val;
	val = *(volatile unsigned char *)(addr);
	return val;
#endif
#endif
}

void gf_write_mmio8(void *addr, unsigned char val)
{
#ifndef FAKE_CHIP
#ifndef __BIG_ENDIAN__
	writeb(val, addr);
#else
	*(volatile unsigned char *)(addr) = val;
#endif
#endif
}

#define GF_LOCKED 0
#define GF_LOCK_FAILED 1

void *gf_malloc(unsigned long size)
{
	void *addr = NULL;

	if (size <= 4 * 1024) {
		addr = kmalloc(size, GFP_KERNEL);
	}

	if (addr == NULL) {
		addr = vmalloc(size);
	}

	return addr;
}

void *gf_calloc(unsigned long size)
{
	void *addr = gf_malloc(size);

	if (addr != NULL) {
		memset(addr, 0, size);
	}

	return addr;
}

void gf_free(void *addr)
{
	if (addr == NULL)
		return;

	if (is_vmalloc_addr(addr)) {
		vfree(addr);
	} else {
		kfree(addr);
	}
}

static void *gf_mem_copy(    void       *dstPtr,  const void *srcPtr, unsigned length)
{
	void *ret = dstPtr;
	unsigned dwords, bytes = length;
	unsigned char *dst = dstPtr;
	const unsigned char *src = srcPtr;

	if ((length >= 128) && (((unsigned long long)dst & 3) == 0) & (((unsigned long long)src & 3) == 0))
	{
		dwords = (length / sizeof(unsigned));
		bytes = (length % sizeof(unsigned));

		while (dwords != 0)
		{
			*(unsigned *)dst = *(const unsigned *)src;
			dst += sizeof(unsigned);
			src += sizeof(unsigned);
			dwords--;
		}
	}

	while (bytes != 0)
	{
		*dst = *src;
		dst++;
		src++;
		bytes--;
	}

	return ret;
}

static void gf_memcpy_fromio(void *to, const volatile void __iomem *from, size_t count)
{
	while (count && !IS_ALIGNED((unsigned long)from, 8)) {
		*(u8 *)to = __raw_readb(from);
		from++;
		to++;
		count--;
	}

	while (count >= 8) {
		*(u64 *)to = __raw_readq(from);
		from += 8;
		to += 8;
		count -= 8;
	}

	while (count) {
		*(u8 *)to = __raw_readb(from);
		from++;
		to++;
		count--;
	}
}

static void gf_memcpy_toio(volatile void __iomem *to, const void *from, size_t count)
{
	while (count && !IS_ALIGNED((unsigned long)to, 8)) {
		__raw_writeb(*(u8 *)from, to);
		from++;
		to++;
		count--;
	}

	while (count >= 8) {
		__raw_writeq(*(u64 *)from, to);
		from += 8;
		to += 8;
		count -= 8;
	}

	while (count) {
		__raw_writeb(*(u8 *)from, to);
		from++;
		to++;
		count--;
	}
}

static void gf_memset_io(void *dst, int c, unsigned long count)
{
	u64 qc = (u8)c;

	qc |= qc << 8;
	qc |= qc << 16;
	qc |= qc << 32;

	while (count && !IS_ALIGNED((unsigned long)dst, 8)) {
		__raw_writeb(c, dst);
		dst++;
		count--;
	}

	while (count >= 8) {
		__raw_writeq(qc, dst);
		dst += 8;
		count -= 8;
	}

	while (count) {
		__raw_writeb(c, dst);
		dst++;
		count--;
	}
}

void gf_memset(void *addr, int data, unsigned long n)
{
#if defined(__arm64__) || defined(__aarch64__) || defined(__loongarch__)
	if (is_vmalloc_addr(addr))
	{
		gf_memset_io(addr, (int)data, n);
	}
	else
#endif
	{
		memset(addr, data, n);
	}
}

void *gf_memcpy(void *dst, const void *src, unsigned long n)
{
#if defined(__arm64__) || defined(__aarch64__) || defined(__loongarch__)
	if (is_vmalloc_addr(dst) && !is_vmalloc_addr(src))
	{
		gf_memcpy_toio(dst, src, n);
		return dst;
	}
	else if (!is_vmalloc_addr(dst) && is_vmalloc_addr(src))
	{
		gf_memcpy_fromio(dst, src, n);
		return dst;
	}
	else if (is_vmalloc_addr(dst) && is_vmalloc_addr(src))
	{
		gf_memcpy_toio(dst, src, n);
		return dst;
	}
	else
#endif
	{
#if defined(CONFIG_CC_OPTIMIZE_FOR_SIZE)
		return gf_mem_copy(dst, src, n);
#else
		return memcpy(dst, src, n);
#endif
	}
}

int gf_memcmp(const void *str1, const void *str2, unsigned long n)
{
	return memcmp(str1, str2, n);
}

void *gf_kvcalloc(int num, unsigned long size, int flag)
{
	void *addr = kvcalloc(num, size, flag);
	return addr;
}

void gf_kvfree(void *addr)
{
	kvfree(addr);
}

void gf_dump_commands(char * str, void* pcmds, unsigned int dw)
{
	uint32_t *pCmdDW = (uint32_t *)pcmds;
	uint32_t col = dw/4, i;

	if(!pCmdDW)
		return;

	gf_info("----------start %s cmd dump dw:0x%x----------\n", str, dw);
	for( i=0; i < col; i++)
		gf_info("%08x %08x %08x %08x\n", *(pCmdDW+i*4), *(pCmdDW+i*4 +1), *(pCmdDW+i*4 +2), *(pCmdDW+i*4 +3));

	if(dw%4 == 3)
		gf_info("%08x %08x %08x\n", *(pCmdDW+col*4), *(pCmdDW+col*4 +1), *(pCmdDW+col*4 +2));
	if(dw%4 == 2)
		gf_info("%08x %08x\n", *(pCmdDW+col*4), *(pCmdDW+col*4 +1));
	if(dw%4 == 1)
		gf_info("%08x\n", *(pCmdDW+col*4));
	gf_info("----------end %s cmd dump----------\n", str);
}

struct file * gf_create_filp(char * file_name)
{
	return filp_open(file_name, O_RDWR|O_CREAT, 0644);
}

void gf_close_filp(struct file *filp)
{
	filp_close(filp, NULL);
}

int gf_read_filp(struct file *filp, unsigned long size, unsigned long long *offset, void *pbuf)
{
	int ret = 0;

	if(!size || !offset)
		return ret;

	return kernel_read(filp, pbuf, size, (loff_t*)offset);
}

int gf_write_filp(struct file *filp, unsigned long size, void *pbuf)
{
	return kernel_write(filp, pbuf, size, &filp->f_pos);
}

void gf_dump_buffer_to_file(unsigned char *buffer, unsigned int size, const char *msg)
{
	char file_name[256];
	struct file  *filp;
	unsigned int len = 0, i;
	uint32_t dw= size/4, col = dw/4;
	unsigned int *pCmdDW = (unsigned int *)buffer;

	gf_vsnprintf(file_name, sizeof(file_name), "/tmp/%s.txt", msg);
	filp = gf_create_filp(file_name);
	if(!filp)
		return;

	for( i=0; i < col; i++){
		len = gf_vsnprintf(file_name, sizeof(file_name), "%08x %08x %08x %08x\n", *(pCmdDW+i*4), *(pCmdDW+i*4 +1), *(pCmdDW+i*4 +2), *(pCmdDW+i*4 +3));
		gf_write_filp(filp, len, file_name);
	}
	if(dw%4 == 3){
		len = gf_vsnprintf(file_name, sizeof(file_name), "%08x %08x %08x\n", *(pCmdDW+col*4), *(pCmdDW+col*4 +1), *(pCmdDW+col*4 +2));
		gf_write_filp(filp, len, file_name);
	}
	if(dw%4 == 2){
		len = gf_vsnprintf(file_name, sizeof(file_name), "%08x %08x\n", *(pCmdDW+col*4), *(pCmdDW+col*4 +1));
		gf_write_filp(filp, len, file_name);
	}
	if(dw%4 == 1){
		len = gf_vsnprintf(file_name, sizeof(file_name), "%08x\n", *(pCmdDW+col*4));
		gf_write_filp(filp, len, file_name);
	}
	gf_close_filp(filp);
}


void gf_get_ktime(u64 *ntime, unsigned long *nrem_sec)
{
	u64 time = local_clock();
	unsigned long rem_nsec = do_div(time, 1000000000);

	if(ntime)
		*ntime = time;
	if(nrem_sec)
		*nrem_sec = rem_nsec;
}

void gf_msleep(unsigned int msecs)
{
	if (msecs > 0) {
		msleep(msecs);
	}
}

void gf_usleep_range(unsigned long min, unsigned long max)
{
	usleep_range(min, max);
}

void gf_pci_msi_set_enable(void *pci_dev, unsigned int msi_cap, int enable)
{
	u16 control;

	pci_read_config_word(pci_dev, msi_cap + PCI_MSI_FLAGS, &control);
	control &= ~PCI_MSI_FLAGS_ENABLE;
	if (enable)
		control |= PCI_MSI_FLAGS_ENABLE;
	pci_write_config_word(pci_dev, msi_cap + PCI_MSI_FLAGS, control);
}
