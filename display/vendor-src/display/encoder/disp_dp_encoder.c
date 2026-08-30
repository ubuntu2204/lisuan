/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#include "disp_dp_encoder.h"
#include "disp_device.h"
#include "disp_dp_aux.h"
#include "disp_encoder.h"
#include "disp_connector.h"
#include "disp_dp_connector.h"
#include "DP_register.h"
#include "disp_reg.h"
#include "disp_dp_phy.h"
#include "disp_hdmi_encoder.h"
#include "disp_pll.h"
#include "SR_register.h"

#define DISP_DP_RETRY_LT_TIMES 16
#define DISP_DP_CTS_RETRY_LT_TIMES 5

#define disp_print(level, log, ...) disp_print_ex(DISP_PRINT_MASK_ENCODER_DP, level, log, ##__VA_ARGS__)
#define disp_print_pure(level, log, ...) disp_print_pure_ex(DISP_PRINT_MASK_ENCODER_DP, level, log, ##__VA_ARGS__)
#define disp_print_addition_file(level, log, ...) \
{ \
    disp_print_ex(DISP_PRINT_MASK_ENCODER_DP, level, log, ##__VA_ARGS__); \
    disp_print_to_file_ex(pdd, DISP_PRINT_MASK_ENCODER_DP, level, log, ##__VA_ARGS__); \
}

static disp_void_t encoder_dp_set_timing(pdisp_encoder_t pdisp_encoder)
{
    pdisp_device_t pdd = pdisp_encoder->disp_device;
    disp_crtc_t* disp_crtc = &pdd->disp_crtc[pdisp_encoder->state.crtc_index];
    disp_display_mode_t mode = {0};
    disp_u32_t h_offset, v_offset, hsync_polarity, vsync_polarity, hsync, vsync;

    crtc_get_property_hw_mode(&disp_crtc->property, &mode);

    h_offset = mode.htotal - mode.hsync_start;
    v_offset = mode.vtotal - mode.vsync_start;
    hsync_polarity = mode.flags & DISP_MODE_FLAG_PHSYNC;
    vsync_polarity = mode.flags & DISP_MODE_FLAG_PVSYNC;
    hsync = (mode.hsync_end - mode.hsync_start) | (hsync_polarity << 15);
    vsync = (mode.vsync_end - mode.vsync_start) | (vsync_polarity << 15);

    dp_write_field(pdisp_encoder->index, Reg_Dpt_Reg1e, Register_Dpt_Reg1e_Start, 2,
            Htotal, mode.htotal,
            Hsync, hsync);

    dp_write_field(pdisp_encoder->index, Reg_Dpt_Reg1d, Register_Dpt_Reg1d_Start, 2,
            Hoffset, h_offset,
            Hden, mode.hdisplay);

    dp_write_field(pdisp_encoder->index, Reg_Dpt_Reg1f, Register_Dpt_Reg1f_Start, 2,
            Vtotal, mode.vtotal,
            Vsync, vsync);

    dp_write_field(pdisp_encoder->index, Reg_Dpt_Reg20, Register_Dpt_Reg20_Start, 2,
            Voffset, v_offset,
            Vden, mode.vdisplay);
}

static disp_void_t encoder_dp_set_msa_misc(pdisp_encoder_t pdisp_encoder)
{
    pdisp_device_t pdd = pdisp_encoder->disp_device;
    disp_u8_t misc0 = 0, misc1 = 0;

    // tofill: ref dp spec 2.1 Table 2-96: 
    // MSA MISC1 and MISC0 Bits for Pixel Encoding/Colorimetry Format Indication

    pdisp_encoder->policy.flag.bits.msa_ref_vsc = 0;
    if(!pdisp_encoder->policy.custom.dp.async_clock)
        misc0 |= 0x1;

    if(DISP_COLOR_FORMAT_TYPE_YUV422 == pdisp_encoder->policy.color_format)
    {
        misc0 |= (1 << 1);
    }
    else if(DISP_COLOR_FORMAT_TYPE_YUV444 == pdisp_encoder->policy.color_format)
    {
        misc0 |= (2 << 1);
    }
    else if(DISP_COLOR_FORMAT_TYPE_YUV420 == pdisp_encoder->policy.color_format)
    {
        misc1 |= (1 << 6); // use VSC & ignore MISC1[7] and MISC0[7:1]
        pdisp_encoder->policy.flag.bits.msa_ref_vsc = 1;
    }

    if (pdisp_encoder->policy.flag.bits.hdr_metadata && pdisp_encoder->policy.flag.bits.hdr_on)
    {
        misc1 |= (1 << 6);
        pdisp_encoder->policy.flag.bits.msa_ref_vsc = 1;
    }

    misc0 |= (DISP_RANGE_TYPE_CEA == pdisp_encoder->policy.custom.dp.range_type) ? (1 << 3) : 0;
    misc0 |= (DISP_YUV_COEFFICIENTS_ITU709 == pdisp_encoder->policy.custom.dp.ycbcr_coefficients) ? (1 << 4) : 0;

    if(pdisp_encoder->policy.bpc == 6)
        misc0 |= (0 << 5);
    else if(pdisp_encoder->policy.bpc == 8)
        misc0 |= (1 << 5);
    else if(pdisp_encoder->policy.bpc == 10)
        misc0 |= (2 << 5);
    else if(pdisp_encoder->policy.bpc == 12)
        misc0 |= (3 << 5);
    else if(pdisp_encoder->policy.bpc == 16)
        misc0 |= (4 << 5);

    dp_write_field(pdisp_encoder->index, Reg_Dpt_Reg21, Register_Dpt_Reg21_Start, 1,
        Misc0, misc0);

    dp_write_field(pdisp_encoder->index, Reg_Dpt_Reg22, Register_Dpt_Reg22_Start, 1,
        Misc1, misc1);
}

static disp_s32_t disp_dp_find_idle_sdp_fifo(pdisp_encoder_t pdisp_encoder)
{
    pdisp_device_t pdd = pdisp_encoder->disp_device;
    disp_u32_t sdp_state = 0, temp = 0;
    disp_s32_t index = 0;

    dp_read_field(pdisp_encoder->index, Reg_Dpt_Reg9d, Register_Dpt_Reg9d_Start, 1,
        Sdp_Tx_Status_0, &sdp_state);

    dp_read_field(pdisp_encoder->index, Reg_Dpt_Reg9e, Register_Dpt_Reg9e_Start, 1,
        Sdp_Tx_Status_1, &temp);

    for(index = 0; index < DP_SDP_FIFO_STATE_COUNT - 1; ++index)
    {
        if(!(sdp_state & DP_SDP_FIFO_STATE_MASK(index)))
            return index;
    }

    dp_read_field(pdisp_encoder->index, Reg_Dpt_Reg9e, Register_Dpt_Reg9e_Start, 1,
        Sdp_Tx_Status_1, &sdp_state);

    for(index = 0; index < DP_SDP_FIFO_STATE_COUNT - 1; ++index)
    {
        if(!(sdp_state & DP_SDP_FIFO_STATE_MASK(index)))
            return index + DP_SDP_FIFO_STATE_COUNT;
    }

    disp_print(DDM_WARNING, "disp_dp_find_idle_sdp_fifo, no found state1[%#x], index[%d]\n", sdp_state, index);
    return -DISP_EBUSY;
}

static disp_s32_t disp_dp_get_sdp_fifo_state(pdisp_encoder_t pdisp_encoder, disp_s32_t index)
{
    pdisp_device_t pdd = pdisp_encoder->disp_device;
    disp_u32_t sdp_state = 0;
    disp_s32_t idx = 0;

    if(index > DP_SDP_FIFO_STATE_COUNT - 1)
    {
        disp_print(DDM_WARNING, "disp_dp_get_sdp_fifo_state, invalid index[%#x]\n", index);
        return -DISP_EINVAL;
    }

    if(index >= DP_SDP_FIFO_STATE_COUNT)
    {
        dp_read_field(pdisp_encoder->index, Reg_Dpt_Reg9e, Register_Dpt_Reg9e_Start, 1,
            Sdp_Tx_Status_1, &sdp_state);
    }
    else
    {
        dp_read_field(pdisp_encoder->index, Reg_Dpt_Reg9d, Register_Dpt_Reg9d_Start, 1,
            Sdp_Tx_Status_0, &sdp_state);
    }

    idx = (index >= DP_SDP_FIFO_STATE_COUNT) ? index - DP_SDP_FIFO_STATE_COUNT : index;
    return (sdp_state & DP_SDP_FIFO_STATE_MASK(idx)) >> (idx * 2);
}

static disp_void_t disp_dp_clear_sdp_fifo(pdisp_encoder_t pdisp_encoder, disp_s32_t index)
{
    pdisp_device_t pdd = pdisp_encoder->disp_device;

    // check: need set DPT_REG0D_SDP_REQ_IDLE & sleep ?

    dp_write_field(pdisp_encoder->index, Reg_Dpt_Reg0d, Register_Dpt_Reg0d_Start, 2,
        Sdp_Req, DPT_REG0D_SDP_REQ_TX_CLEAR,
        Sdp_Req_Addr, index);
}

static disp_void_t disp_dp_sdp_flush(pdisp_encoder_t pdisp_encoder, struct dp_sdp* psdp)
{
    pdisp_device_t pdd = pdisp_encoder->disp_device;

    dp_write_field(pdisp_encoder->index, Reg_Dpt_Reg10, Register_Dpt_Reg10_Start, 1,
        Sdp_Header, psdp->header.uint);

    dp_write_field(pdisp_encoder->index, Reg_Dpt_Reg11, Register_Dpt_Reg11_Start, 1,
        Sdp_Payload_0, psdp->payload.u32[0]);
    dp_write_field(pdisp_encoder->index, Reg_Dpt_Reg12, Register_Dpt_Reg12_Start, 1,
        Sdp_Payload_1, psdp->payload.u32[1]);
    dp_write_field(pdisp_encoder->index, Reg_Dpt_Reg13, Register_Dpt_Reg13_Start, 1,
        Sdp_Payload_2, psdp->payload.u32[2]);
    dp_write_field(pdisp_encoder->index, Reg_Dpt_Reg14, Register_Dpt_Reg14_Start, 1,
        Sdp_Payload_3, psdp->payload.u32[3]);
    dp_write_field(pdisp_encoder->index, Reg_Dpt_Reg15, Register_Dpt_Reg15_Start, 1,
        Sdp_Payload_4, psdp->payload.u32[4]);
    dp_write_field(pdisp_encoder->index, Reg_Dpt_Reg16, Register_Dpt_Reg16_Start, 1,
        Sdp_Payload_5, psdp->payload.u32[5]);
    dp_write_field(pdisp_encoder->index, Reg_Dpt_Reg17, Register_Dpt_Reg17_Start, 1,
        Sdp_Payload_6, psdp->payload.u32[6]);
    dp_write_field(pdisp_encoder->index, Reg_Dpt_Reg18, Register_Dpt_Reg18_Start, 1,
        Sdp_Payload_7, psdp->payload.u32[7]);

    dp_write_field(pdisp_encoder->index, Reg_Dpt_Reg0d, Register_Dpt_Reg0d_Start, 3,
        Sdp_Len, psdp->sdp_len,
        Sdp_Req, psdp->sdp_req,
        Sdp_Req_Addr, psdp->fifo_index);
}

static disp_void_t disp_dp_sdp_dump(struct dp_sdp* psdp)
{
    disp_s32_t index = 0;
    disp_u32_t level = DDM_TRACE;

    disp_print(level, "dp_sdp, fifo: req_type[%u], len_type[%u], fifo_index[%u]\n",
        psdp->sdp_req, psdp->sdp_len, psdp->fifo_index);
    disp_print_pure(level, "\theader: %#x, %#x, %#x, %#x\n",
        psdp->header.byte.b0, psdp->header.byte.b1, psdp->header.byte.b2, psdp->header.byte.b3);
    disp_print_pure(level, "\tpayload: ");
    for(index = 0; index < DP_SDP_PAYLOAD_MAX_DATA_BYTES; index += 4)
    {
        disp_print_pure(level, "%#x %#x %#x %#x, ", psdp->payload.u8[index],
            psdp->payload.u8[index + 1], psdp->payload.u8[index + 2], psdp->payload.u8[index + 3]);
    }
    disp_print_pure(level, "\n");
}

static enum dp_pixelformat disp_to_dp_pixel_fmt(DISP_COLOR_FORMAT_TYPE disp_pixel_fmt)
{
    switch(disp_pixel_fmt)
    {
        case DISP_COLOR_FORMAT_TYPE_RGB:
            return DP_PIXELFORMAT_RGB;
        case DISP_COLOR_FORMAT_TYPE_YUV444:
            return DP_PIXELFORMAT_YUV444;
        case DISP_COLOR_FORMAT_TYPE_YUV422:
            return DP_PIXELFORMAT_YUV422;
        case DISP_COLOR_FORMAT_TYPE_YUV420:
            return DP_PIXELFORMAT_YUV420;
        default:
            disp_print(DDM_TRACE, "disp_to_dp_pixel_fmt, invalid policy color_format[%d], use RGB\n", disp_pixel_fmt);
    }
    return DP_PIXELFORMAT_RGB;
}

static enum dp_colorimetry disp_to_dp_colorimetry(DISP_COLORIMETRY disp_colorimetry)
{
    switch(disp_colorimetry)
    {
        case DISP_COLORIMETRY_SRGB:
        case DISP_COLORIMETRY_YCC_NONE:
        case DISP_COLORIMETRY_YCC_SMPTE_170M:
            return DP_COLORIMETRY_DEFAULT;
        case DISP_COLORIMETRY_YCC_BT709:
            return DP_COLORIMETRY_BT709_YCC;
        case DISP_COLORIMETRY_XVYCC_601:
            return DP_COLORIMETRY_XVYCC_601;
        case DISP_COLORIMETRY_XVYCC_709:
            return DP_COLORIMETRY_XVYCC_709;
        case DISP_COLORIMETRY_SYCC_601:
            return DP_COLORIMETRY_SYCC_601;
        case DISP_COLORIMETRY_YCC_ADOBE_601:
            return DP_COLORIMETRY_OPYCC_601;
        case DISP_COLORIMETRY_RGB_ADOBE:
            return DP_COLORIMETRY_OPRGB;
        case DISP_COLORIMETRY_CYCC_BT2020:
            return DP_COLORIMETRY_BT2020_CYCC;
        case DISP_COLORIMETRY_YCC_BT2020:
            return DP_COLORIMETRY_BT2020_YCC;
        case DISP_COLORIMETRY_RGB_BT2020:
            return DP_COLORIMETRY_BT2020_RGB;
        case DISP_COLORIMETRY_RGB_DCI:
        case DISP_COLORIMETRY_RGB_DCI_D65:
            return DP_COLORIMETRY_DCI_P3_RGB;
        default:
            disp_print(DDM_TRACE, "disp_to_dp_colorimetry, invalid policy colorimetry[%d], use default\n", disp_colorimetry);
    }
    return DP_COLORIMETRY_DEFAULT;
}

static enum dp_bpc disp_to_dp_bpc(disp_u32_t disp_bpc)
{
    switch(disp_bpc)
    {
        case 6:
            return DP_BPC_6;
        case 8:
            return DP_BPC_8;
        case 10:
            return DP_BPC_10;
        default:
            disp_print(DDM_WARNING, "disp_to_dp_bpc, invalid policy bpc[%d], use 8bpc\n", disp_bpc);
    }
    return DP_BPC_8;
}

// SDP VSC
static disp_void_t disp_dp_generate_vsc_payload_colorimetry(pdisp_encoder_t pencoder, struct disp_dp_sdp_vsc* pvsc)
{
    struct disp_encoder_policy *ppolicy = &pencoder->policy;

    pvsc->pixelformat = disp_to_dp_pixel_fmt(ppolicy->color_format);
    pvsc->colorimetry = disp_to_dp_colorimetry(ppolicy->colorimetry);
    pvsc->bpc = disp_to_dp_bpc(ppolicy->bpc);
    pvsc->dynamic_range = (DISP_RANGE_TYPE_CEA == ppolicy->custom.dp.range_type) ? DP_DYNAMIC_RANGE_CTA : DP_DYNAMIC_RANGE_VESA;
    pvsc->content_type = DP_CONTENT_TYPE_NOT_DEFINED;
}

static disp_void_t disp_dp_generate_vsc_payload_psr(pdisp_encoder_t pencoder, struct disp_dp_sdp_vsc* pvsc)
{
    pdisp_device_t pdd = pencoder->disp_device;
    pdisp_connector_t pconnector = &pdd->disp_connector[pencoder->state.connector_index];
    pdisp_edp_info_t pedp = (pdisp_edp_info_t)pconnector->disp_info.edp_info;
    disp_u32_t reg_value = 0;

    switch(pencoder->state.priv.dp.psr_type)
    {
    case DISP_PSR_EXIT:
        pvsc->psr_active = 0;
        break;
    case DISP_PSR_ENTRY:
    case DISP_PSR_UPDATE:
        pvsc->psr_active = 1;
        pvsc->psr_rfb = pedp->psr.set_update_rfb ? 1 : 0;
        if (pedp->psr.prefer_crc_on)
        {
            dp_read_field(pencoder->index, Reg_Dpt_Reg27, Register_Dpt_Reg27_Start, 1,
                Psr_Crc_0, &reg_value);
            pvsc->psr_crc_rgb[0] = (disp_u16_t)reg_value;
            dp_read_field(pencoder->index, Reg_Dpt_Reg28, Register_Dpt_Reg28_Start, 1,
                Psr_Crc_1, &reg_value);
            pvsc->psr_crc_rgb[1] = (disp_u16_t)reg_value;
            pvsc->psr_crc_rgb[2] = (disp_u16_t)(reg_value >> 16);
        }
        if (pedp->psr.set_su_y)
        {
            disp_print(DDM_WARNING, "disp_dp_generate_vsc_payload_psr, su not handle!\n");
        }
        break;
    default:
        disp_print(DDM_WARNING, "disp_dp_generate_vsc_payload_psr, psr_type[%d] not handle!\n",
            pencoder->state.priv.dp.psr_type);
        break;
    }
}

static disp_void_t disp_dp_generate_vsc(pdisp_encoder_t pencoder, struct disp_dp_sdp_vsc* pvsc)
{
    pdisp_device_t pdd = DISP_NULL;
    pdisp_connector_t pconnector = DISP_NULL;
    pdisp_edp_info_t pedp = DISP_NULL;

    disp_memset(pvsc, 0, sizeof(*pvsc));
    pvsc->sdp_id = 0;
    pvsc->sdp_type = DP_SDP_VSC;

    if (pencoder->state.priv.dp.flag.bits.sdp_vsc_colorimetry_once)
    {
        pvsc->revision = DP_SDP_VSC_TYPE_3D_PSR2_COLORIMETRY;
        pvsc->length = DP_SDP_VSC_LENGTH_3D_PSR2_COLORIMETRY;
        disp_dp_generate_vsc_payload_colorimetry(pencoder, pvsc);
        if (pencoder->state.priv.dp.flag.bits.sdp_vsc_psr_once)
            disp_dp_generate_vsc_payload_psr(pencoder, pvsc);
    }
    else if (pencoder->state.priv.dp.flag.bits.sdp_vsc_psr_once)
    {
        pdd = pencoder->disp_device;
        pconnector = &pdd->disp_connector[pencoder->state.connector_index];
        pedp = (pdisp_edp_info_t)pconnector->disp_info.edp_info;
        if (1 == pedp->psr.version)
        {
            pvsc->revision = DP_SDP_VSC_TYPE_3D_PSR;
            pvsc->length = DP_SDP_VSC_LENGTH_3D_PSR;
        }
        else
        {
            pvsc->revision = DP_SDP_VSC_TYPE_3D_PSR2;
            pvsc->length = DP_SDP_VSC_LENGTH_3D_PSR2;
        }
        disp_dp_generate_vsc_payload_psr(pencoder, pvsc);
    }
    else
    {
        disp_print(DDM_WARNING, "disp_dp_generate_vsc, no vsc to send!\n");
    }
}

static disp_void_t disp_dp_vsc_set_sdp(struct disp_dp_sdp_vsc* pvsc, struct dp_sdp* psdp, disp_u32_t fifo_index)
{
    /* vsc sdp header:
     *   0x0 0x7 0x6 0x10(payload size) // 3D + PSR
     *   0x0 0x7 0x7 0x13(payload size) // 3D + PSR + colorimetry
     * payload:
     *   Byte0 3D: 0x0
     *   Byte1 PSR flag, bit0~3: state, reserved, crc, update_rfb
     *   Byte2~7 crc: RGB, 16bits * 3
     *   Byte8~9 Update Region Upper-left Corner X-coordinate (Pixel Count)
     *   Byte10~11 Width of Selective Update Region Rectangle (Pixel Count)
     *   Byte12~13 Update Region First Scan Line Y-coordinate
     *   Byte14~15 Rectangle Height (Line Count)
     *   Byte16~18 colorimetry
     */

    disp_memset(psdp, 0, sizeof(*psdp));
    psdp->header.byte.b0 = pvsc->sdp_id;
    psdp->header.byte.b1 = pvsc->sdp_type;
    psdp->header.byte.b2 = pvsc->revision;
    psdp->header.byte.b3 = pvsc->length;
    psdp->payload.u8[0] = pvsc->stereo;
    psdp->payload.u8[1] = pvsc->psr_active | (pvsc->psr_rfb << 1) | (pvsc->psr_crc << 2) |
        (pvsc->psr_su << 1) | (pvsc->psr_su_first << 1) | (pvsc->psr_su_last << 1);
    psdp->payload.u8[2] = (disp_u8_t)pvsc->psr_crc_rgb[0];
    psdp->payload.u8[3] = (disp_u8_t)(pvsc->psr_crc_rgb[0] >> 8);
    psdp->payload.u8[4] = (disp_u8_t)pvsc->psr_crc_rgb[1];
    psdp->payload.u8[5] = (disp_u8_t)(pvsc->psr_crc_rgb[1] >> 8);
    psdp->payload.u8[6] = (disp_u8_t)pvsc->psr_crc_rgb[2];
    psdp->payload.u8[7] = (disp_u8_t)(pvsc->psr_crc_rgb[2] >> 8);
    psdp->payload.u8[8] = (disp_u8_t)pvsc->psr_su_x;
    psdp->payload.u8[9] = (disp_u8_t)(pvsc->psr_su_x >> 8);
    psdp->payload.u8[10] = (disp_u8_t)pvsc->psr_rect_w;
    psdp->payload.u8[11] = (disp_u8_t)(pvsc->psr_rect_w >> 8);
    psdp->payload.u8[12] = (disp_u8_t)pvsc->psr_su_y;
    psdp->payload.u8[13] = (disp_u8_t)(pvsc->psr_su_y >> 8);
    psdp->payload.u8[16] = (disp_u8_t)((pvsc->pixelformat << 4) | pvsc->colorimetry);
    psdp->payload.u8[17] = (disp_u8_t)((pvsc->dynamic_range << 7) | pvsc->bpc);
    psdp->payload.u8[18] = pvsc->content_type;

    psdp->sdp_len = DPT_REG0D_SDP_LEN_LEN_256;
    psdp->sdp_req = DPT_REG0D_SDP_REQ_TX_EVERY_FRAME;
    psdp->fifo_index = fifo_index;
}

static char *dp_pixelformat_get_name(enum dp_pixelformat pixelformat)
{
    if (pixelformat < 0 || pixelformat > DP_PIXELFORMAT_RESERVED)
        return "Invalid";

    switch (pixelformat) {
    case DP_PIXELFORMAT_RGB:
        return "RGB";
    case DP_PIXELFORMAT_YUV444:
        return "YUV444";
    case DP_PIXELFORMAT_YUV422:
        return "YUV422";
    case DP_PIXELFORMAT_YUV420:
        return "YUV420";
    case DP_PIXELFORMAT_Y_ONLY:
        return "Y_ONLY";
    case DP_PIXELFORMAT_RAW:
        return "RAW";
    default:
        return "Reserved";
    }
}

static char *dp_colorimetry_get_name(enum dp_pixelformat pixelformat,
                       enum dp_colorimetry colorimetry)
{
    if (pixelformat < 0 || pixelformat > DP_PIXELFORMAT_RESERVED)
        return "Invalid";

    switch (colorimetry) {
    case DP_COLORIMETRY_DEFAULT:
        switch (pixelformat) {
        case DP_PIXELFORMAT_RGB:
            return "sRGB";
        case DP_PIXELFORMAT_YUV444:
        case DP_PIXELFORMAT_YUV422:
        case DP_PIXELFORMAT_YUV420:
            return "BT.601";
        case DP_PIXELFORMAT_Y_ONLY:
            return "DICOM PS3.14";
        case DP_PIXELFORMAT_RAW:
            return "Custom Color Profile";
        default:
            return "Reserved";
        }
    case DP_COLORIMETRY_RGB_WIDE_FIXED: /* and DP_COLORIMETRY_BT709_YCC */
        switch (pixelformat) {
        case DP_PIXELFORMAT_RGB:
            return "Wide Fixed";
        case DP_PIXELFORMAT_YUV444:
        case DP_PIXELFORMAT_YUV422:
        case DP_PIXELFORMAT_YUV420:
            return "BT.709";
        default:
            return "Reserved";
        }
    case DP_COLORIMETRY_RGB_WIDE_FLOAT: /* and DP_COLORIMETRY_XVYCC_601 */
        switch (pixelformat) {
        case DP_PIXELFORMAT_RGB:
            return "Wide Float";
        case DP_PIXELFORMAT_YUV444:
        case DP_PIXELFORMAT_YUV422:
        case DP_PIXELFORMAT_YUV420:
            return "xvYCC 601";
        default:
            return "Reserved";
        }
    case DP_COLORIMETRY_OPRGB: /* and DP_COLORIMETRY_XVYCC_709 */
        switch (pixelformat) {
        case DP_PIXELFORMAT_RGB:
            return "OpRGB";
        case DP_PIXELFORMAT_YUV444:
        case DP_PIXELFORMAT_YUV422:
        case DP_PIXELFORMAT_YUV420:
            return "xvYCC 709";
        default:
            return "Reserved";
        }
    case DP_COLORIMETRY_DCI_P3_RGB: /* and DP_COLORIMETRY_SYCC_601 */
        switch (pixelformat) {
        case DP_PIXELFORMAT_RGB:
            return "DCI-P3";
        case DP_PIXELFORMAT_YUV444:
        case DP_PIXELFORMAT_YUV422:
        case DP_PIXELFORMAT_YUV420:
            return "sYCC 601";
        default:
            return "Reserved";
        }
    case DP_COLORIMETRY_RGB_CUSTOM: /* and DP_COLORIMETRY_OPYCC_601 */
        switch (pixelformat) {
        case DP_PIXELFORMAT_RGB:
            return "Custom Profile";
        case DP_PIXELFORMAT_YUV444:
        case DP_PIXELFORMAT_YUV422:
        case DP_PIXELFORMAT_YUV420:
            return "OpYCC 601";
        default:
            return "Reserved";
        }
    case DP_COLORIMETRY_BT2020_RGB: /* and DP_COLORIMETRY_BT2020_CYCC */
        switch (pixelformat) {
        case DP_PIXELFORMAT_RGB:
            return "BT.2020 RGB";
        case DP_PIXELFORMAT_YUV444:
        case DP_PIXELFORMAT_YUV422:
        case DP_PIXELFORMAT_YUV420:
            return "BT.2020 CYCC";
        default:
            return "Reserved";
        }
    case DP_COLORIMETRY_BT2020_YCC:
        switch (pixelformat) {
        case DP_PIXELFORMAT_YUV444:
        case DP_PIXELFORMAT_YUV422:
        case DP_PIXELFORMAT_YUV420:
            return "BT.2020 YCC";
        default:
            return "Reserved";
        }
    default:
        return "Invalid";
    }
}

static char *dp_dynamic_range_get_name(enum dp_dynamic_range dynamic_range)
{
    switch (dynamic_range) {
    case DP_DYNAMIC_RANGE_VESA:
        return "VESA range";
    case DP_DYNAMIC_RANGE_CTA:
        return "CTA range";
    default:
        return "Invalid";
    }
}

static char *dp_content_type_get_name(enum dp_content_type content_type)
{
    switch (content_type) {
    case DP_CONTENT_TYPE_NOT_DEFINED:
        return "Not defined";
    case DP_CONTENT_TYPE_GRAPHICS:
        return "Graphics";
    case DP_CONTENT_TYPE_PHOTO:
        return "Photo";
    case DP_CONTENT_TYPE_VIDEO:
        return "Video";
    case DP_CONTENT_TYPE_GAME:
        return "Game";
    default:
        return "Reserved";
    }
}

static void disp_dp_sdp_vsc_log(struct disp_dp_sdp_vsc *vsc)
{
    disp_u32_t level = DDM_TRACE;

    disp_print(level, "DP SDP: %s, revision %u, length %u\n", "VSC", vsc->revision, vsc->length);
    disp_print_pure(level, "\tpsr: active[%u] rfb[%u] crc[%u] su_first_last[%u %u %u] y_coordinate[%u]\n",
        vsc->psr_active, vsc->psr_rfb, vsc->psr_crc, vsc->psr_su, vsc->psr_su_first, vsc->psr_su_last, vsc->psr_y);
    disp_print_pure(level, "\tpixelformat: %s\n", dp_pixelformat_get_name(vsc->pixelformat));
    disp_print_pure(level, "\tcolorimetry: %s\n", dp_colorimetry_get_name(vsc->pixelformat, vsc->colorimetry));
    disp_print_pure(level, "\tbpc: %u\n", vsc->bpc);
    disp_print_pure(level, "\tdynamic range: %s\n", dp_dynamic_range_get_name(vsc->dynamic_range));
    disp_print_pure(level, "\tcontent type: %s\n", dp_content_type_get_name(vsc->content_type));
}

// infoframe
static disp_void_t disp_dp_infoframe_set_sdp(struct disp_dp_sdp_infoframe* pinfoframe, struct dp_sdp* psdp, disp_u32_t fifo_index)
{
    disp_s32_t byte_count_1 = pinfoframe->data_count - 1;

    disp_memset(psdp, 0, sizeof(*psdp));
    psdp->header.byte.b0 = pinfoframe->sdp_id;
    psdp->header.byte.b1 = pinfoframe->sdp_type;
    psdp->header.byte.b2 = byte_count_1 & 0xff;
    psdp->header.byte.b3 = ((byte_count_1 & 0x300) >> 8) | (pinfoframe->sdp_ver << 2);

    psdp->payload.u8[0] = pinfoframe->infoframe_ver;
    psdp->payload.u8[1] = pinfoframe->infoframe_len;
    disp_memcpy(&psdp->payload.u8[2], &pinfoframe->data_u8[0], DP_SDP_INFOFRAME_MAX_DATA_BYTES);

    psdp->sdp_len = DPT_REG0D_SDP_LEN_LEN_256;
    psdp->sdp_req = DPT_REG0D_SDP_REQ_TX_EVERY_FRAME;
    psdp->fifo_index = fifo_index;
}

static disp_void_t disp_dp_sdp_infoframe_log(struct disp_dp_sdp_infoframe* pinfoframe)
{
    disp_s32_t index = 0;

    disp_print(DDM_TRACE, "sdp infoframe: id[%u], type[%u], data_count[%u], sdp_ver[%u]\n",
        pinfoframe->sdp_id, pinfoframe->sdp_type, pinfoframe->data_count, pinfoframe->sdp_ver);
    if(0x13 == pinfoframe->sdp_ver)
        disp_print(DDM_TRACE, "\tinfoframe_ver[%u], infoframe_len[%u]\n", pinfoframe->infoframe_ver, pinfoframe->infoframe_len);
    disp_print(DDM_TRACE, "\tinfoframe_data:\t");
    for(index = 0; index < DP_SDP_INFOFRAME_MAX_DATA_BYTES; index += 4)
        disp_print(DDM_TRACE, "\t[%#x, %#x, %#x, %#x],\t", pinfoframe->data_u8[index], pinfoframe->data_u8[index + 1],
            pinfoframe->data_u8[index + 2], pinfoframe->data_u8[index + 3]);
    disp_print(DDM_TRACE, "\n");
}

static disp_void_t encoder_sdp_set_vsc(pdisp_encoder_t pencoder, disp_u32_t fifo_index)
{
    struct dp_sdp sdp;
    struct disp_dp_sdp_vsc vsc;

    disp_memset(&sdp, 0, sizeof(sdp));
    disp_memset(&vsc, 0, sizeof(vsc));

    disp_dp_generate_vsc(pencoder, &vsc);
    disp_dp_sdp_vsc_log(&vsc);
    disp_dp_vsc_set_sdp(&vsc, &sdp, fifo_index);
    disp_dp_sdp_dump(&sdp);
    disp_dp_sdp_flush(pencoder, &sdp);

    // clear
    pencoder->state.priv.dp.flag.bits.sdp_vsc_psr_once = 0;
    pencoder->state.priv.dp.flag.bits.sdp_vsc_colorimetry_once = 0;
}

static disp_void_t encoder_sdp_set_avi(pdisp_encoder_t pencoder, disp_u32_t fifo_index)
{
    struct dp_sdp sdp;
    struct disp_dp_sdp_infoframe infoframe;
    hdmi_avi_infoframe_t hdmi_avi_infoframe;
    disp_u8_t infoframe_buf[32] = {0};

    disp_memset(&sdp, 0, sizeof(sdp));
    disp_memset(&infoframe, 0, sizeof(infoframe));
    disp_memset(&hdmi_avi_infoframe, 0, sizeof(hdmi_avi_infoframe));
    disp_memset(infoframe_buf, 0, 32);

    hdmi_avi_infoframe_construct(pencoder, &hdmi_avi_infoframe);
    hdmi_avi_infoframe_pack(&hdmi_avi_infoframe, infoframe_buf, 32);
    infoframe.sdp_id = 0;
    infoframe.sdp_type = hdmi_avi_infoframe.type;
    infoframe.data_count = 0x1e; // len 30
    infoframe.sdp_ver = 0x12;    // sdp v1.2
    disp_memcpy(&infoframe.data_u8[0], &infoframe_buf[HDMI_INFOFRAME_HEADER_SIZE], DP_SDP_INFOFRAME_MAX_DATA_BYTES);
    disp_dp_infoframe_set_sdp(&infoframe, &sdp, fifo_index);
    disp_dp_sdp_dump(&sdp);
    disp_dp_sdp_flush(pencoder, &sdp);
}

static disp_void_t encoder_sdp_set_drm(pdisp_encoder_t pencoder, disp_u32_t fifo_index)
{
    struct dp_sdp sdp;
    struct disp_dp_sdp_infoframe infoframe;
    hdmi_drm_infoframe_t hdmi_drm_infoframe;
    disp_u8_t infoframe_buf[32] = {0};

    disp_memset(&sdp, 0, sizeof(sdp));
    disp_memset(&infoframe, 0, sizeof(infoframe));
    disp_memset(&hdmi_drm_infoframe, 0, sizeof(hdmi_drm_infoframe));
    disp_memset(infoframe_buf, 0, 32);

    hdmi_drm_infoframe_construct(pencoder, &hdmi_drm_infoframe);
    hdmi_drm_infoframe_pack(&hdmi_drm_infoframe, infoframe_buf, 32);

    infoframe.sdp_id = 0;
    infoframe.sdp_type = hdmi_drm_infoframe.type;
    infoframe.data_count = 0x1e; // len 30
    infoframe.sdp_ver = 0x13;    // sdp v1.3
    infoframe.infoframe_ver = 0x1;
    infoframe.infoframe_len = 0x1a;
    disp_memcpy(&infoframe.data_u8[0], &infoframe_buf[HDMI_INFOFRAME_HEADER_SIZE], DP_SDP_INFOFRAME_MAX_DATA_BYTES);
    disp_dp_infoframe_set_sdp(&infoframe, &sdp, fifo_index);
    disp_dp_sdp_dump(&sdp);
    disp_dp_sdp_flush(pencoder, &sdp);
}

static disp_void_t encoder_sdp_set_audio_info(pdisp_encoder_t pencoder, pdisp_audio_infoframe_t paudio, disp_u32_t fifo_index)
{
    struct dp_sdp sdp;

    disp_memset(&sdp, 0, sizeof(sdp));
    sdp.header.byte.b0 = 0;
    sdp.header.byte.b1 = 0x84; // 0x80 + 0x4
    sdp.header.byte.b2 = 0x1b; // 28 Bytes
    sdp.header.byte.b3 = 0x48; // 0x12 >> 2 // version 1.2

    sdp.payload.u8[0] |= paudio->channel_count - 1;
    sdp.payload.u8[0] |= paudio->coding_type * 16;

    sdp.sdp_len = DPT_REG0D_SDP_LEN_LEN_256;
    sdp.sdp_req = DPT_REG0D_SDP_REQ_TX_EVERY_FRAME;
    sdp.fifo_index = fifo_index;

    disp_dp_sdp_dump(&sdp);
    disp_dp_sdp_flush(pencoder, &sdp);
}

static DISP_STATUS encoder_dp_set_sdp_audio(pdisp_encoder_t pdisp_encoder, pdisp_audio_infoframe_t paudio)
{
    disp_u8_t fifo_index = 0;
    struct dp_encoder_state *pdp_st = &pdisp_encoder->state.priv.dp;

    if (paudio->valid)
    {
        if (pdp_st->flag.bits.fifo_in_use)
        {
            if (pdp_st->flag.bits.fifo_have_audio)
                fifo_index = pdp_st->fifo_index_audio;
            else
            {
                fifo_index = pdp_st->fifo_index_idle;
                pdp_st->fifo_index_idle++;
            }
        }
        else
        {   // sdp fifo not in use, direct use fifo group 0, index 0
            fifo_index = 0;
            pdp_st->flag.bits.fifo_in_use = 1;
            pdp_st->flag.bits.fifo_idle_group = 1;
            pdp_st->fifo_index_idle = 1;
        }
        pdp_st->fifo_index_audio = fifo_index;
        pdp_st->flag.bits.fifo_have_audio = 1;

        encoder_sdp_set_audio_info(pdisp_encoder, paudio, fifo_index);
    }
    else if (pdp_st->flag.bits.fifo_in_use && pdp_st->flag.bits.fifo_have_audio)
    {
        disp_dp_clear_sdp_fifo(pdisp_encoder, pdp_st->fifo_index_audio);
    }
    else
    {
        // do nothing.
    }

    return DISP_OK;
}

static disp_void_t encoder_dp_set_sdp(pdisp_encoder_t pdisp_encoder)
{
    pdisp_device_t pdd = pdisp_encoder->disp_device;
    pdisp_connector_t pconnector = &pdd->disp_connector[pdisp_encoder->state.connector_index];
    disp_u8_t fifo_index = 0, idx = 0;
    disp_s32_t drm_valid = 0, vsc_valid = 0, avi_valid = 0;

    drm_valid = (pdisp_encoder->policy.flag.bits.hdr_metadata) ? 1 : 0;
    vsc_valid = (pdisp_encoder->policy.flag.bits.msa_ref_vsc)  ? 1 : 0;

    /* DP not need avi.
     * HDMI PCON could use direct avi_infoframe over sdp as option.
     * but HDMI PCON also should transfer MSA & vsc -> avi_infoframe, so no need enable avi.
     * ref DPv2.1 spec: HDMI Protocol Converter Adapters
     */
    avi_valid = 0;

    // swap fifo group, clear the idle group for next sdp change
    if(pdisp_encoder->state.priv.dp.flag.bits.fifo_in_use)
    {
        fifo_index = pdisp_encoder->state.priv.dp.flag.bits.fifo_idle_group ? 0 : DP_SDP_FIFO_STATE_COUNT;
        for(idx = 0; idx < DP_SDP_FIFO_STATE_COUNT; ++idx)
            disp_dp_clear_sdp_fifo(pdisp_encoder, idx + fifo_index);
        pdisp_encoder->state.priv.dp.flag.bits.fifo_in_use = 0;

        // clear psr & audio flag
        pdisp_encoder->state.priv.dp.fifo_index_vsc = 0;
        pdisp_encoder->state.priv.dp.flag.bits.fifo_have_vsc = 0;
        pdisp_encoder->state.priv.dp.fifo_index_audio = 0;
        pdisp_encoder->state.priv.dp.flag.bits.fifo_have_audio = 0;
    }

    if(!vsc_valid && !avi_valid && !drm_valid)
        return;

    pdisp_encoder->state.priv.dp.flag.bits.fifo_idle_group = pdisp_encoder->state.priv.dp.flag.bits.fifo_idle_group ? 0 : 1;
    pdisp_encoder->state.priv.dp.flag.bits.fifo_in_use = 1;
    fifo_index = pdisp_encoder->state.priv.dp.flag.bits.fifo_idle_group ? 0 : DP_SDP_FIFO_STATE_COUNT;

    if(vsc_valid)
    {
        if(!(DISP_COLOR_FORMAT_YCRCB420 & pconnector->disp_info.color_formats))
        {
            disp_print(DDM_TRACE, "msa_yuv420_config_by_sdp, edid yuv420 not support, color_formats[%#x]\n",
                pconnector->disp_info.color_formats);
        }
        pdisp_encoder->state.priv.dp.fifo_index_vsc = fifo_index;
        pdisp_encoder->state.priv.dp.flag.bits.fifo_have_vsc = 1;
        pdisp_encoder->state.priv.dp.flag.bits.sdp_vsc_colorimetry_once = 1;
        encoder_sdp_set_vsc(pdisp_encoder, fifo_index++);
    }

    if(avi_valid)
    {
        encoder_sdp_set_avi(pdisp_encoder, fifo_index++);
    }

    if(drm_valid)
    {
        encoder_sdp_set_drm(pdisp_encoder, fifo_index++);
    }

    if ((!pdisp_encoder->state.priv.dp.flag.bits.fifo_idle_group && fifo_index > DP_SDP_FIFO_STATE_COUNT * 2) ||
        (pdisp_encoder->state.priv.dp.flag.bits.fifo_idle_group && fifo_index > DP_SDP_FIFO_STATE_COUNT))
    {
        disp_print(DDM_WARNING, "encoder_dp_set_sdp, sdp fifo out of limit! idle_group[%u], cur_index[%u]\n",
            pdisp_encoder->state.priv.dp.flag.bits.fifo_idle_group, fifo_index);
    }
    pdisp_encoder->state.priv.dp.fifo_index_idle = fifo_index;
}

disp_void_t dp_free_sync_set_sink(pdisp_connector_t pdisp_connector, disp_u8_t enable);
static disp_void_t encoder_dp_set_free_sync(pdisp_encoder_t pdisp_encoder)
{
    pdisp_device_t pdd = pdisp_encoder->disp_device;
    struct disp_encoder_policy *ppolicy = &pdisp_encoder->policy;
    disp_u8_t free_sync_insert_line_0, free_sync_insert_line_1;

    if (ppolicy->flag.bits.free_sync_enable)
    {
        free_sync_insert_line_0 = (disp_u8_t)(ppolicy->free_sync_insert_line & 0xff);
        free_sync_insert_line_1 = (disp_u8_t)(ppolicy->free_sync_insert_line >> 8);

        pdisp_encoder->state.flag.bits.free_sync_on = 1;
        dp_free_sync_set_sink(&pdd->disp_connector[pdisp_encoder->index], DISP_TRUE);

        sr_write_field(pdisp_encoder->index, Reg_Sr_2a, Register_Sr_2a_Start, 1,
            Max_Linsert_0, free_sync_insert_line_0);
        sr_write_field(pdisp_encoder->index, Reg_Sr_2b, Register_Sr_2b_Start, 3,
            Max_Linsert_1, free_sync_insert_line_1,
            Sldfr_En, 0,
            Dynfr_En, 1);
        dp_write_field(pdisp_encoder->index, Reg_Dpt_Reg0, Register_Dpt_Reg0_Start, 3,
            Adaptive_Sync_Spec_Version, ppolicy->flag.bits.free_sync_sdp_version,
            Adaptive_Sync_Pos, ppolicy->flag.bits.free_sync_sdp_position,
            Adaptive_En, 1);
    }
    else if (!ppolicy->flag.bits.free_sync_enable && pdisp_encoder->state.flag.bits.free_sync_on)
    {
        sr_write_field(pdisp_encoder->index, Reg_Sr_2b, Register_Sr_2b_Start, 1,
            Dynfr_En, 0);
        dp_write_field(pdisp_encoder->index, Reg_Dpt_Reg0, Register_Dpt_Reg0_Start, 1,
            Adaptive_En, 0);

        dp_free_sync_set_sink(&pdd->disp_connector[pdisp_encoder->index], DISP_FALSE);
        pdisp_encoder->state.flag.bits.free_sync_on = 0;
    }
}

static DISP_STATUS dp_encoder_mode_set(pdisp_encoder_t pdisp_encoder)
{
    encoder_dp_set_timing(pdisp_encoder);
    encoder_dp_set_msa_misc(pdisp_encoder);
    encoder_dp_set_sdp(pdisp_encoder);
    encoder_dp_set_free_sync(pdisp_encoder);

    return DISP_OK;
}

static DISP_STATUS dp_encoder_fec_enable(pdisp_encoder_t pdisp_encoder, disp_u8_t enable)
{
    pdisp_device_t pdd = pdisp_encoder->disp_device;

    pdisp_encoder->state.flag.bits.fec_on = enable ? 1 : 0;

    dp_write_field(pdisp_encoder->index, Reg_Dpt_Reg4, Register_Dpt_Reg4_Start, 2,
        Fec_Org_Version, pdisp_encoder->policy.flag.bits.fec_org_dp_1_4,
        Fec_En, enable);

    return DISP_OK;
}

static disp_void_t dp_wait_fec_dis_complete(pdisp_encoder_t pdisp_encoder)
{
    pdisp_device_t pdd = pdisp_encoder->disp_device;
    disp_u32_t fec_on = 0, times = 2000, delay_us = 1000;

    dp_read_field(pdisp_encoder->index, Reg_Dpt_Reg29, Register_Dpt_Reg29_Start, 1,
        Fec_On_Flag, &fec_on);
    while(fec_on && times)
    {
        disp_delay_us(delay_us);
        dp_read_field(pdisp_encoder->index, Reg_Dpt_Reg29, Register_Dpt_Reg29_Start, 1,
            Fec_On_Flag, &fec_on);
        --times;
    }
    if(fec_on)
        disp_print(0, "fec_on[%u], wait fec_off timeout, times[%u]\n", fec_on, times);
}

static DISP_STATUS dp_encoder_dsc_enable(pdisp_encoder_t pdisp_encoder, pdisp_dsc_t pdsc, disp_u8_t enable)
{
    pdisp_device_t pdd = pdisp_encoder->disp_device;

    dp_write_field(pdisp_encoder->index, Reg_Dpt_Reg74, Register_Dpt_Reg74_Start, 2,
        Dsc_Crc_En, pdsc->policy.flag.bits.dsc_crc,
        Dsc_En, enable);

    return DISP_OK;
}

static disp_void_t dp_encoder_set_dsc(pdisp_encoder_t pdisp_encoder, disp_u8_t enable)
{
    pdisp_device_t pdd = pdisp_encoder->disp_device;
    pdisp_dsc_t pdsc = (pdisp_dsc_t)pdisp_encoder->pdsc;

    if(!pdsc || !pdsc->func.module_set || !pdsc->func.need_enable_dsc || !pdsc->func.need_dsc_off)
    {
        disp_print(DDM_TRACE, "dp_encoder_set_dsc, dsc not bind.\n");
        return;
    }

    if(enable && pdsc->func.need_enable_dsc(pdsc))
    {
        dp_encoder_dsc_enable(pdisp_encoder, pdsc, enable);
        pdsc->func.module_set(pdsc, enable);

        disp_dp_set_sink_dsc(&pdd->disp_connector[pdisp_encoder->index], enable);
    }
    else if (!enable && pdsc->func.need_dsc_off(pdsc))
    {
        disp_dp_set_sink_dsc(&pdd->disp_connector[pdisp_encoder->index], enable);

        pdsc->func.module_set(pdsc, enable);
        dp_encoder_dsc_enable(pdisp_encoder, pdsc, enable);
    }
}

static DISP_STATUS dp_encoder_wait_for_tu_ratio_ready(pdisp_encoder_t pdisp_encoder)
{
    disp_s32_t times = 0, tu_ratio_ready = 0, tu_int = 0;
    pdisp_device_t pdd = pdisp_encoder->disp_device;

    dp_write_field(pdisp_encoder->index, Reg_Dpt_Reg76, Register_Dpt_Reg76_Start, 1,
        Reset_Ratio, 1);
    dp_write_field(pdisp_encoder->index, Reg_Dpt_Reg76, Register_Dpt_Reg76_Start, 1,
        Reset_Ratio, 0);

    do
    {
        disp_delay_us(100000); // 100ms

        dp_read_field(pdisp_encoder->index, Reg_Dpt_Rega0, Register_Dpt_Rega0_Start, 1,
            Hw_Tu_Ratio_Ready, &tu_ratio_ready);

        if (tu_ratio_ready == 1)
            break;
    } while(++times < 200); // max wait 20s
    dp_read_field(pdisp_encoder->index, Reg_Dpt_Rega0, Register_Dpt_Rega0_Start, 1,
        Hw_Tu_Int, &tu_int);
    disp_print(DDM_TRACE, "dp_encoder_wait_for_tu_ratio_ready, Hw_Tu_Ratio_Ready times[%d], Hw_Tu_Int[%u]\n", times, tu_int);

    if(times >= 200)
    {
        disp_print(DDM_WARNING, "dp_encoder_wait_for_tu_ratio_ready, time out\n");
        return -DISP_ETIMEDOUT;
    }
    return DISP_OK;
}

static DISP_STATUS dp_encoder_video_enable(pdisp_encoder_t pdisp_encoder, disp_u8_t benable)
{
    pdisp_device_t pdd = pdisp_encoder->disp_device;
    pdisp_display_mode_t phw_mode = &pdd->disp_crtc[pdisp_encoder->index].property.hw_mode;
    disp_u32_t dely_num = 0, hblank = 0, dely_num_multi = 0;

    /* Dely_Num default 0x20, for most case match: Dely_Num < hblank / 4
     * For hblank too small case, if not match half decrease Dely_Num.
     * Yuv420 dp clk half, need match: Dely_Num < hblank / 8, if not match, half decrease Dely_Num.
     */
    dely_num = 0x20;
    hblank = phw_mode->htotal - phw_mode->hdisplay;
    dely_num_multi = (DISP_COLOR_FORMAT_TYPE_YUV420 == pdisp_encoder->policy.color_format) ? 8 : 4;
    dely_num = (dely_num * dely_num_multi > hblank) ? 0x10 : dely_num;

    dp_write_field(pdisp_encoder->index, Reg_Dpt_Reg23, Register_Dpt_Reg23_Start, 2,
        Dely_Num, dely_num,
        Link_Video_En, benable);

    return DISP_OK;
}

static DISP_STATUS dp_encoder_audio_enable(pdisp_encoder_t pdisp_encoder, disp_u8_t benable)
{
    pdisp_device_t pdd = pdisp_encoder->disp_device;

    dp_write_field(pdisp_encoder->index, Reg_Dpt_Reg0, Register_Dpt_Reg0_Start, 2,
            Audio_En, benable,
            En_Audio_Timestamp, 1);

    dp_write_field(pdisp_encoder->index, Reg_Dpt_Reg0b, Register_Dpt_Reg0b_Start, 5,
            Hau_Info_Tx_En, benable,
            Audio_N_Sel, 2, // for ssc, 2 is ok, 0/1 not enough
            Block_Audio_Before_Maud, 0, // No block
            Mute_Audio_Before_Maud, 0,
            Maud_Mod, 1); //enable audio send audio infoframe

    dp_write_field(pdisp_encoder->index, Reg_Dpt_Reg0c, Register_Dpt_Reg0c_Start, 2,
            Hau_Info_Tx_Num, 0,
            Naud, 0x8000);

    return DISP_OK;
}

static DISP_STATUS dp_encoder_dp_enable(pdisp_encoder_t pdisp_encoder, disp_u8_t benable)
{
    pdisp_device_t pdd = pdisp_encoder->disp_device;

    dp_write_field(pdisp_encoder->index, Reg_Dpt_Reg0, Register_Dpt_Reg0_Start, 1,
            Dpt_Enable, benable);

    return DISP_OK;
}

static disp_void_t dp_lt_set_version(pdisp_encoder_t pdisp_encoder)
{
    pdisp_device_t pdd = pdisp_encoder->disp_device;

//check with victor for dpcd version
//sw need calculate the cr/eq duration according dpcd version and rd_interval

//sdp version is used to calculate the audio time stamp
    dp_write_field(pdisp_encoder->index, Reg_Dpt_Reg0d, Register_Dpt_Reg0d_Start, 1,
        Sdp_Version, 0x12);//fixed by hw
}

static disp_void_t dp_lt_set_rate_lane(pdisp_encoder_t pdisp_encoder)
{
    pdisp_device_t pdd = pdisp_encoder->disp_device;

    disp_u32_t lt_rate = 0, lane_count = 0;

    switch (pdisp_encoder->policy.custom.dp.link_rate) {
        case DP_LINK_BW_1_62:
            lt_rate = 0;
            break;
        case DP_LINK_BW_2_7:
            lt_rate = 1;
            break;
        case DP_LINK_BW_5_4:
            lt_rate = 2;
            break;
        case DP_LINK_BW_8_1:
        default:
            lt_rate = 3;
            break;
    }

    switch (pdisp_encoder->policy.custom.dp.lane_count) {
        case 1:
            lane_count = 0;
            break;
        case 2:
            lane_count = 1;
            break;
        case 4:
        default:
            lane_count = 2;
            break;
    }

    dp_write_field(pdisp_encoder->index, Reg_Dpt_Reg3, Register_Dpt_Reg3_Start, 3,
        Lt_Rate, lt_rate,
        Lt_Lane_Num, lane_count,
        Lt_Lane_Num_Force, 0);

    disp_print(DDM_TRACE, "dp_lt_set_rate_lane, lt_rate[%u] lane_count[%u]\n", lt_rate, lane_count);
}

static disp_void_t dp_lt_set_pattern(pdisp_encoder_t pdisp_encoder)
{
    pdisp_device_t pdd = pdisp_encoder->disp_device;
    struct disp_dp_policy* dp_policy = &pdisp_encoder->policy.custom.dp;
    disp_u32_t eq_pattern = dp_policy->tps - 2; // 2/3/4 -> DPT_REG3_EQ_PAT

    dp_write_field(pdisp_encoder->index, Reg_Dpt_Reg3, Register_Dpt_Reg3_Start, 1,
        Eq_Pat, eq_pattern);

    disp_print(DDM_TRACE, "dp_lt_set_pattern, eq_pattern[%d]\n", eq_pattern);
}

static disp_void_t dp_lt_set_rd_interval(pdisp_encoder_t pdisp_encoder)
{
    pdisp_device_t pdd = pdisp_encoder->disp_device;
    pdisp_connector_t disp_connector = &pdd->disp_connector[pdisp_encoder->state.connector_index];
    pdisp_dp_sink_caps_t sink_caps = (pdisp_dp_sink_caps_t)disp_connector->disp_info.dp_sink_caps;
    disp_u32_t cr_interval = 4000; // 4ms better than 400us, as some monitor 400us lt cr failed or down lr/lc.
    disp_u32_t eq_interval = (disp_u32_t)disp_8b10b_channel_eq_delay_us(disp_dp_get_rd_interval(sink_caps->dpcd));

    if(pdd->flag.info.pz1_config && DP_LINK_BW_8_1 == pdisp_encoder->policy.custom.dp.link_rate)
    {
        cr_interval = PZ1_RD_INTERVAL;
        eq_interval = PZ1_RD_INTERVAL;
    }

    dp_write_field(pdisp_encoder->index, Reg_Dpt_Reg2, Register_Dpt_Reg2_Start, 2,
        Cr_Interval, cr_interval,
        Eq_Interval, eq_interval);

    dp_write_field(pdisp_encoder->index, Reg_Dpt_Reg3, Register_Dpt_Reg3_Start, 1,
        Interval_1us, 26);//27M refrence clock, 27 cycle is 1us

    disp_print(DDM_TRACE, "dp_lt_set_rd_interval cr[%d], eq[%d]\n", cr_interval, eq_interval);
}

static disp_void_t dp_lt_set_common(pdisp_encoder_t pdisp_encoder)
{
    pdisp_device_t pdd = pdisp_encoder->disp_device;
    struct disp_dp_policy* dp_policy = &pdisp_encoder->policy.custom.dp;

    // Standard DP specific config. Note: eDP may be different.
    dp_write_field(pdisp_encoder->index, Reg_Dpt_Reg0, Register_Dpt_Reg0_Start, 3,
        Enhance_Mod, dp_policy->enhanced_frame_mode,
        Scramble_Disable, 0,
        Dp_Alternate, 0); // alternate mode disable.

    dp_write_field(pdisp_encoder->index, Reg_Dpt_Reg3, Register_Dpt_Reg3_Start, 1,
        Linkrate_Set_Use, 0); // use DPCD 00100h.

    dp_write_field(pdisp_encoder->index, Reg_Dpt_Reg1, Register_Dpt_Reg1_Start, 4,
        Linkrate_Set_Cfg0, DP_LINK_BW_1_62,
        Linkrate_Set_Cfg1, DP_LINK_BW_2_7,
        Linkrate_Set_Cfg2, DP_LINK_BW_5_4,
        Linkrate_Set_Cfg3, DP_LINK_BW_8_1);

    // General Config for both Standard DP and eDP.
    dp_write_field(pdisp_encoder->index, Reg_Dpt_Reg3, Register_Dpt_Reg3_Start, 8,
        Max_Emphasis_Support, 3, // max emphasis
        Max_Swing_Support, 3, // max swing
        Emphasis_Pre_Set, 0,
        Voltage_Pre_Set, 0,
        Clear_Dpcd102_When_Fail, 1,
        Retry_Lt_Clear_Dpcd102, 1,
        Dprx_Recv_Clk_Out_En, 0,
        Force_Lt_Pass, 0);

    /* CTS test 4/4/8 will lt pass, other setting lt failed.
     * spec says 5 or more iterations, 5/5/10 is OK for monitor & dp-hdmi cable.
     * HW confirm: Eq_Loop_Max/Same_Set_Retry_Max counting from 1, "4" means 5 times,
     *      Total_Retry_Max counting from 2, "8" means 10 times.
     */
    dp_write_field(pdisp_encoder->index, Reg_Dpt_Reg4, Register_Dpt_Reg4_Start, 4,
        Eq_Loop_Max, 4,
        Same_Set_Retry_Max, 4,
        Total_Retry_Max, 8,
        Aux_Retry_Max, 15);
}

static disp_void_t dp_lt_reset(pdisp_encoder_t pdisp_encoder)
{
    pdisp_device_t pdd = pdisp_encoder->disp_device;

    dp_write_field(pdisp_encoder->index, Reg_Dpt_Reg3, Register_Dpt_Reg3_Start, 1,
        Lt_Rst, 1);

    //disp_delay_us(100);

    dp_write_field(pdisp_encoder->index, Reg_Dpt_Reg3, Register_Dpt_Reg3_Start, 1,
        Lt_Rst, 0);
}

static disp_void_t dp_lt_start(pdisp_encoder_t pdisp_encoder)
{
    pdisp_device_t pdd = pdisp_encoder->disp_device;
    dp_write_field(pdisp_encoder->index, Reg_Dpt_Reg3, Register_Dpt_Reg3_Start, 1,
        Lt_Start, 1);
}

static disp_u8_t dp_lt_check_result(pdisp_encoder_t pdisp_encoder)
{
    pdisp_device_t pdd = pdisp_encoder->disp_device;
    disp_u32_t retry = 1000;
    disp_u32_t delay_us = 1000;
    disp_u32_t lt_done_o = 0;
    disp_u32_t lt_result_o = 0;
    disp_u32_t lt_rate_o = 0;
    disp_u32_t lt_Lane_Num_o = 0;
    disp_u8_t ret = DISP_FALSE;

    disp_u32_t lt_rate, lane_count;
    switch (pdisp_encoder->policy.custom.dp.link_rate) {
        case DP_LINK_BW_1_62:
            lt_rate = 0;
            break;
        case DP_LINK_BW_2_7:
            lt_rate = 1;
            break;
        case DP_LINK_BW_5_4:
            lt_rate = 2;
            break;
        case DP_LINK_BW_8_1:
        default:
            lt_rate = 3;
            break;
    }
    switch (pdisp_encoder->policy.custom.dp.lane_count) {
        case 1:
            lane_count = 0;
            break;
        case 2:
            lane_count = 1;
            break;
        case 4:
        default:
            lane_count = 2;
            break;
    }

    while (retry && !lt_done_o)
    {
        disp_delay_us (delay_us);
        dp_read_field(pdisp_encoder->index, Reg_Dpt_Reg29, Register_Dpt_Reg29_Start, 1,
            Lt_Done_O, &lt_done_o);
        retry--;
    }

    if (lt_done_o)
    {
        dp_read_field(pdisp_encoder->index, Reg_Dpt_Reg29, Register_Dpt_Reg29_Start, 3,
            Lt_Result_O, &(lt_result_o),
            Lt_Rate_O, &lt_rate_o,
            Lt_Lane_Num_O, &(lt_Lane_Num_o));
        if (DPT_REG29_LT_RESULT_O_LT_PASSED == lt_result_o)
        {
            if (lt_rate > lt_rate_o || lane_count > lt_Lane_Num_o)
            {
                disp_print_addition_file(DDM_KEY,
                    "LT Result: %u PASS but lt/lc Down!. Rate Level: %u, Lane Level: %u\n",
                    pdisp_encoder->index, lt_rate_o, lt_Lane_Num_o);
                return DISP_FALSE;
            }
            else
            {
                disp_print_addition_file(DDM_KEY, "LT Result: %u PASS. Rate Level: %u, Lane Level: %u\n",
                    pdisp_encoder->index, lt_rate_o, lt_Lane_Num_o);
                ret = DISP_TRUE;
            }
        }
        else
        {
            disp_print_addition_file(DDM_WARNING, "LT Result: %u FAIL! result[%u]: %s\n",
                pdisp_encoder->index, lt_result_o, (DPT_REG29_LT_RESULT_O_LT_FAILED_AUX == lt_result_o) ?
                "AUX Error" : ((DPT_REG29_LT_RESULT_O_LT_FAILED_CR == lt_result_o) ? "CR Error" : "EQ Error"));
            ret = DISP_FALSE;
        }
    }
    else
    {
        disp_print_addition_file(DDM_WARNING, "LT Result: %u Timeout!\n", pdisp_encoder->index);
        ret = DISP_FALSE;
    }

    if(pdd->flag.info.dp_cts)
    {
        dp_write_field(pdisp_encoder->index, Reg_Dpt_Reg4, Register_Dpt_Reg4_Start, 1,
            Aux_Retry_Max, 0);
    }

    return ret;
}

static disp_void_t encoder_dp_get_ink_rate_in_use(pdisp_encoder_t pdisp_encoder)
{
    pdisp_device_t pdd = pdisp_encoder->disp_device;
    disp_u32_t link_rate = 0;
    disp_u32_t lane_cnt = 0;

    dp_read_field(pdisp_encoder->index, Reg_Dpt_Reg29, Register_Dpt_Reg29_Start, 2,
        Lt_Rate_O, &link_rate,
        Lt_Lane_Num_O, &lane_cnt);

    disp_print(DDM_TRACE, "DP training reg: Lt_Rate_O[%u], Lt_Lane_Num_O[%u]\n", link_rate, lane_cnt);

    switch (link_rate) {
        case 0:
            pdisp_encoder->state.priv.dp.link_rate = DP_LINK_BW_1_62;
            break;
        case 1:
            pdisp_encoder->state.priv.dp.link_rate = DP_LINK_BW_2_7;
            break;
        case 2:
            pdisp_encoder->state.priv.dp.link_rate = DP_LINK_BW_5_4;
            break;
        case 3:
            pdisp_encoder->state.priv.dp.link_rate = DP_LINK_BW_8_1;
            break;
        default:
            disp_print(DDM_WARNING, "error link rate in use\n");
            break;
    }

    switch (lane_cnt) {
        case 0:
            pdisp_encoder->state.priv.dp.lane_count = 1;
            break;
        case 1:
            pdisp_encoder->state.priv.dp.lane_count = 2;
            break;
        case 2:
            pdisp_encoder->state.priv.dp.lane_count = 4;
            break;
        default:
            disp_print(DDM_WARNING, "error lane count in use\n");
    }

}

static disp_u8_t dp_encoder_link_training(pdisp_encoder_t pdisp_encoder)
{
    pdisp_device_t pdd = pdisp_encoder->disp_device;
    pdisp_connector_t pdisp_connector = &pdd->disp_connector[pdisp_encoder->state.connector_index];

    dp_lt_set_version(pdisp_encoder);
    dp_lt_set_rate_lane(pdisp_encoder);
    dp_lt_set_pattern(pdisp_encoder);
    dp_lt_set_rd_interval(pdisp_encoder);
    dp_lt_set_common(pdisp_encoder);

    pdisp_encoder->phy.link_rate = pdisp_encoder->policy.custom.dp.link_rate;
    pdisp_encoder->phy.lane_count = pdisp_encoder->policy.custom.dp.lane_count;
    pdisp_encoder->phy.ssc_en = pdisp_encoder->policy.custom.dp.flag.bits.ssc_on ? DISP_TRUE : DISP_FALSE;
    disp_dp_phy_link_training(&pdisp_encoder->phy);
    if (disp_dp_set_main_link_coding(pdisp_connector) < 0)
    {
        return DISP_FALSE;
    }

    dp_lt_reset(pdisp_encoder);
    dp_lt_start(pdisp_encoder);

    return dp_lt_check_result(pdisp_encoder);
}

static disp_u64_t get_bandwidth_kbps(pdisp_encoder_t pdisp_encoder)
{
    pdisp_device_t pdd = pdisp_encoder->disp_device;
    disp_crtc_t* disp_crtc = &pdd->disp_crtc[pdisp_encoder->state.crtc_index];
    disp_display_mode_t mode = {0};
    disp_u64_t bd;

    crtc_get_property_hw_mode(&disp_crtc->property, &mode);
    bd = mode.clock * pdisp_encoder->policy.bpc;

    if(DISP_COLOR_FORMAT_TYPE_YUV420 == pdisp_encoder->policy.color_format)
        bd = bd * 3 / 2;
    else if(DISP_COLOR_FORMAT_TYPE_YUV422 == pdisp_encoder->policy.color_format)
        bd *=  2;
    else
        bd *= 3;

    return bd;
}

static disp_u32_t get_sym_clk_khz(disp_u8_t lr)
{
    disp_u32_t sym_clk= DP_RBR_SYM_CLK_IN_KHZ;

    switch (lr) {
        case DP_LINK_BW_1_62:
            sym_clk = DP_RBR_SYM_CLK_IN_KHZ;
            break;
        case DP_LINK_BW_2_7:
            sym_clk = DP_HBR_SYM_CLK_IN_KHZ;
            break;
        case DP_LINK_BW_5_4:
            sym_clk = DP_HBR2_SYM_CLK_IN_KHZ;
            break;
        case DP_LINK_BW_8_1:
            sym_clk = DP_HBR3_SYM_CLK_IN_KHZ;
            break;
    }

    return sym_clk;
}

static disp_u64_t get_link_bandwidth_kbps(disp_u8_t link_rate, disp_u8_t lane_count)
{
    disp_u64_t lbd = 0;

    lbd  = get_sym_clk_khz(link_rate) * lane_count *8;

    return lbd;
}

static disp_void_t encoder_dp_set_tu(pdisp_encoder_t pdisp_encoder)
{
    pdisp_device_t pdd = pdisp_encoder->disp_device;
    struct disp_encoder_policy* ppolicy = &pdisp_encoder->policy;
    disp_u32_t video_bpc, video_fmt, bpp, dsc_bpp;
    disp_u64_t bandwidth, link_bandwidth;
    disp_u32_t tu_ratio_int, tu_ratio_fra, tu_size, auto_video_ratio;
    pdisp_dsc_t pdsc = (pdisp_dsc_t)pdisp_encoder->pdsc;
    disp_crtc_t* disp_crtc = &pdd->disp_crtc[pdisp_encoder->state.crtc_index];
    disp_display_mode_t mode = {0};

    switch(ppolicy->color_format)
    {
        case DISP_COLOR_FORMAT_TYPE_RGB:
            video_fmt = 0;
            bpp = 3 * ppolicy->bpc;
            break;
        case DISP_COLOR_FORMAT_TYPE_YUV444:
            video_fmt = 1;
            bpp = 3 * ppolicy->bpc;
            break;
        case DISP_COLOR_FORMAT_TYPE_YUV422:
            video_fmt = 2;
            bpp = 2 * ppolicy->bpc;
            break;
        case DISP_COLOR_FORMAT_TYPE_YUV420:
            video_fmt = 3;
            bpp = 3 * ppolicy->bpc;
            break;
        default:
            disp_print(DDM_WARNING, "encoder_dp_set_tu invalid color_format[%u], use RGB\n", ppolicy->color_format);
            video_fmt = 0;
            bpp = 3 * ppolicy->bpc;
            break;
    }
    switch(ppolicy->bpc)
    {
        case 6:
            video_bpc = 0;
            break;
        case 8:
            video_bpc = 1;
            break;
        case 10:
            video_bpc = 2;
            break;
        default:
            disp_print(DDM_WARNING, "encoder_dp_set_tu invalid bpc[%u], use 8bit\n", ppolicy->bpc);
            video_bpc = 1;
            bpp = bpp * 8 / ppolicy->bpc;
            break;
    }
    disp_print(DDM_TRACE, "encoder_dp_set_tu, fmt[%u], bpc[%u], bpp[%u], lt[%u], lc[%u]\n", video_fmt,
        video_bpc, bpp, pdisp_encoder->state.priv.dp.link_rate, pdisp_encoder->state.priv.dp.lane_count);

    auto_video_ratio = ppolicy->custom.dp.flag.bits.auto_tu_ratio;
    tu_size = ppolicy->custom.dp.TU_size;
    if(pdsc && pdsc->func.need_enable_dsc && pdsc->func.need_enable_dsc(pdsc))
    {
        dsc_bpp = pdsc->policy.bpp;
        crtc_get_property_hw_mode(&disp_crtc->property, &mode);
        bandwidth = mode.clock * dsc_bpp;
    }
    else
    {
        bandwidth = get_bandwidth_kbps(pdisp_encoder);
    }
    if (pdisp_encoder->policy.flag.bits.fec_enable)
    {
        bandwidth = DISP_FEC_BANDWIDTH(bandwidth);
    }
    link_bandwidth = get_link_bandwidth_kbps(pdisp_encoder->state.priv.dp.link_rate,
        pdisp_encoder->state.priv.dp.lane_count);
    if (bandwidth > link_bandwidth)
    {
        disp_print(DDM_WARNING, "encoder_dp_set_tu, auto[%u], tu_ratio[%u/%u] > 1.0!\n",
            auto_video_ratio, bandwidth, link_bandwidth);
    }

    // tu_ratio = bandwidth / link_bandwidth // (0, 1]
    tu_ratio_int = (disp_u32_t)(bandwidth * tu_size / link_bandwidth);
    tu_ratio_fra = (disp_u32_t)(bandwidth * tu_size * 0x10000 / link_bandwidth) & 0xffff;
    disp_print(DDM_TRACE, "encoder_dp_set_tu, auto[%u], tu_size[%u], tu_ratio[%u/%u], int[%u] fra[0x%x]\n",
        auto_video_ratio, tu_size, bandwidth, link_bandwidth, tu_ratio_int, tu_ratio_fra);

    dp_write_field(pdisp_encoder->index, Reg_Dpt_Reg24, Register_Dpt_Reg24_Start, 3,
        Tu_Size, tu_size - 1,
        Video_Bpc, video_bpc,
        Video_Fmt, video_fmt);

    dp_write_field(pdisp_encoder->index, Reg_Dpt_Reg74, Register_Dpt_Reg74_Start, 1,
        Pix_Bpp, bpp);

    dp_write_field(pdisp_encoder->index, Reg_Dpt_Reg76, Register_Dpt_Reg76_Start, 1,
        Auto_Video_Ratio, auto_video_ratio);

    /* use sw auto video ratio */
    if (0 == auto_video_ratio) {
        /* When FEC on, Tu_Act_Int & Tu_Act_Fra not work, Tu_Act_Int_Fec & Tu_Act_Fra_Fec work */
        if (pdisp_encoder->policy.flag.bits.fec_enable) {
            dp_write_field(pdisp_encoder->index, Reg_Dpt_Reg76, Register_Dpt_Reg76_Start, 2,
                Tu_Act_Int_Fec, tu_ratio_int,
                Tu_Act_Fra_Fec, tu_ratio_fra);
        } else {
            dp_write_field(pdisp_encoder->index, Reg_Dpt_Reg23, Register_Dpt_Reg23_Start, 2,
                Tu_Act_Int, tu_ratio_int,
                Tu_Act_Fra, tu_ratio_fra);
        }
        dp_write_field(pdisp_encoder->index, Reg_Dpt_Reg76, Register_Dpt_Reg76_Start, 1,
            Tu_Ratio_Ready, 1);
    }
}

static disp_void_t encoder_dp_set_mvid_nvid(pdisp_encoder_t pdisp_encoder)
{
    pdisp_device_t pdd = pdisp_encoder->disp_device;
    disp_crtc_t* disp_crtc = &pdd->disp_crtc[pdisp_encoder->state.crtc_index];
    disp_display_mode_t mode = {0};
    disp_u32_t M;

    crtc_get_property_hw_mode(&disp_crtc->property, &mode);

    M = DP_VID_N * mode.clock / get_sym_clk_khz(pdisp_encoder->state.priv.dp.link_rate);

    if (pdisp_encoder->policy.custom.dp.async_clock == DISP_DP_ASYNC_CLOCK_ASYNC) {
        //hw M
        dp_write_field(pdisp_encoder->index, Reg_Dpt_Reg1c, Register_Dpt_Reg1c_Start, 1,
            Mvid_Mod, 1);
    } else {
        //sw M
        dp_write_field(pdisp_encoder->index, Reg_Dpt_Reg1c, Register_Dpt_Reg1c_Start, 1,
            Mvid_Mod, 0);
    }

    dp_write_field(pdisp_encoder->index, Reg_Dpt_Reg21, Register_Dpt_Reg21_Start, 1,
        Mvid, M);

    dp_write_field(pdisp_encoder->index, Reg_Dpt_Reg22, Register_Dpt_Reg22_Start, 1,
        Nvid, DP_VID_N);
}

/**
static disp_void_t encoder_dp_set_duration(pdisp_encoder_t pdisp_encoder, struct dp_lt_setting *lt)
{
    pdisp_device_t pdd = pdisp_encoder->disp_device;
    disp_crtc_t* disp_crtc = &pdd->disp_crtc[pdisp_encoder->state.crtc_index];
    disp_display_mode_t mode = {0};
    disp_u64_t hblank, dur_hblank, dur_htotal;
    disp_u32_t h_width;

    crtc_get_property_hw_mode(&disp_crtc->property, &mode);
    hblank = mode.htotal - mode.hdisplay;
    dur_hblank = disp_do_div((disp_u64_t)get_sym_clk_khz(lt->link_rate) * (disp_u64_t)hblank, mode.clock) - 64;
    dur_htotal = disp_do_div((disp_u64_t)get_sym_clk_khz(lt->link_rate) * (disp_u64_t)mode.htotal, mode.clock) - 64;

    //sw duration
    dp_write_field(pdisp_encoder->index, Reg_Dpt_Reg1c, Register_Dpt_Reg1c_Start, 3,
        Htotal_Dur, dur_htotal,
        Hblank_Dur, dur_hblank,
        Auto_Dur, 0);

    //hw duration
    dp_write_field(pdisp_encoder->index, Reg_Dpt_Reg1c, Register_Dpt_Reg1c_Start, 1,
        Auto_Dur, 1);
}
**/

static DISP_STATUS dp_encoder_setup_link(pdisp_encoder_t pdisp_encoder)
{
    encoder_dp_set_tu(pdisp_encoder);
    encoder_dp_set_mvid_nvid(pdisp_encoder);

    //not need caculate duration now
    //encoder_dp_set_duration(pdisp_encoder, lt);

    return DISP_OK;
}

static disp_void_t dp_encoder_dpms_policy_adjust_by_cts(pdisp_encoder_t pdisp_encoder)
{
    struct disp_dp_policy* dp_policy = &pdisp_encoder->policy.custom.dp;
    struct disp_dp_cts_data* pdp_cts = &dp_policy->cts_data;

    if (dp_policy->flag.bits.cts_valid){
        if(pdp_cts->valid.bits.link_rate_valid)
        {
            switch(pdp_cts->link_rate)
            {
                case DISP_DP_LINK_RATE_TYPE_2_7:
                    dp_policy->link_rate = DP_LINK_BW_2_7;
                    break;
                case DISP_DP_LINK_RATE_TYPE_5_4:
                    dp_policy->link_rate = DP_LINK_BW_5_4;
                    break;
                case DISP_DP_LINK_RATE_TYPE_8_1:
                    dp_policy->link_rate = DP_LINK_BW_8_1;
                    break;
                case DISP_DP_LINK_RATE_TYPE_1_62:
                default:
                    dp_policy->link_rate = DP_LINK_BW_1_62;
                    break;
            }
        }
        dp_policy->lane_count = pdp_cts->valid.bits.lane_count_valid ? pdp_cts->lane_count : dp_policy->lane_count;
        dp_policy->tps = pdp_cts->valid.bits.tps_valid ? pdp_cts->tps : dp_policy->tps;
        dp_policy->async_clock = pdp_cts->valid.bits.async_clock_valid ? pdp_cts->async_clock : dp_policy->async_clock;
        dp_policy->range_type = pdp_cts->valid.bits.range_type_valid ? pdp_cts->range_type : dp_policy->range_type;
        dp_policy->ycbcr_coefficients = pdp_cts->valid.bits.ycbcr_coefficients_valid ? pdp_cts->ycbcr_coefficients : dp_policy->ycbcr_coefficients;
        dp_policy->enhanced_frame_mode = pdp_cts->valid.bits.enhanced_frame_valid ? pdp_cts->enhanced_frame_mode : dp_policy->enhanced_frame_mode;
    }
}

static disp_void_t dp_encoder_dpms_policy_update(pdisp_encoder_t pdisp_encoder)
{
    pdisp_device_t pdd = pdisp_encoder->disp_device;
    pdisp_connector_t disp_connector = &pdd->disp_connector[pdisp_encoder->state.connector_index];
    pdisp_dp_sink_caps_t sink_caps = (pdisp_dp_sink_caps_t)disp_connector->disp_info.dp_sink_caps;
    disp_crtc_t* disp_crtc = &pdd->disp_crtc[pdisp_encoder->state.crtc_index];
    disp_u32_t max_link_rate, max_lane_count, bpp;
    disp_u32_t req_bandwidth, rbr_bandwidth, hbr_bandwidth, hbr2_bandwidth, hbr3_bandwidth;
    struct disp_encoder_policy* policy = &pdisp_encoder->policy;
    struct disp_dp_policy* dp_policy = &pdisp_encoder->policy.custom.dp;
    pdisp_dsc_t pdsc = (pdisp_dsc_t)pdisp_encoder->pdsc;

    max_link_rate = dp_policy->max_possible_link_rate;
    max_lane_count = dp_policy->max_possible_lane_count;

    if(pdsc && pdsc->func.need_enable_dsc && pdsc->func.need_enable_dsc(pdsc))
        bpp = pdsc->policy.bpp;
    else if(DISP_COLOR_FORMAT_TYPE_YUV422 == policy->color_format)
        bpp = policy->bpc * 2;
    else if(DISP_COLOR_FORMAT_TYPE_YUV420 == policy->color_format)
        bpp = policy->bpc * 3 / 2;
    else
        bpp = policy->bpc * 3;
    req_bandwidth = disp_crtc->property.hw_mode.clock * bpp / 8;//symbols in khz
    rbr_bandwidth = 162000 * max_lane_count;//162000 k symbols each lane
    hbr_bandwidth = 270000 * max_lane_count;
    hbr2_bandwidth = 540000 * max_lane_count;
    hbr3_bandwidth = 810000 * max_lane_count;

    dp_policy->bpp = (disp_u8_t)bpp;
    dp_policy->lane_count = (disp_u8_t)max_lane_count;//use max lane count

    //calc bandwith here, assume we use the max lane count
    if(pdd->flag.info.dp_cts)
        dp_policy->link_rate = (disp_u8_t)max_link_rate;
    else if (rbr_bandwidth > req_bandwidth)
        dp_policy->link_rate = DP_LINK_BW_1_62;
    else if (hbr_bandwidth > req_bandwidth && max_link_rate >= DP_LINK_BW_2_7)
        dp_policy->link_rate = DP_LINK_BW_2_7;
    else if (hbr2_bandwidth > req_bandwidth && max_link_rate >= DP_LINK_BW_5_4)
        dp_policy->link_rate = DP_LINK_BW_5_4;
    else if (hbr3_bandwidth > req_bandwidth && max_link_rate >= DP_LINK_BW_8_1)
        dp_policy->link_rate = DP_LINK_BW_8_1;
    else
    {
        disp_print_addition_file(DDM_WARNING, "dp[%u] lt warning: req_bandwidth[%u] too large\n",
            pdisp_encoder->index, req_bandwidth);
        dp_policy->link_rate = (disp_u8_t)max_link_rate;
    }

    if(disp_dp_tps4_supported(sink_caps->dpcd))
        dp_policy->tps = 4;
    else
        dp_policy->tps = disp_dp_tps3_supported(sink_caps->dpcd) ? 3 : 2;
    dp_policy->enhanced_frame_mode = disp_dp_enhanced_frame_cap(sink_caps->dpcd);

    if (dp_policy->flag.bits.cts_valid)
        dp_encoder_dpms_policy_adjust_by_cts(pdisp_encoder);

    dp_policy->flag.bits.ssc_on = (dp_policy->flag.bits.ssc_prefer &&
        disp_dp_max_downspread(sink_caps->dpcd)) ? 1 : 0;

    disp_print(DDM_TRACE, "dp lt info: cts[%u], fmt[%d] bpc[%u] bpp[%u] clk_khz[%d], lt_rate/count[%#x, %u], tps[%u], enhance[%d]\n", dp_policy->flag.bits.cts_valid,
        policy->color_format, policy->bpc, bpp, disp_crtc->property.hw_mode.clock, dp_policy->link_rate, dp_policy->lane_count, dp_policy->tps, dp_policy->enhanced_frame_mode);
}

static disp_void_t dp_encoder_dpms_prepare(pdisp_encoder_t pdisp_encoder)
{
    pdisp_dsc_t pdsc = (pdisp_dsc_t)pdisp_encoder->pdsc;
    disp_u8_t fec_en = pdisp_encoder->policy.flag.bits.fec_prefer ? DISP_TRUE : DISP_FALSE;

    if(pdsc && pdsc->func.policy_update)
        pdsc->func.policy_update(pdsc, pdisp_encoder->policy.custom.dp.max_possible_bandwidth_kbyte);
    if(pdsc && pdsc->func.need_enable_dsc)
        fec_en |= pdsc->func.need_enable_dsc(pdsc);
    pdisp_encoder->policy.flag.bits.fec_enable = fec_en ? 1 : 0;
    dp_encoder_dpms_policy_update(pdisp_encoder);
}

static DISP_STATUS dp_encoder_dpms(pdisp_encoder_t pdisp_encoder, DISP_POWER_STATE state)
{
    struct disp_encoder_state *encoder_state = &pdisp_encoder->state;
    pdisp_device_t pdd = (pdisp_device_t)pdisp_encoder->disp_device;
    DISP_POWER_STATE power_state = state;
    disp_u8_t timeout = DISP_DP_RETRY_LT_TIMES, lt_status = DISP_FALSE;
    disp_u8_t fec_en = pdisp_encoder->policy.flag.bits.fec_enable ? DISP_TRUE : DISP_FALSE;
    disp_u8_t ssc_on = pdisp_encoder->policy.custom.dp.flag.bits.ssc_on ? DISP_TRUE : DISP_FALSE;

    if (pdd->disp_connector[pdisp_encoder->index].flag.info.cheater)
    {   // Could not access dpcd, just set pll, skip lt.
        if (DISP_POWER_ON == power_state)
        {
            disp_pll_on(&pdd->disp_crtc[pdisp_encoder->index]);
        }
        else
        {
            //disp_pll_off(&pdd->disp_crtc[pdisp_encoder->index]);
        }
        disp_print_addition_file(DDM_KEY, "dp[%u] cheater dpms %s.\n", pdisp_encoder->index, state ? "on" : "off");
        return DISP_OK;
    }

    if(DISP_POWER_ON == power_state)
    {
        /* Before set pll, check dsc first. After that link training */
        disp_pll_on(&pdd->disp_crtc[pdisp_encoder->index]);

        if(pdd->flag.info.pz1_config)
            disp_dp_phy_pz1_setting(&pdisp_encoder->phy);
        dp_encoder_dp_enable(pdisp_encoder, DISP_TRUE);
        disp_dp_set_sink_power_state(&pdd->disp_connector[pdisp_encoder->index], DISP_TRUE);
        if(fec_en)
            disp_dp_set_sink_fec(&pdd->disp_connector[pdisp_encoder->index], DISP_TRUE);

        disp_print_addition_file(DDM_KEY, "dp[%u] dpms on: fmt[%d] bpc[%u] clrmt[%d] bpp[%u], fec[%u] ssc[%u] lr[%u] lc[%u]\n",
            pdisp_encoder->index, pdisp_encoder->policy.color_format, pdisp_encoder->policy.bpc,
            pdisp_encoder->policy.colorimetry, pdisp_encoder->policy.custom.dp.bpp, fec_en, ssc_on,
            pdisp_encoder->policy.custom.dp.link_rate, pdisp_encoder->policy.custom.dp.lane_count);

        timeout = (pdd->flag.info.dp_cts) ? DISP_DP_CTS_RETRY_LT_TIMES : DISP_DP_RETRY_LT_TIMES;
        lt_status = DISP_FALSE;
        while (!lt_status && timeout)
        {
            lt_status = dp_encoder_link_training(pdisp_encoder);
            timeout--;
            disp_print(DDM_TRACE, "dp[%u] lt_status[%u] timeout[%u]\n",
                pdisp_encoder->index, lt_status, timeout);
        }
        if (lt_status)
        {
            encoder_dp_get_ink_rate_in_use(pdisp_encoder);
            dp_encoder_setup_link(pdisp_encoder);
            disp_dp_set_sink_ssc(&pdd->disp_connector[pdisp_encoder->index], ssc_on);

            if(fec_en)
                dp_encoder_fec_enable(pdisp_encoder, DISP_TRUE);
            dp_encoder_set_dsc(pdisp_encoder, DISP_TRUE);
            if(pdisp_encoder->policy.custom.dp.flag.bits.auto_tu_ratio)
                dp_encoder_wait_for_tu_ratio_ready(pdisp_encoder);

            dp_encoder_video_enable(pdisp_encoder, DISP_TRUE);
            dp_encoder_audio_enable(pdisp_encoder, DISP_TRUE);
            disp_print(DDM_TRACE, "dp[%u] lt success\n", pdisp_encoder->index);
        }
        else
        {
            power_state = DISP_POWER_OFF;
            disp_print_addition_file(DDM_WARNING, "dp[%u] lt failed!\n", pdisp_encoder->index);
        }
    }

    if(DISP_POWER_OFF == power_state)
    {
        dp_encoder_video_enable(pdisp_encoder, DISP_FALSE);
        dp_encoder_audio_enable(pdisp_encoder, DISP_FALSE);
        dp_encoder_set_dsc(pdisp_encoder, DISP_FALSE);
        if(pdisp_encoder->state.flag.bits.fec_on)
        {
            dp_encoder_fec_enable(pdisp_encoder, DISP_FALSE);
            dp_wait_fec_dis_complete(pdisp_encoder);
            disp_dp_set_sink_fec(&pdd->disp_connector[pdisp_encoder->index], DISP_FALSE);
        }
        dp_encoder_dp_enable(pdisp_encoder, DISP_FALSE);
        disp_dp_phy_reset(&pdisp_encoder->phy, DISP_TRUE);

        /* Do not pll off, otherwise 2.7Gbps could not light on.
         * In the future, calculate dsc & set pll in crtc, then we may off pll.
         */
        //disp_pll_off(&pdd->disp_crtc[pdisp_encoder->index]);

        disp_print_addition_file(DDM_KEY, "dp[%u] dpms off\n", pdisp_encoder->index);
    }

    encoder_state->power_state = power_state;
    return DISP_OK;
}

static DISP_STATUS dp_encoder_hw_init(pdisp_encoder_t pdisp_encoder)
{
    disp_dp_phy_init(&pdisp_encoder->phy);

    return DISP_OK;
}

static DISP_STATUS dp_encoder_hw_deinit(pdisp_encoder_t pdisp_encoder)
{
    disp_print(DDM_TRACE, "tofill: %#x\n", pdisp_encoder);
    return DISP_OK;
}

static DISP_STATUS dp_encoder_policy_config(pdisp_encoder_t pdisp_encoder, DISP_COLORIMETRY colorimetry)
{
    struct disp_encoder_policy* policy = &pdisp_encoder->policy;

    if(pdisp_encoder->type != DISP_ENCODER_DP)
        return DISP_OK;

    switch(colorimetry)
    {
        case DISP_COLORIMETRY_YCC_NONE:
        case DISP_COLORIMETRY_YCC_SMPTE_170M:
        case DISP_COLORIMETRY_XVYCC_601:
        case DISP_COLORIMETRY_SYCC_601:
        case DISP_COLORIMETRY_YCC_ADOBE_601:
            policy->custom.dp.ycbcr_coefficients = DISP_YUV_COEFFICIENTS_ITU601;
            break;
        case DISP_COLORIMETRY_YCC_BT709:
        case DISP_COLORIMETRY_XVYCC_709:
            policy->custom.dp.ycbcr_coefficients = DISP_YUV_COEFFICIENTS_ITU709;
            break;
        case DISP_COLORIMETRY_YCC_BT2020:
        case DISP_COLORIMETRY_CYCC_BT2020:
        case DISP_COLORIMETRY_SRGB:
        case DISP_COLORIMETRY_RGB_ADOBE:
        case DISP_COLORIMETRY_RGB_BT2020:
        case DISP_COLORIMETRY_RGB_DCI:
        case DISP_COLORIMETRY_RGB_DCI_D65:
        default:
            break;
    }

    return DISP_OK;
}

/* We could disable PSR module or keep enable after psr_exit */
static disp_void_t encoder_edp_psr_module_enable(pdisp_encoder_t pdisp_encoder, disp_u8_t enable)
{
    pdisp_device_t pdd = pdisp_encoder->disp_device;
    pdisp_connector_t pdisp_connector = &pdd->disp_connector[pdisp_encoder->index];
    pdisp_edp_info_t pedp = (pdisp_edp_info_t)pdisp_connector->disp_info.edp_info;

    dp_write_field(pdisp_encoder->index, Reg_Dpt_Reg26, Register_Dpt_Reg26_Start, 1,
        Psr_En, enable);
    pdisp_encoder->state.priv.dp.flag.bits.psr_module_enable = enable;

    pedp->psr.set_active = enable;
    if (enable != disp_edp_psr_aux_is_enable(pdisp_connector->ddc))
    {   /* 1. disable module then disable aux.
         * 2. enable module, but aux not enable yet, re-enable aux again.
         */
        disp_edp_psr_aux_set(pdisp_connector->ddc, &pedp->psr);
    }
}

static disp_void_t encoder_edp_psr_entry(pdisp_encoder_t pdisp_encoder)
{
    pdisp_device_t pdd = pdisp_encoder->disp_device;
    pdisp_connector_t pdisp_connector = &pdd->disp_connector[pdisp_encoder->index];
    pdisp_edp_info_t pedp = (pdisp_edp_info_t)pdisp_connector->disp_info.edp_info;
    disp_u32_t reactive_mode=0, ml_on=0, ml_hw_send=0, ml_send_num=0, ml_standby=0, crc_en=0, crc_hw=0;

    // main link policy
    ml_on = pedp->psr.prefer_main_link_remain;
    if (ml_on)
    {
        ml_hw_send = 1;  // If set 0, need SW trigger Ml_Sleep_Req to send
        ml_send_num = 7; // Set to max now, check with HW if we use main link on
        ml_standby = 1;  // 0-sleep, 1-standby
    }
    reactive_mode = 1; // 0-lt, 1-idle pattern

    // crc policy
    crc_en = pedp->psr.prefer_crc_on;
    crc_hw = 1; // If enable crc, HW calculate(if SW calculate, no reg to write, need use fifo sdp)

    dp_write_field(pdisp_encoder->index, Reg_Dpt_Reg26, Register_Dpt_Reg26_Start, 7,
        Psr_Enter, 1, // HW auto clear
        Psr_Frame_Start_Sel, pedp->psr.prefer_frame_start_vsync,
        Psr_Enter_Mode, pedp->psr.set_frame_next,
        Psr_Reactive_Mode, reactive_mode,
        // Psr_Reactive_Idle_Pat_Num, 5, // Use default value, spec use 5 idle pattern
        Psr_Ml_On, ml_on,
        Psr_Crc_En, crc_en,
        Psr_Crc_Valid_O, crc_hw);
    if (ml_on)
    {
        dp_write_field(pdisp_encoder->index, Reg_Dpt_Reg26, Register_Dpt_Reg26_Start, 3,
            Psr_Send_Ml_Sleep, ml_hw_send,
            Psr_Ml_Sleep_Num, ml_send_num,
            Psr_Ml_Sleep_Type, ml_standby);
    }
}

static disp_void_t encoder_edp_psr_update_single_frame(pdisp_encoder_t pdisp_encoder)
{
    pdisp_device_t pdd = pdisp_encoder->disp_device;
    disp_u32_t direct_update = 1; // 0-inactive first then active, 1-direct update RFB

    dp_write_field(pdisp_encoder->index, Reg_Dpt_Reg26, Register_Dpt_Reg26_Start, 2,
        Psr_Update, 1, // HW auto clear
        Psr_Update_Mode, direct_update);
}

static disp_void_t encoder_edp_psr_update_burst_enable(pdisp_encoder_t pdisp_encoder, disp_u8_t enable)
{
    pdisp_device_t pdd = pdisp_encoder->disp_device;

    dp_write_field(pdisp_encoder->index, Reg_Dpt_Reg26, Register_Dpt_Reg26_Start, 1,
        Psr_Burst_Update, enable);
}

static disp_u8_t encoder_edp_psr_update_burst_sync(pdisp_encoder_t pdisp_encoder)
{
    pdisp_device_t pdd = pdisp_encoder->disp_device;
    disp_u32_t hvae_send=0;

    dp_read_field(pdisp_encoder->index, Reg_Dpt_Reg26, Register_Dpt_Reg26_Start, 1,
        Psr_Burst_Update_Start, &hvae_send); // 1-frame sdp have send
    if (hvae_send)
    {
        dp_write_field(pdisp_encoder->index, Reg_Dpt_Reg26, Register_Dpt_Reg26_Start, 1,
            Psr_Burst_Update_Start, 1); // clear
    }
    return hvae_send ? DISP_TRUE : DISP_FALSE;
}

static disp_void_t encoder_edp_psr_sleep(pdisp_encoder_t pdisp_encoder)
{   // Note: set dpcd before set HW
    pdisp_device_t pdd = pdisp_encoder->disp_device;
    dp_write_field(pdisp_encoder->index, Reg_Dpt_Reg26, Register_Dpt_Reg26_Start, 1,
        Psr_Sleep, 1); // HW auto clear when wake ?
}

static disp_void_t encoder_edp_psr_exit(pdisp_encoder_t pdisp_encoder)
{
    pdisp_device_t pdd = pdisp_encoder->disp_device;
    dp_write_field(pdisp_encoder->index, Reg_Dpt_Reg26, Register_Dpt_Reg26_Start, 1,
        Psr_Exit, 1); // HW auto clear
}

static disp_void_t encoder_edp_psr_hw_status(pdisp_encoder_t pdisp_encoder)
{
    pdisp_device_t pdd = pdisp_encoder->disp_device;
    disp_u32_t crc_0, crc_12, psr_ml_off, psr_hw_state;

    dp_read_field(pdisp_encoder->index, Reg_Dpt_Reg27, Register_Dpt_Reg27_Start, 3,
        Psr_Crc_0, &crc_0,
        Psr_Ml_Off_St, &psr_ml_off, // 1-psr main link off
        Psr_Fsm_State_O, &psr_hw_state); // HW module state
    dp_read_field(pdisp_encoder->index, Reg_Dpt_Reg28, Register_Dpt_Reg28_Start, 1,
        Psr_Crc_1, &crc_12);

    disp_print(DDM_INFO, "encoder_edp_psr_hw_status, hw_state[%x], ml_off[%x] crc[%x %x %x]\n",
        psr_hw_state, psr_ml_off, crc_0, (crc_12 >> 16) & 0xffff, crc_12 & 0xffff);
}

static disp_void_t encoder_edp_fifo_psr_sdp(pdisp_encoder_t pdisp_encoder)
{
    struct dp_encoder_state *pdp_st = &pdisp_encoder->state.priv.dp;
    disp_u8_t fifo_index = 0;

    if (pdp_st->flag.bits.fifo_in_use)
    {
        if (pdp_st->flag.bits.fifo_have_vsc)
            fifo_index = pdp_st->fifo_index_vsc;
        else
        {
            fifo_index = pdp_st->fifo_index_idle;
            pdp_st->fifo_index_idle++;
        }
    }
    else
    {   // sdp fifo not in use, direct use fifo group 0, index 0
        fifo_index = 0;
        pdp_st->flag.bits.fifo_in_use = 1;
        pdp_st->flag.bits.fifo_idle_group = 1;
        pdp_st->fifo_index_idle = 1;
    }
    pdp_st->fifo_index_vsc = fifo_index;
    pdp_st->flag.bits.fifo_have_vsc = 1;

    encoder_sdp_set_vsc(pdisp_encoder, fifo_index);
}

static DISP_STATUS encoder_edp_psr_set(pdisp_encoder_t pdisp_encoder, DISP_PSR_SET_TYPE psr_type)
{
    pdisp_device_t pdd = pdisp_encoder->disp_device;
    pdisp_connector_t pdisp_connector = &pdd->disp_connector[pdisp_encoder->index];
    pdisp_edp_info_t pedp = (pdisp_edp_info_t)pdisp_connector->disp_info.edp_info;
    DISP_PSR_SET_TYPE cur_psr = DISP_PSR_EXIT;

    cur_psr = pdisp_encoder->state.priv.dp.psr_type;
    pedp->psr.set_update_rfb = 0;
    switch(psr_type)
    {
    case DISP_PSR_EXIT:
        encoder_edp_psr_exit(pdisp_encoder);
        break;
    case DISP_PSR_ENTRY:
        encoder_edp_psr_entry(pdisp_encoder);
        pedp->psr.set_update_rfb = (DISP_PSR_EXIT == cur_psr) ?  1 : 0;
        break;
    case DISP_PSR_UPDATE:
        encoder_edp_psr_update_single_frame(pdisp_encoder);
        pedp->psr.set_update_rfb = 1;
        break;
    case DISP_PSR_UPDATE_BURST_START:
        encoder_edp_psr_update_burst_enable(pdisp_encoder, DISP_TRUE);
        pedp->psr.set_update_rfb = 1;
        break;
    case DISP_PSR_UPDATE_BURST_END:
        encoder_edp_psr_update_burst_enable(pdisp_encoder, DISP_FALSE);
        break;
    case DISP_PSR_SLEEP:
        encoder_edp_psr_sleep(pdisp_encoder);
        break;
    default:
        disp_print(DDM_WARNING, "encoder_edp_psr_set, invalid psr_type[%x]!\n", psr_type);
        return -DISP_EINVAL;
    }

    if (!pdisp_encoder->state.priv.dp.flag.bits.psr_module_enable)
        encoder_edp_psr_module_enable(pdisp_encoder, DISP_TRUE);
    pdisp_encoder->state.priv.dp.psr_type = psr_type;

    if (pedp->psr.prefer_sw_sdp && (DISP_PSR_EXIT == psr_type ||
        DISP_PSR_ENTRY == psr_type || DISP_PSR_UPDATE == psr_type))
    {
        pdisp_encoder->state.priv.dp.flag.bits.sdp_vsc_psr_once = 1;
        encoder_edp_fifo_psr_sdp(pdisp_encoder);
    }

    return DISP_OK;
}

struct disp_encoder_func dp_encoder_func =
{
    .mode_set = dp_encoder_mode_set,
    .dpms_prepare = dp_encoder_dpms_prepare,
    .dpms = dp_encoder_dpms,
    .hw_init = dp_encoder_hw_init,
    .hw_deinit = dp_encoder_hw_deinit,
    .policy_config = dp_encoder_policy_config,
    .edp_psr_set = encoder_edp_psr_set,
    .set_audio_info = encoder_dp_set_sdp_audio,
};

disp_void_t disp_dp_encoder_sw_init(pdisp_void_t pdisp_encoder)
{
    pdisp_encoder_t pdp_encoder = (pdisp_encoder_t)pdisp_encoder;
    pdisp_device_t pdd = (pdisp_device_t)pdp_encoder->disp_device;
    struct dp_encoder_caps *caps = &pdp_encoder->caps.priv.dp;
    struct dp_phy_t *phy = &pdp_encoder->phy;
    struct disp_dp_policy* dp_policy = &pdp_encoder->policy.custom.dp;

    pdp_encoder->func = &dp_encoder_func;

    caps->source_max_out_bpp = 10 * 3; // encoder max 10 bpc
    caps->source_max_lane_count = 4;
    caps->source_max_link_rate = DP_LINK_BW_8_1;
    caps->source_support_TPS3 = DISP_TRUE;

    phy->index = pdp_encoder->index;
    phy->pdisp_deivce = pdp_encoder->disp_device;
    phy->ssc_en = DISP_FALSE;

    /* HW Tu Ratio have a little loss of accuracy when divide 3
     * HW difficult to handle divide 3, so rollback to SW calculate Tu Ratio
     */
    dp_policy->TU_size = 64;
    dp_policy->range_type = DISP_RANGE_TYPE_CEA;
    dp_policy->async_clock = DISP_DP_ASYNC_CLOCK_ASYNC; // Mvid_Mod calculate by HW
    dp_policy->flag.bits.ssc_prefer = pdd->flag.info.dp_cts ? 0 : 1; // CTS ssc_on will lt failed. Trace later.
    dp_policy->flag.bits.auto_tu_ratio = 0;
}
