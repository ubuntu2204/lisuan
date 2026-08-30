/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#include "disp_hdmi_encoder.h"
#include "disp_device.h"

#define disp_print(level, log, ...) disp_print_ex(DISP_PRINT_MASK_ENCODER_HDMI, level, log, ##__VA_ARGS__)

static HDMI_3D_STRUCTURE_TYPE get_s3d_structure_from_display_mode(pdisp_display_mode_t mode)
{
    disp_u32_t layout = mode->flags & DISP_MODE_FLAG_3D_MASK;

    switch(layout)
    {
        case DISP_MODE_FLAG_3D_FRAME_PACKING:
            return HDMI_3D_STRUCTURE_FRAME_PACKING;
        case DISP_MODE_FLAG_3D_FIELD_ALTERNATIVE:
            return HDMI_3D_STRUCTURE_FIELD_ALTERNATIVE;
        case DISP_MODE_FLAG_3D_LINE_ALTERNATIVE:
            return HDMI_3D_STRUCTURE_LINE_ALTERNATIVE;
        case DISP_MODE_FLAG_3D_SIDE_BY_SIDE_FULL:
            return HDMI_3D_STRUCTURE_SIDE_BY_SIDE_FULL;
        case DISP_MODE_FLAG_3D_L_DEPTH:
            return HDMI_3D_STRUCTURE_L_DEPTH;
        case DISP_MODE_FLAG_3D_L_DEPTH_GFX_GFX_DEPTH:
            return HDMI_3D_STRUCTURE_L_DEPTH_GFX_GFX_DEPTH;
        case DISP_MODE_FLAG_3D_TOP_AND_BOTTOM:
            return HDMI_3D_STRUCTURE_TOP_AND_BOTTOM;
        case DISP_MODE_FLAG_3D_SIDE_BY_SIDE_HALF:
            return HDMI_3D_STRUCTURE_SIDE_BY_SIDE_HALF;
        default:
            disp_print(DDM_WARNING, "get_s3d_structure_from_display_mode, invalid 3d structural, modeFlag[%#x]", mode->flags);
            return HDMI_3D_STRUCTURE_INVALID;
    }
}

static HDMI_COLORSPACE_TYPE get_hdmi_color_space_from_crtc(pdisp_encoder_t pencoder,
    HDMI_COLORIMETRY_TYPE* pcolorimetry, HDMI_EXTENDED_COLORIMETRY_TYPE* pextended_colorimetry)
{
    DISP_COLORIMETRY colorimetry = pencoder->policy.colorimetry;

    switch(colorimetry)
    {
        case DISP_COLORIMETRY_SRGB:
            *pcolorimetry = HDMI_COLORIMETRY_NONE;
            break;
        case DISP_COLORIMETRY_YCC_NONE:
            *pcolorimetry = HDMI_COLORIMETRY_NONE;
            break;
        case DISP_COLORIMETRY_YCC_SMPTE_170M:
            *pcolorimetry = HDMI_COLORIMETRY_ITU_601;
            break;
        case DISP_COLORIMETRY_YCC_BT709:
            *pcolorimetry = HDMI_COLORIMETRY_ITU_709;
            break;
        default: // extend colorimetry
            *pcolorimetry = HDMI_COLORIMETRY_EXTENDED;
            switch(colorimetry)
            {
                case DISP_COLORIMETRY_XVYCC_601:
                    *pextended_colorimetry = HDMI_EXTENDED_COLORIMETRY_XV_YCC_601;
                    break;
                case DISP_COLORIMETRY_XVYCC_709:
                    *pextended_colorimetry = HDMI_EXTENDED_COLORIMETRY_XV_YCC_709;
                    break;
                case DISP_COLORIMETRY_SYCC_601:
                    *pextended_colorimetry = HDMI_EXTENDED_COLORIMETRY_S_YCC_601;
                    break;
                case DISP_COLORIMETRY_YCC_ADOBE_601:
                    *pextended_colorimetry = HDMI_EXTENDED_COLORIMETRY_OPYCC_601;
                    break;
                case DISP_COLORIMETRY_RGB_ADOBE:
                    *pextended_colorimetry = HDMI_EXTENDED_COLORIMETRY_OPRGB;
                    break;
                case DISP_COLORIMETRY_CYCC_BT2020:
                    *pextended_colorimetry = HDMI_EXTENDED_COLORIMETRY_BT2020_CONST_LUM;
                    break;
                case DISP_COLORIMETRY_YCC_BT2020:
                    *pextended_colorimetry = HDMI_EXTENDED_COLORIMETRY_BT2020;
                    break;
                case DISP_COLORIMETRY_RGB_BT2020:
                    *pextended_colorimetry = HDMI_EXTENDED_COLORIMETRY_BT2020;
                    break;
                case DISP_COLORIMETRY_RGB_DCI_D65:
                    *pextended_colorimetry = HDMI_EXTENDED_COLORIMETRY_ADDITIONAL_D65;
                    break;
                case DISP_COLORIMETRY_RGB_DCI:
                    *pextended_colorimetry = HDMI_EXTENDED_COLORIMETRY_ADDITIONAL_THEATER;
                    break;
                default:
                    disp_print(DDM_WARNING, "hdmi construct, colorimetry[%u] not support", colorimetry);
                    break;
            }
            break;
    }

    // tofill: IDO defined

    switch(pencoder->policy.color_format)
    {
        case DISP_COLOR_FORMAT_TYPE_RGB:
            return HDMI_COLORSPACE_RGB;
        case DISP_COLOR_FORMAT_TYPE_YUV444:
            return HDMI_COLORSPACE_YUV444;
        case DISP_COLOR_FORMAT_TYPE_YUV422:
            return HDMI_COLORSPACE_YUV422;
        case DISP_COLOR_FORMAT_TYPE_YUV420:
            return HDMI_COLORSPACE_YUV420;
        default:
            disp_print(DDM_WARNING, "hdmi construct, output_signal[%u] not support", pencoder->policy.color_format);
    }
    return HDMI_COLORSPACE_RESERVED4;
}

static DISP_STATUS hdmi_encoder_mode_set(pdisp_encoder_t pdisp_encoder)
{
    disp_print(DDM_TRACE, "tofill: %#x\n", pdisp_encoder);
    //hv sync
    //output signal
    //color depth
    //scramble
    //dvi mode
    return DISP_OK;
}

static DISP_STATUS hdmi_encoder_dpms(pdisp_encoder_t pdisp_encoder, DISP_POWER_STATE state)
{
    //hdmi module enable

    hdmi_encoder_set_infoframe(pdisp_encoder, state);

    //scdc
    //avmute
    //phy

    return DISP_OK;
}

