/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#include <linux/idr.h>
#include <drm/drm_file.h>
#include <drm/drm_gem.h>
#include <linux/mm.h>
#include <linux/dma-buf.h>

#include "lisuan_drm.h"
#include "lisuan_device.h"
#include "lisuan_gem.h"
#include "lisuan_vm.h"
#include "lisuan_ring.h"
#include "gf_hw_misc.h"
#include "lisuan_procid.h"
#include "gf_job.h"
#include "gf_os_interface.h"

#include "gf_hw_misc.h"

extern struct lisuan_vm_update_map_funcs lisuan_vm_gpu_funcs;
extern struct lisuan_vm_update_map_funcs lisuan_vm_cpu_funcs;

static unsigned long long gf_mm_dump_hole(const struct drm_mm_node *entry)
{
	unsigned long long start, size;
	size = entry->hole_size;
	if (size) {
		start = drm_mm_hole_node_start(entry);
		//gf_info("%#018llx-%#018llx: %llu: free\n",start, start + size, size);
	}

	return size;
}

void gf_mm_print(const struct drm_mm *mm, unsigned long long *used, unsigned long long *free,  unsigned long long *total)
{
	const struct drm_mm_node *entry = &mm->head_node;
	unsigned long long total_used = 0, total_free = 0, total_all = 0;

	total_free += gf_mm_dump_hole(entry);

	drm_mm_for_each_node(entry, mm) {
		//gf_info("%#018llx-%#018llx: %llx: used\n", entry->start,entry->start + entry->size, entry->size);
		total_used += entry->size;
		total_free += gf_mm_dump_hole(entry);
	}
	total_all = total_free + total_used;

	if(used || free || total)
	{
		if(used) *used = total_used;
		if(free) *free = total_free;
		if(total) *total = total_all;
	}
	else
	{
		gf_info("total: %llx, used %llx free %llx\n", total_all, total_used, total_free);
	}
}

struct drm_mm_node *lisuan_vram_mgr_new(struct lisuan_mman *mmgr, unsigned int size, unsigned int align, int direction)
{
	int ret = 0;
	struct drm_mm_node *node = (struct drm_mm_node *)gf_kvcalloc(1, sizeof(struct drm_mm_node), GFP_KERNEL);
	if (!node)
		return NULL;
	spin_lock(&mmgr->lock);
	ret = drm_mm_insert_node_in_range(mmgr->mm, node,
					  size, align, 0, mmgr->start_addr, mmgr->end_addr, (direction ? DRM_MM_INSERT_HIGH : DRM_MM_INSERT_LOW));
	spin_unlock(&mmgr->lock);
	if (ret) {
#ifdef _DEBUG_
		spin_lock(&mmgr->lock);
		gf_mm_print(mmgr->mm, NULL, NULL, NULL);
		spin_unlock(&mmgr->lock);
#endif
		gf_kvfree(node);
		return NULL;
	}
	return node;
}

struct lisuan_vm_node *lisuan_vm_alloc_node(struct lisuan_mman *mmgr, struct lisuan_gem_bo * gem, unsigned int size, unsigned int align, int direction)
{
	int ret = 0;
	struct lisuan_vm_node *vm_node;
	struct drm_mm_node *mm_node;

	vm_node = (struct lisuan_vm_node *)gf_kvcalloc(1, sizeof(struct lisuan_vm_node), GFP_KERNEL);
	if (!vm_node)
		return NULL;

	vm_node->mmgr = mmgr;
	vm_node->age = 0;
	vm_node->free = lisuan_vm_free_node;
	vm_node->free_locked = lisuan_vm_free_node_locked;

	mm_node = lisuan_vram_mgr_new(mmgr, size, align, direction);
	if (!mm_node) {
		gf_kvfree(vm_node);
		return NULL;
	}

	vm_node->mm_node = mm_node;
	vm_node->gem = gem;

	vm_node->size = vm_node->mm_node->size;
	vm_node->offset = vm_node->mm_node->start;

	if(mmgr->type_2)
	{
		spin_lock(&mmgr->lock);
		list_add_tail(&vm_node->mm_item, &mmgr->list_head);
		spin_unlock(&mmgr->lock);
	}

	return vm_node;
}

void lisuan_vm_free_node_locked(struct lisuan_vm_node *vm_node)
{
	struct lisuan_mman *mmgr = NULL;

	if (!vm_node)
		return;

	mmgr = vm_node->mmgr;

	drm_mm_remove_node(vm_node->mm_node);

	if(mmgr->type_2)
	{
		list_del_init(&vm_node->mm_item);
	}else{
		struct lisuan_gem_bo * gem = vm_node->gem;
		list_del_init(&gem->item);
	}

	gf_kvfree(vm_node->mm_node);
	gf_kvfree(vm_node);
}

void lisuan_vm_free_node(struct lisuan_vm_node *vm_node)
{
	struct lisuan_mman *mmgr = NULL;

	if (!vm_node)
		return;

	mmgr = vm_node->mmgr;
	spin_lock(&mmgr->lock);
	lisuan_vm_free_node_locked(vm_node);
	spin_unlock(&mmgr->lock);
}

static void lisuan_buffer_config_copy(struct lisuan_buffer_config *dst, struct lisuan_buffer_config *src)
{
	dst->block = src->block;
	dst->large_page = src->large_page;
	dst->nsec = src->nsec;
	dst->pa_exist = src->pa_exist;
	dst->snoop = src->snoop;
	dst->vram = src->vram;
}

int lisuan_vm_map_va_range(struct lisuan_device *ls_dev, struct lisuan_file *ls_file, struct lisuan_gem_bo *buffer, struct lisuan_vm_node *vm_node)
{
	struct lisuan_vm_node *tmp_node;
	int ret = 0;

	ASSERT(buffer->flag.pa_exist);

	if (vm_node) {
		tmp_node = vm_node;
		goto map;
	}
	mutex_lock(&buffer->va_list_lock.lock);
	list_for_each_entry(tmp_node, &buffer->va_list, node_item) {
map: 
		if (tmp_node->age < atomic64_read(&buffer->resident_cnt)) 
		{
			if (tmp_node->byPass) {
				lisuan_vm_map_bypass_pt(ls_dev, buffer, tmp_node);
				tmp_node->age = atomic64_read(&buffer->resident_cnt);
			} else {
				struct lisuan_vm_update_params params = { 0 };

				if (tmp_node->ls_file == ls_file) {
					lisuan_buffer_config_copy(&params.flag, &buffer->flag);
					params.flag.byPass = tmp_node->byPass;
					params.vm = &ls_file->vm;
					params.node = tmp_node;
					params.tbo = &buffer->tbo;
					params.bitop_func = __bitmap_set;
					params.unmap = 0;
					if (buffer->flag.block)
						params.mtype = MEM_TYPE_4MB;
					else if (buffer->flag.large_page)
						params.mtype = MEM_TYPE_64KB;
					else
						params.mtype = MEM_TYPE_4KB;

					ret = lisuan_vm_bo_update_mapping(ls_dev, &params);
					if(params.mtype == MEM_TYPE_64KB){
						mutex_lock(&ls_file->va_list_lock->lock);
						list_add(&tmp_node->vm_item, &ls_file->va_64K_list);
						mutex_unlock(&ls_file->va_list_lock->lock);
					}
					if(ret == 0)
						tmp_node->age = atomic64_read(&buffer->resident_cnt);
				}
			}

			if (vm_node)
				return ret;
		}
	}
	mutex_unlock(&buffer->va_list_lock.lock);
	return ret;
}

