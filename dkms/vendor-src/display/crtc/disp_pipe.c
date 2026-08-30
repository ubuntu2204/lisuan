/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#include "disp_pipe.h"
#include "disp_device.h"
#include "disp_callback.h"
#include "SR_register.h"
#include "disp_reg.h"
#include "PLNS_register.h"
#include "PIPE_register.h"
#include "CR_register.h"
#include "CR_SHARE_register.h"
#include "disp_plane.h"
#include "disp_hw_common.h"

#define pipe_write_field(pipe_index, reg_name, reg_start, num, ...) \
{ \
    reg32_write_field(reg_name, (reg_start * 4 + pdd->reg_off.pipe[pipe_index]), num, __VA_ARGS__) \
}

#define pipe_read_field(pipe_index, reg_name, reg_start, num, ...) \
{ \
    reg32_read_field(reg_name, (reg_start * 4 + pdd->reg_off.pipe[pipe_index]), num, __VA_ARGS__) \
}

#define disp_print(level, log, ...) disp_print_ex(DISP_PRINT_MASK_PIPE, level, log, ##__VA_ARGS__)

DISP_STATUS disp_pipe_sw_init(pdisp_pipe_t pdisp_pipe)
{
    pdisp_device_t pdd = DISP_NULL;

    if(!pdisp_pipe)
        return -DISP_EINVAL;

    pdd = (pdisp_device_t)pdisp_pipe->pdisp_device;

    if(pdd->device_id == DISP_DEVICE_G100)
    {
        pdisp_pipe->pus.support_pu = DISP_TRUE;
        pdisp_pipe->pus.max_pu_src_hor = 2560;
        pdisp_pipe->pus.max_pu_src_ver = 1600;
        pdisp_pipe->pus.max_pu_dst_hor = 4096;
        pdisp_pipe->pus.max_pu_dst_ver = 2340;

        return DISP_OK;
    }
    else
    {
        return -DISP_EPRIV_DEVICE_ID;
    }
}

DISP_STATUS disp_pipe_sw_deinit(pdisp_pipe_t pdisp_pipe)
{
    disp_print(DDM_TRACE, "tofill: %#x\n", pdisp_pipe);
    return DISP_OK;
}

DISP_STATUS disp_pipe_hw_init(pdisp_pipe_t pdisp_pipe)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_pipe->pdisp_device;

//actually follow crtc register should be touch in crtc but not in pipe. refine later

//set trigger
    crtc_write_field(pdisp_pipe->crtc_index, Reg_Plns_Mmio_1, Register_Plns_Mmio_1_Start, 10,
        Epl0_Sw_Flip_En, 1,
        Epl1_Sw_Flip_En, 1,
        Cs_Sw_Flip_En, 1,
        Scl_Sw_Flip_En, 1,
        Bld0_Sw_Flip_En, 1,
        Bld1_Sw_Flip_En, 1,
        Pus_Sw_Flip_En, 1,
        Gam_Sw_Flip_En, 1,
        Ocsc_Sw_Flip_En, 1,
        Pipe_Sw_Flip_En, 1);

//vsync on flip
    sr_write_field(pdisp_pipe->crtc_index, Reg_Sr_2f, Register_Sr_2f_Start, 4,
        Epl0_Vsync_On_Flip, SR_2F_EPL0_VSYNC_ON_FLIP_ENABLE,
        Epl1_Vsync_On_Flip, 1,
        Cs_Vsync_On_Flip, 1,
        Scl_Vsync_On_Flip, 1);

    sr_write_field(pdisp_pipe->crtc_index, Reg_Sr_30, Register_Sr_30_Start, 7,
        Bld0_Vsync_On_Flip, SR_30_BLD0_VSYNC_ON_FLIP_ENABLE,
        Bld1_Vsync_On_Flip, 1,
        Pus_Vsync_On_Flip, 1,
        Gam_Vsync_On_Flip, 1,
        Ocsc_Vsync_On_Flip, 1,
        Pipe_Vsync_On_Flip, 1,
        Dhi_Vsync_On_Flip, 1);

    /* Usually set Sel_Div to 0, pll set will check mode clk and re-write Sel_Div again. */
    cr_write_field(pdisp_pipe->crtc_index, Reg_Cr_81, Register_Cr_81_Start, 1,
        Sel_Div, 0);

    return DISP_OK;
}

DISP_STATUS disp_pipe_hw_deinit(pdisp_pipe_t pdisp_pipe)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_pipe->pdisp_device;

//clear trigger
    crtc_write_field(pdisp_pipe->crtc_index, Reg_Plns_Mmio_1, Register_Plns_Mmio_1_Start, 9,
        Epl0_Sw_Flip_En, 0,
        Epl1_Sw_Flip_En, 0,
        Cs_Sw_Flip_En, 0,
        Scl_Sw_Flip_En, 0,
        Bld0_Sw_Flip_En, 0,
        Bld1_Sw_Flip_En, 0,
        Pus_Sw_Flip_En, 0,
        Gam_Sw_Flip_En, 0,
        Ocsc_Sw_Flip_En, 0);

//clear vsync on flip
    sr_write_field(pdisp_pipe->crtc_index, Reg_Sr_2f, Register_Sr_2f_Start, 4,
        Epl0_Vsync_On_Flip, SR_2F_EPL0_VSYNC_ON_FLIP_DISABLE,
        Epl1_Vsync_On_Flip, 0,
        Cs_Vsync_On_Flip, 0,
        Scl_Vsync_On_Flip, 0);

    sr_write_field(pdisp_pipe->crtc_index, Reg_Sr_30, Register_Sr_30_Start, 7,
        Bld0_Vsync_On_Flip, SR_30_BLD0_VSYNC_ON_FLIP_DISABLE,
        Bld1_Vsync_On_Flip, 0,
        Pus_Vsync_On_Flip, 0,
        Gam_Vsync_On_Flip, 0,
        Ocsc_Vsync_On_Flip, 0,
        Pipe_Vsync_On_Flip, 0,
        Dhi_Vsync_On_Flip, 0);

    cr_write_field(pdisp_pipe->crtc_index, Reg_Cr_81, Register_Cr_81_Start, 1,
        Sel_Div, 0);

    return DISP_OK;
}

DISP_STATUS disp_pipe_reset(pdisp_pipe_t pdisp_pipe)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_pipe->pdisp_device;

    sr_write_field(pdisp_pipe->crtc_index, Reg_Sr_32, Register_Sr_32_Start, 4,
        Epl0_Rst, SR_32_EPL0_RST_RESET,
        Epl1_Rst, SR_32_EPL1_RST_RESET,
        Cs_Rst, SR_32_CS_RST_RESET,
        Tg_Rst, SR_32_CS_RST_RESET);

