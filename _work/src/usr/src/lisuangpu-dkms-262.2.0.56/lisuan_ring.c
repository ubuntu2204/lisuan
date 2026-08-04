/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#include <linux/console.h>
#include "lisuan_scheduler.h"
#include "gf_hw_misc.h"

#include "gf_job.h"
#include "lisuan_vm.h"
#include "gf_ringbuffer.h"
#include "lisuan_ring.h"

void lisuan_sched_status_show(struct lisuan_device * ls_dev)
{
	unsigned int i =0 ;

	for (i = 0; i < ls_dev->rb_count; i++) {
		struct lisuan_ring *ring = ls_dev->sched[i];
		struct lisuan_scheduler *sched = &ring->base;

		gf_info("=====================start sched(%p) %d, jobs exec:%d========================\n", sched, ring->rb_index, atomic_read(&sched->hw_rq_count));
		lisuan_sched_show_ring_jobs(sched);
		lisuan_sched_show_rq_top_status(sched);
	}

}

void lisuan_ring_dump_jobs(struct lisuan_device * ls_dev)
{
	unsigned int i =0 ;

	//only for hw hang and reset not enable dump
	//since driver couldn't detect which engine cause hw hang
	//job dump may dump the wrong engine's job.
	//add this debugfs dump code to dump more engine's job.
	for (i = 0; i < ls_dev->rb_count; i++) {
		struct lisuan_ring *ring = ls_dev->sched[i];
		struct lisuan_scheduler *sched = &ring->base;

		lisuan_sched_dump_ring_jobs(sched);
	}

}

void lisuan_dump_debugbus(struct lisuan_device * ls_dev)
{
	gf_dump_debug_status(ls_dev->mmio_base);
}

void lisuan_init_ring(struct lisuan_device *ls_dev)
{
	struct lisuan_kbo_mman *kcommand = &ls_dev->kcommand;
	struct lisuan_kbo_mman *kfence = &ls_dev->kfence;

	int i, ret;

	kfence->bufObj = gf_kvcalloc(1, sizeof(struct lisuan_gem_bo), GFP_KERNEL);
	kfence->bufObj->size = 4096;
	kfence->bufObj->align = 64 * 1024;
	kfence->bufObj->domain = ls_dev->bufsel.fence_buffer_dom;
	kfence->bufObj->flag.byPass = 1;
	kfence->bufObj->pm_discard = 1;
	kfence->alignment = sizeof(unsigned long long);
	atomic_set(&kfence->bufObj->unpagable, 1);
	kfence->bufObj->priority = gem_priority_high;
	kfence->bufObj->flag.vram = (kfence->bufObj->domain <= dom_invisual_vram) ? 1 : 0;
	kfence->bufObj->flag.snoop = ((kfence->bufObj->domain == dom_snoop_system_pages) ? 1 : 0);

	kcommand->bufObj = gf_kvcalloc(1, sizeof(struct lisuan_gem_bo), GFP_KERNEL);
	kcommand->bufObj->size = 2 * 1024 * 1024; //2M
	kcommand->bufObj->align = 64 * 1024;
	kcommand->bufObj->domain = ls_dev->bufsel.kcmd_buffer_dom;
	kcommand->bufObj->flag.byPass = 1;
	kcommand->bufObj->pm_discard = 1;
	kcommand->alignment = sizeof(unsigned int);
	atomic_set(&kcommand->bufObj->unpagable, 1);
	kcommand->bufObj->priority = gem_priority_high;
	kcommand->bufObj->flag.vram = (kcommand->bufObj->domain <= dom_invisual_vram) ? 1 : 0;
	kcommand->bufObj->flag.snoop = ((kcommand->bufObj->domain == dom_snoop_system_pages) ? 1 : 0);


	ret = lisuan_reserve_kbo(ls_dev, kfence);
	if (ret)
		return;
	ret = lisuan_reserve_kbo(ls_dev, kcommand);
	if (ret)
		return;

	for (i = 0; i < ls_dev->rb_count; i++) {
		struct lisuan_ring *gf_ring = (struct lisuan_ring *)gf_kvcalloc(1, sizeof(struct lisuan_ring), GFP_KERNEL);
		struct lisuan_scheduler *sched = &gf_ring->base;
		ls_dev->sched[i] = gf_ring;
		gf_ring->device = ls_dev;
		gf_ring->rb_index = i;

		gf_ring->rb_lock = lisuan_init_os_mutex();

		gf_ring->fence_node.size = sizeof(unsigned long long);
		ret = lisuan_alloc_kbo_node(&ls_dev->kfence, &gf_ring->fence_node);
		ASSERT(ret == 0);
		gf_debug("rb:%d fence gpuva:%llx\n", i, gf_ring->fence_node.start);
		snprintf(gf_ring->name, 66, "sched rb %d", i);
		spin_lock_init(&gf_ring->ring_fence_lock);
		atomic64_set(&gf_ring->last_submit_fence_id, 0);
		if (ls_dev->chip_id == CHIP_GF) {
			switch (i) {
			case RB_INDEX_GFX:
				lisuan_sched_init(sched, &gfx_sched_ops, 2, 2, ls_dev->hang_timeout, gf_ring->name);
				gf_init_gfx_ring(gf_ring, i);
				break;
			case RB_INDEX_GCE:
				lisuan_sched_init(sched, &gce_sched_ops, 2, 2, ls_dev->hang_timeout, gf_ring->name);
				gf_init_gce_ring(gf_ring, i);
				break;
			case RB_INDEX_VEM:
				lisuan_sched_init(sched, &vem_sched_ops, 2, 2, ls_dev->hang_timeout, gf_ring->name);
				gf_init_vem_ring(gf_ring, i);
				ls_dev->vem_sync_fence_buf_ptr    = gf_ring->fence_node.cpu_virt_addr;
				ls_dev->vem_sync_fence_buf_gpu_va = gf_ring->fence_node.start;
				break;
			case RB_INDEX_DEC0:
			case RB_INDEX_DEC1:
				lisuan_sched_init(sched, &dec_sched_ops, 1, 1, ls_dev->hang_timeout, gf_ring->name);
				gf_init_dec_ring(gf_ring, i);
				break;
			case RB_INDEX_ENC:
			case RB_INDEX_IM:
				lisuan_sched_init(sched, &enc_sched_ops, 1, 1, ls_dev->hang_timeout, gf_ring->name);
				gf_init_enc_ring(gf_ring, i);
				break;
			default:
				break;
			}
		}
	}
fail:
	return;
}

