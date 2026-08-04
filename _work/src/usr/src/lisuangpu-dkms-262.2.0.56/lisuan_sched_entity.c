/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */


#include <linux/kthread.h>
#include <linux/slab.h>

#include <drm/drm_print.h>
#include "lisuan_scheduler.h"
#include "gf_job.h"
#include "lisuan_trace.h"

//#include "gpu_scheduler_trace.h"

#define to_gf_sched_job(sched_job)		\
		container_of((sched_job), struct lisuan_sched_job, queue_node)

/**
 * lisuan_sched_entity_init - Init a context entity used by scheduler when
 * submit to HW ring.
 *
 * @entity: scheduler entity to init
 * @rq_list: the list of run queue on which jobs from this
 *           entity can be submitted
 * @num_rq_list: number of run queue in rq_list
 * @guilty: atomic_t set to 1 when a job on this queue
 *          is found to be guilty causing a timeout
 *
 * Note: the rq_list should have atleast one element to schedule
 *       the entity
 *
 * Returns 0 on success or a negative error code on failure.
 */
int lisuan_sched_entity_init(struct lisuan_sched_entity *entity, struct lisuan_sched_rq **rq_list, unsigned int num_rq_list, atomic_t * guilty)
{
	int i;

	if (!(entity && rq_list && (num_rq_list == 0 || rq_list[0])))
		return -EINVAL;

	gf_memset(entity, 0, sizeof(struct lisuan_sched_entity));
	INIT_LIST_HEAD(&entity->list);
	entity->rq = NULL;
	entity->guilty = guilty;
	entity->num_rq_list = num_rq_list;
	entity->rq_list = kcalloc(num_rq_list, sizeof(struct lisuan_sched_rq *), GFP_KERNEL);
	if (!entity->rq_list)
		return -ENOMEM;

	for (i = 0; i < num_rq_list; ++i)
		entity->rq_list[i] = rq_list[i];

	if (num_rq_list)
		entity->rq = rq_list[0];

	entity->last_scheduled = NULL;

	spin_lock_init(&entity->rq_lock);
	spsc_queue_init(&entity->job_queue);

	atomic_set(&entity->fence_seq, 0);
	entity->fence_context = dma_fence_context_alloc(2);
	init_completion(&entity->entity_idle);

	return 0;
}

/**
 * lisuan_sched_entity_is_idle - Check if entity is idle
 *
 * @entity: scheduler entity
 *
 * Returns true if the entity does not have any unscheduled jobs.
 */
bool lisuan_sched_entity_is_idle(struct lisuan_sched_entity *entity)
{
	rmb();			/* for list_empty to work without lock */

	if (list_empty(&entity->list) ||
		spsc_queue_count(&entity->job_queue) == 0 ||
		entity->stopped)
		return true;

	return false;
}

/**
 * lisuan_sched_entity_is_ready - Check if entity is ready
 *
 * @entity: scheduler entity
 *
 * Return true if entity could provide a job.
 */
bool lisuan_sched_entity_is_ready(struct lisuan_sched_entity *entity)
{
	if (spsc_queue_peek(&entity->job_queue) == NULL)
		return false;

	if (READ_ONCE(entity->dependency))
		return false;

	return true;
}

/**
 * lisuan_sched_entity_get_free_sched - Get the rq from rq_list with least load
 *
 * @entity: scheduler entity
 *
 * Return the pointer to the rq with least load.
 */
static struct lisuan_sched_rq *lisuan_sched_entity_get_free_sched(struct lisuan_sched_entity *entity)
{
	struct lisuan_sched_rq *rq = NULL;
	unsigned int min_jobs = UINT_MAX, num_jobs;
	int i;

	for (i = 0; i < entity->num_rq_list; ++i) {
		struct lisuan_scheduler *sched = entity->rq_list[i]->sched;

		if (!entity->rq_list[i]->sched->ready) {
			DRM_WARN("sched%s is not ready, skipping", sched->name);
			continue;
		}

		num_jobs = atomic_read(&sched->num_jobs);
		if (num_jobs < min_jobs) {
			min_jobs = num_jobs;
			rq = entity->rq_list[i];
		}
	}

	return rq;
}

