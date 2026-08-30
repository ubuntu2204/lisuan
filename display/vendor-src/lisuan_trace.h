/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#if !defined(_LSGPU_TRACE_H_) || defined(TRACE_HEADER_MULTI_READ)
#define _LSGPU_TRACE_H_

#include <linux/stringify.h>
#include <linux/types.h>
#include <linux/tracepoint.h>

#undef TRACE_SYSTEM
#define TRACE_SYSTEM lisuan
#define TRACE_INCLUDE_FILE lisuan_trace

/*
Usage:
echo 1 >> /sys/kernel/debug/tracing/events/lisuan/enable
echo 0 >> /sys/kernel/debug/tracing/events/lisuan/enable
cat /sys/kernel/debug/tracing/trace
*/

TRACE_EVENT(lisuan_sched_job_init,
		TP_PROTO(struct lisuan_sched_job *job),
		TP_ARGS(job),
		TP_STRUCT__entry(
				__field(uint64_t, sched_job_id)
				__string(ring, job->sched->name)
				__field(struct dma_fence *, fence)
				__field(unsigned int, ctx)
				__field(unsigned int, seqno)
			),

		TP_fast_assign(
				__entry->sched_job_id = job->id;
#if DRM_VERSION_CODE >= KERNEL_VERSION(6,11,0)
				__assign_str(ring);
#else
				__assign_str(ring, job->sched->name);
#endif
				__entry->fence = &job->s_fence->finished;
				__entry->ctx = job->s_fence->finished.context;
				__entry->seqno = job->s_fence->finished.seqno;
			),
		TP_printk("job(gen)=%llu, dma_fence=%p, ring=%s, ctx=%u, seqno=%u",
			__entry->sched_job_id, __entry->fence, 
			__get_str(ring), __entry->ctx, __entry->seqno)
);


TRACE_EVENT(gf_run_job,
		TP_PROTO(struct gf_job *job),
		TP_ARGS(job),
		TP_STRUCT__entry(
				__field(uint64_t, sched_job_id)
				__string(ring, job->base.sched->name)
				__field(unsigned int, tgid)
				__field(struct dma_fence *, fence)
				__field(unsigned int, ctx)
				__field(unsigned int, seqno)
				__field(unsigned int, procid)
				__field(unsigned int, bo_cnt)
				__field(unsigned int, is_signal_job)
			),

		TP_fast_assign(
				__entry->sched_job_id = job->base.id;
#if DRM_VERSION_CODE >= KERNEL_VERSION(6,11,0)
				__assign_str(ring);
#else
				__assign_str(ring, job->base.sched->name);
#endif
				__entry->tgid = job->tgid;
				__entry->fence = &job->base.s_fence->finished;
				__entry->ctx = job->base.s_fence->finished.context;
				__entry->seqno = job->base.s_fence->finished.seqno;
				__entry->procid = job->procid;
				__entry->bo_cnt = job->bo_count;
				__entry->is_signal_job = job->is_signal_job;
				),
		TP_printk("job(kick)=%llu, dma_fence=%p, tgid=%u, ring=%s, ctx=%u, seqno=%u, procid:%d, bo_cnt:%d, is_signal_job:%d",
			__entry->sched_job_id, __entry->fence, __entry->tgid,
			__get_str(ring), __entry->ctx, __entry->seqno, __entry->procid, __entry->bo_cnt, __entry->is_signal_job)
);

TRACE_EVENT(gf_job_free,
		TP_PROTO(struct gf_job *job),
		TP_ARGS(job),
		TP_STRUCT__entry(
				__field(uint64_t, sched_job_id)
				__string(ring, job->base.sched->name)
				__field(unsigned int, tgid)
				__field(struct dma_fence *, fence)
				__field(unsigned int, ctx)
				__field(unsigned int, seqno)
			),

		TP_fast_assign(
				__entry->sched_job_id = job->base.id;
#if DRM_VERSION_CODE >= KERNEL_VERSION(6,11,0)
				__assign_str(ring);
#else
				__assign_str(ring, job->base.sched->name);
#endif
				__entry->tgid = job->tgid;
				__entry->fence = &job->base.s_fence->finished;
				__entry->ctx = job->base.s_fence->finished.context;
				__entry->seqno = job->base.s_fence->finished.seqno;
				),
		TP_printk("job(free)=%llu, dma_fence=%p, tgid=%u, ring=%s, ctx=%u, seqno=%u",
			__entry->sched_job_id, __entry->fence, __entry->tgid,
			__get_str(ring), __entry->ctx, __entry->seqno)
);

