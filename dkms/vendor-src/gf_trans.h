/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#ifndef _GF_TRANS_H
#define _GF_TRANS_H

enum lisuan_trans_op {
	lisuan_trans_op_evict = 0,//local to pcie
	lisuan_trans_op_resident,//pcie to local
	lisuan_trans_op_blt,
	lisuan_trans_op_clear,
	lisuan_trans_op_auto_clear,
	lisuan_trans_op_max,
};

void gf_trans(struct lisuan_device *ls_dev, struct lisuan_gem_bo * src, struct lisuan_gem_bo * dst, int trans_op, struct dma_fence ** fence);
void gf_send_ifence_wait(struct lisuan_device *ls_dev, unsigned int **dma_ptr, unsigned int rb_index);
#endif