void lisuan_vm_map_bypass_pt(struct lisuan_device *ls_dev, struct lisuan_gem_bo *gbo, struct lisuan_vm_node *vm_node)
{
	struct lisuan_bypass_pt *bypasspt = &ls_dev->pt;
	unsigned long long gpuva = vm_node->offset, pteindex;
	struct scatterlist *sg;

	unsigned int level2_pte = 0;

	int page_cnt, page_num = ls_dev->sys_page_size / GPU_PAGE_SIZE;
	int j, n = 0, total_sys_page = 0, page_idx = 0, dump_one=1;
	
	if (gbo->flag.vram ||
		!gbo->flag.byPass ||
		!gbo->tbo.sg)
		return;

	pteindex = (gpuva - ls_dev->mmu_bypass_pcie_start) >> GPU_PAGE_SHIFT;
	for_each_sg(gbo->tbo.sg->sgl, sg, gbo->tbo.sg->nents, n) {
		unsigned long long sg_addr = 0;

		sg_addr = (ls_dev->hwctl.iommu_enabled ? sg_dma_address(sg) : sg_phys(sg));
		page_cnt = (ls_dev->hwctl.iommu_enabled ? sg_dma_len(sg) : sg->length)>> ls_dev->sys_page_shift;

		for (page_idx = 0; page_idx < page_cnt; page_idx++) {
			for (j = 0; j < page_num; j++) {
				unsigned long long phys_addr = (sg_addr + page_idx * ls_dev->sys_page_size + j * GPU_PAGE_SIZE) >> GPU_PAGE_SHIFT;
				ASSERT(!(phys_addr & ~0xFFFFFFFULL));
					gf_hw_init_tlb_level2_pte(&level2_pte,1,0,1,gbo->flag.snoop,
					phys_addr);
				*(bypasspt->PTB2VA + pteindex + (total_sys_page + page_idx) * page_num + j) = level2_pte;

				if(dump_one){
					 vm_info("[bypass] gem:0x%x from tgid-pid:%d-%d, size:0x%llx, gpuva start:0x%llx, sg:0x%llx, offset:%x, pfn:%llx, gpuva:%llx, mem:%x:%x\n",
					 	gbo->handle, 
					 	gbo->ls_file?gbo->ls_file->tgid:(ls_dev->vm_ctx?ls_dev->vm_ctx->tgid:0),
					 	gbo->ls_file?gbo->ls_file->pid:(ls_dev->vm_ctx?ls_dev->vm_ctx->pid:0),
					 	gbo->size, gpuva, gbo->tbo.sg,
					 	pteindex + (total_sys_page + page_idx) * page_num + j, phys_addr, 
					 	gpuva + (total_sys_page + page_idx)*ls_dev->sys_page_size + j*GPU_PAGE_SIZE, 
					 	*(bypasspt->PTB2VA + pteindex + (total_sys_page + page_idx)*page_num + j), level2_pte);
				 	dump_one=0;
				}
			}
		}
		total_sys_page += page_cnt;
	}

	ASSERT(total_sys_page * ls_dev->sys_page_size >= gbo->size);
	vm_node->age = atomic64_read(&gbo->resident_cnt);

	spin_lock(&ls_dev->bypass_lock);
	ls_dev->is_bypass_update = 0xFF;
	spin_unlock(&ls_dev->bypass_lock);

}

void lisuan_vm_unmap_va_range(struct lisuan_device *ls_dev, struct lisuan_file *ls_file, struct lisuan_gem_bo *buffer, struct lisuan_vm_node *vm_node)
{
	struct lisuan_vm_node *tmp_node;

	if (vm_node) {
		tmp_node = vm_node;
		goto unmap;
	}

	mutex_lock(&buffer->va_list_lock.lock);
	list_for_each_entry(tmp_node, &buffer->va_list, node_item)
	{
unmap:
		if (tmp_node->byPass)
		{
			lisuan_vm_unmap_bypass_pt(ls_dev, buffer, tmp_node);
		}
		else 
		{
			if (tmp_node->ls_file == ls_file)
			{
				struct lisuan_vm_update_params params = { 0 };

				params.vm = &ls_file->vm;
				params.node = tmp_node;
				params.unmap = 1;
				lisuan_buffer_config_copy(&params.flag, &buffer->flag);
				params.flag.byPass = tmp_node->byPass;
				params.bitop_func = __bitmap_clear;
				params.tbo = &buffer->tbo;
				if (buffer->flag.block)
					params.mtype = MEM_TYPE_4MB;
				else if (buffer->flag.large_page)
					params.mtype = MEM_TYPE_64KB;
				else
					params.mtype = MEM_TYPE_4KB;

				if(params.mtype == MEM_TYPE_64KB)
				{
					mutex_lock(&ls_file->va_list_lock->lock);
					list_del(&tmp_node->vm_item);
					mutex_unlock(&ls_file->va_list_lock->lock);
				}

				lisuan_vm_unmap_and_release_pt(ls_dev, &params);
			}
		}

		if (vm_node)
			return;
	}
	mutex_unlock(&buffer->va_list_lock.lock);
}

void lisuan_vm_unmap_bypass_pt(struct lisuan_device *ls_dev, struct lisuan_gem_bo *gem, struct lisuan_vm_node *vm_node)
{
	struct lisuan_bypass_pt *bypasspt = &ls_dev->pt;
	unsigned long long gpuva = vm_node->offset, pteidx;
	struct scatterlist *sg;
	int j, page_num = ls_dev->sys_page_size / GPU_PAGE_SIZE; 
	int total_sys_page = gem->size / ls_dev->sys_page_size, page_idx = 0, dump_one=1;

	unsigned int level2_pte = 0;

	if (ls_dev->hwctl.skip_invalidate 
		|| gem->flag.vram 
		|| !gem->flag.byPass
		|| !gem->tbo.sg)
		return;

	gf_hw_init_tlb_level2_pte(&level2_pte,0,0,0,gem->flag.snoop,0);

	pteidx = (gpuva - ls_dev->mmu_bypass_pcie_start) >> GPU_PAGE_SHIFT;

	for (page_idx = 0; page_idx < total_sys_page; page_idx++) {
		for (j = 0; j < page_num; j++) {
			*(bypasspt->PTB2VA + pteidx + page_idx * page_num + j) = level2_pte;
			if(dump_one){
				 vm_info("[inv bypass] gem:0x%x from tgid-pid:%d-%d, size:0x%llx, gpuva start:0x%llx, sg:0x%llx, offset:%x, gpuva:%llx, mem:%x:%x\n",
					gem->handle, 
					gem->ls_file?gem->ls_file->tgid:(ls_dev->vm_ctx?ls_dev->vm_ctx->tgid:0),
					gem->ls_file?gem->ls_file->pid:(ls_dev->vm_ctx?ls_dev->vm_ctx->pid:0),
					gem->size, gpuva, gem->tbo.sg,
					pteidx + page_idx * page_num + j,
					gpuva + page_idx*ls_dev->sys_page_size + j*GPU_PAGE_SIZE, 
					*(bypasspt->PTB2VA + pteidx + page_idx * page_num + j), level2_pte);
				dump_one=0;
			}
		}
	}

	spin_lock(&ls_dev->bypass_lock);
	ls_dev->is_bypass_update = 0xFF;
	spin_unlock(&ls_dev->bypass_lock);
}

