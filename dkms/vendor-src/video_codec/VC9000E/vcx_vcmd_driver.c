/*
 * Copyright (c) 2015, Verisilicon Inc. - All Rights Reserved
 * Copyright (c) 2011-2014, Google Inc. - All Rights Reserved
 *
 * This software is dual licensed, either GPL-2.0 or BSD-3-Clause, at your
 * option.
 *
 ********************************************************************************
 *
 * GPL-2.0
 *
 ********************************************************************************
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51 Franklin
 * Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 ********************************************************************************
 *
 * Alternatively, This software may be distributed under the terms of
 * BSD-3-Clause, in which case the following provisions apply instead of the ones
 * mentioned above :
 *
 ********************************************************************************
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ********************************************************************************
 */
#include <linux/kthread.h>
#include "vcmdswhwregisters.h"
#include "bidirect_list.h"
#include "vcx_driver.h"
#include "vcx_vcmd_priv.h"
#include "vcx_abnormal_irq.h"
#include "vcx_vcmd_cfg.h"
#ifdef SUPPORT_DBGFS
#include "vcx_vcmd_dbgfs.h"
#endif
#include "vc9000_common.h"

#include "gf_os_interface.h"
#include "lisuan_device.h"
#include "lisuan_drm.h"
#include "vcx_vcmd_driver.h"
#include "lisuan_gem.h"
#include "vcx_axife.h"


/****************************************************************
 * Macro definitions
 ***************************************************************/
#define TIMEOUT_IRQ_TIMER

/*---------------------------------------------------------------
 * Macros related to sub-system config
 *---------------------------------------------------------------
 */
/* VCMD master_out_clk mode */
#define APB_CLK_ON	(1)
#define APB_CLK_OFF	(0)
#define APB_CLK_MODE	APB_CLK_ON

/*---------------------------------------------------------------
 * Macros related to debug
 *---------------------------------------------------------------
 */


//#define IRQ_SIMULATION

/*---------------------------------------------------------------
 * Macros related to PCIe Platform Config
 *---------------------------------------------------------------
 */
#ifdef PCIE_EN
/*******************PCIE CONFIG*************************/
#ifdef EMU
#define VCMD_BUF_POOL_OFFSET               0x4800000
#define PCI_VENDOR_ID_HANTRO           0x1d9b//0x10ee//0x16c3
#define PCI_DEVICE_ID_HANTRO       0xface//0x8014// 0x7011
/* Base address got control register */
#define PCI_H2_BAR              2
/* Base address DDR register */
#define PCI_DDR_BAR             4
#else // EMU
#define VCMD_BUF_POOL_OFFSET               0x800000
#define PCI_VENDOR_ID_HANTRO           0x10ee//0x16c3
#define PCI_DEVICE_ID_HANTRO       0x8014// 0x7011
/* Base address got control register */
#define PCI_H2_BAR              4
/* Base address DDR register */
#define PCI_DDR_BAR             0
#endif //EMU
#endif // PCIE_EN

/*---------------------------------------------------------------
 * Macros related to dev/process workload management
 *---------------------------------------------------------------
 */
#define VCMD_WORKLOAD_UNIT   (8192L * 4096L)
#define VCMD_INTR_INTERVAL   (VCMD_WORKLOAD_UNIT * 1)
#define PROCESS_MAX_WORKLOAD (VCMD_WORKLOAD_UNIT * 32)

#define GET_BITS(val, start, end)    (((unsigned long long)(val) >> (start)) & ((1ULL << ((end) - (start) + 1)) - 1))

/****************************************************************
 * external/global variables declarations
 ***************************************************************/
int vsi_kloglvl = LOGLVL_WARNING;

/****************************************************************
 * local functions declarations
 ***************************************************************/
static struct cmd_jmp_t *_get_jmp_cmd(struct cmdbuf_obj *obj);
static struct cmd_end_t *_get_end_cmd(struct cmdbuf_obj *obj);

/****************************************************************
 * watchdog, irq_simulation and debug functions
 ***************************************************************/

#ifdef IRQ_SIMULATION
struct timer_manager {
	void *vcmd_mgr;
	void *obj;
};
static struct timer_list irq_simul_timer[SLOT_NUM_CMDBUF];
struct timer_manager irq_simul_ctx[SLOT_NUM_CMDBUF];

void get_random_bytes(void *buf, int nbytes);
/**
 * @brief trigger isr processing for irq simulation.
 */
void _irq_simul_trigger_isr(struct timer_list *timer)
{
	struct cmdbuf_obj *obj;
	u32 timer_id = 0;

	timer_id = timer - irq_simul_timer;
	if (irq_simul_ctx[timer_id].obj) {
		obj = (struct cmdbuf_obj *)irq_simul_ctx[timer_id].obj;
	} else {
		vcmd_klog(LOGLVL_ERROR, "tigger isr failed, the cmdbuf obj is NULL\n");
		return;
	}

	vcmd_klog(LOGLVL_FLOW, "trigger core 0 irq\n");
	hantroenc_vcmd_isr(obj->core_id, irq_simul_ctx[timer_id].vcmd_mgr);
	del_timer(timer);
	irq_simul_ctx[timer_id].obj = NULL;
}

/**
 * @brief add timer for irq simulation.
 */
void _irq_simul_add_timer(struct cmdbuf_obj *obj)
{
	u64 random_num;
	struct timer_list *temp_timer = NULL;

	//get_random_bytes(&random_num, sizeof(u32));
	random_num = (u32)((u64)100 * obj->workload / VCMD_WORKLOAD_UNIT + 50);
	vcmd_klog(LOGLVL_FLOW, "random_num=%lld\n", random_num);

	temp_timer = &irq_simul_timer[obj->cmdbuf_id];
	irq_simul_ctx[obj->cmdbuf_id].obj = (void *)obj;

	//if (obj->core_id==0)
	if (temp_timer) {
		//init_timer(&timer0);
		//timer0.function =
		//hantrovcmd_trigger_irq_0;
		timer_setup(temp_timer, _irq_simul_trigger_isr, 0);
		//the expires time is 1s
		temp_timer->expires = jiffies + random_num * HZ / 10;
		add_timer(temp_timer);
	}
}

/**
 * @brief init timers for irq simulation.
 */
void _irq_simul_init(void *vcmd_mgr)
{
	u32 i;

	for (i = 0; i < SLOT_NUM_CMDBUF; i++) {
		irq_simul_ctx[i].obj = NULL;
		irq_simul_ctx[i].vcmd_mgr = vcmd_mgr;
	}
}
#endif //IRQ_SIMULATION

#ifdef SUPPORT_WATCHDOG
/**
 * @brief hook function for system-driver to do further process
 *  for tiggered watchdog.
 */
static void hook_vcmd_watchdog(struct hantrovcmd_dev *dev, u32 id,
							int succeed)
{
	if (succeed) {
		vcmd_klog(LOGLVL_ERROR, "axife[%d] flush succeed!!", id);
	} else {
		vcmd_klog(LOGLVL_ERROR, "axife[%d] flush failed, need to re-power sub-system",
				  id);
	}
}

/**
 * @brief stop vcmd when watchdog triggered
 */
static int watchdog_stop_vcmd(struct hantrovcmd_dev *dev)
{
	int loop_cnt = 0;
	unsigned long flags = 0;
	u32 state;

	spin_lock_irqsave(dev->spinlock, flags);
	state = vcmd_get_register_value((const void *)dev->hwregs,
				dev->reg_mirror, HWIF_VCMD_WORK_STATE) & 0x7;

	if (state == HW_WORK_STATE_IDLE || state == HW_WORK_STATE_PEND) {
		dev->state = VCMD_STATE_POWER_ON;
		spin_unlock_irqrestore(dev->spinlock, flags);
		return 0;
	}

	//if state is not in IDLE/PEND, abort VCMD by sw.
	vcmd_set_reg_mirror(dev->reg_mirror, HWIF_VCMD_ABORT_MODE, 0x1);
	vcmd_write_register_value((const void *)dev->hwregs,
					dev->reg_mirror,
					HWIF_VCMD_START_TRIGGER, 0);
	dev->state = VCMD_STATE_POWER_ON;
	spin_unlock_irqrestore(dev->spinlock, flags);

	//wait vcmd core aborted and vcmd enters IDLE mode.
	do {
		state = vcmd_get_register_value((const void *)dev->hwregs,
									dev->reg_mirror, HWIF_VCMD_WORK_STATE);
		if (state == HW_WORK_STATE_IDLE) {
			//aborted
			return 0;
		}
		mdelay(10); // wait 10ms
	} while (++loop_cnt < 100);

	vcmd_klog(LOGLVL_ERROR, "%s, can't go to IDLE\n", __func__);
	return -1;
}

extern void watchdog_stop_vce(volatile u8 *reg_base, spinlock_t *spinlock);
/**
 * @brief process when watchdog triggered.
 */
static void _vcmd_watchdog_process(struct hantrovcmd_dev *dev)
{
	struct vcmd_subsys_info *subsys = dev->subsys_info;
	int succeed, i;
	u32 axife_id;

	watchdog_stop_vcmd(dev);
	watchdog_stop_vce(subsys->hwregs[SUB_MOD_MAIN], dev->spinlock);

#ifdef HANTROAXIFE_SUPPORT
	for (i = 0, axife_id = SUB_MOD_AXIFE0; i < 2; i++,
		axife_id = SUB_MOD_AXIFE1) {
		succeed = 1;
		if (subsys->reg_off[axife_id] == 0xffff)
			continue;
		if (AXIFEFlush(subsys->hwregs[axife_id]) == -1)
			succeed = 0;
		hook_vcmd_watchdog(dev, i, succeed);
	}
#endif
}
#endif //SUPPORT_WATCHDOG
/**
 * @brief process external vcmd timeout.
 */
static void hook_vcmd_external_timeout(void *_dev)
{
	unsigned long flags = 0;
	struct hantrovcmd_dev *dev = (struct hantrovcmd_dev *)_dev;

	spin_lock_irqsave(dev->spinlock, flags);
	dev->state = VCMD_STATE_IDLE;
	spin_unlock_irqrestore(dev->spinlock, flags);

	vcmd_klog(LOGLVL_ERROR, "vcmd abort is not waited, the timeout is from external system!\n");

	//need to do sub-system reset
}

/**
 * @brief To check vcmd_mgr/dev's actions which need kthread to process
 * @return int: 0: no actions; 1: have actions
 */
static int _vcmd_kthread_actions(vcmd_mgr_t *vcmd_mgr,
			struct hantrovcmd_dev **dev)
{
	int ret = 0, i;

	if (vcmd_mgr->stop_kthread == 1)
		return 1;

	for (i = 0; i < vcmd_mgr->subsys_num; i++) {
		*dev = &vcmd_mgr->dev_ctx[i];
		if ((*dev)->timeout_timer_expired == 1 ||
		    (*dev)->watchdog_triggered == 1) {
			ret = 1;
			break;
		} else {
			ret = 0;
			/*TODO*/
		}
	}

	return ret;
}

/**
 * @brief vcmd kernel thread main function
 */
static int _vcmd_kthread_fn(void *data)
{
	vcmd_mgr_t *vcmd_mgr = (vcmd_mgr_t *)data;
	struct hantrovcmd_dev *dev = NULL;

	while (!kthread_should_stop()) {
		if (wait_event_interruptible(vcmd_mgr->kthread_waitq,
				_vcmd_kthread_actions(vcmd_mgr, &dev))) {
			vcmd_klog(LOGLVL_ERROR, "%s: signaled!!!\n", __func__);
			return -ERESTARTSYS;
		}

		if (dev == NULL)
			continue;

		if (dev->timeout_timer_expired == 1) {
			dev->timeout_timer_expired = 0;
			hook_vcmd_external_timeout(dev);
			continue;
		}
#ifdef SUPPORT_WATCHDOG
		else if (dev->watchdog_triggered == 1) {
			dev->watchdog_triggered = 0;
			_vcmd_watchdog_process(dev);
			continue;
		}
#endif
	}

	return 0;
}

/**
 * @brief wake up vcmd kernel thread
 */
static void _vcmd_kthread_wakeup(vcmd_mgr_t *vcmd_mgr)
{

	if (IS_ERR(vcmd_mgr->kthread))
		return;

	wake_up_interruptible_all(&vcmd_mgr->kthread_waitq);
}

/**
 * @brief create kernel thread for vcmd driver
 */
static void _vcmd_kthread_create(vcmd_mgr_t *vcmd_mgr)
{
	vcmd_mgr->stop_kthread = 0;
	init_waitqueue_head(&vcmd_mgr->kthread_waitq);
	vcmd_mgr->kthread =
		kthread_run(_vcmd_kthread_fn, (void *)vcmd_mgr, "vcmd_kthread");
	if (IS_ERR(vcmd_mgr->kthread)) {
		vcmd_klog(LOGLVL_ERROR, "create vcmd kthread failed\n");
		return;
	}
}

/**
 * @brief stop kernel thread vcmd driver
 */
static void _vcmd_kthread_stop(vcmd_mgr_t *vcmd_mgr)
{
	if (!IS_ERR(vcmd_mgr->kthread)) {
		vcmd_mgr->stop_kthread = 1;
		kthread_stop(vcmd_mgr->kthread);
		vcmd_mgr->kthread = NULL;
	}
}

#ifdef TIMEOUT_IRQ_TIMER
/**
 * @brief timer callback of vcmd timeout timer
 */
#if (KERNEL_VERSION(4, 14, 0) > LINUX_VERSION_CODE)
static void _vcmd_timeout_timer_cb(unsigned long arg)
#else
static void _vcmd_timeout_timer_cb(struct timer_list *timer)
#endif
{
#if (KERNEL_VERSION(4, 14, 0) > LINUX_VERSION_CODE)
	struct timer_list *timer = (struct timer_list *)arg;
#endif
	vcmd_mgr_t *vcmd_mgr;
	struct hantrovcmd_dev *dev;

	dev = container_of(timer, struct hantrovcmd_dev, timeout_timer);
	vcmd_mgr = (vcmd_mgr_t *)dev->handler;

	dev->timeout_timer_expired = 1;

	_vcmd_kthread_wakeup(vcmd_mgr);
}

/**
 * @brief init timer for vcmd timeout
 */
static void _vcmd_timeout_add_timer(struct hantrovcmd_dev *dev)
{
	struct timer_list *timer = &dev->timeout_timer;

	dev->timeout_timer_active = 1;
#if (KERNEL_VERSION(4, 14, 0) > LINUX_VERSION_CODE)
	init_timer(timer);
	timer->expires =  jiffies + HZ * 350 / 1000; // 350ms
	timer->function = _vcmd_timeout_timer_cb;
	timer->data = (unsigned long)timer;
	add_timer(timer);
#else
	timer_setup(timer, _vcmd_timeout_timer_cb, 0);
	timer->expires =  jiffies + HZ * 350 / 1000;
	add_timer(timer);
#endif
}

/**
 * @brief delete vcmd timeout timer
 */
static void _vcmd_timeout_del_timer(struct hantrovcmd_dev *dev)
{
	if (dev->timeout_timer_active) {
#if DRM_VERSION_CODE >= KERNEL_VERSION(6,15,0)
		timer_delete(&dev->timeout_timer);
#else
		del_timer(&dev->timeout_timer);
#endif
		dev->timeout_timer_active = 0;
	}
}
#endif //TIMEOUT_IRQ_TIMER

#ifdef SUPPORT_WATCHDOG
/**
 * @brief wait jobs count that need to be finished from dev work_list
 */
static u32 dev_wait_job_num(struct hantrovcmd_dev *dev)
{
	struct cmdbuf_obj *obj;
	bi_list_node *node = dev->work_list.head;
	u32 num = 0;

	while (node) {
		obj = (struct cmdbuf_obj *)node->data;
		if (obj->cmdbuf_run_done == 0) {
			num++;
			if (obj->has_jmp_cmd == 0 || obj->jmp_ie)
				return num;
		}
		node = node->next;
	}

	return num;
}

/**
 * @brief timer callback function of watchdog
 */
#if (KERNEL_VERSION(4, 14, 0) > LINUX_VERSION_CODE)
static void _vcmd_watchdog_cb(unsigned long arg)
#else
static void _vcmd_watchdog_cb(struct timer_list *timer)
#endif
{
#if (KERNEL_VERSION(4, 14, 0) > LINUX_VERSION_CODE)
	struct timer_list *timer = (struct timer_list *)arg;
#endif
	vcmd_mgr_t *vcmd_mgr;
	struct hantrovcmd_dev *dev;

	dev = container_of(timer, struct hantrovcmd_dev, watchdog_timer);
	vcmd_mgr = (vcmd_mgr_t *)dev->handler;

	dev->watchdog_triggered = 1;
	_vcmd_kthread_wakeup(vcmd_mgr);
}

/**
 * @brief init vcmd watchdog
 */
static void _vcmd_watchdog_start(struct hantrovcmd_dev *dev)
{
	struct timer_list *timer = &dev->watchdog_timer;

#if (KERNEL_VERSION(4, 14, 0) > LINUX_VERSION_CODE)
	init_timer(timer);
	timer->function = _vcmd_watchdog_cb;
	timer->data = (unsigned long)timer;
#else
	timer_setup(timer, _vcmd_watchdog_cb, 0);
#endif
	dev->watchdog_active = 1;
}

/**
 * stop vcmd watchdog
 */
static void _vcmd_watchdog_stop(struct hantrovcmd_dev *dev)
{
	if (dev->watchdog_active) {
		del_timer(&dev->watchdog_timer);
		dev->watchdog_active = 0;
	}
}

/**
 * feed and start vcmd watchdog
 */
static void _vcmd_watchdog_feed(struct hantrovcmd_dev *dev)
{
	u32 num = 0;

	num = dev_wait_job_num(dev);
	if (num == 0) {
		if (dev->watchdog_active)
			_vcmd_watchdog_stop(dev);
	} else {
		if (dev->watchdog_active == 0)
			_vcmd_watchdog_start(dev);
		mod_timer(&dev->watchdog_timer,
			(jiffies + num * (HZ * ONE_JOB_WAIT_TIME / 1000)));
	}
}
#endif //SUPPORT_WATCHDOG