TRACE_EVENT(lisuan_sched_push_job,
		TP_PROTO(struct lisuan_sched_job *sched_job, struct lisuan_sched_entity *entity),
		TP_ARGS(sched_job, entity),
		TP_STRUCT__entry(
				__field(struct lisuan_sched_entity *, entity)
				__field(struct dma_fence *, fence)
				__string(name, sched_job->sched->name)
				__field(uint64_t, id)
				__field(u32, job_count)
				__field(int, hw_job_count)
			),

		TP_fast_assign(
				__entry->entity = entity;
				__entry->id = sched_job->id;
				__entry->fence = &sched_job->s_fence->finished;
#if DRM_VERSION_CODE >= KERNEL_VERSION(6,11,0)
				__assign_str(name);
#else
				__assign_str(name, sched_job->sched->name);
#endif
				__entry->job_count = spsc_queue_count(&entity->job_queue);
				__entry->hw_job_count = atomic_read(&sched_job->sched->hw_rq_count);
				),
		TP_printk("job(queue)=%llu, fence=%p, entity=%p, ring=%s, pending=%u, hw=%d",
			__entry->id, __entry->fence,
			__entry->entity, __get_str(name),
			__entry->job_count, __entry->hw_job_count)
);

TRACE_EVENT(lisuan_sched_run_job,
		TP_PROTO(struct lisuan_sched_job *sched_job, struct lisuan_sched_entity *entity),
		TP_ARGS(sched_job, entity),
		TP_STRUCT__entry(
				__field(struct lisuan_sched_entity *, entity)
				__field(struct dma_fence *, fence)
				__string(name, sched_job->sched->name)
				__field(uint64_t, id)
				__field(u32, job_count)
				__field(int, hw_job_count)
			),

		TP_fast_assign(
				__entry->entity = entity;
				__entry->id = sched_job->id;
				__entry->fence = &sched_job->s_fence->finished;
#if DRM_VERSION_CODE >= KERNEL_VERSION(6,11,0)
				__assign_str(name);
#else
				__assign_str(name, sched_job->sched->name);
#endif
				__entry->job_count = spsc_queue_count(&entity->job_queue);
				__entry->hw_job_count = atomic_read(&sched_job->sched->hw_rq_count);
			),
		TP_printk("job(dequeue)=%llu, fence=%p, ring=%s, entity=%p, pending=%u, hw=%d",
			__entry->id, __entry->fence,
			__get_str(name), __entry->entity,
			__entry->job_count, __entry->hw_job_count)
);

TRACE_EVENT(lisuan_sched_resubmit_jobs,
		TP_PROTO(struct lisuan_sched_job *sched_job),
		TP_ARGS(sched_job),
		TP_STRUCT__entry(
				__field(struct dma_fence *, fence)
				__string(name, sched_job->sched->name)
				__field(uint64_t, id)
				__field(int, hw_job_count)
			),

		TP_fast_assign(
				__entry->id = sched_job->id;
				__entry->fence = &sched_job->s_fence->finished;
#if DRM_VERSION_CODE >= KERNEL_VERSION(6,11,0)
				__assign_str(name);
#else
				__assign_str(name, sched_job->sched->name);
#endif

				__entry->hw_job_count = atomic_read(&sched_job->sched->hw_rq_count);
			),
		TP_printk("job(rerun)=%llu, fence=%p, ring=%s, hw=%d",
			__entry->id, __entry->fence,
			__get_str(name), __entry->hw_job_count)
);

TRACE_EVENT(lisuan_sched_process_job,
		TP_PROTO(struct lisuan_sched_job *sched_job),
		TP_ARGS(sched_job),
		TP_STRUCT__entry(
			__field(struct dma_fence *, fence)
			__string(name, sched_job->sched->name)
			__field(uint64_t, id)
			__field(int, hw_job_count)
		),
		TP_fast_assign(
			__entry->id = sched_job->id;
			__entry->fence = &sched_job->s_fence->finished;
#if DRM_VERSION_CODE >= KERNEL_VERSION(6,11,0)
			__assign_str(name);
#else
			__assign_str(name, sched_job->sched->name);
#endif
			__entry->hw_job_count = atomic_read(
				&sched_job->sched->hw_rq_count);
		),
		TP_printk("job(back)=%llu, fence=%p, ring=%s, hw=%d",
			__entry->id, __entry->fence,
			__get_str(name), __entry->hw_job_count)
);

