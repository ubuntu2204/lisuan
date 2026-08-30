/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#include <linux/bitmap.h>
#include <linux/list.h>
#include <linux/idr.h>
#include <drm/drm_file.h>
#include <linux/mm.h>
#include <linux/spinlock.h>
#include <drm/drm_gem.h>

#if !defined(__loongarch__)
#include <asm/set_memory.h>
#endif

#include <linux/ww_mutex.h>
#include <asm/atomic.h>
#include <linux/dma-buf.h>
#include <linux/version.h>
#include <linux/mman.h>
#include <drm/drm_cache.h>
#include <linux/shmem_fs.h>
#include <linux/dma-buf.h>
#if DRM_VERSION_CODE >= KERNEL_VERSION(5,15,0)
#include <drm/ttm/ttm_resource.h>
#endif
#include "lisuan_device.h"
#include "lisuan_drm.h"
#include "lisuan_device.h"
#include "lisuan_vm.h"
#include "lisuan_gem.h"
#include "lisuan_drv.h"
#include "gf_job.h"
#include "lisuan_ring.h"
#include "gf_trans.h"
#include "gf_os_interface.h"
#include "lisuan_p2pdma.h"


#if DRM_VERSION_CODE >= KERNEL_VERSION(6,12,0)
MODULE_IMPORT_NS("DMA_BUF");
#elif DRM_VERSION_CODE >= KERNEL_VERSION(6,6,0)
MODULE_IMPORT_NS(DMA_BUF);
#endif

struct drm_gem_object *tbo_to_drm_gem_bo(struct ttm_buffer_object *tbo)
{
#if DRM_VERSION_CODE >= KERNEL_VERSION(5,4,0)
	return &tbo->base;
#else
	struct lisuan_gem_bo *bo = NULL;
	bo = container_of(tbo, struct lisuan_gem_bo, tbo);
	return bo ? &bo->base : NULL;
#endif
}

struct ttm_buffer_object *drm_gem_bo_to_tbo(struct drm_gem_object *gem)
{
#if DRM_VERSION_CODE >= KERNEL_VERSION(5,4,0)
	return container_of(gem, struct ttm_buffer_object, base);
#else
	struct lisuan_gem_bo *bo = NULL;
	bo = container_of(gem, struct lisuan_gem_bo, base);
	return bo ? &bo->tbo : NULL;
#endif
}

struct lisuan_gem_bo *gem_to_gf_bo(struct drm_gem_object *gem)
{
	struct ttm_buffer_object *tbo = drm_gem_bo_to_tbo(gem);
	struct lisuan_gem_bo *bo=NULL;
	if(tbo)
		bo = container_of(tbo, struct lisuan_gem_bo, tbo);
	return bo;
}

void drm_gem_object_put_compat(struct drm_gem_object *gem)
{
#if DRM_VERSION_CODE  >= KERNEL_VERSION(5,7,0)
	drm_gem_object_put(gem);
#else
    mutex_lock(&gem->dev->struct_mutex);
    drm_gem_object_put(gem);
    mutex_unlock(&gem->dev->struct_mutex);	
#endif
}
#if DRM_VERSION_CODE < KERNEL_VERSION(5,4,233)
#define for_each_sgtable_page(sgt, piter, pgoffset)	\
	for_each_sg_page((sgt)->sgl, piter, (sgt)->orig_nents, pgoffset)
#endif

static inline struct scatterlist *__sg_next(struct scatterlist *sg)
{
	if (sg_is_last(sg))
		return NULL;
	++sg;
	if (unlikely(sg_is_chain(sg)))
		sg = sg_chain_ptr(sg);
	return sg;
}

static inline struct page **collect_page(struct sg_table *st, unsigned int page_cnt)
{
	unsigned int i = 0;
	struct page **pages = gf_kvcalloc(page_cnt, sizeof(struct page *), GFP_KERNEL);
	struct page *page;
	struct sg_page_iter sg_iter;

	for_each_sg_page(st->sgl, &sg_iter, st->nents, 0) {
		page = sg_page_iter_page(&sg_iter);
		pages[i++] = page;
	}

	ASSERT(i == page_cnt);
	return pages;
}

struct lisuan_vm_node *lisuan_gem_map_vmnode(struct lisuan_mman *mmgr, struct lisuan_device *ls_dev, 
	struct lisuan_file *ls_file, struct lisuan_gem_bo *buffer, int direction)
{
	struct lisuan_vm_node *vm_node;
	int ret = 0;

	vm_node = lisuan_vm_alloc_node(mmgr, buffer, buffer->size, buffer->align, direction);
	if (!vm_node)
		goto fail;

	if (buffer->flag.byPass)
	{
		vm_node->byPass = true;
		if (buffer->domain >= dom_visual_vram && 
			buffer->domain <= dom_invisual_vram) {

			buffer->flag.vram = 1;
			buffer->flag.nsec = 1;
			buffer->flag.pa_exist = 1;
			atomic64_inc(&buffer->resident_cnt);
			vm_node->age = atomic64_read(&buffer->resident_cnt);
			buffer->vm_node = vm_node;
			spin_lock(&mmgr->lock);
			list_add_tail(&buffer->item, &mmgr->list_lru[buffer->priority]);
			spin_unlock(&mmgr->lock);
		}
	}
	else{
		vm_node->ls_file = ls_file;
		vm_node->byPass = false;
	}
	mutex_lock(&buffer->va_list_lock.lock);
	list_add(&vm_node->node_item, &buffer->va_list);
	mutex_unlock(&buffer->va_list_lock.lock);
	vm_node->gem = buffer;
	vm_node->ref_cnt = 1;

	if (buffer->flag.pa_exist)
		lisuan_vm_map_va_range(ls_dev, ls_file, buffer, vm_node);
	return vm_node;
fail:
	gem_info("fail to alloc range size:0x%x\n", buffer->size);
	return NULL;
}

int lisuan_gem_create_ioctl(struct drm_device *dev, void *data, struct drm_file *filp)
{
	struct lisuan_device *ls_dev = dev->dev_private;
	struct lisuan_file *ls_file = filp->driver_priv;
	int ret = 0;

	struct lisuan_gem_create_info *alloc_info = (struct lisuan_gem_create_info *)data;
	struct lisuan_gem_bo *gbo = (struct lisuan_gem_bo *)gf_kvcalloc(1, sizeof(struct lisuan_gem_bo), GFP_KERNEL);

	struct lisuan_ring *sched=ls_dev->sched[0];

	//init the preferred domains according to the umd
	gbo->first_map = 1;
	gbo->prefer_flags = alloc_info->flags;
	gbo->preferred_domains = alloc_info->preferredDomain;
	gbo->usage_hint = alloc_info->usage_hint;

	if (ls_dev->hwctl.use_vram) {
		if (gbo->prefer_flags.cpu_visible)
			gbo->preferred_domains.DomainId0 = gbo->preferred_domains.DomainId1 = dom_visual_vram;
		else
			gbo->preferred_domains.DomainId0 = gbo->preferred_domains.DomainId1 = dom_invisual_vram;
	}

	gbo->size = 
	gbo->size_org = alloc_info->sizeInByte;
	gbo->align = 
	gbo->align_org = alloc_info->align;


	//gem buf size need align to sys page size, or gem init will bug check fail in drm_gem_private_object_init
	if(gbo->align < ls_dev->sys_page_size)
		gbo->align =  ls_dev->sys_page_size;
	gbo->size = ALIGN64_SIZE(gbo->size , ls_dev->sys_page_size);

	if(!ls_dev->hwctl.support_wc)
	{
		if(gbo->preferred_domains.DomainId0 == dom_system_pages)
			gbo->preferred_domains.DomainId0 = dom_snoop_system_pages;
		if(gbo->preferred_domains.DomainId1 == dom_system_pages)
			gbo->preferred_domains.DomainId1 = dom_snoop_system_pages;
		if(gbo->preferred_domains.DomainId2 == dom_system_pages)
			gbo->preferred_domains.DomainId2 = dom_snoop_system_pages;
		if(gbo->preferred_domains.DomainId3 == dom_system_pages)
			gbo->preferred_domains.DomainId3 = dom_snoop_system_pages;
		if(gbo->preferred_domains.DomainId4 == dom_system_pages)
			gbo->preferred_domains.DomainId4 = dom_snoop_system_pages;
	}else if(gbo->prefer_flags.cached){
		if(gbo->preferred_domains.DomainId0 == dom_system_pages)
			gbo->preferred_domains.DomainId0 = dom_snoop_system_pages;
		if(gbo->preferred_domains.DomainId1 == dom_system_pages)
			gbo->preferred_domains.DomainId1 = dom_snoop_system_pages;
		if(gbo->preferred_domains.DomainId2 == dom_system_pages)
			gbo->preferred_domains.DomainId2 = dom_snoop_system_pages;
		if(gbo->preferred_domains.DomainId3 == dom_system_pages)
			gbo->preferred_domains.DomainId3 = dom_snoop_system_pages;
		if(gbo->preferred_domains.DomainId4 == dom_system_pages)
			gbo->preferred_domains.DomainId4 = dom_snoop_system_pages;
	}

	if(ls_dev->hwctl.enable_local_huge_page && gbo->align != GPU_HUGE_PAGE_SIZE &&
	((gbo->preferred_domains.DomainId0 > 0 && gbo->preferred_domains.DomainId0 < dom_system_pages) ||
	(gbo->preferred_domains.DomainId1 > 0 && gbo->preferred_domains.DomainId1 < dom_system_pages) ||
	(gbo->preferred_domains.DomainId2 > 0 && gbo->preferred_domains.DomainId2 < dom_system_pages) ||
	(gbo->preferred_domains.DomainId3 > 0 && gbo->preferred_domains.DomainId3 < dom_system_pages) ||
	(gbo->preferred_domains.DomainId4 > 0 && gbo->preferred_domains.DomainId4 < dom_system_pages)))
	{
		gbo->align = GPU_HUGE_PAGE_SIZE;
		gbo->size = (gbo->size_org + gbo->align -1) & ~(gbo->align - 1);
	}

	gbo->compressFormat = alloc_info->compressFormat;
	gbo->is_present    = alloc_info->is_present;
	gbo->is_tiled    = alloc_info->is_tiled;
	gbo->priority = alloc_info->priority < gem_priority_count? alloc_info->priority:gem_priority_normal;

	if(gbo->is_present)
		atomic_set(&gbo->unpagable, 1);
	else
		atomic_set(&gbo->unpagable, 0);

	ret = lisuan_alloc_gem_bo(ls_dev, ls_file, gbo);
	if (gbo->compressFormat) {
		struct lisuan_cf_node *node = (struct lisuan_cf_node *)gf_kvcalloc(1, sizeof(struct lisuan_cf_node), GFP_KERNEL);

		gbo->cf_node = node;
		node->cf_size = gbo->size;

		lisuan_alloc_cf_node(ls_dev, gbo->cf_node);

		if(!ls_dev->hwctl.skip_clr){
			struct dma_fence *fence = NULL;

			gf_trans(ls_dev, NULL, gbo, lisuan_trans_op_auto_clear, &fence);

			if(fence)
			{
				dma_fence_wait(fence, false);
				dma_fence_put(fence);
			}
		}
	}
	gem_info("create gem-tgid:0x%x-%d, %p, is_present:%d, cur tgid:%d, size = 0x%llx\n", 
		gbo->handle, gbo->ls_file->tgid, gbo, gbo->is_present,  (unsigned long long)current->tgid, gbo->size);
exit:
	alloc_info->buffer = gbo->handle;
	return ret;
}

