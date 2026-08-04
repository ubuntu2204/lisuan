/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#include "disp_encoder.h"
#include "disp_dp_encoder.h"
#include "disp_hdmi_encoder.h"
#include "disp_dvo_encoder.h"
#include "disp_callback.h"
#include "disp_device.h"

#define disp_print(level, log, ...) disp_print_ex(DISP_PRINT_MASK_ENCODER, level, log, ##__VA_ARGS__)

DISP_STATUS disp_encoder_sw_init(pdisp_encoder_t pdisp_encoder)
{
    disp_device_t *pdd = pdisp_encoder->disp_device;
    struct disp_encoder_state *state;
    struct disp_encoder_caps *caps;
    struct disp_encoder_policy *ppolicy;
    pdisp_codec_converter_t pconverter = DISP_NULL;

    if(!pdisp_encoder)
    {
        return -DISP_EINVAL;
    }

    state = &pdisp_encoder->state;
    state->power_state = DISP_POWER_OFF;
    state->crtc_index = pdisp_encoder->index;
    state->connector_index = pdisp_encoder->index;

    caps = &pdisp_encoder->caps;
    caps->max_encoder_clock = 600;//MHz, 1c4p cat get 2400MHz
    caps->max_output_bpc = 10; // support 6/8/10 bpc

    ppolicy = &pdisp_encoder->policy;
    ppolicy->color_format = DISP_COLOR_FORMAT_TYPE_RGB;
    ppolicy->colorimetry = DISP_COLORIMETRY_SRGB;
    ppolicy->bpc = 8;
    ppolicy->flag.bits.free_sync_prefer = pdd->flag.info.prefer_free_sync;

    if (DISP_ENCODER_DVO == pdisp_encoder->type)
    {
        disp_dvo_encoder_sw_init(pdisp_encoder);
    }
    else if (DISP_ENCODER_DP == pdisp_encoder->type)
    {
        disp_dp_encoder_sw_init(pdisp_encoder);
    }
    else
    {
        disp_hdmi_encoder_sw_init(pdisp_encoder);
    }

    // All encoder support codec converter
    pdisp_encoder->pcodec_converter = disp_malloc(sizeof(disp_codec_converter_t));
    if (!pdisp_encoder->pcodec_converter)
    {
        disp_print(DDM_ERROR, "disp_encoder_sw_init, alloc codec converter failed, encoder[%u]\n", pdisp_encoder->index);
        return -DISP_ENOMEM;
    }
    pconverter = (pdisp_codec_converter_t)pdisp_encoder->pcodec_converter;
    disp_memset(pconverter, 0, sizeof(disp_codec_converter_t));
    pconverter->disp_device = pdisp_encoder->disp_device;
    pconverter->encoder_index = pdisp_encoder->index;

    return DISP_OK;
}

disp_void_t disp_encoder_sw_deinit(pdisp_encoder_t pdisp_encoder)
{
    if (pdisp_encoder->pcodec_converter)
    {
        disp_free(pdisp_encoder->pcodec_converter);
        pdisp_encoder->pcodec_converter = DISP_NULL;
    }
}

DISP_STATUS disp_encoder_hw_init(pdisp_encoder_t pdisp_encoder)
{
    struct disp_encoder_func* pfunc = DISP_NULL;
    DISP_STATUS ret = 0;

    if(!pdisp_encoder)
        return -DISP_EINVAL;

    pfunc = pdisp_encoder->func;
    
    if(pfunc->hw_init)
    {
        ret = pfunc->hw_init(pdisp_encoder);
    }

    return ret;
}

DISP_STATUS disp_encoder_hw_deinit(pdisp_encoder_t pdisp_encoder)
{
    struct disp_encoder_func* pfunc = DISP_NULL;
    DISP_STATUS ret = 0;

    if(!pdisp_encoder)
        return -DISP_EINVAL;

    pfunc = pdisp_encoder->func;
    
    if(pfunc->hw_deinit)
    {
        ret = pfunc->hw_deinit(pdisp_encoder);
    }

    return ret;
}

DISP_STATUS disp_encoder_dpms(pdisp_encoder_t pdisp_encoder, disp_u8_t benable)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_encoder->disp_device;

    if (pdd->flag.info.pz1_capture_pipe)
    {
        return DISP_OK;
    }
    else if (pdisp_encoder->func->dpms)
    {
        if (pdisp_encoder->func->dpms_prepare && benable)
            pdisp_encoder->func->dpms_prepare(pdisp_encoder);
        return pdisp_encoder->func->dpms(pdisp_encoder, benable);
    }
    else
    {
        disp_print(DDM_WARNING, "no dpms func on this encoder\n");
        return -DISP_EPRIV_INTERNAL;      
    }
}

