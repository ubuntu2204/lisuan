/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */
#ifndef __GF_HW_CMDS__H__
#define __GF_HW_CMDS__H__

#define MAX_GCE_OP_WIDTH (0x4000)
#define MAX_GCE_OP_HEIGH (0x4000)

#define ALIGN_SIZE(Value, Alignment)  (((Value) + (Alignment - 1)) & ~(Alignment - 1))

typedef enum {
	RB_INDEX_GFX = 0,
	RB_INDEX_GCE,
	RB_INDEX_VEM,
	RB_INDEX_DEC0,
	RB_INDEX_DEC1,
	RB_INDEX_ENC,
	RB_INDEX_IM,
	RB_INDEX_LAST
} gf_rb_index;

enum gf_cmd_type {
	GF_CMD_TYPE_GFX,
	GF_CMD_TYPE_GCE,
	GF_CMD_TYPE_VEM,
};
enum gf_gce_cmd_type {
	GF_GCE_HOST_DATA_UPLOAD,
	GF_GCE_CLR_CMDS,
	GF_GCE_BLT_CMDS,
};

struct srb_item_s
{
	uint32_t u_blk_id;
	uint32_t u_grp_addr_in_srb;
	uint32_t u_start_addr_in_grp;
	uint32_t u_end_addr_in_grp;
};


int gf_hw_init_save_restore_cmds(unsigned int rb_index, void** save, void **restore, unsigned long long dummy_gpuva);
int gf_get_tlb_inv_cmd(unsigned int **dma_ptr, unsigned int rb_index, 
						int procid, unsigned long long vm_node_offset, unsigned long long dummy_vm_node_offset, 
						unsigned int cur_vm_L0_dirty, unsigned int cur_vm_L1_dirty, unsigned int cur_vm_L2_dirty, unsigned int bypass_L2_dirty);
void gf_hw_emit_vf_efence(unsigned char **rb_space);
void gf_init_cmd_template(void **p_rb_cmd_template, unsigned int slc_mask);
void gf_deinit_cmd_template(void **p_rb_cmd_template);
void gf_hw_init_gce_host_cmds(void **gce_host_upload, bool enable_gl2);
void gf_hw_init_gce_clr_cmds(void ** p_gce_clr, bool enable_gl2);
void gf_hw_init_gce_blt_cmds(void ** p_gce_blt);
void gf_hw_get_init_gpu_state_cmds(unsigned int **p_rb_tail, void *rb_cmd_template, bool is_virtfn, bool en_new_cl_clr);
void gf_hw_get_gfx_rb_deinit_cmds(unsigned int **p_rb_tail, unsigned long long dummy_node_offset, void *gfx_cmd_temp);
void gf_hw_get_ifence_wait_cmds(unsigned int **p_dma_ptr, unsigned int rb_index, unsigned long long dummy_node_offset, bool enable_gl2);
void gf_hw_get_do_auto_clear_cmds(unsigned int ** ppCmd, unsigned long long offset, unsigned long long start_addr_in_dpa, unsigned long long end_addr_in_dpa, int fill_size);
void gf_hw_get_do_clear_cmds(unsigned int ** p_dma_ptr, unsigned int rb_index, unsigned long long bypass_gpuva , int clr_size, void *gce_clr_cmds_template);
void gf_hw_get_raw_blit_cmds(unsigned int ** p_dma_ptr, unsigned int rb_index, unsigned long long src_addr , unsigned long long dst_addr, unsigned int blit_size, void *gce_blt_cmds_template, bool enable_gl2);
unsigned int gf_hw_get_mem_op_write_cmds(unsigned int PredOn, unsigned long long Addr, unsigned int ByteNum, unsigned int Gl2En, unsigned int IsStreamData, unsigned int *pCommand);
unsigned int gf_hw_get_cpia_gce_end_cmds(unsigned int PreemptOn, unsigned int* pCommand);
unsigned int gf_hw_get_job_run_rb_size(unsigned int job_dma_count, bool is_virtfn, enum gf_cmd_type cmd_type);
void gf_hw_get_job_run_begin_cmds(unsigned char **p_rb_space, int job_procid, void *cmd_temp, enum gf_cmd_type cmd_type);
void gf_hw_get_job_run_end_cmds(unsigned char **p_rb_space, int job_procid, unsigned char * rb_space_orig, unsigned int rb_size, void *cmd_temp);
void gf_hw_get_job_run_dma_cmds(unsigned char **p_rb_space, int dma_idb1_index, unsigned int dma_idb_type, unsigned int dma_size_in_byte, unsigned long long dma_gpu_va, bool is_virtfn, void *cmd_temp);
void gf_hw_get_job_run_fence_cmds(unsigned char **p_rb_space, bool fill_cmd_only, unsigned long long fence_va, unsigned long long ring_fence_value, void *cmd_temp, enum gf_cmd_type cmd_type);
unsigned int gf_hw_get_gce_cmd_size(enum gf_gce_cmd_type cmd_type);
void gf_send_skip_cmd(char** p_rb_ptr, unsigned int skip_size, unsigned int rb_index, unsigned long long ksave_va, unsigned long long ksave_size, unsigned long long krestore_va, unsigned long long krestore_size, void *gfx_cmd_temp);
void gf_hw_get_gce_host_update_pde_cmds(unsigned int ** ppCmd, unsigned int offset, unsigned long long dst_va, void * gce_host_upload_template);
void gf_hw_get_gce_host_update_pte_cmds(unsigned int ** ppCmd, unsigned int nptes, unsigned int offset, unsigned long long dst_va, void * gce_host_upload_template);
void gf_hw_get_vcmd_fill_indicator_and_end_cmds(unsigned int **p_cmd_va, unsigned int cmd_size, int proc_id, unsigned int vm_node_offset, unsigned long long fence_va, unsigned long long fence_value);
#endif