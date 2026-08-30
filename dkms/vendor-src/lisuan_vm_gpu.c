/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#include "lisuan_device.h"
#include "lisuan_vm.h"
#include "gf_job.h"
#include "lisuan_sync.h"
#include "lisuan_gem.h"
#include "lisuan_ring.h"
#include "gf_ringbuffer.h"
#include "lisuan_scheduler.h"
#include "gf_kmd_event.h"
#include "gf_trans.h"
#include "gf_hw_misc.h"

static unsigned long cal_vm_gpu_update_pte_cmd_size(unsigned int gem_size)
{
    unsigned long l2_cnt = gem_size / 4096 + 2 * 1024;
    unsigned long l1_cnt = l2_cnt / 1024 + 2;
    unsigned long l0_cnt = 6;

    unsigned long total = l2_cnt * 4 + (l2_cnt / 1024) * 32 * 4 + (l0_cnt + l1_cnt) * (32 + 1) * 4;

    return total;
}

static int lisuan_update_truncate_idb_info(struct gf_job *job, unsigned int * pCmd, unsigned int * pCmd_base)
{
	unsigned int global_bytes_offset = (pCmd_base - job->p_cmd_base) * sizeof(unsigned int);
	unsigned int offset_bytes_initial = (pCmd_base - job->p_cmd_lastet_truncation) * sizeof(unsigned int);
	unsigned int offset_bytes_final = (pCmd - job->p_cmd_lastet_truncation) * sizeof(unsigned int);

	if (offset_bytes_initial < RING_IDB_BUFFER_SIZE_MAX && offset_bytes_final >= RING_IDB_BUFFER_SIZE_MAX)
	{
		job->truncation_point[job->truncation_point_count++] = global_bytes_offset;
		job->p_cmd_lastet_truncation = pCmd_base;
	}
	ASSERT(job->truncation_point_count < 64);

	return 0;
}
static int lisuan_vm_gpu_prepare(struct lisuan_device *ls_dev, struct lisuan_vm_update_params *params)
{
	struct lisuan_vm *vm = params->vm;
	struct gf_job *gf_job = (struct gf_job *)gf_kvcalloc(1, sizeof(struct gf_job), GFP_KERNEL);
	struct lisuan_ctx *context = ls_dev->vm_ctx;
	struct lisuan_gem_bo *gem = params->tbo ? container_of(params->tbo, struct lisuan_gem_bo, tbo) : 0;
	struct lisuan_gem_bo *bufObj = NULL;
	unsigned long cmd_size = 0;
	unsigned long gem_size = gem ? gem->size : params->node->size;
	int ret = 0;

	gf_job->context = context;

	INIT_LIST_HEAD(&gf_job->sync);
	INIT_LIST_HEAD(&gf_job->pt_buf_list);

	ret = lisuan_sched_job_init(&gf_job->base, &gf_job->context->entity, (void *)0ul);
	if(ret)
		goto exit;

	cmd_size = cal_vm_gpu_update_pte_cmd_size(gem_size);

	if(cmd_size < SHADOW_IDB_SIZE)
	{
		gf_job->p_cmd = 
		gf_job->p_cmd_base =
		gf_job->p_cmd_lastet_truncation = vm->shadow;
	}
	else
	{
		bufObj = lisuan_alloc_idb_buffer(ls_dev, cmd_size);
		if (!bufObj)
		{
			return ret;
		}

		gf_job->p_cmd = 
		gf_job->p_cmd_base =
		gf_job->p_cmd_lastet_truncation = bufObj->virt_addres; 

		list_add_tail(&bufObj->free_pt_buffer, &gf_job->pt_buf_list);
	}
	gf_job->truncation_point[gf_job->truncation_point_count++] = 0;
	params->job = gf_job;
exit:
	return ret;
}

