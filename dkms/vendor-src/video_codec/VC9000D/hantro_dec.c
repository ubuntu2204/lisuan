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

#include "hantrodec.h"
#include "vcmdswhwregisters.h"
#include "dwl_defs.h"
#include "subsys.h"
#include "hantroaxife.h"
#include "lisuan_drm.h"
#include "lisuan_device.h"

static unsigned long long gBaseHdwr;
unsigned long long gBaseDDRHw; /* PCI base register address (memalloc) */
static u32 gBaseLen; /* Base register address Length */
//#endif

/* hantro VCD reg config */
#define HANTRO_VCD_REGS             MAX_REG_COUNT /*VCD total regs*/
#define HANTRO_VCD_FIRST_REG        0
#define HANTRO_VCD_LAST_REG         (HANTRO_VCD_REGS - 1)
#define HANTRODEC_HWBUILD_ID_OFF        (309 * 4)

/* Logic module IRQs */
#define HXDEC_NO_IRQ                    -1

#ifndef MAX
#define MAX(a, b)                       (((a) > (b)) ? (a) : (b))
#endif

#define DEC_IO_SIZE_MAX                 (HANTRO_VCD_REGS * 4)

/* User should modify these configuration if do porting to own platform. */
/* Please guarantee the base_addr, io_size, dec_irq belong to same core. */

/* Defines use kernel clk cfg or not**/
//#define CLK_CFG
#ifdef CLK_CFG
/*this id should conform with platform define*/
#define CLK_ID   "hantrodec_clk"
#endif

/* Logic module base address */

#define DEC_IO_SIZE_0                   DEC_IO_SIZE_MAX /* bytes */
#define DEC_IO_SIZE_1                   DEC_IO_SIZE_MAX /* bytes */

#define DEC_IRQ_0                       HXDEC_NO_IRQ
#define DEC_IRQ_1                       HXDEC_NO_IRQ

#define IS_G1(hw_id)                    (((hw_id) == 0x6731) ? 1 : 0)
#define IS_G2(hw_id)                    (((hw_id) == 0x6732) ? 1 : 0)
#define IS_VCD(hw_id)               (((hw_id) == 0x9001) ? 1 : 0)
#define IS_BIGOCEAN(hw_id)              (((hw_id) == 0xB16D) ? 1 : 0)

/* Some IPs HW configuration parameters for APB Filter */
/* Because now such information can't be
 * read from APB filter configuration registers
 */
/* The fixed value have to be used */
#define VCD_NUM_MASK_REG            336
#define VCD_NUM_MODE                4
#define VCD_MASK_REG_OFFSET         4096
#define VCD_MASK_BITS_PER_REG       1

#define VCDJ_NUM_MASK_REG           332
#define VCDJ_NUM_MODE               1
#define VCDJ_MASK_REG_OFFSET        4096
#define VCDJ_MASK_BITS_PER_REG      1

#define AV1_NUM_MASK_REG                303
#define AV1_NUM_MODE                    1
#define AV1_MASK_REG_OFFSET             4096
#define AV1_MASK_BITS_PER_REG           1

#define AXIFE_NUM_MASK_REG              144
#define AXIFE_NUM_MODE                  1
#define AXIFE_MASK_REG_OFFSET           4096
#define AXIFE_MASK_BITS_PER_REG         1

#define CORE_MASK_CLIENTTYPE_BITMAP 16

#define DRIVER_NAME "hantrodma"

/***********local variable declaration********/

static const int DecHwId[] = {
	0x6731, /* G1 */
	0x6732, /* G2 */
	0xB16D, /* BigOcean */
	0x9001 /* VCD */
};

static unsigned long long base_port = -1;
static unsigned int reg_access_opt;
static unsigned int vcmd;
unsigned long alloc_size = -1;
unsigned long long alloc_base = -1;
unsigned int  has_apb_arbiter;
char *dec_dev_n = "hantrodec";

#ifdef CLK_CFG
struct clk *clk_cfg;
int is_clk_on;
struct timer_list timer;
#endif

extern struct vcmd_config vcmd_core_array[MAX_SUBSYS_NUM];

#ifdef SUPPORT_DBGFS
/*debugfs for performance statistics*/
static struct dentry *debug_root;
static int N_reserved = 50;
#define MAX_RESERVED_TIME 256
#define IDLE 0
#define RESERVED 1
#define DECODING 2
#define DONE 3
//static char *subsys_name[MAX_SUBSYS_NUM] = {"reg_subsys0", "reg_subsys1", "reg_subsys2", "reg_subsys3"};
#endif

/* here's all the must remember stuff */
typedef struct {
	char *buffer;

	volatile unsigned int iosize[HXDEC_MAX_CORES];
	/* mapped address to different HW cores regs*/
	u8 *hwregs[HXDEC_MAX_CORES][HW_CORE_MAX];
	/* mapped address to different HW cores regs*/
	u8 *apbfilter_hwregs[HXDEC_MAX_CORES][HW_CORE_MAX];
	volatile int irq[HXDEC_MAX_CORES];
	int hw_id[HXDEC_MAX_CORES][HW_CORE_MAX];
	/* Requested client type for given core,
	 * used when a subsys has multiple
	 * decoders, e.g., VCD+VCDJ+BigOcean
	 */
	int cores;
	struct fasync_struct *async_queue_dec;
	struct fasync_struct *async_queue_pp;
} hantrodec_t;

struct core_cfg {
	u32 cfg[HXDEC_MAX_CORES];              /* indicate the supported format */
};

static struct SubsysMgr {
	struct platform_device *platformdev;
	/* for non-vcmd */
	unsigned long long multicorebase[HXDEC_MAX_CORES];
	int irq[HXDEC_MAX_CORES];
	int iosize[HXDEC_MAX_CORES];
	struct subsys_config vpu_subsys[MAX_SUBSYS_NUM];
	struct apbfilter_cfg apbfilter_cfg[MAX_SUBSYS_NUM][HW_CORE_MAX];
	struct axife_cfg axife_cfg[MAX_SUBSYS_NUM];
	int elements;
	hantrodec_t hantrodec_data; /* dynamic allocation? */
	int hantrodec_major; /* dynamic allocation */

	u32 dec_regs[HXDEC_MAX_CORES][DEC_IO_SIZE_MAX / 4];
	u32 apbfilter_regs[HXDEC_MAX_CORES][DEC_IO_SIZE_MAX / 4 + 1];
	/* shadow_regs used to compare whether it's necessary to write to registers */
	u32 shadow_dec_regs[HXDEC_MAX_CORES][DEC_IO_SIZE_MAX / 4];

	struct semaphore dec_core_sem;
	struct semaphore pp_core_sem;

	int dec_irq;
	int pp_irq;

	atomic_t irq_rx;
	atomic_t irq_tx;

	struct drm_file *dec_owner[HXDEC_MAX_CORES];
	struct drm_file *pp_owner[HXDEC_MAX_CORES];
	struct core_cfg config;

	spinlock_t owner_lock;

	wait_queue_head_t dec_wait_queue;
	wait_queue_head_t pp_wait_queue;
	wait_queue_head_t hw_queue;

	/* for VCMD */
	void *vcmd_mgr;   /* allocated in hantro_vcmd.c */
#ifdef SUPPORT_DBGFS
	/* for debugfs */
	unsigned long hw_return_time[HXDEC_MAX_CORES][MAX_RESERVED_TIME];
	unsigned long reserved_time[HXDEC_MAX_CORES][MAX_RESERVED_TIME];
	unsigned long start_hw_time[HXDEC_MAX_CORES][MAX_RESERVED_TIME];
	unsigned long release_time[HXDEC_MAX_CORES][MAX_RESERVED_TIME];
	unsigned long active_start_time[MAX_RESERVED_TIME];
	unsigned long active_return_time[MAX_RESERVED_TIME];
	int r_index[HXDEC_MAX_CORES], hw_r_index[HXDEC_MAX_CORES], start_hw_index[HXDEC_MAX_CORES], release_index[HXDEC_MAX_CORES];
	int active_hw_index;
	int decode_state[HXDEC_MAX_CORES];
#endif
} subsys_mgr;

static int ReserveIO(struct lisuan_device *gf_device);
static void ResetAsic(hantrodec_t *dev);

#ifdef HANTRODEC_DEBUG
static void dump_regs(hantrodec_t *dev);
#endif

static int CoreHasFormat(const u32 *cfg, int core, u32 format);

#ifdef SUPPORT_DBGFS
#if 0
static char *subsys_state_read(int i)
{
	if (subsys_mgr.decode_state[i] == RESERVED)
		return "reserved\n";
	else if (subsys_mgr.decode_state[i] ==  IDLE)
		return "idle\n";
	else if (subsys_mgr.decode_state[i] == DECODING)
		return "decoding\n";
	else
		return "done\n";
}
#endif

static ssize_t subsys_state(struct drm_file *file, char __user *user_buf, size_t count, loff_t *ppos)
{
	char *val, v[50];
	int ret, i;

	val = kzalloc(100, GFP_KERNEL);
	for (i = 0; i < subsys_mgr.hantrodec_data.cores; i++) {
		sprintf(v, "core[%d] work state\n", i);
		strcat(val, v);
		if (subsys_mgr.decode_state[i] == RESERVED)
			sprintf(v, "reserved\n");
		else if (subsys_mgr.decode_state[i] ==  IDLE)
			sprintf(v, "idle\n");
		else if (subsys_mgr.decode_state[i] == DECODING)
			sprintf(v, "decoding\n");
		else
			sprintf(v, "done\n");
		strcat(val, v);
	}
	ret = simple_read_from_buffer(user_buf, count, ppos, val, strlen(val));
	kfree(val);
	return ret;
}

const struct file_operations fileop_subsys_state = {
	.read = subsys_state,
};

