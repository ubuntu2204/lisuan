/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#ifndef __LISUAN_KMS_H__
#define __LISUAN_KMS_H__

#include <drm/drm_crtc.h>
#include <drm/drm_plane.h>
#include <drm/drm_encoder.h>
#include <drm/drm_connector.h>
#include "gf_os_interface.h"
#if DRM_VERSION_CODE >= KERNEL_VERSION(6,6,0)
#include <drm/display/drm_dp_helper.h>
#else
#include <drm/drm_dp_helper.h>
#endif
#include <linux/hrtimer.h>
#include "display.h"

struct lisuan_device;
struct plane_t;
struct crtc_t;
struct encoder_t;
struct connector_t;

struct display_adapter {
	struct lisuan_device *gdevice;
	void *display_device;
	unsigned char *mmio_base;
	unsigned char *rom_base;

	//resource
	int crtc_num;
	int plane_num;
	int pu_support;
	int connector_support;

	int connector_detected;
	int connector_num;
	struct crtc_t *crtcs;
	struct encoder_t *encoders;
	struct connector_t *connectors;

	atomic_t hpd_mask;
	int detected_mask; // protected by mode_config.mutex

	union {
		struct {
			unsigned int sgnt_enable            : 1;
			unsigned int sgnt_intr_a            : 1;
			unsigned int sgnt_intr_b            : 1;
			unsigned int sgnt_intr_c            : 1;
			unsigned int sgnt_intr_d            : 1;
			unsigned int reserved               :27;
		} bits;
		unsigned int val;
	} flag;
};


struct plane_state_t {
	struct drm_plane_state base;
	int key_mode;
	unsigned int kp_ks;
	int alpha_sel;
	int key_sel;
	bool invert_alpha;
	int cursor_type;
	int key_value;

	struct drm_property_blob* hdr_dg_lut_blob; // lut_y + lut_s, length: 2 * HDR_DEGAMMA_LUT_LENGTH
	struct drm_property_blob* sgnt_param_blob;
};

struct plane_t {
	int index;
	int crtc_index;
	struct drm_plane base;
	struct display_adapter *adapter;
	struct drm_property *key_mode;
	struct drm_property *kp_ks;
	struct drm_property *alpha_sel;
	struct drm_property *key_sel;
	struct drm_property *invert_alpha;
	struct drm_property *cursor_type;
	struct drm_property *key_value;
	struct drm_property *hdr_dg_lut;
	struct drm_property *sgnt_param;

	void *phdr_mgr;
};

struct pipe_crc_t {
	bool crc_enabled;
	bool has_window;
	uint16_t window_x_start;
	uint16_t window_x_end;
	uint16_t window_y_start;
	uint16_t window_y_end;
};

struct crtc_state_t {
	struct drm_crtc_state base;
	uint64_t hdr_info_u64;
	uint64_t hdr_mode_u64;
};

struct gf_legacy_cursor_info {
	struct drm_gem_object *gem_obj;
	uint32_t width;
	uint32_t height;
};

struct crtc_t {
	struct drm_crtc base;
	struct display_adapter *adapter;
	struct plane_t *planes;
	int index;
	bool dpms;

	//for virtaul vsync timer
	struct hrtimer vblank_hrtimer;

	struct pipe_crc_t pipe_crc;
	struct drm_property *hdr_info;
	struct drm_property *hdr_mode;
	struct gf_legacy_cursor_info legacy_cursor;
};

struct encoder_t {
	struct drm_encoder base;
	struct display_adapter *adapter;
	int connector_index;
	int index;
};

struct cts_dp_state_t {
	unsigned int valid_bits;
	unsigned int link_rate;
	unsigned int lane_count;
	unsigned int tps;
	unsigned int async_clock;
	unsigned int range_type;
	unsigned int ycbcr_coefficients;
	unsigned int enhanced_frame;
};

struct dp_compliance_data {
	unsigned long edid;
	u8 video_pattern;
	u16 hdisplay, vdisplay;
	u8 bpc;
	struct drm_dp_phy_test_params phytest;
	unsigned int range_type;
	unsigned int color_format;
};

struct dp_complicance {
	unsigned long test_type;
	struct dp_compliance_data test_data;
	bool test_active;
	int test_link_rate;
	u8 test_lane_count;
};

