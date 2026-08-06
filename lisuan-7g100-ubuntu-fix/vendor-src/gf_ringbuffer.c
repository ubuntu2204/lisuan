/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#include "lisuan_device.h"
#include "lisuan_ring.h"
#include "lisuan_gem.h"
#include "lisuan_vm.h"
#include "gf_hw_misc.h"
#include "gf_hw_cmds.h"
#include "lisuan_vm.h"

#include "gf_ringbuffer.h"
#include "gf_irq.h"

void gf_init_gfx_ring(struct lisuan_ring *ring, int rb_index)
{
	struct lisuan_device *ls_dev = ring->device;
	struct gf_gfx_ringbuffer *gfx_rb = (struct gf_gfx_ringbuffer *)gf_kvcalloc(1, sizeof(struct gf_gfx_ringbuffer), GFP_KERNEL);
	struct lisuan_gem_bo *buf, *krestore,*ksave;
	struct gf_ring_buffer *gfx_br;
	int ret = 0;
	struct lisuan_vm_node *dummy_vm_node = container_of(ls_dev->dummy_page_buf->va_list.next, struct lisuan_vm_node, node_item);

	buf = gf_kvcalloc(1, sizeof(struct lisuan_gem_bo), GFP_KERNEL);
	buf->size = 64 * 1024;
	buf->align = 64 * 1024;
	buf->domain = ls_dev->bufsel.ringbuf_cmd_buffer_dom;
	buf->flag.byPass = 1;
	buf->pm_discard = 1;
	atomic_set(&buf->unpagable, 1);
	buf->priority = gem_priority_high;
	buf->flag.vram = (buf->domain <= dom_invisual_vram) ? 1 : 0;
	buf->flag.snoop = ((buf->domain == dom_snoop_system_pages) ? 1 : 0);

	ret = lisuan_gem_alloc_oneshot(ls_dev, NULL, buf, 1);
	if (ret)
		return;
	gfx_rb->gfx_br.buf = buf;

	if(ls_dev->is_virtfn){
		unsigned int * p_cmd_save = NULL, * p_cmd_restore = NULL;
		krestore = gf_kvcalloc(1, sizeof(struct lisuan_gem_bo), GFP_KERNEL);
		krestore->size = 4 * 1024;
		krestore->align = 4 * 1024;
		krestore->domain = ls_dev->bufsel.internal_cmd_buffer_dom;
		krestore->flag.byPass = 1;
		atomic_set(&krestore->unpagable, 1);
		krestore->priority = gem_priority_high;
		krestore->flag.vram = (krestore->domain <= dom_invisual_vram) ? 1 : 0;
		krestore->flag.snoop = ((krestore->domain == dom_snoop_system_pages) ? 1 : 0);

		ret = lisuan_gem_alloc_oneshot(ls_dev, NULL, krestore, 1);
		if (ret)return;
		gfx_rb->gfx_br.krestore = krestore;

		ksave = gf_kvcalloc(1, sizeof(struct lisuan_gem_bo), GFP_KERNEL);
		ksave->size = 4 * 1024;
		ksave->align = 4 * 1024;
		ksave->domain = ls_dev->bufsel.internal_cmd_buffer_dom;
		ksave->flag.byPass = 1;
		atomic_set(&ksave->unpagable, 1);
		ksave->priority = gem_priority_high;
		ksave->flag.vram = (ksave->domain <= dom_invisual_vram) ? 1 : 0;
		ksave->flag.snoop = ((ksave->domain == dom_snoop_system_pages) ? 1 : 0);

		ret = lisuan_gem_alloc_oneshot(ls_dev, NULL, ksave, 1);
		if (ret)return;
		gfx_rb->gfx_br.ksave = ksave;

		p_cmd_save = ksave->virt_addres;
		p_cmd_restore = krestore->virt_addres;
		gf_hw_init_save_restore_cmds(ring->rb_index, (void**)&p_cmd_save, (void**)&p_cmd_restore, dummy_vm_node->offset);
		ksave->cmd_size = (void*)p_cmd_save - ksave->virt_addres;
		krestore->cmd_size = (void*)p_cmd_restore - krestore->virt_addres;
	}

	gfx_br = &gfx_rb->gfx_br;
	gfx_br->ring = ring;
	ring->chip_priv = gfx_rb;

	gf_ringbuffer_init(ring, rb_index);
}

