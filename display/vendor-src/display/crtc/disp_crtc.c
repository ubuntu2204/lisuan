/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#include "disp_crtc.h"
#include "disp_device.h"
#include "disp_plane.h"
#include "disp_reg.h"
#include "PLNS_register.h"
#include "SR_register.h"
#include "disp_dhi.h"
#include "PIPE_register.h"
#include "disp_dp_encoder.h"
#include "disp_intr.h"

#define disp_print(level, log, ...) disp_print_ex(DISP_PRINT_MASK_CRTC, level, log, ##__VA_ARGS__)

DISP_STATUS disp_crtc_sw_init(pdisp_crtc_t pdisp_crtc)
{
    pdisp_device_t pdd = DISP_NULL;
    pdisp_plane_t pdisp_plane = DISP_NULL;
    pdisp_pipe_t pdisp_pipe = DISP_NULL;
    DISP_STATUS ret = 0;

    if(!pdisp_crtc)
    {
        disp_print(DDM_WARNING, "pdisp_crtc is NULL\n");
        return -DISP_EINVAL;
    }

    pdd = pdisp_crtc->disp_device;

    if(pdd->device_id == DISP_DEVICE_G100)
    {
        pdisp_crtc->max_pixel_clock = 600000;//600M
    }
    else
    {
        disp_print(DDM_WARNING, "unknown device id\n");
        return -DISP_EPRIV_DEVICE_ID;
    }

    pdisp_plane = &pdisp_crtc->primary;
    pdisp_plane->crtc_index = pdisp_crtc->index;
    pdisp_plane->disp_device = (pdisp_void_t)pdd;
    pdisp_plane->plane_index = DISP_PRIMARY_PLANE;

    ret = disp_plane_sw_init(pdisp_plane);
    if(ret)
        return ret;

    pdisp_plane = &pdisp_crtc->cursor;
    pdisp_plane->crtc_index = pdisp_crtc->index;
    pdisp_plane->disp_device = (pdisp_void_t)pdd;
    pdisp_plane->plane_index = DISP_CURSOR_PLANE;

    ret = disp_plane_sw_init(pdisp_plane);
    if(ret)
        return ret;

    //here should loop DISP_OVERLAY_PLANE_NUM
    pdisp_plane = &pdisp_crtc->overlay_plane[0];
    pdisp_plane->crtc_index = pdisp_crtc->index;
    pdisp_plane->disp_device = (pdisp_void_t)pdd;
    pdisp_plane->plane_index = DISP_OVERLAY_PLANE;

    ret = disp_plane_sw_init(pdisp_plane);
    if(ret)
        return ret;

    pdisp_pipe = &pdisp_crtc->pipe;
    pdisp_pipe->crtc_index = (disp_u8_t)pdisp_crtc->index;
    pdisp_pipe->pdisp_device = (pdisp_void_t)pdd;
    
    return disp_pipe_sw_init(pdisp_pipe);
}

DISP_STATUS disp_crtc_hw_init(pdisp_crtc_t pdisp_crtc)
{
    pdisp_plane_t pdisp_plane = DISP_NULL;
    pdisp_pipe_t pdisp_pipe = DISP_NULL;
    DISP_STATUS ret = 0;

    if(!pdisp_crtc)
        return -DISP_EINVAL;

    pdisp_plane = &pdisp_crtc->primary;
    ret = disp_plane_hw_init(pdisp_plane);
    if(ret)
        return ret;

    pdisp_plane = &pdisp_crtc->cursor;
    ret = disp_plane_hw_init(pdisp_plane);
    if(ret)
        return ret;

    pdisp_plane = &pdisp_crtc->overlay_plane[0];
    ret = disp_plane_hw_init(pdisp_plane);
    if(ret)
        return ret;

    pdisp_pipe = &pdisp_crtc->pipe;
    return disp_pipe_hw_init(pdisp_pipe);
}

