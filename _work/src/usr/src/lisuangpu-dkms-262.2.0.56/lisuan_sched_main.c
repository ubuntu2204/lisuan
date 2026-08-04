/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */


#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <uapi/linux/sched/types.h>

#include <drm/drm_print.h>
#include <drm/spsc_queue.h>
#include <linux/module.h>

#include "lisuan_scheduler.h"
#include "lisuan_ring.h"
#include "gf_job.h"
#include "lisuan_trace.h"
#include "gf_kmd_event.h"

#define to_gf_sched_job(sched_job)		\
		container_of((sched_job), struct lisuan_sched_job, queue_node)


/**
 * lisuan_sched_rq_init - initialize a given run queue struct
 *
 * @rq: scheduler run queue
 *
 * Initializes a scheduler runqueue.
 */
static void lisuan_sched_rq_init(struct lisuan_scheduler *sched, struct lisuan_sched_rq *rq)
{
	spin_lock_init(&rq->lock);
	INIT_LIST_HEAD(&rq->entities);
	rq->current_entity = NULL;
	rq->sched = sched;
}

/**
 * lisuan_sched_rq_add_entity - add an entity
 *
 * @rq: scheduler run queue
 * @entity: scheduler entity
 *
 * Adds a scheduler entity to the run queue.
 */
void lisuan_sched_rq_add_entity(struct lisuan_sched_rq *rq, struct lisuan_sched_entity *entity)
{
	if (!list_empty(&entity->list))
		return;
	spin_lock(&rq->lock);
	list_add_tail(&entity->list, &rq->entities);
	spin_unlock(&rq->lock);
}

/**
 * lisuan_sched_rq_remove_entity - remove an entity
 *
 * @rq: scheduler run queue
 * @entity: scheduler entity
 *
 * Removes a scheduler entity from the run queue.
 */
void lisuan_sched_rq_remove_entity(struct lisuan_sched_rq *rq, struct lisuan_sched_entity *entity)
{
	if (list_empty(&entity->list))
		return;
	spin_lock(&rq->lock);
	list_del_init(&entity->list);
	if (rq->current_entity == entity)
		rq->current_entity = NULL;
	spin_unlock(&rq->lock);
}

/**
 * lisuan_sched_rq_select_entity - Select an entity which could provide a job to run
 *
 * @rq: scheduler run queue to check.
 *
 * Try to find a ready entity, returns NULL if none found.
 */
static struct lisuan_sched_entity *lisuan_sched_rq_select_entity(struct lisuan_sched_rq *rq)
{
	struct lisuan_sched_entity *entity;

	spin_lock(&rq->lock);

	entity = rq->current_entity;
	if (entity) {
		list_for_each_entry_continue(entity, &rq->entities, list) {
			if (lisuan_sched_entity_is_ready(entity)) {
				rq->current_entity = entity;				
				reinit_completion(&entity->entity_idle);
				spin_unlock(&rq->lock);
				return entity;
			}
		}
	}

	list_for_each_entry(entity, &rq->entities, list) {

		if (lisuan_sched_entity_is_ready(entity)) {
			rq->current_entity = entity;
			reinit_completion(&entity->entity_idle);
			spin_unlock(&rq->lock);
			return entity;
		}

		if (entity == rq->current_entity)
			break;
	}

	spin_unlock(&rq->lock);

	return NULL;
}

/**
 * lisuan_sched_dependency_optimized
 *
 * @fence: the dependency fence
 * @entity: the entity which depends on the above fence
 *
 * Returns true if the dependency can be optimized and false otherwise
 */
bool lisuan_sched_dependency_optimized(struct dma_fence *fence, struct lisuan_sched_entity *entity)
{
	struct lisuan_scheduler *sched = entity->rq->sched;
	struct lisuan_sched_fence *s_fence;

	if (!fence || dma_fence_is_signaled(fence))
		return false;
	if (fence->context == entity->fence_context)
		return true;
	s_fence = to_gf_sched_fence(fence);
	if (s_fence && s_fence->sched == sched)
		return true;

	return false;
}

/**
 * lisuan_sched_start_timeout - start timeout for reset worker
 *
 * @sched: scheduler instance to start the worker for
 *
 * Start the timeout for the given scheduler.
 */
void lisuan_sched_start_timeout(struct lisuan_scheduler *sched)
{
	if (sched->timeout != MAX_SCHEDULE_TIMEOUT && !list_empty(&sched->ring_mirror_list))
		schedule_delayed_work(&sched->work_tdr, sched->timeout);
}