void gf_init_gce_ring(struct lisuan_ring *ring, int rb_index)
{
	struct lisuan_device *ls_dev = ring->device;
	struct gf_gce_ringbuffer *gce_rb = (struct gf_gce_ringbuffer *)gf_kvcalloc(1, sizeof(struct gf_gce_ringbuffer), GFP_KERNEL);
	unsigned int domain = ls_dev->bufsel.ringbuf_cmd_buffer_dom;
	struct lisuan_gem_bo *buf, *krestore,*ksave;

	int ret = 0;

	struct gf_ring_buffer *gce_br;
	struct lisuan_vm_node *vm_node = container_of(ls_dev->dummy_page_buf->va_list.next, struct lisuan_vm_node, node_item);

	buf = gf_kvcalloc(1, sizeof(struct lisuan_gem_bo), GFP_KERNEL);
	buf->size = 64 * 1024;
	buf->align = 64 * 1024;
	buf->domain = domain;
	buf->flag.byPass = 1;
	buf->pm_discard = 1;
	atomic_set(&buf->unpagable, 1);
	buf->priority = gem_priority_high;
	buf->flag.vram = (buf->domain <= dom_invisual_vram) ? 1 : 0;
	buf->flag.snoop = ((buf->domain == dom_snoop_system_pages) ? 1 : 0);

	ret = lisuan_gem_alloc_oneshot(ls_dev, NULL, buf, 1);
	if (ret)
		return;
	gce_rb->gce_br.buf = buf;

	if(ls_dev->is_virtfn){
		unsigned int * p_cmd_save = NULL, * p_cmd_restore = NULL;

		krestore = gf_kvcalloc(1, sizeof(struct lisuan_gem_bo), GFP_KERNEL);
		krestore->size = 4 * 1024;
		krestore->align = 4 * 1024;
		krestore->domain = ls_dev->bufsel.internal_cmd_buffer_dom;
		krestore->flag.byPass = 1;
		atomic_set(&krestore->unpagable, 1);
		krestore->priority = gem_priority_high;
		krestore->flag.vram = (krestore->domain <= dom_invisual_vram) ? 1 : 0;
		krestore->flag.snoop = ((krestore->domain == dom_snoop_system_pages) ? 1 : 0);

		ret = lisuan_gem_alloc_oneshot(ls_dev, NULL, krestore, 1);
		if (ret)return;
		gce_rb->gce_br.krestore = krestore;

		ksave = gf_kvcalloc(1, sizeof(struct lisuan_gem_bo), GFP_KERNEL);
		ksave->size = 4 * 1024;
		ksave->align = 4 * 1024;
		ksave->domain = ls_dev->bufsel.internal_cmd_buffer_dom;
		ksave->flag.byPass = 1;
		atomic_set(&ksave->unpagable, 1);
		ksave->priority = gem_priority_high;
		ksave->flag.vram = (ksave->domain <= dom_invisual_vram) ? 1 : 0;
		ksave->flag.snoop = ((ksave->domain == dom_snoop_system_pages) ? 1 : 0);

		ret = lisuan_gem_alloc_oneshot(ls_dev, NULL, ksave, 1);
		if (ret)return;
		gce_rb->gce_br.ksave = ksave;

		p_cmd_save = ksave->virt_addres;
		p_cmd_restore = krestore->virt_addres;
		gf_hw_init_save_restore_cmds(ring->rb_index, (void**)&p_cmd_save, (void**)&p_cmd_restore, vm_node->offset);
		ksave->cmd_size = (void*)p_cmd_save - ksave->virt_addres;
		krestore->cmd_size = (void*)p_cmd_restore - krestore->virt_addres;
	}

	//init the gf_ring_buffer struct
	gce_br = &gce_rb->gce_br;
	gce_br->ring = ring;
	ring->chip_priv = gce_rb;
	
	gce_rb->gce_host_upload = gf_kvcalloc(1, gf_hw_get_gce_cmd_size(GF_GCE_HOST_DATA_UPLOAD), GFP_KERNEL);
	gce_rb->gce_clr = gf_kvcalloc(1, gf_hw_get_gce_cmd_size(GF_GCE_CLR_CMDS), GFP_KERNEL);
	gce_rb->gce_blt = gf_kvcalloc(1, gf_hw_get_gce_cmd_size(GF_GCE_BLT_CMDS), GFP_KERNEL);

	gf_hw_init_gce_host_cmds(&gce_rb->gce_host_upload, ls_dev->hwctl.enable_gl2);
	gf_hw_init_gce_clr_cmds(&gce_rb->gce_clr, ls_dev->hwctl.enable_gl2);
	gf_hw_init_gce_blt_cmds(&gce_rb->gce_blt);

	gf_ringbuffer_init(ring, rb_index);
}