DISP_STATUS disp_crtc_hw_deinit(pdisp_crtc_t pdisp_crtc)
{
    pdisp_plane_t pdisp_plane = DISP_NULL;
    pdisp_pipe_t pdisp_pipe = DISP_NULL;
    DISP_STATUS ret = 0;

    if(!pdisp_crtc)
        return -DISP_EINVAL;

    pdisp_pipe = &pdisp_crtc->pipe;
    ret = disp_pipe_hw_deinit(pdisp_pipe);
    if(ret)
        return ret;

    pdisp_plane = &pdisp_crtc->primary;
    ret = disp_plane_hw_deinit(pdisp_plane);
    if(ret)
        return ret;

    pdisp_plane = &pdisp_crtc->cursor;
    ret = disp_plane_hw_deinit(pdisp_plane);
    if(ret)
        return ret;

    pdisp_plane = &pdisp_crtc->overlay_plane[0];
    ret = disp_plane_hw_deinit(pdisp_plane);
    if(ret)
        return ret;
    
    return ret;
}

static disp_u8_t disp_crtc_wait_vblank_over(pdisp_crtc_t pdisp_crtc)
{
    disp_u8_t flag = 0;
    disp_u8_t ret = DISP_FALSE;
    Reg_Plns_Mmio_2c plns_2c;
    pdisp_device_t pdd = (pdisp_device_t)pdisp_crtc->disp_device;
    disp_u32_t offset = pdd->reg_off.plns[pdisp_crtc->index] + Register_Plns_Mmio_2c_Start * 4;
    disp_u32_t max_times = 0x20000;

    /* Mmio read once:
     * Win10: about 0.5 us, 0x20000 tims, 65 ms is safe.
     * Linux: ??
     * GOP: ??
     */
    while (flag < 2 && max_times)
    {
        plns_2c.uint = disp_read_u32(pdisp_crtc->disp_device, offset);
        flag = (plns_2c.reg.Rback_Vblk && 0 == flag) ? 1 : flag;
        flag = (plns_2c.reg.Rback_Vde && flag) ? 2 : flag;
        max_times--;
    }

    ret = (flag < 2) ?  DISP_FALSE : DISP_TRUE;
    if (!ret)
    {
        disp_print(DDM_WARNING, "wait vblank timeout!\n");
    }
    return ret;
}

DISP_STATUS disp_crtc_dpms(pdisp_crtc_t pdisp_crtc, pdisp_crtc_dpms_t crtc_dpms)
{
    disp_sig_set_t sig_set;
    disp_intr_t intr;
    DISP_STATUS ret = DISP_OK;
    Reg_Sr_38 sr_38;
    Reg_Sr_32 sr_32;
    pdisp_device_t pdd = (pdisp_device_t)pdisp_crtc->disp_device;
    disp_u32_t off_sr_38 = pdd->reg_off.sr[pdisp_crtc->index] + Register_Sr_38_Start;
    disp_u32_t off_sr_32 = pdd->reg_off.sr[pdisp_crtc->index] + Register_Sr_32_Start;

    if(!pdisp_crtc)
        return -DISP_EINVAL;

    // Prepare for mmio write, we need write as soon as possible.
    sr_38.uchar = disp_read_u8(pdisp_crtc->disp_device, off_sr_38);
    sr_32.uchar = disp_read_u8(pdisp_crtc->disp_device, off_sr_32);
    sr_38.reg.Vga_Scrn_Off = crtc_dpms->enable ? 0 : 1;
    sr_38.reg.Vga_Std_Scrnoff = crtc_dpms->enable ? 0 : 1;
    sr_32.reg.Epl0_Rst = crtc_dpms->enable ? 0 : 1;
    sr_32.reg.Epl1_Rst = crtc_dpms->enable ? 0 : 1;

    /* G100 A0 patch: screen off/on epl underflow when compress on. */
    if (!crtc_dpms->enable)
    {
        disp_write_u8(pdisp_crtc->disp_device, off_sr_32, sr_32.uchar);
        //disp_crtc_wait_vblank_over(pdisp_crtc);
        disp_write_u8(pdisp_crtc->disp_device, off_sr_38, sr_38.uchar);
    }
    else
    {
        disp_write_u8(pdisp_crtc->disp_device, off_sr_38, sr_38.uchar);
        disp_write_u8(pdisp_crtc->disp_device, off_sr_32, sr_32.uchar);
    }

    crtc_set_property_dpms(&pdisp_crtc->property, crtc_dpms->enable);

    if (crtc_dpms->sgnt_global_enable) {
        sig_set.crtc_index = pdisp_crtc->index;
        sig_set.benable = crtc_dpms->enable;
        sig_set.isr.x = 0;
        sig_set.isr.y = 0;
        sig_set.isr.w = pdisp_crtc->property.hw_mode.hdisplay;
        sig_set.isr.h = pdisp_crtc->property.hw_mode.vdisplay;
        ret = disp_pipe_set_sig(&pdisp_crtc->pipe, &sig_set);

        intr.intr = 0;
        intr.info.Pipea_Sgnt = 1;
        intr.info.Pipeb_Sgnt = 1;
        intr.info.Pipec_Sgnt = 1;
        intr.info.Piped_Sgnt = 1;
        disp_intr_enable(pdisp_crtc->disp_device, crtc_dpms->enable, intr);
    }

    return DISP_OK;
}

