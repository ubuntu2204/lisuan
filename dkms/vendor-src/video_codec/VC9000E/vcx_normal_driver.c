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

/* our own stuff */
#include <linux/kthread.h>
#include "gf_os_interface.h"
#include "vcx_driver.h"
#include "vcx_normal_cfg.h"
#include "lisuan_device.h"
#include "lisuan_drm.h"

#ifdef HANTROMMU_SUPPORT
extern unsigned int mmu_enable;
volatile u8 *mmu_hwregs[HXDEC_MAX_CORES][2];
#else
static unsigned int mmu_enable;
#endif

/**
 * Add function get_of_property() to get data from DTB,
 * it's added to match Qemu implementition
 * (node & irq string are hard-coded in Qemu model).
 */
#ifdef DTB_SUPPORT
#define VCE_DTB_NODE_NAME "verisilicon,vcx"
#define VCE_DTB_IRQ_NAME  "vcx_irq"
#endif

//#define MULTI_THR_TEST
#ifdef MULTI_THR_TEST

#define WAIT_NODE_NUM 32
struct wait_list_node {
	u32 node_id; //index of the node
	u32 used_flag; //1:the node is insert to the wait queue list.
	u32 sem_used; //1:the source is released and the semphone is uped.
	struct semaphore wait_sem; //the unique semphone for per reserve_encoder thread.
	u32 wait_cond; //the condition for wait. Equal to the "core_info".
	struct list_head wait_list; //list node.
};

static struct list_head reserve_header;
static struct wait_list_node res_wait_node[WAIT_NODE_NUM];

static void wait_delay(unsigned int delay)
{
	if (delay > 0) {
#if KERNEL_VERSION(2, 6, 28) <= LINUX_VERSION_CODE
		ktime_t dl = ktime_set((delay / MSEC_PER_SEC),
				       (delay % MSEC_PER_SEC) * NSEC_PER_MSEC);
		__set_current_state(TASK_UNINTERRUPTIBLE);
		schedule_hrtimeout(&dl, HRTIMER_MODE_REL);
#else
		gf_msleep(delay);
#endif
	}
}

static u32 request_wait_node(struct wait_list_node **node, u32 start_id)
{
	u32 i;
	struct wait_list_node *temp_node;

	while (1) {
		for (i = start_id; i < WAIT_NODE_NUM; i++) {
			temp_node = &res_wait_node[i];
			if (temp_node->used_flag == 0) {
				temp_node->used_flag = 1;
				*node = temp_node;
				return i;
			}
		}
		wait_delay(10);
	}
}

static void request_wait_sema(struct wait_list_node **node)
{
	u32 i;
	struct wait_list_node *temp_node;

	while (1) {
		for (i = 0; i < WAIT_NODE_NUM; i++) {
			temp_node = &res_wait_node[i];
			if (temp_node->used_flag == 0 &&
			    temp_node->sem_used == 0) {
				temp_node->sem_used = 1;
				*node = temp_node;
				return;
			}
		}
		wait_delay(10);
	}
}

static void init_wait_node(struct wait_list_node *node, u32 cond, u32 sem_flag)
{
	node->used_flag = 0;
	node->wait_cond = cond;
	sema_init(&node->wait_sem, sem_flag);
	INIT_LIST_HEAD(&node->wait_list);
	if (sem_flag > 0)
		node->sem_used = 1;
}

static void init_reserve_wait(u32 dev_num)
{
	u32 i;
	u32 cond = 0x80000001;
	u32 sem_flag = 0;
	struct wait_list_node *node;

	//    printk("%s,%d, dev_num %d\n",__FUNCTION__,__LINE__,dev_num);

	INIT_LIST_HEAD(&reserve_header);

	for (i = 0; i < WAIT_NODE_NUM; i++) {
		if (i < dev_num)
			sem_flag = 1;
		else
			sem_flag = 0;
		node = &res_wait_node[i];
		node->node_id = i;
		init_wait_node(node, cond, sem_flag);
	}
}

void release_reserve_wait(u32 dev_num)
{
}

#endif
/********variables declaration related with race condition**********/

static struct semaphore enc_core_sem;
static DECLARE_WAIT_QUEUE_HEAD(hw_queue);
static DEFINE_SPINLOCK(owner_lock);
static DECLARE_WAIT_QUEUE_HEAD(enc_wait_queue);

#ifdef PCIE_EN
/*******************PCIE CONFIG*************************/
#ifdef EMU
#define PCI_VENDOR_ID_HANTRO          0x1d9b//0x10ee//0x16c3
#define PCI_DEVICE_ID_HANTRO_PCI      0xface//0x8014// 0x7011
/* Base address got control register */
#define PCI_H2_BAR              2
/* Base address DDR register */
#define PCI_DDR_BAR             4
#else
#define PCI_VENDOR_ID_HANTRO          0x10ee//0x16c3
#define PCI_DEVICE_ID_HANTRO_PCI      0x8014// 0x7011
/* Base address got control register */
#define PCI_H2_BAR              4
/* Base address DDR register */
#define PCI_DDR_BAR             0
#endif

struct pci_dev *gDev;    /* PCI device structure. */
unsigned long gBaseHdwr; /* PCI base register address (Hardware address) */
unsigned long gBaseDDRHw;/* PCI base register address (memalloc) */
static u32 gBaseLen;            /* Base register address Length */
#endif

/*------------------------------------------------------------------------
 *****************************PORTING LAYER********************************
 *--------------------------------------------------------------------------
 */
/*------------------------------END-------------------------------------*/

/***************************TYPE AND FUNCTION DECLARATION****************/

/* here's all the must remember stuff */
typedef struct {
	SUBSYS_DATA subsys_data; //config of each core,such as base addr, iosize,etc
	u32 hw_id; //VCE/VCEJ hw id to indicate project
	u32 subsys_id; //subsys id for driver and sw internal use
	u32 is_valid; //indicate this subsys is hantro's core or not
	int pid[CORE_MAX]; //indicate which process is occupying the subsys
	u32 is_reserved[CORE_MAX]; //indicate this subsys is occupied by user or not
	u32 irq_received[CORE_MAX]; //indicate which core receives irq
	u32 irq_status[CORE_MAX]; //IRQ status of each core
	// poll sliceinfo timeout IRQ status of each core
	u32 poll_sliceinfo_timeout_irq_status[CORE_MAX];
	// ufbc decode error IRQ status of each core
	u32 ufbc_dec_err_irq_status[CORE_MAX];
	u32 job_id[CORE_MAX];
	char *buffer;
	unsigned int buffsize;

	u8 *hwregs;
	struct fasync_struct *async_queue;

	//watchdog timer to check if there is no response from hardware.
	struct timer_list watchdog_timer;
	u8 watchdog_active; // 1: watchdog is start
	u8 watchdog_triggered; // 1: watchdog is triggered
} hantroenc_t;

//static void ResetAsic(hantroenc_t * dev);

#ifdef hantroenc_DEBUG
static void dump_regs(unsigned long data);
#endif

/*********************local variable declaration*****************/
static unsigned long sram_base;
static unsigned int sram_size;
static int linebuffer_sram_used;
/* and this is our MAJOR; use 0 for dynamic allocation (recommended)*/
static int hantroenc_major;
static int total_subsys_num;
static int total_core_num;
static volatile unsigned int asic_status;
/* dynamic allocation*/
static hantroenc_t *hantroenc_data;
#ifdef IRQ_SIMULATION
struct timer_list timer0;
struct timer_list timer1;
#endif

/********************************************************************
 * watchdog and kthread functions
 ********************************************************************/
