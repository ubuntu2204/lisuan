/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#ifndef __DISP_PLL_H__
#define __DISP_PLL_H__

#include "display.h"

typedef struct _disp_pll_table_t
{
    //disp_u32_t pixel_clk_khz;
    disp_u32_t pllout_hz;

    disp_u8_t  sel_div;
    disp_u32_t divq;
    disp_u32_t divr;
    disp_u32_t range;
    disp_u32_t divfi;
    disp_u32_t divff;
} disp_pll_table_t;

typedef struct _disp_pll_para_t
{
    // sw decide pll policy
    disp_u8_t  use_new_div;
    disp_u8_t  dsc_en;
    disp_u8_t  dsc_slice;

    // pll decide cr reg
    disp_u8_t  sel_div;

    // input parameter
    disp_u32_t pixel_clk_khz;

    // pll common reg
    disp_u32_t divq;
    disp_u32_t divr;
    disp_u32_t range;
    disp_u32_t divfi;
    disp_u32_t divff;

    union {
        struct {
            disp_u32_t dvo              : 1;
            disp_u32_t reserved         :31;
        } bits;
        disp_u32_t val;
    } flags;
} disp_pll_para_t, *pdisp_pll_para_t;

disp_void_t disp_pll_on(pdisp_void_t pdisp_crtc);
disp_void_t disp_pll_off(pdisp_void_t pdisp_crtc);

#endif
