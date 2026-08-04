/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#include <linux/dma-fence-array.h>

#include "lisuan_scheduler.h"
#include "lisuan_device.h"
#include "lisuan_context.h"
#include "lisuan_sync.h"
#include "gf_job.h"
#include "lisuan_ring.h"
#include "lisuan_procid.h"
#include "gf_ringbuffer.h"
#include "lisuan_trace.h"

void lisuan_init_procid_mgr(struct lisuan_device *ls_dev)
{
	struct lisuan_procid_mgr *procid_mgr = (struct lisuan_procid_mgr *)gf_kvcalloc(1, sizeof(struct lisuan_procid_mgr), GFP_KERNEL);
	int i, j;

	mutex_init(&procid_mgr->id_lock.lock);
	INIT_LIST_HEAD(&procid_mgr->procid_lru);

	procid_mgr->num_ids = GFGPU_NUM_PROCID;

	//0 used for kmd.
	for (i = 1; i < procid_mgr->num_ids; i++) {
		struct lisuan_procid *procid = &procid_mgr->ids[i];

		procid->id = i;
		procid->mgr = procid_mgr;

		INIT_LIST_HEAD(&procid->active_sync);
		list_add_tail(&procid->list, &procid_mgr->procid_lru);
	}

	procid_mgr->fence_context = dma_fence_context_alloc(RB_INDEX_LAST);

	ls_dev->procid_mgr = procid_mgr;
	lisuan_init_vm_context(ls_dev);
}

void lisuan_fini_procid_mgr(struct lisuan_device *ls_dev)
{
	struct lisuan_procid_mgr *procid_mgr = ls_dev->procid_mgr;
	int i, j;

	mutex_lock(&procid_mgr->id_lock.lock);
	for (i = 1; i < procid_mgr->num_ids; i++) {
		struct lisuan_procid *procid = &procid_mgr->ids[i];
		struct gf_sync_fence *sync_fence, *tmp;

		list_for_each_entry_safe(sync_fence, tmp, &procid->active_sync, list_item) 
		{
			lisuan_sync_fence_free(sync_fence, true);
		}

		dma_fence_put(procid->last_flush.fence);
	}
	mutex_unlock(&procid_mgr->id_lock.lock);

	lisuan_fini_vm_context(ls_dev);

	gf_kvfree(procid_mgr);
}

void lisuan_procid_vm_detach(struct lisuan_device *ls_dev, struct lisuan_vm *vm)
{
	struct lisuan_procid_mgr *procid_mgr = ls_dev->procid_mgr;
	int i=0;

	mutex_lock(&procid_mgr->id_lock.lock);
	for (i = 1; i < procid_mgr->num_ids; i++) {
		struct lisuan_procid *procid = &procid_mgr->ids[i];

		if(procid->owner == vm->fence_context){
			if(procid->last_flush.fence && !dma_fence_is_signaled(procid->last_flush.fence))
				dma_fence_wait(procid->last_flush.fence, true);

			dma_fence_put(procid->last_flush.fence);
			procid->last_flush.fence = NULL;
			procid->owner = 0;
		}
	}
	mutex_unlock(&procid_mgr->id_lock.lock);
}

static int lisuan_can_procid_reuse(struct lisuan_ring *ring, 
		struct lisuan_vm *vm, struct gf_job *job, struct lisuan_procid **procid)
{
	struct lisuan_device *ls_dev = ring->device;
	struct lisuan_procid_mgr *procid_mgr = ls_dev->procid_mgr;
	struct lisuan_procid *procid_idle = NULL, *procid_used = NULL;
	int rb_index = job ? job->context->rb_index : 0xFFFFFFFF;
	struct dma_fence **fences = NULL;
	struct gf_sync_fence *sync_fence;

	//Check if we can use a VMID already assigned to this VM
	list_for_each_entry_reverse((*procid), &procid_mgr->procid_lru, list) {
		//Check all the prerequisites to using this VMID 
		if ((*procid)->owner != vm->fence_context)
			continue;

		if((*procid)->last_flush.sched != ring)
			continue;

		//Good, we can reuse this VMID. 
		//Remember this submission as user of the VMID.
		sync_fence = lisuan_sync_fence(&job->base.s_fence->finished);
		sync_fence->sched = ring;
		list_add_tail(&sync_fence->list_item, &(*procid)->active_sync);
		dma_fence_info("%s:%d finish fence:%p, ref:%d\n", __func__, __LINE__, sync_fence->fence, sync_fence->fence->refcount.refcount.refs.counter);
		return 0;
	}

	*procid = NULL;
	return 0;
}

static struct dma_fence *lisuan_sync_peek_fence(struct lisuan_procid *procid)
{
	struct gf_sync_fence *sync_fence, *tmp;

	list_for_each_entry_safe(sync_fence, tmp, &procid->active_sync, list_item) {
		struct dma_fence *f = sync_fence->fence;

		if (dma_fence_is_signaled(f)) {
			list_del(&sync_fence->list_item);
		
			dma_fence_info("%s:%d finish fence:%p, ref:%d\n", __func__, __LINE__, f, f->refcount.refcount.refs.counter);
		
			lisuan_sync_fence_free(sync_fence, true);
			continue;
		}

		return f;
	}