int lisuan_sparse_reserve_gpuva_ioctl(struct drm_device *dev, void *data, struct drm_file *filp)
{
	struct lisuan_drm_reserve_gpu_va *info = (struct lisuan_drm_reserve_gpu_va*)data;
	struct lisuan_device *ls_dev = (struct lisuan_device *)dev->dev_private;
	struct lisuan_file *ls_file = (struct lisuan_file *)filp->driver_priv;
	struct lisuan_vm_node *va_node;
	struct lisuan_vm_update_params params = { 0 };
	struct lisuan_vm_node *map_node = gf_kvcalloc(1, sizeof(struct lisuan_vm_node), 1);
	int ret = 0;

	va_node = lisuan_vm_alloc_node(&ls_file->vm.mmgr, NULL, info->size_in_bytes, GPU_HUGE_PAGE_SIZE, 0);

	if(!va_node){
			gf_error("ls_file:%p, nonbypass va exhausted, need:0x%llx \n", ls_file, info->size_in_bytes);
			return -ENOMEM;
	}
	vm_info("sparse reserve va range:0x%llx-0x%llx\n", va_node->offset, va_node->offset + va_node->size);

	/*
	* It is essential to set the valid flag to 0 for reserved GPU virtual addresses (VAs) in 
	* the page table buffer to prevent the UMD from accessing unbound GPU VAs,
	* which could lead to fetching incorrect physical addresses (PAs) from the page table.
	*/

	map_node->byPass = false;
	map_node->ls_file = ls_file;
	map_node->offset = va_node->offset;
	map_node->size   = va_node->size;

	params.sparse_path = 1;
	params.sparse_clear_op = 1;

	params.flag.byPass = 0;
	params.vm = &ls_file->vm;
	params.node = map_node;
	params.tbo = NULL;
	params.bitop_func = __bitmap_set;
	params.mtype = MEM_TYPE_4KB;
	params.unmap = 0;

	ret = lisuan_vm_bo_update_mapping(ls_dev, &params);
	if(ret)
		goto exit;

	va_node->ls_file = ls_file;
	info->virtual_address = va_node->offset;

exit:
	if(map_node)
		gf_kvfree(map_node);

	return ret;
}

int lisuan_sparse_update_gpuva_ioctl(struct drm_device *dev, void *data, struct drm_file *filp)
{
	struct lisuan_drm_update_gpu_va *info = (struct lisuan_drm_update_gpu_va *)data;
	const void __user *uptr = u64_to_user_ptr(info->p_ops);
	struct lisuan_device *ls_dev = (struct lisuan_device *)dev->dev_private;
	struct lisuan_file *ls_file = (struct lisuan_file *)filp->driver_priv;
	struct drm_gem_object *base=NULL;
	struct lisuan_gem_bo *buffer;
	struct update_tile_mapping_info *map_info = gf_kvcalloc(info->num_ops, sizeof(struct update_tile_mapping_info), 1);
	unsigned i;
	struct lisuan_vm_node *map_node = gf_kvcalloc(1, sizeof(struct lisuan_vm_node), 1);
	struct lisuan_vm_node *vm_node = NULL;
	int ret = 0;

	ret = copy_from_user(map_info, uptr, sizeof(struct update_tile_mapping_info) * info->num_ops);
	if(ret)
		goto exit;
	
	for (i = 0; i < info->num_ops; i++)
	{
		struct lisuan_vm_update_params params = { 0 };

		spin_lock(&ls_file->vm.mmgr.lock);
		list_for_each_entry(vm_node, &ls_file->vm.mmgr.list_head, vm_item){
			if(map_info[i].map.tile_gpu_va >= vm_node->offset && map_info[i].map.tile_gpu_va < (vm_node->offset+vm_node->size)){
				vm_node->sparse_path=true;
				break;
			}
		}
		spin_unlock(&ls_file->vm.mmgr.lock);
		switch(map_info[i].operation_type)
		{
			case map:
				gem_info("[SPARSE] handle:%d, gpu_va:%llx\n", map_info[i].map.h_allocation, map_info[i].map.tile_gpu_va);
				base = drm_gem_object_lookup(filp, map_info[i].map.h_allocation);

				if(!base)
				{
					gem_info("[SPARSE] wrong gem handle:0x%x ?\n",map_info[i].map.h_allocation);
					continue;
				}
				
				buffer = gem_to_gf_bo(base);

				if (buffer->flag.pa_exist) 
				{
					map_node->byPass = false;
					map_node->ls_file = ls_file;
					map_node->offset = map_info[i].map.tile_gpu_va;
					map_node->size   = map_info[i].map.size;

					params.flag.vram = buffer->flag.vram;
					params.flag.snoop = buffer->flag.snoop;
					params.pa_offset = map_info[i].map.offset;
					params.sparse_path = 1;
					params.flag.byPass = map_node->byPass;
					params.vm = &ls_file->vm;
					params.node = map_node;
					params.tbo = &buffer->tbo;
					params.bitop_func = __bitmap_set;
					params.mtype = MEM_TYPE_4KB;
					params.unmap = 0;

					ret = lisuan_vm_bo_update_mapping(ls_dev, &params);
				}
				drm_gem_object_put_compat(base);
				if(ret)
					goto exit;
				break;
			case unmap:
				{
					map_node->byPass = false;
					map_node->ls_file = ls_file;
					map_node->offset = map_info[i].unmap.tile_gpu_va;
					map_node->size   = map_info[i].unmap.size;

					params.vm = &ls_file->vm;
					params.node = map_node;
					params.unmap = 1;
					params.sparse_path = 1;
					//params.flag.do_map = buffer->flag.do_map;
					//params.flag.vram = buffer->flag.vram;
					//params.flag.snoop = buffer->flag.snoop;
					params.flag.byPass = map_node->byPass;
					params.bitop_func = __bitmap_clear;
					params.tbo = NULL; // update_pte need check
					params.mtype = MEM_TYPE_4KB;
					ret = lisuan_vm_unmap_and_release_pt(ls_dev, &params);
					if(ret)
						goto exit;
					gem_info("[SPARSE] unmap gpu_va:%llx\n",__func__, map_node->offset);
				}

				break;
			default:break;
		}
	}

exit:
	gf_kvfree(map_node);
	gf_kvfree(map_info);
    return ret;
}

int lisuan_gem_map_gpuva_ioctl(struct drm_device *dev, void *data, struct drm_file *filp)
{
	struct lisuan_device *ls_dev = (struct lisuan_device *)dev->dev_private;
	struct lisuan_file *ls_file = (struct lisuan_file *)filp->driver_priv;
	int shift = 0, direction = 0;
	unsigned int value;
	int ret = 0;

	struct lisuan_gem_map_gpuva_info *map_info = (struct lisuan_gem_map_gpuva_info *)data;

	struct lisuan_mman *va_mm;
	struct drm_gem_object *base = drm_gem_object_lookup(filp, map_info->buffer);
	struct lisuan_gem_bo *buffer = gem_to_gf_bo(base);
	struct lisuan_vm_node *va_node = NULL, *temp = NULL;
	unsigned int prefer_domain, allowed_domain;
	int trycnt = 0;

    if(!map_info->buffer || !base){
        return -EINVAL;
    }

	if(buffer->first_map)
	{
		if(buffer->is_present || ls_dev->hwctl.use_byPass)
		{
			buffer->flag.byPass = 1;
		}

		buffer->first_map = 0;
	}
	else
	{
		/*fetch the va_node, if va_node is bypass mapped, 
		 *all the umd use the same bypass va_node, no matter they are the same ls_file or not
		 */
		mutex_lock(&buffer->va_list_lock.lock);
		list_for_each_entry_safe(va_node, temp, &buffer->va_list, node_item) 
		{
			if (va_node->byPass || (ls_file == va_node->ls_file))
			{
				va_node->ref_cnt++;

				gem_info("remap gpuva arg gem-tgid:0x%x-%d, buf gem-tgid:0x%x-%d, %p, is_present:%d, bypass:%d, ref:%d\n", 
					map_info->buffer, ls_file->tgid, buffer->handle, buffer->ls_file->tgid,
					buffer, buffer->is_present, buffer->flag.byPass, va_node->ref_cnt);

				mutex_unlock(&buffer->va_list_lock.lock);
				goto exit;
			}
		}
		mutex_unlock(&buffer->va_list_lock.lock);
	}

	if (buffer->flag.byPass)
	{
retry:
		shift = 0;
		do {
			value = buffer->preferred_domains.Value >> (shift++) * 6;
			prefer_domain = value & 0x1F;
			direction = !!(value & 0x20);

			if (prefer_domain < dom_visual_vram ||
				prefer_domain > dom_snoop_system_pages)
				continue;

			if(buffer->is_present && prefer_domain > dom_invisual_vram)
				prefer_domain = dom_invisual_vram;

			if(prefer_domain == dom_invisual_vram &&
			  ls_dev->mmgr[prefer_domain - 1].size == 0)
				prefer_domain = dom_visual_vram;

			if (prefer_domain == dom_snoop_system_pages)
				allowed_domain = dom_system_pages;
			else
				allowed_domain = prefer_domain;

			buffer->domain = prefer_domain;
			va_mm = &ls_dev->mmgr[allowed_domain - 1];
			va_node = lisuan_gem_map_vmnode(va_mm, ls_dev, NULL, buffer, direction);
			if(!va_node)
				buffer->domain = 0;
		} while (!va_node && shift < 5);

		if(!va_node && shift >=5){
			if(trycnt && trycnt--){
				lisuan_try_evict_phys_mem(ls_dev, ls_file, buffer->size);
				goto retry;
			}
			gf_warning("all prefer bypass va exhausted on prefer dom:%x\n", buffer->preferred_domains.Value);
			buffer->domain = 0;
			ret = -ENOMEM;
		}
	} else {
		va_mm = &ls_file->vm.mmgr;
		va_node = lisuan_gem_map_vmnode(va_mm, ls_dev, ls_file, buffer, direction);
	}

	gem_info("map gpuva arg gem-tgid:0x%x-%d, buf gem-tgid:0x%x-%d, %p, is_present:%d, bypass:%d, gpuva:0x%llx-0x%llx\n", 
		map_info->buffer, ls_file->tgid, buffer->handle, buffer->ls_file->tgid, buffer, buffer->is_present, 
		buffer->flag.byPass, va_node?va_node->offset:0, va_node?va_node->size:0);

exit:
	drm_gem_object_put_compat(tbo_to_drm_gem_bo(&buffer->tbo));
	if (va_node) {
		map_info->gpu_va = va_node->offset;
		return ret;
	}
	return ret;
}

