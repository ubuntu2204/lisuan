/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#include <linux/idr.h>
#include <drm/drm_file.h>
#include <drm/drm_syncobj.h>
#include <drm/drm_gem.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <uapi/linux/sched/types.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/ww_mutex.h>
#include <linux/module.h>
#include <linux/uaccess.h>

#include "lisuan_drm.h"
#include "lisuan_device.h"
#include "lisuan_gem.h"
#include "lisuan_vm.h"
#include "lisuan_ring.h"
#include "gf_job.h"
#include "gf_ringbuffer.h"
#include "lisuan_sync.h"

#include <linux/slab.h>

#if DRM_VERSION_CODE >= KERNEL_VERSION(5,2,0)
#include <linux/dma-fence-chain.h>
#else
struct dma_fence_chain {
	struct dma_fence base;
	struct dma_fence *prev;
};
static const char *gf_fence_get_driver_name(struct dma_fence *f)
{
	return "gf_chain";
}
static const char *gf_fence_get_timeline_name(struct dma_fence *f)
{
	return "gf_timeline";
}
static const struct dma_fence_ops dma_fence_chain_ops = {
	.get_driver_name = gf_fence_get_driver_name,
	.get_timeline_name = gf_fence_get_timeline_name,
};
/*compat function for timeline syncobj feature*/
static int drm_syncobj_add_point(struct drm_syncobj *syncobj,
							struct dma_fence_chain *chain,
							struct dma_fence *fence,
							u64 point)
{
	if (!syncobj || !chain || !fence)
		return -EINVAL;

	dma_fence_init(&chain->base,
					&dma_fence_chain_ops, 
					fence->lock,
					fence->context,
					fence->seqno + 1); 

	chain->prev = fence;

	drm_syncobj_replace_fence(syncobj, &chain->base);

	return 0;
}
#endif

#if DRM_VERSION_CODE < KERNEL_VERSION(5,15,0)
static struct dma_fence_chain *compat_dma_fence_chain_alloc(void)
{
	return kmalloc(sizeof(struct dma_fence_chain), GFP_KERNEL);
}
#endif

static struct kmem_cache *lisuan_ring_fence_slab;

static const char *lisuan_ring_fence_get_timeline_name(struct dma_fence *fence)
{
	struct lisuan_ring_fence *ring_fence = container_of(fence, struct lisuan_ring_fence, base);
	return ring_fence->ring->base.name;
}

static bool lisuan_ring_fence_enable_signaling(struct dma_fence *fence)
{
	struct lisuan_ring_fence *ring_fence = container_of(fence, struct lisuan_ring_fence, base);
	struct lisuan_device *device = ring_fence->ring->device;
	unsigned int rb_index = ring_fence->rb_index;

	if (dma_fence_is_signaled_locked(fence))
		return false;

	spin_lock(&device->ring_fence_lock[rb_index]);

	dma_fence_get(&ring_fence->base);
	list_add_tail(&ring_fence->list_in_device, &device->ring_fence_list[rb_index]);
	device->ring_fence_count[rb_index]++;

	spin_unlock(&device->ring_fence_lock[rb_index]);
	return true;
}

static const char *lisuan_fence_get_driver_name(struct dma_fence *fence)
{
	return "lisuan";
}

static bool lisuan_ring_fence_signaled(struct dma_fence *fence)
{
	struct lisuan_ring_fence *ring_fence = container_of(fence, struct lisuan_ring_fence, base);
	if (ring_fence->ring_fence_value <= atomic64_read(&ring_fence->ring->returned_fence_id))
		return true;
	else
		return false;
}

static void lisuan_ring_fence_release(struct dma_fence *fence)
{
	struct lisuan_ring_fence *ring_fence = container_of(fence, struct lisuan_ring_fence, base);

	kmem_cache_free(lisuan_ring_fence_slab, ring_fence);
}

const struct dma_fence_ops lisuan_ring_fence_ops = {
	.get_driver_name = lisuan_fence_get_driver_name,
	.get_timeline_name = lisuan_ring_fence_get_timeline_name,
	.enable_signaling = lisuan_ring_fence_enable_signaling,
	.signaled = lisuan_ring_fence_signaled,
	.release = lisuan_ring_fence_release,
};