static ssize_t Hw_Register_Print(struct drm_file *file, char __user *user_buf, size_t count, loff_t *ppos)
{
	char *v0, v1[30];
	int num_regs = 0, NUM_REGS = 0;
	int swreg_mes, j, i, k, ret = 0;
	hantrodec_t hantrodec_data;

	hantrodec_data = subsys_mgr.hantrodec_data;
	for (i = 0; i < subsys_mgr.hantrodec_data.cores; i++) {
		for (j = HW_VCD; j < HW_CORE_MAX - 1; j++)
			num_regs += subsys_mgr.vpu_subsys[i].submodule_iosize[j] / 4;
	}
	v0 = kzalloc(num_regs * 100, GFP_KERNEL);
	for (i = 0; i < subsys_mgr.hantrodec_data.cores; i++) {
		sprintf(v1, "core[%d]\n", i);
		strcat(v0, v1);
		for (k = HW_VCD; k < HW_CORE_MAX - 1; k++) {
			if (k == HW_VCD && hantrodec_data.hwregs[i][HW_VCD]) {
				sprintf(v1, "[VCD]\n");
				strcat(v0, v1);
				for (j = 0; j < MAX_REG_COUNT;) {
					swreg_mes = gf_read_mmio32((hantrodec_data.hwregs[i][HW_VCD] + j * 4));
					sprintf(v1, "%d: %08x ", j, swreg_mes);
					strcat(v0, v1);
					j++;
					if (j < MAX_REG_COUNT) {
						swreg_mes = gf_read_mmio32((hantrodec_data.hwregs[i][HW_VCD] + j * 4));
						sprintf(v1, "%08x ", swreg_mes);
						strcat(v0, v1);
						j++;
					}
					if (j < MAX_REG_COUNT) {
						swreg_mes = gf_read_mmio32((hantrodec_data.hwregs[i][HW_VCD] + j * 4));
						sprintf(v1, "%08x ", swreg_mes);
						strcat(v0, v1);
						j++;
					}
					if (j < MAX_REG_COUNT) {
						swreg_mes = gf_read_mmio32((hantrodec_data.hwregs[i][HW_VCD] + j * 4));
						sprintf(v1, "%08x\n", swreg_mes);
						strcat(v0, v1);
						j++;
					}
				}
			}
			if (k == HW_VCDJ && hantrodec_data.hwregs[i][HW_VCDJ]) {
				NUM_REGS = subsys_mgr.vpu_subsys[i].submodule_iosize[k] / 4;
				sprintf(v1, "[VCDJ]\n");
				strcat(v0, v1);
				for (j = 0; j < NUM_REGS;) {
					swreg_mes = gf_read_mmio32((hantrodec_data.hwregs[i][HW_VCDJ] + j * 4));
					sprintf(v1, "%3d: %08x ", j, swreg_mes);
					strcat(v0, v1);
					j++;
					if (j < NUM_REGS) {
						swreg_mes = gf_read_mmio32((hantrodec_data.hwregs[i][HW_VCDJ] + j * 4));
						sprintf(v1, "%08x ", swreg_mes);
						strcat(v0, v1);
					}
					j++;
					if (j < NUM_REGS) {
						swreg_mes = gf_read_mmio32((hantrodec_data.hwregs[i][HW_VCDJ] + j * 4));
						sprintf(v1, "%08x ", swreg_mes);
						strcat(v0, v1);
					}
					j++;
					if (j < NUM_REGS) {
						swreg_mes = gf_read_mmio32((hantrodec_data.hwregs[i][HW_VCDJ] + j * 4));
						sprintf(v1, "%08x\n", swreg_mes);
						strcat(v0, v1);
					}
					j++;
				}
			}
			if (k == HW_BIGOCEAN && hantrodec_data.hwregs[i][HW_BIGOCEAN]) {
				NUM_REGS = subsys_mgr.vpu_subsys[i].submodule_iosize[k] / 4;
				sprintf(v1, "\n[BIGOCEAN]\n");
				strcat(v0, v1);
				for (j = 0; j < NUM_REGS;) {
					swreg_mes = gf_read_mmio32((hantrodec_data.hwregs[i][HW_BIGOCEAN] + j * 4));
					sprintf(v1, "%3d: %08x ", j, swreg_mes);
					strcat(v0, v1);
					j++;
					if (j < NUM_REGS) {
						swreg_mes = gf_read_mmio32((hantrodec_data.hwregs[i][HW_BIGOCEAN] + j * 4));
						sprintf(v1, "%08x ", swreg_mes);
						strcat(v0, v1);
					}
					j++;
					if (j < NUM_REGS) {
						swreg_mes = gf_read_mmio32((hantrodec_data.hwregs[i][HW_BIGOCEAN] + j * 4));
						sprintf(v1, "%08x ", swreg_mes);
						strcat(v0, v1);
					}
					j++;
					if (j < NUM_REGS) {
						swreg_mes = gf_read_mmio32((hantrodec_data.hwregs[i][HW_BIGOCEAN] + j * 4));
						sprintf(v1, "%08x\n", swreg_mes);
						strcat(v0, v1);
					}
					j++;
				}
			}
			if (k == HW_MMU && hantrodec_data.hwregs[i][HW_MMU]) {
				NUM_REGS = subsys_mgr.vpu_subsys[i].submodule_iosize[k] / 4;
				sprintf(v1, "\n[MMU]\n");
				strcat(v0, v1);
				for (j = 0; j < NUM_REGS;) {
					swreg_mes = gf_read_mmio32((hantrodec_data.hwregs[i][HW_MMU] + j * 4));
					sprintf(v1, "%d: %08x ", j, swreg_mes);
					strcat(v0, v1);
					j++;
					if (j < NUM_REGS) {
						swreg_mes = gf_read_mmio32((hantrodec_data.hwregs[i][HW_MMU] + j * 4));
						sprintf(v1, "%08x ", swreg_mes);
						strcat(v0, v1);
						j++;
					}
					if (j < NUM_REGS) {
						swreg_mes = gf_read_mmio32((hantrodec_data.hwregs[i][HW_MMU] + j * 4));
						sprintf(v1, "%08x ", swreg_mes);
						strcat(v0, v1);
						j++;
					}
					if (j < NUM_REGS) {
						swreg_mes = gf_read_mmio32((hantrodec_data.hwregs[i][HW_MMU] + j * 4));
						sprintf(v1, "%08x\n", swreg_mes);
						strcat(v0, v1);
						j++;
					}
				}
			}
			if (k == HW_MMU_WR && hantrodec_data.hwregs[i][HW_MMU_WR]) {
				NUM_REGS = subsys_mgr.vpu_subsys[i].submodule_iosize[k] / 4;
				sprintf(v1, "\n[MMU_WR]\n");
				strcat(v0, v1);
				for (j = 0; j < NUM_REGS;) {
					swreg_mes = gf_read_mmio32((hantrodec_data.hwregs[i][k] + j * 4));
					sprintf(v1, "%3d: %08x ", j, swreg_mes);
					strcat(v0, v1);
					j++;
					if (j < NUM_REGS) {
						swreg_mes = gf_read_mmio32((hantrodec_data.hwregs[i][k] + j * 4));
						sprintf(v1, "%08x ", swreg_mes);
						strcat(v0, v1);
					}
					j++;
					if (j < NUM_REGS) {
						swreg_mes = gf_read_mmio32((hantrodec_data.hwregs[i][k] + j * 4));
						sprintf(v1, "%08x ", swreg_mes);
						strcat(v0, v1);
					}
					j++;
					if (j < NUM_REGS) {
						swreg_mes = gf_read_mmio32((hantrodec_data.hwregs[i][k] + j * 4));
						sprintf(v1, "%08x\n", swreg_mes);
						strcat(v0, v1);
					}
					j++;
				}
			}
			if (k == HW_DEC400 && hantrodec_data.hwregs[i][HW_DEC400]) {
				NUM_REGS = subsys_mgr.vpu_subsys[i].submodule_iosize[k] / 4;
				sprintf(v1, "\n[DEC400]\n");
				strcat(v0, v1);
				for (j = 0; j < NUM_REGS;) {
					swreg_mes = gf_read_mmio32((hantrodec_data.hwregs[i][k] + j * 4));
					sprintf(v1, "%3d: %08x ", j, swreg_mes);
					strcat(v0, v1);
					j++;
					if (j < NUM_REGS) {
						swreg_mes = gf_read_mmio32((hantrodec_data.hwregs[i][k] + j * 4));
						sprintf(v1, "%08x ", swreg_mes);
						strcat(v0, v1);
						j++;
					}
					if (j < NUM_REGS) {
						swreg_mes = gf_read_mmio32((hantrodec_data.hwregs[i][k] + j * 4));
						sprintf(v1, "%08x ", swreg_mes);
						strcat(v0, v1);
						j++;
					}
					if (j < NUM_REGS) {
						swreg_mes = gf_read_mmio32((hantrodec_data.hwregs[i][k] + j * 4));
						sprintf(v1, "%08x\n", swreg_mes);
						strcat(v0, v1);
						j++;
					}
				}
			}
			if (k == HW_AXIFE && hantrodec_data.hwregs[i][HW_AXIFE]) {
				NUM_REGS = subsys_mgr.vpu_subsys[i].submodule_iosize[k] / 4;
				sprintf(v1, "\n[AXIFE]\n");
				strcat(v0, v1);
				for (j = 0; j < NUM_REGS;) {
					swreg_mes = gf_read_mmio32((hantrodec_data.hwregs[i][k] + j * 4));
					sprintf(v1, "%3d: %08x ", j, swreg_mes);
					strcat(v0, v1);
					j++;
					if (j < NUM_REGS) {
						swreg_mes = gf_read_mmio32((hantrodec_data.hwregs[i][k] + j * 4));
						sprintf(v1, "%08x ", swreg_mes);
						strcat(v0, v1);
					}
					j++;
					if (j < NUM_REGS) {
						swreg_mes = gf_read_mmio32((hantrodec_data.hwregs[i][k] + j * 4));
						sprintf(v1, "%08x ", swreg_mes);
						strcat(v0, v1);
					}
					j++;
					if (j < NUM_REGS) {
						swreg_mes = gf_read_mmio32((hantrodec_data.hwregs[i][k] + j * 4));
						sprintf(v1, "%08x\n", swreg_mes);
						strcat(v0, v1);
					}
					j++;
				}
			}
			if (k == HW_AFBC && hantrodec_data.hwregs[i][HW_AFBC]) {
				NUM_REGS = subsys_mgr.vpu_subsys[i].submodule_iosize[k] / 4;
				sprintf(v1, "\n[AFBC]\n");
				strcat(v0, v1);
				for (j = 0; j < NUM_REGS;) {
					swreg_mes = gf_read_mmio32((hantrodec_data.hwregs[i][k] + j * 4));
					sprintf(v1, "%3d: %08x ", j, swreg_mes);
					strcat(v0, v1);
					j++;
					if (j < NUM_REGS) {
						swreg_mes = gf_read_mmio32((hantrodec_data.hwregs[i][k] + j * 4));
						sprintf(v1, "%08x ", swreg_mes);
						strcat(v0, v1);
					}
					j++;
					if (j < NUM_REGS) {
						swreg_mes = gf_read_mmio32((hantrodec_data.hwregs[i][k] + j * 4));
						sprintf(v1, "%08x ", swreg_mes);
						strcat(v0, v1);
					}
					j++;
					if (j < NUM_REGS) {
						swreg_mes = gf_read_mmio32((hantrodec_data.hwregs[i][k] + j * 4));
						sprintf(v1, "%08x\n", swreg_mes);
						strcat(v0, v1);
					}
					j++;
				}
			}
		}
	}
	ret = simple_read_from_buffer(user_buf, count, ppos, v0, strlen(v0));
	kfree(v0);
	return ret;
}

const struct file_operations fileop_hw_reg_print = {
	.read = Hw_Register_Print,
};

static ssize_t hw_cycles(struct drm_file *file, char __user *user_buf, size_t count, loff_t *ppos)
{
	char *v0;
	char v1[60];
	u32 swreg_mes = 0;
	int i;
	int ret;

	v0 = kzalloc(subsys_mgr.hantrodec_data.cores * 40, GFP_KERNEL);
	for (i = 0; i < subsys_mgr.hantrodec_data.cores; i++) {
		if (subsys_mgr.hantrodec_data.hwregs[i][HW_VCD])
			swreg_mes = gf_read_mmio32((subsys_mgr.hantrodec_data.hwregs[i][HW_VCD] + 63 * 4));
		if (subsys_mgr.hantrodec_data.hwregs[i][HW_VCDJ])
			swreg_mes = gf_read_mmio32((subsys_mgr.hantrodec_data.hwregs[i][HW_VCDJ] + 63 * 4));
		sprintf(v1, "hw cycles(core[%d] swreg 63) = %u\n", i, swreg_mes);
		strcat(v0, v1);
	}
	ret = simple_read_from_buffer(user_buf, count, ppos, v0, strlen(v0));
	kfree(v0);
	return ret;
}

const struct file_operations fileop_cycles = {
	.read = hw_cycles,
};

static ssize_t perf_statistic_write(struct drm_file *file, const char __user *user_buf, size_t count, loff_t *ppos)
{
	char In_fo[10];
	int bytes_to_read = 4;

	simple_write_to_buffer(In_fo, count, ppos, user_buf, bytes_to_read);
	kstrtoint(In_fo, 10, &N_reserved);
	//sscanf(In_fo, "%d", &N_reserved);

	return count;
}

static ssize_t perf_statistic_read(struct drm_file *file, char __user *user_buf, size_t count, loff_t *ppos)
{
	char v[200], *t;
	int i, j, index, ret;
	u32 total_time_eachcore[HXDEC_MAX_CORES] = {0, 0,};
	u32 total_time = 0;

	t = kmalloc(N_reserved * 60 * subsys_mgr.hantrodec_data.cores, GFP_KERNEL);
	for (j = 0; j < subsys_mgr.hantrodec_data.cores; j++) {
		sprintf(v, "core[%d]\n", j);
		strcat(t, v);
		strcat(t, "[reserved time, enable time - reserved time, done_time - reserved time, release_time - reserved time, decode_done_time - decode_starting_time] /ms\n");
		index = subsys_mgr.r_index[j];
		if (index >= N_reserved) {
			for (i = 0; i < N_reserved; i++) {
				sprintf(v, "[%lu, ", subsys_mgr.reserved_time[j][i + index - N_reserved]);
				strcat(t, v);
				sprintf(v, "%lu, ", subsys_mgr.start_hw_time[j][i + index - N_reserved] - subsys_mgr.reserved_time[j][i + index - N_reserved]);
				strcat(t, v);
				sprintf(v, "%lu, ", subsys_mgr.hw_return_time[j][i + index - N_reserved] - subsys_mgr.reserved_time[j][i + index - N_reserved]);
				strcat(t, v);
				sprintf(v, "%lu, ", subsys_mgr.release_time[j][i + index - N_reserved] - subsys_mgr.reserved_time[j][i + index - N_reserved]);
				strcat(t, v);
				sprintf(v, "%lu]\n", subsys_mgr.hw_return_time[j][i + index - N_reserved] - subsys_mgr.start_hw_time[j][i + index - N_reserved]);
				strcat(t, v);
				total_time_eachcore[j] += subsys_mgr.hw_return_time[j][i + index - N_reserved] - subsys_mgr.start_hw_time[j][i + index - N_reserved];
			}
		} else {
			for (i = 0; i < index; i++) {
				sprintf(v, "[%lu, ", subsys_mgr.reserved_time[j][i]);
				strcat(t, v);
				sprintf(v, "%lu, ", subsys_mgr.start_hw_time[j][i] - subsys_mgr.reserved_time[j][i]);
				strcat(t, v);
				sprintf(v, "%lu, ", subsys_mgr.hw_return_time[j][i] - subsys_mgr.reserved_time[j][i]);
				strcat(t, v);
				sprintf(v, "%lu, ", subsys_mgr.release_time[j][i] - subsys_mgr.reserved_time[j][i]);
				strcat(t, v);
				sprintf(v, "%lu]\n", subsys_mgr.hw_return_time[j][i] - subsys_mgr.start_hw_time[j][i]);
				strcat(t, v);
				total_time_eachcore[j] += subsys_mgr.hw_return_time[j][i] - subsys_mgr.start_hw_time[j][i];
			}

			for (i = MAX_RESERVED_TIME - 1; i > MAX_RESERVED_TIME - 1 - (N_reserved - index); i--) {
				sprintf(v, "[%lu, ", subsys_mgr.reserved_time[j][i]);
				strcat(t, v);
				sprintf(v, "%lu, ", subsys_mgr.start_hw_time[j][i] - subsys_mgr.reserved_time[j][i]);
				strcat(t, v);
				sprintf(v, "%lu, ", subsys_mgr.hw_return_time[j][i] - subsys_mgr.reserved_time[j][i]);
				strcat(t, v);
				sprintf(v, "%lu, ", subsys_mgr.release_time[j][i] - subsys_mgr.reserved_time[j][i]);
				strcat(t, v);
				sprintf(v, "%lu]\n", subsys_mgr.hw_return_time[j][i] - subsys_mgr.start_hw_time[j][i]);
				strcat(t, v);
				total_time_eachcore[j] += subsys_mgr.hw_return_time[j][i] - subsys_mgr.start_hw_time[j][i];
			}
		}
		sprintf(v, "core[%d] total decoding time: %lu\n", j, total_time_eachcore[j]);
		strcat(t, v);
	}
	if (subsys_mgr.hantrodec_data.cores == 1) {
		for (j = 0; j < subsys_mgr.hantrodec_data.cores; j++) {
			//for (i = 0; i < MAX_RESERVED_TIME; i++) {
			total_time += total_time_eachcore[j];
			//}
		}
	} else {
		for (i = 0; i < subsys_mgr.active_hw_index; i++)
			total_time += subsys_mgr.active_return_time[i] - subsys_mgr.active_start_time[i];
	}
	sprintf(v, "total decoding time: %lu\n", total_time);
	strcat(t, v);
	ret = simple_read_from_buffer(user_buf, count, ppos, t, strlen(t));
	kfree(t);
	return ret;
}