int lisuan_make_resident_ioctl(struct drm_device *dev, void *data, struct drm_file *filp)
{
	struct lisuan_device *ls_dev = (struct lisuan_device *)dev->dev_private;
	struct lisuan_file *ls_file = (struct lisuan_file *)filp->driver_priv;
	unsigned int shift = 0, value, direction = 0;
	struct lisuan_make_resident_info *resident_info = (struct lisuan_make_resident_info *)data;
	struct drm_gem_object *base = drm_gem_object_lookup(filp, resident_info->buffer);
	struct lisuan_gem_bo *buffer = gem_to_gf_bo(base);
	unsigned int prefer_domain, allowed_domain=0;
	struct lisuan_mman *mmgr = NULL;
	struct lisuan_vm_node *vm_node = NULL;
	int ret = 0, trycnt=0;

	lisuan_prepare_gem(ls_dev, buffer);

	mutex_lock(&buffer->lock.lock);

	if (buffer->flag.pa_exist) {
		goto mapping;
	}

	if(!buffer->flag.byPass)
	{
retry:
		shift = 0;
		ret = 0;
		do {
			value = buffer->preferred_domains.Value >> (shift++ * 6);
			prefer_domain = value & 0x1F;
			direction = !!(value & 0x20);

			if (prefer_domain < dom_visual_vram ||
				prefer_domain > dom_snoop_system_pages)
				continue;

			if(buffer->is_present && prefer_domain > dom_invisual_vram)
				prefer_domain = dom_invisual_vram;

			if(prefer_domain == dom_invisual_vram &&
			  ls_dev->mmgr[prefer_domain - 1].size == 0)
				prefer_domain = dom_visual_vram;


			if (prefer_domain <= dom_invisual_vram)
			{
				mmgr = &ls_dev->mmgr[prefer_domain - 1];
				vm_node = lisuan_vm_alloc_node(mmgr, buffer, buffer->size, buffer->align, direction);
				if (!vm_node) {
					ret = -ENOMEM;
					continue;
				}else{
					ret = 0;
				}
				gf_assert(!buffer->vm_node && !buffer->tbo.sg);
				buffer->domain = prefer_domain;
				buffer->flag.vram = 1;
				buffer->flag.nsec = 1;
				buffer->flag.pa_exist = 1;
				buffer->flag.large_page = ls_dev->hwctl.enable_local_huge_page; //now the local only support 64kpage
				buffer->vm_node = vm_node;
				spin_lock(&mmgr->lock);
				list_add_tail(&buffer->item, &mmgr->list_lru[buffer->priority]);
				spin_unlock(&mmgr->lock);

#if DRM_VERSION_CODE >= KERNEL_VERSION(5,15,0)
				buffer->tbo.resource->start = vm_node->offset;
#else
				buffer->tbo.mem.start = vm_node->offset;
#endif
				atomic64_inc(&buffer->resident_cnt);
			} 
			else if (prefer_domain <= dom_snoop_system_pages)
			{
				gf_assert(!buffer->vm_node && !buffer->tbo.sg);
				ret = lisuan_alloc_system_pages(ls_dev, ls_file, 
					base->size, &buffer->tbo.sg, (prefer_domain == dom_snoop_system_pages));
				if (ret)
					continue;
				buffer->domain = prefer_domain;
				buffer->flag.snoop = ((buffer->domain == dom_snoop_system_pages) ? 1 : 0);
				buffer->flag.vram = 0;
				buffer->flag.nsec = 1;
				buffer->flag.pa_exist = 1;
				atomic64_inc(&buffer->resident_cnt);
			}
		} while (ret && shift < 5);

		if(ret && shift >= 5) {
			if(trycnt && trycnt--){
				lisuan_try_evict_phys_mem(ls_dev, ls_file, buffer->size);
				goto retry;
			}
			gf_warning("phys mem exhausted, prefers:0x%x\n", buffer->preferred_domains.Value);
			goto exit;
		}
	}
	else
	{
		//bypass in local mem should be ready at map gpuva stage.
		gf_assert(buffer->domain >= dom_system_pages && buffer->domain <= dom_snoop_system_pages);

		ret = lisuan_alloc_system_pages(ls_dev, ls_file, base->size, 
			&buffer->tbo.sg, (allowed_domain == dom_snoop_system_pages));
		if (ret)
			goto exit;

		buffer->flag.snoop = ((buffer->domain == dom_snoop_system_pages) ? 1 : 0);
		buffer->flag.vram = 0;
		buffer->flag.nsec = 1;
		buffer->flag.pa_exist = 1;
		atomic64_inc(&buffer->resident_cnt);
	}
mapping:
	//step 2 update page table
	ret = lisuan_vm_map_va_range(ls_dev, ls_file, buffer, NULL);

exit:
	mutex_unlock(&buffer->lock.lock);
	drm_gem_object_put_compat(base);

	gem_info("make resident gem-tgid:0x%x-%d, %p, is_present:%d, cur tgid:%d, size = 0x%llx, domain:%d\n", 
		buffer->handle, buffer->ls_file->tgid, buffer, buffer->is_present,  (unsigned long long)current->tgid, buffer->size, buffer->domain);
	return ret;
}

void vma_config(struct vm_area_struct *vma, bool snoop)
{
	vma->vm_page_prot = vm_get_page_prot(vma->vm_flags);
	if (!snoop) {
#ifdef CONFIG_X86
#ifdef CONFIG_X86_PAT
		vma->vm_page_prot = __pgprot((pgprot_val(vma->vm_page_prot) & ~_PAGE_CACHE_MASK)
					     | cachemode2protval(_PAGE_CACHE_MODE_WC));
#else
		vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
#endif
#else
		vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
#endif
	}
}

int lisuan_gem_free_gpuva_ioctl(struct drm_device *dev, void *data, struct drm_file *filp)
{
	struct lisuan_device *ls_dev = (struct lisuan_device *)dev->dev_private;
	struct lisuan_file *ls_file = (struct lisuan_file *)filp->driver_priv;

	struct lisuan_vm_node *va_node = NULL, *tmp = NULL;
	int ret = 0, can_free = 0;

	struct lisuan_gem_free_gpuva_info *free_info = (struct lisuan_gem_free_gpuva_info *)data;
	unsigned long long map_offset = free_info->map_offset;
	struct drm_gem_object *base = drm_gem_object_lookup(filp, free_info->map_buffer);
	struct lisuan_gem_bo *buffer = NULL;

	/*the handle 0 only used by the sparse path, reserce gpu_va*/
	if(!base)
	{
		if(map_offset)
		{
			spin_lock(&ls_file->vm.mmgr.lock);
			list_for_each_entry_safe(va_node, tmp, &ls_file->vm.mmgr.list_head, mm_item){
				if(map_offset == va_node->offset)
				{
					can_free = 1;
					break;
				}
			}
			spin_unlock(&ls_file->vm.mmgr.lock);
			if(can_free)
				va_node->free(va_node);
		}
		return ret;
	}

	buffer = gem_to_gf_bo(base);

	mutex_lock(&buffer->va_list_lock.lock);
	list_for_each_entry_safe(va_node, tmp, &buffer->va_list, node_item) {
		if (map_offset == va_node->offset) {
			if (va_node->byPass || va_node->ls_file == ls_file)
			{
				if(--va_node->ref_cnt == 0){
					if(!va_node->byPass){//bypass will release at gem free stage
						list_del(&va_node->node_item);
						can_free = 1;
					}
					break;
				}
			}
		}
	}
	mutex_unlock(&buffer->va_list_lock.lock);

	if (can_free) {
		gem_info("free gpuva arg gem-tgid:0x%x-%d, buf gem-tgid:0x%x-%d, %p, gpuva:0x%llx-0x%llx, bypass:%d\n", 
			free_info->map_buffer, ls_file->tgid, buffer->handle, buffer->ls_file?buffer->ls_file->tgid:0,
			buffer, va_node->offset, va_node->offset+va_node->size, va_node->byPass);

		if(va_node->age && va_node != buffer->vm_node){
			lisuan_vm_unmap_va_range(ls_dev, ls_file, buffer, va_node);
			va_node->free(va_node);
		}
	}

	drm_gem_object_put_compat(base);
	return ret;
}

int lisuan_gem_map_offset_ioctl(struct drm_device *dev, void *data, struct drm_file *filp)
{
	struct lisuan_gem_map_offset_info *map_offset = (struct lisuan_gem_map_offset_info *)data;

	struct drm_gem_object *base = drm_gem_object_lookup(filp, map_offset->buffer);
	struct lisuan_gem_bo *buffer = gem_to_gf_bo(base);

	int ret;
	
	if (!buffer){
		if(base)
			drm_gem_object_put_compat(base);
		return -ENOENT;
	}
	ret = drm_gem_create_mmap_offset(base);
	if (ret) {
		goto exit;
	}
	map_offset->offset = drm_vma_node_offset_addr(&base->vma_node);

	buffer->fault_map = map_offset->fault_map;
	buffer->premap_num = map_offset->premap_num;

exit:
	drm_gem_object_put_compat(base);
	return ret;
}

int lisuan_get_cellindex_ioctl(struct drm_device *dev, void *data, struct drm_file *filp)
{
	struct lisuan_device *ls_dev = dev->dev_private;
	struct lisuan_file *ls_file = filp->driver_priv;
	struct drm_gem_object *base;
	struct lisuan_gem_bo *buffer;
	struct lisuan_drm_get_cellindex *alloc_info = (struct lisuan_drm_get_cellindex *)data;
	int ret = 0;

	base = drm_gem_object_lookup(filp, alloc_info->buffer);
	buffer = gem_to_gf_bo(base);

	if (buffer->compressFormat){
		alloc_info->cellindex = buffer->cf_node->cell_index;
		alloc_info->pad       = buffer->cf_node->cf_size;
	}

    if (alloc_info->should_map==1 && buffer->cf_node) {//map cur cl buf
        resource_size_t start;
        unsigned long offset_in_page = 0;
        unsigned long long align_down, align_up;
        struct mmap_args args = {0};

        if(ls_dev->cf.buf->domain != dom_visual_vram)
            return -EACCES;

        start = buffer->cf_node->cf_start + ls_dev->cf.mmgr.start_addr + ls_dev->pdev_info.start_addr_vram - ls_dev->mmu_bypass_start;

        mutex_lock(&ls_file->cf_pool_lock->lock);

        ls_file->gbo_cf = (void*)&args;

        align_down = ALIGN_DOWN(start, ls_dev->sys_page_size);
        align_up   = ALIGN(start, ls_dev->sys_page_size);
        offset_in_page = start - align_down;

        args.size = align_up - align_down;
        args.domain = ls_dev->cf.buf->domain;

        alloc_info->umd_cpu_va =vm_mmap(filp->filp, 0, args.size, PROT_READ | PROT_WRITE, MAP_SHARED, align_down);
        alloc_info->umd_cpu_va +=offset_in_page;

        ls_file->gbo_cf = NULL;
        mutex_unlock(&ls_file->cf_pool_lock->lock);
    }else if(alloc_info->should_map == 2){//map whole cl pool
        struct mmap_args args = {0};
        resource_size_t start;

        if(ls_dev->cf.buf->domain != dom_visual_vram)
            return -EACCES;

        start = ls_dev->cf.mmgr.start_addr + ls_dev->pdev_info.start_addr_vram - ls_dev->mmu_bypass_start;

        mutex_lock(&ls_file->cf_pool_lock->lock);

        args.size = ls_dev->cf.cf_total_size;
        args.domain = buffer->domain;

        ls_file->gbo_cf = (void*)&args;
        alloc_info->umd_cpu_va = vm_mmap(filp->filp, 0, ls_dev->cf.cf_total_size, PROT_READ | PROT_WRITE, MAP_SHARED, start);
        ls_file->gbo_cf = NULL;
        mutex_unlock(&ls_file->cf_pool_lock->lock);
    }
exit:
	drm_gem_object_put_compat(base);
	return ret;

}

int lisuan_get_kmdfmt_ioctl(struct drm_device *dev, void *data, struct drm_file *filp)
{
	struct drm_gem_object *base;
	struct lisuan_gem_bo *buffer;
	struct lisuan_drm_get_kmd_fmt *fmt_info = (struct lisuan_drm_get_kmd_fmt *)data;

	base = drm_gem_object_lookup(filp, fmt_info->buffer);
	buffer = gem_to_gf_bo(base);
	if(!buffer)
		return 0;

	fmt_info->kmd_fmt = buffer->compressFormat;
	drm_gem_object_put_compat(base);
	return 0;

}

