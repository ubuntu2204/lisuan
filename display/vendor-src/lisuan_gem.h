/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#ifndef __LISUAN_GEM_H__
#define __LISUAN_GEM_H__

#include <drm/drm_device.h>
#include <drm/drm_file.h>
#include <linux/device.h>
#include <linux/pci.h>
#include <drm/drm_gem.h>
#include <drm/drm_mm.h>
#include <linux/scatterlist.h>
#include "lisuan_drm.h"

struct lisuan_mman;

#include "lisuan_device.h"
#include "gf_os_interface.h"
#include "lisuan_vm.h"
#if DRM_VERSION_CODE >= KERNEL_VERSION(6,6,0)
#include <drm/ttm/ttm_bo.h>
#else
#include <drm/ttm/ttm_bo_api.h>
#endif
#define MAX_LONG	((long)(~0UL >> 1))

#include <linux/mmzone.h> 

#ifndef MAX_PAGE_ORDER
#  ifdef MAX_ORDER
#    define MAX_PAGE_ORDER MAX_ORDER
#  else
#    error "Neither MAX_PAGE_ORDER nor MAX_ORDER is defined"
#  endif
#endif

/*
*used for gem buffer object description
*/

struct lisuan_gem_bo {
	//in:prefered domain and the buffer flag
	unsigned long long size;
	unsigned long long align;
	unsigned long long cmd_size;
	unsigned long long size_org;
	unsigned long long align_org;
	unsigned int compressFormat;
	unsigned int domain;
	unsigned long long mmu_bypass_start;
	bool is_present;
	bool is_tiled;
	bool first_map;
	usage_hint_s usage_hint;
	struct lisuan_buffer_config flag;
	struct lisuan_prefer_domains preferred_domains;
	struct allocation_info_flags_wddm2_0_s prefer_flags;

	/*drm_gem_object: use the tbo->base as the gembufferobj's gem
	 *ttm_mem_reg   : use the tbo->mem  as the framebuffer vram_node info
	 *mm_node    : the framebuffer node, only for non byPass range physical memory
	 */
	struct ttm_buffer_object tbo;
#if DRM_VERSION_CODE < KERNEL_VERSION(5,4,0)
	struct drm_gem_object base;//we do not resue the ttm_resv;
	struct reservation_object _tbo_resv;
#endif

	unsigned int handle;	//out: buffer object handle
	struct lisuan_file *ls_file;	//only used for pagetable update
	struct lisuan_vm_node *vm_node;	//the framebuffer node

	struct os_mutex va_list_lock;	//for lisuan virtual address used
	struct list_head va_list;	//for gpu_va
	void *virt_addres;	//cpu va of kernel

	atomic64_t resident_cnt;	//make resident count

	struct lisuan_cf_node *cf_node;	//compress format

	//used for map_offset
	unsigned int fault_map;
	unsigned int premap_num;

	unsigned int priority;
	unsigned int pm_discard;

	struct list_head free_pt_buffer;    //used for mtype is conficate, the old pt buffer should free

	void * swap_storage;
	bool evicted;
	struct list_head item;//used for eviction

	struct os_mutex lock;
	atomic_t unpagable;
};

struct lisuan_rect
{
    long  x;
    long  y;
    long  width;
    long  height;
};

struct lisuan_pru_blt_arg
{
    struct lisuan_gem_bo*         src_allocation_ptr;
    unsigned long long            src_gpu_va;
    struct lisuan_gem_bo*         dst_allocation_ptr;
    unsigned long long            dst_gpu_va;
    struct lisuan_rect            dst_rect;
    struct lisuan_rect            src_rect;
};

static inline bool
lisuan_need_resident(struct lisuan_device *ls_dev, struct lisuan_gem_bo *gem)
{
	bool need_resident = false;

	mutex_lock(&gem->lock.lock);
	if(gem->swap_storage || gem->evicted)
		need_resident = true;
	mutex_unlock(&gem->lock.lock);
	return need_resident;
}

int lisuan_gem_create_ioctl(struct drm_device *dev, void *data, struct drm_file *filp);
int lisuan_gem_map_gpuva_ioctl(struct drm_device *dev, void *data, struct drm_file *filp);
int lisuan_sparse_reserve_gpuva_ioctl(struct drm_device *dev, void *data, struct drm_file *filp);
int lisuan_sparse_update_gpuva_ioctl(struct drm_device *dev, void *data, struct drm_file *filp);

struct lisuan_gem_bo *gem_to_gf_bo(struct drm_gem_object *gem);