/**
 * lisuan_sched_entity_flush - Flush a context entity
 *
 * @entity: scheduler entity
 * @timeout: time to wait in for Q to become empty in jiffies.
 *
 * Splitting drm_sched_entity_fini() into two functions, The first one does the
 * waiting, removes the entity from the runqueue and returns an error when the
 * process was killed.
 *
 * Returns the remaining time in jiffies left from the input timeout
 */
static long lisuan_sched_entity_flush(struct lisuan_sched_entity *entity, long timeout)
{
	struct lisuan_scheduler *sched;
	struct task_struct *last_user;
	long ret = timeout;

	if (!entity->rq)
		return 0;

	sched = entity->rq->sched;
	/**
	 * The client will not queue more IBs during this fini, consume existing
	 * queued IBs or discard them on SIGKILL
	 */
	if (current->flags & PF_EXITING) {
		if (timeout)
			ret = wait_event_timeout(sched->job_scheduled, lisuan_sched_entity_is_idle(entity), timeout);
	} else {
		wait_event_killable(sched->job_scheduled, lisuan_sched_entity_is_idle(entity));
	}

	/* For killed process disable any more IBs enqueue right now */
	last_user = cmpxchg(&entity->last_user, current->group_leader, NULL);
	if ((!last_user || last_user == current->group_leader) && (current->flags & PF_EXITING) && (current->exit_code == SIGKILL)) {
		spin_lock(&entity->rq_lock);
		entity->stopped = true;
		lisuan_sched_rq_remove_entity(entity->rq, entity);
		spin_unlock(&entity->rq_lock);
	}
	return ret;
}

/**
 * lisuan_sched_entity_kill_jobs - helper for drm_sched_entity_kill_jobs
 *
 * @f: signaled fence
 * @cb: our callback structure
 *
 * Signal the scheduler finished fence when the entity in question is killed.
 */
static void lisuan_sched_entity_kill_jobs_cb(struct dma_fence *f, struct dma_fence_cb *cb)
{
	struct lisuan_sched_job *job = container_of(cb, struct lisuan_sched_job,
						   finish_cb);

	lisuan_sched_fence_finished(job->s_fence);
	WARN_ON(job->s_fence->parent);
	job->sched->ops->free_job(job);
}

/**
 * lisuan_sched_entity_kill_jobs - Make sure all remaining jobs are killed
 *
 * @entity: entity which is cleaned up
 *
 * Makes sure that all remaining jobs in an entity are killed before it is
 * destroyed.
 */
static void lisuan_sched_entity_kill_jobs(struct lisuan_sched_entity *entity)
{
	struct lisuan_sched_job *job;
	int r;

	while ((job = to_gf_sched_job(spsc_queue_pop(&entity->job_queue)))) {
		struct gf_job *gf_job = container_of(job, struct gf_job, base);
		struct lisuan_sched_fence *s_fence = job->s_fence;

		lisuan_sched_fence_scheduled(s_fence);
		dma_fence_set_error(&s_fence->finished, -ESRCH);

		/*
		 * When pipe is hanged by older entity, new entity might
		 * not even have chance to submit it's first job to HW
		 * and so entity->last_scheduled will remain NULL
		 */
		if (!entity->last_scheduled) {
			lisuan_sched_entity_kill_jobs_cb(NULL, &job->finish_cb);
			continue;
		}

		r = dma_fence_add_callback(entity->last_scheduled, &job->finish_cb, lisuan_sched_entity_kill_jobs_cb);
		if (r == -ENOENT)
			lisuan_sched_entity_kill_jobs_cb(NULL, &job->finish_cb);
		else if (r)
			DRM_ERROR("fence add callback failed (%d)\n", r);
	}
}

static void lisuan_sched_entity_fini(struct lisuan_sched_entity *entity)
{
	struct lisuan_scheduler *sched = NULL;

	if (entity->rq) {
		sched = entity->rq->sched;
		lisuan_sched_rq_remove_entity(entity->rq, entity);
	}

	/* Consumption of existing IBs wasn't completed. Forcefully
	 * remove them here.
	 */
	if (spsc_queue_count(&entity->job_queue)) {
		if (sched) {
			/* Park the kernel for a moment to make sure it isn't processing
			 * our enity.
			 */
			wait_for_completion(&entity->entity_idle);
		}
		if (entity->dependency) {
			dma_fence_remove_callback(entity->dependency, &entity->cb);
			dma_fence_put(entity->dependency);
			entity->dependency = NULL;
		}

		lisuan_sched_entity_kill_jobs(entity);
	}

	dma_fence_put(entity->last_scheduled);
	entity->last_scheduled = NULL;
	kfree(entity->rq_list);
}