const struct file_operations fileop_perfor_statistic = {
	.write = perf_statistic_write,
	.read = perf_statistic_read,
};
#endif

#ifdef CLK_CFG
DEFINE_SPINLOCK(clk_lock);
#endif

#define DWL_CLIENT_TYPE_H264_DEC        1U
#define DWL_CLIENT_TYPE_MPEG4_DEC       2U
#define DWL_CLIENT_TYPE_JPEG_DEC        3U
#define DWL_CLIENT_TYPE_PP              4U
#define DWL_CLIENT_TYPE_VC1_DEC         5U
#define DWL_CLIENT_TYPE_MPEG2_DEC       6U
#define DWL_CLIENT_TYPE_VP6_DEC         7U
#define DWL_CLIENT_TYPE_AVS_DEC         8U
#define DWL_CLIENT_TYPE_RV_DEC          9U
#define DWL_CLIENT_TYPE_VP8_DEC         10U
#define DWL_CLIENT_TYPE_VP9_DEC         11U
#define DWL_CLIENT_TYPE_HEVC_DEC        12U
#define DWL_CLIENT_TYPE_ST_PP           14U
#define DWL_CLIENT_TYPE_H264_MAIN10     15U
#define DWL_CLIENT_TYPE_AVS2_DEC        16U
#define DWL_CLIENT_TYPE_AV1_DEC         17U
#define DWL_CLIENT_TYPE_VVC_DEC         18U

#define CORE_TYPE_STR_CASE(ct) { case (ct): return(#ct + '\0'); }

static char *CoreTypeStr(enum CoreType ct)
{
	switch (ct) {
		CORE_TYPE_STR_CASE(HW_VCD)
		CORE_TYPE_STR_CASE(HW_VCDJ)
		CORE_TYPE_STR_CASE(HW_VCMD)
		CORE_TYPE_STR_CASE(HW_MMU)
		CORE_TYPE_STR_CASE(HW_MMU_WR)
		CORE_TYPE_STR_CASE(HW_DEC400)
		CORE_TYPE_STR_CASE(HW_AXIFE)
		CORE_TYPE_STR_CASE(HW_AFBC)
	default :
		return "Invalid core type";
	}
}

static void ReadCoreConfig(hantrodec_t *dev)
{
	int c, j;
	u32 reg, tmp, mask;

	gf_memset(subsys_mgr.config.cfg, 0, sizeof(subsys_mgr.config.cfg));

	for (c = 0; c < dev->cores; c++) {
		for (j = 0; j < HW_CORE_MAX; j++) {
			if (j != HW_VCD && j != HW_VCDJ)
				continue;
			/* NOT defined core type */
			if (!dev->hwregs[c][j])
				continue;
			/* Decoder configuration */
			if (IS_VCD(dev->hw_id[c][j])) {
				reg = gf_read_mmio32((dev->hwregs[c][j] +
					HANTRODEC_SYNTH_CFG * 4));

				gf_debug("hantrodec: subsys[%d] swreg[%d] = 0x%08x\n",
					c, HANTRODEC_SYNTH_CFG, reg);

				tmp = (reg >> DWL_H264_E) & 0x3U;
				if (tmp)
					gf_debug("hantrodec: subsys[%d] has H264\n",
						c);
				subsys_mgr.config.cfg[c] |=
					tmp ? 1 << DWL_CLIENT_TYPE_H264_DEC : 0;

				tmp = (reg >> DWL_H264HIGH10_E) & 0x01U;
				if (tmp)
					gf_debug("hantrodec: subsys[%d] has H264HIGH10\n",
						c);
				subsys_mgr.config.cfg[c] |=
					tmp ? 1 << DWL_CLIENT_TYPE_H264_DEC : 0;

				tmp = (reg >> DWL_AVS2_E) & 0x03U;
				if (tmp)
					gf_debug("hantrodec: subsys[%d] has AVS2\n",
						c);
				subsys_mgr.config.cfg[c] |=
					tmp ? 1 << DWL_CLIENT_TYPE_AVS2_DEC : 0;

				tmp = (reg >> DWL_AV1_E) & 0x01U;
				if (tmp)
					gf_debug("hantrodec: subsys[%d] has AV1\n",
						c);
				subsys_mgr.config.cfg[c] |=
					tmp ? 1 << DWL_CLIENT_TYPE_AV1_DEC : 0;

				tmp = (reg >> DWL_JPEG_E) & 0x01U;
				if (tmp)
					gf_debug("hantrodec: subsys[%d] has JPEG\n",
						c);
				subsys_mgr.config.cfg[c] |=
					tmp ? 1 << DWL_CLIENT_TYPE_JPEG_DEC : 0;

				tmp = (reg >> DWL_HJPEG_E) & 0x01U;
				if (tmp)
					gf_debug("hantrodec: subsys[%d] has HJPEG\n",
						c);
				subsys_mgr.config.cfg[c] |=
					tmp ? 1 << DWL_CLIENT_TYPE_JPEG_DEC : 0;

				tmp = (reg >> DWL_VVC_E) & 0x01U;
				if (tmp)
					gf_debug("hantrodec: subsys[%d] has VVC\n",
						c);
				subsys_mgr.config.cfg[c] |=
					tmp ? 1 << DWL_CLIENT_TYPE_VVC_DEC : 0;

				tmp = (reg >> DWL_MPEG4_E) & 0x3U;
				if (tmp)
					gf_debug("hantrodec: subsys[%d] has MPEG4\n",
						c);
				subsys_mgr.config.cfg[c] |=
					tmp ? 1 << DWL_CLIENT_TYPE_MPEG4_DEC : 0;

				tmp = (reg >> DWL_VC1_E) & 0x3U;
				if (tmp)
					gf_debug("hantrodec: subsys[%d] has VC1\n",
						c);
				subsys_mgr.config.cfg[c] |=
					tmp ? 1 << DWL_CLIENT_TYPE_VC1_DEC : 0;

				tmp = (reg >> DWL_MPEG2_E) & 0x01U;
				if (tmp)
					gf_debug("hantrodec: subsys[%d] has MPEG2\n",
						c);
				subsys_mgr.config.cfg[c] |=
					tmp ? 1 << DWL_CLIENT_TYPE_MPEG2_DEC : 0;

				tmp = (reg >> DWL_VP6_E) & 0x01U;
				if (tmp)
					gf_debug("hantrodec: subsys[%d] has VP6\n",
						c);
				subsys_mgr.config.cfg[c] |=
					tmp ? 1 << DWL_CLIENT_TYPE_VP6_DEC : 0;

				reg = gf_read_mmio32(
					(dev->hwregs[c][j] +
					HANTRODEC_SYNTH_CFG_2 * 4));

				gf_debug("hantrodec: subsys[%d] swreg[%d] = 0x%08x\n",
					c, HANTRODEC_SYNTH_CFG_2, reg);

				if (gf_read_mmio32(
					(dev->hwregs[c][j] +
					HANTRODEC_HWBUILD_ID_OFF)) !=
					0x1F70) {
					/* VP7 and WEBP is part of VP8 */
					mask = (1 << DWL_VP8_E) |
						(1 << DWL_VP7_E) |
						(1 << DWL_WEBP_E);
					tmp = (reg & mask);
					if (tmp & (1 << DWL_VP8_E))
						gf_debug("hantrodec: subsys[%d] has VP8\n",
							c);
					if (tmp & (1 << DWL_VP7_E))
						gf_debug("hantrodec: subsys[%d] has VP7\n",
							c);
					if (tmp & (1 << DWL_WEBP_E))
						gf_debug("hantrodec: subsys[%d] has WebP\n",
							c);
					subsys_mgr.config.cfg[c] |=
					  tmp ? 1 << DWL_CLIENT_TYPE_VP8_DEC :
					  0;
				}

				tmp = (reg >> DWL_AVS_E) & 0x01U;
				if (tmp)
					gf_debug("hantrodec: subsys[%d] has AVS\n",
						c);
				subsys_mgr.config.cfg[c] |=
					tmp ? 1 << DWL_CLIENT_TYPE_AVS_DEC : 0;

				tmp = (reg >> DWL_RV_E) & 0x03U;
				if (tmp)
					gf_debug("hantrodec: subsys[%d] has RV\n",
						c);
				subsys_mgr.config.cfg[c] |=
					tmp ? 1 << DWL_CLIENT_TYPE_RV_DEC : 0;

				reg = gf_read_mmio32(
					  (dev->hwregs[c][j] +
					  HANTRODEC_SYNTH_CFG_3 * 4));
				gf_debug("hantrodec: subsys[%d] swreg[%d] = 0x%08x\n",
					c, HANTRODEC_SYNTH_CFG_3, reg);

				tmp = (reg >> DWL_HEVC_E) & 0x07U;
				if (tmp)
					gf_debug("hantrodec: subsys[%d] has HEVC\n",
						c);
				subsys_mgr.config.cfg[c] |=
					tmp ? 1 << DWL_CLIENT_TYPE_HEVC_DEC : 0;

				tmp = (reg >> DWL_VP9_E) & 0x07U;
				if (tmp)
					gf_debug("hantrodec: subsys[%d] has VP9\n",
						c);
				subsys_mgr.config.cfg[c] |=
					tmp ? 1 << DWL_CLIENT_TYPE_VP9_DEC : 0;

				/* Post-processor configuration */
				reg = gf_read_mmio32(
					  (dev->hwregs[c][j] +
					  HANTRODECPP_CFG_STAT * 4));

				tmp = (reg >> DWL_PP_E) & 0x01U;
				if (tmp)
					gf_debug("hantrodec: subsys[%d] has PP\n",
						c);
				subsys_mgr.config.cfg[c] |=
					tmp ? 1 << DWL_CLIENT_TYPE_PP : 0;

				subsys_mgr.config.cfg[c] |= 1 << DWL_CLIENT_TYPE_ST_PP;
			}
		}
	}
}

static int CoreHasFormat(const u32 *cfg, int core, u32 format)
{
	return (cfg[core] & (1 << format)) ? 1 : 0;
}

static int IsValideCore(long core, unsigned long format)
{
	u32 core_mask = format & (0xffffffff >>
							CORE_MASK_CLIENTTYPE_BITMAP);

	if ((core_mask >> core) & 0x01)
		return 1;
	return 0;
}

static int GetDecCore(long core, hantrodec_t *dev, struct drm_file *filp,
		      unsigned long format)
{
	int success = 0;
	unsigned long flags = 0;
	#ifdef SUPPORT_DBGFS
		u64 time_num;
		struct timeval time_now;
	#endif

	spin_lock_irqsave(&subsys_mgr.owner_lock, flags);
	if (IsValideCore(core, format) && !subsys_mgr.dec_owner[core]) {
		subsys_mgr.dec_owner[core] = filp;
		success = 1;
#ifdef SUPPORT_DBGFS

		subsys_mgr.decode_state[core] = RESERVED;
#if (KERNEL_VERSION(4, 19, 94) > LINUX_VERSION_CODE)

		do_gettimeofday(&time_now);
			time_num = time_now.tv_sec * 1000 + time_now.tv_usec / 1000;
#else
		struct timespec64 time_now;

		ktime_get_real_ts64(&time_now);
			time_num = time_now.tv_sec * 1000 + time_now.tv_nsec / 1000000;
#endif
		subsys_mgr.reserved_time[core][subsys_mgr.r_index[core]] = time_num;
			subsys_mgr.r_index[core] = subsys_mgr.r_index[core] + 1;
		if (subsys_mgr.r_index[core] == MAX_RESERVED_TIME)
			subsys_mgr.r_index[core] = 0;
#endif
	}
	spin_unlock_irqrestore(&subsys_mgr.owner_lock, flags);

	return success;
}

static int GetDecCoreAny(long *core, hantrodec_t *dev,
			 struct drm_file *filp,
			 unsigned long format)
{
	int success = 0;
	long c;

	*core = -1;
	for (c = 0; c < dev->cores; c++) {
		/* a free core that has format */
		if (GetDecCore(c, dev, filp, format)) {
			success = 1;
			*core = c;
			break;
		}
	}

	return success;
}

static int GetDecCoreID(hantrodec_t *dev,
			struct drm_file *filp,
			unsigned long format)
{
	long c;
	unsigned long flags = 0;

	int core_id = -1;

	for (c = 0; c < dev->cores; c++) {
		/* a core that has format */
		spin_lock_irqsave(&subsys_mgr.owner_lock, flags);
		if (CoreHasFormat(subsys_mgr.config.cfg, c, format)) {
			core_id = c;
			spin_unlock_irqrestore(&subsys_mgr.owner_lock, flags);
			break;
		}
		spin_unlock_irqrestore(&subsys_mgr.owner_lock, flags);
	}
	return core_id;
}