/**
 * lisuan_sched_fault - immediately start timeout handler
 *
 * @sched: scheduler where the timeout handling should be started.
 *
 * Start timeout handling immediately when the driver detects a hardware fault.
 */
void lisuan_sched_fault(struct lisuan_scheduler *sched)
{
	mod_delayed_work(system_wq, &sched->work_tdr, 0);
}

/**
 * lisuan_sched_suspend_timeout - Suspend scheduler job timeout
 *
 * @sched: scheduler instance for which to suspend the timeout
 *
 * Suspend the delayed work timeout for the scheduler. This is done by
 * modifying the delayed work timeout to an arbitrary large value,
 * MAX_SCHEDULE_TIMEOUT in this case. Note that this function can be
 * called from an IRQ context.
 *
 * Returns the timeout remaining
 *
 */
unsigned long lisuan_sched_suspend_timeout(struct lisuan_scheduler *sched)
{
	unsigned long sched_timeout, now = jiffies;

	sched_timeout = sched->work_tdr.timer.expires;

	/*
	 * Modify the timeout to an arbitrarily large value. This also prevents
	 * the timeout to be restarted when new submissions arrive
	 */
	if (mod_delayed_work(system_wq, &sched->work_tdr, MAX_SCHEDULE_TIMEOUT)
	    && time_after(sched_timeout, now))
		return sched_timeout - now;
	else
		return sched->timeout;
}

/**
 * lisuan_sched_resume_timeout - Resume scheduler job timeout
 *
 * @sched: scheduler instance for which to resume the timeout
 * @remaining: remaining timeout
 *
 * Resume the delayed work timeout for the scheduler. Note that
 * this function can be called from an IRQ context.
 */
void lisuan_sched_resume_timeout(struct lisuan_scheduler *sched, unsigned long remaining)
{
	unsigned long flags;

	spin_lock_irqsave(&sched->job_list_lock, flags);

	if (list_empty(&sched->ring_mirror_list))
		cancel_delayed_work(&sched->work_tdr);
	else
		mod_delayed_work(system_wq, &sched->work_tdr, remaining);

	spin_unlock_irqrestore(&sched->job_list_lock, flags);
}

void lisuan_sched_job_begin(struct lisuan_sched_job *s_job)
{
	struct lisuan_scheduler *sched = s_job->sched;
	unsigned long flags;

	spin_lock_irqsave(&sched->job_list_lock, flags);
	list_add_tail(&s_job->node, &sched->ring_mirror_list);
	lisuan_sched_start_timeout(sched);
	spin_unlock_irqrestore(&sched->job_list_lock, flags);
}

static void lisuan_sched_job_timedout(struct work_struct *work)
{
	struct lisuan_scheduler *sched;
	struct lisuan_sched_job *job;
	unsigned long flags;

	sched = container_of(work, struct lisuan_scheduler, work_tdr.work);

	/* Protects against concurrent deletion in drm_sched_get_cleanup_job */
	spin_lock_irqsave(&sched->job_list_lock, flags);
	job = list_first_entry_or_null(&sched->ring_mirror_list, struct lisuan_sched_job, node);

	if (job) {
		/*
		 * Remove the bad job so it cannot be freed by concurrent
		 * drm_sched_cleanup_jobs. It will be reinserted back after sched->thread
		 * is parked at which point it's safe.
		 */
		list_del_init(&job->node);
		spin_unlock_irqrestore(&sched->job_list_lock, flags);

		job->sched->ops->timedout_job(job);

		/*
		 * Guilty job did complete and hence needs to be manually removed
		 * See drm_sched_stop doc.
		 */
		if (sched->free_guilty) {
			job->sched->ops->free_job(job);
			sched->free_guilty = false;
		}
	} else {
		spin_unlock_irqrestore(&sched->job_list_lock, flags);
	}

	spin_lock_irqsave(&sched->job_list_lock, flags);
	lisuan_sched_start_timeout(sched);
	spin_unlock_irqrestore(&sched->job_list_lock, flags);
}

 /**
  * lisuan_sched_increase_karma - Update sched_entity guilty flag
  *
  * @bad: The job guilty of time out
  *
  * Increment on every hang caused by the 'bad' job. If this exceeds the hang
  * limit of the scheduler then the respective sched entity is marked guilty and
  * jobs from it will not be scheduled further
  */