static DISP_STATUS hdmi_encoder_hw_init(pdisp_encoder_t pdisp_encoder)
{
    disp_print(DDM_TRACE, "tofill: %#x\n", pdisp_encoder);
    return DISP_OK;
}

static DISP_STATUS hdmi_encoder_hw_deinit(pdisp_encoder_t pdisp_encoder)
{
    disp_print(DDM_TRACE, "tofill: %#x\n", pdisp_encoder);
    return DISP_OK;
}

struct disp_encoder_func hdmi_encoder_func =
{
    .mode_set = hdmi_encoder_mode_set,
    .dpms = hdmi_encoder_dpms,
    .hw_init = hdmi_encoder_hw_init,
    .hw_deinit = hdmi_encoder_hw_deinit,
    .policy_config = DISP_NULL,
};

disp_void_t disp_hdmi_encoder_sw_init(pdisp_void_t pdisp_encoder)
{
    pdisp_encoder_t phdmi_encoder = (pdisp_encoder_t)pdisp_encoder;
    struct hdmi_encoder_caps *caps = &phdmi_encoder->caps.priv.hdmi;

    phdmi_encoder->func = &hdmi_encoder_func;

    caps->support_read_request = DISP_TRUE;
    caps->support_scrambling = DISP_TRUE;
}

// infoFrame vendor hdmi
DISP_STATUS hdmi_vendor_infoframe_pack(phdmi_vendor_infoframe_t pframe, pdisp_void_t pbuffer, disp_u32_t size)
{
    pdisp_u8_t pcur = (pdisp_u8_t)pbuffer;
    disp_print(DDM_TRACE, "hdmi_vendor_infoframe_pack entry, tofill: %d\n", size);

    pcur += HDMI_INFOFRAME_HEADER_SIZE;
    *pcur = pframe->ieee_code & 0xff;
    *(++pcur) = (disp_u8_t)((pframe->ieee_code & 0xff00) >> 8);
    *(++pcur) = (disp_u8_t)((pframe->ieee_code & 0xff0000) >> 16);

    *(++pcur) = (pframe->video_format << 5) & 0xe0; // HDMI_Video_Format
    
    if(HDMI_VIDEO_FORMAT_EXTENDED_RESOLUTION == pframe->video_format)
        *(++pcur) = pframe->hdmi_vic;
    else if(HDMI_VIDEO_FORMAT_3D_STRUCTURE == pframe->video_format)
    {
        *(++pcur) = (pframe->s3d_struct << 4) & 0xf0;
        if(pframe->s3d_struct > HDMI_3D_STRUCTURE_SIDE_BY_SIDE_HALF)
            *(++pcur) = (pframe->s3d_ext_data << 4) & 0xf0;
    }

    return DISP_OK;
}

DISP_STATUS hdmi_vendor_infoframe_unpack(phdmi_vendor_infoframe_t pframe, pdisp_void_t pbuffer, disp_u32_t size)
{
    pdisp_u8_t pcur = (pdisp_u8_t)pbuffer;
    disp_print(DDM_TRACE, "hdmi_vendor_infoframe_unpack entry, tofill: %d\n", size);

    pcur += HDMI_INFOFRAME_HEADER_SIZE + 3; // skip header + checksum + IEEE
    pframe->video_format = (*pcur & 0xe0) >> 5;

    if(HDMI_VIDEO_FORMAT_EXTENDED_RESOLUTION == pframe->video_format)
        pframe->hdmi_vic = *(++pcur);
    else if(HDMI_VIDEO_FORMAT_3D_STRUCTURE == pframe->video_format)
    {
        pframe->s3d_struct = (*(++pcur) & 0xf0) >> 4;
        if(pframe->s3d_struct > HDMI_3D_STRUCTURE_SIDE_BY_SIDE_HALF)
            pframe->s3d_ext_data = (*(++pcur) & 0xf0) >> 4;
    }

    return DISP_OK;
}

DISP_STATUS hdmi_vendor_infoframe_construct(pdisp_void_t pdisp_encoder, phdmi_vendor_infoframe_t pframe)
{
    pdisp_encoder_t pencoder = (pdisp_encoder_t)pdisp_encoder;
    pdisp_device_t pdevice = (pdisp_device_t)pencoder->disp_device;
    pdisp_crtc_t pcrtc = &pdevice->disp_crtc[pencoder->index];
    pdisp_display_mode_t pmode = &pcrtc->property.hw_mode;
    disp_print(DDM_TRACE, "hdmi_vendor_infoframe_construct entry\n");

    pframe->type = HDMI_INFOFRAME_TYPE_VENDOR;
    pframe->length = HDMI_VENDOR_INFOFRAME_SIZE;
    pframe->version = HDMI_VENDOR_INFOFRAME_VER;
    pframe->ieee_code = HDMI_IEEE_OUI;

    if(pmode->flags & DISP_MODE_FLAG_3D_MASK)
    {
        pframe->video_format = HDMI_VIDEO_FORMAT_3D_STRUCTURE;
        pframe->s3d_struct = get_s3d_structure_from_display_mode(pmode);
        pframe->length++; // 0x5
        if(pframe->s3d_struct > HDMI_3D_STRUCTURE_SIDE_BY_SIDE_HALF)
        {
            pframe->s3d_ext_data = 0; // tofill: when support 3D ext
            pframe->length++; // 0x6
        }
    }
    else
    {
        /* 4k mode(2D) use HDMI_VIC(1 2 3 4), VIC(95/105 94/104 93/103 98) use for 3D */
        if(pmode->hdmi_vic)
        {
            pframe->video_format = HDMI_VIDEO_FORMAT_EXTENDED_RESOLUTION;
            pframe->hdmi_vic = pmode->hdmi_vic;
            pframe->length++; // 0x5
        }
        else
            pframe->video_format = HDMI_VIDEO_FORMAT_NO_ADDITION;
    }        

    return DISP_OK;
}

