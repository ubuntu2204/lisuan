/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#include "disp_dsc.h"
#include "disp_device.h"
#include "disp_reg.h"
#include "DSC_register.h"

#define disp_print(level, log, ...) disp_print_ex(DISP_PRINT_MASK_DSC, level, log, ##__VA_ARGS__)

#define DISP_DSC_MIN_HBLANK 1056

disp_s32_t disp_mode_vrefresh(pdisp_display_mode_t pmode);

static disp_u8_t dsc_pps_rc_8bpc_to_12bpp[DSC_PPS_RC_SIZE] = {
    0x03, 0x0C, 0x20, 0x00, 0x06, 0x0B, 0x0B, 0x33, 0x0E, 0x1C, 0x2A, 0x38, 0x46, 0x54, 0x62, 0x69,
    0x70, 0x77, 0x79, 0x7B, 0x7D, 0x7E, 0x00, 0x82, 0x01, 0x00, 0x09, 0x40, 0x09, 0xBE, 0x19, 0xFC,
    0x19, 0xFA, 0x19, 0xF8, 0x1A, 0x38, 0x1A, 0x38, 0x1A, 0x76, 0x2A, 0x76, 0x2A, 0x74, 0x2A, 0x74,
    0x3A, 0xB4, 0x52, 0xF4};
static disp_u8_t dsc_pps_rc_8bpc_to_10bpp[DSC_PPS_RC_SIZE] = {
    0x03, 0x0C, 0x20, 0x00, 0x06, 0x0B, 0x0B, 0x33, 0x0E, 0x1C, 0x2A, 0x38, 0x46, 0x54, 0x62, 0x69,
    0x70, 0x77, 0x79, 0x7B, 0x7D, 0x7E, 0x00, 0xC2, 0x01, 0x00, 0x09, 0x40, 0x11, 0xBE, 0x19, 0xFC,
    0x19, 0xFA, 0x19, 0xF8, 0x1A, 0x38, 0x1A, 0x78, 0x1A, 0x76, 0x2A, 0xB6, 0x2A, 0xB6, 0x2A, 0xF4,
    0x3A, 0xF4, 0x5B, 0x34};
static disp_u8_t dsc_pps_rc_8bpc_to_8bpp[DSC_PPS_RC_SIZE] = {
    0x03, 0x0C, 0x20, 0x00, 0x06, 0x0B, 0x0B, 0x33, 0x0E, 0x1C, 0x2A, 0x38, 0x46, 0x54, 0x62, 0x69,
    0x70, 0x77, 0x79, 0x7B, 0x7D, 0x7E, 0x01, 0x02, 0x01, 0x00, 0x09, 0x40, 0x09, 0xBE, 0x19, 0xFC,
    0x19, 0xFA, 0x19, 0xF8, 0x1A, 0x38, 0x1A, 0x78, 0x1A, 0xB6, 0x2A, 0xB6, 0x2A, 0xF4, 0x2A, 0xF4,
    0x4B, 0x34, 0x63, 0x74};
static disp_u8_t dsc_pps_rc_10bpc_to_12bpp[DSC_PPS_RC_SIZE] = {
    0x07, 0x10, 0x20, 0x00, 0x06, 0x0F, 0x0F, 0x33, 0x0E, 0x1C, 0x2A, 0x38, 0x46, 0x54, 0x62, 0x69,
    0x70, 0x77, 0x79, 0x7B, 0x7D, 0x7E, 0x00, 0x82, 0x11, 0x40, 0x19, 0xC0, 0x22, 0x3E, 0x32, 0x7C,
    0x3A, 0xBA, 0x3A, 0xF8, 0x3B, 0x38, 0x3B, 0x38, 0x3B, 0x76, 0x4B, 0x76, 0x4B, 0x74, 0x4B, 0x74,
    0x5B, 0xB4, 0x73, 0xF4};
static disp_u8_t dsc_pps_rc_10bpc_to_10bpp[DSC_PPS_RC_SIZE] = {
    0x07, 0x10, 0x20, 0x00, 0x06, 0x0F, 0x0F, 0x33, 0x0E, 0x1C, 0x2A, 0x38, 0x46, 0x54, 0x62, 0x69,
    0x70, 0x77, 0x79, 0x7B, 0x7D, 0x7E, 0x01, 0xC2, 0x22, 0x00, 0x2A, 0x40, 0x32, 0xBE, 0x3A, 0xFC,
    0x3A, 0xFA, 0x3A, 0xF8, 0x3B, 0x38, 0x3B, 0x78, 0x3B, 0x76, 0x4B, 0xB6, 0x4B, 0xB6, 0x4B, 0xF4,
    0x5B, 0xF4, 0x7C, 0x34};
static disp_u8_t dsc_pps_rc_10bpc_to_8bpp[DSC_PPS_RC_SIZE] = {
    0x07, 0x10, 0x20, 0x00, 0x06, 0x0F, 0x0F, 0x33, 0x0E, 0x1C, 0x2A, 0x38, 0x46, 0x54, 0x62, 0x69,
    0x70, 0x77, 0x79, 0x7B, 0x7D, 0x7E, 0x02, 0x02, 0x22, 0x00, 0x2A, 0x40, 0x2A, 0xBE, 0x3A, 0xFC,
    0x3A, 0xFA, 0x3A, 0xF8, 0x3B, 0x38, 0x3B, 0x78, 0x3B, 0xB6, 0x4B, 0xB6, 0x4B, 0xF4, 0x4B, 0xF4,
    0x6C, 0x34, 0x84, 0x74};

static disp_void_t dsc_hw_caps_init(pdisp_dsc_caps_t pdsc_hw_caps)
{
    pdsc_hw_caps->flag.bits.fec_caps = 1;
    pdsc_hw_caps->flag.bits.dsc_caps = 1;
    pdsc_hw_caps->flag.bits.dsc_ver_1_1 = 1;
    pdsc_hw_caps->flag.bits.dsc_ver_1_2 = 1;
    pdsc_hw_caps->flag.bits.dp = 1;
    pdsc_hw_caps->flag.bits.rgb = 1;
    pdsc_hw_caps->flag.bits.yuv_444 = 1;
    pdsc_hw_caps->flag.bits.bpc8 = 1;
    pdsc_hw_caps->flag.bits.bpc10 = 1;
    pdsc_hw_caps->flag.bits.slice_num_1 = 1;
    pdsc_hw_caps->flag.bits.slice_num_2 = 1;
    pdsc_hw_caps->flag.bits.slice_num_4 = 1;
    pdsc_hw_caps->flag.bits.slice_num_8 = 1;
    pdsc_hw_caps->max_linebuf_depth = 16;           // no limited, set to max
    pdsc_hw_caps->min_increment_of_bpp = 0;         // no limited, set to 1/16
    /* ref dsc spec:
     * The real physical rate buffer must be slightly larger than minRateBufferSize.
     *
     * For modes other than Native, an upper bound is:
     *   rc_model_size - initial_offset + ceil(initial_xmit_delay * bits_per_pixel) + groupsPerLine * first_line_bpg_offset
     *   2.82 or 3(KB) will be enough for now hex pps param, may dynamic calculate it in future.
     *
     * For Native modes, setting minRateBufferSize to:
     *   rc_model_size - initial_offset + maxOffset
     *   not support Native mode yet.
     */
    pdsc_hw_caps->min_rc_buf_size_kb = 3;
    pdsc_hw_caps->max_slice_width = 2560;
    pdsc_hw_caps->min_slice_height = 108;           // for quality, should more than 100 lines
    pdsc_hw_caps->max_throughput_mode0_mps = 600;   // DSC clk 200M, limited to 200*3
    pdsc_hw_caps->max_throughput_mode1_mps = 0;     // not support
}

