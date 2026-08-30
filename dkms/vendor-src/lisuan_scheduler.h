/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#ifndef _LISUAN_SCHEDULER_H_
#define _LISUAN_SCHEDULER_H_

//refer to linux_source/include/drm/gpu_scheduler.h
#include <drm/spsc_queue.h>
#include <linux/dma-fence.h>
#include <linux/completion.h>
#include <linux/workqueue.h>

#define MAX_WAIT_SCHED_ENTITY_Q_EMPTY msecs_to_jiffies(1000)

struct lisuan_scheduler;
struct lisuan_sched_rq;

enum lisuan_sched_priority {
	DRM_SCHED_PRIORITY_NORMAL,
	DRM_SCHED_PRIORITY_HIGH_SW,
	DRM_SCHED_PRIORITY_MAX
};

/**
 * struct lisuan_sched_entity - A wrapper around a job queue (typically
 * attached to the DRM file_priv).
 *
 * @list: used to append this struct to the list of entities in the
 *        runqueue.
 * @rq: runqueue on which this entity is currently scheduled.
 * @rq_list: a list of run queues on which jobs from this entity can
 *           be scheduled
 * @num_rq_list: number of run queues in the rq_list
 * @rq_lock: lock to modify the runqueue to which this entity belongs.
 * @job_queue: the list of jobs of this entity.
 * @fence_seq: a linearly increasing seqno incremented with each
 *             new &drm_sched_fence which is part of the entity.
 * @fence_context: a unique context for all the fences which belong
 *                 to this entity.
 *                 The &drm_sched_fence.scheduled uses the
 *                 fence_context but &drm_sched_fence.finished uses
 *                 fence_context + 1.
 * @dependency: the dependency fence of the job which is on the top
 *              of the job queue.
 * @cb: callback for the dependency fence above.
 * @guilty: points to ctx's guilty.
 * @fini_status: contains the exit status in case the process was signalled.
 * @last_scheduled: points to the finished fence of the last scheduled job.
 * @last_user: last group leader pushing a job into the entity.
 * @stopped: Marks the enity as removed from rq and destined for termination.
 *
 * Entities will emit jobs in order to their corresponding hardware
 * ring, and the scheduler will alternate between entities based on
 * scheduling policy.
 */
struct lisuan_sched_entity {
	struct list_head list;
	struct lisuan_sched_rq *rq;
	struct lisuan_sched_rq **rq_list;
	unsigned int num_rq_list;
	spinlock_t rq_lock;

	struct spsc_queue job_queue;

	atomic_t fence_seq;
	uint64_t fence_context;

	struct dma_fence *dependency;
	struct dma_fence_cb cb;
	atomic_t *guilty;
	struct dma_fence *last_scheduled;
	struct task_struct *last_user;
	bool stopped;
	struct completion entity_idle;
};

/**
 * struct lisuan_sched_rq - queue of entities to be scheduled.
 *
 * @lock: to modify the entities list.
 * @sched: the scheduler to which this rq belongs to.
 * @entities: list of the entities to be scheduled.
 * @current_entity: the entity which is to be scheduled.
 *
 * Run queue is a set of entities scheduling command submissions for
 * one specific ring. It implements the scheduling policy that selects
 * the next entity to emit commands from.
 */
struct lisuan_sched_rq {
	spinlock_t lock;
	struct lisuan_scheduler *sched;
	struct list_head entities;
	struct lisuan_sched_entity *current_entity;
};

/**
 * struct drm_sched_fence - fences corresponding to the scheduling of a job.
 */
struct lisuan_sched_fence {
	/**
         * @scheduled: this fence is what will be signaled by the scheduler
         * when the job is scheduled.
         */
	struct dma_fence scheduled;

	/**
         * @finished: this fence is what will be signaled by the scheduler
         * when the job is completed.
         *
         * When setting up an out fence for the job, you should use
         * this, since it's available immediately upon
         * drm_sched_job_init(), and the fence returned by the driver
         * from run_job() won't be created until the dependencies have
         * resolved.
         */
	struct dma_fence finished;

