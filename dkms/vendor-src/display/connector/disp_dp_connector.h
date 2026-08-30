/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#ifndef __DISP_DP_CONNECTOR_H__
#define __DISP_DP_CONNECTOR_H__

#include "display.h"
#include "disp_dp_aux.h"

typedef struct _disp_dp_sink_caps_t
{
    disp_u8_t dpcd[DP_RECEIVER_CAP_SIZE];
    disp_u8_t psr_dpcd[EDP_PSR_RECEIVER_CAP_SIZE];
    disp_u8_t downstream_ports[DP_MAX_DOWNSTREAM_PORTS];
    disp_u8_t edp_dpcd[EDP_DISPLAY_CTL_CAP_SIZE];
    disp_u8_t dsc_dpcd[DP_DSC_RECEIVER_CAP_SIZE];
    disp_u8_t lttpr_common_caps[DP_LTTPR_COMMON_CAP_SIZE];
    disp_u8_t lttpr_phy_caps[DP_MAX_LTTPR_COUNT][DP_LTTPR_PHY_CAP_SIZE];
    disp_u8_t fec_capable;
    disp_u8_t sink_status[DP_SINK_STATUS_SIZE]; // Link/Sink Device Status
}disp_dp_sink_caps_t, *pdisp_dp_sink_caps_t;

typedef struct _disp_edp_info_t
{
    struct disp_edp_backlight_info backlight;
    struct disp_edp_psr_info psr;

    disp_u8_t support_backlight_pwm_pin         : 1;
    disp_u8_t support_tcon_backlight_adjustment : 1;
    disp_u8_t support_psr                       : 1;
} disp_edp_info_t, *pdisp_edp_info_t;

disp_void_t disp_dp_connector_sw_init(pdisp_void_t pdisp_connector);
disp_void_t disp_dp_connector_sw_deinit(pdisp_void_t pdisp_connector);
disp_s32_t disp_dp_set_sink_dsc(pdisp_connector_t pdisp_connector, disp_u8_t enable);
disp_s32_t disp_dp_set_sink_fec(pdisp_connector_t pdisp_connector, disp_u8_t enable);
disp_s32_t disp_dp_set_main_link_coding(pdisp_connector_t pdisp_connector);
disp_s32_t disp_dp_set_sink_power_state(pdisp_connector_t pdisp_connector, disp_u8_t enable);
disp_void_t disp_dp_set_sink_ssc(pdisp_connector_t pdisp_connector, disp_u8_t ssc_on);
#endif