//select pipe
    if(pdisp_pipe->crtc_index == 0)
    {
        cr_share_write_field(Reg_Cr_27, Register_Cr_27_Start, 1,
            Io_Sel, CR_27_IO_SEL_SEL_PIPEA_IO);
    }
    else if(pdisp_pipe->crtc_index == 1)
    {
        cr_share_write_field(Reg_Cr_27, Register_Cr_27_Start, 1,
            Io_Sel, CR_27_IO_SEL_SEL_PIPEB_IO);
    }
    else if(pdisp_pipe->crtc_index == 2)
    {
        cr_share_write_field(Reg_Cr_27, Register_Cr_27_Start, 1,
            Io_Sel, CR_27_IO_SEL_SEL_PIPEC_IO);
    }
    else
    {
        cr_share_write_field(Reg_Cr_27, Register_Cr_27_Start, 1,
            Io_Sel, CR_27_IO_SEL_SEL_PIPED_IO);
    }

//disable vsync, otherwise vsync will always in reset state, this is timing trigger
    cr_share_write_field(Reg_Cr_17, Register_Cr_17_Start, 1,
        Cr17_Sync_Rstn, CR_17_CR17_SYNC_RSTN_RESET);

    return DISP_OK;
}

DISP_STATUS disp_pipe_clear_reset(pdisp_pipe_t pdisp_pipe)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_pipe->pdisp_device;

    sr_write_field(pdisp_pipe->crtc_index, Reg_Sr_32, Register_Sr_32_Start, 4,
        Epl0_Rst, SR_32_EPL0_RST_NOT_RESET,
        Epl1_Rst, SR_32_EPL1_RST_NOT_RESET,
        Cs_Rst, SR_32_CS_RST_NOT_RESET,
        Tg_Rst, SR_32_CS_RST_NOT_RESET);

//select pipe
    if(pdisp_pipe->crtc_index == 0)
    {
        cr_share_write_field(Reg_Cr_27, Register_Cr_27_Start, 1,
            Io_Sel, CR_27_IO_SEL_SEL_PIPEA_IO);
    }
    else if(pdisp_pipe->crtc_index == 1)
    {
        cr_share_write_field(Reg_Cr_27, Register_Cr_27_Start, 1,
            Io_Sel, CR_27_IO_SEL_SEL_PIPEB_IO);
    }
    else if(pdisp_pipe->crtc_index == 2)
    {
        cr_share_write_field(Reg_Cr_27, Register_Cr_27_Start, 1,
            Io_Sel, CR_27_IO_SEL_SEL_PIPEC_IO);
    }
    else
    {
        cr_share_write_field(Reg_Cr_27, Register_Cr_27_Start, 1,
            Io_Sel, CR_27_IO_SEL_SEL_PIPED_IO);
    }

//enable vsync, otherwise vsync will always in reset state, this is timing trigger
    cr_share_write_field(Reg_Cr_17, Register_Cr_17_Start, 1,
        Cr17_Sync_Rstn, CR_17_CR17_SYNC_RSTN_ENABLE);

    return DISP_OK;
}

static disp_void_t ov_generate(pdisp_pipe_t pdisp_pipe, pdisp_plane_t pdisp_plane)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_pipe->pdisp_device;
    pdisp_crtc_t pcrtc = &pdd->disp_crtc[pdisp_pipe->crtc_index];
    pdisp_plane_t pprimary = &pcrtc->primary;
    pdisp_plane_t poverlay = &pcrtc->overlay_plane[0];
    pdisp_ov_t pov0 = &pdisp_pipe->ov[0];
    pdisp_ov_t pov1 = &pdisp_pipe->ov[1];

    if(pdisp_plane->plane_index == DISP_PRIMARY_PLANE)
    {
        disp_memset(pov0, 0, sizeof(disp_ov_t));

        //for primary, must be B over A window key, and kp = 0, ks = 8
        if(pprimary->blend_info.blend_mode == DISP_BLEND_A_WINDOW_KEY)
            pov0->key_mode = PIPE_MMIO_2_BLD0_MODE_A_WIN_KEY;
        else if(pprimary->blend_info.blend_mode == DISP_BLEND_B_WINDOW_KEY)
            pov0->key_mode = PIPE_MMIO_2_BLD0_MODE_B_WIN_KEY;
        else if(pprimary->blend_info.blend_mode == DISP_BLEND_A_ALPHA_KEY)
            pov0->key_mode = PIPE_MMIO_2_BLD0_MODE_A_ALP_KEY;
        else if(pprimary->blend_info.blend_mode == DISP_BLEND_B_ALPHA_KEY)
            pov0->key_mode = PIPE_MMIO_2_BLD0_MODE_B_ALP_KEY;
        else if(pprimary->blend_info.blend_mode == DISP_BLEND_A_COLOR_KEY)
            pov0->key_mode = PIPE_MMIO_2_BLD0_MODE_A_COL_KEY;
        else if(pprimary->blend_info.blend_mode == DISP_BLEND_B_COLOR_KEY)
            pov0->key_mode = PIPE_MMIO_2_BLD0_MODE_B_COL_KEY;
        else if(pprimary->blend_info.blend_mode == DISP_BLEND_A_ALPHA_BLENDING)
            pov0->key_mode = PIPE_MMIO_2_BLD0_MODE_A_ALP;
        else if(pprimary->blend_info.blend_mode == DISP_BLEND_B_ALPHA_BLENDING)
            pov0->key_mode = PIPE_MMIO_2_BLD0_MODE_B_ALP;
        else if (pprimary->blend_info.blend_mode == DISP_BLEND_A_CONSTANT_ALPHA)
            pov0->key_mode = PIPE_MMIO_2_BLD0_MODE_A_CONST_ALP;
        else if (pprimary->blend_info.blend_mode == DISP_BLEND_B_CONSTANT_ALPHA)
            pov0->key_mode = PIPE_MMIO_2_BLD0_MODE_B_CONST_ALP;//bld_mode[2] = 1, means B over A constant alpha blending
        else
            pov0->key_mode = PIPE_MMIO_2_BLD0_MODE_B_WIN_KEY;

        pov0->kp = pprimary->blend_info.factor.kp_kmix;
        pov0->ks = pprimary->blend_info.factor.ks_kt;
        pov0->bld_mode = pprimary->blend_info.pre_multi;
        pov0->inv_alpha = pprimary->blend_info.invert_a;
        pov0->alpha_sel = pprimary->blend_info.alpha_sel;
        pov0->key_sel = pprimary->blend_info.key_sel;
        pov0->plane_alpha = pprimary->blend_info.plane_alpha;
    } 
    else if(pdisp_plane->plane_index == DISP_OVERLAY_PLANE)
    {
        disp_memset(pov1, 0, sizeof(disp_ov_t));

        if(poverlay->blend_info.blend_mode == DISP_BLEND_A_WINDOW_KEY)
            pov1->key_mode = PIPE_MMIO_3_BLD1_MODE_A_WIN_KEY;
        else if(poverlay->blend_info.blend_mode == DISP_BLEND_B_WINDOW_KEY)
            pov1->key_mode = PIPE_MMIO_3_BLD1_MODE_B_WIN_KEY;
        else if(poverlay->blend_info.blend_mode == DISP_BLEND_A_ALPHA_KEY)
            pov1->key_mode = PIPE_MMIO_3_BLD1_MODE_A_ALP_KEY;
        else if(poverlay->blend_info.blend_mode == DISP_BLEND_B_ALPHA_KEY)
            pov1->key_mode = PIPE_MMIO_3_BLD1_MODE_B_ALP_KEY;
        else if(poverlay->blend_info.blend_mode == DISP_BLEND_A_COLOR_KEY)
            pov1->key_mode = PIPE_MMIO_3_BLD1_MODE_A_COL_KEY;
        else if(poverlay->blend_info.blend_mode == DISP_BLEND_B_COLOR_KEY)
            pov1->key_mode = PIPE_MMIO_3_BLD1_MODE_B_COL_KEY;
        else if(poverlay->blend_info.blend_mode == DISP_BLEND_A_ALPHA_BLENDING)
            pov1->key_mode = PIPE_MMIO_3_BLD1_MODE_A_ALP;
        else if(poverlay->blend_info.blend_mode == DISP_BLEND_B_ALPHA_BLENDING)
            pov1->key_mode = PIPE_MMIO_3_BLD1_MODE_B_ALP;
        else if (poverlay->blend_info.blend_mode == DISP_BLEND_A_CONSTANT_ALPHA)
            pov1->key_mode = PIPE_MMIO_3_BLD1_MODE_A_CONST_ALP;
        else if (poverlay->blend_info.blend_mode == DISP_BLEND_B_CONSTANT_ALPHA)
            pov1->key_mode = PIPE_MMIO_3_BLD1_MODE_B_CONST_ALP;//bld_mode[2] = 1, means B over A constant alpha blending
        else
            pov1->key_mode = PIPE_MMIO_3_BLD1_MODE_B_WIN_KEY;

        pov1->kp = poverlay->blend_info.factor.kp_kmix;
        pov1->ks = poverlay->blend_info.factor.ks_kt;
        pov1->bld_mode = poverlay->blend_info.pre_multi;
        pov1->inv_alpha = poverlay->blend_info.invert_a;
        pov1->alpha_sel = poverlay->blend_info.alpha_sel;
        pov1->key_sel = poverlay->blend_info.key_sel;
        pov1->plane_alpha = poverlay->blend_info.plane_alpha;
    }
}