void lisuan_sched_increase_karma(struct lisuan_sched_job *bad)
{
	int i;
	struct lisuan_sched_entity *tmp;
	struct lisuan_sched_entity *entity;
	struct lisuan_scheduler *sched = bad->sched;

	/* don't increase @bad's karma if it's from KERNEL RQ,
	 * because sometimes GPU hang would cause kernel jobs (like VM updating jobs)
	 * corrupt but keep in mind that kernel jobs always considered good.
	 */
	if (bad->s_priority != DRM_SCHED_PRIORITY_NORMAL) {
		atomic_inc(&bad->karma);
		for (i = DRM_SCHED_PRIORITY_NORMAL; i < DRM_SCHED_PRIORITY_MAX; i++) {
			struct lisuan_sched_rq *rq = &sched->sched_rq[i];

			spin_lock(&rq->lock);
			list_for_each_entry_safe(entity, tmp, &rq->entities, list) {
				if (bad->s_fence->scheduled.context == entity->fence_context) {
					if (atomic_read(&bad->karma) > bad->sched->hang_limit)
						if (entity->guilty)
							atomic_set(entity->guilty, 1);
					break;
				}
			}
			spin_unlock(&rq->lock);
			if (&entity->list != &rq->entities)
				break;
		}
	}
}

/**
 * lisuan_sched_stop - stop the scheduler
 *
 * @sched: scheduler instance
 * @bad: job which caused the time out
 *
 * Stop the scheduler and also removes and frees all completed jobs.
 * Note: bad job will not be freed as it might be used later and so it's
 * callers responsibility to release it manually if it's not part of the
 * mirror list any more.
 *
 */
void lisuan_sched_stop(struct lisuan_scheduler *sched, struct lisuan_sched_job *bad)
{
	struct lisuan_sched_job *s_job, *tmp;
	unsigned long flags;

	kthread_park(sched->thread);

	/*
	 * Reinsert back the bad job here - now it's safe as
	 * drm_sched_get_cleanup_job cannot race against us and release the
	 * bad job at this point - we parked (waited for) any in progress
	 * (earlier) cleanups and drm_sched_get_cleanup_job will not be called
	 * now until the scheduler thread is unparked.
	 */
	if (bad && bad->sched == sched)
		/*
		 * Add at the head of the queue to reflect it was the earliest
		 * job extracted.
		 */
		list_add(&bad->node, &sched->ring_mirror_list);

	/*
	 * Iterate the job list from later to  earlier one and either deactive
	 * their HW callbacks or remove them from mirror list if they already
	 * signaled.
	 * This iteration is thread safe as sched thread is stopped.
	 */
	list_for_each_entry_safe_reverse(s_job, tmp, &sched->ring_mirror_list, node) {
		if (s_job->s_fence->parent && dma_fence_remove_callback(s_job->s_fence->parent, &s_job->cb)) {
			atomic_dec(&sched->hw_rq_count);
		} else {
			/*
			 * remove job from ring_mirror_list.
			 * Locking here is for concurrent resume timeout
			 */
			spin_lock(&sched->job_list_lock);
			list_del_init(&s_job->node);
			spin_unlock(&sched->job_list_lock);

			/*
			 * Wait for job's HW fence callback to finish using s_job
			 * before releasing it.
			 *
			 * Job is still alive so fence refcount at least 1
			 */
			dma_fence_wait(&s_job->s_fence->finished, false);

			/*
			 * We must keep bad job alive for later use during
			 * recovery by some of the drivers but leave a hint
			 * that the guilty job must be released.
			 */
			if (bad != s_job)
				sched->ops->free_job(s_job);
			else
				sched->free_guilty = true;
		}
	}

	/*
	 * Stop pending timer in flight as we rearm it in  drm_sched_start. This
	 * avoids the pending timeout work in progress to fire right away after
	 * this TDR finished and before the newly restarted jobs had a
	 * chance to complete.
	 */
	cancel_delayed_work(&sched->work_tdr);
}

/**
 * gf_sched_job_recovery - recover jobs after a reset
 *
 * @sched: scheduler instance
 * @full_recovery: proceed with complete sched restart
 *
 */