int lisuan_alloc_cell_index_ioctl(struct drm_device *dev, void *data, struct drm_file *filp)
{
	struct drm_gem_object *base;
	struct lisuan_gem_bo *buffer;
	struct lisuan_drm_alloc_cell_index *p_arg = (struct lisuan_drm_alloc_cell_index *)data;

	base = drm_gem_object_lookup(filp, p_arg->buffer);
	buffer = gem_to_gf_bo(base);

	if(!buffer)
		return 0;

	if(buffer->cf_node){
		if(buffer->compressFormat == 0xFF)
			buffer->compressFormat = p_arg->compress_fmt;
		if(buffer->compressFormat != p_arg->compress_fmt)
			gf_info("comp fmt conflict, default-new:%d-%d\n", buffer->compressFormat, p_arg->compress_fmt);
		p_arg->cell_index = buffer->cf_node->cell_index;
	}else{
		buffer->compressFormat = 0;
		p_arg->cell_index = 0xFFFFFFFF;
	}
	drm_gem_object_put_compat(base);
	return 0;
}

#define QUIET (__GFP_NORETRY | __GFP_NOWARN | __GFP_HIGHMEM)
#define MAYFAIL (__GFP_RETRY_MAYFAIL | __GFP_NOWARN | __GFP_HIGHMEM)

int lisuan_alloc_system_pages(struct lisuan_device *ls_dev, struct lisuan_file *ls_file, unsigned long long size, struct sg_table **table, unsigned int cacheable)
{
	struct sg_table *st = gf_kvcalloc(1, sizeof(*st), GFP_KERNEL);
	unsigned int npages, alloc_size = 0;
	int max_order = MAX_PAGE_ORDER;
	int order;
	gfp_t gfp = GFP_KERNEL | __GFP_HIGHMEM | __GFP_RECLAIMABLE;
	struct scatterlist *sg;
	struct page *page;

	if (!st)
		return -ENOMEM;

	gf_assert(size == ALIGN64_SIZE(size, ls_dev->sys_page_size));

	npages = size >> ls_dev->sys_page_shift;
	if (sg_alloc_table(st, npages, GFP_KERNEL)) {
		gf_kvfree(st);
		return -ENOMEM;
	}

	sg = st->sgl;
	st->nents = 0;
	do {
		int order = min(fls(npages) - 1, max_order);
		struct page *page;

		do {
			page = alloc_pages(gfp | (order ? QUIET : MAYFAIL),
					   order);
			if (page)
				break;
			if (!order--)
				order = 1;

			/* Limit subsequent allocations as well */
			max_order = order;
		} while (1);

		alloc_size += (ls_dev->sys_page_size << order);
		sg_set_page(sg, page, ls_dev->sys_page_size << order, 0);
		st->nents++;

		npages -= 1 << order;
		if (!npages) {
			sg_mark_end(sg);
			break;
		}

		sg = __sg_next(sg);
	} while (1);

	if(ls_file)
		ls_file->sys_size += alloc_size;

#ifdef CONFIG_X86
		if(!cacheable){
			int npages = size >> ls_dev->sys_page_shift;
			struct page **pages = collect_page(st, npages);

			set_pages_array_wc(pages, npages);
			gf_kvfree(pages);
		}
#endif

	if (ls_dev->hwctl.iommu_enabled && !dma_map_sg(ls_dev->dev, st->sgl, st->nents, DMA_BIDIRECTIONAL)) {
		gf_error("failed to dma_map_sg the sg\n");
		goto dma_map_fail;
	}

	*table = st;
	return 0;
dma_map_fail:
	lisuan_free_system_pages(ls_dev, ls_file, st, cacheable, size);
	*table = NULL;
	return -ENOMEM;
}

int lisuan_free_system_pages(struct lisuan_device *ls_dev, struct lisuan_file *ls_file, struct sg_table *table, int cache, int size)
{
	int free_size = 0;
	struct page **pages;
	struct scatterlist *sg;

	if (!table)
		return 0;

	sg = table->sgl;

#ifdef CONFIG_X86
	if (!cache)
	{
		int i = 0, order=0;
		struct scatterlist *tsg = sg;

		for (; tsg; tsg = __sg_next(tsg)) {
			if (sg_page(tsg) && !PageHighMem(sg_page(tsg)))
			{
				order = get_order(tsg->length);
				set_pages_wb(sg_page(tsg), 1 << order);
			}
		}
	}
#endif

	if(ls_dev->hwctl.iommu_enabled)
		dma_unmap_sg(ls_dev->dev, sg, table->nents, DMA_BIDIRECTIONAL);

	for (; sg; sg = __sg_next(sg)) {
		if (sg_page(sg))
		{
			free_size += sg->length;
			__free_pages(sg_page(sg), get_order(sg->length));
		}
	}

	sg_free_table(table);

	gf_kvfree(table);

	if(ls_file)
		ls_file->sys_size -= free_size;
	return 0;
}


static int 
lisuan_vram_mgr_alloc_sgt(struct lisuan_device *ls_dev,
			struct lisuan_gem_bo *gbo,
			struct device *dev,
			enum dma_data_direction dir,
			struct sg_table **sgt)
{
	struct lisuan_vm_node *vm_node = gbo->vm_node;
	struct scatterlist *sg;
	struct sg_table *st;
	int num_entries = 1;//we export local as a continued large page
	int i, ret=0;
	phys_addr_t phys;
	size_t size;
	dma_addr_t addr;

	gf_assert(!!vm_node);

	phys = ls_dev->pdev_info.start_addr_vram + vm_node->offset - ls_dev->mmu_bypass_start;
	size = vm_node->size;

	st = gf_kvcalloc(1, sizeof(*st), GFP_KERNEL);
	if (!st)
		return -ENOMEM;

	ret = sg_alloc_table(st, num_entries, GFP_KERNEL);
	if (ret)
		goto error_free;

	addr = dma_map_resource(dev, phys, size, dir, DMA_ATTR_SKIP_CPU_SYNC);
	ret = dma_mapping_error(dev, addr);
	if (ret)
		goto error_unmap;

	sg = st->sgl;
	sg_set_page(sg, NULL, size, 0);
	sg_dma_address(sg) = addr;
	sg_dma_len(sg) = size;

	*sgt = st;

	return ret;
error_unmap:
	sg_free_table(st);

error_free:
	gf_kvfree(st);
	return ret;
}


static void
lisuan_vram_mgr_free_sgt(struct device *dev,
				enum dma_data_direction dir,
				struct sg_table *sgt)
{
	struct scatterlist *sg = NULL;

	sg = sgt->sgl;
	for (; sg; sg = __sg_next(sg)) {
		dma_unmap_resource(dev, sg_dma_address(sg),
				sg_dma_len(sg), dir, DMA_ATTR_SKIP_CPU_SYNC);
	}
	sg_free_table(sgt);
	gf_kvfree(sgt);
}


static int 
lisuan_system_alloc_sgt(struct lisuan_device *ls_dev,
			struct lisuan_gem_bo *gbo,
			struct device *dev,
			enum dma_data_direction dir,
			struct sg_table **sgt)
{
	struct sg_table *st;
	struct scatterlist *src, *dst;
	int ret = 0, i;

	gf_assert(!!gbo->tbo.sg);

	st = gf_kvcalloc(1, sizeof(struct sg_table), GFP_KERNEL);
	if (st == NULL) {
		ret = -ENOMEM;
		goto err;
	}

	ret = sg_alloc_table(st, gbo->tbo.sg->nents, GFP_KERNEL);
	if (ret)
		goto err_free;

	src = gbo->tbo.sg->sgl;
	dst = st->sgl;
	for (i = 0; i < gbo->tbo.sg->nents; i++) {
		sg_set_page(dst, sg_page(src),  src->length, 0);
		dst = sg_next(dst);
		src = sg_next(src);
	}

	if (!dma_map_sg(dev, st->sgl, st->nents, dir)){
		ret = -ENOMEM;
		goto err_free_sg;
	}

	*sgt = st;

	return ret;

err_free_sg:
	sg_free_table(st);
err_free:
	gf_kvfree(st);
err:
	return ret;
}

static void
lisuan_system_free_sgt(struct dma_buf_attachment *attachment, struct sg_table *st, enum dma_data_direction dir)
{
	dma_unmap_sg(attachment->dev, st->sgl, st->nents, dir);
	sg_free_table(st);
	gf_kvfree(st);
}

static struct sg_table *lisuan_map_dma_buf(struct dma_buf_attachment *attachment, enum dma_data_direction dir)
{
	struct drm_gem_object *gem = attachment->dmabuf->priv;
	struct lisuan_gem_bo *gbo = gem_to_gf_bo(gem);
	struct lisuan_device *ls_dev =  (struct lisuan_device *)gem->dev->dev_private;
	struct sg_table *st = NULL;

	if(gbo->domain == dom_invisual_vram ||
		(gbo->domain == dom_visual_vram && 
		lisuan_pci_p2pdma_distance_many(to_pci_dev(gem->dev->dev), &attachment->dev, 1, false) < 0))
		return ERR_PTR(-EOPNOTSUPP);

	if(gbo->domain < dom_system_pages){
		lisuan_vram_mgr_alloc_sgt(ls_dev, gbo, attachment->dev, dir, &st);
	}else{
		lisuan_system_alloc_sgt(ls_dev, gbo, attachment->dev, dir, &st);
	}

	return st;
}

static void lisuan_unmap_dma_buf(struct dma_buf_attachment *attachment, struct sg_table *st, enum dma_data_direction dir)
{
	struct lisuan_gem_bo *gbo = gem_to_gf_bo(attachment->dmabuf->priv);

	if (gbo->domain >= dom_system_pages) {
		lisuan_system_free_sgt(attachment, st, dir);
	} else {
		lisuan_vram_mgr_free_sgt(attachment->dev, dir, st);
	}
}

struct drm_mm_node *__get_gem_buffer_mm_node(struct lisuan_gem_bo *buffer)
{
	struct drm_mm_node *mm_node = NULL;

	if (buffer->flag.vram) {
		mm_node = buffer->vm_node->mm_node;
	}
	return mm_node;
}

int lisuan_gem_map_system_pages(struct lisuan_file *ls_file, struct lisuan_gem_bo *buffer, struct vm_area_struct *vma)
{
	struct sg_table *table = buffer->tbo.sg;
	struct scatterlist *sg = table->sgl;
	unsigned long long start = vma->vm_start, pfn;

	struct page **pages;
#if DRM_VERSION_CODE >= KERNEL_VERSION(6,6,0)
	vm_flags_set(vma, VM_MIXEDMAP);
#else
	vma->vm_flags |= VM_MIXEDMAP;
#endif
	vma_config(vma, buffer->flag.snoop);

	while (sg) {
		if (sg_page(sg)) {
			pfn = page_to_pfn(sg_page(sg));
			if (remap_pfn_range(vma, start, pfn, sg->length, vma->vm_page_prot)) {
				return -EAGAIN;
			}
			start += sg->length;
		}
		sg = __sg_next(sg);
	}
	return 0;
}

int lisuan_gem_map_vmarea(struct lisuan_file *ls_file, struct lisuan_gem_bo *buffer, struct vm_area_struct *vma)
{
	struct lisuan_device *ls_dev = ls_file->ls_dev;
	resource_size_t start = ls_dev->pdev_info.start_addr_vram;
	struct drm_mm_node *mm_node = __get_gem_buffer_mm_node(buffer);

	ASSERT(mm_node && buffer->flag.vram && buffer->domain == dom_visual_vram);

	vma->vm_pgoff = (mm_node->start + start - ls_dev->mmu_bypass_start) >> ls_dev->sys_page_shift;

	vma_config(vma, buffer->flag.snoop);

	if (remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff, vma->vm_end - vma->vm_start, vma->vm_page_prot)) {
		return -EAGAIN;
	}
	return 0;
}