void _dbg_log_instr(u32 offset, u32 instr, u32 *size, char *str)
{
	u32 opcode = instr & OPCODE_MASK;

	if (opcode == OPCODE_WREG) {
		int length = ((instr >> 16) & 0x3FF);

		sprintf(str, "current cmdbuf data %u = 0x%08x => [%s %s %d 0x%x]\n",
			offset, instr, "WREG", ((instr >> 26) & 0x1) ? "FIX" : "",
			   length, (instr & 0xFFFF));
		*size = ((length + 2) >> 1) << 1;
	} else if (opcode == OPCODE_END) {
		sprintf(str, "current cmdbuf data %u = 0x%08x => [%s]\n", offset,
			instr, "END");
		*size = 2;
	} else if (opcode == OPCODE_NOP) {
		sprintf(str, "current cmdbuf data %u = 0x%08x => [%s]\n", offset,
			instr, "NOP");
		*size = 2;
	} else if (opcode == OPCODE_RREG) {
		int length = ((instr >> 16) & 0x3FF);

		sprintf(str, "current cmdbuf data %u = 0x%08x => [%s %s %d 0x%x]\n",
			offset, instr, "RREG", ((instr >> 26) & 0x1) ? "FIX" : "",
			   length, (instr & 0xFFFF));
		*size = 4;
	} else if (opcode == OPCODE_JMP) {
		sprintf(str, "current cmdbuf data %u = 0x%08x => [%s %s %s]\n",
			offset, instr, "JMP", ((instr >> 26) & 0x1) ? "RDY" : "",
			   ((instr >> 25) & 0x1) ? "IE" : "");
		*size = 4;
	} else if (opcode == OPCODE_STALL) {
		sprintf(str, "current cmdbuf data %u = 0x%08x => [%s %s 0x%x]\n",
			offset, instr, "STALL", ((instr >> 26) & 0x1) ? "IM" : "",
			   (instr & 0xFFFF));
		*size = 2;
	} else if (opcode == OPCODE_CLRINT) {
		sprintf(str, "current cmdbuf data %u = 0x%08x => [%s %u 0x%x]\n",
			offset, instr, "CLRINT", (instr >> 25) & 0x3,
			   (instr & 0xFFFF));
		*size = 2;
	} else if (opcode == OPCODE_M2M) {
		sprintf(str, "current cmdbuf data %u = 0x%08x => [%s]\n",
			offset, instr, "M2M");
		*size = 6;
	} else {
		sprintf(str, "current cmdbuf data %u = 0x%08x => [%s]\n",
			offset, instr, "UNKNOWN CMD");
		*size = 1;
	}
}

#ifdef VCMD_DEBUG_INTERNAL
static void _dbg_log_last_cmd(struct cmdbuf_obj *obj)
{
	u32 *p, len, offset, size;
	char log_buf[512];

	vcmd_klog(LOGLVL_FLOW, "last cmdbuf content:\n");
	if (obj->has_jmp_cmd) {
		p = (void *)_get_jmp_cmd(obj);
		len = sizeof(struct cmd_end_t) / sizeof(u32);
	} else {
		p = (void *)_get_end_cmd(obj);
		len = sizeof(struct cmd_end_t) / sizeof(u32);
	}

	offset = p - obj->cmd_va;
	_dbg_log_instr(offset, *p, &size, log_buf);
	vcmd_klog(LOGLVL_FLOW, "%s", log_buf);
	len--;
	offset++;

	while (len--) {
		vcmd_klog(LOGLVL_FLOW, "current cmdbuf data %d = 0x%x\n",
						offset, *(obj->cmd_va + offset));
		offset++;
	}
}

static void _dbg_log_cmdbuf(struct cmdbuf_obj *obj)
{
	u32 i, instr = 0, size = 0;
	char log_buf[512];

	vcmd_klog(LOGLVL_FLOW, "vcmd link, current cmdbuf content\n");
	for (i = 0; i < obj->cmdbuf_size / 4; i++) {
		if (i == instr) {
			//gf_memset(log_buf, 0, sizeof(log_buf));
			_dbg_log_instr(i, *(obj->cmd_va + i), &size, log_buf);
			vcmd_klog(LOGLVL_FLOW, "%s", log_buf);
			instr += size;
		} else {
			vcmd_klog(LOGLVL_FLOW, "current cmdbuf data %d = 0x%x\n",
					i, *(obj->cmd_va + i));
		}
	}
}

static void _dbg_log_dev_regs(struct hantrovcmd_dev *dev, u32 dump)
{
	u32 i, reg_val;

	if (dump) {
		for (i = 0; i < ASIC_VCMD_SWREG_AMOUNT; i++) {
			reg_val = vcmd_read_reg((const void *)dev->hwregs, i * 4);
			vcmd_klog(LOGLVL_FLOW, "vcmd swreg%d: 0x%x\n", i, reg_val);
		}
	} else {
		for (i = 0; i < ASIC_VCMD_SWREG_AMOUNT; i++) {
			reg_val = *(dev->reg_mem_va + i);
			vcmd_klog(LOGLVL_FLOW, "ddr vcmd swreg%d: 0x%x\n", i, reg_val);
		}
	}
}
#endif

static void printk_vcmd_register_debug(const void *hwregs, char *info)
{
#ifdef VCMD_DEBUG_INTERNAL
	u32 i, fordebug;

	for (i = 0; i < ASIC_VCMD_SWREG_AMOUNT; i++) {
		fordebug = vcmd_read_reg((const void *)hwregs, i * 4);
		vcmd_klog(LOGLVL_FLOW, "%s vcmd register %d:0x%x\n", info, i,
			fordebug);
	}
#endif
}

static void _log_ioctl_cmd(unsigned int cmd)
{
	static unsigned int last_cmd;

	if ((cmd == HANTRO_IOCH_POLLING_CMDBUF) && (last_cmd == cmd))
		return;
	last_cmd = cmd;
	vcmd_klog(LOGLVL_FLOW, "ioctl cmd 0x%08x\n", cmd);
}

/*======================= cmdbuf object management ================*/
/**
 * @brief initialize all of cmdbuf objs of vcmd driver handler.
 */
static void vcmd_init_objs(vcmd_mgr_t *vcmd_mgr)
{
	u32 i;
	struct cmdbuf_obj *obj;
	struct noncache_mem *m0, *m1;

	m0 = &vcmd_mgr->mem_vcmd;
	m1 = &vcmd_mgr->mem_status;
	for (i = 0; i < SLOT_NUM_CMDBUF; i++) {
		obj = &vcmd_mgr->objs[i];
		obj->cmdbuf_id = i;
		obj->cmdbuf_size = SLOT_SIZE_CMDBUF;
		obj->cmd_va = m0->va + CMDBUF_OFF_32(i);
		obj->cmd_pa = m0->pa + CMDBUF_OFF(i);
		obj->mmu_cmd_ba = m0->mmu_ba + CMDBUF_OFF(i);

		obj->status_size = SLOT_SIZE_STATUSBUF;
		obj->status_va = m1->va + STATUSBUF_OFF_32(i);
		obj->status_pa = m1->pa + STATUSBUF_OFF(i);
		obj->mmu_status_ba = m1->mmu_ba + STATUSBUF_OFF(i);
	}
}

/**
 * @brief initialize all of cmdbuf nodes of vcmd driver handler.
 */
static void vcmd_init_nodes(vcmd_mgr_t *vcmd_mgr)
{
	u32 i;

	for (i = 0; i < SLOT_NUM_CMDBUF; i++)
		vcmd_mgr->nodes[i].data = (void *)&vcmd_mgr->objs[i];
}

/**
 * @brief reset flag/status of cmdbuf obj specified by id.
 */
static void reset_cmdbuf_obj(vcmd_mgr_t *vcmd_mgr, u32 id)
{
	struct cmdbuf_obj *obj = &vcmd_mgr->objs[id];

	obj->executing_status = CMDBUF_EXE_STATUS_OK;
	obj->cmdbuf_run_done = 0;
	obj->cmdbuf_linked = 0;
	obj->cmdbuf_need_remove = 0;
	obj->core_id = 0xFFFF;
	obj->cmdbuf_size = SLOT_SIZE_CMDBUF;

	obj->has_jmp_cmd = 1;
	obj->jmp_ie = 0;
}

/**
 * @brief reset pointer of cmdbuf node specified by id.
 */
static void reset_cmdbuf_node(vcmd_mgr_t *vcmd_mgr, u32 id)
{
	struct bi_list_node  *node;

	node = &vcmd_mgr->nodes[id];
	node->next = NULL;
	node->previous = NULL;

	node = &vcmd_mgr->po_jobs[id];
	node->next = NULL;
	node->previous = NULL;
	node->data = NULL;
}

/**
 * @brief put a node to tail of si-list.
 */
static void _si_list_put(struct si_linked_list *list,
							struct si_linked_node *node)
{
	if (list->head == NULL) {
		list->head = list->tail = node;
	} else {
		list->tail->next = node;
		list->tail = node;
	}
	node->next = NULL;
}

/**
 * @brief get & remove a node from head of si-list.
 * @return struct si_linked_node *: NULL: no node got; other: the node.
 */
static struct si_linked_node *_si_list_get(struct si_linked_list *list)
{
	struct si_linked_node *node = list->head;

	if (list->head) {
		list->head = node->next;
		if (list->head == NULL)
			list->tail = NULL;
	}

	return node;
}

/**
 * @brief init a si-list.
 */
static void vcmd_init_si_list(struct si_linked_list *list)
{

	spin_lock_init(&list->spinlock);
	list->head = NULL;
	list->tail = NULL;
}

/**
 * @brief init free obj (cmdbuf) list of vcmd driver handler.
 */
static void vcmd_init_free_obj_list(vcmd_mgr_t *vcmd_mgr)
{
	u32 i;
	struct si_linked_list *list = &vcmd_mgr->free_obj_list;

	vcmd_init_si_list(list);
	sema_init(&vcmd_mgr->free_obj_sema, SLOT_NUM_CMDBUF);

	for (i = 0; i < SLOT_NUM_CMDBUF; i++) {
		list->nodes[i].data = (void *)&vcmd_mgr->objs[i];
		_si_list_put(list, &list->nodes[i]);
	}
}

/**
 * @brief acquire a free cmdbuf.
 * @param u32 *id: to store acquired free cmdbuf id.
 * @return int: 0: succeed; otherwise: failed.
 */
static int acquire_cmdbuf(vcmd_mgr_t *vcmd_mgr, u32 *id)
{
	struct si_linked_list *list = &vcmd_mgr->free_obj_list;
	struct si_linked_node *node;
	struct cmdbuf_obj *obj;
	unsigned long flags = 0;

	//if (down_interruptible(&vcmd_mgr->free_obj_sema)) {
	//	vcmd_klog(LOGLVL_ERROR, "%s: sema-down is interrupted!", __func__);
	//	return -1;
	//}

	spin_lock_irqsave(&list->spinlock, flags);
	node = _si_list_get(list);
	spin_unlock_irqrestore(&list->spinlock, flags);
	if (node == NULL) {
		vcmd_klog(LOGLVL_ERROR, "%s: get NULL node!", __func__);
		return -1;
	}

	obj = (struct cmdbuf_obj *)node->data;
	*id = obj->cmdbuf_id;
	return 0;
}

/**
 * @brief return (release) a cmdbuf specified by id.
 */
static void return_cmdbuf(vcmd_mgr_t *vcmd_mgr, u32 id)
{
	unsigned long flags = 0;

	struct si_linked_list *list = &vcmd_mgr->free_obj_list;
	struct si_linked_node *node = &list->nodes[id];
	struct cmdbuf_obj *obj = (struct cmdbuf_obj *)node->data;

	obj->po = NULL;
	obj->cmdbuf_run_done = 0;
	spin_lock_irqsave(&list->spinlock, flags);
	_si_list_put(list, node);
	spin_unlock_irqrestore(&list->spinlock, flags);
	up(&vcmd_mgr->free_obj_sema);
}

/**
 * @brief get cmdbuf node by cmd physical addr.
 * @param addr_t pa: the cmd physical addr.
 * @return bi_list_node *: NULL: no cmdbuf found; otherwise: cmdbuf node.
 */
static bi_list_node *get_cmdbuf_node_by_addr(vcmd_mgr_t *vcmd_mgr,
					struct hantrovcmd_dev *dev, addr_t pa)
{
	bi_list_node *node;
	struct cmdbuf_obj *obj;

	u32 id;
	addr_t pa_base = vcmd_mgr->mem_vcmd.pa - vcmd_mgr->pa_trans_offset;

	id = (pa - pa_base) / SLOT_SIZE_CMDBUF;
	if (id > SLOT_NUM_CMDBUF) {
		vcmd_klog(LOGLVL_ERROR, "cmdbuf_id greater than the ceiling !!\n");
		return NULL;
	}

	obj = &vcmd_mgr->objs[id];
	node = &vcmd_mgr->nodes[id];

	if (obj->core_id != dev->core_id) {
		vcmd_klog(LOGLVL_ERROR, "cmdbuf is not in dev[%d] list !!\n", dev->core_id);
		return NULL;
	}

	return node;
}

/**
 * @brief get the va of vcmdbuf obj's JMP cmd.
 */
static struct cmd_jmp_t *_get_jmp_cmd(struct cmdbuf_obj *obj)
{
	u8 *p = (u8 *)obj->cmd_va;

	p += obj->cmdbuf_size - sizeof(struct cmd_jmp_t);
	return (struct cmd_jmp_t *)p;
}

/**
 * @brief get the va of vcmdbuf obj's END cmd.
 */
static struct cmd_end_t *_get_end_cmd(struct cmdbuf_obj *obj)
{
	u8 *p = (u8 *)obj->cmd_va;

	p += obj->cmdbuf_size - sizeof(struct cmd_end_t);
	return (struct cmd_end_t *)p;
}

/**
 * @brief set bus-address to 2 rreg cmd with dev reg mem ba,
 * the 2 rreg cmds are the 1st & the last but one cmd of specified cmdbuf.
 */
static void _set_rreg_addr(struct hantrovcmd_dev *dev, struct cmdbuf_obj *obj, int is_internal)
{
	addr_t reg_mem_ba;
	struct cmd_rreg_t *cmd_rreg;
	u8 *p;

	if (dev->hw_version_id <= HW_ID_1_0_C)
		return;

	reg_mem_ba = dev->reg_mem_ba;
	if (dev->mmu_enable)
		reg_mem_ba = (addr_t)dev->mmu_reg_mem_ba;

	//read vcmd executing ID register into ddr memory.
	if (!is_internal)
		cmd_rreg = (struct cmd_rreg_t *)(obj->cmd_va + 18 + 4 * 12);
	else
		cmd_rreg = (struct cmd_rreg_t *)(obj->cmd_va);
	CMD_SET_ADDR(cmd_rreg, reg_mem_ba + REG_ID2_CMDBUF_EXE_ID * 4);
	//read vcmd all registers into ddr memory.
	if (obj->has_jmp_cmd)
		p = (u8 *)_get_jmp_cmd(obj);
	else
		p = (u8 *)_get_end_cmd(obj);

	if (!is_internal)
		cmd_rreg = (struct cmd_rreg_t *)(p - sizeof(struct cmd_rreg_t) - sizeof(struct cmd_wreg_t) - 4);
	else
		cmd_rreg = (struct cmd_rreg_t *)(p - sizeof(struct cmd_rreg_t));
	CMD_SET_ADDR(cmd_rreg, reg_mem_ba);
}

/**
 * @brief update intr-enable bit of JMP cmd in specified cmdbuf.
 */
static void _update_jmp_ie(struct hantrovcmd_dev *dev, struct cmdbuf_obj *obj)
{
	struct cmd_jmp_t *cmd_jmp;

	if (obj->has_jmp_cmd) {
		//update dev->duration and adjust JMP_IE accordingly
		if (obj->jmp_ie) {
			dev->duration = 0;
		} else {
			dev->duration += obj->workload;
			if (dev->duration >= VCMD_INTR_INTERVAL) {
				cmd_jmp = _get_jmp_cmd(obj);
				obj->jmp_ie = 1;
				cmd_jmp->opcode |= JMP_IE(1);
			}
		}
	}
}

/**
 * @brief link 2 specified cmdbufs' data by JMP cmd of prev comdbuf.
 */
static void dev_link_cmdbuf(struct hantrovcmd_dev *dev,
								bi_list_node *prev_node,
								bi_list_node *next_node)
{
	struct cmdbuf_obj *next_obj, *prev_obj;
	struct cmd_jmp_t *cmd_jmp;
	addr_t next_ba;
	u32 op;

	if (!prev_node)
		return;

	prev_obj = (struct cmdbuf_obj *)prev_node->data;

	if (prev_obj->has_jmp_cmd) {
		cmd_jmp = _get_jmp_cmd(prev_obj);
		if (!next_node) {
			// If next cmdbuf is not available, set the RDY to 0.
			u32 op = cmd_jmp->opcode;

			cmd_jmp->opcode = OPCODE_JMP |
								JMP_RDY(0) | JMP_IE(JMP_G_IE(op)) |
								JMP_NEXT_LEN(0);
		} else {
			next_obj = (struct cmdbuf_obj *)next_node->data;
			if (dev->hw_version_id > HW_ID_1_0_C) {
				//set next cmdbuf id
				cmd_jmp->id = next_obj->cmdbuf_id;
			}
			next_ba = next_obj->cmd_pa - dev->pa_trans_offset;
			if (dev->mmu_enable)
				next_ba = (addr_t)next_obj->mmu_cmd_ba;

			CMD_SET_ADDR(cmd_jmp, next_ba);
			op = cmd_jmp->opcode;
			cmd_jmp->opcode = OPCODE_JMP |
								JMP_RDY(1) | JMP_IE(JMP_G_IE(op)) |
								JMP_NEXT_LEN((next_obj->cmdbuf_size + 7) / 8);
		}
	}

#ifdef VCMD_DEBUG_INTERNAL
	_dbg_log_last_cmd(prev_obj);
#endif
}

/**
 * @brief create a process object
 */
static struct proc_obj *create_process_object(void)
{
	struct proc_obj *po;

	po = gf_kvcalloc(1, sizeof(struct proc_obj), GFP_KERNEL);
	if (!po) {
		vcmd_klog(LOGLVL_ERROR, "%s: gf_kvcalloc failed!\n", __func__);
		return NULL;
	}

	gf_memset(po, 0, sizeof(struct proc_obj));
	spin_lock_init(&po->spinlock);
	init_waitqueue_head(&po->resource_waitq);
	init_waitqueue_head(&po->job_waitq);

	spin_lock_init(&po->job_lock);
	init_bi_list(&po->job_done_list);
	return po;
}

/**
 * @brief free a process object
 */
static void free_process_object(struct proc_obj *po)
{
	if (!po) {
		vcmd_klog(LOGLVL_ERROR, "%s: po is NULL!\n", __func__);
		return;
	}
	gf_kvfree(po);
}

/**
 * @brief add done obj to job_done_list of its po, wake-up job_waitq if needed.
 */
void proc_add_done_job(vcmd_mgr_t *vcmd_mgr, struct cmdbuf_obj *obj)
{
	u16 id = obj->cmdbuf_id;
	struct proc_obj *po = obj->po;
	struct bi_list *list = &po->job_done_list;
	unsigned long flags = 0;
	u32 is_empty, is_wait;

	spin_lock_irqsave(&po->job_lock, flags);

	if (vcmd_mgr->po_jobs[id].data) {
		//already in job-done list, do nothing
		spin_unlock_irqrestore(&po->job_lock, flags);
		return;
	}

	vcmd_mgr->po_jobs[id].data = (void *)&vcmd_mgr->objs[id];
	is_empty = (list->head == NULL);
	is_wait = po->in_wait;
	po->in_wait = 0;
	bi_list_insert_node_tail(list, &vcmd_mgr->po_jobs[id]);

	spin_unlock_irqrestore(&po->job_lock, flags);

	if (is_empty || is_wait)
		wake_up_interruptible_all(&po->job_waitq);
}

