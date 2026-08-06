/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#include <drm/drm_crtc.h>

enum gf_kms_pipe_crc_source {
	PIPE_CRC_SOURCE_NONE = 0,
	PIPE_CRC_SOURCE_AUTO,
	PIPE_CRC_SOURCE_SUPER,
	PIPE_CRC_SOURCE_INVALID = -1,
};

const char *const *gf_kms_get_crc_sources(struct drm_crtc *crtc,
						size_t *count);

int gf_kms_verify_crc_source(struct drm_crtc *crtc, const char *source,
				size_t *values_cnt);

int gf_kms_set_crc_source(struct drm_crtc *crtc, const char *source);

int gf_kms_crtc_get_crc(struct drm_crtc *crtc);