int lisuan_make_resident_ioctl(struct drm_device *dev, void *data, struct drm_file *filp);
int lisuan_gem_free_gpuva_ioctl(struct drm_device *dev, void *data, struct drm_file *filp);
int lisuan_gem_map_offset_ioctl(struct drm_device *dev, void *data, struct drm_file *filp);
int lisuan_get_cellindex_ioctl(struct drm_device *dev, void *data, struct drm_file *filp);
int lisuan_gem_query_infos_ioctl(struct drm_device *dev, void *data, struct drm_file *filp);
int lisuan_get_kmdfmt_ioctl(struct drm_device *dev, void *data, struct drm_file *filp);
int lisuan_gem_is_busy_ioctl(struct drm_device *dev, void *data, struct drm_file *filp);
int lisuan_alloc_cell_index_ioctl(struct drm_device *dev, void *data, struct drm_file *filp);

void lisuan_get_phys_addr(struct lisuan_device *ls_dev, struct ttm_buffer_object *tbo, unsigned page_idx, unsigned page_size, unsigned long long *pa, bool vram);

struct drm_mm_node *__get_gem_buffer_mm_node(struct lisuan_gem_bo *buffer);
int lisuan_gem_map_vmarea(struct lisuan_file *ls_file, struct lisuan_gem_bo *buffer, struct vm_area_struct *vma);
void *lisuan_map_vram_node(struct lisuan_device *ls_dev, struct drm_mm_node *mm_node, bool byPass);
int lisuan_alloc_gem_bo(struct lisuan_device *ls_dev, struct lisuan_file *ls_file, struct lisuan_gem_bo *buffer);
struct lisuan_vm_node *lisuan_gem_map_vmnode(struct lisuan_mman *mmgr, struct lisuan_device *ls_dev, 
	struct lisuan_file *ls_file, struct lisuan_gem_bo *buffer, int direction);

int lisuan_gem_map_system_pages(struct lisuan_file *ls_file, struct lisuan_gem_bo *buffer, struct vm_area_struct *vma);

int lisuan_alloc_system_pages(struct lisuan_device *ls_dev, struct lisuan_file * ls_file, unsigned long long size, struct sg_table **table, unsigned int cacheable);
void *lisuan_map_system_pages(struct lisuan_device *ls_dev, unsigned int cached, unsigned int size, struct sg_table *sg_table);
int lisuan_free_system_pages(struct lisuan_device *ls_dev, struct lisuan_file * ls_file, struct sg_table *table, int cache, int size);

void vma_config(struct vm_area_struct *vma, bool snoop);
void lisuan_gem_content_dump(struct lisuan_device *ls_dev, struct lisuan_gem_bo *gem, char* msg, unsigned int offset, unsigned int size);

int lisuan_gem_alloc_oneshot(struct lisuan_device *ls_dev, struct lisuan_file *ls_file, struct lisuan_gem_bo *gbo, unsigned int need_map);
void lisuan_gem_free_oneshot(struct lisuan_device *ls_dev, struct lisuan_gem_bo *gem);
void lisuan_prepare_gem(struct lisuan_device *ls_dev, struct lisuan_gem_bo *gem);
void lisuan_try_evict_phys_mem(struct lisuan_device *ls_dev, struct lisuan_file *ls_file, unsigned int size);

void lisuan_gem_object_free(struct drm_gem_object *gobj);
int lisuan_gem_object_open(struct drm_gem_object *obj, struct drm_file *file_priv);
void lisuan_gem_object_close(struct drm_gem_object *gem_buffer, struct drm_file *file_priv);

struct drm_gem_object *tbo_to_drm_gem_bo(struct ttm_buffer_object *tbo);
struct ttm_buffer_object *drm_gem_bo_to_tbo(struct drm_gem_object *gem);

void drm_gem_object_put_compat(struct drm_gem_object *gem);
int lisuan_gem_evict(struct lisuan_device *ls_dev, struct lisuan_gem_bo * gem, bool pm_op, int force);
void lisuan_gem_evict_all(struct lisuan_device *ls_dev);
int lisuan_gem_resident(struct lisuan_device *ls_dev, struct lisuan_gem_bo * gem, bool pm_op, int force);
void lisuan_gem_resident_all(struct lisuan_device *ls_dev);
struct lisuan_gem_bo * lisuan_alloc_idb_buffer(struct lisuan_device *ls_dev, unsigned int cmd_size);

#if DRM_VERSION_CODE >= KERNEL_VERSION(5,0,0)
struct dma_buf *lisuan_gem_object_export(struct drm_gem_object *gem_obj, int flags);
#else
struct dma_buf *lisuan_gem_object_export(struct drm_device *dev, struct drm_gem_object *gem_obj, int flags);
#endif
#endif