/**
 * @brief get & remove a done obj from po's job_done_list.
 * @param struct cmdbuf_obj **pobj: *pobj==NULL: get head node from list.
 *									otherwise, get specified node from list.
 * @return int: 0: no done obj; 1: obj is done.
 */
static int proc_get_done_job(vcmd_mgr_t *vcmd_mgr, struct proc_obj *po,
							struct cmdbuf_obj **pobj)
{

	struct bi_list *list = &po->job_done_list;
	bi_list_node *node;
	unsigned long flags = 0;
	struct cmdbuf_obj *obj = NULL;
	int is_done = 0;

	spin_lock_irqsave(&po->job_lock, flags);
	node = list->head;

	if (node == NULL) {
		// job done list is empty
		po->in_wait = 1;
		spin_unlock_irqrestore(&po->job_lock, flags);
		return 0;
	}

	if (*pobj == NULL) {
		//any po's cmdbuf ready, return head of job_done_list
		*pobj = (struct cmdbuf_obj *)node->data;
		is_done = 1;
	} else {
		//specified cmdbuf ready?
		obj = *pobj;
		if (obj->cmdbuf_run_done) {
			// check if the obj is in job done list
			while (node && node->data != (void *)obj)
				node = node->next;

			if (node == NULL) {
				vcmd_klog(LOGLVL_BRIEF, "%s: cmdbuf[%d] is done, but not in done-list!\n",
						__func__, obj->cmdbuf_id);
			} else {
				vcmd_klog(LOGLVL_BRIEF, "%s: cmdbuf[%d] is done!\n", __func__, obj->cmdbuf_id);
				is_done = 1;
			}
			//obj->slice_run_done = 0;
		}
	}

	if (is_done) {
		bi_list_remove_node(list, &vcmd_mgr->po_jobs[(*pobj)->cmdbuf_id]);
		vcmd_mgr->po_jobs[(*pobj)->cmdbuf_id].data = NULL;
	} else {
		po->in_wait = 1;
	}
	spin_unlock_irqrestore(&po->job_lock, flags);

	return is_done;
}

/**
 * @brief remove a job (cmdbuf node) from device work_list,
 * and de-link from cmdbuf jmp list
 * @return int: 1: succeed; 0: failed, the node's JMP cmd is still used by hw.
 */
static int dev_delink_job(struct hantrovcmd_dev *dev,
							bi_list_node *node)
{
	struct cmdbuf_obj *obj = (struct cmdbuf_obj *)node->data;

	if (obj->cmdbuf_linked == 0) {
		//already de-linked or not link into work list yet
		return 1;
	}

	if (dev->state != VCMD_STATE_WORKING ||
		obj->has_jmp_cmd == 0 || node->next) {
		//delink cmdbuf, and remove node from work list.
		dev_link_cmdbuf(dev, node->previous, node->next);
		bi_list_remove_node(&dev->work_list, node);
		obj->cmdbuf_linked = 0;
		return 1;
	}
	// Not remove the last node, its JMP cmd is needed to link new cmd.
	return 0;
}

/**
 * @brief add a job to tail of device work_list,
 *  and link to cmdbuf jmp list if needed.
 */
static int dev_add_job(struct hantrovcmd_dev *dev, bi_list_node *job_node, int is_internal)
{

	struct cmdbuf_obj *obj;

	obj = (struct cmdbuf_obj *)job_node->data;
	_set_rreg_addr(dev, obj, is_internal);

	bi_list_insert_node_tail(&dev->work_list, job_node);
	_update_jmp_ie(dev, obj);

#ifdef SUPPORT_DBGFS
	_dbgfs_record_active_start_time(dev->dbgfs_info);
#endif

	dev_link_cmdbuf(dev, job_node->previous, job_node);
	obj->core_id = dev->core_id;
	obj->cmdbuf_linked = 1;

	return 0;
}

/**
 * @brief insert job_node prior to base_node of dev work_list,
 *  and insert to cmdbuf jmp list accordingly if needed.
 */
static int dev_insert_job(struct hantrovcmd_dev *dev,
							bi_list_node *base_node,
							bi_list_node *job_node,
							int is_internal)
{
	struct cmdbuf_obj *obj;

	obj = (struct cmdbuf_obj *)job_node->data;
	_set_rreg_addr(dev, obj, is_internal);

	bi_list_insert_node_before(&dev->work_list, base_node, job_node);

#ifdef SUPPORT_DBGFS
	_dbgfs_record_active_start_time(dev->dbgfs_info);
#endif

	dev_link_cmdbuf(dev, job_node->previous, job_node);
	dev_link_cmdbuf(dev, job_node, job_node->next);
	obj->core_id = dev->core_id;
	obj->cmdbuf_linked = 1;

	return 0;
}

/**
 * @brief remove a job from dev work_list,
 *  de-link it from cmdbuf jmp list if needed, and return the cmdbuf.
 */
static int dev_remove_job(vcmd_mgr_t *vcmd_mgr, struct hantrovcmd_dev *dev,
						bi_list_node *node)
{
	struct cmdbuf_obj *obj;

	obj = (struct cmdbuf_obj *)node->data;

	vcmd_klog(LOGLVL_FLOW, "Delink and remove cmdbuf [%d] from dev [%d].\n",
			obj->cmdbuf_id, dev->core_id);
	if (node->previous) {
		vcmd_klog(LOGLVL_BRIEF, "prev cmdbuf [%d].\n",
			   ((struct cmdbuf_obj *)node->previous->data)->cmdbuf_id);
	} else {
		vcmd_klog(LOGLVL_BRIEF, "NO prev cmdbuf.\n");
	}
	if (node->next) {
		vcmd_klog(LOGLVL_BRIEF, "next cmdbuf [%d].\n",
			   ((struct cmdbuf_obj *)node->next->data)->cmdbuf_id);
	} else {
		vcmd_klog(LOGLVL_BRIEF, "NO next cmdbuf.\n");
	}

	if (dev_delink_job(dev, node))
		return_cmdbuf(vcmd_mgr, obj->cmdbuf_id);

	return 0;
}

/**
 * @brief get remain (un-do) jobs count from dev work_list,
 */
static u32 dev_get_job_num(struct hantrovcmd_dev *dev)
{
	struct cmdbuf_obj *obj;
	bi_list_node *node = dev->work_list.head;
	u32 num = 0;

	while (node) {
		obj = (struct cmdbuf_obj *)node->data;
		if (obj->cmdbuf_run_done == 0)
			num++;
		node = node->next;
	}
	return num;
}

/**
 * @brief calculate workload after specified node in dev work_list.
 */
static u32 calc_workload_after_node(bi_list_node *node)
{
	u32 sum = 0;
	struct cmdbuf_obj *obj;

	while (node) {
		obj = (struct cmdbuf_obj *)node->data;
		sum += obj->workload;
		node = node->next;
	}
	return sum;
}

/**
 * @brief check if specified device is in core_mask
 * @return int: 1: device is in core_mask; 0: not in core_mask.
 */
static int is_supported_core(u32 core_mask, u16 dev_id)
{
	if (core_mask && ((core_mask >> dev_id) & 0x1))
		return 1; //found one supported core

	return 0; //not found the supported core
}

struct sub_ip_init_cfg {
	u32 reg_id;
	u32 reg_val;
};

#ifdef HANTROAXIFE_SUPPORT
struct sub_ip_init_cfg axife_init_cfg[] = {
	{AXI_REG10_SW_FRONTEND_EN, 0x02},
	{AXI_REG11_SW_WORK_MODE, 0x02},
	{0xffff, }	//end guard
};
#endif

#ifdef HANTROMMU_SUPPORT
struct sub_ip_init_cfg mmu_init_cfg[] = {
	{MMU_REG_ADDRESS, 0x0000},		//not move, sw will update mmu addr LSB to index0
	{MMU_REG_ADDRESS_MSB, 0x0000},	//not move, sw will update mmu addr MSB to index1
	{MMU_REG_PAGE_TABLE_ID, 0x10000},
	{MMU_REG_PAGE_TABLE_ID, 0x00000},
	{MMU_REG_CONTROL, 0x01},
	{0xffff, }	//end guard
};
#endif

/**
 * @brief set reg id/val of specified sub-ip to init-cmd regs.
 */
static void _set_module_init_cmds(struct hantrovcmd_dev *dev, u32 module_id,
									struct sub_ip_init_cfg *module_cfg)
{
	u16 reg_off = dev->subsys_info->reg_off[module_id];
	u32 opcode = OPCODE_WREG | WREG_MODE(WREG_ADDR_FIX) | WREG_LEN(1);

	if (reg_off == 0xffff)
		return;

	while (module_cfg->reg_id != 0xffff) {
		dev->reg_mirror[dev->init_cmd_idx++] = opcode |
												(reg_off + module_cfg->reg_id);
		dev->reg_mirror[dev->init_cmd_idx++] = module_cfg->reg_val;
		module_cfg++;
	}
}

/**
 * @brief set init-cmd regs to init sub-ips if necessary.
 */
static void vcmd_set_init_cmds(struct hantrovcmd_dev *dev)
{
#ifdef HANTROVCMD_ENABLE_IP_SUPPORT
	u32 i;

	dev->init_cmd_idx = VCMD_REG_ID_SW_INIT_CMD0;

#ifdef HANTROAXIFE_SUPPORT
	//enable AXIFE by VCMD
	_set_module_init_cmds(dev, SUB_MOD_AXIFE0, axife_init_cfg);
	_set_module_init_cmds(dev, SUB_MOD_AXIFE1, axife_init_cfg);
#endif

#ifdef HANTROMMU_SUPPORT
	//enable MMU by VCMD
	if (dev->mmu_enable) {
		u64 mmu_addr = GetMMUAddress();

		vcmd_klog(LOGLVL_FLOW, "%s: mmu address = 0x%llx", __func__, mmu_addr);
		mmu_init_cfg[0].reg_val = (u32)mmu_addr;
		mmu_init_cfg[1].reg_val = (u32)(mmu_addr >> 32);
		_set_module_init_cmds(dev, SUB_MOD_MMU0, mmu_init_cfg);
		_set_module_init_cmds(dev, SUB_MOD_MMU1, mmu_init_cfg);
	}
#endif

	//finished with END command
	dev->reg_mirror[dev->init_cmd_idx++] = OPCODE_END;
	dev->reg_mirror[dev->init_cmd_idx++] = 0x00;
	for (i = VCMD_REG_ID_SW_INIT_CMD0; i < dev->init_cmd_idx; i++) {
		vcmd_write_reg((const void *)dev->hwregs, i*4, dev->reg_mirror[i]);
	}
#endif
}

/**
 * @brief reset all vcmd hw devices.
 */
static void vcmd_reset_asic(vcmd_mgr_t *vcmd_mgr)
{
	int i, n;
	u32 status;
	struct hantrovcmd_dev *dev = vcmd_mgr->dev_ctx;

	for (n = 0; n < vcmd_mgr->subsys_num; n++) {
		if (dev[n].hwregs) {
			//disable interrupt at first
			vcmd_write_reg((const void *)dev[n].hwregs,
					   VCMD_REGISTER_INT_CTL_OFFSET, 0x0000);
			//reset core
			vcmd_write_reg((const void *)dev[n].hwregs,
					   VCMD_REGISTER_CONTROL_OFFSET, 0x0004);
			//read status register
			status = vcmd_read_reg((const void *)dev[n].hwregs,
						   VCMD_REGISTER_INT_STATUS_OFFSET);
			//clean status register
			vcmd_write_reg((const void *)dev[n].hwregs,
					   VCMD_REGISTER_INT_STATUS_OFFSET, status);
			//when reset core need clear reg[3]
			vcmd_write_reg((const void *)dev[n].hwregs,
							VCMD_REGISTER_EXE_CMDBUF_COUNT_OFFSET, 0x0000);
			for (i = VCMD_REGISTER_CONTROL_OFFSET;
				 i < dev[n].subsys_info->io_size[SUB_MOD_VCMD]; i += 4) {
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
					   dev[n].intr_gate_mask);

			if (dev->hw_version_id >= HW_ID_1_5_2) {
				vcmd_write_reg((const void *)dev[n].hwregs,
					   VCMD_REGISTER_ARB_OFFSET,
					   0);

				vcmd_write_reg((const void *)dev[n].hwregs,
					   VCMD_REGISTER_ARBITER_CONFIG_OFFSET,
					   ENC_VCMD_ARBITER_PARAMS);
			}
		}
	}
}

/**
 * @brief reset core to the specified vcmd hw device.
 */
static void vcmd_reset_current_asic(struct hantrovcmd_dev *dev)
{
	u32 status;

	volatile u8 *hwregs = dev->hwregs;

	if (hwregs) {
		//disable interrupt at first
		vcmd_write_reg((const void *)hwregs,
				   VCMD_REGISTER_INT_CTL_OFFSET, 0x0000);
		//reset core
		vcmd_write_reg((const void *)hwregs,
				   VCMD_REGISTER_CONTROL_OFFSET, 0x0004);
		//read status register
		status = vcmd_read_reg((const void *)hwregs,
					   VCMD_REGISTER_INT_STATUS_OFFSET);
		//clean status register
		vcmd_write_reg((const void *)hwregs,
				   VCMD_REGISTER_INT_STATUS_OFFSET, status);
		//when reset core need clear reg[3]
		vcmd_write_reg((const void *)hwregs,
						VCMD_REGISTER_EXE_CMDBUF_COUNT_OFFSET, 0x0000);
	}
}

/**
 * @brief start a vcmd hw device to run the 1st not-done job in its work list.
 */
static void vcmd_start(struct hantrovcmd_dev *dev)
{
	struct cmdbuf_obj *obj = NULL;
	const void *hwregs = (const void *)dev->hwregs;
	bi_list_node *node;
	u32 *reg_mirror = dev->reg_mirror;
	addr_t cmd_ba;
	u32 ba_msb = 0;

	if (dev->state == VCMD_STATE_WORKING) {
		vcmd_klog(LOGLVL_BRIEF, "%s: vcmd is already in working state!\n", __func__);
		return;
	}

	dev->sw_cmdbuf_rdy_num = dev_get_job_num(dev);
	node = dev->work_list.head;
	while (node && ((struct cmdbuf_obj *)node->data)->cmdbuf_run_done)
		node = node->next;

	if (dev->sw_cmdbuf_rdy_num == 0 || node == NULL) {
		vcmd_klog(LOGLVL_BRIEF, "%s: no cmdbuf to start yet! rdy num: %d, node ptr: 0x%x\n", __func__, dev->sw_cmdbuf_rdy_num, node);
#ifdef SUPPORT_WATCHDOG
		_vcmd_watchdog_stop(dev);
#endif
		return;
	}

	obj = (struct cmdbuf_obj *)node->data;

	printk_vcmd_register_debug(hwregs, "vcmd start enters");
	vcmd_klog(LOGLVL_FLOW, "vcmd start for cmdbuf id %d, cmdbuf_run_done = %d\n",
							obj->cmdbuf_id, obj->cmdbuf_run_done);

	//init HWIF_VCMD_EXE_CMDBUF_COUNT
	vcmd_write_register_value(hwregs, reg_mirror, HWIF_VCMD_EXE_CMDBUF_COUNT, 0);
	//for old hw version which reg EXE_CMDBUF_COUNT is read-only.
	dev->sw_cmdbuf_rdy_num += vcmd_get_register_value((const void *)hwregs,
									reg_mirror, HWIF_VCMD_EXE_CMDBUF_COUNT);

	vcmd_set_reg_mirror(reg_mirror, HWIF_VCMD_RDY_CMDBUF_COUNT,
							dev->sw_cmdbuf_rdy_num);

	if (dev->state == VCMD_STATE_POWER_ON) {
		//0x40
	#ifdef HANTROVCMD_ENABLE_IP_SUPPORT
		//when start vcmd, first vcmd is init mode
		vcmd_set_reg_mirror(reg_mirror, HWIF_VCMD_INIT_MODE, dev->init_mode);
	#endif
		vcmd_set_reg_mirror(reg_mirror, HWIF_VCMD_AXI_CLK_GATE_DISABLE, 0);
		vcmd_set_reg_mirror(reg_mirror,
						HWIF_VCMD_MASTER_OUT_CLK_GATE_DISABLE, APB_CLK_MODE);
		vcmd_set_reg_mirror(reg_mirror, HWIF_VCMD_CORE_CLK_GATE_DISABLE, 0);
		vcmd_set_reg_mirror(reg_mirror, HWIF_VCMD_ABORT_MODE, dev->abort_mode);
		vcmd_set_reg_mirror(reg_mirror, HWIF_VCMD_RESET_CORE, 0);
		vcmd_set_reg_mirror(reg_mirror, HWIF_VCMD_RESET_ALL, 0);
		vcmd_set_reg_mirror(reg_mirror, HWIF_VCMD_START_TRIGGER, 0);
		//0x48
		if (dev->hw_version_id <= HW_ID_1_0_C) {
			vcmd_set_reg_mirror(reg_mirror, HWIF_VCMD_IRQ_INTCMD_EN, 0xffff);
		} else {
			vcmd_set_reg_mirror(reg_mirror, HWIF_VCMD_IRQ_JMPP_EN, 1);
			vcmd_set_reg_mirror(reg_mirror, HWIF_VCMD_IRQ_JMPD_EN, 1);
		}

		if (dev->hw_version_id < HW_ID_1_1_1)
			vcmd_set_reg_mirror(reg_mirror, HWIF_VCMD_IRQ_RESET_EN, 1);

		vcmd_set_reg_mirror(reg_mirror, HWIF_VCMD_IRQ_ABORT_EN, 1);
		vcmd_set_reg_mirror(reg_mirror, HWIF_VCMD_IRQ_CMDERR_EN, 1);
		vcmd_set_reg_mirror(reg_mirror, HWIF_VCMD_IRQ_TIMEOUT_EN, 1);
		vcmd_set_reg_mirror(reg_mirror, HWIF_VCMD_IRQ_BUSERR_EN, 1);
		vcmd_set_reg_mirror(reg_mirror, HWIF_VCMD_IRQ_ENDCMD_EN, 1);
		//0x4c
		vcmd_set_reg_mirror(reg_mirror, HWIF_VCMD_TIMEOUT_EN, 1);
		vcmd_set_reg_mirror(reg_mirror, HWIF_VCMD_TIMEOUT_CYCLES, 500000000);

		vcmd_set_reg_mirror(reg_mirror, HWIF_VCMD_MAX_BURST_LEN, 0x10);
	}

	cmd_ba = obj->cmd_pa - dev->pa_trans_offset;
	if (dev->mmu_enable)
		cmd_ba = (addr_t)obj->mmu_cmd_ba;
	if (sizeof(addr_t) == 8)
		ba_msb = (u32)(cmd_ba >> 32);


	vcmd_set_reg_mirror(reg_mirror, HWIF_VCMD_EXECUTING_CMD_ADDR, (u32)cmd_ba);
	vcmd_set_reg_mirror(reg_mirror, HWIF_VCMD_EXECUTING_CMD_ADDR_MSB, ba_msb);
	vcmd_set_reg_mirror(reg_mirror, HWIF_VCMD_EXE_CMDBUF_LENGTH,
							(obj->cmdbuf_size + 7) / 8);

	if (dev->hw_version_id > HW_ID_1_0_C)
		vcmd_write_register_value(hwregs, reg_mirror,
									HWIF_VCMD_CMDBUF_EXECUTING_ID,
									(u32)obj->cmdbuf_id);

	if (dev->state == VCMD_STATE_POWER_ON) {
		vcmd_write_reg(hwregs, 0x44, vcmd_read_reg(hwregs, 0x44));
		vcmd_write_reg(hwregs, 0x40, reg_mirror[0x40 / 4]);
		vcmd_write_reg(hwregs, 0x48, reg_mirror[0x48 / 4]);
		vcmd_write_reg(hwregs, 0x4c, reg_mirror[0x4c / 4]);
		vcmd_write_reg(hwregs, 0x5c, reg_mirror[0x5c / 4]);
		if (dev->hw_version_id >= HW_ID_1_5_0)
			vcmd_write_reg(hwregs, 0x64, 0xfffeffff); //not normal interrupt cpu
		else
			vcmd_write_reg(hwregs, 0x64, 0xffffffff); //not interrupt cpu
		if (dev->hw_version_id >= HW_ID_1_2_1)
			vcmd_set_init_cmds(dev);
	}

	vcmd_write_reg(hwregs, 0x50, reg_mirror[0x50 / 4]);
	vcmd_write_reg(hwregs, 0x54, reg_mirror[0x54 / 4]);
	vcmd_write_reg(hwregs, 0x58, reg_mirror[0x58 / 4]);
	vcmd_write_reg(hwregs, 0x60, reg_mirror[0x60 / 4]);

	dev->state = VCMD_STATE_WORKING;

	if (dev->hw_version_id >= HW_ID_1_5_2) {
		if (dev->hw_feature.has_apb_arbiter) {
			u32 arb_cfg = vcmd_read_reg(hwregs,
								VCMD_REGISTER_ARBITER_CONFIG_OFFSET);
			arb_cfg |= 0x100;
			vcmd_write_reg(hwregs,
								VCMD_REGISTER_ARBITER_CONFIG_OFFSET, arb_cfg);
		}
	}

	//start
	vcmd_set_reg_mirror(reg_mirror, HWIF_VCMD_START_TRIGGER, 1);
	vcmd_write_reg(hwregs, 0x40, reg_mirror[0x40 / 4]);

#ifdef SUPPORT_WATCHDOG
	_vcmd_watchdog_feed(dev);
#endif

	printk_vcmd_register_debug(hwregs, "vcmd start exits");
}