unsigned lisuan_vm_num_entries(struct lisuan_device *ls_dev, int level, int mtype)
{
	return LS_VM_PTE_COUNT(ls_dev, level, mtype);
}

int lisuan_vm_entries_mask(struct lisuan_device *ls_dev, int level, unsigned int mtype)
{
	int entries = lisuan_vm_num_entries(ls_dev, level, mtype);

	return entries - 1;
}

unsigned lisuan_vm_level_shift(struct lisuan_device *ls_dev, unsigned level, unsigned mtype)
{
	unsigned int bitcnt = 0;
	unsigned int root_level = LS_VM_PD0;

	while (root_level <= level)
		bitcnt += ls_dev->pt_count[mtype][root_level++];
	return ls_dev->max_address_bits - bitcnt;

}

static bool lisuan_vm_pt_descendant(struct lisuan_device *ls_dev, struct lisuan_vm_pt_cursor *cursor)
{
	unsigned mask, shift, idx, mtype = cursor->mtype;

	if (!cursor->entry->entries)
		return false;

	mask = lisuan_vm_entries_mask(ls_dev, cursor->level, mtype);
	shift = lisuan_vm_level_shift(ls_dev, cursor->level, mtype);

	++cursor->level;
	idx = (cursor->start >> shift) & mask;
	cursor->parent = cursor->entry;
	cursor->entry = &cursor->entry->entries[idx];
	return true;
}

static int lisuan_vm_free_pt(struct lisuan_file *ls_file, struct lisuan_vm_pt *pt)
{
	if (pt->buffer) {
		lisuan_gem_free_oneshot(ls_file->ls_dev, pt->buffer);
	}

	if (pt->entries) {
		gf_kvfree(pt->entries);
		pt->entries = NULL;
	}

	if (pt->bitmaps) {
		bitmap_free(pt->bitmaps);
		pt->bitmaps=NULL;
	}

	return 0;
}
/*
static int lisuan_vm_free_pds(struct lisuan_device *ls_dev, struct lisuan_vm_iter_pd_params *params)
{
	struct lisuan_vm *vm = params->vm;
	struct gf_job *job = params->job;
	struct lisuan_vm_iter_pd_params t_params = *params;
	struct lisuan_file *ls_file = container_of(vm, struct lisuan_file, vm);
	struct lisuan_vm_pt_cursor *cursor = (struct lisuan_vm_pt_cursor *)gf_kvcalloc(1, sizeof(struct lisuan_vm_pt_cursor), GFP_KERNEL);
	struct lisuan_vm_pt *parent = t_params.parent;
	unsigned pdestart, pdeend, pdecur;
	unsigned mask, idx, num_ptes, empty = 0;
	unsigned long long shift;

	mask = lisuan_vm_entries_mask(ls_dev, t_params.level, t_params.mtype);
	shift = lisuan_vm_level_shift(ls_dev, t_params.level, t_params.mtype);
	num_ptes = lisuan_vm_num_entries(ls_dev, t_params.level, t_params.mtype);

	pdestart = (t_params.start >> shift) & mask;
	pdeend = (t_params.end >> shift) & mask;

	ASSERT(pdestart <= pdeend && pdeend < num_ptes);
	cursor->parent = params->parent;
	cursor->level = params->level;
	cursor->mtype = params->mtype;
	cursor->entry = params->parent;
	cursor->start = params->start;

	pdecur = pdestart;
	while (cursor->entry->entries && (pdecur <= pdeend)) {
		struct lisuan_vm_pt *pt = &cursor->entry->entries[pdecur];
		int mtype_changed = 0;
		cursor->level = t_params.level + 1;
		vm_info("%s,level:%d , ptype:%d, parastype:%d, va[%llx-%llx], the start[%d-%d],\n",__func__, cursor->level,pt->mtype, t_params.mtype,t_params.start,t_params.end, pdecur, pdeend);

		if (cursor->level == LS_VM_PD2 && pt->mtype != t_params.mtype) {
			t_params.mtype = pt->mtype;
			//mask = lisuan_vm_entries_mask(ls_dev, t_params.level, pt->mtype);
			//shift = lisuan_vm_level_shift(ls_dev, t_params.level, pt->mtype);
			num_ptes = lisuan_vm_num_entries(ls_dev, t_params.level, pt->mtype);
			//pdestart = (t_params.start >> shift) & mask;
			//pdeend = (t_params.end >> shift) & mask;
			//pdecur = pdestart;
			//continue;
		}
		if (cursor->level != LS_VM_PT) {
			struct lisuan_vm_iter_pd_params td_params = *params;
			td_params.parent = pt;
			td_params.level = cursor->level;
			td_params.mtype = t_params.mtype;
			td_params.start = (pdecur == pdestart ? params->start : ((params->start & ~((0x1ULL << shift) - 1)) + ((unsigned long long)(pdecur - pdestart) << shift)));
			td_params.end = (pdecur == pdeend ? params->end : (td_params.start | ((0x1ULL << shift) - 1)));

			lisuan_vm_free_pds(ls_dev, &td_params);
		}
		pdecur++;
	}

	if (cursor->entry->level != LS_VM_PD0 && num_ptes == find_first_bit(parent->bitmaps, num_ptes)) {
		lisuan_vm_free_pt(ls_file, cursor->entry);
	}
	gf_kvfree(cursor);
	return 0;

}
*/
int lisuan_vm_unmap_and_release_pt(struct lisuan_device *ls_dev, struct lisuan_vm_update_params *params)
{
	return lisuan_vm_bo_update_mapping(ls_dev, params);
}

static unsigned lisuan_vm_bo_size(struct lisuan_device *ls_dev, int level, int mtype)
{
	int size = lisuan_vm_num_entries(ls_dev, level, mtype) * GPU_PTE_SIZE;
	return ALIGN(size, GPU_PAGE_SIZE);
}

static void lisuan_vm_bo_params(struct lisuan_device *ls_dev, struct lisuan_vm_pt *pt, int level, int mtype)
{
	pt->buffer = gf_kvcalloc(1, sizeof(struct lisuan_gem_bo), GFP_KERNEL);
	pt->buffer->size = lisuan_vm_bo_size(ls_dev, level, mtype);
	pt->buffer->align = GPU_PAGE_SIZE;
	pt->buffer->domain = ls_dev->bufsel.none_bypass_pt_buffer_dom;
	pt->buffer->flag.byPass = 1;
	atomic_set(&pt->buffer->unpagable, 1);
	pt->buffer->priority = gem_priority_high;
	pt->buffer->flag.vram = (pt->buffer->domain <= dom_invisual_vram) ? 1 : 0;
	pt->buffer->flag.snoop = ((pt->buffer->domain == dom_snoop_system_pages) ? 1 : 0);

}

