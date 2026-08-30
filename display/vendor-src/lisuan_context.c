/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#include <linux/idr.h>
#include <drm/drm_file.h>
#include <linux/mm.h>
#include <linux/idr.h>
#include <linux/uaccess.h>
#include <linux/mman.h>

#include "lisuan_drm.h"
#include "lisuan_device.h"
#include "gf_job.h"
#include "lisuan_sync.h"
#include "lisuan_ring.h"
#include "gf_ringbuffer.h"

#include "lisuan_context.h"
#include "lisuan_vm.h"
#include "gf_kmd_event.h"

/*entity是context相关的，每个context创建entity，
*根据rb_index确定sched_list，sched_list中所有的sched的rq[priority]赋值到entity的rq_list中。
*一个entity对应一组sched_list，包含sched_list_num个rq,这些rq都是priority的
*context在创建时已经根据rb_index指定sched（如果相同sched有多个engine，那此时就是sched_list）,rq_list就是这些sched_list中的rq[priority]
*
*lisuan_ring,每个engine都只有一个sched(gfx,gce,vce,encode,decode),所以context初始化时，对应sched_list只有一个sched，
*所以rq已经确定，只有一个sched的rq[priority].
*当前context的rq已经确定，后续所有job在初始化的时候sched_job_init，首先会选择一个sched ready的rq,sched_entity_select_rq(lisuan只有一个rq，直接返回,不做选择)
job会push到此entity->job_queue中。当此entity第一次push job到job_queue时, entity会添加到rq->entities中
*sched_main thread会select_entity(lisuan_sched_rq_select_entity),按照rq的优先级，先在高优先级的rq->entities中选择entity(首先顺序选择下一个entity，如果没有或者都没有ready，则从头开始找，直到查找到current_entity)
*选择完entity之后，会在entity的job_queue中拿第一个job去submit
*linux5.4中rq_list的已经被linux5.15中的sched_list所替代。本文中依然采用rq_list。
*/
int lisuan_ctx_alloc_ioctl(struct drm_device *dev, void *data, struct drm_file *filp)
{
	struct lisuan_device *ls_dev = (struct lisuan_device *)dev->dev_private;
	struct lisuan_sched_rq *rq[1];	//只有一个sched，因此只有一个rq
	struct lisuan_ring *ring;
	struct lisuan_ctx_alloc_info *ctx_info = (struct lisuan_ctx_alloc_info *)data;
	struct lisuan_file *ls_file = (struct lisuan_file *)filp->driver_priv;
	struct lisuan_ctx *context = (struct lisuan_ctx *)gf_kvcalloc(1, sizeof(struct lisuan_ctx), GFP_KERNEL);

	context->rb_index = ctx_info->ring_buffer;
	context->priority = ctx_info->priority;

	//now alloc only one entity,because 3d engine only have one ringbuffer

	ring = ls_dev->sched[context->rb_index];
	rq[0] = &(ring->base.sched_rq[ctx_info->priority]);

	lisuan_sched_entity_init(&context->entity, rq, 1, NULL);
	context->ls_dev = ls_dev;
	context->ls_file = ls_file;
	context->tgid = (unsigned long)current->group_leader->pid;
	context->pid = (unsigned long)current->pid;

	mutex_lock(&ls_file->context_lock->lock);
	ctx_info->context = idr_alloc(&ls_file->context_handles, context, 1, 0, GFP_KERNEL);
	mutex_unlock(&ls_file->context_lock->lock);
	gf_debug("tgid-pid:%d-%d alloc ctx %d on file vm tgid-pid:%d-%d\n", context->tgid, context->pid, ctx_info->context, ls_file->tgid, ls_file->pid);

	ls_file->used_pid = context->pid;
	ls_file->used_tgid = context->tgid;
	snprintf(ls_file->usedname, sizeof(current->comm), "%s",current->comm);
	return 0;
}

int lisuan_ctx_free_ioctl(struct drm_device *dev, void *data, struct drm_file *filp)
{
	struct lisuan_device *ls_dev = (struct lisuan_device *)dev->dev_private;
	int i = 0, num_entities = 2;
	struct lisuan_file *ls_file = (struct lisuan_file *)filp->driver_priv;

	struct lisuan_ctx_free_info *ctx_info = (struct lisuan_ctx_free_info *)data;
	struct lisuan_ctx *context;

	mutex_lock(&ls_file->context_lock->lock);
	context = (struct lisuan_ctx *)idr_remove(&ls_file->context_handles, ctx_info->context);
	mutex_unlock(&ls_file->context_lock->lock);
	gf_debug("tgid-pid:%d-%d free ctx on file vm tgid-pid:%d-%d\n", context->tgid, context->pid, ls_file->tgid, ls_file->pid);

	//destroy the entity,TODO num_entities is hard code to 1
	lisuan_sched_entity_destroy(&context->entity);

	gf_kvfree(context);

	return 0;
}