static disp_u8_t disp_crtc_set_mode_444_get_fix_output_signal(pdisp_crtc_set_mode_t pset)
{
    if (pset->output_signal == DISP_COLOR_FORMAT_RGB444 ||
        pset->output_signal == DISP_COLOR_FORMAT_YCRCB444)
    {
        return pset->output_signal;
    }
    else if (pset->hw_mode.output_format_mask & DISP_COLOR_FORMAT_RGB444)
    {
        return DISP_COLOR_FORMAT_RGB444;
    }
    else
    {
        return DISP_COLOR_FORMAT_YCRCB444;
    }
}

static disp_u8_t disp_crtc_set_mode_get_max_available_fix(pdisp_crtc_set_mode_t pset,
    disp_u32_t max_bd_kbyte, pdisp_u8_t poutput_signal, pdisp_u8_t pbpc)
{
    if (!pset->hw_mode.output_format_mask)
    {
        disp_print(DDM_WARNING, "disp_crtc_set_mode_get_max_available_fix, no available fmt!\n");
        return DISP_FALSE;
    }

    /* Format/Bpp:
     * 444_10 >  444_8 > 422_10 >  444_6 >  422_8 > 420_10 >  420_8 =  422_6 > 420_6
     * bpp_30 > bpp_24 > bpp_20 > bpp_18 > bpp_16 > bpp_15 > bpp_12 = bpp_12 > bpp_9
     */
    if ((pset->hw_mode.output_444_bpc_mask & (1 << DISP_COLOR_DEPTH_10))
        && (30ul * pset->hw_mode.clock + 7) / 8 <= max_bd_kbyte)
    {   // 444_10
        *poutput_signal = disp_crtc_set_mode_444_get_fix_output_signal(pset);
        *pbpc = 10;
        return DISP_TRUE;
    }
    else if ((pset->hw_mode.output_444_bpc_mask & (1 << DISP_COLOR_DEPTH_8))
        && (24ul * pset->hw_mode.clock + 7) / 8 <= max_bd_kbyte)
    {   // 444_8
        *poutput_signal = disp_crtc_set_mode_444_get_fix_output_signal(pset);
        *pbpc = 8;
        return DISP_TRUE;
    }
    else if ((pset->hw_mode.output_422_bpc_mask & (1 << DISP_COLOR_DEPTH_10))
        && (20ul * pset->hw_mode.clock + 7) / 8 <= max_bd_kbyte)
    {   // 422_10
        *poutput_signal = DISP_COLOR_FORMAT_YCRCB422;
        *pbpc = 10;
        return DISP_TRUE;
    }
    else if ((pset->hw_mode.output_444_bpc_mask & (1 << DISP_COLOR_DEPTH_6))
        && (18ul * pset->hw_mode.clock + 7) / 8 <= max_bd_kbyte)
    {   // 444_6
        *poutput_signal = disp_crtc_set_mode_444_get_fix_output_signal(pset);
        *pbpc = 6;
        return DISP_TRUE;
    }
    else if ((pset->hw_mode.output_422_bpc_mask & (1 << DISP_COLOR_DEPTH_8))
        && (16ul * pset->hw_mode.clock + 7) / 8 <= max_bd_kbyte)
    {   // 422_8
        *poutput_signal = DISP_COLOR_FORMAT_YCRCB422;
        *pbpc = 8;
        return DISP_TRUE;
    }
    else if ((pset->hw_mode.output_420_bpc_mask & (1 << DISP_COLOR_DEPTH_10))
        && (15ul * pset->hw_mode.clock + 7) / 8 <= max_bd_kbyte)
    {   // 420_10
        *poutput_signal = DISP_COLOR_FORMAT_YCRCB420;
        *pbpc = 10;
        return DISP_TRUE;
    }
    else if ((pset->hw_mode.output_420_bpc_mask & (1 << DISP_COLOR_DEPTH_8))
        && (12ul * pset->hw_mode.clock + 7) / 8 <= max_bd_kbyte)
    {   // 420_8
        *poutput_signal = DISP_COLOR_FORMAT_YCRCB420;
        *pbpc = 8;
        return DISP_TRUE;
    }
    else if ((pset->hw_mode.output_422_bpc_mask & (1 << DISP_COLOR_DEPTH_6))
        && (12ul * pset->hw_mode.clock + 7) / 8 <= max_bd_kbyte)
    {   // 422_6
        *poutput_signal = DISP_COLOR_FORMAT_YCRCB422;
        *pbpc = 6;
        return DISP_TRUE;
    }
    else if ((pset->hw_mode.output_420_bpc_mask & (1 << DISP_COLOR_DEPTH_6))
        && (9ul * pset->hw_mode.clock + 7) / 8 <= max_bd_kbyte)
    {   // 420_6
        *poutput_signal = DISP_COLOR_FORMAT_YCRCB420;
        *pbpc = 6;
        return DISP_TRUE;
    }

    disp_print(DDM_WARNING, "disp_crtc_set_mode_get_max_available_fix, no available! fmt_mask[%x] 444[%x] 422[%x] "
        "420[%x] max_bd_kbyte[%u]\n", pset->hw_mode.output_format_mask, pset->hw_mode.output_444_bpc_mask,
        pset->hw_mode.output_422_bpc_mask, pset->hw_mode.output_420_bpc_mask, max_bd_kbyte);
    return DISP_FALSE;
}