static disp_void_t ov_flush(pdisp_pipe_t pdisp_pipe, pdisp_plane_t pdisp_plane)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_pipe->pdisp_device;
    pdisp_ov_t pov0 = &pdisp_pipe->ov[0];
    pdisp_ov_t pov1 = &pdisp_pipe->ov[1];

    pipe_write_field(pdisp_pipe->crtc_index, Reg_Pipe_Mmio_0, Register_Pipe_Mmio_0_Start, 3,
        Pl0_Force_Top, PIPE_MMIO_0_PL0_FORCE_TOP_DISABLE,
        Pl1_Force_Top, PIPE_MMIO_0_PL1_FORCE_TOP_DISABLE,
        Dis_Secbld, PIPE_MMIO_0_DIS_SECBLD_DISABLE);

//set to rgb black now, modify it by hdmi/dp output signal later
    pipe_write_field(pdisp_pipe->crtc_index, Reg_Pipe_Mmio_1, Register_Pipe_Mmio_1_Start, 2,
        Back_Color, 0,
        Back_Ycbcr, PIPE_MMIO_1_BACK_YCBCR_RGB);
//for ycbcr black
//    pipe_write_field(pdisp_pipe->crtc_index, Reg_Pipe_Mmio_1, Register_Pipe_Mmio_1_Start, 2,
//        Back_Color, 0x80200,
//        Back_Ycbcr, PIPE_MMIO_1_BACK_YCBCR_YCBCR);
    
    if (pdisp_plane->plane_index == DISP_PRIMARY_PLANE)
    {
        pipe_write_field(pdisp_pipe->crtc_index, Reg_Pipe_Mmio_2, Register_Pipe_Mmio_2_Start, 9,
            Bld0_Key_Sel, pov0->key_sel,
            Bld0_Alp_Sel, pov0->alpha_sel,
            Bld0_Alp_Area, 0,//check with hw
            Bld0_Reverse, pov0->plane_alpha,
            Bld0_Const_B, pov0->ks,
            Bld0_Const_A, pov0->kp,
            Bld0_Mode, pov0->key_mode,
            Bld0_Inv_Alp, pov0->inv_alpha,
            Bld0_Pre_Mult, pov0->bld_mode);
    }
    else if (pdisp_plane->plane_index == DISP_OVERLAY_PLANE)
    {
        pipe_write_field(pdisp_pipe->crtc_index, Reg_Pipe_Mmio_3, Register_Pipe_Mmio_3_Start, 9,
            Bld1_Key_Sel, pov1->key_sel,
            Bld1_Alp_Sel, pov1->alpha_sel,
            Bld1_Alp_Area, 0,
            Bld1_Reverse, pov1->plane_alpha,
            Bld1_Const_B, pov1->ks,
            Bld1_Const_A, pov1->kp,
            Bld1_Mode, pov1->key_mode,
            Bld1_Inv_Alp, pov1->inv_alpha,
            Bld1_Pre_Mult, pov1->bld_mode);
    }
}

DISP_STATUS disp_pipe_check_overlay(pdisp_pipe_t pdisp_pipe)
{
    disp_print(DDM_TRACE, "tofill: %#x\n", pdisp_pipe);
    return DISP_OK;
}

DISP_STATUS disp_pipe_set_overlay(pdisp_pipe_t pdisp_pipe, pdisp_plane_t pplane)
{
    ov_generate(pdisp_pipe, pplane);
    ov_flush(pdisp_pipe, pplane);

    return DISP_OK;
}

DISP_STATUS disp_plane_ov_trigger(pdisp_plane_t pdisp_plane)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_plane->disp_device;

    if(pdisp_plane->plane_index == DISP_PRIMARY_PLANE)
    {
        crtc_write_field(pdisp_plane->crtc_index, Reg_Plns_Mmio_1, Register_Plns_Mmio_1_Start, 3,
            Epl0_Sw_Flip_Trig, 1,
            Bld0_Sw_Flip_Trig, 1,
            Scl_Sw_Flip_Trig, 1);
    }
    else if(pdisp_plane->plane_index == DISP_OVERLAY_PLANE)
    {
        crtc_write_field(pdisp_plane->crtc_index, Reg_Plns_Mmio_1, Register_Plns_Mmio_1_Start, 3,
            Epl1_Sw_Flip_Trig, 1,
            Bld1_Sw_Flip_Trig, 1,
            Scl_Sw_Flip_Trig, 1);
    }
    else
    {
        crtc_write_field(pdisp_plane->crtc_index, Reg_Plns_Mmio_1, Register_Plns_Mmio_1_Start, 1,
            Cs_Sw_Flip_Trig, 1);
    }

    return DISP_OK;
}