int lisuan_cs_ioctl(struct drm_device *dev, void *data, struct drm_file *filp)
{
	struct lisuan_submit_cmd_info *cmd_info = (struct lisuan_submit_cmd_info *)data;
	const void __user *uptr = u64_to_user_ptr(cmd_info->dma_info);
	struct lisuan_ctx *context;
	unsigned int i,j, write;
	int ret;

	struct lisuan_file *ls_file = (struct lisuan_file *)filp->driver_priv;
	struct lisuan_gem_bo *buffer;
	struct gf_job *sched_job;
	struct lisuan_device *ls_dev = (struct lisuan_device *)dev->dev_private;

	mutex_lock(&ls_file->context_lock->lock);
	context = (struct lisuan_ctx *)idr_find(&ls_file->context_handles, cmd_info->context);
	mutex_unlock(&ls_file->context_lock->lock);

	sched_job = (struct gf_job *)gf_kvcalloc(1, sizeof(struct gf_job), GFP_KERNEL);

	if (context->rb_index == RB_INDEX_VEM) {
		sched_job->vem_cmd_size = cmd_info->dma_count;
		sched_job->vem_cmd = gf_calloc(sched_job->vem_cmd_size);

		ret = copy_from_user(sched_job->vem_cmd, uptr, sched_job->vem_cmd_size);
		if (ret)
			goto exit;
	} else if (context->rb_index == RB_INDEX_DEC0 || context->rb_index == RB_INDEX_DEC1) {
		gf_memcpy(&sched_job->dec_para, &cmd_info->dec_para, sizeof(struct exchange_parameter));
	} else if (context->rb_index == RB_INDEX_ENC || context->rb_index == RB_INDEX_IM) {
		gf_memcpy(&sched_job->enc_para, &cmd_info->enc_para, sizeof(struct enc_exchange_parameter));
	} else {
		sched_job->dma_count = cmd_info->dma_count;
		sched_job->dma_info = (struct dma_info_s *)gf_kvcalloc(cmd_info->dma_count, sizeof(struct dma_info_s), GFP_KERNEL);
		
		if(cmd_info->dma_count) {
			ret = copy_from_user(sched_job->dma_info, uptr, sizeof(struct dma_info_s) * cmd_info->dma_count);
			if(ret)
				goto exit;
		}
	}

	sched_job->context = context;

	INIT_LIST_HEAD(&sched_job->sync);
	INIT_LIST_HEAD(&sched_job->pt_buf_list);

	lisuan_sched_job_init(&sched_job->base, &context->entity, (void *)0ul);
	sched_job->vm = &ls_file->vm;
	sched_job->uf_sequence = atomic_read(&context->entity.fence_seq);

	lisuan_syncobj_lookup_and_add_to_sync_wait(filp, sched_job, cmd_info);

	if (cmd_info->bo_list_info.bo_count) 
	{
		const void __user *bo_uptr = u64_to_user_ptr(cmd_info->bo_list_info.bo_handle_list);
		const void __user *bo_op_uptr = u64_to_user_ptr(cmd_info->bo_list_info.bo_op_list);
		unsigned int *bo_list = (unsigned int *)gf_kvcalloc(cmd_info->bo_list_info.bo_count * 4, sizeof(unsigned int), GFP_KERNEL);
		unsigned int *bo_op = bo_list + cmd_info->bo_list_info.bo_count;
		unsigned int *bo_list_filter = bo_list + cmd_info->bo_list_info.bo_count * 2;
		unsigned int *bo_op_filter = bo_list + cmd_info->bo_list_info.bo_count * 3;
		struct drm_gem_object *base;
		bool need_resident = false;
		unsigned int bo_count = 0;

		ret = copy_from_user(bo_list, bo_uptr, sizeof(unsigned int) * cmd_info->bo_list_info.bo_count);
		ret = copy_from_user(bo_op, bo_op_uptr, sizeof(unsigned int) * cmd_info->bo_list_info.bo_count);

		//filer same bo, umd enable mgt will include same bo, add code to workaround
		for(i = 0; i < cmd_info->bo_list_info.bo_count; i++)
		{
			unsigned int bo = bo_list[i];
			bool found = false;

			for(j = 0; j < bo_count; j++)
			{
				if(bo_list_filter[j] == bo){
					bo_op_filter[j] |= bo_op[i];
					found = true;
					break;
				}
			}

			if(!found){
				bo_list_filter[bo_count] = bo;
				bo_op_filter[bo_count] = bo_op[i];
				bo_count ++;
			}
		}

		sched_job->bo_count = bo_count;
		sched_job->bo_list = gf_kvcalloc(sched_job->bo_count, sizeof(struct lisuan_gem_bo *), GFP_KERNEL);

		for(i = 0; i < sched_job->bo_count; i++)
		{
			base = drm_gem_object_lookup(filp, bo_list_filter[i]);
			buffer = gem_to_gf_bo(base);
			drm_gem_object_put_compat(base);

			lisuan_sync_bo_fence(sched_job, &buffer->tbo, bo_op_filter[i]);

			need_resident |= lisuan_need_resident(ls_dev, buffer);
			sched_job->bo_list[i]= buffer;
		}

		init_completion(&sched_job->prepare_ready);
		if(need_resident)
		{
			unsigned long flags;

			spin_lock_irqsave(&ls_dev->job_prepare_lock, flags);
			list_add_tail(&sched_job->base.prepare_node, &ls_dev->job_prepare_list);
			spin_unlock_irqrestore(&ls_dev->job_prepare_lock, flags);

			wake_up_interruptible(&ls_dev->job_prepare_wq);
		}
		else
		{
			complete(&sched_job->prepare_ready);
		}

		for(i = 0; i < sched_job->bo_count; i++)
		{
			buffer = sched_job->bo_list[i];

			lisuan_add_sync_fence_to_buffer(&buffer->tbo, &sched_job->base.s_fence->finished, bo_op_filter[i]);
		}

		gf_kvfree(bo_list);
	}

	dma_fence_info("%s:%d finish fence:%p, ref:%d\n", __func__, __LINE__, &sched_job->base.s_fence->finished, 
		sched_job->base.s_fence->finished.refcount.refcount.refs.counter);

	lisuan_syncobj_lookup_and_add_to_sync_signal(filp, sched_job, cmd_info);

	lisuan_sched_entity_push_job(&sched_job->base, &context->entity);

	gf_record_evt_kickoff(ls_dev, context->rb_index, sched_job->base.id, context->rb_index);

	return 0;
exit:
	ASSERT(0);
	return ret;
}

