// SPDX-License-Identifier: GPL-2.0
/****************************************************************************
 *
 *    The MIT License (MIT)
 *
 *    Copyright (C) 2020  VeriSilicon Microelectronics Co., Ltd.
 *
 *    Permission is hereby granted, free of charge, to any person obtaining a
 *    copy of this software and associated documentation files (the "Software"),
 *    to deal in the Software without restriction, including without limitation
 *    the rights to use, copy, modify, merge, publish, distribute, sublicense,
 *    and/or sell copies of the Software, and to permit persons to whom the
 *    Software is furnished to do so, subject to the following conditions:
 *
 *    The above copyright notice and this permission notice shall be included in
 *    all copies or substantial portions of the Software.
 *
 *    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 *    THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 *    DEALINGS IN THE SOFTWARE.
 *
 *****************************************************************************
 *
 *    The GPL License (GPL)
 *
 *    Copyright (C) 2020  VeriSilicon Microelectronics Co., Ltd.
 *
 *    This program is free software; you can redistribute it and/or
 *    modify it under the terms of the GNU General Public License
 *    as published by the Free Software Foundation; either version 2
 *    of the License, or (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software Foundation,
 *    Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *****************************************************************************
 *
 *    Note: This software is released under dual MIT and GPL licenses. A
 *    recipient may use this file under the terms of either the MIT license or
 *    GPL License. If you wish to use only one license not the other, you can
 *    indicate your decision by deleting one of the above license notices
 *    in your version of this file.
 *
 *****************************************************************************
 */

#include "dwl_defs.h"
#include "vcmdswhwregisters.h"
#include "bidirect_list.h"
#include "vcmdswhwregisters.h"
#include "hantrovcmd.h"
#include "subsys.h"
#include "hantroaxife.h"
#include "lisuan_drm.h"
#include "hantrodec.h"
#include "lisuan_gem.h"
#include "vc9000_common.h"
#include "lisuan_ring.h"
#include "gf_hw_misc.h"

/*
 * Macros to use watchdog
 */
#undef USE_WATCHDOG
#ifdef USE_WATCHDOG
#define SUPPORT_WATCHDOG
#endif

/*these size need to be modified according to hw config.*/
#define VCMD_ENCODER_REGISTER_SIZE              (479 * 4)
#define VCMD_DECODER_REGISTER_SIZE              (MAX_REG_COUNT * 4)
#define VCMD_IM_REGISTER_SIZE                   (479 * 4)
#define VCMD_JPEG_ENCODER_REGISTER_SIZE         (479 * 4)
#define VCMD_JPEG_DECODER_REGISTER_SIZE         (MAX_REG_COUNT * 4)

#define MAX_VCMD_NUMBER (MAX_VCMD_TYPE * MAX_SAME_MODULE_TYPE_CORE_NUMBER)

#define HW_WORK_STATE_PEND            3

#define MAX_CMDBUF_INT_NUMBER         1
#define INT_MIN_SUM_OF_IMAGE_SIZE                                              \
	(4096 * 2160 * MAX_SAME_MODULE_TYPE_CORE_NUMBER * MAX_CMDBUF_INT_NUMBER)
#define MAX_PROCESS_CORE_NUMBER (4 * 8)
#define PROCESS_MAX_VIDEO_SIZE                                                 \
	(4096 * 2160 * MAX_SAME_MODULE_TYPE_CORE_NUMBER *                      \
	 MAX_PROCESS_CORE_NUMBER)
#define PROCESS_MAX_JPEG_SIZE (2147483648U) //32K*32K*2
#define PROCESS_MAX_SUM_OF_IMAGE_SIZE                                          \
	(PROCESS_MAX_VIDEO_SIZE > PROCESS_MAX_JPEG_SIZE ?                      \
		 PROCESS_MAX_VIDEO_SIZE :                                      \
		 PROCESS_MAX_JPEG_SIZE)

#define MAX_SAME_MODULE_TYPE_CORE_NUMBER 4

#ifdef EMU
/*******************PCIE CONFIG*************************/

  #define PCI_VENDOR_ID_HANTRO            0x1d9b//0x16c3
  #define PCI_DEVICE_ID_HANTRO      0xface// 0x7011

  /* Base address got control register */
  #define PCI_H2_BAR              2

  /* Base address DDR register */
  #define PCI_DDR_BAR             4
#else
/*******************PCIE CONFIG*************************/
#ifdef PLATFORM_GEN7
  #define PCI_VENDOR_ID_HANTRO            0x10ee//0x16c3
  #define PCI_DEVICE_ID_HANTRO      0x9014// 0x7011

  /* Base address got control register */
  #define PCI_H2_BAR              0

  /* Base address DDR register */
  #define PCI_DDR_BAR             2
#else
  #define PCI_VENDOR_ID_HANTRO            0x10ee//0x16c3
  #define PCI_DEVICE_ID_HANTRO      0x8014// 0x7011

  /* Base address got control register */
  #define PCI_H2_BAR              4

  /* Base address DDR register */
  #define PCI_DDR_BAR             0
#endif
#endif

#ifdef EMU
#define VCMD_BUF_POOL_OFFSET 0x4800000
#else
#define VCMD_BUF_POOL_OFFSET 0x800000
#endif

/********variables declaration related with race condition**********/

#define CMDBUF_MAX_SIZE (512 * 4 * 4)

//approximately=128x(320x240)=128x2k=128x8kbyte=1Mbytes
#define CMDBUF_POOL_TOTAL_SIZE (2 * 1024 * 1024)
#define TOTAL_DISCRETE_CMDBUF_NUM (CMDBUF_POOL_TOTAL_SIZE / CMDBUF_MAX_SIZE)
#define CMDBUF_VCMD_REGISTER_TOTAL_SIZE                                        \
	(9 * 1024 * 1024 - CMDBUF_POOL_TOTAL_SIZE * 2)
#define VCMD_REGISTER_SIZE (128 * 4)
#define GET_BITS(val, start, end)    (((unsigned long long)(val) >> (start)) & ((1ULL << ((end) - (start) + 1)) - 1))

#ifndef DYNAMIC_MALLOC_VCMDNODE
static struct cmdbuf_obj *g_cmdbuf_obj_pool;
static struct bi_list_node *g_cmdbuf_node_pool;
#endif

struct noncache_mem {
	u32 *virtual_address;
	dma_addr_t bus_address;
	size_t mmu_bus_address; /* buffer physical address in MMU*/
	u32 size;
	u16 cmdbuf_id;
};

struct process_manager_obj {
	struct drm_file *file;
	u64 total_exe_time;
	spinlock_t spinlock;
	wait_queue_head_t wait_queue;
	/* for mutli core multi stream */
	bi_list_node *cmdbuf_nodes[TOTAL_DISCRETE_CMDBUF_NUM];
	u32 module_type; /* correspond to file, indicates subsys type */
};

struct cmdbuf_obj {
	/* current CMDBUF type: input vce=0,
	 * IM=1,vcd=2, pege=3, jpegd=4
	 */
	u32 module_type;
	/* current CMDBUFpriority: normal=0, high=1 */
	u32 priority;
	/* current CMDBUFexecuting_time=encoded_image_size*(rdoLevel+1)*(rdoq+1);*/
	u64 executing_time;
	/* current CMDBUF size */
	u32 cmdbuf_size;
	/* current CMDBUF start virtual address.*/
	u32 *cmdbuf_virtual_address;
	/* current CMDBUF start physical address. */
	size_t cmdbuf_bus_address;
	//current CMDBUF start mmu mapping address.
	size_t mmu_cmdbuf_bus_address;
	/*current status CMDBUF start virtual address.*/
	u32 *status_virtual_address;
	/*current status CMDBUF start physical address.*/
	size_t status_bus_address;
	//current status CMDBUF start mmu mapping address.
	size_t mmu_status_bus_address;
	/*current status CMDBUF size*/
	u32 status_size;
	/*current CMDBUF executing status.*/
	u32 executing_status;
	/*file pointer in the same process.*/
	struct drm_file *file;
	/* which vcmd core is used.*/
	u16 core_id;
	/* which cores can be uesd.*/
	u32 core_mask;
	/* used to manage CMDBUF in driver.It is a handle
	 * to identify cmdbuf.also is an interrupt vector.
	 * position in pool,same as status position.
	 */
	u16 cmdbuf_id;
	/* 0 means sw has not copied data into this CMDBUF;
	 * 1 means sw has copied data into this CMDBUF
	 */
	u8 cmdbuf_data_loaded;
	/* 0 :not linked, 1:linked into a vcmd core list*/
	u8 cmdbuf_data_linked;
	/* if 0,waiting for CMDBUF finish; if 1,
	 * op code in CMDBUF has finished one by one.
	 * HANTRO_VCMD_IOCH_WAIT_CMDBUF will check this variable.
	 */
	u8 cmdbuf_run_done;
	/* if 0, not need to remove CMDBUF; 1 CMDBUF
	 * can be removed if it is not the last CMDBUF;
	 */
	u8 cmdbuf_need_remove;
	/* if 0, the cmd buf hasn't been waited,
	 * otherwise, has been waited.
	 */
	u32 waited;
	/* if 1, the last opcode is end opCode. */
	u8 has_end_cmdbuf;
	/* if 1, JMP will not send normal interrupt. */
	u8 no_normal_int_cmdbuf;
	u8 slice_run_done;
	struct process_manager_obj *process_manager_obj;
	unsigned long long submit_fence_id;
	struct dma_fence_cb fence_cb;
};

struct hantrovcmd_dev {
	//config of each core,such as base addr, irq,etc
	struct vcmd_config vcmd_core_cfg;
	//vcmd core id for driver and sw internal use
	u32 core_id;
	u32 sw_cmdbuf_rdy_num;
	spinlock_t *spinlock;
	wait_queue_head_t *wait_queue;
	wait_queue_head_t *wait_abort_queue;
	bi_list list_manager;

	u8 *hwregs; /* IO mem base */
	u8 *hwregs_submodule_main; /* IO mem base */
	u32 reg_mirror[ASIC_VCMD_SWREG_AMOUNT];
	u32 reg_mirror_main[MAX_REG_COUNT];
	//number of cmdbufs without interrupt.
	u32 duration_without_int;
	volatile u8 working_state;
	u64 total_exe_time;
	u16 status_cmdbuf_id; //used for analyse configuration in cwl.
	u32 hw_version_id; /*megvii 0x43421001, later 0x43421401*/
	//start virtual address of vcmd registers memory of  CMDBUF.
	u32 *vcmd_reg_mem_virtual_address;
	//start physical address of vcmd registers memory of  CMDBUF.
	size_t vcmd_reg_mem_bus_address;
	//start mmu mapping address of vcmd registers memory of CMDBUF.
	unsigned int mmu_vcmd_reg_mem_bus_address;
	// size of vcmd registers memory of CMDBUF.
	u32 vcmd_reg_mem_size;
	size_t base_ddr_addr;

	spinlock_t abnormal_lock;

	u32 irq_status;
	u32 cur_cmd_id;

	struct lisuan_ring *dec_ring;
	unsigned long long last_notify_fence;
};

typedef struct {
	/* input configuration to vcmd manager */
	int total_vcmd_core_num;
	struct vcmd_config vcmd_core_array[MAX_SUBSYS_NUM];
	struct platform_device *platformdev;

	/* three linear buffers: vcmd buffer / status / registers */
	struct noncache_mem vcmd_buf_mem_pool;
	struct noncache_mem vcmd_status_buf_mem_pool;
	struct noncache_mem vcmd_registers_mem_pool;
	size_t base_ddr_addr;  /* base address: need to subtract this value then can be put to register*/

	u16 cmdbuf_used[TOTAL_DISCRETE_CMDBUF_NUM];
	u16 cmdbuf_used_pos;
	u16 cmdbuf_used_residual;

	struct hantrovcmd_dev *vcmd_manager[MAX_VCMD_TYPE][MAX_VCMD_NUMBER];
	struct semaphore vcmd_reserve_cmdbuf_sem[MAX_VCMD_TYPE]; //for reserve
	bi_list_node * global_cmdbuf_node[TOTAL_DISCRETE_CMDBUF_NUM];

	bi_list global_process_manager;

	u16 vcmd_position[MAX_VCMD_TYPE];
	int vcmd_type_core_num[MAX_VCMD_TYPE];

	struct hantrovcmd_dev *hantrovcmd_data;

	//hw_queue can be used for reserve cmdbuf memory
	wait_queue_head_t vcmd_cmdbuf_memory_wait;
	spinlock_t vcmd_cmdbuf_alloc_lock;
	spinlock_t vcmd_process_manager_lock;
	spinlock_t owner_lock_vcmd[MAX_VCMD_NUMBER];
	wait_queue_head_t wait_queue_vcmd[MAX_VCMD_NUMBER];
	wait_queue_head_t abort_queue_vcmd[MAX_VCMD_NUMBER];

	/* mc wait queue, used in wait_cmdbuf_ready with ANY_CMDBUF_ID. */
	wait_queue_head_t mc_wait_queue;
	/* owner of decoder MC token, matched by drm_file context. */
	struct drm_file *mc_token_owner;
	u64 vcd_cycles;
	u8 mmu_enable;

	u8 vcmd_irq_enabled;
	spinlock_t mutex;
} vcmd_mgr_t;

struct vcmd_config vcmd_core_array[MAX_SUBSYS_NUM];

#ifdef SUPPORT_WATCHDOG
//timer for watch dog
static struct timer_list vcmd_timer;
static vcmd_mgr_t *watch_dev;
static u32 vcmd_count[HXDEC_MAX_CORES] = {0};
static u32 vcmd_state[HXDEC_MAX_CORES] = {0};
static volatile unsigned long vcd_virt[HXDEC_MAX_CORES];
#endif

/* for debugfs */
#ifdef SUPPORT_DBGFS
#define MAX_RESERVED_TIME 100
static int N_reserved = 10, cycle_index[MAX_VCMD_NUMBER], e_index[MAX_VCMD_NUMBER], r_index[MAX_VCMD_NUMBER], l_index[MAX_VCMD_NUMBER], i_index[MAX_VCMD_NUMBER];
static int num_cmdbuf_linked[MAX_VCMD_NUMBER];
static u32  decode_cycles[MAX_VCMD_NUMBER][MAX_RESERVED_TIME];
static u32 num_cmdbuf_exe[MAX_VCMD_NUMBER][MAX_RESERVED_TIME];
static u32 num_cmdbuf_reserved[MAX_VCMD_NUMBER][MAX_RESERVED_TIME];
static struct dentry *debug_root;
static struct dentry *root_cmdbuf[TOTAL_DISCRETE_CMDBUF_NUM];
static u32 num_cmdbuf_twoidle[MAX_VCMD_NUMBER], pre_hw_rdy_cmdbuf_num[MAX_VCMD_NUMBER];
static char *subsys_name[MAX_VCMD_NUMBER] = {"reg_subsys0", "reg_subsys1", "reg_subsys2", "reg_subsys3"};
static struct dentry *debugfs_root;
#define IDLE 0
#define RESERVED 1
#define DECODING 2
#define DONE 3
static u64 reserved_time[HXDEC_MAX_CORES][MAX_RESERVED_TIME];
static u32 reserved_cmdbufid[HXDEC_MAX_CORES][MAX_RESERVED_TIME];
static u64 link_time[HXDEC_MAX_CORES][MAX_RESERVED_TIME];
static u32 link_cmdbufid[HXDEC_MAX_CORES][MAX_RESERVED_TIME];
static u64 vcmd_workload[HXDEC_MAX_CORES];
static u64 interrupt_time[HXDEC_MAX_CORES][MAX_RESERVED_TIME];
static u64 active_start_time[HXDEC_MAX_CORES][MAX_RESERVED_TIME];
static u64 active_return_time[HXDEC_MAX_CORES][MAX_RESERVED_TIME];
static u32 cmdbuf_num_done[HXDEC_MAX_CORES][MAX_RESERVED_TIME];
static u32 prev_cmdbuf_done[HXDEC_MAX_CORES][MAX_RESERVED_TIME];
static u32 active_hw_index[HXDEC_MAX_CORES];
static u32 active_state[HXDEC_MAX_CORES];
struct hantrovcmd_dev *hantrovcmd_data_debugfs;
vcmd_mgr_t *vcmd_mgr_debugfs;
#endif
/*
 * Ioctl definitions
 */
#define VCMD_HW_ID 0x4342

#define EXECUTING_CMDBUF_ID_ADDR 26
#define VCMD_EXE_CMDBUF_COUNT 3

#define WORKING_STATE_IDLE 0
#define WORKING_STATE_WORKING 1
#define CMDBUF_EXE_STATUS_OK 0
#define CMDBUF_EXE_STATUS_CMDERR 1
#define CMDBUF_EXE_STATUS_BUSERR 2

//struct semaphore vcmd_reserve_cmdbuf_sem[MAX_VCMD_TYPE]; //for reserve

//#ifdef PCIE_EN
/* PCI base register address (memalloc) */
extern unsigned long long gBaseDDRHw;
//#endif
//extern unsigned int mmu_enable;
//extern unsigned long alloc_base;
//extern unsigned long alloc_size;

/******TYPE AND FUNCTION DECLARATION********/

/* here's all the must remember stuff */
static int vcmd_reserve_IO(struct lisuan_device *gf_device, vcmd_mgr_t *vcmd_mgr);
static void vcmd_release_IO(vcmd_mgr_t *vcmd_mgr);
static void vcmd_reset_current_asic(struct hantrovcmd_dev *dev);
static int allocate_cmdbuf(vcmd_mgr_t *vcmd_mgr, struct noncache_mem *new_cmdbuf_addr, struct noncache_mem *new_status_cmdbuf_addr);
static void vcmd_link_cmdbuf(struct hantrovcmd_dev *dev, bi_list_node *last_linked_cmdbuf_node);
static void vcmd_start(struct hantrovcmd_dev *dev, bi_list_node *first_linked_cmdbuf_node);
static void create_kernel_process_manager(vcmd_mgr_t *vcmd_mgr);

static void printk_vcmd_register_debug(const void *hwregs, char *info);

/********local variable declaration********/
/* and this is our MAJOR; use 0
 * for dynamic allocation (recommended)
 */


//#define IRQ_SIMULATION

#ifdef IRQ_SIMULATION
struct timer_manager {
	u32 core_id; //vcmd core id for driver and sw internal use
	u32 timer_id;
	struct timer_list *timer;
};

static struct timer_list timer[10000];
struct timer_manager timer_reserve[10000];
#if 0
static struct timer_list timer0;
static struct timer_list timer1;
#endif
#endif

#if 0
//hw_queue can be used for reserve cmdbuf memory
DECLARE_WAIT_QUEUE_HEAD(vcmd_cmdbuf_memory_wait);

DEFINE_SPINLOCK(vcmd_cmdbuf_alloc_lock);
DEFINE_SPINLOCK(vcmd_process_manager_lock);

spinlock_t owner_lock_vcmd[MAX_VCMD_NUMBER];

wait_queue_head_t wait_queue_vcmd[MAX_VCMD_NUMBER];

wait_queue_head_t abort_queue_vcmd[MAX_VCMD_NUMBER];

/* mc wait queue, used in wait_cmdbuf_ready with ANY_CMDBUF_ID. */
static wait_queue_head_t mc_wait_queue;
#endif

static void _dbg_log_instr(u32 offset, u32 instr, u32 *size, char *str)
{
	if ((instr & 0xF8000000) == OPCODE_WREG) {
		int length = ((instr >> 16) & 0x3FF);

		sprintf(str, "current cmdbuf data %d = 0x%08x => [%s %s %d 0x%x]\n",
			offset, instr, "WREG", ((instr >> 26) & 0x1) ? "FIX" : "",
			   length, (instr & 0xFFFF));
		*size = ((length + 2) >> 1) << 1;
	} else if ((instr & 0xF8000000) == OPCODE_END) {
		sprintf(str, "current cmdbuf data %d = 0x%08x => [%s]\n", offset,
			instr, "END");
		*size = 2;
	} else if ((instr & 0xF8000000) == OPCODE_NOP) {
		sprintf(str, "current cmdbuf data %d = 0x%08x => [%s]\n", offset,
			instr, "NOP");
		*size = 2;
	} else if ((instr & 0xF8000000) == OPCODE_RREG) {
		int length = ((instr >> 16) & 0x3FF);

		sprintf(str, "current cmdbuf data %d = 0x%08x => [%s %s %d 0x%x]\n",
			offset, instr, "RREG", ((instr >> 26) & 0x1) ? "FIX" : "",
			   length, (instr & 0xFFFF));
		*size = 4;
	} else if ((instr & 0xF8000000) == OPCODE_JMP) {
		sprintf(str, "current cmdbuf data %d = 0x%08x => [%s %s %s]\n",
			offset, instr, "JMP", ((instr >> 26) & 0x1) ? "RDY" : "",
			   ((instr >> 25) & 0x1) ? "IE" : "");
		*size = 4;
	} else if ((instr & 0xF8000000) == OPCODE_STALL) {
		sprintf(str, "current cmdbuf data %d = 0x%08x => [%s %s 0x%x]\n",
			offset, instr, "STALL", ((instr >> 26) & 0x1) ? "IM" : "",
			   (instr & 0xFFFF));
		*size = 2;
	} else if ((instr & 0xF8000000) == OPCODE_CLRINT) {
		sprintf(str, "current cmdbuf data %d = 0x%08x => [%s %d 0x%x]\n",
			offset, instr, "CLRINT", (instr >> 25) & 0x3,
			   (instr & 0xFFFF));
		*size = 2;
	} else if ((instr & 0xF8000000) == OPCODE_M2M) {
		sprintf(str, "current cmdbuf data %d = 0x%08x => [%s]\n",
			offset, instr, "M2M");
		*size = 6;
	} else
		*size = 1;
}

#ifdef SUPPORT_DBGFS
/* debugfs for VCD cycles */
static ssize_t vcd_cycles_read(struct drm_file *file, char __user *user_buf, size_t count, loff_t *ppos)
{
	char v[300], *t;
	int i, j, index, ret;
	u32 hw_rdy_cmdbuf_num;
	struct hantrovcmd_dev *hantrovcmd_data = hantrovcmd_data_debugfs;
	u64 first_start_time;

	t = kzalloc(N_reserved * 1500 * vcmd_mgr_debugfs->total_vcmd_core_num, GFP_KERNEL);
	for (j = 0; j < vcmd_mgr_debugfs->total_vcmd_core_num; j++) {
		sprintf(v, "vcmdcore[%d]\n", j);
		strcat(t, v);
		sprintf(v, "********************************************************************************\n");
		strcat(t, v);
		sprintf(v, "decode total cmfbuf cycles of recent N interruption\t\tinterrupt_time /ms\n");
			strcat(t, v);
		index = cycle_index[j];
		if (index >= N_reserved) {
			for (i = 0; i < N_reserved; i++) {
				sprintf(v, "%u\t\t\t\t\t\t\t\t", decode_cycles[j][i + index - N_reserved]);
				strcat(t, v);
				sprintf(v, "%lu\n", interrupt_time[j][i + index - N_reserved]);
				strcat(t, v);
			}
		} else {
			for (i = 0; i < index; i++) {
				sprintf(v, "%u\t\t\t\t\t\t\t\t", decode_cycles[j][i]);
				strcat(t, v);
				sprintf(v, "%lu\n", interrupt_time[j][i]);
				strcat(t, v);
			}
			for (i = MAX_RESERVED_TIME - 1; i > MAX_RESERVED_TIME - 1 - (N_reserved - index); i--) {
				sprintf(v, "%u\t\t\t\t\t\t\t\t", decode_cycles[j][i]);
				strcat(t, v);
				sprintf(v, "%lu\n", interrupt_time[j][i]);
				strcat(t, v);
			}
		}
		sprintf(v, "\ncmdbuffer executed number between recent N interrupt time\n");
		strcat(t, v);
		index = e_index[j];
		if (index >= N_reserved) {
			for (i = 0; i < N_reserved; i++) {
				sprintf(v, "%u\n", num_cmdbuf_exe[j][i + index - N_reserved]);
				strcat(t, v);
			}
		} else {
			for (i = 0; i < index; i++) {
				sprintf(v, "%u\n", num_cmdbuf_exe[j][i]);
				strcat(t, v);
			}
			for (i = MAX_RESERVED_TIME - 1; i > MAX_RESERVED_TIME - 1 - (N_reserved - index); i--) {
				sprintf(v, "%u\n", num_cmdbuf_exe[j][i]);
				strcat(t, v);
			}
		}
		sprintf(v, "\ncmdbuf id\tcmdbuffer reserved time /ms\n");
		strcat(t, v);
		index = r_index[0];
		if (index >= N_reserved * 2) {
			for (i = 0; i < N_reserved * 2; i++) {
				sprintf(v, "%-4d\t\t", reserved_cmdbufid[0][i + index - N_reserved * 2]);
				strcat(t, v);
				sprintf(v, "%-25lu\n", reserved_time[0][i + index - N_reserved * 2]);
				strcat(t, v);
			}
		} else {
			for (i = 0; i < index; i++) {
				sprintf(v, "%-4d\t\t", reserved_cmdbufid[0][i]);
				strcat(t, v);
				sprintf(v, "%-25lu\n", reserved_time[0][i]);
				strcat(t, v);
			}
			for (i = MAX_RESERVED_TIME - 1; i > MAX_RESERVED_TIME - 1 - (N_reserved * 2 - index); i--) {
				sprintf(v, "%-4d\t\t", reserved_cmdbufid[0][i]);
				strcat(t, v);
				sprintf(v, "%-25lu\n", reserved_time[0][i]);
				strcat(t, v);
			}
		}

		sprintf(v, "\ncmdbuf id\tcore_id\t\t\tcmdbuffer link time /ms\t\t\t\n");
		strcat(t, v);
		index = l_index[j];
		if (index >= N_reserved) {
			for (i = 0; i < N_reserved; i++) {
				sprintf(v, "%-4d\t\t", link_cmdbufid[j][i + index - N_reserved]);
				strcat(t, v);
				sprintf(v, "%-10lu\t\t\t", j);
				strcat(t, v);
				sprintf(v, "%-10lu\t\t\n", link_time[j][i + index - N_reserved]);
				strcat(t, v);
			}
		} else {
			for (i = 0; i < index; i++) {
				sprintf(v, "%-4d\t\t", link_cmdbufid[j][i]);
				strcat(t, v);
				sprintf(v, "%-10lu\t\t\t", j);
				strcat(t, v);
				sprintf(v, "%-10lu\t\t\n", link_time[j][i]);
				strcat(t, v);
			}
			for (i = MAX_RESERVED_TIME - 1; i > MAX_RESERVED_TIME - 1 - (N_reserved - index); i--) {
				sprintf(v, "%-4d\t\t", link_cmdbufid[j][i]);
				strcat(t, v);
				sprintf(v, "%-10lu\t\t\t", j);
				strcat(t, v);
				sprintf(v, "%-10lu\t\t\n", link_time[j][i]);
				strcat(t, v);
			}
		}

		sprintf(v, "\nVCMD\t\tstart time /ms\t\t\tidle time /ms\t\t\tcmdbuf num\t\t\n");
		strcat(t, v);
		index = active_hw_index[j];
		if (index >= N_reserved) {
			if (vcmd_mgr_debugfs->total_vcmd_core_num > 1)
				if (active_start_time[0][index - N_reserved] > active_start_time[1][index - N_reserved])
					first_start_time = active_start_time[1][index - N_reserved];
			else
				first_start_time = active_start_time[0][index - N_reserved];
			for (i = 0; i < N_reserved; i++) {
				sprintf(v, "%-4d\t\t", j);
				strcat(t, v);
				sprintf(v, "%-10lu\t\t\t", active_start_time[j][i + index - N_reserved] ?
						active_start_time[j][i + index - N_reserved] - first_start_time : 0);
				strcat(t, v);
				sprintf(v, "%-10lu\t\t\t", active_return_time[j][i + index - N_reserved] ?
						active_return_time[j][i + index - N_reserved - first_start_time : 0);
				strcat(t, v);
				sprintf(v, "%u\t\t\n", cmdbuf_num_done[j][i + index - N_reserved]);
				strcat(t, v);
			}
		} else {
			if (vcmd_mgr_debugfs->total_vcmd_core_num > 1)
				if (active_start_time[0][0] > active_start_time[1][0])
					first_start_time = active_start_time[1][0];
			else
				first_start_time = active_start_time[0][0];
			for (i = 0; i < index; i++) {
				sprintf(v, "%-4d\t\t", j);
				strcat(t, v);
				sprintf(v, "%-10lu\t\t\t", active_start_time[j][i] ? active_start_time[j][i] - first_start_time : 0);
				strcat(t, v);
				sprintf(v, "%-10lu\t\t\t", active_return_time[j][i] ? active_return_time[j][i] - first_start_time : 0);
				strcat(t, v);
				sprintf(v, "%u\t\t\n", cmdbuf_num_done[j][i]);
				strcat(t, v);
			}
			for (i = MAX_RESERVED_TIME - 1; i > MAX_RESERVED_TIME - 1 - (N_reserved - index); i--) {
				sprintf(v, "%-4d\t\t", j);
				strcat(t, v);
				sprintf(v, "%-10lu\t\t\t", active_start_time[j][i] ? active_start_time[j][i] - first_start_time : 0);
				strcat(t, v);
				sprintf(v, "%-10lu\t\t\t", active_return_time[j][i] ? active_return_time[j][i] - first_start_time : 0);
				strcat(t, v);
				sprintf(v, "%u\t\t\n", cmdbuf_num_done[j][i]);
				strcat(t, v);
			}
		}

		sprintf(v, "\nvcmd workload: %llu\n", vcmd_workload[j]);
		strcat(t, v);
		hw_rdy_cmdbuf_num = gf_read_mmio32((hantrovcmd_data[j].hwregs + 3 * 4));
		sprintf(v, "\nthe number of cmdbuffer linked: %d\n", hantrovcmd_data[j].sw_cmdbuf_rdy_num);
		strcat(t, v);
		if (hantrovcmd_data[j].sw_cmdbuf_rdy_num - hw_rdy_cmdbuf_num)
			sprintf(v, "\nVCD state: decoding\n");
		else
			sprintf(v, "\nVCD state: idle\n");

		strcat(t, v);
		sprintf(v, "\nnumber of unprocessed cmdbuf remaining: %d\n", hantrovcmd_data[j].sw_cmdbuf_rdy_num - hw_rdy_cmdbuf_num);
		strcat(t, v);
		sprintf(v, "\nnumber of processed cmdbuf between latest two idle time:%d\n", num_cmdbuf_twoidle[j]);
		strcat(t, v);
		sprintf(v, "********************************************************************************\n\n");
		strcat(t, v);
	}
	ret = simple_read_from_buffer(user_buf, count, ppos, t, strlen(t));
	kfree(t);
	return ret;
}

static ssize_t vcd_cycles_write(struct drm_file *file, const char __user *user_buf, size_t count, loff_t *ppos)
{
	char In_fo[10];
	int n;

	simple_write_to_buffer(In_fo, count, ppos, user_buf, 4);
	kstrtoint(In_fo, 10, &N_reserved);
	//n = sscanf(In_fo, "%d", &N_reserved);

	return count;
}

const struct file_operations fileop_N_vcd_cycles = {
	.write = vcd_cycles_write,
	.read = vcd_cycles_read,
};

static ssize_t cmdbuf_parse(struct drm_file *file, char __user *user_buf, size_t count, loff_t *ppos)
{
	u16 cmdbuf_id = 0;
	char *v0, *v1;
	int ret, i, n, b = 0, pow = 1, j;
	struct cmdbuf_obj *cmdbuf_obj = NULL;
	bi_list_node *new_cmdbuf_node = NULL;
	char *v = file->f_path.dentry->d_iname;

	n = strlen(v);
	for (i = 0; i < strlen(v); i++) {
		b = v[i] - '0';
		pow = 1;
		for (j = 1; j < n; j++)
			pow *= 10;
		b *= pow;
		n -= 1;
		cmdbuf_id += b;
	}
	new_cmdbuf_node = vcmd_mgr_debugfs->global_cmdbuf_node[cmdbuf_id];
	v0 = kzalloc(50000, GFP_KERNEL);
	v1 = kzalloc(1500, GFP_KERNEL);
	if (!new_cmdbuf_node) {
		//should not happen
		sprintf(v0, "%s", "hantrovcmd: ERROR cmdbuf_id !!\n");
		ret = simple_read_from_buffer(user_buf, count, ppos, v0, strlen(v0));
		kfree(v0);
		kfree(v1);
		return ret;
	}
	cmdbuf_obj = (struct cmdbuf_obj *)new_cmdbuf_node->data;
	cmdbuf_obj->cmdbuf_data_loaded = 1;
	cmdbuf_obj->waited = 0;
	{
	u32 i, inst = 0, size = 0;

	sprintf(v1, "%s", "vcmd link, current cmdbuf content\n");
	strcat(v0, v1);
	for (i = 0; i < cmdbuf_obj->cmdbuf_size / 4; i++) {
		if (i == inst) {
			_dbg_log_instr(i, *(cmdbuf_obj->cmdbuf_virtual_address + i), &size, v1);
			strcat(v0, v1);
			inst += size;
		} else {
			sprintf(v1, "current cmdbuf data %d = 0x%x\n", i, *(cmdbuf_obj->cmdbuf_virtual_address + i));
			strcat(v0, v1);
		}
	}
}
ret = simple_read_from_buffer(user_buf, count, ppos, v0, strlen(v0));
kfree(v0);
kfree(v1);
return ret;
}

const struct file_operations cmdbuf_op = {
	.read = cmdbuf_parse,
};

static ssize_t hw_register_print(struct drm_file *file, char __user *user_buf, size_t count, loff_t *ppos)
{
	char *v0, v1[50];
	int swreg_mes, j, i, ret, num_regs = 0;
	struct hantrovcmd_dev *hantrovcmd_data = hantrovcmd_data_debugfs;
	int total_vcmd_core_num = vcmd_mgr_debugfs->total_vcmd_core_num;

	for (i = 0; i < total_vcmd_core_num; i++) {
		num_regs += hantrovcmd_data[i].vcmd_core_cfg.submodule_vcd_iosize + hantrovcmd_data[i].vcmd_core_cfg.submodule_dec400_iosize +
		hantrovcmd_data[i].vcmd_core_cfg.submodule_MMU_iosize + hantrovcmd_data[i].vcmd_core_cfg.submodule_MMUWrite_iosize +
		hantrovcmd_data[i].vcmd_core_cfg.submodule_axife_iosize + hantrovcmd_data[i].vcmd_core_cfg.vcmd_iosize;
	}
	num_regs /= 4;

	v0 = kzalloc(70 * num_regs, GFP_KERNEL);
	for (i = 0; i < total_vcmd_core_num; i++) {
		sprintf(v1, "vcmdcore[%d]\n", i);
		strcat(v0, v1);
		for (j = 0; j < hantrovcmd_data[0].vcmd_core_cfg.vcmd_iosize / 4;) {
			swreg_mes = gf_read_mmio32((hantrovcmd_data[i].hwregs + j * 4));
			sprintf(v1, "%-4d: %08x ", j, swreg_mes);
			strcat(v0, v1);
		j++;
			if (j < hantrovcmd_data[0].vcmd_core_cfg.vcmd_iosize / 4) {
				swreg_mes = gf_read_mmio32((hantrovcmd_data[i].hwregs + j * 4));
				sprintf(v1, "%08x ", swreg_mes);
		strcat(v0, v1);
		j++;
			}
			if (j < hantrovcmd_data[0].vcmd_core_cfg.vcmd_iosize / 4) {
				swreg_mes = gf_read_mmio32((hantrovcmd_data[i].hwregs + j * 4));
				sprintf(v1, "%08x ", swreg_mes);
				strcat(v0, v1);
		j++;
			}
			if (j < hantrovcmd_data[0].vcmd_core_cfg.vcmd_iosize / 4) {
				swreg_mes = gf_read_mmio32((hantrovcmd_data[i].hwregs + j * 4));
				sprintf(v1, "%08x", swreg_mes);
		strcat(v0, v1);
		j++;
			}
			strcat(v0, "\n");
		}

		if (hantrovcmd_data[i].vcmd_core_cfg.submodule_vcd_iosize) {
			sprintf(v1, "[VCD]\n");
			strcat(v0, v1);
			for (j = 0; j < hantrovcmd_data[i].vcmd_core_cfg.submodule_vcd_iosize / 4;) {
				swreg_mes = gf_read_mmio32((hantrovcmd_data[i].vcmd_core_cfg.submodule_vcd_virtual_address + j * 4));
				sprintf(v1, "%-4d: %08x ", j, swreg_mes);
				strcat(v0, v1);
				j++;
				if (j < MAX_REG_COUNT) {
					swreg_mes = gf_read_mmio32((hantrovcmd_data[i].vcmd_core_cfg.submodule_vcd_virtual_address + j * 4));
					sprintf(v1, "%08x ", swreg_mes);
					strcat(v0, v1);
					j++;
				}
				if (j < MAX_REG_COUNT) {
					swreg_mes = gf_read_mmio32((hantrovcmd_data[i].vcmd_core_cfg.submodule_vcd_virtual_address + j * 4));
					sprintf(v1, "%08x ", swreg_mes);
					strcat(v0, v1);
					j++;
				}
				if (j < MAX_REG_COUNT) {
					swreg_mes = gf_read_mmio32((hantrovcmd_data[i].vcmd_core_cfg.submodule_vcd_virtual_address + j * 4));
					sprintf(v1, "%08x", swreg_mes);
					strcat(v0, v1);
					j++;
				}
				strcat(v0, "\n");
			}
		}
		if (hantrovcmd_data[i].vcmd_core_cfg.submodule_dec400_iosize) {
			sprintf(v1, "[DEC400]\n");
			strcat(v0, v1);
			for (j = 0; j < hantrovcmd_data[i].vcmd_core_cfg.submodule_dec400_iosize / 4;) {
				swreg_mes = gf_read_mmio32((hantrovcmd_data[i].vcmd_core_cfg.submodule_dec400_virtual_address + j * 4));
				sprintf(v1, "%-4d: %08x ", j, swreg_mes);

				strcat(v0, v1);
				j++;
				if (j < hantrovcmd_data[i].vcmd_core_cfg.submodule_dec400_iosize / 4) {
					swreg_mes = gf_read_mmio32((hantrovcmd_data[i].vcmd_core_cfg.submodule_dec400_virtual_address + j * 4));
					sprintf(v1, "%08x ", swreg_mes);
					strcat(v0, v1);
					j++;
				}
				if (j < hantrovcmd_data[i].vcmd_core_cfg.submodule_dec400_iosize / 4) {
					swreg_mes = gf_read_mmio32((hantrovcmd_data[i].vcmd_core_cfg.submodule_dec400_virtual_address + j * 4));
					sprintf(v1, "%08x ", swreg_mes);
					strcat(v0, v1);
					j++;
				}
				if (j < hantrovcmd_data[i].vcmd_core_cfg.submodule_dec400_iosize / 4) {
					swreg_mes = gf_read_mmio32((hantrovcmd_data[i].vcmd_core_cfg.submodule_dec400_virtual_address + j * 4));
					sprintf(v1, "%08x", swreg_mes);
					strcat(v0, v1);
					j++;
				}
				strcat(v0, "\n");
			}
		}
		if (hantrovcmd_data[i].vcmd_core_cfg.submodule_MMU_iosize) {
			sprintf(v1, "[MMU]\n");
			strcat(v0, v1);
			for (j = 0; j < hantrovcmd_data[i].vcmd_core_cfg.submodule_MMU_iosize / 4;) {
				swreg_mes = gf_read_mmio32((hantrovcmd_data[i].vcmd_core_cfg.submodule_MMU_virtual_address + j * 4));
				sprintf(v1, "%-4d: %08x ", j, swreg_mes);

				strcat(v0, v1);
				j++;
				if (j < hantrovcmd_data[i].vcmd_core_cfg.submodule_MMU_iosize / 4) {
					swreg_mes = gf_read_mmio32((hantrovcmd_data[i].vcmd_core_cfg.submodule_MMU_virtual_address + j * 4));
					sprintf(v1, "%08x ", swreg_mes);
					strcat(v0, v1);
					j++;
				}
				if (j < hantrovcmd_data[i].vcmd_core_cfg.submodule_MMU_iosize / 4) {
					swreg_mes = gf_read_mmio32((hantrovcmd_data[i].vcmd_core_cfg.submodule_MMU_virtual_address + j * 4));
					sprintf(v1, "%08x ", swreg_mes);
					strcat(v0, v1);
					j++;
				}
				if (j < hantrovcmd_data[i].vcmd_core_cfg.submodule_MMU_iosize / 4) {
					swreg_mes = gf_read_mmio32((hantrovcmd_data[i].vcmd_core_cfg.submodule_MMU_virtual_address + j * 4));
					sprintf(v1, "%08x", swreg_mes);
					strcat(v0, v1);
					j++;
				}
				strcat(v0, "\n");
			}
		}
	}
	ret = simple_read_from_buffer(user_buf, count, ppos, v0, strlen(v0));
	kfree(v0);

	return ret;
}

const struct file_operations fileop_vcmd_reg_print = {
	.read = hw_register_print,
};

#endif

#ifdef VCMD_DEBUG_INTERNAL
static void PrintInstr(u32 offset, u32 instr, u32 *size)
{
	char log_buf[512];
	_dbg_log_instr(offset, instr, size, log_buf);
	gf_debug("%s", log_buf);
}
#endif

/*********cmdbuf object management********/
static struct cmdbuf_obj *create_cmdbuf_obj(void)
{
	struct cmdbuf_obj *cmdbuf_obj = NULL;

	cmdbuf_obj = gf_kvcalloc(1, sizeof(*cmdbuf_obj), GFP_KERNEL);
	if (!cmdbuf_obj) {
		gf_debug("%s\n", "gf_kvcalloc for cmdbuf_obj fail!");
		return cmdbuf_obj;
	}
	gf_memset(cmdbuf_obj, 0, sizeof(struct cmdbuf_obj));
	return cmdbuf_obj;
}

static void free_cmdbuf_obj(struct cmdbuf_obj *cmdbuf_obj)
{
#ifdef DYNAMIC_MALLOC_VCMDNODE
	if (!cmdbuf_obj) {
		gf_debug("%s\n", "remove_cmdbuf_obj NULL");
		return;
	}
	//free current cmdbuf_obj
	gf_kvfree(cmdbuf_obj);
#endif
}

static struct cmdbuf_obj *create_vcmd_cmdbuf_obj(u16 cmdbuf_id)
{
	struct cmdbuf_obj *cmdbuf_obj = NULL;

#ifdef DYNAMIC_MALLOC_VCMDNODE
	cmdbuf_obj = create_cmdbuf_obj();
#else
	cmdbuf_obj = g_cmdbuf_obj_pool + cmdbuf_id;
	if (cmdbuf_obj)
		gf_memset(cmdbuf_obj, 0, sizeof(struct cmdbuf_obj));
#endif
	return cmdbuf_obj;
}

static bi_list_node *bi_list_create_vcmd_node(u16 cmdbuf_id)
{
	bi_list_node *node = NULL;

#ifdef DYNAMIC_MALLOC_VCMDNODE
	node = bi_list_create_node();
#else
	node = g_cmdbuf_node_pool + cmdbuf_id;
	if (node)
		gf_memset(node, 0, sizeof(bi_list_node));
#endif
	return node;
}

static void free_cmdbuf_mem(vcmd_mgr_t *vcmd_mgr, u16 cmdbuf_id)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&vcmd_mgr->vcmd_cmdbuf_alloc_lock, flags);
	vcmd_mgr->cmdbuf_used[cmdbuf_id] = 0;
	vcmd_mgr->cmdbuf_used_residual += 1;
	spin_unlock_irqrestore(&vcmd_mgr->vcmd_cmdbuf_alloc_lock, flags);
	wake_up_interruptible_all(&vcmd_mgr->vcmd_cmdbuf_memory_wait);
}