struct lisuan_ring_fence *lisuan_ring_fence_create(struct lisuan_ring *ring)
{
	struct lisuan_device *ls_dev = ring->device;
	struct lisuan_ring_fence *ring_fence = kmem_cache_alloc(lisuan_ring_fence_slab, GFP_KERNEL);

	if (!ring_fence)
		return NULL;

	ring_fence->ring = ring;
	ring_fence->rb_index = ring->rb_index;
	ring_fence->ring_fence_value = atomic64_inc_return(&ring->last_submit_fence_id);
	spin_lock_init(&ring_fence->fence_lock);
	dma_fence_init(&ring_fence->base, &lisuan_ring_fence_ops, &ring_fence->fence_lock, 
		ls_dev->ring_fence_context + ring->rb_index, ring_fence->ring_fence_value);

	return ring_fence;
}

int lisuan_sync_create_ioctl(struct drm_device *dev, void *data, struct drm_file *filp)
{
	int ret = 0, rb_index;
	struct lisuan_device *ls_dev = (struct lisuan_device *)dev->dev_private;
	struct lisuan_file *ls_file = (struct lisuan_file *)filp->driver_priv;
	resource_size_t start = ls_dev->pdev_info.start_addr_vram;
	struct lisuan_sync_cteate_info *create_info;
	struct lisuan_gem_bo *buffer = NULL;
	struct lisuan_monitor_fence *monitor_fence=NULL;
	struct lisuan_mman *mmgr;
	struct lisuan_vm_node *vm_node;

	create_info = (struct lisuan_sync_cteate_info *)data;
	buffer = (struct lisuan_gem_bo *)gf_kvcalloc(1, sizeof(struct lisuan_gem_bo), GFP_KERNEL);

	buffer->size = 4 * 1024;
	buffer->align = ls_dev->hwctl.use_vram?64 * 1024:(ls_dev->sys_page_size);
	buffer->domain = ls_dev->hwctl.use_vram?dom_visual_vram:(ls_dev->hwctl.support_wc?dom_system_pages:dom_snoop_system_pages);
	buffer->flag.byPass = 1;
	buffer->flag.nsec = 1;
	atomic_set(&buffer->unpagable, 1);
	buffer->priority = gem_priority_normal;
	buffer->flag.vram = (buffer->domain <= dom_invisual_vram) ? 1 : 0;
	buffer->flag.snoop = ((buffer->domain == dom_snoop_system_pages) ? 1 : 0);

	ret = lisuan_gem_alloc_oneshot(ls_dev, ls_file, buffer, 1);
	if(ret)
		goto fail;
	monitor_fence = (struct lisuan_monitor_fence *)gf_kvcalloc(1, sizeof(struct lisuan_monitor_fence), GFP_KERNEL);
	monitor_fence->buffer = buffer;
	monitor_fence->kmd_va = buffer->virt_addres;
	monitor_fence->sched = ls_dev->sched[create_info->ring_buffer];

	mutex_lock(&ls_file->sync_object_lock->lock);
	create_info->sync_object_handle = idr_alloc(&ls_file->sync_object_handles, monitor_fence, 0, -1, GFP_KERNEL);
	mutex_unlock(&ls_file->sync_object_lock->lock);
	create_info->buffer_handle = buffer->handle;
	return ret;
fail:
	if(buffer)
		gf_kvfree(buffer);
	if(monitor_fence)
		gf_kvfree(monitor_fence);
	return ret;
}

