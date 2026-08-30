/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#ifndef __GF_DEBUGFS_H__
#define __GF_DEBUGFS_H__

#include <linux/idr.h>
#include <drm/drm_file.h>

#include "gf_kms.h"

void connector_debugfs_init(struct connector_t *gconnector);

#if DRM_VERSION_CODE < KERNEL_VERSION(5,8,0)
int gf_debugfs_init(struct drm_minor *minor);
#else
void gf_debugfs_init(struct drm_minor *minor);
#endif

#endif