static bi_list_node *create_cmdbuf_node(vcmd_mgr_t *vcmd_mgr)
{
	bi_list_node *current_node = NULL;
	struct cmdbuf_obj *cmdbuf_obj = NULL;
	struct noncache_mem new_cmdbuf_addr;
	struct noncache_mem new_status_cmdbuf_addr;

	if (wait_event_interruptible(vcmd_mgr->vcmd_cmdbuf_memory_wait,
					 allocate_cmdbuf(vcmd_mgr,  &new_cmdbuf_addr,
							 &new_status_cmdbuf_addr)))
		return NULL;
	cmdbuf_obj = create_vcmd_cmdbuf_obj(new_cmdbuf_addr.cmdbuf_id);
	if (!cmdbuf_obj) {
		gf_debug("%s\n", "create_vcmd_cmdbuf_obj fail!");
		free_cmdbuf_mem(vcmd_mgr, new_cmdbuf_addr.cmdbuf_id);
		return NULL;
	}
	cmdbuf_obj->cmdbuf_bus_address = new_cmdbuf_addr.bus_address;
	cmdbuf_obj->mmu_cmdbuf_bus_address = new_cmdbuf_addr.mmu_bus_address;
	cmdbuf_obj->cmdbuf_virtual_address = new_cmdbuf_addr.virtual_address;
	cmdbuf_obj->cmdbuf_size = new_cmdbuf_addr.size;
	cmdbuf_obj->cmdbuf_id = new_cmdbuf_addr.cmdbuf_id;
	cmdbuf_obj->status_bus_address = new_status_cmdbuf_addr.bus_address;
	cmdbuf_obj->mmu_status_bus_address =
		new_status_cmdbuf_addr.mmu_bus_address;
	cmdbuf_obj->status_virtual_address =
		new_status_cmdbuf_addr.virtual_address;
	cmdbuf_obj->status_size = new_status_cmdbuf_addr.size;
	current_node = bi_list_create_vcmd_node(cmdbuf_obj->cmdbuf_id);
	if (!current_node) {
		gf_debug("%s\n", "bi_list_create_vcmd_node fail!");
		free_cmdbuf_mem(vcmd_mgr, new_cmdbuf_addr.cmdbuf_id);
		free_cmdbuf_obj(cmdbuf_obj);
		return NULL;
	}
	current_node->data = (void *)cmdbuf_obj;
	current_node->next = NULL;
	current_node->previous = NULL;
	return current_node;
}

static void free_cmdbuf_node(vcmd_mgr_t *vcmd_mgr, bi_list_node *cmdbuf_node)
{
	struct cmdbuf_obj *cmdbuf_obj = NULL;

	if (!cmdbuf_node) {
		gf_debug("%s\n", "remove_cmdbuf_node NULL");
		return;
	}
	cmdbuf_obj = (struct cmdbuf_obj *)cmdbuf_node->data;
	//free cmdbuf mem in pool
	free_cmdbuf_mem(vcmd_mgr, cmdbuf_obj->cmdbuf_id);

	//free struct cmdbuf_obj
	free_cmdbuf_obj(cmdbuf_obj);
#ifdef DYNAMIC_MALLOC_VCMDNODE
	//free current cmdbuf_node entity.
	bi_list_free_node(cmdbuf_node);
#endif
}

//just remove, not free the node.
static bi_list_node *remove_cmdbuf_node_from_list(bi_list *list,
						  bi_list_node *cmdbuf_node)
{
	struct cmdbuf_obj *cmdbuf_obj_temp = NULL;
	if (!cmdbuf_node) {
		gf_debug("%s\n", "remove_cmdbuf_node_from_list  NULL");
		return NULL;
	}
	if (cmdbuf_node->next) {
		cmdbuf_obj_temp = (struct cmdbuf_obj *)cmdbuf_node->next->data;
		if (cmdbuf_obj_temp->cmdbuf_data_linked != 1) {//next node is not linked, not delate current
			gf_debug("can't remove current node\n");
			return NULL;
		}
		bi_list_remove_node(list, cmdbuf_node);
		return cmdbuf_node;
	}
	//the last one, should not be removed.
	return NULL;
}

//calculate executing_time of each vcmd
static u64 calculate_executing_time_after_node(bi_list_node *exe_cmdbuf_node)
{
	u64 time_run_all = 0;
	struct cmdbuf_obj *cmdbuf_obj_temp = NULL;

	while (1) {
		if (!exe_cmdbuf_node)
			break;
		cmdbuf_obj_temp = (struct cmdbuf_obj *)exe_cmdbuf_node->data;
		time_run_all += cmdbuf_obj_temp->executing_time;
		exe_cmdbuf_node = exe_cmdbuf_node->next;
	}
	return time_run_all;
}

static u64
calculate_executing_time_after_node_high_priority(bi_list_node *exe_cmdbuf_node)
{
	u64 time_run_all = 0;
	struct cmdbuf_obj *cmdbuf_obj_temp = NULL;

	if (!exe_cmdbuf_node)
		return time_run_all;
	cmdbuf_obj_temp = (struct cmdbuf_obj *)exe_cmdbuf_node->data;
	time_run_all += cmdbuf_obj_temp->executing_time;
	exe_cmdbuf_node = exe_cmdbuf_node->next;
	while (1) {
		if (!exe_cmdbuf_node)
			break;
		cmdbuf_obj_temp = (struct cmdbuf_obj *)exe_cmdbuf_node->data;
		if (cmdbuf_obj_temp->priority == CMDBUF_PRIORITY_NORMAL)
			break;
		time_run_all += cmdbuf_obj_temp->executing_time;
		exe_cmdbuf_node = exe_cmdbuf_node->next;
	}
	return time_run_all;
}

/**********cmdbuf pool management*************/

static int allocate_cmdbuf(vcmd_mgr_t *vcmd_mgr,
			   struct noncache_mem *new_cmdbuf_addr,
			   struct noncache_mem *new_status_cmdbuf_addr)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&vcmd_mgr->vcmd_cmdbuf_alloc_lock, flags);
	if (vcmd_mgr->cmdbuf_used_residual == 0) {
		spin_unlock_irqrestore(&vcmd_mgr->vcmd_cmdbuf_alloc_lock, flags);
		//no empty cmdbuf
		return 0;
	}
	//there is one cmdbuf at least
	while (1) {
		if (vcmd_mgr->cmdbuf_used[vcmd_mgr->cmdbuf_used_pos] == 0 &&
			!vcmd_mgr->global_cmdbuf_node[vcmd_mgr->cmdbuf_used_pos]) {
			vcmd_mgr->cmdbuf_used[vcmd_mgr->cmdbuf_used_pos] = 1;
			vcmd_mgr->cmdbuf_used_residual -= 1;
			new_cmdbuf_addr->virtual_address =
				vcmd_mgr->vcmd_buf_mem_pool.virtual_address +
				vcmd_mgr->cmdbuf_used_pos * CMDBUF_MAX_SIZE / 4;
			new_cmdbuf_addr->bus_address =
				vcmd_mgr->vcmd_buf_mem_pool.bus_address +
				vcmd_mgr->cmdbuf_used_pos * CMDBUF_MAX_SIZE;

			new_cmdbuf_addr->mmu_bus_address =
				vcmd_mgr->vcmd_buf_mem_pool.mmu_bus_address +
				vcmd_mgr->cmdbuf_used_pos * CMDBUF_MAX_SIZE;

			new_cmdbuf_addr->size = CMDBUF_MAX_SIZE;
			new_cmdbuf_addr->cmdbuf_id = vcmd_mgr->cmdbuf_used_pos;
			new_status_cmdbuf_addr->virtual_address =
				vcmd_mgr->vcmd_status_buf_mem_pool.virtual_address +
				vcmd_mgr->cmdbuf_used_pos * CMDBUF_MAX_SIZE / 4;
			new_status_cmdbuf_addr->bus_address =
				vcmd_mgr->vcmd_status_buf_mem_pool.bus_address +
				vcmd_mgr->cmdbuf_used_pos * CMDBUF_MAX_SIZE;

			new_status_cmdbuf_addr->mmu_bus_address =
				vcmd_mgr->vcmd_status_buf_mem_pool.mmu_bus_address +
				vcmd_mgr->cmdbuf_used_pos * CMDBUF_MAX_SIZE;

			new_status_cmdbuf_addr->size = CMDBUF_MAX_SIZE;
			new_status_cmdbuf_addr->cmdbuf_id = vcmd_mgr->cmdbuf_used_pos;
			vcmd_mgr->cmdbuf_used_pos++;
			if (vcmd_mgr->cmdbuf_used_pos >= TOTAL_DISCRETE_CMDBUF_NUM)
				vcmd_mgr->cmdbuf_used_pos = 0;
			spin_unlock_irqrestore(&vcmd_mgr->vcmd_cmdbuf_alloc_lock, flags);

	gf_debug("new vcmd_buf_mem_pool.virtual_address=0x%llx, vcmd_mgr->vcmd_buf_mem_pool.bus_address=0x%llx\n",
		(unsigned long long)new_cmdbuf_addr->virtual_address,
		(unsigned long long)new_cmdbuf_addr->bus_address);

	gf_debug("new vcmd_status_buf_mem_pool.virtual_address=0x%llx, vcmd_mgr->vcmd_status_buf_mem_pool.bus_address = 0x%llx\n",
		(unsigned long long)new_status_cmdbuf_addr->virtual_address,
		(unsigned long long)new_status_cmdbuf_addr->bus_address);

			return 1;
		}
		vcmd_mgr->cmdbuf_used_pos++;
		if (vcmd_mgr->cmdbuf_used_pos >= TOTAL_DISCRETE_CMDBUF_NUM)
			vcmd_mgr->cmdbuf_used_pos = 0;
	}
	return 0;
}

static bi_list_node *get_cmdbuf_node_in_list_by_addr(size_t cmdbuf_addr,
							 bi_list *list, vcmd_mgr_t *vcmd_mgr)
{
	bi_list_node *new_cmdbuf_node = NULL;
	struct cmdbuf_obj *cmdbuf_obj = NULL;

	new_cmdbuf_node = list->head;
	while (1) {
		if (!new_cmdbuf_node)
			return NULL;
		cmdbuf_obj = (struct cmdbuf_obj *)new_cmdbuf_node->data;
		if (((cmdbuf_obj->cmdbuf_bus_address - vcmd_mgr->base_ddr_addr) <=
			 cmdbuf_addr) &&
			(((cmdbuf_obj->cmdbuf_bus_address - vcmd_mgr->base_ddr_addr +
			   cmdbuf_obj->cmdbuf_size) > cmdbuf_addr))) {
			return new_cmdbuf_node;
		}
		new_cmdbuf_node = new_cmdbuf_node->next;
	}
	return NULL;
}

static int wait_abort_rdy(struct hantrovcmd_dev *dev)
{
	return dev->working_state == WORKING_STATE_IDLE;
}

static int is_supported_core(u32 core_mask, u16 dev_pos)
{
	if (core_mask && ((core_mask >> dev_pos) & 0x1))
		return 1; //found one supported core

	return 0; //not found the supported core
}

static int select_vcmd(vcmd_mgr_t *vcmd_mgr, bi_list_node *new_cmdbuf_node)
{
	struct cmdbuf_obj *cmdbuf_obj = NULL;
	bi_list_node *curr_cmdbuf_node = NULL;
	bi_list *list = NULL;
	struct hantrovcmd_dev *dev = NULL;
	struct hantrovcmd_dev *smallest_dev = NULL;
	u64 executing_time = 0xffffffffffffffff;

	int counter = 0;
	unsigned long flags = 0;
	u32 hw_rdy_cmdbuf_num = 0;
	size_t exe_cmdbuf_addr = 0;
	struct cmdbuf_obj *cmdbuf_obj_temp = NULL;
	u32 cmdbuf_id = 0;
	u32 core_mask;
	int ret;

	cmdbuf_obj = (struct cmdbuf_obj *)new_cmdbuf_node->data;
	core_mask = cmdbuf_obj->core_mask;
	gf_debug("select vcmd\n");
	//there is an empty vcmd to be used
	while (1) {
		ret = is_supported_core(core_mask, vcmd_mgr->vcmd_position[cmdbuf_obj->module_type]);
		if (ret != 0) {
			dev = vcmd_mgr->vcmd_manager[cmdbuf_obj->module_type][vcmd_mgr->vcmd_position[cmdbuf_obj->module_type]];

			list = &dev->list_manager;
			spin_lock_irqsave(dev->spinlock, flags);
			if (!list->tail) {
				bi_list_insert_node_tail(list, new_cmdbuf_node);
				spin_unlock_irqrestore(dev->spinlock, flags);
				vcmd_mgr->vcmd_position[cmdbuf_obj->module_type]++;
				if (vcmd_mgr->vcmd_position[cmdbuf_obj->module_type] >=
					vcmd_mgr->vcmd_type_core_num[cmdbuf_obj->module_type])
					vcmd_mgr->vcmd_position[cmdbuf_obj->module_type] = 0;
				cmdbuf_obj->core_id = dev->core_id;
				return 0;
			}
			spin_unlock_irqrestore(dev->spinlock, flags);
		}
		vcmd_mgr->vcmd_position[cmdbuf_obj->module_type]++;
		if (vcmd_mgr->vcmd_position[cmdbuf_obj->module_type] >=
				vcmd_mgr->vcmd_type_core_num[cmdbuf_obj->module_type])
			vcmd_mgr->vcmd_position[cmdbuf_obj->module_type] = 0;
		counter++;

		if (counter >= vcmd_mgr->vcmd_type_core_num[cmdbuf_obj->module_type])
			break;
	}
	/* there is a vcmd which tail node -> cmdbuf_run_done == 1.
	 * It means this vcmd has nothing to do, so we select it.
	 */
	counter = 0;
	while (1) {
		ret = is_supported_core(core_mask, vcmd_mgr->vcmd_position[cmdbuf_obj->module_type]);

		if (ret != 0) {
			dev = vcmd_mgr->vcmd_manager[cmdbuf_obj->module_type][vcmd_mgr->vcmd_position[cmdbuf_obj->module_type]];
			list = &dev->list_manager;
			spin_lock_irqsave(dev->spinlock, flags);
			curr_cmdbuf_node = list->tail;
			if (!curr_cmdbuf_node) {
				bi_list_insert_node_tail(list, new_cmdbuf_node);
				spin_unlock_irqrestore(dev->spinlock, flags);
				vcmd_mgr->vcmd_position[cmdbuf_obj->module_type]++;
				if (vcmd_mgr->vcmd_position[cmdbuf_obj->module_type] >=
					vcmd_mgr->vcmd_type_core_num[cmdbuf_obj->module_type])
					vcmd_mgr->vcmd_position[cmdbuf_obj->module_type] = 0;
				cmdbuf_obj->core_id = dev->core_id;
				return 0;
			}
			cmdbuf_obj_temp = (struct cmdbuf_obj *)curr_cmdbuf_node->data;
			if (cmdbuf_obj_temp->cmdbuf_run_done == 1) {
				bi_list_insert_node_tail(list, new_cmdbuf_node);
				spin_unlock_irqrestore(dev->spinlock, flags);
				vcmd_mgr->vcmd_position[cmdbuf_obj->module_type]++;
				if (vcmd_mgr->vcmd_position[cmdbuf_obj->module_type] >=
					vcmd_mgr->vcmd_type_core_num[cmdbuf_obj->module_type])
					vcmd_mgr->vcmd_position[cmdbuf_obj->module_type] = 0;
				cmdbuf_obj->core_id = dev->core_id;
				return 0;
			}
			spin_unlock_irqrestore(dev->spinlock, flags);
		}
		vcmd_mgr->vcmd_position[cmdbuf_obj->module_type]++;
		if (vcmd_mgr->vcmd_position[cmdbuf_obj->module_type] >=
				vcmd_mgr->vcmd_type_core_num[cmdbuf_obj->module_type])
			vcmd_mgr->vcmd_position[cmdbuf_obj->module_type] = 0;
		counter++;

		if (counter >= vcmd_mgr->vcmd_type_core_num[cmdbuf_obj->module_type])
			break;
	}

	/* another case, tail = executing node, and
	 * vcmd=pend state (finish but not generate interrupt)
	 */
	counter = 0;
	while (1) {
		ret = is_supported_core(core_mask, vcmd_mgr->vcmd_position[cmdbuf_obj->module_type]);
		if (ret != 0) {
			dev = vcmd_mgr->vcmd_manager[cmdbuf_obj->module_type]
					[vcmd_mgr->vcmd_position[cmdbuf_obj->module_type]];
			list = &dev->list_manager;
			//read executing cmdbuf address
			if (dev->hw_version_id <= HW_ID_1_0_C)
				hw_rdy_cmdbuf_num = vcmd_get_register_value(
					(const void *)dev->hwregs, dev->reg_mirror,
					HWIF_VCMD_EXE_CMDBUF_COUNT);
			else {
				hw_rdy_cmdbuf_num =
					*(dev->vcmd_reg_mem_virtual_address +
					VCMD_EXE_CMDBUF_COUNT);
				if (hw_rdy_cmdbuf_num != dev->sw_cmdbuf_rdy_num)
					hw_rdy_cmdbuf_num += 1;
			}
			spin_lock_irqsave(dev->spinlock, flags);
			curr_cmdbuf_node = list->tail;
			if (!curr_cmdbuf_node) {
				bi_list_insert_node_tail(list, new_cmdbuf_node);
				spin_unlock_irqrestore(dev->spinlock, flags);
				vcmd_mgr->vcmd_position[cmdbuf_obj->module_type]++;
				if (vcmd_mgr->vcmd_position[cmdbuf_obj->module_type] >=
					vcmd_mgr->vcmd_type_core_num[cmdbuf_obj->module_type])
					vcmd_mgr->vcmd_position[cmdbuf_obj->module_type] = 0;
				cmdbuf_obj->core_id = dev->core_id;
				return 0;
			}
			if (dev->sw_cmdbuf_rdy_num == hw_rdy_cmdbuf_num) {
				bi_list_insert_node_tail(list, new_cmdbuf_node);
				spin_unlock_irqrestore(dev->spinlock, flags);
				vcmd_mgr->vcmd_position[cmdbuf_obj->module_type]++;
				if (vcmd_mgr->vcmd_position[cmdbuf_obj->module_type] >=
					vcmd_mgr->vcmd_type_core_num[cmdbuf_obj->module_type])
					vcmd_mgr->vcmd_position[cmdbuf_obj->module_type] = 0;
				cmdbuf_obj->core_id = dev->core_id;
				return 0;
			}
			spin_unlock_irqrestore(dev->spinlock, flags);
		}
		vcmd_mgr->vcmd_position[cmdbuf_obj->module_type]++;
		if (vcmd_mgr->vcmd_position[cmdbuf_obj->module_type] >=
				vcmd_mgr->vcmd_type_core_num[cmdbuf_obj->module_type])
			vcmd_mgr->vcmd_position[cmdbuf_obj->module_type] = 0;
		counter++;

		if (counter >= vcmd_mgr->vcmd_type_core_num[cmdbuf_obj->module_type])
			break;
	}
	// there is no idle vcmd,if low priority,
	// calculate exe time, select the least one.
	// or if high priority, calculate the exe time,
	// select the least one and abort it.
	if (cmdbuf_obj->priority == CMDBUF_PRIORITY_NORMAL) {
		counter = 0;
		//calculate total execute time of all devices
		while (1) {
			ret = is_supported_core(core_mask, vcmd_mgr->vcmd_position[cmdbuf_obj->module_type]);

			if (ret != 0) {
				dev = vcmd_mgr->vcmd_manager
					[cmdbuf_obj->module_type]
					[vcmd_mgr->vcmd_position[cmdbuf_obj->module_type]];
				//read executing cmdbuf address
				if (dev->hw_version_id <= HW_ID_1_0_C) {
					exe_cmdbuf_addr = VCMDGetAddrRegisterValue(
						(const void *)dev->hwregs,
						dev->reg_mirror,
						HWIF_VCMD_EXECUTING_CMD_ADDR);
					list = &dev->list_manager;
					spin_lock_irqsave(dev->spinlock, flags);
					//get the executing cmdbuf node.
					curr_cmdbuf_node =
						get_cmdbuf_node_in_list_by_addr(
							exe_cmdbuf_addr, list, vcmd_mgr);

					//calculate total execute time of this device
					dev->total_exe_time =
						calculate_executing_time_after_node(
							curr_cmdbuf_node);
					spin_unlock_irqrestore(dev->spinlock, flags);
				} else {
					//cmdbuf_id = vcmd_get_register_value((const void *)dev->hwregs,
					//dev->reg_mirror,HWIF_VCMD_CMDBUF_EXECUTING_ID);
					cmdbuf_id =
						*(dev->vcmd_reg_mem_virtual_address +
						EXECUTING_CMDBUF_ID_ADDR + 1);
					spin_lock_irqsave(dev->spinlock, flags);
					if (cmdbuf_id >= TOTAL_DISCRETE_CMDBUF_NUM ||
						cmdbuf_id == 0) {
						gf_error("cmdbuf_id greater than the ceiling !!\n");
						spin_unlock_irqrestore(dev->spinlock,
									flags);
						return -1;
					}
					//get the executing cmdbuf node.
					curr_cmdbuf_node =
						vcmd_mgr->global_cmdbuf_node[cmdbuf_id];
					if (!curr_cmdbuf_node) {
						list = &dev->list_manager;
						curr_cmdbuf_node = list->head;
						while (1) {
							if (!curr_cmdbuf_node)
								break;
							cmdbuf_obj_temp =
								(struct cmdbuf_obj *)
									curr_cmdbuf_node
										->data;
							if (cmdbuf_obj_temp
									->cmdbuf_data_linked &&
								cmdbuf_obj_temp->cmdbuf_run_done ==
									0)
								break;
							curr_cmdbuf_node =
								curr_cmdbuf_node->next;
						}
					}

					//calculate total execute time of this device
					dev->total_exe_time =
						calculate_executing_time_after_node(
							curr_cmdbuf_node);
					spin_unlock_irqrestore(dev->spinlock, flags);
				}
			}
			vcmd_mgr->vcmd_position[cmdbuf_obj->module_type]++;
			if (vcmd_mgr->vcmd_position[cmdbuf_obj->module_type] >=
				vcmd_mgr->vcmd_type_core_num[cmdbuf_obj->module_type])
				vcmd_mgr->vcmd_position[cmdbuf_obj->module_type] = 0;
			counter++;
			if (counter >=
				vcmd_mgr->vcmd_type_core_num[cmdbuf_obj->module_type])
				break;
		}
		//find the device with the least total_exe_time.
		counter = 0;
		executing_time = 0xffffffffffffffff;
		while (1) {
			ret = is_supported_core(core_mask, vcmd_mgr->vcmd_position[cmdbuf_obj->module_type]);

			if (ret != 0) {
				dev = vcmd_mgr->vcmd_manager
					[cmdbuf_obj->module_type]
					[vcmd_mgr->vcmd_position[cmdbuf_obj->module_type]];
				if (dev->total_exe_time <= executing_time) {
					executing_time = dev->total_exe_time;
					smallest_dev = dev;
				}
			}
			vcmd_mgr->vcmd_position[cmdbuf_obj->module_type]++;
			if (vcmd_mgr->vcmd_position[cmdbuf_obj->module_type] >=
				vcmd_mgr->vcmd_type_core_num[cmdbuf_obj->module_type])
				vcmd_mgr->vcmd_position[cmdbuf_obj->module_type] = 0;
			counter++;
			if (counter >=
				vcmd_mgr->vcmd_type_core_num[cmdbuf_obj->module_type])
				break;
		}
		//insert list
		list = &smallest_dev->list_manager;
		gf_debug("select vcmd smallest_dev==NULL?%d\n", smallest_dev==NULL?1:0);
		if(!smallest_dev){
			spin_lock_irqsave(smallest_dev->spinlock, flags);
			bi_list_insert_node_tail(list, new_cmdbuf_node);
			spin_unlock_irqrestore(smallest_dev->spinlock, flags);
			cmdbuf_obj->core_id = smallest_dev->core_id;
		}
		return 0;
	}
	//CMDBUF_PRIORITY_HIGH
	counter = 0;
	//calculate total execute time of all devices
	while (1) {
		ret = is_supported_core(core_mask, vcmd_mgr->vcmd_position[cmdbuf_obj->module_type]);
		if (ret != 0) {
			dev = vcmd_mgr->vcmd_manager[cmdbuf_obj->module_type][vcmd_mgr->vcmd_position[cmdbuf_obj->module_type]];
			if (dev->hw_version_id <= HW_ID_1_0_C) {
				//read executing cmdbuf address
				exe_cmdbuf_addr = VCMDGetAddrRegisterValue((const void *)dev->hwregs,
									dev->reg_mirror,
					HWIF_VCMD_EXECUTING_CMD_ADDR);
				list = &dev->list_manager;
				spin_lock_irqsave(dev->spinlock, flags);
				//get the executing cmdbuf node.
				curr_cmdbuf_node =
					get_cmdbuf_node_in_list_by_addr(exe_cmdbuf_addr,
									list, vcmd_mgr);

				//calculate total execute time of this device
				dev->total_exe_time =
					calculate_executing_time_after_node_high_priority(curr_cmdbuf_node);
				spin_unlock_irqrestore(dev->spinlock, flags);
			} else {
				//cmdbuf_id = vcmd_get_register_value((const void *)dev->hwregs,
				//dev->reg_mirror,HWIF_VCMD_CMDBUF_EXECUTING_ID);
				cmdbuf_id = *(dev->vcmd_reg_mem_virtual_address +
						EXECUTING_CMDBUF_ID_ADDR);
				spin_lock_irqsave(dev->spinlock, flags);
				if (cmdbuf_id >= TOTAL_DISCRETE_CMDBUF_NUM ||
					cmdbuf_id == 0) {
					gf_error("cmdbuf_id greater than the ceiling !!\n");
					spin_unlock_irqrestore(dev->spinlock,
								flags);
					return -1;
				}
				//get the executing cmdbuf node.
				curr_cmdbuf_node = vcmd_mgr->global_cmdbuf_node[cmdbuf_id];
				if (!curr_cmdbuf_node) {
					list = &dev->list_manager;
					curr_cmdbuf_node = list->head;
					while (1) {
						if (!curr_cmdbuf_node)
							break;
						cmdbuf_obj_temp = (struct cmdbuf_obj *)curr_cmdbuf_node->data;
						if (cmdbuf_obj_temp->cmdbuf_data_linked &&
							cmdbuf_obj_temp->cmdbuf_run_done ==
							0)
							break;
						curr_cmdbuf_node = curr_cmdbuf_node->next;
					}
				}

				//calculate total execute time of this device
				dev->total_exe_time =
					calculate_executing_time_after_node(curr_cmdbuf_node);
				spin_unlock_irqrestore(dev->spinlock, flags);
			}
		}
		vcmd_mgr->vcmd_position[cmdbuf_obj->module_type]++;
		if (vcmd_mgr->vcmd_position[cmdbuf_obj->module_type] >=
			vcmd_mgr->vcmd_type_core_num[cmdbuf_obj->module_type])
			vcmd_mgr->vcmd_position[cmdbuf_obj->module_type] = 0;
		counter++;
		if (counter >= vcmd_mgr->vcmd_type_core_num[cmdbuf_obj->module_type])
			break;
	}
	//find the smallest device.
	counter = 0;
	executing_time = 0xffffffffffffffff;
	while (1) {
		ret = is_supported_core(core_mask, vcmd_mgr->vcmd_position[cmdbuf_obj->module_type]);
		if (ret != 0) {
			dev = vcmd_mgr->vcmd_manager[cmdbuf_obj->module_type][vcmd_mgr->vcmd_position[cmdbuf_obj->module_type]];
			if (dev->total_exe_time <= executing_time) {
				executing_time = dev->total_exe_time;
				smallest_dev = dev;
			}
		}
		vcmd_mgr->vcmd_position[cmdbuf_obj->module_type]++;
		if (vcmd_mgr->vcmd_position[cmdbuf_obj->module_type] >=
			vcmd_mgr->vcmd_type_core_num[cmdbuf_obj->module_type])
			vcmd_mgr->vcmd_position[cmdbuf_obj->module_type] = 0;
		counter++;
		if (counter >= vcmd_mgr->vcmd_type_core_num[cmdbuf_obj->module_type])
			break;
	}
	//abort the vcmd and wait
	vcmd_write_register_value((const void *)smallest_dev->hwregs,
				  smallest_dev->reg_mirror,
					HWIF_VCMD_START_TRIGGER, 0);
	if (wait_event_interruptible(*smallest_dev->wait_abort_queue,
					 wait_abort_rdy(smallest_dev)))
		return -ERESTARTSYS;

	
	// need to select inserting position again
	// because hw maybe have run to the next node.
	// CMDBUF_PRIORITY_HIGH
	spin_lock_irqsave(smallest_dev->spinlock, flags);
	curr_cmdbuf_node = smallest_dev->list_manager.head;
	
	while (1) {
		//if list is empty or tail,insert to tail
		if (!curr_cmdbuf_node)
			break;
		cmdbuf_obj_temp =
			(struct cmdbuf_obj *)curr_cmdbuf_node->data;
		//if find the first node which priority is normal,
		//insert node prior to  the node
		if ((cmdbuf_obj_temp->priority ==
			CMDBUF_PRIORITY_NORMAL) &&
			(cmdbuf_obj_temp->cmdbuf_run_done == 0))
			break;
		curr_cmdbuf_node = curr_cmdbuf_node->next;
	}


	bi_list_insert_node_before(list, curr_cmdbuf_node,
				   new_cmdbuf_node);
	cmdbuf_obj->core_id = smallest_dev->core_id;
	spin_unlock_irqrestore(smallest_dev->spinlock, flags);
	return 0;
}