void gf_init_vem_ring(struct lisuan_ring *ring, int rb_index)
{
	struct lisuan_device *ls_dev = ring->device;
	struct gf_vem_ringbuffer *vem_rb = (struct gf_vem_ringbuffer *)gf_kvcalloc(1, sizeof(struct gf_vem_ringbuffer), GFP_KERNEL);
	struct gf_ring_buffer *vem_br = &vem_rb->vem_br;
	unsigned int domain = ls_dev->bufsel.ringbuf_cmd_buffer_dom;
	struct lisuan_gem_bo *buf;
	int ret = 0;

	buf = gf_kvcalloc(1, sizeof(struct lisuan_gem_bo), GFP_KERNEL);
	buf->size = 64 * 1024;
	buf->align = 64 * 1024;
	buf->domain = domain;
	buf->flag.byPass = 1;
	buf->pm_discard = 1;
	atomic_set(&buf->unpagable, 1);
	buf->priority = gem_priority_high;
	buf->flag.vram = (buf->domain <= dom_invisual_vram) ? 1 : 0;
	buf->flag.snoop = ((buf->domain == dom_snoop_system_pages) ? 1 : 0);

	ret = lisuan_gem_alloc_oneshot(ls_dev, NULL, buf, 1);
	if (ret)
		return;
	vem_br->buf = buf;

	//init the gf_ring_buffer struct
	vem_br->ring = ring;
	ring->chip_priv = vem_rb;

	gf_ringbuffer_init(ring, rb_index);
}

void gf_init_dec_ring(struct lisuan_ring *ring, int rb_index)
{
	struct lisuan_device *ls_dev = ring->device;
	struct gf_dec_ringbuffer *dec_rb = (struct gf_dec_ringbuffer *)gf_kvcalloc(1, sizeof(struct gf_dec_ringbuffer), GFP_KERNEL);
	struct gf_ring_buffer *dec_br;
	int ret = 0;

	dec_br = &dec_rb->dec_br;
	dec_br->ring = ring;
	ring->chip_priv = dec_rb;

	*ring->fence_node.cpu_virt_addr = atomic64_read(&ring->last_submit_fence_id);	//set fence to lasted state.
	atomic64_set(&ring->returned_fence_id, atomic64_read(&ring->last_submit_fence_id));
}

void gf_init_enc_ring(struct lisuan_ring *ring, int rb_index)
{
	struct lisuan_device *ls_dev = ring->device;
	struct gf_enc_ringbuffer *enc_rb = (struct gf_enc_ringbuffer *)gf_kvcalloc(1, sizeof(struct gf_enc_ringbuffer), GFP_KERNEL);
	struct gf_ring_buffer *enc_br;
	int ret = 0;

	enc_br = &enc_rb->enc_br;
	enc_br->ring = ring;
	ring->chip_priv = enc_rb;
	*ring->fence_node.cpu_virt_addr = atomic64_read(&ring->last_submit_fence_id);	//set fence to lasted state.
	atomic64_set(&ring->returned_fence_id, atomic64_read(&ring->last_submit_fence_id));
}

