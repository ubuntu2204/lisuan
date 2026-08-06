/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

/* fec & dsc SW logic for DDM */

#ifndef __DISP_DSC_H__
#define __DISP_DSC_H__

#include "display.h"

enum DISP_DSC_FMT_TYPE
{
    DISP_DSC_FMT_RGB,
    DISP_DSC_FMT_YUV_444,
    DISP_DSC_FMT_YUV_SIMPLE422,
    DISP_DSC_FMT_YUV_NATIVE422,
    DISP_DSC_FMT_YUV_NATIVE420,
};

typedef struct _disp_dsc_policy_t
{
    disp_u8_t dsc_ver_minor;
    disp_u8_t slice_num_hor;
    disp_u8_t slice_num_ver;
    disp_u8_t linebuf_depth;
    disp_u8_t bpp;
    disp_u8_t bpp_fraction;     // unit 1/16
    disp_u8_t prefer_bpp;       // the larger the better
    disp_u8_t increment_of_bpp;
    disp_u8_t h_delay_denom;
    disp_u8_t dsc_module_index; // 0-dsc_a, 1-dsc_b
    disp_u8_t bpc;
    disp_u16_t pic_height;
    disp_u16_t pic_width;
    disp_u32_t clk_khz;
    DISP_COLOR_FORMAT_TYPE color_fmt;
    enum DISP_DSC_FMT_TYPE dsc_fmt;

    union
    {
        struct {
            disp_u32_t fec_en                       : 1;
            disp_u32_t dsc_en                       : 1;
            disp_u32_t dsc_crc                      : 1;
            disp_u32_t fec_org_dp_1_4               : 1; // 0-dp1.4a, 1-dp1.4
            disp_u32_t block_prediction             : 1;
            disp_u32_t use_min_slices_h             : 1;
            disp_u32_t prefer_fec                   : 1;
            disp_u32_t prefer_dsc                   : 1;
            disp_u32_t dynamic_link                 : 1; // [0]encoder 0,1 support dsc, 2,3 not support. [1] dynamic link encoder
            disp_u32_t use_fraction_bpp             : 1;
            disp_u32_t reserved                     :22;
        }bits;
        disp_u32_t value;
    }flag;
}disp_dsc_policy_t;

#define DSC_PPS_RC_OFFSET 36 // include flatness
#define DSC_PPS_RC_SIZE 52
#define DSC_AVAILABLE_SLICES_SIZE 10
typedef struct _dsc_pps_t // 128 Bytes
{
    struct {
        disp_u8_t minor                 : 4;// 4 low
        disp_u8_t major                 : 4;// 4 high
    } version;
    disp_u8_t pps_identifier;
    disp_u8_t reserved1;
    struct {
        disp_u8_t linebuf_depth         : 4;
        disp_u8_t bits_per_component    : 4;
    } bpc;
    struct {
        disp_u8_t bits_per_pixel_high   : 2;// valid if bpp larger than 16
        disp_u8_t vbr_enable            : 1;
        disp_u8_t simple_422            : 1;
        disp_u8_t convert_rgb           : 1;
        disp_u8_t block_pred_enable     : 1;
        disp_u8_t reserved              : 2;
    } flag1;
    disp_u8_t bits_per_pixel;               // all 10bits for bpp, unit 1/16, [0, 63.9375]
    disp_u16_t pic_height;
    disp_u16_t pic_width;
    disp_u16_t slice_height;
    disp_u16_t slice_width;
    disp_u16_t chunk_size;
    disp_u16_t initial_xmit_delay;          // 10 + RESERVED 6
    disp_u16_t initial_dec_delay;
    disp_u16_t initial_scale_value;         // 6 + RESERVED 10
    disp_u16_t scale_increment_interval;
    disp_u16_t scale_decrement_interval;    // 12 + RESERVED 4
    disp_u16_t first_line_bpg_offset;       // 5 + RESERVED 11
    disp_u16_t nfl_bpg_offset;
    disp_u16_t slice_bpg_offset;
    disp_u16_t initial_offset;
    disp_u16_t final_offset;
    disp_u8_t flatness_min_qp;              // 5 + RESERVED 3
    disp_u8_t flatness_max_qp;              // 5 + RESERVED 3
    struct {
        disp_u16_t model_size;
        disp_u8_t edge_factor;              // 4 + RESERVED 4
        disp_u8_t quant_incr_limit0;        // 5 + RESERVED 3
        disp_u8_t quant_incr_limit1;        // 5 + RESERVED 3
        struct {
            disp_u8_t low               : 4;
            disp_u8_t high              : 4;
        } tgt_offset;
        disp_u8_t buf_thresh[14];           // 14x8
        struct {
            disp_u16_t bpg_offset : 6;
            disp_u16_t max_qp     : 5;
            disp_u16_t min_qp     : 5;
        } range[15];          // 15x16
    }rc;                      // 400
    struct {
        disp_u8_t native_420            : 1;
        disp_u8_t native_422            : 1;
        disp_u8_t reserved              : 6;
    } flag2;                                // 2 + RESERVED 6
    disp_u8_t second_line_bpg_offset;       // 5 + RESERVED 3
    disp_u16_t nsl_bpg_offset;
    disp_u16_t second_line_offset_adj;
    disp_u8_t reserved2[34];                // 272
}dsc_pps_t, *pdsc_pps_t;

typedef struct _disp_dsc_state_t
{
    union
    {
        struct {
            disp_u32_t already_bound                : 1;
            disp_u32_t dsc_on                       : 1;
            disp_u32_t reserved                     :30;
        }bits;
        disp_u32_t value;
    }flag;

    disp_s32_t encoder_index; // bound encoder index, if already bound
    dsc_pps_t pps; // big endian
}disp_dsc_state_t;

typedef struct _disp_dsc_t disp_dsc_t, *pdisp_dsc_t;

typedef struct _disp_dsc_func
{
    disp_void_t (*policy_update)(pdisp_dsc_t pdisp_dsc, disp_u32_t max_bandwidth);
    disp_void_t (*module_set)(pdisp_dsc_t pdisp_dsc, disp_u8_t enable);
    disp_u8_t (*need_enable_dsc)(pdisp_dsc_t pdisp_dsc);
    disp_u8_t (*need_dsc_off)(pdisp_dsc_t pdisp_dsc);
    disp_u8_t (*check_timing)(pdisp_dsc_t pdisp_dsc, pdisp_display_mode_t pmode, disp_u8_t bpc);
}disp_dsc_func;

struct _disp_dsc_t
{
    pdisp_void_t disp_device;
    disp_s32_t index;

    pdisp_void_t phw_caps;
    disp_dsc_policy_t policy;
    disp_dsc_state_t state;
    disp_dsc_func func;
};

static inline disp_u8_t dsc_already_bind(pdisp_dsc_t pdsc)
    { return pdsc->state.flag.bits.already_bound ? DISP_TRUE : DISP_FALSE; }
static inline disp_u8_t dsc_need_enable_dsc(pdisp_dsc_t pdsc)
    { return pdsc->policy.flag.bits.dsc_en ? DISP_TRUE : DISP_FALSE; }
static inline disp_u8_t dsc_need_off(pdisp_dsc_t pdsc)
    { return pdsc->state.flag.bits.dsc_on ? DISP_TRUE : DISP_FALSE; }

DISP_STATUS disp_dsc_sw_init(pdisp_dsc_t pdsc, pdisp_void_t pdisp_device, disp_u32_t dsc_index, disp_u32_t encoder_index);
disp_void_t disp_dsc_sw_deinit(pdisp_dsc_t pdsc);

#endif
