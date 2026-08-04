/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#ifndef __GF_RINGBUFFER_H__
#define __GF_RINGBUFFER_H__

#include "lisuan_ring.h"
#include "lisuan_device.h"
#include "gf_hw_cmds.h"

typedef struct gf_ring_buffer {
	unsigned char *mmio_start;
	unsigned char *mmio_head;
	unsigned char *mmio_tail;
	unsigned char *mmio_size;

	struct lisuan_ring *ring;
	struct lisuan_gem_bo *buf;
	struct lisuan_gem_bo *krestore;
	struct lisuan_gem_bo *ksave;

	unsigned int size;
	unsigned int free_size;
	unsigned char *tail_virtual_addr;
	unsigned char *rb_virtual_addr;
} gf_base_ring;

struct gf_gfx_ringbuffer {
	struct gf_ring_buffer gfx_br;
};

struct gf_gce_ringbuffer {
	struct gf_ring_buffer gce_br;
	void *gce_host_upload;
	void *gce_clr;
	void *gce_blt;
};

struct gf_vem_ringbuffer {
	struct gf_ring_buffer vem_br;
};

struct gf_dec_ringbuffer {
	struct gf_ring_buffer dec_br;
	struct exchange_parameter para;
};

struct gf_enc_ringbuffer {
	struct gf_ring_buffer enc_br;
	struct enc_exchange_parameter para;
};

void gf_scheduler_reinit(struct lisuan_device *ls_dev, int rb_index, struct lisuan_ring *sched);
void gf_ringbuffer_init(struct lisuan_ring *ring, int rb_index);
void gf_init_gfx_ring(struct lisuan_ring *ring, int rb_index);
void gf_init_gce_ring(struct lisuan_ring *ring, int rb_index);
void gf_init_vem_ring(struct lisuan_ring *ring, int rb_index);
void gf_init_dec_ring(struct lisuan_ring *ring, int rb_index);
void gf_init_enc_ring(struct lisuan_ring *ring, int rb_index);

void gf_init_gpu_state(struct lisuan_ring *sched);

unsigned char *gf_get_rb_space(struct gf_ring_buffer *rb, int pad, unsigned char *mmio_head, unsigned char *mmio_tail, unsigned int size);
void gf_ringbuffer_deinit(struct lisuan_device *ls_dev, int pad, struct lisuan_ring *sched);
#endif