void *lisuan_map_system_pages(struct lisuan_device *ls_dev, unsigned int cached, unsigned int size, struct sg_table *sg_table)
{
	pgprot_t prot = PAGE_KERNEL, old_prot = PAGE_KERNEL;
	void *virt_addr = NULL;
	int page_cnt = PAGE_ALIGN(size) >> ls_dev->sys_page_shift;
	struct page **pages = collect_page(sg_table, page_cnt);

	if (!cached) {
#ifdef CONFIG_X86
#ifdef CONFIG_X86_PAT
		prot = __pgprot((pgprot_val(old_prot) & ~_PAGE_CACHE_MASK) | cachemode2protval(_PAGE_CACHE_MODE_WC));
#else
		prot = pgprot_noncached(old_prot);
#endif
#else
		prot = pgprot_writecombine(old_prot);
#endif
	}

	virt_addr = vmap(pages, page_cnt, VM_MAP, prot);

	if (pages)
		gf_kvfree(pages);

	return virt_addr;
}

int lisuan_gem_query_infos_ioctl(struct drm_device *dev, void *data, struct drm_file *filp)
{
    struct lisuan_device *ls_dev = dev->dev_private;
    struct lisuan_file *ls_file = filp->driver_priv;
    struct drm_gem_object *base = NULL;
    struct lisuan_gem_bo *buffer = NULL;
    struct lisuan_drm_query_gem_info *query_info = (struct lisuan_drm_query_gem_info*)data;
    void __user *uptr = u64_to_user_ptr(query_info->p_private_driver_data);
    struct allocation_info_s alloc_infos = {0};
    int ret = 0;

    ret = copy_from_user(&alloc_infos, uptr, sizeof(struct allocation_info_s));
    base = drm_gem_object_lookup(filp, query_info->h_allocation_);
    if(!base)
        return -1;

    buffer = gem_to_gf_bo(base);

    alloc_infos.size_in_byte = buffer->size_org;
    alloc_infos.align = buffer->align_org;
    alloc_infos.supported_read_segment_set = buffer->domain;
    alloc_infos.is_tiled = buffer->is_tiled;

    if(buffer->compressFormat){
        alloc_infos.compress_format = buffer->compressFormat;
        alloc_infos.cl_cell_index = buffer->cf_node->cell_index;
    }
    ret = copy_to_user(uptr, &alloc_infos, sizeof(struct allocation_info_s));

	drm_gem_object_put_compat(base);
    return ret;
}

int lisuan_gem_is_busy_ioctl(struct drm_device *dev, void *data, struct drm_file *filp)
{
	struct lisuan_gem_busy_info *binfo = data;
	struct lisuan_device *ls_dev = dev->dev_private;
	struct lisuan_file *ls_file = filp->driver_priv;
	struct drm_gem_object *gobj;
	struct lisuan_gem_bo *buffer = NULL;
	int ret;

	gobj = drm_gem_object_lookup(filp, binfo->handle);
	if (!gobj) {
		gf_error("%u isn't a GEM object handle\n", binfo->handle);
		return -ENOENT;
	}
#if DRM_VERSION_CODE >= KERNEL_VERSION(6,6,0)
	ret = dma_resv_test_signaled(gobj->resv, DMA_RESV_USAGE_WRITE);
#elif DRM_VERSION_CODE >= KERNEL_VERSION(5,15,0)
	//only test only test the exclusive fence
	ret = dma_resv_test_signaled(gobj->resv, false);
#elif DRM_VERSION_CODE >= KERNEL_VERSION(5,4,0)
	ret = dma_resv_test_signaled_rcu(gobj->resv, false);
#else
	buffer = gem_to_gf_bo(gobj);
	ret = reservation_object_test_signaled_rcu(buffer->tbo.resv, false);
#endif
	if (ret == true) {
		binfo->status = 0;
	} else {
		binfo->status = EBUSY;
	}

	drm_gem_object_put_compat(gobj);

	return ret;
}

static int lisuan_dma_buf_attach(struct dma_buf *dmabuf,
				 struct dma_buf_attachment *attach)
{
	struct drm_gem_object *gem = dmabuf->priv;
	struct lisuan_gem_bo *gbo = gem_to_gf_bo(gem);
	struct lisuan_device *ls_dev =  (struct lisuan_device *)gem->dev->dev_private;
	int r = 0;

	if(gbo->domain == dom_invisual_vram ||
		(gbo->domain == dom_visual_vram && 
		lisuan_pci_p2pdma_distance_many(to_pci_dev(gem->dev->dev), &attach->dev, 1, false) < 0))
		return -EOPNOTSUPP;

	if (attach->dev->driver == ls_dev->dev->driver)
		return 0;

	return r;
}

static void lisuan_dma_buf_detach(struct dma_buf *dmabuf,
				  struct dma_buf_attachment *attach)
{
	struct drm_gem_object *gem = dmabuf->priv;
	struct lisuan_gem_bo *gbo = gem_to_gf_bo(gem);
	struct lisuan_device *ls_dev =  (struct lisuan_device *)gem->dev->dev_private;

}

const struct dma_buf_ops lisuan_dmabuf_ops = {
	.attach = lisuan_dma_buf_attach,
	.detach = lisuan_dma_buf_detach,

	.map_dma_buf = lisuan_map_dma_buf,
	.unmap_dma_buf = lisuan_unmap_dma_buf,

	.release = drm_gem_dmabuf_release,
	.begin_cpu_access = NULL,
	.end_cpu_access = NULL,

	.mmap = lisuan_gem_dmabuf_mmap,
	.vmap = drm_gem_dmabuf_vmap,
	.vunmap = drm_gem_dmabuf_vunmap,
};

void lisuan_gem_object_free(struct drm_gem_object *gobj)
{
	struct lisuan_gem_bo *buffer = gem_to_gf_bo(gobj);
	struct ttm_buffer_object *tbo;
	struct lisuan_file *ls_file;
	struct lisuan_device *ls_dev;
	struct lisuan_vm_node *vm_node, *tmp;
	struct dma_buf *dma_buf;

	if(!buffer)
		return;

	ls_file = buffer->ls_file;
	ls_dev =  (struct lisuan_device *)gobj->dev->dev_private;

	gem_info("free gem-tgid:0x%x-%d, %p\n", buffer->handle, buffer->ls_file?buffer->ls_file->tgid:0, buffer);

	if(buffer->evicted){
		//TODO free
		gf_assert(0);
		mutex_lock(&ls_dev->lock.lock);
		list_del_init(&buffer->item);
		mutex_unlock(&ls_dev->lock.lock);
	}

	//dma_resv_wait_timeout(buffer->tbo.base.resv, true, true, MAX_LONG);
	if (buffer->virt_addres) {
		if (buffer->domain <= dom_invisual_vram)
			iounmap(buffer->virt_addres);
		else if (buffer->domain <= dom_snoop_system_pages) {
			vunmap(buffer->virt_addres);
		}
		buffer->virt_addres = NULL;
	}

	list_for_each_entry_safe(vm_node, tmp, &buffer->va_list, node_item) 
	{
		gf_assert(vm_node->byPass);

		//release bypass pcie
		if (buffer->tbo.sg != NULL)
		{
			gf_assert(buffer->tbo.sg != NULL);

			lisuan_vm_unmap_va_range(ls_dev, ls_file, buffer, vm_node);
			list_del(&vm_node->node_item);
			vm_node->free(vm_node);
		}
	}

	if (gobj->import_attach)
	{
		struct dma_buf *dmabuf = gobj->import_attach->dmabuf;

		dma_buf_unmap_attachment(gobj->import_attach, buffer->tbo.sg, DMA_BIDIRECTIONAL);
		dma_buf_detach(dmabuf, gobj->import_attach);
		dma_buf_put(dmabuf);
	}
	else
	{
		if(buffer->swap_storage)
		{
			fput(buffer->swap_storage);
			buffer->swap_storage = NULL;
		}

		if (buffer->tbo.sg) {
			lisuan_free_system_pages(ls_dev, ls_file, buffer->tbo.sg, buffer->flag.snoop, buffer->size);
			buffer->tbo.sg = NULL;
		}

		if(buffer->vm_node)
		{
			vm_node = buffer->vm_node;
			vm_node->free(vm_node);
			buffer->vm_node = NULL;
		}
	}

#if DRM_VERSION_CODE >= KERNEL_VERSION(5,15,0)
	gf_kvfree(buffer->tbo.resource);
#endif

	if (buffer->cf_node) {
		buffer->cf_node->free(buffer->cf_node);
		gf_kvfree(buffer->cf_node);
		buffer->cf_node = NULL;
	}

	drm_gem_object_release(gobj);
	gf_kvfree(buffer);
}

int lisuan_gem_object_open(struct drm_gem_object *obj, struct drm_file *file_priv)
{
	struct lisuan_gem_bo *buffer = gem_to_gf_bo(obj);
	struct lisuan_file *ls_file = (struct lisuan_file *)file_priv->driver_priv;

	//a patch for other vender device's mem import,
	//since we not drm file in import stage.
	if(obj->import_attach && buffer->ls_file == NULL)
	{
		//TODO: loop idr table to comfirm the handle, 
		//temp not do it since we no need it.
		buffer->ls_file = ls_file;
	}
	gem_info("open gem-tgid:0x%x-%d-%d-%s-%s, cur:%d-%s, %p, dom:%d, bypass:%d in file tgid:%d-%d-%s-%s, handle count:%d\n",
		buffer->handle, buffer->ls_file?buffer->ls_file->tgid:0, buffer->ls_file?buffer->ls_file->pid:0, 
		buffer->ls_file?buffer->ls_file->name:0, buffer->ls_file?buffer->ls_file->usedname:0, 
		current->pid,current->comm, buffer, buffer->domain, buffer->flag.byPass, 
		ls_file->tgid,ls_file->pid, ls_file->name, ls_file->usedname, obj->handle_count);

	return 0;
}