static disp_void_t dsc_policy_init(disp_dsc_policy_t *pdsc_policy)
{
    pdsc_policy->flag.bits.dsc_crc = 0; // debug use
    /* sink could be compatible with both fec_org dp1.4 & dp1.4a
     * source default dp1.4a fec_org pattern
     * if sink show not right, check dpcd fec error state
     */
    pdsc_policy->flag.bits.fec_org_dp_1_4 = 0;
    pdsc_policy->flag.bits.use_min_slices_h = 1;
    pdsc_policy->flag.bits.prefer_fec = 0;
    pdsc_policy->flag.bits.prefer_dsc = 0;
    pdsc_policy->prefer_bpp = 12;
}

static disp_u8_t need_force_dsc(pdisp_dsc_t pdsc, pdisp_encoder_t pdisp_encoder, disp_u32_t clk_khz, disp_u32_t max_bandwidth)
{
    disp_u32_t req_bandwidth = 0, bpp = 0, bpp_fra = 0, bpp_fra_base = 1, increment_of_bpp = 0;
    disp_u8_t force_dsc = DISP_FALSE;

    if(DISP_COLOR_FORMAT_TYPE_YUV422 == pdisp_encoder->policy.color_format)
        bpp = pdisp_encoder->policy.bpc * 2;
    else if(DISP_COLOR_FORMAT_TYPE_YUV420 == pdisp_encoder->policy.color_format)
        bpp = pdisp_encoder->policy.bpc * 3 / 2;
    else
        bpp = pdisp_encoder->policy.bpc * 3;

    // judge normal dp bandwidth enough or not
    req_bandwidth = (clk_khz * bpp + 7) / 8;
    force_dsc = (req_bandwidth > max_bandwidth) ? DISP_TRUE : DISP_FALSE;

    // calculate dsc bpp
    bpp = pdsc->policy.prefer_bpp;
    req_bandwidth = (clk_khz * bpp + 7) / 8;
    req_bandwidth = DISP_FEC_BANDWIDTH(req_bandwidth);
    while(req_bandwidth > max_bandwidth)
    {
        bpp -= 2;
        req_bandwidth = (clk_khz * bpp + 7) / 8;
        req_bandwidth = DISP_FEC_BANDWIDTH(req_bandwidth);
    }
    pdsc->policy.bpp = (disp_u8_t)bpp;

    // calculate fraction bpp
    if(pdsc->policy.flag.bits.use_fraction_bpp && bpp < pdsc->policy.prefer_bpp)
    {
        // 1. check odd bpp first
        bpp = (clk_khz * (bpp + 1) / 8 <= max_bandwidth) ? bpp + 1 : bpp;
        // 2. calculate increment base
        increment_of_bpp = pdsc->policy.increment_of_bpp;
        while (increment_of_bpp)
        {
            bpp_fra_base *= 2;
            increment_of_bpp--;
        }
        // 3. add fraction bpp till bandwidth shortage
        req_bandwidth = (clk_khz * bpp + 7) / 8;
        req_bandwidth = DISP_FEC_BANDWIDTH(req_bandwidth);
        bpp_fra = 0;
        while (req_bandwidth < max_bandwidth && bpp_fra < 16)
        {
            bpp_fra += bpp_fra_base;
            req_bandwidth = (clk_khz * (bpp * 16 + bpp_fra) + 127) / 128;
            req_bandwidth = DISP_FEC_BANDWIDTH(req_bandwidth);
        }
        // 4. fallback to previous bpp_fra
        pdsc->policy.bpp_fraction = (disp_u8_t)(bpp_fra - bpp_fra_base);
    }

    if(pdsc->policy.bpp < 8)
    {
        disp_print(DDM_WARNING, "dp dsc, bpp[%u] too small, max_bandwidth[%u]\n", bpp, max_bandwidth);
    }

    return force_dsc;
}