int lisuan_sync_signal_ioctl(struct drm_device *dev, void *data, struct drm_file *filp)
{
	struct lisuan_monitor_fence *monitor_fence;
	struct lisuan_device *ls_dev = (struct lisuan_device *)dev->dev_private;
	struct lisuan_file *ls_file = (struct lisuan_file *)filp->driver_priv;
	int ret = 0, dma_idx=0;
	struct lisuan_ctx *context;
	struct gf_job *sched_job;
	struct lisuan_scheduler *sched;
	struct lisuan_vm_node *vm_node;
	struct lisuan_sync_signal_info *signal_info = (struct lisuan_sync_signal_info *)data;
	struct lisuan_ring *ring = NULL;
	struct gf_ring_buffer *ringbuffer = NULL;
	unsigned int cmd_size = 5*sizeof(unsigned int);
	unsigned char *pCmd;
	struct drm_gem_object *first_gem = NULL;
	struct ttm_buffer_object *first_gem_ttm = NULL;

	mutex_lock(&ls_file->sync_object_lock->lock);
	monitor_fence = (struct lisuan_monitor_fence *)idr_find(&ls_file->sync_object_handles, signal_info->sync_object_handle);
	mutex_unlock(&ls_file->sync_object_lock->lock);

	mutex_lock(&ls_file->context_lock->lock);
	context = (struct lisuan_ctx *)idr_find(&ls_file->context_handles, signal_info->context);
	mutex_unlock(&ls_file->context_lock->lock);

	ring = ls_dev->sched[context->rb_index];
	ringbuffer = (struct gf_ring_buffer *)ring->chip_priv;

	sched_job = (struct gf_job *)gf_kvcalloc(1, sizeof(struct gf_job), GFP_KERNEL);

	INIT_LIST_HEAD(&sched_job->sync);
	INIT_LIST_HEAD(&sched_job->pt_buf_list);
	sched_job->context = context;
	lisuan_sched_job_init(&sched_job->base, &context->entity, (void *)0ul);
	sched_job->vm = &ls_file->vm;
	sched_job->uf_sequence = atomic_read(&context->entity.fence_seq);

	sched_job->dma_count = (ls_dev->is_virtfn && ring->rb_index < RB_INDEX_VEM)?3:1;
	sched_job->dma_info = (struct dma_info_s *)gf_kvcalloc(sched_job->dma_count, sizeof(struct dma_info_s), GFP_KERNEL);
	sched = sched_job->base.sched;

	vm_node = container_of(monitor_fence->buffer->va_list.next, struct lisuan_vm_node, node_item);
	pCmd = gf_calloc(cmd_size);

	switch(context->rb_index)
	{
		case RB_INDEX_GFX:
			gf_hw_get_job_run_fence_cmds(&pCmd, true, vm_node->mm_node->start, signal_info->fence_value, ls_dev->rb_cmd_template, GF_CMD_TYPE_GFX);
			break;
		case RB_INDEX_GCE:
			gf_hw_get_job_run_fence_cmds(&pCmd, true, vm_node->mm_node->start, signal_info->fence_value, ls_dev->rb_cmd_template, GF_CMD_TYPE_GCE);
			break;
		case RB_INDEX_VEM:
			gf_hw_get_job_run_fence_cmds(&pCmd, true, vm_node->mm_node->start, signal_info->fence_value, ls_dev->rb_cmd_template, GF_CMD_TYPE_VEM);
			break;
		default:
			break;
	}
	
	if(context->rb_index == RB_INDEX_VEM)
	{
		sched_job->vem_cmd = (void*)pCmd;
		sched_job->vem_cmd_size = cmd_size;
	}
	else
	{
		sched_job->kcommand = (struct lisuan_kbo_node *)gf_kvcalloc(1, sizeof(struct lisuan_kbo_node), GFP_KERNEL);
		sched_job->kcommand->size = cmd_size;

retry:
		ret = lisuan_alloc_kbo_node(&ls_dev->kcommand, sched_job->kcommand);
		if(ret)
		{
			wake_up_interruptible(&sched->wake_up_worker);
			
			udelay(40);
			goto retry;
		}
		gf_memcpy((void*)sched_job->kcommand->pCmd, (void*)pCmd, cmd_size);

		if(ls_dev->is_virtfn && ring->rb_index < RB_INDEX_VEM){
			struct lisuan_vm_node *vm_node = container_of(ringbuffer->krestore->va_list.next, struct lisuan_vm_node, node_item);
			sched_job->dma_info[dma_idx].dma_gpu_va = vm_node->offset;
			sched_job->dma_info[dma_idx].dma_size_in_byte = ringbuffer->krestore->cmd_size;
			sched_job->dma_info[dma_idx++].idb_type = 2;//IDB_IDB_TYPE_RESTORE;
		}

		sched_job->dma_info[dma_idx].dma_gpu_va = sched_job->kcommand->start;
		sched_job->dma_info[dma_idx].dma_size_in_byte = cmd_size;
		sched_job->dma_info[dma_idx++].idb_type = 0;//IDB_IDB_TYPE_NORMAL;

		if(ls_dev->is_virtfn && ring->rb_index < RB_INDEX_VEM){
			struct lisuan_vm_node *vm_node = container_of(ringbuffer->ksave->va_list.next, struct lisuan_vm_node, node_item);
			sched_job->dma_info[dma_idx].dma_gpu_va = vm_node->offset;
			sched_job->dma_info[dma_idx].dma_size_in_byte = ringbuffer->ksave->cmd_size;
			sched_job->dma_info[dma_idx++].idb_type = 1;//IDB_IDB_TYPE_SAVE;
		}
		gf_free(pCmd);
	}

	sched_job->is_signal_job = true;
	sched_job->kmd_va = monitor_fence->kmd_va;
	sched_job->signal_value = signal_info->fence_value;

	//get first gem for syncobj dependency tracking
	first_gem = drm_gem_object_lookup(filp, 1);
	if(first_gem)
	{
		first_gem_ttm = drm_gem_bo_to_tbo(first_gem);
		lisuan_add_sync_fence_to_buffer(first_gem_ttm, &sched_job->base.s_fence->finished, false);
		drm_gem_object_put_compat(first_gem);
	}

	//lisuan_sync_bo_fence(sched_job, &monitor_fence->buffer->tbo, true);
	lisuan_add_sync_fence_to_buffer(&monitor_fence->buffer->tbo, &sched_job->base.s_fence->finished, true);
	lisuan_sched_entity_push_job(&sched_job->base, &context->entity);

#ifdef FAKE_CHIP
	*(volatile unsigned long long *)monitor_fence->kmd_va = signal_info->fence_value;
#endif

	return 0;
}

