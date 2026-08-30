/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#include "disp_dvo_encoder.h"
#include "disp_device.h"
#include "disp_pll.h"
#include "disp_reg.h"
#include "SR_SHARE_register.h"

#define disp_print(level, log, ...) disp_print_ex(DISP_PRINT_MASK_ENCODER_DVO, level, log, ##__VA_ARGS__)

static DISP_STATUS dvo_encoder_hw_init(pdisp_encoder_t pencoder)
{
    pdisp_device_t pdd = pencoder->disp_device;

    /* PAD MUX Setting */
    disp_write_u32(pdd, 0x250044, 0x0000);
    disp_write_u32(pdd, 0x250050, 0x0000);
    disp_write_u32(pdd, 0x250054, 0x0000);
    disp_write_u32(pdd, 0x250098, 0x2000);
    disp_write_u32(pdd, 0x25009c, 0x2000);
    disp_write_u32(pdd, 0x2500a8, 0x2000);
    disp_write_u32(pdd, 0x2500ac, 0x2000);
    disp_write_u32(pdd, 0x2500b0, 0x2000);
    disp_write_u32(pdd, 0x2500b4, 0x2000);
    disp_write_u32(pdd, 0x2500b8, 0x2000);
    disp_write_u32(pdd, 0x2500bc, 0x2000);
    disp_write_u32(pdd, 0x2500c0, 0x2000);
    disp_write_u32(pdd, 0x2500c4, 0x2000);
    disp_write_u32(pdd, 0x2500c8, 0x2000);
    disp_write_u32(pdd, 0x2500cc, 0x2000);
    disp_write_u32(pdd, 0x2500d0, 0x2000);
    disp_write_u32(pdd, 0x2500d4, 0x2000);
    disp_write_u32(pdd, 0x2500d8, 0x2000);
    disp_write_u32(pdd, 0x2500dc, 0x2000);
    disp_write_u32(pdd, 0x2500e0, 0x2000);
    disp_write_u32(pdd, 0x2500e4, 0x2000);
    disp_write_u32(pdd, 0x2500e8, 0x2000);
    disp_write_u32(pdd, 0x2500ec, 0x2000);
    disp_write_u32(pdd, 0x2500f0, 0x2000);
    disp_write_u32(pdd, 0x2500f4, 0x0000);
    disp_write_u32(pdd, 0x2500f8, 0x0000);
    disp_write_u32(pdd, 0x250010, 0x2000);
    disp_write_u32(pdd, 0x250014, 0x2000);

    return DISP_OK;
}

static DISP_STATUS dvo_encoder_mode_set(pdisp_encoder_t pencoder)
{
    pdisp_device_t pdd = pencoder->disp_device;
    pdisp_crtc_t pcrtc = &pdd->disp_crtc[pencoder->index];
    disp_display_mode_t hw_mode;

    crtc_get_property_hw_mode(&pcrtc->property, &hw_mode);

    if (hw_mode.vdisplay > 720)
    {
        disp_print(DDM_WARNING, "dvo_encoder_mode_set index[%u], dvo mode more than 720p! wh[%u %u] clk_khz[%d]\n",
            pencoder->index, hw_mode.hdisplay, hw_mode.vdisplay, hw_mode.clock);
    }

    return DISP_OK;
}

static DISP_STATUS dvo_encoder_dpms(pdisp_encoder_t pencoder, DISP_POWER_STATE state)
{
    pdisp_device_t pdd = pencoder->disp_device;
    pdisp_crtc_t pcrtc = &pdd->disp_crtc[pencoder->index];

    if (DISP_POWER_ON == state)
    {
        disp_pll_on(pcrtc);
        sr_share_write_field(Reg_Sr_0a, Register_Sr_0a_Start, 3,
            Dvo_En, 1,
            Dvo_Src_Sel, 0, // hex pipe_a
            Dvo_Clk_Inv, 1);
    }
    else // off
    {
        sr_share_write_field(Reg_Sr_0a, Register_Sr_0a_Start, 1,
            Dvo_En, 0);
        disp_pll_off(pcrtc);
    }

    return DISP_OK;
}

struct disp_encoder_func dvo_encoder_func =
{
    .hw_init = dvo_encoder_hw_init,
    .hw_deinit = DISP_NULL, // nothing to do
    .mode_set = dvo_encoder_mode_set,
    .dpms = dvo_encoder_dpms,
};

disp_void_t disp_dvo_encoder_sw_init(pdisp_encoder_t pencoder)
{
    pencoder->func = &dvo_encoder_func;
}