static void wait_fence_ready(vcmd_mgr_t* vcmd_mgr, u32 cmd_id)
{
	u32 cur_cmd_id = 0, fence_data = 0;
	u32 count=0;
	struct cmdbuf_obj *cmd_node = (struct cmdbuf_obj *)(vcmd_mgr->nodes[cmd_id].data);
	u32 core_id = cmd_node->core_id;
	struct hantrovcmd_dev *dev = &vcmd_mgr->dev_ctx[core_id];

	while (1) {
		if (*dev->enc_ring->fence_node.cpu_virt_addr >= cmd_node->submit_fence_id){
			break;
		}
		udelay(5);
		//count++;
		//if (count == 1000) {
		//	return;
		//}
	}
}

/**
 * @brief abort a specified vcmd hw device.
 * @param u32 *aborted_id: the id of aborted cmdbuf.
 */
static int vcmd_abort(vcmd_mgr_t *vcmd_mgr, struct hantrovcmd_dev *dev,
					u32 *aborted_id)
{
	unsigned long flags = 0;

	spin_lock_irqsave(dev->spinlock, flags);
	vcmd_write_register_value((const void *)dev->hwregs,
								dev->reg_mirror,
								HWIF_VCMD_START_TRIGGER, 0);
	spin_unlock_irqrestore(dev->spinlock, flags);
	if (1) {//(vcmd_isr_polling == 0) {
		if (wait_event_timeout(dev->abort_queue_vcmd, dev->state == VCMD_STATE_IDLE, msecs_to_jiffies(100)) == 0) {
		}
	} else {
		u32 irq, cnt = 100;

		irq = (dev->subsys_info->irq == -1) ?
				dev->core_id : dev->subsys_info->irq;

		while (cnt--) {
			gf_usleep_range(100, 120);
			lisuan_enc_vcmd_clear_irq_status(irq, vcmd_mgr);
			hantroenc_vcmd_isr(irq, vcmd_mgr, 0);
			if (dev->state == VCMD_STATE_IDLE) {
				cnt += 1;
				break;
			}
		}
		if (cnt == 0) {
			vcmd_klog(LOGLVL_ERROR, "%s: can't wait aborted!!!\n", __func__);
			return -ERESTARTSYS;
		}
	}

	if (aborted_id)
		*aborted_id = dev->aborted_cmdbuf_id;
	wait_fence_ready(vcmd_mgr, *aborted_id);

	return 0;

}

/**
 * @brief select a suitable device, and add/insert a job node to its work_list.
 */
static int select_vcmd(vcmd_mgr_t *vcmd_mgr, bi_list_node *new_node, int is_internal)
{
	struct hantrovcmd_dev *dev, *smallest_dev;
	struct vcmd_module_mgr *module;
	struct cmdbuf_obj *obj, *tmp_obj;
	bi_list_node *curr_node;
	bi_list *list;
	u32 least_workload;
	u32 reg_id_exe;

	u32 cmdbuf_id, i;
	unsigned long flags = 0;
	addr_t curr_exe_addr;
	int ret;

	obj = (struct cmdbuf_obj *)new_node->data;
	module = &vcmd_mgr->module_mgr[obj->module_type];

	/* To check if there is free dev, or dev which tail node is run-done. */
	for (i = 0; i < module->num; i++) {
		dev = module->dev[i];
		ret = is_supported_core(obj->core_mask, dev->id_in_type);
		if (ret == 0)
			continue;

		list = &dev->work_list;
		spin_lock_irqsave(dev->spinlock, flags);
		if (!list->tail ||
			((struct cmdbuf_obj *)list->tail->data)->cmdbuf_run_done) {
			dev_add_job(dev, new_node, is_internal);
			spin_unlock_irqrestore(dev->spinlock, flags);
			return 0;
		}
		spin_unlock_irqrestore(dev->spinlock, flags);
	}

	// There is no vcmd in free, calculate each workload, select the least one.
	// If low priority, insert to tail.
	// If high priority, abort the dev, and insert to "head".
	reg_id_exe = REG_ID_CMDBUF_EXE_ID;
	if (obj->priority == CMDBUF_PRIORITY_NORMAL)
		reg_id_exe = REG_ID2_CMDBUF_EXE_ID;

	least_workload = 0xffffffff;
	smallest_dev = NULL;

	//calculate remain workload of all dev, find the least one
	for (i = 0; i < module->num; i++) {
		dev = module->dev[i];
		ret = is_supported_core(obj->core_mask, dev->core_id);
		if (ret == 0)
			continue;

		list = &dev->work_list;

		//get the executing cmdbuf node.
		if (dev->hw_version_id <= HW_ID_1_0_C) {
			curr_exe_addr = VCMDGetAddrRegisterValue((const void *)dev->hwregs,
												dev->reg_mirror,
												HWIF_VCMD_EXECUTING_CMD_ADDR);

			curr_node = get_cmdbuf_node_by_addr(vcmd_mgr, dev, curr_exe_addr);

		} else {
			//cmdbuf_id = vcmd_get_register_value((const void *)dev->hwregs,
			//dev->reg_mirror,HWIF_VCMD_CMDBUF_EXECUTING_ID);
			cmdbuf_id = *(dev->reg_mem_va + reg_id_exe);
			if (cmdbuf_id >= SLOT_NUM_CMDBUF) {
				vcmd_klog(LOGLVL_ERROR, "cmdbuf_id greater than the ceiling !!\n");
				return -1;
			}

			curr_node = &vcmd_mgr->nodes[cmdbuf_id];
		}

		spin_lock_irqsave(dev->spinlock, flags);
		if (!curr_node)
			curr_node = list->head;
		//calculate total workload of this device
		dev->total_workload = calc_workload_after_node(curr_node);
		spin_unlock_irqrestore(dev->spinlock, flags);

		if (dev->total_workload <= least_workload) {
			least_workload = dev->total_workload;
			smallest_dev = dev;
		}
	}

	if (smallest_dev == NULL) {
		vcmd_klog(LOGLVL_ERROR, "%s: no dev is available to cmdbuf [%d] with core_mask 0x%x\n",
					__func__, obj->cmdbuf_id, obj->core_mask);
		return -EINVAL;
	}

	list = &smallest_dev->work_list;
	if (obj->priority == CMDBUF_PRIORITY_NORMAL) {
		//insert to tail
		spin_lock_irqsave(smallest_dev->spinlock, flags);
		dev_add_job(smallest_dev, new_node, is_internal);
		spin_unlock_irqrestore(smallest_dev->spinlock, flags);
		return 0;
	}

	//CMDBUF_PRIORITY_HIGH
	//abort the vcmd and wait
	if (vcmd_abort(vcmd_mgr, smallest_dev, &cmdbuf_id))
		return -ERESTARTSYS; //abort failed

	// need to select inserting position again
	// because hw maybe have run to the next node.
	// CMDBUF_PRIORITY_HIGH
	spin_lock_irqsave(smallest_dev->spinlock, flags);
	curr_node = &vcmd_mgr->nodes[cmdbuf_id];
	if (smallest_dev->abort_mode == 0)
		curr_node = curr_node->next;
	while (curr_node) {
		tmp_obj = (struct cmdbuf_obj *)curr_node->data;
		//find the 1st node with normal priority, and insert node prior to it
		if (tmp_obj->priority == CMDBUF_PRIORITY_NORMAL)
			break;
		curr_node = curr_node->next;
	}

	//insert to "head" of normal priority nodes
	dev_insert_job(smallest_dev, curr_node, new_node, is_internal);
	spin_unlock_irqrestore(smallest_dev->spinlock, flags);
	return 0;
}

/**
 * @brief acquire workload from process object.
 * @return int: 0: succeed; Others: failed.
 */
static int wait_process_resource_rdy(struct proc_obj *po)
{
	return po->total_workload <= PROCESS_MAX_WORKLOAD;
}

static int acquire_process_resource(struct proc_obj *po, u32 workload)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&po->spinlock, flags);
	po->total_workload += workload;
	spin_unlock_irqrestore(&po->spinlock, flags);
	if (wait_event_interruptible(po->resource_waitq,
									wait_process_resource_rdy(po))) {
		vcmd_klog(LOGLVL_ERROR, "%s: wait event is interrupted!", __func__);
		return -1;
	}

	return 0;
}

/**
 * @brief return cmdbuf obj's workload to process object.
 */
static void return_process_resource(struct proc_obj *po,
									struct cmdbuf_obj *obj)
{
	unsigned long flags = 0;

	if (po && obj->workload) {
		spin_lock_irqsave(&po->spinlock, flags);
		po->total_workload -= obj->workload;
		spin_unlock_irqrestore(&po->spinlock, flags);
		obj->workload = 0;
		wake_up_interruptible_all(&po->resource_waitq);
	}
}

/**
 * @brief return cmdbuf obj's workload to process object.
 */
static long reserve_cmdbuf(vcmd_mgr_t *vcmd_mgr, struct proc_obj *po,
							struct enc_exchange_parameter *param)
{
	struct cmdbuf_obj *obj;
	u32 cmdbuf_id = 0;
	u32 workload;

	if (param->cmdbuf_size > SLOT_SIZE_CMDBUF) {
		vcmd_klog(LOGLVL_ERROR, "%s size is larger than slot size !!\n", __func__);
		return -1;
	}

	if (!po) {
		vcmd_klog(LOGLVL_ERROR, "%s: not find process obj!\n", __func__);
		return -1;
	}
	vcmd_klog(LOGLVL_FLOW, "reserve cmdbuf by filp %p\n", (void *)po->filp);

	workload = (param->interrupt_ctrl) & 0x7fffffff; //bit31 for interrupt
	//if (acquire_process_resource(po, workload))
	//	return -1;

	if (acquire_cmdbuf(vcmd_mgr, &cmdbuf_id))
		return -ERESTARTSYS;

	reset_cmdbuf_obj(vcmd_mgr, cmdbuf_id);
	reset_cmdbuf_node(vcmd_mgr, cmdbuf_id);

	obj = &vcmd_mgr->objs[cmdbuf_id];
	obj->module_type = param->module_type;
	obj->priority = param->priority;
	obj->workload = workload;
	obj->interrupt_ctrl = param->interrupt_ctrl;
	obj->filp = po->filp;
	obj->po = po;
	obj->core_mask = param->core_mask;

	param->cmdbuf_size = SLOT_SIZE_CMDBUF;
	param->cmdbuf_id = cmdbuf_id;
	vcmd_klog(LOGLVL_FLOW, "%s, filp[%p] reserved cmdbuf[%d]: obj %p, node %p\n",
			__func__, (void *)obj->filp, cmdbuf_id, (void *)obj,
			(void *)&vcmd_mgr->nodes[cmdbuf_id]);

#ifdef SUPPORT_DBGFS
	_dbgfs_record_reserved_time(vcmd_mgr->dev_ctx[0].dbgfs_info,
								param->cmdbuf_id);
#endif

	return 0;
}

/**
 * @brief release a specified cmdbuf.
 * @return long: 0: succeed; oters: failed.
 */
static long release_cmdbuf(vcmd_mgr_t *vcmd_mgr,
							struct proc_obj *po, u16 cmdbuf_id)
{
	struct cmdbuf_obj *obj = NULL;
	bi_list_node *curr_node = NULL;
	unsigned long flags = 0;
	struct hantrovcmd_dev *dev = NULL;

	if (cmdbuf_id >= SLOT_NUM_CMDBUF) {
		//should not happen
		vcmd_klog(LOGLVL_ERROR, "%s: ERROR cmdbuf_id %d!!\n", __func__, cmdbuf_id);
		return -1;
	}

	curr_node = &vcmd_mgr->nodes[cmdbuf_id];
	obj = (struct cmdbuf_obj *)curr_node->data;
	if (obj->po != po) {
		//should not happen
		vcmd_klog(LOGLVL_ERROR, "%s: cmdbuf[%d] po not match: owned by %p, released by %p!!\n",
						__func__, cmdbuf_id, obj->po, po);
		return -1;
	}

	if (obj->cmdbuf_linked == 0) {
		//not link_and_run yet
		obj = (struct cmdbuf_obj *)curr_node->data;
		//return_process_resource(obj->po, obj);
		return_cmdbuf(vcmd_mgr, cmdbuf_id);
	} else {
		obj->cmdbuf_need_remove = 1;
		dev = &vcmd_mgr->dev_ctx[obj->core_id];

		spin_lock_irqsave(dev->spinlock, flags);
		//return_process_resource(obj->po, obj);
		dev_remove_job(vcmd_mgr, dev, curr_node);
		spin_unlock_irqrestore(dev->spinlock, flags);
	}
	return 0;
}

static void vcmd_fill_indicator_and_end(struct cmdbuf_obj *cmdbuf_obj, struct hantrovcmd_dev *dev, struct drm_file *file, int procid)
{
	struct lisuan_file *gf_file = file->driver_priv;
	struct lisuan_vm *vm = &gf_file->vm;
	struct lisuan_vm_node *vm_node = container_of(vm->root.buffer->va_list.next, struct lisuan_vm_node, node_item);
	int i = 0;

	gf_hw_get_vcmd_fill_indicator_and_end_cmds(&cmdbuf_obj->cmd_va, cmdbuf_obj->cmdbuf_size, procid, (unsigned int)((vm_node->offset - gf_file->ls_dev->mmu_bypass_start) >> 9),
											   dev->enc_ring->fence_node.start,
											   atomic64_read(&dev->enc_ring->last_submit_fence_id));
	for(i = 0;i < 12; i++)
		*(int*)(cmdbuf_obj->cmd_va + 19 + i * 4) = gf_file->ls_dev->enc_reserve_offset[i];
}


/**
 * @brief add/insert a cmdbuf (job) to suitable device,
 *  and start the device hw if it is not working.
 * @param struct enc_exchange_parameter *param: the param of cmdbuf to link & run.
 * @return long: 0: succeed; oters: failed.
 */
static long link_and_run_cmdbuf(vcmd_mgr_t *vcmd_mgr, struct proc_obj *po,
								struct enc_exchange_parameter *param, int is_internal, int procid)
{
	struct cmdbuf_obj *obj;
	bi_list_node *curr_node;

	struct hantrovcmd_dev *dev = NULL;
	unsigned long flags = 0;
	int ret;
	u16 cmdbuf_id = param->cmdbuf_id;

	struct cmd_jmp_t *cmd_jmp;

	if (cmdbuf_id >= SLOT_NUM_CMDBUF) {
		//should not happen
		vcmd_klog(LOGLVL_ERROR, "%s: ERROR cmdbuf_id %d!!\n", __func__, cmdbuf_id);
		return -1;
	}

	curr_node = &vcmd_mgr->nodes[cmdbuf_id];
	obj = (struct cmdbuf_obj *)curr_node->data;
	if (obj->po != po) {
		//should not happen
		vcmd_klog(LOGLVL_ERROR, "%s: cmdbuf[%d] po not match: owned by %p, link by %p!!\n",
						__func__, cmdbuf_id, obj->po, po);
		return -1;
	}

	obj->cmdbuf_size = param->cmdbuf_size;
	obj->interrupt_ctrl = param->interrupt_ctrl;

	//0: has jmp opcode,1 has end code
	obj->has_jmp_cmd = param->has_end_cmd ? 0 : 1;

	if (obj->has_jmp_cmd) {
		//last command is JMP, get its IE value.
		cmd_jmp = _get_jmp_cmd(obj);
		vcmd_klog(LOGLVL_FLOW, "has jmp cmd and the last cmd is JMP!\n");

		if ((cmd_jmp->opcode & OPCODE_MASK) != OPCODE_JMP) {
			vcmd_klog(LOGLVL_ERROR, "%s: cmdbuf[%d] is not terminated by JMP, not match with its flag!",
					  __func__, obj->cmdbuf_id);
			return -1;
		}
		obj->jmp_ie = 0;
		if (obj->interrupt_ctrl >> 31)
			obj->jmp_ie = obj->interrupt_ctrl & 1; //force jmp_ie to be 1 or 0
		else if (JMP_G_IE(cmd_jmp->opcode))
			obj->jmp_ie = 1;
	}

