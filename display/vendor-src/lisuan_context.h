/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#ifndef __LISUAN_CONTEXT_H__
#define __LISUAN_CONTEXT_H__

#include <drm/drm_device.h>
#include "lisuan_device.h"

#define DMA_BUFFER_PRIVATE_DATA_SIZE (0x8000)

struct lisuan_ctx {
	struct lisuan_device *ls_dev;

	struct lisuan_file *ls_file;

	struct lisuan_sched_entity entity;

	unsigned int rb_index;

	unsigned long tgid;//processid
	unsigned long pid;//threadid for thread, pid for process

	unsigned int priority;

	unsigned int null_render_flag;
};

int lisuan_ctx_alloc_ioctl(struct drm_device *dev, void *data, struct drm_file *filp);
void lisuan_init_vm_context(struct lisuan_device *ls_dev);
void lisuan_fini_vm_context(struct lisuan_device *ls_dev);
int lisuan_ctx_free_ioctl(struct drm_device *dev, void *data, struct drm_file *filp);
int lisuan_cs_ioctl(struct drm_device *dev, void *data, struct drm_file *filp);
int lisuan_get_vem_info_ioctl(struct drm_device *dev, void *data, struct drm_file *filp);
void lisuan_ctx_mgr_fini(struct lisuan_device *ls_dev, struct lisuan_file *ls_file);

#endif