// infoFrame vendor hdmi_forum
DISP_STATUS hdmi_forum_vendor_infoframe_pack(phdmi_forum_vendor_infoframe_t pframe, pdisp_void_t pbuffer, disp_u32_t size)
{
    pdisp_u8_t pcur = (pdisp_u8_t)pbuffer;
    disp_print(DDM_TRACE, "hdmi_forum_vendor_infoframe_pack entry, tofill: %d\n", size);

    pcur += HDMI_INFOFRAME_HEADER_SIZE;
    *pcur = (disp_u8_t)(pframe->ieee_code & 0xff);
    *(++pcur) = (disp_u8_t)((pframe->ieee_code & 0xff00) >> 8);
    *(++pcur) = (disp_u8_t)((pframe->ieee_code & 0xff0000) >> 16);
    *(++pcur) = pframe->forum_version;

    *(++pcur) |= (pframe->s3d_flag & HDMI_3D_STRUCTURE_FLAG_3D_VALID) ? 0x01 : 0;

    *(++pcur) |= (pframe->s3d_struct << 4);
    *pcur |= (pframe->s3d_flag & HDMI_3D_STRUCTURE_FLAG_ADDITION) ? 0x08 : 0;
    *pcur |= (pframe->s3d_flag & HDMI_3D_STRUCTURE_FLAG_DISPARITY) ? 0x04 : 0;
    *pcur |= (pframe->s3d_flag & HDMI_3D_STRUCTURE_FLAG_META) ? 0x02 : 0;

    *(++pcur) |= (pframe->s3d_ext_data << 4);

    *(++pcur) |= (pframe->s3d_flag & HDMI_3D_STRUCTURE_FLAG_3D_DUAL_VIEW) ? 0x10 : 0;
    *pcur |= (pframe->view_dependency << 2);
    *pcur |= (pframe->view_preferred << 2);

    *(++pcur) |= (1 << 5); // disparty_ver=1
    *pcur |= pframe->disparty_len;
    *pcur += pframe->disparty_len; // tofill: disparty_data

    *(++pcur) |= (1 << 5); // meta_ver=1
    *pcur |= pframe->meta_len;
    *pcur += pframe->meta_len; // tofill: meta_data

    return DISP_OK;
}

DISP_STATUS hdmi_forum_vendor_infoframe_unpack(phdmi_forum_vendor_infoframe_t pframe, pdisp_void_t pbuffer, disp_u32_t size)
{
    pdisp_u8_t pcur = (pdisp_u8_t)pbuffer;
    disp_print(DDM_TRACE, "hdmi_forum_vendor_infoframe_unpack entry, tofill: %d\n", size);

    pcur += HDMI_INFOFRAME_HEADER_SIZE + 3;
    pframe->forum_version = *(++pcur);
    
    pframe->s3d_flag |= (*(++pcur) & 0x01) ? HDMI_3D_STRUCTURE_FLAG_3D_VALID : 0;

    pframe->s3d_struct = (*(++pcur) & 0xf0) >> 4; // 3D_F_Structure
    pframe->s3d_flag |= (*pcur & 0x08) ? HDMI_3D_STRUCTURE_FLAG_ADDITION : 0;
    pframe->s3d_flag |= (*pcur & 0x04) ? HDMI_3D_STRUCTURE_FLAG_DISPARITY : 0;
    pframe->s3d_flag |= (*pcur & 0x02) ? HDMI_3D_STRUCTURE_FLAG_META : 0;

    pframe->s3d_ext_data = (*(++pcur) & 0xf0) >> 4;

    ++pcur;
    pframe->s3d_flag |= (*pcur & 0x10) ? HDMI_3D_STRUCTURE_FLAG_3D_DUAL_VIEW : 0;
    pframe->view_dependency = (*pcur & 0x0c) >> 2;
    pframe->view_preferred = HDMI_3D_VIEW_PREFERRED_2D_VIEW_NONE + (*pcur & 0x03);

    ++pcur;
    // = (*pcur & 0xe0) >> 5; // 3D_DisparityData_version = 1
    pframe->disparty_len = (*pcur & 0x1f);
    pcur += pframe->disparty_len; // 3D_DisparityData x 3D_DisparityData_length

    // = (*pcur & 0xe0) >> 5; // 3D_Metadata_type = 1
    pframe->meta_len = (*pcur & 0x1f);
    pcur += pframe->meta_len; // 3D_Metadata x 3D_Metadata_Length

    // check length
    if(pframe->length != 10 + pframe->disparty_len + pframe->meta_len)
        disp_print(DDM_WARNING, "hdmi_forum_vendor_infoframe_unpack, length mismatch [%u, %u, %u]\n", 
            pframe->length, pframe->disparty_len, pframe->meta_len);

    return DISP_OK;
}

DISP_STATUS hdmi_forum_vendor_infoframe_construct(pdisp_void_t pdisp_encoder, phdmi_forum_vendor_infoframe_t pframe)
{
    disp_u8_t flag_valid_3d = 0, disparty_len = 0, meta_len = 0; // tofill:
    disp_print(DDM_TRACE, "hdmi_forum_vendor_infoframe_construct tofill: %#x\n", pdisp_encoder);

    pframe->type = HDMI_INFOFRAME_TYPE_VENDOR;
    pframe->version = HDMI_VENDOR_INFOFRAME_VER;
    // IEEE(3) + ver(1) + valid(1) + 3D(1) + 3DExt(1) + 3DAddition(1) + dispartyVerLen(1) + metaVerLen(1)
    pframe->length = 10 + pframe->disparty_len + pframe->meta_len;
    pframe->forum_version = HDMI_VENDOR_INFOFRAME_FORUM_VER;

    if(0 == flag_valid_3d)
    {
        pframe->length = 0x5; // IEEE(3) + vsif_version(1) + 3d_valid(1)
        return DISP_OK;
    }

    pframe->disparty_len = disparty_len;
    pframe->meta_len = meta_len;
    pframe->s3d_flag = HDMI_3D_STRUCTURE_FLAG_3D_VALID | HDMI_3D_STRUCTURE_FLAG_ADDITION // support 3D
        | HDMI_3D_STRUCTURE_FLAG_DISPARITY | HDMI_3D_STRUCTURE_FLAG_META | HDMI_3D_STRUCTURE_FLAG_3D_DUAL_VIEW;

    /* tofill: 
     * pframe->disparty_len
     * pframe->meta_len
     * disparty_data & meta_data: transfer to func-pack directly
     */

    return DISP_OK;
}