static struct process_manager_obj *get_process_manager_obj(
															vcmd_mgr_t *vcmd_mgr, struct drm_file *file)
{
	unsigned long flags = 0;
	bi_list_node *process_manager_node = NULL;
	struct process_manager_obj *pmo = NULL;

	spin_lock_irqsave(&vcmd_mgr->vcmd_process_manager_lock, flags);
	process_manager_node = vcmd_mgr->global_process_manager.head;
	while (1) {
		if (!process_manager_node) {
			gf_error("hantrovcmd: ERROR process_manager_node !!\n");
			spin_unlock_irqrestore(&vcmd_mgr->vcmd_process_manager_lock, flags);
			return NULL;
		}
		pmo = (struct process_manager_obj *)process_manager_node->data;
		gf_debug("reserve process loop: node %p for file %p\n",
						(void *)process_manager_node, (void *)pmo->file);
		if (file == pmo->file)
			break;

		process_manager_node = process_manager_node->next;
	}
	spin_unlock_irqrestore(&vcmd_mgr->vcmd_process_manager_lock, flags);

	return pmo;
}

static int
wait_process_resource_rdy(struct process_manager_obj *process_manager_obj)
{
	return process_manager_obj->total_exe_time <=
		   PROCESS_MAX_SUM_OF_IMAGE_SIZE;
}

static int select_vcmd_in_reserve(vcmd_mgr_t *vcmd_mgr,
					bi_list_node *new_cmdbuf_node)
{
	struct cmdbuf_obj *cmdbuf_obj = NULL;
	bi_list_node *curr_cmdbuf_node = NULL;
	bi_list *list = NULL;
	struct hantrovcmd_dev *dev = NULL;
	struct hantrovcmd_dev *smallest_dev = NULL;
	unsigned long long executing_time = 0xffffffffffffffff;
	int counter = 0;
	unsigned long flags = 0;
	unsigned int hw_rdy_cmdbuf_num = 0;
	size_t exe_cmdbuf_addr = 0;
	struct cmdbuf_obj *cmdbuf_obj_temp = NULL;
	unsigned int cmdbuf_id = 0;
	unsigned int core_mask;
	int ret;

	cmdbuf_obj = (struct cmdbuf_obj *)new_cmdbuf_node->data;
	if (!cmdbuf_obj)
		return -EINVAL;

	core_mask = cmdbuf_obj->core_mask;

	//there is an empty vcmd to be used
	while (1) {
		ret = is_supported_core(core_mask, vcmd_mgr->vcmd_position[cmdbuf_obj->module_type]);
		if (ret != 0) {
			dev = vcmd_mgr->vcmd_manager[cmdbuf_obj->module_type][vcmd_mgr->vcmd_position[cmdbuf_obj->module_type]];

			list = &dev->list_manager;
			spin_lock_irqsave(dev->spinlock, flags);
			if (!list->tail) {
				spin_unlock_irqrestore(dev->spinlock, flags);
				vcmd_mgr->vcmd_position[cmdbuf_obj->module_type]++;
				if (vcmd_mgr->vcmd_position[cmdbuf_obj->module_type] >=
				    vcmd_mgr->vcmd_type_core_num[cmdbuf_obj->module_type])
					vcmd_mgr->vcmd_position[cmdbuf_obj->module_type] = 0;
				cmdbuf_obj->core_id = dev->core_id;
				return 0;
			}
			spin_unlock_irqrestore(dev->spinlock, flags);
		}
		vcmd_mgr->vcmd_position[cmdbuf_obj->module_type]++;
		if (vcmd_mgr->vcmd_position[cmdbuf_obj->module_type] >=
				vcmd_mgr->vcmd_type_core_num[cmdbuf_obj->module_type])
			vcmd_mgr->vcmd_position[cmdbuf_obj->module_type] = 0;
		counter++;

		if (counter >= vcmd_mgr->vcmd_type_core_num[cmdbuf_obj->module_type])
			break;
	}

	/* there is a vcmd which tail node -> cmdbuf_run_done == 1.
	 * It means this vcmd has nothing to do, so we select it.
	 */
	counter = 0;
	while (1) {
		ret = is_supported_core(core_mask, vcmd_mgr->vcmd_position[cmdbuf_obj->module_type]);

		if (ret != 0) {
			dev = vcmd_mgr->vcmd_manager[cmdbuf_obj->module_type][vcmd_mgr->vcmd_position[cmdbuf_obj->module_type]];
			list = &dev->list_manager;
			spin_lock_irqsave(dev->spinlock, flags);
			curr_cmdbuf_node = list->tail;
			if (!curr_cmdbuf_node) {
				spin_unlock_irqrestore(dev->spinlock, flags);
				vcmd_mgr->vcmd_position[cmdbuf_obj->module_type]++;
				if (vcmd_mgr->vcmd_position[cmdbuf_obj->module_type] >=
					vcmd_mgr->vcmd_type_core_num[cmdbuf_obj->module_type])
					vcmd_mgr->vcmd_position[cmdbuf_obj->module_type] = 0;
				cmdbuf_obj->core_id = dev->core_id;
				return 0;
			}
			cmdbuf_obj_temp = (struct cmdbuf_obj *)curr_cmdbuf_node->data;
			if (cmdbuf_obj_temp->cmdbuf_run_done == 1) {
				spin_unlock_irqrestore(dev->spinlock, flags);
				vcmd_mgr->vcmd_position[cmdbuf_obj->module_type]++;
				if (vcmd_mgr->vcmd_position[cmdbuf_obj->module_type] >=
					vcmd_mgr->vcmd_type_core_num[cmdbuf_obj->module_type])
					vcmd_mgr->vcmd_position[cmdbuf_obj->module_type] = 0;
				cmdbuf_obj->core_id = dev->core_id;
				return 0;
			}
			spin_unlock_irqrestore(dev->spinlock, flags);
		}
		vcmd_mgr->vcmd_position[cmdbuf_obj->module_type]++;
		if (vcmd_mgr->vcmd_position[cmdbuf_obj->module_type] >=
				vcmd_mgr->vcmd_type_core_num[cmdbuf_obj->module_type])
			vcmd_mgr->vcmd_position[cmdbuf_obj->module_type] = 0;
		counter++;

		if (counter >= vcmd_mgr->vcmd_type_core_num[cmdbuf_obj->module_type])
			break;
	}

	/* another case, tail = executing node, and
	 * vcmd=pend state (finish but not generate interrupt)
	 */
	counter = 0;
	while (1) {
		ret = is_supported_core(core_mask, vcmd_mgr->vcmd_position[cmdbuf_obj->module_type]);
		if (ret != 0) {
			dev = vcmd_mgr->vcmd_manager[cmdbuf_obj->module_type]
					[vcmd_mgr->vcmd_position[cmdbuf_obj->module_type]];
			list = &dev->list_manager;
			//read executing cmdbuf address
			if (dev->hw_version_id <= HW_ID_1_0_C)
				hw_rdy_cmdbuf_num = vcmd_get_register_value(
					(const void *)dev->hwregs, dev->reg_mirror,
					HWIF_VCMD_EXE_CMDBUF_COUNT);
			else {
				hw_rdy_cmdbuf_num =
					*(dev->vcmd_reg_mem_virtual_address +
					VCMD_EXE_CMDBUF_COUNT);
				if (hw_rdy_cmdbuf_num != dev->sw_cmdbuf_rdy_num)
					hw_rdy_cmdbuf_num += 1;
			}
			spin_lock_irqsave(dev->spinlock, flags);
			curr_cmdbuf_node = list->tail;
			if (!curr_cmdbuf_node) {
				spin_unlock_irqrestore(dev->spinlock, flags);
				vcmd_mgr->vcmd_position[cmdbuf_obj->module_type]++;
				if (vcmd_mgr->vcmd_position[cmdbuf_obj->module_type] >=
					vcmd_mgr->vcmd_type_core_num[cmdbuf_obj->module_type])
					vcmd_mgr->vcmd_position[cmdbuf_obj->module_type] = 0;
				cmdbuf_obj->core_id = dev->core_id;
				return 0;
			}
			if (dev->sw_cmdbuf_rdy_num == hw_rdy_cmdbuf_num) {
				spin_unlock_irqrestore(dev->spinlock, flags);
				vcmd_mgr->vcmd_position[cmdbuf_obj->module_type]++;
				if (vcmd_mgr->vcmd_position[cmdbuf_obj->module_type] >=
					vcmd_mgr->vcmd_type_core_num[cmdbuf_obj->module_type])
					vcmd_mgr->vcmd_position[cmdbuf_obj->module_type] = 0;
				cmdbuf_obj->core_id = dev->core_id;
				return 0;
			}
				spin_unlock_irqrestore(dev->spinlock, flags);
		}
		vcmd_mgr->vcmd_position[cmdbuf_obj->module_type]++;
		if (vcmd_mgr->vcmd_position[cmdbuf_obj->module_type] >=
				vcmd_mgr->vcmd_type_core_num[cmdbuf_obj->module_type])
			vcmd_mgr->vcmd_position[cmdbuf_obj->module_type] = 0;
		counter++;

		if (counter >= vcmd_mgr->vcmd_type_core_num[cmdbuf_obj->module_type])
			break;
	}

	// there is no idle vcmd,if low priority,
	// calculate exe time, select the least one.
	// or if high priority, calculate the exe time,
	// select the least one and abort it.
		counter = 0;
		//calculate total execute time of all devices
		while (1) {
			ret = is_supported_core(core_mask, vcmd_mgr->vcmd_position[cmdbuf_obj->module_type]);

			if (ret != 0) {
				dev = vcmd_mgr->vcmd_manager
					[cmdbuf_obj->module_type]
					[vcmd_mgr->vcmd_position[cmdbuf_obj->module_type]];
				//read executing cmdbuf address
				if (dev->hw_version_id <= HW_ID_1_0_C) {
					exe_cmdbuf_addr = VCMDGetAddrRegisterValue(
						(const void *)dev->hwregs,
						dev->reg_mirror,
						HWIF_VCMD_EXECUTING_CMD_ADDR);
					list = &dev->list_manager;
					spin_lock_irqsave(dev->spinlock, flags);
					//get the executing cmdbuf node.
					curr_cmdbuf_node =
						get_cmdbuf_node_in_list_by_addr(
							exe_cmdbuf_addr, list, vcmd_mgr);

					//calculate total execute time of this device
					dev->total_exe_time =
						calculate_executing_time_after_node(
							curr_cmdbuf_node);
						spin_unlock_irqrestore(dev->spinlock, flags);
				} else {
					//cmdbuf_id = vcmd_get_register_value((const void *)dev->hwregs,
					//dev->reg_mirror,HWIF_VCMD_CMDBUF_EXECUTING_ID);
					cmdbuf_id =
						*(dev->vcmd_reg_mem_virtual_address +
						EXECUTING_CMDBUF_ID_ADDR + 1);
					spin_lock_irqsave(dev->spinlock, flags);
					if (cmdbuf_id >= TOTAL_DISCRETE_CMDBUF_NUM ||
						cmdbuf_id == 0) {
						gf_error("cmdbuf_id greater than the ceiling !!\n");
						spin_unlock_irqrestore(dev->spinlock, flags);
						return -1;
					}
					//get the executing cmdbuf node.
					curr_cmdbuf_node =
						vcmd_mgr->global_cmdbuf_node[cmdbuf_id];
					if (!curr_cmdbuf_node) {
						list = &dev->list_manager;
						curr_cmdbuf_node = list->head;
						while (1) {
							if (!curr_cmdbuf_node)
								break;
							cmdbuf_obj_temp =
								(struct cmdbuf_obj *)
									curr_cmdbuf_node
										->data;
							if (cmdbuf_obj_temp
									->cmdbuf_data_linked &&
								cmdbuf_obj_temp->cmdbuf_run_done ==
									0)
								break;
							curr_cmdbuf_node =
								curr_cmdbuf_node->next;
						}
					}

					//calculate total execute time of this device
					dev->total_exe_time =
						calculate_executing_time_after_node(
							curr_cmdbuf_node);
					spin_unlock_irqrestore(dev->spinlock, flags);
				}
			}
			vcmd_mgr->vcmd_position[cmdbuf_obj->module_type]++;
			if (vcmd_mgr->vcmd_position[cmdbuf_obj->module_type] >=
				vcmd_mgr->vcmd_type_core_num[cmdbuf_obj->module_type])
				vcmd_mgr->vcmd_position[cmdbuf_obj->module_type] = 0;
			counter++;
			if (counter >=
				vcmd_mgr->vcmd_type_core_num[cmdbuf_obj->module_type])
				break;
		}
		//find the device with the least total_exe_time.
		counter = 0;
		executing_time = 0xffffffffffffffff;
		while (1) {
			ret = is_supported_core(core_mask, vcmd_mgr->vcmd_position[cmdbuf_obj->module_type]);

			if (ret != 0) {
				dev = vcmd_mgr->vcmd_manager
					[cmdbuf_obj->module_type]
					[vcmd_mgr->vcmd_position[cmdbuf_obj->module_type]];
				if (dev->total_exe_time <= executing_time) {
					executing_time = dev->total_exe_time;
					smallest_dev = dev;
				}
			}
			vcmd_mgr->vcmd_position[cmdbuf_obj->module_type]++;
			if (vcmd_mgr->vcmd_position[cmdbuf_obj->module_type] >=
				vcmd_mgr->vcmd_type_core_num[cmdbuf_obj->module_type])
				vcmd_mgr->vcmd_position[cmdbuf_obj->module_type] = 0;
			counter++;
			if (counter >=
				vcmd_mgr->vcmd_type_core_num[cmdbuf_obj->module_type])
				break;
		}
		if (!smallest_dev)
			return -ENODEV;
		cmdbuf_obj->core_id = smallest_dev->core_id;
		return 0;
}

static long reserve_cmdbuf(vcmd_mgr_t *vcmd_mgr, struct drm_file *file,
			   struct exchange_parameter *input_para)
{
	bi_list_node *new_cmdbuf_node = NULL;
	struct cmdbuf_obj *cmdbuf_obj = NULL;
	struct process_manager_obj *pmo = NULL;
	unsigned long flags = 0;

	input_para->cmdbuf_id = 0;
	if (input_para->cmdbuf_size > CMDBUF_MAX_SIZE)
		return -1;
	gf_debug("reserve cmdbuf file %p\n", (void *)file);
	pmo = get_process_manager_obj(vcmd_mgr, file);
	if (pmo == NULL) {
		//should not happen
		return -1;
	}

	new_cmdbuf_node = create_cmdbuf_node(vcmd_mgr);
	if (!new_cmdbuf_node)
		return -1;

	cmdbuf_obj = (struct cmdbuf_obj *)new_cmdbuf_node->data;
	cmdbuf_obj->module_type = input_para->module_type;
	cmdbuf_obj->priority = input_para->priority;
	cmdbuf_obj->executing_time = input_para->executing_time;
	cmdbuf_obj->cmdbuf_size = CMDBUF_MAX_SIZE;
	input_para->cmdbuf_size = CMDBUF_MAX_SIZE;
	cmdbuf_obj->file = file;
	cmdbuf_obj->process_manager_obj = pmo;
	cmdbuf_obj->core_mask = input_para->core_mask;
	spin_lock_irqsave(&vcmd_mgr->mutex, flags);
	if (select_vcmd_in_reserve(vcmd_mgr, new_cmdbuf_node))
	{
		spin_unlock_irqrestore(&vcmd_mgr->mutex, flags);
		return -1;
	}
	spin_unlock_irqrestore(&vcmd_mgr->mutex, flags);
	input_para->cmdbuf_id = cmdbuf_obj->cmdbuf_id;
	vcmd_mgr->global_cmdbuf_node[input_para->cmdbuf_id] = new_cmdbuf_node;
	spin_lock_irqsave(&pmo->spinlock, flags);
	pmo->cmdbuf_nodes[input_para->cmdbuf_id] = new_cmdbuf_node;
	gf_debug("%s, process obj %p for file %p, new_cmdbuf_node %p\n",
			__func__, (void *)pmo, (void *)pmo->file, (void *)new_cmdbuf_node);
	spin_unlock_irqrestore(&pmo->spinlock, flags);
#ifdef SUPPORT_DBGFS
	u64 time_num;
#if (KERNEL_VERSION(4, 19, 94) > LINUX_VERSION_CODE)
	struct timeval time_now;

	do_gettimeofday(&time_now);
	time_num = time_now.tv_sec * 1000 + time_now.tv_usec / 1000;
#else
	struct timespec64 time_now;

	ktime_get_real_ts64(&time_now);
	time_num = time_now.tv_sec * 1000 + time_now.tv_nsec / 1000000;
#endif
	reserved_time[input_para->core_id][r_index[input_para->core_id]] = time_num;
	reserved_cmdbufid[input_para->core_id][r_index[input_para->core_id]] = input_para->cmdbuf_id;
	r_index[input_para->core_id] = r_index[input_para->core_id] + 1;
	if (r_index[input_para->core_id] == MAX_RESERVED_TIME)
		r_index[input_para->core_id] = 0;
#endif

	input_para->core_id = cmdbuf_obj->core_id;

	return 0;
}

static long release_cmdbuf_notlinked(vcmd_mgr_t *vcmd_mgr, struct drm_file *file, u16 cmdbuf_id)
{
	struct cmdbuf_obj *cmdbuf_obj = NULL;
	bi_list_node *new_cmdbuf_node = NULL;
	u32 module_type;
	unsigned long flags = 0;
	struct process_manager_obj *pmo = NULL;

	/*get cmdbuf object according to cmdbuf_id*/
	new_cmdbuf_node = vcmd_mgr->global_cmdbuf_node[cmdbuf_id];
	if (!new_cmdbuf_node) {
		//should not happen
		gf_error("hantrovcmd: ERROR cmdbuf_id !!\n");
		return -1;
	}
	cmdbuf_obj = (struct cmdbuf_obj *)new_cmdbuf_node->data;
	if (cmdbuf_obj->file != file) {
		//should not happen
		gf_error("hantrovcmd: ERROR cmdbuf_id !!\n");
		return -1;
	}
	module_type = cmdbuf_obj->module_type;
	cmdbuf_obj->cmdbuf_need_remove = 1;
	do {
		//remove current node
		cmdbuf_obj = (struct cmdbuf_obj *)new_cmdbuf_node->data;
		if (cmdbuf_obj->cmdbuf_need_remove == 1) {
			if (new_cmdbuf_node) {
				//free node
				vcmd_mgr->global_cmdbuf_node[cmdbuf_obj->cmdbuf_id] = NULL;
				free_cmdbuf_node(vcmd_mgr, new_cmdbuf_node);
			}
		}
	} while (0);
	return 0;
}

static void dump_bi_list(bi_list *list)
{
	bi_list_node *new_cmdbuf_node = list->head;
	struct cmdbuf_obj *cmdbuf_obj = NULL;

	while (new_cmdbuf_node) {
			cmdbuf_obj = (struct cmdbuf_obj *)new_cmdbuf_node->data;
			gf_error("%d \n", cmdbuf_obj->cmdbuf_id);
			new_cmdbuf_node = new_cmdbuf_node->next;
	}
}

static long release_cmdbuf(vcmd_mgr_t *vcmd_mgr, struct drm_file *file, u16 cmdbuf_id)
{
	struct cmdbuf_obj *cmdbuf_obj = NULL;
	bi_list_node *last_cmdbuf_node = NULL;
	bi_list_node *new_cmdbuf_node = NULL;
	bi_list *list = NULL;
	u32 module_type;
	struct process_manager_obj *pmo = NULL;
	unsigned long flags = 0;
	unsigned long flags_dev = 0;
	struct hantrovcmd_dev *dev = NULL;

	/*get cmdbuf object according to cmdbuf_id*/
	new_cmdbuf_node = vcmd_mgr->global_cmdbuf_node[cmdbuf_id];
	if (!new_cmdbuf_node) {
		//should not happen
		gf_error("hantrovcmd: ERROR cmdbuf_id !!\n");
		return -1;
	}
	cmdbuf_obj = (struct cmdbuf_obj *)new_cmdbuf_node->data;
	if (cmdbuf_obj->file != file) {
		//should not happen
		gf_error("hantrovcmd: ERROR cmdbuf_id !!\n");
		return -1;
	}
	module_type = cmdbuf_obj->module_type;

	pmo = cmdbuf_obj->process_manager_obj;
	spin_lock_irqsave(&pmo->spinlock, flags);
	pmo->cmdbuf_nodes[cmdbuf_id] = NULL;
	spin_unlock_irqrestore(&pmo->spinlock, flags);
	//TODO
	//if (down_interruptible(&vcmd_mgr->vcmd_reserve_cmdbuf_sem[module_type]))
	//	return -ERESTARTSYS;
	dev = &vcmd_mgr->hantrovcmd_data[cmdbuf_obj->core_id];

	spin_lock_irqsave(dev->spinlock, flags_dev);
	list = &dev->list_manager;
	cmdbuf_obj->cmdbuf_need_remove = 1;
	last_cmdbuf_node = new_cmdbuf_node->previous;
	while (1) {
		//remove current node
		cmdbuf_obj = (struct cmdbuf_obj *)new_cmdbuf_node->data;
		if (cmdbuf_obj->cmdbuf_need_remove == 1) {
			new_cmdbuf_node = remove_cmdbuf_node_from_list(
				list, new_cmdbuf_node);
			if (new_cmdbuf_node) {
				//free node
				vcmd_mgr->global_cmdbuf_node[cmdbuf_obj->cmdbuf_id] = NULL;
				free_cmdbuf_node(vcmd_mgr, new_cmdbuf_node);
			}
		}
		if (!last_cmdbuf_node)
			break;
		new_cmdbuf_node = last_cmdbuf_node;
		last_cmdbuf_node = new_cmdbuf_node->previous;
	}
	spin_unlock_irqrestore(dev->spinlock, flags_dev);
	//up(&vcmd_mgr->vcmd_reserve_cmdbuf_sem[module_type]);
	return 0;
}

static long release_cmdbuf_node(vcmd_mgr_t *vcmd_mgr, bi_list *list, bi_list_node *cmdbuf_node)
{
	bi_list_node *new_cmdbuf_node = NULL;
	struct cmdbuf_obj *cmdbuf_obj = NULL;
	/*get cmdbuf object according to cmdbuf_id*/
	new_cmdbuf_node = cmdbuf_node;
	if (!new_cmdbuf_node)
		return -1;
	//remove node from list
	new_cmdbuf_node = remove_cmdbuf_node_from_list(list, new_cmdbuf_node);
	if (new_cmdbuf_node) {
		//free node
		cmdbuf_obj = (struct cmdbuf_obj *)new_cmdbuf_node->data;
		vcmd_mgr->global_cmdbuf_node[cmdbuf_obj->cmdbuf_id] = NULL;
		free_cmdbuf_node(vcmd_mgr, new_cmdbuf_node);
		return 0;
	}
	return 1;
}

static long release_cmdbuf_node_cleanup(vcmd_mgr_t *vcmd_mgr, bi_list *list)
{
	bi_list_node *new_cmdbuf_node = NULL;
	struct cmdbuf_obj *cmdbuf_obj = NULL;

	while (1) {
		new_cmdbuf_node = list->head;
		if (!new_cmdbuf_node)
			return 0;
		//remove node from list
		bi_list_remove_node(list, new_cmdbuf_node);
		//free node
		cmdbuf_obj = (struct cmdbuf_obj *)new_cmdbuf_node->data;
		vcmd_mgr->global_cmdbuf_node[cmdbuf_obj->cmdbuf_id] = NULL;
		free_cmdbuf_node(vcmd_mgr, new_cmdbuf_node);
	}
	return 0;
}

static bi_list_node *find_last_linked_cmdbuf(bi_list_node *current_node)
{
	bi_list_node *new_cmdbuf_node = current_node;
	bi_list_node *last_cmdbuf_node;
	struct cmdbuf_obj *cmdbuf_obj = NULL;

	if (!current_node)
		return NULL;
	last_cmdbuf_node = new_cmdbuf_node;
	new_cmdbuf_node = new_cmdbuf_node->previous;
	while (1) {
		if (!new_cmdbuf_node)
			return last_cmdbuf_node;
		cmdbuf_obj = (struct cmdbuf_obj *)new_cmdbuf_node->data;
		if (cmdbuf_obj->cmdbuf_data_linked)
			return new_cmdbuf_node;
		last_cmdbuf_node = new_cmdbuf_node;
		new_cmdbuf_node = new_cmdbuf_node->previous;
	}
	return NULL;
}

static void vcmd_fill_indicator_and_end(struct cmdbuf_obj *cmdbuf_obj, struct hantrovcmd_dev *dev, struct drm_file *file, int procid)
{
	struct lisuan_file *gf_file = file->driver_priv;
	struct lisuan_vm *vm = &gf_file->vm;
	struct lisuan_vm_node *vm_node = container_of(vm->root.buffer->va_list.next, struct lisuan_vm_node, node_item);
	int i = 0;
	gf_hw_get_vcmd_fill_indicator_and_end_cmds(&cmdbuf_obj->cmdbuf_virtual_address, cmdbuf_obj->cmdbuf_size, procid, (unsigned int)((vm_node->offset - gf_file->ls_dev->mmu_bypass_start) >> 9),
											   dev->dec_ring->fence_node.start,
											   atomic64_read(&dev->dec_ring->last_submit_fence_id));
    for(i = 0;i < 12; i++)
		*(int*)(cmdbuf_obj->cmdbuf_virtual_address + 19 + i*4) = gf_file->ls_dev->reserve_offset[i];
}


