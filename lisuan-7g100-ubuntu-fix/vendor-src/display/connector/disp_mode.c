/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#include "disp_mode.h"
#include "disp_device.h"
#include "disp_edid.h"
#include "disp_callback.h"

#define disp_print(level, log, ...) disp_print_ex(DISP_PRINT_MASK_MODE, level, log, ##__VA_ARGS__)
#define disp_print_pure(level, log, ...) disp_print_pure_ex(DISP_PRINT_MASK_MODE, level, log, ##__VA_ARGS__)

disp_void_t disp_mode_debug_printmodeline(pdisp_display_mode_t pmode, disp_u32_t level)
{
    disp_print_pure(level, "Modeline " DISP_MODE_FMT "\n", DISP_MODE_ARG(pmode));
}

static disp_u8_t disp_mode_probed_add_check(pdisp_connector_t pdisp_connector, pdisp_display_mode_t pmode)
{
    pdisp_device_t pdd = pdisp_connector->disp_device;
    pdisp_encoder_t pencoder = &pdd->disp_encoder[pdisp_connector->index];
    pdisp_dsc_t pdsc = (pdisp_dsc_t)pencoder->pdsc;
    disp_u32_t max_bd_kbyte = 0;
    disp_u32_t max_bpc = DISP_MIN(pencoder->caps.max_output_bpc, pdisp_connector->disp_info.bpc);
    disp_u32_t bd_kbyte_16bpc = 0, bd_kbyte_12bpc = 0; // HW not support.
    disp_u32_t bd_kbyte_10bpc = (max_bpc >= 10) ? (30ul * pmode->clock + 7) / 8 : 0;
    disp_u32_t bd_kbyte_8bpc = (max_bpc >= 8) ? 3ul * pmode->clock : 0;
    disp_u32_t bd_kbyte_6bpc = (max_bpc >= 6) ? (18ul * pmode->clock + 7) / 8 : 0;
    disp_u32_t temp_mask = 0;
    disp_u8_t support_444 = ((pdisp_connector->disp_info.color_formats & DISP_COLOR_FORMAT_RGB444)
        || (pdisp_connector->disp_info.color_formats & DISP_COLOR_FORMAT_YCRCB444)) ? DISP_TRUE : DISP_FALSE;
    disp_u8_t support_422 = (pdisp_connector->disp_info.color_formats & DISP_COLOR_FORMAT_YCRCB422) ? DISP_TRUE : DISP_FALSE;
    disp_u8_t support_420 = (pmode->output_format_mask & DISP_COLOR_FORMAT_YCRCB420) ? DISP_TRUE : DISP_FALSE;
    disp_u8_t support_dsc = (pdsc && pdisp_connector->disp_info.dsc_sink_caps.flag.bits.dsc_caps) ? DISP_TRUE : DISP_FALSE;
    disp_u8_t try_dsc10 = DISP_FALSE, try_dsc8 = DISP_FALSE;
    disp_u8_t check_dsc10 = DISP_FALSE, check_dsc8 = DISP_FALSE;

    // Not Support interlaced mode.
    if (pmode->flags & DISP_MODE_FLAG_INTERLACE)
    {
        disp_print(DDM_TRACE, "disp_mode_probed_add_check, skip interlaced mode clk[%d]\n", pmode->clock);
        return DISP_FALSE;
    }

    if (DISP_CONNECTOR_DP == pdisp_connector->type || DISP_CONNECTOR_eDP == pdisp_connector->type)
    {
        max_bd_kbyte = pencoder->policy.custom.dp.max_possible_bandwidth_kbyte;
    }
    else
    {
        disp_print(DDM_WARNING, "disp_mode_probed_add_check, TODO hdmi\n");
        return DISP_FALSE;
    }

    // Mask output format. 444 & 422 for all mode, 420 for specific mode.
    temp_mask = pdisp_connector->disp_info.color_formats;
    temp_mask &= ~DISP_COLOR_FORMAT_YCRCB420;
    pmode->output_format_mask |= temp_mask;

    if (bd_kbyte_10bpc && max_bd_kbyte >= bd_kbyte_10bpc)
    {
        // Do nothing.
    }
    else if (bd_kbyte_8bpc && max_bd_kbyte >= bd_kbyte_8bpc)
    {
        try_dsc10 = DISP_TRUE;
        if (support_dsc && pdsc->func.check_timing)
            check_dsc10 = pdsc->func.check_timing(pdsc, pmode, 10);
        if (check_dsc10) // dsc10 OK, assert dsc8 OK.
            pmode->flags |= DISP_MODE_FLAG_DSC;
    }
    else
    {
        try_dsc8 = DISP_TRUE;
        if (support_dsc && pdsc->func.check_timing)
            check_dsc8 = pdsc->func.check_timing(pdsc, pmode, 8);
        if (check_dsc8)
            pmode->flags |= DISP_MODE_FLAG_DSC;
    }

    if ((!try_dsc10 && !try_dsc8)   // No need DSC.
        || try_dsc10                // 10 bpc DSC mode. 8 bpc normal mode.
        || (try_dsc8 && check_dsc8))// 8 bpc DSC mode.
    {
        // DSC mode OK, assert all available bpc OK.
        temp_mask = bd_kbyte_6bpc ? (1 << DISP_COLOR_DEPTH_6) : 0;
        temp_mask |= bd_kbyte_8bpc ? (1 << DISP_COLOR_DEPTH_8) : 0;
        temp_mask |= bd_kbyte_10bpc ? (1 << DISP_COLOR_DEPTH_10) : 0;
        temp_mask |= bd_kbyte_12bpc ? (1 << DISP_COLOR_DEPTH_12) : 0;
        temp_mask |= bd_kbyte_16bpc ? (1 << DISP_COLOR_DEPTH_16) : 0;

        pmode->output_444_bpc_mask = support_444 ? (disp_u8_t)temp_mask : 0;
        pmode->output_422_bpc_mask = support_422 ? (disp_u8_t)temp_mask : 0;
        pmode->output_420_bpc_mask = support_420 ? (disp_u8_t)temp_mask : 0;

        if (try_dsc10 && !check_dsc10) // 444_10 unavailable.
        {
            temp_mask = ~(1ul << DISP_COLOR_DEPTH_10);
            pmode->output_444_bpc_mask &= temp_mask;
        }
    }
    else if (try_dsc8 && !check_dsc8) // DSC unavailable, 444_8 unavailable, normal mode bandwidth check.
    {
        if (support_444)
        {
            pmode->output_444_bpc_mask = (bd_kbyte_6bpc && max_bd_kbyte >= bd_kbyte_6bpc) ? (1 << DISP_COLOR_DEPTH_6) : 0;
            pmode->output_444_bpc_mask |= (bd_kbyte_8bpc && max_bd_kbyte >= bd_kbyte_8bpc) ? (1 << DISP_COLOR_DEPTH_8) : 0;
            pmode->output_444_bpc_mask |= (bd_kbyte_10bpc && max_bd_kbyte >= bd_kbyte_10bpc) ? (1 << DISP_COLOR_DEPTH_10) : 0;
            pmode->output_444_bpc_mask |= (bd_kbyte_12bpc && max_bd_kbyte >= bd_kbyte_12bpc) ? (1 << DISP_COLOR_DEPTH_12) : 0;
            pmode->output_444_bpc_mask |= (bd_kbyte_16bpc && max_bd_kbyte >= bd_kbyte_16bpc) ? (1 << DISP_COLOR_DEPTH_16) : 0;
        }

        if (support_422)
        {
            pmode->output_422_bpc_mask = (bd_kbyte_6bpc && max_bd_kbyte >= bd_kbyte_6bpc) ? (1 << DISP_COLOR_DEPTH_6) : 0;
            pmode->output_422_bpc_mask |= (bd_kbyte_8bpc && max_bd_kbyte >= bd_kbyte_8bpc) ? (1 << DISP_COLOR_DEPTH_8) : 0;
            pmode->output_422_bpc_mask |= (bd_kbyte_10bpc && max_bd_kbyte >= bd_kbyte_10bpc) ? (1 << DISP_COLOR_DEPTH_10) : 0;
            pmode->output_422_bpc_mask |= (bd_kbyte_12bpc && max_bd_kbyte >= bd_kbyte_12bpc) ? (1 << DISP_COLOR_DEPTH_12) : 0;
            pmode->output_422_bpc_mask |= (bd_kbyte_16bpc && max_bd_kbyte >= bd_kbyte_16bpc) ? (1 << DISP_COLOR_DEPTH_16) : 0;
        }

        if (support_420)
        {
            pmode->output_420_bpc_mask = (bd_kbyte_6bpc && max_bd_kbyte >= bd_kbyte_6bpc) ? (1 << DISP_COLOR_DEPTH_6) : 0;
            pmode->output_420_bpc_mask |= (bd_kbyte_8bpc && max_bd_kbyte >= bd_kbyte_8bpc) ? (1 << DISP_COLOR_DEPTH_8) : 0;
            pmode->output_420_bpc_mask |= (bd_kbyte_10bpc && max_bd_kbyte >= bd_kbyte_10bpc) ? (1 << DISP_COLOR_DEPTH_10) : 0;
            pmode->output_420_bpc_mask |= (bd_kbyte_12bpc && max_bd_kbyte >= bd_kbyte_12bpc) ? (1 << DISP_COLOR_DEPTH_12) : 0;
            pmode->output_420_bpc_mask |= (bd_kbyte_16bpc && max_bd_kbyte >= bd_kbyte_16bpc) ? (1 << DISP_COLOR_DEPTH_16) : 0;
        }
    }

    // Remove format support if no bpc available.
    if (support_444 && !pmode->output_444_bpc_mask)
        pmode->output_format_mask &= (disp_u8_t)(~(DISP_COLOR_FORMAT_RGB444 | DISP_COLOR_FORMAT_YCRCB444));
    if (support_422 && !pmode->output_422_bpc_mask)
        pmode->output_format_mask &= (disp_u8_t)(~DISP_COLOR_FORMAT_YCRCB422);
    if (support_420 && !pmode->output_420_bpc_mask)
        pmode->output_format_mask &= (disp_u8_t)(~DISP_COLOR_FORMAT_YCRCB420);

    if ((!pmode->output_format_mask) || (!pmode->output_444_bpc_mask &&
        !pmode->output_422_bpc_mask && !pmode->output_420_bpc_mask))
    {
        disp_print(DDM_TRACE, "disp_mode_probed_add_check, no valid bpc! "
            "fmt[%x] max_bpc[%u] clk[%d] bd[%u] lr[%x] lc[%x]\n",
            pmode->output_format_mask, max_bpc, pmode->clock, max_bd_kbyte,
            pencoder->policy.custom.dp.max_possible_link_rate,
            pencoder->policy.custom.dp.max_possible_lane_count);
        return DISP_FALSE;
    }

    return DISP_TRUE;
}