// infoFrame avi
DISP_STATUS hdmi_avi_infoframe_pack(phdmi_avi_infoframe_t pframe, pdisp_void_t pbuffer, disp_u32_t size)
{
    pdisp_u8_t pcur = (pdisp_u8_t)pbuffer;
    disp_print(DDM_TRACE, "hdmi_avi_infoframe_pack entry, tofill: %d\n", size);

    if( (*pcur != HDMI_INFOFRAME_TYPE_AVI) // header check
        || !((*(pcur+1) == 0x4 && *(pcur+2) == 0xe) || ((*(pcur+1) == 0x2 || *(pcur+1) == 0x3) && *(pcur+2) == 0xd)))
    {
        disp_print(DDM_WARNING, "hdmi_avi_infoframe_pack, header mismatch, [%#x, %#x, %#x]\n", *pcur, *(pcur+1), *(pcur+2));
    }

    pcur += HDMI_INFOFRAME_HEADER_SIZE;
    *pcur |= (pframe->colorspace << 5) | (pframe->active_aspect_valid << 4) | (pframe->bar_type << 2) | (pframe->scan_mode);

    *(++pcur) |= (pframe->colorimetry << 6) | (pframe->active_aspect);
    *pcur |= (pframe->picture_aspect < HDMI_PICTURE_ASPECT_64_27) ? (pframe->picture_aspect << 4) : 0x30;

    *(++pcur) |= pframe->itc ? 0x80 : 0;
    if(HDMI_COLORIMETRY_EXTENDED == pframe->colorimetry)
        *pcur |= pframe->extended_colorimetry << 4;
    *pcur |= (pframe->quantization_range << 2) | (pframe->nups);

    *(++pcur) = pframe->video_code;

    *(++pcur) |= (pframe->ycc_quantization_range << 6);
    if(pframe->itc)
        *pcur |= (pframe->content_type << 4);
    *pcur |= (pframe->pixel_repeat);

    *(++pcur) = pframe->top_bar & 0xff;         // lower
    *(++pcur) = (pframe->top_bar >> 8) & 0xff;  // upper
    *(++pcur) = pframe->bottom_bar & 0xff;
    *(++pcur) = (pframe->bottom_bar >> 8) & 0xff;
    *(++pcur) = pframe->left_bar & 0xff;
    *(++pcur) = (pframe->left_bar >> 8) & 0xff;
    *(++pcur) = pframe->right_bar & 0xff;
    *(++pcur) = (pframe->right_bar >> 8) & 0xff;

    if(4 == pframe->version && pframe->extended_colorimetry > HDMI_EXTENDED_COLORIMETRY_ADDITIONAL_EXTENSION)
    {
        *(++pcur) |= (pframe->extended_colorimetry - HDMI_EXTENDED_COLORIMETRY_ADDITIONAL_D65) << 4;
        pframe->length++;
    }

    return DISP_OK;
}

DISP_STATUS hdmi_avi_infoframe_unpack(phdmi_avi_infoframe_t pframe, pdisp_void_t pbuffer, disp_u32_t size)
{
    pdisp_u8_t pcur = (pdisp_u8_t)pbuffer;
    disp_print(DDM_TRACE, "hdmi_avi_infoframe_unpack entry, tofill: %d\n", size);

    pcur += HDMI_INFOFRAME_HEADER_SIZE; // skip
    pframe->colorspace = (*pcur & 0xe0) >> 5;
    pframe->active_aspect_valid = (*pcur & 0x10) ? 1 : 0;
    pframe->bar_type = (*pcur & 0x0c) >> 2;
    pframe->scan_mode = *pcur & 0x3;

    ++pcur;
    pframe->colorimetry = (*pcur & 0xc0) >> 6;
    pframe->picture_aspect = (*pcur & 0x30) >> 4; // check: over HDMI_PICTURE_ASPECT_64_27 match with video_code
    pframe->active_aspect = *pcur & 0x0f;

    ++pcur;
    pframe->itc = (*pcur & 0x80) ? 1 : 0;
    if(HDMI_COLORIMETRY_EXTENDED == pframe->colorimetry)
        pframe->extended_colorimetry = (*pcur & 0x70) >> 4;
    pframe->quantization_range = (*pcur & 0x0c) >> 2;
    pframe->nups = *pcur & 0x03;

    pframe->video_code = *(++pcur) & 0xff;

    ++pcur;
    pframe->ycc_quantization_range = HDMI_YCC_QUANTIZATION_RANGE_LIMITED + ((*pcur & 0xc0) >> 6);    
    if(pframe->itc) 
        pframe->content_type = (*pcur & 0x30) >> 4;
    pframe->pixel_repeat = *pcur & 0x0f;

    pframe->top_bar |= (*(++pcur) & 0xffff);
    pframe->top_bar |= (*(++pcur) & 0xffff) << 8;
    pframe->bottom_bar |= (*(++pcur) & 0xffff);
    pframe->bottom_bar |= (*(++pcur) & 0xffff) << 8;
    pframe->left_bar |= (*(++pcur) & 0xffff);
    pframe->left_bar |= (*(++pcur) & 0xffff) << 8;
    pframe->right_bar |= (*(++pcur) & 0xffff);
    pframe->right_bar |= (*(++pcur) & 0xffff) << 8;

    ++pcur; // ver 4, PB14
    if(HDMI_EXTENDED_COLORIMETRY_ADDITIONAL_EXTENSION == pframe->extended_colorimetry)
        pframe->extended_colorimetry = HDMI_EXTENDED_COLORIMETRY_ADDITIONAL_D65 + ((*pcur & 0xf0) >> 4);

    // Debug
    disp_print(DDM_TRACE, "hdmi_infoframe_unpack, header[%d, %#x, %#x], 1[%d, %d], "
        "2[%d, %d, %d], 3[%d, %d, %d, %d], 4_video_code[%u] 5[%d, %d, %u], bar[%#x, %#x, %#x, %#x]\n", 
        pframe->type, pframe->version, pframe->length, pframe->colorspace, pframe->scan_mode, pframe->colorimetry, 
        pframe->picture_aspect, pframe->active_aspect, pframe->itc, pframe->extended_colorimetry, pframe->quantization_range, 
        pframe->nups, pframe->video_code, pframe->ycc_quantization_range, pframe->content_type, pframe->pixel_repeat,
        pframe->top_bar, pframe->bottom_bar, pframe->left_bar, pframe->right_bar);

    return DISP_OK;
}

