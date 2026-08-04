/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#ifndef _GF_KMS_VIRT_H_
#define _GF_KMS_VIRT_H_

#include "gf_kms.h"

int gf_kms_virt_init(struct lisuan_device *gdevice);
int gf_kms_virt_deinit(struct lisuan_device *gdevice);
void gf_kms_virt_vblank_init(struct display_adapter *adapter, int crtc_index);
int gf_kms_virt_vblank_enable(struct drm_crtc *crtc);
void gf_kms_virt_vblank_disable(struct drm_crtc *crtc);
bool gf_kms_virt_get_vblank_timestamp(struct drm_crtc *crtc, int *max_error, ktime_t *vblank_time, bool in_vblank_irq);
#endif /* _gf_kms_virt_H_ */