disp_s8_t disp_mode_probed_add(pdisp_connector_t pdisp_connector,
    pdisp_display_mode_t pmode, disp_u8_t free_mode)
{
    if (!pmode)
    {
        disp_print(DDM_WARNING, "disp_mode_probed_add, pmode NULL!\n");
        return DISP_FALSE;
    }

    if (!disp_mode_probed_add_check(pdisp_connector, pmode))
    {
        if (free_mode) disp_free(pmode);
        return DISP_FALSE;
    }

    if (pdisp_connector->flag.info.add_mode_to_modelist)
    {
        disp_memcpy(pdisp_connector->mode_list.modes + pdisp_connector->mode_list.num,
            pmode, sizeof(disp_display_mode_t));
        disp_mode_debug_printmodeline(pmode, DDM_TRACE);
        pdisp_connector->mode_list.num++;
        if (!pdisp_connector->native_mode && (pmode->type & DISP_MODE_TYPE_PREFERRED))
        {   // Choose first prefer mode as native mode.
            pdisp_connector->native_mode = pdisp_connector->mode_list.modes + (pdisp_connector->mode_list.num - 1);
        }
        if (pdisp_connector->max_clk_khz < pmode->clock)
        {
            pdisp_connector->max_clk_khz = pmode->clock;
        }
    }

    if (free_mode) disp_free(pmode);
    return DISP_TRUE;
}