DISP_STATUS hdmi_avi_infoframe_construct(pdisp_void_t pdisp_encoder, phdmi_avi_infoframe_t pframe)
{
    pdisp_encoder_t pencoder = (pdisp_encoder_t)pdisp_encoder;
    pdisp_device_t pdevice = (pdisp_device_t)pencoder->disp_device;
    pdisp_crtc_t pcrtc = &pdevice->disp_crtc[pencoder->index];
    pdisp_display_mode_t pmode = &pcrtc->property.hw_mode;
    pdisp_connector_t pconnector = &pdevice->disp_connector[pencoder->index];
    pdisp_display_info_t pinfo = &pconnector->disp_info;
    disp_print(DDM_TRACE, "hdmi_avi_infoframe_construct entry\n");

    // header
    pframe->type = HDMI_INFOFRAME_TYPE_AVI;
    pframe->version = HDMI_AVI_INFOFRAME_VER;
    pframe->length = HDMI_AVI_INFOFRAME_SIZE;

    // body
    pframe->video_code = pmode->vic;
    pframe->pixel_repeat = (pmode->flags | DISP_MODE_FLAG_DBLCLK) ? 1 : 0;

    pframe->colorspace = get_hdmi_color_space_from_crtc(pencoder,
        &pframe->colorimetry, 
        &pframe->extended_colorimetry);

    pframe->picture_aspect = pmode->picture_aspect_ratio;
    pframe->active_aspect_valid = 0; // check:
    pframe->active_aspect = HDMI_ACTIVE_ASPECT_PICTURE;

    pframe->itc = 0; // not support
    pframe->content_type = HDMI_CONTENT_TYPE_GRAPHICS;

    pframe->scan_mode = HDMI_SCAN_MODE_NONE;
    pframe->nups = HDMI_NUPS_UNKNOWN;

    /* CEA-861:
     * "A Source shall not send a non-zero Q value that does not correspond
     *  to the default RGB Quantization Range for the transmitted Picture
     *  unless the Sink indicates support for the Q bit in a Video
     *  Capabilities Data Block."
     *
     * HDMI 2.0 recommends sending non-zero Q when it does match the
     * default RGB quantization range for the mode, even when QS=0.
     *
     * All CEA modes other than VIC 1 use limited quantization range.
     */
    if(pinfo->rgb_quant_range_selectable)
        pframe->quantization_range = (pmode->vic > 1) ? HDMI_QUANTIZATION_RANGE_LIMITED : HDMI_QUANTIZATION_RANGE_FULL;
    else
        pframe->quantization_range = HDMI_QUANTIZATION_RANGE_DEFAULT;
    /* CEA-861-F:
     * "When transmitting any RGB colorimetry, the Source should set the
     *  YQ-field to match the RGB Quantization Range being transmitted
     *  (e.g., when Limited Range RGB, set YQ=0 or when Full Range RGB,
     *  set YQ=1) and the Sink shall ignore the YQ-field."
     *
     * Unfortunate certain sinks (eg. VIZ Model 67/E261VA) get confused
     * by non-zero YQ when receiving RGB. There doesn't seem to be any
     * good way to tell which version of CEA-861 the sink supports, so
     * we limit non-zero YQ to HDMI 2.0 sinks only as HDMI 2.0 is based
     * on on CEA-861-F.
     *
     * FIXME: sil-sii8620 doesn't have a connector around when
     * we need one, so we have to be prepared for a NULL connector.
     */
    if(!(pinfo->hdmi.scdc.supported || pinfo->color_formats & DISP_COLOR_FORMAT_YCRCB420) 
        || HDMI_QUANTIZATION_RANGE_LIMITED == pframe->quantization_range)
        pframe->ycc_quantization_range = HDMI_YCC_QUANTIZATION_RANGE_LIMITED;
    else
        pframe->ycc_quantization_range = HDMI_YCC_QUANTIZATION_RANGE_FULL;

    /* not support bar */
    pframe->bar_type = HDMI_BAR_DATA_PRESENT_NONE; 
    pframe->top_bar = 0;
    pframe->bottom_bar = 0;
    pframe->left_bar = 0;
    pframe->right_bar = 0;

    return DISP_OK;
}

// infoFrame drm
DISP_STATUS hdmi_drm_infoframe_pack(phdmi_drm_infoframe_t pframe, pdisp_void_t pbuffer, disp_u32_t size)
{
    pdisp_u8_t pcur = (pdisp_u8_t)pbuffer;
    disp_s32_t index = 0;
    disp_print(DDM_TRACE, "hdmi_drm_infoframe_pack entry, tofill: %d\n", size);

    pcur += HDMI_INFOFRAME_HEADER_SIZE;
    *pcur |= pframe->eotf;
    *(++pcur) |= pframe->metadata_type;

    for(index = 0; index < 3; ++index) // 4x3 Bytes
    {
        *(++pcur) = pframe->display_primaries[index].x & 0xff;
        *(++pcur) = (pframe->display_primaries[index].x >> 8) & 0xff;
        *(++pcur) = pframe->display_primaries[index].y & 0xff;
        *(++pcur) = (pframe->display_primaries[index].y >> 8) & 0xff;
    }

    *(++pcur) = pframe->white_point.x & 0xff;
    *(++pcur) = (pframe->white_point.x >> 8) & 0xff;
    *(++pcur) = pframe->white_point.y & 0xff;
    *(++pcur) = (pframe->white_point.y >> 8) & 0xff;

    *(++pcur) = pframe->max_display_mastering_luminance & 0xff;
    *(++pcur) = (pframe->max_display_mastering_luminance >> 8) & 0xff;
    *(++pcur) = pframe->min_display_mastering_luminance & 0xff;
    *(++pcur) = (pframe->min_display_mastering_luminance >> 8) & 0xff;

    *(++pcur) = pframe->max_content_ligh_level & 0xff;
    *(++pcur) = (pframe->max_content_ligh_level >> 8) & 0xff;

    *(++pcur) = pframe->max_frame_average_light_level & 0xff;
    *(++pcur) = (pframe->max_frame_average_light_level >> 8) & 0xff;

    return DISP_OK; 
}

DISP_STATUS hdmi_drm_infoframe_unpack(phdmi_drm_infoframe_t pframe, pdisp_void_t pbuffer, disp_u32_t size)
{
    pdisp_u8_t pcur = (pdisp_u8_t)pbuffer;
    disp_s32_t index = 0;
    disp_print(DDM_TRACE, "hdmi_drm_infoframe_unpack entry, tofill: %d\n", size);

    pcur += HDMI_INFOFRAME_HEADER_SIZE; // skip
    pframe->eotf = *pcur & 0x7;
    pframe->metadata_type = *(++pcur) & 0x7;

    for(index = 0; index < 3; ++index)
    {
        pframe->display_primaries[index].x |= *(++pcur) & 0xffff;
        pframe->display_primaries[index].x |= (*(++pcur) & 0xffff) << 8;
        pframe->display_primaries[index].y |= *(++pcur) & 0xffff;
        pframe->display_primaries[index].y |= (*(++pcur) & 0xffff) << 8;
    }

    pframe->white_point.x |= *(++pcur) & 0xffff;
    pframe->white_point.x |= (*(++pcur) & 0xffff) << 8;
    pframe->white_point.y |= *(++pcur) & 0xffff;
    pframe->white_point.y |= (*(++pcur) & 0xffff) << 8;

    pframe->max_display_mastering_luminance |= *(++pcur) & 0xffff;
    pframe->max_display_mastering_luminance |= (*(++pcur) & 0xffff) << 8;
    pframe->min_display_mastering_luminance |= *(++pcur) & 0xffff;
    pframe->min_display_mastering_luminance |= (*(++pcur) & 0xffff) << 8;

    pframe->max_content_ligh_level |= *(++pcur) & 0xffff;
    pframe->max_content_ligh_level |= (*(++pcur) & 0xffff) << 8;
    pframe->max_frame_average_light_level |= *(++pcur) & 0xffff;
    pframe->max_frame_average_light_level |= (*(++pcur) & 0xffff) << 8;

    return DISP_OK;
}

