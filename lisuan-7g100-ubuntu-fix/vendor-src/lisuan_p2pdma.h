/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#ifndef __LISUAN_P2PMDA_H__
#define __LISUAN_P2PMDA_H__

#include <linux/pci.h>
#include "lisuan_device.h"

int lisuan_pci_p2pdma_distance_many(struct pci_dev *provider, struct device **clients,
				   int num_clients, bool verbose);
bool lisuan_device_is_peer_accessible(struct lisuan_device *ls_dev,
				struct device *peer_adev);
#endif