void gf_ringbuffer_init(struct lisuan_ring *ring, int rb_index)
{
	struct lisuan_device *ls_dev = ring->device;
	struct gf_ring_buffer *ringbuffer = (struct gf_ring_buffer *)ring->chip_priv;
	struct lisuan_vm_node *vm_node = container_of(ringbuffer->buf->va_list.next, struct lisuan_vm_node, node_item);

	ringbuffer->free_size = 
	ringbuffer->size = ringbuffer->buf->size / 4;
	ringbuffer->tail_virtual_addr = 
	ringbuffer->rb_virtual_addr = ringbuffer->buf->virt_addres;

	 *ring->fence_node.cpu_virt_addr = atomic64_read(&ring->last_submit_fence_id);	 //set fence to lasted state.
	 atomic64_set(&ring->returned_fence_id, atomic64_read(&ring->last_submit_fence_id));

     if((rb_index == RB_INDEX_GFX && ls_dev->hwctl.no_gfx) ||
        (rb_index == RB_INDEX_GCE && ls_dev->hwctl.no_gce) ||
        (rb_index == RB_INDEX_VEM && ls_dev->hwctl.no_vem))
        return ;


    gf_init_rb_hw(rb_index, ls_dev->mmio_base, ls_dev->is_virtfn,
                    vm_node->offset >> 12, ringbuffer->buf->size,
                    &ringbuffer->mmio_start,
                    &ringbuffer->mmio_size,
                    &ringbuffer->mmio_head,
                    &ringbuffer->mmio_tail);
}

void gf_scheduler_reinit(struct lisuan_device *ls_dev, int rb_index, struct lisuan_ring *sched)
{
	if (rb_index <= RB_INDEX_VEM)
		gf_ringbuffer_init(sched, rb_index);
}

unsigned char *gf_get_rb_space(struct gf_ring_buffer *rb, int pad, unsigned char *mmio_head, unsigned char *mmio_tail, unsigned int size)
{
	unsigned int head;
	unsigned int tail;
	void *gfx_cmd_temp = rb->ring->device->rb_cmd_template;

	if (size >= rb->free_size) {
	head = gf_read_mmio32(mmio_head);

	if ((rb->tail_virtual_addr + size * 4) >= (rb->rb_virtual_addr + rb->size * 4)) {
		unsigned int skip_size;

		tail = gf_read_mmio32(mmio_tail);
		while ((size * 4 >= head) || (head > tail)) {
		    head = gf_read_mmio32(mmio_head);
		}
		rb->free_size = head / 4;

		skip_size = (unsigned int)(rb->rb_virtual_addr + rb->size * 4 - rb->tail_virtual_addr);

		if (skip_size) {
			char* rb_ptr = rb->tail_virtual_addr;

			gf_memset(rb_ptr, 0, skip_size);

			if(rb->ring->device->is_virtfn && rb->ring->rb_index <= RB_INDEX_GCE) {
				struct lisuan_vm_node *vm_node_krestore = container_of(rb->krestore->va_list.next, struct lisuan_vm_node, node_item);
				struct lisuan_vm_node *vm_node_ksave  = container_of(rb->ksave->va_list.next, struct lisuan_vm_node, node_item);

				gf_send_skip_cmd(&rb_ptr, skip_size, rb->ring->rb_index,
					vm_node_ksave->offset, rb->ksave->cmd_size,
					vm_node_krestore->offset, rb->krestore->cmd_size,
					gfx_cmd_temp);
			}
		}

		rb->tail_virtual_addr = rb->rb_virtual_addr;
	} else {
		tail = (unsigned int)(rb->tail_virtual_addr - rb->rb_virtual_addr);

		while (head > tail) {
			head = gf_read_mmio32(mmio_head);
		}
		rb->free_size = rb->size - (tail >> 2);}
	}

	return rb->tail_virtual_addr;
}