DISP_STATUS hdmi_drm_infoframe_construct(pdisp_void_t pdisp_encoder, phdmi_drm_infoframe_t pframe)
{
    pdisp_encoder_t pencoder = (pdisp_encoder_t)pdisp_encoder;
    pdisp_hdr_static_metadata_t phdr_info = &pencoder->policy.hdr_metadata.data.static_data;
    disp_s32_t index = 0;
    disp_print(DDM_TRACE, "hdmi_drm_infoframe_construct entry\n");

    // header
    pframe->type = HDMI_INFOFRAME_TYPE_DRM;
    pframe->version = HDMI_DRM_INFOFRAME_VER;
    pframe->length = HDMI_DRM_INFOFRAME_SIZE;

    // body
    pframe->eotf = phdr_info->eotf;
    pframe->metadata_type = HDMI_STATIC_METADATA_TYPE1;

    // tofill: hdr_meta_data from OS
    // new a struct for transfer, or add this members in hdr_sink_metadata
    for(index = 0; index < 3; ++index)
    {
        pframe->display_primaries[index].x = phdr_info->primary_point[index].x;
        pframe->display_primaries[index].y = phdr_info->primary_point[index].y;
    }
    pframe->white_point.x = phdr_info->white_point.x;
    pframe->white_point.y = phdr_info->white_point.y;
    pframe->max_display_mastering_luminance = phdr_info->max_luminance;
    pframe->min_display_mastering_luminance = phdr_info->min_luminance;

    pframe->max_content_ligh_level = phdr_info->max_content_light_level;
    pframe->max_frame_average_light_level = phdr_info->max_frame_average_luminance;

    return DISP_OK;
}

// infoFrame spd
DISP_STATUS hdmi_spd_infoframe_pack(phdmi_spd_infoframe_t pframe, pdisp_void_t pbuffer, disp_u32_t size)
{
    pdisp_u8_t pcur = (pdisp_u8_t)pbuffer;
    disp_s32_t index = 0;
    disp_print(DDM_TRACE, "hdmi_spd_infoframe_pack entry, tofill: %d\n", size);

    pcur += HDMI_INFOFRAME_HEADER_SIZE;
    *pcur = pframe->vendor[0];
    for(index = 1; index < 8; ++index)
        *(++pcur) = pframe->vendor[index];

    for(index = 0; index < 16; ++index)
        *(++pcur) = pframe->product[index];

    *(++pcur) = pframe->sdi;

    return DISP_OK;
}
DISP_STATUS hdmi_spd_infoframe_unpack(phdmi_spd_infoframe_t pframe, pdisp_void_t pbuffer, disp_u32_t size)
{
    pdisp_u8_t pcur = (pdisp_u8_t)pbuffer;
    disp_s32_t index = 0;
    disp_print(DDM_TRACE, "hdmi_spd_infoframe_unpack entry, tofill: %d\n", size);

    pcur += HDMI_INFOFRAME_HEADER_SIZE;
    pframe->vendor[0] = *pcur & 0xff;
    for(index = 1; index < 8; ++index)
        pframe->vendor[index] = *(++pcur) & 0xff;

    for(index = 0; index < 16; ++index)
        pframe->product[index] = *(++pcur) & 0xff;

    pframe->sdi = *(++pcur) & 0xff;

    return DISP_OK;
}
DISP_STATUS hdmi_spd_infoframe_construct(pdisp_void_t pdisp_encoder, phdmi_spd_infoframe_t pframe)
{
    disp_print(DDM_TRACE, "hdmi_spd_infoframe_construct tofill: %#x\n", pdisp_encoder);

    // header
    pframe->type = HDMI_INFOFRAME_TYPE_SPD;
    pframe->version = HDMI_SPD_INFOFRAME_VER;
    pframe->length = HDMI_SPD_INFOFRAME_SIZE;

    // body tofill:
    disp_memset(pframe->vendor, 0, 8);
    disp_memset(pframe->product, 0, 16);
    pframe->sdi = HDMI_SPD_SDI_UNKNOWN;

    return DISP_OK;
}

// infoFrame audio
DISP_STATUS hdmi_audio_infoframe_pack(phdmi_audio_infoframe_t pframe, pdisp_void_t pbuffer, disp_u32_t size)
{
    pdisp_u8_t pcur = (pdisp_u8_t)pbuffer;
    //disp_s32_t index = 0;
    disp_print(DDM_TRACE, "hdmi_audio_infoframe_pack entry, tofill %u\n", size);

    pcur += HDMI_INFOFRAME_HEADER_SIZE;
    *pcur = (disp_u8_t)((pframe->coding_type << 4) | (pframe->channels & 0x07));
    *(++pcur) = (disp_u8_t)((pframe->sample_frequency << 2) | pframe->sample_size);
    *(++pcur) = pframe->coding_type_ext & 0x1f; // pframe->coding_type == HDMI_AUDIO_CODING_TYPE_CXT
    *(++pcur) = pframe->channel_allocation;
    *(++pcur) = (disp_u8_t)((pframe->downmix_inhibit << 7) | (pframe->level_shift_value << 3) | (pframe->lfe_playback_level));

    return DISP_OK;
}

DISP_STATUS hdmi_audio_infoframe_unpack(phdmi_audio_infoframe_t pframe, pdisp_void_t pbuffer, disp_u32_t size)
{
    pdisp_u8_t pcur = (pdisp_u8_t)pbuffer;
    disp_print(DDM_TRACE, "hdmi_audio_infoframe_unpack tofill: %u\n", size);

    pcur += HDMI_INFOFRAME_HEADER_SIZE;
    pframe->coding_type = (*pcur & 0xf0) >> 4;
    pframe->channels = *pcur & 0x07;

    ++pcur;
    pframe->sample_frequency = (*pcur & 0x1c) >> 2;
    pframe->sample_size = *pcur & 0x03;

    ++pcur;
    pframe->channel_allocation = *(pcur) & 0xff;

    ++pcur;
    pframe->downmix_inhibit = (*pcur & 0x80) >> 7;
    pframe->level_shift_value = (*pcur & 0x78) >> 3;
    pframe->lfe_playback_level = *pcur & 0x03;

    return DISP_OK;
}

DISP_STATUS hdmi_audio_infoframe_construct(pdisp_void_t pdisp_encoder, phdmi_audio_infoframe_t pframe)
{
    disp_print(DDM_TRACE, "hdmi_audio_infoframe_construct, tofill: %#x, %#x\n", pdisp_encoder, pframe);

    return DISP_OK;
}