	//if (down_interruptible(&vcmd_mgr->module_mgr[obj->module_type].sem))
	//	return -ERESTARTSYS;

	ret = select_vcmd(vcmd_mgr, curr_node, is_internal);
	if (ret) {
		up(&vcmd_mgr->module_mgr[obj->module_type].sem);
		return ret;
	}

	dev = &vcmd_mgr->dev_ctx[obj->core_id];
	spin_lock_irqsave(dev->spinlock, flags);
	param->core_id = obj->core_id;
	vcmd_klog(LOGLVL_FLOW, "*** Assign cmdbuf[%d] to core[%d]\n",
			  cmdbuf_id, param->core_id);

	dev->cur_cmd_id = obj->cmdbuf_id;

	if (!is_internal)
		//add indicate header and frame end to command buffer, umd has reserve the allocation
		vcmd_fill_indicator_and_end(obj, dev, po->filp, procid);

#ifdef VCMD_DEBUG_INTERNAL
	_dbg_log_cmdbuf(obj);
#endif

	barrier_rw_local(dev->enc_ring->device->barrier_buf->virt_addres, dev->enc_ring->rb_index, dev->enc_ring->device->mhb_num, dev->enc_ring->ring_fence.ring_fence_value);

	//start to run
	if (dev->state != VCMD_STATE_WORKING) {
		//start vcmd
		vcmd_start(dev);
	} else {
		dev->sw_cmdbuf_rdy_num++;
#ifdef SUPPORT_DBGFS
		_dbgfs_record_link_time(dev->dbgfs_info, cmdbuf_id,
								dev->sw_cmdbuf_rdy_num,
								obj->workload);
#endif
		//just update cmdbuf ready number
		vcmd_write_register_value((const void *)dev->hwregs,
									dev->reg_mirror,
									HWIF_VCMD_RDY_CMDBUF_COUNT,
									dev->sw_cmdbuf_rdy_num);
#ifdef SUPPORT_WATCHDOG
		_vcmd_watchdog_feed(dev);
#endif
	}

	//new cmdbuf linked, and free the removeable cmdbuf.
	if (curr_node->previous) {
		obj = (struct cmdbuf_obj *)curr_node->previous->data;
		if (obj->cmdbuf_need_remove) {
			//free the job
			dev_remove_job(vcmd_mgr, dev, curr_node->previous);
		}
	}

	spin_unlock_irqrestore(dev->spinlock, flags);

	//up(&vcmd_mgr->module_mgr[obj->module_type].sem);

	return 0;
}

/**
 * @brief wait a cmdbuf runs done.
 * @param u16 cmdbuf_id: the id of cmdbuf to wait.
 * @param u16 *done_id: point to the id of done cmdbuf.
 * @return long: 0: succeed; oters: failed.
 */
static long wait_cmdbuf_ready(vcmd_mgr_t *vcmd_mgr, struct proc_obj *po,
								u16 cmdbuf_id, u16 *done_id, int is_internal)
{
	struct cmdbuf_obj *obj = NULL;

	if (!po) {
		vcmd_klog(LOGLVL_ERROR, "%s: not find process obj!\n", __func__);
		return -ERESTARTSYS;
	}

	if (cmdbuf_id != ANY_CMDBUF_ID) {
		vcmd_klog(LOGLVL_FLOW, "%s\n", __func__);
		if (cmdbuf_id >= SLOT_NUM_CMDBUF) {
			vcmd_klog(LOGLVL_ERROR, "%s: ERROR cmdbuf id to wait\n", __func__);
			return -1;
		}
		obj = &vcmd_mgr->objs[cmdbuf_id];
		if (obj->po != po) {
			//should not happen
			vcmd_klog(LOGLVL_ERROR, "%s: ERROR cmdbuf filp not match!\n", __func__);
			return -1;
		}
#ifdef IRQ_SIMULATION
		_irq_simul_add_timer(obj);
#endif
	}

	if (wait_event_interruptible(po->job_waitq,
						proc_get_done_job(vcmd_mgr, po, &obj))) {
		po->undone_cmdbuf_id = cmdbuf_id;
		po->has_undone = 1;
		return -ERESTARTSYS;
	}

	wait_fence_ready(vcmd_mgr, cmdbuf_id);

	*done_id = obj->cmdbuf_id;
	if (obj->cmdbuf_run_done == 1)
		return 0;
	else
		return 1;
}

/**
 * @brief clean-up reserved PCIe resources.
 */
static void _vcmd_pcie_cleanup(struct lisuan_device *gf_device, vcmd_mgr_t *vcmd_mgr)
{
#ifdef HANTROMMU_SUPPORT
	struct kernel_addr_desc mmu_addr;

	if (vcmd_mgr->mmu_enable && vcmd_mgr->pcie_pool.mmu_ba) {
		mmu_addr.bus_address = vcmd_mgr->pcie_pool.pa;
		mmu_addr.size = vcmd_mgr->pcie_pool.size;
		MMUKernelMemNodeUnmap(&mmu_addr);
		vcmd_mgr->pcie_pool.mmu_ba = 0;
	}
#endif

	lisuan_gem_free_oneshot(gf_device, gf_device->enc_bufObj);

	vcmd_mgr->pcie_pool.va = NULL;
}

/**
 * @brief allocate memory pools, and init PCIe access if it is PCIe device.
 * @return int: 0: succeed; other: failed.
 */

