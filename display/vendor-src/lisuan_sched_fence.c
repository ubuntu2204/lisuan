/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */


#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/wait.h>

#include "lisuan_scheduler.h"
#include "gf_os_interface.h"

static struct kmem_cache *sched_fence_slab=NULL;

int lisuan_sched_fence_slab_init(void)
{
	sched_fence_slab = kmem_cache_create("lisuan_sched_fence", sizeof(struct lisuan_sched_fence), 0, SLAB_HWCACHE_ALIGN, NULL);
	if (!sched_fence_slab)
		return -ENOMEM;

	return 0;
}

void lisuan_sched_fence_slab_fini(void)
{
	rcu_barrier();
	kmem_cache_destroy(sched_fence_slab);
}

void lisuan_sched_fence_scheduled(struct lisuan_sched_fence *fence)
{
	dma_fence_signal(&fence->scheduled);
}

void lisuan_sched_fence_finished(struct lisuan_sched_fence *fence)
{
	dma_fence_signal(&fence->finished);
}

static const char *lisuan_sched_fence_get_driver_name(struct dma_fence *fence)
{
	return "gf_sched";
}

static const char *lisuan_sched_fence_get_timeline_name(struct dma_fence *f)
{
	struct lisuan_sched_fence *fence = to_gf_sched_fence(f);
	return (const char *)fence->sched->name;
}

/**
 * lisuan_sched_fence_free - free up the fence memory
 *
 * @rcu: RCU callback head
 *
 * Free up the fence memory after the RCU grace period.
 */
static void lisuan_sched_fence_free(struct rcu_head *rcu)
{
	struct dma_fence *f = container_of(rcu, struct dma_fence, rcu);
	struct lisuan_sched_fence *fence = to_gf_sched_fence(f);

	kmem_cache_free(sched_fence_slab, fence);
}

/**
 * lisuan_sched_fence_release_scheduled - callback that fence can be freed
 *
 * @fence: fence
 *
 * This function is called when the reference count becomes zero.
 * It just RCU schedules freeing up the fence.
 */
static void lisuan_sched_fence_release_scheduled(struct dma_fence *f)
{
	struct lisuan_sched_fence *fence = to_gf_sched_fence(f);

	dma_fence_info("%s:%d finish fence:%p, ref:%d\n", __func__, __LINE__, &fence->finished, 
		fence->finished.refcount.refcount.refs.counter);
	dma_fence_put(fence->parent);
	call_rcu(&fence->finished.rcu, lisuan_sched_fence_free);
}

/**
 * gf_sched_fence_release_finished - drop extra reference
 *
 * @f: fence
 *
 * Drop the extra reference from the scheduled fence to the base fence.
 */
static void lisuan_sched_fence_release_finished(struct dma_fence *f)
{
	struct lisuan_sched_fence *fence = to_gf_sched_fence(f);
	dma_fence_info("%s:%d finish fence:%p, ref:%d\n", __func__, __LINE__, &fence->finished, 
		fence->finished.refcount.refcount.refs.counter);

	dma_fence_put(&fence->scheduled);
}

const struct dma_fence_ops lisuan_sched_fence_ops_scheduled = {
	.get_driver_name = lisuan_sched_fence_get_driver_name,
	.get_timeline_name = lisuan_sched_fence_get_timeline_name,
	.release = lisuan_sched_fence_release_scheduled,
};

const struct dma_fence_ops lisuan_sched_fence_ops_finished = {
	.get_driver_name = lisuan_sched_fence_get_driver_name,
	.get_timeline_name = lisuan_sched_fence_get_timeline_name,
	.release = lisuan_sched_fence_release_finished,
};

struct lisuan_sched_fence *to_gf_sched_fence(struct dma_fence *f)
{
	if (f->ops == &lisuan_sched_fence_ops_scheduled)
		return container_of(f, struct lisuan_sched_fence, scheduled);

	if (f->ops == &lisuan_sched_fence_ops_finished)
		return container_of(f, struct lisuan_sched_fence, finished);

	return NULL;
}

struct lisuan_sched_fence *lisuan_sched_fence_create(struct lisuan_sched_entity *entity, void *owner)
{
	struct lisuan_sched_fence *fence = NULL;
	unsigned seq;

	if(!sched_fence_slab)
	{
		gf_error("the slab not ready, firstly call the lisuan_sched_fence_slab_init\n");
		return NULL;
	}
	fence = kmem_cache_zalloc(sched_fence_slab, GFP_KERNEL);
	if (fence == NULL)
		return NULL;

	fence->owner = owner;
	fence->sched = entity->rq->sched;
	spin_lock_init(&fence->lock);

	seq = atomic_inc_return(&entity->fence_seq);
	dma_fence_init(&fence->scheduled, &lisuan_sched_fence_ops_scheduled, &fence->lock, entity->fence_context, seq);
	dma_fence_init(&fence->finished, &lisuan_sched_fence_ops_finished, &fence->lock, entity->fence_context + 1, seq);

	return fence;
}