static struct task_struct *kthread;
static u8 stop_kthread;
static wait_queue_head_t kthread_waitq;

#ifdef SUPPORT_WATCHDOG
/**
 * @brief hook function for system-driver to do further process
 *  for tiggered watchdog.
 */
static void hook_watchdog(void *_dev, u32 id, int succeed)
{
	if (succeed)
		gf_error("hantroenc: axife[%d]flush succeed!!", id);
	else
		gf_error("hantroenc: axife[%d] flush failed, need to re-power sub-system",
				id);
}

/**
 * @brief stop vce when watchdog triggered
 */
int watchdog_stop_vce(volatile u8 *reg_base, spinlock_t *spinlock)
{
	u32 loop_count = 20;
	unsigned long flags = 0;

	if ((u32)gf_read_mmio32((reg_base + 0x14)) & 0x1) {
		//Stop VCE by setting reg5 bit0 to 0.
		spin_lock_irqsave(spinlock, flags);
		gf_write_mmio32((reg_base + 0x14), 0);
		spin_unlock_irqrestore(spinlock, flags);
		// wait vce core disable.
		do {
			if (((u32)gf_read_mmio32((reg_base + 0x14)) & 0x1) == 0)
				return 0;
			mdelay(10); // wait 10ms
		} while (loop_count--);
		gf_error("hantroenc: too long before vce core disable\n");
		return -1;
	}
	return 0;
}

/**
 * @brief process when watchdog triggered.
 */
static void watchdog_process(hantroenc_t *dev)
{
	int succeed, i;
	u32 core_id = dev->subsys_id;
	u32 axife_id;

	volatile u8 *axife_hwregs;

	if (core_id >= total_subsys_num)
		return;

	watchdog_stop_vce(dev->hwregs, &owner_lock);
#ifdef HANTROAXIFE_SUPPORT
	for (i = 0, axife_id = CORE_AXIFE; i < 2; i++, axife_id = CORE_AXIFE_1) {
		succeed = 1;
		if (dev->subsys_data.core_info.offset[axife_id] == 0)
			continue;
		axife_hwregs = dev->hwregs +
					   dev->subsys_data.core_info.offset[axife_id];
		if (AXIFEFlush(axife_hwregs) == -1)
			succeed = 0;
		hook_watchdog(dev, i, succeed);
	}
#endif
}
#endif //SUPPORT_WATCHDOG

/**
 * @brief To check subsys/dev's actions which need kthread to process
 * @return int: 0: no actions; 1: have actions
 */
