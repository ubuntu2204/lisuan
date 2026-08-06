/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#ifndef __LISUAN_RING_H__
#define __LISUAN_RING_H__

#include "lisuan_scheduler.h"
#include "lisuan_sync.h"

#if defined(__i386__) || defined(__x86_64__)
#define lisuan_barrier()    __asm__ __volatile__("mfence": : :"memory")
#else
#define lisuan_barrier()    mb()
#endif


struct lisuan_ring_fence;
struct gf_job;
extern const struct lisuan_sched_backend_ops gfx_sched_ops;
extern const struct lisuan_sched_backend_ops gce_sched_ops;
extern const struct lisuan_sched_backend_ops vem_sched_ops;
extern const struct lisuan_sched_backend_ops dec_sched_ops;
extern const struct lisuan_sched_backend_ops enc_sched_ops;

struct lisuan_ring {
	struct lisuan_device *device;
	unsigned int rb_index;
	struct lisuan_scheduler base;

	void *chip_priv;

	struct os_mutex *rb_lock;
	unsigned int rb_submit_limit;

	struct lisuan_kbo_node fence_node;

	//TODO: last_submit_fence & returned_fence_id should be code
	//used for ring_fence,job_run should return the fence
	struct lisuan_ring_fence ring_fence;
	spinlock_t ring_fence_lock;

	atomic64_t last_submit_fence_id;
	atomic64_t returned_fence_id;

	char			name[16];
};

/*
 * According to the design specification, the maximum size of an IDB submission 
 * in a ring buffer is 18 bits
 * 
 * defined in G100_GPU_OPCODE.h
 * typedef struct _Cmd_Idb                           // IDB command
 * {
 *          unsigned int     Dwn                   : 18;  // how many dwords to read from indirect buffer
 *
 *  which corresponds to 0x100000 byte. 
 * 
 * To be on the safe side, we reserve some margin and define it as 0xF0000. 
 */
#define RING_IDB_BUFFER_SIZE_MAX 0xFF000

void lisuan_sched_status_show(struct lisuan_device * ls_dev);
void lisuan_dump_debugbus(struct lisuan_device * ls_dev);
void lisuan_ring_dump_jobs(struct lisuan_device * ls_dev);

void lisuan_init_ring(struct lisuan_device *ls_dev);
void lisuan_fini_ring(struct lisuan_device *ls_dev);
int lisuan_ring_check_hang(struct lisuan_ring *ring, struct gf_job *job);
void lisuan_sched_recover_gpu(struct lisuan_ring *sched);
void lisuan_sched_fence_irq_worklet(struct lisuan_device *ls_dev);

static inline void 
barrier_rw_local(void *barrier_buf_va, int engine_offset, int mhb_num, unsigned long long wait_value)
{
	int i=0;
	void * pDate = (barrier_buf_va + engine_offset * 8);// 3d use offset 0(0x100, 0x200...), gce use offset 8(0x108, 0x208..)...

	for(i=0;i<mhb_num;i++)//now the 1ch,3ch,6ch are correct ,need check 4&6 ch mhb_id(TODO)
	{
		lisuan_barrier();
		*(volatile unsigned long long *)(pDate+0x100*i) = wait_value;
		while(*(volatile unsigned long long *)(pDate + 0x100*i) != wait_value){int n=0; n++;}
		lisuan_barrier();
	}
	wmb();
}

#endif