void lisuan_sched_start(struct lisuan_scheduler *sched, bool full_recovery)
{
	struct lisuan_sched_job *s_job, *tmp;
	unsigned long flags;
	int r;

	/*
	 * Locking the list is not required here as the sched thread is parked
	 * so no new jobs are being inserted or removed. Also concurrent
	 * GPU recovers can't run in parallel.
	 */
	list_for_each_entry_safe(s_job, tmp, &sched->ring_mirror_list, node) {
		struct dma_fence *fence = s_job->s_fence->parent;

		atomic_inc(&sched->hw_rq_count);

		if (!full_recovery)
			continue;

		if (fence) {
			r = dma_fence_add_callback(fence, &s_job->cb, lisuan_sched_process_job);
			if (r == -ENOENT)
				lisuan_sched_process_job(fence, &s_job->cb);
			else if (r)
				DRM_ERROR("fence add callback failed (%d)\n", r);
		} else
			lisuan_sched_process_job(NULL, &s_job->cb);
	}

	if (full_recovery) {
		spin_lock_irqsave(&sched->job_list_lock, flags);
		lisuan_sched_start_timeout(sched);
		spin_unlock_irqrestore(&sched->job_list_lock, flags);
	}

	kthread_unpark(sched->thread);
}

/**
 * lisuan_sched_resubmit_jobs - helper to relunch job from mirror ring list
 *
 * @sched: scheduler instance
 *
 */
void lisuan_sched_resubmit_jobs(struct lisuan_scheduler *sched)
{
	struct lisuan_sched_job *s_job, *tmp;
	uint64_t guilty_context;
	bool found_guilty = false;
	struct dma_fence *fence;

	list_for_each_entry_safe(s_job, tmp, &sched->ring_mirror_list, node) {
		struct lisuan_sched_fence *s_fence = s_job->s_fence;

		if (!found_guilty && atomic_read(&s_job->karma) > sched->hang_limit) {
			found_guilty = true;
			guilty_context = s_job->s_fence->scheduled.context;
		}

		if (found_guilty && s_job->s_fence->scheduled.context == guilty_context)
			dma_fence_set_error(&s_fence->finished, -ECANCELED);

		dma_fence_put(s_job->s_fence->parent);
		trace_lisuan_sched_resubmit_jobs(s_job);
		fence = sched->ops->run_job(s_job);

		if (IS_ERR_OR_NULL(fence)) {
			if (IS_ERR(fence))
				dma_fence_set_error(&s_fence->finished, PTR_ERR(fence));

			s_job->s_fence->parent = NULL;
		} else {
			s_job->s_fence->parent = fence;
		}

	}
}

/**
 * lisuan_sched_job_init - init a scheduler job
 *
 * @job: scheduler job to init
 * @entity: scheduler entity to use
 * @owner: job owner for debugging
 *
 * Refer to drm_sched_entity_push_job() documentation
 * for locking considerations.
 *
 * Returns 0 for success, negative error code otherwise.
 */
int lisuan_sched_job_init(struct lisuan_sched_job *sched_job, struct lisuan_sched_entity *entity, void *owner)
{
	struct lisuan_scheduler *sched;
	struct gf_job *job = (struct gf_job *)sched_job;

	lisuan_sched_entity_select_rq(entity);
	if (!entity->rq)
		return -ENOENT;

	sched = entity->rq->sched;

	sched_job->sched = sched;
	sched_job->entity = entity;
	sched_job->s_priority = entity->rq - sched->sched_rq;
	sched_job->s_fence = lisuan_sched_fence_create(entity, owner);
	if (!sched_job->s_fence)
		return -ENOMEM;
	sched_job->id = atomic64_inc_return(&sched->job_id_count);
	job->tgid = job->context->tgid;
	job->pid = job->context->pid;
	job_info("ini job:%d from tgid-pid:%d-%d\n", sched_job->id, job->tgid, job->pid);
	dma_fence_info("%s:%d finish fence:%p, ref:%d\n", __func__, __LINE__, &sched_job->s_fence->finished, 
		sched_job->s_fence->finished.refcount.refcount.refs.counter);

	INIT_LIST_HEAD(&sched_job->node);
	INIT_LIST_HEAD(&sched_job->prepare_node);
	trace_lisuan_sched_job_init(sched_job);
	return 0;
}

void lisuan_sched_show_ring_jobs(struct lisuan_scheduler *sched)
{
	struct lisuan_sched_job *job, *tmp;
	unsigned long flags;

	spin_lock_irqsave(&sched->job_list_lock, flags);
	list_for_each_entry_safe(job, tmp, &sched->ring_mirror_list, node){
		sched->ops->show_job_info(job);
	}
	spin_unlock_irqrestore(&sched->job_list_lock, flags);
}

