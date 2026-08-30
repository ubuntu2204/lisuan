/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#include "lisuan_drm.h"
#include "lisuan_gem.h"
#include "lisuan_ring.h"
#include "lisuan_device.h"
#include "lisuan_sync.h"
#include "lisuan_vm.h"
#include "lisuan_mailbox.h"
#include "lisuan_trace.h"
#include "gf_ringbuffer.h"
#include "gf_hw_misc.h"
#include "gf_kmd_event.h"

#include "gf_job.h"

#include "subsys.h"
#include "vcx_normal_driver.h"
#include "vcx_vcmd_driver.h"

extern struct srb_item_s c_srb_desc[18];
void gf_dump_ctx_buffer(struct lisuan_device *ls_dev, struct lisuan_gem_bo *gem, char* msg, unsigned int offset, unsigned int size)
{
	char file_name[256];
	struct file  *filp;
	unsigned int len = 0;
	uint32_t blk_cnt = sizeof(c_srb_desc) / sizeof(struct srb_item_s);
	void *data=NULL;
	uint32_t i = 0;

	struct dump_chip_image_header_s header = { 0 };
	header.magic_number = 0x77badbed;
	header.block_cnt = blk_cnt;

	if(gem->domain == dom_invisual_vram)
		return;

	gf_vsnprintf(file_name, sizeof(file_name), "/tmp/%s.img", msg);
	filp = gf_create_filp(file_name);
	if(!filp)
		return;

	//dump header
	gf_write_filp(filp, sizeof(header), &header);

	//dump block info
	for (i = 0; i < blk_cnt; i++)
	{
		struct dump_chip_image_block_info_s blockinfo = { 0 };

		blockinfo.block_id = c_srb_desc[i].u_blk_id;
		blockinfo.reg_offset = c_srb_desc[i].u_start_addr_in_grp;
		blockinfo.reg_count = (c_srb_desc[i].u_end_addr_in_grp - c_srb_desc[i].u_start_addr_in_grp);
		gf_write_filp(filp, sizeof(blockinfo), &blockinfo);
	}

	if(gem->domain < dom_invisual_vram) 
		data = lisuan_map_vram_node(ls_dev, __get_gem_buffer_mm_node(gem), gem->flag.byPass);
	else
		data = lisuan_map_system_pages(ls_dev, gem->flag.snoop, gem->size, gem->tbo.sg);

	gf_write_filp(filp, gem->size, data);

	if (gem->domain < dom_invisual_vram)
		iounmap(data);
	else
		vunmap(data);
	gf_close_filp(filp);
}