static int lisuan_vm_alloc_pts(struct lisuan_device *ls_dev, struct lisuan_vm *gfvm, struct lisuan_vm_pt *pt, struct lisuan_vm_pt_cursor *cursor)
{
	int ret = 0;
	struct lisuan_file *ls_file = container_of(gfvm, struct lisuan_file, vm);
	struct lisuan_vm_node *vm_node;
	struct lisuan_mman *mmgr = NULL;

	unsigned num_entries = lisuan_vm_num_entries(ls_dev, cursor->level, cursor->mtype);

	if (!pt->entries && !(cursor->level == LS_VM_PD2 || (cursor->level == LS_VM_PD1 && cursor->mtype == MEM_TYPE_4MB))) {
		pt->entries = gf_kvcalloc(num_entries, sizeof(struct lisuan_vm_pt), GFP_KERNEL);
		if (!pt->entries)
			return -ENOMEM;
	}
	if (!pt->bitmaps) {
		pt->bitmaps = bitmap_zalloc(num_entries, GFP_KERNEL);
	}
	if (!pt->buffer) {
		lisuan_vm_bo_params(ls_dev, pt, cursor->level, cursor->mtype);
		if(lisuan_gem_alloc_oneshot(ls_dev, NULL, pt->buffer, ls_dev->hwctl.cpu_update_ptes))
			goto fail;
	}
	pt->parent = cursor->parent;
	if(cursor->level == LS_VM_PD2)
		pt->mtype = cursor->mtype;

	pt->level = cursor->level;
	pt->update_shadow_buffer = 0;
	return 0;
fail:
	if (pt->buffer) {
		gf_error("%s: fail alloc pt from bypass\n", __func__);
		gf_kvfree(pt->buffer);
		pt->buffer = NULL;
	}
	return -1;
}

void lisuan_vm_init_vm(struct lisuan_file *ls_file)
{
	struct lisuan_device *ls_dev = ls_file->ls_dev;
	struct lisuan_vm *vm = &ls_file->vm;
	struct lisuan_vm_pt_cursor *cursor = (struct lisuan_vm_pt_cursor *)gf_kvcalloc(1, sizeof(struct lisuan_vm_pt_cursor), GFP_KERNEL);
	int i;

	vm->file  = ls_file;
	ls_dev->max_address_bits = 40;
	lisuan_init_mm_range2(&vm->mmgr, ls_dev->mmu_bypass_end, (1ULL << ls_dev->max_address_bits));

	for (i = 0; i < MEM_TYPE_MAX; i++) {
		ls_dev->pt_count[i][LS_VM_PD0] = 8;
		ls_dev->pt_count[i][LS_VM_PD1] = 10;
		ls_dev->pt_count[i][LS_VM_PD2] = (i == MEM_TYPE_4KB ? 10 : (i == MEM_TYPE_64KB ? 6 : 0));
	}

	vm->pt_lock = lisuan_init_os_mutex();

	vm->fence_context = dma_fence_context_alloc(2);
	INIT_LIST_HEAD(&vm->defer_free_pts);

	if (ls_dev->hwctl.cpu_update_ptes) {
		vm->update_func = &lisuan_vm_cpu_funcs;	//TODO: lisuan_vm_update_map_funcs gf_vm_cpu_funcs
	} else {
		vm->update_func = &lisuan_vm_gpu_funcs;
	}
	cursor->parent = NULL;
	cursor->level = LS_VM_PD0;
	cursor->mtype = MEM_TYPE_4KB;
	cursor->entry = &vm->root;
	lisuan_vm_alloc_pts(ls_dev, vm, &vm->root, cursor);
	gf_kvfree(cursor);

	vm->is_L0_dirty = 0xFF;
	vm->is_L1_dirty = 0xFF;
	vm->is_L2_dirty = 0xFF;

	vm->shadow = gf_calloc(SHADOW_IDB_SIZE);
}

void lisuan_vm_fini_vm(struct lisuan_file *ls_file)
{
	struct lisuan_device *ls_dev = ls_file->ls_dev;
	struct lisuan_vm *vm = &ls_file->vm;
	unsigned int i = 0;
/*
	struct lisuan_file *ls_file=vm->file;
	struct lisuan_vm_node *va_node;

	//firstly free the unmap gpuva only used for reserve_gpu_va not normally free
	list_for_each_entry(va_node, &vm->mmgr.list_head, vm_item){

		gf_info("%s, the va_node:%llx\n",__func__, va_node->offset);
		if(va_node->sparse_path){
			struct lisuan_vm_update_params params = { 0 };
	
			va_node->byPass = 0;
			va_node->ls_file = ls_file;
	
			params.vm = vm;
			params.node = va_node;
			params.unmap = 1;
			params.flag.byPass = va_node->byPass;
			params.bitop_func = __bitmap_clear;
			params.tbo = NULL; // update_pte need check
			params.mtype = MEM_TYPE_4KB;
			lisuan_vm_unmap_and_release_pt(ls_dev, &params);
			}
		va_node->free(va_node); 
	}
*/
	mutex_lock(&vm->pt_lock->lock);
	if(vm->last_update.fence && !dma_fence_is_signaled(vm->last_update.fence))
		dma_fence_wait(vm->last_update.fence, true);
	dma_fence_put(vm->last_update.fence);

	for(i = 0; i < ls_dev->rb_count; i++){
		if(vm->last_used[i].fence && !dma_fence_is_signaled(vm->last_used[i].fence))
			dma_fence_wait(vm->last_used[i].fence, true);
		dma_fence_put(vm->last_used[i].fence);
	}
	mutex_unlock(&vm->pt_lock->lock);

	lisuan_procid_vm_detach(ls_dev, vm);

	lisuan_vm_free_pt(vm->file, &vm->root);

	lisuan_fini_mm_range(&vm->mmgr);

	mutex_destroy(&vm->pt_lock->lock);
	gf_kvfree(vm->pt_lock);
	vm->pt_lock = NULL;
	vm->procid  = NULL;
	if(vm->shadow)
		gf_free(vm->shadow);
}

void lisuan_free_cf_node(struct lisuan_cf_node *cf_node)
{
	struct lisuan_mman *mmgr = cf_node->cf_mmgr;

	spin_lock(&mmgr->lock);
	drm_mm_remove_node(cf_node->cf_node);
	spin_unlock(&mmgr->lock);
	gf_kvfree(cf_node->cf_node);
	cf_node->cf_node=NULL;
}

int lisuan_alloc_cf_node(struct lisuan_device *ls_dev, struct lisuan_cf_node *cf_node)
{
	struct lisuan_cf_man *cf = &ls_dev->cf;
	struct drm_mm_node *mm_node = (struct drm_mm_node *)gf_kvcalloc(1, sizeof(struct drm_mm_node), GFP_KERNEL);
	unsigned int align_size = ALIGN_SIZE(cf_node->cf_size, (cf->cf_cell_size << E_COMPRESS_RATE));
	unsigned int cf_size = ALIGN_SIZE((align_size >> E_COMPRESS_RATE), (unsigned long long)cf->cf_cell_size);
	int ret = 0;

	spin_lock(&cf->mmgr.lock);
	ret = drm_mm_insert_node_in_range(cf->mmgr.mm, mm_node, cf_size, cf->cf_cell_size, 0, cf->mmgr.start_addr, cf->mmgr.end_addr, DRM_MM_INSERT_LOW);
	spin_unlock(&cf->mmgr.lock);

	if (ret)
		goto fail;

	cf_node->cf_size = mm_node->size;
	cf_node->cf_node = mm_node;
	cf_node->cf_mmgr = &cf->mmgr;
	cf_node->cf_start = mm_node->start - cf->mmgr.start_addr;
	cf_node->cell_index = cf_node->cf_start / cf->cf_cell_size;
	cf_node->cf_map = false;
	cf_node->free = lisuan_free_cf_node;

	return ret;
fail:
	if (mm_node)
		gf_kvfree(mm_node);
	return ret;
}