static disp_u8_t dsc_policy_set_by_hw_and_sink(pdisp_dsc_t pdsc, pdisp_dsc_caps_t pdsc_sink)
{
    pdisp_dsc_caps_t pdsc_hw = (pdisp_dsc_caps_t)pdsc->phw_caps;
    disp_dsc_policy_t* pdsc_policy = &pdsc->policy;
    disp_u8_t max_linebuf_depth = 0, index = 0;
    disp_u8_t available_slice_count = 0;
    disp_u8_t available_slice_h[DSC_AVAILABLE_SLICES_SIZE];
    disp_u8_t min_slice_hor = 1, max_slice_hor = 1, max_slice_ver = 1;
    disp_u32_t max_throughput_mps = 0, max_slice_width = 0;
    DISP_COLOR_FORMAT_TYPE fmt = pdsc_policy->color_fmt;
    disp_u32_t bpc = pdsc_policy->bpc;
    disp_u16_t hdisplay = pdsc_policy->pic_width;
    disp_u16_t vdisplay = pdsc_policy->pic_height;
    disp_u32_t clk_khz = pdsc_policy->clk_khz;

    if (!(pdsc_sink->flag.bits.dsc_caps) || !(pdsc_hw->flag.bits.dsc_caps))
    {
        disp_print(DDM_TRACE, "dsc check failed, dsc not support!\n");
        return DISP_FALSE;
    }

    if (pdsc_hw->flag.bits.dsc_ver_1_2 && pdsc_sink->flag.bits.dsc_ver_1_2)
        pdsc_policy->dsc_ver_minor = 2;
    else if (pdsc_hw->flag.bits.dsc_ver_1_1 && pdsc_sink->flag.bits.dsc_ver_1_1)
        pdsc_policy->dsc_ver_minor = 1;
    else
    {
        disp_print(DDM_WARNING, "warning, no valid dsc version, hw-1.2-1.1[%u, %u], sink-1.2-1.1[%u, %u]\n",
            pdsc_hw->flag.bits.dsc_ver_1_2, pdsc_hw->flag.bits.dsc_ver_1_1,
            pdsc_sink->flag.bits.dsc_ver_1_2, pdsc_sink->flag.bits.dsc_ver_1_1);
        return DISP_FALSE;
    }

    if(pdsc_sink->min_rc_buf_size_kb < pdsc_hw->min_rc_buf_size_kb)
    {
        disp_print(DDM_TRACE, "dsc check failed, rc_buf_size_kb sink[%u] < hw[%u]\n",
            pdsc_sink->min_rc_buf_size_kb, pdsc_hw->min_rc_buf_size_kb);
        return DISP_FALSE;
    }

    if(pdsc_sink->flag.bits.slice_num_1 && pdsc_hw->flag.bits.slice_num_1)
        available_slice_h[available_slice_count++] = 1;
    if(pdsc_sink->flag.bits.slice_num_2 && pdsc_hw->flag.bits.slice_num_2)
        available_slice_h[available_slice_count++] = 2;
    if(pdsc_sink->flag.bits.slice_num_4 && pdsc_hw->flag.bits.slice_num_4)
        available_slice_h[available_slice_count++] = 4;
    if(pdsc_sink->flag.bits.slice_num_6 && pdsc_hw->flag.bits.slice_num_6)
        available_slice_h[available_slice_count++] = 6;
    if(pdsc_sink->flag.bits.slice_num_8 && pdsc_hw->flag.bits.slice_num_8)
        available_slice_h[available_slice_count++] = 8;
    if(pdsc_sink->flag.bits.slice_num_10 && pdsc_hw->flag.bits.slice_num_10)
        available_slice_h[available_slice_count++] = 10;
    if(pdsc_sink->flag.bits.slice_num_12 && pdsc_hw->flag.bits.slice_num_12)
        available_slice_h[available_slice_count++] = 12;
    if(pdsc_sink->flag.bits.slice_num_16 && pdsc_hw->flag.bits.slice_num_16)
        available_slice_h[available_slice_count++] = 16;
    if(pdsc_sink->flag.bits.slice_num_20 && pdsc_hw->flag.bits.slice_num_20)
        available_slice_h[available_slice_count++] = 20;
    if(pdsc_sink->flag.bits.slice_num_24 && pdsc_hw->flag.bits.slice_num_24)
        available_slice_h[available_slice_count++] = 24;
    if(!available_slice_count)
    {
        disp_print(DDM_TRACE, "dsc check failed, no available slice\n");
        return DISP_FALSE;
    }

    max_linebuf_depth = DISP_MIN(pdsc_hw->max_linebuf_depth, pdsc_sink->max_linebuf_depth);
    if(max_linebuf_depth < bpc + 1)
    {
        disp_print(DDM_TRACE, "dsc check failed, max_linebuf_depth[%u]\n", max_linebuf_depth);
        return DISP_FALSE;
    }
    else
        pdsc_policy->linebuf_depth = (disp_u8_t)(bpc + 1);

    pdsc_policy->flag.bits.block_prediction = (pdsc_sink->flag.bits.block_prediction
        && pdsc_hw->flag.bits.block_prediction) ? DISP_TRUE : DISP_FALSE;

    // DP_DSC_MAX_BITS_PER_PIXEL_LOW & HI, max bpp DP2.1 should support, not use yet

    switch(fmt)
    {
        case DISP_COLOR_FORMAT_TYPE_RGB:
            if(pdsc_sink->flag.bits.rgb && pdsc_hw->flag.bits.rgb)
                pdsc_policy->dsc_fmt = DISP_DSC_FMT_RGB;
            else
                return DISP_FALSE;
            break;
        case DISP_COLOR_FORMAT_TYPE_YUV444:
            if(pdsc_sink->flag.bits.yuv_444 && pdsc_hw->flag.bits.yuv_444)
                pdsc_policy->dsc_fmt = DISP_DSC_FMT_YUV_444;
            else
                return DISP_FALSE;
            break;
        case DISP_COLOR_FORMAT_TYPE_YUV422:
            if(pdsc_sink->flag.bits.yuv_native422 && pdsc_hw->flag.bits.yuv_native422)
                pdsc_policy->dsc_fmt = DISP_DSC_FMT_YUV_NATIVE422;
            else if(pdsc_sink->flag.bits.yuv_simple422 && pdsc_hw->flag.bits.yuv_simple422)
                pdsc_policy->dsc_fmt = DISP_DSC_FMT_YUV_SIMPLE422;
            else
                return DISP_FALSE;
            break;
        case DISP_COLOR_FORMAT_TYPE_YUV420:
            if(pdsc_sink->flag.bits.yuv_native420 && pdsc_hw->flag.bits.yuv_native420)
                pdsc_policy->dsc_fmt = DISP_DSC_FMT_YUV_NATIVE420;
            else
                return DISP_FALSE;
            break;
        default:
            return DISP_FALSE;
    }
    disp_print(DDM_TRACE, "dsc check fmt[%u] OK\n", fmt);

    switch(bpc)
    {
        case 8:
            if(!pdsc_sink->flag.bits.bpc8 || !pdsc_hw->flag.bits.bpc8)
                return DISP_FALSE;
            break;
        case 10:
            if(!pdsc_sink->flag.bits.bpc10 || !pdsc_hw->flag.bits.bpc10)
                return DISP_FALSE;
            break;
        case 12:
            if(!pdsc_sink->flag.bits.bpc12 || !pdsc_hw->flag.bits.bpc12)
                return DISP_FALSE;
            break;
        default:
            return DISP_FALSE;
    }
    disp_print(DDM_TRACE, "dsc check bpc[%u] OK\n", bpc);

    if(DISP_DSC_FMT_YUV_NATIVE422 == pdsc_policy->dsc_fmt || DISP_DSC_FMT_YUV_NATIVE420 == pdsc_policy->dsc_fmt)
        max_throughput_mps = DISP_MIN(pdsc_hw->max_throughput_mode1_mps, pdsc_sink->max_throughput_mode1_mps);
    else // RGB & yuv_simple
        max_throughput_mps = DISP_MIN(pdsc_hw->max_throughput_mode0_mps, pdsc_sink->max_throughput_mode0_mps);
    max_slice_width = DISP_MIN(pdsc_hw->max_slice_width, pdsc_sink->max_slice_width);
    pdsc_policy->increment_of_bpp = DISP_MAX(pdsc_hw->min_increment_of_bpp, pdsc_sink->min_increment_of_bpp);

    // slice fit
    for(index = available_slice_count - 1; index >= 0; --index)
    {
        max_slice_hor = available_slice_h[index];
        if (hdisplay % max_slice_hor == 0)
            break;
    }

    min_slice_hor = (disp_u8_t)(hdisplay / max_slice_width);
    if(hdisplay % max_slice_width != 0)
        ++min_slice_hor;
    for(index = 0; index < available_slice_count; ++index)
    {
        if(min_slice_hor <= available_slice_h[index])
        {
            min_slice_hor = available_slice_h[index];
            /* G100 max throughput is controversial:
             * 1. pixel clock is safe.
             * 2. hactive * vactive * vfresh is dangerous, may not enough to bring up.
             * 3. HW: we could save hblank part, but we must consume vblank part,
                    so theoretical value is: hactive * vtotal * vfresh.
             */
            if((hdisplay % min_slice_hor == 0) && (clk_khz / min_slice_hor <= max_throughput_mps * 1000))
                break;
        }
    }

    if(min_slice_hor > max_slice_hor)
    {
        disp_print(DDM_TRACE, "dsc check failed, min_slice[%u] > max_slice[%u]\n", min_slice_hor, max_slice_hor);
        return DISP_FALSE;
    }
    pdsc_policy->slice_num_hor = (pdsc_policy->flag.bits.use_min_slices_h) ? min_slice_hor : max_slice_hor;

    max_slice_ver = (disp_u8_t)(vdisplay / pdsc_hw->min_slice_height);
    if(vdisplay % pdsc_hw->min_slice_height != 0)
    {
        while(vdisplay % max_slice_ver)
            max_slice_ver--;
    }
    pdsc_policy->slice_num_ver = (disp_u8_t)max_slice_ver;

    /* Dsc_H_Delay need adjust by slice_h
     * 4/8 slice:   xmit_delay
     * 2 slice:     xmit_delay /= 2
     * 1 slice:     xmit_delay /= 4
     */
    if(pdsc_policy->slice_num_hor == 1)
        pdsc_policy->h_delay_denom = 4;
    else if(pdsc_policy->slice_num_hor == 2)
        pdsc_policy->h_delay_denom = 2;
    else
        pdsc_policy->h_delay_denom = 1;

    return DISP_TRUE;
}