	return NULL;
}

static int lisuan_procid_grab_idle(struct lisuan_ring *ring, 
		struct lisuan_vm *vm, struct gf_job *job, struct lisuan_procid **procid)

{
	struct lisuan_device *ls_dev = ring->device;
	struct lisuan_procid_mgr *procid_mgr = ls_dev->procid_mgr;
	struct lisuan_procid *procid_tmp = NULL, *procid_used = NULL;
	struct dma_fence **fences = NULL;

	unsigned seqno, i = 0, j = 0, idle_found = 0;
	struct dma_fence_array *array;
	struct gf_sync_fence *sync_fence;
	struct lisuan_procid *procid_ = NULL;
	int r = 0;

	/* Check if we have an idle VMID */
	list_for_each_entry(procid_, &procid_mgr->procid_lru, list) {
		/* Don't use per engine and per process VMID at the same time */
		procid_mgr->fences[i] = lisuan_sync_peek_fence(procid_);
		if (!procid_mgr->fences[i])
		{
			if(!idle_found)
			{
				idle_found = true;
				*procid = procid_;
			}
		}
		else {
			++i;
		}
	}

	/* If we can't find a idle VMID to use, wait till one becomes available */
	if (!idle_found) {
		uint64_t fence_context = ls_dev->procid_mgr->fence_context + ring->rb_index;
		fences = kmalloc(i * sizeof(void *), GFP_KERNEL);
		if (!fences)
			return -ENOMEM;

		seqno = ++ls_dev->procid_mgr->seqno[ring->rb_index];

		*procid = NULL;
		for (j = 0; j < i; ++j)
			fences[j] = dma_fence_get(procid_mgr->fences[j]);

		array = dma_fence_array_create(i, fences, fence_context, seqno, true);
		if (!array) {
			for (j = 0; j < i; ++j)
				dma_fence_put(fences[j]);

			kfree(fences);
			return -ENOMEM;
		}
		sync_fence = lisuan_sync_fence(&array->base);

		vmid_info("not found idle used procid, emit a sync fence array:%p\n", sync_fence->fence);
		sync_fence->sched = ring;
		list_add_tail(&sync_fence->list_item, &job->sync);
		return -EBUSY;
	}

	return r;
}

//TODO fetch vm's last_update fence onto job dependency
int lisuan_grab_procid(struct lisuan_ring *ring, struct lisuan_vm *vm, struct gf_job *job)
{
	struct lisuan_device *ls_dev = ring->device;
	struct lisuan_procid_mgr *procid_mgr = ls_dev->procid_mgr;
	struct lisuan_procid *procid_idle = NULL, *procid_used = NULL;
	struct dma_fence **fences = NULL;

	unsigned seqno, j;
	struct dma_fence_array *array;
	struct gf_sync_fence *sync_fence;
	int r = 0;


	mutex_lock(&procid_mgr->id_lock.lock);
	trace_lisuan_grab_procid(&job->base);

	//check if idle one exist,  add a array based fence to job sync fence if no one exist.
	r = lisuan_procid_grab_idle(ring, vm, job, &procid_idle);
	if(r || !procid_idle)
		goto exit;

	//check if we can resue the procid already assigned to this vm
	r = lisuan_can_procid_reuse(ring, vm, job, &procid_used);
	if(r)
		goto exit;

	r = gf_job_populate_vm_dirty_state(&job->base);
	if(r)
		goto exit;

	if(!procid_used) {
		/* Can't reuse last vm's procid? Then use the idle one found earlier */
		procid_used = procid_idle;

		/* mark it on procid's active sync*/
		sync_fence = lisuan_sync_fence(&job->base.s_fence->finished);
		sync_fence->sched = ring;
		list_add_tail(&sync_fence->list_item, &procid_used->active_sync);

		list_move_tail(&procid_used->list, &procid_mgr->procid_lru);
		dma_fence_info("%s:%d finish fence:%p, ref:%d\n", __func__, __LINE__, sync_fence->fence, sync_fence->fence->refcount.refcount.refs.counter);

		//no one to reuse, need inv tlb before use
		job->need_vm_inv = 1;
		vmid_info("tid-pid:%d-%d, use proc:%d, before:%d\n", vm->file->tgid, vm->file->pid, procid_used->id, vm->file->proc_id);
	}

	//update procid's last use fence
	procid_used->owner = vm->fence_context;
	dma_fence_replace(&procid_used->last_flush.fence, &job->base.s_fence->finished);
	procid_used->last_flush.sched = ring;
	dma_fence_info("%s:%d finish fence:%p, ref:%d\n", __func__, __LINE__, procid_used->last_flush.fence, 
		procid_used->last_flush.fence->refcount.refcount.refs.counter);

	job->procid = procid_used->id;
	vm->file->proc_id = job->procid;
exit:
	mutex_unlock(&procid_mgr->id_lock.lock);
	return r;
}