/**
 * lisuan_sched_entity_fini - Destroy a context entity
 *
 * @entity: scheduler entity
 *
 * Calls drm_sched_entity_do_release() and drm_sched_entity_cleanup()
 */
void lisuan_sched_entity_destroy(struct lisuan_sched_entity *entity)
{
	lisuan_sched_entity_flush(entity, MAX_WAIT_SCHED_ENTITY_Q_EMPTY);
	lisuan_sched_entity_fini(entity);
}

/**
 * lisuan_sched_entity_clear_dep - callback to clear the entities dependency
 */
static void lisuan_sched_entity_clear_dep(struct dma_fence *f, struct dma_fence_cb *cb)
{
	struct lisuan_sched_entity *entity = container_of(cb, struct lisuan_sched_entity, cb);

	entity->dependency = NULL;
	dma_fence_put(f);
}

/**
 * lisuan_sched_entity_clear_dep - callback to clear the entities dependency and
 * wake up scheduler
 */
static void lisuan_sched_entity_wakeup(struct dma_fence *f, struct dma_fence_cb *cb)
{
	struct lisuan_sched_entity *entity = container_of(cb, struct lisuan_sched_entity, cb);

	lisuan_sched_entity_clear_dep(f, cb);
	lisuan_sched_wakeup(entity->rq->sched);
}

/**
 * lisuan_sched_entity_set_rq_priority - helper for drm_sched_entity_set_priority
 */
static void lisuan_sched_entity_set_rq_priority(struct lisuan_sched_rq **rq, enum lisuan_sched_priority priority)
{
	*rq = &(*rq)->sched->sched_rq[priority];
}

/**
 * gf_sched_entity_set_priority - Sets priority of the entity
 *
 * @entity: scheduler entity
 * @priority: scheduler priority
 *
 * Update the priority of runqueus used for the entity.
 */
static void gf_sched_entity_set_priority(struct lisuan_sched_entity *entity, enum lisuan_sched_priority priority)
{
	unsigned int i;

	spin_lock(&entity->rq_lock);

	for (i = 0; i < entity->num_rq_list; ++i)
		lisuan_sched_entity_set_rq_priority(&entity->rq_list[i], priority);

	if (entity->rq) {
		lisuan_sched_rq_remove_entity(entity->rq, entity);
		lisuan_sched_entity_set_rq_priority(&entity->rq, priority);
		lisuan_sched_rq_add_entity(entity->rq, entity);
	}

	spin_unlock(&entity->rq_lock);
}

/**
 * lisuan_sched_entity_add_dependency_cb - add callback for the entities dependency
 *
 * @entity: entity with dependency
 *
 * Add a callback to the current dependency of the entity to wake up the
 * scheduler when the entity becomes available.
 */
static bool lisuan_sched_entity_add_dependency_cb(struct lisuan_sched_entity *entity)
{
	struct lisuan_scheduler *sched = entity->rq->sched;
	struct dma_fence *fence = entity->dependency;
	struct lisuan_sched_fence *s_fence;

	if (fence->context == entity->fence_context || fence->context == entity->fence_context + 1) {
		/*
		 * Fence is a scheduled/finished fence from a job
		 * which belongs to the same entity, we can ignore
		 * fences from ourself
		 */
		dma_fence_put(entity->dependency);
		return false;
	}

	s_fence = to_gf_sched_fence(fence);
	if (s_fence && s_fence->sched == sched) {

		/*
		 * Fence is from the same scheduler, only need to wait for
		 * it to be scheduled
		 */
		fence = dma_fence_get(&s_fence->scheduled);
		dma_fence_put(entity->dependency);
		entity->dependency = fence;
		if (!dma_fence_add_callback(fence, &entity->cb, lisuan_sched_entity_clear_dep))
			return true;

		/* Ignore it when it is already scheduled */
		dma_fence_put(fence);
		return false;
	}

	if (!dma_fence_add_callback(entity->dependency, &entity->cb, lisuan_sched_entity_wakeup))
		return true;

	dma_fence_put(entity->dependency);
	return false;
}

/**
 * lisuan_sched_entity_pop_job - get a ready to be scheduled job from the entity
 *
 * @entity: entity to get the job from
 *
 * Process all dependencies and try to get one job from the entities queue.
 */