static disp_void_t tg_generate(pdisp_pipe_t pdisp_pipe)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_pipe->pdisp_device;
    pdisp_crtc_t pcrtc = &pdd->disp_crtc[pdisp_pipe->crtc_index];
    pdisp_tg_t ptg = &pdisp_pipe->tg;
    disp_display_mode_t mode = {0};

    disp_memset(ptg, 0 , sizeof(disp_tg_t));
    crtc_get_property_hw_mode(&pcrtc->property, &mode);

    ptg->ht = mode.htotal;
    ptg->hde = mode.hdisplay;
    ptg->hbs = mode.hdisplay;
    ptg->hbe = mode.htotal;
    ptg->hss = mode.hsync_start;
    ptg->hse = mode.hsync_end;
    ptg->vt = mode.vtotal;
    ptg->vde = mode.vdisplay;
    ptg->vbs = mode.vdisplay;
    ptg->vbe = mode.vtotal;
    ptg->vss = mode.vsync_start;
    ptg->vse = mode.vsync_end;

    if (pdd->flag.info.pz1_config)
    {
        pdisp_pipe->hsync_negative = 1;
        pdisp_pipe->vsync_negative = 1;
    }
    else
    {
        pdisp_pipe->hsync_negative = (mode.flags & DISP_MODE_FLAG_NHSYNC) ? 1 : 0;
        pdisp_pipe->vsync_negative = (mode.flags & DISP_MODE_FLAG_NVSYNC) ? 1 : 0;
    }
}

static disp_void_t tg_flush(pdisp_pipe_t pdisp_pipe)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_pipe->pdisp_device;
    pdisp_tg_t ptg = &pdisp_pipe->tg;
    disp_u8_t reg_sync_negative = 0;

    sr_write_field(pdisp_pipe->crtc_index, Reg_Sr_10, Register_Sr_10_Start, 1,
        Htotal_0, (ptg->ht - 4) & 0xFF);
    sr_write_field(pdisp_pipe->crtc_index, Reg_Sr_11, Register_Sr_11_Start, 1, 
        Htotal_1, ((ptg->ht - 4) >> 8) & 0x3F);

    sr_write_field(pdisp_pipe->crtc_index, Reg_Sr_12, Register_Sr_12_Start, 1,
        Hde_0, ptg->hde & 0xFF);
    sr_write_field(pdisp_pipe->crtc_index, Reg_Sr_13, Register_Sr_13_Start, 1,
        Hde_1, (ptg->hde >> 8) & 0x3F);

    sr_write_field(pdisp_pipe->crtc_index, Reg_Sr_14, Register_Sr_14_Start, 1,
        Hblk_Beg_0, ptg->hbs & 0xFF);
    sr_write_field(pdisp_pipe->crtc_index, Reg_Sr_15, Register_Sr_15_Start, 1,
        Hblk_Beg_1, (ptg->hbs >> 8) & 0x3F);

    sr_write_field(pdisp_pipe->crtc_index, Reg_Sr_16, Register_Sr_16_Start, 1,
        Hblk_End_0, ptg->hbe & 0xFF);
    sr_write_field(pdisp_pipe->crtc_index, Reg_Sr_17, Register_Sr_17_Start, 1,
        Hblk_End_1, (ptg->hbe >> 8) & 0x3F);

    sr_write_field(pdisp_pipe->crtc_index, Reg_Sr_18, Register_Sr_18_Start, 1,
        Hsync_Beg_0, ptg->hss & 0xFF);
    sr_write_field(pdisp_pipe->crtc_index, Reg_Sr_19, Register_Sr_19_Start, 1,
        Hsync_Beg_1, (ptg->hss >> 8) & 0x3F);

    sr_write_field(pdisp_pipe->crtc_index, Reg_Sr_1a, Register_Sr_1a_Start, 1,
        Hsync_End_0, ptg->hse & 0xFF);
    sr_write_field(pdisp_pipe->crtc_index, Reg_Sr_1b, Register_Sr_1b_Start, 1,
        Hsync_End_1, (ptg->hse >> 8) & 0x3F);

    sr_write_field(pdisp_pipe->crtc_index, Reg_Sr_1e, Register_Sr_1e_Start, 1,
        Vtotal_0, (ptg->vt - 1) & 0xFF);
    sr_write_field(pdisp_pipe->crtc_index, Reg_Sr_1f, Register_Sr_1f_Start, 1,
        Vtotal_1, ((ptg->vt - 1) >> 8) & 0x1F);

    sr_write_field(pdisp_pipe->crtc_index, Reg_Sr_20, Register_Sr_20_Start, 1,
        Vde_0, ptg->vde & 0xFF);
    sr_write_field(pdisp_pipe->crtc_index, Reg_Sr_21, Register_Sr_21_Start, 1,
        Vde_1, (ptg->vde >> 8) & 0x1F);

    sr_write_field(pdisp_pipe->crtc_index, Reg_Sr_22, Register_Sr_22_Start, 1,
        Vblk_Beg_0, ptg->vbs & 0xFF);
    sr_write_field(pdisp_pipe->crtc_index, Reg_Sr_23, Register_Sr_23_Start, 1,
        Vblk_Beg_1, (ptg->vbs >> 8) & 0x1F);

    sr_write_field(pdisp_pipe->crtc_index, Reg_Sr_24, Register_Sr_24_Start, 1,
        Vblk_End_0, ptg->vbe & 0xFF);
    sr_write_field(pdisp_pipe->crtc_index, Reg_Sr_25, Register_Sr_25_Start, 1,
        Vblk_End_1, (ptg->vbe >> 8) & 0x1F);

    sr_write_field(pdisp_pipe->crtc_index, Reg_Sr_26, Register_Sr_26_Start, 1,
        Vsync_Beg_0, ptg->vss & 0xFF);
    sr_write_field(pdisp_pipe->crtc_index, Reg_Sr_27, Register_Sr_27_Start, 1,
        Vsync_Beg_1, (ptg->vss >> 8) & 0x1F);
    
    sr_write_field(pdisp_pipe->crtc_index, Reg_Sr_28, Register_Sr_28_Start, 1,
        Vsync_End_0, ptg->vse & 0xFF);
    sr_write_field(pdisp_pipe->crtc_index, Reg_Sr_29, Register_Sr_29_Start, 1,
        Vsync_End_1, (ptg->vse >> 8) & 0x1F);

    //select pipe
    if (pdisp_pipe->crtc_index == 0)
    {
        cr_share_write_field(Reg_Cr_27, Register_Cr_27_Start, 1,
            Io_Sel, CR_27_IO_SEL_SEL_PIPEA_IO);
    }
    else if (pdisp_pipe->crtc_index == 1)
    {
        cr_share_write_field(Reg_Cr_27, Register_Cr_27_Start, 1,
            Io_Sel, CR_27_IO_SEL_SEL_PIPEB_IO);
    }
    else if(pdisp_pipe->crtc_index == 2)
    {
        cr_share_write_field(Reg_Cr_27, Register_Cr_27_Start, 1,
            Io_Sel, CR_27_IO_SEL_SEL_PIPEC_IO);
    }
    else
    {
        cr_share_write_field(Reg_Cr_27, Register_Cr_27_Start, 1,
            Io_Sel, CR_27_IO_SEL_SEL_PIPED_IO);
    }