/**
 * disp_cvt_mode -create a modeline based on the CVT algorithm
 * @hdisplay: hdisplay size
 * @vdisplay: vdisplay size
 * @vrefresh: vrefresh rate
 * @reduced: whether to use reduced blanking
 * @interlaced: whether to compute an interlaced mode
 * @margins: whether to add margins (borders)
 *
 * This function is called to generate the modeline based on CVT algorithm
 * according to the hdisplay, vdisplay, vrefresh.
 * It is based from the VESA(TM) Coordinated Video Timing Generator by
 * Graham Loveridge April 9, 2003 available at
 * http://www.elo.utfsm.cl/~elo212/docs/CVTd6r1.xls 
 *
 * And it is copied from xf86CVTmode in xserver/hw/xfree86/modes/xf86cvt.c.
 * What I have done is to translate it by using integer calculation.
 *
 * Returns:
 * The modeline based on the CVT algorithm stored in a disp_display_mode_t object.
 * The display mode object is allocated with disp_mode_create(). Returns NULL
 * when no mode could be allocated.
 */
disp_u8_t disp_cvt_mode(disp_s32_t hdisplay,
                      disp_s32_t vdisplay, disp_s32_t vrefresh,
                      disp_u8_t reduced, disp_u8_t interlaced, disp_u8_t margins, pdisp_display_mode_t pmode)
{
#define HV_FACTOR            1000
    /* 1) top/bottom margin size (% of height) - default: 1.8, */
#define    CVT_MARGIN_PERCENTAGE        18
    /* 2) character cell horizontal granularity (pixels) - default 8 */
#define    CVT_H_GRANULARITY        8
    /* 3) Minimum vertical porch (lines) - default 3 */
#define    CVT_MIN_V_PORCH            3
    /* 4) Minimum number of vertical back porch lines - default 6 */
#define    CVT_MIN_V_BPORCH        6
    /* Pixel Clock step (kHz) */
#define CVT_CLOCK_STEP            250
    disp_u32_t vfieldrate, hperiod;
    disp_s32_t hdisplay_rnd, hmargin, vdisplay_rnd, vmargin;
    disp_s32_t interlace;
    disp_u64_t tmp;
    disp_u16_t vsync;

    if(!hdisplay || !vdisplay)
        return DISP_FALSE;

    /* the CVT default refresh rate is 60Hz */
    if(!vrefresh)
        vrefresh = 60;

    /* the required field fresh rate */
    if(interlaced)
        vfieldrate = vrefresh * 2;
    else
        vfieldrate = vrefresh;

    /* horizontal pixels */
    hdisplay_rnd = hdisplay - (hdisplay % CVT_H_GRANULARITY);

    /* determine the left&right borders */
    hmargin = 0;
    if(margins) 
    {
        hmargin = hdisplay_rnd * CVT_MARGIN_PERCENTAGE / 1000;
        hmargin -= hmargin % CVT_H_GRANULARITY;
    }
    /* find the total active pixels */
    pmode->hdisplay = (disp_u16_t)(hdisplay_rnd + 2 * hmargin);

    /* find the number of lines per field */
    if(interlaced)
        vdisplay_rnd = vdisplay / 2;
    else
        vdisplay_rnd = vdisplay;

    /* find the top & bottom borders */
    vmargin = 0;
    if(margins)
        vmargin = vdisplay_rnd * CVT_MARGIN_PERCENTAGE / 1000;

    pmode->vdisplay = (disp_u16_t)(vdisplay + 2 * vmargin);

    /* Interlaced */
    if(interlaced)
        interlace = 1;
    else
        interlace = 0;

    /* Determine VSync Width from aspect ratio */
    if(!(vdisplay % 3) && ((vdisplay * 4 / 3) == hdisplay))
        vsync = 4;
    else if(!(vdisplay % 9) && ((vdisplay * 16 / 9) == hdisplay))
        vsync = 5;
    else if(!(vdisplay % 10) && ((vdisplay * 16 / 10) == hdisplay))
        vsync = 6;
    else if(!(vdisplay % 4) && ((vdisplay * 5 / 4) == hdisplay))
        vsync = 7;
    else if(!(vdisplay % 9) && ((vdisplay * 15 / 9) == hdisplay))
        vsync = 7;
    else /* custom */
        vsync = 10;

    if(!reduced) 
    {
        /* simplify the GTF calculation */
        /* 4) Minimum time of vertical sync + back porch interval (µs)
         * default 550.0
         */
        disp_s32_t tmp1, tmp2;
#define CVT_MIN_VSYNC_BP    550
        /* 3) Nominal HSync width (% of line period) - default 8 */
#define CVT_HSYNC_PERCENTAGE    8
        disp_u32_t hblank_percentage;
        disp_s32_t vsyncandback_porch, vback_porch, hblank;

        /* estimated the horizontal period */
        tmp1 = HV_FACTOR * 1000000  -
                CVT_MIN_VSYNC_BP * HV_FACTOR * vfieldrate;
        tmp2 = (vdisplay_rnd + 2 * vmargin + CVT_MIN_V_PORCH) * 2 +
                interlace;
        hperiod = tmp1 * 2 / (tmp2 * vfieldrate);

        tmp1 = CVT_MIN_VSYNC_BP * HV_FACTOR / hperiod + 1;
        /* 9. Find number of lines in sync + backporch */
        if(tmp1 < (vsync + CVT_MIN_V_PORCH))
            vsyncandback_porch = vsync + CVT_MIN_V_PORCH;
        else
            vsyncandback_porch = tmp1;
        /* 10. Find number of lines in back porch */
        vback_porch = vsyncandback_porch - vsync;
        pmode->vtotal = (disp_u16_t)(vdisplay_rnd + 2 * vmargin +
                vsyncandback_porch + CVT_MIN_V_PORCH);
        /* 5) Definition of Horizontal blanking time limitation */
        /* Gradient (%/kHz) - default 600 */
#define CVT_M_FACTOR    600
        /* Offset (%) - default 40 */
#define CVT_C_FACTOR    40
        /* Blanking time scaling factor - default 128 */
#define CVT_K_FACTOR    128
        /* Scaling factor weighting - default 20 */
#define CVT_J_FACTOR    20
#define CVT_M_PRIME    (CVT_M_FACTOR * CVT_K_FACTOR / 256)
#define CVT_C_PRIME    ((CVT_C_FACTOR - CVT_J_FACTOR) * CVT_K_FACTOR / 256 + \
             CVT_J_FACTOR)
        /* 12. Find ideal blanking duty cycle from formula */
        hblank_percentage = CVT_C_PRIME * HV_FACTOR - CVT_M_PRIME *
                    hperiod / 1000;
        /* 13. Blanking time */
        if(hblank_percentage < 20 * HV_FACTOR)
            hblank_percentage = 20 * HV_FACTOR;
        hblank = pmode->hdisplay * hblank_percentage /
             (100 * HV_FACTOR - hblank_percentage);
        hblank -= hblank % (2 * CVT_H_GRANULARITY);
        /* 14. find the total pixels per line */
        pmode->htotal = (disp_u16_t)(pmode->hdisplay + hblank);
        pmode->hsync_end = pmode->hdisplay + (disp_u16_t)hblank / 2;
        pmode->hsync_start = pmode->hsync_end -
            (pmode->htotal * CVT_HSYNC_PERCENTAGE) / 100;
        pmode->hsync_start += CVT_H_GRANULARITY -
            pmode->hsync_start % CVT_H_GRANULARITY;
        /* fill the Vsync values */
        pmode->vsync_start = pmode->vdisplay + CVT_MIN_V_PORCH;
        pmode->vsync_end = pmode->vsync_start + vsync;
    } 
    else 
    {
        /* Reduced blanking */
        /* Minimum vertical blanking interval time (µs)- default 460 */
#define CVT_RB_MIN_VBLANK    460
        /* Fixed number of clocks for horizontal sync */
#define CVT_RB_H_SYNC        32
        /* Fixed number of clocks for horizontal blanking */
#define CVT_RB_H_BLANK        160
        /* Fixed number of lines for vertical front porch - default 3*/
#define CVT_RB_VFPORCH        3
        disp_s32_t vbilines;
        disp_s32_t tmp1, tmp2;
        /* 8. Estimate Horizontal period. */
        tmp1 = HV_FACTOR * 1000000 -
            CVT_RB_MIN_VBLANK * HV_FACTOR * vfieldrate;
        tmp2 = vdisplay_rnd + 2 * vmargin;
        hperiod = tmp1 / (tmp2 * vfieldrate);
        /* 9. Find number of lines in vertical blanking */
        vbilines = CVT_RB_MIN_VBLANK * HV_FACTOR / hperiod + 1;
        /* 10. Check if vertical blanking is sufficient */
        if(vbilines < (CVT_RB_VFPORCH + vsync + CVT_MIN_V_BPORCH))
            vbilines = CVT_RB_VFPORCH + vsync + CVT_MIN_V_BPORCH;
        /* 11. Find total number of lines in vertical field */
        pmode->vtotal = (disp_u16_t)(vdisplay_rnd + 2 * vmargin + vbilines);
        /* 12. Find total number of pixels in a line */
        pmode->htotal = pmode->hdisplay + CVT_RB_H_BLANK;
        /* Fill in HSync values */
        pmode->hsync_end = pmode->hdisplay + CVT_RB_H_BLANK / 2;
        pmode->hsync_start = pmode->hsync_end - CVT_RB_H_SYNC;
        /* Fill in VSync values */
        pmode->vsync_start = pmode->vdisplay + CVT_RB_VFPORCH;
        pmode->vsync_end = pmode->vsync_start + vsync;
    }
    /* 15/13. Find pixel clock frequency (kHz for xf86) */
    tmp = pmode->htotal; /* perform intermediate calcs in u64 */
    tmp *= HV_FACTOR * 1000;