int lisuan_init_cf_pool(struct lisuan_device *ls_dev)
{
	struct lisuan_cf_man *cf = &ls_dev->cf;
	unsigned long long vram_size = ls_dev->vram_size;
	int ret = 0;

	//first init the cf size
	cf->buf = gf_kvcalloc(1, sizeof(struct lisuan_gem_bo), GFP_KERNEL);
	cf->buf->domain = ls_dev->bufsel.cf_pool_dom;
	cf->buf->flag.byPass = 1;
	cf->buf->flag.direction = 1;
	cf->cf_start_align = 0x800000;	// 8M align
	cf->buf->align = cf->cf_start_align;
	atomic_set(&cf->buf->unpagable, 1);
	cf->buf->priority = gem_priority_high;
	cf->buf->flag.vram = (cf->buf->domain <= dom_invisual_vram) ? 1 : 0;
	cf->buf->flag.snoop = ((cf->buf->domain == dom_snoop_system_pages) ? 1 : 0);

	// cf->cf_cell_size = 1 << (adapter_ptr->bits_of_compress_slot_size - 9)
	// cf->cf_slot_num = adapter_ptr->compress_heap.total_size
	switch (ls_dev->mhb_num)
	{
	case 1:
	case 2:
		cf->cf_total_size = 8 * 1024 * 1024;
		cf->cf_cell_size = (1<<7);
		cf->cf_slot_num = (1<<16);	
		break;
	case 3:
		cf->cf_total_size = 16 * 1024 * 1024;
		cf->cf_cell_size = (1<<8);
		cf->cf_slot_num = (1<<16);		
		break;
	case 4:
		cf->cf_total_size = 32 * 1024 * 1024;
		cf->cf_cell_size = (1<<8);
		cf->cf_slot_num = (1<<17);		
		break;
	case 5:
	case 6:
		cf->cf_total_size = 64 * 1024 * 1024;
		cf->cf_cell_size = (1<<8);
		cf->cf_slot_num = (1<<18);		
		break;
	default:
		ASSERT(0);
		break;
	}

	cf->buf->size = cf->cf_total_size;
	cf->buf->size += 0x100000;

	ret = lisuan_gem_alloc_oneshot(ls_dev, NULL, cf->buf, cf->buf->domain == dom_visual_vram);
	if (ret)
		return ret;

	lisuan_init_mm_range2(&cf->mmgr, cf->buf->vm_node->offset, (cf->buf->vm_node->offset + cf->buf->vm_node->size));

	cf->cf_cpu_virt_address = cf->buf->virt_addres;
	return ret;
}

int lisuan_fini_cf_pool(struct lisuan_device *ls_dev)
{
	struct lisuan_cf_man *cf = &ls_dev->cf;
	struct lisuan_mman *mmgr = &cf->mmgr;

	lisuan_gem_free_oneshot(ls_dev, cf->buf);
	lisuan_fini_mm_range(mmgr);

	return 0;
}

int lisuan_vm_init_bypass_pt(struct lisuan_device *ls_dev)
{
	int ret = 0, i, procidMAX = 16, procid;
	struct lisuan_bypass_pt *pt = &ls_dev->pt;
	//init the page table L0 & L1 
	unsigned int Pt0Pte = 0;
	unsigned int Pt1Pte = 0;
	//init the pt2 params
	unsigned long long pt2sgpts, pt2ptbs, pt2ptbs_gpu_pa;
	//init the pt1 params
	unsigned long long pt1pts, pt1ptbs, pt1ptbs_gpu_pa;


	pt2sgpts = (ls_dev->mmu_bypass_end - ls_dev->mmu_bypass_pcie_start) / GPU_PAGE_SIZE;

	pt1pts = PACK_1K(pt2sgpts);
	pt1ptbs = PACK_1K(pt1pts);

	pt2ptbs = pt1pts;

	pt->buf = gf_kvcalloc(1, sizeof(struct lisuan_gem_bo), GFP_KERNEL);
	pt->buf->align = GPU_HUGE_PAGE_SIZE;
	pt->buf->size = (pt1ptbs + pt2ptbs) * GPU_PAGE_SIZE;
	pt->buf->domain = ls_dev->bufsel.bypass_pt_buffer_dom;
	pt->buf->flag.byPass = 1;
	atomic_set(&pt->buf->unpagable, 1);
	pt->buf->priority = gem_priority_high;
	pt->buf->flag.vram = (pt->buf->domain <= dom_invisual_vram) ? 1 : 0;
	pt->buf->flag.snoop = ((pt->buf->domain == dom_snoop_system_pages) ? 1 : 0);

	mutex_init(&pt->ptb_lock.lock);
	spin_lock_init(&ls_dev->bypass_lock);

	ret = lisuan_gem_alloc_oneshot(ls_dev, NULL, pt->buf, 1);
	if (ret)
		return ret;

	gf_memset(pt->buf->virt_addres, 0, pt->buf->size);
	pt1ptbs_gpu_pa = pt->buf->vm_node->offset - ls_dev->mmu_bypass_start;
	pt2ptbs_gpu_pa = pt1ptbs_gpu_pa + pt1ptbs * GPU_PAGE_SIZE;

	pt->PTB1VA = (unsigned int *)pt->buf->virt_addres;
	pt->PTB2VA = (unsigned int *)(pt->buf->virt_addres + pt1ptbs * GPU_PAGE_SIZE);
	
	vm_info("pt1ptbs_gpu_pa:0x%llx, pt1ptbs:%d, cpuva: 0x%llx, pt2ptbs_gpu_pa:0x%llx, pt2ptbs:%d, cpuva: 0x%llx\n",
        pt1ptbs_gpu_pa, pt1ptbs, pt->PTB1VA,
        pt2ptbs_gpu_pa, pt2ptbs, pt->PTB2VA);

	//point level 1 table to level 2 table
	for (i = 0; i < pt2ptbs; i++) {
		gf_hw_init_tlb_level1_pte(&Pt1Pte,1,0,0,0,0,0,
			(pt2ptbs_gpu_pa >> GPU_PAGE_SHIFT) + i);
		*(pt->PTB1VA + i) = Pt1Pte;
	}
	ls_dev->is_bypass_update = 0xFF;


	//point level 0 table to level 1 table
	for (i = 0; i < pt1ptbs; i++) {
		gf_hw_init_tlb_level0_pte(&Pt0Pte,1,0,0,0,
		(pt1ptbs_gpu_pa >> GPU_PAGE_SHIFT) + i);

		gf_set_mmu_pid_0_entry(ls_dev->mmio_base, i, Pt0Pte);
	}
	gf_debug("global L0 base value:0x%x, 0x%x, 0x%x\n", (pt1ptbs_gpu_pa >> GPU_PAGE_SHIFT) + 0, (pt1ptbs_gpu_pa >> GPU_PAGE_SHIFT) + 1,
		(pt1ptbs_gpu_pa >> GPU_PAGE_SHIFT) + 2);

	gf_set_mmu_bypass_begin(ls_dev->mmio_base, ls_dev->mmu_bypass_start >> 20);
	gf_set_mmu_bypass_end(ls_dev->mmio_base, (ls_dev->mmu_bypass_end >> 20) - 1);

	gf_mmu_bypass_inv(ls_dev->mmio_base);

	if (ls_dev->is_virtfn)
		return 0;	//maybe add value check for vf later.

	/*
		hw always check vf0's base and size, will move this setting to fsgk_init_pa_space and remove following setting later
		fsgk_write_mhu_register_uint(adapter_ptr, Register_Mhb_Fb_Size_Start, reg_Mhb_Fb_Size.uint);
	*/
	gf_set_mhb_fb_vf(ls_dev->mmio_base, 0, ls_dev->vram_size / 0x100000);
	
	return 0;
}