//bit 6 = 1, mean hsync -, bit 6 = 0, meas hsync +
//bit 7 = 1, mean vsync -, bit 7 = 0, meas vsync +
    reg_sync_negative = 0;
    reg_sync_negative |= pdisp_pipe->hsync_negative << 6;
    reg_sync_negative |= pdisp_pipe->vsync_negative << 7;
    disp_print(DDM_INFO, "tg_flush reg_sync_negative[%x]\n", reg_sync_negative);
    disp_write_u8(pdd, 0x83c2, reg_sync_negative);
}

disp_void_t disp_pipe_set_tg(pdisp_pipe_t pdisp_pipe)
{
    tg_generate(pdisp_pipe);
    tg_flush(pdisp_pipe);
}

static disp_void_t disp_pipe_pus_update_from_mode(pdisp_pipe_t pdisp_pipe)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_pipe->pdisp_device;
    pdisp_crtc_t pcrtc = &pdd->disp_crtc[pdisp_pipe->crtc_index];

    disp_memset(&pdisp_pipe->iwin, 0, sizeof(disp_iwindow_t));

    pdisp_pipe->iwin.iwin_en = 1;
    pdisp_pipe->iwin.scale.start_x = 0;
    pdisp_pipe->iwin.scale.start_y = 0;
    pdisp_pipe->iwin.scale.src_width = pcrtc->property.mode.hdisplay;
    pdisp_pipe->iwin.scale.src_height = pcrtc->property.mode.vdisplay;
    pdisp_pipe->iwin.scale.dst_width = pcrtc->property.hw_mode.hdisplay;
    pdisp_pipe->iwin.scale.dst_height = pcrtc->property.hw_mode.vdisplay;
}

static disp_void_t pus_generate(pdisp_pipe_t pdisp_pipe)
{
    pdisp_pus_t ppus = &pdisp_pipe->pus;
    pdisp_iwindow_t piwin = &pdisp_pipe->iwin;
    disp_u64_t temp = 0;

    disp_memset(ppus, 0, sizeof(disp_pus_t));

    //set pu
    ppus->pus_ratio_mode = PIPE_MMIO_4_PUS_HW_RATIO_SW_RATIO;
    ppus->pus_cos2tap = PIPE_MMIO_4_PUS_HCOSTAP_COSINE;
    ppus->pus_ratio_plus = PIPE_MMIO_4_PUS_RATIO_PLUS_ORG;
    ppus->pus_vsync_dlyval = PIPE_MMIO_4_PUS_VDUP_INTERP;

    if (piwin->scale.dst_width && (piwin->scale.src_width != piwin->scale.dst_width))
    {
        ppus->pus_hen = PIPE_MMIO_4_PUS_H_EN_ENABLE;
        temp = (disp_u64_t)0x400000 * piwin->scale.src_width;
        ppus->pus_hacc = (temp / piwin->scale.dst_width) & 0x3FFFFF;
    }
    else
    {
        ppus->pus_hen = PIPE_MMIO_4_PUS_H_EN_DISABLE;
        ppus->pus_hacc = 0x400000;
    }

    if (piwin->scale.dst_height && (piwin->scale.src_height != piwin->scale.dst_height))
    {
        ppus->pus_ven = PIPE_MMIO_4_PUS_V_EN_ENABLE;
        temp = (disp_u64_t)0x400000 * piwin->scale.src_height;
        ppus->pus_vacc = (temp / piwin->scale.dst_height) & 0x3FFFFF;
    }
    else
    {
        ppus->pus_ven = PIPE_MMIO_4_PUS_V_EN_DISABLE;
        ppus->pus_vacc = 0x400000;
    }

    ppus->pus_en = ppus->pus_hen | ppus->pus_ven;

    if(ppus->pus_en)
    {
        ppus->pus_width_dst = piwin->scale.dst_width;
        ppus->pus_height_dst = piwin->scale.dst_height;
    }
    else
    {//if scaler disable, no conception of dst window
        ppus->pus_width_dst = 0;
        ppus->pus_height_dst = 0;
    }
}

static disp_u8_t disp_pipe_wait_vblank(pdisp_pipe_t pdisp_pipe)
{
    disp_u8_t ret = DISP_FALSE;
    Reg_Plns_Mmio_2c plns_2c;
    pdisp_device_t pdd = (pdisp_device_t)pdisp_pipe->pdisp_device;
    disp_u32_t offset = pdd->reg_off.plns[pdisp_pipe->crtc_index] + Register_Plns_Mmio_2c_Start * 4;
    disp_u32_t max_times = 0x20000;

    while (!ret && max_times)
    {
        plns_2c.uint = disp_read_u32(pdd, offset);
        ret = (plns_2c.reg.Rback_Vblk && 0 == ret) ? 1 : 0;
        max_times--;
    }

    if (!ret)
    {
        disp_print(DDM_WARNING, "wait vblank timeout!\n");
    }
    return ret;
}

static disp_void_t pus_flush(pdisp_pipe_t pdisp_pipe)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_pipe->pdisp_device;
    pdisp_pus_t ppus = &pdisp_pipe->pus;
    pdisp_iwindow_t piwin = &pdisp_pipe->iwin;

    crtc_write_field(pdisp_pipe->crtc_index, Reg_Plns_Mmio_2, Register_Plns_Mmio_2_Start, 3,
        Swin_Xstart, piwin->scale.start_x,
        Swin_Ystart, piwin->scale.start_y,
        Swin_En, piwin->iwin_en);

    crtc_write_field(pdisp_pipe->crtc_index, Reg_Plns_Mmio_3, Register_Plns_Mmio_3_Start, 2,
        Swin_Width, piwin->scale.src_width,
        Swin_Height, piwin->scale.src_height);

    pipe_write_field(pdisp_pipe->crtc_index, Reg_Pipe_Mmio_4, Register_Pipe_Mmio_4_Start, 8, 
        Pus_V_En, ppus->pus_ven,
        Pus_H_En, ppus->pus_hen,
        Pus_En, ppus->pus_en,
        Pus_Vdup, ppus->pus_vsync_dlyval,
        Pus_Hcostap, ppus->pus_cos2tap,
        Pus_Vcostap, ppus->pus_cos2tap,
        Pus_Hw_Ratio, ppus->pus_ratio_mode,
        Pus_Ratio_Plus, ppus->pus_ratio_plus);
    
    pipe_write_field(pdisp_pipe->crtc_index, Reg_Pipe_Mmio_5, Register_Pipe_Mmio_5_Start, 1,
        Pus_V_Ratio, (disp_u32_t)ppus->pus_vacc);
    pipe_write_field(pdisp_pipe->crtc_index, Reg_Pipe_Mmio_6, Register_Pipe_Mmio_6_Start, 1,
        Pus_H_Ratio, (disp_u32_t)ppus->pus_hacc);
    
    pipe_write_field(pdisp_pipe->crtc_index, Reg_Pipe_Mmio_7, Register_Pipe_Mmio_7_Start, 2,
        Pus_Dst_Ht, ppus->pus_height_dst,
        Pus_Dst_Wd, ppus->pus_width_dst);

    crtc_write_field(pdisp_pipe->crtc_index, Reg_Plns_Mmio_1, Register_Plns_Mmio_1_Start, 1,
        Pus_Sw_Flip_Trig, 1);
}