static long ReserveDecoder(hantrodec_t *dev,
			   struct drm_file *filp,
	 unsigned long format)
{
	long core = -1;

	/* reserve a core */
	if (down_interruptible(&subsys_mgr.dec_core_sem))
		return -ERESTARTSYS;

	/* lock a core that has specific format*/
	if (wait_event_interruptible(subsys_mgr.hw_queue,
				     GetDecCoreAny(&core, dev,
						       filp, format) != 0))

		return -ERESTARTSYS;

	return core;
}

static void ReleaseDecoder(hantrodec_t *dev, long core)
{
	u32 status;
	unsigned long flags = 0;
	u32 core_type = HW_VCD;
	#ifdef SUPPORT_DBGFS
		u64 time_num;
		struct timeval time_now;
	#endif
	if (!dev->hwregs[core][core_type])
		core_type = HW_VCDJ;
	gf_debug("%s %ld\n", __func__, core);

	status = gf_read_mmio32(
			(dev->hwregs[core][core_type] +
			HANTRODEC_IRQ_STAT_DEC_OFF));

	/* make sure HW is disabled */
	if (status & HANTRODEC_DEC_E) {
		gf_debug("hantrodec: DEC[%li] still enabled -> reset\n",
			core);

		/* abort decoder */
		status |= HANTRODEC_DEC_ABORT | HANTRODEC_DEC_IRQ_DISABLE;
		gf_write_mmio32(
		  (dev->hwregs[core][core_type] +
		  HANTRODEC_IRQ_STAT_DEC_OFF), status);
	}

	spin_lock_irqsave(&subsys_mgr.owner_lock, flags);

	subsys_mgr.dec_owner[core] = NULL;
#ifdef SUPPORT_DBGFS
#if (KERNEL_VERSION(4, 19, 94) > LINUX_VERSION_CODE)

	do_gettimeofday(&time_now);
	time_num = time_now.tv_sec * 1000 + time_now.tv_usec / 1000;
#else
	struct timespec64 time_now;

	ktime_get_real_ts64(&time_now);
	time_num = time_now.tv_sec * 1000 + time_now.tv_nsec / 1000000;
#endif
	subsys_mgr.release_time[core][subsys_mgr.release_index[core]] = time_num;
	subsys_mgr.release_index[core] = subsys_mgr.release_index[core] + 1;
	if (subsys_mgr.release_index[core] == MAX_RESERVED_TIME)
		subsys_mgr.release_index[core] = 0;
	subsys_mgr.decode_state[core] = IDLE;
#endif
	subsys_mgr.dec_irq &= ~(1 << core);

	spin_unlock_irqrestore(&subsys_mgr.owner_lock, flags);

	up(&subsys_mgr.dec_core_sem);

	wake_up_interruptible_all(&subsys_mgr.hw_queue);
}

#ifdef HANTRODEC_DEBUG
static u32 flush_count; /* times of calling of DecFlushRegs */
static u32 flush_regs; /* total number of registers flushed */
#endif

static long DecFlushRegs(hantrodec_t *dev, struct core_desc *core)
{
	long ret = 0, i;
#ifdef HANTRODEC_DEBUG
	int reg_wr = 2;
#endif
#ifdef SUPPORT_DBGFS
		u64 time_num;
		struct timeval time_now;
#endif
	u32 id = core->id;
	u32 type = core->type;

	//gf_debug("hantrodec: %S\n", __func__);
	gf_debug("hantrodec: id = %d, type = %d [ %s ], size = %d, reg_id = %d\n",
	       core->id, core->type, CoreTypeStr(core->type), core->size,
		   core->reg_id);

	if (type == HW_VCD && !subsys_mgr.vpu_subsys[id].submodule_hwregs[type])
		type = HW_VCDJ;

	if (id >= MAX_SUBSYS_NUM || !subsys_mgr.vpu_subsys[id].base_addr ||
	    core->type >= HW_CORE_MAX || !subsys_mgr.vpu_subsys[id].submodule_hwregs[type])
		return -EINVAL;

	gf_debug("hantrodec: submodule_iosize = %d\n",
	       subsys_mgr.vpu_subsys[id].submodule_iosize[type]);
	//gf_debug("hantrodec: reg count = %d\n", reg_count[id]);

	gf_memcpy(subsys_mgr.dec_regs[id], (__u32 __user *)core->regs,
			     subsys_mgr.vpu_subsys[id].submodule_iosize[type]);

	if (type == HW_VCD || type == HW_BIGOCEAN || type == HW_VCDJ) {
		/* write all regs but the status reg[1] to hardware */
		if (reg_access_opt) {
			for (i = 3;
				 i < subsys_mgr.vpu_subsys[id].submodule_iosize[type] / 4;
				 i++) {
				/* check whether register value is updated. */
				if (subsys_mgr.dec_regs[id][i] != subsys_mgr.shadow_dec_regs[id][i]) {
					gf_write_mmio32( (dev->hwregs[id][type] + i * 4), subsys_mgr.dec_regs[id][i]);
					subsys_mgr.shadow_dec_regs[id][i] = subsys_mgr.dec_regs[id][i];

#ifdef HANTRODEC_DEBUG
					reg_wr++;
#endif
				}
			}
		} else {
			for (i = 3;
				 i < subsys_mgr.vpu_subsys[id].submodule_iosize[type] / 4;
				 i++) {
				gf_write_mmio32( (dev->hwregs[id][type] + i * 4), subsys_mgr.dec_regs[id][i]);

#ifdef VALIDATE_REGS_WRITE
	if (subsys_mgr.dec_regs[id][i] !=
			gf_read_mmio32((void *)(dev->hwregs[id][type] +
			  i * 4)))
		gf_debug("hantrodec: swreg[%ld]: read %08x != write %08x *\n",
			i,
				gf_read_mmio32((void *)(dev->hwregs[id][type] + i * 4)),
				subsys_mgr.dec_regs[id][i]);
#endif
			}
#ifdef HANTRODEC_DEBUG
			reg_wr = subsys_mgr.vpu_subsys[id].submodule_iosize[type] / 4 - 1;
#endif
		}

		/* write swreg2 for AV1, in which bit0 is the start bit */
		gf_write_mmio32((dev->hwregs[id][type] + 8),subsys_mgr.dec_regs[id][2]);
		subsys_mgr.shadow_dec_regs[id][2] = subsys_mgr.dec_regs[id][2];

		/* write the status register, which may start the decoder */
		gf_write_mmio32((dev->hwregs[id][type] + 4),subsys_mgr.dec_regs[id][1]);
#ifdef SUPPORT_DBGFS

	if ((subsys_mgr.dec_regs[id][1] & 0x1) == 1) {
		subsys_mgr.decode_state[id] = DECODING;
#if (KERNEL_VERSION(4, 19, 94) > LINUX_VERSION_CODE)

		do_gettimeofday(&time_now);
		time_num = time_now.tv_sec * 1000 + time_now.tv_usec / 1000;
#else
		struct timespec64 time_now;

		ktime_get_real_ts64(&time_now);
		time_num = time_now.tv_sec * 1000 + time_now.tv_nsec / 1000000;
#endif
		subsys_mgr.start_hw_time[id][subsys_mgr.start_hw_index[id]] = time_num;
		subsys_mgr.start_hw_index[id] = subsys_mgr.start_hw_index[id] + 1;
		if (subsys_mgr.start_hw_index[id] == MAX_RESERVED_TIME)
			subsys_mgr.start_hw_index[id] = 0;
		if (subsys_mgr.hantrodec_data.cores > 1) {
			int active_flag = 1;

			for (i = 0; i < subsys_mgr.hantrodec_data.cores; i++) {
				if (id == i)
					continue;
				if (subsys_mgr.decode_state[i] ==  DECODING)
					active_flag = 0;
			}
			if (active_flag == 1)
				subsys_mgr.active_start_time[subsys_mgr.active_hw_index] = time_num;
		}
	}
#endif
		subsys_mgr.shadow_dec_regs[id][1] = subsys_mgr.dec_regs[id][1];

#ifdef HANTRODEC_DEBUG
		flush_count++;
		flush_regs += reg_wr;
#endif

		gf_debug("flushed registers on core %d\n", id);
	} else {
		/* write all regs but the status reg[1] to hardware */
		for (i = 0; i < subsys_mgr.vpu_subsys[id].submodule_iosize[type] / 4;
			 i++) {
			gf_write_mmio32((dev->hwregs[id][type] + i * 4), subsys_mgr.dec_regs[id][i]);
#ifdef VALIDATE_REGS_WRITE
			if (subsys_mgr.dec_regs[id][i] !=
				gf_read_mmio32((void *)(dev->hwregs[id][type] + i * 4)))
				gf_debug(
					   "hantrodec: swreg[%ld]: read %08x != write %08x *\n",
					   i,
					   gf_read_mmio32((void *)(dev->hwregs[id][type] +
					   i * 4)),
					   subsys_mgr.dec_regs[id][i]);
#endif
		}
	}

	return 0;
}

static long DecWriteRegs(hantrodec_t *dev, struct core_desc *core)
{
	long ret = 0;
	u32 i = core->reg_id;
	u32 id = core->id;
	u32 type = core->type;

	gf_debug("hantrodec: %s\n", __func__);
	gf_debug("hantrodec: id = %d, type = %d [ %s ], size = %d, reg_id = %d\n",
	       core->id, core->type, CoreTypeStr(core->type), core->size,
		   core->reg_id);

	if (type == HW_VCD && !subsys_mgr.vpu_subsys[id].submodule_hwregs[type])
		type = HW_VCDJ;

	if (id >= MAX_SUBSYS_NUM || !subsys_mgr.vpu_subsys[id].base_addr ||
	    type >= HW_CORE_MAX || !subsys_mgr.vpu_subsys[id].submodule_hwregs[type] ||
		(core->size & 0x3) || core->reg_id * 4 + core->size >
		subsys_mgr.vpu_subsys[id].submodule_iosize[type])

		return -EINVAL;

	gf_memcpy(subsys_mgr.dec_regs[id], core->regs,
			     core->size);

	for (i = core->reg_id; i < core->reg_id + core->size / 4; i++) {
		gf_debug("hantrodec: write %08x to reg[%d] core %d\n",
		       subsys_mgr.dec_regs[id][i - core->reg_id], i, id);
		gf_write_mmio32((dev->hwregs[id][type] + i * 4),subsys_mgr.dec_regs[id][i - core->reg_id]);
		if (type == HW_VCD)
			subsys_mgr.shadow_dec_regs[id][i] = subsys_mgr.dec_regs[id][i - core->reg_id];
	}
	return 0;
}

static long DecWriteApbFilterRegs(hantrodec_t *dev, struct core_desc *core)
{
	long ret = 0;
	u32 i = core->reg_id;
	u32 id = core->id;

	gf_debug("hantrodec: %s\n", __func__);
	gf_debug("hantrodec: id = %d, type = %d, size = %d, reg_id = %d\n",
	       core->id, core->type, core->size, core->reg_id);

	if (id >= MAX_SUBSYS_NUM || !subsys_mgr.vpu_subsys[id].base_addr ||
	    core->type >= HW_CORE_MAX ||
		!subsys_mgr.vpu_subsys[id].submodule_hwregs[core->type] ||
		(core->size & 0x3) ||
		core->reg_id * 4 + core->size >
		subsys_mgr.vpu_subsys[id].submodule_iosize[core->type] + 4)

		return -EINVAL;

	gf_memcpy(subsys_mgr.apbfilter_regs[id], (__u32 __user *)core->regs,
			     core->size);

	for (i = core->reg_id; i < core->reg_id + core->size / 4; i++) {
		gf_debug("hantrodec: write %08x to reg[%d] core %d\n",
		       subsys_mgr.dec_regs[id][i - core->reg_id], i, id);
		gf_write_mmio32((dev->apbfilter_hwregs[id][core->type] +
				  i * 4),subsys_mgr.apbfilter_regs[id][i - core->reg_id]);
	}
	return 0;
}

