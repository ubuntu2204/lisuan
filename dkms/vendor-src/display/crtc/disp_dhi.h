/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#ifndef __DISP_DHI_H__
#define __DISP_DHI_H__

#include "display.h"

disp_void_t disp_register_unlock(pdisp_void_t pdisp_deivce, disp_u8_t unlock);
disp_void_t disp_dhi_hw_init(pdisp_void_t pdisp_device);
disp_void_t disp_dhi_hw_deinit(pdisp_void_t pdisp_device);
disp_void_t disp_check_underflow(pdisp_void_t pdisp_device);

#endif