int lisuan_sync_wait_ioctl(struct drm_device *dev, void *data, struct drm_file *filp)
{
	struct lisuan_device *ls_dev = (struct lisuan_device *)dev->dev_private;
	struct lisuan_file *ls_file = (struct lisuan_file *)filp->driver_priv;

	struct lisuan_sync_signal_info *wait_info = (struct lisuan_sync_signal_info *)data;
	struct gf_job *sched_job = (struct gf_job *)gf_kvcalloc(1, sizeof(struct gf_job), GFP_KERNEL);
	struct lisuan_monitor_fence *monitor_fence;
	struct lisuan_ctx *context;
	struct lisuan_ring *sched;

	mutex_lock(&ls_file->sync_object_lock->lock);
	monitor_fence = (struct lisuan_monitor_fence *)idr_find(&ls_file->sync_object_handles, wait_info->sync_object_handle);
	mutex_unlock(&ls_file->sync_object_lock->lock);
	mutex_lock(&ls_file->context_lock->lock);
	context = (struct lisuan_ctx *)idr_find(&ls_file->context_handles, wait_info->context);
	mutex_unlock(&ls_file->context_lock->lock);

	if (*monitor_fence->kmd_va >= wait_info->fence_value) {
		return 0;
	}

	sched = ls_dev->sched[context->rb_index];
	sched_job->base.sched = &sched->base;
	sched_job->context = context;
	sched_job->dma_count = 0;

	INIT_LIST_HEAD(&sched_job->sync);
	INIT_LIST_HEAD(&sched_job->pt_buf_list);
	lisuan_sched_job_init(&sched_job->base, &context->entity, (void *)0ul);
	sched_job->vm = &ls_file->vm;
	sched_job->uf_sequence = atomic_read(&context->entity.fence_seq);

	//lisuan_sync_bo_fence(sched_job, &monitor_fence->buffer->tbo, false);	
	//lisuan_add_sync_fence_to_buffer(&monitor_fence->buffer->tbo, &sched_job->base.s_fence->finished, false);
	lisuan_sched_entity_push_job(&sched_job->base, &context->entity);

	return 0;
}