static long DecReadRegs(hantrodec_t *dev, struct core_desc *core)
{
	long ret;
	u32 id = core->id;
	u32 i = core->reg_id;
	u32 type = core->type;
	#ifdef SUPPORT_DBGFS
		u64 time_num;
		struct timeval time_now;
	#endif
	gf_debug("hantrodec: %s\n", __func__);
	gf_debug("hantrodec: id = %d, type = %d [ %s ], size = %d, reg_id = %d\n",
	       core->id, core->type, CoreTypeStr(core->type), core->size,
	 core->reg_id);

	if (type == HW_VCD && !subsys_mgr.vpu_subsys[id].submodule_hwregs[type])
		type = HW_VCDJ;

	if (id >= MAX_SUBSYS_NUM || !subsys_mgr.vpu_subsys[id].base_addr ||
	    type >= HW_CORE_MAX || !subsys_mgr.vpu_subsys[id].submodule_hwregs[type] ||
			(core->size & 0x3) ||
			core->reg_id * 4 + core->size >
			subsys_mgr.vpu_subsys[id].submodule_iosize[type])
		return -EINVAL;

	/* read specific registers from hardware */
	for (i = core->reg_id; i < core->reg_id + core->size / 4; i++) {
		subsys_mgr.dec_regs[id][i] = gf_read_mmio32(
			(dev->hwregs[id][type] + i * 4));
#ifdef SUPPORT_DBGFS
		if ((i == 1) && ((subsys_mgr.dec_regs[id][1] & 0x1) == 0)) {
			subsys_mgr.decode_state[id] = DONE;
#if (KERNEL_VERSION(4, 19, 94) > LINUX_VERSION_CODE)

			do_gettimeofday(&time_now);
			time_num = time_now.tv_sec * 1000 + time_now.tv_usec / 1000;
#else
			struct timespec64 time_now;

			ktime_get_real_ts64(&time_now);
			time_num = time_now.tv_sec * 1000 + time_now.tv_nsec / 1000000;
#endif
			subsys_mgr.hw_return_time[id][subsys_mgr.hw_r_index[id]] = time_num;
			subsys_mgr.hw_r_index[id] = subsys_mgr.hw_r_index[id] + 1;
			if (subsys_mgr.hw_r_index[id] == MAX_RESERVED_TIME)
				subsys_mgr.hw_r_index[id] = 0;

			if (subsys_mgr.hantrodec_data.cores > 1) {
				int active_flag = 1;

				for (i = 0; i < subsys_mgr.hantrodec_data.cores; i++) {
					if (id == i)
						continue;
					if (subsys_mgr.decode_state[i] ==  DECODING)
						active_flag = 0;
				}
				if (active_flag == 1) {
					subsys_mgr.active_return_time[subsys_mgr.active_hw_index] = time_num;
					subsys_mgr.active_hw_index += 1;
				}
		}
		}
#endif
		gf_debug("hantrodec: read %08x from reg[%d] core %d\n",
		       subsys_mgr.dec_regs[id][i], i, id);
		if (type == HW_VCD)
			subsys_mgr.shadow_dec_regs[id][i] = subsys_mgr.dec_regs[id][i];
	}

	/* put registers to user space*/
	gf_memcpy(core->regs, &subsys_mgr.dec_regs[id][core->reg_id],
			   core->size);
	return 0;
}

static long DecRefreshRegs(hantrodec_t *dev, struct core_desc *core)
{
	long ret, i;
	u32 id = core->id;
	u32 type = core->type;

	gf_debug("hantrodec: %s\n", __func__);
	gf_debug("hantrodec: id = %d, type = %d [ %s ], size = %d, reg_id = %d\n",
	       core->id, core->type, CoreTypeStr(core->type), core->size,
	 core->reg_id);

	if (type == HW_VCD && !subsys_mgr.vpu_subsys[id].submodule_hwregs[type])
		type = HW_VCDJ;

	if (id >= MAX_SUBSYS_NUM || !subsys_mgr.vpu_subsys[id].base_addr ||
	    type >= HW_CORE_MAX || !subsys_mgr.vpu_subsys[id].submodule_hwregs[type])
		return -EINVAL;

	//gf_debug("hantrodec: submodule_iosize = %d\n",
	//       vpu_subsys[id].submodule_iosize[type]);

	if (!reg_access_opt) {
		for (i = 0; i < subsys_mgr.vpu_subsys[id].submodule_iosize[type] / 4;
	 i++) {
			subsys_mgr.dec_regs[id][i] = gf_read_mmio32((
	void __iomem *)(dev->hwregs[id][type] + i * 4));
		}
	} else {
		// only need to read swreg1,62(?),63,168,169
#define REFRESH_REG(idx)                                                       \
	do {                                                                   \
		i = (idx);                                                     \
		subsys_mgr.shadow_dec_regs[id][i] = subsys_mgr.dec_regs[id][i] = gf_read_mmio32(           \
			(dev->hwregs[id][type] + i * 4));      \
	} while (0)

		REFRESH_REG(0);
		REFRESH_REG(1);
		REFRESH_REG(62);
		REFRESH_REG(63);
		REFRESH_REG(168);
		REFRESH_REG(169);
#undef REFRESH_REG
	}

	gf_memcpy(core->regs, subsys_mgr.dec_regs[id],
			   subsys_mgr.vpu_subsys[id].submodule_iosize[type]);

	return 0;
}

static int CheckDecIrq(hantrodec_t *dev, int id)
{
	unsigned long flags = 0;
	int rdy = 0;

	const u32 irq_mask = (1 << id);

	spin_lock_irqsave(&subsys_mgr.owner_lock, flags);

	if (subsys_mgr.dec_irq & irq_mask) {
		/* reset the wait condition(s) */
		subsys_mgr.dec_irq &= ~irq_mask;
		rdy = 1;
	}

	spin_unlock_irqrestore(&subsys_mgr.owner_lock, flags);

	return rdy;
}

static long WaitDecReadyAndRefreshRegs(hantrodec_t *dev, struct core_desc *core)
{
	u32 id = core->id;
	long ret;

	gf_debug("wait_event_interruptible DEC[%d]\n", id);
#ifdef USE_SW_TIMEOUT
	u32 status;

	ret = wait_event_interruptible_timeout(subsys_mgr.dec_wait_queue,
					       CheckDecIrq(dev, id),
		  msecs_to_jiffies(2000));
	if (ret < 0) {
		gf_debug("DEC[%d]  wait_event_interruptible interrupted\n", id);
		return -ERESTARTSYS;
	} else if (ret == 0) {
		gf_debug("DEC[%d]  wait_event_interruptible timeout\n", id);
		status = gf_read_mmio32((void *)(dev->hwregs[id][HW_VCD] +
		HANTRODEC_IRQ_STAT_DEC_OFF));
		/* check if HW is enabled */
		if (status & HANTRODEC_DEC_E) {
			gf_debug("hantrodec: DEC[%d] reset because of timeout\n",
				id);

			/* abort decoder */
			status |= HANTRODEC_DEC_ABORT |
					  HANTRODEC_DEC_IRQ_DISABLE;
			gf_write_mmio32( (void *)(dev->hwregs[id][HW_VCD] +
		HANTRODEC_IRQ_STAT_DEC_OFF),status);
		}
	}
#else
	ret = wait_event_interruptible(subsys_mgr.dec_wait_queue, CheckDecIrq(dev, id));
	if (ret) {
		gf_debug("DEC[%d]  wait_event_interruptible interrupted\n", id);
		return -ERESTARTSYS;
	}
#endif
	atomic_inc(&subsys_mgr.irq_tx);

	/* refresh registers */
	return DecRefreshRegs(dev, core);
}

static int CheckCoreIrq(hantrodec_t *dev, const struct drm_file *filp, int *id)
{
	unsigned long flags = 0;
	int rdy = 0, n = 0;

	do {
		u32 irq_mask = (1 << n);

		spin_lock_irqsave(&subsys_mgr.owner_lock, flags);

		if (subsys_mgr.dec_irq & irq_mask) {
			if (subsys_mgr.dec_owner[n] == filp) {
				/* we have an IRQ for our client */

				/* reset the wait condition(s) */
				subsys_mgr.dec_irq &= ~irq_mask;

				/* signal ready core no. for our client */
				*id = n;

				rdy = 1;

				spin_unlock_irqrestore(&subsys_mgr.owner_lock, flags);
				break;
			} else if (!subsys_mgr.dec_owner[n]) {
				/* zombie IRQ */
				gf_debug("IRQ on core[%d], but no owner!!!\n",
					n);

				/* reset the wait condition(s) */
				subsys_mgr.dec_irq &= ~irq_mask;
			}
		}

		spin_unlock_irqrestore(&subsys_mgr.owner_lock, flags);

		n++; /* next core */
	} while (n < dev->cores);

	return rdy;
}

static long WaitCoreReady(hantrodec_t *dev, const struct drm_file *filp, int *id)
{
	long ret;

	gf_debug("wait_event_interruptible CORE\n");
#ifdef USE_SW_TIMEOUT
	u32 i, status;

	ret = wait_event_interruptible_timeout(subsys_mgr.dec_wait_queue,
					       CheckCoreIrq(dev, filp, id),
		msecs_to_jiffies(2000));
	if (ret < 0) {
		gf_debug("CORE  wait_event_interruptible interrupted\n");
		return -ERESTARTSYS;
	} else if (ret == 0) {
		gf_debug("CORE  wait_event_interruptible timeout\n");
		for (i = 0; i < dev->cores; i++) {
			status = gf_read_mmio32((void *)(dev->hwregs[i][HW_VCD] +
			HANTRODEC_IRQ_STAT_DEC_OFF));
			/* check if HW is enabled */
			if ((status & HANTRODEC_DEC_E) &&
			    subsys_mgr.dec_owner[i] == filp) {
				gf_debug("hantrodec: CORE[%d] reset because of timeout\n",
					i);
				*id = i;
				/* abort decoder */
				status |= HANTRODEC_DEC_ABORT |
						HANTRODEC_DEC_IRQ_DISABLE;
				gf_write_mmio32((dev->hwregs[i][HW_VCD] +
					HANTRODEC_IRQ_STAT_DEC_OFF),status);
				break;
			}
		}
	}
#else
	ret = wait_event_interruptible(subsys_mgr.dec_wait_queue,
				       CheckCoreIrq(dev, filp, id));
	if (ret) {
		gf_debug("CORE[%d] wait_event_interruptible interrupted with 0x%lx\n",
		       *id, ret);
		return -ERESTARTSYS;
	}
#endif
	atomic_inc(&subsys_mgr.irq_tx);

	return 0;
}

int xx_HANTRODEC_IOCGHWOFFSET(struct drm_device *dev, void *data,
		       struct drm_file *file_priv)
{
		__u32 id;

		id = *(unsigned long *)data;

		if (id >= subsys_mgr.hantrodec_data.cores)
			return -EFAULT;

		*(unsigned long *)data = subsys_mgr.multicorebase[id];

	return 0;
}

int xx_HANTRODEC_IOCGHWIOSIZE(struct drm_device *dev, void *data,
		       struct drm_file *file_priv)
{
	struct regsize_desc core;

	gf_memcpy(&core, data, sizeof(struct regsize_desc));

	if (core.id >= MAX_SUBSYS_NUM)
		return -EFAULT;

	{
		core.size =
			subsys_mgr.vpu_subsys[core.id].submodule_iosize[core.type];
		if (core.type == HW_VCD && !core.size &&
			subsys_mgr.vpu_subsys[core.id].submodule_hwregs[HW_VCDJ])
			/* If VCD doesn't exists, while VCDJ
				* exists, return VCDJ.
				*/
			core.size = subsys_mgr.vpu_subsys[core.id].submodule_iosize[HW_VCDJ];
	}

	gf_memcpy(data, &core, sizeof(struct regsize_desc));

	return 0;
}

int xx_HANTRODEC_IOC_MC_OFFSETS(struct drm_device *dev, void *data,
		       struct drm_file *file_priv)
{
	gf_memcpy(data, subsys_mgr.multicorebase, sizeof(subsys_mgr.multicorebase));
	return 0;
}

int xx_HANTRODEC_IOC_MC_CORES(struct drm_device *dev, void *data,
		       struct drm_file *file_priv)
{
	*(int*)data = subsys_mgr.hantrodec_data.cores;
	gf_debug("subsys_mgr.hantrodec_data.cores=%d\n", subsys_mgr.hantrodec_data.cores);

	return 0;
}

int xx_HANTRODEC_IOCS_DEC_PUSH_REG(struct drm_device *dev, void *data,
		       struct drm_file *file_priv)
{
		struct core_desc core;

		/* get registers from user space*/
		gf_memcpy(&core, data,
				     sizeof(struct core_desc));

		return DecFlushRegs(&subsys_mgr.hantrodec_data, &core);
}

int xx_HANTRODEC_IOCS_DEC_WRITE_REG(struct drm_device *dev, void *data,
		       struct drm_file *file_priv)
{
		struct core_desc core;

		/* get registers from user space*/
		gf_memcpy(&core, data,
				     sizeof(struct core_desc));

		return DecWriteRegs(&subsys_mgr.hantrodec_data, &core);
}

int xx_HANTRODEC_IOCS_DEC_WRITE_APBFILTER_REG(struct drm_device *dev, void *data,
		       struct drm_file *file_priv)
{		
		struct core_desc core;

		/* get registers from user space*/
		gf_memcpy(&core, data,
				     sizeof(struct core_desc));

		return DecWriteApbFilterRegs(&subsys_mgr.hantrodec_data, &core);
}

int xx_HANTRODEC_IOCS_DEC_PULL_REG(struct drm_device *dev, void *data,
		       struct drm_file *file_priv)
{
		struct core_desc core;

		/* get registers from user space*/
		gf_memcpy(&core, data,
				     sizeof(struct core_desc));

		return DecRefreshRegs(&subsys_mgr.hantrodec_data, &core);
}

int xx_HANTRODEC_IOCS_DEC_READ_REG(struct drm_device *dev, void *data,
		       struct drm_file *file_priv)
{
		struct core_desc core;

		/* get registers from user space*/
		gf_memcpy(&core, data,
				     sizeof(struct core_desc));

		return DecReadRegs(&subsys_mgr.hantrodec_data, &core);
}

int xx_HANTRODEC_IOCH_DEC_RESERVE(struct drm_device *dev, void *data,
		       struct drm_file *file_priv)
{
		u32 format = 0;

		format = *(u32*)data;
		gf_debug("Reserve DEC core, format = %d\n", format);
		return ReserveDecoder(&subsys_mgr.hantrodec_data, file_priv, format);
}

int xx_HANTRODEC_IOCT_DEC_RELEASE(struct drm_device *dev, void *data,
		       struct drm_file *file_priv)
{
		u32 core = 0;

		core = *(u32*)data;
		if (core >= subsys_mgr.hantrodec_data.cores || subsys_mgr.dec_owner[core] != file_priv) {
			gf_debug("bogus DEC release, core = %d\n", core);
			return -EFAULT;
		}