static disp_u8_t disp_crtc_set_mode_check(pdisp_crtc_t pdisp_crtc, pdisp_crtc_set_mode_t pset)
{
    pdisp_device_t pdd = pdisp_crtc->disp_device;
    pdisp_encoder_t pencoder = &pdd->disp_encoder[pdisp_crtc->index];
    pdisp_connector_t pconnector = &pdd->disp_connector[pdisp_crtc->index];
    disp_u32_t bd_kbyte = 0, max_bd_kbyte = 0, bpp = 0;
    disp_u8_t new_bpc = 0, new_output_signal = 0;

    if (pset->hw_mode.flags & DISP_MODE_FLAG_DSC)
    {   // DSC mode, no need check bandwidth.
        return DISP_TRUE;
    }

    switch (pset->output_signal)
    {
    case DISP_COLOR_FORMAT_YCRCB422:
        bpp = pset->bpc * 2;
        break;
    case DISP_COLOR_FORMAT_YCRCB420:
        bpp = pset->bpc * 3 / 2;
        break;
    case DISP_COLOR_FORMAT_RGB444:
    case DISP_COLOR_FORMAT_YCRCB444:
    default:
        bpp = pset->bpc * 3;
        break;
    }
    bd_kbyte = (bpp * pset->hw_mode.clock + 7) / 8;

    if (DISP_CONNECTOR_DP == pconnector->type || DISP_CONNECTOR_eDP == pconnector->type)
    {
        max_bd_kbyte = pencoder->policy.custom.dp.max_possible_bandwidth_kbyte;
    }
    else
    {
        disp_print(DDM_WARNING, "disp_crtc_set_mode_check, TODO hdmi\n");
        return DISP_FALSE;
    }

    if (bd_kbyte > max_bd_kbyte)
    {
        if (disp_crtc_set_mode_get_max_available_fix(pset, max_bd_kbyte, &new_output_signal, &new_bpc))
        {
            disp_print(DDM_TRACE, "disp_crtc_set_mode_check, bd[%u] max_bd[%u], fmt[%u -> %u] bpc[%u -> %u]\n",
                bd_kbyte, max_bd_kbyte, pset->output_signal, new_output_signal, pset->bpc, new_bpc);
            pset->output_signal = new_output_signal;
            pset->bpc = new_bpc;
        }
        else
        {
            return DISP_FALSE;
        }
    }

    return DISP_TRUE;
}