int lisuan_sync_destroy_ioctl(struct drm_device *dev, void *data, struct drm_file *filp)
{
	struct lisuan_device *ls_dev = (struct lisuan_device *)dev->dev_private;
	struct lisuan_file *ls_file = (struct lisuan_file *)filp->driver_priv;

	struct lisuan_sync_destroy_info *destroy_info = (struct lisuan_sync_destroy_info *)data;
	struct lisuan_monitor_fence *monitor_fence;
	struct lisuan_gem_bo *buffer;

	int ret;

	mutex_lock(&ls_file->sync_object_lock->lock);
	monitor_fence = (struct lisuan_monitor_fence *)idr_remove(&ls_file->sync_object_handles, destroy_info->sync_object_handle);
	mutex_unlock(&ls_file->sync_object_lock->lock);

	if(!monitor_fence)
		return 0;
		
	buffer = monitor_fence->buffer;

	//free monitor_fence
	gf_kvfree(monitor_fence);

	//free monitor->buffer
	{
		drm_gem_handle_delete(filp, buffer->handle);
	}

	return 0;
}

static struct kmem_cache *lisuan_sync_slab;

struct gf_sync_fence *lisuan_sync_fence(struct dma_fence *f)
{
	struct gf_sync_fence * sync;

	if (!f)
		return NULL;

	sync = kmem_cache_alloc(lisuan_sync_slab, GFP_KERNEL);
	if (!sync)
		return NULL;

	sync->fence = dma_fence_get(f);
	return sync;
}

void lisuan_sync_fence_free(struct gf_sync_fence *sync, bool need_put)
{
	if(sync)
	{  
		if(need_put)
			dma_fence_put(sync->fence);
		kmem_cache_free(lisuan_sync_slab, sync);
	}
}

/*add the sync fence to job->sync list, used for gf_job_dependency*/
void lisuan_sync_bo_fence(struct gf_job *sched_job, struct ttm_buffer_object *tbo, unsigned int bo_operation)
{
#if DRM_VERSION_CODE >= KERNEL_VERSION(5,19,0)
	struct dma_fence **fence = NULL;
	unsigned int fence_count, i;
	struct gf_sync_fence *sync_fence;
	
	if (bo_operation) {
		dma_resv_get_fences(tbo_to_drm_gem_bo(tbo)->resv, DMA_RESV_USAGE_READ, &fence_count, &fence);
	}else{
		dma_resv_get_fences(tbo_to_drm_gem_bo(tbo)->resv, DMA_RESV_USAGE_WRITE, &fence_count, &fence);
	}
		
	for (i = 0; i < fence_count; i++) {
		struct lisuan_sched_fence *sched_fence = container_of(fence[i], struct lisuan_sched_fence, finished);
		struct lisuan_ring *ring = container_of(sched_fence->sched, struct lisuan_ring, base);
		if (!dma_fence_is_signaled(fence[i])) {
			sync_fence = lisuan_sync_fence(fence[i]);
			sync_fence->sched = ring;
			list_add_tail(&sync_fence->list_item, &sched_job->sync);
		}
		dma_fence_put(fence[i]);
	}
	gf_free(fence);
#else
	struct dma_fence *exclusive_fence = NULL;
	struct dma_fence **shared_fence = NULL;
	struct gf_sync_fence *sync_fence;
	unsigned int shared_count, i;

	if (!tbo) {
		gf_info("%s, the tbo is NULL\n", __func__);
		return;
	}

	if (bo_operation) {
#if DRM_VERSION_CODE >= KERNEL_VERSION(5,14,0)
		dma_resv_get_fences(tbo_to_drm_gem_bo(tbo)->resv, &exclusive_fence, &shared_count, &shared_fence);
#elif DRM_VERSION_CODE >= KERNEL_VERSION(5,4,0)
		dma_resv_get_fences_rcu(tbo_to_drm_gem_bo(tbo)->resv, &exclusive_fence, &shared_count, &shared_fence);
#else
		reservation_object_get_fences_rcu(tbo->resv, &exclusive_fence, &shared_count, &shared_fence);
#endif
		for (i = 0; i < shared_count; i++) {
			struct lisuan_sched_fence *sched_fence = container_of(shared_fence[i], struct lisuan_sched_fence, finished);
			struct lisuan_ring *ring = container_of(sched_fence->sched, struct lisuan_ring, base);
			if (!dma_fence_is_signaled(shared_fence[i])) {
				sync_fence = lisuan_sync_fence(shared_fence[i]);
				sync_fence->sched = ring;
				list_add_tail(&sync_fence->list_item, &sched_job->sync);
			}
			dma_fence_put(shared_fence[i]);
		}
		gf_free(shared_fence);
	} else {
#if DRM_VERSION_CODE >= KERNEL_VERSION(5,14,0)
		exclusive_fence = dma_resv_get_excl_unlocked(tbo_to_drm_gem_bo(tbo)->resv);
#elif DRM_VERSION_CODE >= KERNEL_VERSION(5,4,0)
		exclusive_fence = dma_resv_get_excl_rcu(tbo_to_drm_gem_bo(tbo)->resv);
#else
		exclusive_fence = reservation_object_get_excl_rcu(tbo->resv);
#endif
	}

	if (exclusive_fence) {
		struct lisuan_sched_fence *sched_fence = container_of(exclusive_fence, struct lisuan_sched_fence, finished);
		struct lisuan_ring *ring = container_of(sched_fence->sched, struct lisuan_ring, base);
		if (!dma_fence_is_signaled(exclusive_fence)) {
			sync_fence = lisuan_sync_fence(exclusive_fence);
			sync_fence->sched = ring;
			list_add_tail(&sync_fence->list_item, &sched_job->sync);
		}
		dma_fence_put(exclusive_fence);
	}
#endif

}