static long link_and_run_cmdbuf(vcmd_mgr_t *vcmd_mgr, struct drm_file *file,
				struct exchange_parameter *input_para, uint32_t is_internal, int procid)
{
	struct cmdbuf_obj *cmdbuf_obj = NULL;
	bi_list_node *new_cmdbuf_node = NULL;
	bi_list_node *last_cmdbuf_node;
	bi_list* list = NULL;
	u32 *jmp_addr = NULL;
	u32 opCode;
	u32 tempOpcode;
	u32 record_last_cmdbuf_rdy_num;
	struct hantrovcmd_dev *dev = NULL;
	unsigned long flags = 0;
	int return_value;
	u16 cmdbuf_id = input_para->cmdbuf_id;
	struct hantrovcmd_dev *hantrovcmd_data = vcmd_mgr->hantrovcmd_data;
	new_cmdbuf_node = vcmd_mgr->global_cmdbuf_node[cmdbuf_id];
	if (!new_cmdbuf_node) {
		//should not happen
		gf_error("hantrovcmd: ERROR cmdbuf_id !!\n");
		return -1;
	}
	cmdbuf_obj = (struct cmdbuf_obj *)new_cmdbuf_node->data;
	if (cmdbuf_obj->file != file) {
		//should not happen
		gf_error("hantrovcmd: ERROR cmdbuf_id !!\n");
		return -1;
	}
	cmdbuf_obj->cmdbuf_data_loaded = 1;
	cmdbuf_obj->cmdbuf_size = input_para->cmdbuf_size;
	cmdbuf_obj->waited = 0;
	//test nop and end opcode, then assign value.
	//0: has jmp opcode,1 has end code
	cmdbuf_obj->has_end_cmdbuf = 0;
	//0: interrupt when JMP,1 not interrupt when JMP
	cmdbuf_obj->no_normal_int_cmdbuf = 0;
	jmp_addr = cmdbuf_obj->cmdbuf_virtual_address +
		   (cmdbuf_obj->cmdbuf_size / 4);
	opCode = tempOpcode = *(jmp_addr - 4);
	opCode >>= 27;
	opCode <<= 27;
	/* we can't identify END opcode or JMP opcode, so we
	 * don't support END opcode in control sw and driver.
	 */
	if (opCode == OPCODE_JMP) {
		//jmp
		opCode = tempOpcode;
		opCode &= 0x02000000;
		if (opCode == JMP_IE_1)
			cmdbuf_obj->no_normal_int_cmdbuf = 0;
		else
			cmdbuf_obj->no_normal_int_cmdbuf = 1;
	} else
		//not support other opcode
		return -1;
/**
	if (down_interruptible(
			&vcmd_mgr->vcmd_reserve_cmdbuf_sem[cmdbuf_obj->module_type])){
		return -ERESTARTSYS;
	}

	if (return_value) {
		up(&vcmd_mgr->vcmd_reserve_cmdbuf_sem[cmdbuf_obj->module_type]);
		return return_value;
	}
**/
	dev = &hantrovcmd_data[cmdbuf_obj->core_id];
	spin_lock_irqsave(dev->spinlock, flags);
	input_para->core_id = cmdbuf_obj->core_id;

	gf_debug("Allocate cmd buffer [%d] to core [%d]\n", cmdbuf_id,
		input_para->core_id);
	dev->cur_cmd_id = cmdbuf_obj->cmdbuf_id;
	//set ddr address for vcmd registers copy.
	if (dev->hw_version_id > HW_ID_1_0_C) {
		if (!is_internal) {
			//read vcmd executing register into ddr memory.
			//now core id is got and output ddr address of
			//vcmd register can be filled in.each core has
			//its own fixed output ddr address of vcmd registers.
			jmp_addr = cmdbuf_obj->cmdbuf_virtual_address + 18;
			if (0) {
				// vsi mmu only support 32bit virtural addr
				*(jmp_addr + 2) = 0;
				*(jmp_addr + 1) =
					(u32)((dev->mmu_vcmd_reg_mem_bus_address +
						(EXECUTING_CMDBUF_ID_ADDR + 1) * 4));
			} else {
				if (sizeof(size_t) == 8) {
					*(jmp_addr + 2) = (u32)(
						(u64)(dev->vcmd_reg_mem_bus_address +
							(EXECUTING_CMDBUF_ID_ADDR + 1) *
								4) >>
						32);
				} else {
					*(jmp_addr + 2) = 0;
				}
				*(jmp_addr + 1) =
					(u32)((dev->vcmd_reg_mem_bus_address +
						(EXECUTING_CMDBUF_ID_ADDR + 1) * 4));
			}

			jmp_addr = cmdbuf_obj->cmdbuf_virtual_address +
				(cmdbuf_obj->cmdbuf_size / 4);
			//read vcmd all registers into ddr memory.
			//now core id is got and output ddr address of
			//vcmd registers can be filled in.
			//each core has its own fixed output ddr address
			// of vcmd registers.
			if (0) {
				if (sizeof(size_t) == 8)
					*(jmp_addr - 6) = 0;
				*(jmp_addr - 7) =
					(u32)(dev->mmu_vcmd_reg_mem_bus_address);
			} else {
				if (sizeof(size_t) == 8)
					*(jmp_addr - 8) = (u32)(
						(u64)dev->vcmd_reg_mem_bus_address >>
						32);
				else
					*(jmp_addr - 6) = 0;
				*(jmp_addr - 9) = (u32)(dev->vcmd_reg_mem_bus_address);
			}
		}
		else {
			//read vcmd executing register into ddr memory.
			//now core id is got and output ddr address of
			//vcmd register can be filled in.each core has
			//its own fixed output ddr address of vcmd registers.
			jmp_addr = cmdbuf_obj->cmdbuf_virtual_address;
			if (0) {
				// vsi mmu only support 32bit virtural addr
				*(jmp_addr + 2) = 0;
				*(jmp_addr + 1) =
					(u32)((dev->mmu_vcmd_reg_mem_bus_address +
						(EXECUTING_CMDBUF_ID_ADDR + 1) * 4));
			} else {
				if (sizeof(size_t) == 8) {
					*(jmp_addr + 2) = (u32)(
						(u64)(dev->vcmd_reg_mem_bus_address +
							(EXECUTING_CMDBUF_ID_ADDR + 1) *
								4) >>
						32);
				} else {
					*(jmp_addr + 2) = 0;
				}
				*(jmp_addr + 1) =
					(u32)((dev->vcmd_reg_mem_bus_address +
						(EXECUTING_CMDBUF_ID_ADDR + 1) * 4));
			}

			jmp_addr = cmdbuf_obj->cmdbuf_virtual_address +
				(cmdbuf_obj->cmdbuf_size / 4);
			//read vcmd all registers into ddr memory.
			//now core id is got and output ddr address of
			//vcmd registers can be filled in.
			//each core has its own fixed output ddr address
			// of vcmd registers.
			if (0) {
				if (sizeof(size_t) == 8)
					*(jmp_addr - 6) = 0;
				*(jmp_addr - 7) =
					(u32)(dev->mmu_vcmd_reg_mem_bus_address);
			} else {
				if (sizeof(size_t) == 8)
					*(jmp_addr - 6) = (u32)(
						(u64)dev->vcmd_reg_mem_bus_address >>
						32);
				else
					*(jmp_addr - 6) = 0;
				*(jmp_addr - 7) = (u32)(dev->vcmd_reg_mem_bus_address);
			}
		}
	}
	if (!is_internal)
		//add indicate header and frame end to command buffer, umd has reserve the allocation
		vcmd_fill_indicator_and_end(cmdbuf_obj, dev, file, procid);
#ifdef VCMD_DEBUG_INTERNAL	
	{
		u32 i, inst = 0, size = 0;

		gf_debug("vcmd link, current cmdbuf content\n");
		for (i = 0; i < cmdbuf_obj->cmdbuf_size / 4; i++) {
			if (i == inst) {
				PrintInstr(i,
						*(cmdbuf_obj->cmdbuf_virtual_address + i),
					&size);
				inst += size;
			} else {
				gf_debug("current cmdbuf data %d = 0x%x\n",
					i,
					*(cmdbuf_obj->cmdbuf_virtual_address + i));
			}
		}
	}
#endif
	//start to link and/or run
	list = &dev->list_manager;
	bi_list_insert_node_tail(list, new_cmdbuf_node);
	last_cmdbuf_node = find_last_linked_cmdbuf(new_cmdbuf_node);
	record_last_cmdbuf_rdy_num = dev->sw_cmdbuf_rdy_num;
	vcmd_link_cmdbuf(dev, last_cmdbuf_node);
#ifdef SUPPORT_DBGFS
	u32 hw_rdy_cmdbuf_num = gf_read_mmio32((hantrovcmd_data[cmdbuf_obj->core_id].hwregs + 3 * 4));

	num_cmdbuf_linked[input_para->core_id] = dev->sw_cmdbuf_rdy_num - hw_rdy_cmdbuf_num;
	char v[5];

	sprintf(v, "%d", cmdbuf_id);
	root_cmdbuf[cmdbuf_id] = debugfs_create_file(v, 0444, debug_root, NULL, &cmdbuf_op);
	u64 time_num;
#if (KERNEL_VERSION(4, 19, 94) > LINUX_VERSION_CODE)
	struct timeval time_now;

	do_gettimeofday(&time_now);
	time_num = time_now.tv_sec * 1000 + time_now.tv_usec / 1000;
#else
	struct timespec64 time_now;

	ktime_get_real_ts64(&time_now);
	time_num = time_now.tv_sec * 1000 + time_now.tv_nsec / 1000000;
#endif
	link_time[input_para->core_id][l_index[input_para->core_id]] = time_num;
	link_cmdbufid[input_para->core_id][l_index[input_para->core_id]] = cmdbuf_id;
	vcmd_workload[input_para->core_id] += cmdbuf_obj->executing_time;
	l_index[input_para->core_id]++;
	if (l_index[input_para->core_id] == MAX_RESERVED_TIME)
		l_index[input_para->core_id] = 0;
#endif
	mb();

	barrier_rw_local(dev->dec_ring->device->barrier_buf->virt_addres, dev->dec_ring->rb_index, dev->dec_ring->device->mhb_num, dev->dec_ring->ring_fence.ring_fence_value);

	if (dev->working_state == WORKING_STATE_IDLE) {
		//run
		while (last_cmdbuf_node &&
			   ((struct cmdbuf_obj *)last_cmdbuf_node->data)
				   ->cmdbuf_run_done)
			last_cmdbuf_node = last_cmdbuf_node->next;

		if (last_cmdbuf_node && last_cmdbuf_node->data) {
			gf_debug("vcmd start for cmdbuf id %d, cmdbuf_run_done = %d\n",
				   ((struct cmdbuf_obj *)last_cmdbuf_node->data)
					   ->cmdbuf_id,
				   ((struct cmdbuf_obj *)last_cmdbuf_node->data)
					   ->cmdbuf_run_done);
		}
		vcmd_start(dev, last_cmdbuf_node);
	} else {
		//just update cmdbuf ready number
		if (record_last_cmdbuf_rdy_num != dev->sw_cmdbuf_rdy_num)
			vcmd_write_register_value((const void *)dev->hwregs,
						  dev->reg_mirror,
						  HWIF_VCMD_RDY_CMDBUF_COUNT,
						  dev->sw_cmdbuf_rdy_num);
	}
	spin_unlock_irqrestore(dev->spinlock, flags);
	//up(&vcmd_mgr->vcmd_reserve_cmdbuf_sem[cmdbuf_obj->module_type]);

	return 0;
}

static int check_cmdbuf_irq(struct hantrovcmd_dev *dev,
				struct cmdbuf_obj *cmdbuf_obj, u32 *irq_status_ret)
{
	int rdy = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(dev->spinlock, flags);
	if (cmdbuf_obj->cmdbuf_run_done || cmdbuf_obj->slice_run_done) {
		rdy = 1;
		//need to decide how to assign this variable
		*irq_status_ret = cmdbuf_obj->executing_status;
	}
	spin_unlock_irqrestore(dev->spinlock, flags);
	return rdy;
}

static int check_mc_cmdbuf_irq(vcmd_mgr_t *vcmd_mgr,
							struct process_manager_obj *pmo, u32 *irq_status_ret)
{
	unsigned long flags = 0, i;
	bi_list_node *new_cmdbuf_node = NULL;
	struct hantrovcmd_dev *dev = NULL;
	struct cmdbuf_obj *cmdbuf_obj = NULL;

	spin_lock_irqsave(&pmo->spinlock, flags);
	for (i = 0; i < TOTAL_DISCRETE_CMDBUF_NUM; i++) {
		new_cmdbuf_node = pmo->cmdbuf_nodes[i];
		if (new_cmdbuf_node == NULL)
			continue;

		cmdbuf_obj = (struct cmdbuf_obj *)new_cmdbuf_node->data;
		if (!cmdbuf_obj)
			continue;

		dev = &vcmd_mgr->hantrovcmd_data[cmdbuf_obj->core_id];
		if (check_cmdbuf_irq(dev, cmdbuf_obj, irq_status_ret) == 1) {
			/* Return cmdbuf_id when ANY_CMDBUF_ID is used. */
			if (!cmdbuf_obj->waited) {
				gf_debug("wait_cmdbuf_ready(%s), new_cmdbuf_node %p\n",
						__func__, (void *)new_cmdbuf_node);
				*irq_status_ret = cmdbuf_obj->cmdbuf_id;
				cmdbuf_obj->waited = 1;
				spin_unlock_irqrestore(&pmo->spinlock, flags);
				return 1;
			}
		}
	}
	spin_unlock_irqrestore(&pmo->spinlock, flags);

	return 0;
}

#ifdef SUPPORT_WATCHDOG
//timer callback function
#if (KERNEL_VERSION(4, 14, 0) > LINUX_VERSION_CODE)
static void vcmd_watchdog(unsigned long arg)
#else
static void vcmd_watchdog(struct timer_list *timer)
#endif
{
	vcmd_mgr_t *vcmd_mgr = watch_dev;
	struct hantrovcmd_dev *dev = NULL;
	u32 tmp_count, tmp_state, dec_mode, i, vcd_status;

	for (i = 0; i < vcmd_mgr->total_vcmd_core_num; i++) {
		if (vcd_virt[i]) {
			dec_mode = gf_read_mmio32((unsigned long *)(vcd_virt[i] + HANTRODEC_DEC_MODE_OFF));
			dec_mode = (dec_mode >> 27) & 0x1F;
			//only enable when dec_mode is av1
			if (dec_mode == 17) {
				dev = &vcmd_mgr->hantrovcmd_data[i];
				if (dev) {
					tmp_count = vcmd_get_register_value((const void *)dev->hwregs, dev->reg_mirror, HWIF_VCMD_EXE_CMDBUF_COUNT);
					tmp_state = vcmd_get_register_value((const void *)dev->hwregs, dev->reg_mirror, HWIF_VCMD_WORK_STATE);
					//ensure vcd in decoding status
					vcd_status = gf_read_mmio32((vcd_virt[i] + HANTRODEC_IRQ_STAT_DEC_OFF));
					//detect if count has changed and vcmd state is stall
					if (tmp_count == vcmd_count[i] && tmp_state == vcmd_state[i] && tmp_state == 2 && (vcd_status & HANTRODEC_DEC_E)) {
						vcd_status |= HANTRODEC_DEC_ABORT;
						gf_write_mmio32((vcd_virt[i] + HANTRODEC_IRQ_STAT_DEC_OFF), vcd_status);
					} else {
						vcmd_count[i] = tmp_count;
						vcmd_state[i] = tmp_state;
					}
				}
			}
		}
	}
	mod_timer(&vcmd_timer, jiffies + 2000);
}
#endif

#ifdef IRQ_SIMULATION
void get_random_bytes(void *buf, int nbytes);
#if 0
void hantrovcmd_trigger_irq_0(struct timer_list *timer)
{
	gf_debug("trigger core 0 irq\n");
	del_timer(timer);
	hantrovcmd_isr(0, (void *)vcmd_mgr);
}

void hantrovcmd_trigger_irq_1(struct timer_list *timer)
{
	gf_debug("trigger core 1 irq\n");
	del_timer(timer);
	hantrovcmd_isr(0, (void *)vcmd_mgr);
}
#else
void hantrovcmd_trigger_irq(vcmd_mgr_t *vcmd_mgr, struct timer_list *timer)
{
	u32 timer_id = 0;
	u32 core_id = 0;
	u32 i;

	for (i = 0; i < 10000; i++) {
		if (timer_reserve[i].timer == timer) {
			timer_id = timer_reserve[i].timer_id;
			core_id = timer_reserve[i].core_id;
			break;
		}
	}
	gf_debug("trigger core 0 irq\n");
	hantrovcmd_isr(core_id, (void *)vcmd_mgr);
	del_timer(timer);
	timer_reserve[timer_id].timer = NULL;
}

#endif
#endif

static unsigned int wait_cmdbuf_ready(vcmd_mgr_t *vcmd_mgr, struct drm_file *file,
					  u16 cmdbuf_id, u32 *irq_status_ret, uint32_t is_internal)
{
	struct cmdbuf_obj *cmdbuf_obj = NULL;
	bi_list_node *new_cmdbuf_node = NULL;
	struct hantrovcmd_dev *dev = NULL;
	struct process_manager_obj *pmo = NULL;

	if (cmdbuf_id != ANY_CMDBUF_ID) {
		gf_debug("%s\n", __func__);
		new_cmdbuf_node = vcmd_mgr->global_cmdbuf_node[cmdbuf_id];
		if (!new_cmdbuf_node) {
			//should not happen
			gf_error("hantrovcmd: ERROR cmdbuf_id !!\n");
			return -1;
		}
		cmdbuf_obj = (struct cmdbuf_obj *)new_cmdbuf_node->data;
		if (cmdbuf_obj->file != file) {
			//should not happen
			gf_error("hantrovcmd: ERROR cmdbuf_id !!\n");
			return -1;
		}
		dev = &vcmd_mgr->hantrovcmd_data[cmdbuf_obj->core_id];
#ifdef IRQ_SIMULATION
		{
			u64 random_num;
			//get_random_bytes(&random_num, sizeof(u32));
			random_num =
				(u32)((u64)100 * cmdbuf_obj->executing_time /
						  (4096 * 2160) +
					  50);
			gf_debug("random_num=%d\n", random_num);
#if 0
			/*init a timer to trigger irq*/
			if (cmdbuf_obj->core_id == 0) {
				//init_timer(&timer0);
				//timer0.function = hantrovcmd_trigger_irq_0;
				timer_setup(&timer0, hantrovcmd_trigger_irq_0, 0);
				//the expires time is 1s
				timer0.expires =  jiffies + random_num * HZ / 10;
				add_timer(&timer0);
			}

			if (cmdbuf_obj->core_id == 1) {
				//init_timer(&timer1);
				//timer1.function = hantrovcmd_trigger_irq_1;
				timer_setup(&timer1, hantrovcmd_trigger_irq_1, 0);
				//the expires time is 1s
				timer1.expires =  jiffies + random_num * HZ / 10;
				add_timer(&timer1);
			}
#else
			{
				u32 i;
				struct timer_list *temp_timer = NULL;

				for (i = 0; i < 10000; i++) {
					if (!timer_reserve[i].timer) {
						timer_reserve[i].timer_id = i;
						timer_reserve[i].core_id =
							cmdbuf_obj->core_id;
						temp_timer =
							timer_reserve[i].timer =
								&timer[i];
						break;
					}
				}
				//if (cmdbuf_obj->core_id==0)
				{
					//init_timer(&timer0);
					//timer0.function =
					//hantrovcmd_trigger_irq_0;
					timer_setup(temp_timer,
							hantrovcmd_trigger_irq, 0);
					//the expires time is 1s
					temp_timer->expires =
						jiffies + random_num * HZ / 10;
					add_timer(temp_timer);
				}
			}
#endif
		}
#endif
		if (wait_event_interruptible(*dev->wait_queue,
						 check_cmdbuf_irq(dev, cmdbuf_obj,
								  irq_status_ret))) {
			gf_debug("vcmd_wait_queue_0 interrupted\n");
			return -ERESTARTSYS;
		}
		return 0;
	}

	pmo = get_process_manager_obj(vcmd_mgr, file);
	if (pmo == NULL) {
		//should not happen
		return -ERESTARTSYS;
	}

	if (check_mc_cmdbuf_irq(vcmd_mgr, pmo, irq_status_ret))
		return 0;
	if (wait_event_interruptible(vcmd_mgr->mc_wait_queue,
								check_mc_cmdbuf_irq(vcmd_mgr, pmo, irq_status_ret))) {
		gf_debug("multicore wait queue interrupted\n");
		return -ERESTARTSYS;
	}

	return 0;
}

static long hantrovcmd_flush_slice_regs(vcmd_mgr_t *vcmd_mgr, struct core_desc *core)
{
	u32 cmdbuf_id = core->id;
	struct hantrovcmd_dev *dev = NULL;
	struct cmdbuf_obj *cmdbuf_obj = NULL;
	bi_list_node *new_cmdbuf_node = NULL;
	unsigned long flags = 0;

	dev = vcmd_mgr->hantrovcmd_data;

	new_cmdbuf_node = vcmd_mgr->global_cmdbuf_node[cmdbuf_id];
	if (!new_cmdbuf_node) {
		gf_error("%s:  error cmdbuf_id =%d!!\n", __func__, cmdbuf_id);
		return -EFAULT;
	}
	cmdbuf_obj = (struct cmdbuf_obj *)new_cmdbuf_node->data;
	if (dev == NULL) {
		return -EFAULT;
	} else {
		u32 ret = copy_from_user(dev->reg_mirror_main, (__u32 __user *)core->regs,
			     dev->vcmd_core_cfg.submodule_vcd_iosize);
		if (ret) {
			gf_debug("copy_from_user failed, returned %d\n", ret);
			return -EFAULT;
		}

		gf_write_mmio32((dev->hwregs_submodule_main + 4*5), dev->reg_mirror_main[5]);
		gf_write_mmio32((dev->hwregs_submodule_main + 4*6), dev->reg_mirror_main[6]);
		gf_write_mmio32((dev->hwregs_submodule_main + 4*8), dev->reg_mirror_main[8]);
		gf_write_mmio32((dev->hwregs_submodule_main + 4*12), dev->reg_mirror_main[12]);
		gf_write_mmio32((dev->hwregs_submodule_main + 4*122), dev->reg_mirror_main[122]);
		gf_write_mmio32((dev->hwregs_submodule_main + 4*168), dev->reg_mirror_main[168]);
		gf_write_mmio32((dev->hwregs_submodule_main + 4*169), dev->reg_mirror_main[169]);
		gf_write_mmio32((dev->hwregs_submodule_main + 4*258), dev->reg_mirror_main[258]);
		gf_write_mmio32((dev->hwregs_submodule_main + 4*259), dev->reg_mirror_main[259]);

		spin_lock_irqsave(&dev->abnormal_lock, flags);
		gf_write_mmio32((dev->hwregs_submodule_main + 0x4),dev->reg_mirror_main[1]);
		cmdbuf_obj->slice_run_done = 0;
		spin_unlock_irqrestore(&dev->abnormal_lock, flags);

		vcmd_write_reg((const void *)dev->hwregs,
			VCMD_REGISTER_EXT_INT_GATE_OFFSET,
			0xfeffffff);
	}
	return 0;
}

int xx_HANTRO_VCMD_IOCH_GET_CMDBUF_PARAMETER(struct drm_device *dev, void *data,
		       struct drm_file *file_priv)
{
	struct lisuan_device *gf_device = dev->dev_private;
	vcmd_mgr_t *vcmd_mgr = (vcmd_mgr_t *)gf_device->vcmd_mgr;
	struct cmdbuf_mem_parameter local_cmdbuf_mem_data;
	struct lisuan_file *ls_file = (struct lisuan_file *)file_priv->driver_priv;
	resource_size_t start = 0;
	struct mmap_args args = {0};
	uint64_t vcmd_buffer_cpuva = 0;

	gf_debug(" GET CMDBUF PARAMETER\n");
	local_cmdbuf_mem_data.cmdbuf_unit_size = CMDBUF_MAX_SIZE;
	local_cmdbuf_mem_data.status_cmdbuf_unit_size = CMDBUF_MAX_SIZE;
	local_cmdbuf_mem_data.cmdbuf_total_size = CMDBUF_POOL_TOTAL_SIZE;
	local_cmdbuf_mem_data.status_cmdbuf_total_size = CMDBUF_POOL_TOTAL_SIZE;
	local_cmdbuf_mem_data.phy_status_cmdbuf_addr = vcmd_mgr->vcmd_status_buf_mem_pool.bus_address  - gf_device->mmu_bypass_start + gf_device->pdev_info.start_addr_vram;
	local_cmdbuf_mem_data.phy_cmdbuf_addr = vcmd_mgr->vcmd_buf_mem_pool.bus_address  - gf_device->mmu_bypass_start + gf_device->pdev_info.start_addr_vram;

	local_cmdbuf_mem_data.mmu_phy_status_cmdbuf_addr = 0;
	local_cmdbuf_mem_data.mmu_phy_cmdbuf_addr = 0;

	local_cmdbuf_mem_data.base_ddr_addr = vcmd_mgr->base_ddr_addr - gf_device->mmu_bypass_start + gf_device->pdev_info.start_addr_vram ;

	//map vcmd buffer
	args.size = gf_device->dec_bufObj->size;
	args.domain = gf_device->dec_bufObj->domain;

	if (gf_device->dec_bufObj->flag.vram) {
		start = gf_device->dec_bufObj->vm_node->offset + gf_device->pdev_info.start_addr_vram - gf_device->mmu_bypass_start;
	} else {
		args.sg_table = gf_device->dec_bufObj->tbo.sg;
		args.snoop = gf_device->dec_bufObj->flag.snoop;
	}

	ls_file->dec_vcmd_map =(void*)&args;
	vcmd_buffer_cpuva = vm_mmap(file_priv->filp, 0, args.size, PROT_READ | PROT_WRITE, MAP_SHARED, start);
	ls_file->dec_vcmd_map = NULL;

	local_cmdbuf_mem_data.virt_cmdbuf_addr = vcmd_buffer_cpuva;
	local_cmdbuf_mem_data.virt_status_cmdbuf_addr = vcmd_buffer_cpuva + CMDBUF_POOL_TOTAL_SIZE;

	gf_memcpy(data,
				&local_cmdbuf_mem_data,
				sizeof(struct cmdbuf_mem_parameter));
	return 0;
}

int xx_HANTRO_VCMD_IOCH_GET_VCMD_PARAMETER(struct drm_device *dev, void *data,
		       struct drm_file *file_priv)
{
	struct config_parameter input_para;
	struct process_manager_obj *pmo = NULL;
	unsigned long flags = 0;
	struct lisuan_device *gf_device = dev->dev_private;
	vcmd_mgr_t *vcmd_mgr = (vcmd_mgr_t *)gf_device->vcmd_mgr;
	gf_debug(" VCMD get vcmd config parameter\n");
	gf_memcpy(&input_para,
				data,
				sizeof(struct config_parameter));

	pmo = get_process_manager_obj(vcmd_mgr, file_priv);
	if (pmo == NULL) {
		//should not happen
		return -1;
	}
	spin_lock_irqsave(&pmo->spinlock, flags);
	pmo->module_type = input_para.module_type;
	spin_unlock_irqrestore(&pmo->spinlock, flags);
	if (vcmd_mgr->vcmd_type_core_num[input_para.module_type]) {
		input_para.submodule_main_addr =
			vcmd_mgr->vcmd_manager[input_para.module_type][0]
				->vcmd_core_cfg.submodule_main_addr;
		input_para.submodule_dec400_addr =
			vcmd_mgr->vcmd_manager[input_para.module_type][0]
				->vcmd_core_cfg.submodule_dec400_addr;
		input_para.submodule_MMU_addr =
			vcmd_mgr->vcmd_manager[input_para.module_type][0]
				->vcmd_core_cfg.submodule_MMU_addr;
		input_para.submodule_MMUWrite_addr =
			vcmd_mgr->vcmd_manager[input_para.module_type][0]
				->vcmd_core_cfg.submodule_MMUWrite_addr;
		input_para.submodule_axife_addr =
			vcmd_mgr->vcmd_manager[input_para.module_type][0]
				->vcmd_core_cfg.submodule_axife_addr;
		input_para.config_status_cmdbuf_id =
			vcmd_mgr->vcmd_manager[input_para.module_type][0]
				->status_cmdbuf_id;
		input_para.vcmd_hw_version_id =
			vcmd_mgr->vcmd_manager[input_para.module_type][0]
				->hw_version_id;
		input_para.vcmd_core_num =
			vcmd_mgr->vcmd_type_core_num[input_para.module_type];
	} else {
		input_para.submodule_main_addr = 0xffff;
		input_para.submodule_dec400_addr = 0xffff;
		input_para.submodule_MMU_addr = 0xffff;
		input_para.submodule_MMUWrite_addr = 0xffff;
		input_para.submodule_axife_addr = 0xffff;
		input_para.config_status_cmdbuf_id = 0;
		input_para.vcmd_core_num = 0;
		input_para.vcmd_hw_version_id = HW_ID_1_0_C;
	}
	gf_memcpy(data, &input_para,
				sizeof(struct config_parameter));
	return 0;
}

int xx_HANTRO_VCMD_IOCH_RESERVE_CMDBUF(struct drm_device *dev, void *data,
		       struct drm_file *file_priv)
{
	int ret;
	struct exchange_parameter input_para;
	struct lisuan_device *gf_device = dev->dev_private;
	vcmd_mgr_t *vcmd_mgr = (vcmd_mgr_t *)gf_device->vcmd_mgr;
	gf_memcpy(&input_para,
				data,
				sizeof(struct exchange_parameter));
	if(input_para.core_mask == 0){
		gf_error("reserve command buffer fail, core_mask = 0\n");
		return -1;
	}
	ret = reserve_cmdbuf(vcmd_mgr, file_priv, &input_para);
	if (ret == 0) {
		gf_memcpy(data,
					&input_para,
					sizeof(struct exchange_parameter));
	}
	gf_debug("VCMD Reserve CMDBUF %d, core id = %d\n", input_para.cmdbuf_id, input_para.core_id);
	return ret;
}

int xx_HANTRO_VCMD_IOCH_LINK_RUN_CMDBUF(struct drm_device *dev, void *data,
		       struct drm_file *file_priv)
{
	struct exchange_parameter input_para;
	long retVal;
	struct lisuan_device *gf_device = dev->dev_private;
	vcmd_mgr_t *vcmd_mgr = (vcmd_mgr_t *)gf_device->vcmd_mgr;
	gf_memcpy(&input_para,
				data,
				sizeof(struct exchange_parameter));

	retVal = link_and_run_cmdbuf(vcmd_mgr, file_priv, &input_para, 0, 0);
	gf_memcpy(data,
				&input_para, sizeof(struct exchange_parameter));

	return retVal;
}

void dma_fence_cb(struct dma_fence* fence,
                  struct dma_fence_cb* cb)
{
	struct cmdbuf_obj *obj = NULL;
	vcmd_mgr_t *vcmd_mgr =  NULL;
	struct hantrovcmd_dev *dev = NULL;
	unsigned long flags = 0;

	if (!fence || !cb)
		return;

	obj = container_of(cb, struct cmdbuf_obj, fence_cb);
	vcmd_mgr = ((struct lisuan_file *)obj->file->driver_priv)->ls_dev->vcmd_mgr;
	dev = &vcmd_mgr->hantrovcmd_data[obj->core_id];
	spin_lock_irqsave(dev->spinlock, flags);
	obj->cmdbuf_run_done = 1;
	dev->last_notify_fence = obj->submit_fence_id;
	spin_unlock_irqrestore(dev->spinlock, flags);
	wake_up_interruptible_all(dev->wait_queue);
	wake_up_interruptible_all(&vcmd_mgr->mc_wait_queue);

	gf_debug("dma_fence_cb: fence_id=%u\n", obj->submit_fence_id);
}

int gf_dec_link_and_run(struct drm_device *dev, void *data, struct drm_file *file_priv, const unsigned long long fence, void* cb_fence, int procid)
{
	long ret;
	const struct lisuan_device *gf_device = dev->dev_private;
	vcmd_mgr_t *vcmd_mgr = gf_device->vcmd_mgr;
	struct exchange_parameter input_para;
	bi_list_node *node = NULL;
	struct cmdbuf_obj *obj = NULL;
	gf_memcpy(&input_para, data, sizeof(struct exchange_parameter));

	node = vcmd_mgr->global_cmdbuf_node[input_para.cmdbuf_id];
	obj = (struct cmdbuf_obj *)node->data;
	obj->submit_fence_id = fence;
	ret = dma_fence_add_callback(cb_fence, &obj->fence_cb, dma_fence_cb);
	gf_assert(ret == 0);

	ret = link_and_run_cmdbuf(vcmd_mgr, file_priv, &input_para, 0, procid);
	if (ret == 0)
		gf_memcpy(data, &input_para, sizeof(struct exchange_parameter));
	gf_debug("dec link_and_run: core= %d, cmd_id=%d, fence=%d, ret=%d\r\n", input_para.core_id, input_para.cmdbuf_id,
	         fence, ret);
	return ret;
}

int xx_HANTRO_VCMD_IOCH_WAIT_CMDBUF(struct drm_device *dev, void *data,
		       struct drm_file *file_priv)
{
	u16 cmdbuf_id;
	unsigned int tmp;
	u32 irq_status_ret = 0;
	struct lisuan_device *gf_device = dev->dev_private;
	vcmd_mgr_t *vcmd_mgr = (vcmd_mgr_t *)gf_device->vcmd_mgr;
	cmdbuf_id = *(u16*)data;
	/*high 16 bits are core id, low 16 bits are cmdbuf_id*/

	gf_debug("VCMD wait for CMDBUF %d finishing.\n", cmdbuf_id);

	//TODO
	tmp = wait_cmdbuf_ready(vcmd_mgr, file_priv, cmdbuf_id, &irq_status_ret, 0);
	cmdbuf_id = (u16)irq_status_ret;
	if (tmp == 0) {
		*(u16*)data = cmdbuf_id;
		return tmp; //return core_id
	}
	*(u16*)data = 0;
	return -1;
}