disp_void_t disp_pipe_set_pus(pdisp_pipe_t pdisp_pipe, disp_u8_t mode_update)
{
    if (mode_update)
        disp_pipe_pus_update_from_mode(pdisp_pipe);
    pus_generate(pdisp_pipe);
    if (!mode_update) // iwin no trigger, wait vblank to avoid garbage before flip.
        disp_pipe_wait_vblank(pdisp_pipe);
    pus_flush(pdisp_pipe);
}

disp_void_t disp_pipe_set_lut(pdisp_pipe_t pdisp_pipe, pdisp_gamma_info_t gamma_info)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_pipe->pdisp_device;
    disp_fifo_para_t lut_fifo = {0};
    disp_u8_t gamma_enable;
/*
1. lut info select
	cr share 28: Lut_Info_Sel
    0 pipe0 lut
	1 pipe1 lut
	2 pipe2 lut
	3 pipe3 lut
	4 all pipe lut, select all pipe's lut, w/r same time
	5 dp a infoframe
	6 dp b infoframe
	7 hd audio a or hdmi a
	8 hd autio b or hdmi b
	9 ha audio c
	10 ha audio d

2. disable lut project bit
	cf share 1f[0] = 0

3. select gamma setting
	6/8/10 bit
	interpolation or not

4. write gamma table
	CFF0 index,
	write CFF4: write data
	read CFF4: read data

5. triger gamma

6. enable lut project bit
*/
    if(!gamma_info->gamma_enable)
    {
        pipe_write_field(pdisp_pipe->crtc_index, Reg_Pipe_Mmio_8, Register_Pipe_Mmio_8_Start, 1,
            Gam_En, PIPE_MMIO_8_GAM_EN_DISABLE);//use mmio way, hw always take 10bit

        return;
    }

    cr_share_write_field(Reg_Cr_1f, Register_Cr_1f_Start, 1,
        Lut_Pr, 0);

    lut_fifo.fifo_select = pdisp_pipe->crtc_index;//refine later

    lut_fifo.data = disp_malloc(gamma_info->lut_length * sizeof(disp_u32_t));
    if(!lut_fifo.data)
    {
        disp_print(DDM_ERROR, "disp_pipe_set_lut, alloc failed\n");
        return;
    }

    disp_prepare_gamma_fifo(&lut_fifo, gamma_info);

    disp_write_fifo(pdd, &lut_fifo);

    disp_free(lut_fifo.data);
    lut_fifo.data = DISP_NULL;

    //for cts video pattern, disable gamma for no 8 bit -> 10 bit concatenate 
    gamma_enable = pdd->flag.info.dp_cts ? PIPE_MMIO_8_GAM_EN_DISABLE : PIPE_MMIO_8_GAM_EN_ENABLE;

    pipe_write_field(pdisp_pipe->crtc_index, Reg_Pipe_Mmio_8, Register_Pipe_Mmio_8_Start, 3,
        Gam_En, gamma_enable,
        Gam_Interp, PIPE_MMIO_8_GAM_INTERP_DISABLE,
        Gam_Width, PIPE_MMIO_8_GAM_WIDTH_WID_30B);//use mmio way, hw always take 10bit

    crtc_write_field(pdisp_pipe->crtc_index, Reg_Plns_Mmio_1, Register_Plns_Mmio_1_Start, 1,
        Gam_Sw_Flip_Trig, 1);

    cr_share_write_field(Reg_Cr_1f, Register_Cr_1f_Start, 1,
        Lut_Pr, 1);
}

disp_void_t disp_pipe_csc_generate(pdisp_pipe_t pdisp_pipe, pdisp_csc_para_t pcsc_para)
{
    pdisp_csc_t pcsc = &pdisp_pipe->csc;
    pdisp_device_t pdd = (pdisp_device_t)pdisp_pipe->pdisp_device;
    pdisp_encoder_t pdisp_encoder = &pdd->disp_encoder[pdisp_pipe->crtc_index];
    struct disp_encoder_policy* ppolicy = &pdisp_encoder->policy;

    pcsc->input_fmt = PIPE_MMIO_9_OCSC_IN_FMT_RGB_FULL;
    pcsc->output_fmt = PIPE_MMIO_9_OCSC_IN_FMT_RGB_FULL;

    if(DISP_COLOR_FORMAT_TYPE_RGB == ppolicy->color_format)
        pcsc->output_fmt = PIPE_MMIO_9_OCSC_IN_FMT_RGB_FULL;
    else
    {
        switch(ppolicy->colorimetry)
        {
            case DISP_COLORIMETRY_YCC_NONE:
                pcsc->output_fmt = PIPE_MMIO_9_OCSC_IN_FMT_YUV601;
                break;
            case DISP_COLORIMETRY_YCC_SMPTE_170M:
            case DISP_COLORIMETRY_XVYCC_601:
            case DISP_COLORIMETRY_SYCC_601:
            case DISP_COLORIMETRY_YCC_ADOBE_601:
                pcsc->output_fmt = PIPE_MMIO_9_OCSC_IN_FMT_YCBCR601;
                break;
            case DISP_COLORIMETRY_YCC_BT709:
                pcsc->output_fmt = PIPE_MMIO_9_OCSC_IN_FMT_YUV709;
                break;
            case DISP_COLORIMETRY_XVYCC_709:
                pcsc->output_fmt = PIPE_MMIO_9_OCSC_IN_FMT_YCBCR709;
                break;
            case DISP_COLORIMETRY_YCC_BT2020:
                pcsc->output_fmt = PIPE_MMIO_9_OCSC_IN_FMT_YUV2020;
                break;
            case DISP_COLORIMETRY_CYCC_BT2020:
                pcsc->output_fmt = PIPE_MMIO_9_OCSC_IN_FMT_YCBCR2020;
                break;
            case DISP_COLORIMETRY_SRGB:
            case DISP_COLORIMETRY_RGB_ADOBE:
            case DISP_COLORIMETRY_RGB_BT2020:
            case DISP_COLORIMETRY_RGB_DCI:
            case DISP_COLORIMETRY_RGB_DCI_D65:
            default:
                disp_print(DDM_INFO, "disp_pipe_csc_generate, invalid colorimetry[%u], use rgb_full\n", ppolicy->colorimetry);
                pcsc->output_fmt = PIPE_MMIO_9_OCSC_IN_FMT_RGB_FULL;
                break;
        }
    }
    pcsc->yuv422 = (DISP_COLOR_FORMAT_TYPE_YUV422 == ppolicy->color_format) ? 1 : 0;
    pcsc->yuv420 = (DISP_COLOR_FORMAT_TYPE_YUV420 == ppolicy->color_format) ? 1 : 0;
    pcsc->convert_mode = 0; // drop mode
    pcsc->range = 0; // full range

    pcsc->prog = pcsc_para->flag.bprogrammable;
    pcsc->bypss = (pdd->flag.info.dp_cts) ? 1 : pcsc_para->flag.bbypass;
    pcsc->brightness = pcsc_para->brightness;

    pcsc->coef1 = pcsc_para->coef_1;
    pcsc->coef2 = pcsc_para->coef_2;
    pcsc->coef3 = pcsc_para->coef_3;
    pcsc->coef4 = pcsc_para->coef_4;
    pcsc->coef5 = pcsc_para->coef_5;
    pcsc->coef6 = pcsc_para->coef_6;
    pcsc->coef7 = pcsc_para->coef_7;
    pcsc->coef8 = pcsc_para->coef_8;
    pcsc->coef9 = pcsc_para->coef_9;

    disp_csc_dump(pcsc);
}