		gf_debug("Release DEC, core = %d\n", core);

		ReleaseDecoder(&subsys_mgr.hantrodec_data, core);
		return 0;
}

int xx_HANTRODEC_IOCX_DEC_WAIT(struct drm_device *dev, void *data,
		       struct drm_file *file_priv)
{
	struct core_desc core;

	/* get registers from user space */
	gf_memcpy(&core, data,
					sizeof(struct core_desc));

	return WaitDecReadyAndRefreshRegs(&subsys_mgr.hantrodec_data, &core);
}

int xx_HANTRODEC_IOCG_CORE_WAIT(struct drm_device *dev, void *data,
		       struct drm_file *file_priv)
{
	int id;

	int tmp = WaitCoreReady(&subsys_mgr.hantrodec_data, file_priv, &id);
	*(int*)data = id;
	return tmp;
}

int xx_HANTRODEC_IOX_ASIC_ID(struct drm_device *dev, void *data,
		       struct drm_file *file_priv)
{
	struct core_param core;

	/* get registers from user space*/
	gf_memcpy(&core, data,
					sizeof(struct core_param));

	/*subsys_mgr.hantrodec_data.cores*/
	if (core.id >= MAX_SUBSYS_NUM  ||
		((core.type == HW_VCD ||
core.type == HW_VCDJ) &&
!subsys_mgr.vpu_subsys[core.id]
.submodule_iosize[core.type == HW_VCD] &&
!subsys_mgr.vpu_subsys[core.id]
.submodule_iosize[core.type == HW_VCDJ]) ||
((core.type != HW_VCD && core.type != HW_VCDJ) &&
!subsys_mgr.vpu_subsys[core.id].submodule_iosize[core.type]))
		return -EFAULT;

	core.size = subsys_mgr.vpu_subsys[core.id].submodule_iosize[core.type];
	if (subsys_mgr.vpu_subsys[core.id].submodule_hwregs[core.type]) {
		core.asic_id = gf_read_mmio32(
			subsys_mgr.hantrodec_data.hwregs[core.id][core.type]);
	} else if (core.type == HW_VCD &&
			subsys_mgr.hantrodec_data.hwregs[core.id][HW_VCDJ]) {
		core.asic_id =
			gf_read_mmio32(
			subsys_mgr.hantrodec_data.hwregs[core.id][HW_VCDJ]);
	} else {
		core.asic_id = 0;
	}
	gf_memcpy(data, &core,
				sizeof(struct core_param));

	return 0;
}

int xx_HANTRODEC_IOCG_CORE_ID(struct drm_device *dev, void *data,
		       struct drm_file *file_priv)
{
	u32 format = 0;

	format = *(u32*)data;

	gf_debug("Get DEC Core_id, format = %d\n", format);
	return GetDecCoreID(&subsys_mgr.hantrodec_data, file_priv, format);
}

int xx_HANTRODEC_IOX_ASIC_BUILD_ID(struct drm_device *dev, void *data,
		       struct drm_file *file_priv)
{
	u32 id, hw_id;

	id = *(u32*)data;

	if (id >= subsys_mgr.hantrodec_data.cores)
		return -EFAULT;
	if (subsys_mgr.hantrodec_data.hwregs[id][HW_VCD] ||
		subsys_mgr.hantrodec_data.hwregs[id][HW_VCDJ]) {
		u8 *hwregs;
		/* VCD first if it exists, otherwise VCDJ. */
		if (subsys_mgr.hantrodec_data.hwregs[id][HW_VCD])
			hwregs = subsys_mgr.hantrodec_data.hwregs[id][HW_VCD];
		else
			hwregs = subsys_mgr.hantrodec_data.hwregs[id][HW_VCDJ];

		hw_id = gf_read_mmio32((hwregs +
					HANTRODEC_HW_BUILD_ID_OFF));
		*(u32*)data = hw_id;
	}
	return 0;
}

int xx_HANTRODEC_IOX_SUBSYS(struct drm_device *dev, void *data,
		       struct drm_file *file_priv)
{
	struct subsys_desc subsys = { 0 };
	/* TODO(min): check all the subsys */
	if (vcmd) {
		subsys.subsys_vcmd_num = 1;
		subsys.subsys_num = subsys.subsys_vcmd_num;
	} else {
		subsys.subsys_num = subsys_mgr.hantrodec_data.cores;
		subsys.subsys_vcmd_num = 0;
	}
	gf_memcpy(data, &subsys,
				sizeof(struct subsys_desc));

	return 0;
}

int xx_HANTRODEC_IOCX_POLL(struct drm_device *dev, void *data,
		       struct drm_file *file_priv)
{
	struct lisuan_device *gdev = dev->dev_private;
	hantrodec_isr(gdev);
	return 0;
}

int xx_HANTRODEC_IOC_APBFILTER_CONFIG(struct drm_device *dev, void *data,
		       struct drm_file *file_priv)
{
	struct apbfilter_cfg tmp_apbfilter;

	/* get registers from user space*/
	gf_memcpy(&tmp_apbfilter, data,
					sizeof(struct apbfilter_cfg));

	if (tmp_apbfilter.id >= MAX_SUBSYS_NUM ||
		tmp_apbfilter.type >= HW_CORE_MAX)
		return -EFAULT;

	subsys_mgr.apbfilter_cfg[tmp_apbfilter.id][tmp_apbfilter.type].id =
		tmp_apbfilter.id;
	subsys_mgr.apbfilter_cfg[tmp_apbfilter.id][tmp_apbfilter.type].type =
		tmp_apbfilter.type;

	gf_memcpy(&tmp_apbfilter,
			&subsys_mgr.apbfilter_cfg[tmp_apbfilter.id][tmp_apbfilter.type],
		sizeof(struct apbfilter_cfg));

	gf_memcpy(data, &tmp_apbfilter,
				sizeof(struct apbfilter_cfg));

	return 0;
}

int xx_HANTRODEC_IOC_AXIFE_CONFIG(struct drm_device *dev, void *data,
		       struct drm_file *file_priv)
{
	struct axife_cfg tmp_axife;

	/* get registers from user space*/
	gf_memcpy(&tmp_axife, data,
					sizeof(struct axife_cfg));

	if (tmp_axife.id >= MAX_SUBSYS_NUM)
		return -EFAULT;

	subsys_mgr.axife_cfg[tmp_axife.id].id = tmp_axife.id;

	gf_memcpy(&tmp_axife, &subsys_mgr.axife_cfg[tmp_axife.id],
			sizeof(struct axife_cfg));

	gf_memcpy(data, &tmp_axife,
				sizeof(struct axife_cfg));

	return 0;
}

#ifdef DTB_SUPPORT
extern struct SubsysDesc subsys_array[];
extern struct CoreDesc core_array[];
/* get base address and irq number from DTB */
static int get_of_property(void)
{
	struct resource res;
	int irq_num;
	struct device_node *np;
	int ret;
	long long vcd_base = 0;

	np = of_find_compatible_node(NULL, NULL, "verisilicon,vcd");

	ret = of_address_to_resource(np, 0, &res);
	if (ret) {
		gf_error("can't get VCD base address from DTB. ret=%d,\n", ret);
		return -EINVAL;
	}
	vcd_base = res.start;
	core_array[1].offset = 0;
	core_array[1].core_type = HW_VCD;
	gf_debug("get VCD base address 0x%llx\n", vcd_base);

	irq_num = of_irq_get_byname(np, "vcd_irq");
	if (irq_num > 0) {
		gf_debug("get VCD irq number %d\n", irq_num);
		core_array[1].irq = irq_num;
	}
	{
		np = of_find_compatible_node(NULL, NULL, "verisilicon,vcmd");

		ret = of_address_to_resource(np, 0, &res);
		if (ret) {
			gf_error("can't get VCMD base address from DTB. ret=%d,\n", ret);
			return -EINVAL;
		}
		subsys_array[0].base = res.start;
		gf_debug("get VCMD base address 0x%llx\n", subsys_array[0].base);
		core_array[0].offset = 0;
		core_array[0].core_type = HW_VCMD;
		core_array[1].offset = vcd_base - res.start;

		irq_num = of_irq_get_byname(np, "vcmd_irq");
		if (irq_num > 0) {
			gf_debug("get VCMD irq number %d\n", irq_num);
			core_array[0].irq = irq_num;
			core_array[1].irq = -1;
		}
	}

	return 0;
}
#endif /* DTB_SUPPORT */

/*
 * Function name   : hantrodec_open
 * Description     : open method

 * Return type     : int
 */
int hantrodec_open(struct lisuan_device *gf_dev, struct drm_file *file)
{
	gf_debug("dev opened\n");
	if (vcmd) {
		hantrodec_vcmd_open(gf_dev, file);
	}
	#ifdef SUPPORT_DBGFS
	if (!vcmd) {
		int i, j;

		for (i = 0; i < HXDEC_MAX_CORES; i++) {
			for (j = 0; j < MAX_RESERVED_TIME; j++) {
				subsys_mgr.hw_return_time[i][j] = 0;
				subsys_mgr.reserved_time[i][j] = 0;
				subsys_mgr.start_hw_time[i][j] = 0;
				subsys_mgr.active_start_time[j] = 0;
				subsys_mgr.active_return_time[j] = 0;
				subsys_mgr.release_time[i][j] = 0;
			}
			subsys_mgr.r_index[i] = 0;
			subsys_mgr.hw_r_index[i] = 0;
			subsys_mgr.start_hw_index[i] = 0;
			subsys_mgr.release_index[i] = 0;
		}
		subsys_mgr.active_hw_index = 0;
	}
#endif
	return 0;
}

/*
 * Function name   : hantrodec_release
 * Description     : Release driver

 * Return type     : int
 */

int hantrodec_release(struct drm_device *ddev,
			     struct drm_file *filp)
{
	int n;
	hantrodec_t *dev = &subsys_mgr.hantrodec_data;
	struct lisuan_device *gf_device = (struct lisuan_device *)ddev->dev_private;

	gf_debug("closing ...\n");


	if (vcmd) {
		hantrodec_vcmd_release(gf_device, filp);
		if (subsys_mgr.hantrodec_data.hwregs[0][HW_MMU])
			//MMURelease(filp, subsys_mgr.hantrodec_data.hwregs[0][HW_MMU]);
		return 0;
	}

	for (n = 0; n < dev->cores; n++) {
		if (subsys_mgr.dec_owner[n] == filp) {
			gf_debug("releasing dec core %i lock\n", n);
			ReleaseDecoder(dev, n);
		}
	}

	//MMURelease(filp, subsys_mgr.hantrodec_data.hwregs[0][HW_MMU]);

	gf_debug("closed\n");
	return 0;
}

#ifdef CLK_CFG
void hantrodec_disable_clk(unsigned long value)
{
	unsigned long flags;
	/* entering this function means decoder
	 * is idle over expiry.So disable clk
	 */
	if (clk_cfg && !IS_ERR(clk_cfg)) {
		spin_lock_irqsave(&clk_lock, flags);
		if (is_clk_on == 1) {
			clk_disable(clk_cfg);
			is_clk_on = 0;
			gf_debug("turned off hantrodec clk\n");
		}
		spin_unlock_irqrestore(&clk_lock, flags);
	}
}
#endif

void hantro_dec_resume(struct lisuan_device *gf_device)
{
	hantro_vcmd_resume(gf_device, subsys_mgr.vpu_subsys);
}

/*
 *Function name   : hantrodec_init
 *Description     : Initialize the driver

 *Return type     : int
 */