DISP_STATUS disp_crtc_set_mode(pdisp_crtc_t pdisp_crtc, disp_crtc_set_mode_t crtc_set_mode)
{
    pdisp_device_t pdd = pdisp_crtc->disp_device;
    pdisp_pipe_t pdisp_pipe = &pdisp_crtc->pipe;
    disp_csc_para_t csc_para = {0};
    disp_gamma_info_t gamma_info = {0};

    if(!pdisp_crtc)
        return -DISP_EINVAL;

    disp_crtc_set_mode_check(pdisp_crtc, &crtc_set_mode);

    crtc_set_property_mode(&pdisp_crtc->property, &crtc_set_mode.mode);
    crtc_set_property_hw_mode(&pdisp_crtc->property, &crtc_set_mode.hw_mode);
    crtc_set_property_output_signal(&pdisp_crtc->property, crtc_set_mode.output_signal);
    crtc_set_property_colorimetry(&pdisp_crtc->property, crtc_set_mode.colorimetry);
    crtc_set_property_bpc(&pdisp_crtc->property, crtc_set_mode.bpc);

    pdisp_pipe = &pdisp_crtc->pipe;

    disp_encoder_policy_config(&pdd->disp_encoder[pdisp_crtc->index]);
    if (pdd->disp_encoder[pdisp_crtc->index].policy.flag.bits.hdr_metadata)
    {
        pdd->disp_encoder[pdisp_crtc->index].policy.flag.bits.hdr_on = crtc_set_mode.hdr_mode;
        disp_memcpy(&pdd->disp_encoder[pdisp_crtc->index].policy.hdr_metadata,
            &crtc_set_mode.hdr_metadata, sizeof(disp_hdr_metadata_t));
    }

    disp_pipe_reset(pdisp_pipe);
    disp_pipe_set_tg(pdisp_pipe);//timing
    disp_pipe_set_pus(pdisp_pipe, DISP_TRUE);

    gamma_info.gamma_enable = 0;
    disp_pipe_set_lut(pdisp_pipe, &gamma_info);//lut
    disp_pipe_clear_reset(pdisp_pipe);

    csc_para.flag.bbypass = 0;
    csc_para.flag.bprogrammable = 0;
    disp_pipe_set_csc(pdisp_pipe, &csc_para);//color enhance

    disp_pipe_set_dither(pdisp_pipe);//dither

    return DISP_OK;
}

pdisp_plane_t disp_crtc_get_plane(pdisp_crtc_t pdisp_crtc, disp_u32_t plane_index)
{
    pdisp_plane_t pplane = DISP_NULL;


    if(!pdisp_crtc || plane_index > DISP_OVERLAY_PLANE_NUM + 2)
        return DISP_NULL;

    if(plane_index == DISP_PRIMARY_PLANE)
        pplane = &pdisp_crtc->primary;

    if(plane_index == DISP_OVERLAY_PLANE)
        pplane = &pdisp_crtc->overlay_plane[0];

    if(plane_index == DISP_CURSOR_PLANE)
        pplane = &pdisp_crtc->cursor;

    return pplane;

}

DISP_STATUS disp_crtc_plane_check(pdisp_crtc_t pdisp_crtc, pdisp_plane_update_para_t pupdate_para)
{
    pdisp_plane_t pplane = DISP_NULL;
    DISP_STATUS ret = 0;

    pplane = disp_crtc_get_plane(pdisp_crtc, pupdate_para->plane_index);

    if(!pplane)
        return -DISP_EINVAL;
    
    ret = disp_plane_check(pplane, pupdate_para);
    if(ret)
        return ret;
    ret = disp_pipe_check_overlay(&pdisp_crtc->pipe);
    if(ret)
        return ret;

    return DISP_OK;
}