disp_void_t disp_pipe_csc_flush(pdisp_pipe_t pdisp_pipe)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_pipe->pdisp_device;
    pdisp_csc_t pcsc = &pdisp_pipe->csc;

    pipe_write_field(pdisp_pipe->crtc_index, Reg_Pipe_Mmio_9, Register_Pipe_Mmio_9_Start, 5,
        Ocsc_In_Fmt, pcsc->input_fmt,
        Ocsc_Out_Fmt, pcsc->output_fmt,
        Ocsc_Prog, pcsc->prog,
        Ocsc_Brightness, pcsc->brightness,
        Ocsc_Bypass, pcsc->bypss);

    pipe_write_field(pdisp_pipe->crtc_index, Reg_Pipe_Mmio_10, Register_Pipe_Mmio_10_Start, 5,
        Fcvt_En420, pcsc->yuv420,
        Fcvt_Mode420, pcsc->convert_mode,
        Fcvt_En422, pcsc->yuv422,
        Fcvt_Mode422, pcsc->convert_mode,
        Fcvt_Range, pcsc->range);

    pipe_write_field(pdisp_pipe->crtc_index, Reg_Pipe_Mmio_A, Register_Pipe_Mmio_A_Start, 2,
        Ocsc_Coef_F1, pcsc->coef1,
        Ocsc_Coef_F2, pcsc->coef2);
    pipe_write_field(pdisp_pipe->crtc_index, Reg_Pipe_Mmio_B, Register_Pipe_Mmio_B_Start, 2,
        Ocsc_Coef_F3, pcsc->coef3,
        Ocsc_Coef_F4, pcsc->coef4);
    pipe_write_field(pdisp_pipe->crtc_index, Reg_Pipe_Mmio_C, Register_Pipe_Mmio_C_Start, 2,
        Ocsc_Coef_F5, pcsc->coef5,
        Ocsc_Coef_F6, pcsc->coef6);
    pipe_write_field(pdisp_pipe->crtc_index, Reg_Pipe_Mmio_D, Register_Pipe_Mmio_D_Start, 2,
        Ocsc_Coef_F7, pcsc->coef7,
        Ocsc_Coef_F8, pcsc->coef8);
    pipe_write_field(pdisp_pipe->crtc_index, Reg_Pipe_Mmio_E, Register_Pipe_Mmio_E_Start, 1,
        Ocsc_Coef_F9, pcsc->coef9);
    
    crtc_write_field(pdisp_pipe->crtc_index, Reg_Plns_Mmio_1, Register_Plns_Mmio_1_Start, 1,
        Ocsc_Sw_Flip_Trig, 1);
}

disp_void_t disp_pipe_set_csc(pdisp_pipe_t pdisp_pipe, pdisp_csc_para_t pcsc_para)
{
    disp_pipe_csc_generate(pdisp_pipe, pcsc_para);
    disp_pipe_csc_flush(pdisp_pipe);
}

disp_void_t disp_pipe_dither_genarete(pdisp_pipe_t pdisp_pipe)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_pipe->pdisp_device;
    pdisp_dither_t pdither = &pdisp_pipe->dither;
    pdisp_encoder_t pencoder = &pdd->disp_encoder[pdisp_pipe->crtc_index];

    /* Dith_Patsel: if discarded bits no more than 2, 2x2 no different from 4x4
     * Dith_Distr: The effect is not clear, need test
     */
    pdither->dither_dist_en = PIPE_MMIO_F_DITH_DISTR_DISABLE;
    pdither->dither_bitsel = PIPE_MMIO_F_DITH_PATSEL_DITH_2X2;
    if (!pdd->flag.info.dp_cts && 6 == pencoder->policy.bpc)
    {
        pdither->dither_en = PIPE_MMIO_F_DITH_EN_ENABLE;
        pdither->dither_base = PIPE_MMIO_F_DITH_BASE_DITH_6BIT;
    }
    else
    {
        pdither->dither_en = PIPE_MMIO_F_DITH_EN_DISABLE;
        pdither->dither_base = PIPE_MMIO_F_DITH_BASE_NO_DITHER;
    }
}

disp_void_t disp_pipe_dither_flush(pdisp_pipe_t pdisp_pipe)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_pipe->pdisp_device;
    pdisp_dither_t pdither = &pdisp_pipe->dither;

    pipe_write_field(pdisp_pipe->crtc_index, Reg_Pipe_Mmio_F, Register_Pipe_Mmio_F_Start, 4,
        Dith_En, pdither->dither_en,
        Dith_Distr, pdither->dither_dist_en,
        Dith_Patsel, pdither->dither_bitsel,
        Dith_Base, pdither->dither_base);
//trigger here? 
}

disp_void_t disp_pipe_set_dither(pdisp_pipe_t pdisp_pipe)
{
    disp_pipe_dither_genarete(pdisp_pipe);
    disp_pipe_dither_flush(pdisp_pipe);
}

