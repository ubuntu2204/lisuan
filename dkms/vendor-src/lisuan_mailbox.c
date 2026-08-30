/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */
#include <linux/kernel.h>
#include <linux/console.h>
#include <linux/kthread.h>

#include "lisuan_device.h"
#include "lisuan_mailbox.h"
#include "lisuan_ring.h"
#include "gf_ringbuffer.h"
#include "lisuan_scheduler.h"
#include "gf_hw_misc.h"
#include "gf_os_interface.h"

int lisuan_send_request(struct lisuan_device *ls_dev, unsigned int request_id)
{
	gf_send_request(ls_dev->mmio_base, request_id);
	return 0;
}

int lisuan_wait_chip_reset_finish(struct lisuan_device *ls_dev)
{
	int wait_time = 1000, retry = 5;
	while(!atomic_read(&ls_dev->pf_finish_reset) && retry > 0) {
		gf_msleep(5);		
		wait_time -= 5;
		if (wait_time <= 0) {
			if (--retry){
				mb_info("wait chip reset finish timeout, retry again:%d\n",retry);
			}
			wait_time = 1000;
		}
	}
	return 0;
}

#if 0
int lisuan_poll_request_ack(struct lisuan_device *ls_dev, unsigned ack)
{
	int wait_time = 1000, retry = 5;
	unsigned int request_id;

	if (ack != MAILBOX_CUSTOM_MSG_CHIP_RESET_NOTIFICATION_ACK)
		return 0;

	request_id = gf_read_mmio32(ls_dev->mmio_base + MMIO_MB_START_ADDRESS_BASE + Register_MB_DB_VF_MSG2_DATA_start);
	while (request_id != MAILBOX_CUSTOM_MSG_CHIP_RESET_NOTIFICATION_ACK && retry > 0) {
		//TODO need save request that not expect, and call pf_process_event
		gf_msleep(5);
		wait_time -= 5;
		while(request_id != 0x12345678){
			request_id = gf_read_mmio32(ls_dev->mmio_base + MMIO_MB_START_ADDRESS_BASE + Register_MB_DB_VF_MSG2_DATA_start);
			if(request_id == MAILBOX_CUSTOM_MSG_CHIP_RESET_FINISH_NOTIFICATION)
				break;
		}

		if (wait_time <= 0) {
			if (--retry)
				mb_info("polling ack:%d timeout, retry again\n", ack);
			wait_time = 1000;
		}
	}

	return 0;

}
#endif

int lisuan_send_event_ack(struct lisuan_device *ls_dev, unsigned ack)
{
	if (ack != MAILBOX_CUSTOM_MSG_CHIP_RESET_NOTIFICATION_ACK && ack != MAILBOX_CUSTOM_MSG_VF_RESET_NOTIFICATION_ACK)
		return 0;

	mb_info("send ack:%d\n", ack);
	gf_send_event_ack(ls_dev->mmio_base, ack);	//TODO add msg mmio offset
	return 0;
}

void lisuan_mailbox_broadcast_reset_request(struct lisuan_device *ls_dev, unsigned int request_id)
{
	if (!ls_dev->is_virtfn)
		return;

	switch (request_id){
	case MAILBOX_CUSTOM_MSG_CHIP_RESET_NOTIFICATION:
		if(atomic_cmpxchg(&ls_dev->pf_finish_reset, 1, 0) == 1)
		{
			lisuan_send_request(ls_dev, request_id);
			mb_info("broadcast reset req:%d\n",request_id);
		}
		lisuan_wait_chip_reset_finish(ls_dev);
		break;
	}
}

void lisuan_process_chip_reset_event(struct lisuan_device *ls_dev, unsigned int event_id)
{
	unsigned i = 0;
	struct lisuan_ring *sched;

	switch (event_id) {
	case MAILBOX_CUSTOM_MSG_CHIP_RESET_NOTIFICATION:
		{
			mb_info("pf reset start, event id:%d\n", event_id);

			if(atomic_cmpxchg(&ls_dev->checked_hang, 0, 1) == 0)
			{
				atomic_cmpxchg(&ls_dev->pf_finish_reset, 1, 0);
				atomic_set(&ls_dev->pf_req_reset, 1);
				for (i = 0; i < ls_dev->rb_count; i++)
				{
					sched = ls_dev->sched[i];
					lisuan_sched_stop(&sched->base, NULL);
				}
			}
			break;
		}
	case MAILBOX_CUSTOM_MSG_CHIP_RESET_FINISH_NOTIFICATION:
		{
			mb_info("pf reset finish, event id:%d\n", event_id);
			if(atomic_read(&ls_dev->pf_req_reset) == 1)
			{
				lisuan_sched_recover_gpu(ls_dev->sched[0]);

				for (i = 0; i < ls_dev->rb_count; i++) {
					sched = ls_dev->sched[i];

					if (!sched || !sched->base.thread)
						continue;

					lisuan_sched_resubmit_jobs(&sched->base);
					lisuan_sched_start(&sched->base, true);
					wake_up_interruptible(&sched->base.wake_up_worker);
				}
				atomic_set(&ls_dev->pf_req_reset, 0);
				atomic_set(&ls_dev->checked_hang, 0);
			}
			atomic_set(&ls_dev->pf_finish_reset, 1);
			break;
		}
	}
}

void lisuan_process_custom_msg_event(struct lisuan_device *ls_dev)
{
	unsigned int event_id = gf_get_event_id(ls_dev->mmio_base);

	switch (event_id) {
	case MAILBOX_CUSTOM_MSG_CHIP_RESET_NOTIFICATION:
	case MAILBOX_CUSTOM_MSG_CHIP_RESET_FINISH_NOTIFICATION:
		lisuan_process_chip_reset_event(ls_dev, event_id);
		break;
	default:
		mb_info("receive event:%d not handle\n", event_id);
		break;
	}
}

void lisuan_mailbox_worker(struct work_struct *work)
{
	struct lisuan_device *ls_dev = container_of(work, struct lisuan_device, mailbox_work);

	//process pf request
	lisuan_process_custom_msg_event(ls_dev);
}

void lisuan_mailbox_get_therm_worker(struct work_struct *work)
{
	struct lisuan_device *ls_dev = container_of(work, struct lisuan_device, therm_work);
	
	gf_get_hw_therm(ls_dev->mmio_base, NULL);
}