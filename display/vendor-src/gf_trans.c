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
#include "gf_trans.h"

unsigned int
gf_calc_cl_dpa(struct lisuan_device *ls_dev, unsigned int start_addr, unsigned int mhb_num)
{
	unsigned int final_uStartAddr = start_addr;
	if (mhb_num == 6)
	{
		unsigned int pa_remove_chid = (start_addr >> 7) * 22 + (start_addr & 0x7f) / 6;
		final_uStartAddr = pa_remove_chid * 6 + ((start_addr & 0x7f) % 6);
	}
	else if (mhb_num == 3)
	{
		unsigned int pa_remove_chid = (start_addr >> 7) * 43 + (start_addr & 0x7f) / 3;
		final_uStartAddr = pa_remove_chid * 3 + ((start_addr & 0x7f) % 3);
	}

	return final_uStartAddr;
}

static unsigned long long
gf_get_bypass_addr(struct lisuan_gem_bo * gbo)
{
	if(gbo->flag.vram)
		return gbo->vm_node->offset;
	else{
		struct lisuan_vm_node *vm_node = container_of(gbo->va_list.next, struct lisuan_vm_node, node_item);
		gf_assert(vm_node->byPass);
		return vm_node->offset;
	}
}
void gf_send_ifence_wait(struct lisuan_device *ls_dev, unsigned int **dma_ptr, unsigned int rb_index)
{
	struct lisuan_vm_node *vm_node = NULL;
	struct lisuan_vm_node *dummy_vm_node = NULL;

	dummy_vm_node = container_of(ls_dev->dummy_page_buf->va_list.next, struct lisuan_vm_node, node_item);
	
	gf_hw_get_ifence_wait_cmds(dma_ptr, rb_index, dummy_vm_node->offset, ls_dev->hwctl.enable_gl2);
}

static void gf_do_auto_clear(struct lisuan_device *ls_dev, struct lisuan_gem_bo * gbo, 
	unsigned int ** ppCmd)
{
	struct lisuan_cf_node *cf_node = gbo->cf_node;
	struct lisuan_vm_node *vm_node = container_of(ls_dev->dummy_page_buf->va_list.next, struct lisuan_vm_node, node_item);

	unsigned long long start_addr, end_addr;
	unsigned long long start_addr_in_dpa, end_addr_in_dpa;
	int fill_size = cf_node->cf_size;

	start_addr = cf_node->cell_index * ls_dev->cf.cf_cell_size;
	end_addr = start_addr + fill_size;

	start_addr_in_dpa = gf_calc_cl_dpa(ls_dev, start_addr, ls_dev->mhb_num);
	end_addr_in_dpa = gf_calc_cl_dpa(ls_dev, end_addr, ls_dev->mhb_num);
	
	gf_hw_get_do_auto_clear_cmds(ppCmd, vm_node->offset, start_addr_in_dpa, end_addr_in_dpa, fill_size);
}

static void gf_do_clear(struct lisuan_device *ls_dev, struct lisuan_gem_bo * gbo, 
	unsigned int ** ppCmd)
{
	struct lisuan_ctx *context = ls_dev->vm_ctx;
	struct lisuan_ring *ring = ls_dev->sched[context->rb_index];
	struct lisuan_vm_node *vm_node = container_of(ls_dev->dummy_page_buf->va_list.next, struct lisuan_vm_node, node_item);
	struct gf_gce_ringbuffer *gce_rb = (struct gf_gce_ringbuffer *)ring->chip_priv;
	unsigned long long bypass_gpuva = gf_get_bypass_addr(gbo);
	int clr_size = gbo->size;

	gf_hw_get_do_clear_cmds(ppCmd, ring->rb_index, bypass_gpuva, clr_size, ring->rb_index == RB_INDEX_GCE ? gce_rb->gce_clr : NULL);
}

static void gf_do_blt(struct lisuan_device *ls_dev, struct lisuan_gem_bo * src, 
	struct lisuan_gem_bo * dst, unsigned int ** ppCmd)
{
	gf_assert(0);
}

static void gf_do_raw_blt(struct lisuan_device *ls_dev, struct lisuan_gem_bo * src, 
	struct lisuan_gem_bo * dst, unsigned int ** ppCmd)
{
	struct lisuan_ctx *context = ls_dev->vm_ctx;
	struct lisuan_ring *ring = ls_dev->sched[context->rb_index];
	struct gf_gce_ringbuffer *gce_rb = (struct gf_gce_ringbuffer *)ring->chip_priv;

	unsigned long long src_addr = gf_get_bypass_addr(src);
	unsigned long long dst_addr = gf_get_bypass_addr(dst);

	gf_hw_get_raw_blit_cmds(ppCmd, ring->rb_index, src_addr, dst_addr, src->size, ring->rb_index == RB_INDEX_GCE ? gce_rb->gce_blt : NULL, ls_dev->hwctl.enable_gl2);
}