void gf_sched_job_dump(struct lisuan_sched_job *sched_job)
{
	struct gf_job *job = container_of(sched_job, struct gf_job, base);
	struct lisuan_scheduler *sched = job->base.sched;
	struct lisuan_ctx *context = job->context;
	struct lisuan_device *ls_dev = context->ls_dev;
	struct lisuan_ring *ring = job->ring_fence->ring;
	int i;

	if(!ls_dev->hwctl.endebug)
		return;

	gf_dump_status(ls_dev->mmio_base, ls_dev->is_virtfn, ls_dev->virtfn_id, ring->rb_index);

	gf_info("Detect hang job:%d of tgid-pid:%d-%d, gpu vmid:%d,job finished fence:0x%llx, uf_sequence:0x%llx\n", job->base.id, job->tgid, job->pid, job->procid, job->ring_fence->ring_fence_value, job->uf_sequence);
	gf_info("Submit ringbuffer:%d, last send fence:0x%llx, returned:0x%llx, hw return:0x%llx\n",
		context->rb_index, atomic64_read(&ring->last_submit_fence_id), atomic64_read(&ring->returned_fence_id), *ring->fence_node.cpu_virt_addr);

	for (i = 0; i < job->dma_count; i++) {
		gf_info("Cmd stream:%d, gpu va:0x%llx, cmd size:0x%x\n", i, job->dma_info[i].dma_gpu_va, job->dma_info[i].dma_size_in_byte);

		if(job->dma_info[i].preempt_chip_image_gpu_va)
		{
			int j=0, found = 0;
			for (j = 0; j < job->bo_count && !found; j++) 
			{
				struct lisuan_gem_bo *buffer = job->bo_list[j];
				struct lisuan_vm_node *vm_node = NULL, *tmp=NULL;

				if(buffer && buffer->size){
					mutex_lock(&buffer->va_list_lock.lock);
					list_for_each_entry_safe(vm_node, tmp, &buffer->va_list, node_item) 
					{
						if(vm_node->ls_file == context->ls_file &&
							vm_node->offset <= job->dma_info[i].preempt_chip_image_gpu_va && 
							(vm_node->offset + job->dma_info[i].preempt_chip_image_size_in_byte) >= job->dma_info[i].preempt_chip_image_gpu_va){
							gf_info("Gem ctx handle:%x, gpu va:0x%llx(tid:%d), segment:%d, byPass:%d, size org:0x%llx\n", buffer->handle, 
								vm_node->offset, (vm_node->ls_file?vm_node->ls_file->tgid:0), buffer->domain, vm_node->byPass, buffer->size_org);
							{
								char file_name[128];
								gf_vsnprintf(file_name, 128, "ctx_handle_%x_gpuva_%llx", buffer->handle, vm_node->offset);
								gf_dump_ctx_buffer(ls_dev, buffer, file_name, 0, vm_node->size);
								found = 1;
								break;
							}
						}
					}
					mutex_unlock(&buffer->va_list_lock.lock);
				}
			}
		}
	}

	if(job->kcommand)
			gf_dump_commands("idb", (void*)job->kcommand->cpu_virt_addr, job->kcommand->size/4);
	if(job->rb)
			gf_dump_commands("rb", (void*)job->rb, job->rb_size);

	for (i = 0; i < job->bo_count; i++) {
		struct lisuan_gem_bo *buffer = job->bo_list[i];
		struct lisuan_vm_node *vm_node = NULL, *tmp=NULL;

		if(buffer && buffer->size){
			mutex_lock(&buffer->va_list_lock.lock);
			list_for_each_entry_safe(vm_node, tmp, &buffer->va_list, node_item) 
			{
				if(vm_node->ls_file == context->ls_file){
					gf_info("Gem handle:%x, gpu va:0x%llx(tid:%d), segment:%d, byPass:%d, size org:0x%llx\n", buffer->handle, 
						vm_node->offset, (vm_node->ls_file?vm_node->ls_file->tgid:0), buffer->domain, vm_node->byPass, buffer->size_org);
					if(buffer->usage_hint.shader){
						char file_name[128];
						gf_vsnprintf(file_name, 128, "shader_handle_%x_gpuva_%llx", buffer->handle, vm_node->offset);
						lisuan_gem_content_dump(ls_dev, buffer, file_name, 0, vm_node->size);
					}
					else if (buffer->usage_hint.is_cmdbuf){
						char file_name[128];
						int i=0;
						for(i=0; i<job->dma_count; i++)
						{
							if(vm_node->offset <= job->dma_info[i].dma_gpu_va &&
								(job->dma_info[i].dma_gpu_va + job->dma_info[i].dma_size_in_byte) <= (vm_node->offset + vm_node->size))
							{
								gf_vsnprintf(file_name, 128, "cmd_handle_%x_gpuva_%llx_0x%llx", buffer->handle, job->dma_info[i].dma_gpu_va, job->dma_info[i].dma_size_in_byte);
								lisuan_gem_content_dump(ls_dev, buffer, file_name, job->dma_info[i].dma_gpu_va - vm_node->offset, job->dma_info[i].dma_size_in_byte);
							}
						}
					}
					else
					{
						char file_name[128];
						gf_vsnprintf(file_name, 128, "handle_%x_gpuva_%llx", buffer->handle, vm_node->offset);
						lisuan_gem_content_dump(ls_dev, buffer, file_name, 0, vm_node->size);
					}
				}
			}
			mutex_unlock(&buffer->va_list_lock.lock);
		}
	}

	gf_dump_debug_status(ls_dev->mmio_base);
}

#define is_dirty(a, b, c)      \
	if((c = (a & (1 << b)))) {  \
		a &=~(1 << b);        \
	}

void gf_tlb_inv(struct lisuan_sched_job *sched_job, unsigned char **rb_ptr)
{
	struct lisuan_ring *ring = container_of(sched_job->sched, struct lisuan_ring, base);
	struct gf_job *job = container_of(sched_job, struct gf_job, base);
	struct lisuan_device *device = ring->device;
	struct lisuan_vm *vm = job->vm;

	unsigned int *dma_ptr = (unsigned int *)*rb_ptr;
	struct lisuan_vm_node *vm_node = NULL;
	struct lisuan_vm_node *dummy_vm_node = NULL;

	if(job->need_vm_inv){
		job->cur_vm_L0_dirty =
		job->cur_vm_L1_dirty =
		job->cur_vm_L2_dirty = true;
	}

	if(!(job->cur_vm_L0_dirty || 
		job->cur_vm_L1_dirty ||
		job->cur_vm_L2_dirty ||
		job->bypass_L2_dirty))
		return;

	job_info("tlb rb:%d job:%d from tgid-pid:%d-%d,vm owner:%d-%d, procid:%d, bypass dirty:%d, L0-L1-L2:%d-%d-%d\n", 
		job->context->rb_index, sched_job->id, job->tgid, job->pid, vm?vm->file->tgid:0, vm?vm->file->pid:0,
		job->procid, job->bypass_L2_dirty, job->cur_vm_L0_dirty, job->cur_vm_L1_dirty, job->cur_vm_L2_dirty);

	if(vm)
		vm_node = container_of(job->vm->root.buffer->va_list.next, struct lisuan_vm_node, node_item);
	dummy_vm_node = container_of(device->dummy_page_buf->va_list.next, struct lisuan_vm_node, node_item);

	if(gf_get_tlb_inv_cmd(&dma_ptr, ring->rb_index, 
		job->procid, vm_node ? vm_node->offset - device->mmu_bypass_start : 0, dummy_vm_node->offset, 
		job->cur_vm_L0_dirty, job->cur_vm_L1_dirty, job->cur_vm_L2_dirty, job->bypass_L2_dirty))
	{
		gf_error("get tlb inv cmd failed rb:%d job:%d from tgid-pid:%d-%d, procid:%d\n", 
			job->context->rb_index, sched_job->id, job->tgid, job->pid, job->procid);
		return; 
	}
	
	*rb_ptr = (unsigned char*)dma_ptr;
}

