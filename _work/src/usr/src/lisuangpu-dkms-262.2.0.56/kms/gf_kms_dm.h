/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#ifndef __GF_KMS_DM_H__
#define __GF_KMS_DM_H__

#include "gf_kms.h"

int dm_hw_init(struct display_adapter *adapter);
int dm_init(struct display_adapter *adapter);
int dm_deinit(struct display_adapter *adapter);
int dm_query_resource(struct display_adapter *adapter);
int dm_detect_connector(struct display_adapter *adapter, int to_detect, int to_fake, int* detected);
int dm_connector_get_modelist(struct display_adapter *adapter, struct connector_t *connector);
int dm_connector_get_edid(struct display_adapter *adapter, struct connector_t *connector);
int dm_encoder_dpms(struct display_adapter *adapter, struct encoder_t *encoder, bool on);
int dm_encoder_set_mode(struct display_adapter *adapter, struct encoder_t *encoder);
int dm_crtc_dpms(struct display_adapter *adapter, struct crtc_t *crtc, bool on);
int dm_crtc_set_mode(struct display_adapter *adapter, struct crtc_t *crtc,
					 pdisp_display_mode_t mode, pdisp_display_mode_t hw_mode);
int dm_plane_implement(struct display_adapter *adapter, pdisp_plane_update_para_t param, unsigned char is_update);
unsigned int dm_crtc_get_vblank_counter(struct display_adapter *adapter, int crtc_index);
int dm_crtc_enable_vblank(struct display_adapter *adapter, int crtc_index, bool enable);
int dm_crtc_get_hv_position(struct display_adapter *adapter, int crtc_index, int *hpos, int *vpos, int *in_hblank, int *in_vblank);
int dm_intr_enable(struct display_adapter *adapter, bool enable, int intr_bit);
int dm_intr_get_status(struct display_adapter *adapter);
int dm_intr_multi_vsync_enable(struct display_adapter *adapter, bool enable);
int dm_intr_clear_vsync_status(struct display_adapter *adapter, int crtc_index);
int dm_pipe_set_crc(struct display_adapter *adapter, struct crtc_t *gcrtc);
int dm_pipe_get_crc(struct display_adapter *adapter, struct crtc_t *gcrtc,
					uint32_t *crc_0, uint32_t *crc_1, uint32_t *crc_2, uint32_t *crc_3);
int dm_pipe_get_sgnt_value(struct display_adapter *adapter, uint32_t crtc_index, uint32_t *psgnt, uint32_t sgnt_len);
int dm_pipe_set_csc(struct display_adapter *adpter, pdisp_csc_para_t pcsc_para);
int dm_pipe_set_gamma(struct display_adapter *adpter, pdisp_gamma_info_t pgamma_info);
int dm_aux_transfer(struct display_adapter *adapter, pdisp_aux_para_t paux_para);
int dm_get_hpd_state(struct display_adapter *adapter, pdisp_get_hpd_state_t phpd_state);
int dm_connector_get_i2c_status(struct display_adapter *adapter, pdisp_get_i2c_status_t pdisp_i2c_status);
int dm_check_underflow(struct display_adapter *adapter);
unsigned char dm_read_rom_byte(struct display_adapter *adapter, int offset);
int dm_notify_irq_hpd(struct display_adapter *adapter, disp_u8_t connector_index);
int dm_backlight_query(struct display_adapter *adapter, pdisp_backlight_query_t pquery);
int dm_backlight_set(struct display_adapter *adapter, pdisp_backlight_set_t pset);
int dm_psr_query(struct display_adapter *adapter, pdisp_psr_query_t pquery);
int dm_psr_set(struct display_adapter *adapter, pdisp_psr_set_t pset);
int dm_hda_handle(struct display_adapter *adapter);
#endif
