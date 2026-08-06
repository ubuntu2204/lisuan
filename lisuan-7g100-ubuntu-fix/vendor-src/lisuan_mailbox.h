/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#ifndef __LISUAN_MAILBOX_H__
#define __LISUAN_MAILBOX_H__

#include <linux/workqueue.h>
#include "gf_hw_misc.h"

void lisuan_mailbox_worker(struct work_struct *work);
void lisuan_mailbox_broadcast_reset_request(struct lisuan_device *ls_dev, unsigned int request_id);
void lisuan_mailbox_get_therm_worker(struct work_struct *work);
#endif