int xx_HANTRO_VCMD_IOCH_ACQUIRE_MC_TOKEN(struct drm_device *dev, void *data,
		       struct drm_file *file_priv)
{
	u32 got_token = 0;
	unsigned long flags = 0;
	struct lisuan_device *gf_device = dev->dev_private;
	vcmd_mgr_t *vcmd_mgr = gf_device->vcmd_mgr;

	if (!data)
		return -EINVAL;

	spin_lock_irqsave(&vcmd_mgr->mutex, flags);
	if (!vcmd_mgr->mc_token_owner) {
		vcmd_mgr->mc_token_owner = file_priv;
		got_token = 1;
	}
	spin_unlock_irqrestore(&vcmd_mgr->mutex, flags);

	*(u32 *)data = got_token;
	return 0;
}

int xx_HANTRO_VCMD_IOCH_PUSH_SLICE_REG(struct drm_device *dev, void *data,
		       struct drm_file *file_priv)
{
	struct core_desc core;
	struct lisuan_device *gf_device = dev->dev_private;
	vcmd_mgr_t *vcmd_mgr = (vcmd_mgr_t *)gf_device->vcmd_mgr;

	/* get registers from user space*/
	gf_memcpy(&core, data,
					sizeof(struct core_desc));

	return hantrovcmd_flush_slice_regs(vcmd_mgr, &core);
}

int xx_HANTRO_VCMD_IOCH_RELEASE_CMDBUF(struct drm_device *dev, void *data,
		       struct drm_file *file_priv)
{
	u16 cmdbuf_id;
	struct lisuan_device *gf_device = dev->dev_private;
	vcmd_mgr_t *vcmd_mgr = (vcmd_mgr_t *)gf_device->vcmd_mgr;
	cmdbuf_id = *(u16*)data;
	/*16 bits are cmdbuf_id*/

	gf_debug("VCMD release CMDBUF %d\n", cmdbuf_id);

	release_cmdbuf(vcmd_mgr, file_priv, cmdbuf_id);
	return 0;
}
int xx_HANTRO_VCMD_IOCH_POLLING_CMDBUF(struct drm_device *dev, void *data,
		       struct drm_file *file_priv)
{
	u16 core_id;
	struct lisuan_device *gf_device = dev->dev_private;
	vcmd_mgr_t *vcmd_mgr = (vcmd_mgr_t *)gf_device->vcmd_mgr;

	core_id= *(u16*)data;
	/*16 bits are cmdbuf_id*/
	if (core_id >= vcmd_mgr->total_vcmd_core_num)
		return -1;
	lisuan_dec_vcmd_clear_irq_status(core_id, gf_device->vcmd_mgr);
	hantrovcmd_isr(core_id, vcmd_mgr, 0);
	return 0;
}

/*******process manager object management*******/

static struct process_manager_obj *create_process_manager_obj(void)
{
	struct process_manager_obj *process_manager_obj = NULL;

	process_manager_obj = gf_kvcalloc(1, sizeof(*process_manager_obj), GFP_KERNEL);
	if (!process_manager_obj) {
		gf_debug("%s\n", "gf_kvcalloc for process_manager_obj fail!");
		return process_manager_obj;
	}
	gf_memset(process_manager_obj, 0, sizeof(struct process_manager_obj));
	return process_manager_obj;
}

static void
free_process_manager_obj(struct process_manager_obj *process_manager_obj)
{
	if (!process_manager_obj) {
		gf_debug("%s\n", "free_process_manager_obj NULL");
		return;
	}
	//free current cmdbuf_obj
	gf_kvfree(process_manager_obj);
}

static bi_list_node *create_process_manager_node(void)
{
	bi_list_node *current_node = NULL;
	struct process_manager_obj *process_manager_obj = NULL;

	process_manager_obj = create_process_manager_obj();
	if (!process_manager_obj) {
		gf_debug("%s\n", "create_process_manager_obj fail!");
		return NULL;
	}
	process_manager_obj->total_exe_time = 0;
	spin_lock_init(&process_manager_obj->spinlock);
	init_waitqueue_head(&process_manager_obj->wait_queue);
	current_node = bi_list_create_node();
	if (!current_node) {
		gf_debug("%s\n", "bi_list_create_node fail!");
		free_process_manager_obj(process_manager_obj);
		return NULL;
	}
	current_node->data = (void *)process_manager_obj;
	return current_node;
}

static void free_process_manager_node(bi_list_node *process_node)
{
	struct process_manager_obj *process_manager_obj = NULL;

	if (!process_node) {
		gf_debug("%s\n", "free_process_manager_node NULL");
		return;
	}
	process_manager_obj = (struct process_manager_obj *)process_node->data;
	//free struct process_manager_obj
	free_process_manager_obj(process_manager_obj);
	//free current process_manager_obj entity.
	bi_list_free_node(process_node);
}

static long release_process_node_cleanup(bi_list *list)
{
	bi_list_node *new_process_node = NULL;

	while (1) {
		new_process_node = list->head;
		if (!new_process_node)
			break;
		//remove node from list
		bi_list_remove_node(list, new_process_node);
		//remove node from list
		free_process_manager_node(new_process_node);
	}
	return 0;
}

static void create_kernel_process_manager(vcmd_mgr_t *vcmd_mgr)
{
	bi_list_node *process_manager_node;
	struct process_manager_obj *process_manager_obj = NULL;

	process_manager_node = create_process_manager_node();
	process_manager_obj =
		(struct process_manager_obj *)process_manager_node->data;
	process_manager_obj->file = NULL;
	bi_list_insert_node_tail(&vcmd_mgr->global_process_manager, process_manager_node);
}

/* Update the last JMP cmd in cmdbuf_ojb in
 * order to jump to next_cmdbuf_obj.
 */
static void cmdbuf_update_jmp_cmd(int hw_version_id,
				  struct cmdbuf_obj *cmdbuf_obj,
				  struct cmdbuf_obj *next_cmdbuf_obj,
				  int jmp_IE_1, size_t base_ddr_addr)
{
	u32 *jmp_addr;
	u32 operation_code;

	if (!cmdbuf_obj)
		return;

	if (cmdbuf_obj->has_end_cmdbuf == 0) {
		//need to link, current cmdbuf link to next cmdbuf
		jmp_addr = cmdbuf_obj->cmdbuf_virtual_address +
			   (cmdbuf_obj->cmdbuf_size / 4);
		if (!next_cmdbuf_obj) {
			// If next cmdbuf is not available, set the RDY to 0.
			operation_code = *(jmp_addr - 4);
			operation_code >>= 16;
			operation_code <<= 16;
			*(jmp_addr - 4) = (u32)(operation_code & ~JMP_RDY_1);
		} else {
			if (hw_version_id > HW_ID_1_0_C)
				//set next cmdbuf id
				*(jmp_addr - 1) = next_cmdbuf_obj->cmdbuf_id;
			if (0) {
				if (sizeof(size_t) == 8) {
					*(jmp_addr - 2) = (u32)(
						(u64)(next_cmdbuf_obj
								  ->mmu_cmdbuf_bus_address) >>
						32);
				} else {
					*(jmp_addr - 2) = 0;
				}
				*(jmp_addr - 3) = (u32)(
					next_cmdbuf_obj->mmu_cmdbuf_bus_address);
			} else {
				if (sizeof(size_t) == 8) {
					*(jmp_addr - 2) = (u32)(
						(u64)(next_cmdbuf_obj
								  ->cmdbuf_bus_address -
							  base_ddr_addr) >>
						32);
				} else
					*(jmp_addr - 2) = 0;

				*(jmp_addr - 3) = (u32)(
					next_cmdbuf_obj->cmdbuf_bus_address -
					base_ddr_addr);
			}
			operation_code = *(jmp_addr - 4);
			operation_code >>= 16;
			operation_code <<= 16;
			*(jmp_addr - 4) =
				(u32)(operation_code | JMP_RDY_1 | (jmp_IE_1?JMP_IE_1:0) |
					  ((next_cmdbuf_obj->cmdbuf_size + 7) / 8));
		}

#ifdef VCMD_DEBUG_INTERNAL
		{
			u32 i;

			gf_debug("vcmd link, last cmdbuf content\n");
			for (i = cmdbuf_obj->cmdbuf_size / 4 - 8;
				 i < cmdbuf_obj->cmdbuf_size / 4; i++) {
				gf_debug("current linked cmdbuf data %d =0x%x\n",
					i,
					   *(cmdbuf_obj->cmdbuf_virtual_address +
					 i));
			}
		}
#endif
	}
}

/* delink given cmd buffer (cmdbuf_node) and remove it from list.
 *	 Also modify the last JMP of buf P to point to cmdbuf N.
 *	 Used when a process is terminated but there are pending cmd
 *	 bufs in vmcd list.
 *	 E.g.,
 *	 before:

 *		 L->L->...->P->X->N->        ...        ->L
 *		 ^             ^                          ^
 *		 head          cmdbuf_node                 tail

 *	 end:

 *		 L->L->...->P->N->        ...        ->L
 *		 ^                                     ^
 *		 head                                  tail

 *	 Return: pointer to N or NULL if N doesn't exist.
 */
static void vcmd_delink_rm_cmdbuf(vcmd_mgr_t *vcmd_mgr, struct hantrovcmd_dev *dev,
				  bi_list_node *cmdbuf_node)
{
	bi_list *list = &dev->list_manager;
	struct cmdbuf_obj *cmdbuf_obj = (struct cmdbuf_obj *)cmdbuf_node->data;
	bi_list_node *prev = cmdbuf_node->previous;
	bi_list_node *next = cmdbuf_node->next;

	gf_debug("Delink and remove cmdbuf [%d] from vcmd list.\n",
		   cmdbuf_obj->cmdbuf_id);
#ifdef HANTRO_VCMD_DRIVER_DEBUG
	if (prev)
		gf_debug("prev cmdbuf [%d].\n",
			   ((struct cmdbuf_obj *)prev->data)->cmdbuf_id);
	else
		gf_debug("NO prev cmdbuf.\n");
	if (next)
		gf_debug("next cmdbuf [%d].\n",
			   ((struct cmdbuf_obj *)next->data)->cmdbuf_id);
	else
		gf_debug("NO next cmdbuf.\n");
#endif

	bi_list_remove_node(list, cmdbuf_node);
	vcmd_mgr->global_cmdbuf_node[cmdbuf_obj->cmdbuf_id] = NULL;
	free_cmdbuf_node(vcmd_mgr, cmdbuf_node);

	cmdbuf_update_jmp_cmd(dev->hw_version_id, prev ? prev->data : NULL,
					next ? next->data : NULL,
					dev->duration_without_int > INT_MIN_SUM_OF_IMAGE_SIZE,
					vcmd_mgr->base_ddr_addr);
}

int hantrodec_vcmd_open(struct lisuan_device *gf_dev, struct drm_file *file)
{
	int result = 0;
	vcmd_mgr_t *vcmd_mgr = (vcmd_mgr_t *)gf_dev->vcmd_mgr;
	bi_list_node *process_manager_node;
	unsigned long flags = 0;
	struct process_manager_obj *process_manager_obj = NULL;

	if(!vcmd_mgr) {
		gf_error("decoder vcmd_mgr is NULL\n");
		return 0;
	}

#ifdef SUPPORT_DBGFS
	int i, k;
	/* for debugfs */
	u32 hw_rdy_cmdbuf_num;

	for (i = 0; i < vcmd_mgr->total_vcmd_core_num; i++) {
		cycle_index[i] = 0;
		e_index[i] = 0;
		r_index[i] = 0;
		l_index[i] = 0;
		i_index[i] = 0;
		num_cmdbuf_linked[i] = 0;
		vcmd_workload[i] = 0;
		active_hw_index[i] = 0;
		hw_rdy_cmdbuf_num = gf_read_mmio32((hantrovcmd_data_debugfs[i].hwregs + 3 * 4));
		active_state[i] = 0;
		for (k = 0; k < MAX_RESERVED_TIME; k++) {
			decode_cycles[i][k] = 0;
			num_cmdbuf_exe[i][k] = 0;
			num_cmdbuf_reserved[i][k] = 0;
			reserved_time[i][k] = 0;
			reserved_cmdbufid[i][k] = 0;
			link_time[i][k] = 0;
			link_cmdbufid[i][k] = 0;
			interrupt_time[i][k] = 0;
			active_start_time[i][k] = 0;
			active_return_time[i][k] = 0;
			cmdbuf_num_done[i][k] = 0;
			prev_cmdbuf_done[i][k] = 0;
		}
		prev_cmdbuf_done[i][0] = hw_rdy_cmdbuf_num;
	}
#endif
	process_manager_node = create_process_manager_node();
	if (!process_manager_node)
		return -1;
	process_manager_obj =
		(struct process_manager_obj *)process_manager_node->data;
	process_manager_obj->file = file;
	spin_lock_irqsave(&vcmd_mgr->vcmd_process_manager_lock, flags);
	bi_list_insert_node_tail(&vcmd_mgr->global_process_manager, process_manager_node);
	spin_unlock_irqrestore(&vcmd_mgr->vcmd_process_manager_lock, flags);

	gf_debug("dev opened\n");
	gf_debug("process node %p for file opened %p\n",
		   (void *)process_manager_node, (void *)file);
	return result;
}

int hantrodec_vcmd_release(struct lisuan_device *gf_dev, struct drm_file *file)
{
	vcmd_mgr_t *vcmd_mgr = (vcmd_mgr_t *)gf_dev->vcmd_mgr;
	struct hantrovcmd_dev *dev;
	u32 module_type;
	u32 core_id = 0;
	u32 release_cmdbuf_num = 0;
	bi_list_node *new_cmdbuf_node = NULL;
	bi_list_node *new_cmdbuf_node_next = NULL;
	struct cmdbuf_obj *cmdbuf_obj_temp = NULL;
	bi_list_node *process_manager_node;
	struct process_manager_obj *process_manager_obj = NULL;
	int vcmd_aborted = 0; // vcmd is aborted in this function
	struct cmdbuf_obj *restart_cmdbuf = NULL;
	int cmdbuf_used_pos = 2;
	int ret;
	struct process_manager_obj *pmo = NULL;

	unsigned long flags = 0;
	long retVal = 0;

	if (!vcmd_mgr) {
		gf_error("decoder vcmd_mgr is NULL\n");
		return 0;
	}

	spin_lock_irqsave(&vcmd_mgr->mutex, flags);
	if (vcmd_mgr->mc_token_owner == file)
		vcmd_mgr->mc_token_owner = NULL;
	spin_unlock_irqrestore(&vcmd_mgr->mutex, flags);

	dev = vcmd_mgr->hantrovcmd_data;
	module_type	= dev->vcmd_core_cfg.sub_module_type;

	gf_debug("dev closed for process %s via %p\n", __func__,
		   (void *)file);
	pmo = get_process_manager_obj(vcmd_mgr, file);
	if (pmo == NULL) {
		//should not happen
		return -1;
	}
	if (pmo->module_type)
		module_type = pmo->module_type;
	//if (down_interruptible(&vcmd_mgr->vcmd_reserve_cmdbuf_sem[module_type]))
	//	return -ERESTARTSYS;

	for (core_id = 0; core_id < vcmd_mgr->total_vcmd_core_num; core_id++) {
		spin_lock_irqsave(dev[core_id].spinlock, flags);
		new_cmdbuf_node = dev[core_id].list_manager.head;
		while (1) {
			if (!new_cmdbuf_node)
				break;
			cmdbuf_obj_temp = (struct cmdbuf_obj *)
						  new_cmdbuf_node->data;
			gf_debug("Process %p is releasing: checking cmdbuf %d of process %p.\n",
				   file, cmdbuf_obj_temp->cmdbuf_id,
				   cmdbuf_obj_temp->file);
			new_cmdbuf_node_next = new_cmdbuf_node->next;
			if (dev[core_id].hwregs &&
				(cmdbuf_obj_temp->file == file)) {
				if (cmdbuf_obj_temp->cmdbuf_run_done) {
					cmdbuf_obj_temp->cmdbuf_need_remove = 1;
					retVal = release_cmdbuf_node(vcmd_mgr, &dev[core_id].list_manager, new_cmdbuf_node);
					if (retVal == 1)
						cmdbuf_obj_temp->process_manager_obj = NULL;
				} else if (cmdbuf_obj_temp->cmdbuf_data_linked == 0) {
					cmdbuf_obj_temp->cmdbuf_data_linked = 1;
					cmdbuf_obj_temp->cmdbuf_run_done = 1;
					cmdbuf_obj_temp->cmdbuf_need_remove = 1;
					retVal = release_cmdbuf_node(vcmd_mgr, &dev[core_id].list_manager,
									 new_cmdbuf_node);
					if (retVal == 1)
						cmdbuf_obj_temp->process_manager_obj = NULL;
				} else if (cmdbuf_obj_temp->cmdbuf_data_linked ==
						   1 &&
						   dev[core_id].working_state ==
						   WORKING_STATE_IDLE) {
					vcmd_delink_rm_cmdbuf(vcmd_mgr, &dev[core_id],
								  new_cmdbuf_node);
					if (restart_cmdbuf == cmdbuf_obj_temp)
						restart_cmdbuf = new_cmdbuf_node->next ?
							new_cmdbuf_node->next->data : NULL;
					if (restart_cmdbuf) {
						gf_debug("Set restart cmdbuf [%d].\n",
							   restart_cmdbuf->cmdbuf_id);
					}
					else {
						gf_debug("Set restart cmdbuf to NULL.\n");
					}

				} else if (cmdbuf_obj_temp->cmdbuf_data_linked == 1 &&
						   dev[core_id].working_state ==
						   WORKING_STATE_WORKING) {
					bi_list_node *last_cmdbuf_node =
						NULL;
					bi_list_node *done_cmdbuf_node =
						NULL;
					int abort_cmdbuf_id;
					int loop_count = 0;

					//abort the vcmd and wait
					gf_debug("Abort due to linked cmdbuf %d of current process.\n",
						   cmdbuf_obj_temp->cmdbuf_id);
					printk_vcmd_register_debug((const void *)dev[core_id].hwregs,
								   "Before trigger to 0");
					// disable abort interrupt
					//vcmd_write_register_value((const void *)dev[core_id].hwregs,
					//dev[core_id].reg_mirror,HWIF_VCMD_IRQ_ABORT_EN,0);
					vcmd_write_register_value((const void *)dev[core_id].hwregs,
								  dev[core_id].reg_mirror,
						HWIF_VCMD_START_TRIGGER,
						0);
					vcmd_aborted = 1;

					printk_vcmd_register_debug((const void *)dev[core_id].hwregs,
								   "After trigger to 0");
					// Wait vcmd core aborted and vcmd enters IDLE mode.
					//while (dev[core_id].working_state != WORKING_STATE_IDLE) {
					while (vcmd_get_register_value((const void *)dev[core_id].hwregs,
									   dev[core_id].reg_mirror,
						HWIF_VCMD_WORK_STATE)) {
						loop_count++;
						if (!(loop_count % 10)) {
							u32 irq_status = vcmd_read_reg((const void *)dev[core_id].hwregs,
								VCMD_REGISTER_INT_STATUS_OFFSET);

							gf_error("hantrovcmd: expected idle state, but irq status = 0x%0x\n",
								   irq_status);
							gf_error("hantrovcmd: vcmd current status is %d\n",
								   vcmd_get_register_value((const void *)dev[core_id].hwregs,
											   dev[core_id].reg_mirror,
									   HWIF_VCMD_WORK_STATE));
						}
						mdelay(10); // wait 10ms
						 // too long
						// Expand the loop_count to ensure hardware idle and update sw working status to avoid precess hang next time
						if (loop_count > 300) {
							process_manager_node = vcmd_mgr->global_process_manager.head;
							while (1) {
								if (!process_manager_node)
									break;
								process_manager_obj = (struct process_manager_obj *)process_manager_node->data;
								if (process_manager_obj->file == file)
									break;
								process_manager_node = process_manager_node->next;
							}
							gf_error("hantrovcmd: too long before vcmd core to IDLE state\n");
							bi_list_remove_node(&vcmd_mgr->global_process_manager, process_manager_node);
							spin_unlock_irqrestore(dev[core_id].spinlock, flags);
							free_process_manager_node(process_manager_node);
							//up(&vcmd_mgr->vcmd_reserve_cmdbuf_sem[module_type]);
							dev[core_id].working_state = WORKING_STATE_IDLE;
							return -ERESTARTSYS;
						}
					}
					dev[core_id].working_state =
						WORKING_STATE_IDLE;
					// clear interrupt & restore abort_e
					if (vcmd_get_register_value((const void *)dev[core_id].hwregs,
									dev[core_id].reg_mirror,
							HWIF_VCMD_IRQ_ABORT)) {
						gf_debug("Abort interrupt triggered, now clear it to avoid abort int...\n");
						vcmd_write_reg((const void *)dev[core_id].hwregs,
								   VCMD_REGISTER_INT_STATUS_OFFSET,
							0x1 << 4);
						gf_debug("Now irq status = 0x%0x.\n",
							   vcmd_read_reg((const void *)dev[core_id].hwregs,
									 VCMD_REGISTER_INT_STATUS_OFFSET));
					}
					//printk_vcmd_register_debug((const void *)dev->hwregs, "vcmd status to IDLE");

					abort_cmdbuf_id = vcmd_get_register_value(
						(const void *)dev[core_id].hwregs,
						dev[core_id].reg_mirror,
						HWIF_VCMD_CMDBUF_EXECUTING_ID);
					gf_debug("Abort when executing cmd buf %d.\n",
						   abort_cmdbuf_id);
					dev[core_id].sw_cmdbuf_rdy_num = 0;
					dev[core_id].duration_without_int = 0;
					vcmd_write_register_value((const void *)dev[core_id].hwregs,
								  dev[core_id].reg_mirror,
						HWIF_VCMD_EXE_CMDBUF_COUNT,
						0);
					vcmd_write_register_value((const void *)dev[core_id].hwregs,
								  dev[core_id].reg_mirror,
						HWIF_VCMD_RDY_CMDBUF_COUNT,
						0);

					/* Mark cmdbuf_run_done to 1 for all
					 * the cmd buf executed.
					 */
					done_cmdbuf_node =
						dev[core_id].list_manager.head;
					while (done_cmdbuf_node) {
						if (!((struct cmdbuf_obj *)done_cmdbuf_node->data)->cmdbuf_run_done) {
							((struct cmdbuf_obj *)done_cmdbuf_node->data)->cmdbuf_run_done = 1;
							((struct cmdbuf_obj *)done_cmdbuf_node->data)->cmdbuf_data_linked = 0;
							gf_debug("Set cmdbuf [%d] cmdbuf_run_done to 1.\n",
								   ((struct cmdbuf_obj *)done_cmdbuf_node->data)->cmdbuf_id);
						}
						if (((struct cmdbuf_obj *)done_cmdbuf_node->data)->cmdbuf_id == abort_cmdbuf_id)
							break;
						done_cmdbuf_node = done_cmdbuf_node->next;
					}
					if (cmdbuf_obj_temp->cmdbuf_run_done) {
						/* current cmdbuf is in fact has been executed,
						 * but due to interrupt is not triggered
						 * the status is not updated.
						 * Just delink and remove it from the list.
						 */
						if (done_cmdbuf_node &&
							done_cmdbuf_node->data) {
							gf_debug("done_cmdbuf_node is cmdbuf [%d].\n",
								   ((struct cmdbuf_obj *)done_cmdbuf_node->data)->cmdbuf_id);
						}
						if (done_cmdbuf_node) {
							done_cmdbuf_node = done_cmdbuf_node->next;
						}
						if (done_cmdbuf_node) {
							restart_cmdbuf = (struct cmdbuf_obj *)done_cmdbuf_node->data;
						}
						if (restart_cmdbuf) {
							gf_debug("Set restart cmdbuf [%d] via if.\n",
								   restart_cmdbuf->cmdbuf_id);
						}
					} else {
						last_cmdbuf_node = new_cmdbuf_node;
						/* cmd buf num from aborted cmd buf to current cmdbuf_obj_temp */
						if (cmdbuf_obj_temp->cmdbuf_id !=
							abort_cmdbuf_id) {
							last_cmdbuf_node = new_cmdbuf_node->previous;
							while (last_cmdbuf_node &&
								   ((struct cmdbuf_obj *)last_cmdbuf_node->data)->cmdbuf_id !=
								   abort_cmdbuf_id) {
								restart_cmdbuf =
									(struct cmdbuf_obj *)last_cmdbuf_node->data;
								last_cmdbuf_node =
									last_cmdbuf_node->previous;
								dev[core_id].sw_cmdbuf_rdy_num++;
								dev[core_id].duration_without_int +=
									restart_cmdbuf->executing_time;
								gf_debug("Keep valid cmdbuf [%d] in the list.\n",
									   restart_cmdbuf->cmdbuf_id);
							}
						}
						if (restart_cmdbuf) {
							gf_debug("Set restart cmdbuf [%d] via else.\n",
								   restart_cmdbuf->cmdbuf_id);
						}
					}

					//remove first linked cmdbuf from list
					vcmd_delink_rm_cmdbuf(vcmd_mgr, &dev[core_id],
								  new_cmdbuf_node);
				}
				release_cmdbuf_num++;
				gf_debug("release reserved cmdbuf\n");
			} else if (vcmd_aborted &&
				   !cmdbuf_obj_temp->cmdbuf_run_done) {
				/* VCMD is aborted, need to re-calculate
				 * the duration_without_int
				 */
				if (!restart_cmdbuf)
					/* first cmdbuf to be restarted */
					restart_cmdbuf =
					  cmdbuf_obj_temp;
				dev[core_id].duration_without_int +=
					cmdbuf_obj_temp->executing_time;
				dev[core_id].sw_cmdbuf_rdy_num++;
			}
			new_cmdbuf_node = new_cmdbuf_node_next;
		}

		if (restart_cmdbuf) {
			u32 irq_status1, irq_status2;

			gf_debug("Restart from cmdbuf [%d] after aborting.\n",
				   restart_cmdbuf->cmdbuf_id);

			irq_status1 = vcmd_read_reg(
				(const void *)dev[core_id].hwregs,
				VCMD_REGISTER_INT_STATUS_OFFSET);
			vcmd_write_reg((const void *)dev[core_id].hwregs,
					   VCMD_REGISTER_INT_STATUS_OFFSET,
					   irq_status1);
			irq_status2 = vcmd_read_reg((const void *)dev[core_id].hwregs,
							VCMD_REGISTER_INT_STATUS_OFFSET);
			gf_debug("Clear irq status from 0x%0x -> 0x%0x\n",
				   irq_status1, irq_status2);
			if (0) {
				vcmd_write_register_value((const void *)dev[core_id].hwregs,
							  dev[core_id].reg_mirror,
					HWIF_VCMD_EXECUTING_CMD_ADDR,
					(u32)(restart_cmdbuf->mmu_cmdbuf_bus_address));
				if (sizeof(size_t) == 8)
					vcmd_write_register_value(
						(const void *)dev[core_id].hwregs,
						dev[core_id].reg_mirror,
						HWIF_VCMD_EXECUTING_CMD_ADDR_MSB,
						(u32)((u64)(restart_cmdbuf->mmu_cmdbuf_bus_address) >> 32));
				else
					vcmd_write_register_value(
						(const void *)dev[core_id].hwregs,
						dev[core_id].reg_mirror,
						HWIF_VCMD_EXECUTING_CMD_ADDR_MSB,
						0);
			} else {
				vcmd_write_register_value(
					(const void *)dev[core_id].hwregs,
					dev[core_id].reg_mirror,
					HWIF_VCMD_EXECUTING_CMD_ADDR,
					(u32)(restart_cmdbuf->cmdbuf_bus_address -
						  vcmd_mgr->base_ddr_addr));
				if (sizeof(size_t) == 8)
					vcmd_write_register_value((const void *)dev[core_id].hwregs,
								  dev[core_id].reg_mirror,
						HWIF_VCMD_EXECUTING_CMD_ADDR_MSB,
						(u32)((u64)(restart_cmdbuf->cmdbuf_bus_address -
								vcmd_mgr->base_ddr_addr) >> 32));
				else
					vcmd_write_register_value((const void *)dev[core_id].hwregs,
								  dev[core_id].reg_mirror,
						HWIF_VCMD_EXECUTING_CMD_ADDR_MSB,
						0);
			}
			vcmd_write_register_value(
				(const void *)dev[core_id].hwregs,
				dev[core_id].reg_mirror,
				HWIF_VCMD_EXE_CMDBUF_COUNT, 0);
			vcmd_write_register_value(
				(const void *)dev[core_id].hwregs,
				dev[core_id].reg_mirror,
				HWIF_VCMD_EXE_CMDBUF_LENGTH,
				(u32)((restart_cmdbuf->cmdbuf_size + 7) /
					  8));
			vcmd_write_register_value(
				(const void *)dev[core_id].hwregs,
				dev[core_id].reg_mirror,
				HWIF_VCMD_CMDBUF_EXECUTING_ID,
				restart_cmdbuf->cmdbuf_id);
			gf_debug("====dev->sw_cmdbuf_rdy_num is %d\n",
				   dev[core_id].sw_cmdbuf_rdy_num);
			vcmd_write_register_value(
				(const void *)dev[core_id].hwregs,
				dev[core_id].reg_mirror,
				HWIF_VCMD_RDY_CMDBUF_COUNT,
				dev[core_id].sw_cmdbuf_rdy_num);
			printk_vcmd_register_debug(
				(const void *)dev[core_id].hwregs,
				"before restart");
			vcmd_write_register_value(
				(const void *)dev[core_id].hwregs,
				dev[core_id].reg_mirror,
				HWIF_VCMD_START_TRIGGER, 1);

			gf_debug("Restart from cmdbuf [%d] after aborting: start trigger = %d.\n",
				   restart_cmdbuf->cmdbuf_id,
				   vcmd_get_register_value(
					   (const void *)dev[core_id].hwregs,
					   dev[core_id].reg_mirror,
					   HWIF_VCMD_START_TRIGGER));
			gf_debug("dev state from %d -> WORKING.\n",
				   dev[core_id].working_state);
			dev[core_id].working_state =
				WORKING_STATE_WORKING;
			printk_vcmd_register_debug(
				(const void *)dev[core_id].hwregs,
				"after restart");
		} else {
			gf_debug("No more command buffer to be restarted!\n");
		}
		spin_unlock_irqrestore(dev[core_id].spinlock, flags);
		// VCMD aborted but not restarted, nedd to wake up
		if (vcmd_aborted && !restart_cmdbuf)
			wake_up_interruptible_all(
				dev[core_id].wait_queue);
	}
	if (release_cmdbuf_num)
		wake_up_interruptible_all(&vcmd_mgr->vcmd_cmdbuf_memory_wait);
	spin_lock_irqsave(&vcmd_mgr->vcmd_process_manager_lock, flags);
	process_manager_node = vcmd_mgr->global_process_manager.head;
	while (1) {
		if (!process_manager_node)
			break;
		process_manager_obj = (struct process_manager_obj *)process_manager_node->data;
		if (process_manager_obj->file == file)
			break;
		process_manager_node = process_manager_node->next;
	}
	// remove cmdbuf reserved but not linked
	while (1) {
		if (vcmd_mgr->cmdbuf_used[cmdbuf_used_pos]) {
			cmdbuf_obj_temp = (struct cmdbuf_obj *)vcmd_mgr->global_cmdbuf_node[cmdbuf_used_pos]->data;
			if (cmdbuf_obj_temp->file == file && cmdbuf_obj_temp->cmdbuf_data_linked == 0 && cmdbuf_obj_temp->cmdbuf_run_done == 0) {
				cmdbuf_obj_temp = (struct cmdbuf_obj *)vcmd_mgr->global_cmdbuf_node[cmdbuf_used_pos]->data;
				cmdbuf_obj_temp->cmdbuf_data_linked = 1;
				cmdbuf_obj_temp->cmdbuf_run_done = 1;
				cmdbuf_obj_temp->cmdbuf_need_remove = 1;
				ret = release_cmdbuf_notlinked(vcmd_mgr, file, cmdbuf_used_pos);
			if (ret != 0)
				cmdbuf_obj_temp->process_manager_obj = NULL;
			}
		}
		cmdbuf_used_pos++;
		if (cmdbuf_used_pos >= TOTAL_DISCRETE_CMDBUF_NUM)
			break;
	}
	//remove node from list
	gf_debug("process node %p for file to be removed: %p\n",
		   (void *)process_manager_node, (void *)process_manager_obj->file);
	bi_list_remove_node(&vcmd_mgr->global_process_manager, process_manager_node);
	spin_unlock_irqrestore(&vcmd_mgr->vcmd_process_manager_lock, flags);
	free_process_manager_node(process_manager_node);
	//up(&vcmd_mgr->vcmd_reserve_cmdbuf_sem[module_type]);
	return 0;
}

int reduce_xor(unsigned int x) {
	int ret = 0;
	int i = 0;
    for(i = 0;i < 32; i++)
        {
            if((x&0x1) == 1)
                ret++;
            x = x>>1;
        }
    return ret % 2;
}