int gf_job_populate_vm_dirty_state(struct lisuan_sched_job *sched_job)
{
	struct lisuan_ring *ring = container_of(sched_job->sched, struct lisuan_ring, base);
	struct gf_job *job = container_of(sched_job, struct gf_job, base);
	struct lisuan_device *ls_dev = ring->device;
	struct lisuan_vm *vm = job->vm;
	int ret = 0;

	//workload job need wait upload job
	if(vm)
	{
		trace_lisuan_populate_vm_dirty_state(sched_job);
		mutex_lock(&vm->pt_lock->lock);
		if(vm->last_update.fence && !dma_fence_is_signaled(vm->last_update.fence)){
			struct gf_sync_fence *sync_fence = lisuan_sync_fence(vm->last_update.fence);
			sync_fence->sched = vm->last_update.sched;
			list_add_tail(&sync_fence->list_item, &job->sync);
			ret = -1;
			mutex_unlock(&vm->pt_lock->lock);
			return ret;
		}

		is_dirty(vm->is_L0_dirty, ring->rb_index, job->cur_vm_L0_dirty)
		is_dirty(vm->is_L1_dirty, ring->rb_index, job->cur_vm_L1_dirty)
		is_dirty(vm->is_L2_dirty, ring->rb_index, job->cur_vm_L2_dirty)

		if(!list_empty(&vm->defer_free_pts))
		{
			list_splice(&vm->defer_free_pts, &job->pt_buf_list);
			INIT_LIST_HEAD(&vm->defer_free_pts);
		}
		mutex_unlock(&vm->pt_lock->lock);
	}

	spin_lock(&ls_dev->bypass_lock);
	is_dirty(ls_dev->is_bypass_update, ring->rb_index, job->bypass_L2_dirty)
	spin_unlock(&ls_dev->bypass_lock);

	return ret;
}

struct dma_fence *gf_gfx_job_run(struct lisuan_sched_job *sched_job)
{
	struct lisuan_ring *ring = container_of(sched_job->sched, struct lisuan_ring, base);
	struct gf_job *job = container_of(sched_job, struct gf_job, base);
	struct gf_gfx_ringbuffer *gfx_rb = (struct gf_gfx_ringbuffer *)ring->chip_priv;
	struct lisuan_device *device = ring->device;
	void *gfx_cmd_temp = device->rb_cmd_template;
	int size = 0, pad = 0, i;
	unsigned char *rb_space, *rb_space_orig, *rb_tail;
	unsigned int *rb_space_int;
	unsigned int rb_size = 0, tail;
	unsigned int is_reserve_vm = !job->vm;

	/* if gpu reset, hw fence will be replaced here */
	if (job->ring_fence){
		dma_fence_put(&job->ring_fence->base);
		job->need_vm_inv = !!job->vm; //force vm inv
	}

	job->ring_fence = lisuan_ring_fence_create(ring);
	dma_fence_get(&job->ring_fence->base);

	//replace with lastest update job fence
	if(job->vm){
		mutex_lock(&job->vm->pt_lock->lock);
		if(job->vm->last_used[ring->rb_index].fence){
			dma_fence_info("%s:%d finish fence:%p, ref:%d\n", __func__, __LINE__, job->vm->last_used[ring->rb_index].fence, 
				job->vm->last_used[ring->rb_index].fence->refcount.refcount.refs.counter-1);
		}

		dma_fence_replace(&job->vm->last_used[ring->rb_index].fence, &sched_job->s_fence->finished);

		dma_fence_info("%s:%d finish fence:%p, ref:%d\n", __func__, __LINE__, job->vm->last_used[ring->rb_index].fence, 
		job->vm->last_used[ring->rb_index].fence->refcount.refcount.refs.counter);
		mutex_unlock(&job->vm->pt_lock->lock);
	}

	rb_size = gf_hw_get_job_run_rb_size(job->dma_count, device->is_virtfn, GF_CMD_TYPE_GFX);

	rb_space_orig =
	rb_space = (unsigned char *)gf_calloc(rb_size * 4);

	gf_hw_get_job_run_begin_cmds(&rb_space, job->procid, gfx_cmd_temp, GF_CMD_TYPE_GFX);
	
	gf_tlb_inv(sched_job, &rb_space);