void gf_trans(struct lisuan_device *ls_dev, struct lisuan_gem_bo * src, 
	struct lisuan_gem_bo * dst, int trans_op, struct dma_fence ** fence)
{
	struct gf_job *gf_job = (struct gf_job *)gf_kvcalloc(1, sizeof(struct gf_job), GFP_KERNEL);
	struct lisuan_ctx *context = ls_dev->vm_ctx;
	struct lisuan_sched_fence *s_fence;
	struct lisuan_scheduler *sched = gf_job->base.sched;
	int ret = 0, i, size, dma_idx=0;	
	struct lisuan_ring *ring;
	struct gf_ring_buffer *ringbuffer;
	unsigned int *p_cmd, *shadow_idb;

	gf_job->context = context;

	INIT_LIST_HEAD(&gf_job->sync);
	INIT_LIST_HEAD(&gf_job->pt_buf_list);

	lisuan_sched_job_init(&gf_job->base, &gf_job->context->entity, (void *)0ul);

	gf_job->uf_sequence = atomic_read(&gf_job->context->entity.fence_seq);

	gf_job->vm = NULL;
	gf_job->procid = 0;

	ring  = container_of(gf_job->base.sched, struct lisuan_ring, base);
	ringbuffer= (struct gf_ring_buffer *)ring->chip_priv;

	p_cmd = 
	shadow_idb = gf_calloc(64*1024);

	switch(trans_op)
	{
	case lisuan_trans_op_evict:
		gf_job->evict_gem = dst;
		gf_do_raw_blt(ls_dev, src, dst, &p_cmd);
		break;
	case lisuan_trans_op_resident:
		gf_job->evict_gem = src;
		gf_do_raw_blt(ls_dev, src, dst, &p_cmd);
		break;
	case lisuan_trans_op_blt:
		gf_do_blt(ls_dev, src, dst, &p_cmd);
		break;
	case lisuan_trans_op_clear:
		gf_do_clear(ls_dev, dst, &p_cmd);
		break;
	case lisuan_trans_op_auto_clear:
		gf_do_auto_clear(ls_dev, dst, &p_cmd);
		break;
	default:
		break;
	}

	gf_send_ifence_wait(ls_dev, &p_cmd, gf_job->context->rb_index);

	size = (int)((char *)p_cmd - (char *)shadow_idb);
	gf_job->kcommand = (struct lisuan_kbo_node *)gf_kvcalloc(1, sizeof(struct lisuan_kbo_node), GFP_KERNEL);
	gf_job->kcommand->size = size;

retry:
	ret = lisuan_alloc_kbo_node(&ls_dev->kcommand, gf_job->kcommand);
	if(ret)
	{
		wake_up_interruptible(&sched->wake_up_worker);
		//wait_event_interruptible_timeout(ls_dev->ring_fence_wake_up_task,
		//				 lisuan_any_ring_fence_signaled(ls_dev), msecs_to_jiffies(500));
		
		udelay(40);
		goto retry;
	}
	gf_memcpy((void*)gf_job->kcommand->pCmd, (char *)shadow_idb, size);

	//gf_dump_commands("trans cmd", (void*)gf_job->kcommand->pCmd, size/4);
	gf_job->dma_count = (ls_dev->is_virtfn && ring->rb_index < RB_INDEX_VEM)?3:1;
	gf_job->dma_info = (struct dma_info_s *)gf_kvcalloc(gf_job->dma_count, sizeof(struct dma_info_s), GFP_KERNEL);

	if(ls_dev->is_virtfn && ring->rb_index < RB_INDEX_VEM){
		struct lisuan_vm_node *vm_node = container_of(ringbuffer->krestore->va_list.next, struct lisuan_vm_node, node_item);
		gf_job->dma_info[dma_idx].dma_gpu_va = vm_node->offset;
		gf_job->dma_info[dma_idx].dma_size_in_byte = ringbuffer->krestore->cmd_size;
		gf_job->dma_info[dma_idx++].idb_type = 2;//IDB_IDB_TYPE_RESTORE;
	}

	gf_job->dma_info[dma_idx].dma_size_in_byte = size;
	gf_job->dma_info[dma_idx].dma_gpu_va = gf_job->kcommand->start;
	gf_job->dma_info[dma_idx++].idb_type = 0;//IDB_IDB_TYPE_NORMAL;

	if(ls_dev->is_virtfn && ring->rb_index < RB_INDEX_VEM){
		struct lisuan_vm_node *vm_node = container_of(ringbuffer->ksave->va_list.next, struct lisuan_vm_node, node_item);
		gf_job->dma_info[dma_idx].dma_gpu_va = vm_node->offset;
		gf_job->dma_info[dma_idx].dma_size_in_byte = ringbuffer->ksave->cmd_size;
		gf_job->dma_info[dma_idx++].idb_type = 1;//IDB_IDB_TYPE_SAVE;
	}

	if(src)
	{
		lisuan_sync_bo_fence(gf_job, &src->tbo, false);
		lisuan_add_sync_fence_to_buffer(&src->tbo, &gf_job->base.s_fence->finished, false);
	}

	if(dst)
	{
		lisuan_sync_bo_fence(gf_job, &dst->tbo, true);
		lisuan_add_sync_fence_to_buffer(&dst->tbo, &gf_job->base.s_fence->finished, true);
	}

	if(fence)
		*fence = dma_fence_get(&gf_job->base.s_fence->finished);

	lisuan_sched_entity_push_job(&gf_job->base, &gf_job->context->entity);

	gf_free(shadow_idb);
}