int lisuan_syncobj_lookup_and_add_to_sync_wait(struct drm_file *filp, struct gf_job *sched_job, 
    struct lisuan_submit_cmd_info *cmdinfo)
{
	struct dma_fence *fence;
	struct gf_sync_fence * sync_fence;
	struct lisuan_ring *ring = container_of(sched_job->base.sched, struct lisuan_ring, base);
	unsigned int * psync_wait_handle = NULL;
	unsigned long long *psync_wait_value = NULL;

	int r = 0, i;

	if( !cmdinfo->wait_objects.syncobj_count)
		return 0;

	psync_wait_handle = (unsigned int*)gf_kvcalloc(cmdinfo->wait_objects.syncobj_count, sizeof(unsigned int), GFP_KERNEL);
	psync_wait_value = (unsigned long long*)gf_kvcalloc(cmdinfo->wait_objects.syncobj_count, sizeof(unsigned long long), GFP_KERNEL);

	r = copy_from_user(psync_wait_handle, u64_to_user_ptr(cmdinfo->wait_objects.syncobj), sizeof(unsigned int) * cmdinfo->wait_objects.syncobj_count);
	r |= copy_from_user(psync_wait_value, u64_to_user_ptr(cmdinfo->wait_objects.points), sizeof(unsigned long long) * cmdinfo->wait_objects.syncobj_count);
	if(r){
		gf_error("%s, copy user data fail, %llx, %llx\n", __func__, cmdinfo->wait_objects.syncobj, cmdinfo->wait_objects.points);
		goto exit;
	}

	for (i = 0; i < cmdinfo->wait_objects.syncobj_count; i++) {
#if DRM_VERSION_CODE >= KERNEL_VERSION(5,0,0)
		r = drm_syncobj_find_fence(filp, psync_wait_handle[i], psync_wait_value[i], 0, &fence);
#elif DRM_VERSION_CODE >= KERNEL_VERSION(4,20,0)
		r = drm_syncobj_find_fence(filp, psync_wait_handle[i], psync_wait_value[i], &fence);
#else
		r = drm_syncobj_find_fence(filp, psync_wait_handle[i], &fence);
#endif
		if (r) {
			gf_error("syncobj %u failed to find fence @ %llu (%d)!\n",
				psync_wait_handle[i], psync_wait_value[i], r);
			goto exit;
		}

		if(fence && !dma_fence_is_signaled(fence)){
			sync_fence = lisuan_sync_fence(fence);
			sync_fence->sched = ring;
			list_add_tail(&sync_fence->list_item, &sched_job->sync);
		}

		dma_fence_put(fence);//find fence has get
	}

exit:
	if(psync_wait_handle)
		gf_kvfree(psync_wait_handle);
	if(psync_wait_value)
		gf_kvfree(psync_wait_value);
	return r;
}

int lisuan_syncobj_lookup_and_add_to_sync_signal(struct drm_file *filp, struct gf_job *sched_job, 
    struct lisuan_submit_cmd_info *cmdinfo)
{
	struct drm_syncobj *syncobj;
	struct dma_fence_chain *chain;
	unsigned int * psync_signal_handle = NULL;
	unsigned long long *psync_signal_value = NULL;
	int r = 0, i;