	for (i = 0; i < job->dma_count; i++) {
		gf_hw_get_job_run_dma_cmds(&rb_space, job->dma_info[i].idb1_index, job->dma_info[i].idb_type, 
			job->dma_info[i].dma_size_in_byte, job->dma_info[i].dma_gpu_va, device->is_virtfn, gfx_cmd_temp);
	}
	gf_hw_get_job_run_fence_cmds(&rb_space, false, ring->fence_node.start, job->ring_fence->ring_fence_value, gfx_cmd_temp, GF_CMD_TYPE_GFX);
	// send ifence to wait efence write finished
	if (device->is_virtfn)
	{
		gf_hw_emit_vf_efence(&rb_space);
	}
	gf_hw_get_job_run_end_cmds(&rb_space, job->procid, rb_space_orig, rb_size, gfx_cmd_temp);
	gf_record_evt_submit(device,ring->rb_index, job->base.id, ring->rb_index, ring->rb_index, job->ring_fence->ring_fence_value);

	gf_gpuusage_submit(device, ring->rb_index, job->ring_fence->ring_fence_value);

	trace_gf_run_job(job);

#ifndef FAKE_CHIP
	rb_tail = gf_get_rb_space(&gfx_rb->gfx_br, pad, gfx_rb->gfx_br.mmio_head, gfx_rb->gfx_br.mmio_tail, rb_size);
	gf_memcpy(rb_tail, rb_space_orig, rb_size * 4);
	job->rb = rb_tail;
	job->rb_size = rb_size;
	if(device->dump_rb_cmd){
		gf_dump_commands("gfx rb", rb_space_orig, rb_size);
		if(job->kcommand)
			gf_dump_commands("idb", (void*)job->kcommand->cpu_virt_addr, job->kcommand->size/4);
	}
	gfx_rb->gfx_br.free_size -= rb_size;
	gfx_rb->gfx_br.tail_virtual_addr += rb_size * 4;
	tail = (unsigned int)(gfx_rb->gfx_br.tail_virtual_addr - gfx_rb->gfx_br.rb_virtual_addr);

	ASSERT(tail <= gfx_rb->gfx_br.size * 4);
	barrier_rw_local(ring->device->barrier_buf->virt_addres, ring->rb_index, ring->device->mhb_num, job->ring_fence->ring_fence_value);

	gf_write_mmio32(gfx_rb->gfx_br.mmio_tail, tail);
#endif

	gf_free(rb_space_orig);

	return &job->ring_fence->base;
}

struct dma_fence *gf_gce_job_run(struct lisuan_sched_job *sched_job)
{
	struct lisuan_ring *ring = container_of(sched_job->sched, struct lisuan_ring, base);
	struct gf_job *job = container_of(sched_job, struct gf_job, base);
	struct gf_gce_ringbuffer *gce_rb = (struct gf_gce_ringbuffer *)ring->chip_priv;
	struct lisuan_device *device = ring->device;
	void *gce_cmd_temp = device->rb_cmd_template;
	unsigned char *rb_space, *rb_space_orig;
	unsigned char *rb_tail;
	unsigned int *rb_space_int;
	int i, need_invalidate_pte=0, bypass_update=0, pad = 0;
	unsigned int rb_size = 0, tail;
	unsigned int is_reserve_vm = !job->vm;

	/* if gpu reset, hw fence will be replaced here */
	if (job->ring_fence){
		dma_fence_put(&job->ring_fence->base);
		job->need_vm_inv = !!job->vm; //force vm inv
	}
	job->ring_fence = lisuan_ring_fence_create(ring);
	dma_fence_get(&job->ring_fence->base);

	//replace with lastest update job fence
	if(job->vm){
		mutex_lock(&job->vm->pt_lock->lock);
		dma_fence_replace(&job->vm->last_used[ring->rb_index].fence, &sched_job->s_fence->finished);
		mutex_unlock(&job->vm->pt_lock->lock);
	}

	rb_size = gf_hw_get_job_run_rb_size(job->dma_count, device->is_virtfn, GF_CMD_TYPE_GCE);
	
	rb_space_orig =
	rb_space = (unsigned char *)gf_calloc(rb_size * 4);

	gf_hw_get_job_run_begin_cmds(&rb_space, job->procid, gce_cmd_temp, GF_CMD_TYPE_GCE);
	
	gf_tlb_inv(sched_job, &rb_space);

	for (i = 0; i < job->dma_count; i++) {
		gf_hw_get_job_run_dma_cmds(&rb_space, job->dma_info[i].idb1_index, job->dma_info[i].idb_type, 
			job->dma_info[i].dma_size_in_byte, job->dma_info[i].dma_gpu_va, device->is_virtfn, gce_cmd_temp);
	}

	gf_hw_get_job_run_fence_cmds(&rb_space, false, ring->fence_node.start, job->ring_fence->ring_fence_value, gce_cmd_temp, GF_CMD_TYPE_GCE);
	// send ifence to wait efence write finished
	if (ring->device->is_virtfn)
	{
		gf_hw_emit_vf_efence(&rb_space);
	}