//called by umd DRM_IOCTL_GEM_CLOSE
void lisuan_gem_object_close(struct drm_gem_object *gem_buffer, struct drm_file *file_priv)
{
	struct lisuan_gem_bo *buffer = gem_to_gf_bo(gem_buffer);
	struct lisuan_file *ls_file = (struct lisuan_file *)file_priv->driver_priv;
	struct lisuan_vm_node *vm_node, *tmp;
	struct lisuan_device *ls_dev =  (struct lisuan_device *)gem_buffer->dev->dev_private;
	int ret;
	bool intr = true;

	/*
	* When the application is forcibly terminated via Ctrl+C, it enters the drm_release
	* flow, which in turn calls close_gem step by step from smallest to largest gem handle.
	* The gem with handle = 1 will be the first one to be closed.
	*
	* The UMD ensures that all jobs in the drm file are added to this first gem, and
	* requires that the subsequent dma fence idle wait cannot be interrupted. This guarantees
	* that the application can safely exit.
	*/
	if(buffer->handle <= 2)
		intr = false;

	gem_info("close gem-tgid:0x%x-%d-%d-%s-%s, cur:%d-%s, %p, dom:%d, bypass:%d in file tgid:%d-%d-%s-%s, handle count:%d\n",
		buffer->handle, buffer->ls_file?buffer->ls_file->tgid:0, buffer->ls_file?buffer->ls_file->pid:0, 
		buffer->ls_file?buffer->ls_file->name:0, buffer->ls_file?buffer->ls_file->usedname:0, 
		 current->pid,current->comm, buffer, buffer->domain, buffer->flag.byPass, 
		ls_file->tgid,ls_file->pid, ls_file->name, ls_file->usedname, gem_buffer->handle_count);

#if DRM_VERSION_CODE >= KERNEL_VERSION(5,19,0)
	if (!dma_resv_test_signaled(gem_buffer->resv, DMA_RESV_USAGE_BOOKKEEP))
		dma_resv_wait_timeout(gem_buffer->resv, DMA_RESV_USAGE_BOOKKEEP, intr, MAX_LONG);
#elif DRM_VERSION_CODE >= KERNEL_VERSION(5,14,0)
	if(!dma_resv_test_signaled(gem_buffer->resv, true))
		dma_resv_wait_timeout(gem_buffer->resv, true, intr, MAX_LONG);
#elif DRM_VERSION_CODE >= KERNEL_VERSION(5,4,0)
	if(!dma_resv_test_signaled_rcu(gem_buffer->resv, true))
		dma_resv_wait_timeout_rcu(gem_buffer->resv, true, intr, MAX_LONG);
#else
	if(!reservation_object_test_signaled_rcu(&buffer->_tbo_resv, true))
		reservation_object_wait_timeout_rcu(&buffer->_tbo_resv, true, intr, MAX_LONG);
#endif

	mutex_lock(&buffer->va_list_lock.lock);
	list_for_each_entry_safe(vm_node, tmp, &buffer->va_list, node_item) 
	{
		if(vm_node->byPass || vm_node->ls_file != ls_file)
		{
			continue;
		}

		gem_info("gem-tgid-pid:0x%x-%d-%d(%p), cur:%d, in file:%d-%d(%p), %p, leak gpu_va:0x%llx-0x%llx vm_node from tid-pid:%d-%d(%p), vm node ref:%d, gem handle cnt:%d\n",
			buffer->handle, buffer->ls_file->tgid, buffer->ls_file->pid, buffer->ls_file,  current->pid, ls_file->tgid, ls_file->pid, ls_file, buffer,
			vm_node->offset, vm_node->offset+ vm_node->size,
			vm_node->ls_file?vm_node->ls_file->tgid:0, vm_node->ls_file?vm_node->ls_file->pid:0, vm_node->ls_file, vm_node->ref_cnt, gem_buffer->handle_count);

		//unmap maped va ranged
		if(vm_node->age)
			lisuan_vm_unmap_va_range(ls_dev, ls_file, buffer, vm_node);
		list_del(&vm_node->node_item);
		vm_node->free(vm_node);
	}
	mutex_unlock(&buffer->va_list_lock.lock);
}
#if DRM_VERSION_CODE >= KERNEL_VERSION(5,0,0)
struct dma_buf *lisuan_gem_object_export(struct drm_gem_object *gem_obj, int flags)
#else
struct dma_buf *lisuan_gem_object_export(struct drm_device *dev, struct drm_gem_object *gem_obj, int flags)
#endif
{
	struct lisuan_gem_bo *buffer = gem_to_gf_bo(gem_obj);
	struct dma_buf *dma_buf;
	struct drm_gem_object *geo = tbo_to_drm_gem_bo(&buffer->tbo);

#if DRM_VERSION_CODE >= KERNEL_VERSION(5,0,0)
	dma_buf = drm_gem_prime_export(gem_obj, flags);
#else
	dma_buf = drm_gem_prime_export(dev, gem_obj, flags);
#endif

	if (!IS_ERR(dma_buf)) {
		dma_buf->file->f_mapping = gem_obj->dev->anon_inode->i_mapping;
		dma_buf->ops = &lisuan_dmabuf_ops;

		gem_info("export gem-tgid:0x%x-%d, %p, cur tgid:%d-%s, is_present:%d, bypass:%d, export fd, dma_buf:0x%p, evict:%d\n", 
			buffer->handle, buffer->ls_file->tgid, buffer, (unsigned long long)current->tgid,  current->comm,
			buffer->is_present, buffer->flag.byPass, dma_buf, buffer->evicted);//fd handle still not ready, generate from dma_buf_fd

		atomic_inc(&buffer->unpagable);
	}
	return dma_buf;
}

static void gf_vm_open(struct vm_area_struct *vma)
{
	drm_gem_vm_open(vma);
}

static void gf_vm_close(struct vm_area_struct *vma)
{
	drm_gem_vm_close(vma);
}

struct vm_operations_struct gf_vm_ops = {
	.open = gf_vm_open,
	.close = gf_vm_close,
};

#if DRM_VERSION_CODE >= KERNEL_VERSION(5,0,0)
static const struct drm_gem_object_funcs lisuan_gem_object_funcs = {
	.free   = lisuan_gem_object_free,
	.open   = lisuan_gem_object_open,
	.close  = lisuan_gem_object_close,
	.export = lisuan_gem_object_export,
	.vmap   = lisuan_gem_prime_vmap,
	.vunmap = lisuan_gem_prime_vunmap,
	.vm_ops = &gf_vm_ops,
};
#endif
int lisuan_alloc_gem_bo(struct lisuan_device *ls_device, struct lisuan_file *ls_file, struct lisuan_gem_bo *buffer)
{
	int ret = 0;
	struct drm_gem_object *gem = tbo_to_drm_gem_bo(&buffer->tbo);
#if DRM_VERSION_CODE >= KERNEL_VERSION(5,15,0)
	buffer->tbo.resource = (struct ttm_resource *)gf_kvcalloc(1, sizeof(struct ttm_resource), GFP_KERNEL);
#endif
	buffer->ls_file = ls_file;

	drm_gem_private_object_init(ls_device->ddev, gem, buffer->size);

#if DRM_VERSION_CODE < KERNEL_VERSION(5,4,0)
	if (!buffer->tbo.resv){
		reservation_object_init(&buffer->_tbo_resv);
		buffer->tbo.resv = &buffer->_tbo_resv;
	}
#endif

#if DRM_VERSION_CODE > KERNEL_VERSION(5,0,0)
	gem->funcs = &lisuan_gem_object_funcs;
#endif

	mutex_init(&buffer->lock.lock);
	mutex_init(&buffer->va_list_lock.lock);
	INIT_LIST_HEAD(&buffer->va_list);
	INIT_LIST_HEAD(&buffer->item);
	INIT_LIST_HEAD(&buffer->free_pt_buffer);

	atomic64_set(&buffer->resident_cnt, 0);

	buffer->mmu_bypass_start = ls_device->mmu_bypass_start;

	if(ls_file){
		drm_gem_handle_create(ls_file->drm_file, gem, &buffer->handle);
		//drm_gem_object_put_unlocked not in drm_gem.h file,cannot be used
		drm_gem_object_put_compat(gem);
	}

	return 0;
}

void *lisuan_map_vram_node(struct lisuan_device *ls_dev, struct drm_mm_node *mm_node, bool byPass)
{
	resource_size_t cpupa_start = ls_dev->pdev_info.start_addr_vram;
	unsigned long long bypass_start = ls_dev->mmu_bypass_start;

	ASSERT(mm_node->start >= bypass_start && ((mm_node->start + mm_node->size) <= (bypass_start + ls_dev->visual_vram_size)));

	return ioremap_wc(cpupa_start + mm_node->start - bypass_start, mm_node->size);
}

int lisuan_gem_alloc_oneshot(struct lisuan_device *ls_dev, struct lisuan_file *ls_file, struct lisuan_gem_bo *gbo, unsigned int need_map)
{
	int domain = (gbo->domain <= dom_invisual_vram)?gbo->domain:dom_system_pages;
	struct lisuan_mman *mmgr = &ls_dev->mmgr[domain - 1];
	struct lisuan_vm_node *vm_node = NULL;
	struct drm_gem_object * gem = tbo_to_drm_gem_bo(&gbo->tbo);
	int ret = 0;

	gbo->size_org = gbo->size;
	gbo->align_org = gbo->align;

	//gem buf size need align to sys page size, or gem init will bug check fail in drm_gem_private_object_init
	if(gbo->align < ls_dev->sys_page_size)
		gbo->align =  ls_dev->sys_page_size;
	gbo->size = ALIGN64_SIZE(gbo->size , ls_dev->sys_page_size);

	gf_assert(gbo->flag.byPass == 1);

	lisuan_alloc_gem_bo(ls_dev, ls_file, gbo);

	vm_node = lisuan_gem_map_vmnode(mmgr, ls_dev, ls_file, gbo, 0);
	if (!vm_node) {
		ret = -1;
		goto fail;
	}

	if(gbo->domain <= dom_invisual_vram)
	{
#if DRM_VERSION_CODE >= KERNEL_VERSION(5,15,0)
		gbo->tbo.resource->start = vm_node->offset;
#else
		gbo->tbo.mem.start = vm_node->offset;
#endif
		if (need_map)
		{
			ASSERT(gbo->domain != dom_invisual_vram);
			gbo->virt_addres = lisuan_map_vram_node(ls_dev, vm_node->mm_node, gbo->flag.byPass);
		}
	}
	else
	{
		if(!gbo->tbo.sg){
			ret = lisuan_alloc_system_pages(ls_dev, ls_file, gem->size, &gbo->tbo.sg, gbo->flag.snoop);
			if(ret)
				goto fail;
		}

		if (need_map) {
			gbo->virt_addres = lisuan_map_system_pages(ls_dev, gbo->flag.snoop, gbo->size, gbo->tbo.sg);
		}
	}

	gbo->flag.pa_exist = 1;
	atomic64_inc(&gbo->resident_cnt);
	lisuan_vm_map_bypass_pt(ls_dev, gbo, vm_node);

	return ret;
fail:
	if(vm_node)
		vm_node->free(vm_node);
#if DRM_VERSION_CODE >= KERNEL_VERSION(5,15,0)
	if(gbo->tbo.resource)
		gf_kvfree(gbo->tbo.resource);
#endif
	return ret;
}

void lisuan_gem_free_oneshot(struct lisuan_device *ls_dev, struct lisuan_gem_bo *gem)
{
	struct lisuan_vm_node *vm_node, *tmp;

	ASSERT(gem->flag.byPass == 1);

	if (gem->virt_addres) {
		if (gem->domain <= dom_invisual_vram)
			iounmap(gem->virt_addres);
		else if (gem->domain <= dom_snoop_system_pages) {
			vunmap(gem->virt_addres);
		}
		gem->virt_addres = NULL;
	}

	mutex_lock(&gem->va_list_lock.lock);
	list_for_each_entry_safe(vm_node, tmp, &gem->va_list, node_item) 
	{
		list_del(&vm_node->node_item);

		if(vm_node == gem->vm_node)
		{
			continue;
		}
	
		lisuan_vm_unmap_va_range(ls_dev, gem->ls_file, gem, vm_node);

		//TODO:direct free here, may cause hw page fault
		vm_node->free(vm_node);
	}
	mutex_unlock(&gem->va_list_lock.lock);

	lisuan_gem_object_free(tbo_to_drm_gem_bo(&gem->tbo));
}

struct lisuan_gem_bo * lisuan_alloc_idb_buffer(struct lisuan_device *ls_dev, unsigned int cmd_size)
{
	struct lisuan_gem_bo *bufObj = NULL;
	int ret = 0;

	bufObj = gf_kvcalloc(1, sizeof(struct lisuan_gem_bo), GFP_KERNEL);
	if(!bufObj)
		return NULL;