void get_addr_2k(unsigned int base_addr, uint64_t* offset_arr)
{
	unsigned int addr_2k = 0;
	int ptn = 0;
	int ch = 0;
	unsigned int bits_value = 0;
	int core_id = 0;
	int req_id = 0;
	int ch_num = 6; //can be 6/3/4/1
	unsigned int ch_mask = 0;
	unsigned int set_mask_0 = 0;
	unsigned int set_mask_1 = 0;
	unsigned int set_mask_10 = 0;
	int offset_index = 0;
	int aim_ptn = 0;
	int aim_ch = 0;
	int i = 0;
	ch_mask = (ch_num == 6 || ch_num == 3) ? 0x550204 : 0x010204;
	set_mask_0 = (ch_num == 6 || ch_num == 3) ? 0x200802 : 0x10;
	set_mask_1 = (ch_num == 6 || ch_num == 3) ? 0x401004 : 0x20;
	set_mask_10 = (ch_num == 6 || ch_num == 3) ? 0xaaaa : 0xaaaa;

	for(aim_ptn = 0;aim_ptn < 6;aim_ptn++)
	{
		for(aim_ch = 0;aim_ch < 2;aim_ch++)
		{
			for(i = 0;i < 0x100; i = i+1)  //traverse 0x100 * addr_2k
			{
				addr_2k = base_addr + i; //base_addr is start addr_2k
				ptn = ch_num == 6 ? GET_BITS(addr_2k, 1, 7) % 6 :  ch_num == 3 ?  GET_BITS(addr_2k, 1, 7) % 3 : 3;ch_num == 4 ? (reduce_xor(GET_BITS(addr_2k, 0, 7) & GET_BITS(ch_mask, 0, 7))<1) & (reduce_xor(GET_BITS(addr_2k, 0, 7) & GET_BITS(ch_mask, 8, 15))) : 0;
				ch = reduce_xor(GET_BITS(addr_2k, 0, 7) & GET_BITS(ch_mask, 16, 23));
				bits_value = ch_num == 6 ? GET_BITS(addr_2k, 8, 31) * 22 + (GET_BITS(addr_2k, 1, 7) / 6) : ch_num == 3 ? GET_BITS(addr_2k, 8, 31) * 43 + (GET_BITS(addr_2k, 1, 7) / 3) : addr_2k;
				core_id = (reduce_xor(bits_value & set_mask_0)) ^ (reduce_xor(addr_2k & set_mask_10));
				req_id = (reduce_xor(bits_value & set_mask_1));
				if(core_id == 0 && req_id == 0 && aim_ptn == ptn && aim_ch == ch)
				{
					offset_arr[offset_index] = (addr_2k << 8);
					offset_index++;
					break;
				}
			}
		}
	}
	return;
}

/*-------------------------------------------
 * Function name   : vcmd_pcie_init
 * Description     : Initialize PCI Hw access

 * Return type     : int
 *-------------------------------------------
 */
static int vcmd_init(struct lisuan_device *gf_device, vcmd_mgr_t *vcmd_mgr)
{
	/* PCI base register address (memalloc) */
	unsigned long long g_vcmd_base_ddr_hw, reserve_gpu_va;
	/* Base register address Length */
	u32 g_vcmd_base_len;
	struct lisuan_vm_node *vm_node;

	vcmd_mgr->mmu_enable = false;
	gf_debug("hantrovcmd: MMU %s.\n", vcmd_mgr->mmu_enable ? "ENABLE" : "DISABLE");

	g_vcmd_base_len = 9*1024*1024;
	gf_device->dec_bufObj = gf_kvcalloc(1, sizeof(struct lisuan_gem_bo), GFP_KERNEL);
	gf_device->dec_bufObj->size = g_vcmd_base_len;
	gf_device->dec_bufObj->align = 64*1024;
	gf_device->dec_bufObj->domain = dom_visual_vram;
	gf_device->dec_bufObj->flag.byPass = 1;
	atomic_set(&gf_device->dec_bufObj->unpagable, 1);
	gf_device->dec_bufObj->priority = gem_priority_high;
	gf_device->dec_bufObj->flag.vram = (gf_device->dec_bufObj->domain <= dom_invisual_vram) ? 1 : 0;
	gf_device->dec_bufObj->flag.snoop = ((gf_device->dec_bufObj->domain == dom_snoop_system_pages) ? 1 : 0);

	lisuan_gem_alloc_oneshot(gf_device, NULL, gf_device->dec_bufObj, 1);
	vm_node = container_of(gf_device->dec_bufObj->va_list.next, struct lisuan_vm_node, node_item);

	g_vcmd_base_ddr_hw = vm_node->offset;
		//pci_resource_start(g_vcmd_dev, PCI_DDR_BAR) + ddr_offset;
	if (g_vcmd_base_ddr_hw == 0) {
		gf_debug("PcieInit: Base Address not set.\n");
		return -1;
	}
	vcmd_mgr->base_ddr_addr = 0;//g_vcmd_base_ddr_hw;// - ddr_offset;

	gf_debug("gpu va 0x%llx, gpu pa = %llx\n", g_vcmd_base_ddr_hw, g_vcmd_base_ddr_hw - gf_device->mmu_bypass_start);
	gf_debug("Base memory len 0x%x\n", g_vcmd_base_len);

	gf_debug("cpu pa = 0x%llx, bypass start = 0x%llx, mem bar = 0x%llx\n", 
		g_vcmd_base_ddr_hw - gf_device->mmu_bypass_start + gf_device->pdev_info.start_addr_vram,
		gf_device->mmu_bypass_start, 
		gf_device->pdev_info.start_addr_vram);

	vcmd_mgr->vcmd_buf_mem_pool.bus_address = g_vcmd_base_ddr_hw;// + VCMD_BUF_POOL_OFFSET;
	vcmd_mgr->vcmd_buf_mem_pool.virtual_address = (u32 *)gf_device->dec_bufObj->virt_addres;
	vcmd_mgr->vcmd_buf_mem_pool.size = CMDBUF_POOL_TOTAL_SIZE;
	gf_debug("Init: vcmd_mgr->vcmd_buf_mem_pool.virtual_address=0x%llx, vcmd_mgr->vcmd_buf_mem_pool.bus_address=0x%llx\n",
		(unsigned long long)vcmd_mgr->vcmd_buf_mem_pool.virtual_address,
		(unsigned long long)vcmd_mgr->vcmd_buf_mem_pool.bus_address);

	vcmd_mgr->vcmd_status_buf_mem_pool.bus_address =
		//g_vcmd_base_ddr_hw + VCMD_BUF_POOL_OFFSET + CMDBUF_POOL_TOTAL_SIZE;
		g_vcmd_base_ddr_hw + CMDBUF_POOL_TOTAL_SIZE;
	vcmd_mgr->vcmd_status_buf_mem_pool.virtual_address = vcmd_mgr->vcmd_buf_mem_pool.virtual_address + CMDBUF_POOL_TOTAL_SIZE / 4;
	vcmd_mgr->vcmd_status_buf_mem_pool.size = CMDBUF_POOL_TOTAL_SIZE;

	gf_debug("Init: vcmd_mgr->vcmd_status_buf_mem_pool.virtual_address=0x%llx, vcmd_mgr->vcmd_status_buf_mem_pool.bus_address = 0x%llx\n",
		(unsigned long long)vcmd_mgr->vcmd_status_buf_mem_pool.virtual_address,
		(unsigned long long)vcmd_mgr->vcmd_status_buf_mem_pool.bus_address);

	vcmd_mgr->vcmd_registers_mem_pool.bus_address =
		g_vcmd_base_ddr_hw + //VCMD_BUF_POOL_OFFSET +
		CMDBUF_POOL_TOTAL_SIZE * 2;
	vcmd_mgr->vcmd_registers_mem_pool.virtual_address = vcmd_mgr->vcmd_buf_mem_pool.virtual_address + (CMDBUF_POOL_TOTAL_SIZE * 2) / 4;
	vcmd_mgr->vcmd_registers_mem_pool.size = CMDBUF_VCMD_REGISTER_TOTAL_SIZE;

	gf_debug("Init: vcmd_mgr->vcmd_registers_mem_pool.virtual_address=0x%llx, vcmd_registers_mem_pool.bus_address= 0x%llx\n",
		(unsigned long long)vcmd_mgr->vcmd_registers_mem_pool.virtual_address,
		(unsigned long long)vcmd_mgr->vcmd_registers_mem_pool.bus_address);

	gf_device->reserve_bufObj = gf_kvcalloc(1, sizeof(struct lisuan_gem_bo), GFP_KERNEL);
	gf_device->reserve_bufObj->size = 512 * 1024;//512k byte
	gf_device->reserve_bufObj->align = 64 * 1024;
	gf_device->reserve_bufObj->domain = dom_visual_vram;
	gf_device->reserve_bufObj->flag.byPass = 1;
	atomic_set(&gf_device->reserve_bufObj->unpagable, 1);
	gf_device->reserve_bufObj->priority = gem_priority_high;
	gf_device->reserve_bufObj->flag.vram = (gf_device->dec_bufObj->domain <= dom_invisual_vram) ? 1 : 0;
	gf_device->reserve_bufObj->flag.snoop = ((gf_device->dec_bufObj->domain == dom_snoop_system_pages) ? 1 : 0);
	lisuan_gem_alloc_oneshot(gf_device, NULL, gf_device->reserve_bufObj, 1);
	vm_node = container_of(gf_device->reserve_bufObj->va_list.next, struct lisuan_vm_node, node_item);
	reserve_gpu_va = vm_node->offset;
	get_addr_2k(reserve_gpu_va, gf_device->reserve_offset);

	return 0;

out:
	return -1;
}

static void vcmd_link_cmdbuf(struct hantrovcmd_dev *dev,
				 bi_list_node *last_linked_cmdbuf_node)
{
	bi_list_node *new_cmdbuf_node = NULL;
	bi_list_node *next_cmdbuf_node = NULL;
	struct cmdbuf_obj *cmdbuf_obj = NULL;
	struct cmdbuf_obj *next_cmdbuf_obj = NULL;
	u32 *jmp_addr = NULL;
	u32 operation_code;
#ifdef SUPPORT_DBGFS
	if (active_state[dev->core_id] == 0) {
		u64 time_num;
		struct timeval time_now;
		#if (KERNEL_VERSION(4, 19, 94) > LINUX_VERSION_CODE)
			do_gettimeofday(&time_now);
			time_num = time_now.tv_sec * 1000 + time_now.tv_usec / 1000;
		#else
			struct timespec64 time_now;

			ktime_get_real_ts64(&time_now);
			time_num = time_now.tv_sec * 1000 + time_now.tv_nsec / 1000000;
		#endif
			active_start_time[dev->core_id][active_hw_index[dev->core_id]] = time_num;
			active_state[dev->core_id] = 1;
			if (active_hw_index[dev->core_id] == MAX_RESERVED_TIME)
				active_hw_index[dev->core_id] = 0;
	}
#endif

	new_cmdbuf_node = last_linked_cmdbuf_node;
	//for the first cmdbuf.
	if (new_cmdbuf_node) {
		cmdbuf_obj = (struct cmdbuf_obj *)new_cmdbuf_node->data;
		if (cmdbuf_obj->cmdbuf_data_linked == 0) {
			dev->sw_cmdbuf_rdy_num++;
			cmdbuf_obj->cmdbuf_data_linked = 1;
			dev->duration_without_int = 0;
			if (cmdbuf_obj->has_end_cmdbuf == 0) {
				if (cmdbuf_obj->no_normal_int_cmdbuf == 1) {
					dev->duration_without_int =
						cmdbuf_obj->executing_time;
					//maybe nop is modified, so write back.
					if (dev->duration_without_int >=
						INT_MIN_SUM_OF_IMAGE_SIZE) {
						jmp_addr = cmdbuf_obj->cmdbuf_virtual_address +
							(cmdbuf_obj->cmdbuf_size / 4);
						operation_code =
							*(jmp_addr - 4);
						operation_code = JMP_IE_1 |
								 operation_code;
						*(jmp_addr - 4) =
							operation_code;
						dev->duration_without_int = 0;
					}
				}
			}
		}
	}
	while (1) {
		if (!new_cmdbuf_node)
			break;
		if (!new_cmdbuf_node->next)
			break;
		next_cmdbuf_node = new_cmdbuf_node->next;
		cmdbuf_obj = (struct cmdbuf_obj *)new_cmdbuf_node->data;
		next_cmdbuf_obj = (struct cmdbuf_obj *)next_cmdbuf_node->data;
		if (cmdbuf_obj->has_end_cmdbuf == 0 &&
			!next_cmdbuf_obj->cmdbuf_run_done) {
			//need to link, current cmdbuf link to next cmdbuf
			gf_debug("Link cmdbuf %d to cmdbuf %d",
				   cmdbuf_obj->cmdbuf_id,
				   next_cmdbuf_obj->cmdbuf_id);
			jmp_addr = cmdbuf_obj->cmdbuf_virtual_address +
					   (cmdbuf_obj->cmdbuf_size / 4);
			if (dev->hw_version_id > HW_ID_1_0_C)

				//set next cmdbuf id
				*(jmp_addr - 1) = next_cmdbuf_obj->cmdbuf_id;


			if (0) {
				if (sizeof(size_t) == 8)
					*(jmp_addr - 2) = (u32)((u64)(next_cmdbuf_obj->mmu_cmdbuf_bus_address) >> 32);
				else
					*(jmp_addr - 2) = 0;

				*(jmp_addr - 3) = (u32)(next_cmdbuf_obj->mmu_cmdbuf_bus_address);
			} else {
				if (sizeof(size_t) == 8)
					*(jmp_addr - 2) = (u32)(
						(u64)(next_cmdbuf_obj->cmdbuf_bus_address -
							  dev->base_ddr_addr) >> 32);
				else
					*(jmp_addr - 2) = 0;

				*(jmp_addr - 3) = (u32)(next_cmdbuf_obj->cmdbuf_bus_address - dev->base_ddr_addr);
			}
			operation_code = *(jmp_addr - 4);
			operation_code >>= 16;
			operation_code <<= 16;
			*(jmp_addr - 4) =
				(u32)(operation_code | JMP_RDY_1 |
					  ((next_cmdbuf_obj->cmdbuf_size + 7) / 8));
			next_cmdbuf_obj->cmdbuf_data_linked = 1;
			dev->sw_cmdbuf_rdy_num++;
			//modify nop code of next cmdbuf
			if (next_cmdbuf_obj->has_end_cmdbuf == 0) {
				if (next_cmdbuf_obj->no_normal_int_cmdbuf ==
					1) {
					dev->duration_without_int +=
						next_cmdbuf_obj->executing_time;
					//maybe we see the modified nop before abort, so need to write back.
					if (dev->duration_without_int >=
						INT_MIN_SUM_OF_IMAGE_SIZE) {
						jmp_addr =
							next_cmdbuf_obj->cmdbuf_virtual_address +
							(next_cmdbuf_obj->cmdbuf_size /  4);
						operation_code =
							*(jmp_addr - 4);
						operation_code = JMP_IE_1 |
								 operation_code;
						*(jmp_addr - 4) =
							operation_code;
						dev->duration_without_int = 0;
					}
				}
			} else
				dev->duration_without_int = 0;
#ifdef VCMD_DEBUG_INTERNAL
			{
				u32 i;

				gf_debug("vcmd link, last cmdbuf content\n");
				for (i = cmdbuf_obj->cmdbuf_size / 4 - 8;
					 i < cmdbuf_obj->cmdbuf_size / 4; i++) {
					gf_debug("current linked cmdbuf data %d =0x%x\n",
						i,
						   *(cmdbuf_obj->cmdbuf_virtual_address +
						   i));
				}
			}
#endif
		}
		new_cmdbuf_node = new_cmdbuf_node->next;
	}
}

/* delink all the cmd buffers from the cmdbuf in front of last_linked_cmdbuf_node
 * to head of the list. All the cmd bufs marked as X will be delinked.
 * E.g.,
 * L->L->...->L->L->X->        ...        ->X
 *               ^                          ^
 * head          last_linked_cmdbuf_node    tail
 */
static void vcmd_delink_cmdbuf(struct hantrovcmd_dev *dev,
				   bi_list_node *last_linked_cmdbuf_node)
{
	bi_list_node *new_cmdbuf_node = NULL;
	struct cmdbuf_obj *cmdbuf_obj = NULL;

	new_cmdbuf_node = last_linked_cmdbuf_node;
	while (1) {
		if (!new_cmdbuf_node)
			break;
		cmdbuf_obj = (struct cmdbuf_obj *)new_cmdbuf_node->data;
		if (cmdbuf_obj->cmdbuf_data_linked)
			cmdbuf_obj->cmdbuf_data_linked = 0;
		else
			break;
		new_cmdbuf_node = new_cmdbuf_node->next;
	}
	dev->sw_cmdbuf_rdy_num = 0;
}

static void ConfigAXIFE_MMU_BYVCMD(struct hantrovcmd_dev **device)
{
#ifdef HANTROVCMD_ENABLE_IP_SUPPORT
	u32 i = 0;
	u64 address = 0, address_ext = 0;
	u32 mirror_index, register_index, register_value;
	u32 write_command = 0;
	u16 mmu_offset_address = 0;
	struct hantrovcmd_dev *dev;

	if (!device)
		return;

	dev = *device;
	mirror_index = VCMD_REGISTER_INDEX_SW_INIT_CMD0;
	write_command = OPCODE_WREG | (1 << 26) | (1 << 16);
	//enable AXIFE by VCMD
	if (dev->vcmd_core_cfg.submodule_axife_addr != 0xffff) {
		register_index = AXI_REG10_SW_FRONTEND_EN;
		register_value = 0x02;
		dev->reg_mirror[mirror_index++] =
			write_command |
			(dev->vcmd_core_cfg.submodule_axife_addr +
			 register_index);
		dev->reg_mirror[mirror_index++] = register_value;

		register_index = AXI_REG11_SW_WORK_MODE;
		register_value = 0x02;
		dev->reg_mirror[mirror_index++] =
			write_command |
			(dev->vcmd_core_cfg.submodule_axife_addr +
			 register_index);
		dev->reg_mirror[mirror_index++] = register_value;
	}
#if 0
	//enable MMU by VCMD
	address = GetMMUAddress();
#ifndef PCIE_EN
	address_ext = ((u32)(address >> 32)) & 0xff;
#endif
	gf_debug("%s: address = 0x%llx", __func__, address);
	for (i = 0; i < 2; i++) {
		mmu_offset_address =
			(i == 0) ? dev->vcmd_core_cfg.submodule_MMU_addr :
				   dev->vcmd_core_cfg.submodule_MMUWrite_addr;
		if (mmu_offset_address != 0xffff) {
			register_index = MMU_REG_ADDRESS;
			register_value = address;
			dev->reg_mirror[mirror_index++] =
				write_command |
				(mmu_offset_address + register_index);
			dev->reg_mirror[mirror_index++] = register_value;

			register_index = MMU_REG_ADDRESS_MSB;
			register_value = address_ext;
			dev->reg_mirror[mirror_index++] =
				write_command |
				(mmu_offset_address + register_index);
			dev->reg_mirror[mirror_index++] = register_value;

			register_index = MMU_REG_PAGE_TABLE_ID;
			register_value = 0x10000;
			dev->reg_mirror[mirror_index++] =
				write_command |
				(mmu_offset_address + register_index);
			dev->reg_mirror[mirror_index++] = register_value;

			register_index = MMU_REG_PAGE_TABLE_ID;
			register_value = 0x00000;
			dev->reg_mirror[mirror_index++] =
				write_command |
				(mmu_offset_address + register_index);
			dev->reg_mirror[mirror_index++] = register_value;

			register_index = MMU_REG_CONTROL;
			register_value = 1;
			dev->reg_mirror[mirror_index++] =
				write_command |
				(mmu_offset_address + register_index);
			dev->reg_mirror[mirror_index++] = register_value;
		}
	}
	//END command
#endif
	dev->reg_mirror[mirror_index++] = OPCODE_END;
	dev->reg_mirror[mirror_index] = 0x00;

	for (i = 0; i < mirror_index - VCMD_REGISTER_INDEX_SW_INIT_CMD0; i++) {
		register_index = (i + VCMD_REGISTER_INDEX_SW_INIT_CMD0) * 4;
		vcmd_write_reg(
			(const void *)dev->hwregs, register_index,
			dev->reg_mirror[i + VCMD_REGISTER_INDEX_SW_INIT_CMD0]);
	}
#endif
}

static void vcmd_start(struct hantrovcmd_dev *dev,
			   bi_list_node *first_linked_cmdbuf_node)
{
	struct cmdbuf_obj *cmdbuf_obj = NULL;

	if (dev->working_state == WORKING_STATE_IDLE) {
		if ((first_linked_cmdbuf_node) && dev->sw_cmdbuf_rdy_num) {
			cmdbuf_obj = (struct cmdbuf_obj *)
						 first_linked_cmdbuf_node->data;
			printk_vcmd_register_debug((const void *)dev->hwregs,
						   "vcmd_start enters");
			//0x40
#ifdef HANTROVCMD_ENABLE_IP_SUPPORT
			//when start vcmd, first vcmd is init mode
			vcmd_set_register_mirror_value(dev->reg_mirror,
							   HWIF_VCMD_INIT_MODE, 1);
#endif
			vcmd_set_register_mirror_value(
				dev->reg_mirror, HWIF_VCMD_AXI_CLK_GATE_DISABLE,
				0);
			//this bit should be set 1 only when need to reset dec400
			vcmd_set_register_mirror_value(
				dev->reg_mirror,
				HWIF_VCMD_MASTER_OUT_CLK_GATE_DISABLE, 1);
			vcmd_set_register_mirror_value(
				dev->reg_mirror,
				HWIF_VCMD_CORE_CLK_GATE_DISABLE, 0);
			vcmd_set_register_mirror_value(dev->reg_mirror,
							   HWIF_VCMD_ABORT_MODE, 0);
			vcmd_set_register_mirror_value(dev->reg_mirror,
							   HWIF_VCMD_RESET_CORE, 0);
			vcmd_set_register_mirror_value(dev->reg_mirror,
							   HWIF_VCMD_RESET_ALL, 0);
			vcmd_set_register_mirror_value(
				dev->reg_mirror, HWIF_VCMD_START_TRIGGER, 0);
			//0x48
			if (dev->hw_version_id <= HW_ID_1_0_C)
				vcmd_set_register_mirror_value(
					dev->reg_mirror,
					HWIF_VCMD_IRQ_INTCMD_EN, 0xffff);
			else {
				vcmd_set_register_mirror_value(
					dev->reg_mirror, HWIF_VCMD_IRQ_JMPP_EN,
					1);
				vcmd_set_register_mirror_value(
					dev->reg_mirror, HWIF_VCMD_IRQ_JMPD_EN,
					1);
			}

			if (dev->hw_version_id < HW_ID_1_1_1)
				vcmd_set_register_mirror_value(
					dev->reg_mirror, HWIF_VCMD_IRQ_RESET_EN,
					1);

			vcmd_set_register_mirror_value(
				dev->reg_mirror, HWIF_VCMD_IRQ_ABORT_EN, 1);
			vcmd_set_register_mirror_value(
				dev->reg_mirror, HWIF_VCMD_IRQ_CMDERR_EN, 1);
			vcmd_set_register_mirror_value(
				dev->reg_mirror, HWIF_VCMD_IRQ_TIMEOUT_EN, 1);
			vcmd_set_register_mirror_value(
				dev->reg_mirror, HWIF_VCMD_IRQ_BUSERR_EN, 1);
			vcmd_set_register_mirror_value(
				dev->reg_mirror, HWIF_VCMD_IRQ_ENDCMD_EN, 1);
			//0x4c
			vcmd_set_register_mirror_value(dev->reg_mirror,
							   HWIF_VCMD_TIMEOUT_EN, 1);
			vcmd_set_register_mirror_value(dev->reg_mirror,
							   HWIF_VCMD_TIMEOUT_CYCLES,
							   0x1dcd6500);
			if (0) {
				vcmd_set_register_mirror_value(
				  dev->reg_mirror,
				  HWIF_VCMD_EXECUTING_CMD_ADDR,
				  (u32)(cmdbuf_obj->mmu_cmdbuf_bus_address));
				if (sizeof(size_t) == 8)
					vcmd_set_register_mirror_value(
					  dev->reg_mirror,
					  HWIF_VCMD_EXECUTING_CMD_ADDR_MSB,
					  (u32)((u64)(
						cmdbuf_obj->mmu_cmdbuf_bus_address) >>
						32));
				else
					vcmd_set_register_mirror_value(
					  dev->reg_mirror,
					  HWIF_VCMD_EXECUTING_CMD_ADDR_MSB,
					  0);
			} else {
				vcmd_set_register_mirror_value(
				  dev->reg_mirror,
				  HWIF_VCMD_EXECUTING_CMD_ADDR,
				  (u32)(cmdbuf_obj->cmdbuf_bus_address -
					dev->base_ddr_addr));
					gf_debug("cmdbuf_obj->cmdbuf_bus_address = 0x%lx, dev->base_ddr_addr == 0x%lx", cmdbuf_obj->cmdbuf_bus_address, dev->base_ddr_addr);
				if (sizeof(size_t) == 8)
					vcmd_set_register_mirror_value(
					  dev->reg_mirror,
					  HWIF_VCMD_EXECUTING_CMD_ADDR_MSB,
					  (u32)((u64)
						(cmdbuf_obj->cmdbuf_bus_address -
						dev->base_ddr_addr) >> 32));
				else
					vcmd_set_register_mirror_value(
					  dev->reg_mirror,
					  HWIF_VCMD_EXECUTING_CMD_ADDR_MSB,
					  0);
			}
			vcmd_set_register_mirror_value(
				dev->reg_mirror, HWIF_VCMD_EXE_CMDBUF_LENGTH,
				(u32)((cmdbuf_obj->cmdbuf_size + 7) / 8));
			vcmd_set_register_mirror_value(
				dev->reg_mirror, HWIF_VCMD_RDY_CMDBUF_COUNT,
				dev->sw_cmdbuf_rdy_num);
			vcmd_set_register_mirror_value(
				dev->reg_mirror, HWIF_VCMD_MAX_BURST_LEN, 0x10);
			if (dev->hw_version_id > HW_ID_1_0_C)
				vcmd_write_register_value(
					(const void *)dev->hwregs,
					dev->reg_mirror,
					HWIF_VCMD_CMDBUF_EXECUTING_ID,
					(u32)cmdbuf_obj->cmdbuf_id);

			vcmd_write_reg((const void *)dev->hwregs, 0x44,
					   vcmd_read_reg((const void *)dev->hwregs,
							 0x44));
			vcmd_write_reg((const void *)dev->hwregs, 0x40,
					   dev->reg_mirror[0x40 / 4]);
			vcmd_write_reg((const void *)dev->hwregs, 0x48,
					   dev->reg_mirror[0x48 / 4]);
			vcmd_write_reg((const void *)dev->hwregs, 0x4c,
					   dev->reg_mirror[0x4c / 4]);
			vcmd_write_reg((const void *)dev->hwregs, 0x50,
					   dev->reg_mirror[0x50 / 4]);
			vcmd_write_reg((const void *)dev->hwregs, 0x54,
					   dev->reg_mirror[0x54 / 4]);
			vcmd_write_reg((const void *)dev->hwregs, 0x58,
					   dev->reg_mirror[0x58 / 4]);
			vcmd_write_reg((const void *)dev->hwregs, 0x5c,
					   dev->reg_mirror[0x5c / 4]);
			vcmd_write_reg((const void *)dev->hwregs, 0x60,
					   dev->reg_mirror[0x60 / 4]);
			vcmd_write_reg((const void *)dev->hwregs, 0x64,
					   0xffffffff); //not interrupt cpu

			//init HWIF_VCMD_EXE_CMDBUF_COUNT
			if (!vcmd_get_register_value((const void *)dev->hwregs,
						     dev->reg_mirror,
						     HWIF_VCMD_START_TRIGGER))
				vcmd_write_register_value((const void *)dev->hwregs,
							  dev->reg_mirror,
							  HWIF_VCMD_EXE_CMDBUF_COUNT, 0);

			dev->working_state = WORKING_STATE_WORKING;

			if (dev->hw_version_id >= HW_ID_1_2_1)
				ConfigAXIFE_MMU_BYVCMD(&dev);

			//start
			//this bit should be set 1 only when need to reset dec400 in asic(like Zebu)
			vcmd_set_register_mirror_value(
				dev->reg_mirror,
				HWIF_VCMD_MASTER_OUT_CLK_GATE_DISABLE, 1);
			vcmd_set_register_mirror_value(
				dev->reg_mirror, HWIF_VCMD_START_TRIGGER, 1);
			vcmd_write_reg((const void *)dev->hwregs, 0x40,
					   dev->reg_mirror[0x40 / 4]);

			printk_vcmd_register_debug((const void *)dev->hwregs,
						   "vcmd_start exits ");
		}
	}
}

static void
create_read_all_registers_cmdbuf(vcmd_mgr_t *vcmd_mgr, struct exchange_parameter *input_para)
{
	u32 register_range[] = { VCMD_ENCODER_REGISTER_SIZE,
				 VCMD_IM_REGISTER_SIZE,
				 VCMD_DECODER_REGISTER_SIZE,
				 VCMD_JPEG_ENCODER_REGISTER_SIZE,
				 VCMD_JPEG_DECODER_REGISTER_SIZE };
	u32 counter_cmdbuf_size = 0;
	u32 *set_base_addr = vcmd_mgr->vcmd_buf_mem_pool.virtual_address +
				 input_para->cmdbuf_id * CMDBUF_MAX_SIZE / 4;
	//u32 *status_base_virt_addr=vcmd_mgr->vcmd_status_buf_mem_pool.virtual_address +
	//input_para->cmdbuf_id*CMDBUF_MAX_SIZE/4+
	//(vcmd_mgr->vcmd_manager[input_para->module_type][0]->vcmd_core_cfg.submodule_main_addr/2/4+0);
	addr_t status_base_phy_addr =
		vcmd_mgr->vcmd_status_buf_mem_pool.bus_address +
		input_para->cmdbuf_id * CMDBUF_MAX_SIZE +
		(vcmd_mgr->vcmd_manager[input_para->module_type][0]
				 ->vcmd_core_cfg.submodule_main_addr /
			 2 +
		 0);
	u32 map_status_base_phy_addr =
		vcmd_mgr->vcmd_status_buf_mem_pool.mmu_bus_address +
		input_para->cmdbuf_id * CMDBUF_MAX_SIZE +
		(vcmd_mgr->vcmd_manager[input_para->module_type][0]
				 ->vcmd_core_cfg.submodule_main_addr /
			 2 +
		 0);
	u32 offset_inc = 0;
int i = 0;
	if (vcmd_mgr->vcmd_manager[input_para->module_type][0]->hw_version_id >
		HW_ID_1_0_C) {
		gf_debug("hantrovcmd:create cmdbuf data when hw_version_id = 0x%x\n",
			vcmd_mgr->vcmd_manager[input_para->module_type][0]->hw_version_id);

		//read vcmd executing cmdbuf id registers
		//to ddr for balancing core load.

		*(set_base_addr + 0) = (u32)((OPCODE_RREG) | (1 << 16) |
					   (EXECUTING_CMDBUF_ID_ADDR * 4));
		counter_cmdbuf_size += 4;
		*(set_base_addr + 1) = (u32)0; //will be changed in link stage
		counter_cmdbuf_size += 4;
		*(set_base_addr + 2) = (u32)0; //will be changed in link stage
		counter_cmdbuf_size += 4;
		//alignment
		*(set_base_addr + 3) = (u32)0;
		counter_cmdbuf_size += 4;

		//read main IP all registers
		*(set_base_addr + 4) =
			(u32)((OPCODE_RREG) |
			((register_range[input_para->module_type] / 4) << 16) |
			(vcmd_mgr->vcmd_manager[input_para->module_type][0]->vcmd_core_cfg.submodule_main_addr +  0));
		counter_cmdbuf_size += 4;
		if (0)
			*(set_base_addr + 5) = map_status_base_phy_addr;
		else
			*(set_base_addr + 5) =
				(u32)(status_base_phy_addr - vcmd_mgr->base_ddr_addr);

		counter_cmdbuf_size += 4;
		if (0)
			*(set_base_addr + 6) = (u32)0;
		else {
			if (sizeof(size_t) == 8)
				*(set_base_addr + 6) = (u32)((u64)(status_base_phy_addr - vcmd_mgr->base_ddr_addr) >> 32);
			else
				*(set_base_addr + 6) = (u32)0;
		}
		counter_cmdbuf_size += 4;
		//alignment
		*(set_base_addr + 7) = (u32)0;
		counter_cmdbuf_size += 4;
		//read vcmd registers to ddr
		*(set_base_addr + 8 + offset_inc) =
			(u32)((OPCODE_RREG) | (27 << 16) | (0));
		counter_cmdbuf_size += 4;
		*(set_base_addr + 9 + offset_inc) =
			(u32)0; //will be changed in link stage
		counter_cmdbuf_size += 4;
		*(set_base_addr + 10 + offset_inc) =
			(u32)0; //will be changed in link stage
		counter_cmdbuf_size += 4;
		//alignment
		*(set_base_addr + 11 + offset_inc) = (u32)0;
		counter_cmdbuf_size += 4;
		//JMP RDY = 0
		*(set_base_addr + 12 + offset_inc) =
			(u32)((OPCODE_JMP_RDY0) | 0 | JMP_IE_1 | 0);
		counter_cmdbuf_size += 4;
		*(set_base_addr + 13 + offset_inc) = (u32)0;
		counter_cmdbuf_size += 4;
		*(set_base_addr + 14 + offset_inc) = (u32)0;
		counter_cmdbuf_size += 4;
		*(set_base_addr + 15 + offset_inc) = input_para->cmdbuf_id;
		//don't add the last alignment DWORD in order
		//to  identify END command or JMP command.
		//counter_cmdbuf_size += 4;
		input_para->cmdbuf_size = (16 + offset_inc) * 4;
	} else {
		gf_debug("hantrovcmd:create cmdbuf data when hw_version_id = 0x%x\n",
			vcmd_mgr->vcmd_manager[input_para->module_type][0]->hw_version_id);
		//read all registers
		*(set_base_addr + 0) =
			(u32)((OPCODE_RREG) |
			((register_range[input_para->module_type] / 4) << 16) |
			(vcmd_mgr->vcmd_manager[input_para->module_type][0]->vcmd_core_cfg.submodule_main_addr + 0));
		counter_cmdbuf_size += 4;
		if (0)
			*(set_base_addr + 1) = map_status_base_phy_addr;
		else
			*(set_base_addr + 1) =
				(u32)(status_base_phy_addr - vcmd_mgr->base_ddr_addr);

		counter_cmdbuf_size += 4;
		if (0) {
			*(set_base_addr + 2) = (u32)0;
		} else {
			if (sizeof(size_t) == 8)
				*(set_base_addr + 2) =
					(u32)((u64)(status_base_phy_addr -
							vcmd_mgr->base_ddr_addr) >> 32);
			else
				*(set_base_addr + 2) = (u32)0;
		}
		counter_cmdbuf_size += 4;
		//alignment
		*(set_base_addr + 3) = (u32)0;
		counter_cmdbuf_size += 4;
#if 0
			//INT code, interrupt immediately
			*(set_base_addr + 4) = (OPCODE_INT) |
			0 | input_para->cmdbuf_id;
			counter_cmdbuf_size += 4;
			//alignment
			*(set_base_addr + 5) = 0;
			counter_cmdbuf_size += 4;
#endif
		//JMP RDY = 0
		*(set_base_addr + 4) = (u32)((OPCODE_JMP_RDY0) | 0 | JMP_IE_1 | 0);
		counter_cmdbuf_size += 4;
		*(set_base_addr + 5) = (u32)0;
		counter_cmdbuf_size += 4;
		*(set_base_addr + 6) = (u32)0;
		counter_cmdbuf_size += 4;
		*(set_base_addr + 7) = input_para->cmdbuf_id;
		//don't add the last alignment DWORD in order to
		//identify END command or JMP command.
		//counter_cmdbuf_size += 4;
		input_para->cmdbuf_size = 8 * 4;
	}
}