static int _kthread_actions(hantroenc_t **dev)
{
	int ret = 0, i;

	if (stop_kthread == 1)
		return 1;

	for (i = 0; i < total_subsys_num; i++) {
		*dev = &hantroenc_data[i];
		if ((*dev)->watchdog_triggered == 1) {
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
 * @brief hantroenc kernel thread main function
 */
static int _kthread_fn(void *data)
{
	hantroenc_t *dev = NULL;

	while (!kthread_should_stop()) {
		if (wait_event_interruptible(kthread_waitq,
				_kthread_actions(&dev))) {
			gf_error("hantroenc: %s: signaled!!!\n", __func__);
			return -ERESTARTSYS;
		}

		if (dev == NULL)
			continue;

#ifdef SUPPORT_WATCHDOG
		if (dev->watchdog_triggered == 1) {
			dev->watchdog_triggered = 0;
			watchdog_process(dev);
			continue;
		}
#endif
	}

	return 0;
}

/**
 * @brief wake up hantroenc kernel thread
 */
static void hantroenc_kthread_wakeup(void)
{

	if (IS_ERR(kthread))
		return;

	wake_up_interruptible_all(&kthread_waitq);
}

/**
 * @brief create kernel thread for hantroenc driver
 */
static void hantroenc_kthread_create(void)
{
	stop_kthread = 0;
	init_waitqueue_head(&kthread_waitq);
	kthread = kthread_run(_kthread_fn, NULL, "hantroenc_kthread");
	if (IS_ERR(kthread)) {
		gf_error("hantroenc: create hantroenc kthread failed\n");
		return;
	}
}

/**
 * @brief stop kernel thread
 */
static void hantroenc_kthread_stop(void)
{
	if (!IS_ERR(kthread)) {
		stop_kthread = 1;
		kthread_stop(kthread);
		kthread = NULL;
	}
}

#ifdef SUPPORT_WATCHDOG
/**
 * @brief timer callback function of watchdog
 */
#if (KERNEL_VERSION(4, 14, 0) > LINUX_VERSION_CODE)
static void _watchdog_cb(unsigned long arg)
#else
static void _watchdog_cb(struct timer_list *timer)
#endif
{
#if (KERNEL_VERSION(4, 14, 0) > LINUX_VERSION_CODE)
	struct timer_list *timer = (struct timer_list *)arg;
#endif
	hantroenc_t *dev;

	dev = container_of(timer, hantroenc_t, watchdog_timer);

	dev->watchdog_triggered = 1;
	hantroenc_kthread_wakeup();
}

/**
 * @brief start watchdog
 */
static void _watchdog_start(hantroenc_t *dev)
{
	struct timer_list *timer = &dev->watchdog_timer;

#if (KERNEL_VERSION(4, 14, 0) > LINUX_VERSION_CODE)
	init_timer(timer);
	timer->function = _watchdog_cb;
	timer->data = (unsigned long)timer;
#else
	timer_setup(timer, _watchdog_cb, 0);
#endif
	dev->watchdog_active = 1;
}

/**
 * stop watchdog
 */
static void _watchdog_stop(hantroenc_t *dev)
{
	if (dev->watchdog_active) {
		del_timer(&dev->watchdog_timer);
		dev->watchdog_active = 0;
	}
}

/**
 * feed and start watchdog
 */
static void _watchdog_feed(hantroenc_t *dev)
{
	if (dev->watchdog_active == 0)
		_watchdog_start(dev);
	mod_timer(&dev->watchdog_timer,
		(jiffies + HZ * ONE_JOB_WAIT_TIME / 1000));
}
#endif // SUPPORT_WATCHDOG



/******************************************************************************/
static int CheckEncIrq(hantroenc_t *dev, u32 *core_info, u32 *irq_status,
		       u32 *job_id)
{
	unsigned long flags = 0;
	int rdy = 0;
	u8 core_type = 0;
	u8 subsys_idx = 0;

	core_type = (u8)(*core_info & 0x0F);
	subsys_idx = (u8)(*core_info >> 4);

	if (subsys_idx > total_subsys_num - 1) {
		*core_info = -1;
		*irq_status = 0;
		return 1;
	}

	spin_lock_irqsave(&owner_lock, flags);

	if (dev[subsys_idx].irq_received[core_type]) {
		/* reset the wait condition(s) */
		gf_debug("check subsys[%d][%d] irq ready\n", subsys_idx, core_type);
		//dev[subsys_idx].irq_received[core_type] = 0;
		rdy = 1;
		*core_info = subsys_idx;
		*irq_status = dev[subsys_idx].irq_status[core_type];
		if (job_id)
			*job_id = dev[subsys_idx].job_id[core_type];
	}

#ifdef LOW_LATENCY_SLICEINFO_SUPPORT
	if (dev[subsys_idx].poll_sliceinfo_timeout_irq_status[core_type] &
			ASIC_STATUS_POLL_SLICEINFO_TIMEOUT && job_id) {
		rdy = 1;
		*core_info = subsys_idx;
		*irq_status |= dev[subsys_idx].ufbc_dec_err_irq_status[core_type];
		*job_id = dev[subsys_idx].job_id[core_type];
	}
#endif

#ifdef SUPPORT_UFBC
	if (dev[subsys_idx].ufbc_dec_err_irq_status[core_type] &
			ASIC_STATUS_UFBC_DEC_ERR && job_id) {
		rdy = 1;
		*core_info = subsys_idx;
		*irq_status |= dev[subsys_idx].ufbc_dec_err_irq_status[core_type];
		*job_id = dev[subsys_idx].job_id[core_type];
	}
#endif

	spin_unlock_irqrestore(&owner_lock, flags);

	return rdy;
}

static unsigned int WaitEncReady(hantroenc_t *dev, u32 *core_info,
				 u32 *irq_status)
{
	gf_debug("%s\n", __func__);

	if (wait_event_interruptible(enc_wait_queue,
				     CheckEncIrq(dev, core_info, irq_status, NULL))) {
		gf_debug("ENC wait_event_interruptible interrupted\n");
		return -ERESTARTSYS;
	}

	return 0;
}

static int CheckEncIrqbyPolling(hantroenc_t *dev, u32 *core_info,
				u32 *irq_status, u32 *job_id)
{
	unsigned long flags = 0;
	int rdy = 0;
	u8 core_type = 0;
	u8 subsys_idx = 0;
	u32 irq, hwId, majorId, wClr, minorId, productId;
	unsigned long reg_offset = 0;
	u32 loop = 300;
	u32 interval = 10;
	u32 enable_status = 0;

	core_type = (u8)(*core_info & 0x0F);
	subsys_idx = (u8)(*core_info >> 4);

	if (subsys_idx > total_subsys_num - 1) {
		*core_info = -1;
		*irq_status = 0;
		return 1;
	}

	do {
		spin_lock_irqsave(&owner_lock, flags);
		if (dev[subsys_idx].is_reserved[core_type] == 0) {
			/*printk(KERN_DEBUG"subsys[%d][%d]  is not reserved\n",
			 *subsys_idx, core_type);
			 */
			goto end_1;
		} else if (dev[subsys_idx].irq_received[core_type] &&
			   (dev[subsys_idx].irq_status[core_type] &
			    (ASIC_STATUS_FUSE_ERROR | ASIC_STATUS_HW_TIMEOUT |
			     ASIC_STATUS_BUFF_FULL | ASIC_STATUS_HW_RESET |
			     ASIC_STATUS_ERROR | ASIC_STATUS_FRAME_READY))) {
			rdy = 1;
			*core_info = subsys_idx;
			*irq_status = dev[subsys_idx].irq_status[core_type];
			*job_id = dev[subsys_idx].job_id[core_type];
			goto end_1;
		}

		reg_offset =
			dev[subsys_idx].subsys_data.core_info.offset[core_type];
		irq = (u32)gf_read_mmio32(
			(dev[subsys_idx].hwregs + reg_offset + 0x04));

		enable_status = (u32)gf_read_mmio32(
			(dev[subsys_idx].hwregs + reg_offset + 20));

		if (irq & ASIC_STATUS_ALL) {
			gf_debug("check subsys[%d][%d] irq ready\n", subsys_idx,
			       core_type);
			if (irq & 0x20)
				gf_write_mmio32((dev[subsys_idx].hwregs +
						      reg_offset + 0x14), 0);

			/* clear all IRQ bits. (hwId >= 0x80006100) means IRQ is cleared
			 * by writing 1
			 */
			hwId = gf_read_mmio32(dev[subsys_idx].hwregs +
					reg_offset);
			productId = (hwId & 0xFFFF0000) >> 16;
			majorId = (hwId & 0x0000FF00) >> 8;
			minorId = (hwId & 0x000000FF);
			wClr = ((majorId >= 0x61) || (productId == 0x9000) || (productId == 0x9010) ||
			((productId == 0x8000) && (minorId >= 1))) ? irq : (irq & (~0x1FD));
			gf_write_mmio32((dev[subsys_idx].hwregs +
						 reg_offset + 0x04), wClr);

			rdy = 1;
			*core_info = subsys_idx;
			*irq_status = irq;
			dev[subsys_idx].irq_received[core_type] = 1;
			dev[subsys_idx].irq_status[core_type] = irq;
			*job_id = dev[subsys_idx].job_id[core_type];
			goto end_1;
		}

#ifdef LOW_LATENCY_SLICEINFO_SUPPORT
		u32 poll_sliceinfo_irq_status = 0;
		u32 hw_reset_irq = 0;

		poll_sliceinfo_irq_status =
			 (u32)gf_read_mmio32((dev[subsys_idx].hwregs + reg_offset + 0x1d8));
		poll_sliceinfo_irq_status = (poll_sliceinfo_irq_status & 0x01) << 13;
		if (poll_sliceinfo_irq_status & ASIC_STATUS_POLL_SLICEINFO_TIMEOUT) {
			wClr = poll_sliceinfo_irq_status >> 13;
			gf_write_mmio32((dev[subsys_idx].hwregs + reg_offset + 0x1d8), wClr);
			dev[subsys_idx].poll_sliceinfo_timeout_irq_status[core_type] =
				poll_sliceinfo_irq_status;
			rdy = 1;
			*core_info = subsys_idx;
			dev[subsys_idx].irq_received[core_type] = 1;
			*irq_status =
			     dev[subsys_idx].poll_sliceinfo_timeout_irq_status[core_type];
			*job_id = dev[subsys_idx].job_id[core_type];
			goto end_1;
		}
#endif

#ifdef SUPPORT_UFBC
		u32 ufbc_dec_irq_status = 0;

		ufbc_dec_irq_status =
			 (u32)gf_read_mmio32((dev[subsys_idx].hwregs + reg_offset + 0xc20));
		ufbc_dec_irq_status = (ufbc_dec_irq_status & 0x01) << 14;
		if (ufbc_dec_irq_status & ASIC_STATUS_UFBC_DEC_ERR) {
			wClr = ufbc_dec_irq_status >> 14;
			gf_write_mmio32((dev[subsys_idx].hwregs + reg_offset + 0xc20), wClr);
			dev[subsys_idx].ufbc_dec_err_irq_status[core_type] =
				ufbc_dec_irq_status;
			rdy = 1;
			*core_info = subsys_idx;
			*irq_status = irq;
			*irq_status |=
			     dev[subsys_idx].ufbc_dec_err_irq_status[core_type];
			*job_id = dev[subsys_idx].job_id[core_type];
			goto end_1;
		}
#endif

		spin_unlock_irqrestore(&owner_lock, flags);
		mdelay(interval);
	} while (loop--);
	goto end_2;

end_1:
	spin_unlock_irqrestore(&owner_lock, flags);
end_2:
	return rdy;
}

static int CheckEncAnyIrqByPolling(hantroenc_t *dev, CORE_WAIT_OUT *out)
{
	u32 i;
	int rdy = 0;
	u32 core_info, irq_status, job_id=0;
	u32 core_type = CORE_VCE;

	for (i = 0; i < total_subsys_num; i++) {
		if (!(dev[i].subsys_data.core_info.type_info & (1 << core_type)))
			continue;

		core_info = ((i << 4) | core_type);
		if ((CheckEncIrqbyPolling(dev, &core_info, &irq_status, &job_id) == 1) &&
		    (core_info == i)) {
#ifdef SUPPORT_WATCHDOG
			/* if there is an interrupt(exclusive slice ready) from VCE,
			 * should stop the watchdog.
			 */
			if (irq_status & ASIC_STATUS_ALL & ~ASIC_STATUS_SLICE_READY)
				_watchdog_stop(&dev[i]);
#endif
			out->job_id[out->irq_num] = job_id;
			out->irq_status[out->irq_num] = irq_status;
			//printk(KERN_DEBUG "irq_status of subsys %d job_id %d is:%x\n",i,job_id,irq_status);
			out->irq_num++;
			rdy = 1;
		}
	}

	return rdy;
}

static unsigned int WaitEncAnyReadyByPolling(hantroenc_t *dev, CORE_WAIT_OUT *out)
{
	CheckEncAnyIrqByPolling(dev, out);
	return 0;
}

static int CheckEncAnyIrq(hantroenc_t *dev, CORE_WAIT_OUT *out)
{
	u32 i;
	int rdy = 0;
	u32 core_info, irq_status, job_id=0;
	u32 core_type = CORE_VCE;

	for (i = 0; i < total_subsys_num; i++) {
		if (!(dev[i].subsys_data.core_info.type_info &
		      (1 << core_type)))
			continue;

		core_info = ((i << 4) | core_type);
		if ((CheckEncIrq(dev, &core_info, &irq_status,
					  &job_id) == 1) && core_info == i) {
#ifdef SUPPORT_WATCHDOG
			/* if there is an interrupt(exclusive slice ready) from VCE,
			 * should stop the watchdog.
			 */
			if (irq_status & ASIC_STATUS_ALL & ~ASIC_STATUS_SLICE_READY)
				_watchdog_stop(&dev[i]);
#endif
			out->job_id[out->irq_num] = job_id;
			out->irq_status[out->irq_num] = irq_status;
			/*printk(KERN_DEBUG "irq_status of subsys %d job_id %d is:%x\n",
			 *i,job_id,irq_status);
			 */
			out->irq_num++;
			rdy = 1;
		}
	}

	return rdy;
}

static unsigned int WaitEncAnyReady(hantroenc_t *dev, CORE_WAIT_OUT *out)
{
	if (wait_event_interruptible(enc_wait_queue,
				     CheckEncAnyIrq(dev, out))) {
		gf_debug("ENC wait_event_interruptible interrupted\n");
		return -ERESTARTSYS;
	}

	return 0;
}

static int CheckCoreOccupation(hantroenc_t *dev, u8 core_type)
{
	int ret = 0;
	unsigned long flags = 0;

	core_type = (core_type == CORE_VCEJ ? CORE_VCE : core_type);

	spin_lock_irqsave(&owner_lock, flags);
	if (!dev->is_reserved[core_type]) {
		dev->is_reserved[core_type] = 1;
#ifndef MULTI_THR_TEST
		dev->pid[core_type] = current->pid;
#endif
		ret = 1;
		gf_debug("%s pid=%d\n", __func__, dev->pid[core_type]);
	}

	spin_unlock_irqrestore(&owner_lock, flags);

	return ret;
}

static int GetWorkableCore(hantroenc_t *dev, u32 *core_info, u32 *core_info_tmp)
{
	int ret = 0;
	u32 i = 0;
	u32 cores;
	u8 core_type = 0;
	static u32 reserved_job_id;
	unsigned long flags = 0;
	/*input core_info[32 bit]: mode[1bit](1:all 0:specified)+
	 *reserved feature[3bit](reserved)+reserved+core_type[8bit]

	 *output core_info[32 bit]: the reserved core info to user space and
	 *defined as below.
	 *mode[1bit](1:all 0:specified)+amount[3bit](reserved total core num)+
	 *reserved+subsys_mapping[8bit]
	 */
	cores = *core_info;
	core_type = (u8)(cores & 0xFF);

	gf_debug("%s: core_info=%x\n", __func__, *core_info);

	/* a valid free Core with specified core type */
	for (i = 0; i < total_subsys_num; i++) {
		if (dev[i].subsys_data.core_info.type_info & (1 << core_type)) {
			core_type = (core_type == CORE_VCEJ ? CORE_VCE : core_type);
			if (dev[i].is_valid && CheckCoreOccupation(&dev[i], core_type)) {
				*core_info_tmp |= (1 << i);
				spin_lock_irqsave(&owner_lock, flags);
				*core_info = (reserved_job_id << 16) | (*core_info_tmp & 0xFF);
				dev[i].job_id[core_type] = reserved_job_id;
				/*maintain job_id in 16 bits for core_info can
				 *only save job_id in high 16 bits
				 */
				reserved_job_id = (reserved_job_id + 1) & 0xFFFF;
				spin_unlock_irqrestore(&owner_lock, flags);
				core_info_tmp = 0;
				ret = 1;
#ifdef SUPPORT_WATCHDOG
				/* start watchdog due to usr has start hw
				 * but not notisfy kernel driver
				 */
				_watchdog_feed(&dev[i]);
#endif
				break;
			}
		}
	}

	gf_debug("*core_info = %x\n", *core_info);
	return ret;
}

static long ReserveEncoder(hantroenc_t *dev, u32 *core_info)
{
	u32 core_info_tmp = 0;
#ifdef MULTI_THR_TEST
	struct wait_list_node *wait_node;
	u32 start_id = 0;
#endif

	/*If HW resources are shared inter cores, just make sure only one is
	 *using the HW
	 */
	if (dev[0].subsys_data.cfg.resource_shared) {
		if (down_interruptible(&enc_core_sem))
			return -ERESTARTSYS;
	}

#ifdef MULTI_THR_TEST
	while (1) {
		start_id = request_wait_node(&wait_node, start_id);
		if (wait_node->sem_used == 1) {
			if (GetWorkableCore(dev, core_info, &core_info_tmp)) {
				down_interruptible(&wait_node->wait_sem);
				wait_node->sem_used = 0;
				wait_node->used_flag = 0;
				break;
			} else {
				start_id++;
			}
		} else {
			wait_node->wait_cond = *core_info;
			list_add_tail(&wait_node->wait_list, &reserve_header);
			down_interruptible(&wait_node->wait_sem);
			*core_info = wait_node->wait_cond;
			list_del(&wait_node->wait_list);
			wait_node->sem_used = 0;
			wait_node->used_flag = 0;
			break;
		}
	}
#else

	/* lock a core that has specified core id*/
	if (wait_event_interruptible(hw_queue,
				     GetWorkableCore(dev, core_info, &core_info_tmp) != 0))
		return -ERESTARTSYS;
#endif

	return 0;
}

static void ReleaseEncoder(hantroenc_t *dev, u32 *core_info)
{
	unsigned long flags = 0;
	u8 core_type = 0, subsys_idx = 0, unCheckPid = 0;

	unCheckPid = (u8)((*core_info) >> 31);
#ifdef MULTI_THR_TEST
	u32 release_ok = 0;
	struct list_head *node;
	struct wait_list_node *wait_node;
	u32 core_info_tmp = 0;
#endif
	subsys_idx = (u8)((*core_info & 0xF0) >> 4);
	core_type = (u8)(*core_info & 0x0F);

	gf_debug("%s:subsys_idx=%d,core_type=%x\n", __func__, subsys_idx,
	       core_type);
	/* release specified subsys and core type */

	if (dev[subsys_idx].subsys_data.core_info.type_info &
	    (1 << core_type)) {
		core_type =
			(core_type == CORE_VCEJ ? CORE_VCE : core_type);
		spin_lock_irqsave(&owner_lock, flags);
		gf_debug("subsys[%d].pid[%d]=%d,current->pid=%d\n", subsys_idx,
		       core_type, dev[subsys_idx].pid[core_type], current->pid);
#ifdef MULTI_THR_TEST
		if (dev[subsys_idx].is_reserved[core_type])
#else
		if (dev[subsys_idx].is_reserved[core_type] &&
		    (dev[subsys_idx].pid[core_type] == current->pid ||
		     unCheckPid == 1))
#endif
		{
			dev[subsys_idx].pid[core_type] = -1;
			dev[subsys_idx].is_reserved[core_type] = 0;
			dev[subsys_idx].irq_received[core_type] = 0;
			dev[subsys_idx].irq_status[core_type] = 0;
			dev[subsys_idx].job_id[core_type] = 0;
#ifdef LOW_LATENCY_SLICEINFO_SUPPORT
			dev[subsys_idx].poll_sliceinfo_timeout_irq_status[core_type] = 0;
#endif
#ifdef SUPPORT_UFBC
			dev[subsys_idx].ufbc_dec_err_irq_status[core_type] = 0;
#endif
			spin_unlock_irqrestore(&owner_lock, flags);
#ifdef MULTI_THR_TEST
			release_ok = 0;
			if (list_empty(&reserve_header)) {
				request_wait_sema(&wait_node);
				up(&wait_node->wait_sem);
			} else {
				list_for_each(node, &reserve_header) {
					wait_node = container_of(node, struct wait_list_node, wait_list);
					if ((GetWorkableCore(dev, &wait_node->wait_cond, &core_info_tmp)) &&
					    wait_node->sem_used == 0) {
						release_ok = 1;
						wait_node->sem_used = 1;
						up(&wait_node->wait_sem);
						break;
					}
				}
				if (release_ok == 0) {
					request_wait_sema(&wait_node);
					up(&wait_node->wait_sem);
				}
			}
#endif

		} else {
			if (dev[subsys_idx].pid[core_type] != current->pid &&
			    unCheckPid == 0)
				gf_debug("WARNING:pid(%d) is trying to release core reserved by pid(%d)\n",
					current->pid, dev[subsys_idx].pid[core_type]);
			spin_unlock_irqrestore(&owner_lock, flags);
		}
		//wake_up_interruptible_all(&hw_queue);
	}
#ifndef MULTI_THR_TEST
	wake_up_interruptible_all(&hw_queue);
#endif
	if (dev->subsys_data.cfg.resource_shared)
		up(&enc_core_sem);
}

#ifdef IRQ_SIMULATION
static void get_random_bytes(void *buf, int nbytes);

static void hantroenc_trigger_irq_0(unsigned long value)
{
	gf_debug("trigger core 0 irq\n");
	del_timer(&timer0);
	hantroenc_isr(0, (void *)&hantroenc_data[0]);
}

static void hantroenc_trigger_irq_1(unsigned long value)
{
	gf_debug("trigger core 1 irq\n");
	del_timer(&timer1);
	hantroenc_isr(0, (void *)&hantroenc_data[1]);
}

#endif

int xx_HANTRO_IOCH_GET_VCMD_ENABLE_normal(struct drm_device *dev, void *data,
			struct drm_file *file_priv)
{
	*(unsigned long *)data = 0;
	return 0;
}

int xx_HANTRO_IOCH_GET_MMU_ENABLE_normal(struct drm_device *dev, void *data,
			struct drm_file *file_priv)
{
	*(unsigned int *)data = mmu_enable;
	return 0;
}

int xx_HANTRO_IOCG_HWOFFSET(struct drm_device *dev, void *data,
		       struct drm_file *file_priv)
{
	u32 id;

	id = *(u32 *)data;

	if (id >= total_subsys_num)
		return -EFAULT;
	*(unsigned long *)data = hantroenc_data[id].subsys_data.cfg.base_addr;

	return 0;
}

int xx_HANTRO_IOCG_HWIOSIZE(struct drm_device *dev, void *data,
		       struct drm_file *file_priv)
{
	u32 id;
	u32 io_size;

	id = *(u32 *)data;

	if (id >= total_subsys_num)
		return -EFAULT;
	io_size = hantroenc_data[id].subsys_data.cfg.iosize;
	*(u32 *)data = io_size;

	return 0;
}

int xx_HANTRO_IOCG_SRAMOFFSET(struct drm_device *dev, void *data,
		       struct drm_file *file_priv)
{
	*(unsigned long *)data = sram_base;
	linebuffer_sram_used = 1;
	return 0;
}

int xx_HANTRO_IOCG_SRAMEIOSIZE(struct drm_device *dev, void *data,
		       struct drm_file *file_priv)
{
	*(unsigned int __user *)data = sram_size;
	return 0;
}

int xx_HANTRO_IOCG_CORE_NUM(struct drm_device *dev, void *data,
		       struct drm_file *file_priv)
{
	*(unsigned int *)data = total_subsys_num;
	return 0;
}

int xx_HANTRO_IOCG_CORE_INFO(struct drm_device *dev, void *data,
		       struct drm_file *file_priv)
{
	u32 idx;
	SUBSYS_CORE_INFO in_data;

	gf_memcpy(&in_data, (void *)data, sizeof(SUBSYS_CORE_INFO));

	idx = in_data.type_info;
	if (idx > total_subsys_num - 1)
		return -1;

	gf_memcpy((void *)data, &hantroenc_data[idx].subsys_data.core_info, sizeof(SUBSYS_CORE_INFO));
	return 0;
}

int xx_HANTRO_IOCH_ENC_RESERVE(struct drm_device *dev, void *data,
		       struct drm_file *file_priv)
{
	u32 core_info;
	int ret;

	gf_debug("Reserve ENC Cores\n");
	core_info = *(u32 *)data;
	ret = ReserveEncoder(hantroenc_data, &core_info);
	if (ret == 0)
		*(u32 *)data = core_info;
	return ret;
}

int xx_HANTRO_IOCH_ENC_RELEASE(struct drm_device *dev, void *data,
		       struct drm_file *file_priv)
{
	u32 core_info;

	core_info = *(u32 *)data;

	gf_debug("Release ENC Core\n");

	ReleaseEncoder(hantroenc_data, &core_info);

	return 0;
}

int xx_HANTRO_IOCG_CORE_WAIT(struct drm_device *dev, void *data,
		       struct drm_file *file_priv)
{
	u32 core_info;
	u32 irq_status;
	unsigned int tmp = 0;

	core_info = *(u32 *)data;
#ifdef IRQ_SIMULATION
	u32 random_num;

	get_random_bytes(&random_num, sizeof(u32));
	random_num = random_num % 10 + 80;
	gf_debug("random_num=%d\n", random_num);

	/*init a timer to trigger irq*/
	if (core_info == 1) {
		init_timer(&timer0);
		timer0.function = &hantroenc_trigger_irq_0;
		//the expires time is 1s
		timer0.expires = jiffies + random_num * HZ / 10;
		add_timer(&timer0);
	}

	if (core_info == 2) {
		init_timer(&timer1);
		timer1.function = &hantroenc_trigger_irq_1;
		//the expires time is 1s
		timer1.expires = jiffies + random_num * HZ / 10;
		add_timer(&timer1);
	}
#endif

	tmp = WaitEncReady(hantroenc_data, &core_info, &irq_status);
	if (tmp == 0) {
		*(unsigned int *)data = irq_status;
		return core_info; //return core_id
	} else {
		return -1;
	}
}

int xx_HANTRO_IOCG_ANYCORE_WAIT_POLLING(struct drm_device *dev, void *data,
		       struct drm_file *file_priv)
{
	CORE_WAIT_OUT out;
	unsigned int tmp = 0;

	gf_memset(&out, 0, sizeof(CORE_WAIT_OUT));
#ifdef IRQ_SIMULATION
	u32 random_num;

	get_random_bytes(&random_num, sizeof(u32));
	random_num = random_num % 10 + 80;
	gf_debug("random_num=%d\n", random_num);

	/*init a timer to trigger irq*/
	if (core_info == 1) {
		init_timer(&timer0);
		timer0.function = &hantroenc_trigger_irq_0;
		//the expires time is 1s
		timer0.expires = jiffies + random_num * HZ / 10;
		add_timer(&timer0);
	}

	if (core_info == 2) {
		init_timer(&timer1);
		timer1.function = &hantroenc_trigger_irq_1;
		//the expires time is 1s
		timer1.expires = jiffies + random_num * HZ / 10;
		add_timer(&timer1);
	}
#endif

	tmp = WaitEncAnyReadyByPolling(hantroenc_data, &out);
	if (tmp == 0) {
		gf_memcpy((void *)data, &out, sizeof(CORE_WAIT_OUT));
		return 0;
	} else
		return -1;
}

int xx_HANTRO_IOCG_ANYCORE_WAIT(struct drm_device *dev, void *data,
		       struct drm_file *file_priv)
{
	CORE_WAIT_OUT out;
	unsigned int tmp = 0;

	gf_memset(&out, 0, sizeof(CORE_WAIT_OUT));

	tmp = WaitEncAnyReady(hantroenc_data, &out);
	if (tmp == 0) {
		gf_memcpy(data, &out, sizeof(CORE_WAIT_OUT));
		return 0;
	} else
		return -1;
}

int hantroenc_open(struct lisuan_device *gf_device, struct drm_file *filp)
{
	int result = 0;
	hantroenc_t *dev = hantroenc_data;

	gf_device->enc_dev = (void *)dev;

	gf_debug("dev opened\n");
	return result;
}

int hantroenc_release(struct lisuan_device *gf_device, struct drm_file *filp)
{
	hantroenc_t *dev = (hantroenc_t *)gf_device->enc_dev;
	u32 core_id = 0, i = 0;

#ifdef hantroenc_DEBUG
	dump_regs((unsigned long)dev); /* dump the regs */
#endif
	unsigned long flags = 0;

	gf_debug("dev closed\n");

	for (i = 0; i < total_subsys_num; i++) {
		for (core_id = 0; core_id < CORE_MAX; core_id++) {
			spin_lock_irqsave(&owner_lock, flags);
			if (dev[i].is_reserved[core_id] == 1 &&
			    dev[i].pid[core_id] == current->pid) {
				dev[i].pid[core_id] = -1;
				dev[i].is_reserved[core_id] = 0;
				dev[i].irq_received[core_id] = 0;
				dev[i].irq_status[core_id] = 0;
#ifdef LOW_LATENCY_SLICEINFO_SUPPORT
				dev[i].poll_sliceinfo_timeout_irq_status[core_id] = 0;
#endif
#ifdef SUPPORT_UFBC
				dev[i].ufbc_dec_err_irq_status[core_id] = 0;
#endif
				gf_debug("release reserved core\n");
			}
			spin_unlock_irqrestore(&owner_lock, flags);
		}
	}

#ifdef HANTROMMU_SUPPORT
	for (i = 0; i < total_subsys_num; i++) {
		if (!(hantroenc_data[i].subsys_data.core_info.type_info &
		      (1 << CORE_MMU)))
			continue;

		MMURelease(filp, hantroenc_data[i].hwregs +
					 hantroenc_data[i].subsys_data.core_info.offset[CORE_MMU]);
		break;
	}
#endif

	wake_up_interruptible_all(&hw_queue);

	if (dev->subsys_data.cfg.resource_shared)
		up(&enc_core_sem);

	return 0;
}

static const struct vm_operations_struct hantroenc_vm_ops = {
#ifdef CONFIG_HAVE_IOREMAP_PROT
	.access = generic_access_phys
#endif
};

#ifdef PCIE_EN
/*------------------------------------------------------------------------------
 *Function name   : PcieInit
 *Description     : Initialize PCI Hw access
 *
 *Return type     : int
 *------------------------------------------------------------------------------
 */
static int PcieInit(void)
{
	int i = 0;

	gDev = pci_get_device(PCI_VENDOR_ID_HANTRO, PCI_DEVICE_ID_HANTRO_PCI, gDev);
	if (!gDev) {
		gf_error("Init: Hardware not found.\n");
		goto out;
	}

	if (pci_enable_device(gDev) < 0) {
		gf_error("Init: Device not enabled.\n");
		goto out;
	}

	gBaseHdwr = pci_resource_start(gDev, PCI_H2_BAR);
	if (gBaseHdwr < 0) {
		gf_debug("Init: Base Address not set.\n");
		goto out_pci_disable_device;
	}
	gf_debug("Base hw val 0x%llx\n", (unsigned long long)gBaseHdwr);

	gBaseLen = pci_resource_len(gDev, PCI_H2_BAR);
	gf_debug("Base hw len 0x%08x\n", (unsigned int)gBaseLen);

	for (i = 0; i < total_subsys_num; i++) {
		//the offset is based on which bus interface is chosen
		subsys_array[i].base_addr =
			gBaseHdwr + subsys_array[i].base_addr;
	}

	sram_base = gBaseHdwr + 0x000000; //axi0 interface
	sram_size = 0x80000;

	gBaseDDRHw = pci_resource_start(gDev, PCI_DDR_BAR);
	if (gBaseDDRHw == 0) {
		gf_debug("%s: Base Address not set.\n", __func__);
		goto out_pci_disable_device;
	}
	gf_debug("Base memory val 0x%llx\n", (unsigned long long)gBaseDDRHw);

	gBaseLen = pci_resource_len(gDev, PCI_DDR_BAR);
	gf_debug("Base memory len 0x%08x\n", (unsigned int)gBaseLen);

	return 0;

out_pci_disable_device:
	pci_disable_device(gDev);
out:
	return -1;
}
#endif

#ifdef DTB_SUPPORT
/* get base address and irq number from DTB */
static int get_of_property(void)
{
	struct resource res;
	int irq_num;
	struct device_node *np;
	int ret;

	np = of_find_compatible_node(NULL, NULL, VCE_DTB_NODE_NAME);

	ret = of_address_to_resource(np, 0, &res);
	if (ret) {
		gf_error("hantroenc: can't get base address from DTB. ret=%d,\n", ret);
		return -EINVAL;
	}
	gf_debug("hantroenc: get base address 0x%llx\n", res.start);
	subsys_array[0].base_addr = res.start;

	irq_num = of_irq_get_byname(np, VCE_DTB_IRQ_NAME);
	if (irq_num > 0) {
		gf_debug("hantroenc: get irq number %d\n", irq_num);
		core_array[0].irq = irq_num;
	}

	return 0;
}
#endif

static void ReleaseIO(void)
{
	u32 i;

	for (i = 0; i < total_subsys_num; i++) {
		if (hantroenc_data[i].is_valid == 0)
			continue;
		if (hantroenc_data[i].hwregs)
			hantroenc_data[i].hwregs = NULL;
	}
}

static int ReserveIO(struct lisuan_device *gf_device)
{
	u32 hwid;
	int i;
	u32 found_hw = 0, hw_cfg;
	u32 VCE_core_idx;

	for (i = 0; i < total_subsys_num; i++) {
		hantroenc_data[i].hwregs = gf_device->mmio_base + hantroenc_data[i].subsys_data.cfg.base_addr;

		/*read hwid and check validness and store it*/
		VCE_core_idx = GET_ENCODER_IDX(
			hantroenc_data[0].subsys_data.core_info.type_info);
		if (!(hantroenc_data[i].subsys_data.core_info.type_info &
		      (1 << CORE_VCE)))
			VCE_core_idx = CORE_CUTREE;
		hwid = (u32)gf_read_mmio32(
			hantroenc_data[i].hwregs +
			hantroenc_data[i].subsys_data.core_info.offset[VCE_core_idx]);
		gf_debug("hwid=0x%08x\n", hwid);

		/* check for encoder HW ID */
		if (((((hwid >> 16) & 0xFFFF) !=
			  ((ENC_HW_ID1 >> 16) & 0xFFFF))) &&
			  ((((hwid >> 16) & 0xFFFF) !=
			  ((ENC_HW_ID2 >> 16) & 0xFFFF))) &&
			  (hwid != hantroenc_data[i].subsys_data.cfg.asic_id)) {
			gf_debug("hantroenc: HW not found at %p\n",
				(void *)hantroenc_data[i]
					.subsys_data.cfg.base_addr);
#ifdef hantroenc_DEBUG
			dump_regs((unsigned long)&hantroenc_data);
#endif
			ReleaseIO();
			hantroenc_data[i].is_valid = 0;
			continue;
		}
		hantroenc_data[i].hw_id = hwid;
		hantroenc_data[i].is_valid = 1;
		found_hw = 1;

		hw_cfg = (u32)gf_read_mmio32(
			hantroenc_data[i].hwregs +
			hantroenc_data[i].subsys_data.core_info.offset[VCE_core_idx] + 320);
		hantroenc_data[i].subsys_data.core_info.type_info &= 0xFFFFFFFC;
		if (hw_cfg & 0x88000000)
			hantroenc_data[i].subsys_data.core_info.type_info |= (1 << CORE_VCE);
		if (hw_cfg & 0x00008000)
			hantroenc_data[i].subsys_data.core_info.type_info |= (1 << CORE_VCEJ);

		gf_debug("hantroenc: HW at base <%p> with ID <0x%08x>\n",
			(void *)hantroenc_data[i].subsys_data.cfg.base_addr, hwid);
	}

	if (found_hw == 0) {
		gf_error("hantroenc: NO ANY HW found!!\n");
		return -1;
	}

	return 0;
}

int hantroenc_normal_init(struct lisuan_device *gf_device)
{
	int result = 0;
	int i, j;

#ifdef DTB_SUPPORT
	result = get_of_property();
	if (result)
		goto err1;
#endif

#ifdef HANTROMMU_SUPPORT
	enum MMUStatus mmu_status = MMU_STATUS_FALSE;
#endif

	total_subsys_num = sizeof(subsys_array) / sizeof(SUBSYS_CONFIG);

#ifdef PCIE_EN
	result = PcieInit();
	if (result)
		goto err1;
#endif

	for (i = 0; i < total_subsys_num; i++) {
		gf_debug("hantroenc: module init - subsys[%d] addr =%p\n", i,
			(void *)subsys_array[i].base_addr);
	}

	hantroenc_data = gf_kvcalloc(1, sizeof(hantroenc_t) * total_subsys_num, GFP_KERNEL);
	if (!hantroenc_data)
		goto err1;
	gf_memset(hantroenc_data, 0, sizeof(hantroenc_t) * total_subsys_num);

	for (i = 0; i < total_subsys_num; i++) {
		hantroenc_data[i].subsys_data.cfg = subsys_array[i];
		hantroenc_data[i].async_queue = NULL;
		hantroenc_data[i].hwregs = NULL;
		hantroenc_data[i].subsys_id = i;
		for (j = 0; j < CORE_MAX; j++)
			hantroenc_data[i].subsys_data.core_info.irq[j] = -1;
	}

	total_core_num = sizeof(core_array) / sizeof(CORE_CONFIG);
	for (i = 0; i < total_core_num; i++) {
		hantroenc_data[core_array[i].subsys_idx].subsys_data.core_info.type_info |= (1 << (core_array[i].core_type));
		hantroenc_data[core_array[i].subsys_idx].subsys_data.core_info.offset[core_array[i].core_type] = core_array[i].offset;
		hantroenc_data[core_array[i].subsys_idx].subsys_data.core_info.regSize[core_array[i].core_type] = core_array[i].reg_size;
		hantroenc_data[core_array[i].subsys_idx].subsys_data.core_info.irq[core_array[i].core_type] = core_array[i].irq;
	}

	result = ReserveIO(gf_device);
	if (result < 0)
		goto err;

	//ResetAsic(hantroenc_data);  /* reset hardware */

	sema_init(&enc_core_sem, 1);

#ifdef HANTROMMU_SUPPORT
	/* MMU only initial once No matter how many MMU we have */
	for (i = 0; i < total_subsys_num; i++) {
		if (!(hantroenc_data[i].subsys_data.core_info.type_info & (1 << CORE_MMU)))
			continue;

		result = MMUInit(
			hantroenc_data[i].hwregs +
			hantroenc_data[i].subsys_data.core_info.offset[CORE_MMU]);
		if (result == MMU_STATUS_NOT_FOUND)
			gf_debug("MMU does not exist!\n");
		else if (result != MMU_STATUS_OK) {
			ReleaseIO();
			goto err;
		}

		break;
	}

	gf_memset(mmu_hwregs, 0, MAX_SUBSYS_NUM * 2);
	for (i = 0; i < total_subsys_num; i++) {
		if (hantroenc_data[i].subsys_data.core_info.type_info &
		    (1 << CORE_MMU))
			mmu_hwregs[i][0] =
				hantroenc_data[i].hwregs +
				hantroenc_data[i].subsys_data.core_info.offset[CORE_MMU];

		if (hantroenc_data[i].subsys_data.core_info.type_info &
		    (1 << CORE_MMU_1))
			mmu_hwregs[i][1] = hantroenc_data[i].hwregs +
					   hantroenc_data[i].subsys_data.core_info.offset[CORE_MMU_1];
	}

	mmu_status = MMUEnable(mmu_hwregs);
	gf_debug("mmu_enable is %d\n", mmu_enable);
	if (mmu_status != MMU_STATUS_OK) {
		gf_debug("MMUEnable: MMU enable failed\n");
		return -3;
	}
#endif

#ifdef MULTI_THR_TEST
	init_reserve_wait(total_subsys_num);
#endif

	/* create kthread for normal driver, but it in sleep status, should wake up it when needed */
	hantroenc_kthread_create();
	gf_debug("hantroenc: module inserted. Major <%d>\n", hantroenc_major);

	return 0;

err:
err1:
	if (hantroenc_data)
		gf_kvfree(hantroenc_data);
	gf_debug("hantroenc: module not inserted\n");
	return result;
}

void hantroenc_normal_cleanup(struct lisuan_device *gf_device)
{
	int i = 0, j = 0;

	hantroenc_kthread_stop();

	for (i = 0; i < total_subsys_num; i++) {
		if (hantroenc_data[i].is_valid == 0)
			continue;
		//writel(0, hantroenc_data[i].hwregs + 0x14); /* disable HW */
		//writel(0, hantroenc_data[i].hwregs + 0x04); /* clear enc IRQ */
	}

#ifdef HANTROMMU_SUPPORT
	MMUCleanup(mmu_hwregs);
#endif

	ReleaseIO();
	gf_kvfree(hantroenc_data);

	gf_debug("hantroenc: module removed\n");
}

int hantroenc_isr(void *vdev)
{
	unsigned int handled = 0;
	hantroenc_t *dev = (hantroenc_t *)vdev;
	u32 irq_status;
	unsigned long flags = 0;
	u32 core_type = 0, i = 0;
	unsigned long reg_offset = 0;
	u32 hwId, majorId, wClr, minorId, productId;

	/*get core id by irq from subsys config*/
	core_type = CORE_VCE;
	reg_offset = dev->subsys_data.core_info.offset[CORE_VCE];

	/*If core is not reserved by any user, but irq is received, just clean it*/
	spin_lock_irqsave(&owner_lock, flags);
	if (!dev->is_reserved[core_type]) {
		gf_debug(
			"hantroenc_isr:received IRQ but core is not reserved!\n");
		irq_status = (u32)gf_read_mmio32(
			(dev->hwregs + reg_offset + 0x04));
		if (irq_status & 0x01) {
#ifdef SUPPORT_WATCHDOG
			/* if there is an interrupt(exclusive slice ready) from VCE,
			 * should stop the watchdog.
			 */
			if (irq_status & ASIC_STATUS_ALL & ~ASIC_STATUS_SLICE_READY)
				_watchdog_stop(dev);
#endif
			/*  Disable HW when buffer over-flow happen
			 *  HW behavior changed in over-flow
			 *    in-pass, HW cleanup HWIF_ENC_E auto
			 *    new version:  ask SW cleanup HWIF_ENC_E when buffer over-flow
			 */
			if (irq_status & 0x20)
				gf_write_mmio32((dev->hwregs + reg_offset + 0x14), 0);

			/* clear all IRQ bits. (hwId >= 0x80006100) means IRQ is cleared by writing 1 */
			hwId = gf_read_mmio32(dev->hwregs + reg_offset);
			productId = (hwId & 0xFFFF0000) >> 16;
			majorId = (hwId & 0x0000FF00) >> 8;
			minorId = (hwId & 0x000000FF);
			wClr = ((majorId >= 0x61) || (productId == 0x9000) || (productId == 0x9010) ||
			((productId == 0x8000) && (minorId >= 1))) ? irq_status : (irq_status & (~0x1FD));
			gf_write_mmio32((dev->hwregs + reg_offset + 0x04), wClr);
		}
		spin_unlock_irqrestore(&owner_lock, flags);
		return IRQ_HANDLED;
	}
	spin_unlock_irqrestore(&owner_lock, flags);

	gf_debug("hantroenc_isr:received IRQ!\n");
	irq_status = (u32)gf_read_mmio32((dev->hwregs + reg_offset + 0x04));
	gf_debug("irq_status of subsys %d core %d is:%x\n", dev->subsys_id,
		 core_type, irq_status);
	if (irq_status & 0x01) {
#ifdef SUPPORT_WATCHDOG
		/* if there is an interrupt(exclusive slice ready) from VCE,
		 * should stop the watchdog.
		 */
		if (irq_status & ASIC_STATUS_ALL & ~ASIC_STATUS_SLICE_READY)
			_watchdog_stop(dev);
#endif
		/*  Disable HW when buffer over-flow happen
		 *  HW behavior changed in over-flow
		 *    in-pass, HW cleanup HWIF_ENC_E auto
		 *    new version:  ask SW cleanup HWIF_ENC_E when buffer over-flow
		 */
		if (irq_status & 0x20)
			gf_write_mmio32((dev->hwregs + reg_offset + 0x14), 0);

		/* clear all IRQ bits. (hwId >= 0x80006100) means IRQ is cleared by writing 1 */
		hwId = gf_read_mmio32(dev->hwregs + reg_offset);
		productId = (hwId & 0xFFFF0000) >> 16;
		majorId = (hwId & 0x0000FF00) >> 8;
		minorId = (hwId & 0x000000FF);
		wClr = ((majorId >= 0x61) || (productId == 0x9000) || (productId == 0x9010) ||
		((productId == 0x8000) && (minorId >= 1))) ? irq_status : (irq_status & (~0x1FD));
		gf_write_mmio32((dev->hwregs + reg_offset + 0x04), wClr);

		spin_lock_irqsave(&owner_lock, flags);
		dev->irq_received[core_type] = 1;
		dev->irq_status[core_type] = irq_status & (~0x01);
		spin_unlock_irqrestore(&owner_lock, flags);

#ifdef LOW_LATENCY_SLICEINFO_SUPPORT
		u32 poll_sliceinfo_irq_status = 0;
		u32 hw_reset_irq = 0;

		gf_debug("hantroenc_isr:received polling sliceinfo timeout IRQ!\n");
		/* swreg118 bit 0 for polling sliceinfo timeout IRQ status */
		poll_sliceinfo_irq_status =
			 (u32)gf_read_mmio32((dev->hwregs + reg_offset + 0x1d8));
		gf_debug("poll_sliceinfo_irq_status of subsys %d core %d is:%x\n",
			    dev->subsys_id, core_type, poll_sliceinfo_irq_status);
		if (poll_sliceinfo_irq_status & 0x1) {
			/* clear irq bit by writing 1 */
			wClr = poll_sliceinfo_irq_status;
			gf_write_mmio32((dev->hwregs + reg_offset + 0x1d8), wClr);

			spin_lock_irqsave(&owner_lock, flags);
			dev->irq_received[core_type] = 1;
			dev->poll_sliceinfo_timeout_irq_status[core_type] =
				(poll_sliceinfo_irq_status & 0x01) << 13;
			spin_unlock_irqrestore(&owner_lock, flags);
			handled++;
		}
#endif

#ifdef SUPPORT_UFBC
		u32 ufbc_dec_irq_status = 0;

		gf_debug("hantroenc_isr:received ufbc decode error IRQ!\n");
		/* swreg118 bit 0 for polling sliceinfo timeout IRQ status */
		ufbc_dec_irq_status =
			 (u32)gf_read_mmio32((dev->hwregs + reg_offset + 0xc20));
		gf_debug("poll_sliceinfo_irq_status of subsys %d core %d is:%x\n",
			    dev->subsys_id, core_type, ufbc_dec_irq_status);
		if (ufbc_dec_irq_status & 0x1) {
			/* clear irq bit by writing 1 */
			wClr = ufbc_dec_irq_status;
			gf_write_mmio32((dev->hwregs + reg_offset + 0xc20), wClr);

			spin_lock_irqsave(&owner_lock, flags);
			dev->ufbc_dec_err_irq_status[core_type] =
				(ufbc_dec_irq_status & 0x01) << 14;
			spin_unlock_irqrestore(&owner_lock, flags);
			handled++;
		}
#endif

		wake_up_interruptible_all(&enc_wait_queue);
		handled++;
	}
	if (!handled) {
		gf_debug("IRQ received, but not hantro's!\n");
	}
	return IRQ_HANDLED;
}

#ifdef hantroenc_DEBUG
static void ResetAsic(hantroenc_t *dev)
{
	int i, n;

	for (n = 0; n < total_subsys_num; n++) {
		if (dev[n].is_valid == 0)
			continue;
		gf_write_mmio32((void *)(dev[n].hwregs + 0x14), 0);
		for (i = 4; i < dev[n].subsys_data.cfg.iosize; i += 4)
			gf_write_mmio32((void *)(dev[n].hwregs + i), 0);
	}
}

static void dump_regs(unsigned long data)
{
	hantroenc_t *dev = (hantroenc_t *)data;
	int i;

	gf_debug("Reg Dump Start\n");
	for (i = 0; i < dev->iosize; i += 4)
		gf_debug("\toffset %02X = %08X\n", i, gf_read_mmio32(dev->hwregs + i));
	gf_debug("Reg Dump End\n");
}
#endif