DISP_STATUS disp_pipe_get_itg_counter(pdisp_pipe_t pdisp_pipe, pdisp_itg_counter_t pitg_counter)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_pipe->pdisp_device;
    Reg_Plns_Mmio_2c mmio_plns_2c;
    Reg_Pipe_Mmio_1b mmio_pipe_1b;
    Reg_Pipe_Mmio_1c mmio_pipe_1c;
    disp_u32_t offset_plns_2c = 0, offset_pipe_1b = 0, offset_pipe_1c = 0;

    if(!pitg_counter)
        return -DISP_EINVAL;

    offset_plns_2c = pdd->reg_off.plns[pdisp_pipe->crtc_index] + Register_Plns_Mmio_2c_Start * 4;
    offset_pipe_1b = pdd->reg_off.pipe[pdisp_pipe->crtc_index] + Register_Pipe_Mmio_1b_Start * 4;
    offset_pipe_1c = pdd->reg_off.pipe[pdisp_pipe->crtc_index] + Register_Pipe_Mmio_1c_Start * 4;

    mmio_plns_2c.uint = disp_read_u32(pdd, offset_plns_2c);
    mmio_pipe_1b.uint = disp_read_u32(pdd, offset_pipe_1b);
    mmio_pipe_1c.uint = disp_read_u32(pdd, offset_pipe_1c);

    pitg_counter->bInVblank = (disp_u8_t)mmio_plns_2c.reg.Rback_Vblk;
    pitg_counter->bInHblank = (disp_u8_t)mmio_plns_2c.reg.Rback_Hblk;
    pitg_counter->bInVsync = (disp_u8_t)mmio_plns_2c.reg.Rback_Vsync;
    pitg_counter->bInHsync = (disp_u8_t)mmio_plns_2c.reg.Rback_Hsync;

    if(pitg_counter->flag.bGetFrameCnt)
        pitg_counter->Value[DISP_COUNTER_FRAME] = mmio_pipe_1b.reg.Dpc_Frame;

    if(pitg_counter->flag.bGetLineCnt)
        pitg_counter->Value[DISP_COUNTER_LINE] = mmio_plns_2c.reg.Rback_Vcounter;

    if(pitg_counter->flag.bGetPixelCnt)
        pitg_counter->Value[DISP_COUNTER_PIXEL] = mmio_plns_2c.reg.Rback_Hcounter * 4;

    if(pitg_counter->flag.bGetLineDpcCnt)
        pitg_counter->Value[DISP_COUNTER_LINE_DPC] = mmio_pipe_1b.reg.Dpc_Line;

    if(pitg_counter->flag.bGetPixelDpcCnt)
        pitg_counter->Value[DISP_COUNTER_PIXEL_DPC] = mmio_pipe_1c.reg.Dpc_Pxl;

    disp_print(DDM_TRACE, "tg_fr[%u]: vpos[%u] hpos[%u]"
        ", counter-de-blk-sync: V[%u %u %u %u] H[%u %u %u %u]\n",
        mmio_pipe_1b.reg.Dpc_Frame,
        mmio_pipe_1b.reg.Dpc_Line,
        mmio_pipe_1c.reg.Dpc_Pxl,
        mmio_plns_2c.reg.Rback_Vcounter,
        mmio_plns_2c.reg.Rback_Vde,
        mmio_plns_2c.reg.Rback_Vblk,
        mmio_plns_2c.reg.Rback_Vsync,
        mmio_plns_2c.reg.Rback_Hcounter,
        mmio_plns_2c.reg.Rback_Hde,
        mmio_plns_2c.reg.Rback_Hblk,
        mmio_plns_2c.reg.Rback_Hsync);

    return DISP_OK;
}

DISP_STATUS disp_pipe_set_sig(pdisp_pipe_t pdisp_pipe, pdisp_sig_set_t psig_set)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_pipe->pdisp_device;

    pipe_write_field(pdisp_pipe->crtc_index, Reg_Pipe_Mmio_11, Register_Pipe_Mmio_11_Start, 3,
        Sgnt_En, psig_set->benable,
        Sgnt_Sta_V, psig_set->isr.x,
        Sgnt_Sta_H, psig_set->isr.y);

    pipe_write_field(pdisp_pipe->crtc_index, Reg_Pipe_Mmio_12, Register_Pipe_Mmio_12_Start, 2,
        Sgnt_Height, psig_set->isr.h,
        Sgnt_Width, psig_set->isr.w);

    disp_memcpy(&pdisp_pipe->sig_set, psig_set, sizeof(disp_sig_set_t));

    crtc_write_field(pdisp_pipe->crtc_index, Reg_Plns_Mmio_1, Register_Plns_Mmio_1_Start, 1,
        Pipe_Sw_Flip_Trig, 1);

    return DISP_OK;
}

DISP_STATUS disp_pipe_get_sig(pdisp_pipe_t pdisp_pipe, pdisp_sig_get_t psig_get)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_pipe->pdisp_device;

    pipe_read_field(pdisp_pipe->crtc_index, Reg_Pipe_Mmio_17, Register_Pipe_Mmio_17_Start, 1,
                    Sgnt_0, &psig_get->crc[0]);

    pipe_read_field(pdisp_pipe->crtc_index, Reg_Pipe_Mmio_18, Register_Pipe_Mmio_18_Start, 1,
                    Sgnt_1, &psig_get->crc[1]);

    pipe_read_field(pdisp_pipe->crtc_index, Reg_Pipe_Mmio_19, Register_Pipe_Mmio_19_Start, 1,
                    Sgnt_2, &psig_get->crc[2]);

    pipe_read_field(pdisp_pipe->crtc_index, Reg_Pipe_Mmio_1a, Register_Pipe_Mmio_1a_Start, 1,
                    Sgnt_3, &psig_get->crc[3]);

    return DISP_OK;
}

DISP_STATUS disp_pipe_set_sig_golden(pdisp_pipe_t pdisp_pipe)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_pipe->pdisp_device;

    pipe_write_field(pdisp_pipe->crtc_index, Reg_Pipe_Mmio_13, Register_Pipe_Mmio_13_Start, 1,
        Sgnt_Gold_0, pdisp_pipe->sig_set.crc[0]);

    pipe_write_field(pdisp_pipe->crtc_index, Reg_Pipe_Mmio_14, Register_Pipe_Mmio_14_Start, 1,
        Sgnt_Gold_1, pdisp_pipe->sig_set.crc[1]);

    pipe_write_field(pdisp_pipe->crtc_index, Reg_Pipe_Mmio_15, Register_Pipe_Mmio_15_Start, 1,
        Sgnt_Gold_2, pdisp_pipe->sig_set.crc[2]);

    pipe_write_field(pdisp_pipe->crtc_index, Reg_Pipe_Mmio_16, Register_Pipe_Mmio_16_Start, 1,
        Sgnt_Gold_3, pdisp_pipe->sig_set.crc[3]);

    crtc_write_field(pdisp_pipe->crtc_index, Reg_Plns_Mmio_1, Register_Plns_Mmio_1_Start, 1,
        Pipe_Sw_Flip_Trig, 1);

    return DISP_OK;
}

DISP_STATUS disp_pipe_get_sig_golden(pdisp_pipe_t pdisp_pipe, pdisp_sig_get_t psig_get)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_pipe->pdisp_device;

    pipe_read_field(pdisp_pipe->crtc_index, Reg_Pipe_Mmio_13, Register_Pipe_Mmio_13_Start, 1,
        Sgnt_Gold_0, &psig_get->crc[0]);

    pipe_read_field(pdisp_pipe->crtc_index, Reg_Pipe_Mmio_14, Register_Pipe_Mmio_14_Start, 1,
        Sgnt_Gold_1, &psig_get->crc[1]);

    pipe_read_field(pdisp_pipe->crtc_index, Reg_Pipe_Mmio_15, Register_Pipe_Mmio_15_Start, 1,
        Sgnt_Gold_2, &psig_get->crc[2]);

    pipe_read_field(pdisp_pipe->crtc_index, Reg_Pipe_Mmio_16, Register_Pipe_Mmio_16_Start, 1,
        Sgnt_Gold_3, &psig_get->crc[3]);

    return DISP_OK;
}