DISP_STATUS disp_encoder_set_mode(pdisp_encoder_t pdisp_encoder)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_encoder->disp_device;

    if (pdd->flag.info.pz1_capture_pipe)
    {
        return DISP_OK;
    }
    else if (pdisp_encoder->func->mode_set)
    {
        return pdisp_encoder->func->mode_set(pdisp_encoder);
    }
    else
    {
        disp_print(DDM_WARNING, "no set mode func on this encoder\n");
        return -DISP_EPRIV_INTERNAL;      
    }
}

/* only config policy that dp encoder set mode use */
DISP_STATUS disp_encoder_policy_config(pdisp_encoder_t pdisp_encoder)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_encoder->disp_device;
    pdisp_crtc_t pdisp_crtc = &pdd->disp_crtc[pdisp_encoder->index];
    pdisp_connector_t pdisp_conn = &pdd->disp_connector[pdisp_encoder->index];
    struct disp_encoder_policy* policy = &pdisp_encoder->policy;
    pdisp_display_mode_t phw_mode = &pdisp_crtc->property.hw_mode;
    disp_u32_t vfresh = (disp_u32_t)phw_mode->clock * 1000 / ((disp_u32_t)phw_mode->htotal * phw_mode->vtotal);
    disp_u32_t free_sync_insert_line = 0;
    DISP_STATUS state = DISP_OK;

    policy->colorimetry = pdisp_crtc->property.colorimetry;
    policy->bpc = pdisp_crtc->property.bpc;
    switch(pdisp_crtc->property.output_signal)
    {
        case DISP_COLOR_FORMAT_YCRCB444:
            policy->color_format = DISP_COLOR_FORMAT_TYPE_YUV444;
            break;
        case DISP_COLOR_FORMAT_YCRCB422:
            policy->color_format = DISP_COLOR_FORMAT_TYPE_YUV422;
            break;
        case DISP_COLOR_FORMAT_YCRCB420:
            policy->color_format = DISP_COLOR_FORMAT_TYPE_YUV420;
            break;
        case DISP_COLOR_FORMAT_RGB444:
        default:
            policy->color_format = DISP_COLOR_FORMAT_TYPE_RGB;
            break;
    }

    // free sync
    if (policy->flag.bits.free_sync_prefer
        && policy->flag.bits.free_sync_sink_support
        && vfresh >= (disp_u32_t)pdisp_conn->disp_info.monitor_range.min_vfreq + 1
        && vfresh <= (disp_u32_t)pdisp_conn->disp_info.monitor_range.max_vfreq)
    {
        // free_sync_insert_line = (vfresh / min_vfreq - 1) * vtotal
        free_sync_insert_line = (disp_u32_t)((disp_u64_t)phw_mode->clock * 1000 / ((disp_u64_t)phw_mode->htotal *
            pdisp_conn->disp_info.monitor_range.min_vfreq) - phw_mode->vtotal);

        disp_print(DDM_KEY, "free sync en: min-max[%u %u] mode_vf[%u] insert[%u] vtotal[%u]\n",
            pdisp_conn->disp_info.monitor_range.min_vfreq,
            pdisp_conn->disp_info.monitor_range.max_vfreq,
            vfresh, free_sync_insert_line, phw_mode->vtotal);

        policy->flag.bits.free_sync_enable  = 1;
        policy->flag.bits.free_sync_sdp_version  = 0; // HB2 is 01h
        policy->flag.bits.free_sync_sdp_position = 0; // sdp at vsync rising edge
        policy->free_sync_insert_line = free_sync_insert_line;
    }
    else
    {
        disp_print(DDM_KEY, "free sync dis: mode_vf[%u] prefer[%u]sink[%u] min-max[%u %u]\n",
            (disp_u32_t)vfresh, policy->flag.bits.free_sync_prefer,
            policy->flag.bits.free_sync_sink_support,
            pdisp_conn->disp_info.monitor_range.min_vfreq,
            pdisp_conn->disp_info.monitor_range.max_vfreq);
        policy->flag.bits.free_sync_enable  = 0;
    }

    if(pdisp_encoder->func && pdisp_encoder->func->policy_config)
        state = pdisp_encoder->func->policy_config(pdisp_encoder, pdisp_crtc->property.colorimetry);

    return state;
}