DISP_STATUS disp_crtc_plane_update(pdisp_crtc_t pdisp_crtc, pdisp_plane_update_para_t pupdate_para)
{
    DISP_STATUS ret = 0;
    pdisp_device_t pdd = (pdisp_device_t)pdisp_crtc->disp_device;
    Reg_Plns_Mmio_1 reg_plns_1;
    disp_u32_t try_times;
    disp_u32_t offset_plns_1 = pdd->reg_off.plns[pdisp_crtc->index] + Register_Plns_Mmio_1_Start * 4;
    disp_u32_t mask_plns_1_epl = 0;
    disp_u8_t flip_immediate = pupdate_para->flag.valid.flip_immediate ? DISP_TRUE : DISP_FALSE;
    disp_u8_t dpms = pdisp_crtc->property.dpms;
    pdisp_plane_t pplane = disp_crtc_get_plane(pdisp_crtc, pupdate_para->plane_index);

#if 0 // Win10 behavior: immediate flip may come before dpms on. No damage now, do nothing.
    if (!dpms)
    {
        return DISP_OK;
    }
#endif

    if (DISP_PRIMARY_PLANE == pupdate_para->plane_index)
    {
        mask_plns_1_epl = 0x1;
        sr_write_field(pdisp_crtc->index, Reg_Sr_2f, Register_Sr_2f_Start, 1,
            Epl0_Vsync_On_Flip, flip_immediate ? 0 : 1);
    }
    else if (DISP_OVERLAY_PLANE == pupdate_para->plane_index)
    {
        mask_plns_1_epl = 0x2;
        sr_write_field(pdisp_crtc->index, Reg_Sr_2f, Register_Sr_2f_Start, 1,
            Epl1_Vsync_On_Flip, flip_immediate ? 0 : 1);
    }
    else if (DISP_CURSOR_PLANE == pupdate_para->plane_index)
    {
        mask_plns_1_epl = 0x4;
        sr_write_field(pdisp_crtc->index, Reg_Sr_2f, Register_Sr_2f_Start, 1,
            Cs_Vsync_On_Flip, flip_immediate ? 0 : 1);
    }

    ret = disp_plane_update(pplane, pupdate_para);
    if(ret)
        return ret;

    if (pupdate_para->flag.valid.overlay_scale_flush)
    {
        disp_plane_update_scale(&pdisp_crtc->overlay_plane[0], &pdisp_crtc->pipe.iwin.scale);
    }

    ret = disp_pipe_set_overlay(&pdisp_crtc->pipe, pplane);
    if(ret)
        return ret;

    /* G100 A0 patch: for some compress case that not robust.
     * High level driver make sure this case not happen, no need patch now.
     */
#if 0
    if (pplane->flip_immediate && !flip_immediate && dpms)
    {
        if (DISP_PRIMARY_PLANE == pplane->plane_index)
        {
            sr_write_field(pdisp_crtc->index, Reg_Sr_32, Register_Sr_32_Start, 1,
                Epl0_Rst, 1);
            sr_write_field(pdisp_crtc->index, Reg_Sr_32, Register_Sr_32_Start, 1,
                Epl0_Rst, 0);
        }
        else if (DISP_OVERLAY_PLANE == pplane->plane_index)
        {
            sr_write_field(pdisp_crtc->index, Reg_Sr_32, Register_Sr_32_Start, 1,
                Epl1_Rst, 1);
            sr_write_field(pdisp_crtc->index, Reg_Sr_32, Register_Sr_32_Start, 1,
                Epl1_Rst, 0);
        }
    }
#endif

    disp_plane_ov_trigger(pplane);

    if (pupdate_para->flag.valid.panel_scale)
    {
        pdisp_crtc->pipe.iwin.iwin_en = 1;
        disp_memcpy(&pdisp_crtc->pipe.iwin.scale, &pupdate_para->panel_scale, sizeof(disp_scale_t));
        disp_pipe_set_pus(&pdisp_crtc->pipe, DISP_FALSE);
    }

    // Wait trigger clear 0 after flip. Not wait if dpms off.
    pplane->flag.bits.flip_immediate = flip_immediate ? 1 : 0;
    if (flip_immediate && dpms)
    {
        try_times = 0x1000;
        reg_plns_1.uint = disp_read_u32(pdd, offset_plns_1);
        while (try_times && (reg_plns_1.uint & mask_plns_1_epl))
        {
            reg_plns_1.uint = disp_read_u32(pdd, offset_plns_1);
            try_times--;
        }
        if (!try_times)
        {
            disp_print(DDM_INFO, "wait trigger timeout! reg_plns_1[%x]\n", reg_plns_1.uint);
            disp_write_u32(pdd, offset_plns_1,  reg_plns_1.uint & (~mask_plns_1_epl));
        }
    }

    pdisp_crtc->pipe.sig_set.crc[0] = pupdate_para->sgnt_golden[0];
    pdisp_crtc->pipe.sig_set.crc[1] = pupdate_para->sgnt_golden[1];
    pdisp_crtc->pipe.sig_set.crc[2] = pupdate_para->sgnt_golden[2];
    pdisp_crtc->pipe.sig_set.crc[3] = pupdate_para->sgnt_golden[3];
    disp_pipe_set_sig_golden(&pdisp_crtc->pipe);

    disp_check_underflow(pplane->disp_device);

    return DISP_OK;
}

