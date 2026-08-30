/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#ifndef __DISP_HW_COMMON_H__
#define __DISP_HW_COMMON_H__

#include "display.h"

typedef struct _disp_fifo_para_t
{
    disp_u8_t fifo_select;
    disp_u8_t index;
    disp_u32_t *data;
    disp_u32_t size; // per 32 bits
    disp_u8_t order;
}disp_fifo_para_t, *pdisp_fifo_para_t;

disp_void_t disp_prepare_gamma_fifo(pdisp_fifo_para_t pfifo, pdisp_gamma_info_t gamma_info);
disp_void_t disp_prepare_eld_fifo(pdisp_fifo_para_t pfifo, pdisp_u8_t peld);
disp_void_t disp_write_fifo(pdisp_void_t pdisp_device, pdisp_fifo_para_t pfifo);
disp_void_t disp_read_fifo(pdisp_void_t pdisp_device, pdisp_fifo_para_t pfifo);

#endif