static int lisuan_vm_gpu_update_pde(struct lisuan_device *ls_dev, struct lisuan_vm_iter_pd_params *params, unsigned pteoffset, struct lisuan_vm_pt *pt)
{
	struct lisuan_gem_bo *gbo = pt->buffer;
	struct gf_job *job = params->job;
	unsigned int *pCmd = job->p_cmd;
	unsigned int *pCmd_base = job->p_cmd;
	unsigned num_entries = lisuan_vm_num_entries(ls_dev, params->level, params->mtype);
	unsigned mask = lisuan_vm_entries_mask(ls_dev, params->level, params->mtype);
	unsigned long long shift = lisuan_vm_level_shift(ls_dev, params->level, params->mtype);
	struct lisuan_ctx *vm_ctx = ls_dev->vm_ctx;
	struct lisuan_ring *ring = ls_dev->sched[vm_ctx->rb_index];
	struct lisuan_vm *vm = params->vm;
	struct lisuan_vm_pt *parent = params->parent;
	struct lisuan_vm_node *vm_node = container_of(parent->buffer->va_list.next, struct lisuan_vm_node, node_item);

	unsigned long long pa = 0ull, dst_va = vm_node->offset + pteoffset*sizeof(unsigned int);
	int page_size = (params->mtype == MEM_TYPE_4MB ? GPU_BLOCK_SIZE : (params->mtype == MEM_TYPE_64KB ? GPU_HUGE_PAGE_SIZE : GPU_PAGE_SIZE));

	if (pteoffset >= num_entries)
		return -1;
	if (params->unmap && ls_dev->hwctl.skip_invalidate){
		
		vm_info("pde-gpu: tgid:%d, skip unmap the pde handle:0x%x\n", vm->file->tgid, gbo->handle);
		return 0;
	}
	if (params->level == LS_VM_PD1 && params->mtype == MEM_TYPE_4MB)
		return 0;


	if(ring->rb_index == RB_INDEX_GFX){
		pCmd += gf_hw_get_mem_op_write_cmds(false, dst_va, 1 * sizeof(unsigned int), ls_dev->hwctl.enable_gl2, 0, pCmd);
	}else if(ring->rb_index == RB_INDEX_GCE){
		struct gf_gce_ringbuffer *gce_rb = (struct gf_gce_ringbuffer *)ring->chip_priv;
		unsigned int offset = (dst_va & 0xFF)/4;//gce start addr need 256byte align

		gf_hw_get_gce_host_update_pde_cmds(&pCmd, offset, dst_va, gce_rb->gce_host_upload);

	}

	lisuan_get_phys_addr(ls_dev, &gbo->tbo, 0, page_size, &pa, gbo->flag.vram);

	switch (params->level) {
	case LS_VM_PD0:
		{
			unsigned int level0_pte = 0;
			gf_hw_init_tlb_level0_pte(&level0_pte, !params->unmap, 0,!gbo->flag.vram, gbo->flag.snoop, pa >> GPU_PAGE_SHIFT);

			*pCmd++ = level0_pte;

			vm_info("pde-gpu: tgid:%d, set PD0(pa:0x%llx) offset:%d to %08x(pa:0x%llx), unmap:%d\n", 
				vm->file->tgid, (vm_node->offset - ls_dev->mmu_bypass_start), pteoffset, level0_pte, pa, params->unmap);

			vm->is_L0_dirty = 0xFF;
			break;
		}
	case LS_VM_PD1:
		{
			unsigned int level1_pte = 0;
			gf_hw_init_tlb_level1_pte(&level1_pte,!params->unmap,0,!gbo->flag.vram,gbo->flag.snoop,params->mtype == MEM_TYPE_4MB,params->mtype == MEM_TYPE_64KB,
				(params->mtype == MEM_TYPE_4MB) ? (pa >> GPU_BLOCK_SHIFT) : (pa >> GPU_PAGE_SHIFT));

			*pCmd++ = level1_pte;

			vm_info("pde-gpu: tgid:%d ,set PD1(pa:0x%llx) offset:%d to %08x(pa:0x%llx), unmap:%d\n",
				vm->file->tgid, (vm_node->offset - ls_dev->mmu_bypass_start), pteoffset, level1_pte, pa, params->unmap);

			vm->is_L1_dirty = 0xFF;
			break;
		}

	}

	if(ring->rb_index == RB_INDEX_GCE){
		gf_hw_get_cpia_gce_end_cmds(true, pCmd++);
	}

	lisuan_update_truncate_idb_info(job, pCmd, pCmd_base);

	params->bitop_func(params->parent->bitmaps, pteoffset, 1);

	job->p_cmd = pCmd;
	return 0;
}