void lisuan_sched_dump_ring_jobs(struct lisuan_scheduler *sched)
{
	struct lisuan_sched_job *job, *tmp;
	unsigned long flags;

	//spin_lock_irqsave(&sched->job_list_lock, flags);
	list_for_each_entry_safe(job, tmp, &sched->ring_mirror_list, node){
		sched->ops->dump_job(job);
	}
	//spin_unlock_irqrestore(&sched->job_list_lock, flags);
}

void lisuan_sched_rq_iter_entity_top_status(struct lisuan_sched_rq *rq)
{
	struct lisuan_sched_entity *entity,*tmp;

	spin_lock(&rq->lock);

	list_for_each_entry_safe(entity, tmp, &rq->entities, list) 
	{
		struct gf_job *job;
		struct lisuan_sched_job *sched_job;
		struct lisuan_sched_fence *s_fence;

		if(entity->dependency){
			s_fence = to_gf_sched_fence(entity->dependency);
			gf_info("entity %p has dep, belong sched:%p signaled:%d",
				entity, s_fence?s_fence->sched:0, dma_fence_is_signaled(entity->dependency));
		}

		if (spsc_queue_peek(&entity->job_queue) == NULL)
			continue;
		sched_job = to_gf_sched_job(spsc_queue_peek(&entity->job_queue));
		job = container_of(sched_job, struct gf_job, base);

		gf_info("top job %d on tgid:%d entity:%p, bo cnt:%d prepare done:%d, is signal job:%d, is kcmd:%d, dep empty:%d\n", 
			job->base.id, job->context->tgid, entity, job->bo_count, READ_ONCE(job->prepare_ready.done),
			job->is_signal_job, (!!job->kcommand),list_empty(&job->sync));
	}

	spin_unlock(&rq->lock);
}

void lisuan_sched_show_rq_top_status(struct lisuan_scheduler *sched)
{
	int i;

	gf_info("=====================entity top ========================\n");
	/* Kernel run queue has higher priority than normal run queue */
	for (i = DRM_SCHED_PRIORITY_MAX - 1; i >= DRM_SCHED_PRIORITY_NORMAL; i--) {
		lisuan_sched_rq_iter_entity_top_status(&sched->sched_rq[i]);
	}
}

/**
 * lisuan_sched_job_cleanup - clean up scheduler job resources
 *
 * @job: scheduler job to clean up
 */
void lisuan_sched_job_cleanup(struct lisuan_sched_job *job)
{
	dma_fence_info("%s:%d finish fence:%p, ref:%d\n", __func__, __LINE__, &job->s_fence->finished, 
		job->s_fence->finished.refcount.refcount.refs.counter-1);

	dma_fence_put(&job->s_fence->finished);
	job->s_fence = NULL;
}

/**
 * lisuan_sched_ready - is the scheduler ready
 *
 * @sched: scheduler instance
 *
 * Return true if we can push more jobs to the hw, otherwise false.
 */
static bool lisuan_sched_ready(struct lisuan_scheduler *sched)
{
	return atomic_read(&sched->hw_rq_count) < sched->hw_submission_limit;
}

/**
 * lisuan_sched_wakeup - Wake up the scheduler when it is ready
 *
 * @sched: scheduler instance
 *
 */
void lisuan_sched_wakeup(struct lisuan_scheduler *sched)
{
	if (lisuan_sched_ready(sched))
		wake_up_interruptible(&sched->wake_up_worker);
}

/**
 * lisuan_sched_select_entity - Select next entity to process
 *
 * @sched: scheduler instance
 *
 * Returns the entity to process or NULL if none are found.
 */
static struct lisuan_sched_entity *lisuan_sched_select_entity(struct lisuan_scheduler *sched)
{
	struct lisuan_sched_entity *entity;
	int i;

	if (!lisuan_sched_ready(sched))
		return NULL;

	/* Kernel run queue has higher priority than normal run queue */
	for (i = DRM_SCHED_PRIORITY_MAX - 1; i >= DRM_SCHED_PRIORITY_NORMAL; i--) {
		entity = lisuan_sched_rq_select_entity(&sched->sched_rq[i]);
		if (entity)
			break;
	}