	bufObj->size = ALIGN(cmd_size, GPU_PAGE_SIZE);
	bufObj->align = 64 * 1024;
	bufObj->domain = dom_visual_vram;
	bufObj->flag.byPass = 1;
	bufObj->pm_discard = 1;
	atomic_set(&bufObj->unpagable, 1);
	bufObj->priority = gem_priority_high;
	bufObj->flag.vram = (bufObj->domain <= dom_invisual_vram) ? 1 : 0;
	bufObj->flag.snoop = ((bufObj->domain == dom_snoop_system_pages) ? 1 : 0);

	ret = lisuan_gem_alloc_oneshot(ls_dev, NULL, bufObj, 1);
	if (ret)
	{
		gf_error("fail allocate idb size:%d\n", cmd_size);
		gf_kvfree(bufObj);
		return NULL;
	}

	return bufObj;
}
void lisuan_get_phys_addr(struct lisuan_device *ls_dev, struct ttm_buffer_object *tbo, unsigned page_idx, unsigned page_size, unsigned long long *pa, bool vram)
{
	struct lisuan_gem_bo * gem = container_of(tbo, struct lisuan_gem_bo, tbo);
	int total_sys_page = 0, n;
	struct scatterlist *sg;
	unsigned long long sg_addr;

	if (vram) {
#if DRM_VERSION_CODE >= KERNEL_VERSION(5,15,0)
		*pa = tbo->resource->start + page_idx * page_size;
#else
		*pa = tbo->mem.start + page_idx * page_size;
#endif
		*pa -= ls_dev->mmu_bypass_start;
		gf_assert(page_idx * page_size <= gem->size);
	} else {
		if (tbo && tbo->sg) {
			gf_assert(GPU_PAGE_SIZE == page_size);

			for_each_sg(tbo->sg->sgl, sg, tbo->sg->nents, n) {
				int page_num = ls_dev->sys_page_size / GPU_PAGE_SIZE;
				int cur_sg_entry_sys_page_cnt = (ls_dev->hwctl.iommu_enabled ? sg_dma_len(sg) : sg->length) >> ls_dev->sys_page_shift;

				if ((total_sys_page + cur_sg_entry_sys_page_cnt)*page_num >= (page_idx + 1))
				{
					sg_addr = (ls_dev->hwctl.iommu_enabled ? sg_dma_address(sg) : sg_phys(sg));

					*pa = sg_addr + (page_idx - total_sys_page*page_num) * GPU_PAGE_SIZE;
					return;
				}
				total_sys_page += cur_sg_entry_sys_page_cnt;
			}
			gf_assert(0);
		}
	}
}

static int lisuan_gem_swapin(struct lisuan_device *ls_dev, struct lisuan_gem_bo * gem)
{
	struct address_space *swap_space;
	struct file *swap_storage;
	struct page *from_page;
	struct page *to_page;
	gfp_t gfp_mask;
	int i=0, ret = 0;
	struct scatterlist *sg;
	struct sg_page_iter sg_iter;
	
	mutex_lock(&gem->lock.lock);

	if(!gem->swap_storage)
		goto out_err;

	swap_storage = gem->swap_storage;
	BUG_ON(swap_storage == NULL);

	ret = lisuan_alloc_system_pages(ls_dev, NULL, gem->size, &gem->tbo.sg, gem->flag.snoop);
	if (ret)
		goto out_err;

	swap_space = swap_storage->f_mapping;
	gfp_mask = mapping_gfp_mask(swap_space);

	for_each_sgtable_page(gem->tbo.sg, &sg_iter, 0)
	{
		to_page = sg_page_iter_page(&sg_iter);
		if (unlikely(to_page == NULL)) {
			ret = -ENOMEM;
			goto out_err;
		}

		from_page = shmem_read_mapping_page_gfp(swap_space, i++,
							gfp_mask);
		if (IS_ERR(from_page)) {
			ret = -1;
			goto out_err;
		}

		copy_highpage(to_page, from_page);
		put_page(from_page);
	}

	fput(swap_storage);
	gem->swap_storage = NULL;
	gem->flag.pa_exist = 1;

	mutex_unlock(&gem->lock.lock);
	atomic64_inc(&gem->resident_cnt);
	return 0;
out_err:	
	mutex_unlock(&gem->lock.lock);
	return ret;
}

static int lisuan_gem_swapout(struct lisuan_device *ls_dev, struct lisuan_gem_bo * gem, int force)
{
	loff_t size = gem->size;
	struct address_space *swap_space;
	struct file *swap_storage;
	struct page *from_page;
	struct page *to_page;
	int i=0, ret = 0, n;
	struct sg_page_iter sg_iter;
	gfp_t gfp_flags = GFP_KERNEL;

	mutex_lock(&gem->lock.lock);
#if DRM_VERSION_CODE < KERNEL_VERSION(7,0,0)
	swap_storage = shmem_file_setup("gem swap", size, 0);
#else
	swap_storage = shmem_file_setup("gem swap", size, EMPTY_VMA_FLAGS);
#endif
	if (IS_ERR(swap_storage)) {
		gf_error("Failed allocating swap storage\n");
		ret = -1;
		goto out_err;
	}

	swap_space = swap_storage->f_mapping;
	gfp_flags &= mapping_gfp_mask(swap_space);

	for_each_sgtable_page(gem->tbo.sg, &sg_iter, 0)
	{
		from_page = sg_page_iter_page(&sg_iter);
		if (unlikely(from_page == NULL))
			continue;
		
		to_page = shmem_read_mapping_page_gfp(swap_space, i++, gfp_flags);
		if (IS_ERR(to_page)) {
			ret = -1;
			goto out_err;
		}
		copy_highpage(to_page, from_page);
		set_page_dirty(to_page);
		mark_page_accessed(to_page);
		put_page(to_page);
	}

	gem->swap_storage = swap_storage;

	lisuan_free_system_pages(ls_dev, NULL, gem->tbo.sg, gem->flag.snoop, gem->size);
	gem->tbo.sg = NULL;
	gem->flag.pa_exist = 0;

	mutex_unlock(&gem->lock.lock);
	return ret;
out_err:
	mutex_unlock(&gem->lock.lock);
	if(swap_storage)
		fput(swap_storage);
	return ret;
}

int lisuan_gem_evict(struct lisuan_device *ls_dev, struct lisuan_gem_bo * gem, bool pm_op, int force)
{
	int ret = -1;
	struct lisuan_gem_bo * tmp;
	struct dma_fence * fence  = NULL;
	struct lisuan_vm_node *vm_node = NULL, *va_node=NULL, *temp;

	mutex_lock(&gem->lock.lock);

	if(gem->evicted || !gem->flag.vram)
		goto exit;

	if(atomic_read(&gem->unpagable) && !force)
		goto exit;

	gf_assert(gem->flag.vram &&gem->vm_node);
	tmp = gf_kvcalloc(1, sizeof(struct lisuan_gem_bo), GFP_KERNEL);
	tmp->size = gem->size;
	tmp->align = gem->align;
	tmp->domain = (ls_dev->hwctl.support_wc?dom_system_pages:dom_snoop_system_pages);
	tmp->flag.byPass = 1;
	tmp->pm_discard = 1;
	atomic_set(&tmp->unpagable, 1);
	tmp->priority = gem_priority_high;
	tmp->flag.vram = (tmp->domain <= dom_invisual_vram) ? 1 : 0;
	tmp->flag.snoop = ((tmp->domain == dom_snoop_system_pages) ? 1 : 0);

	ret = lisuan_gem_alloc_oneshot(ls_dev, NULL, tmp, 0);
	if(ret){
		gem_info("evict create tmp gem failed:%d\n", ret);
		goto fail;
	}

	gem->tbo.sg = tmp->tbo.sg;
	tmp->tbo.sg = NULL;

	vm_node = container_of(tmp->va_list.next, struct lisuan_vm_node, node_item);
	gem_info("evict handle:0x%x from tgid-pid:%d-%d, vm offset:0x%llx, size:0x%x, dst gpuva:0x%llx\n", gem->handle,
		gem->ls_file?gem->ls_file->tgid:ls_dev->vm_ctx->tgid,
		gem->ls_file?gem->ls_file->pid:ls_dev->vm_ctx->pid,
		gem->vm_node->offset,gem->size, vm_node->offset);

	mutex_lock(&gem->va_list_lock.lock);
	list_for_each_entry_safe(va_node, temp, &gem->va_list, node_item) {
		gem_info("evict handle:0x%x size:0x%x, from tgid-pid:%d-%d, va range:0x%llx-0x%llx\n", 
			gem->handle, gem->size,
			gem->ls_file?gem->ls_file->tgid:ls_dev->vm_ctx->tgid,
			gem->ls_file?gem->ls_file->pid:ls_dev->vm_ctx->pid,
			va_node->offset,va_node->offset + va_node->size);
	}
	mutex_unlock(&gem->va_list_lock.lock);

	gf_trans(ls_dev, gem, tmp, lisuan_trans_op_evict, &fence);

	dma_fence_wait(fence, true);

	//only none bypass gem's pa can free, since bypass va=pa+bypass_start
	if(!gem->flag.byPass && !pm_op)
	{
		//TODO unmap all va range
		gem->vm_node->free(gem->vm_node);
		gem->vm_node = NULL;
	}
	
	dma_fence_put(fence);
	gem->evicted = true;
	gem->flag.pa_exist = 0;

	mutex_lock(&ls_dev->lock.lock);
	list_move_tail(&gem->item, &ls_dev->list_lru[gem->priority]);
	mutex_unlock(&ls_dev->lock.lock);

exit:
	mutex_unlock(&gem->lock.lock);
	return ret;
fail:
	mutex_unlock(&gem->lock.lock);
	if(tmp){
		lisuan_gem_free_oneshot(ls_dev, tmp);
	}
	return ret;
}

