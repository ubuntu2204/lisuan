/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#ifndef __DISP_DP_ENCODER_H__
#define __DISP_DP_ENCODER_H__

#include "display.h"

#define HW_LINKTRAINING_TIMEOUT_IN_US 10000
#define DP_VID_N 0x8000
#define DP_AUD_N 0x8000

#define DP_RBR_SYM_CLK_IN_KHZ 162000
#define DP_HBR_SYM_CLK_IN_KHZ 270000
#define DP_HBR2_SYM_CLK_IN_KHZ 540000
#define DP_HBR3_SYM_CLK_IN_KHZ 810000

#define DP_SDP_INFOFRAME_MAX_DATA_BYTES 28 // infoframe sdp only support 28 bytes payload
/* struct disp_dp_infoframe_sdp - sdp infoframe:
 *
 * ref: DP spec v2.1 [Table 2-128 - Table 2-131]
 *
 * @sdp_id: 0(non-audio), packetID(audio)
 * @sdp_type: 0x80 + infoframe type, 0x84(audio)
 * @data_count: data byte count, 10 bits
 * @sdp_ver: 0x12 or 0x13, 6 bits
 * @infoframe_ver: DB[0], // SDPv1.3 only
 * @infoframe_len: DB[1], // SDPv1.3 only
 * @dp_sdp_uint[7]: DB[0]~DB[27](SDPv1.2), DB[2]~DB[29](SDPv1.3)
 */
struct disp_dp_sdp_infoframe {
    disp_u8_t sdp_id;
    disp_u8_t sdp_type;
    disp_u16_t data_count;
    disp_u8_t sdp_ver;
    disp_u8_t infoframe_ver;
    disp_u8_t infoframe_len;
    disp_u8_t data_u8[DP_SDP_INFOFRAME_MAX_DATA_BYTES];
};

disp_void_t disp_dp_encoder_sw_init(pdisp_void_t pdisp_encoder);

#endif
