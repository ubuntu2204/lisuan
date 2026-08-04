/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#ifndef __LISUAN_DRV_H__
#define __LISUAN_DRV_H__

#include "gf_os_interface.h"

#define DRIVER_NAME     "gf"
#define DRIVER_DESC     "gf gpu drm driver, shared by umd gfx, video and compute"
#define DRIVER_AUTHOR   "gf driver team"

#define DRIVER_DATE     "20211008"

#define     DRM_DRIVER_MAJOR        0
#define     DRM_DRIVER_MINOR        0
#define     DRM_DRIVER_PATCHLEVEL   0

#if DRM_VERSION_CODE >= KERNEL_VERSION(6,6,0)
int lisuan_gem_prime_vmap(struct drm_gem_object *base, struct iosys_map *map);
void lisuan_gem_prime_vunmap(struct drm_gem_object *base, struct iosys_map *map);
#elif DRM_VERSION_CODE >= KERNEL_VERSION(5,15,0)
int lisuan_gem_prime_vmap(struct drm_gem_object *base, struct dma_buf_map *map);
void lisuan_gem_prime_vunmap(struct drm_gem_object *base, struct dma_buf_map *map);
#else
void *lisuan_gem_prime_vmap(struct drm_gem_object *base);
void lisuan_gem_prime_vunmap(struct drm_gem_object *base, void *vaddr);
#endif
void *lisuan_gem_vmap(struct drm_gem_object *base);
#endif

int lisuan_gem_dmabuf_mmap(struct dma_buf *dma_buf, struct vm_area_struct *vma);