void lisuan_vm_reinit_bypass_pt(struct lisuan_device *ls_dev)
{
	unsigned long long pt1pts, pt1ptbs,pt2sgpts,pt1ptbs_gpu_pa;
	int i = 0;	
	unsigned int Pt0Pte = { 0 };

	struct lisuan_vm_node *vm_node = container_of(ls_dev->pt.buf->va_list.next, struct lisuan_vm_node, node_item);
	
	pt2sgpts = (ls_dev->mmu_bypass_end - ls_dev->mmu_bypass_pcie_start) / GPU_PAGE_SIZE;

	pt1pts = PACK_1K(pt2sgpts);
	pt1ptbs = PACK_1K(pt1pts);

	pt1ptbs_gpu_pa = ls_dev->pt.buf->vm_node->offset - ls_dev->mmu_bypass_start;
	
	//point level 0 table to level 1 table
	for (i = 0; i < pt1ptbs; i++) {
		gf_hw_init_tlb_level0_pte(&Pt0Pte,1,0,0,0,(pt1ptbs_gpu_pa >> GPU_PAGE_SHIFT) + i);
		gf_set_mmu_pid_0_entry(ls_dev->mmio_base, i, Pt0Pte);
	}

	gf_debug("global L0 base value:0x%x, 0x%x, 0x%x\n", (pt1ptbs_gpu_pa >> GPU_PAGE_SHIFT) + 0, (pt1ptbs_gpu_pa >> GPU_PAGE_SHIFT) + 1,
		(pt1ptbs_gpu_pa >> GPU_PAGE_SHIFT) + 2);

	gf_set_mmu_bypass_begin(ls_dev->mmio_base, ls_dev->mmu_bypass_start >> 20);
	gf_set_mmu_bypass_end(ls_dev->mmio_base, (ls_dev->mmu_bypass_end >> 20) - 1);

	gf_mmu_bypass_inv(ls_dev->mmio_base);

	if (!ls_dev->is_virtfn)
	{
		gf_set_mhb_fb_vf(ls_dev->mmio_base, 0, ls_dev->vram_size / 0x100000);
	}
}

int lisuan_do_mem_verify(struct lisuan_device *ls_dev)
{
	int ret = 0, i = 0, test_size=1*1024;
	unsigned long long start_offset=1*1024*1024*1024;
	void *virt_addres = ioremap_wc(ls_dev->pdev_info.start_addr_vram + start_offset, test_size);
	gf_info("start mem test [0x%llx-0x%llx]\n", start_offset, start_offset+test_size);
	for (i = 0; i < test_size / sizeof(unsigned int); i++) {
		*((unsigned int *)virt_addres + i) = i;
		if (*((unsigned int *)virt_addres + i) == i) {
			gf_info("dst gpu va:0x%llx write fail, write:0x%x, read:0x%x\n",
				start_offset + i * sizeof(unsigned int), i, *((unsigned int *)virt_addres + i));
		}
	}
	
	gf_info("end mem test [0x%llx-0x%llx]\n", start_offset, start_offset+test_size);
	iounmap(virt_addres);

	return 0;
}

void lisuan_vm_fini_bypass_pt(struct lisuan_device *ls_dev)
{
	struct lisuan_bypass_pt *bPpt = &ls_dev->pt;

	lisuan_gem_free_oneshot(ls_dev, bPpt->buf);
	mutex_destroy(&bPpt->ptb_lock.lock);
}

