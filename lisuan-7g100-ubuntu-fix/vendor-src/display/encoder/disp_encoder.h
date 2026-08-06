/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#ifndef __DISP_ENCODER_H__
#define __DISP_ENCODER_H__

#include "display.h"
#include "disp_dp_phy.h"

typedef enum _DISP_ENCODER_TYPE
{
    DISP_ENCODER_HDMI,
    DISP_ENCODER_DP,
    DISP_ENCODER_DVO,
}DISP_ENCODER_TYPE;

struct dp_encoder_caps
{
    disp_u8_t source_max_out_bpp;
    disp_u8_t source_max_lane_count;
    disp_u8_t source_max_link_rate;
    disp_u8_t source_support_TPS3;
};

struct hdmi_encoder_caps
{
    disp_u32_t support_scrambling;
    disp_u32_t support_read_request;
};

union disp_encoder_private_caps
{
    struct hdmi_encoder_caps hdmi;
    struct dp_encoder_caps dp;
};

struct disp_encoder_caps
{
    disp_u32_t max_output_bpc;
    disp_u32_t max_encoder_clock;
    union disp_encoder_private_caps priv;
};

struct disp_hdmi_cts_data
{
    disp_u32_t tmds_bit_clock_ratio;
    disp_u32_t read_request_enable;
    disp_u32_t scrambling_enable;
};

struct hdmi_encoder_state
{
    //infoframe;
    disp_u32_t max_packet_num_hblank;
    disp_u32_t max_packet_num_vblank;

    /**
    * this is tmds clock, may be not equal with pll clock
    * depend on hw implementation
    */
    disp_u32_t tmds_clock;

    disp_u32_t cts_test;
    struct disp_hdmi_cts_data cts_data;
};

struct disp_dp_cts_data
{
    DISP_DP_LINK_RATE_TYPE link_rate;  //0x6:1.62    0xA : 2.7G    0x14: 5.4G
    disp_u8_t lane_count; //1: one lane   2 : two lanes  4: four lanes
    disp_u8_t tps; // 2-tps2, 3-tps3, 4-tps4
    //disp_u32_t test_pattern; //0: no test pattern  1: color ramps  2: black and white vertical lines 3: color square
    DISP_DP_ASYNC_CLOCK async_clock;
    DISP_RANGE_TYPE range_type;
    DISP_YUV_COEFFICIENTS ycbcr_coefficients;
    DISP_DP_ENHANCED_FRAME_MODE enhanced_frame_mode;

    union
    {
        struct {
            disp_u32_t link_rate_valid :1;
            disp_u32_t lane_count_valid :1;
            disp_u32_t tps_valid :1;
            disp_u32_t async_clock_valid :1;
            disp_u32_t range_type_valid :1;
            disp_u32_t ycbcr_coefficients_valid :1;
            disp_u32_t enhanced_frame_valid :1;
            disp_u32_t reserved :25;
        }bits;
        disp_u32_t value;
    }valid;
};

typedef struct _dp_dfp_state_t
{
    disp_u8_t type; // DP_DS_PORT_TYPE_DP
    disp_u8_t hpd_aware;
    disp_u8_t max_bpc;
    disp_u8_t pixel_cap; // DP_DS_HDMI_FRAME_SEQ_TO_FRAME_PACK
    disp_s32_t max_tdms_clk_khz;
} dp_dfp_state_t;

typedef struct _disp_audio_infoframe_t {
    disp_u8_t valid;
    disp_u8_t coding_type;
    disp_u8_t channel_count;
    disp_u8_t stream_size;
    disp_u8_t stream_frequency;
    disp_u8_t coding_type_ext;
    // CA, LSV, LFEPBL0, LFEPBL1
} disp_audio_infoframe_t, *pdisp_audio_infoframe_t;

struct dp_encoder_state
{
    disp_u8_t link_rate;
    disp_u8_t lane_count;

    disp_u8_t fifo_index_idle; // fifo_index that prepare to use for next sdp
    disp_u8_t fifo_index_vsc;
    disp_u8_t fifo_index_audio;

    DISP_PSR_SET_TYPE psr_type;

    dp_dfp_state_t dfp[4]; // dp spec 4 dfp

    union
    {
        struct {
            disp_u32_t fifo_in_use      : 1;
            disp_u32_t fifo_idle_group  : 1;
            disp_u32_t fifo_have_vsc    : 1;
            disp_u32_t fifo_have_audio  : 1;
            disp_u32_t psr_module_enable: 1;
            disp_u32_t sdp_vsc_psr_once : 1;
            disp_u32_t sdp_vsc_colorimetry_once : 1;
            disp_u32_t dfp_branch_device: 1;
            disp_u32_t reserved         :24;
        } bits;
        disp_u32_t value;
    } flag;
};

union encoder_private_state
{
    struct hdmi_encoder_state hdmi;
    struct dp_encoder_state dp;
};