	if(!cmdinfo->signal_objects.syncobj_count)
		return 0;

	psync_signal_handle = (unsigned int*)gf_kvcalloc(cmdinfo->signal_objects.syncobj_count, sizeof(unsigned int), GFP_KERNEL);
	psync_signal_value = (unsigned long long*)gf_kvcalloc(cmdinfo->signal_objects.syncobj_count, sizeof(unsigned long long), GFP_KERNEL);

	r = copy_from_user(psync_signal_handle, u64_to_user_ptr(cmdinfo->signal_objects.syncobj), sizeof(unsigned int) * cmdinfo->signal_objects.syncobj_count);
	r |= copy_from_user(psync_signal_value, u64_to_user_ptr(cmdinfo->signal_objects.points), sizeof(unsigned long long) * cmdinfo->signal_objects.syncobj_count);
	if(r){
		gf_error("%s, copy user data fail, %llx, %llx\n", __func__, cmdinfo->signal_objects.syncobj, cmdinfo->signal_objects.points);
		goto exit;
	}

	for(i = 0; i < cmdinfo->signal_objects.syncobj_count; i++) {
		syncobj = drm_syncobj_find(filp, psync_signal_handle[i]);
		if(!syncobj) {
			r = -EINVAL;
			gf_error("syncobj 0x%x failed to find (%d)!\n",psync_signal_handle[i], r);
			goto exit;
		}

		if(psync_signal_value[i]) {//timeline 
#if DRM_VERSION_CODE >= KERNEL_VERSION(5,15,0)
			chain = dma_fence_chain_alloc();
#else
			chain = compat_dma_fence_chain_alloc();
#endif
			if (!chain) {
				r = -ENOMEM;
				gf_error("syncobj 0x%x failed to alloc chain (%d)!\n", psync_signal_handle[i], r);
				goto exit;
			}
		}

		if(psync_signal_value[i] && chain)//timeline
			drm_syncobj_add_point(syncobj, chain, &sched_job->base.s_fence->finished, psync_signal_value[i]);
		else//binary
			drm_syncobj_replace_fence(syncobj, &sched_job->base.s_fence->finished);

		drm_syncobj_put(syncobj);
	}

exit:
	if(psync_signal_handle)
		gf_kvfree(psync_signal_handle);
	if(psync_signal_value)
		gf_kvfree((psync_signal_value));

	return r;
}

/*add the sync fence to dma_buffer, used for next time cs_ioctl lisuan_sync_bo_fence used*/
void lisuan_add_sync_fence_to_buffer(struct ttm_buffer_object *tbo, struct dma_fence *fence, unsigned int write)
{
#if DRM_VERSION_CODE >= KERNEL_VERSION(5,19,0)
	dma_resv_lock(tbo->base.resv, NULL);
	if(!dma_resv_reserve_fences(tbo->base.resv, 1))
	{
		if (write) {
			dma_resv_add_fence(tbo->base.resv, fence, DMA_RESV_USAGE_WRITE);
		} else {
			dma_resv_add_fence(tbo->base.resv, fence, DMA_RESV_USAGE_READ);
		}	
	}
	dma_resv_unlock(tbo->base.resv);
#elif DRM_VERSION_CODE >= KERNEL_VERSION(5,4,0)
	dma_resv_lock(tbo->base.resv, NULL);
	if (write) {
		dma_resv_add_excl_fence(tbo->base.resv, fence);
	} else if (dma_resv_reserve_shared(tbo->base.resv, 1) == 0) {
		dma_resv_add_shared_fence(tbo->base.resv, fence);
	}
	dma_resv_unlock(tbo->base.resv);
#else 
	struct drm_gem_object *gem = tbo_to_drm_gem_bo(tbo);
	struct lisuan_gem_bo *buffer = gem_to_gf_bo(gem);

	reservation_object_lock(buffer->tbo.resv, NULL);
	if (write) {
		reservation_object_add_excl_fence(buffer->tbo.resv, fence);
	} else if (reservation_object_reserve_shared(buffer->tbo.resv) == 0) {
		reservation_object_add_shared_fence(buffer->tbo.resv, fence);
	}
	reservation_object_unlock(buffer->tbo.resv);
#endif

}