static void read_main_module_all_registers(vcmd_mgr_t *vcmd_mgr, u32 main_module_type)
{
	int ret;
	struct exchange_parameter input_para;
	u32 irq_status_ret = 0;
	u32 *status_base_virt_addr;

	input_para.executing_time = 0;
	input_para.priority = CMDBUF_PRIORITY_NORMAL;
	input_para.module_type = main_module_type;
	input_para.cmdbuf_size = 0;
	input_para.core_mask = 1 << vcmd_mgr->vcmd_manager[main_module_type][0]->core_id;
	ret = reserve_cmdbuf(vcmd_mgr, NULL, &input_para);

	vcmd_mgr->vcmd_manager[main_module_type][0]->status_cmdbuf_id =
		input_para.cmdbuf_id;
	create_read_all_registers_cmdbuf(vcmd_mgr, &input_para);

	link_and_run_cmdbuf(vcmd_mgr, NULL, &input_para, 1, 0);

	/* used to make sure vcmd can compelte job, and polling to clear irq
	 * irq mode can comment out this codes
	 */
	{
		gf_msleep(1000);
		lisuan_dec_vcmd_clear_irq_status(input_para.core_id, vcmd_mgr);
		hantrovcmd_isr(input_para.core_id, vcmd_mgr, 1);
	}

	wait_cmdbuf_ready(vcmd_mgr, NULL, input_para.cmdbuf_id, &irq_status_ret, 1);

	vcmd_mgr->vcd_cycles = 0;
	status_base_virt_addr =
		vcmd_mgr->vcmd_status_buf_mem_pool.virtual_address +
		input_para.cmdbuf_id * CMDBUF_MAX_SIZE / 4 +
		(vcmd_mgr->vcmd_manager[input_para.module_type][0]->vcmd_core_cfg.submodule_main_addr / 2 / 4 + 0);
	gf_debug("hantrovcmd: main module register 0:0x%x\n",
		*status_base_virt_addr);
	gf_debug("hantrovcmd: main module register 50:0x%08x\n",
		*(status_base_virt_addr + 50));
	gf_debug("hantrovcmd: main module register 54:0x%08x\n",
		*(status_base_virt_addr + 54));
	gf_debug("hantrovcmd: main module register 56:0x%08x\n",
		*(status_base_virt_addr + 56));
	gf_debug("hantrovcmd: main module register 309:0x%x\n",
		*(status_base_virt_addr + 309));
	//don't release cmdbuf because it can be used repeatedly
	//release_cmdbuf(input_para.cmdbuf_id);

	input_para.executing_time = 0;
	input_para.priority = CMDBUF_PRIORITY_NORMAL;
	input_para.module_type = main_module_type;
	input_para.cmdbuf_size = 0;
	input_para.core_mask = 1 << vcmd_mgr->vcmd_manager[main_module_type][1]->core_id;
	ret = reserve_cmdbuf(vcmd_mgr, NULL, &input_para);

	vcmd_mgr->vcmd_manager[main_module_type][1]->status_cmdbuf_id =
		input_para.cmdbuf_id;
	create_read_all_registers_cmdbuf(vcmd_mgr, &input_para);

	link_and_run_cmdbuf(vcmd_mgr, NULL, &input_para, 1, 0);

	/* used to make sure vcmd can compelte job, and polling to clear irq
	 * irq mode can comment out this codes
	 */
	{
		gf_msleep(1000);
		lisuan_dec_vcmd_clear_irq_status(input_para.core_id, vcmd_mgr);
		hantrovcmd_isr(input_para.core_id, vcmd_mgr, 1);
	}

	wait_cmdbuf_ready(vcmd_mgr, NULL, input_para.cmdbuf_id, &irq_status_ret, 1);

	vcmd_mgr->vcd_cycles = 0;
	status_base_virt_addr =
		vcmd_mgr->vcmd_status_buf_mem_pool.virtual_address +
		input_para.cmdbuf_id * CMDBUF_MAX_SIZE / 4 +
		(vcmd_mgr->vcmd_manager[input_para.module_type][1]->vcmd_core_cfg.submodule_main_addr / 2 / 4 + 0);
	gf_debug("hantrovcmd: main module register 0:0x%x\n",
		*status_base_virt_addr);
	gf_debug("hantrovcmd: main module register 50:0x%08x\n",
		*(status_base_virt_addr + 50));
	gf_debug("hantrovcmd: main module register 54:0x%08x\n",
		*(status_base_virt_addr + 54));
	gf_debug("hantrovcmd: main module register 56:0x%08x\n",
		*(status_base_virt_addr + 56));
	gf_debug("hantrovcmd: main module register 309:0x%x\n",
		*(status_base_virt_addr + 309));
	//don't release cmdbuf because it can be used repeatedly
	//release_cmdbuf(input_para.cmdbuf_id);
}

static void SubsysToVcmdCoreCfg(struct subsys_config *subsys, int subsys_num, vcmd_mgr_t *vcmd_mgr)
{
	int i, j = 0;

	/* To plug into hantro_vcmd.c */
	for (i = 0; i < subsys_num; i++) {
		if (subsys[i].submodule_iosize[HW_VCMD]) {
			u32 core_type = HW_VCD; /* vcd */

			vcmd_mgr->vcmd_core_array[j].vcmd_base_addr = subsys[i].base_addr;
			vcmd_mgr->vcmd_core_array[j].vcmd_iosize = subsys[i].submodule_iosize[HW_VCMD];
			vcmd_mgr->vcmd_core_array[j].vcmd_irq = subsys[i].irq;
			vcmd_mgr->vcmd_core_array[j].sub_module_type = subsys[i].subsys_type;
			vcmd_mgr->vcmd_core_array[j].submodule_main_addr = subsys[i].submodule_offset[core_type];
			vcmd_mgr->vcmd_core_array[j].submodule_dec400_addr = subsys[i].submodule_offset[HW_DEC400];
			vcmd_mgr->vcmd_core_array[j].submodule_MMU_addr = subsys[i].submodule_offset[HW_MMU];
			vcmd_mgr->vcmd_core_array[j].submodule_MMUWrite_addr = subsys[i].submodule_offset[HW_MMU_WR];
			vcmd_mgr->vcmd_core_array[j].submodule_axife_addr = subsys[i].submodule_offset[HW_AXIFE];

			/* for debugfs */
			vcmd_mgr->vcmd_core_array[j].submodule_vcd_iosize =
				subsys[i].submodule_iosize[core_type];
			vcmd_mgr->vcmd_core_array[j].submodule_dec400_iosize =
				subsys[i].submodule_iosize[HW_DEC400];
			vcmd_mgr->vcmd_core_array[j].submodule_MMU_iosize =
				subsys[i].submodule_iosize[HW_MMU];
			vcmd_mgr->vcmd_core_array[j].submodule_MMUWrite_iosize =
				subsys[i].submodule_iosize[HW_MMU_WR];
			vcmd_mgr->vcmd_core_array[j].submodule_axife_iosize =
				subsys[i].submodule_iosize[HW_AXIFE];

			vcmd_mgr->vcmd_core_array[j].submodule_vcd_virtual_address = vcmd_core_array[i].submodule_vcd_virtual_address;
			vcmd_mgr->vcmd_core_array[j].submodule_dec400_virtual_address = vcmd_core_array[i].submodule_dec400_virtual_address;
			vcmd_mgr->vcmd_core_array[j].submodule_MMU_virtual_address = vcmd_core_array[i].submodule_MMU_virtual_address;
			vcmd_mgr->vcmd_core_array[j].submodule_MMUWrite_virtual_address = vcmd_core_array[i].submodule_MMUWrite_virtual_address;
			vcmd_mgr->vcmd_core_array[j].submodule_axife_virtual_address = vcmd_core_array[i].submodule_axife_virtual_address;

			j++;
		}
	}
	vcmd_mgr->total_vcmd_core_num = j;
	gf_debug("hantrovcmd: %d VCMD cores found\n", j);
}

void* hantro_vcmd_resume(struct lisuan_device *gf_device, struct subsys_config *subsys)
{
	int i, k;
	int result;
	struct hantrovcmd_dev *hantrovcmd_data;
	vcmd_mgr_t *vcmd_mgr = gf_device->vcmd_mgr;

	if (!vcmd_mgr)
		return NULL;

	if (!vcmd_mgr->total_vcmd_core_num)
		goto err;

	hantrovcmd_data = vcmd_mgr->hantrovcmd_data;
	if (!hantrovcmd_data)
		goto err;

	for (i = 0; i < vcmd_mgr->total_vcmd_core_num; i++) {
		hantrovcmd_data[i].working_state = WORKING_STATE_IDLE;
		hantrovcmd_data[i].sw_cmdbuf_rdy_num = 0;
	}

	vcmd_reset_asic(vcmd_mgr);

	return vcmd_mgr;

err:
	return NULL;
}

/* returns a pointer to a vcmd_mgr_t structure */
void *hantrodec_vcmd_init(struct lisuan_device *gf_device, struct subsys_config *subsys, int subsys_num, void *platformdev)
{
	int i, k;
	int result;
	int total_vcmd_core_num;
	struct hantrovcmd_dev *hantrovcmd_data;
	vcmd_mgr_t *vcmd_mgr;

	vcmd_mgr = gf_kvcalloc(1, sizeof(vcmd_mgr_t), GFP_KERNEL);
	if (!vcmd_mgr)
		return NULL;

	gf_memset(vcmd_mgr, 0, sizeof(vcmd_mgr_t));
	SubsysToVcmdCoreCfg(subsys, subsys_num, vcmd_mgr);
#ifdef SUPPORT_DBGFS
	vcmd_mgr_debugfs = vcmd_mgr;
#endif
	if (!vcmd_mgr->total_vcmd_core_num)
		goto err;

	total_vcmd_core_num = vcmd_mgr->total_vcmd_core_num;
	vcmd_mgr->platformdev = (struct platform_device *)platformdev;
	result = vcmd_init(gf_device, vcmd_mgr);
	if (result)
		goto err;

	for (i = 0; i < total_vcmd_core_num; i++) {
		gf_debug("%s vcmd: module init - vcmdcore[%d] addr =0x%llx\n",
			__func__, i,
			   (unsigned long long)vcmd_mgr->vcmd_core_array[i].vcmd_base_addr);
	}
	hantrovcmd_data = gf_kvcalloc(1, sizeof(*hantrovcmd_data) * total_vcmd_core_num, GFP_KERNEL);
	if (!hantrovcmd_data)
		goto err;

	vcmd_mgr->hantrovcmd_data = hantrovcmd_data;
	gf_memset(hantrovcmd_data, 0,
		   sizeof(struct hantrovcmd_dev) * vcmd_mgr->total_vcmd_core_num);
	for (k = 0; k < MAX_VCMD_TYPE; k++) {
		vcmd_mgr->vcmd_type_core_num[k] = 0;
		vcmd_mgr->vcmd_position[k] = 0;
		for (i = 0; i < MAX_VCMD_NUMBER; i++)
			vcmd_mgr->vcmd_manager[k][i] = NULL;
	}

	init_bi_list(&vcmd_mgr->global_process_manager);

	init_waitqueue_head(&vcmd_mgr->vcmd_cmdbuf_memory_wait);
	spin_lock_init(&vcmd_mgr->vcmd_cmdbuf_alloc_lock);
	spin_lock_init(&vcmd_mgr->vcmd_process_manager_lock);

	for (i = 0; i < vcmd_mgr->total_vcmd_core_num; i++) {
		hantrovcmd_data[i].vcmd_core_cfg = vcmd_mgr->vcmd_core_array[i];
		hantrovcmd_data[i].hwregs = NULL;
		hantrovcmd_data[i].core_id = i;
		hantrovcmd_data[i].working_state = WORKING_STATE_IDLE;
		hantrovcmd_data[i].sw_cmdbuf_rdy_num = 0;
		hantrovcmd_data[i].spinlock = &vcmd_mgr->owner_lock_vcmd[i];
		spin_lock_init(&vcmd_mgr->owner_lock_vcmd[i]);
		spin_lock_init(&hantrovcmd_data[i].abnormal_lock);
		hantrovcmd_data[i].wait_queue = &vcmd_mgr->wait_queue_vcmd[i];
		init_waitqueue_head(&vcmd_mgr->wait_queue_vcmd[i]);
		hantrovcmd_data[i].wait_abort_queue = &vcmd_mgr->abort_queue_vcmd[i];
		init_waitqueue_head(&vcmd_mgr->abort_queue_vcmd[i]);
		init_bi_list(&hantrovcmd_data[i].list_manager);
		hantrovcmd_data[i].duration_without_int = 0;
		vcmd_mgr->vcmd_manager[vcmd_mgr->vcmd_core_array[i].sub_module_type][vcmd_mgr->vcmd_type_core_num[vcmd_mgr->vcmd_core_array[i].sub_module_type]] =
					&hantrovcmd_data[i];
		vcmd_mgr->vcmd_type_core_num[vcmd_mgr->vcmd_core_array[i].sub_module_type]++;
		hantrovcmd_data[i].vcmd_reg_mem_bus_address =
			vcmd_mgr->vcmd_registers_mem_pool.bus_address +
			i * VCMD_REGISTER_SIZE - vcmd_mgr->base_ddr_addr;
		hantrovcmd_data[i].mmu_vcmd_reg_mem_bus_address =
			vcmd_mgr->vcmd_registers_mem_pool.mmu_bus_address +
			i * VCMD_REGISTER_SIZE;
		hantrovcmd_data[i].vcmd_reg_mem_virtual_address =
			vcmd_mgr->vcmd_registers_mem_pool.virtual_address +
			i * VCMD_REGISTER_SIZE / 4;
		hantrovcmd_data[i].vcmd_reg_mem_size = VCMD_REGISTER_SIZE;
		gf_memset(hantrovcmd_data[i].vcmd_reg_mem_virtual_address, 0,
			   VCMD_REGISTER_SIZE);
		hantrovcmd_data[i].base_ddr_addr = vcmd_mgr->base_ddr_addr;
#ifdef SUPPORT_DBGFS
		/* for debugfs */
		hantrovcmd_data_debugfs = vcmd_mgr->hantrovcmd_data;
		cycle_index[i] = 0;
		e_index[i] = 0;
		r_index[i] = 0;
		l_index[i] = 0;
		i_index[i] = 0;
		active_hw_index[i] = 0;
		active_state[i] = 0;
		num_cmdbuf_linked[i] = 0;
		num_cmdbuf_twoidle[i] = 0;
		pre_hw_rdy_cmdbuf_num[i] = 0;
		vcmd_workload[i] = 0;
		for (k = 0; k < MAX_RESERVED_TIME; k++) {
			decode_cycles[i][k] = 0;
			num_cmdbuf_exe[i][k] = 0;
			num_cmdbuf_reserved[i][k] = 0;
			reserved_time[i][k] = 0;
			reserved_cmdbufid[i][k] = 0;
			link_time[i][k] = 0;
			link_cmdbufid[i][k] = 0;
			interrupt_time[i][k] = 0;
			active_start_time[i][k] = 0;
			active_return_time[i][k] = 0;
			cmdbuf_num_done[i][k] = 0;
			prev_cmdbuf_done[i][k] = 0;
		}
#endif
		hantrovcmd_data[i].dec_ring = gf_device->sched[i + 3];
	}
	init_waitqueue_head(&vcmd_mgr->mc_wait_queue);
	spin_lock_init(&vcmd_mgr->mutex);
	vcmd_mgr->mc_token_owner = NULL;
#ifdef SUPPORT_WATCHDOG
	//remap swreg of vcd
	for (i = 0; i < total_vcmd_core_num; i++)
		vcd_virt[i] = (unsigned long)ioremap(vcmd_mgr->vcmd_core_array[i].vcmd_base_addr + vcmd_mgr->vcmd_core_array[i].submodule_main_addr, 0x20);
	watch_dev = vcmd_mgr;
#if (KERNEL_VERSION(4, 14, 0) > LINUX_VERSION_CODE)
	init_timer(&vcmd_timer);
	vcmd_timer.expires =  jiffies + 200;
	vcmd_timer.function = vcmd_watchdog;
	add_timer(&vcmd_timer);
#else
	timer_setup(&vcmd_timer, vcmd_watchdog, 0);
	vcmd_timer.expires =  jiffies + 200;
	add_timer(&vcmd_timer);
#endif
#endif
	result = vcmd_reserve_IO(gf_device, vcmd_mgr);

	if (result < 0)
		goto err;
#ifdef SUPPORT_DBGFS
	debugfs_root = debugfs_create_dir(dec_dev_n, NULL);
	if (!debugfs_root)
		goto err;
	debugfs_create_u64("hw_cycle", 0644, debugfs_root, &vcmd_mgr->vcd_cycles);

	debug_root = debugfs_create_dir("VCDEC", NULL);
	debugfs_create_file("performance", 0444, debug_root, NULL, &fileop_N_vcd_cycles);

	if (!debug_root)
		goto err;
#endif
	vcmd_reset_asic(vcmd_mgr);

#ifdef SUPPORT_DBGFS
	debugfs_create_file("regprint", 0444, debug_root, NULL, &fileop_vcmd_reg_print);
#endif
	vcmd_mgr->vcmd_irq_enabled = 0;
	for (i = 0; i < vcmd_mgr->total_vcmd_core_num; i++) {
		if (hantrovcmd_data[i].vcmd_core_cfg.vcmd_irq != -1) {
			vcmd_mgr->vcmd_irq_enabled = 1;
			break;
		}
	}
	//cmdbuf pool allocation
	//init_vcmd_non_cachable_memory_allocate();
	//for cmdbuf management
	vcmd_mgr->cmdbuf_used_pos = 0;
	for (k = 0; k < TOTAL_DISCRETE_CMDBUF_NUM; k++) {
		vcmd_mgr->cmdbuf_used[k] = 0;
		vcmd_mgr->global_cmdbuf_node[k] = NULL;
	}
	//vcmd_mgr->cmdbuf_used[0] not be used, because int vector must non-zero
	vcmd_mgr->cmdbuf_used_residual = TOTAL_DISCRETE_CMDBUF_NUM;
	vcmd_mgr->cmdbuf_used_pos = 1;
	vcmd_mgr->cmdbuf_used[0] = 1;
	vcmd_mgr->cmdbuf_used_residual -= 1;

	create_kernel_process_manager(vcmd_mgr);

	for (i = 0; i < MAX_VCMD_TYPE; i++) {
		if (vcmd_mgr->vcmd_type_core_num[i] == 0)
			continue;
		sema_init(&vcmd_mgr->vcmd_reserve_cmdbuf_sem[i], 1);
	}

#ifdef IRQ_SIMULATION
	for (i = 0; i < 10000; i++)
		timer_reserve[i].timer = NULL;
#endif
#ifndef DYNAMIC_MALLOC_VCMDNODE
	g_cmdbuf_obj_pool = gf_kvcalloc(1, sizeof(struct cmdbuf_obj) * TOTAL_DISCRETE_CMDBUF_NUM, GFP_KERNEL);
	g_cmdbuf_node_pool = gf_kvcalloc(1, sizeof(bi_list_node) * TOTAL_DISCRETE_CMDBUF_NUM, GFP_KERNEL);
#endif

	/* read all registers for each type of module
	 * for analyzing configuration in cwl
	 */
	for (i = 0; i < MAX_VCMD_TYPE; i++) {
		if (vcmd_mgr->vcmd_type_core_num[i] == 0)
			continue;
		read_main_module_all_registers(vcmd_mgr, i);
	}
	return vcmd_mgr;

err:
	if (vcmd_mgr->hantrovcmd_data) {
		gf_kvfree(vcmd_mgr->hantrovcmd_data);
	}
	if (vcmd_mgr)
		gf_kvfree(vcmd_mgr);

	lisuan_gem_free_oneshot(gf_device, gf_device->dec_bufObj);
	gf_debug("hantrovcmd: module not inserted\n");
	return NULL;
}

void hantrovcmd_cleanup(struct lisuan_device *gf_dev, void *_vcmd_mgr)
{
	int i = 0;
	u32 result;
	vcmd_mgr_t *vcmd_mgr = (vcmd_mgr_t *)_vcmd_mgr;
	struct hantrovcmd_dev *hantrovcmd_data;

	if (vcmd_mgr == NULL) {
		return;
	}

	hantrovcmd_data = vcmd_mgr->hantrovcmd_data;

#ifdef SUPPORT_WATCHDOG
	del_timer(&vcmd_timer);
	for (i = 0; i < vcmd_mgr->total_vcmd_core_num; i++)
		iounmap(vcd_virt[i]);
#endif

	for (i = 0; i < vcmd_mgr->total_vcmd_core_num; i++) {
		if (!hantrovcmd_data[i].hwregs)
			continue;
		//disable interrupt at first
		vcmd_write_reg((const void *)hantrovcmd_data[i].hwregs,
				   VCMD_REGISTER_INT_CTL_OFFSET, 0x0000);
		//disable HW
		vcmd_write_reg((const void *)hantrovcmd_data[i].hwregs,
				   VCMD_REGISTER_CONTROL_OFFSET, 0x0000);
		//read status register
		result = vcmd_read_reg((const void *)hantrovcmd_data[i].hwregs,
					   VCMD_REGISTER_INT_STATUS_OFFSET);
		//clean status register
		vcmd_write_reg((const void *)hantrovcmd_data[i].hwregs,
				   VCMD_REGISTER_INT_STATUS_OFFSET, result);

		release_cmdbuf_node_cleanup(vcmd_mgr, &hantrovcmd_data[i].list_manager);
	}

	release_process_node_cleanup(&vcmd_mgr->global_process_manager);
#ifdef SUPPORT_DBGFS
	debugfs_remove_recursive(debugfs_root);
	debugfs_root = NULL;
	debugfs_remove_recursive(debug_root);
	debug_root = NULL;
#endif
	vcmd_release_IO(vcmd_mgr);

	gf_kvfree(hantrovcmd_data);

#ifndef DYNAMIC_MALLOC_VCMDNODE
	if (g_cmdbuf_obj_pool) {
		gf_kvfree(g_cmdbuf_obj_pool);
		g_cmdbuf_obj_pool = NULL;
	}
	if (g_cmdbuf_node_pool) {
		gf_kvfree(g_cmdbuf_node_pool);
		g_cmdbuf_node_pool = NULL;
	}
#endif

	lisuan_gem_free_oneshot(gf_dev, gf_dev->dec_bufObj);

	gf_debug("hantrovcmd: module removed\n");
}

static int vcmd_reserve_IO(struct lisuan_device *gf_device, vcmd_mgr_t *vcmd_mgr)
{
	u32 hwid;
	int i;
	u32 found_hw = 0;
	struct hantrovcmd_dev *hantrovcmd_data = vcmd_mgr->hantrovcmd_data;

	for (i = 0; i < vcmd_mgr->total_vcmd_core_num; i++) {
		hantrovcmd_data[i].hwregs = NULL;

		hantrovcmd_data[i].hwregs = (gf_device->mmio_base + hantrovcmd_data[i].vcmd_core_cfg.vcmd_base_addr);
		hantrovcmd_data[i].hwregs_submodule_main = (gf_device->mmio_base + hantrovcmd_data[i].vcmd_core_cfg.vcmd_base_addr
											+ hantrovcmd_data[i].vcmd_core_cfg.submodule_main_addr);
		/*read hwid and check validness and store it*/
		hwid = (u32)gf_read_mmio32(hantrovcmd_data[i].hwregs);
		gf_debug("hwid=0x%08x\n", hwid);
		hantrovcmd_data[i].hw_version_id = hwid;

		/* check for vcmd HW ID */
		if (((hwid >> 16) & 0xFFFF) != VCMD_HW_ID) {
			gf_debug("hantrovcmd: HW not found at 0x%llx\n",
				(unsigned long long)
				hantrovcmd_data[i].vcmd_core_cfg.vcmd_base_addr);
			continue;
		}
		found_hw = 1;
		gf_debug("hantrovcmd: HW at base <0x%llx> with ID <0x%08x>\n",
			(unsigned long long)hantrovcmd_data[i]
				   .vcmd_core_cfg.vcmd_base_addr,
			   hwid);
	}

	if (found_hw == 0) {
		gf_error("hantrovcmd: NO ANY HW found!!\n");
		return -1;
	}

	return 0;
}

static void vcmd_release_IO(vcmd_mgr_t *vcmd_mgr)
{
	u32 i;

	for (i = 0; i < vcmd_mgr->total_vcmd_core_num; i++) {
		if (vcmd_mgr->hantrovcmd_data[i].hwregs) {
			vcmd_mgr->hantrovcmd_data[i].hwregs = NULL;
		}
	}
}

static u32 wait_fence_ready(struct hantrovcmd_dev *dev)
{
	u32 cur_cmd_id = 0, fence_data = 0;
	u32 count=0;

	while (1) {
		if ((*dev->dec_ring->fence_node.cpu_virt_addr == atomic64_read(&dev->dec_ring->last_submit_fence_id)) &&
			(*dev->dec_ring->fence_node.cpu_virt_addr == atomic64_read(&dev->dec_ring->returned_fence_id))) {
			cur_cmd_id = *(dev->vcmd_reg_mem_virtual_address + EXECUTING_CMDBUF_ID_ADDR);
			break;
		}
		udelay(5);
		count++;
		if (count == 1000) 
		{
			return *(dev->vcmd_reg_mem_virtual_address + EXECUTING_CMDBUF_ID_ADDR);
		}
	}

	return cur_cmd_id;
}

