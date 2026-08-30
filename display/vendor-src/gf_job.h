/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#ifndef __GFGPU_JOB_H__
#define __GFGPU_JOB_H__

#include <linux/kthread.h>

#include "lisuan_scheduler.h"
#include "lisuan_procid.h"
#include "lisuan_device.h"

struct lisuan_ring;

struct gf_job {
	struct lisuan_sched_job base;
	struct lisuan_ctx *context;

	unsigned int s_priority;

	unsigned int dma_count;
	struct dma_info_s *dma_info;

	unsigned int vem_cmd_size;
	unsigned int *vem_cmd;

	//for gpu upload pte only
	unsigned int *shadow_idb;
	unsigned int *p_cmd;
	unsigned int *p_cmd_base;

	unsigned int *p_cmd_lastet_truncation;
	unsigned int truncation_point[64];
	unsigned long truncation_point_count;

	unsigned int bo_count;
	struct lisuan_gem_bo **bo_list;
	bool is_signal_job;
	volatile unsigned long long *kmd_va;//signal job monitor fence kmd_va
	unsigned long long signal_value;//signal job wait value

	struct list_head sync;	//dependency fence

	struct list_head job_pool_item;

	struct list_head submitted_item;
	//for all bo_list's gem buf finish va mapping check.
	struct completion prepare_ready;
	struct work_struct prepare_work;

	//used for job timeout check.
	struct delayed_work timeout_work;

	int procid;
	struct lisuan_kbo_node *kcommand;	//only for kmd dma cmd 
	//user fence handling
	uint64_t uf_sequence;
	struct lisuan_ring_fence *ring_fence;
	struct lisuan_vm *vm; //null means from reserved vm for kmd
	unsigned long tgid;//processid
	unsigned long pid;//threadid for thread, pid for process

	struct list_head pt_buf_list;	//for gpu upload pt
	unsigned int need_vm_inv;

	unsigned int cur_vm_L0_dirty;
	unsigned int cur_vm_L1_dirty;
	unsigned int cur_vm_L2_dirty;
	unsigned int bypass_L2_dirty;

	void * evict_gem;
	void * rb;
	unsigned int rb_size;

	unsigned int populated;

	struct exchange_parameter dec_para;
	struct enc_exchange_parameter enc_para;
};

struct dump_chip_image_header_s
{
	uint32_t magic_number;
	uint32_t block_cnt;
};

struct dump_chip_image_block_info_s
{
	uint32_t block_id;
	uint32_t reg_offset;
	uint32_t reg_count;
};

void gf_sched_cleanup_finish_job(struct lisuan_ring *sched);
struct dma_fence *gf_gfx_job_run(struct lisuan_sched_job *sched_job);
struct dma_fence *gf_gce_job_run(struct lisuan_sched_job *sched_job);
struct dma_fence *gf_vem_job_run(struct lisuan_sched_job *sched_job);
void gf_job_free_cb(struct lisuan_sched_job *s_job);
int gf_job_populate_vm_dirty_state(struct lisuan_sched_job *sched_job);
int gf_job_is_vm_busy(struct lisuan_sched_job *sched_job);

#endif