void lisuan_init_ring_fence_thread(struct lisuan_device *ls_dev)
{
	int i = 0;
	for (i = 0; i < ls_dev->rb_count; i++) {
		spin_lock_init(&ls_dev->ring_fence_lock[i]);
		INIT_LIST_HEAD(&ls_dev->ring_fence_list[i]);
	}

	init_waitqueue_head(&ls_dev->ring_fence_wake_up_task);

	snprintf(ls_dev->ring_fence_thread_name, 66, "gf_fence");
	ls_dev->ring_fence_signal_thread = kthread_run(lisuan_ring_fence_thread_main, ls_dev, ls_dev->ring_fence_thread_name);
}

void lisuan_fini_ring_fence_thread(struct lisuan_device *ls_dev)
{
	if (ls_dev->ring_fence_signal_thread)
		kthread_stop(ls_dev->ring_fence_signal_thread);
}

int lisuan_ring_fence_thread_main(void *data)
{
	struct lisuan_device *ls_dev = (struct lisuan_device *)data;
	struct lisuan_ring_fence *fence_head;
	int i = 0, ret;

	/**************************************************
	 * for linux-5.11.0 or high version we should use sched_set_fifo_low instead the sched_setscheduler
     *struct sched_param sparam = {.sched_priority = 1 };
	 *sched_setscheduler(current, SCHED_FIFO, &sparam);
	 **************************************************/
#if DRM_VERSION_CODE >= KERNEL_VERSION(5,0,0)
	sched_set_fifo_low(current);
#endif

	while (!kthread_should_stop()) {
		wait_event_interruptible_timeout(ls_dev->ring_fence_wake_up_task,
						 kthread_should_stop() || lisuan_any_ring_fence_signaled(ls_dev), msecs_to_jiffies(500));

		for (i = 0; i < ls_dev->rb_count; i++) {
			while (ls_dev->ring_fence_count[i] > 0) {
				spin_lock(&ls_dev->ring_fence_lock[i]);
				fence_head = list_first_entry(&ls_dev->ring_fence_list[i], struct lisuan_ring_fence, list_in_device);
				if (fence_head && dma_fence_is_signaled(&fence_head->base)) {
					list_del(&fence_head->list_in_device);
					ls_dev->ring_fence_count[i]--;
					dma_fence_signal(&fence_head->base);
					dma_fence_put(&fence_head->base);
					spin_unlock(&ls_dev->ring_fence_lock[i]);
				} else {
					spin_unlock(&ls_dev->ring_fence_lock[i]);
					break;
				}
			}
		}

	}

	return 0;
}

int lisuan_any_ring_fence_signaled(struct lisuan_device *ls_dev)
{
	int i = 0;

	for (i = 0; i < ls_dev->rb_count; i++) {
		if (ls_dev->ring_fence_count[i] > 0) {
			struct lisuan_ring_fence *fence_head;
			spin_lock(&ls_dev->ring_fence_lock[i]);
			fence_head = list_first_entry(&ls_dev->ring_fence_list[i], struct lisuan_ring_fence, list_in_device);
			if (fence_head && dma_fence_is_signaled(&fence_head->base)) {
				spin_unlock(&ls_dev->ring_fence_lock[i]);
				return true;
			}
			spin_unlock(&ls_dev->ring_fence_lock[i]);
		}
	}

	return false;
}

int lisuan_sync_init(void)
{
	lisuan_sync_slab = kmem_cache_create(
		"lisuan_sync", sizeof(struct gf_sync_fence), 0,
		SLAB_HWCACHE_ALIGN, NULL);
	if (!lisuan_sync_slab)
		return -ENOMEM;

	return 0;
}

void lisuan_sync_fini(void)
{
	kmem_cache_destroy(lisuan_sync_slab);
}

int lisuan_ring_fence_init(void)
{
	lisuan_ring_fence_slab = kmem_cache_create(
		"lisuan_ring_fence", sizeof(struct lisuan_ring_fence), 0,
		SLAB_HWCACHE_ALIGN, NULL);
	if (!lisuan_ring_fence_slab)
		return -ENOMEM;

	return 0;
}

void lisuan_ring_fence_fini(void)
{
	kmem_cache_destroy(lisuan_ring_fence_slab);
}

MODULE_LICENSE("GPL");