	return entity;
}

/**
 * lisuan_sched_process_job - process a job
 *
 * @f: fence
 * @cb: fence callbacks
 *
 * Called after job has finished execution.
 */
void lisuan_sched_process_job(struct dma_fence *f, struct dma_fence_cb *cb)
{
	struct lisuan_sched_job *s_job = container_of(cb, struct lisuan_sched_job, cb);
	struct lisuan_sched_fence *s_fence = s_job->s_fence;
	struct lisuan_scheduler *sched = s_fence->sched;
	struct gf_job *gf_job = (struct gf_job *)s_job;
	struct lisuan_device *ls_device = gf_job->context->ls_dev;

	atomic_dec(&sched->hw_rq_count);
	atomic_dec(&sched->num_jobs);

	trace_lisuan_sched_process_job(s_job);
	job_info("finish exec job:%d from tgid-pid:%d-%d, %d-%d, procid:%d, rb:%d, vm:%d-%d\n", 
		s_job->id, gf_job->tgid, gf_job->pid, 
		s_fence->scheduled.refcount.refcount.refs.counter, s_fence->finished.refcount.refcount.refs.counter, 
		gf_job->procid,  gf_job->ring_fence->rb_index,
		gf_job->vm?gf_job->vm->file->tgid:0,
		gf_job->vm?gf_job->vm->file->pid:0);

	gf_record_evt_finish(ls_device,gf_job->ring_fence->rb_index, gf_job->ring_fence->ring_fence_value);

	gf_gpuusage_complete(ls_device, gf_job->ring_fence->rb_index, gf_job->ring_fence->ring_fence_value);

	dma_fence_get(&s_fence->finished);
	lisuan_sched_fence_finished(s_fence);
	dma_fence_put(&s_fence->finished);

	wake_up_interruptible(&sched->wake_up_worker);
}

/**
 * lisuan_sched_get_cleanup_job - fetch the next finished job to be destroyed
 *
 * @sched: scheduler instance
 *
 * Returns the next finished job from the mirror list (if there is one)
 * ready for it to be destroyed.
 */
static struct lisuan_sched_job *lisuan_sched_get_cleanup_job(struct lisuan_scheduler *sched)
{
	struct lisuan_sched_job *job;
	unsigned long flags;

	/* Don't destroy jobs while the timeout worker is running */
	if (sched->timeout != MAX_SCHEDULE_TIMEOUT && !cancel_delayed_work(&sched->work_tdr))
		return NULL;

	spin_lock_irqsave(&sched->job_list_lock, flags);

	job = list_first_entry_or_null(&sched->ring_mirror_list, struct lisuan_sched_job, node);

	if (job && dma_fence_is_signaled(&job->s_fence->finished)) {
		/* remove job from ring_mirror_list */
		list_del_init(&job->node);
	} else {
		job = NULL;
		/* queue timeout for next job */
		lisuan_sched_start_timeout(sched);
	}

	spin_unlock_irqrestore(&sched->job_list_lock, flags);

	return job;
}

/**
 * lisuan_sched_blocked - check if the scheduler is blocked
 *
 * @sched: scheduler instance
 *
 * Returns true if blocked, otherwise false.
 */
static bool lisuan_sched_blocked(struct lisuan_scheduler *sched)
{
	if (kthread_should_park()) {
		kthread_parkme();
		return true;
	}

	return false;
}

/**
 * lisuan_sched_main - main scheduler thread
 *
 * @param: scheduler instance
 *
 * Returns 0.
 */