static unsigned long long lisuan_vm_gpu_update_pte(struct lisuan_device *ls_dev, struct lisuan_vm_update_params *params, struct lisuan_vm_pt_cursor *cursor)
{
	unsigned mtype=0, pdestart, nptes;
	unsigned long long shift, end, entry_end;
	unsigned mask, ret = 0, i, j, page_offset, page_shift;
	struct gf_job *sched_job = params->job;
	unsigned int *pCmd = sched_job->p_cmd;
	unsigned int *pCmd_base = sched_job->p_cmd;
	unsigned page_size;
	unsigned long long pa = 0ull, dst_va;
	int page_num = 1;
	struct lisuan_vm * vm = params->vm;
	struct lisuan_ctx *vm_ctx = ls_dev->vm_ctx;
	struct lisuan_ring *ring = ls_dev->sched[vm_ctx->rb_index];
	struct lisuan_vm_pt *pt = cursor->entry;
	struct lisuan_vm_node *vm_node;
	unsigned int dump_one = 1;

	end = params->node->offset + params->node->size - 1;

	if (cursor->level == LS_VM_PD2) {
		mtype = cursor->entry->mtype;//do as pt type, even param is 64K.
	} else if (params->mtype == MEM_TYPE_4MB) {//4M pd1 has remove the mtype now.
		ASSERT(cursor->level == LS_VM_PD1);
		mtype = params->mtype;
	}

	page_size= (mtype == MEM_TYPE_4MB ? GPU_BLOCK_SIZE : (mtype == MEM_TYPE_64KB ? GPU_HUGE_PAGE_SIZE : GPU_PAGE_SIZE));
	mask = lisuan_vm_entries_mask(ls_dev, cursor->level, mtype);
	shift = lisuan_vm_level_shift(ls_dev, cursor->level, mtype);

	entry_end = ((uint64_t) mask + 1) << shift;
	entry_end += (cursor->start & ~(entry_end - 1)) - 1;
	entry_end = min(entry_end, end);

	pdestart = (cursor->start >> shift) & mask;
	nptes = ((entry_end >> shift) & mask) - pdestart + 1;

	if(pt->buffer){
		vm_node = container_of(pt->buffer->va_list.next, struct lisuan_vm_node, node_item);
	}
	else{
		vm_info("pte-gpu: update pte but pt buffer invalid tgid:%d, level:%d, mtype:%d\n", vm->file->tgid, cursor->level, mtype);
		return entry_end;
	}

	dst_va = vm_node->offset + pdestart*sizeof(unsigned int);

	if (params->unmap && ls_dev->hwctl.skip_invalidate){	
		vm_info("pte-gpu: skip unmap tgid:%d, level:%d, mtype:%d, ptb VA-PA:0x%llx-0x%llx, pteoffset:%d, npte:%d, va range[0x%llx-0x%llx], unmap:%d, resolve 64k to 4k:%d\n", 
		vm->file->tgid, cursor->level, mtype, vm_node->offset, (vm_node->offset - ls_dev->mmu_bypass_start), pdestart, nptes, cursor->start, entry_end, params->unmap, cursor->entry->mtype != params->mtype);
		return entry_end;
	}

	vm_info("pte-gpu: tgid:%d, level:%d, mtype:%d, ptb VA-PA:0x%llx-0x%llx, dst va:0x%llx pteoffset:%d, npte:%d, va range[0x%llx-0x%llx], unmap:%d, resolve 64k to 4k:%d\n", 
		vm->file->tgid, cursor->level, mtype, vm_node->offset, (vm_node->offset - ls_dev->mmu_bypass_start), dst_va, pdestart, nptes, cursor->start, entry_end, params->unmap, cursor->entry->mtype != params->mtype);

	page_shift = (mtype == MEM_TYPE_4MB ? GPU_BLOCK_SHIFT : (mtype == MEM_TYPE_64KB ? GPU_HUGE_PAGE_SHIFT : GPU_PAGE_SHIFT));

	if (cursor->entry->mtype != params->mtype)
	{
		if(cursor->entry->mtype != MEM_TYPE_4KB)
			gf_info("level:%d, mtype:%d-%d-%d\n", cursor->level, cursor->entry->mtype, params->mtype, mtype);
		ASSERT(cursor->entry->mtype == MEM_TYPE_4KB);
	}

	if(ring->rb_index == RB_INDEX_GFX){
		pCmd += gf_hw_get_mem_op_write_cmds(false, dst_va, nptes * sizeof(unsigned int), ls_dev->hwctl.enable_gl2, 0, pCmd);
	}else if(ring->rb_index == RB_INDEX_GCE){
		struct gf_gce_ringbuffer *gce_rb = (struct gf_gce_ringbuffer *)ring->chip_priv;
		unsigned int offset = (dst_va & 0xFF)/4;//gce start addr need 256byte align

		gf_hw_get_gce_host_update_pte_cmds(&pCmd, nptes, offset, dst_va, gce_rb->gce_host_upload);
	}

	if(params->sparse_path)
	{
		unsigned int level2_pte = 0;

		if(!params->sparse_clear_op)
		{
			gf_hw_init_tlb_level2_pte(&level2_pte,!params->unmap,0,!params->flag.vram,params->flag.snoop,
				0);
		}

		page_offset = (cursor->start - params->node->offset + params->pa_offset) / page_size;

		for (j = 0; j < nptes; j++) {
			if(params->tbo){
				lisuan_get_phys_addr(ls_dev, params->tbo, page_offset  + j, page_size, &pa, params->flag.vram);
				pa >>= page_shift;
				if(params->flag.vram)
					pa = (pa << 2) & 0x7FFFFFC;
			}
			gf_hw_init_tlb_level2_pte(&level2_pte,!params->unmap,0,!params->flag.vram,params->flag.snoop,
				pa);
			*pCmd++ = level2_pte;
			if(dump_one) {
				vm_info("pte-gpu-sparse: tgid:%d, set offset(%d):%08x, shifted PA:0x%x\n", 
					vm->file->tgid, pdestart + j, level2_pte, pa);
				dump_one = 0;
			}
		}
		vm->is_L2_dirty = 0xFF;

		goto exit;
	}

	switch (cursor->level) {
	case LS_VM_PD1:
		{
			//4M case
			unsigned int level1_pte = 0;
			page_offset = (cursor->start - params->node->offset) / GPU_BLOCK_SIZE;

			for (i = 0; i < nptes; i++) {
				if(params->tbo){
					lisuan_get_phys_addr(ls_dev, params->tbo, page_offset + i, page_size, &pa, params->flag.vram);
					gf_hw_init_tlb_level1_pte(&level1_pte,!params->unmap,0,!params->flag.vram,params->flag.snoop,0,0,
						pa >> GPU_BLOCK_SHIFT);
				}
				*pCmd++ = level1_pte;
				if(dump_one) {
					vm_info("pte-gpu: tgid:%d, block pte, level:%d, set offset(%d):%08x, shifted PA:0x%x\n", 
						vm->file->tgid, cursor->level, pdestart + i, level1_pte, pa);
					dump_one=0;
				}
				
			}
			vm->is_L1_dirty = 0xFF;
			break;
		}

	case LS_VM_PD2:
		{
			unsigned int level2_pte = 0;

			page_offset = (cursor->start - params->node->offset) / page_size;
			for (i = 0; i < nptes; i++) {
				if(params->tbo){
					lisuan_get_phys_addr(ls_dev, params->tbo, page_offset + i, page_size, &pa, params->flag.vram);
					pa >>= page_shift;
				
					if(params->flag.vram)
						pa = (pa << 2) & 0x7FFFFFC;
					else
						ASSERT(!(pa & ~0xFFFFFFFULL));
						gf_hw_init_tlb_level2_pte(&level2_pte,!params->unmap,0,!params->flag.vram,params->flag.snoop,
												pa);
				}		
				*pCmd++ = level2_pte;
				if(dump_one) {
					vm_info("pte-gpu: tgid:%d, level:%d, set offset(%d):%08x, shifted PA:0x%x\n", 
						vm->file->tgid, cursor->level, pdestart + i, level2_pte, pa);
					dump_one=0;
				}
			}
			vm->is_L2_dirty = 0xFF;
			break;
		}
	}


exit:
	if(ring->rb_index == RB_INDEX_GCE){
		gf_hw_get_cpia_gce_end_cmds(true, pCmd++);
	}

	lisuan_update_truncate_idb_info(sched_job, pCmd, pCmd_base);

	sched_job->p_cmd = pCmd;
	params->bitop_func(pt->bitmaps, pdestart, nptes);

	return entry_end;

}