	gf_hw_get_job_run_end_cmds(&rb_space, job->procid, rb_space_orig, rb_size, gce_cmd_temp);
	// record submit event
	//gf_record_evt_submit(job->pid, job->base.id, ring->rb_index, ring->rb_index, ring_fence->ring_fence_value);
	gf_record_evt_submit(device, ring->rb_index, job->base.id, ring->rb_index, ring->rb_index, job->ring_fence->ring_fence_value);

	gf_gpuusage_submit(device, ring->rb_index, job->ring_fence->ring_fence_value);

	trace_gf_run_job(job);

#ifndef FAKE_CHIP
	rb_tail = gf_get_rb_space(&gce_rb->gce_br, pad, gce_rb->gce_br.mmio_head, gce_rb->gce_br.mmio_tail, rb_size);
	gf_memcpy(rb_tail, rb_space_orig, rb_size * 4);
	job->rb = rb_tail;
	job->rb_size = rb_size;

	if(device->dump_rb_cmd){
		gf_dump_commands("gce rb", rb_space_orig, rb_size);
		if(job->kcommand)
			gf_dump_commands("idb", (void*)job->kcommand->cpu_virt_addr, job->kcommand->size/4);
	}
	gce_rb->gce_br.free_size -= rb_size;
	gce_rb->gce_br.tail_virtual_addr += rb_size * 4;
	tail = (unsigned int)(gce_rb->gce_br.tail_virtual_addr - gce_rb->gce_br.rb_virtual_addr);

	ASSERT(tail <= gce_rb->gce_br.size * 4);
	barrier_rw_local(ring->device->barrier_buf->virt_addres,ring->rb_index, ring->device->mhb_num, job->ring_fence->ring_fence_value);
	gf_write_mmio32(gce_rb->gce_br.mmio_tail, tail);
#endif
	gf_free(rb_space_orig);

	return &job->ring_fence->base;
}

struct dma_fence *gf_vem_job_run(struct lisuan_sched_job *sched_job)
{
	struct lisuan_ring *ring = container_of(sched_job->sched, struct lisuan_ring, base);
	struct gf_job *job = container_of(sched_job, struct gf_job, base);
	struct gf_vem_ringbuffer *vem_rb = (struct gf_vem_ringbuffer *)ring->chip_priv;
	struct lisuan_device *device = ring->device;
	void *vem_cmd_temp = ring->device->rb_cmd_template;
	unsigned char *rb_space, *rb_space_orig;
	unsigned int *rb_space_int;
	unsigned char *rb_tail;
	unsigned int rb_size = 0, tail, pad=0, i;
	unsigned int is_reserve_vm = !job->vm;

	/* if gpu reset, hw fence will be replaced here */
	if (job->ring_fence){
		dma_fence_put(&job->ring_fence->base);
		job->need_vm_inv = !!job->vm; //force vm inv
	}

	job->ring_fence = lisuan_ring_fence_create(ring);
	dma_fence_get(&job->ring_fence->base);

	//replace with lastest update job fence
	if(job->vm){
		mutex_lock(&job->vm->pt_lock->lock);
		dma_fence_replace(&job->vm->last_used[ring->rb_index].fence, &sched_job->s_fence->finished);
		mutex_unlock(&job->vm->pt_lock->lock);
	}

	rb_size = gf_hw_get_job_run_rb_size(job->vem_cmd_size, device->is_virtfn, GF_CMD_TYPE_VEM);

	rb_space_orig =
	rb_space = (unsigned char *)gf_calloc(rb_size * 4);

	gf_hw_get_job_run_begin_cmds(&rb_space, job->procid, vem_cmd_temp, GF_CMD_TYPE_VEM);
	gf_tlb_inv(sched_job, &rb_space);

	gf_memcpy(rb_space, job->vem_cmd, job->vem_cmd_size);
	rb_space += job->vem_cmd_size;
	
	gf_hw_get_job_run_fence_cmds(&rb_space, false, ring->fence_node.start, job->ring_fence->ring_fence_value, vem_cmd_temp, GF_CMD_TYPE_VEM);

	if (ring->device->is_virtfn)
	{
		gf_hw_emit_vf_efence(&rb_space);
	}

	gf_hw_get_job_run_end_cmds(&rb_space, job->procid, rb_space_orig, rb_size, vem_cmd_temp);

	// record submit event
	//gf_record_evt_submit(job->pid, job->base.id, ring->rb_index, ring->rb_index, ring_fence->ring_fence_value);
	gf_record_evt_submit(device, ring->rb_index, job->base.id, ring->rb_index, ring->rb_index, job->ring_fence->ring_fence_value);

	gf_gpuusage_submit(device, ring->rb_index, job->ring_fence->ring_fence_value);

	trace_gf_run_job(job);

#ifndef FAKE_CHIP
	rb_tail = gf_get_rb_space(&vem_rb->vem_br, pad, vem_rb->vem_br.mmio_head, vem_rb->vem_br.mmio_tail, rb_size);
	gf_memcpy(rb_tail, rb_space_orig, rb_size * 4);
	job->rb = rb_tail;
	job->rb_size = rb_size;