static int lisuan_vm_alloc_pds(struct lisuan_device *ls_dev, struct lisuan_vm_iter_pd_params *params)
{
	struct lisuan_vm *vm = params->vm;
	struct lisuan_file *ls_file = vm->file;
	struct lisuan_vm_pt_cursor *cursor = (struct lisuan_vm_pt_cursor *)gf_kvcalloc(1, sizeof(struct lisuan_vm_pt_cursor), GFP_KERNEL);
	unsigned mask = lisuan_vm_entries_mask(ls_dev, params->level, params->mtype);
	unsigned num_ptes = lisuan_vm_num_entries(ls_dev, params->level, params->mtype);
	unsigned shift = lisuan_vm_level_shift(ls_dev, params->level, params->mtype);
	unsigned pdestart = (params->start >> shift) & mask;
	unsigned pdeend = (params->end >> shift) & mask;
	unsigned nptes = 0, mtype, pdecur = pdestart;
	int page_size;
	unsigned long long entry_end;
	struct lisuan_vm_node *vm_node=NULL, *temp=NULL;	//gpu_va_node
	int ret = 0;

	ASSERT(pdestart <= pdeend && pdeend < num_ptes);

	cursor->parent = params->parent;
	cursor->level = params->level + 1;//start iter nxt level.
	cursor->mtype = params->mtype;
	cursor->entry = params->parent;
	cursor->start = params->start;

	while (pdecur <= pdeend) {
		struct lisuan_vm_pt *pt = &params->parent->entries[pdecur];

		//alloc nxt pt and update to cur pde if necessary
		if (!pt->buffer) {
			if(lisuan_vm_alloc_pts(ls_dev, vm, pt, cursor))
				return -ENOMEM;

			vm->update_func->update_pde(ls_dev, params, pdecur, pt);
		}

		//iter for nxt's nxt pt
		if (cursor->level < LS_VM_PD2 && params->mtype != MEM_TYPE_4MB) {
			struct lisuan_vm_iter_pd_params iter_params = *params;

			iter_params.parent = pt;
			iter_params.level = cursor->level;
			iter_params.start = (pdecur == pdestart ? params->start : ((params->start & ~((0x1ULL << shift) - 1)) + ((unsigned long long)(pdecur - pdestart) << shift)));
			iter_params.end = (pdecur == pdeend ? params->end : (iter_params.start | ((0x1ULL << shift) - 1)));
			iter_params.mtype = cursor->mtype;
			iter_params.vm = params->vm;

			ret = lisuan_vm_alloc_pds(ls_dev, &iter_params);
			if(ret)
				return ret;
		}

		if(cursor->level == LS_VM_PD2 && cursor->mtype != pt->mtype)
		{
			//for pt is 4K, params expect 64k, will divide into 4k as normal at pte update stage
			//for pt is 64K, params expect 4k, allocate a new shadow 4k, and resolve already mapped
			//64k to 4k into shadow buf, and the shadow buf will replace into the page table.
			if(cursor->mtype == MEM_TYPE_4KB && pt->mtype == MEM_TYPE_64KB)
			{
				unsigned long long params_gpu_va_start, params_gpu_va_end, pt_shadow_gpu_0, pt_shadow_gpu_1024;	
				struct lisuan_vm_pt *pt_shadow = (struct lisuan_vm_pt *)gf_kvcalloc(1, sizeof(struct lisuan_vm_pt), GFP_KERNEL);

				unsigned mask_t = lisuan_vm_entries_mask(ls_dev, LS_VM_PD2, MEM_TYPE_4KB);
				unsigned shift_t = lisuan_vm_level_shift(ls_dev, LS_VM_PD2, MEM_TYPE_4KB);

				params_gpu_va_start = params->start + (pdecur-pdestart)*1024* GPU_PAGE_SIZE;
				params_gpu_va_end   = (pdecur == pdeend ? params->end : (params_gpu_va_start | ((0x1ULL << shift_t) - 1)));
				pt_shadow_gpu_0     = params_gpu_va_start - ((params_gpu_va_start >> shift_t) & mask_t)*GPU_PAGE_SIZE;
				pt_shadow_gpu_1024  = pt_shadow_gpu_0 + 1024 * GPU_PAGE_SIZE;

				//alloc the shadow 4k pt
				if(lisuan_vm_alloc_pts(ls_dev, vm, pt_shadow, cursor)){
					gf_kvfree(pt_shadow);
					return -ENOMEM;
				}

				list_add_tail(&pt->buffer->free_pt_buffer, &vm->free_pts);
				{
					if (pt->entries) {
						gf_kvfree(pt->entries);
						pt->entries = NULL;
					}
					if (pt->bitmaps) {
						bitmap_free(pt->bitmaps);
						pt->bitmaps=NULL;
					}
					pt->buffer = NULL;
				}

				pt_shadow->update_shadow_buffer = 1;
				pt->parent->entries[pdecur] = *pt_shadow;
				gf_kvfree(pt_shadow);

				vm_info("PD2: tgid %d, pt 64K to 4K, level parent-cur:%d-%d, pdecur %d, params va_range:0x%llx-0x%llx, pt va_range:0x%llx-0x%llx\n",
					vm->file->tgid, params->level, cursor->level, pdecur, params_gpu_va_start,params_gpu_va_end, pt_shadow_gpu_0, pt_shadow_gpu_1024);

				mutex_lock(&ls_file->va_list_lock->lock);
				list_for_each_entry_safe(vm_node, temp, &ls_file->va_64K_list, vm_item) 
				{
					if((vm_node->ls_file == ls_file) &&
						((vm_node->offset >= pt_shadow_gpu_0 && vm_node->offset < pt_shadow_gpu_1024) ||
						((vm_node->offset + vm_node->size -1) >= pt_shadow_gpu_0 && (vm_node->offset + vm_node->size -1) < pt_shadow_gpu_1024)))
					{
						//if we find the should remap vm_node, the vm_node should do the update_pte to shadow_buffer
						struct lisuan_vm_update_params remap_params = { 0 };
						unsigned long long remap_start, remap_end;
						struct lisuan_vm_pt_cursor cursor_t = {0};
						struct lisuan_gem_bo *gem= vm_node->gem;

						gf_assert(!vm_node->byPass);
						if(vm_node->offset >= pt_shadow_gpu_0)
						{
							remap_start = vm_node->offset;
							remap_end   = min(vm_node->offset + vm_node->size, pt_shadow_gpu_1024);
						}
						else
						{
							remap_start = pt_shadow_gpu_0;
							remap_end   = min(vm_node->offset + vm_node->size,pt_shadow_gpu_1024); 
						}
						remap_params.vm = params->vm;
						remap_params.job = params->job;
						remap_params.mtype = params->mtype;
						remap_params.bitop_func = __bitmap_set;
						remap_params.node = vm_node;
						remap_params.tbo = &gem->tbo;
						lisuan_buffer_config_copy(&(remap_params.flag), &gem->flag);

						vm_info("PD2: tgid %d, gem:0x%x(tgid:%d)va range:%llx-%llx, remap va range[0x%llx-0x%llx]\n",
							vm->file->tgid, gem->handle, gem->ls_file->tgid, vm_node->offset, vm_node->offset + vm_node->size,remap_start, remap_end);

						do {
							cursor_t.start = remap_start;
							cursor_t.mtype = remap_params.mtype;
							cursor_t.level = LS_VM_PD0;
							cursor_t.parent = NULL;
							cursor_t.entry = &vm->root;
							while (false != lisuan_vm_pt_descendant(ls_dev, &cursor_t)) ;
							entry_end = vm->update_func->update_pte(ls_dev, &remap_params, &cursor_t);
							remap_start = entry_end + 1;
						} while (remap_start < remap_end);
					}
				}
				mutex_unlock(&ls_file->va_list_lock->lock);
			}
		}
		pdecur++;
	}

	gf_kvfree(cursor);
	return ret;
}

int lisuan_vm_invalidate_pds(struct lisuan_device *ls_dev, struct lisuan_vm_iter_pd_params *params)
{
	struct lisuan_vm *vm = params->vm;
	struct gf_job *job = params->job;
	struct lisuan_vm_pt *parent = params->parent;
	struct lisuan_vm_pt_cursor *cursor;

	unsigned mtype = (params->level == LS_VM_PD2) ? params->parent->mtype:params->mtype;
	unsigned mask = lisuan_vm_entries_mask(ls_dev, params->level, mtype);
	unsigned shift = lisuan_vm_level_shift(ls_dev, params->level, mtype);
	unsigned num_ptes = lisuan_vm_num_entries(ls_dev, params->level, mtype);

	unsigned pdestart = (params->start >> shift) & mask;
	unsigned pdeend = (params->end >> shift) & mask;
	unsigned npde = pdeend - pdestart + 1;
	unsigned idx, empty = 0, pdecur = pdestart, mtype_changed=0;

	ASSERT(pdestart <= pdeend && pdeend < num_ptes);

	while (params->parent->entries && (pdecur <= pdeend)) {
		struct lisuan_vm_pt *pt = &params->parent->entries[pdecur];

		if(!pt->buffer)
			goto next;

		if (params->level < LS_VM_PD2) {
			struct lisuan_vm_iter_pd_params td_params = *params;
			td_params.parent = pt;
			td_params.level = params->level+1;
			td_params.start = (pdecur == pdestart ? params->start : ((params->start & ~((0x1ULL << shift) - 1)) + ((unsigned long long)(pdecur - pdestart) << shift)));
			td_params.end = (pdecur == pdeend ? params->end : (td_params.start | ((0x1ULL << shift) - 1)));

			//return 1 means child entry is all invalidate
			if (lisuan_vm_invalidate_pds(ls_dev, &td_params)) {	
				//next level is empty, we update cur pde inv with any mtype is ok.
				vm_info("PD%d: tgid %d, pdecur %d, PD:%d is empty, last inv va_range:0x%llx-0x%llx\n",
				params->level, vm->file->tgid, pdecur, td_params.level, td_params.start, td_params.end);

				vm->update_func->update_pde(ls_dev, params, pdecur, pt);
				//params->bitop_func(parent->bitmaps, pdecur, 1);

				list_add_tail(&pt->buffer->free_pt_buffer, &vm->free_pts);
				{
					if (pt->entries) {
						gf_kvfree(pt->entries);
						pt->entries = NULL;
					}
					if (pt->bitmaps) {
						bitmap_free(pt->bitmaps);
						pt->bitmaps=NULL;
					}
					pt->buffer = NULL;
				}
			}
		}
next:
		pdecur++;
	}

	if (num_ptes == find_first_bit(parent->bitmaps, num_ptes)) {
		empty = 1;
	}
	return empty;
}