int lisuan_gem_resident(struct lisuan_device *ls_dev, struct lisuan_gem_bo * gem, bool pm_op, int force)
{
	int ret = 0;
	struct lisuan_gem_bo * tmp = NULL;
	struct lisuan_vm_node * vm_node=NULL;
	unsigned int domain;
	struct lisuan_mman * mmgr = NULL;
	struct dma_fence * fence  = NULL;

	mutex_lock(&gem->lock.lock);
	if(!gem->evicted)
		goto fail;

	domain = (gem->domain <= dom_invisual_vram)?gem->domain:dom_system_pages;
	mmgr= &ls_dev->mmgr[domain - 1];
	gf_assert(gem->evicted && !gem->swap_storage);
	gf_assert(gem->flag.vram && gem->tbo.sg);

	//bypass local will not release vm_node
	if(!gem->vm_node && !pm_op){

		gf_assert(!gem->flag.byPass);
retry:
		mmgr = &ls_dev->mmgr[domain - 1];
		vm_node = lisuan_vm_alloc_node(mmgr, gem, gem->size, gem->align, 0);
		if(!vm_node){
			if(domain == dom_visual_vram)
				domain = dom_invisual_vram;
			else
				domain = dom_visual_vram;
			goto retry;
		}
		vm_node->gem = gem;
		gem->domain = domain;
		gem->vm_node = vm_node;
		spin_lock(&mmgr->lock);
		list_move_tail(&gem->item, &mmgr->list_lru[gem->priority]);
		spin_unlock(&mmgr->lock);

#if DRM_VERSION_CODE >= KERNEL_VERSION(5,15,0)
		gem->tbo.resource->start = vm_node->offset;
#else
		gem->tbo.mem.start = vm_node->offset;
#endif
		atomic64_inc(&gem->resident_cnt);
	}

	tmp = gf_kvcalloc(1, sizeof(struct lisuan_gem_bo), GFP_KERNEL);
	tmp->size = gem->size;
	tmp->align = gem->align;
	tmp->domain = (ls_dev->hwctl.support_wc?dom_system_pages:dom_snoop_system_pages);
	tmp->flag.byPass = 1;
	tmp->pm_discard = 1;
	tmp->tbo.sg = gem->tbo.sg;
	atomic_set(&tmp->unpagable, 1);
	tmp->priority = gem_priority_high;
	tmp->flag.vram = (tmp->domain <= dom_invisual_vram) ? 1 : 0;
	tmp->flag.snoop = ((tmp->domain == dom_snoop_system_pages) ? 1 : 0);

	ret = lisuan_gem_alloc_oneshot(ls_dev, NULL, tmp, 0);
	if(ret){
		gem_info("resident create tmp gem failed:%d\n", ret);
		goto fail;
	}

	gem->tbo.sg = NULL;
	gf_trans(ls_dev, tmp, gem,  lisuan_trans_op_resident, &fence);

	if(pm_op)
		dma_fence_wait(fence, true);
	dma_fence_put(fence);

	gem_info("resident handle:0x%x from tgid-pid:%d-%d, vm offset:0x%llx, size:%d\n", gem->handle,
		gem->ls_file?gem->ls_file->tgid:ls_dev->vm_ctx->tgid,
		gem->ls_file?gem->ls_file->pid:ls_dev->vm_ctx->pid,
		gem->vm_node->offset, gem->size);

	spin_lock(&mmgr->lock);
	list_move_tail(&gem->item, &mmgr->list_lru[gem->priority]);
	spin_unlock(&mmgr->lock);

	gem->flag.pa_exist = 1;
	gem->evicted = false;

	mutex_unlock(&gem->lock.lock);
	return ret;
fail:
	mutex_unlock(&gem->lock.lock);
	if(tmp)
	{
		tmp->tbo.sg = NULL;
		lisuan_gem_free_oneshot(ls_dev, tmp);
	}
	return ret;
}

void lisuan_try_evict_phys_mem(struct lisuan_device *ls_dev, struct lisuan_file *ls_file, unsigned int size)
{
	unsigned int domain = dom_visual_vram, i, ret;
	unsigned int evict_gem_num, max_evict_gem_num_per_domain = 5;
	struct lisuan_mman *mmgr;
	struct lisuan_gem_bo *gem, *tmp;
	struct lisuan_vm_node * vm_node, *vm_tmp;
	struct list_head todo;

	//only evict none bypass local mem
	for(;domain < dom_system_pages; domain++)
	{
		evict_gem_num = max_evict_gem_num_per_domain;
		INIT_LIST_HEAD(&todo);
		mmgr = &ls_dev->mmgr[domain -1];

		spin_lock(&mmgr->lock);
		for(i = gem_priority_unused; i < gem_priority_count; i++)
		{
			list_for_each_entry_safe(gem, tmp, &mmgr->list_lru[i], item)
			{
				//gf_info("try priority:%d gem:%x, unpage:%d, bpass:%d, vram:%d, size:0x%x\n", 
				//	i, gem->handle, gem->unpagable ,gem->flag.byPass ,gem->flag.vram ,gem->size);

				if(gem->flag.pa_exist && !atomic_read(&gem->unpagable) && !gem->flag.byPass && gem->flag.vram && gem->size >= size)
				{
#if DRM_VERSION_CODE >= KERNEL_VERSION(5,19,0)
					ret = dma_resv_test_signaled(tbo_to_drm_gem_bo(&gem->tbo)->resv, DMA_RESV_USAGE_BOOKKEEP);
#elif DRM_VERSION_CODE >= KERNEL_VERSION(5,14,0)
					ret = dma_resv_test_signaled(tbo_to_drm_gem_bo(&gem->tbo)->resv, true);
#elif DRM_VERSION_CODE >= KERNEL_VERSION(5,4,0)
					ret = dma_resv_test_signaled_rcu(tbo_to_drm_gem_bo(&gem->tbo)->resv, true);
#else
					ret = reservation_object_test_signaled_rcu(&gem->_tbo_resv, true);
#endif
					if (ret <= 0)
						continue;
					list_move_tail(&gem->item, &todo);

					if(--evict_gem_num == 0)
						goto nxt;
				}
			}
		}
nxt:
		spin_unlock(&mmgr->lock);

		list_for_each_entry_safe(gem, tmp, &todo, item)
		{
			lisuan_gem_evict(ls_dev, gem, false, false);
		}

		if(!list_empty(&todo))
		{
			spin_lock(&mmgr->lock);
			list_for_each_entry_safe(gem, tmp, &todo, item)
			{
				list_move_tail(&gem->item, &mmgr->list_lru[gem->priority]);
			}
			spin_unlock(&mmgr->lock);
		}
	}

	//swapout cur process's none bypass pcie mem
	evict_gem_num = max_evict_gem_num_per_domain;
	INIT_LIST_HEAD(&todo);
	mmgr = &ls_file->vm.mmgr;
	spin_lock(&mmgr->lock);
	list_for_each_entry_safe(vm_node, vm_tmp, &mmgr->list_head, mm_item)
	{
		struct lisuan_gem_bo * gem = vm_node->gem;

		if(gem->flag.pa_exist &&
			!atomic_read(&gem->unpagable) && 
			!gem->flag.byPass && 
			!gem->swap_storage &&
			!gem->flag.vram &&
			gem->tbo.sg &&
			gem->size >= size)
		{
#if DRM_VERSION_CODE >= KERNEL_VERSION(5,19,0)
			ret = dma_resv_test_signaled(tbo_to_drm_gem_bo(&gem->tbo)->resv, DMA_RESV_USAGE_BOOKKEEP);
#elif DRM_VERSION_CODE >= KERNEL_VERSION(5,14,0)
			ret = dma_resv_test_signaled(tbo_to_drm_gem_bo(&gem->tbo)->resv, true);
#elif DRM_VERSION_CODE >= KERNEL_VERSION(5,4,0)
			ret = dma_resv_test_signaled_rcu(tbo_to_drm_gem_bo(&gem->tbo)->resv, true);
#else
			ret = reservation_object_test_signaled_rcu(&gem->_tbo_resv, true);
#endif

			if (ret <= 0)
				continue;
			//TODO unmap all va range
			ret = lisuan_gem_swapout(ls_dev, gem, false);
			if(!ret && (--evict_gem_num == 0))
				goto nxt2;
		}
	}
nxt2:
	spin_unlock(&mmgr->lock);

}

void lisuan_gem_evict_all(struct lisuan_device *ls_dev)
{
	struct lisuan_mman *mmgr;
	struct lisuan_gem_bo * gem = NULL, *tmp = NULL;
	int domain = dom_visual_vram, ret = 0, i = 0, j = 0, evict_count = 0;
	struct dma_fence *fence;
	struct lisuan_gem_bo **evict_list = NULL;

	//only evict local mem
	for(;domain < dom_system_pages; domain++)
	{
		mmgr = &ls_dev->mmgr[domain -1];
		for(i = gem_priority_unused; i < gem_priority_count; i++)
		{
			evict_count = 0;
			evict_list = NULL;
			spin_lock(&mmgr->lock);
			list_for_each_entry_safe(gem, tmp, &mmgr->list_lru[i], item)
			{
				if(!gem->pm_discard)
				{	
					if(gem == ls_dev->pt.buf)
					{
						pm_info("The pt buffer found, and requires special treatment, skip %lx",gem);
						continue;
					}
					evict_count++;
					evict_list = krealloc(evict_list, evict_count * sizeof(*evict_list), GFP_KERNEL);
					if (!evict_list) {
 						spin_unlock(&mmgr->lock);
						gf_error("Failed to allocate evict_list\n");
						return;
					}
					evict_list[evict_count - 1] = gem;
				}
			}
			spin_unlock(&mmgr->lock);

			for (j = 0; j < evict_count; j++) {
				gem = evict_list[j];

#if DRM_VERSION_CODE >= KERNEL_VERSION(5,19,0)
				ret = dma_resv_wait_timeout(tbo_to_drm_gem_bo(&gem->tbo)->resv, DMA_RESV_USAGE_BOOKKEEP, true, MAX_LONG);
#elif DRM_VERSION_CODE >= KERNEL_VERSION(5,14,0)
				ret = dma_resv_wait_timeout(tbo_to_drm_gem_bo(&gem->tbo)->resv, true, true, MAX_LONG);
#elif DRM_VERSION_CODE >= KERNEL_VERSION(5,4,0)
				ret = dma_resv_wait_timeout_rcu(tbo_to_drm_gem_bo(&gem->tbo)->resv, true, true, MAX_LONG);
#else
				ret = reservation_object_wait_timeout_rcu(&gem->_tbo_resv, true, true, MAX_LONG);
#endif
				ret = lisuan_gem_evict(ls_dev, gem, true, true);
			}
			
			kfree(evict_list);
		}
	}
}

void lisuan_gem_resident_all(struct lisuan_device *ls_dev)
{
	struct lisuan_gem_bo * gem, *tmp;
	int ret = 0, i;

	//here we just resident bypass local mem
	//none bypass local will be residented at job prepare stage.
	mutex_lock(&ls_dev->lock.lock);
	for(i = gem_priority_unused; i < gem_priority_count; i++)
	{
		list_for_each_entry_safe(gem, tmp, &ls_dev->list_lru[i], item)
		{
			if(!gem->pm_discard && gem->flag.byPass)
			{
				if(gem->swap_storage){
					ret = lisuan_gem_swapin(ls_dev, gem);
					if(ret)
						gf_error("swapin gem:%x from tgid-pid:%d-%d, failed:%d\n",
							gem->handle,
							gem->ls_file?gem->ls_file->tgid:ls_dev->vm_ctx->tgid,
							gem->ls_file?gem->ls_file->pid:ls_dev->vm_ctx->pid,
							ret);
				}
				ret = lisuan_gem_resident(ls_dev, gem, false, true);
				if(ret){
					gf_error("resident gem:%x from tgid-pid:%d-%d, failed:%d\n",
						gem->handle,
						gem->ls_file?gem->ls_file->tgid:ls_dev->vm_ctx->tgid,
						gem->ls_file?gem->ls_file->pid:ls_dev->vm_ctx->pid,
						ret);
				}
			}
		}
	}
	mutex_unlock(&ls_dev->lock.lock);
}

void lisuan_prepare_gem(struct lisuan_device *ls_dev, struct lisuan_gem_bo *gem)
{
	lisuan_gem_swapin(ls_dev, gem);
	lisuan_gem_resident(ls_dev, gem, false, true);
}

void lisuan_gem_content_dump(struct lisuan_device *ls_dev, struct lisuan_gem_bo *gem, char* msg, unsigned int offset, unsigned int size)
{
	void *data=NULL, *p=NULL;

	if(gem->domain == dom_invisual_vram)
		return;

	if(gem->domain < dom_invisual_vram) 
		data = lisuan_map_vram_node(ls_dev, __get_gem_buffer_mm_node(gem), gem->flag.byPass);
	else
		data = lisuan_map_system_pages(ls_dev, gem->flag.snoop, gem->size, gem->tbo.sg);

	p = data;
	if(offset)
		p += offset;
	gf_dump_buffer_to_file(p, size ,msg);

	if (gem->domain < dom_invisual_vram)
		iounmap(data);
	else
		vunmap(data);
}
