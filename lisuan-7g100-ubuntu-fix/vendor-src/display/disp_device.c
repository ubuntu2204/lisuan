/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#include "disp_device.h"
#include "disp_dhi.h"
#include "disp_crtc.h"
#include "disp_hdmi_encoder.h"
#include "disp_dp_encoder.h"
#include "disp_hdmi_connector.h"
#include "disp_dp_connector.h"
#include "disp_callback.h"
#include "disp_reg.h"
#include "PLNS_register.h"

#define disp_print(level, log, ...) disp_print_ex(DISP_PRINT_MASK_DEVICE, level, log, ##__VA_ARGS__)

disp_void_t disp_sw_map_from_chip_define(pdisp_device_t pdisp_device)
{
    disp_u32_t idx = 0;

    /* Default 4 dp. If dvo enable, force pipe_a dvo. */

    for (idx = 0; idx < DISP_ENCODER_NUM; idx++)
    {
        pdisp_device->disp_encoder[idx].type = DISP_ENCODER_DP;
        pdisp_device->disp_connector[idx].type = DISP_CONNECTOR_DP;
    }

    if (pdisp_device->flag.info.dvo_enable)
    {
        pdisp_device->disp_encoder[0].type = DISP_ENCODER_DVO;
        pdisp_device->disp_connector[0].type = DISP_CONNECTOR_DVO;
    }

    /* mmio */
    pdisp_device->reg_off.sr[0] = SR_A_OFFSET;
    pdisp_device->reg_off.sr[1] = SR_B_OFFSET;
    pdisp_device->reg_off.sr[2] = SR_C_OFFSET;
    pdisp_device->reg_off.sr[3] = SR_D_OFFSET;
    pdisp_device->reg_off.cr[0] = CR_A_OFFSET;
    pdisp_device->reg_off.cr[1] = CR_B_OFFSET;
    pdisp_device->reg_off.cr[2] = CR_C_OFFSET;
    pdisp_device->reg_off.cr[3] = CR_D_OFFSET;
    pdisp_device->reg_off.plns[0] = PLNS_A_OFFSET;
    pdisp_device->reg_off.plns[1] = PLNS_B_OFFSET;
    pdisp_device->reg_off.plns[2] = PLNS_C_OFFSET;
    pdisp_device->reg_off.plns[3] = PLNS_D_OFFSET;
    pdisp_device->reg_off.pipe[0] = PIPE_A_OFFSET;
    pdisp_device->reg_off.pipe[1] = PIPE_B_OFFSET;
    pdisp_device->reg_off.pipe[2] = PIPE_C_OFFSET;
    pdisp_device->reg_off.pipe[3] = PIPE_D_OFFSET;
    pdisp_device->reg_off.dp[0] = DP_A_OFFSET;
    pdisp_device->reg_off.dp[1] = DP_B_OFFSET;
    pdisp_device->reg_off.dp[2] = DP_C_OFFSET;
    pdisp_device->reg_off.dp[3] = DP_D_OFFSET;
    pdisp_device->reg_off.pll[0] = PLLCTL_A_OFFSET;
    pdisp_device->reg_off.pll[1] = PLLCTL_B_OFFSET;
    pdisp_device->reg_off.pll[2] = PLLCTL_C_OFFSET;
    pdisp_device->reg_off.pll[3] = PLLCTL_D_OFFSET;
    pdisp_device->reg_off.ppa[0] = PPA_DP_EPHY_A_OFFSET;
    pdisp_device->reg_off.ppa[1] = PPA_DP_EPHY_B_OFFSET;
    pdisp_device->reg_off.ppa[2] = PPA_DP_EPHY_C_OFFSET;
    pdisp_device->reg_off.ppa[3] = PPA_DP_EPHY_D_OFFSET;
    pdisp_device->reg_off.sr_share = SR_SHARE_OFFSET;
    pdisp_device->reg_off.cr_share = CR_SHARE_OFFSET;
    pdisp_device->reg_off.dsc[0] = DSC_A_OFFSET;
    pdisp_device->reg_off.dsc[1] = DSC_B_OFFSET;
    pdisp_device->reg_off.dhi = DHI_OFFSET;
    pdisp_device->reg_off.hau = HAU_OFFSET;
    pdisp_device->reg_off.intr = INTR_OFFSET;
    pdisp_device->reg_off.pwm = PWM_OFFSET;
    pdisp_device->reg_off.layer_plns[0] = 0;
    pdisp_device->reg_off.layer_plns[1] = (Register_Plns_Mmio_1d_Start - Register_Plns_Mmio_10_Start) * 4;
    pdisp_device->reg_off.layer_hdr[0] = 0;
    pdisp_device->reg_off.layer_hdr[1] = (Register_Plns_Mmio_C1_Start - Register_Plns_Mmio_30_Start) * 4;
}