static void lisuan_vm_update_shadow_pd(struct lisuan_device *ls_dev, struct lisuan_vm_iter_pd_params *params)
{
	struct lisuan_vm *vm = params->vm;
	struct lisuan_file *ls_file = vm->file;
	struct lisuan_vm_pt_cursor *cursor = (struct lisuan_vm_pt_cursor *)gf_kvcalloc(1, sizeof(struct lisuan_vm_pt_cursor), GFP_KERNEL);

	unsigned mask = lisuan_vm_entries_mask(ls_dev, params->level, params->mtype);
	unsigned num_ptes = lisuan_vm_num_entries(ls_dev, params->level, params->mtype);
	unsigned shift = lisuan_vm_level_shift(ls_dev, params->level, params->mtype);

	unsigned pdestart = (params->start >> shift) & mask;
	unsigned pdeend = (params->end >> shift) & mask;
	unsigned nptes = 0, mtype, pdecur = pdestart;

	struct lisuan_vm_node *vm_node;
	int ret = 0;
	
	ASSERT(pdestart <= pdeend && pdeend < num_ptes);
	
	cursor->parent = params->parent;
	cursor->level = params->level + 1;
	cursor->mtype = params->mtype;
	cursor->entry = params->parent;
	cursor->start = params->start;
	while (pdecur <= pdeend) {		
		struct lisuan_vm_pt *pt = &cursor->entry->entries[pdecur];
		if (cursor->level < LS_VM_PD2 ) {
			struct lisuan_vm_iter_pd_params td_params = *params;

			td_params.parent = pt;
			td_params.level = cursor->level;
			td_params.start = (pdecur == pdestart ? params->start : ((params->start & ~((0x1ULL << shift) - 1)) + ((unsigned long long)(pdecur - pdestart) << shift)));
			td_params.end = (pdecur == pdeend ? params->end : (td_params.start | ((0x1ULL << shift) - 1)));
			td_params.mtype = cursor->mtype;
			td_params.vm = params->vm;

			lisuan_vm_update_shadow_pd(ls_dev, &td_params);
		}
		if(cursor->level == LS_VM_PD2)
		{
			if(pt->update_shadow_buffer){

				vm_info("Update shadow PD%d: tgid %d, pdecur:%d mtype:%d, level:%d, va range:0x%llx-0x%llx\n",
					cursor->level, vm->file->tgid, pdecur, params->mtype, params->level, params->start, params->end);

				vm->update_func->update_pde(ls_dev, params, pdecur, pt);
				//params->bitop_func(params->parent->bitmaps, pdecur, 1);
				pt->update_shadow_buffer = 0;
			}
		}
		pdecur++;
	}
	gf_kvfree(cursor);
}
static int lisuan_vm_update_ptes(struct lisuan_device *ls_dev, struct lisuan_vm_update_params *params)
{
	struct lisuan_vm *vm = params->vm;
	struct lisuan_vm_pt_cursor cursor;
	unsigned long long entry_end;
	int ret = 0;
	struct lisuan_vm_iter_pd_params ipp = { 0 };
	struct lisuan_gem_bo *pt_buffer=NULL, *temp=NULL;

	ipp.vm = params->vm;
	ipp.job = params->job;
	ipp.start = params->node->offset;
	ipp.end = params->node->offset + params->node->size - 1;
	ipp.mtype = params->mtype;
	ipp.level = LS_VM_PD0;
	ipp.parent = &vm->root;
	ipp.bitop_func = params->bitop_func;

	INIT_LIST_HEAD(&vm->free_pts);

	if (!params->unmap) {
		ret = lisuan_vm_alloc_pds(ls_dev, &ipp);
		if(ret)
			return ret;
	}

	//Looks good now, update the va_node into the pte
	do {
		cursor.start = ipp.start;
		cursor.mtype = params->mtype;
		cursor.level = LS_VM_PD0;
		cursor.parent = NULL;
		cursor.entry = &vm->root;

		while (false != lisuan_vm_pt_descendant(ls_dev, &cursor)) ;

		entry_end = vm->update_func->update_pte(ls_dev, params, &cursor);
		ipp.start = entry_end + 1;
	} while (ipp.start < ipp.end);

	if (params->unmap) {
		ipp.start = params->node->offset;
		lisuan_vm_invalidate_pds(ls_dev, &ipp);
	}

	//handle pt that needs free
	//1. from shadow map
	//2. from unmap 
	if (!list_empty(&vm->free_pts))
	{
		//update shadow pd
		if(!params->unmap){
			ipp.start = params->node->offset;
			ipp.mtype = MEM_TYPE_4KB;
			lisuan_vm_update_shadow_pd(ls_dev, &ipp);
		}

		//list_for_each_entry_safe(pt_buffer, temp, &vm->free_pts, free_pt_buffer) 
		{
			//for gpu upload, need free when this job finish.
			if(!ls_dev->hwctl.cpu_update_ptes)
			{
				list_splice(&vm->free_pts, &params->job->pt_buf_list);
			}
			//for cpu update, add to defer free list
			else
			{
				list_splice(&vm->free_pts, &vm->defer_free_pts);
			}
		}

		INIT_LIST_HEAD(&vm->free_pts);
	}

	return 0;
}

int lisuan_vm_bo_update_mapping(struct lisuan_device *ls_dev, struct lisuan_vm_update_params *params)
{
	struct lisuan_vm *vm = params->vm;
	int ret = 0;

	mutex_lock(&vm->pt_lock->lock);

	ret = vm->update_func->prepare(ls_dev, params);
	if (ret)
		goto exit;
	ret = lisuan_vm_update_ptes(ls_dev, params);
	if (ret)
		goto exit;
	ret = vm->update_func->commit(ls_dev, params);

exit:
	mutex_unlock(&vm->pt_lock->lock);

	return ret;
}

void lisuan_free_kbo(struct lisuan_device *ls_device, struct lisuan_kbo_mman *kbo)
{
	struct lisuan_mman *mmgr = &kbo->mman;

	lisuan_gem_free_oneshot(ls_device, kbo->bufObj);
	lisuan_fini_mm_range(mmgr);
}

int lisuan_reserve_kbo(struct lisuan_device *ls_device, struct lisuan_kbo_mman *kbo)
{
	int ret = 0;
	unsigned long long start_address, end_address;
	struct lisuan_vm_node *vm_node;

	ret = lisuan_gem_alloc_oneshot(ls_device, NULL, kbo->bufObj, 1);
	if (ret)
		return ret;
	vm_node = container_of(kbo->bufObj->va_list.next, struct lisuan_vm_node, node_item);

	start_address = vm_node->offset;
	end_address = start_address + kbo->bufObj->size;

	lisuan_init_mm_range2(&kbo->mman, start_address, end_address);
	kbo->free = lisuan_free_kbo;
	return ret;
}

int lisuan_alloc_kbo_node(struct lisuan_kbo_mman *kbo, struct lisuan_kbo_node *kbo_node)
{
	kbo_node->mm_node = lisuan_vram_mgr_new(&kbo->mman, kbo_node->size, kbo->alignment, 0);
	if (!kbo_node->mm_node){
		//gf_info("kbo node allo fail, need size:0x%x, pool align:0x%x\n", kbo_node->size, kbo->alignment);
		return -1;
	}

	kbo_node->mman = &kbo->mman;
	kbo_node->start = kbo_node->mm_node->start;
	kbo_node->cpu_virt_addr = kbo->bufObj->virt_addres + kbo_node->mm_node->start - kbo->mman.start_addr;
	kbo_node->pCmd = (unsigned int *)kbo_node->cpu_virt_addr;
	kbo_node->free = lisuan_free_kbo_node;

	return 0;

}

void lisuan_free_kbo_node(struct lisuan_kbo_node *kbo_node)
{
	struct lisuan_mman *mman = kbo_node->mman;

	spin_lock(&mman->lock);
	drm_mm_remove_node(kbo_node->mm_node);
	spin_unlock(&mman->lock);
	gf_kvfree(kbo_node->mm_node);
}