	if(device->dump_rb_cmd)
		gf_dump_commands("vem rb", rb_space_orig, rb_size);

	vem_rb->vem_br.free_size -= rb_size;
	vem_rb->vem_br.tail_virtual_addr += rb_size * 4;
	tail = (unsigned int)(vem_rb->vem_br.tail_virtual_addr - vem_rb->vem_br.rb_virtual_addr);

	ASSERT(tail <= vem_rb->vem_br.size * 4);
	barrier_rw_local(ring->device->barrier_buf->virt_addres, ring->rb_index, ring->device->mhb_num, job->ring_fence->ring_fence_value);

	gf_write_mmio32(vem_rb->vem_br.mmio_tail, tail);
#endif
	gf_free(rb_space_orig);
	gf_free(job->vem_cmd);
	return &job->ring_fence->base;
}

struct dma_fence *gf_dec_job_run(struct lisuan_sched_job *sched_job)
{
	struct lisuan_ring *ring = container_of(sched_job->sched, struct lisuan_ring, base);
	struct gf_job *job = container_of(sched_job, struct gf_job, base);
	struct lisuan_device *device = ring->device;
	struct lisuan_file *ls_file = job->context->ls_file;
	int ret = 0;

	/* if gpu reset, hw fence will be replaced here */
	if (job->ring_fence)
		dma_fence_put(&job->ring_fence->base);

	job->ring_fence = lisuan_ring_fence_create(ring);
	dma_fence_get(&job->ring_fence->base);

	//replace with lastest update job fence
	if(job->vm){
		mutex_lock(&job->vm->pt_lock->lock);
		dma_fence_replace(&job->vm->last_used[ring->rb_index].fence, &sched_job->s_fence->finished);
		mutex_unlock(&job->vm->pt_lock->lock);
	}

	//gf_record_evt_submit(job->pid, job->base.id, ring->rb_index, ring->rb_index, ring_fence->ring_fence_value);	
	gf_record_evt_submit(device, ring->rb_index, job->base.id, ring->rb_index, ring->rb_index, job->ring_fence->ring_fence_value);

	gf_gpuusage_submit(device, ring->rb_index, job->ring_fence->ring_fence_value);

	ret = gf_dec_link_and_run(device->ddev, &job->dec_para, ls_file->drm_file, job->ring_fence->ring_fence_value,
	                          &sched_job->s_fence->finished, job->procid);
	if (ret != 0) {
		// TODO: release failed dec job
	}
	return &job->ring_fence->base;
}


struct dma_fence *gf_enc_job_run(struct lisuan_sched_job *sched_job)
{
	struct lisuan_ring *ring = container_of(sched_job->sched, struct lisuan_ring, base);
	struct gf_job *job = container_of(sched_job, struct gf_job, base);
	struct lisuan_device *device = ring->device;
	struct lisuan_file *ls_file = job->context->ls_file;
	int ret = 0;

	/* if gpu reset, hw fence will be replaced here */
	if (job->ring_fence)
		dma_fence_put(&job->ring_fence->base);

	job->ring_fence = lisuan_ring_fence_create(ring);
	dma_fence_get(&job->ring_fence->base);

	//replace with lastest update job fence
	if(job->vm){
		mutex_lock(&job->vm->pt_lock->lock);
		dma_fence_replace(&job->vm->last_used[ring->rb_index].fence, &sched_job->s_fence->finished);
		mutex_unlock(&job->vm->pt_lock->lock);
	}

	//gf_record_evt_submit(job->pid, job->base.id, ring->rb_index, ring->rb_index, ring_fence->ring_fence_value);	
	gf_record_evt_submit(device,ring->rb_index, job->base.id, ring->rb_index, ring->rb_index, job->ring_fence->ring_fence_value);

	gf_gpuusage_submit(device, ring->rb_index, job->ring_fence->ring_fence_value);

	ret = gf_enc_link_and_run(device->ddev, &job->enc_para, ls_file->drm_file, job->ring_fence->ring_fence_value, job->procid);

	if (ret != 0) {
		// TODO: release failed enc job
	}

	return &job->ring_fence->base;
}