static int reduce_xor(unsigned int x) {
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

static void get_addr_2k(unsigned int base_addr, uint64_t* offset_arr)
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

static int vcmd_init(struct lisuan_device *gf_device, vcmd_mgr_t *vcmd_mgr)
{
	u8 *va;
	struct noncache_mem *mem_pcie = &vcmd_mgr->pcie_pool;
	u32 offset;
	struct lisuan_vm_node *vm_node;
	unsigned long long reserve_gpu_va;

	mem_pcie->size = vcmd_mgr->mem_vcmd.size +
						vcmd_mgr->mem_status.size +
						vcmd_mgr->mem_regs.size;
	gf_device->enc_bufObj = gf_kvcalloc(1, sizeof(struct lisuan_gem_bo), GFP_KERNEL);
	gf_device->enc_bufObj->size = mem_pcie->size;
	gf_device->enc_bufObj->align = 64*1024;
	gf_device->enc_bufObj->domain = dom_visual_vram;
	gf_device->enc_bufObj->flag.byPass = 1;
	atomic_set(&gf_device->enc_bufObj->unpagable, 1);
	gf_device->enc_bufObj->priority = gem_priority_high;
	gf_device->enc_bufObj->flag.vram = (gf_device->enc_bufObj->domain <= dom_invisual_vram) ? 1 : 0;
	gf_device->enc_bufObj->flag.snoop = ((gf_device->enc_bufObj->domain == dom_snoop_system_pages) ? 1 : 0);

	lisuan_gem_alloc_oneshot(gf_device, NULL, gf_device->enc_bufObj, 1);
	vm_node= container_of(gf_device->enc_bufObj->va_list.next, struct lisuan_vm_node, node_item);

	vcmd_mgr->pa_trans_offset = 0;
	vcmd_mgr->reg_base_offset = 0;

	/* allocate a single pool for all noncache_mems*/
	mem_pcie->pa = vm_node->offset;
	vcmd_klog(LOGLVL_CONFIG, "Init: gpu va = 0x%llx, gpu pa = %llx\n", (unsigned long long)mem_pcie->pa, (unsigned long long)(mem_pcie->pa - gf_device->mmu_bypass_start));
	mem_pcie->va = (u32*)gf_device->enc_bufObj->virt_addres;
	vcmd_klog(LOGLVL_CONFIG, "%s: pcie pool va=0x%p.\n", __func__, mem_pcie->va);

	/* layout this pool to noncache_mems*/
	offset = 0;
	va = (u8 *)mem_pcie->va;

	vcmd_mgr->mem_vcmd.pa = mem_pcie->pa + offset;
	vcmd_mgr->mem_vcmd.va = (u32 *)(va + offset);
	offset += vcmd_mgr->mem_vcmd.size;

	vcmd_mgr->mem_status.pa = mem_pcie->pa + offset;
	vcmd_mgr->mem_status.va = (u32 *)(va + offset);
	offset += vcmd_mgr->mem_status.size;

	vcmd_mgr->mem_regs.pa = mem_pcie->pa + offset;
	vcmd_mgr->mem_regs.va = (u32 *)(va + offset);

	vcmd_klog(LOGLVL_CONFIG, "Init: mem_vcmd pa = %llx, cpu va = %llx\n", (unsigned long long)(vcmd_mgr->mem_vcmd.pa), (unsigned long long)vcmd_mgr->mem_vcmd.va);
	vcmd_klog(LOGLVL_CONFIG, "Init: mem_status pa = %llx, cpu va = %llx\n", (unsigned long long)(vcmd_mgr->mem_status.pa), (unsigned long long)vcmd_mgr->mem_status.va);
	vcmd_klog(LOGLVL_CONFIG, "Init: mem_regs pa = %llx, cpu va = %llx\n", (unsigned long long)(vcmd_mgr->mem_regs.pa), (unsigned long long)vcmd_mgr->mem_regs.va);

	gf_device->enc_reserve_bufObj = gf_kvcalloc(1, sizeof(struct lisuan_gem_bo), GFP_KERNEL);
	gf_device->enc_reserve_bufObj->size = 512 * 1024;//512k byte
	gf_device->enc_reserve_bufObj->align = 64 * 1024;
	gf_device->enc_reserve_bufObj->domain = dom_visual_vram;
	gf_device->enc_reserve_bufObj->flag.byPass = 1;
	atomic_set(&gf_device->enc_reserve_bufObj->unpagable, 1);
	gf_device->enc_reserve_bufObj->priority = gem_priority_high;
	gf_device->enc_reserve_bufObj->flag.vram = (gf_device->enc_bufObj->domain <= dom_invisual_vram) ? 1 : 0;
	gf_device->enc_reserve_bufObj->flag.snoop = ((gf_device->enc_bufObj->domain == dom_snoop_system_pages) ? 1 : 0);
	lisuan_gem_alloc_oneshot(gf_device, NULL, gf_device->enc_reserve_bufObj, 1);
	vm_node = container_of(gf_device->enc_reserve_bufObj->va_list.next, struct lisuan_vm_node, node_item);
	reserve_gpu_va = vm_node->offset;
	get_addr_2k(reserve_gpu_va, gf_device->enc_reserve_offset);

	return 0;
}

/**
 * @brief initialize context of all vcmd dev
 */
static void dev_ctx_init(vcmd_mgr_t *vcmd_mgr, struct lisuan_device *gf_device)
{
	u32 i;
	struct hantrovcmd_dev *dev;
	u32 m_type;
	struct vcmd_module_mgr *module;

	gf_memset(vcmd_mgr->dev_ctx,
			0, sizeof(struct hantrovcmd_dev) * vcmd_mgr->subsys_num);
	for (i = 0; i < vcmd_mgr->subsys_num; i++) {
		dev = &vcmd_mgr->dev_ctx[i];

		dev->handler = (void *)vcmd_mgr;
		dev->subsys_info = &vcmd_mgr->core_array[i];
		/* previously, reg_base not add reg_base_offset */
		dev->subsys_info->reg_base += vcmd_mgr->reg_base_offset;
		m_type = dev->subsys_info->sub_module_type;

		dev->core_id = i;
		dev->state = VCMD_STATE_POWER_ON;
		dev->sw_cmdbuf_rdy_num = 0;
		/* the abnormal interrupts source from VCE(0) */
		dev->abn_irq_mask = 1 << ABNORMAL_IRQ_BIT_MASK(0);
		dev->intr_gate_mask = 0xFFFFFFFF & (~dev->abn_irq_mask);
		dev->timeout_timer_expired = 0;
		dev->timeout_timer_active = 0;

		dev->watchdog_triggered = 0;
		dev->watchdog_active = 0;

		dev->abort_mode = 0;
		dev->init_mode = VCMD_INIT_NORMAL;
#ifdef HANTROVCMD_ENABLE_IP_SUPPORT
		if (dev->hw_feature.has_init_mode)
			dev->init_mode = DEFAULT_VCMD_INIT_MODE;
#endif
		dev->spinlock = &dev->owner_lock_vcmd;
		spin_lock_init(dev->spinlock);
		spin_lock_init(&dev->abn_irq_lock);
		//dev->abort_waitq = &dev->abort_queue_vcmd;
		init_waitqueue_head(&dev->abort_queue_vcmd);

		init_bi_list(&dev->work_list);

		dev->reg_mem_ba = vcmd_mgr->mem_regs.pa +
							i * SLOT_SIZE_REGBUF - vcmd_mgr->pa_trans_offset;

		dev->reg_mem_va = vcmd_mgr->mem_regs.va + i * SLOT_SIZE_REGBUF / 4;
		dev->reg_mem_sz = SLOT_SIZE_REGBUF;
		gf_memset(dev->reg_mem_va, 0, dev->reg_mem_sz);
		dev->pa_trans_offset = vcmd_mgr->pa_trans_offset;

		module = &vcmd_mgr->module_mgr[m_type];
		if (module->num == 0)
			sema_init(&module->sem, 1);
		dev->id_in_type=module->num;
		module->dev[module->num++] = dev;

		dev->enc_ring = gf_device->sched[i + 5];

		vcmd_klog(LOGLVL_CONFIG, "module init - vcmdcore[%d] addr =0x%llx\n",
			i, (unsigned long long)dev->subsys_info->reg_base);
	}
}

/**
 * @brief fill cmdbuf to read sub-module's all regs.
 */
#define VCMD_READ_CMD(cmd, n, off, addr) \
	do { \
		(cmd)->opcode = OPCODE_RREG | RREG_MODE(RREG_ADDR_INC) | \
						RREG_LEN(n) | \
						RREG_START_ADDR(off); \
		CMD_SET_ADDR(cmd, (addr)+(off)); \
		(cmd)->padding = 0; \
		cmd++; \
	} while (0)

static void create_read_all_registers_cmdbuf(vcmd_mgr_t *vcmd_mgr,
					struct enc_exchange_parameter *param)
{
	struct hantrovcmd_dev *dev;
	struct vcmd_subsys_info *subsys;
	struct cmd_rreg_t *cmd_rreg;
	struct cmd_end_t *cmd_end;
	u8 *p_cmdbuf;
	addr_t reg_ba;
	int i;
	u16 reg_off;

	dev = &vcmd_mgr->dev_ctx[param->core_id];
	subsys = dev->subsys_info;

	reg_ba = dev->reg_mem_ba;
	if (dev->mmu_enable)
		reg_ba = (addr_t)dev->mmu_reg_mem_ba;
	//reg_ba += param->core_id * SLOT_SIZE_REGBUF; confuse, why still add SLOT_SIZE_REGBUF?

	p_cmdbuf = (u8 *)vcmd_mgr->mem_vcmd.va + CMDBUF_OFF(param->cmdbuf_id);

	cmd_rreg = (struct cmd_rreg_t *)(p_cmdbuf + 0);
	if (dev->hw_version_id > HW_ID_1_0_C) {
		//read vcmd executing cmdbuf id registers to ddr for balancing core load.
		VCMD_READ_CMD(cmd_rreg, 1, REG_ID_CMDBUF_EXE_ID * 4, reg_ba);
	}

	/* read submodule register to ddr */
	for (i = 0; i < SUB_MOD_MAX; i++) {
		if (subsys->reg_off[i] != 0xffff && subsys->rreg_id[i] != 0xffff) {
			reg_off = subsys->reg_off[i] + subsys->rreg_id[i] * 4;
			VCMD_READ_CMD(cmd_rreg,
							subsys->rreg_num[i],
							reg_off,
							reg_ba);
		}
	}

	if (dev->hw_version_id > HW_ID_1_0_C) {
		//read vcmd registers to ddr, to compliant with user cmdbuf
		VCMD_READ_CMD(cmd_rreg, ASIC_VCMD_SWREG_AMOUNT, 0, reg_ba);
	}
	//end cmd
	cmd_end = (struct cmd_end_t *)cmd_rreg;
	cmd_end->opcode = OPCODE_END;
	cmd_end->padding = 0;
	cmd_end++;

	param->cmdbuf_size = (u8 *)cmd_end - p_cmdbuf;
}

/**
 * @brief release submodule IO resource for vcmd driver
 */
static void vcmd_release_submodule_IO(struct vcmd_subsys_info *subsys,
										u32 sub_mod_id)
{
	if (subsys->hwregs[sub_mod_id]) {
		//iounmap((volatile u8 __iomem *)subsys->hwregs[sub_mod_id]);
		//release_mem_region(subsys->reg_base + subsys->reg_off[sub_mod_id],
		//					subsys->io_size[sub_mod_id]);
		subsys->hwregs[sub_mod_id] = NULL;
	}
}

/**
 * @brief reserve submodule IO resource for vcmd driver
 */
static u32 vcmd_reserve_submodule_IO(struct lisuan_device *gf_device,
										struct vcmd_subsys_info *subsys,
										u32 sub_mod_id)
{
	addr_t pa = subsys->reg_base + subsys->reg_off[sub_mod_id];

	subsys->hwregs[sub_mod_id] = gf_device->mmio_base + pa;

	return 0;
}

#ifdef HANTROMMU_SUPPORT
/**
 * @brief MMU kernel map for vcmd driver
*/
static int vcmd_mmu_kernel_map(vcmd_mgr_t *vcmd_mgr)
{
	struct kernel_addr_desc mmu_addr;

#ifdef PCIE_EN
	struct noncache_mem *mem_pcie = &vcmd_mgr->pcie_pool;
	u32 offset = 0;

	mmu_addr.bus_address = mem_pcie->pa - vcmd_mgr->pa_trans_offset;
	mmu_addr.size = mem_pcie->size;
	if (MMUKernelMemNodeMap(&mmu_addr) != MMU_STATUS_OK)
		return -1;
	mem_pcie->mmu_ba = mmu_addr.mmu_bus_address;
	vcmd_klog(LOGLVL_CONFIG, "%s: pool mmu_ba=0x%llx.\n", __func__,
		(unsigned long long)mem_pcie->mmu_ba);

	vcmd_mgr->mem_vcmd.mmu_ba = mem_pcie->mmu_ba + offset;
	offset += vcmd_mgr->mem_vcmd.size;
	vcmd_mgr->mem_status.mmu_ba = mem_pcie->mmu_ba + offset;
	offset += vcmd_mgr->mem_status.size;
	vcmd_mgr->mem_regs.mmu_ba = mem_pcie->mmu_ba + offset;
#else
	struct noncache_mem mem[3];
	int i;

	mem[0] = vcmd_mgr->mem_vcmd,
	mem[1] = vcmd_mgr->mem_status,
	mem[2] = vcmd_mgr->mem_regs

	for (i = 0; i < 3; i++) {
		mmu_addr.bus_address = mem[i].pa;
		mmu_addr.size = mem[i].size;
		if (MMUKernelMemNodeMap(&mmu_addr) != MMU_STATUS_OK) {
			vcmd_klog(LOGLVL_ERROR, "Init[%s] mmu map mem failed\n", __func__);
			return -1;
		}
		mem[i].mmu_ba = mmu_addr.mmu_bus_address;
		vcmd_klog(LOGLVL_FLOW, "Init[%s]: mem->mmu_ba=0x%llx.\n", __func__,
				(unsigned long long)mem[i].mmu_ba);
	}
#endif

	return 0;
}
#endif

/**
 * @brief config modules for vcmd driver
 */
static int vcmd_config_modules(vcmd_mgr_t *vcmd_mgr)
{
	int i;
	struct vcmd_subsys_info *subsys;
#ifdef HANTROMMU_SUPPORT
	enum MMUStatus mmu_status = MMU_STATUS_FALSE;
	int ret = 0;
#endif

	for (i = 0; i < vcmd_mgr->subsys_num; i++) {
		subsys = &vcmd_mgr->core_array[i];
		/* config AXIFE*/
#ifdef HANTROAXIFE_SUPPORT
		if (subsys->hwregs[SUB_MOD_AXIFE0])
			AXIFEEnable(subsys->hwregs[SUB_MOD_AXIFE0], 1);
		if (subsys->hwregs[SUB_MOD_AXIFE1])
			AXIFEEnable(subsys->hwregs[SUB_MOD_AXIFE1], 1);
#endif
		/* config MMU */
#ifdef HANTROMMU_SUPPORT
		if (subsys->hwregs[SUB_MOD_MMU0]) {
			mmu_status = MMUInit(subsys->hwregs[SUB_MOD_MMU0]);
			if (mmu_status == MMU_STATUS_NOT_FOUND) {
				vcmd_klog(LOGLVL_ERROR, "MMU does not exist!\n");
				return -1;
			} else if (mmu_status != MMU_STATUS_OK) {
				return -2;
			} else {
				vcmd_klog(LOGLVL_BRIEF, "MMU detected!\n");
			}
		}
#endif
	}
	/* config MMU*/
#ifdef HANTROMMU_SUPPORT
	mmu_status = MMUEnable(vcmd_mgr->mmu_hwregs);
	if (mmu_status != MMU_STATUS_OK) {
		vcmd_klog(LOGLVL_ERROR, "MMUEnable: MMU enable failed\n");
		return -3;
	}
	vcmd_mgr->mmu_enable = mmu_enable;
	vcmd_klog(LOGLVL_CONFIG, "MMU %s.\n", vcmd_mgr->mmu_enable ? "ENABLE" : "DISABLE");

	if (vcmd_mgr->mmu_enable) {
		ret = vcmd_mmu_kernel_map(vcmd_mgr);
		if (ret < 0)
			return -4;
	}
	for (i = 0; i < vcmd_mgr->subsys_num; i++) {
		vcmd_mgr->dev_ctx[i].mmu_enable = vcmd_mgr->mmu_enable;
		vcmd_mgr->dev_ctx[i].mmu_reg_mem_ba = vcmd_mgr->mem_regs.mmu_ba +
							i * SLOT_SIZE_REGBUF;
	}
#endif

	return 0;
}

/**
 * @brief reserve IO resources for vcmd driver.
 */
static int vcmd_reserve_IO(struct lisuan_device *gf_device, vcmd_mgr_t *vcmd_mgr)
{
	u32 hwid;
	int i, j;
	u32 found_hw = 0;
	struct hantrovcmd_dev *dev;
	struct vcmd_subsys_info *subsys;
	u32 reg_val;
	int ret;

	vcmd_klog(LOGLVL_CONFIG, "%s: total_vcmd_core_num is %d\n", __func__,
		vcmd_mgr->subsys_num);
	for (i = 0; i < vcmd_mgr->subsys_num; i++) {
		dev = &vcmd_mgr->dev_ctx[i];
		subsys = dev->subsys_info;
		dev->hwregs = NULL;

		for (j = SUB_MOD_VCMD; j < SUB_MOD_MAX; j++) {
			if (subsys->reg_off[j] == 0xffff)
				continue;
			ret = vcmd_reserve_submodule_IO(gf_device, subsys, j);
			if (ret < 0) {
				vcmd_klog(LOGLVL_ERROR,
						  "failed to reserve submodule[%d] HW regs for vcmd %d\n",
						  j, i);
				vcmd_klog(LOGLVL_ERROR,
							"vcmd_base_addr = 0x%08lx, iosize = %d\n",
							subsys->reg_base, subsys->io_size[j]);
				continue;
			}
			if (!subsys->hwregs[j]) {
				vcmd_klog(LOGLVL_ERROR,
							"failed to ioremap submodule[%d] HW regs\n",
							j);
				continue;
			}
		}

		dev->hwregs = subsys->hwregs[SUB_MOD_VCMD];
		//return -1;//when load decoder only db, we can't touch encoder register, otherwise pcie will hang
		vcmd_mgr->mmu_hwregs[i][0] = subsys->hwregs[SUB_MOD_MMU0];
		vcmd_mgr->mmu_hwregs[i][1] = subsys->hwregs[SUB_MOD_MMU1];

		/*read hwid and check validness and store it*/
		hwid = gf_read_mmio32(dev->hwregs + 0x0);
		vcmd_klog(LOGLVL_CONFIG, "%s: hantrovcmd_data[%d].hwregs=0x%p\n",
					__func__, i, dev->hwregs);
		vcmd_klog(LOGLVL_CONFIG, "hwid=0x%08x\n", hwid);

		/* check for vcmd HW ID */
		if (((hwid >> 16) & 0xFFFF) != VCMD_HW_ID) {
			vcmd_klog(LOGLVL_ERROR, "HW not found at 0x%lx\n",
						subsys->reg_base);
			dev->hwregs = NULL;
			continue;
		}

		dev->hw_version_id = hwid;
		reg_val =gf_read_mmio32(dev->hwregs +
									VCMD_REGISTER_HW_APB_ARBITER_MODE_OFFSET);
		dev->hw_feature.has_apb_arbiter = (u8)((reg_val >>
										  HW_APB_ARBITER_MODE_BIT) & 0x01);
		reg_val = gf_read_mmio32(dev->hwregs +
									VCMD_REGISTER_HW_INIT_MODE_OFFSET);
		dev->hw_feature.has_init_mode = (u8)((reg_val >> HW_INIT_MODE_BIT) &
										0x01);

		found_hw = 1;

		vcmd_klog(LOGLVL_CONFIG, "HW at base <0x%llx> with ID <0x%08x>\n",
					(unsigned long long)subsys->reg_base, hwid);
	}

	if (found_hw == 0) {
		vcmd_klog(LOGLVL_ERROR, "NO ANY HW found!!\n");
		return -1;
	}

	return 0;
}

/**
 * @brief release IO resources of vcmd driver.
 */
static void vcmd_release_IO(vcmd_mgr_t *vcmd_mgr)
{
	u32 i, j;
	struct vcmd_subsys_info *subsys;

	for (i = 0; i < vcmd_mgr->subsys_num; i++) {
		subsys = &vcmd_mgr->core_array[i];
		for (j = SUB_MOD_VCMD; j < SUB_MOD_MAX; j++)
			vcmd_release_submodule_IO(subsys, j);
		vcmd_mgr->dev_ctx[i].hwregs = NULL;
	}
}

/**
 * @brief read main module's all regs for all vcmd hw devices.
 */
static void read_main_module_all_registers(vcmd_mgr_t *vcmd_mgr)
{
	int ret;
	struct enc_exchange_parameter param[MAX_SUBSYS_NUM];
	u16 done_id = 0;
	struct proc_obj *po;
	u32 i, irq;
	struct hantrovcmd_dev *dev;
	struct vcmd_module_mgr *module;
	u32 *main_regs_va;

	po = vcmd_mgr->init_po;

	for (i = 0; i < vcmd_mgr->subsys_num; i++) {
		dev = &vcmd_mgr->dev_ctx[i];
		param[i].interrupt_ctrl = 0;
		param[i].priority = CMDBUF_PRIORITY_NORMAL;
		param[i].cmdbuf_size = 0;
		param[i].module_type = dev->subsys_info->sub_module_type;
		param[i].core_mask = 1 << dev->id_in_type;
		param[i].core_mask = 1 << dev->core_id;
		param[i].core_id = dev->core_id;
		param[i].has_end_cmd = 1;
		module = &vcmd_mgr->module_mgr[param[i].module_type];

		ret = reserve_cmdbuf(vcmd_mgr, po, &param[i]);
		create_read_all_registers_cmdbuf(vcmd_mgr, &param[i]);
		link_and_run_cmdbuf(vcmd_mgr, po, &param[i], 1, 0);
	}

	/* make sure vcmd can compelte job, and polling to clear irq
	 * irq mode can comment out this codes
	 */
	gf_msleep(100);
	for (i = 0; i < vcmd_mgr->subsys_num; i++) {
		dev = &vcmd_mgr->dev_ctx[i];

		irq = vcmd_mgr->vcmd_irq_enabled ? dev->subsys_info->irq : dev->core_id;
		lisuan_enc_vcmd_clear_irq_status(irq, vcmd_mgr);
		hantroenc_vcmd_isr(irq, vcmd_mgr, 1);

		wait_cmdbuf_ready(vcmd_mgr, po, param[i].cmdbuf_id, &done_id, 1);
		main_regs_va = dev->reg_mem_va +
					dev->subsys_info->reg_off[SUB_MOD_MAIN] / 4;
		vcmd_klog(LOGLVL_CONFIG, "main module register 0:0x%x\n",
			*main_regs_va + 0);
		vcmd_klog(LOGLVL_CONFIG, "main module register 80:0x%08x\n",
			*(main_regs_va + 80));
		vcmd_klog(LOGLVL_CONFIG, "main module register 214:0x%08x\n",
			*(main_regs_va + 214));
		vcmd_klog(LOGLVL_CONFIG, "main module register 226:0x%08x\n",
			*(main_regs_va + 226));
		vcmd_klog(LOGLVL_CONFIG, "main module register 287:0x%x\n",
			*(main_regs_va + 287));

		release_cmdbuf(vcmd_mgr, po, param[i].cmdbuf_id);
	}
}

/**
 * @brief convert subsys info to core array info of vcmd driver context.
 */
static void SubsysToVcmdCoreCfg(vcmd_mgr_t *vcmd_mgr)
{
	int i, k, array_sz;
	struct vcmd_subsys_info *subsys;
	struct sub_mod_cfg *mod_cfg;
	enum subsys_module_id mod_id;

	array_sz = ARRAY_SIZE(vcmd_core_array);

	/* To plug into vcx_vcmd_driver.c */
	for (i = 0; i < array_sz; i++) {
		if (vcmd_core_array[i].vcmd_base_addr) {
			subsys = &vcmd_mgr->core_array[vcmd_mgr->subsys_num];
			subsys->irq = vcmd_core_array[i].vcmd_irq;
			/* reg_base = vcmd_base_addr + reg_base_offset,
			 * but now reg_base_offset is 0, will be added later
			 */
			subsys->reg_base = vcmd_core_array[i].vcmd_base_addr +
							   /* vcmd_mgr->reg_base_offset */ 0;
			subsys->sub_module_type = vcmd_core_array[i].sub_module_type;
			subsys->vcmd_priority = vcmd_core_array[i].priority;

			for (k = 0; k < SUB_MOD_MAX; k++) {
				mod_cfg = &vcmd_core_array[i].submodule_cfg[k];
				mod_id = mod_cfg->sub_mod_id;
				if (mod_id < SUB_MOD_MAX) {
					subsys->reg_off[mod_id] = mod_cfg->io_off;
					if (mod_cfg->io_off != 0xffff) {
						subsys->io_size[mod_id] = mod_cfg->io_size;
						subsys->rreg_id[mod_id] = mod_cfg->rreg_id;
						subsys->rreg_num[mod_id] = mod_cfg->rreg_num;
					}
				} else {
					vcmd_klog(LOGLVL_ERROR, "wrong sub-module id in vcmd_core_array[%d].submodule_cfg[%d]\n",
							  i, k);
				}
			}

			vcmd_mgr->subsys_num++;
		}
	}
	vcmd_klog(LOGLVL_CONFIG, "%d VCMD cores found\n", vcmd_mgr->subsys_num);
}

/**
 * @brief get submodule offset in status buffer
 */
static void get_submodule_offset_in_status_buf(struct vcmd_subsys_info *subsys,
			struct enc_config_parameter *param)
{
	u16 offset;
	u32 size;

	param->status_main_addr = 0;
	size = ALIGN_64(subsys->io_size[SUB_MOD_MAIN]);
	offset = param->status_main_addr + size;

	if (param->submodule_L2Cache_addr != 0xffff) {
		param->status_L2Cache_addr = offset;
		size = ALIGN_64(subsys->io_size[SUB_MOD_L2CACHE]);
		offset += size;
	}
	if (param->submodule_dec400_addr != 0xffff) {
		param->status_dec400_addr = offset;
		size = ALIGN_64(subsys->io_size[SUB_MOD_DEC400]);
		offset += size;
	}
	if (param->submodule_MMU_addr[0] != 0xffff) {
		param->status_MMU_addr[0] = offset;
		size = ALIGN_64(subsys->io_size[SUB_MOD_MMU0]);
		offset += size;
	}
	if (param->submodule_MMU_addr[1] != 0xffff) {
		param->status_MMU_addr[1] = offset;
		size = ALIGN_64(subsys->io_size[SUB_MOD_MMU0]);
		offset += size;
	}
	if (param->submodule_axife_addr[0] != 0xffff) {
		param->status_axife_addr[0] = offset;
		size = ALIGN_64(subsys->io_size[SUB_MOD_AXIFE0]);
		offset += size;
	}
	if (param->submodule_axife_addr[1] != 0xffff) {
		param->status_axife_addr[0] = offset;
		size = ALIGN_64(subsys->io_size[SUB_MOD_AXIFE1]);
		offset += size;
	}
	if (param->submodule_ufbc_addr != 0xffff) {
		param->status_ufbc_addr = offset;
		size = ALIGN_64(subsys->io_size[SUB_MOD_UFBC]);
		offset += size;
	}

}

int xx_HANTRO_IOCH_GET_VCMD_ENABLE_vcmd(struct drm_device *dev, void *data,
		       struct drm_file *file_priv)
{
	*(unsigned long *)data = 1;
	return 0;
}

int xx_HANTRO_IOCH_GET_MMU_ENABLE_vcmd(struct drm_device *dev, void *data,
		       struct drm_file *file_priv)
{
	struct lisuan_file *gf_file = (struct lisuan_file *)file_priv->driver_priv;
	struct vcmd_priv_ctx *ctx = (struct vcmd_priv_ctx *)gf_file->enc_vcmd_priv;
	vcmd_mgr_t *vcmd_mgr = (vcmd_mgr_t *)ctx->vcmd_mgr;

	*(unsigned int *)data = vcmd_mgr->mmu_enable;
	return 0;
}

int xx_HANTRO_IOCH_GET_CMDBUF_PARAMETER(struct drm_device *dev, void *data,
		       struct drm_file *file_priv)
{
	struct enc_cmdbuf_mem_parameter mem;
	struct lisuan_file *gf_file = (struct lisuan_file *)file_priv->driver_priv;
	struct vcmd_priv_ctx *ctx = (struct vcmd_priv_ctx *)gf_file->enc_vcmd_priv;
	struct lisuan_device *gf_device = dev->dev_private;
	vcmd_mgr_t *vcmd_mgr = (vcmd_mgr_t *)ctx->vcmd_mgr;
	resource_size_t start = 0;
	struct mmap_args args = {0};
	uint64_t vcmd_buffer_cpuva = 0;

	vcmd_klog(LOGLVL_FLOW, " VCMD GET_CMDBUF_PARAMETER\n");
	mem.cmd_unit_size = SLOT_SIZE_CMDBUF;
	mem.status_unit_size = SLOT_SIZE_STATUSBUF;
	mem.reg_unit_size = SLOT_SIZE_REGBUF;
	mem.cmd_total_size = vcmd_mgr->mem_vcmd.size;
	mem.reg_total_size = vcmd_mgr->mem_regs.size;
	mem.status_total_size = vcmd_mgr->mem_status.size;
	mem.status_phy_addr = vcmd_mgr->mem_status.pa;
	mem.cmd_phy_addr = vcmd_mgr->mem_vcmd.pa;
	mem.reg_phy_addr = vcmd_mgr->mem_regs.pa;

	mem.status_hw_addr = vcmd_mgr->mem_status.pa;
	mem.cmd_hw_addr = vcmd_mgr->mem_vcmd.pa;
	mem.reg_hw_addr = vcmd_mgr->mem_regs.pa;
	mem.base_ddr_addr = vcmd_mgr->pa_trans_offset;

	//map vcmd buffer
	args.size = gf_device->enc_bufObj->size;
	args.domain = gf_device->enc_bufObj->domain;

	if (gf_device->enc_bufObj->flag.vram) {
		start = gf_device->enc_bufObj->vm_node->offset + gf_device->pdev_info.start_addr_vram - gf_device->mmu_bypass_start;
	} else {
		args.sg_table = gf_device->enc_bufObj->tbo.sg;
		args.snoop = gf_device->enc_bufObj->flag.snoop;
	}

	gf_file->enc_vcmd_map =(void*)&args;
	vcmd_buffer_cpuva = vm_mmap(file_priv->filp, 0, args.size, PROT_READ | PROT_WRITE, MAP_SHARED, start);
	gf_file->enc_vcmd_map = NULL;

	mem.cmd_virt_addr = vcmd_buffer_cpuva;
	mem.status_virt_addr = vcmd_buffer_cpuva + vcmd_mgr->mem_vcmd.size;
	mem.reg_virt_addr = vcmd_buffer_cpuva + vcmd_mgr->mem_vcmd.size + vcmd_mgr->mem_status.size;

	gf_memcpy((struct enc_cmdbuf_mem_parameter *)data, &mem,sizeof(struct enc_cmdbuf_mem_parameter));

	//printk("report cmd cpu pa 0x%lx, status cpu pa = 0x%lx, reg cpu pa = 0x%lx\n", mem.cmd_phy_addr, mem.status_phy_addr, mem.reg_phy_addr);
	//printk("report cmd hw pa 0x%lx, status hw pa = 0x%lx, reg hw pa = 0x%lx\n", mem.cmd_hw_addr, mem.status_hw_addr, mem.reg_hw_addr);

	return 0;
}

int xx_HANTRO_IOCH_GET_VCMD_PARAMETER(struct drm_device *ddev, void *data,
		       struct drm_file *file_priv)
{
	struct lisuan_file *gf_file = (struct lisuan_file *)file_priv->driver_priv;
	struct vcmd_priv_ctx *ctx = (struct vcmd_priv_ctx *)gf_file->enc_vcmd_priv;
	vcmd_mgr_t *vcmd_mgr = (vcmd_mgr_t *)ctx->vcmd_mgr;
	int i;
	struct enc_config_parameter param;
	struct proc_obj *po = NULL;
	unsigned long flags = 0;
	u16 m_type;
	struct hantrovcmd_dev *dev;
	struct vcmd_subsys_info *info = NULL;

	vcmd_klog(LOGLVL_FLOW, " VCMD get vcmd config parameter\n");
	gf_memcpy(&param, (struct enc_config_parameter *)data, sizeof(struct enc_config_parameter));

	po = (struct proc_obj *)ctx->po;
	if (!po) {
		vcmd_klog(LOGLVL_ERROR, "%s: not find process obj!\n", __func__);
		return -1;
	}
	spin_lock_irqsave(&po->spinlock, flags);
	po->module_type = param.module_type;
	m_type = param.module_type;
	spin_unlock_irqrestore(&po->spinlock, flags);
	if (vcmd_mgr->module_mgr[m_type].num) {
		dev = vcmd_mgr->module_mgr[m_type].dev[0];
		info = dev->subsys_info;
		param.submodule_main_addr = info->reg_off[SUB_MOD_MAIN];
		param.submodule_L2Cache_addr = info->reg_off[SUB_MOD_L2CACHE];
		param.submodule_dec400_addr = info->reg_off[SUB_MOD_DEC400];
		param.submodule_MMU_addr[0] = info->reg_off[SUB_MOD_MMU0];
		param.submodule_MMU_addr[1] = info->reg_off[SUB_MOD_MMU1];
		param.submodule_axife_addr[0] = info->reg_off[SUB_MOD_AXIFE0];
		param.submodule_axife_addr[1] = info->reg_off[SUB_MOD_AXIFE1];
		param.submodule_ufbc_addr = info->reg_off[SUB_MOD_UFBC];
		param.vcmd_hw_version_id =	dev->hw_version_id;
		param.vcmd_core_num = vcmd_mgr->module_mgr[m_type].num;
		//get priority of vcmd
		for (i = 0; i < vcmd_mgr->subsys_num; i++)
			param.vcmd_priority[i] = info->vcmd_priority;
	} else {
		param.submodule_main_addr = 0xffff;
		param.submodule_dec400_addr = 0xffff;
		param.submodule_L2Cache_addr = 0xffff;
		param.submodule_MMU_addr[0] = 0xffff;
		param.submodule_MMU_addr[1] = 0xffff;
		param.submodule_axife_addr[0] = 0xffff;
		param.submodule_axife_addr[1] = 0xffff;
		param.submodule_ufbc_addr = 0xffff;
		param.vcmd_core_num = 0;
		param.vcmd_hw_version_id = HW_ID_1_0_C;
		for (i = 0; i < vcmd_mgr->subsys_num; i++)
			param.vcmd_priority[i] = 0;
	}

	get_submodule_offset_in_status_buf(info, &param);

	vcmd_klog(LOGLVL_FLOW, "submodule_main_addr = 0x%x\n", param.submodule_main_addr);

	gf_memcpy((struct enc_config_parameter *)data, &param, sizeof(struct enc_config_parameter));

	return 0;
}

int xx_HANTRO_IOCH_RESERVE_CMDBUF(struct drm_device *dev, void *data,
		       struct drm_file *file_priv)
{
	struct lisuan_file *gf_file = (struct lisuan_file *)file_priv->driver_priv;
	struct vcmd_priv_ctx *ctx = (struct vcmd_priv_ctx *)gf_file->enc_vcmd_priv;
	vcmd_mgr_t *vcmd_mgr = (vcmd_mgr_t *)ctx->vcmd_mgr;
	long ret;
	struct enc_exchange_parameter param;
	struct proc_obj *po = ctx->po;

	gf_memcpy(&param, (struct enc_exchange_parameter *)data, sizeof(struct enc_exchange_parameter));

	ret = reserve_cmdbuf(vcmd_mgr, po, &param);
	if (ret == 0) {
		gf_memcpy((struct enc_exchange_parameter *)data, &param, sizeof(struct enc_exchange_parameter));
	}

	vcmd_klog(LOGLVL_FLOW, " VCMD Reserve CMDBUF %d\n", param.cmdbuf_id);
	return ret;
}

int xx_HANTRO_IOCH_LINK_RUN_CMDBUF(struct drm_device *dev, void *data,
		       struct drm_file *file_priv)
{
	struct lisuan_file *gf_file = (struct lisuan_file *)file_priv->driver_priv;
	struct vcmd_priv_ctx *ctx = (struct vcmd_priv_ctx *)gf_file->enc_vcmd_priv;
	vcmd_mgr_t *vcmd_mgr = (vcmd_mgr_t *)ctx->vcmd_mgr;
	struct enc_exchange_parameter param;
	long retVal;
	struct proc_obj *po = ctx->po;

	gf_memcpy(&param, (struct enc_exchange_parameter *)data, sizeof(struct enc_exchange_parameter));

	vcmd_klog(LOGLVL_CONFIG, "VCMD link and run CMDBUF %d\n", param.cmdbuf_id);
	retVal = link_and_run_cmdbuf(vcmd_mgr, po, &param, 0, 0);
	gf_memcpy((struct enc_exchange_parameter *)data, &param, sizeof(struct enc_exchange_parameter));

	return retVal;
}

int xx_HANTRO_IOCH_WAIT_CMDBUF(struct drm_device *dev, void *data,
		       struct drm_file *file_priv)
{
	struct lisuan_file *gf_file = (struct lisuan_file *)file_priv->driver_priv;
	struct vcmd_priv_ctx *ctx = (struct vcmd_priv_ctx *)gf_file->enc_vcmd_priv;
	vcmd_mgr_t *vcmd_mgr = (vcmd_mgr_t *)ctx->vcmd_mgr;
	u16 cmdbuf_id;
	long tmp;
	struct proc_obj *po = ctx->po;


	cmdbuf_id = *(u16 *)data;
	/*high 16 bits are core id, low 16 bits are cmdbuf_id*/

	vcmd_klog(LOGLVL_FLOW, "VCMD wait for CMDBUF %d finishing.\n", cmdbuf_id);

	//TODO
	tmp = wait_cmdbuf_ready(vcmd_mgr, po, cmdbuf_id, &cmdbuf_id, 0);
	if (tmp == 0 || tmp == 1) {
		*(u16 *)data = cmdbuf_id;
		return tmp; //return core_id
	} else {
		return -1;
	}

	return 0;
}

int xx_HANTRO_IOCH_RELEASE_CMDBUF(struct drm_device *dev, void *data,
		       struct drm_file *file_priv)
{
	struct lisuan_file *gf_file = (struct lisuan_file *)file_priv->driver_priv;
	struct vcmd_priv_ctx *ctx = (struct vcmd_priv_ctx *)gf_file->enc_vcmd_priv;
	vcmd_mgr_t *vcmd_mgr = (vcmd_mgr_t *)ctx->vcmd_mgr;
	u16 cmdbuf_id;
	struct proc_obj *po = ctx->po;

	cmdbuf_id = *(u16 *)data;
	/*16 bits are cmdbuf_id*/

	vcmd_klog(LOGLVL_FLOW, "VCMD release CMDBUF %d\n", cmdbuf_id);

	release_cmdbuf(vcmd_mgr, po, cmdbuf_id);
	return 0;
}

int xx_HANTRO_IOCH_POLLING_CMDBUF(struct drm_device *dev, void *data,
		       struct drm_file *file_priv)
{
	struct lisuan_file *gf_file = (struct lisuan_file *)file_priv->driver_priv;
	struct vcmd_priv_ctx *ctx = (struct vcmd_priv_ctx *)gf_file->enc_vcmd_priv;
	vcmd_mgr_t *vcmd_mgr = (vcmd_mgr_t *)ctx->vcmd_mgr;
	u16 core_id;
	u32 i;

	core_id = *(u16 *)data;

	vcmd_klog(LOGLVL_FLOW, "VCMD polling core 0x%x\n", core_id);

	/*16 bits are cmdbuf_id*/
	if ((core_id >= vcmd_mgr->subsys_num) && (core_id != 0xffff))
		return -1;
	//if (down_interruptible(&vcmd_mgr->isr_polling_sema))
	//	return -ERESTARTSYS;
	if (core_id != 0xffff) {
		lisuan_enc_vcmd_clear_irq_status(core_id, vcmd_mgr);
		hantroenc_vcmd_isr(core_id, vcmd_mgr, 0);
	} else {
		for (i = 0; i < vcmd_mgr->subsys_num; i++) {
			lisuan_enc_vcmd_clear_irq_status(core_id, vcmd_mgr);
			hantroenc_vcmd_isr(i, vcmd_mgr, 0);
		}
	}
	//up(&vcmd_mgr->isr_polling_sema);

	return 0;
}

/**
 * @brief open hantro vcmd device.
 */
int hantrovcmd_open(struct lisuan_device *gf_device, struct drm_file *filp)
{
	struct proc_obj *po = NULL;
	struct vcmd_priv_ctx *ctx = NULL;
	struct lisuan_file *gf_file = (struct lisuan_file *)filp->driver_priv;

	if (!gf_device->enc_vcmd_mgr) {
		gf_info("gf_device->enc_vcmd_mgr is NULL\n");
		return -1;
	}

	ctx = gf_kvcalloc(1, sizeof(struct vcmd_priv_ctx), GFP_KERNEL);
	if (!ctx) {
		vcmd_klog(LOGLVL_ERROR, "Create vcmd private context failed!\n");
		return -EINVAL;
	}
	gf_memset(ctx, 0, sizeof(struct vcmd_priv_ctx));

	gf_file->enc_vcmd_priv = (void *)ctx;

	ctx->vcmd_mgr = (void *)gf_device->enc_vcmd_mgr;

#ifdef SUPPORT_DBGFS
	/* for debugfs */
	_dbgfs_init_ctx(ctx->vcmd_mgr, 1);
#endif

	po = create_process_object();
	if (!po) {
		vcmd_klog(LOGLVL_ERROR, "Create process object failed!\n");
		return -EINVAL;
	}
	po->undone_cmdbuf_id = 0;
	po->has_undone = 0;
	po->filp = filp;
	po->module_type = ((vcmd_mgr_t *)ctx->vcmd_mgr)->core_array[0].sub_module_type;
	ctx->po = (void *)po;

	vcmd_klog(LOGLVL_FLOW, "dev opened\n");
	vcmd_klog(LOGLVL_FLOW, "process obj %p for filp opened %p\n", (void *)po, (void *)filp);
	return 0;
}

/**
 * @brief release hantro vcmd device.
 */
int hantrovcmd_release(struct lisuan_device *gf_device, struct drm_file *filp)
{
	struct lisuan_file *gf_file = (struct lisuan_file *)filp->driver_priv;
	struct vcmd_priv_ctx *ctx = (struct vcmd_priv_ctx *)gf_file->enc_vcmd_priv;
	vcmd_mgr_t *vcmd_mgr;
	struct proc_obj *po;
	struct hantrovcmd_dev *dev;
	bi_list *list;
	bi_list_node *node;
	struct cmdbuf_obj *obj;

	u32 module_type;
	u32 i;
	u32 has_work_node;
	unsigned long flags = 0;
	int abort_cmdbuf_id;

	if (!ctx) {
		gf_info("ctx is NULL\n");
		return 0;
	}

	vcmd_mgr = (vcmd_mgr_t *)ctx->vcmd_mgr;

	if (!vcmd_mgr) {
		gf_info("vcmd_mgr is NULL\n");
		return 0;
	}

	vcmd_klog(LOGLVL_FLOW, "%s process %p start release\n", __func__, (void *)filp);
	po = (struct proc_obj *)ctx->po;
	if (!po) {
		vcmd_klog(LOGLVL_ERROR, "%s: not find process obj!\n", __func__);
		return -1;
	}

	module_type = po->module_type;
	//if (down_interruptible(&vcmd_mgr->module_mgr[module_type].sem))
	//	return -ERESTARTSYS;

	//remove nodes in dev->work_list
	for (i = 0; i < vcmd_mgr->subsys_num; i++) {
		dev = &vcmd_mgr->dev_ctx[i];
		if (dev == NULL || dev->hwregs == NULL)
			continue;

		list = &dev->work_list;

		has_work_node = 0;
		spin_lock_irqsave(dev->spinlock, flags);
		if (dev->state == VCMD_STATE_WORKING) {
			node = list->head;
			while (node) {
				obj = (struct cmdbuf_obj *)node->data;
				if (obj->po == po) {
					has_work_node = 1;
					break;
				}
				node = node->next;
			}
		}
		spin_unlock_irqrestore(dev->spinlock, flags);

		if (has_work_node) {
			vcmd_abort(vcmd_mgr, dev, &abort_cmdbuf_id);
			
			if (dev->state != VCMD_STATE_IDLE) {
				continue;
			}

			//wait_fence_ready(vcmd_mgr, po->undone_cmdbuf_id);
			//release_cmdbuf(vcmd_mgr, po, po->undone_cmdbuf_id);
			spin_lock_irqsave(dev->spinlock, flags);
			node = list->head;
			while (node) {
				obj = (struct cmdbuf_obj *)node->data;
				vcmd_klog(LOGLVL_FLOW, "Process %p release: checking cmdbuf %d of process %p\n",
					filp, obj->cmdbuf_id, obj->filp);
				if (obj->po == po || obj->cmdbuf_need_remove)
					dev_remove_job(vcmd_mgr, dev, node);
				node = node->next;
			}

			vcmd_klog(LOGLVL_FLOW, "Restart dev[%d].\n", dev->core_id);
			vcmd_start(dev);
			spin_unlock_irqrestore(dev->spinlock, flags);
		}
	}

	// remove cmdbuf reserved but not in work_list
	for (i = 0; i < SLOT_NUM_CMDBUF; i++) {
		obj = &vcmd_mgr->objs[i];
		if (obj->po == po)
			return_cmdbuf(vcmd_mgr, i);
	}

	vcmd_klog(LOGLVL_FLOW, "process obj %p for filp to be removed: %p\n",
			(void *)po, (void *)po->filp);
	free_process_object(ctx->po);
	ctx->po = NULL;
	gf_kvfree(ctx);
	//up(&vcmd_mgr->module_mgr[module_type].sem);
	return 0;
}

/**
 * vm_ops
 */
static const struct vm_operations_struct hantrovcmd_vm_ops = {
#ifdef CONFIG_HAVE_IOREMAP_PROT
	.access = generic_access_phys
#endif
};

/**
 * @brief process abnormal interrupt
 */
static void process_abnormal_irq(vcmd_mgr_t *vcmd_mgr,
									struct hantrovcmd_dev *dev,
									struct cmdbuf_obj *obj)
{
	u32 intr_src;

	/* check vcmd interrupt source. */
	intr_src = vcmd_read_reg((const void *)dev->hwregs,
								VCMD_REGISTER_SW_EXT_INT_SRC_OFFSET);
	if (intr_src & dev->abn_irq_mask)
		process_vce_abn_irq(vcmd_mgr, dev, obj);
}

/**
 * @brief mark cmdbuf obj as run_done with specified exe_status,
 * remove it from device work_list, and add it to owner po's job_done_list.
 */
static int isr_process_node(vcmd_mgr_t *vcmd_mgr, bi_list_node *node,
							u32 exe_status)
{
	struct cmdbuf_obj *obj = (struct cmdbuf_obj *)node->data;
	struct hantrovcmd_dev *dev = &vcmd_mgr->dev_ctx[obj->core_id];
	struct proc_obj *po = NULL;

	if (obj->cmdbuf_run_done == 0) {
		obj->cmdbuf_run_done = 1;
		obj->executing_status = exe_status;
		dev_delink_job(dev, node);
		po = obj->po;
		if(po != NULL){
			proc_add_done_job(vcmd_mgr, obj);
		}
#ifdef SUPPORT_DBGFS
		if (exe_status == CMDBUF_EXE_STATUS_OK) {
			_dbgfs_remove_cmdbuf(dev->dbgfs_info, obj->cmdbuf_id);
			_dbgfs_record_vce_cycles(dev->dbgfs_info, obj->cmdbuf_id,
						obj->module_type, dev->hw_feature.has_apb_arbiter);
		}
#endif
		return 0;
	} else {
		vcmd_klog(LOGLVL_FLOW, "%s cmdbuf[%d] is already done!!\n",
				__func__, obj->cmdbuf_id);
		return -1;
	}
}

/**
 * @brief interrupt service routine of vcmd driver.
 */
int hantroenc_vcmd_isr(int irq, void *vvcmd_mgr, int is_internal)
{
	unsigned int handled = 0;
	vcmd_mgr_t *vcmd_mgr = (vcmd_mgr_t *)vvcmd_mgr;
	struct hantrovcmd_dev *dev = NULL;
	u32 irq_status = 0;
	unsigned long flags = 0;
	bi_list_node *node;
	addr_t exe_cmdbuf_ba;
	u32 curr_id = 0;
	struct cmdbuf_obj *curr_obj, *obj;
	bi_list_node *curr_node;
	u32 tmp_status;

	u32 i;
	u8 *hwregs;

	u32 instr = 0, size = 0;
	char log_buf[512];

	if (vcmd_mgr->vcmd_irq_enabled == 0) {
		/* all vcmd_irq==-1, there is no IRQ, use irq as dev id */
		if (irq < vcmd_mgr->subsys_num)
			dev = &vcmd_mgr->dev_ctx[irq];
	} else { /* there is assigned IRQ */
		for (i = 0; i < vcmd_mgr->subsys_num; i++) {
			if (vcmd_mgr->dev_ctx[i].subsys_info->irq == irq) {
				dev = &vcmd_mgr->dev_ctx[i];
				break;
			}
		}
	}

	if (dev == NULL) {
		gf_info("dev is null");
		return IRQ_HANDLED;
	}

#ifdef SUPPORT_DBGFS
	_dbgfs_reset_exe_cmdbuf_num(dev->dbgfs_info);
	_dbgfs_record_cmdbuf_num(dev->dbgfs_info);
#endif

	hwregs = dev->hwregs;

	vcmd_klog(LOGLVL_FLOW, "%s: core 0x%x received IRQ!\n", __func__, irq);
	spin_lock_irqsave(dev->spinlock, flags);
	irq_status = dev->irq_status;

	if (dev->hw_version_id >= HW_ID_1_5_0 && irq_status == 0) {
		curr_id = vcmd_get_register_value((const void *)dev->hwregs,
							dev->reg_mirror,
							HWIF_VCMD_CMDBUF_EXECUTING_ID);
		if (curr_id >= SLOT_NUM_CMDBUF) {
			vcmd_klog(LOGLVL_ERROR, "%s error cmdbuf_id %d !!\n",
						__func__, curr_id);
			spin_unlock_irqrestore(dev->spinlock, flags);
			return IRQ_HANDLED;
		}
		curr_obj = &vcmd_mgr->objs[curr_id];

		process_abnormal_irq(vcmd_mgr, dev, curr_obj);

		tmp_status = vcmd_read_reg((const void *)dev->hwregs,
									VCMD_REGISTER_INT_STATUS_OFFSET);

		vcmd_klog(LOGLVL_WARNING, "enter abnormal irq status = 0x%x", tmp_status);
		if (!tmp_status) {
			spin_unlock_irqrestore(dev->spinlock, flags);
			return IRQ_HANDLED;
		}
	}

#ifdef VCMD_DEBUG_INTERNAL
	_dbg_log_dev_regs(dev, 1);
#endif

	if (irq_status == 0) {
		//no interrupt source from dev
		vcmd_klog(LOGLVL_WARNING, "%s warning, irq_status is zero!\n", __func__);
		spin_unlock_irqrestore(dev->spinlock, flags);
		return IRQ_HANDLED;
	}

	dev->reg_mirror[VCMD_REGISTER_INT_STATUS_OFFSET / 4] = irq_status;

	node = dev->work_list.head;

	if (node == NULL) {
		//dev is not in use, should not run to here
		vcmd_klog(LOGLVL_ERROR, "%s:received IRQ but core has nothing to do.\n", __func__);
		spin_unlock_irqrestore(dev->spinlock, flags);
		return IRQ_HANDLED;
	}

	// get current node/obj/id
	if (dev->hw_version_id < HW_ID_1_0_C) {
		exe_cmdbuf_ba = VCMDGetAddrRegisterValue((const void *)hwregs,
												dev->reg_mirror,
												HWIF_VCMD_EXECUTING_CMD_ADDR);
		//get the executing cmdbuf node.
		curr_node = get_cmdbuf_node_by_addr(vcmd_mgr, dev, exe_cmdbuf_ba);
		if (curr_node == NULL) {
			vcmd_klog(LOGLVL_ERROR, "%s no node bind to executing cmd ba 0x%lx!!\n",
											__func__, exe_cmdbuf_ba);
			spin_unlock_irqrestore(dev->spinlock, flags);
			return IRQ_HANDLED;
		}
		curr_obj = (struct cmdbuf_obj *)node->data;
		curr_id = curr_obj->cmdbuf_id;
	} else {
		if (irq_status & VCMD_IRQ_ERR_MASK) {
			//if error, read curr_id from register directly.
			curr_id = vcmd_get_register_value((const void *)hwregs,
									dev->reg_mirror,
									HWIF_VCMD_CMDBUF_EXECUTING_ID);

		} else {
			//otherwise, read curr_id from vcmd reg_mem
			//curr_id = *(dev->reg_mem_va + REG_ID_CMDBUF_EXE_ID);
			if (is_internal) {
				curr_id = *(dev->reg_mem_va + REG_ID_CMDBUF_EXE_ID);
			} else {
				curr_id = vcmd_read_reg(dev->hwregs, 0x68);
			}
		}

		if (curr_id >= SLOT_NUM_CMDBUF) {
			vcmd_klog(LOGLVL_ERROR, "%s error cmdbuf_id %d !!\n",  __func__, curr_id);
			spin_unlock_irqrestore(dev->spinlock, flags);
			return IRQ_HANDLED;
		}

		curr_node = &vcmd_mgr->nodes[curr_id];
		curr_obj = &vcmd_mgr->objs[curr_id];

#ifdef VCMD_DEBUG_INTERNAL
		_dbg_log_dev_regs(dev, 0);
#endif

	}

	vcmd_klog(LOGLVL_FLOW, "irq_status of core[%u] is: 0x%x, cmd id is: %d\n", dev->core_id, irq_status, curr_id);

	if (curr_obj->core_id != dev->core_id) {
		vcmd_klog(LOGLVL_ERROR, "%s error cmdbuf_id %d, core_id[%d] is not dev id[%d] !!\n",
					  __func__, curr_id, curr_obj->core_id, dev->core_id);
		spin_unlock_irqrestore(dev->spinlock, flags);
		return IRQ_HANDLED;
	}

	/* process nodes between head node and curr_node (exclusive):
	 * 1. mark run_done = 1 & executing status = OK,
	 * 2. remove from dev work list.
	 * 3. add to owner's job_done_list
	 */
	if (curr_obj->cmdbuf_run_done == 0) {
		node = dev->work_list.head;
		while (node && node != curr_node) {
			obj = (struct cmdbuf_obj *)node->data;
			isr_process_node(vcmd_mgr, node, CMDBUF_EXE_STATUS_OK);

			node = node->next;
		}

		if (node == NULL) {
			vcmd_klog(LOGLVL_ERROR, "%s not find node[%d] in dev work_list!!\n",
						__func__, curr_obj->cmdbuf_id);
			spin_unlock_irqrestore(dev->spinlock, flags);
			return IRQ_HANDLED;
		}
	} else {
		// only occurs when for error irq
		if (!(irq_status & VCMD_IRQ_ERR_MASK)) {
			vcmd_klog(LOGLVL_FLOW, "%s normal irq trigger to already done cmdbuf[%d], status = 0x%x\n",
						__func__, curr_obj->cmdbuf_id, irq_status);
			vcmd_klog(LOGLVL_FLOW, "id in register = %d\n", vcmd_get_register_value((const void *)hwregs, dev->reg_mirror, HWIF_VCMD_CMDBUF_EXECUTING_ID));
		}
	}

	//curr_node process
	if (irq_status & VCMD_IRQ_ABORT) {
#ifdef TIMEOUT_IRQ_TIMER
		/* if vcmd abort waited, del vcmd timeout timer */
		_vcmd_timeout_del_timer(dev);
#endif
#ifdef SUPPORT_WATCHDOG
		_vcmd_watchdog_stop(dev);
#endif
		//abort error
		dev->state = VCMD_STATE_IDLE;
		dev->aborted_cmdbuf_id = curr_id;

		if (dev->abort_mode == 0) {
			// curr node is done
			isr_process_node(vcmd_mgr, curr_node, CMDBUF_EXE_STATUS_OK);
		} else {
			// curr node is aborted
			curr_obj->executing_status = CMDBUF_EXE_STATUS_ABORTED;
		}

		spin_unlock_irqrestore(dev->spinlock, flags);

		//to notify owner which triggered the abort
		wake_up_interruptible_all(&dev->abort_queue_vcmd);
		handled++;
		return IRQ_HANDLED;
	}
	if (irq_status & VCMD_IRQ_BUS_ERR) {
		//bus error, don't need to reset where to record status?
		dev->state = VCMD_STATE_IDLE;
		isr_process_node(vcmd_mgr, curr_node, CMDBUF_EXE_STATUS_BUSERR);

		vcmd_start(dev);
		spin_unlock_irqrestore(dev->spinlock, flags);

		handled++;
		return IRQ_HANDLED;
	}
	if (irq_status & VCMD_IRQ_TIMEOUT) {
		//time out
#ifdef TIMEOUT_IRQ_TIMER
		// start a timer to wait abort irq
		_vcmd_timeout_add_timer(dev);
#else //TIMEOUT_IRQ_TIMER
		//reset dev and re-start from curr node
		dev->state = VCMD_STATE_IDLE;

		vcmd_reset_current_asic(dev);
		vcmd_start(dev);
#endif //TIMEOUT_IRQ_TIMER
		spin_unlock_irqrestore(dev->spinlock, flags);

		handled++;
		return IRQ_HANDLED;
	}
	if (irq_status & VCMD_IRQ_CMD_ERR) {
		gf_error("%d command error\n", curr_id);

		gf_error("%d last in dev\n", dev->cur_cmd_id);
		for (i = 0; i < ASIC_VCMD_SWREG_AMOUNT; i++) {
			vcmd_klog(LOGLVL_ERROR, "vcmd swreg%d: 0x%x\n", i, vcmd_read_reg((const void *)dev->hwregs, i * 4));
		}
		gf_error("command buffer gpu pa = 0x%lx", curr_obj->cmd_pa);
		for (i = 0; i < curr_obj->cmdbuf_size / 4; i++) {
			if (i == instr) {
				//gf_memset(log_buf, 0, sizeof(log_buf));
				_dbg_log_instr(i, *(curr_obj->cmd_va + i), &size, log_buf);
				vcmd_klog(LOGLVL_ERROR, "%s", log_buf);
				instr += size;
			} else {
				vcmd_klog(LOGLVL_ERROR, "current cmdbuf data %d = 0x%x\n",
						i, *(curr_obj->cmd_va + i));
			}
		}

		//command error, re-start from next node
		dev->state = VCMD_STATE_IDLE;
		isr_process_node(vcmd_mgr, curr_node, CMDBUF_EXE_STATUS_CMDERR);

		vcmd_start(dev);
		spin_unlock_irqrestore(dev->spinlock, flags);

		handled++;
		return IRQ_HANDLED;
	}
	if (irq_status & VCMD_IRQ_ARBITER_ERR) {
		vcmd_klog(LOGLVL_ERROR, " get arbiter error, check if touch core register\n");

		spin_unlock_irqrestore(dev->spinlock, flags);
		handled++;
		return IRQ_HANDLED;
	}

	//JMP or END interrupt
	isr_process_node(vcmd_mgr, curr_node, CMDBUF_EXE_STATUS_OK);
	if (irq_status & VCMD_IRQ_END) {
		//end command interrupt, start next node
		dev->state = VCMD_STATE_IDLE;
		vcmd_start(dev);
	} else {
#ifdef SUPPORT_WATCHDOG
		_vcmd_watchdog_feed(dev);
#endif
	}
	spin_unlock_irqrestore(dev->spinlock, flags);
	handled++;

#ifdef SUPPORT_DBGFS
	_dbgfs_update_index(dev->dbgfs_info);
#endif

	if (!handled)
		vcmd_klog(LOGLVL_BRIEF, "IRQ received, but not hantro's!\n");

	return IRQ_HANDLED;
}

/**
 * @brief vcmd driver initialization
 * @return int __init: 0: successful; other: failed.
 */
int hantroenc_vcmd_init(struct lisuan_device *gf_device)
{
	int result = 0;
	vcmd_mgr_t *vcmd_mgr;
	struct hantrovcmd_dev *dev_ctx;

	vcmd_mgr = gf_kvcalloc(1, sizeof(vcmd_mgr_t), GFP_KERNEL);
	if (!vcmd_mgr)
		return -1;
	gf_memset(vcmd_mgr, 0, sizeof(vcmd_mgr_t));
	gf_device->enc_vcmd_mgr = vcmd_mgr;

	SubsysToVcmdCoreCfg(vcmd_mgr);

	vcmd_mgr->mem_vcmd.size = ALIGN_4K(SLOT_NUM_CMDBUF * SLOT_SIZE_CMDBUF);
	vcmd_mgr->mem_status.size = ALIGN_4K(SLOT_NUM_CMDBUF * SLOT_SIZE_STATUSBUF);
	vcmd_mgr->mem_regs.size = ALIGN_4K(vcmd_mgr->subsys_num * SLOT_SIZE_REGBUF);
	result = vcmd_init(gf_device, vcmd_mgr);
	if (result)
		goto err1;

	dev_ctx = gf_kvcalloc(1, sizeof(struct hantrovcmd_dev) * vcmd_mgr->subsys_num, GFP_KERNEL);
	if (!dev_ctx)
		goto err1;
	gf_memset(dev_ctx, 0, sizeof(struct hantrovcmd_dev) * vcmd_mgr->subsys_num);

	vcmd_mgr->dev_ctx = dev_ctx;
	dev_ctx_init(vcmd_mgr, gf_device);

	sema_init(&vcmd_mgr->isr_polling_sema, 1);

	vcmd_mgr->init_po = create_process_object();
	if (!vcmd_mgr->init_po)
		goto err1;

	result = vcmd_reserve_IO(gf_device, vcmd_mgr);

	if (result < 0)
		goto err;

	result = vcmd_config_modules(vcmd_mgr);
	if (result < 0)
		goto err;

	vcmd_reset_asic(vcmd_mgr);

#ifdef SUPPORT_DBGFS
	/* for debugfs */
	if (_dbgfs_init((void *)vcmd_mgr))
		goto err;
	_dbgfs_init_ctx((void *)vcmd_mgr, 0);
#endif

#ifdef IRQ_SIMULATION
	_irq_simul_init((void *)vcmd_mgr);
#endif

	vcmd_init_objs(vcmd_mgr);
	vcmd_init_nodes(vcmd_mgr);
	vcmd_init_free_obj_list(vcmd_mgr);

	/* create vcmd kthread, which need to be woken up */
	_vcmd_kthread_create(vcmd_mgr);

	/* read all registers of main-module for each dev
	 * for analyzing configuration in cwl
	 */
	read_main_module_all_registers(vcmd_mgr);

	vcmd_klog(LOGLVL_CONFIG, "vcx_vcmd_driver: module inserted. Major <%d>\n",
		vcmd_mgr->hantrovcmd_major);

	return 0;

err:
	vcmd_release_IO(vcmd_mgr);
err1:
	_vcmd_pcie_cleanup(gf_device, vcmd_mgr);
	free_process_object(vcmd_mgr->init_po);

	if (vcmd_mgr->dev_ctx) {
		gf_kvfree(vcmd_mgr->dev_ctx);
		vcmd_mgr->dev_ctx = NULL;
	}
	if (vcmd_mgr) {
		gf_kvfree(vcmd_mgr);
		vcmd_mgr = NULL;
	}

	gf_device->enc_vcmd_mgr = NULL;

	vcmd_klog(LOGLVL_ERROR, "module not inserted!\n");
	return result;
}

/**
 * @brief de-init vcmd driver.
 */
void hantroenc_vcmd_cleanup(struct lisuan_device *gf_device)
{
	int i = 0;
	u32 result;
	vcmd_mgr_t *vcmd_mgr = gf_device->enc_vcmd_mgr;
	struct hantrovcmd_dev *dev_ctx;

	if (!vcmd_mgr)
		return;

	dev_ctx = vcmd_mgr->dev_ctx;

	_vcmd_kthread_stop(vcmd_mgr);

	for (i = 0; i < vcmd_mgr->subsys_num; i++) {
		if (!dev_ctx[i].hwregs)
			continue;
		//disable interrupt at first
		vcmd_write_reg((const void *)dev_ctx[i].hwregs,
					 VCMD_REGISTER_INT_CTL_OFFSET, 0x0000);
		//disable HW
		vcmd_write_reg((const void *)dev_ctx[i].hwregs,
					 VCMD_REGISTER_CONTROL_OFFSET, 0x0000);
		//read status register
		result = vcmd_read_reg((const void *)dev_ctx[i].hwregs,
						 VCMD_REGISTER_INT_STATUS_OFFSET);
		//clean status register
		vcmd_write_reg((const void *)dev_ctx[i].hwregs,
					 VCMD_REGISTER_INT_STATUS_OFFSET, result);
	}

#ifdef HANTROMMU_SUPPORT
	MMUCleanup(vcmd_mgr->mmu_hwregs);
#endif

#ifdef SUPPORT_DBGFS
	_dbgfs_cleanup((void *)vcmd_mgr);
#endif

	vcmd_release_IO(vcmd_mgr);
	gf_kvfree(dev_ctx);

	//release_vcmd_non_cachable_memory();

	_vcmd_pcie_cleanup(gf_device, vcmd_mgr);

	free_process_object(vcmd_mgr->init_po);
	gf_kvfree(vcmd_mgr);
	gf_device->enc_vcmd_mgr = NULL;
	vcmd_klog(LOGLVL_FLOW, "module removed\n");
}

void hantro_enc_resume(struct lisuan_device *gf_device)
{
	int n;
	vcmd_mgr_t *vcmd_mgr = gf_device->enc_vcmd_mgr;
	struct hantrovcmd_dev *dev_ctx = vcmd_mgr->dev_ctx;

	for (n = 0; n < vcmd_mgr->subsys_num; n++) {
		dev_ctx[n].state = VCMD_STATE_POWER_ON;
		dev_ctx[n].sw_cmdbuf_rdy_num = 0;
	}

	vcmd_reset_asic(vcmd_mgr);
}

void lisuan_enc_vcmd_clear_irq_status(int irq, void *vvcmd_mgr)
{
	vcmd_mgr_t *vcmd_mgr = (vcmd_mgr_t *)vvcmd_mgr;
	struct hantrovcmd_dev *dev = &vcmd_mgr->dev_ctx[irq];
	u8 *hwregs = dev->hwregs;

	dev->irq_status = vcmd_read_reg((const void *)hwregs, VCMD_REGISTER_INT_STATUS_OFFSET);
	vcmd_write_reg((const void *)hwregs, VCMD_REGISTER_INT_STATUS_OFFSET, dev->irq_status);
}

void lisuan_enc0_vcmd_worker(struct work_struct *work)
{
	struct lisuan_device *ls_dev = container_of(work, struct lisuan_device, enc0_vcmd_work);

	hantroenc_vcmd_isr(0, ls_dev->enc_vcmd_mgr, 0);
}

void lisuan_im_vcmd_worker(struct work_struct *work)
{
	struct lisuan_device *ls_dev = container_of(work, struct lisuan_device, im_vcmd_work);

	hantroenc_vcmd_isr(1, ls_dev->enc_vcmd_mgr, 0);
}

int gf_enc_link_and_run(struct drm_device *dev, void *data, struct drm_file *file_priv, unsigned long long ring_fence_value, int procid)
{
	struct lisuan_file *gf_file = (struct lisuan_file *)file_priv->driver_priv;
	struct vcmd_priv_ctx *ctx = (struct vcmd_priv_ctx *)gf_file->enc_vcmd_priv;
	vcmd_mgr_t *vcmd_mgr = (vcmd_mgr_t *)ctx->vcmd_mgr;
	struct enc_exchange_parameter param;
	long retVal;
	struct proc_obj *po = ctx->po;
	bi_list_node *node = NULL;
	struct cmdbuf_obj *obj = NULL;

	gf_memcpy(&param, (struct enc_exchange_parameter *)data, sizeof(struct enc_exchange_parameter));
	node = &vcmd_mgr->nodes[param.cmdbuf_id];
	obj = (struct cmdbuf_obj *)(node->data);
	obj->submit_fence_id = ring_fence_value;

	vcmd_klog(LOGLVL_CONFIG, "VCMD link and run CMDBUF %d\n", param.cmdbuf_id);
	retVal = link_and_run_cmdbuf(vcmd_mgr, po, &param, 0, procid);
	gf_memcpy((struct enc_exchange_parameter *)data, &param, sizeof(struct enc_exchange_parameter));

	return retVal;
}