struct connector_state_t {
	struct drm_connector_state base;
	int output_signal;
	int color_depth;
	int colorimetry;

/** UMD set dp param to KMD, UMD -> KMD */
	struct cts_dp_state_t cts_dp;

/** dp complicance test for KMD sync UMD, KMD hpd_event -> UMD */
	struct dp_complicance compliance;
};

struct dp_hpd_status_t {
	unsigned char sink_count;			//dpcd 0x200
	unsigned char irq_vector;			//dpcd 0x201
	unsigned char dp_lane0_1_status;	//dpcd 0x202
	unsigned char dp_lane2_3_status;	//dpcd 0x203
	unsigned char dp_lane_align;		//dpcd 0x204
	unsigned char dp_sink_status;		//dpcd 0x205
	unsigned char auto_test_req;		//dpcd 0x218
	unsigned char test_link_bw;			//dpcd 0x219
	unsigned char test_lane_count;		//dpcd 0x220
	unsigned char lane_count;			//dpcd 0x002
};

struct connector_ddc_func_t {
	// Mulit detect need multi-mutex-lock/unlock
	int (*irq_event)(struct connector_t *connector);
	int (*encoder_set_mode)(struct connector_t *pddc);
	int (*encoder_dpms)(struct connector_t *pddc, bool enable);
	int (*backlight_query)(struct connector_t *connector, pdisp_backlight_query_t pquery);
	int (*backlight_set)(struct connector_t *connector, pdisp_backlight_set_t pset);
	int (*psr_query)(struct connector_t *connector, pdisp_psr_query_t pquery);
	int (*psr_set)(struct connector_t *connector, pdisp_psr_set_t pset);
	int (*aux_transfer)(struct connector_t *connector, pdisp_aux_para_t paux);
	int (*aux_readb)(struct connector_t *connector, unsigned int offset, unsigned char *val);
	int (*aux_writeb)(struct connector_t *connector, unsigned int offset, unsigned char val);
};

struct connector_t {
	struct drm_connector base;
	struct display_adapter *adapter;
	int index;
	unsigned char edid[512];
	int mode_num;
	pdisp_display_mode_t modes;
	struct drm_property *output_signal;
	struct drm_property *color_depth;
	struct drm_property *colorimetry;
	// cts dp
	struct drm_property *dp_cts_valid_bits;
	struct drm_property *dp_link_rate;
	struct drm_property *dp_lane_count;
	struct drm_property *dp_tps;
	struct drm_property *dp_async_clock;
	struct drm_property *dp_range_type;
	struct drm_property *dp_ycbcr_coefficients;
	struct drm_property *dp_enhanced_frame;
	struct dp_hpd_status_t dp_hpd_status;

	unsigned char train_set[4];//not sure if useful

	struct connector_ddc_func_t ddc_func;
	struct os_mutex ddc_mutex;
};

#define dplane_to_gplane(x) container_of(x, struct plane_t, base)
#define dcrtc_to_gcrtc(x) container_of(x, struct crtc_t, base)
#define dencoder_to_gencoder(x) container_of(x, struct encoder_t, base)
#define dconnector_to_gconnector(x) container_of(x, struct connector_t, base)

#define dplane_state_to_gplane(x) container_of(x, struct plane_state_t, base)
#define dcrtc_state_to_gcrtc(x) container_of(x, struct crtc_state_t, base)
#define dconnector_state_to_gconnector(x) container_of(x, struct connector_state_t, base)

# define GF_DP_TEST_LINK_TRAINING            (1 << 0)
# define GF_DP_TEST_LINK_VIDEO_PATTERN        (1 << 1)
# define GF_DP_TEST_LINK_EDID_READ            (1 << 2)
# define GF_DP_TEST_LINK_PHY_TEST_PATTERN        (1 << 3) /* DPCD >= 1.1 */
# define GF_DP_TEST_LINK_FAUX_PATTERN        (1 << 4) /* DPCD >= 1.2 */
# define GF_DP_TEST_LINK_AUDIO_PATTERN         (1 << 5) /* DPCD >= 1.2 */
# define GF_DP_TEST_LINK_AUDIO_DISABLED_VIDEO  (1 << 6) /* DPCD >= 1.2 */


int gf_kms_init(struct lisuan_device *gdevice);
int gf_kms_resume(struct lisuan_device *gdevice);
int gf_kms_deinit(struct lisuan_device *gdevice);

#endif