void gf_job_free_cb(struct lisuan_sched_job *job)
{
	struct gf_job *cleanup_job;
	struct gf_sync_fence *pre_fence, *tmp_fence;
	struct lisuan_gem_bo * pt_buf, *tmp_buf;

	if(job == NULL)
		return;
	
	cleanup_job= container_of(job, struct gf_job, base);
	job_info("free rb:%d job:%d from tgid-pid:%d-%d, procid:%d, efence:0x%llx\n", 
		cleanup_job->context->rb_index, job->id, cleanup_job->tgid, cleanup_job->pid,
		cleanup_job->procid, cleanup_job->ring_fence->ring_fence_value);

	trace_gf_job_free(cleanup_job);
	lisuan_sched_job_cleanup(job);

	if (cleanup_job->ring_fence)
		dma_fence_put(&cleanup_job->ring_fence->base);

	if (cleanup_job->dma_info) {
		gf_kvfree(cleanup_job->dma_info);
	}
	if (cleanup_job->bo_count) {
		gf_kvfree(cleanup_job->bo_list);
	}
	if (cleanup_job->kcommand) {
		cleanup_job->kcommand->free(cleanup_job->kcommand);
		gf_kvfree(cleanup_job->kcommand);
	}

	list_for_each_entry_safe(pre_fence, tmp_fence, &cleanup_job->sync, list_item) {
		list_del(&pre_fence->list_item);
		lisuan_sync_fence_free(pre_fence, true);
	}

	list_for_each_entry_safe(pt_buf, tmp_buf, &cleanup_job->pt_buf_list, free_pt_buffer) {
		list_del(&pt_buf->free_pt_buffer);
		lisuan_gem_free_oneshot(cleanup_job->context->ls_dev, pt_buf);
	}

	if(cleanup_job->evict_gem){
		lisuan_gem_free_oneshot(cleanup_job->context->ls_dev, cleanup_job->evict_gem);
	}

	gf_kvfree(cleanup_job);
}

static struct dma_fence *gf_get_sync_fence(struct gf_job *job)
{
	struct gf_sync_fence *sync,*temp;
	struct dma_fence *fence;

	if(!job)
		return NULL;
	list_for_each_entry_safe(sync, temp, &job->sync, list_item) {
		fence = sync->fence;
		
		list_del(&sync->list_item);	//if job dependency fence finished, now delete it from the sync list
		lisuan_sync_fence_free(sync, false);
		if (!dma_fence_is_signaled(fence)) {
			return fence;
		} else {
			dma_fence_put(fence);
		}
	}
	return NULL;
}

static struct dma_fence *gf_job_dependency(struct lisuan_sched_job *sched_job, struct lisuan_sched_entity *s_entity)
{
	struct dma_fence *fence = NULL;
	unsigned int ready = true;
	struct gf_job *job = container_of(sched_job, struct gf_job, base);
	struct lisuan_vm *vm = job->vm;
	struct lisuan_scheduler *sched = sched_job->sched;
	struct lisuan_ring *ring = container_of(sched, struct lisuan_ring, base);
	int r = 0;

	fence = gf_get_sync_fence(job);

	//update pte job use reserved vm with procid 0 and vm is ALWAYS NULL. 
	while (fence == NULL && !job->populated){

		if(vm && !job->procid)
		{
			r = lisuan_grab_procid(ring, vm, job);
		}
		else if(!vm)
		{
			r = gf_job_populate_vm_dirty_state(&job->base);
		}

		if(!r){
			job->populated = true;
		}

		fence = gf_get_sync_fence(job);
	}
	return fence;
}

static void gf_video_job_timeout(struct lisuan_sched_job *s_job)
{
	struct lisuan_scheduler *sched = s_job->sched;
	struct lisuan_sched_fence *s_fence = s_job->s_fence;
	struct lisuan_ring *ring = container_of(sched, struct lisuan_ring, base);
	struct gf_job *job = container_of(s_job, struct gf_job, base);
	gf_info("job %d timeout: returned_fence:0x%llx last_submit_fence:0x%llx fence=0x%llx, cmd id=%d, job id=%d\n",
			ring->rb_index - 3,
	    	atomic64_read(&ring->returned_fence_id),
			atomic64_read(&ring->last_submit_fence_id),
			*ring->fence_node.cpu_virt_addr,
			job->enc_para.cmdbuf_id,
			job->base.id);

	if (*ring->fence_node.cpu_virt_addr >= atomic64_read(&ring->returned_fence_id) &&
		*ring->fence_node.cpu_virt_addr <= atomic64_read(&ring->last_submit_fence_id))
		atomic64_set(&ring->returned_fence_id, *ring->fence_node.cpu_virt_addr);

	tasklet_schedule(&ring->device->fenceirq_work);
}

