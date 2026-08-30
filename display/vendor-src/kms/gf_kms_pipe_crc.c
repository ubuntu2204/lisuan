/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#include "gf_kms_pipe_crc.h"
#include "gf_kms.h"
#include "gf_kms_dm.h"
#include "gf_os_interface.h"

#include <drm/drm_vblank.h>

#define GF_CRC_CNT 4

static const char * const pipe_crc_sources[] = {
	"none",
	"auto",
	"super"
};

const char *const *gf_kms_get_crc_sources(struct drm_crtc *crtc,
						size_t *count)
{
	*count = ARRAY_SIZE(pipe_crc_sources);
	return pipe_crc_sources;
}

static enum gf_kms_pipe_crc_source gf_kms_crc_parse_source(const char *src_name, bool *enabled, bool *has_window)
{
	if (!src_name || (strcmp(src_name, "none") == 0)) {
		*enabled = false;
		*has_window = false;
		return PIPE_CRC_SOURCE_NONE;
	} else if (strcmp(src_name, "auto") == 0) {
		*enabled = true;
		*has_window = false;
		return PIPE_CRC_SOURCE_AUTO;
	} else if (strcmp(src_name, "super") == 0) {
		*enabled = true;
		*has_window = true;
		return PIPE_CRC_SOURCE_SUPER;
	}

	*enabled = false;
	*has_window = false;
	return PIPE_CRC_SOURCE_INVALID;
}

int gf_kms_verify_crc_source(struct drm_crtc *crtc, const char *src_name,
			   size_t *values_cnt)
{
	bool enabled, has_window;

	if (gf_kms_crc_parse_source(src_name, &enabled, &has_window) == PIPE_CRC_SOURCE_INVALID) {
		gf_printk(GF_DRV_ERROR, "unknown source %s\n", src_name);
		return -EINVAL;
	}

	*values_cnt = GF_CRC_CNT;

	return 0;
}

int gf_kms_set_crc_source(struct drm_crtc *crtc, const char *source)
{
	struct crtc_t *gcrtc = dcrtc_to_gcrtc(crtc);
	struct display_adapter *adapter = gcrtc->adapter;
	struct pipe_crc_t *ppipe_crc = &gcrtc->pipe_crc;
	enum gf_kms_pipe_crc_source crc_source = gf_kms_crc_parse_source(source, &ppipe_crc->crc_enabled, &ppipe_crc->has_window);

	if (crc_source == PIPE_CRC_SOURCE_INVALID) {
		gf_printk(GF_DRV_ERROR, "unknown source %s\n", source);
		return -EINVAL;
	}

	if (ppipe_crc->crc_enabled) {
		if (ppipe_crc->has_window) {
			//to do
		} else {//full screen
			ppipe_crc->window_x_start = 0;
			ppipe_crc->window_y_start = 0;
			ppipe_crc->window_x_end = crtc->state->adjusted_mode.hdisplay;
			ppipe_crc->window_y_end = crtc->state->adjusted_mode.vdisplay;

		}
	}

	dm_pipe_set_crc(adapter, gcrtc);

	return 0;
}

int gf_kms_crtc_get_crc(struct drm_crtc *crtc)
{
	struct crtc_t *gcrtc = dcrtc_to_gcrtc(crtc);
	uint32_t crcs[GF_CRC_CNT] = {0};

	if (!gcrtc->pipe_crc.crc_enabled) {
		return -1;
	}

	dm_pipe_get_crc(gcrtc->adapter, gcrtc, &crcs[0], &crcs[1], &crcs[2], &crcs[3]);

	return drm_crtc_add_crc_entry(crtc, true,
			drm_crtc_accurate_vblank_count(crtc), crcs);
}