static disp_u8_t check_fec_possible(pdisp_dsc_caps_t pdsc_hw, pdisp_dsc_caps_t pdsc_sink)
{
    return (pdsc_hw->flag.bits.fec_caps && pdsc_sink->flag.bits.fec_caps) ? DISP_TRUE : DISP_FALSE;
}

static disp_void_t dsc_policy_update(pdisp_dsc_t pdsc, disp_u32_t max_bandwidth)
{
    pdisp_device_t pdd = (pdisp_device_t)pdsc->disp_device;
    pdisp_connector_t pdisp_connector = DISP_NULL;
    pdisp_encoder_t pdisp_encoder = DISP_NULL;
    pdisp_crtc_t pdisp_crtc = DISP_NULL;
    disp_display_mode_t mode = {0};
    disp_u8_t fec_possible = DISP_FALSE, dsc_possible = DISP_FALSE, force_dsc = DISP_FALSE;

    if (!dsc_already_bind(pdsc))
    {
        disp_print(DDM_WARNING, "dsc[%s] unbind!\n", pdsc->index);
        return;
    }

    pdisp_connector = &pdd->disp_connector[pdsc->state.encoder_index];
    pdisp_encoder = &pdd->disp_encoder[pdsc->state.encoder_index];
    pdisp_crtc = &pdd->disp_crtc[pdsc->state.encoder_index];
    crtc_get_property_hw_mode(&pdisp_crtc->property, &mode);
    pdsc->policy.bpc = (disp_u8_t)pdisp_encoder->policy.bpc;
    pdsc->policy.color_fmt = pdisp_encoder->policy.color_format;
    pdsc->policy.pic_width = mode.hdisplay;
    pdsc->policy.pic_height = mode.vdisplay;
    pdsc->policy.clk_khz = (disp_u32_t)mode.clock;

    fec_possible =  check_fec_possible((pdisp_dsc_caps_t)pdsc->phw_caps, &pdisp_connector->disp_info.dsc_sink_caps);
    dsc_possible =  dsc_policy_set_by_hw_and_sink(pdsc, &pdisp_connector->disp_info.dsc_sink_caps);
    if(dsc_possible)
        force_dsc = need_force_dsc(pdsc, pdisp_encoder, pdsc->policy.clk_khz, max_bandwidth);

    if((pdsc->policy.flag.bits.prefer_fec || force_dsc) && fec_possible)
        pdsc->policy.flag.bits.fec_en = 1;
    else
        pdsc->policy.flag.bits.fec_en = 0;

    if(pdsc->policy.flag.bits.fec_en
        && dsc_possible
        && (force_dsc || pdsc->policy.flag.bits.prefer_dsc))
        pdsc->policy.flag.bits.dsc_en = 1;
    else
        pdsc->policy.flag.bits.dsc_en = 0;

    disp_print(DDM_TRACE, "dp dsc policy: flag[%#x], ver_minor[%u], slice_cnt_hv[%u, %u], linebuf[%u], bpp[%u], h_delay_den[%u], dsc_fmt[%d]\n",
        pdsc->policy.flag.value, pdsc->policy.dsc_ver_minor, pdsc->policy.slice_num_hor, pdsc->policy.slice_num_ver,
        pdsc->policy.linebuf_depth, pdsc->policy.bpp, pdsc->policy.h_delay_denom, pdsc->policy.dsc_fmt);
}

static disp_void_t disp_dsc_pps_dump(pdisp_dsc_t pdsc, disp_u32_t level)
{
    pdsc_pps_t pps = &pdsc->state.pps;
    disp_s32_t idx = 0;
    disp_u8_t *pps8 = (pdisp_u8_t)pps;

    disp_print(level, "dsc pps:\n");
    for(idx = 0; idx < 128; idx += 16)
    {
        disp_print(level, "  %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
            pps8[idx], pps8[idx+1], pps8[idx+2], pps8[idx+3], pps8[idx+4], pps8[idx+5], pps8[idx+6], pps8[idx+7],
            pps8[idx+8], pps8[idx+9], pps8[idx+10], pps8[idx+11], pps8[idx+12], pps8[idx+13], pps8[idx+14], pps8[idx+15]);
    }

    disp_print(level, "dsc pps config: ver[%u.%u], id[%u]\n", pps->version.major, pps->version.minor, pps->pps_identifier);
    disp_print(level, "\tflags: vbr[%u], simple422[%u], convert_rgb[%u], block_pred[%u], native422[%u], native420[%u]\n",
        pps->flag1.vbr_enable, pps->flag1.simple_422, pps->flag1.convert_rgb,
        pps->flag1.block_pred_enable, pps->flag2.native_422, pps->flag2.native_420);
    disp_print(level, "\tlinebuf_depth[%u], bpc[%u], bpp[%u+%u/16], picWH[%u, %u], sliceWH[%u, %u], chunk_size[%u]\n",
        pps->bpc.linebuf_depth, pps->bpc.bits_per_component, pps->flag1.bits_per_pixel_high * 16 + pps->bits_per_pixel / 16,
        pps->bits_per_pixel % 16, be16_to_u16(pps->pic_width), be16_to_u16(pps->pic_height),
        be16_to_u16(pps->slice_width), be16_to_u16(pps->slice_height), be16_to_u16(pps->chunk_size));
    disp_print(level, "\txmit_delay[%u], dec_delay[%u], scale_value[%u], increment_interval[%u], decrement_interval[%u]\n",
        be16_to_u16(pps->initial_xmit_delay), be16_to_u16(pps->initial_dec_delay), be16_to_u16(pps->initial_scale_value),
        be16_to_u16(pps->scale_increment_interval), be16_to_u16(pps->scale_decrement_interval));
    disp_print(level, "\tfirst_line_bpg_off[%u], nfl_bpg_off[%u], slice_bpg_off[%u], initial_off[%u], final_off[%u]\n",
        be16_to_u16(pps->first_line_bpg_offset), be16_to_u16(pps->nfl_bpg_offset), be16_to_u16(pps->slice_bpg_offset),
        be16_to_u16(pps->initial_offset), be16_to_u16(pps->final_offset));

    disp_print(level, "dsc pps rc: flatness_min_max[%u, %u]\n", pps->flatness_min_qp, pps->flatness_max_qp);
    disp_print(level, "\tmodel_size[%u], edge_factor[%u], quant_incr_limit0/1[%u, %u], tgt_offset_high/low[%u, %u]\n",
        be16_to_u16(pps->rc.model_size), pps->rc.edge_factor, pps->rc.quant_incr_limit0,
        pps->rc.quant_incr_limit1, pps->rc.tgt_offset.high, pps->rc.tgt_offset.low);
    disp_print(level, "pps rc buf_thresh(0x)[%x %x %x %x %x %x %x %x %x %x %x %x %x %x]\n",
        pps->rc.buf_thresh[0], pps->rc.buf_thresh[1], pps->rc.buf_thresh[2], pps->rc.buf_thresh[3], pps->rc.buf_thresh[4],
        pps->rc.buf_thresh[5], pps->rc.buf_thresh[6], pps->rc.buf_thresh[7], pps->rc.buf_thresh[8], pps->rc.buf_thresh[9],
        pps->rc.buf_thresh[10], pps->rc.buf_thresh[11], pps->rc.buf_thresh[12], pps->rc.buf_thresh[13]);
    disp_print(level, "pps rc range:\n");
    for(idx = 0; idx < 15; ++idx)
    {
        disp_print(level, "\t[%u]: min/max_qp[%u, %u], bpg_offset[%u]\n", idx, be16_to_u16(pps->rc.range[idx].min_qp),
            be16_to_u16(pps->rc.range[idx].max_qp), be16_to_u16(pps->rc.range[idx].bpg_offset));
    }
    disp_print(level, "pps: second_line_bpg_offset[%u], nsl_bpg_offset[%u], second_line_offset_adj[%u]\n",
        be16_to_u16(pps->second_line_bpg_offset), be16_to_u16(pps->nsl_bpg_offset), be16_to_u16(pps->second_line_offset_adj));
}

