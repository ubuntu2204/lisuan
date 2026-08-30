/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#ifndef __DISP_CRTC_H__
#define __DISP_CRTC_H__

#include "display.h"
#include "disp_plane.h"
#include "disp_pipe.h"
#include "disp_callback.h"

typedef struct _disp_crtc_property_t
{
    disp_u8_t dpms;
    disp_display_mode_t mode;
    disp_display_mode_t hw_mode;
    disp_u8_t output_signal;//used for encoder set mode
    DISP_COLORIMETRY colorimetry;
    disp_u8_t bpc;
    disp_u8_t hdr_mode;
}disp_crtc_property_t, *pdisp_crtc_property_t;

typedef struct _disp_crtc_t
{
    disp_void_t *disp_device;
    disp_s32_t index;

    disp_u32_t max_pixel_clock;

    disp_plane_t primary;
    disp_plane_t cursor;
    //wil refine this to alloc dynamically
    disp_plane_t overlay_plane[DISP_OVERLAY_PLANE_NUM];

    disp_pipe_t pipe;

    disp_crtc_property_t property;
}disp_crtc_t, *pdisp_crtc_t;

static inline disp_u8_t crtc_get_property_dpms(pdisp_crtc_property_t pp)
{
    return pp->dpms;
}

static inline disp_void_t crtc_set_property_dpms(pdisp_crtc_property_t pp, disp_u8_t dpms)
{
    pp->dpms = dpms;
}

static inline disp_void_t crtc_get_property_mode(pdisp_crtc_property_t pp, pdisp_display_mode_t pmode)
{
    disp_memcpy(pmode, &pp->mode, sizeof(disp_display_mode_t));
}

static inline disp_void_t crtc_set_property_mode(pdisp_crtc_property_t pp, pdisp_display_mode_t pmode)
{
    disp_memcpy(&pp->mode, pmode, sizeof(disp_display_mode_t));
}

static inline disp_void_t crtc_get_property_hw_mode(pdisp_crtc_property_t pp, pdisp_display_mode_t pmode)
{
    disp_memcpy(pmode, &pp->hw_mode, sizeof(disp_display_mode_t));
}

static inline disp_void_t crtc_set_property_hw_mode(pdisp_crtc_property_t pp, pdisp_display_mode_t pmode)
{
    disp_memcpy(&pp->hw_mode, pmode, sizeof(disp_display_mode_t));
}

static inline disp_u8_t crtc_get_property_output_signal(pdisp_crtc_property_t pp)
{
    return pp->output_signal;
}

static inline disp_void_t crtc_set_property_output_signal(pdisp_crtc_property_t pp, disp_u8_t output_signal)
{
    pp->output_signal = output_signal;
}

static inline DISP_COLORIMETRY crtc_get_property_colorimetry(pdisp_crtc_property_t pp)
{
    return pp->colorimetry;
}

static inline disp_void_t crtc_set_property_colorimetry(pdisp_crtc_property_t pp, DISP_COLORIMETRY colorimetry)
{
    pp->colorimetry = colorimetry;
}

static inline disp_u8_t crtc_get_property_bpc(pdisp_crtc_property_t pp)
{
    return pp->bpc;
}

static inline disp_void_t crtc_set_property_bpc(pdisp_crtc_property_t pp, disp_u8_t bpc)
{
    pp->bpc = bpc;
}

DISP_STATUS disp_crtc_sw_init(pdisp_crtc_t pdisp_crtc);
DISP_STATUS disp_crtc_sw_deinit(pdisp_crtc_t pdisp_crtc);
DISP_STATUS disp_crtc_hw_init(pdisp_crtc_t pdisp_crtc);
DISP_STATUS disp_crtc_hw_deinit(pdisp_crtc_t pdisp_crtc);
DISP_STATUS disp_crtc_dpms(pdisp_crtc_t pdisp_crtc, pdisp_crtc_dpms_t crtc_dpms);
DISP_STATUS disp_crtc_set_mode(pdisp_crtc_t pdisp_crtc, disp_crtc_set_mode_t crtc_set_mode);

pdisp_plane_t disp_crtc_get_plane(pdisp_crtc_t pdisp_crtc, disp_u32_t plane_index);
DISP_STATUS disp_crtc_plane_check(pdisp_crtc_t pdisp_crtc, pdisp_plane_update_para_t pupdate_para);
DISP_STATUS disp_crtc_plane_update(pdisp_crtc_t pdisp_crtc, pdisp_plane_update_para_t pupdate_para);

DISP_STATUS disp_crtc_wait_blank(pdisp_void_t pdisp_device, pdisp_wait_blank_t pParam);
#endif