//    do_div(tmp, hperiod);
    tmp = tmp/hperiod;
    tmp -= pmode->clock % CVT_CLOCK_STEP;
    pmode->clock = (disp_s32_t)tmp;
    /* 18/16. Find actual vertical frame frequency */
    /* ignore - just set the mode flag for interlaced */
    if(interlaced) 
    {
        pmode->vtotal *= 2;
        pmode->flags |= DISP_MODE_FLAG_INTERLACE;
    }
    /* Fill the mode line name */
    disp_mode_set_name(pmode);
    if(reduced)
        pmode->flags |= (DISP_MODE_FLAG_PHSYNC |
                    DISP_MODE_FLAG_NVSYNC);
    else
        pmode->flags |= (DISP_MODE_FLAG_PVSYNC |
                    DISP_MODE_FLAG_NHSYNC);

    return DISP_TRUE;
}

/**
 * disp_gtf_mode_complex - create the modeline based on the full GTF algorithm
 * @dev: drm device
 * @hdisplay: hdisplay size
 * @vdisplay: vdisplay size
 * @vrefresh: vrefresh rate.
 * @interlaced: whether to compute an interlaced mode
 * @margins: desired margin (borders) size
 * @GTF_M: extended GTF formula parameters
 * @GTF_2C: extended GTF formula parameters
 * @GTF_K: extended GTF formula parameters
 * @GTF_2J: extended GTF formula parameters
 *
 * GTF feature blocks specify C and J in multiples of 0.5, so we pass them
 * in here multiplied by two.  For a C of 40, pass in 80.
 *
 * Returns:
 * The modeline based on the full GTF algorithm stored in a disp_display_mode_t object.
 * The display mode object is allocated with disp_mode_create(). Returns DISP_NULL
 * when no mode could be allocated.
 */