static disp_void_t dsc_pps_generate(pdisp_dsc_t pdsc)
{
    disp_dsc_policy_t* pdsc_policy = &pdsc->policy;
    pdsc_pps_t pdsc_pps = &pdsc->state.pps;
    disp_u8_t bpc, *pps8 = (pdisp_u8_t)pdsc_pps;
    disp_u16_t pic_height, pic_width, slice_height, slice_width, chunk_size, initial_dec_delay, final_offset,
        scale_increment_interval, scale_decrement_interval, slice_bpg_offset, initial_scale_value, nfl_bpg_offset,
        initial_xmit_delay, initial_offset, first_line_bpg_offset, slice_num_height, slice_num_width;
    disp_s32_t pixels_per_group, offset_fractional_bits, rc_model_size, final_scale, num_extra_mux_bits,
        groups_per_line, slice_bits, mux_word_size, num_ssps, uncompressed_bpg_rate, max_first_line_bpg_offset;
    disp_u32_t bppi, bppf, bpp16;
    //double bpp;
    enum DISP_DSC_FMT_TYPE dsc_fmt;
    disp_u32_t log_level = DDM_TRACE;

    /* ref: VESA DSC_model_20210623 algorithm, require floating-point operation
     * g100_HW support rgb & yuv444, 8/10 bpc, not support block_predication
     */
    bpc = pdsc->policy.bpc;
    dsc_fmt = pdsc_policy->dsc_fmt;
    pic_height = pdsc->policy.pic_height;
    pic_width = pdsc->policy.pic_width;
    slice_num_height = pdsc_policy->slice_num_ver;
    slice_num_width = pdsc_policy->slice_num_hor;
    //bpp = pdsc_policy->bpp + ((double)pdsc_policy->bpp_fraction / 16);
    bppi = pdsc_policy->bpp;
    bppf = pdsc_policy->bpp_fraction;
    bpp16 = bppi * 16 + bppf;

    // hex in DSC algorithm
    pixels_per_group = 3;
    offset_fractional_bits = 11;
    rc_model_size = 8192;

    slice_height = pic_height / slice_num_height;
    slice_width = pic_width / slice_num_width;
    //initial_xmit_delay = (disp_u16_t)(4096.0 / bpp + 0.5);
    initial_xmit_delay = (disp_u16_t)((4096 * 16 + 8) / bpp16);
    //chunk_size = (disp_u16_t)disp_ceil(slice_width * bpp / 8);
    chunk_size = (disp_u16_t)disp_ceil_div(bpp16 * slice_width, 8 * 16);
    slice_bits = 8 * chunk_size * slice_height;

    uncompressed_bpg_rate = (DISP_DSC_FMT_YUV_NATIVE422 == dsc_fmt)
        ? (3 * bpc * 4) : ((3 * bpc + ((DISP_DSC_FMT_RGB == dsc_fmt) ? 2 : 0)) * 3);
    //max_first_line_bpg_offset = (int)(uncompressed_bpg_rate - 3 * bpp);
    max_first_line_bpg_offset = (int)((uncompressed_bpg_rate * 16 - 3 * bpp16) / 16);
    //first_line_bpg_offset = (slice_height >= 8)
    //    ? (disp_u16_t)(12 + ((int)(0.09 * DISP_MIN(34, slice_height - 8)))) : (2 * (slice_height - 1));
    first_line_bpg_offset = (slice_height >= 8)
        ? (disp_u16_t)(12 + 9 * DISP_MIN(34, slice_height - 8) / 100) : (2 * (slice_height - 1));
    first_line_bpg_offset = (first_line_bpg_offset < 0) ? 0 : first_line_bpg_offset;
    first_line_bpg_offset = (first_line_bpg_offset > max_first_line_bpg_offset)
        ? (disp_u16_t)max_first_line_bpg_offset : first_line_bpg_offset;

    mux_word_size = (bpc <= 10) ? 48 : 64;
    num_ssps = (DISP_DSC_FMT_YUV_NATIVE422 == dsc_fmt) ? 4 : 3;
    if(DISP_DSC_FMT_RGB == dsc_fmt)
        num_extra_mux_bits = (num_ssps * (mux_word_size + (4 * bpc + 4) - 2));
    else if ((DISP_DSC_FMT_YUV_NATIVE422 != dsc_fmt))
        num_extra_mux_bits = (num_ssps * mux_word_size + (4 * bpc + 4) + 2 * (4 * bpc) - 2);
    else
        num_extra_mux_bits = (num_ssps * mux_word_size + (4 * bpc + 4) + 3 * (4 * bpc) - 2);
    while ((num_extra_mux_bits > 0) && ((slice_bits - num_extra_mux_bits) % mux_word_size))
        num_extra_mux_bits--;
    //final_offset = (disp_u16_t)(rc_model_size - ((initial_xmit_delay * (int)(bpp * 16) + 8) >> 4) + num_extra_mux_bits);
    final_offset = (disp_u16_t)(rc_model_size - ((bpp16 * initial_xmit_delay + 8) >> 4) + num_extra_mux_bits);
    final_scale = 8 * rc_model_size / (rc_model_size - final_offset);
    initial_offset = 5632;
    if(DISP_DSC_FMT_YUV_NATIVE422 == dsc_fmt)
    {
        if (bppi >= 16)
            initial_offset = 2048;
        else if (bppi >= 14)
            //initial_offset = (disp_u16_t)(5632 - (int)((bpp - 14.0) * 1792 + 0.5));
            initial_offset = (disp_u16_t)(30720 - bpp16 * 112);
        else if (bppi >= 12)
            initial_offset = 5632;
        else
        {
            disp_print(DDM_WARNING, "dsc, no auto-generated parameters available native422 bpp[%u+%u/16]\n",
                bppi, bppf);
        }
    }
    else
    {
        if (bppi >= 12)
            initial_offset = 2048;
        else if (bppi >= 10)
            //initial_offset = (disp_u16_t)(5632 - (int)((bpp - 10.0) * 1792 + 0.5));
            initial_offset = (disp_u16_t)(23552 - bpp16 * 112);
        else if (bppi >= 8)
            //initial_offset = (disp_u16_t)(6144 - (int)((bpp - 8.0) * 256 + 0.5));
            initial_offset = (disp_u16_t)(8192 - bpp16 * 16);
        else if (bppi >= 6)
            initial_offset = 6144;
    }
    initial_scale_value = (disp_u16_t)(8 * rc_model_size / (rc_model_size - initial_offset));
    groups_per_line = (slice_width + pixels_per_group - 1) / pixels_per_group;
    //nfl_bpg_offset = (disp_u16_t)disp_ceil((double)(first_line_bpg_offset << offset_fractional_bits) / (slice_height - 1));
    nfl_bpg_offset = (disp_u16_t)disp_ceil_div((disp_s64_t)first_line_bpg_offset << offset_fractional_bits, slice_height - 1);
    scale_decrement_interval = (disp_u16_t)(groups_per_line / (initial_scale_value - 8));
    //slice_bpg_offset = (disp_u16_t)disp_ceil((double)(1 << offset_fractional_bits) *
    //    (rc_model_size - initial_offset + num_extra_mux_bits) / (groups_per_line * slice_height));
    slice_bpg_offset = (disp_u16_t)disp_ceil_div(((disp_s64_t)1 << offset_fractional_bits) * (rc_model_size -
        initial_offset + num_extra_mux_bits), groups_per_line * slice_height);
    //scale_increment_interval = (disp_u16_t)((double)(1<<offset_fractional_bits) * final_offset /
    //    ((double)(final_scale - 9) * (nfl_bpg_offset + slice_bpg_offset)));
    scale_increment_interval = (disp_u16_t)(((disp_s64_t)1 << offset_fractional_bits) * final_offset /
        (((disp_s64_t)final_scale - 9) * ((disp_s64_t)nfl_bpg_offset + slice_bpg_offset)));
    //initial_dec_delay = (disp_u16_t)disp_ceil((double)(rc_model_size - initial_offset + initial_xmit_delay * bpp
    //    + groups_per_line * 15) / bpp) - initial_xmit_delay;
    initial_dec_delay = (disp_u16_t)disp_ceil_div(((disp_s64_t)rc_model_size * 16 - (disp_s64_t)initial_offset * 16 +
        (disp_s64_t)initial_xmit_delay * bpp16 + (disp_s64_t)groups_per_line * 240) - (disp_s64_t)initial_xmit_delay * bpp16, bpp16);

    disp_print(log_level, "dsc pps info: bpc[%u], dsc_fmt[%u], display_hv[%u, %u], slice_hv[%u, %u], chunk_size[%u]\n",
        bpc, dsc_fmt, pic_width, pic_height, slice_width, slice_height, chunk_size);
    disp_print(log_level, "ixd[%u], nemb[%u], fc[%u], fo[%u], \n",
        initial_xmit_delay, num_extra_mux_bits, final_offset, final_scale);
    disp_print(log_level, "gpl[%u], ngo[%u], sdi[%u], sbo[%u], sii[%u], idd[%u]\n",
        groups_per_line, nfl_bpg_offset, scale_decrement_interval, slice_bpg_offset, scale_increment_interval, initial_dec_delay);

    disp_memset(pdsc_pps, 0, sizeof(dsc_pps_t));
    pdsc_pps->version.major = 1;
    pdsc_pps->version.minor = pdsc_policy->dsc_ver_minor;
    pdsc_pps->pps_identifier = 0;
    pdsc_pps->bpc.bits_per_component = bpc;
    pdsc_pps->bpc.linebuf_depth = pdsc_policy->linebuf_depth; // bpc + 1
    pdsc_pps->flag1.block_pred_enable = pdsc_policy->flag.bits.block_prediction ? 1 : 0;
    pdsc_pps->flag1.convert_rgb = (DISP_DSC_FMT_RGB == dsc_fmt) ? 1 : 0;
    pdsc_pps->flag1.simple_422 = (DISP_DSC_FMT_YUV_SIMPLE422 == dsc_fmt) ? 1 : 0;
    pdsc_pps->flag1.vbr_enable = 0;
    pdsc_pps->flag1.bits_per_pixel_high = 0;
    pdsc_pps->bits_per_pixel = (disp_u8_t)bpp16;
    pdsc_pps->pic_height = u16_to_be16(pic_height);
    pdsc_pps->pic_width = u16_to_be16(pic_width);
    pdsc_pps->slice_height = u16_to_be16(slice_height);
    pdsc_pps->slice_width = u16_to_be16(slice_width);
    pdsc_pps->chunk_size = u16_to_be16(chunk_size);
    pdsc_pps->initial_xmit_delay = u16_to_be16(initial_xmit_delay);
    pdsc_pps->initial_dec_delay = u16_to_be16(initial_dec_delay);
    pdsc_pps->initial_scale_value = u16_to_be16(initial_scale_value);
    pdsc_pps->scale_increment_interval = u16_to_be16(scale_increment_interval);
    pdsc_pps->scale_decrement_interval = u16_to_be16(scale_decrement_interval);
    pdsc_pps->first_line_bpg_offset = u16_to_be16(first_line_bpg_offset);
    pdsc_pps->nfl_bpg_offset = u16_to_be16(nfl_bpg_offset);
    pdsc_pps->slice_bpg_offset = u16_to_be16(slice_bpg_offset);
    pdsc_pps->initial_offset = u16_to_be16(initial_offset);
    pdsc_pps->final_offset = u16_to_be16(final_offset);

    /* pps config ref: VESA DSC_model_20210623 runing result:
     * rc 8/10bpc  -> 8/10/12bpp
     */
    if(8 == bpc && bppi <= 8)
        disp_memcpy(pps8 + DSC_PPS_RC_OFFSET, dsc_pps_rc_8bpc_to_8bpp, DSC_PPS_RC_SIZE);
    else if(8 == bpc && bppi < 11)
        disp_memcpy(pps8 + DSC_PPS_RC_OFFSET, dsc_pps_rc_8bpc_to_10bpp, DSC_PPS_RC_SIZE);
    else if(8 == bpc && bppi < 13)
        disp_memcpy(pps8 + DSC_PPS_RC_OFFSET, dsc_pps_rc_8bpc_to_12bpp, DSC_PPS_RC_SIZE);
    else if(10 == bpc && bppi <= 8)
        disp_memcpy(pps8 + DSC_PPS_RC_OFFSET, dsc_pps_rc_10bpc_to_8bpp, DSC_PPS_RC_SIZE);
    else if(10 == bpc && bppi < 11)
        disp_memcpy(pps8 + DSC_PPS_RC_OFFSET, dsc_pps_rc_10bpc_to_10bpp, DSC_PPS_RC_SIZE);
    else if(10 == bpc && bppi < 13)
        disp_memcpy(pps8 + DSC_PPS_RC_OFFSET, dsc_pps_rc_10bpc_to_12bpp, DSC_PPS_RC_SIZE);
    else
    {
        disp_print(DDM_WARNING, "dsc_pps_generate, bpc[%u] -> bpp[%u+%u/16] not support\n",
            bpc, bppi, bppf);
    }

    disp_dsc_pps_dump(pdsc, log_level);

    return;
}