static int lisuan_sched_main(void *param)
{
	struct sched_param sparam = {.sched_priority = 1 };
	struct lisuan_scheduler *sched = (struct lisuan_scheduler *)param;
	int r;
	struct lisuan_ring *ring;
	struct gf_job *job;

	/**************************************************
	 * for linux-5.11.0 or high version we should use sched_set_fifo_low instead the sched_setscheduler
	 *sched_setscheduler(current, SCHED_FIFO, &sparam);
	 **************************************************/
#if DRM_VERSION_CODE >= KERNEL_VERSION(5,0,0)
	sched_set_fifo_low(current);
#endif

	while (!kthread_should_stop()) {
		struct lisuan_sched_entity *entity = NULL;
		struct lisuan_sched_fence *s_fence;
		struct lisuan_sched_job *sched_job;
		struct dma_fence *fence;
		struct lisuan_sched_job *cleanup_job = NULL;

		wait_event_interruptible(sched->wake_up_worker,
					 (cleanup_job = lisuan_sched_get_cleanup_job(sched)) ||
					 (!lisuan_sched_blocked(sched) && (entity = lisuan_sched_select_entity(sched))) || kthread_should_stop());

		if (cleanup_job) {
			sched->ops->free_job(cleanup_job);
			/* queue timeout for next job */
			lisuan_sched_start_timeout(sched);
		}

		if (!entity)
			continue;

		sched_job = lisuan_sched_entity_pop_job(entity);
		if (!sched_job){
			complete(&entity->entity_idle);
			continue;
		}

		s_fence = sched_job->s_fence;

		atomic_inc(&sched->hw_rq_count);
		lisuan_sched_job_begin(sched_job);

		trace_lisuan_sched_run_job(sched_job, entity);
		fence = sched->ops->run_job(sched_job);
		complete(&entity->entity_idle);

		lisuan_sched_fence_scheduled(s_fence);

		if (!IS_ERR_OR_NULL(fence)) {
			s_fence->parent = dma_fence_get(fence);
			r = dma_fence_add_callback(fence, &sched_job->cb, lisuan_sched_process_job);
			if (r == -ENOENT)
				lisuan_sched_process_job(fence, &sched_job->cb);
			else if (r)
				DRM_ERROR("fence add callback failed (%d)\n", r);
			dma_fence_put(fence);
		} else {
			if (IS_ERR(fence))
				dma_fence_set_error(&s_fence->finished, PTR_ERR(fence));

			lisuan_sched_process_job(NULL, &sched_job->cb);
		}

		wake_up(&sched->job_scheduled);
#ifdef FAKE_CHIP
		ring = container_of(sched_job->sched, struct lisuan_ring, base);
		job = container_of(sched_job, struct gf_job, base);
		atomic64_set(&ring->returned_fence_id,job->ring_fence->ring_fence_value);
		wake_up_interruptible(&ring->device->ring_fence_wake_up_task);
#endif
	}
	return 0;
}

/**
 * lisuan_sched_init - Init a gpu scheduler instance
 *
 * @sched: scheduler instance
 * @ops: backend operations for this scheduler
 * @hw_submission: number of hw submissions that can be in flight
 * @hang_limit: number of times to allow a job to hang before dropping it
 * @timeout: timeout value in jiffies for the scheduler
 * @name: name used for debugging
 *
 * Return 0 on success, otherwise error code.
 */
int lisuan_sched_init(struct lisuan_scheduler *sched,
		     const struct lisuan_sched_backend_ops *ops, unsigned hw_submission, unsigned hang_limit, long timeout, const char *name)
{
	int i, ret;
	sched->ops = ops;
	sched->hw_submission_limit = hw_submission;
	sched->name = name;
	sched->timeout = timeout;
	sched->hang_limit = hang_limit;
	for (i = DRM_SCHED_PRIORITY_NORMAL; i < DRM_SCHED_PRIORITY_MAX; i++)
		lisuan_sched_rq_init(sched, &sched->sched_rq[i]);

	init_waitqueue_head(&sched->wake_up_worker);
	init_waitqueue_head(&sched->job_scheduled);
	INIT_LIST_HEAD(&sched->ring_mirror_list);
	spin_lock_init(&sched->job_list_lock);
	atomic_set(&sched->hw_rq_count, 0);
	INIT_DELAYED_WORK(&sched->work_tdr, lisuan_sched_job_timedout);
	atomic_set(&sched->num_jobs, 0);
	atomic64_set(&sched->job_id_count, 0);

	/* Each scheduler will run on a seperate kernel thread */
	sched->thread = kthread_run(lisuan_sched_main, sched, sched->name);
	if (IS_ERR(sched->thread)) {
		ret = PTR_ERR(sched->thread);
		sched->thread = NULL;
		DRM_ERROR("Failed to create scheduler for %s.\n", name);
		return ret;
	}

	sched->ready = true;
	return 0;
}

/**
 * lisuan_sched_fini - Destroy a gpu scheduler
 *
 * @sched: scheduler instance
 *
 * Tears down and cleans up the scheduler.
 */
void lisuan_sched_fini(struct lisuan_scheduler *sched)
{
	if (sched->thread)
		kthread_stop(sched->thread);

	sched->ready = false;
}

MODULE_LICENSE("GPL");