// infoFrame
DISP_STATUS hdmi_infoframe_pack(phdmi_infoframe_t pframe, pdisp_void_t pbuffer, disp_u32_t size)
{
    pdisp_u8_t pcur = (pdisp_u8_t)pbuffer;
    disp_u8_t sum = 0;
    disp_s32_t index = 0;
    disp_print(DDM_TRACE, "hdmi_infoframe_pack entry\n");

    *pcur = pframe->any.type;
    *(++pcur) =  pframe->any.version;
    *(++pcur) = pframe->any.length;

    switch(pframe->any.type)
    {
        case HDMI_INFOFRAME_TYPE_VENDOR:
            if(HDMI_IEEE_OUI == pframe->vendor.any.ieee_code)
                hdmi_vendor_infoframe_pack(&pframe->vendor.hdmi, pbuffer, size);
            else if(HDMI_FORUM_IEEE_OUI == pframe->vendor.any.ieee_code)
                hdmi_forum_vendor_infoframe_pack(&pframe->vendor.hdmi_forum, pbuffer, size);
            else
            {
                disp_print(DDM_WARNING, "hdmi_infoframe_pack, vendor not support ieee code[%#x]\n", pframe->vendor.any.ieee_code);
                return DISP_EINVAL;
            }
            break;
        case HDMI_INFOFRAME_TYPE_AVI:
            hdmi_avi_infoframe_pack(&pframe->avi, pbuffer, size);
            break;
        case HDMI_INFOFRAME_TYPE_SPD:
            hdmi_spd_infoframe_pack(&pframe->spd, pbuffer, size);
            break;
        //case HDMI_INFOFRAME_TYPE_AUDIO:
        //case HDMI_INFOFRAME_TYPE_MPEG:
        //case HDMI_INFOFRAME_TYPE_VBI:
        case HDMI_INFOFRAME_TYPE_DRM:
            hdmi_drm_infoframe_pack(&pframe->drm, pbuffer, size);
            break;
        default:
            disp_print(DDM_WARNING, "hdmi_infoframe_pack, not support type[%d]\n", pframe->any.type);
            return DISP_EINVAL;
    }

    // fill checksum
    for(pcur = (pdisp_u8_t)pbuffer; pcur - (pdisp_u8_t)pbuffer < pframe->any.length + 4; ++pcur)
        sum += *pcur;
    *((pdisp_u8_t)pbuffer + 3) -= sum;

    // Debug
    switch(pframe->any.type)
    {
        case HDMI_INFOFRAME_TYPE_VENDOR:
            if(HDMI_IEEE_OUI == pframe->vendor.any.ieee_code)
                disp_print(DDM_TRACE, "pack info vendor hdmi: header[%d, %#x, %#x], ieee[%#x], vf[%d], hdmi_vic[%u], 3d[%d, %#x]\n",
                    pframe->vendor.hdmi.type, pframe->vendor.hdmi.version, pframe->vendor.hdmi.length, pframe->vendor.hdmi.ieee_code,
                    pframe->vendor.hdmi.video_format, pframe->vendor.hdmi.hdmi_vic, pframe->vendor.hdmi.s3d_struct, pframe->vendor.hdmi.s3d_ext_data);
            else if(HDMI_FORUM_IEEE_OUI == pframe->vendor.any.ieee_code)
                disp_print(DDM_TRACE, "pack info vendor hdmi_forum: header[%d, %#x, %#x], ieee[%#x], flags[%#x], 3dLen[%u, %u], view[%d, %d]\n",
                    pframe->vendor.hdmi_forum.type, pframe->vendor.hdmi_forum.version, pframe->vendor.hdmi_forum.length, pframe->vendor.hdmi.ieee_code,
                    pframe->vendor.hdmi_forum.s3d_flag, pframe->vendor.hdmi_forum.disparty_len, pframe->vendor.hdmi_forum.meta_len, 
                    pframe->vendor.hdmi_forum.s3d_struct, pframe->vendor.hdmi_forum.s3d_ext_data, pframe->vendor.hdmi_forum.view_dependency, 
                    pframe->vendor.hdmi_forum.view_preferred);
            break;
        case HDMI_INFOFRAME_TYPE_AVI:
            disp_print(DDM_TRACE, "pack info avi: header[%d, %#x, %#x], 1[%d, %d], "
                "2[%d, %d, %d], 3[%d, %d, %d, %d], 4_video_code[%u] 5[%d, %d, %u], bar[%#x, %#x, %#x, %#x]\n", 
                pframe->avi.type, pframe->avi.version, pframe->avi.length, pframe->avi.colorspace, pframe->avi.scan_mode, pframe->avi.colorimetry, 
                pframe->avi.picture_aspect, pframe->avi.active_aspect, pframe->avi.itc, pframe->avi.extended_colorimetry, pframe->avi.quantization_range, 
                pframe->avi.nups, pframe->avi.video_code, pframe->avi.ycc_quantization_range, pframe->avi.content_type, pframe->avi.pixel_repeat,
                pframe->avi.top_bar, pframe->avi.bottom_bar, pframe->avi.left_bar, pframe->avi.right_bar);
            break;
        case HDMI_INFOFRAME_TYPE_SPD:
            disp_print(DDM_TRACE, "pack info spd: header[%d, %#x, %#x], sdi[%d], vendor[",
                pframe->spd.type, pframe->spd.version, pframe->spd.length, pframe->spd.sdi);
            for(index = 0; index < 8; ++index)
                disp_print(DDM_TRACE, "%s, ", pframe->spd.vendor[index]);
            disp_print(DDM_TRACE, "], product[");
            for(index = 0; index < 16; ++index)
                disp_print(DDM_TRACE, "%s, ", pframe->spd.product[index]);
            disp_print(DDM_TRACE, "]\n");
            break;
        case HDMI_INFOFRAME_TYPE_DRM:
            disp_print(DDM_TRACE, "pack info drm: header[%d, %#x, %#x], type[%d, %d], "
                "wp[%u, %u], max_min_lum[%u, %u], level[%u, %u], ", pframe->drm.type, pframe->drm.version, pframe->drm.length, 
                pframe->drm.eotf, pframe->drm.metadata_type, pframe->drm.white_point.x, pframe->drm.white_point.y, 
                pframe->drm.max_display_mastering_luminance, pframe->drm.min_display_mastering_luminance, 
                pframe->drm.max_content_ligh_level, pframe->drm.max_frame_average_light_level);
            for(index = 0; index < 3; ++index)
                disp_print(DDM_TRACE, "drm_primaries[%d][%u, %u], ",
                    index, pframe->drm.display_primaries[index].x, pframe->drm.display_primaries[index].y);
            disp_print(DDM_TRACE, "\n");
            break;
        default:
            break;
    }

    return DISP_OK;
}

