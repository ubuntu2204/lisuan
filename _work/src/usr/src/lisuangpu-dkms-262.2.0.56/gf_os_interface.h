/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#ifndef __GF_OS_INTERFACE_H__
#define __GF_OS_INTERFACE_H__

#ifndef DRM_VERSION_CODE
#define DRM_VERSION_CODE LINUX_VERSION_CODE
#endif

#include <linux/version.h>
#include <linux/io.h>
#include <linux/mutex.h>
#include <linux/semaphore.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/file.h>
#include <linux/sched/clock.h>
#include <linux/pci.h> 
#include <linux/dma-fence.h>
#include <drm/drm_drv.h>
#if DRM_VERSION_CODE >= KERNEL_VERSION(5,1,0)
#include <drm/drm_probe_helper.h>
#else
#include <drm/drm_crtc_helper.h>
#endif
#include <drm/drm_connector.h>
#include <drm/drm_device.h>
#include <drm/drm_vblank.h>
#include <linux/vgaarb.h>
#include <drm/drm_crtc.h>
#if DRM_VERSION_CODE < KERNEL_VERSION(5,15,0)
#include <drm/drm_irq.h>
#endif
#include <drm/drm_sysfs.h>
#include <linux/types.h>
#include <linux/mman.h>

#ifndef TRUE
#define TRUE  1
#define FALSE 0
#endif

#ifndef NULL
#define NULL ((void *)0)
#endif

#ifndef KERNEL_VERSION
#define KERNEL_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))
#endif

struct lisuan_kvcalloc_item {
	void *kvcalloc_addr;
	unsigned long size;
	struct list_head kvcalloc_item;
};

struct lisuan_kvcalloc_list {
	spinlock_t list_lock;
	struct list_head kvcalloc_list;
};

#ifndef mutex_init
struct os_mutex {
	struct semaphore lock;
};
#define mutex_init(lock)           sema_init(lock, 1)
#define mutex_destroy(lock)        sema_init(lock, -99)
#define mutex_lock(lock)           down(lock)
#define mutex_unlock(lock)         up(lock)
#else
struct os_mutex {
	struct mutex lock;
};
#endif

struct os_spinlock {
	spinlock_t lock;
};

struct gf_job;

#define GF_DRV_DEBUG 0x04
#define GF_DRV_WARNING 0x03
#define GF_DRV_INFO 0x02
#define GF_DRV_ERROR 0x01
#define GF_DRV_EMERG 0x00
#define PACK_1K(par) ((par) / 1024 + ((par) % 1024 ? 1 : 0))

void gf_assert(int match);

void gf_printk(unsigned int msglevel, const char *fmt, ...);

#define gf_emerg(args...) gf_printk(GF_DRV_EMERG, ##args)
#define gf_error(args...) gf_printk(GF_DRV_ERROR, ##args)
#define gf_warning(args...) gf_printk(GF_DRV_WARNING, ##args)
#define gf_info(args...) gf_printk(GF_DRV_INFO,  ##args)
#define gf_debug(args...) gf_printk(GF_DRV_DEBUG, ##args)