void lisuan_fini_ring(struct lisuan_device *ls_dev)
{
	struct lisuan_kbo_mman *fence = &ls_dev->kfence;
	struct lisuan_kbo_mman *kcmd = &ls_dev->kcommand;
	int i;

	for (i = 0; i < ls_dev->rb_count; i++) {
		struct lisuan_ring *sched = ls_dev->sched[i];

		sched->fence_node.free(&sched->fence_node);
		mutex_destroy(&sched->rb_lock->lock);
		gf_kvfree(sched->rb_lock);

		switch (ls_dev->chip_id) {
		case CHIP_GF:
			gf_ringbuffer_deinit(ls_dev, i, sched);
			break;
		default:
			break;
		}

		lisuan_sched_fini(&sched->base);
		gf_kvfree(sched);
	}

	fence->free(ls_dev, fence);
	kcmd->free(ls_dev, kcmd);
}

int lisuan_ring_check_hang(struct lisuan_ring *ring, struct gf_job *job)
{
	struct lisuan_device *ls_dev = ring->device;

	if(atomic64_read(&ring->returned_fence_id) < *ring->fence_node.cpu_virt_addr)
	{
		gf_info("seems irq lost, return-mem-job-last:0x%llx-0x%llx-0x%llx-0x%llx\n",
			atomic64_read(&ring->returned_fence_id),*ring->fence_node.cpu_virt_addr, 
			job->ring_fence->ring_fence_value, atomic64_read(&ring->last_submit_fence_id));

		if (*ring->fence_node.cpu_virt_addr >= atomic64_read(&ring->returned_fence_id) && 
			*ring->fence_node.cpu_virt_addr <= atomic64_read(&ring->last_submit_fence_id))
			atomic64_set(&ring->returned_fence_id, *ring->fence_node.cpu_virt_addr);
		
		tasklet_schedule(&ring->device->fenceirq_work);
		return 0;
	}
	else if(atomic64_read(&ring->returned_fence_id) == *ring->fence_node.cpu_virt_addr &&
		job->ring_fence->ring_fence_value == atomic64_read(&ring->returned_fence_id))
	{
		gf_info("seems ring fence not correct signal or job not clean , return-mem-job-last:0x%llx-0x%llx-0x%llx-0x%llx\n",
			atomic64_read(&ring->returned_fence_id),*ring->fence_node.cpu_virt_addr, 
			job->ring_fence->ring_fence_value, atomic64_read(&ring->last_submit_fence_id));
		wake_up_interruptible(&ls_dev->ring_fence_wake_up_task);
		wake_up_interruptible(&ring->base.wake_up_worker);
		return 0;
	}

	gf_info("timeout checking: rb %d, return-mem-job-last:0x%llx-0x%llx-0x%llx-0x%llx, job:%d from tgid-pid:%d-%d, procid:%d\n", ring->rb_index,
		atomic64_read(&ring->returned_fence_id),*ring->fence_node.cpu_virt_addr, 
		job->ring_fence->ring_fence_value, atomic64_read(&ring->last_submit_fence_id),
		job->base.id, job->tgid, job->pid, job->procid);

	return (job->ring_fence->ring_fence_value > atomic64_read(&ring->returned_fence_id)) &&
		(ls_dev->is_virtfn?gf_is_ring_at_current_vf(ls_dev->mmio_base, ls_dev->is_virtfn, ls_dev->virtfn_id, ring->rb_index):true);
}