	/**
         * @parent: the fence returned by &drm_sched_backend_ops.run_job
         * when scheduling the job on hardware. We signal the
         * &drm_sched_fence.finished fence once parent is signalled.
         */
	struct dma_fence *parent;
	/**
         * @sched: the scheduler instance to which the job having this struct
         * belongs to.
         */
	struct lisuan_scheduler *sched;
	/**
         * @lock: the lock used by the scheduled and the finished fences.
         */
	spinlock_t lock;
	/**
         * @owner: job owner for debugging
         */
	void *owner;
};

struct lisuan_sched_fence *to_gf_sched_fence(struct dma_fence *f);

/**
 * struct lisuan_sched_job - A job to be run by an entity.
 *
 * @queue_node: used to append this struct to the queue of jobs in an entity.
 * @sched: the scheduler instance on which this job is scheduled.
 * @s_fence: contains the fences for the scheduling of job.
 * @finish_cb: the callback for the finished fence.
 * @node: used to append this struct to the @drm_gpu_scheduler.ring_mirror_list.
 * @id: a unique id assigned to each job scheduled on the scheduler.
 * @karma: increment on every hang caused by this job. If this exceeds the hang
 *         limit of the scheduler then the job is marked guilty and will not
 *         be scheduled further.
 * @s_priority: the priority of the job.
 * @entity: the entity to which this job belongs.
 * @cb: the callback for the parent fence in s_fence.
 *
 * A job is created by the driver using drm_sched_job_init(), and
 * should call drm_sched_entity_push_job() once it wants the scheduler
 * to schedule the job.
 */
struct lisuan_sched_job {
	struct spsc_node queue_node;
	struct lisuan_scheduler *sched;
	struct lisuan_sched_fence *s_fence;
	struct dma_fence_cb finish_cb;
	struct list_head node;
	uint64_t id;
	atomic_t karma;
	enum lisuan_sched_priority s_priority;
	struct lisuan_sched_entity *entity;
	struct dma_fence_cb cb;
	struct list_head prepare_node;
};

/**
 * struct lisuan_sched_backend_ops
 *
 * Define the backend operations called by the scheduler,
 * these functions should be implemented in driver side.
 */
struct lisuan_sched_backend_ops {
	/**
         * @dependency: Called when the scheduler is considering scheduling
         * this job next, to get another struct dma_fence for this job to
	 * block on.  Once it returns NULL, run_job() may be called.
	 */
	struct dma_fence *(*dependency) (struct lisuan_sched_job * sched_job, struct lisuan_sched_entity * s_entity);

	/**
         * @run_job: Called to execute the job once all of the dependencies
         * have been resolved.  This may be called multiple times, if
	 * timedout_job() has happened and drm_sched_job_recovery()
	 * decides to try it again.
	 */
	struct dma_fence *(*run_job) (struct lisuan_sched_job * sched_job);

	/**
         * @timedout_job: Called when a job has taken too long to execute,
         * to trigger GPU recovery.
	 */
	void (*timedout_job)(struct lisuan_sched_job * sched_job);

	/**
         * @free_job: Called once the job's finished fence has been signaled
         * and it's time to clean it up.
	 */
	void (*free_job)(struct lisuan_sched_job * sched_job);

	void (*tlb_inv)(struct lisuan_sched_job *sched_job, unsigned char **rb_ptr);
	void (*show_job_info)(struct lisuan_sched_job *sched_job);
	void (*dump_job)(struct lisuan_sched_job *sched_job);
};

/**
 * struct lisuan_scheduler
 *
 * @ops: backend operations provided by the driver.
 * @hw_submission_limit: the max size of the hardware queue.
 * @timeout: the time after which a job is removed from the scheduler.
 * @name: name of the ring for which this scheduler is being used.
 * @sched_rq: priority wise array of run queues.
 * @wake_up_worker: the wait queue on which the scheduler sleeps until a job
 *                  is ready to be scheduled.
 * @job_scheduled: once @drm_sched_entity_do_release is called the scheduler
 *                 waits on this wait queue until all the scheduled jobs are
 *                 finished.
 * @hw_rq_count: the number of jobs currently in the hardware queue.
 * @job_id_count: used to assign unique id to the each job.
 * @work_tdr: schedules a delayed call to @drm_sched_job_timedout after the
 *            timeout interval is over.
 * @thread: the kthread on which the scheduler which run.
 * @ring_mirror_list: the list of jobs which are currently in the job queue.
 * @job_list_lock: lock to protect the ring_mirror_list.
 * @hang_limit: once the hangs by a job crosses this limit then it is marked
 *              guilty and it will be considered for scheduling further.
 * @num_jobs: the number of jobs in queue in the scheduler
 * @ready: marks if the underlying HW is ready to work
 * @free_guilty: A hit to time out handler to free the guilty job.
 *
 * One scheduler is implemented for each hardware ring.
 */