#ifndef ASSERT
#define ASSERT(x)  							\
	if (!(x)) {							\
		gf_error("assertion failed %s: %d: %s\n",	\
		       __FILE__, __LINE__, #x);				\
		BUG();							\
	}
#endif

enum lisuan_debug_category
{
	VM_DEBUG = 0,
	VMID_DEBUG,
	GEM_DEBUG,
	JOB_DEBUG,
	PM_DEBUG,
	MB_DEBUG,
	DMA_FENCE
};

extern unsigned int lisuan_debug;

#define vm_info(fmt, args...)                 \
do {                                               \
    if(lisuan_debug & (1 << VM_DEBUG))          \
    {                                           \
        gf_info("[VM]"fmt, args);             \
    }                                           \
}while(0)

#define vmid_info(fmt, args...)                 \
do {                                               \
    if(lisuan_debug & (1 << VMID_DEBUG))          \
    {                                           \
        gf_info("[VMID]"fmt, args);             \
    }                                           \
}while(0)

#define gem_info(fmt, args...)                 \
do {                                               \
    if(lisuan_debug & (1 << GEM_DEBUG))          \
    {                                           \
        gf_info("[GEM]"fmt, args);             \
    }                                           \
}while(0)

#define job_info(fmt, args...)                 \
do {                                               \
    if(lisuan_debug & (1 << JOB_DEBUG))          \
    {                                           \
        gf_info("[JOB]"fmt, args);             \
    }                                           \
}while(0)

#define pm_info(fmt, args...)                 \
do {                                               \
    if(lisuan_debug & (1 << PM_DEBUG))          \
    {                                           \
        gf_info("[PM]"fmt, args);             \
    }                                           \
}while(0)

#define mb_info(fmt, args...)                 \
do {                                               \
    if(lisuan_debug & (1 << MB_DEBUG))          \
    {                                           \
        gf_info("[MB]"fmt, args);             \
    }                                           \
}while(0)

#define mb_err(fmt, args...)                 \
do {                                               \
    if(lisuan_debug & (1 << MB_DEBUG))          \
    {                                           \
        gf_error("[MB]"fmt, args);             \
    }                                           \
}while(0)


#define dma_fence_info(fmt, args...)                 \
do {                                               \
    if(lisuan_debug & (1 << DMA_FENCE))          \
    {                                           \
        gf_info("[dma_fence]"fmt, args);             \
    }                                           \
}while(0)

int gf_vsnprintf(char *buf, unsigned long size, const char *fmt, ...);
unsigned int gf_read_mmio32(void *addr);
void gf_write_mmio32(void *addr, unsigned int val);
unsigned short gf_read_mmio16(void *addr);
void gf_write_mmio16(void *addr, unsigned short val);
unsigned char gf_read_mmio8(void *addr);
void gf_write_mmio8(void *addr, unsigned char val);

void *gf_malloc(unsigned long size);
void *gf_calloc(unsigned long size);
void gf_free(void *addr);
void *gf_kvcalloc(int num, unsigned long size, int flag);
void gf_kvfree(void *addr);
void gf_memset(void *addr, int data, unsigned long n);
void *gf_memcpy(void *dest, const void *src, unsigned long n);
int gf_memcmp(const void *str1, const void *str2, unsigned long n);

void gf_dump_commands(char * str, void* pcmds, unsigned int dw);

void gf_dump_buffer_to_file(unsigned char *buffer, unsigned int size, const char *msg);

#define GF_GET_LAST_BIT(nr) ((~((nr)-1)) & (nr))
static inline unsigned int gf_get_bit_num(unsigned int value)
{
	unsigned int count = 0;

	while (value) {
		value &= (value - 1);
		count++;
	}

	return count;
}

static inline unsigned int gf_get_last_bit_index(unsigned int value)
{
	unsigned int temp = 0x1;
	unsigned int i;

	for (i = 0; i < 32; i++) {
		if (value & temp) {
			break;
		}
		temp <<= 1;
	}

	return i;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5,10,0)
struct drm_dp_phy_test_params {
	int link_rate;
	u8 num_lanes;
	u8 phy_pattern;
	u8 hbr2_reset[2];
	u8 custom80[10];
	bool enhanced_frame_cap;
};

#endif


#ifndef ARRAY_SIZE
#define ARRAY_SIZE(A)   (sizeof(A)/sizeof(A[0]))
#endif


struct file * gf_create_filp(char * file_name);
void gf_close_filp(struct file *filp);

int gf_read_filp(struct file *filp, unsigned long size, unsigned long long *offset, void *pbuf);

int gf_write_filp(struct file *filp, unsigned long size, void *pbuf);
void gf_get_ktime(u64 *ntime, unsigned long *nrem_sec);

void gf_msleep(unsigned int msecs);
void gf_usleep_range(unsigned long min, unsigned long max);

void gf_pci_msi_set_enable(void *pci_dev, unsigned int msi_cap, int enable);
#endif