int hantrodec_init(struct lisuan_device *gf_device)
{
	int result = 0, i;
	int subsys_num;

	gf_debug("module init\n");

#ifdef DTB_SUPPORT
	result = get_of_property();
	if (result)
		goto err;
#endif

	CheckSubsysCoreArray(subsys_mgr.vpu_subsys, &subsys_num, &vcmd);

	gf_debug("hantrodec: dec/pp kernel module.\n");

	/* If base_port is set when insmod,
	 * use that for single core legacy mode.
	 */
	if (base_port != -1) {
		subsys_mgr.multicorebase[0] = base_port;
		subsys_mgr.elements = 1;
		subsys_mgr.vpu_subsys[0].base_addr = base_port;
		gf_debug("hantrodec: Init single core at 0x%llx IRQ=%i\n",
			subsys_mgr.multicorebase[0], subsys_mgr.irq[0]);
	} else {
		int i;

		gf_memset(subsys_mgr.multicorebase, 0, sizeof(subsys_mgr.multicorebase[0]) * HXDEC_MAX_CORES);
		for (i = 0; i < subsys_num; i++) {
			u32 core_type = HW_VCD; /* vcd */

			subsys_mgr.multicorebase[i] = subsys_mgr.vpu_subsys[i].base_addr + subsys_mgr.vpu_subsys[i].submodule_offset[core_type];
			subsys_mgr.irq[i] = subsys_mgr.vpu_subsys[i].irq;
			subsys_mgr.iosize[i] = subsys_mgr.vpu_subsys[i].submodule_iosize[core_type];
			gf_debug("hantrodec: [%d] multicorebase 0x%llx, iosize %d\n", i, subsys_mgr.multicorebase[i], subsys_mgr.iosize[i]);
		}
		gf_debug("hantrodec: Init multi core[0] at 0x%llx\n"
				"                      core[1] at 0x%llx\n"
				"                      core[2] at 0x%llx\n"
				"                      core[3] at 0x%llx\n"
				"                      IRQ_0=%i\n"
				"                      IRQ_1=%i\n",
				subsys_mgr.multicorebase[0], subsys_mgr.multicorebase[1],
				subsys_mgr.multicorebase[2],
				subsys_mgr.multicorebase[3], subsys_mgr.irq[0], subsys_mgr.irq[1]);
	}

	subsys_mgr.hantrodec_data.cores = 0;

	subsys_mgr.hantrodec_data.iosize[0] = DEC_IO_SIZE_0;
	subsys_mgr.hantrodec_data.irq[0] = subsys_mgr.irq[0];
	subsys_mgr.hantrodec_data.iosize[1] = DEC_IO_SIZE_1;
	subsys_mgr.hantrodec_data.irq[1] = subsys_mgr.irq[1];

	for (i = 0; i < HXDEC_MAX_CORES; i++) {
		int j;

		for (j = 0; j < HW_CORE_MAX; j++)
			subsys_mgr.hantrodec_data.hwregs[i][j] = NULL;
		/* If user gave less core bases that we have
		 * by default,invalidate default bases
		 */
		if (subsys_mgr.elements && i >= subsys_mgr.elements)
			subsys_mgr.multicorebase[i] = 0;
	}

	subsys_mgr.hantrodec_data.async_queue_dec = NULL;
	subsys_mgr.hantrodec_data.async_queue_pp = NULL;
#ifdef CLK_CFG
	/* first get clk instance pointer */
	clk_cfg = clk_get(NULL, CLK_ID);
	if (!clk_cfg || IS_ERR(clk_cfg)) {
		gf_error("get handrodec clk failed!\n");
		goto err;
	}

	/* prepare and enable clk */
	if (clk_prepare_enable(clk_cfg)) {
		gf_error("try to enable handrodec clk failed!\n");
		goto err;
	}
	is_clk_on = 1;

	/* init a timer to disable clk */
	init_timer(&timer);
	timer.function = &hantrodec_disable_clk;
	/* the expires time is 100s */
	timer.expires = jiffies + 100 * HZ;
	add_timer(&timer);
#endif

	result = ReserveIO(gf_device);
	if (result < 0)
		goto err;

	for (i = 0; i < subsys_mgr.hantrodec_data.cores; i++)
		dec_AXIFEEnable(subsys_mgr.hantrodec_data.hwregs[i][HW_AXIFE]);

#ifdef SUPPORT_DBGFS
	if (!vcmd) {
		for (i = 0; i < HXDEC_MAX_CORES; i++) {
			for (j = 0; j < MAX_RESERVED_TIME; j++) {
				subsys_mgr.hw_return_time[i][j] = 0;
				subsys_mgr.reserved_time[i][j] = 0;
				subsys_mgr.start_hw_time[i][j] = 0;
				subsys_mgr.release_time[i][j] = 0;
				subsys_mgr.active_start_time[j] = 0;
				subsys_mgr.active_return_time[j] = 0;
			}
			subsys_mgr.r_index[i] = 0;
			subsys_mgr.hw_r_index[i] = 0;
			subsys_mgr.start_hw_index[i] = 0;
			subsys_mgr.release_index[i] = 0;
			subsys_mgr.decode_state[i] = 0;
		}
		subsys_mgr.active_hw_index = 0;
		debug_root = debugfs_create_dir("VCDEC", NULL);
		debugfs_create_file("decode_cycles", 0444, debug_root, NULL, &fileop_cycles);
		debugfs_create_file("performance_statistic", 0444, debug_root, NULL, &fileop_perfor_statistic);
		debugfs_create_file("subsys_state", 0444, debug_root, NULL, &fileop_subsys_state);
		debugfs_create_file("regprint", 0444, debug_root, NULL, &fileop_hw_reg_print);
	}
#endif

	/* read configuration fo all cores */
	//if (!gf_device->is_virtfn)
		ReadCoreConfig(&subsys_mgr.hantrodec_data);

	if (vcmd) {
		/* unmap and release mem region for VCMD,
		 * since it will be mapped and
		 * reserved again in hantro_vcmd.c
		 */
		for (i = 0; i < subsys_mgr.hantrodec_data.cores; i++) {
#ifdef SUPPORT_DBGFS
			if (subsys_mgr.hantrodec_data.hwregs[i][HW_VCD])
				vcmd_core_array[i].submodule_vcd_virtual_address =
					subsys_mgr.hantrodec_data.hwregs[i][HW_VCD];
			if (subsys_mgr.hantrodec_data.hwregs[i][HW_DEC400])
				vcmd_core_array[i].submodule_dec400_virtual_address =
					subsys_mgr.hantrodec_data.hwregs[i][HW_DEC400];
			if (subsys_mgr.hantrodec_data.hwregs[i][HW_MMU])
				vcmd_core_array[i].submodule_MMU_virtual_address =
					subsys_mgr.hantrodec_data.hwregs[i][HW_MMU];
			if (subsys_mgr.hantrodec_data.hwregs[i][HW_MMU_WR])
				vcmd_core_array[i].submodule_MMUWrite_virtual_address =
					subsys_mgr.hantrodec_data.hwregs[i][HW_MMU_WR];
			if (subsys_mgr.hantrodec_data.hwregs[i][HW_AXIFE])
				vcmd_core_array[i].submodule_axife_virtual_address =
					subsys_mgr.hantrodec_data.hwregs[i][HW_AXIFE];
#endif

			if (subsys_mgr.hantrodec_data.hwregs[i][HW_VCMD]) {
				subsys_mgr.hantrodec_data.hwregs[i][HW_VCMD] = NULL;
			}
		}
		subsys_mgr.vcmd_mgr = hantrodec_vcmd_init(gf_device, subsys_mgr.vpu_subsys, subsys_num, subsys_mgr.platformdev);
		if (!subsys_mgr.vcmd_mgr)
			goto err;

		gf_device->vcmd_mgr = subsys_mgr.vcmd_mgr;
		return 0;
	}

	gf_memset(subsys_mgr.dec_owner, 0, sizeof(subsys_mgr.dec_owner));

	sema_init(&subsys_mgr.dec_core_sem, subsys_mgr.hantrodec_data.cores);
	sema_init(&subsys_mgr.pp_core_sem, 1);

	atomic_set(&subsys_mgr.irq_rx, 0);
	atomic_set(&subsys_mgr.irq_tx, 0);
	spin_lock_init(&subsys_mgr.owner_lock);
	init_waitqueue_head(&subsys_mgr.dec_wait_queue);
	init_waitqueue_head(&subsys_mgr.pp_wait_queue);
	init_waitqueue_head(&subsys_mgr.hw_queue);

	/* reset hardware */
	ResetAsic(&subsys_mgr.hantrodec_data);

	for (i = 0; i < subsys_mgr.hantrodec_data.cores; i++) {
		u32 core_type = HW_VCD;
		u8 *hwregs = subsys_mgr.hantrodec_data.hwregs[i][core_type];

		if (!hwregs) {
			core_type = HW_VCDJ;
			hwregs = subsys_mgr.hantrodec_data.hwregs[i][core_type];
		}

		if (hwregs) {
			gf_debug("hantrodec: %s [%d] has build id 0x%08x\n",
				CoreTypeStr(core_type), i,
				gf_read_mmio32(
				(hwregs + HANTRODEC_HWBUILD_ID_OFF)));
		}
	}

	gf_debug("hantrodec: module inserted. Major = %d\n",
		subsys_mgr.hantrodec_major);

	/* Please call the TEE functions to
	 * set VCD DRM relative registers here
	 */

	return 0;

err:

	return result;
}

/*
 * Function name   : hantrodec_cleanup
 * Description     : clean up
 * Return type     : int
 */
void hantrodec_cleanup(struct lisuan_device *gf_dev)
{
	hantrodec_t *dev = &subsys_mgr.hantrodec_data;
	int i, n = 0;
	u8 *mmu_hwregs[MAX_SUBSYS_NUM][2];
	int has_mmu = 0;
	u32 vcmd_offset;
	u32 vcmd_iosize;
	u8 *vcmd_hwregs = NULL;
	u32 arbiter_cfg;

/** layne
	// Check if APB arbiter is existed
	if (vcmd && has_apb_arbiter) {
		vcmd_offset = subsys_mgr.vpu_subsys[0].base_addr + subsys_mgr.vpu_subsys[0].submodule_offset[HW_VCMD];
		vcmd_iosize = subsys_mgr.vpu_subsys[0].submodule_iosize[HW_VCMD];

		vcmd_hwregs = gf_dev->mmio_base + vcmd_offset;

		arbiter_cfg = gf_read_mmio32((vcmd_hwregs + VCMD_REGISTER_ARBITER_CONFIG_OFFSET));
		gf_write_mmio32((vcmd_hwregs + VCMD_REGISTER_ARBITER_CONFIG_OFFSET),(arbiter_cfg | VCMD_ARBITER_ENABLE << 8) );
		gf_write_mmio32((vcmd_hwregs + VCMD_REGISTER_ARB_OFFSET),0x0 );
		gf_write_mmio32((vcmd_hwregs + VCMD_REGISTER_ARB_OFFSET),0x1 );
		while ((gf_read_mmio32((vcmd_hwregs + VCMD_REGISTER_ARB_OFFSET)) & 0x4) == 0)
			schedule();
	}
*/
	for (i = 0; i < MAX_SUBSYS_NUM; i++) {
		mmu_hwregs[i][0] = dev->hwregs[i][HW_MMU];
		mmu_hwregs[i][1] = dev->hwregs[i][HW_MMU_WR];
		if (dev->hwregs[i][HW_DEC400]) {
			/* disable dec400 when rmmod driver. */
			gf_write_mmio32((dev->hwregs[i][HW_DEC400] +
				 0x800),0x00810002);
		}
		if (dev->hwregs[i][HW_MMU])
			has_mmu = 1;
	}

	//layne if (vcmd && has_apb_arbiter)
		//layne gf_write_mmio32((vcmd_hwregs + VCMD_REGISTER_ARB_OFFSET),0x2 );

	if (vcmd) {
		hantrovcmd_cleanup(gf_dev, subsys_mgr.vcmd_mgr);
	} else {
		/* reset hardware */
		ResetAsic(dev);
	}

#ifdef SUPPORT_DBGFS
	debugfs_remove_recursive(debug_root);
	debug_root = NULL;
#endif

#ifdef CLK_CFG
	if (clk_cfg && !IS_ERR(clk_cfg)) {
		clk_disable_unprepare(clk_cfg);
		is_clk_on = 0;
		gf_debug("turned off hantrodec clk\n");
	}

	/*delete timer*/
	del_timer(&timer);
#endif

	gf_debug("hantrodec: module removed\n");
}

/*
 *Function name   : CheckHwId
 *Return type     : int
 */
static int CheckHwId(hantrodec_t *dev)
{
	int hwid;
	int i, j;
	size_t num_hw = sizeof(DecHwId) / sizeof(*DecHwId);

	int found = 0;

	for (i = 0; i < dev->cores; i++) {
		for (j = 0; j < HW_CORE_MAX; j++) {
			if ((j == HW_VCD || j == HW_VCDJ) &&
			    dev->hwregs[i][j]) {
				hwid = gf_read_mmio32(
					dev->hwregs[i][j]);
				gf_debug("hantrodec: core %d:%d HW ID=0x%08x [ %s ]\n",
					i, j, hwid, CoreTypeStr(j));
				/* product version only */
				hwid = (hwid >> 16) & 0xFFFF;
				while (num_hw--) {
					if (hwid == DecHwId[num_hw]) {
						//gf_debug("hantrodec: Supported HW found at 0x%16lx\n",
						gf_debug("hantrodec: Supported HW found at 0x%llx\n",
							subsys_mgr.vpu_subsys[i].base_addr +
							subsys_mgr.vpu_subsys[i].submodule_offset[j]);
						found++;
						dev->hw_id[i][j] = hwid;
						break;
					}
				}

				if (!found) {
					gf_debug("hantrodec: Unknown HW found at 0x%llx\n",
						subsys_mgr.multicorebase[i]);
					return 0;
				}

				found = 0;
				num_hw = sizeof(DecHwId) / sizeof(*DecHwId);
			}
		}
	}

	return 1;
}

/*
 * Function name   : ReserveIO
 * Description     : IO reserve
 * Return type     : int
 */