disp_u8_t disp_gtf_mode_complex(disp_s32_t hdisplay, disp_s32_t vdisplay,
             disp_s32_t vrefresh, disp_u8_t interlaced, disp_s32_t margins,
             disp_s32_t GTF_M, disp_s32_t GTF_2C, disp_s32_t GTF_K, disp_s32_t GTF_2J, pdisp_display_mode_t pdisp_mode)
{    /* 1) top/bottom margin size (% of height) - default: 1.8, */
#define    GTF_MARGIN_PERCENTAGE        18
    /* 2) character cell horizontal granularity (pixels) - default 8 */
#define    GTF_CELL_GRAN            8
    /* 3) Minimum vertical porch (lines) - default 3 */
#define    GTF_MIN_V_PORCH            1
    /* width of vsync in lines */
#define V_SYNC_RQD            3
    /* width of hsync as % of total line */
#define H_SYNC_PERCENT            8
    /* min time of vsync + back porch (microsec) */
#define MIN_VSYNC_PLUS_BP        550
    /* C' and M' are part of the Blanking Duty Cycle computation */
#define GTF_C_PRIME    ((((GTF_2C - GTF_2J) * GTF_K / 256) + GTF_2J) / 2)
#define GTF_M_PRIME    (GTF_K * GTF_M / 256)
    disp_u32_t hdisplay_rnd, vdisplay_rnd, vfieldrate_rqd;
    disp_s32_t top_margin, bottom_margin;
    disp_s32_t interlace;
    disp_u32_t hfreq_est;
    disp_s32_t vsync_plus_bp, vback_porch;
    disp_u32_t vtotal_lines, vfieldrate_est;
    disp_u32_t hperiod;
    disp_u32_t vfield_rate, vframe_rate;
    disp_s32_t left_margin, right_margin;
    disp_u32_t total_active_pixels, ideal_duty_cycle;
    disp_u32_t hblank, total_pixels, pixel_freq;
    disp_s32_t hsync, hfront_porch, vodd_front_porch_lines;
    disp_u32_t tmp1, tmp2;

    if(!hdisplay || !vdisplay)
        return DISP_FALSE;

    /* 1. In order to give correct results, the number of horizontal
     * pixels requested is first processed to ensure that it is divisible
     * by the character size, by rounding it to the nearest character
     * cell boundary:
     */
    hdisplay_rnd = (hdisplay + GTF_CELL_GRAN / 2) / GTF_CELL_GRAN;
    hdisplay_rnd = hdisplay_rnd * GTF_CELL_GRAN;

    /* 2. If interlace is requested, the number of vertical lines assumed
     * by the calculation must be halved, as the computation calculates
     * the number of vertical lines per field.
     */
    if(interlaced)
        vdisplay_rnd = vdisplay / 2;
    else
        vdisplay_rnd = vdisplay;

    /* 3. Find the frame rate required: */
    if(interlaced)
        vfieldrate_rqd = vrefresh * 2;
    else
        vfieldrate_rqd = vrefresh;

    /* 4. Find number of lines in Top margin: */
    top_margin = 0;
    if(margins)
        top_margin = (vdisplay_rnd * GTF_MARGIN_PERCENTAGE + 500) /
                1000;
    /* 5. Find number of lines in bottom margin: */
    bottom_margin = top_margin;

    /* 6. If interlace is required, then set variable interlace: */
    if(interlaced)
        interlace = 1;
    else
        interlace = 0;

    /* 7. Estimate the Horizontal frequency */
    {
        tmp1 = (1000000  - MIN_VSYNC_PLUS_BP * vfieldrate_rqd) / 500;
        tmp2 = (vdisplay_rnd + 2 * top_margin + GTF_MIN_V_PORCH) *
                2 + interlace;
        hfreq_est = (tmp2 * 1000 * vfieldrate_rqd) / tmp1;
    }

    /* 8. Find the number of lines in V sync + back porch */
    /* [V SYNC+BP] = RINT(([MIN VSYNC+BP] * hfreq_est / 1000000)) */
    vsync_plus_bp = MIN_VSYNC_PLUS_BP * hfreq_est / 1000;
    vsync_plus_bp = (vsync_plus_bp + 500) / 1000;
    /*  9. Find the number of lines in V back porch alone: */
    vback_porch = vsync_plus_bp - V_SYNC_RQD;
    /*  10. Find the total number of lines in Vertical field period: */
    vtotal_lines = vdisplay_rnd + top_margin + bottom_margin +
            vsync_plus_bp + GTF_MIN_V_PORCH;
    /*  11. Estimate the Vertical field frequency: */
    vfieldrate_est = hfreq_est / vtotal_lines;
    /*  12. Find the actual horizontal period: */
    hperiod = 1000000 / (vfieldrate_rqd * vtotal_lines);

    /*  13. Find the actual Vertical field frequency: */
    vfield_rate = hfreq_est / vtotal_lines;
    /*  14. Find the Vertical frame frequency: */
    if(interlaced)
        vframe_rate = vfield_rate / 2;
    else
        vframe_rate = vfield_rate;
    /*  15. Find number of pixels in left margin: */
    if(margins)
        left_margin = (hdisplay_rnd * GTF_MARGIN_PERCENTAGE + 500) /
                1000;
    else
        left_margin = 0;

    /* 16.Find number of pixels in right margin: */
    right_margin = left_margin;
    /* 17.Find total number of active pixels in image and left and right */
    total_active_pixels = hdisplay_rnd + left_margin + right_margin;
    /* 18.Find the ideal blanking duty cycle from blanking duty cycle */
    ideal_duty_cycle = GTF_C_PRIME * 1000 -
                (GTF_M_PRIME * 1000000 / hfreq_est);
    /* 19.Find the number of pixels in the blanking time to the nearest
     * double character cell: */
    hblank = total_active_pixels * ideal_duty_cycle /
            (100000 - ideal_duty_cycle);
    hblank = (hblank + GTF_CELL_GRAN) / (2 * GTF_CELL_GRAN);
    hblank = hblank * 2 * GTF_CELL_GRAN;
    /* 20.Find total number of pixels: */
    total_pixels = total_active_pixels + hblank;
    /* 21.Find pixel clock frequency: */
    pixel_freq = total_pixels * hfreq_est / 1000;
    /* Stage 1 computations are now complete; I should really pass
     * the results to another function and do the Stage 2 computations,
     * but I only need a few more values so I'll just append the
     * computations here for now */
    /* 17. Find the number of pixels in the horizontal sync period: */
    hsync = H_SYNC_PERCENT * total_pixels / 100;
    hsync = (hsync + GTF_CELL_GRAN / 2) / GTF_CELL_GRAN;
    hsync = hsync * GTF_CELL_GRAN;
    /* 18. Find the number of pixels in horizontal front porch period */
    hfront_porch = hblank / 2 - hsync;
    /*  36. Find the number of lines in the odd front porch period: */
    vodd_front_porch_lines = GTF_MIN_V_PORCH ;

    /* finally, pack the results in the mode struct */
    pdisp_mode->hdisplay = (disp_u16_t)hdisplay_rnd;
    pdisp_mode->hsync_start = (disp_u16_t)(hdisplay_rnd + hfront_porch);
    pdisp_mode->hsync_end = (disp_u16_t)(pdisp_mode->hsync_start + hsync);
    pdisp_mode->htotal = (disp_u16_t)total_pixels;
    pdisp_mode->vdisplay = (disp_u16_t)vdisplay_rnd;
    pdisp_mode->vsync_start = (disp_u16_t)(vdisplay_rnd + vodd_front_porch_lines);
    pdisp_mode->vsync_end = pdisp_mode->vsync_start + V_SYNC_RQD;
    pdisp_mode->vtotal = (disp_u16_t)vtotal_lines;

    pdisp_mode->clock = pixel_freq;

    if(interlaced)
    {
        pdisp_mode->vtotal *= 2;
        pdisp_mode->flags |= DISP_MODE_FLAG_INTERLACE;
    }

    disp_mode_set_name(pdisp_mode);
    if(GTF_M == 600 && GTF_2C == 80 && GTF_K == 128 && GTF_2J == 40)
        pdisp_mode->flags = DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_PVSYNC;
    else
        pdisp_mode->flags = DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_NVSYNC;

    return DISP_TRUE;
}