static disp_void_t dsc_module_reset(pdisp_dsc_t pdsc)
{
    pdisp_device_t pdd = pdsc->disp_device;
    disp_u32_t dsc_index = pdsc->index;

    dsc_write_field(dsc_index, Reg_Dsc_Reg0, Register_Dsc_Reg0_Start, 1,
        Dsc_Sw_Reset, 1);
    disp_delay_us(100);
    dsc_write_field(dsc_index, Reg_Dsc_Reg0, Register_Dsc_Reg0_Start, 1,
        Dsc_Sw_Reset, 0);
}

static disp_void_t dsc_module_flush(pdisp_dsc_t pdsc)
{
    pdisp_device_t pdd = pdsc->disp_device;
    disp_s32_t dsc_index = pdsc->index;
    pdsc_pps_t pdsc_pps = &pdsc->state.pps;
    pdisp_u32_t pps = (pdisp_u32_t)pdsc_pps;
    disp_u32_t h_delay = 0, v_delay = 0;

    dsc_module_reset(pdsc);

    /* HW do it in the future, now SW calculate it */
    h_delay = (disp_u32_t)be16_to_u16(pdsc_pps->initial_xmit_delay) / pdsc->policy.h_delay_denom;
    v_delay = DSC_REG0_DSC_V_DELAY_ONE_LINE;
    if(h_delay > (disp_u32_t)be16_to_u16(pdsc_pps->pic_width) / 4)
    {
        disp_print(DDM_WARNING, "DSC abnormal case v_delay need + 1\n");
        h_delay -= (disp_u32_t)be16_to_u16(pdsc_pps->pic_width) / 4;
        v_delay = DSC_REG0_DSC_V_DELAY_TWO_LINE;
    }
    disp_print(DDM_TRACE, "dsc, initial_xmit_delay[%#x], h_delay[%u], v_delay[%u], slice_width[%u]\n",
        pdsc_pps->initial_xmit_delay, h_delay, v_delay, be16_to_u16(pdsc_pps->pic_width));

    dsc_write_field(dsc_index, Reg_Dsc_Reg0, Register_Dsc_Reg0_Start, 9,
        Pps_Tx_Mod, DSC_REG0_PPS_TX_MOD_SEND_EVERY_FRAME,
        Pps_Tx_En, 1,
        Pps_Force_Tx, 1,
        Slice_Mod_Auto, 1,
        Full_Ich_Err_Pred, 0,
        Fifo_Full_Sel, 0,
        Dsc_V_Delay, v_delay,
        Dsc_Sel, pdsc->state.encoder_index,
        Dsc_H_Delay, h_delay);

    dsc_write_field(dsc_index, Reg_Dsc_Reg1, Register_Dsc_Reg1_Start, 1,
        Pps_0, pps[0]);
    dsc_write_field(dsc_index, Reg_Dsc_Reg2, Register_Dsc_Reg2_Start, 1,
        Pps_1, pps[1]);
    dsc_write_field(dsc_index, Reg_Dsc_Reg3, Register_Dsc_Reg3_Start, 1,
        Pps_2, pps[2]);
    dsc_write_field(dsc_index, Reg_Dsc_Reg4, Register_Dsc_Reg4_Start, 1,
        Pps_3, pps[3]);
    dsc_write_field(dsc_index, Reg_Dsc_Reg5, Register_Dsc_Reg5_Start, 1,
        Pps_4, pps[4]);
    dsc_write_field(dsc_index, Reg_Dsc_Reg6, Register_Dsc_Reg6_Start, 1,
        Pps_5, pps[5]);
    dsc_write_field(dsc_index, Reg_Dsc_Reg7, Register_Dsc_Reg7_Start, 1,
        Pps_6, pps[6]);
    dsc_write_field(dsc_index, Reg_Dsc_Reg8, Register_Dsc_Reg8_Start, 1,
        Pps_7, pps[7]);
    dsc_write_field(dsc_index, Reg_Dsc_Reg9, Register_Dsc_Reg9_Start, 1,
        Pps_8, pps[8]);
    dsc_write_field(dsc_index, Reg_Dsc_Reg0a, Register_Dsc_Reg0a_Start, 1,
        Pps_9, pps[9]);
    dsc_write_field(dsc_index, Reg_Dsc_Reg0b, Register_Dsc_Reg0b_Start, 1,
        Pps_10, pps[10]);
    dsc_write_field(dsc_index, Reg_Dsc_Reg0c, Register_Dsc_Reg0c_Start, 1,
        Pps_11, pps[11]);
    dsc_write_field(dsc_index, Reg_Dsc_Reg0d, Register_Dsc_Reg0d_Start, 1,
        Pps_12, pps[12]);
    dsc_write_field(dsc_index, Reg_Dsc_Reg0e, Register_Dsc_Reg0e_Start, 1,
        Pps_13, pps[13]);
    dsc_write_field(dsc_index, Reg_Dsc_Reg0f, Register_Dsc_Reg0f_Start, 1,
        Pps_14, pps[14]);
    dsc_write_field(dsc_index, Reg_Dsc_Reg10, Register_Dsc_Reg10_Start, 1,
        Pps_15, pps[15]);
    dsc_write_field(dsc_index, Reg_Dsc_Reg11, Register_Dsc_Reg11_Start, 1,
        Pps_16, pps[16]);
    dsc_write_field(dsc_index, Reg_Dsc_Reg12, Register_Dsc_Reg12_Start, 1,
        Pps_17, pps[17]);
    dsc_write_field(dsc_index, Reg_Dsc_Reg13, Register_Dsc_Reg13_Start, 1,
        Pps_18, pps[18]);
    dsc_write_field(dsc_index, Reg_Dsc_Reg14, Register_Dsc_Reg14_Start, 1,
        Pps_19, pps[19]);
    dsc_write_field(dsc_index, Reg_Dsc_Reg15, Register_Dsc_Reg15_Start, 1,
        Pps_20, pps[20]);
    dsc_write_field(dsc_index, Reg_Dsc_Reg16, Register_Dsc_Reg16_Start, 1,
        Pps_21, pps[21]);
    dsc_write_field(dsc_index, Reg_Dsc_Reg17, Register_Dsc_Reg17_Start, 1,
        Pps_22, pps[22]);
    dsc_write_field(dsc_index, Reg_Dsc_Reg18, Register_Dsc_Reg18_Start, 1,
        Pps_23, pps[23]);
    dsc_write_field(dsc_index, Reg_Dsc_Reg19, Register_Dsc_Reg19_Start, 1,
        Pps_24, pps[24]);
    dsc_write_field(dsc_index, Reg_Dsc_Reg1a, Register_Dsc_Reg1a_Start, 1,
        Pps_25, pps[25]);
    dsc_write_field(dsc_index, Reg_Dsc_Reg1b, Register_Dsc_Reg1b_Start, 1,
        Pps_26, pps[26]);
    dsc_write_field(dsc_index, Reg_Dsc_Reg1c, Register_Dsc_Reg1c_Start, 1,
        Pps_27, pps[27]);
    dsc_write_field(dsc_index, Reg_Dsc_Reg1d, Register_Dsc_Reg1d_Start, 1,
        Pps_28, pps[28]);
    dsc_write_field(dsc_index, Reg_Dsc_Reg1e, Register_Dsc_Reg1e_Start, 1,
        Pps_29, pps[29]);
    dsc_write_field(dsc_index, Reg_Dsc_Reg1f, Register_Dsc_Reg1f_Start, 1,
        Pps_30, pps[30]);
    dsc_write_field(dsc_index, Reg_Dsc_Reg20, Register_Dsc_Reg20_Start, 1,
        Pps_31, pps[31]);
}

