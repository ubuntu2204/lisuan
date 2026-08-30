/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#ifndef __LISUAN_PROCID_H__
#define __LISUAN_PROCID_H__

#include <linux/dma-fence.h>

#include "lisuan_device.h"

#define GFGPU_NUM_PROCID 14

struct lisuan_procid_mgr;
struct gf_job;

struct lisuan_procid {
	struct list_head list;
	struct lisuan_procid_mgr *mgr;

	unsigned id;
	struct gf_sync_fence last_flush;

	uint64_t owner;
	struct list_head active_sync;//empty means no one use this procid
};

struct lisuan_procid_mgr {
	struct os_mutex id_lock;
	unsigned num_ids;
	/* Handling of VM fences */
	uint64_t fence_context;
	unsigned seqno[RB_INDEX_LAST];
	struct list_head procid_lru;
	struct lisuan_procid ids[GFGPU_NUM_PROCID];
	struct dma_fence * fences[GFGPU_NUM_PROCID];//a shadow for not alway malloc mem 
};

void lisuan_init_procid_mgr(struct lisuan_device *ls_dev);
int lisuan_grab_procid(struct lisuan_ring *ring, struct lisuan_vm *vm, struct gf_job *job);
void lisuan_fini_procid_mgr(struct lisuan_device *ls_dev);
void lisuan_procid_vm_detach(struct lisuan_device *ls_dev, struct lisuan_vm *vm);

#endif
