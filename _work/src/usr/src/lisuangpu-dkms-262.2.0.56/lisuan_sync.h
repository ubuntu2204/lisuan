/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#ifndef __LISUAN_SYNC_H__
#define __LISUAN_SYNC_H__

#include <linux/types.h>
#include <linux/dma-fence.h>
#include <drm/drm_device.h>
#include "lisuan_gem.h"
#include "gf_os_interface.h"

#if DRM_VERSION_CODE >= KERNEL_VERSION(5,0,0)
#include <linux/dma-resv.h>
#else
	//TBD
#endif

struct gf_job;
struct ttm_buffer_object;

extern const struct dma_fence_ops lisuan_ring_fence_ops;

struct lisuan_monitor_fence {
	struct lisuan_gem_bo *buffer;
	struct lisuan_ring *sched;
	volatile unsigned long long *kmd_va;
};

struct lisuan_ring_fence {
	struct dma_fence base;
	struct lisuan_ring *ring;
	unsigned int rb_index;

	struct list_head list_in_device;
	unsigned long long ring_fence_value;
	spinlock_t fence_lock;
};

static inline void dma_fence_replace(struct dma_fence **s, struct dma_fence *d)
{
	struct dma_fence *t = s?*s:NULL;

	if(s)
		*s = dma_fence_get(d);

	dma_fence_put(t);
}

struct gf_sync_fence *lisuan_sync_fence(struct dma_fence *f);
void lisuan_sync_fence_free(struct gf_sync_fence *sync, bool need_put);

int lisuan_sync_create_ioctl(struct drm_device *dev, void *data, struct drm_file *filp);
void lisuan_sync_bo_fence(struct gf_job *sched_job, struct ttm_buffer_object *tbo, unsigned int bo_operation);
int lisuan_sync_signal_ioctl(struct drm_device *dev, void *data, struct drm_file *filp);
void lisuan_add_sync_fence_to_buffer(struct ttm_buffer_object *tbo, struct dma_fence *fence, unsigned int write);
int lisuan_ring_fence_thread_main(void *data);
int lisuan_sync_wait_ioctl(struct drm_device *dev, void *data, struct drm_file *filp);
int lisuan_sync_destroy_ioctl(struct drm_device *dev, void *data, struct drm_file *filp);

int lisuan_syncobj_lookup_and_add_to_sync_signal(struct drm_file *filp, struct gf_job *sched_job, struct lisuan_submit_cmd_info *cmdinfo);
int lisuan_syncobj_lookup_and_add_to_sync_wait(struct drm_file *filp, struct gf_job *sched_job, struct lisuan_submit_cmd_info *cmdinfo);

void lisuan_init_ring_fence_thread(struct lisuan_device *ls_dev);
void lisuan_fini_ring_fence_thread(struct lisuan_device *ls_dev);
int lisuan_any_ring_fence_signaled(struct lisuan_device *ls_dev);
struct lisuan_ring_fence *lisuan_ring_fence_create(struct lisuan_ring *ring);

int lisuan_sync_init(void);
void lisuan_sync_fini(void);
int lisuan_ring_fence_init(void);
void lisuan_ring_fence_fini(void);

#endif