static disp_void_t dsc_module_enable(pdisp_dsc_t pdsc, disp_u8_t enable)
{
    pdisp_device_t pdd = pdsc->disp_device;

    dsc_write_field(pdsc->index, Reg_Dsc_Reg0, Register_Dsc_Reg0_Start, 1,
        Dsc_Enable, enable);
}

static disp_void_t dsc_module_set(pdisp_dsc_t pdsc, disp_u8_t enable)
{
    if (!dsc_already_bind(pdsc))
    {
        disp_print(DDM_WARNING, "dsc[%s] unbind!\n", pdsc->index);
        return;
    }

    if (enable)
    {
        dsc_pps_generate(pdsc);
        dsc_module_flush(pdsc);
        pdsc->state.flag.bits.dsc_on = 1;
    }
    else
    {
        pdsc->state.flag.bits.dsc_on = 0;
    }

    dsc_module_enable(pdsc, enable);
}

static DISP_STATUS dsc_bind_encoder(pdisp_dsc_t pdsc, disp_s32_t encoder_index)
{
    pdisp_device_t pdd = (pdisp_device_t)pdsc->disp_device;
    pdisp_encoder_t pencoder = &pdd->disp_encoder[encoder_index];

    if (pdsc->state.flag.bits.already_bound)
    {
        disp_print(DDM_INFO, "dsc[%d] already bound to encoder[%d]\n", pdsc->index, pdsc->state.encoder_index);
        return -DISP_EBUSY;
    }

    pdsc->state.flag.bits.already_bound = 1;
    pdsc->state.encoder_index = encoder_index;
    pencoder->pdsc = pdsc;
    return DISP_OK;
}

