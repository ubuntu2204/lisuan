/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#ifndef __DISP_PLANE_H__
#define __DISP_PLANE_H__

#include "display.h"

typedef struct _disp_csc_t
{
    disp_u32_t input_fmt;
    disp_u32_t output_fmt;

    disp_u32_t brightness;
    disp_u32_t prog;

    disp_u32_t bypss;

    disp_u32_t yuv422;
    disp_u32_t yuv420;
    disp_u32_t convert_mode;
    disp_u32_t range;

    disp_u32_t coef1;
    disp_u32_t coef2;
    disp_u32_t coef3;
    disp_u32_t coef4;
    disp_u32_t coef5;
    disp_u32_t coef6;
    disp_u32_t coef7;
    disp_u32_t coef8;
    disp_u32_t coef9;
}disp_csc_t, *pdisp_csc_t;

#define DISP_OVERLAY_PLANE_NUM 1

typedef struct _disp_plane_t
{
    disp_void_t *disp_device; //the abstracted display hardware module
    disp_u32_t plane_index; //0 for primary, 1 for overlay, 2 for cursor
    disp_u32_t crtc_index; //index of currently bounded crtc, begin at 0
    DISP_FLIP_MODE flip_mode;
    disp_u32_t flip_flags; // DISP_FLIP_FLAGS_MODE_CHANGE, tofix: flip immediately/nextVsync/shared/independent in plane_flush
    disp_rect_t src;
    disp_rect_t dst;

    disp_fb_t fb;// feeder to this plane
    disp_csc_t csc;
    disp_blend_info_t blend_info;

    //for cursor
    disp_u8_t cursor_type;
    disp_u32_t mono_bg_color;
    disp_u32_t mono_fg_color;

    disp_hdr_module_t hdr_module;

    union
    {
        struct {
            disp_u32_t flip_immediate           : 1;
            disp_u32_t flush_hdr_module         : 1;
            disp_u32_t reserved                 :30;
        } bits;
        disp_u32_t val;
    } flag;
}disp_plane_t, *pdisp_plane_t;

DISP_STATUS disp_plane_sw_init(pdisp_plane_t pdisp_plane);

DISP_STATUS disp_plane_hw_init(pdisp_plane_t pdisp_plane);
DISP_STATUS disp_plane_hw_deinit(pdisp_plane_t pdisp_plane);

DISP_STATUS disp_plane_check(pdisp_plane_t pdisp_plane, pdisp_plane_update_para_t pupdate_para);
DISP_STATUS disp_plane_update(pdisp_plane_t pdisp_plane, pdisp_plane_update_para_t pupdate_para);
DISP_STATUS disp_plane_update_scale(pdisp_plane_t pdisp_plane, pdisp_scale_t pscale);
DISP_STATUS disp_plane_get_addr(pdisp_void_t pdisp_device, pdisp_get_fb_addr_t paddr);

disp_void_t disp_csc_dump(pdisp_csc_t pdisp_csc);
#endif