/**
 * disp_gtf_mode - create the modeline based on the GTF algorithm
 * @dev: drm device
 * @hdisplay: hdisplay size
 * @vdisplay: vdisplay size
 * @vrefresh: vrefresh rate.
 * @interlaced: whether to compute an interlaced mode
 * @margins: desired margin (borders) size
 *
 * return the modeline based on GTF algorithm
 *
 * This function is to create the modeline based on the GTF algorithm.
 * Generalized Timing Formula is derived from:
 *
 *    GTF Spreadsheet by Andy Morrish (1/5/97)
 *    available at https://www.vesa.org
 *
 * And it is copied from the file of xserver/hw/xfree86/modes/xf86gtf.c.
 * What I have done is to translate it by using integer calculation.
 * I also refer to the function of fb_get_mode in the file of
 * drivers/video/fbmon.c
 *
 * Standard GTF parameters::
 *
 *     M = 600
 *     C = 40
 *     K = 128
 *     J = 20
 *
 * Returns:
 * The modeline based on the GTF algorithm stored in a disp_display_mode_t object.
 * The display mode object is allocated with disp_mode_create(). Returns DISP_NULL
 * when no mode could be allocated.
 */
disp_u8_t disp_gtf_mode(disp_s32_t hdisplay, disp_s32_t vdisplay, disp_s32_t vrefresh,
    disp_u8_t interlaced, disp_s32_t margins, pdisp_display_mode_t pmode)
{
    return disp_gtf_mode_complex(hdisplay, vdisplay, vrefresh,
        interlaced, margins, 600, 40 * 2, 128, 20 * 2, pmode);
}

/**
 * disp_mode_set_name - set the name on a mode
 * @mode: name will be set in this mode
 *
 * Set the name of @mode to a standard format which is <hdisplay>x<vdisplay>
 * with an optional 'i' suffix for interlaced modes.
 */
disp_void_t disp_mode_set_name(pdisp_display_mode_t pmode)
{
    disp_u8_t interlaced = !!(pmode->flags & DISP_MODE_FLAG_INTERLACE);

    disp_snprintf(pmode->name, DISP_DISPLAY_MODE_LEN, "%dx%d%s",
         pmode->hdisplay, pmode->vdisplay,
         interlaced ? "i" : "");
}

static inline disp_u64_t mul_u32_u32(disp_u32_t a, disp_u32_t b)
{
    return (disp_u64_t)a * (disp_u64_t)b;
}
#if 0 // Win10 & Qilin V10 compile error for asm
static inline disp_u64_t mul_u32_u32(disp_u32_t a, disp_u32_t b)
{
    disp_u32_t high, low;

    asm ("mull %[b]" : "=a" (low), "=d" (high)
             : [a] "a" (a), [b] "rm" (b) );

    return low | ((disp_u64_t)high) << 32;
}
#endif

/**
 * disp_mode_vrefresh - get the vrefresh of a mode
 * @mode: mode
 *
 * Returns:
 * @modes's vrefresh rate in Hz, rounded to the nearest integer. Calculates the
 * value first if it is not yet set.
 */
disp_s32_t disp_mode_vrefresh(pdisp_display_mode_t pmode)
{
    disp_u32_t num, den;

    if(pmode->htotal == 0 || pmode->vtotal == 0)
        return 0;

    num = pmode->clock;
    den = pmode->htotal * pmode->vtotal;

    if(pmode->flags & DISP_MODE_FLAG_INTERLACE)
        num *= 2;
    if(pmode->flags & DISP_MODE_FLAG_DBLSCAN)
        den *= 2;
    if(pmode->vscan > 1)
        den *= pmode->vscan;

    return (disp_s32_t)DIV_ROUND_CLOSEST_ULL(mul_u32_u32(num, 1000), den);
}

static disp_u8_t disp_mode_match_timings(pdisp_display_mode_t pmode1,
                   pdisp_display_mode_t pmode2)
{
    return pmode1->hdisplay == pmode2->hdisplay &&
        pmode1->hsync_start == pmode2->hsync_start &&
        pmode1->hsync_end == pmode2->hsync_end &&
        pmode1->htotal == pmode2->htotal &&
        pmode1->hskew == pmode2->hskew &&
        pmode1->vdisplay == pmode2->vdisplay &&
        pmode1->vsync_start == pmode2->vsync_start &&
        pmode1->vsync_end == pmode2->vsync_end &&
        pmode1->vtotal == pmode2->vtotal &&
        pmode1->vscan == pmode2->vscan;
}