static int ReserveIO(struct lisuan_device *gf_device)
{
	int i, j;
	long hwid;
	u32 axife_config;
	u32 vcmd_offset;
	unsigned char *vcmd_hwregs = 0;

	gf_memcpy((unsigned int *)(subsys_mgr.hantrodec_data.iosize), subsys_mgr.iosize,
	       HXDEC_MAX_CORES * sizeof(unsigned int));
	gf_memcpy((unsigned int *)(subsys_mgr.hantrodec_data.irq), subsys_mgr.irq,
	       HXDEC_MAX_CORES * sizeof(int));
	// Check if APB arbiter is existed

	if (vcmd) {
		vcmd_offset = subsys_mgr.vpu_subsys[0].base_addr + subsys_mgr.vpu_subsys[0].submodule_offset[HW_VCMD];
	} else {
		vcmd_offset = 0;
	}

	vcmd_hwregs = gf_device->mmio_base + vcmd_offset;

	if (gf_read_mmio32((vcmd_hwregs + 0x3C)) & 0x400)
		has_apb_arbiter = 1;
	if (has_apb_arbiter) {
		u32 arbiter_cfg;
		arbiter_cfg = gf_read_mmio32((vcmd_hwregs + VCMD_REGISTER_ARBITER_CONFIG_OFFSET));
		gf_write_mmio32((vcmd_hwregs + VCMD_REGISTER_ARBITER_CONFIG_OFFSET), (arbiter_cfg | VCMD_ARBITER_ENABLE << 8) );
		gf_write_mmio32((vcmd_hwregs + VCMD_REGISTER_ARB_OFFSET),0x0);
		gf_write_mmio32((vcmd_hwregs + VCMD_REGISTER_ARB_OFFSET),0x1 );
		while ((gf_read_mmio32((vcmd_hwregs + VCMD_REGISTER_ARB_OFFSET)) & 0x4) == 0)
			schedule();
	}

	has_apb_arbiter = 0;

	if (vcmd) {
		vcmd_offset = subsys_mgr.vpu_subsys[1].base_addr + subsys_mgr.vpu_subsys[1].submodule_offset[HW_VCMD];
	} else {
		vcmd_offset = 0;
	}

	vcmd_hwregs = gf_device->mmio_base + vcmd_offset;

	if (gf_read_mmio32((vcmd_hwregs + 0x3C)) & 0x400)
		has_apb_arbiter = 1;
	if (has_apb_arbiter) {
		u32 arbiter_cfg;
		arbiter_cfg = gf_read_mmio32((vcmd_hwregs + VCMD_REGISTER_ARBITER_CONFIG_OFFSET));
		gf_write_mmio32((vcmd_hwregs + VCMD_REGISTER_ARBITER_CONFIG_OFFSET), (arbiter_cfg | VCMD_ARBITER_ENABLE << 8) );
		gf_write_mmio32((vcmd_hwregs + VCMD_REGISTER_ARB_OFFSET),0x0);
		gf_write_mmio32((vcmd_hwregs + VCMD_REGISTER_ARB_OFFSET),0x1 );
		while ((gf_read_mmio32((vcmd_hwregs + VCMD_REGISTER_ARB_OFFSET)) & 0x4) == 0) {
			schedule();
		}
		//gf_write_mmio32((vcmd_hwregs + VCMD_REGISTER_ARB_OFFSET), 0x2);//no need to manauly set arbiter fe, as there is one cmdbuf run later, after cmdbuf frame end, hw will set this arbiter fe
	}

	//if (gf_device->is_virtfn)
	//	return 0;//not touch core/axi fe register

	for (i = 0; i < MAX_SUBSYS_NUM; i++) {
		u32 core_type = HW_VCD; /* vcd */

		if (!subsys_mgr.vpu_subsys[i].base_addr)
			continue;

		for (j = 0; j < HW_CORE_MAX; j++) {
			if (subsys_mgr.vpu_subsys[i].submodule_iosize[j]) {
				gf_debug("hantrodec: base=0x%llx, iosize=%d\n",
					(unsigned long long)subsys_mgr.vpu_subsys[i].base_addr +
					subsys_mgr.vpu_subsys[i].submodule_offset[j],
					subsys_mgr.vpu_subsys[i].submodule_iosize[j]);

				subsys_mgr.vpu_subsys[i].submodule_hwregs[j] =
					subsys_mgr.hantrodec_data.hwregs[i][j] = gf_device->mmio_base +
					subsys_mgr.vpu_subsys[i].base_addr +
					subsys_mgr.vpu_subsys[i].submodule_offset[j];

				if (subsys_mgr.vpu_subsys[i].has_apbfilter[j]) {
					subsys_mgr.apbfilter_cfg[i][j].has_apbfilter = 1;
					hwid = gf_read_mmio32(
						   (subsys_mgr.hantrodec_data.hwregs[i][core_type] +
						   HANTRODEC_HW_BUILD_ID_OFF));
					if (hwid == 0x1F58) {
						if (j == HW_VCD) {
							subsys_mgr.apbfilter_cfg[i][j].nbr_mask_regs =
							  VCD_NUM_MASK_REG;
							subsys_mgr.apbfilter_cfg[i][j].num_mode =
							  VCD_NUM_MODE;
							subsys_mgr.apbfilter_cfg[i][j].mask_reg_offset =
							  VCD_MASK_REG_OFFSET;
							subsys_mgr.apbfilter_cfg[i][j].mask_bits_per_reg =
							  VCD_MASK_BITS_PER_REG;
							subsys_mgr.apbfilter_cfg[i][j].page_sel_addr =
							  subsys_mgr.apbfilter_cfg[i][j].mask_reg_offset +
							  subsys_mgr.apbfilter_cfg[i][j].nbr_mask_regs * 4;
						}
						if (j == HW_AXIFE) {
							subsys_mgr.apbfilter_cfg[i][j].nbr_mask_regs =
							  AXIFE_NUM_MASK_REG;
							subsys_mgr.apbfilter_cfg[i][j].num_mode =
							  AXIFE_NUM_MODE;
							subsys_mgr.apbfilter_cfg[i][j].mask_reg_offset =
							  AXIFE_MASK_REG_OFFSET;
							subsys_mgr.apbfilter_cfg[i][j].mask_bits_per_reg =
							  AXIFE_MASK_BITS_PER_REG;
							subsys_mgr.apbfilter_cfg[i][j].page_sel_addr =
							  subsys_mgr.apbfilter_cfg[i][j].mask_reg_offset +
							  subsys_mgr.apbfilter_cfg[i][j].nbr_mask_regs * 4;
						}
					} else if (hwid == 0x1F59) {
						if (j == HW_VCDJ) {
							subsys_mgr.apbfilter_cfg[i][j].nbr_mask_regs =
							  VCDJ_NUM_MASK_REG;
							subsys_mgr.apbfilter_cfg[i][j].num_mode =
							  VCDJ_NUM_MODE;
							subsys_mgr.apbfilter_cfg[i][j].mask_reg_offset =
							  VCDJ_MASK_REG_OFFSET;
							subsys_mgr.apbfilter_cfg[i][j].mask_bits_per_reg =
							  VCDJ_MASK_BITS_PER_REG;
							subsys_mgr.apbfilter_cfg[i][j].page_sel_addr =
							  subsys_mgr.apbfilter_cfg[i][j].mask_reg_offset +
							  subsys_mgr.apbfilter_cfg[i][j].nbr_mask_regs * 4;
						}
						if (j == HW_AXIFE) {
							subsys_mgr.apbfilter_cfg[i][j].nbr_mask_regs =
							  AXIFE_NUM_MASK_REG;
							subsys_mgr.apbfilter_cfg[i][j].num_mode =
							  AXIFE_NUM_MODE;
							subsys_mgr.apbfilter_cfg[i][j].mask_reg_offset =
							  AXIFE_MASK_REG_OFFSET;
							subsys_mgr.apbfilter_cfg[i][j].mask_bits_per_reg =
							  AXIFE_MASK_BITS_PER_REG;
							subsys_mgr.apbfilter_cfg[i][j].page_sel_addr =
							  subsys_mgr.apbfilter_cfg[i][j].mask_reg_offset +
							  subsys_mgr.apbfilter_cfg[i][j].nbr_mask_regs * 4;
						}
					} else {
						gf_debug("hantrodec: furture APBFILTER canread those configure parameters from REG\n");
					}

					subsys_mgr.hantrodec_data.apbfilter_hwregs[i][j] =
						subsys_mgr.hantrodec_data.hwregs[i][j] +
						subsys_mgr.apbfilter_cfg[i][j].mask_reg_offset;
				} else {
					subsys_mgr.apbfilter_cfg[i][j].has_apbfilter = 0;
				}
				if (j == HW_AXIFE) {
					hwid = gf_read_mmio32(
						(subsys_mgr.hantrodec_data.hwregs[i][core_type] +
						HANTRODEC_HW_BUILD_ID_OFF));
					axife_config = gf_read_mmio32(
						(subsys_mgr.hantrodec_data.hwregs[i][j]));
					subsys_mgr.axife_cfg[i].axi_rd_chn_num = axife_config & 0x7F;
					subsys_mgr.axife_cfg[i].axi_wr_chn_num = (axife_config >> 7) & 0x7F;
					subsys_mgr.axife_cfg[i].axi_rd_burst_length = (axife_config >> 14) & 0x1F;
					subsys_mgr.axife_cfg[i].axi_wr_burst_length = (axife_config >> 22) & 0x1F;
					subsys_mgr.axife_cfg[i].fe_mode = 0; /*need to read from reg in furture*/
					if (hwid == 0x1F66)
						subsys_mgr.axife_cfg[i].fe_mode = 1;
				}

				gf_debug("hantrodec: HW %d reg[0]=0x%08x [ %s ]\n",
					j, gf_read_mmio32(
						subsys_mgr.hantrodec_data.hwregs[i][j]),
						CoreTypeStr(j));
			} else {
				subsys_mgr.hantrodec_data.hwregs[i][j] = NULL;
			}
		}
		subsys_mgr.hantrodec_data.cores++;
	}

	/* check for correct HW */
	if (!CheckHwId(&subsys_mgr.hantrodec_data)) {
		return -EBUSY;
	}

	return 0;
}

int hantrodec_isr(struct lisuan_device *gf_dev)
{
	unsigned long flags = 0;
	unsigned int handled = 0;
	int i;
	u8 *hwregs;
	#ifdef SUPPORT_DBGFS
		u64 time_num;
		struct timeval time_now;
	#endif
	hantrodec_t *dev = &subsys_mgr.hantrodec_data;
	u32 irq_status_dec;

	spin_lock_irqsave(&subsys_mgr.owner_lock, flags);

	for (i = 0; i < dev->cores; i++) {
		u32 core_type = HW_VCD; /* vcd */

		if (!dev->hwregs[i][core_type] && dev->hwregs[i][HW_VCDJ])
			core_type = HW_VCDJ; /* vcdj */

		hwregs = dev->hwregs[i][core_type];

		/* interrupt status register read */
		irq_status_dec = gf_read_mmio32(hwregs + HANTRODEC_IRQ_STAT_DEC_OFF);

		if (irq_status_dec & HANTRODEC_DEC_IRQ) {
#ifdef SUPPORT_DBGFS
			subsys_mgr.decode_state[i] = DONE;
#if (KERNEL_VERSION(4, 19, 94) > LINUX_VERSION_CODE)

			do_gettimeofday(&time_now);
			time_num = time_now.tv_sec * 1000 + time_now.tv_usec / 1000;
#else
			struct timespec64 time_now;

			ktime_get_real_ts64(&time_now);
			time_num = time_now.tv_sec * 1000 + time_now.tv_nsec / 1000000;
#endif
			subsys_mgr.hw_return_time[i][subsys_mgr.hw_r_index[i]] = time_num;
			subsys_mgr.hw_r_index[i] = subsys_mgr.hw_r_index[i] + 1;
			if (subsys_mgr.hw_r_index[i] == MAX_RESERVED_TIME)
				subsys_mgr.hw_r_index[i] = 0;
#endif
		/* clear dec IRQ */

			irq_status_dec &= (~HANTRODEC_DEC_IRQ);
			gf_write_mmio32((hwregs + HANTRODEC_IRQ_STAT_DEC_OFF),irq_status_dec);

			gf_debug("decoder IRQ received! core %d\n", i);

			atomic_inc(&subsys_mgr.irq_rx);

			subsys_mgr.dec_irq |= (1 << i);

			wake_up_interruptible_all(&subsys_mgr.dec_wait_queue);
			handled++;
		}
	}

	spin_unlock_irqrestore(&subsys_mgr.owner_lock, flags);

	if (!handled) {
		gf_debug("IRQ received, but not hantrodec's!\n");
	}

	(void)hwregs;
	return handled;
}

/*
 * Function name   : ResetAsic
 * Description     : reset asic (only VCD supports reset)

 * Return type     :
 */
static void ResetAsic(hantrodec_t *dev)
{
	int i, j;
	u32 status;

	for (j = 0; j < dev->cores; j++) {
		if (!dev->hwregs[j][HW_VCD])
			continue;

		status = gf_read_mmio32((dev->hwregs[j][HW_VCD] +
						  HANTRODEC_IRQ_STAT_DEC_OFF));

		if (status & HANTRODEC_DEC_E) {
			/* abort with IRQ disabled */
			status = HANTRODEC_DEC_ABORT |
					 HANTRODEC_DEC_IRQ_DISABLE;
			gf_write_mmio32(
					  (dev->hwregs[j][HW_VCD] +
					  HANTRODEC_IRQ_STAT_DEC_OFF),status );
		}

		if (IS_G1(dev->hw_id[j][HW_VCD]))
			/* reset PP */
			gf_write_mmio32(
					  (dev->hwregs[j][HW_VCD] +
					  HANTRO_IRQ_STAT_PP_OFF), 0);

		for (i = 4; i < dev->iosize[j]; i += 4)
			gf_write_mmio32(
					  (dev->hwregs[j][HW_VCD] +
					  i), 0);
	}
}

/*
 * Function name   : dump_regs
 * Description     : Dump registers

 * Return type     :
 */
#ifdef HANTRODEC_DEBUG
void dump_regs(hantrodec_t *dev)
{
	int i, c;

	gf_debug("Reg Dump Start\n");
	for (c = 0; c < dev->cores; c++) {
		for (i = 0; i < dev->iosize[c]; i += 4 * 4) {
			gf_debug("\toffset %04X: %08X  %08X  %08X  %08X\n", i,
			       gf_read_mmio32(dev->hwregs[c][HW_VCD] + i),
	gf_read_mmio32((dev->hwregs[c][HW_VCD] + i + 4)),
	gf_read_mmio32((dev->hwregs[c][HW_VCD] + i + 16)),
	gf_read_mmio32((dev->hwregs[c][HW_VCD] + i + 24)));
		}
	}
	gf_debug("Reg Dump End\n");
}
#endif