DISP_STATUS disp_crtc_wait_blank(pdisp_void_t pdisp_device, pdisp_wait_blank_t pParam)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_device;
    Reg_Plns_Mmio_2c plns_2c;
    disp_u32_t offset;
    disp_u32_t left_try_times;
    disp_u8_t blank_hit = DISP_FALSE;
    disp_u8_t hit = DISP_FALSE;

    if (!pdd || !pParam)
        return -DISP_EINVAL;

    offset = pdd->reg_off.plns[pParam->crtc_index] + Register_Plns_Mmio_2c_Start * 4;
    plns_2c.uint = 0;
    hit = DISP_FALSE;
    left_try_times = pParam->max_try_times;

    if (pParam->wait_over_first)
    {
        while (!hit && left_try_times)
        {
            plns_2c.uint = disp_read_u32(pdd, offset);

            if (pParam->hblank) // in hblank
                blank_hit = (plns_2c.reg.Rback_Hblk) ? DISP_TRUE : DISP_FALSE;
            else // in vblank
                blank_hit = (plns_2c.reg.Rback_Vblk) ? DISP_TRUE : DISP_FALSE;

            hit = pParam->inblank ? !blank_hit : blank_hit;
            left_try_times--;
        }

        if (!left_try_times)
        {
            disp_print(DDM_WARNING, "crtc[%u] wait %s%s over timeout!\n", pParam->crtc_index,
                pParam->hblank ? "h-" : "v-", pParam->inblank ? "active" : "blank");
            pParam->left_try_times = 0;
            return DISP_OK;
        }
    }

    hit = DISP_FALSE;
    left_try_times = pParam->max_try_times;
    while (!hit && left_try_times)
    {
        plns_2c.uint = disp_read_u32(pdd, offset);

        if (pParam->hblank) // in hblank
            blank_hit = (plns_2c.reg.Rback_Hblk) ? DISP_TRUE : DISP_FALSE;
        else // in vblank
            blank_hit = (plns_2c.reg.Rback_Vblk) ? DISP_TRUE : DISP_FALSE;

        hit = pParam->inblank ? blank_hit : !blank_hit;
        left_try_times--;
    }

    if (!left_try_times)
    {
        disp_print(DDM_WARNING, "crtc[%u] wait %s%s timeout!\n", pParam->crtc_index,
            pParam->hblank ? "h-" : "v-", pParam->inblank ? "blank" : "active");
    }

    pParam->left_try_times = left_try_times;
    pParam->reg_val = plns_2c.uint;
    return DISP_OK;
}