static disp_u8_t disp_mode_match_clock(pdisp_display_mode_t pmode1,
                  pdisp_display_mode_t pmode2)
{
    /*
     * do clock check convert to PICOS
     * so fb modes get matched the same
     */
    if(pmode1->clock && pmode2->clock)
        return KHZ2PICOS(pmode1->clock) == KHZ2PICOS(pmode2->clock);
    else
        return pmode1->clock == pmode2->clock;
}

static disp_u8_t disp_mode_match_flags(pdisp_display_mode_t pmode1,
                 pdisp_display_mode_t pmode2)
{
    return (pmode1->flags & ~DISP_MODE_FLAG_3D_MASK) ==
        (pmode2->flags & ~DISP_MODE_FLAG_3D_MASK);
}

static disp_u8_t disp_mode_match_3d_flags(pdisp_display_mode_t pmode1,
                    pdisp_display_mode_t pmode2)
{
    return (pmode1->flags & DISP_MODE_FLAG_3D_MASK) ==
        (pmode2->flags & DISP_MODE_FLAG_3D_MASK);
}

static disp_u8_t disp_mode_match_aspect_ratio(pdisp_display_mode_t pmode1,
                    pdisp_display_mode_t pmode2)
{
    return pmode1->picture_aspect_ratio == pmode2->picture_aspect_ratio;
}

/**
 * disp_mode_match - test modes for(partial) equality
 * @mode1: first mode
 * @mode2: second mode
 * @match_flags: which parts need to match (DISP_MODE_MATCH_*)
 *
 * Check to see if @mode1 and @mode2 are equivalent.
 *
 * Returns:
 * True if the modes are (partially) equal, false otherwise.
 */
disp_u8_t disp_mode_match(pdisp_display_mode_t pmode1,
            pdisp_display_mode_t pmode2,
            disp_u32_t match_flags)
{
    if(!pmode1 && !pmode2)
        return DISP_TRUE;

    if(!pmode1 || !pmode2)
        return DISP_FALSE;

    if(match_flags & DISP_MODE_MATCH_TIMINGS &&
        !disp_mode_match_timings(pmode1, pmode2))
        return DISP_FALSE;

    if(match_flags & DISP_MODE_MATCH_CLOCK &&
        !disp_mode_match_clock(pmode1, pmode2))
        return DISP_FALSE;

    if(match_flags & DISP_MODE_MATCH_FLAGS &&
        !disp_mode_match_flags(pmode1, pmode2))
        return DISP_FALSE;

    if(match_flags & DISP_MODE_MATCH_3D_FLAGS &&
        !disp_mode_match_3d_flags(pmode1, pmode2))
        return DISP_FALSE;

    if(match_flags & DISP_MODE_MATCH_ASPECT_RATIO &&
        !disp_mode_match_aspect_ratio(pmode1, pmode2))
        return DISP_FALSE;

    return DISP_TRUE;
}

/**
 * disp_mode_equal - test modes for equality
 * @mode1: first mode
 * @mode2: second mode
 *
 * Check to see if @mode1 and @mode2 are equivalent.
 *
 * Returns:
 * True if the modes are equal, false otherwise.
 */
disp_u8_t disp_mode_equal(pdisp_display_mode_t pmode1,
            pdisp_display_mode_t pmode2)
{
    return disp_mode_match(pmode1, pmode2,
                  DISP_MODE_MATCH_TIMINGS |
                  DISP_MODE_MATCH_CLOCK |
                  DISP_MODE_MATCH_FLAGS |
                  DISP_MODE_MATCH_3D_FLAGS|
                  DISP_MODE_MATCH_ASPECT_RATIO);
}

/**
 * disp_mode_equal_no_clocks - test modes for equality
 * @mode1: first mode
 * @mode2: second mode
 *
 * Check to see if @mode1 and @mode2 are equivalent, but
 * don't check the pixel clocks.
 *
 * Returns:
 * True if the modes are equal, false otherwise.
 */
disp_u8_t disp_mode_equal_no_clocks(pdisp_display_mode_t pmode1,
                  pdisp_display_mode_t pmode2)
{
    return disp_mode_match(pmode1, pmode2,
                  DISP_MODE_MATCH_TIMINGS |
                  DISP_MODE_MATCH_FLAGS |
                  DISP_MODE_MATCH_3D_FLAGS);
}

/**
 * disp_mode_equal_no_clocks_no_stereo - test modes for equality
 * @mode1: first mode
 * @mode2: second mode
 *
 * Check to see if @mode1 and @mode2 are equivalent, but
 * don't check the pixel clocks nor the stereo layout.
 *
 * Returns:
 * True if the modes are equal, false otherwise.
 */
disp_u8_t disp_mode_equal_no_clocks_no_stereo(pdisp_display_mode_t pmode1,
                    pdisp_display_mode_t pmode2)
{
    return disp_mode_match(pmode1, pmode2,
                  DISP_MODE_MATCH_TIMINGS |
                  DISP_MODE_MATCH_FLAGS);
}

static DISP_MODE_STATUS
disp_mode_validate_basic(pdisp_display_mode_t pmode)
{
    if(pmode->type & ~DISP_MODE_TYPE_ALL)
        return MODE_BAD;

    if(pmode->flags & ~DISP_MODE_FLAG_ALL)
        return MODE_BAD;

    if((pmode->flags & DISP_MODE_FLAG_3D_MASK) > DISP_MODE_FLAG_3D_MAX)
        return MODE_BAD;

    if(pmode->clock == 0)
        return MODE_CLOCK_LOW;

    if(pmode->hdisplay == 0 ||
        pmode->hsync_start < pmode->hdisplay ||
        pmode->hsync_end < pmode->hsync_start ||
        pmode->htotal < pmode->hsync_end)
        return MODE_H_ILLEGAL;

    if(pmode->vdisplay == 0 ||
        pmode->vsync_start < pmode->vdisplay ||
        pmode->vsync_end < pmode->vsync_start ||
        pmode->vtotal < pmode->vsync_end)
        return MODE_V_ILLEGAL;

    return MODE_OK;
}