int dm_crtc_dpms(struct display_adapter *adapter, struct crtc_t *crtc, bool on);
void lisuan_sched_recover_gpu(struct lisuan_ring *sched)
{
	struct lisuan_device *ls_dev = sched->device;
	struct lisuan_sched_job *s_job, *tmp;
    struct lisuan_cl_info cl_info;
	int i;
	unsigned char dpms_off[4] = {0, 0, 0, 0};
	unsigned char wait_vblank_index = 0xff;

	if(!ls_dev->is_virtfn) {
		for (i = 0; i < ls_dev->adapter->crtc_num; i++) {
			// First crtc keep active and wait vsync, dpms off other active crtc
			if (0xff == wait_vblank_index && ls_dev->adapter->crtcs[i].dpms) {
				wait_vblank_index = i;
			} else if (0xff != wait_vblank_index && ls_dev->adapter->crtcs[i].dpms) {
				dm_crtc_dpms(ls_dev->adapter, &ls_dev->adapter->crtcs[i], false);
				dpms_off[i] = 1;
			}
		}
	}

	console_lock();
	if(!ls_dev->is_virtfn)
		gf_reset_hw(ls_dev->mmio_base, ls_dev->is_virtfn, ls_dev->virtfn_id, wait_vblank_index);

	lisuan_query_cl_info(ls_dev,&cl_info);
	gf_hw_mhb_init(ls_dev->mmio_base,
		ls_dev->vram_size / 1024 / 1024,
		cl_info.cl_base,
		cl_info.cl_size,
		ls_dev->mhu_disable_mask,
		ls_dev->mhb_num,
		ls_dev->is_virtfn);

	lisuan_vm_reinit_bypass_pt(ls_dev);
	console_unlock();

	if(!ls_dev->is_virtfn)
		for (i = 0; i < ls_dev->adapter->crtc_num; i++)
			if (dpms_off[i])
				dm_crtc_dpms(ls_dev->adapter, &ls_dev->adapter->crtcs[i], true);

	if(ls_dev->hwctl.endebug)
	{
		gf_dump_status(ls_dev->mmio_base, ls_dev->is_virtfn, ls_dev->virtfn_id, sched->rb_index);

		gf_info("hw reset finish, wait_vblank_index[%u] dpms_off_on[%u %u %u %u]\n",
			wait_vblank_index, dpms_off[0], dpms_off[1], dpms_off[2], dpms_off[3]);
	}

	for (i = 0; i < ls_dev->rb_count; i++) {
		gf_scheduler_reinit(ls_dev, i, ls_dev->sched[i]);
	}
	gf_init_gpu_state(ls_dev->sched[0]);

}
void lisuan_sched_fence_irq_worklet(struct lisuan_device *ls_dev)
{
	struct lisuan_ring *sched;
	int i;

	for (i = 0; i < ls_dev->rb_count; i++) {
		sched = ls_dev->sched[i];
#ifndef FAKE_CHIP
		if (*sched->fence_node.cpu_virt_addr >= atomic64_read(&sched->returned_fence_id) && *sched->fence_node.cpu_virt_addr <= atomic64_read(&sched->last_submit_fence_id))
		{
			atomic64_set(&sched->returned_fence_id, *sched->fence_node.cpu_virt_addr);
		}
		else
			gf_error("rb %d returned fence 0x%llx, last submit fence 0x%llx, fence in mem:0x%llx\n",
			i, atomic64_read(&sched->returned_fence_id), atomic64_read(&sched->last_submit_fence_id), *sched->fence_node.cpu_virt_addr);
#endif
	}
	wake_up_interruptible(&ls_dev->ring_fence_wake_up_task);
}