TRACE_EVENT(lisuan_sched_job_wait_dep,
		TP_PROTO(struct lisuan_sched_job *sched_job, struct dma_fence *dep_fence),
		TP_ARGS(sched_job, dep_fence),
		TP_STRUCT__entry(
				__string(name, sched_job->sched->name)
				__field(uint64_t, id)
				__field(struct dma_fence *, fence)
				__field(struct dma_fence *, dep_fence)
				__field(uint64_t, ctx)
				__field(unsigned, seqno)
			),

		TP_fast_assign(
#if DRM_VERSION_CODE >= KERNEL_VERSION(6,11,0)
				__assign_str(name);
#else
				__assign_str(name, sched_job->sched->name);
#endif
				__entry->id = sched_job->id;
				__entry->fence = &sched_job->s_fence->finished;
				__entry->dep_fence = dep_fence;
				__entry->ctx = sched_job->s_fence->finished.context;
				__entry->seqno = sched_job->s_fence->finished.seqno;
			),
		TP_printk("job(dep)=%llu, fence=%p, ring=%s, dep fence=%p, ctx=%llu, seq=%u",
			__entry->id, __entry->fence,
			__get_str(name), __entry->dep_fence,
			__entry->ctx, __entry->seqno)
);


TRACE_EVENT(lisuan_grab_procid,
		TP_PROTO(struct lisuan_sched_job *sched_job),
		TP_ARGS(sched_job),
		TP_STRUCT__entry(
				__string(name, sched_job->sched->name)
				__field(uint64_t, id)
				__field(struct dma_fence *, fence)
				__field(uint64_t, ctx)
				__field(unsigned, seqno)
			),

		TP_fast_assign(
#if DRM_VERSION_CODE >= KERNEL_VERSION(6,11,0)
				__assign_str(name);
#else
				__assign_str(name, sched_job->sched->name);
#endif
				__entry->id = sched_job->id;
				__entry->fence = &sched_job->s_fence->finished;
				__entry->ctx = sched_job->s_fence->finished.context;
				__entry->seqno = sched_job->s_fence->finished.seqno;
			),
		TP_printk("job(procid)=%llu, fence=%p, ring=%s, ctx=%llu, seq=%u",
			__entry->id, __entry->fence,
			__get_str(name),
			__entry->ctx, __entry->seqno)
);

TRACE_EVENT(lisuan_no_idle_proc,
		TP_PROTO(struct lisuan_sched_job *sched_job),
		TP_ARGS(sched_job),
		TP_STRUCT__entry(
				__string(name, sched_job->sched->name)
				__field(uint64_t, id)
				__field(struct dma_fence *, fence)
				__field(uint64_t, ctx)
				__field(unsigned, seqno)
			),

		TP_fast_assign(
#if DRM_VERSION_CODE >= KERNEL_VERSION(6,11,0)
				__assign_str(name);
#else
				__assign_str(name, sched_job->sched->name);
#endif
				__entry->id = sched_job->id;
				__entry->fence = &sched_job->s_fence->finished;
				__entry->ctx = sched_job->s_fence->finished.context;
				__entry->seqno = sched_job->s_fence->finished.seqno;
			),
		TP_printk("job(peak fence)=%llu, fence=%p, ring=%s, ctx=%llu, seq=%u",
			__entry->id, __entry->fence,
			__get_str(name),
			__entry->ctx, __entry->seqno)
);


TRACE_EVENT(lisuan_populate_vm_dirty_state,
		TP_PROTO(struct lisuan_sched_job *sched_job),
		TP_ARGS(sched_job),
		TP_STRUCT__entry(
				__string(name, sched_job->sched->name)
				__field(uint64_t, id)
				__field(struct dma_fence *, fence)
				__field(uint64_t, ctx)
				__field(unsigned, seqno)
			),

		TP_fast_assign(
#if DRM_VERSION_CODE >= KERNEL_VERSION(6,11,0)
				__assign_str(name);
#else
				__assign_str(name, sched_job->sched->name);
#endif
				__entry->id = sched_job->id;
				__entry->fence = &sched_job->s_fence->finished;
				__entry->ctx = sched_job->s_fence->finished.context;
				__entry->seqno = sched_job->s_fence->finished.seqno;
			),
		TP_printk("job(vmdirty)=%llu, fence=%p, ring=%s, ctx=%llu, seq=%u",
			__entry->id, __entry->fence,
			__get_str(name),
			__entry->ctx, __entry->seqno)
);

TRACE_EVENT(lisuan_vsync_index,
		TP_PROTO(unsigned int crtc_index),
		TP_ARGS(crtc_index),
		TP_STRUCT__entry(
				__field(unsigned, index)
			),

		TP_fast_assign(
				__entry->index = crtc_index;
			),
		TP_printk("crtc %d vsync", __entry->index)
);

#endif

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#include <trace/define_trace.h>