void gf_init_gpu_state(struct lisuan_ring *sched)
{
	unsigned int tail;
	struct gf_gfx_ringbuffer *gfx_rb = sched->chip_priv;
	struct lisuan_device *ls_dev = sched->device;

	int pad = 0;
	unsigned int rb_uint_size = 16 + 2 * 16;
	unsigned int *rb_tail;

	spin_lock(&ls_dev->bypass_lock);
	if(ls_dev->is_bypass_update)
		gf_mmu_bypass_inv(ls_dev->mmio_base);
	spin_unlock(&ls_dev->bypass_lock);

	rb_tail = (unsigned int *)gf_get_rb_space((struct gf_ring_buffer *)gfx_rb, pad, gfx_rb->gfx_br.mmio_head, gfx_rb->gfx_br.mmio_tail, rb_uint_size);

	gf_memset(rb_tail, 0, rb_uint_size * 4);

	gf_hw_get_init_gpu_state_cmds(&rb_tail, sched->device->rb_cmd_template, ls_dev->is_virtfn, ls_dev->hwctl.en_new_cl_clr);

	gfx_rb->gfx_br.tail_virtual_addr += rb_uint_size * 4;

	tail = (unsigned int)(gfx_rb->gfx_br.tail_virtual_addr - gfx_rb->gfx_br.rb_virtual_addr);

	ASSERT(tail <= gfx_rb->gfx_br.size * 4);

	gfx_rb->gfx_br.free_size -= rb_uint_size;

	barrier_rw_local(ls_dev->barrier_buf->virt_addres, gfx_rb->gfx_br.ring->rb_index, ls_dev->mhb_num, 0xFF);

	gf_write_mmio32(gfx_rb->gfx_br.mmio_tail, tail);
}

void gf_ringbuffer_deinit(struct lisuan_device *ls_dev, int rb_index, struct lisuan_ring *sched)
{
	struct gf_ring_buffer *ringbuffer = (struct gf_ring_buffer *)sched->chip_priv;

	if(rb_index == RB_INDEX_GFX)
	{
		struct gf_gfx_ringbuffer *gfx_rb = (struct gf_gfx_ringbuffer *)sched->chip_priv;
		struct lisuan_vm_node *dummy_vm_node = container_of(ls_dev->dummy_page_buf->va_list.next, struct lisuan_vm_node, node_item);
		unsigned int rb_uint_size = 32, pad=0, tail=0;
		unsigned int *dma_ptr = NULL;

		dma_ptr = (unsigned int *)gf_get_rb_space((struct gf_ring_buffer *)gfx_rb, pad, gfx_rb->gfx_br.mmio_head, gfx_rb->gfx_br.mmio_tail, rb_uint_size);

		gf_memset(dma_ptr, 0, rb_uint_size * 4);
		
		gf_hw_get_gfx_rb_deinit_cmds(&dma_ptr,dummy_vm_node->offset, &ls_dev->rb_cmd_template);
		
		gfx_rb->gfx_br.tail_virtual_addr += rb_uint_size * 4;
		tail = (unsigned int)(gfx_rb->gfx_br.tail_virtual_addr - gfx_rb->gfx_br.rb_virtual_addr);

		ASSERT(tail <= gfx_rb->gfx_br.size * 4);
		gfx_rb->gfx_br.free_size -= rb_uint_size;
		
		barrier_rw_local(ls_dev->barrier_buf->virt_addres, gfx_rb->gfx_br.ring->rb_index, ls_dev->mhb_num, 0xFF);

		gf_write_mmio32(gfx_rb->gfx_br.mmio_tail, tail);
	}
	else if(rb_index == RB_INDEX_GCE)
	{
		struct gf_gce_ringbuffer *gce_rb = (struct gf_gce_ringbuffer *)sched->chip_priv;
		if(gce_rb->gce_host_upload)
			gf_kvfree(gce_rb->gce_host_upload);
		if(gce_rb->gce_clr)
			gf_kvfree(gce_rb->gce_clr);
		if(gce_rb->gce_blt)
			gf_kvfree(gce_rb->gce_blt);
	}

	while(*sched->fence_node.cpu_virt_addr < atomic64_read(&sched->last_submit_fence_id) && 
		gf_read_mmio32(ringbuffer->mmio_head) != gf_read_mmio32(ringbuffer->mmio_tail))
	{
			gf_msleep(5);
	}
	gf_write_mmio32(ringbuffer->mmio_start, 0);//reset kick 

	if(ls_dev->is_virtfn && ringbuffer->krestore)
		lisuan_gem_free_oneshot(ls_dev, ringbuffer->krestore);
	if(ls_dev->is_virtfn && ringbuffer->ksave)
		lisuan_gem_free_oneshot(ls_dev, ringbuffer->ksave);

	if(ringbuffer->buf)
		lisuan_gem_free_oneshot(ls_dev, ringbuffer->buf);

	gf_kvfree(sched->chip_priv);
}