static int lisuan_vm_gpu_commit(struct lisuan_device *ls_dev, struct lisuan_vm_update_params *params)
{
	struct lisuan_vm *vm = params->vm;
	struct gf_job *gf_job = params->job;
	struct lisuan_sched_fence *s_fence;
	struct dma_fence *fence;
	struct lisuan_scheduler *sched = gf_job->base.sched;
	int ret = 0, i = 0, size = 0, dma_idx = 0;	
	struct lisuan_ring *ring;
	struct gf_ring_buffer *ringbuffer;
	struct lisuan_gem_bo * gem = params->tbo ? container_of(params->tbo, struct lisuan_gem_bo, tbo) : NULL;
	struct lisuan_gem_bo *pt_buf;
	unsigned long long dma_va = 0, dma_va_cur = 0;
	unsigned int truncation_point_count = 0;
	
	if((char *)gf_job->p_cmd == (char *)gf_job->p_cmd_base)
		goto exit;

	gf_send_ifence_wait(ls_dev, &gf_job->p_cmd, gf_job->context->rb_index);

	size = (int)((char *)gf_job->p_cmd - (char *)gf_job->p_cmd_base);

	if(gem)
	{
		if(params->flag.vram)
			job_info("job:%d from tgid-pid:%d-%d, gem handle:%x, vm owner:%d-%d, gpuva range:%llx-%llx, gpa range:%llx-%llx, free:%d", 
			gf_job->base.id, gf_job->tgid, gf_job->pid, gem->handle, vm->file->tgid, vm->file->pid, 
			params->node->offset, params->node->offset+params->node->size, 
			gem->vm_node->offset - gem->mmu_bypass_start, gem->vm_node->offset + gem->vm_node->size - gem->mmu_bypass_start,
			params->unmap);
		else
			job_info("job:%d from tgid-pid:%d-%d, gem handle:%x, vm owner:%d-%d, gpuva range:%llx-%llx, free:%d", 
			gf_job->base.id, gf_job->tgid, gf_job->pid, gem->handle, vm->file->tgid, vm->file->pid, 
			params->node->offset, params->node->offset+params->node->size, params->unmap);
	}
	
	if(gf_job->p_cmd_base != vm->shadow)
	{
		//Handle GPU update page table cases exceeding the SHADOW_IDB_SIZE limit
		list_for_each_entry(pt_buf, &gf_job->pt_buf_list, free_pt_buffer) 
		{
			if(pt_buf->virt_addres == gf_job->p_cmd_base)
			{
#if DRM_VERSION_CODE >= KERNEL_VERSION(5,15,0)
				dma_va = pt_buf->tbo.resource->start; 
#else
				dma_va = pt_buf->tbo.mem.start;
#endif
			}
		}
	}
	else
	{
		gf_job->kcommand = (struct lisuan_kbo_node *)gf_kvcalloc(1, sizeof(struct lisuan_kbo_node), GFP_KERNEL);
		gf_job->kcommand->size = size;

		ret = lisuan_alloc_kbo_node(&ls_dev->kcommand, gf_job->kcommand);
		if(!ret)
		{
			gf_memcpy((void*)gf_job->kcommand->pCmd, (char *)vm->shadow, size);
			dma_va = gf_job->kcommand->start;
		}
		else
		{
			//cmd pool full
			if(gf_job->kcommand)
			{
				gf_kvfree(gf_job->kcommand);
				gf_job->kcommand = NULL;
			}
retry:
			pt_buf = lisuan_alloc_idb_buffer(ls_dev, size);
			if (!pt_buf)
				goto retry;
			list_add_tail(&pt_buf->free_pt_buffer, &gf_job->pt_buf_list);

#if DRM_VERSION_CODE >= KERNEL_VERSION(5,15,0)
			dma_va = pt_buf->tbo.resource->start; 
#else
			dma_va = pt_buf->tbo.mem.start;
#endif

			gf_memcpy((void*)pt_buf->virt_addres, (char *)vm->shadow, size);
		}
	}
	gf_job->vm = NULL;
	gf_job->procid = 0;
	s_fence = gf_job->base.s_fence;
	ring  = container_of(sched, struct lisuan_ring, base);
	ringbuffer= (struct gf_ring_buffer *)ring->chip_priv;
	gf_job->uf_sequence = atomic_read(&gf_job->context->entity.fence_seq);

	gf_job->dma_count =  gf_job->truncation_point_count;
	if(ls_dev->is_virtfn && ring->rb_index < RB_INDEX_VEM)
	{
		gf_job->dma_count += 2;
	}

	gf_job->dma_info = (struct dma_info_s *)gf_kvcalloc(gf_job->dma_count, sizeof(struct dma_info_s), GFP_KERNEL);

	if(ls_dev->is_virtfn && ring->rb_index < RB_INDEX_VEM){
		struct lisuan_vm_node *vm_node = container_of(ringbuffer->krestore->va_list.next, struct lisuan_vm_node, node_item);
		gf_job->dma_info[dma_idx].dma_gpu_va = vm_node->offset;
		gf_job->dma_info[dma_idx].dma_size_in_byte = ringbuffer->krestore->cmd_size;
		gf_job->dma_info[dma_idx++].idb_type = 2;//IDB_IDB_TYPE_RESTORE;
	}

	dma_va_cur = dma_va;
	for(truncation_point_count = 0; truncation_point_count < gf_job->truncation_point_count -1; truncation_point_count ++)
	{
		unsigned dma_size_in_byte = gf_job->truncation_point[truncation_point_count + 1] - gf_job->truncation_point[truncation_point_count];
		gf_job->dma_info[dma_idx].dma_size_in_byte = dma_size_in_byte;
		gf_job->dma_info[dma_idx].dma_gpu_va = dma_va_cur;
		gf_job->dma_info[dma_idx++].idb_type = 0;//IDB_IDB_TYPE_NORMAL;

		dma_va_cur += dma_size_in_byte;
	}

	gf_job->dma_info[dma_idx].dma_size_in_byte = size - (dma_va_cur - dma_va);
	gf_job->dma_info[dma_idx].dma_gpu_va = dma_va_cur;
	gf_job->dma_info[dma_idx++].idb_type = 0;//IDB_IDB_TYPE_NORMAL;

	if(ls_dev->is_virtfn && ring->rb_index < RB_INDEX_VEM){
		struct lisuan_vm_node *vm_node = container_of(ringbuffer->ksave->va_list.next, struct lisuan_vm_node, node_item);
		gf_job->dma_info[dma_idx].dma_gpu_va = vm_node->offset;
		gf_job->dma_info[dma_idx].dma_size_in_byte = ringbuffer->ksave->cmd_size;
		gf_job->dma_info[dma_idx++].idb_type = 1;//IDB_IDB_TYPE_SAVE;
	}

	//lisuan_sync_bo_fence(gf_job, params->tbo, true);//need wait pre w&r

	if (vm->last_update.fence && !dma_fence_is_signaled(vm->last_update.fence))
	{
		struct gf_sync_fence *sync_fence = lisuan_sync_fence(vm->last_update.fence);
		sync_fence->sched = vm->last_update.sched;
		list_add_tail(&sync_fence->list_item, &gf_job->sync);
	}

	//replace lastest update job fence
	dma_fence_replace(&vm->last_update.fence, &s_fence->finished);
	vm->last_update.sched = ring;

	// update job need wait pre workload job, except same rb.
	for(i = 0; i < ls_dev->rb_count; i++){
		if(/*ring->rb_index != i &&*/
			(vm->last_used[i].fence && !dma_fence_is_signaled(vm->last_used[i].fence))){
			struct gf_sync_fence *sync_fence = lisuan_sync_fence(vm->last_used[i].fence);
			sync_fence->sched = vm->last_used[i].sched;
			list_add_tail(&sync_fence->list_item, &gf_job->sync);
		}
	}

	if(params->tbo)
		lisuan_add_sync_fence_to_buffer(params->tbo, &s_fence->finished, true);//replace for later sync
	lisuan_sched_entity_push_job(&gf_job->base, &gf_job->context->entity);

	gf_record_evt_kickoff(ring->device,ring->rb_index, gf_job->base.id, ring->rb_index);

	return ret;
exit:
	gf_job->base.sched->ops->free_job(&gf_job->base);
	return ret;
}

struct lisuan_vm_update_map_funcs lisuan_vm_gpu_funcs = {
	.prepare = lisuan_vm_gpu_prepare,
	.update_pde = lisuan_vm_gpu_update_pde,
	.update_pte = lisuan_vm_gpu_update_pte,
	.commit = lisuan_vm_gpu_commit,
};