DISP_STATUS disp_device_sw_init(pdisp_device_t pdisp_device)
{
    pdisp_crtc_t pdisp_crtc = DISP_NULL;
    pdisp_encoder_t pdisp_encoder = DISP_NULL;
    pdisp_connector_t pdisp_connector = DISP_NULL;
    DISP_STATUS ret = -DISP_EPRIV_UNKNOWN;
    disp_u8_t i = 0;

    disp_sw_map_from_chip_define(pdisp_device);

    for(i =0; i < DISP_CRTC_NUM; i++)
    {
        pdisp_crtc = &pdisp_device->disp_crtc[i];
        pdisp_crtc->disp_device = (pdisp_void_t)pdisp_device;
        pdisp_crtc->index = i;

        ret = disp_crtc_sw_init(pdisp_crtc);
        if(ret)
            return ret;
    }

    for(i = 0; i < DISP_ENCODER_NUM; i++)
    {
        pdisp_encoder = &pdisp_device->disp_encoder[i];
        pdisp_encoder->disp_device = (pdisp_void_t)pdisp_device;
        pdisp_encoder->index = i;

        ret = disp_encoder_sw_init(pdisp_encoder);
        if(ret)
            return ret;
    }

    for(i = 0; i < DISP_CONNECTOR_NUM; i++)
    {
        pdisp_connector = &pdisp_device->disp_connector[i];
        pdisp_connector->disp_device = (pdisp_void_t)pdisp_device;
        pdisp_connector->index = i;

        ret = disp_connector_sw_init(pdisp_connector);
        if(ret)
            return ret;
    }

#ifndef __DISP_GOP__
    pdisp_device->pdisp_dsc = disp_malloc(sizeof(disp_dsc_t) * DISP_DSC_NUM);
    if (!pdisp_device->pdisp_dsc)
    {
        disp_print(DDM_ERROR, "disp_device_sw_init, alloc dsc failed\n");
        return DISP_ENOMEM;
    }
    disp_memset(pdisp_device->pdisp_dsc, 0, sizeof(disp_dsc_t) * DISP_DSC_NUM);
    for(i = 0; i < DISP_DSC_NUM; i++)
    {
        ret = disp_dsc_sw_init((pdisp_dsc_t)pdisp_device->pdisp_dsc + i,
            pdisp_device, (disp_u32_t)i, (disp_u32_t)i * 2);
        if(ret)
            return ret;
    }
#endif

    return DISP_OK;
}

DISP_STATUS disp_device_sw_deinit(pdisp_device_t pdisp_device)
{
    disp_s32_t ret = 0, index = 0;
    pdisp_connector_t pdisp_connector = DISP_NULL;

    for(index = 0; index < DISP_CONNECTOR_NUM; ++index)
    {
        pdisp_connector = &pdisp_device->disp_connector[index];
        ret = disp_connector_sw_deinit(pdisp_connector);
        if(ret)
            return ret;
    }

    for (index = 0; index < DISP_ENCODER_NUM; ++index)
    {
        disp_encoder_sw_deinit(&pdisp_device->disp_encoder[index]);
    }

#ifndef __DISP_GOP__
    for(index = 0; index < DISP_DSC_NUM; index++)
    {
        disp_dsc_sw_deinit((pdisp_dsc_t)pdisp_device->pdisp_dsc + index);
    }
    disp_free(pdisp_device->pdisp_dsc);
    pdisp_device->pdisp_dsc = DISP_NULL;
#endif

    return DISP_OK;
}

DISP_STATUS disp_device_hw_init(pdisp_device_t pdisp_device)
{
    pdisp_crtc_t pdisp_crtc = DISP_NULL;
    pdisp_encoder_t pdisp_encoder = DISP_NULL;
    disp_s32_t i = 0;
    DISP_STATUS ret = 0;

//chip hw init
//register unlock
    disp_register_unlock(pdisp_device, 1);

    disp_dhi_hw_init(pdisp_device);

//crtc hw init
    for(i = 0; i < DISP_CRTC_NUM; i++)
    {
        pdisp_crtc = &pdisp_device->disp_crtc[i];

        ret = disp_crtc_hw_init(pdisp_crtc);
        if(ret)
        {
            disp_print(DDM_ERROR, "crtc %d hw init failed with %d\n",
                i, ret);
            return ret;
        }      
    }

//encoder hw init
    // skip hw setting, if show on pipe
    if(!pdisp_device->flag.info.pz1_capture_pipe)
    {
        for(i = 0; i < DISP_ENCODER_NUM; i++)
        {
            pdisp_encoder = &pdisp_device->disp_encoder[i];

            ret = disp_encoder_hw_init(pdisp_encoder);
            if(ret)
                return ret;
        }
    }

//connector hw init is useless
//maybe need add connector hw init func for i2c and aux initialize

//codec hw init
    disp_codec_hw_init(pdisp_device);

    return ret;
}

DISP_STATUS disp_device_hw_deinit(pdisp_device_t pdisp_device)
{
    pdisp_crtc_t pdisp_crtc = DISP_NULL;
    pdisp_encoder_t pdisp_encoder = DISP_NULL;
    disp_s32_t i = 0;
    DISP_STATUS ret = 0;

//codec hw deinit
    disp_codec_hw_deinit(pdisp_device);

//connector hw init is useless
//encoder hw init
    if(!pdisp_device->flag.info.pz1_capture_pipe)
    {
        for(i = 0; i < DISP_ENCODER_NUM; i++)
        {
            pdisp_encoder = &pdisp_device->disp_encoder[i];
            ret = disp_encoder_hw_deinit(pdisp_encoder);
            if(ret)
                return ret;
        }
    }

//crtc hw deinit
    for(i = 0; i < DISP_CRTC_NUM; i++)
    {
        pdisp_crtc = &pdisp_device->disp_crtc[i];
        ret = disp_crtc_hw_deinit(pdisp_crtc);
        if(ret)
            return ret;
    }

    disp_dhi_hw_deinit(pdisp_device);

//register lock
    disp_register_unlock(pdisp_device, 0);

    return DISP_OK;
}

disp_u8_t disp_device_get_support_connector(pdisp_device_t pdisp_device)
{
    disp_u8_t i = 0;
    disp_u8_t support_connector = 0;

    for(i = 0; i < DISP_CONNECTOR_NUM; i++)
    {
        support_connector |= 1 << (pdisp_device->disp_connector[i].index);
    }

    return support_connector;
}