struct disp_encoder_state
{
    disp_u8_t crtc_index;
    disp_u8_t connector_index;
    DISP_POWER_STATE power_state;
    union encoder_private_state priv;
    union
    {
        struct {
            disp_u32_t fec_on                       : 1;
            disp_u32_t free_sync_on                 : 1;
            disp_u32_t reserved                     :30;
        }bits;
        disp_u32_t value;
    }flag;
};

struct disp_hdmi_policy
{
    disp_u32_t hdmi_policy; // todo:

    disp_u8_t cts_valid; // 0-not valid, 1-valid
};

struct disp_dp_policy
{
    disp_u8_t bpp;
    disp_u8_t max_possible_link_rate;
    disp_u8_t max_possible_lane_count;
    disp_u8_t link_rate; // unit 270Mbps
    disp_u8_t lane_count;
    disp_u8_t tps;
    DISP_DP_ASYNC_CLOCK async_clock;
    DISP_RANGE_TYPE range_type;
    DISP_YUV_COEFFICIENTS ycbcr_coefficients;
    DISP_DP_ENHANCED_FRAME_MODE enhanced_frame_mode;
    disp_u32_t TU_size;  //32 - 64 ,default 48
    disp_u32_t enhance_mode;  // 1 - support HDCP
    disp_u32_t max_possible_bandwidth_kbyte;

    struct disp_dp_cts_data cts_data;

    union
    {
        struct {
            disp_u32_t cts_valid                : 1;
            disp_u32_t ssc_prefer               : 1;
            disp_u32_t ssc_on                   : 1;
            disp_u32_t auto_tu_ratio            : 1;
            disp_u32_t reserved                 :28;
        } bits;
        disp_u32_t value;
    } flag;
};

struct disp_encoder_policy
{
    DISP_COLOR_FORMAT_TYPE color_format;
    DISP_COLORIMETRY colorimetry;
    disp_u32_t bpc; // 6 8 10 12 16
    disp_u32_t free_sync_insert_line; // Max_Linsert_0 + Max_Linsert_1

    union {
        struct disp_hdmi_policy hdmi;
        struct disp_dp_policy dp;
    }custom;

    disp_hdr_metadata_t hdr_metadata;

    union
    {
        struct {
            disp_u32_t fec_prefer                   : 1;
            disp_u32_t fec_org_dp_1_4               : 1; // 0-dp1.4a, 1-dp1.4
            disp_u32_t fec_enable                   : 1;
            disp_u32_t free_sync_sdp_version        : 1; // Adaptive_Sync_Spec_Version
            disp_u32_t free_sync_sdp_position       : 2; // Adaptive_Sync_Pos
            disp_u32_t free_sync_prefer             : 1;
            disp_u32_t free_sync_sink_support       : 1;
            disp_u32_t free_sync_enable             : 1;
            disp_u32_t hdr_metadata                 : 1; // send metadata
            disp_u32_t hdr_on                       : 1;
            disp_u32_t msa_ref_vsc                  : 1;
            disp_u32_t reserved                     :21;
        }bits;
        disp_u32_t value;
    }flag;
};

typedef struct _disp_encoder
{
    disp_void_t *disp_device;
    disp_u8_t index;
    DISP_ENCODER_TYPE type;
    struct disp_encoder_func *func;
    struct disp_encoder_caps caps;
    struct disp_encoder_state state;
    struct disp_encoder_policy policy;
    struct dp_phy_t phy;
    pdisp_void_t pdsc;
    pdisp_void_t pcodec_converter;
}disp_encoder_t, *pdisp_encoder_t;

struct disp_encoder_func
{
    DISP_STATUS (*mode_set)(pdisp_encoder_t pdisp_encoder);
    disp_void_t (*dpms_prepare)(pdisp_encoder_t pdisp_encoder);
    DISP_STATUS (*dpms)(pdisp_encoder_t pdisp_encoder, DISP_POWER_STATE state);
    DISP_STATUS (*hw_init)(pdisp_encoder_t pdisp_encoder);
    DISP_STATUS (*hw_deinit)(pdisp_encoder_t pdisp_encoder);
    DISP_STATUS (*policy_config)(pdisp_encoder_t pdisp_encoder, DISP_COLORIMETRY colorimetry);
    DISP_STATUS (*edp_psr_set)(pdisp_encoder_t pdisp_encoder, DISP_PSR_SET_TYPE psr_type);
    DISP_STATUS (*set_audio_info)(pdisp_encoder_t pdisp_encoder, pdisp_audio_infoframe_t paudio);
};

DISP_STATUS disp_encoder_sw_init(pdisp_encoder_t pdisp_encoder);
disp_void_t disp_encoder_sw_deinit(pdisp_encoder_t pdisp_encoder);
DISP_STATUS disp_encoder_hw_init(pdisp_encoder_t pdisp_encoder);
DISP_STATUS disp_encoder_hw_deinit(pdisp_encoder_t pdisp_encoder);
DISP_STATUS disp_encoder_dpms(pdisp_encoder_t pdisp_encoder, disp_u8_t benable);
DISP_STATUS disp_encoder_set_mode(pdisp_encoder_t pdisp_encoder);
DISP_STATUS disp_encoder_policy_config(pdisp_encoder_t pdisp_encoder);

#endif
