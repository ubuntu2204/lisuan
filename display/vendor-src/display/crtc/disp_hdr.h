/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#ifndef __DISP_HDR_H__
#define __DISP_HDR_H__

#include "display.h"

disp_u8_t disp_hdr_check(pdisp_void_t pplane, pdisp_hdr_module_t pnew, disp_u8_t hdr_module_valid);
DISP_STATUS disp_hdr_flush(pdisp_void_t pplane);
disp_void_t disp_hdr_module_dump(pdisp_hdr_module_t phdr_info);
#endif