int lisuan_job_prepare_is_empty(struct lisuan_device *ls_dev)
{
	int empty = 0;
	unsigned long flags;

	spin_lock_irqsave(&ls_dev->job_prepare_lock, flags);
	empty = list_empty(&ls_dev->job_prepare_list);
	spin_unlock_irqrestore(&ls_dev->job_prepare_lock, flags);
	return empty;
}

int lisuan_job_prepare_thread_main(void *data)
{
	struct lisuan_device *ls_dev = (struct lisuan_device *)data;
	int i = 0, ret;
	struct gf_job *job = NULL;

	/**************************************************
	 * for linux-5.11.0 or high version we should use sched_set_fifo_low instead the sched_setscheduler
     *struct sched_param sparam = {.sched_priority = 1 };
	 *sched_setscheduler(current, SCHED_FIFO, &sparam);
	 **************************************************/
	#if DRM_VERSION_CODE >= KERNEL_VERSION(5,0,0)
	sched_set_fifo_low(current);
	#endif

	while (!kthread_should_stop()) 
	{
		unsigned long flags;
		wait_event_interruptible_timeout(ls_dev->job_prepare_wq,
					 kthread_should_stop() || !lisuan_job_prepare_is_empty(ls_dev), msecs_to_jiffies(500));

retry:
		spin_lock_irqsave(&ls_dev->job_prepare_lock, flags);

		job = (struct gf_job*)list_first_entry_or_null(&ls_dev->job_prepare_list, struct lisuan_sched_job, prepare_node);
		if(job)
			list_del(&job->base.prepare_node);
		spin_unlock_irqrestore(&ls_dev->job_prepare_lock, flags);

		if(job)
		{
			struct lisuan_file *ls_file = job->context->ls_file;
			int i;

			for (i = 0; i < job->bo_count; i++)
			{
				lisuan_prepare_gem(ls_dev, job->bo_list[i]);
				ret = lisuan_vm_map_va_range(ls_dev, ls_file, job->bo_list[i], NULL);
			}
			complete(&job->prepare_ready);

			goto retry;
		}
	}

	return 0;
}

void lisuan_init_job_prepare_thread(struct lisuan_device *ls_dev)
{
	spin_lock_init(&ls_dev->job_prepare_lock);
	INIT_LIST_HEAD(&ls_dev->job_prepare_list);

	init_waitqueue_head(&ls_dev->job_prepare_wq);

	snprintf(ls_dev->job_prepare_thread_name, 66, "job prepare");
	ls_dev->job_prepare_thread = kthread_run(lisuan_job_prepare_thread_main, ls_dev, ls_dev->job_prepare_thread_name);
}