static disp_void_t dsc_unbind_encoder(pdisp_dsc_t pdsc)
{
    pdisp_device_t pdd = (pdisp_device_t)pdsc->disp_device;
    pdisp_encoder_t pencoder = DISP_NULL;

    if(pdsc->state.flag.bits.already_bound)
    {
        pencoder = &pdd->disp_encoder[pdsc->state.encoder_index];
        pencoder->pdsc = DISP_NULL;
        pdsc->state.flag.bits.already_bound = 0;
    }
}

static disp_u8_t dsc_check_timing(pdisp_dsc_t pdsc, pdisp_display_mode_t pmode, disp_u8_t bpc)
{
    pdisp_device_t pdd = (pdisp_device_t)pdsc->disp_device;
    pdisp_connector_t pconnector = pconnector = &pdd->disp_connector[pdsc->state.encoder_index];
    disp_u8_t fec_possible = DISP_FALSE, dsc_possible = DISP_FALSE;
    disp_u8_t ret = DISP_TRUE;

    pdsc->policy.bpc = bpc;
    pdsc->policy.color_fmt = DISP_COLOR_FORMAT_TYPE_RGB;
    pdsc->policy.pic_width = pmode->hdisplay;
    pdsc->policy.pic_height = pmode->vdisplay;
    pdsc->policy.clk_khz = (disp_u32_t)pmode->clock;
    fec_possible = check_fec_possible((pdisp_dsc_caps_t)pdsc->phw_caps, &pconnector->disp_info.dsc_sink_caps);
    dsc_possible = dsc_policy_set_by_hw_and_sink(pdsc, &pconnector->disp_info.dsc_sink_caps);

    if (!fec_possible || !dsc_possible)
    {
        ret = DISP_FALSE;
    }
    else if (8 == pdsc->policy.slice_num_hor && ((pmode->hdisplay / 8) % 12))
    {   /* G100 A0 patch: virtual slice end calculate wrong. */
        ret = DISP_FALSE;
    }
    else if (pmode->htotal - pmode->hdisplay < DISP_DSC_MIN_HBLANK)
    {   /* G100 A0 patch: Some monitor black screen if regenerate timing to increase hblank, so disable them for safe. */
        ret = DISP_FALSE;
    }

    if (DISP_FALSE == ret)
    {
        disp_print(DDM_TRACE, "dsc_check_timing, bpc[%u] mode_clk[%d] wh[%u %u]!\n",
            bpc, pmode->clock, pmode->hdisplay, pmode->vdisplay);
    }

    return ret;
}

DISP_STATUS disp_dsc_sw_init(pdisp_dsc_t pdsc, pdisp_void_t pdisp_device, disp_u32_t dsc_index, disp_u32_t encoder_index)
{
    pdsc->disp_device = pdisp_device;
    pdsc->index = dsc_index;

    pdsc->func.policy_update = dsc_policy_update;
    pdsc->func.module_set = dsc_module_set;
    pdsc->func.need_enable_dsc = dsc_need_enable_dsc;
    pdsc->func.need_dsc_off = dsc_need_off;
    pdsc->func.check_timing = dsc_check_timing;

    pdsc->phw_caps = disp_malloc(sizeof(disp_dsc_caps_t));
    if (!pdsc->phw_caps)
    {
        disp_print(DDM_ERROR, "disp_dsc_sw_init, alloc hw caps failed\n");
        return DISP_ENOMEM;
    }

    dsc_hw_caps_init((pdisp_dsc_caps_t)pdsc->phw_caps);
    dsc_policy_init(&pdsc->policy);
    dsc_bind_encoder(pdsc, encoder_index);

    return DISP_OK;
}

disp_void_t disp_dsc_sw_deinit(pdisp_dsc_t pdsc)
{
    if (pdsc->phw_caps)
        disp_free(pdsc->phw_caps);
    pdsc->phw_caps = DISP_NULL;

    dsc_unbind_encoder(pdsc);

    pdsc->func.policy_update = DISP_NULL;
    pdsc->func.module_set = DISP_NULL;
    pdsc->func.need_enable_dsc = DISP_NULL;
    pdsc->func.need_dsc_off = DISP_NULL;
    pdsc->func.check_timing = DISP_NULL;
}