int hantrovcmd_isr(int irq, void *dev_id, int is_internal)
{
	unsigned int handled = 0;
	vcmd_mgr_t *vcmd_mgr = (vcmd_mgr_t *)dev_id;
	struct hantrovcmd_dev *dev = NULL;
	u32 irq_status = 0;
	unsigned long flags=0, flags2=0;
	bi_list_node *new_cmdbuf_node = NULL;
	bi_list_node *base_cmdbuf_node = NULL;
	struct cmdbuf_obj *cmdbuf_obj = NULL;
	size_t exe_cmdbuf_bus_address;
	u32 cmdbuf_processed_num = 0;
	u32 cmdbuf_id = 0;
	u32 i = 0;
	u32 *status_base_virt_addr = NULL;
#ifdef SUPPORT_DBGFS
	u64 time_num;
	struct timeval time_now;
#endif
	/* all vcmd_irq==-1, there is no IRQ, use irq as core_id */
	if (vcmd_mgr->vcmd_irq_enabled == 0) {
		dev = &vcmd_mgr->hantrovcmd_data[irq];
	} else { /* there is IRQ acturely */
		for (i = 0; i < vcmd_mgr->total_vcmd_core_num; i++) {
			if (vcmd_mgr->hantrovcmd_data[i].vcmd_core_cfg.vcmd_irq == irq) {
				dev = &vcmd_mgr->hantrovcmd_data[i];
				break;
			}
		}
		if (i == vcmd_mgr->total_vcmd_core_num)
			return IRQ_HANDLED;
	}

#ifdef SUPPORT_DBGFS
	u32 hw_rdy_cmdbuf_num;

	if (dev->hw_version_id <= HW_ID_1_0_C)
		hw_rdy_cmdbuf_num = vcmd_get_register_value(
			(const void *)dev->hwregs, dev->reg_mirror,
			HWIF_VCMD_EXE_CMDBUF_COUNT);
	else {
		hw_rdy_cmdbuf_num = *(dev->vcmd_reg_mem_virtual_address +
			VCMD_EXE_CMDBUF_COUNT);
		if (hw_rdy_cmdbuf_num != dev->sw_cmdbuf_rdy_num)
			hw_rdy_cmdbuf_num += 1;
	}

	if ((active_state[dev->core_id] == 1) && (dev->sw_cmdbuf_rdy_num == hw_rdy_cmdbuf_num)) {
		if (vcmd_get_register_value(
					(const void *)dev->hwregs, dev->reg_mirror,
					HWIF_VCMD_IRQ_JMPD)){
		#if (KERNEL_VERSION(4, 19, 94) > LINUX_VERSION_CODE)
			struct timeval time_now;

			do_gettimeofday(&time_now);
			time_num = time_now.tv_sec * 1000 + time_now.tv_usec / 1000;
		#else
			struct timespec64 time_now;

			ktime_get_real_ts64(&time_now);
			time_num = time_now.tv_sec * 1000 + time_now.tv_nsec / 1000000;
		#endif
		active_return_time[dev->core_id][active_hw_index[dev->core_id]] = time_num;
		cmdbuf_num_done[dev->core_id][active_hw_index[dev->core_id]] = hw_rdy_cmdbuf_num - prev_cmdbuf_done[dev->core_id][active_hw_index[dev->core_id]];
		active_hw_index[dev->core_id] += 1;

		if (active_hw_index[dev->core_id] == MAX_RESERVED_TIME)
			active_hw_index[dev->core_id] = 0;

		active_state[dev->core_id] = 0;
		prev_cmdbuf_done[dev->core_id][active_hw_index[dev->core_id]] = hw_rdy_cmdbuf_num;
		}

		num_cmdbuf_twoidle[dev->core_id] = hw_rdy_cmdbuf_num - pre_hw_rdy_cmdbuf_num[dev->core_id];
		pre_hw_rdy_cmdbuf_num[dev->core_id] = hw_rdy_cmdbuf_num;
	}
#endif
	/*If core is not reserved by any user,
	 * but irq is received, just clean it
	 */
	spin_lock_irqsave(dev->spinlock, flags);
	if (!dev->list_manager.head) {
		gf_debug("hantrovcmd_isr:received IRQ but core has nothing to do.\n");
		spin_unlock_irqrestore(dev->spinlock, flags);
		return IRQ_HANDLED;
	}

	gf_debug("hantrovcmd_isr: received IRQ!\n");
	irq_status = dev->irq_status;
	if (dev->hw_version_id >= HW_ID_1_5_0) {
		//The normal and abnormal interrupts source from IPs to CPU.
		u32 intr_src = vcmd_read_reg((const void *)dev->hwregs, VCMD_REGISTER_SW_EXT_INT_SRC_OFFSET);
		u32 submodule_main_reg1 = 0;
		u32 slice_run_done = 0;

		if (intr_src >> 24 == 1 && irq_status == 0) {
			cmdbuf_id = vcmd_get_register_value((const void *)dev->hwregs,
							dev->reg_mirror,
							HWIF_VCMD_CMDBUF_EXECUTING_ID);
			new_cmdbuf_node = vcmd_mgr->global_cmdbuf_node[cmdbuf_id];
			if (!new_cmdbuf_node) {
				gf_error("hantrovcmd_isr error cmdbuf_id !!\n");
				spin_unlock_irqrestore(dev->spinlock, flags);
				wake_up_interruptible_all(dev->wait_queue);
				wake_up_interruptible_all(&vcmd_mgr->mc_wait_queue);
				return IRQ_HANDLED;
			}
			cmdbuf_obj = (struct cmdbuf_obj *)new_cmdbuf_node->data;

			spin_lock_irqsave(&dev->abnormal_lock, flags2);
			submodule_main_reg1 = (u32)gf_read_mmio32((dev->hwregs_submodule_main + 0x4));
			slice_run_done = cmdbuf_obj->slice_run_done;

			spin_unlock_irqrestore(&dev->abnormal_lock, flags2);

			if (submodule_main_reg1 >> 22 == 1) {
				/*Only clear bit 22 of VCD register 1 (line counter) */
				submodule_main_reg1 &= ~0x400000;
				gf_write_mmio32((dev->hwregs_submodule_main + 0x4), submodule_main_reg1);
			} else if (submodule_main_reg1 >> 14 == 1 && slice_run_done == 0) {
				status_base_virt_addr =
				vcmd_mgr->vcmd_status_buf_mem_pool.virtual_address + cmdbuf_id * CMDBUF_MAX_SIZE / 4 +
				(vcmd_mgr->vcmd_manager[cmdbuf_obj->module_type][0]->vcmd_core_cfg.submodule_main_addr / 2 / 4 + 1);
				submodule_main_reg1 &= ~0x100;
				*status_base_virt_addr = submodule_main_reg1;
				cmdbuf_obj->slice_run_done = 1;
				//Gate abnormal interrupt from VCMD so that to aviod repeatly BUF_EMPTY interrupts from decoder.
				vcmd_write_reg((const void *)dev->hwregs, VCMD_REGISTER_EXT_INT_GATE_OFFSET, 0xffffffff);

				irq_status = vcmd_read_reg((const void *)dev->hwregs,
						VCMD_REGISTER_INT_STATUS_OFFSET);
				if (!irq_status) {
					spin_unlock_irqrestore(dev->spinlock, flags);
					wake_up_interruptible_all(dev->wait_queue);
					wake_up_interruptible_all(&vcmd_mgr->mc_wait_queue);
					return IRQ_HANDLED;
				}
			} else {
				spin_unlock_irqrestore(dev->spinlock, flags);
				wake_up_interruptible_all(dev->wait_queue);
				wake_up_interruptible_all(&vcmd_mgr->mc_wait_queue);
				return IRQ_HANDLED;
			}
		}
	}
#ifdef VCMD_DEBUG_INTERNAL
	{
		u32 i, fordebug;

		for (i = 0; i < ASIC_VCMD_SWREG_AMOUNT; i++) {
			fordebug =
				vcmd_read_reg((const void *)dev->hwregs, i * 4);
			//gf_debug("vcmd register %d:0x%x\n", i,
//				fordebug);
		}
	}
#endif

	if (!irq_status) {
		//gf_debug("hantrovcmd_isr error,
		//irq_status :0x%x",irq_status);
		spin_unlock_irqrestore(dev->spinlock, flags);
		wake_up_interruptible_all(dev->wait_queue);
                wake_up_interruptible_all(&vcmd_mgr->mc_wait_queue);
		return IRQ_HANDLED;
	}

	gf_debug("irq_status of %d is: 0x%x\n", dev->core_id, irq_status);
	dev->reg_mirror[VCMD_REGISTER_INT_STATUS_OFFSET / 4] = irq_status;

	if ((dev->hw_version_id > HW_ID_1_0_C) && (irq_status & 0x3f)) {
		//if error,read from register directly.
		cmdbuf_id =
			vcmd_get_register_value((const void *)dev->hwregs,
						dev->reg_mirror,
						HWIF_VCMD_CMDBUF_EXECUTING_ID);
		if (cmdbuf_id >= TOTAL_DISCRETE_CMDBUF_NUM) {
			gf_error("hantrovcmd_isr error cmdbuf_id greater than the ceiling !!\n");
			spin_unlock_irqrestore(dev->spinlock, flags);
			wake_up_interruptible_all(dev->wait_queue);
			wake_up_interruptible_all(&vcmd_mgr->mc_wait_queue);
			return IRQ_HANDLED;
		}
	} else if (dev->hw_version_id > HW_ID_1_0_C) {
		//read cmdbuf id from ddr
#ifdef VCMD_DEBUG_INTERNAL
		{
			u32 i, fordebug;

			gf_debug("ddr vcmd register phy_addr=0x%llx\n",
				(unsigned long long)dev->vcmd_reg_mem_bus_address);
			gf_debug("ddr vcmd register virt_addr=0x%llx\n",
				(unsigned long long)dev->vcmd_reg_mem_virtual_address);
			for (i = 0; i < ASIC_VCMD_SWREG_AMOUNT; i++) {
				fordebug = *(dev->vcmd_reg_mem_virtual_address +
						 i);
				gf_debug("ddr vcmd register %d:0x%x\n",
					i, fordebug);
			}
		}
#endif

		//cmdbuf_id = *(dev->vcmd_reg_mem_virtual_address +
		//		  EXECUTING_CMDBUF_ID_ADDR);
		if (is_internal) {
			cmdbuf_id = *(dev->vcmd_reg_mem_virtual_address +
					  EXECUTING_CMDBUF_ID_ADDR);
		} else {
			spin_unlock_irqrestore(dev->spinlock, flags);
			return IRQ_HANDLED;
		}

		if (cmdbuf_id >= TOTAL_DISCRETE_CMDBUF_NUM) {
			gf_error("hantrovcmd_isr error cmdbuf_id greater than the ceiling !!\n");
			spin_unlock_irqrestore(dev->spinlock, flags);
			wake_up_interruptible_all(dev->wait_queue);
			wake_up_interruptible_all(&vcmd_mgr->mc_wait_queue);
			return IRQ_HANDLED;
		}
	}

	if (dev->hw_version_id < HW_ID_1_1_1) {
		if (vcmd_get_register_mirror_value(dev->reg_mirror,
						   HWIF_VCMD_IRQ_RESET)) {
			//reset error,all cmdbuf that is
			//not done will be run again.
			new_cmdbuf_node = dev->list_manager.head;
			dev->working_state = WORKING_STATE_IDLE;
			//find the first run_done=0
			while (1) {
				if (!new_cmdbuf_node)
					break;
				cmdbuf_obj = (struct cmdbuf_obj *)
							 new_cmdbuf_node->data;
				if ((cmdbuf_obj->cmdbuf_run_done == 0))
					break;
				new_cmdbuf_node = new_cmdbuf_node->next;
			}
			base_cmdbuf_node = new_cmdbuf_node;
			vcmd_delink_cmdbuf(dev, base_cmdbuf_node);
			vcmd_link_cmdbuf(dev, base_cmdbuf_node);
			if (dev->sw_cmdbuf_rdy_num != 0)
				//restart new command
				vcmd_start(dev, base_cmdbuf_node);
			handled++;
			spin_unlock_irqrestore(dev->spinlock, flags);
			wake_up_interruptible_all(dev->wait_queue);
			wake_up_interruptible_all(&vcmd_mgr->mc_wait_queue);
			return IRQ_HANDLED;
		}
	}

	if (vcmd_get_register_mirror_value(dev->reg_mirror,
					   HWIF_VCMD_IRQ_ABORT)) {
		gf_error("dec %d abort\n", irq);
		//abort error,don't need to reset
		new_cmdbuf_node = dev->list_manager.head;
		dev->working_state = WORKING_STATE_IDLE;
		if (dev->hw_version_id > HW_ID_1_0_C) {
			new_cmdbuf_node = vcmd_mgr->global_cmdbuf_node[cmdbuf_id];
			if (!new_cmdbuf_node) {
				gf_error("hantrovcmd_isr ABORT error cmdbuf_id.\n");
				spin_unlock_irqrestore(dev->spinlock, flags);
				wake_up_interruptible_all(dev->wait_queue);
				wake_up_interruptible_all(&vcmd_mgr->mc_wait_queue);
				return IRQ_HANDLED;
			}
		} else {
			exe_cmdbuf_bus_address = VCMDGetAddrRegisterValue(
				(const void *)dev->hwregs, dev->reg_mirror,
				HWIF_VCMD_EXECUTING_CMD_ADDR);
			//find the cmdbuf that triggers ABORT
			while (1) {
				if (!new_cmdbuf_node) {
					spin_unlock_irqrestore(dev->spinlock,
								   flags);
					gf_error("new_cmdbuf_node null\n");
					wake_up_interruptible_all(dev->wait_queue);
					wake_up_interruptible_all(&vcmd_mgr->mc_wait_queue);
					return IRQ_HANDLED;
				}
				cmdbuf_obj = (struct cmdbuf_obj *)
							 new_cmdbuf_node->data;
				if ((((cmdbuf_obj->cmdbuf_bus_address -
					   vcmd_mgr->base_ddr_addr) <=
					  exe_cmdbuf_bus_address) &&
					 (((cmdbuf_obj->cmdbuf_bus_address -
					vcmd_mgr->base_ddr_addr +
					cmdbuf_obj->cmdbuf_size) >
					exe_cmdbuf_bus_address))) &&
					(cmdbuf_obj->cmdbuf_run_done == 0))
					break;
				new_cmdbuf_node = new_cmdbuf_node->next;
			}
		}
		base_cmdbuf_node = new_cmdbuf_node;
		// this cmdbuf and cmdbufs prior to itself, run_done = 1
		while (1) {
			if (!new_cmdbuf_node)
				break;
			cmdbuf_obj = (struct cmdbuf_obj *)new_cmdbuf_node->data;
			if ((cmdbuf_obj->cmdbuf_run_done == 0)) {
				cmdbuf_obj->cmdbuf_run_done = 1;
				cmdbuf_obj->executing_status =
					CMDBUF_EXE_STATUS_OK;
				cmdbuf_processed_num++;
			} else
				break;
			new_cmdbuf_node = new_cmdbuf_node->previous;
		}
		base_cmdbuf_node = base_cmdbuf_node->next;
		vcmd_delink_cmdbuf(dev, base_cmdbuf_node);
		spin_unlock_irqrestore(dev->spinlock, flags);
		if (cmdbuf_processed_num)
			wake_up_interruptible_all(dev->wait_queue);
		//to let high priority cmdbuf be inserted
		wake_up_interruptible_all(dev->wait_abort_queue);
		wake_up_interruptible_all(&vcmd_mgr->mc_wait_queue);
		handled++;
		return IRQ_HANDLED;
	}
	if (vcmd_get_register_mirror_value(dev->reg_mirror,
					   HWIF_VCMD_IRQ_BUSERR)) {
		gf_error("dec %d bus error\n", irq);
		//bus error, don't need to reset where to record status?
		new_cmdbuf_node = dev->list_manager.head;
		dev->working_state = WORKING_STATE_IDLE;//TODO ==> not need
		if (dev->hw_version_id > HW_ID_1_0_C) {
			new_cmdbuf_node = vcmd_mgr->global_cmdbuf_node[cmdbuf_id];
			if (!new_cmdbuf_node) {
				gf_error("hantrovcmd_isr error cmdbuf_id !!\n");
				spin_unlock_irqrestore(dev->spinlock, flags);
				wake_up_interruptible_all(dev->wait_abort_queue);
				wake_up_interruptible_all(&vcmd_mgr->mc_wait_queue);
				return IRQ_HANDLED;
			}
		} else {
			exe_cmdbuf_bus_address = VCMDGetAddrRegisterValue(
				(const void *)dev->hwregs, dev->reg_mirror,
				HWIF_VCMD_EXECUTING_CMD_ADDR);
			//find the buserr cmdbuf
			while (1) {
				if (!new_cmdbuf_node) {
					spin_unlock_irqrestore(dev->spinlock,
								   flags);
					gf_error("00..\n");
					wake_up_interruptible_all(dev->wait_abort_queue);
					wake_up_interruptible_all(&vcmd_mgr->mc_wait_queue);
					return IRQ_HANDLED;
				}
				cmdbuf_obj = (struct cmdbuf_obj *)
							 new_cmdbuf_node->data;
				if ((((cmdbuf_obj->cmdbuf_bus_address -
					   vcmd_mgr->base_ddr_addr) <=
					  exe_cmdbuf_bus_address) &&
					 (((cmdbuf_obj->cmdbuf_bus_address -
					vcmd_mgr->base_ddr_addr +
					cmdbuf_obj->cmdbuf_size) >
					   exe_cmdbuf_bus_address))) &&
					(cmdbuf_obj->cmdbuf_run_done == 0))
					break;
				new_cmdbuf_node = new_cmdbuf_node->next;
			}
		}
		base_cmdbuf_node = new_cmdbuf_node;
		// this cmdbuf and cmdbufs prior to itself, run_done = 1
		while (1) {
			if (!new_cmdbuf_node)
				break;
			cmdbuf_obj = (struct cmdbuf_obj *)new_cmdbuf_node->data;
			if ((cmdbuf_obj->cmdbuf_run_done == 0)) {
				cmdbuf_obj->cmdbuf_run_done = 1;
				cmdbuf_obj->executing_status =
					CMDBUF_EXE_STATUS_OK;
				cmdbuf_processed_num++;
			} else
				break;
			new_cmdbuf_node = new_cmdbuf_node->previous;
		}
		new_cmdbuf_node = base_cmdbuf_node;
		if (new_cmdbuf_node) {
			cmdbuf_obj = (struct cmdbuf_obj *)new_cmdbuf_node->data;
			cmdbuf_obj->executing_status = CMDBUF_EXE_STATUS_BUSERR;
		}
		base_cmdbuf_node = base_cmdbuf_node->next;
		vcmd_delink_cmdbuf(dev, base_cmdbuf_node);
		vcmd_link_cmdbuf(dev, base_cmdbuf_node);
		if (dev->sw_cmdbuf_rdy_num != 0) {
			//restart new command
			vcmd_start(dev, base_cmdbuf_node);
		}
		spin_unlock_irqrestore(dev->spinlock, flags);
		if (cmdbuf_processed_num)
			wake_up_interruptible_all(dev->wait_queue);
		handled++;
		wake_up_interruptible_all(&vcmd_mgr->mc_wait_queue);
		return IRQ_HANDLED;
	}
	if (vcmd_get_register_mirror_value(dev->reg_mirror,
					   HWIF_VCMD_IRQ_TIMEOUT)) {
		gf_error("dec %d timeout\n", irq);
		//time out,need to reset
		new_cmdbuf_node = dev->list_manager.head;
		dev->working_state = WORKING_STATE_IDLE;
		if (dev->hw_version_id > HW_ID_1_0_C) {
			new_cmdbuf_node = vcmd_mgr->global_cmdbuf_node[cmdbuf_id];
			if (!new_cmdbuf_node) {
				gf_error("hantrovcmd_isr error cmdbuf_id !!\n");
				spin_unlock_irqrestore(dev->spinlock, flags);
				wake_up_interruptible_all(dev->wait_queue);
				wake_up_interruptible_all(&vcmd_mgr->mc_wait_queue);
				return IRQ_HANDLED;
			}
		} else {
			exe_cmdbuf_bus_address = VCMDGetAddrRegisterValue(
				(const void *)dev->hwregs, dev->reg_mirror,
				HWIF_VCMD_EXECUTING_CMD_ADDR);
			/* find the timeout cmdbuf */
			while (1) {
				if (!new_cmdbuf_node) {
					spin_unlock_irqrestore(dev->spinlock,
								   flags);
					wake_up_interruptible_all(dev->wait_queue);
					wake_up_interruptible_all(&vcmd_mgr->mc_wait_queue);
					return IRQ_HANDLED;
				}
				cmdbuf_obj = (struct cmdbuf_obj *)
							 new_cmdbuf_node->data;
				if ((((cmdbuf_obj->cmdbuf_bus_address -
					vcmd_mgr->base_ddr_addr) <=
					exe_cmdbuf_bus_address) &&
					(((cmdbuf_obj->cmdbuf_bus_address -
					vcmd_mgr->base_ddr_addr +
					cmdbuf_obj->cmdbuf_size) >
					exe_cmdbuf_bus_address))) &&
					(cmdbuf_obj->cmdbuf_run_done == 0))
					break;
				new_cmdbuf_node = new_cmdbuf_node->next;
			}
		}
		base_cmdbuf_node = new_cmdbuf_node;
		new_cmdbuf_node = new_cmdbuf_node->previous;
		// this cmdbuf and cmdbufs prior to itself, run_done = 1
		while (1) {
			if (!new_cmdbuf_node)
				break;
			cmdbuf_obj = (struct cmdbuf_obj *)new_cmdbuf_node->data;
			if (cmdbuf_obj->cmdbuf_run_done == 0) {
				cmdbuf_obj->cmdbuf_run_done = 1;
				cmdbuf_obj->executing_status =
					CMDBUF_EXE_STATUS_OK;
				cmdbuf_processed_num++;
			} else
				break;
			new_cmdbuf_node = new_cmdbuf_node->previous;
		}
		vcmd_delink_cmdbuf(dev, base_cmdbuf_node);
		vcmd_link_cmdbuf(dev, base_cmdbuf_node);
		if (dev->sw_cmdbuf_rdy_num != 0) {
			//reset
			vcmd_reset_current_asic(dev);
			//restart new command
			vcmd_start(dev, base_cmdbuf_node);
		}
		spin_unlock_irqrestore(dev->spinlock, flags);
		if (cmdbuf_processed_num)
			wake_up_interruptible_all(dev->wait_queue);
		handled++;
		wake_up_interruptible_all(&vcmd_mgr->mc_wait_queue);
		return IRQ_HANDLED;
	}
	if (vcmd_get_register_mirror_value(dev->reg_mirror,
					   HWIF_VCMD_IRQ_CMDERR)) {
		gf_error("dev %d command error\n", irq);
		//command error,don't need to reset
		new_cmdbuf_node = dev->list_manager.head;
		dev->working_state = WORKING_STATE_IDLE;
		if (dev->hw_version_id > HW_ID_1_0_C) {
			new_cmdbuf_node = vcmd_mgr->global_cmdbuf_node[cmdbuf_id];
			if (!new_cmdbuf_node) {
				gf_error("hantrovcmd_isr error cmdbuf_id !!\n");
				wake_up_interruptible_all(dev->wait_queue);
				wake_up_interruptible_all(&vcmd_mgr->mc_wait_queue);
				spin_unlock_irqrestore(dev->spinlock, flags);
				return IRQ_HANDLED;
			}
		} else {
			exe_cmdbuf_bus_address = VCMDGetAddrRegisterValue(
				(const void *)dev->hwregs, dev->reg_mirror,
				HWIF_VCMD_EXECUTING_CMD_ADDR);
			//find the cmderror cmdbuf
			while (1) {
				if (!new_cmdbuf_node) {
					spin_unlock_irqrestore(dev->spinlock,
								   flags);
					wake_up_interruptible_all(dev->wait_queue);
					wake_up_interruptible_all(&vcmd_mgr->mc_wait_queue);
					return IRQ_HANDLED;
				}
				cmdbuf_obj = (struct cmdbuf_obj *)
							 new_cmdbuf_node->data;
				if ((((cmdbuf_obj->cmdbuf_bus_address -
					   vcmd_mgr->base_ddr_addr) <=
					  exe_cmdbuf_bus_address) &&
					 (((cmdbuf_obj->cmdbuf_bus_address -
					vcmd_mgr->base_ddr_addr +
					cmdbuf_obj->cmdbuf_size) >
					   exe_cmdbuf_bus_address))) &&
					(cmdbuf_obj->cmdbuf_run_done == 0))
					break;
				new_cmdbuf_node = new_cmdbuf_node->next;
			}
		}
		base_cmdbuf_node = new_cmdbuf_node;
		// this cmdbuf and cmdbufs prior to itself, run_done = 1
		while (1) {
			if (!new_cmdbuf_node)
				break;
			cmdbuf_obj = (struct cmdbuf_obj *)new_cmdbuf_node->data;
			if (cmdbuf_obj->cmdbuf_run_done == 0) {
				cmdbuf_obj->cmdbuf_run_done = 1;
				cmdbuf_obj->executing_status =
					CMDBUF_EXE_STATUS_OK;
				cmdbuf_processed_num++;
			} else {
				break;
			}
			new_cmdbuf_node = new_cmdbuf_node->previous;
		}
		new_cmdbuf_node = base_cmdbuf_node;
		if (new_cmdbuf_node) {
			cmdbuf_obj = (struct cmdbuf_obj *)new_cmdbuf_node->data;
			cmdbuf_obj->executing_status =
				CMDBUF_EXE_STATUS_CMDERR; //cmderr
		}
		base_cmdbuf_node = base_cmdbuf_node->next;
		vcmd_delink_cmdbuf(dev, base_cmdbuf_node);
		vcmd_link_cmdbuf(dev, base_cmdbuf_node);
		if (dev->sw_cmdbuf_rdy_num != 0) {
			//restart new command
			vcmd_start(dev, base_cmdbuf_node);
		}
		spin_unlock_irqrestore(dev->spinlock, flags);
		if (cmdbuf_processed_num)
			wake_up_interruptible_all(dev->wait_queue);
		handled++;
		wake_up_interruptible_all(&vcmd_mgr->mc_wait_queue);
		return IRQ_HANDLED;
	}
	if (vcmd_get_register_mirror_value(dev->reg_mirror,
					   HWIF_VCMD_IRQ_ENDCMD)) {
		//end command interrupt
		new_cmdbuf_node = dev->list_manager.head;
		dev->working_state = WORKING_STATE_IDLE;
		if (dev->hw_version_id > HW_ID_1_0_C) {
			new_cmdbuf_node = vcmd_mgr->global_cmdbuf_node[cmdbuf_id];
			if (!new_cmdbuf_node) {
				gf_error("hantrovcmd_isr error cmdbuf_id... !!\n");
				spin_unlock_irqrestore(dev->spinlock, flags);
				wake_up_interruptible_all(dev->wait_queue);
				wake_up_interruptible_all(&vcmd_mgr->mc_wait_queue);
				return IRQ_HANDLED;
			}
		} else {
			//find the end cmdbuf
			while (1) {
				if (!new_cmdbuf_node) {
					spin_unlock_irqrestore(dev->spinlock,
								   flags);
					wake_up_interruptible_all(dev->wait_queue);
					wake_up_interruptible_all(&vcmd_mgr->mc_wait_queue);
					return IRQ_HANDLED;
				}
				cmdbuf_obj = (struct cmdbuf_obj *)
							 new_cmdbuf_node->data;
				if ((cmdbuf_obj->has_end_cmdbuf == 1) &&
					(cmdbuf_obj->cmdbuf_run_done == 0))
					break;
				new_cmdbuf_node = new_cmdbuf_node->next;
			}
		}
		base_cmdbuf_node = new_cmdbuf_node;
		// this cmdbuf and cmdbufs prior to itself, run_done = 1
		while (1) {
			if (!new_cmdbuf_node)
				break;
			cmdbuf_obj = (struct cmdbuf_obj *)new_cmdbuf_node->data;
			if ((cmdbuf_obj->cmdbuf_run_done == 0)) {
				cmdbuf_obj->cmdbuf_run_done = 1;
				cmdbuf_obj->executing_status =
					CMDBUF_EXE_STATUS_OK;
				cmdbuf_processed_num++;
			} else
				break;
			new_cmdbuf_node = new_cmdbuf_node->previous;
		}
		base_cmdbuf_node = base_cmdbuf_node->next;
		vcmd_delink_cmdbuf(dev, base_cmdbuf_node);
		vcmd_link_cmdbuf(dev, base_cmdbuf_node);
		if (dev->sw_cmdbuf_rdy_num != 0) {
			//restart new command
			vcmd_start(dev, base_cmdbuf_node);
		}
		spin_unlock_irqrestore(dev->spinlock, flags);
		if (cmdbuf_processed_num)
			wake_up_interruptible_all(dev->wait_queue);
		handled++;
		wake_up_interruptible_all(&vcmd_mgr->mc_wait_queue);
		return IRQ_HANDLED;
	}
	if (vcmd_get_register_mirror_value(dev->reg_mirror,
					HWIF_VCMD_IRQ_RESET)) {
		gf_error("dec %d arbiter error\n", irq);
		spin_unlock_irqrestore(dev->spinlock, flags);
		wake_up_interruptible_all(dev->wait_queue);
		handled++;
		wake_up_interruptible_all(&vcmd_mgr->mc_wait_queue);
		return IRQ_HANDLED;
	}
	if (dev->hw_version_id <= HW_ID_1_0_C)
		cmdbuf_id = vcmd_get_register_mirror_value(dev->reg_mirror,
							   HWIF_VCMD_IRQ_INTCMD);
	if (cmdbuf_id) {
		if (dev->hw_version_id <= HW_ID_1_0_C) {
			if (cmdbuf_id >= TOTAL_DISCRETE_CMDBUF_NUM) {
				gf_error("hantrovcmd_isr error cmdbuf_id greater than the ceiling !!\n");
				spin_unlock_irqrestore(dev->spinlock, flags);
				wake_up_interruptible_all(dev->wait_queue);
				wake_up_interruptible_all(&vcmd_mgr->mc_wait_queue);
				return IRQ_HANDLED;
			}
		}
		new_cmdbuf_node = vcmd_mgr->global_cmdbuf_node[cmdbuf_id];
		if (!new_cmdbuf_node) {
			gf_error("hantrovcmd_isr error cmdbuf_id !!!!!\n");
			spin_unlock_irqrestore(dev->spinlock, flags);
			wake_up_interruptible_all(dev->wait_queue);
			wake_up_interruptible_all(&vcmd_mgr->mc_wait_queue);
			return IRQ_HANDLED;
		}
		// interrupt cmdbuf and cmdbufs prior to itself, run_done = 1
		while (1) {
			if (!new_cmdbuf_node)
				break;
			cmdbuf_obj = (struct cmdbuf_obj *)new_cmdbuf_node->data;
			if ((cmdbuf_obj->cmdbuf_run_done == 0)) {
				cmdbuf_obj->cmdbuf_run_done = 1;
#ifdef SUPPORT_DBGFS
				debugfs_remove(root_cmdbuf[cmdbuf_obj->cmdbuf_id]);
				root_cmdbuf[cmdbuf_obj->cmdbuf_id] = NULL;
#endif
				cmdbuf_obj->executing_status =
					CMDBUF_EXE_STATUS_OK;
				cmdbuf_processed_num++;
#ifdef SUPPORT_DBGFS
				num_cmdbuf_exe[dev->core_id][e_index[dev->core_id]] = cmdbuf_processed_num;
				e_index[dev->core_id]++;
				if (e_index[dev->core_id] == MAX_RESERVED_TIME)
					e_index[dev->core_id] = 0;
				status_base_virt_addr = vcmd_mgr->vcmd_status_buf_mem_pool.virtual_address + cmdbuf_id * CMDBUF_MAX_SIZE / 4 +
				(vcmd_mgr->vcmd_manager[cmdbuf_obj->module_type][0]->vcmd_core_cfg.submodule_main_addr / 2 / 4 + 0);
				u64 time_num;

				if (*(status_base_virt_addr + 1)) {
#if (KERNEL_VERSION(4, 19, 94) > LINUX_VERSION_CODE)
					struct timeval time_now;

					do_gettimeofday(&time_now);
					time_num = time_now.tv_sec * 1000 + time_now.tv_usec / 1000;
#else
					struct timespec64 time_now;

					ktime_get_real_ts64(&time_now);
					time_num = time_now.tv_sec * 1000 + time_now.tv_nsec / 1000000;
#endif
					interrupt_time[dev->core_id][i_index[dev->core_id]] = time_num;
					decode_cycles[dev->core_id][cycle_index[dev->core_id]] += *(status_base_virt_addr + 5);
		}
				if (has_apb_arbiter) {
					if (*(status_base_virt_addr + 1))
						vcmd_mgr->vcd_cycles += *(status_base_virt_addr + 5);
				}
#endif
			} else {
				break;
			}
			new_cmdbuf_node = new_cmdbuf_node->previous;
		}
		handled++;
#ifdef SUPPORT_DBGFS
		i_index[dev->core_id]++;
		if (i_index[dev->core_id] == MAX_RESERVED_TIME)
			i_index[dev->core_id] = 0;
		cycle_index[dev->core_id]++;
		if (cycle_index[dev->core_id] == MAX_RESERVED_TIME)
			cycle_index[dev->core_id] = 0;
#endif
	}
	//Gate abnormal interrupt from VCMD so that to aviod repeatly BUF_EMPTY interrupts from decoder.
	if (dev->hw_version_id >= HW_ID_1_5_0) {
		vcmd_write_reg((const void *)dev->hwregs, 0x64,
				       0xffffffff); //not interrupt cpu
	}
	spin_unlock_irqrestore(dev->spinlock, flags);
	if (cmdbuf_processed_num) {
		wake_up_interruptible_all(dev->wait_queue);
	} else {
		gf_error("no cmd buffer to wake\n");
	}
	if (!handled) {
		gf_debug("IRQ received, but not hantro's!\n");
	}

	wake_up_interruptible_all(&vcmd_mgr->mc_wait_queue);
	return IRQ_HANDLED;
}

void vcmd_reset_asic(void *pvcmd_mgr)
{
	int i, n;
	u32 result;
	vcmd_mgr_t *vcmd_mgr = (vcmd_mgr_t *)pvcmd_mgr;
	struct hantrovcmd_dev *dev = vcmd_mgr->hantrovcmd_data;

	for (n = 0; n < vcmd_mgr->total_vcmd_core_num; n++) {
		if (dev[n].hwregs) {
			//disable interrupt at first
			vcmd_write_reg((const void *)dev[n].hwregs,
					   VCMD_REGISTER_INT_CTL_OFFSET, 0x0000);
			//reset core
			vcmd_write_reg((const void *)dev[n].hwregs,
					   VCMD_REGISTER_CONTROL_OFFSET, 0x0004);
			//read status register
			result = vcmd_read_reg((const void *)dev[n].hwregs,
						   VCMD_REGISTER_INT_STATUS_OFFSET);
			//clean status register
			vcmd_write_reg((const void *)dev[n].hwregs,
					   VCMD_REGISTER_INT_STATUS_OFFSET, result);
			//when reset core need clear reg[3]
			vcmd_write_reg((const void *)dev[n].hwregs,
							VCMD_REGISTER_EXE_CMDBUF_COUNT_OFFSET, 0x0000);
			for (i = VCMD_REGISTER_CONTROL_OFFSET;
				 i < dev[n].vcmd_core_cfg.vcmd_iosize; i += 4) {
				//set all register 0
				vcmd_write_reg((const void *)dev[n].hwregs, i,
						   0x0000);
			}
			//enable all interrupt
			vcmd_write_reg((const void *)dev[n].hwregs,
					   VCMD_REGISTER_INT_CTL_OFFSET,
					   0xffffffff);
			// gate all external interrupts
			vcmd_write_reg((const void *)dev[n].hwregs,
					   VCMD_REGISTER_EXT_INT_GATE_OFFSET,
					   0xffffffff);

			vcmd_write_reg((const void *)dev[n].hwregs, 
						VCMD_REGISTER_ARB_OFFSET, 
						0x0);
			vcmd_write_reg((const void *)dev[n].hwregs,
						VCMD_REGISTER_ARBITER_CONFIG_OFFSET,
						VCMD_ARBITER_PARAMS);
		}
	}
}

static void vcmd_reset_current_asic(struct hantrovcmd_dev *dev)
{
	u32 result;

	if (dev->hwregs) {
		//disable interrupt at first
		vcmd_write_reg((const void *)dev->hwregs,
				   VCMD_REGISTER_INT_CTL_OFFSET, 0x0000);
		//reset core
		vcmd_write_reg((const void *)dev->hwregs,
				   VCMD_REGISTER_CONTROL_OFFSET, 0x0004);
		//read status register
		result = vcmd_read_reg((const void *)dev->hwregs,
					   VCMD_REGISTER_INT_STATUS_OFFSET);
		//clean status register
		vcmd_write_reg((const void *)dev->hwregs,
				   VCMD_REGISTER_INT_STATUS_OFFSET, result);
		//when reset core need clear reg[3]
		vcmd_write_reg((const void *)dev->hwregs,
						VCMD_REGISTER_EXE_CMDBUF_COUNT_OFFSET, 0x0000);
	}
}

static void printk_vcmd_register_debug(const void *hwregs, char *info)
{
#ifdef HANTRO_VCMD_DRIVER_DEBUG
	u32 i, fordebug;

	for (i = 0; i < ASIC_VCMD_SWREG_AMOUNT; i++) {
		fordebug = vcmd_read_reg((const void *)hwregs, i * 4);
		gf_debug("%s vcmd register %d:0x%x\n", info, i,
			fordebug);
	}
#endif
}

void lisuan_dec_vcmd_clear_irq_status(int irq, void *dev_id)
{
	vcmd_mgr_t *vcmd_mgr = (vcmd_mgr_t *)dev_id;
	struct hantrovcmd_dev *dev = &vcmd_mgr->hantrovcmd_data[irq];
	u32 irq_status = 0;

	irq_status = vcmd_read_reg((const void *)dev->hwregs, VCMD_REGISTER_INT_STATUS_OFFSET);
	vcmd_write_reg((const void *)dev->hwregs, VCMD_REGISTER_INT_STATUS_OFFSET, irq_status);

	dev->irq_status = irq_status;
}

void lisuan_dec0_vcmd_worker(struct work_struct *work)
{
	struct lisuan_device *ls_dev = container_of(work, struct lisuan_device, dec0_vcmd_work);

	hantrovcmd_isr(0, ls_dev->vcmd_mgr, 0);
}

void lisuan_dec1_vcmd_worker(struct work_struct *work)
{
	struct lisuan_device *ls_dev = container_of(work, struct lisuan_device, dec1_vcmd_work);

	hantrovcmd_isr(1, ls_dev->vcmd_mgr, 0);
}