struct lisuan_sched_job *lisuan_sched_entity_pop_job(struct lisuan_sched_entity *entity)
{
	struct lisuan_scheduler *sched = entity->rq->sched;
	struct lisuan_sched_job *sched_job;
	struct gf_job *gf_job;

	sched_job = to_gf_sched_job(spsc_queue_peek(&entity->job_queue));
	if (!sched_job)
		return NULL;

	gf_job = container_of(sched_job, struct gf_job, base);

	//vm mapping not finish
	if(gf_job->bo_count && !READ_ONCE(gf_job->prepare_ready.done))
			return NULL;

	while ((entity->dependency = sched->ops->dependency(sched_job, entity))) {
		trace_lisuan_sched_job_wait_dep(sched_job, entity->dependency);

		//only consider add dependency cb success dma_fence
		if (lisuan_sched_entity_add_dependency_cb(entity))
			return NULL;
	}

	/* skip jobs from entity that marked guilty */
	if (entity->guilty && atomic_read(entity->guilty))
		dma_fence_set_error(&sched_job->s_fence->finished, -ECANCELED);

	if(entity->last_scheduled){
		dma_fence_info("%s:%d finish fence:%p, ref:%d\n", __func__, __LINE__, entity->last_scheduled, 
			entity->last_scheduled->refcount.refcount.refs.counter - 1);
	}
	dma_fence_put(entity->last_scheduled);
	entity->last_scheduled = dma_fence_get(&sched_job->s_fence->finished);

	dma_fence_info("%s:%d finish fence:%p, ref:%d\n", __func__, __LINE__, entity->last_scheduled, 
		entity->last_scheduled->refcount.refcount.refs.counter);

	job_info("pop job:%d from tgid-pid:%d-%d\n", sched_job->id, gf_job->tgid, gf_job->pid);

	spsc_queue_pop(&entity->job_queue);
	return sched_job;
}

/**
 * lisuan_sched_entity_select_rq - select a new rq for the entity
 *
 * @entity: scheduler entity
 *
 * Check all prerequisites and select a new rq for the entity for load
 * balancing.
 */
void lisuan_sched_entity_select_rq(struct lisuan_sched_entity *entity)
{
	struct dma_fence *fence;
	struct lisuan_sched_rq *rq;

	if (spsc_queue_count(&entity->job_queue) || entity->num_rq_list <= 1)
		return;

	fence = READ_ONCE(entity->last_scheduled);
	if (fence && !dma_fence_is_signaled(fence))
		return;

	rq = lisuan_sched_entity_get_free_sched(entity);
	if (rq == entity->rq)
		return;

	spin_lock(&entity->rq_lock);
	lisuan_sched_rq_remove_entity(entity->rq, entity);
	entity->rq = rq;
	spin_unlock(&entity->rq_lock);
}

/**
 * lisuan_sched_entity_push_job - Submit a job to the entity's job queue
 *
 * @sched_job: job to submit
 * @entity: scheduler entity
 *
 * Note: To guarantee that the order of insertion to queue matches
 * the job's fence sequence number this function should be
 * called with drm_sched_job_init under common lock.
 *
 * Returns 0 for success, negative error code otherwise.
 */
void lisuan_sched_entity_push_job(struct lisuan_sched_job *sched_job, struct lisuan_sched_entity *entity)
{
	bool first;
	struct gf_job *gf_job = (struct gf_job *)sched_job;

	trace_lisuan_sched_push_job(sched_job, entity);
	atomic_inc(&entity->rq->sched->num_jobs);
	WRITE_ONCE(entity->last_user, current->group_leader);
	first = spsc_queue_push(&entity->job_queue, &sched_job->queue_node);

	job_info("queue job:%d from tgid-pid:%d-%d\n", sched_job->id, gf_job->tgid, gf_job->pid);

	/* first job wakes up scheduler */
	if (first) {
		/* Add the entity to the run queue */
		spin_lock(&entity->rq_lock);
		if (entity->stopped) {
			spin_unlock(&entity->rq_lock);

			DRM_ERROR("Trying to push to a killed entity\n");
			return;
		}
		lisuan_sched_rq_add_entity(entity->rq, entity);
		spin_unlock(&entity->rq_lock);
		lisuan_sched_wakeup(entity->rq->sched);
	}
}
