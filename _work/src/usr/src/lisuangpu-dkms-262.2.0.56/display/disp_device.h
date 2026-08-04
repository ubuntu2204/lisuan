/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#ifndef __DISP_DEVICE_H__
#define __DISP_DEVICE_H__

#include "display.h"
#include "disp_plane.h"
#include "disp_pipe.h"
#include "disp_codec.h"
#include "disp_crtc.h"
#include "disp_encoder.h"
#include "disp_connector.h"
#include "disp_dsc.h"

#define GET_LAST_BIT(nr) ((~((nr)-1)) & (nr))
#define DISP_MAX(a,b) (((a) > (b)) ? (a) : (b))
#define DISP_MIN(a,b) (((a) < (b)) ? (a) : (b))
#ifndef ABS
#define ABS(a) (((a)>0)? (a) : (-a))
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr)/sizeof((arr)[0]))
#endif

#define DISP_FEC_BANDWIDTH(bd) (bd + bd * (6 * 256 + 1) / (256 * 256 + 1))

/* Some arm arch kernel not support floating point, but may useful in the future, so keep comment here.
static inline disp_s64_t disp_ceil(double x)
{
    disp_s64_t intx = (disp_s64_t)x;
    return (x - intx != 0) ? intx + 1 : intx;
}*/
#define disp_ceil_div(x, div) (((x) + (div) - 1) / (div))

// transfer CPU little endian to big endian, for some special struct
static inline disp_u16_t u16_to_be16(disp_u16_t u16)
{
    return (u16 >> 8) + ((u16 & 0xff) << 8);
}
static inline disp_u32_t u32_to_be32(disp_u32_t u32)
{
    return (u32 >> 24) + ((u32 & 0xff0000) >> 8) + ((u32 & 0xff00) << 8) + ((u32 & 0xff) << 24);
}
static inline disp_u16_t be16_to_u16(disp_u16_t be16) { return u16_to_be16(be16); }
static inline disp_u32_t be32_to_u32(disp_u32_t be32) { return u32_to_be32(be32); }

static inline disp_u16_t get_big_endian_16(disp_u16_t pps_bytes)
{
    return (pps_bytes >> 8) + ((pps_bytes & 0xff) << 8);
}

static inline disp_u32_t get_bit_num(disp_u32_t value)
{
    disp_u32_t count = 0;

    while (value) {
        value &= (value - 1);
        count++;
    }

    return count;
}

#define DISP_CRTC_NUM 4
#define DISP_ENCODER_NUM DISP_CRTC_NUM
#define DISP_ENCODER_NUM_DP DISP_ENCODER_NUM
#define DISP_ENCODER_NUM_HDMI (DISP_ENCODER_NUM - DISP_ENCODER_NUM_DP)
#define DISP_CONNECTOR_NUM DISP_CRTC_NUM
#define DISP_DSC_NUM ((DISP_CRTC_NUM + 1) / 2)

// PZ1 config
#define PZ1_RD_INTERVAL 1600 // 8.1Gbps, 400 is not enough for pz1
// PZ1 phy12
#define PZ1_PHY_CAPTURE_HPD_EVENT   0x1
#define PZ1_PHY_CAPTURE_RESKEW      0x4000
#define PZ1_PHY_CAPTURE_SHOW_DP     0x8000

typedef struct _disp_device_t
{
    disp_void_t *disp_adapter;//given by driver, mainly used to touch register
    DISP_DEVICE_ID device_id;//vendor id is useless

    disp_crtc_t disp_crtc[DISP_CRTC_NUM];
    disp_encoder_t disp_encoder[DISP_ENCODER_NUM];
    disp_connector_t disp_connector[DISP_CONNECTOR_NUM];
    pdisp_void_t pdisp_dsc;
    //disp_interrupt_t disp_interrupt;

    struct {
        disp_u32_t sr[DISP_CRTC_NUM];
        disp_u32_t cr[DISP_CRTC_NUM];
        disp_u32_t plns[DISP_CRTC_NUM];
        disp_u32_t pipe[DISP_CRTC_NUM];
        disp_u32_t dp[DISP_CRTC_NUM];
        disp_u32_t pll[DISP_CRTC_NUM];
        disp_u32_t ppa[DISP_CRTC_NUM];
        disp_u32_t sr_share;
        disp_u32_t cr_share;
        disp_u32_t dsc[DISP_DSC_NUM];
        disp_u32_t dhi;
        disp_u32_t hau;
        disp_u32_t intr;
        disp_u32_t pwm;
        disp_u32_t layer_plns[DISP_OVERLAY_PLANE_NUM + 1];
        disp_u32_t layer_hdr[DISP_OVERLAY_PLANE_NUM + 1];
    } reg_off;

    union
    {
        disp_u32_t uint;
        struct
        {
            disp_u32_t multi_vsync_enable           : 1;
            disp_u32_t pz1_config                   : 1; // 0-real chip, 1-pz1
            disp_u32_t pz1_capture_pipe             : 1; // 0-encoder, 1-pipe
            disp_u32_t dvo_enable                   : 1; // 0-normal encoder, 1-pipe_a dvo
            disp_u32_t prefer_free_sync             : 1;
            disp_u32_t dp_cts                       : 1;
            disp_u32_t mode_8k                      : 1;
            disp_u32_t hau_sw_csb                   : 1;
            disp_u32_t hau_sw_info                  : 1;
            disp_u32_t hau_asp_8ch                  : 1;
            disp_u32_t reserved                     :22;
        }info;
    }flag;
}disp_device_t, *pdisp_device_t;

DISP_STATUS disp_device_sw_init(pdisp_device_t pdisp_device);
DISP_STATUS disp_device_sw_deinit(pdisp_device_t pdisp_device);
DISP_STATUS disp_device_hw_init(pdisp_device_t pdisp_device);
DISP_STATUS disp_device_hw_deinit(pdisp_device_t pdisp_device);
disp_u8_t disp_device_get_support_connector(pdisp_device_t pdisp_device);

#endif