struct lisuan_scheduler {
	const struct lisuan_sched_backend_ops *ops;
	uint32_t hw_submission_limit;
	long timeout;
	const char *name;
	struct lisuan_sched_rq sched_rq[DRM_SCHED_PRIORITY_MAX];
	wait_queue_head_t wake_up_worker;
	wait_queue_head_t job_scheduled;
	atomic_t hw_rq_count;
	atomic64_t job_id_count;
	struct delayed_work work_tdr;
	struct task_struct *thread;
	struct list_head ring_mirror_list;
	spinlock_t job_list_lock;
	int hang_limit;
	atomic_t num_jobs;
	bool ready;
	bool free_guilty;
};

//entity related
int lisuan_sched_entity_init(struct lisuan_sched_entity *entity, struct lisuan_sched_rq **rq_list, unsigned int num_rq_list, atomic_t * guilty);
int lisuan_sched_job_init(struct lisuan_sched_job *sched_job, struct lisuan_sched_entity *entity, void *owner);
void lisuan_sched_rq_remove_entity(struct lisuan_sched_rq *rq, struct lisuan_sched_entity *entity);

bool lisuan_sched_entity_is_ready(struct lisuan_sched_entity *entity);
void lisuan_sched_entity_push_job(struct lisuan_sched_job *sched_job, struct lisuan_sched_entity *entity);
void lisuan_sched_entity_destroy(struct lisuan_sched_entity *entity);
struct lisuan_sched_job *lisuan_sched_entity_pop_job(struct lisuan_sched_entity *entity);
void lisuan_sched_job_begin(struct lisuan_sched_job *s_job);
void lisuan_sched_process_job(struct dma_fence *f, struct dma_fence_cb *cb);
void lisuan_sched_start_timeout(struct lisuan_scheduler *sched);

void lisuan_sched_entity_select_rq(struct lisuan_sched_entity *entity);
void lisuan_sched_rq_add_entity(struct lisuan_sched_rq *rq, struct lisuan_sched_entity *entity);
void lisuan_sched_job_cleanup(struct lisuan_sched_job *job);
void lisuan_sched_show_ring_jobs(struct lisuan_scheduler *sched);
void lisuan_sched_show_rq_top_status(struct lisuan_scheduler *sched);
void lisuan_sched_dump_ring_jobs(struct lisuan_scheduler *sched);

//sched related
int lisuan_sched_init(struct lisuan_scheduler *sched,
		     const struct lisuan_sched_backend_ops *ops, unsigned hw_submission, unsigned hang_limit, long timeout, const char *name);
void lisuan_sched_fini(struct lisuan_scheduler *sched);

unsigned long lisuan_sched_suspend_timeout(struct lisuan_scheduler *sched);
void lisuan_sched_resume_timeout(struct lisuan_scheduler *sched, unsigned long remaining);
void lisuan_sched_wakeup(struct lisuan_scheduler *sched);

//fence related
void lisuan_sched_fence_scheduled(struct lisuan_sched_fence *fence);
void lisuan_sched_fence_finished(struct lisuan_sched_fence *fence);
struct lisuan_sched_fence *lisuan_sched_fence_create(struct lisuan_sched_entity *entity, void *owner);
int lisuan_sched_fence_slab_init(void);
void lisuan_sched_fence_slab_fini(void);
void lisuan_sched_stop(struct lisuan_scheduler *sched, struct lisuan_sched_job *bad);
void lisuan_sched_start(struct lisuan_scheduler *sched, bool full_recovery);
void lisuan_sched_resubmit_jobs(struct lisuan_scheduler *sched);
bool lisuan_sched_entity_is_idle(struct lisuan_sched_entity *entity);

#endif