/**
 * disp_mode_validate_ycbcr420 - add 'ycbcr420-only' modes only when allowed
 * @mode: mode to check
 * @connector: drm connector under action
 *
 * This function is a helper which can be used to filter out any YCBCR420
 * only mode, when the source doesn't support it.
 *
 * Returns:
 * The mode status
 */
DISP_MODE_STATUS
disp_mode_validate_ycbcr420(pdisp_display_mode_t pmode,
               pdisp_connector_t pdisp_connector)
{
    disp_u8_t vic = disp_match_cea_mode(pmode);
    DISP_MODE_STATUS status = MODE_OK;
    pdisp_hdmi_info_t phdmi = &pdisp_connector->disp_info.hdmi;

    disp_print(DDM_TRACE, "tofill %#x, %d\n", phdmi, vic);
//refine later
    //if(test_bit(vic, phdmi->y420_vdb_modes)) 
    //{
        //refine later
        //if(!pdisp_connector->ycbcr_420_allowed)
    //        status = MODE_NO_420;
    //}

    return status;
}

#define MODE_STATUS(status) [MODE_ ## status + 3] = #status

static pdisp_s8_t disp_mode_status_names[] = {
    MODE_STATUS(OK),
    MODE_STATUS(HSYNC),
    MODE_STATUS(VSYNC),
    MODE_STATUS(H_ILLEGAL),
    MODE_STATUS(V_ILLEGAL),
    MODE_STATUS(BAD_WIDTH),
    MODE_STATUS(NOMODE),
    MODE_STATUS(NO_INTERLACE),
    MODE_STATUS(NO_DBLESCAN),
    MODE_STATUS(NO_VSCAN),
    MODE_STATUS(MEM),
    MODE_STATUS(VIRTUAL_X),
    MODE_STATUS(VIRTUAL_Y),
    MODE_STATUS(MEM_VIRT),
    MODE_STATUS(NOCLOCK),
    MODE_STATUS(CLOCK_HIGH),
    MODE_STATUS(CLOCK_LOW),
    MODE_STATUS(CLOCK_RANGE),
    MODE_STATUS(BAD_HVALUE),
    MODE_STATUS(BAD_VVALUE),
    MODE_STATUS(BAD_VSCAN),
    MODE_STATUS(HSYNC_NARROW),
    MODE_STATUS(HSYNC_WIDE),
    MODE_STATUS(HBLANK_NARROW),
    MODE_STATUS(HBLANK_WIDE),
    MODE_STATUS(VSYNC_NARROW),
    MODE_STATUS(VSYNC_WIDE),
    MODE_STATUS(VBLANK_NARROW),
    MODE_STATUS(VBLANK_WIDE),
    MODE_STATUS(PANEL),
    MODE_STATUS(INTERLACE_WIDTH),
    MODE_STATUS(ONE_WIDTH),
    MODE_STATUS(ONE_HEIGHT),
    MODE_STATUS(ONE_SIZE),
    MODE_STATUS(NO_REDUCED),
    MODE_STATUS(NO_STEREO),
    MODE_STATUS(NO_420),
    MODE_STATUS(STALE),
    MODE_STATUS(BAD),
    MODE_STATUS(ERROR),
};

#undef MODE_STATUS

pdisp_s8_t disp_get_mode_status_name(DISP_MODE_STATUS status)
{
    disp_s32_t index = status + 3;

    if(index < 0 || index >= ARRAY_SIZE(disp_mode_status_names))
        return "";

    return disp_mode_status_names[index];
}

/**
 * disp_mode_compare - compare modes for favorability
 * @priv: unused
 * @lh_a: list_head for first mode
 * @lh_b: list_head for second mode
 *
 * Compare two modes, given by @lh_a and @lh_b, returning a value indicating
 * which is better.
 *
 * Returns:
 * Negative if @lh_a is better than @lh_b, zero if they're equivalent, or
 * positive if @lh_b is better than @lh_a.
 */
/***
static disp_s32_t disp_mode_compare(void *priv, struct list_head *lh_a,
                struct list_head *lh_b)
{
    //pdisp_display_mode_t pa = list_entry(lh_a, disp_display_mode_t, head);
    //pdisp_display_mode_t pb = list_entry(lh_b, disp_display_mode_t, head);
    disp_s32_t diff;

    //diff = ((b->type & DISP_MODE_TYPE_PREFERRED) != 0) -
    //    ((a->type & DISP_MODE_TYPE_PREFERRED) != 0);
    //if(diff)
    //    return diff;
    //diff = b->hdisplay * b->vdisplay - a->hdisplay * a->vdisplay;
    //if(diff)
    //    return diff;

    //diff = disp_mode_vrefresh(b) - disp_mode_vrefresh(a);
    //if(diff)
    //    return diff;

    //diff = b->clock - a->clock;
    return diff;
}
***/

/***
void disp_mode_sort(struct list_head *mode_list)
{
    //list_sort(DISP_NULL, mode_list, disp_mode_compare);
}
***/

disp_u8_t disp_mode_is_420_only(pdisp_display_info_t pdisplay,
              pdisp_display_mode_t pmode)
{
    disp_u8_t vic = disp_match_cea_mode(pmode);

    disp_print(DDM_TRACE, "tofill %#x, %u\n", pdisplay, vic);
//refine later
//    return test_bit(vic, pdisplay->hdmi.y420_vdb_modes);
    return 0;
}

disp_u8_t disp_mode_is_420_also(pdisp_display_info_t pdisplay,
              pdisp_display_mode_t pmode)
{
    disp_u8_t vic = disp_match_cea_mode(pmode);

    disp_print(DDM_TRACE, "tofill %#x, %u\n", pdisplay, vic);
//refine later
    //return test_bit(vic, pdisplay->hdmi.y420_cmdb_modes);
    return 0;
}

disp_u8_t disp_mode_is_420(pdisp_display_info_t pdisplay,
             pdisp_display_mode_t pmode)
{
    return disp_mode_is_420_only(pdisplay, pmode) ||
        disp_mode_is_420_also(pdisplay, pmode);
}
