/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#ifndef __DISP_PIPE_H__
#define __DISP_PIPE_H__

#include "display.h"
#include "disp_plane.h"

typedef struct _disp_ov_t
{
    disp_u32_t key_mode;
    disp_u32_t kp;
    disp_u32_t ks;
    disp_u32_t bld_mode;//premulti or not
    disp_u32_t inv_alpha;
    disp_u32_t alpha_sel;
    disp_u32_t key_sel;
    disp_u32_t is_alpha_rang;
    disp_u32_t plane_alpha;//0xff, reverse alpha = 0, non 0xff reverse = 1, when plane alpha exist, this field = plane_alpha
}disp_ov_t, *pdisp_ov_t;

typedef struct _disp_tg_t
{
    disp_u16_t ht;//total
    disp_u16_t hde;//display end
    disp_u16_t hbs;//blank start
    disp_u16_t hbe;//blank end
    disp_u16_t hss;//sync start
    disp_u16_t hse;//sync end
    disp_u16_t vt;
    disp_u16_t vde;
    disp_u16_t vbs;
    disp_u16_t vbe;
    disp_u16_t vss;
    disp_u16_t vse;
}disp_tg_t, *pdisp_tg_t;

typedef struct _disp_iwindow_t
{
    disp_u32_t iwin_en;
    disp_scale_t scale;
}disp_iwindow_t, *pdisp_iwindow_t;

typedef struct _disp_pus_t
{
    disp_u32_t support_pu;
    disp_u32_t max_pu_src_hor;
    disp_u32_t max_pu_src_ver;
    disp_u32_t max_pu_dst_hor;
    disp_u32_t max_pu_dst_ver;

    disp_u32_t pus_en;
    disp_u32_t pus_hen;
    disp_u32_t pus_ven;
    disp_u32_t pus_cos2tap;
    disp_u32_t pus_width_dst;
    disp_u32_t pus_height_dst;
    disp_u32_t pus_ratio_mode;
    disp_u32_t pus_ratio_plus;
    disp_u32_t pus_vsync_dlyval;
    disp_u64_t pus_vacc;
    disp_u64_t pus_hacc;
}disp_pus_t, *pdisp_pus_t;

typedef struct _disp_lut_t
{
    disp_u32_t lut_lut_en;
    disp_u32_t lut_intp_on;

    /**
     * 00: 18bit
     * 01: 24bit
     * 1x: 30bit
    */
    disp_u32_t lut_bitwidth;
    disp_u32_t lut_data[256];
}disp_lut_t, *pdisp_lut_t;

typedef struct _disp_dither_t
{
    disp_u32_t dither_en;
    disp_u32_t dither_base;
    disp_u32_t dither_bitsel;
    disp_u32_t dither_dist_en;
}disp_dither_t, *pdisp_dither_t;

typedef struct _disp_pipe_t
{
    pdisp_void_t pdisp_device;
    disp_u8_t crtc_index;//also means pipe_index
    disp_ov_t ov[2];
    disp_tg_t tg;
    disp_iwindow_t iwin;//internal window
    disp_pus_t pus;//panel up-scaler
    disp_lut_t lut;
    disp_csc_t csc;
    disp_dither_t dither;
    disp_sig_set_t sig_set;
    disp_u8_t hsync_negative;
    disp_u8_t vsync_negative;
}disp_pipe_t, *pdisp_pipe_t;

DISP_STATUS disp_pipe_sw_init(pdisp_pipe_t pdisp_pipe);
DISP_STATUS disp_pipe_sw_deinit(pdisp_pipe_t pdisp_pipe);

DISP_STATUS disp_pipe_hw_init(pdisp_pipe_t pdisp_pipe);
DISP_STATUS disp_pipe_hw_deinit(pdisp_pipe_t pdisp_pipe);

DISP_STATUS disp_pipe_reset(pdisp_pipe_t pdisp_pipe);
DISP_STATUS disp_pipe_clear_reset(pdisp_pipe_t pdisp_pipe);

DISP_STATUS disp_pipe_check_overlay(pdisp_pipe_t pdisp_pipe);
DISP_STATUS disp_pipe_set_overlay(pdisp_pipe_t pdisp_pipe, pdisp_plane_t pplane);

DISP_STATUS disp_plane_ov_trigger(pdisp_plane_t pdisp_plane);

disp_void_t disp_pipe_set_tg(pdisp_pipe_t pdisp_pipe);
disp_void_t disp_pipe_set_pus(pdisp_pipe_t pdisp_pipe, disp_u8_t mode_update);
disp_void_t disp_pipe_set_lut(pdisp_pipe_t pdisp_pipe, pdisp_gamma_info_t gamma_info);
disp_void_t disp_pipe_set_csc(pdisp_pipe_t pdisp_pipe, pdisp_csc_para_t pcsc_para);
disp_void_t disp_pipe_set_dither(pdisp_pipe_t pdisp_pipe);
DISP_STATUS disp_pipe_set_sig(pdisp_pipe_t pdisp_pipe, pdisp_sig_set_t psig_set);
DISP_STATUS disp_pipe_get_sig(pdisp_pipe_t pdisp_pipe, pdisp_sig_get_t psig_get);
DISP_STATUS disp_pipe_set_sig_golden(pdisp_pipe_t pdisp_pipe);
DISP_STATUS disp_pipe_get_sig_golden(pdisp_pipe_t pdisp_pipe, pdisp_sig_get_t psig_get);
DISP_STATUS disp_pipe_get_itg_counter(pdisp_pipe_t pdisp_pipe, pdisp_itg_counter_t pitg_counter);
#endif