void lisuan_fini_job_prepare_thread(struct lisuan_device *ls_dev)
{
	if(!lisuan_job_prepare_is_empty(ls_dev)){
		gf_info("prepare job not null, please handle it\n");
	}

	if (ls_dev->job_prepare_thread)
		kthread_stop(ls_dev->job_prepare_thread);
}

void lisuan_init_vm_context(struct lisuan_device *ls_dev)
{
	struct lisuan_ctx *context = gf_kvcalloc(1, sizeof(struct lisuan_ctx), GFP_KERNEL);
	struct lisuan_sched_rq *rq[1];

	struct lisuan_ring *ring;
	gf_memset(context, 0, sizeof(struct lisuan_ctx));
	context->rb_index = ls_dev->hwctl.paging_rb;
	ring = ls_dev->sched[context->rb_index];
	rq[0] = &(ring->base.sched_rq[DRM_SCHED_PRIORITY_HIGH_SW]);	//将vm_update_pte列为高优先级

	lisuan_sched_entity_init(&context->entity, rq, 1, NULL);
	context->tgid = (unsigned long)current->group_leader->pid;
	context->pid = (unsigned long)current->pid;
	context->ls_dev = ls_dev;
	ls_dev->vm_ctx = context;
	gf_debug("tgid-pid:%d-%d alloc ctx on device\n", context->tgid, context->pid);

	lisuan_init_job_prepare_thread(ls_dev);
}

void lisuan_fini_vm_context(struct lisuan_device *ls_dev)
{
	struct lisuan_ctx *context = ls_dev->vm_ctx;

	lisuan_sched_entity_destroy(&context->entity);

	lisuan_fini_job_prepare_thread(ls_dev);

	gf_kvfree(context);
}

int lisuan_get_vem_info_ioctl(struct drm_device *dev, void *data, struct drm_file *filp)
{
    struct lisuan_device *ls_dev = (struct lisuan_device *)dev->dev_private;
    struct lisuan_file *ls_file = (struct lisuan_file *)filp->driver_priv;
    struct vem_info_ioctl *info = (struct vem_info_ioctl*)data;

    struct lisuan_ring *vem_rb = ls_dev->sched[RB_INDEX_VEM];
    struct gf_ring_buffer *ringbuffer_vem = (struct gf_ring_buffer *)vem_rb->chip_priv;

    struct lisuan_vm_node *vm_node = container_of(ls_file->vm.root.buffer->va_list.next, struct lisuan_vm_node, node_item);
	struct lisuan_vm_node *rbvm_node = container_of(ringbuffer_vem->buf->va_list.next, struct lisuan_vm_node, node_item);

    resource_size_t start = 0;
    struct mmap_args args = {0};
    
    info->rb_gpuva = rbvm_node->offset;
    info->vm_root_gpuva = (vm_node->offset - ls_dev->mmu_bypass_start) >> 9;

    args.size = ringbuffer_vem->buf->size;
    args.domain = ringbuffer_vem->buf->domain;
    
    if(ringbuffer_vem->buf->flag.vram){
        start = info->rb_gpuva + ls_dev->pdev_info.start_addr_vram - ls_dev->mmu_bypass_start;
    }else{
        args.sg_table = ringbuffer_vem->buf->tbo.sg;
        args.snoop = ringbuffer_vem->buf->flag.snoop;
    }
    
    ls_file->vem_rb_map=(void*)&args;
    info->rb_cpuva = vm_mmap(filp->filp, 0, ringbuffer_vem->buf->size, PROT_READ | PROT_WRITE, MAP_SHARED, start);
    ls_file->vem_rb_map=NULL;

    gf_info("the vem rb_gpuva:0x%llx,cpuva:0x%llx, 9bit shifted vm root gpuva:0x%llx\n", info->rb_gpuva, info->rb_cpuva, info->vm_root_gpuva);
    return 0;
}

void lisuan_ctx_mgr_fini(struct lisuan_device *ls_dev, struct lisuan_file *ls_file)
{
	struct lisuan_ctx *ls_ctx;
	struct idr *idp;
	uint32_t id, i, j;

	idp = &ls_file->context_handles;
	mutex_lock(&ls_file->context_lock->lock);
	idr_for_each_entry(idp, ls_ctx, id)
	{
		lisuan_sched_entity_destroy(&ls_ctx->entity);
	}
	mutex_unlock(&ls_file->context_lock->lock);

	idr_destroy(&ls_file->context_handles);
	mutex_destroy(&ls_file->context_lock->lock);
	gf_kvfree(ls_file->context_lock);
}