DISP_STATUS hdmi_infoframe_unpack(phdmi_infoframe_t pframe, pdisp_void_t pbuffer, disp_u32_t size)
{
    pdisp_u8_t pcur = (pdisp_u8_t)pbuffer;
    disp_u8_t sum = 0;
    pdisp_u32_t pieee = DISP_NULL;
    disp_print(DDM_TRACE, "hdmi_infoframe_unpack entry\n");

    // header
    pframe->any.type = *pcur;
    pcur++;
    pframe->any.version = *pcur;
    pcur++;
    pframe->any.length = *pcur;

    switch(pframe->any.type)
    {
        case HDMI_INFOFRAME_TYPE_VENDOR:
            pieee = (pdisp_u32_t)pbuffer + 1; // skip to IEEE code
            pframe->vendor.any.ieee_code = *pieee && 0xffffff;
            if(HDMI_IEEE_OUI == pframe->vendor.any.ieee_code)
                hdmi_vendor_infoframe_unpack(&pframe->vendor.hdmi, pbuffer, size);
            else if(HDMI_FORUM_IEEE_OUI == pframe->vendor.any.ieee_code)
                hdmi_forum_vendor_infoframe_unpack(&pframe->vendor.hdmi_forum, pbuffer, size);
            else
            {
                disp_print(DDM_WARNING, "hdmi_infoframe_unpack, vendor not support ieee code[%#x]\n", pframe->vendor.any.ieee_code);
                return DISP_EINVAL;
            }
            break;
        case HDMI_INFOFRAME_TYPE_AVI:
            hdmi_avi_infoframe_unpack(&pframe->avi, pbuffer, size);
            break;
        case HDMI_INFOFRAME_TYPE_SPD:
            hdmi_spd_infoframe_unpack(&pframe->spd, pbuffer, size);
            break;
        case HDMI_INFOFRAME_TYPE_DRM:
            hdmi_drm_infoframe_unpack(&pframe->drm, pbuffer, size);
            break;
        default:
            disp_print(DDM_WARNING, "hdmi_infoframe_unpack, not support type[%d]\n", pframe->any.type);
            return DISP_EINVAL;
    }

    // check checksum
    for(pcur = (pdisp_u8_t)pbuffer; pcur - (pdisp_u8_t)pbuffer < pframe->any.length + 4; ++pcur)
        sum += *pcur;
    if(sum != 0)
        disp_print(DDM_WARNING, "hdmi_infoframe_unpack, checksum mismatch, type[%d] sum[%u], checksum[%u]\n",
            pframe->any.type, sum, *((pdisp_u8_t)pbuffer + 3));

    // debug
    disp_print(DDM_TRACE, "hdmi_infoframe_unpack, headerInfo[%d, %#x, %#x]\n", 
        pframe->any.type, pframe->any.version, pframe->any.length);

    return DISP_OK;
}

DISP_STATUS hdmi_infoframe_construct(pdisp_void_t pdisp_encoder, phdmi_infoframe_t pframe)
{
    disp_print(DDM_TRACE, "hdmi_infoframe_construct entry\n");
    
    switch(pframe->any.type)
    {
        case HDMI_INFOFRAME_TYPE_VENDOR:
            if(HDMI_IEEE_OUI == pframe->vendor.any.ieee_code)
                hdmi_vendor_infoframe_construct(pdisp_encoder, &pframe->vendor.hdmi);
            else if(HDMI_FORUM_IEEE_OUI == pframe->vendor.any.ieee_code)
                hdmi_forum_vendor_infoframe_construct(pdisp_encoder, &pframe->vendor.hdmi_forum);
            else
            {
                disp_print(DDM_WARNING, "hdmi_infoframe_construct, not support ieee code[%#x]\n", pframe->vendor.any.ieee_code);
                return DISP_EINVAL;
            }
            break;
        case HDMI_INFOFRAME_TYPE_AVI:
            hdmi_avi_infoframe_construct(pdisp_encoder, &pframe->avi);
            break;
        case HDMI_INFOFRAME_TYPE_SPD:
            hdmi_spd_infoframe_construct(pdisp_encoder, &pframe->spd);
            break;
        case HDMI_INFOFRAME_TYPE_DRM:
            hdmi_drm_infoframe_construct(pdisp_encoder, &pframe->drm);
            break;
        default:
            disp_print(DDM_WARNING, "hdmi_infoframe_construct, not support type[%d]\n", pframe->any.type);
            return DISP_EINVAL;
    }

    return DISP_OK;
}

DISP_STATUS hdmi_infoframe_set(pdisp_void_t pbuffer, disp_u32_t size)
{
    // tofill: set frame to hardware
    disp_print(DDM_TRACE, "hdmi_infoframe_set entry, buf=%#x, size=%u\n", pbuffer, size);

    return DISP_OK;
}

DISP_STATUS hdmi_encoder_set_infoframe(pdisp_encoder_t pdisp_encoder, DISP_POWER_STATE state)
{
    hdmi_infoframe_t frame;
    disp_u8_t framebuf[32] = {0};
    pdisp_device_t pdevice = (pdisp_device_t)pdisp_encoder->disp_device;
    pdisp_connector_t pconnector = &pdevice->disp_connector[pdisp_encoder->index];
    pdisp_display_info_t pinfo = &pconnector->disp_info;

    if(DISP_POWER_ON == state && pinfo->has_hdmi_infoframe)
    {
        // AVI infoFrame
        disp_memset(&frame, 0, sizeof(hdmi_infoframe_t));
        disp_memset(framebuf, 0, sizeof(framebuf));
        frame.any.type = HDMI_INFOFRAME_TYPE_AVI;
        hdmi_infoframe_construct(pdisp_encoder, &frame);
        hdmi_infoframe_pack(&frame, &framebuf, sizeof(framebuf));
        hdmi_infoframe_set(&framebuf, sizeof(framebuf));

        // vendor infoFrame (hdmi 1.4)
        disp_memset(&frame, 0, sizeof(hdmi_infoframe_t));
        disp_memset(framebuf, 0, sizeof(framebuf));
        frame.any.type = HDMI_INFOFRAME_TYPE_VENDOR;
        frame.vendor.any.ieee_code = HDMI_IEEE_OUI;
        hdmi_infoframe_construct(pdisp_encoder, &frame);
        hdmi_infoframe_pack(&frame, &framebuf, sizeof(framebuf));
        hdmi_infoframe_set(&framebuf, sizeof(framebuf));
    }
    else
    {
        // do nothing: 1.power off. 2.not support infoframe.
    }

    return DISP_OK;
}
