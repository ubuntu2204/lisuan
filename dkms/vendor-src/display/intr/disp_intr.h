/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#ifndef __DISP_INTR_H__
#define __DISP_INTR_H__

#include "display.h"

disp_void_t disp_intr_enable(pdisp_void_t pdevice, disp_u8_t enable, disp_intr_t disp_intr);
disp_u32_t disp_intr_get_status(pdisp_void_t pdevice);
disp_u32_t disp_intr_multi_sync_enable(pdisp_void_t pdevice, disp_u8_t enable);
disp_void_t disp_intr_clear_vsync_status(pdisp_void_t pdevice, disp_s32_t crtc_index);
#endif