static void gf_job_timedout(struct lisuan_sched_job *s_job)
{
	struct lisuan_scheduler *sched = s_job->sched;
	struct lisuan_sched_fence *s_fence = s_job->s_fence;
	struct lisuan_ring *ring = container_of(sched, struct lisuan_ring, base);
	struct lisuan_device *ls_device = ring->device;
	struct gf_job *job = container_of(s_job, struct gf_job, base);
	struct lisuan_sched_job *tmp_job, *tmp;
	unsigned long flags;
	int i = 0;

	if (!lisuan_ring_check_hang(ring, job)){
		spin_lock_irqsave(&sched->job_list_lock, flags);
		list_add(&s_job->node, &sched->ring_mirror_list);
		lisuan_sched_start_timeout(sched);
		spin_unlock_irqrestore(&sched->job_list_lock, flags);
		gf_info("job timeout, but no hw hang detect, go on...\n");
		return;
	}
	if(atomic_cmpxchg(&ls_device->checked_hang, 0, 1) != 0)
		return;

	for (i = 0; i < ls_device->rb_count; i++) {
		struct lisuan_ring *ring_each = ls_device->sched[i];

		if (!ring_each || !ring_each->base.thread)
			continue;
		//mutex_lock(&ring_each->rb_lock->lock);
		lisuan_sched_stop(&ring_each->base, s_job);
	}

	gf_sched_job_dump(&job->base);

	//del job that cause the hw hang
	atomic_dec(&sched->num_jobs);
	spin_lock_irqsave(&sched->job_list_lock, flags);
	list_del_init(&s_job->node);
	spin_unlock_irqrestore(&sched->job_list_lock, flags);

	dma_fence_get(&s_fence->finished);
	if(job->is_signal_job){
		*(volatile unsigned long long *)job->kmd_va = job->signal_value;
	}
	lisuan_sched_fence_finished(s_fence);
	dma_fence_put(&s_fence->finished);

	sched->ops->free_job(s_job);

	if (ls_device->hwctl.recovery_enable) {
		lisuan_mailbox_broadcast_reset_request(ls_device, MAILBOX_CUSTOM_MSG_CHIP_RESET_NOTIFICATION);
		lisuan_sched_recover_gpu(ring);
		//lisuan_mailbox_broadcast_reset_request(ls_device, MAILBOX_CUSTOM_MSG_VF_RESET_FINISH_NOTIFICATION);

		//TODO if reset still hang, need ask PF do whole chip reset when vf enable
	} else {
		//Just assert.
		ASSERT(0);
	}

exit:

	for (i = 0; i < ls_device->rb_count; i++) {
		struct lisuan_ring *ring_each = ls_device->sched[i];

		if (!ring_each || !ring_each->base.thread)
			continue;
		//resubmit jobs in pendinglist
		lisuan_sched_resubmit_jobs(&ring_each->base);
		//lisuan_sched_resume_timeout(&ring_each->base, sched->timeout);
		//mutex_unlock(&ring_each->rb_lock->lock);
		lisuan_sched_start(&ring_each->base, true);
		wake_up_interruptible(&ring_each->base.wake_up_worker);
	}

	atomic_set(&ls_device->checked_hang, 0);
}

void gf_show_job_info(struct lisuan_sched_job *sched_job)
{
	struct gf_job *job = container_of(sched_job, struct gf_job, base);
	struct lisuan_scheduler *sched = sched_job->sched;
	struct lisuan_ctx *context = job->context;
	struct lisuan_device *ls_dev = context->ls_dev;
	struct lisuan_ring *ring = job->ring_fence->ring;
	int i = 0;

	gf_info("job:%d from tgid: %d, ring:%d, submit-return-mem:0x%llx-0x%llx-0x%llx, kcmd size:0x%x", 
		job->base.id, job->tgid, context->rb_index, atomic64_read(&ring->last_submit_fence_id), 
		atomic64_read(&ring->returned_fence_id), *ring->fence_node.cpu_virt_addr, job->kcommand?job->kcommand->size:0);

	//for (i = 0; i < job->dma_count; i++) {
	//	gf_info("Cmd stream:%d, gpu va:0x%llx, cmd size:0x%x\n", i, job->dma_info[i].dma_gpu_va, job->dma_info[i].dma_size_in_byte);
	//}

}

const struct lisuan_sched_backend_ops gfx_sched_ops = {
	.dependency = gf_job_dependency,
	.run_job = gf_gfx_job_run,
	.timedout_job = gf_job_timedout,
	.free_job = gf_job_free_cb,
	.tlb_inv = gf_tlb_inv,
	.show_job_info = gf_show_job_info,
	.dump_job = gf_sched_job_dump,
};

const struct lisuan_sched_backend_ops gce_sched_ops = {
	.dependency = gf_job_dependency,
	.run_job = gf_gce_job_run,
	.timedout_job = gf_job_timedout,
	.free_job = gf_job_free_cb,
	.tlb_inv = gf_tlb_inv,
	.show_job_info = gf_show_job_info,
	.dump_job = gf_sched_job_dump,
};

const struct lisuan_sched_backend_ops vem_sched_ops = {
	.dependency = gf_job_dependency,
	.run_job = gf_vem_job_run,
	.timedout_job = gf_job_timedout,
	.free_job = gf_job_free_cb,
	.tlb_inv = gf_tlb_inv,
	.show_job_info = gf_show_job_info,
	.dump_job = gf_sched_job_dump,
};

const struct lisuan_sched_backend_ops dec_sched_ops = {
	.dependency = gf_job_dependency,
	.run_job = gf_dec_job_run,
	.timedout_job = gf_video_job_timeout,
	.free_job = gf_job_free_cb,
	.tlb_inv = NULL,
	.show_job_info = gf_show_job_info,
	.dump_job = gf_sched_job_dump,
};

const struct lisuan_sched_backend_ops enc_sched_ops = {
	.dependency = gf_job_dependency,
	.run_job = gf_enc_job_run,
	.timedout_job = gf_video_job_timeout,
	.free_job = gf_job_free_cb,
	.tlb_inv = NULL,
	.show_job_info = gf_show_job_info,
	.dump_job = gf_sched_job_dump,
};
