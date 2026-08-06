/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#include "gf_kms.h"
#include "lisuan_device.h"
#include "gf_kms_dm.h"
#include "gf_kms_virt.h"
#include "gf_kms_pipe_crc.h"
#include "lisuan_gem.h"
#include "lisuan_vm.h"
#include "gf_debugfs.h"
#include "gf_kms_hdr.h"
#include "lisuan_drm_fourcc.h"

#include <drm/drm_modeset_helper_vtables.h>
#include <uapi/drm/drm_fourcc.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_edid.h>
#include <drm/drm_gem_framebuffer_helper.h>
#if DRM_VERSION_CODE >= KERNEL_VERSION(6,6,0)
#include <linux/cma.h>
#include <drm/drm_gem.h>
#else
#include <drm/drm_gem_cma_helper.h>
#endif
#include <drm/drm_vblank.h>
#if DRM_VERSION_CODE >= KERNEL_VERSION(5,1,0)
#include <drm/drm_probe_helper.h>
#else
#include <drm/drm_crtc_helper.h>
#endif
#include <drm/drm_crtc.h>
#include <drm/drm_connector.h>
#include <drm/drm_atomic.h>
#include <drm/drm_writeback.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_blend.h>
#include <drm/drm_color_mgmt.h>
#include <drm/drm_framebuffer.h>
#if DRM_VERSION_CODE >= KERNEL_VERSION(5,15,0)
#include <drm/drm_gem_atomic_helper.h>
#endif

static const struct drm_prop_enum_list cursor_type[] = {
	{DISP_CS_TYPE_MONO, "mono_cursor"},
	{DISP_CS_TYPE_COLOR_ALPHA, "normal_alpha"},
	{DISP_CS_TYPE_COLOR_PREALP, "premultiple_alpha"},
	{DISP_CS_TYPE_COLOR_AX, "AX"},
	{DISP_CS_TYPE_COLOR_MASKED, "mask"},
	//Only allowed mask value are 0x00 and 0xff.
	//When the mask value is 0x00, the cursor data will replace the screen data. When the value is 0xff, an XOR operation is performed bewteen cursor and screen data
};

static const struct drm_prop_enum_list key_mode_type[] = {
	{DISP_BLEND_A_WINDOW_KEY, "a_over_b_window_key"},
	{DISP_BLEND_B_WINDOW_KEY, "b_over_a_window_key"},
	{DISP_BLEND_A_ALPHA_KEY, "a_over_b_alpha_key"},
	{DISP_BLEND_B_ALPHA_KEY, "b_over_a_alpha_key"},
	{DISP_BLEND_A_COLOR_KEY, "a_over_b_color_key"},
	{DISP_BLEND_B_COLOR_KEY, "b_over_a_color_key"},
	{DISP_BLEND_A_ALPHA_BLENDING, "a_over_b_alpha_blend"},
	{DISP_BLEND_B_ALPHA_BLENDING, "b_over_a_alpha_blend"},
	{DISP_BLEND_A_CONSTANT_ALPHA, "a over b constant_alpha"},
	{DISP_BLEND_B_CONSTANT_ALPHA, "b over a constant_alpha"},
};

static const struct drm_prop_enum_list key_select_type[] = {
	{DISP_KEY_SEL_A_OR_B_KEY, "output_key = a.key | b.key"},
	{DISP_KEY_SEL_A_KEY, "output a key"},
	{DISP_KEY_SEL_B_KEY, "output b key"},
};

static const struct drm_prop_enum_list alpha_select_type[] = {
	{DISP_ALP_SEL_MIX_ALPHA, "output_mixed_alpha"},
	{DISP_ALP_SEL_A_ALPHA, "output_a_alpha"},
	{DISP_ALP_SEL_B_ALPHA, "output_b_alpha"},
};

static int atomic_replace_property_blob_from_id(struct drm_device *dev,
					struct drm_property_blob **blob,
					uint64_t blob_id,
					ssize_t expected_size,
					ssize_t expected_elem_size,
					bool *replaced)
{
	struct drm_property_blob *new_blob = NULL;

	if (blob_id != 0) {
		new_blob = drm_property_lookup_blob(dev, blob_id);
		if (new_blob == NULL)
			return -EINVAL;

		if (expected_size > 0 &&
			new_blob->length != expected_size) {
			drm_property_blob_put(new_blob);
			return -EINVAL;
		}
		if (expected_elem_size > 0 &&
			new_blob->length % expected_elem_size != 0) {
			drm_property_blob_put(new_blob);
			return -EINVAL;
		}
	}

	*replaced |= drm_property_replace_blob(blob, new_blob);
	drm_property_blob_put(new_blob);

	return 0;
}

static void crtc_install_property(struct crtc_t *gcrtc)
{
	struct drm_crtc *crtc = &gcrtc->base;
	struct drm_device *ddev = crtc->dev;
	property_crtc_hdr_mode_s hdr_mode;
	property_crtc_hdr_info_s hdr_info;

	gcrtc->hdr_info = drm_property_create_range(ddev, DRM_MODE_PROP_RANGE, "hdr_info", 0, 0xffffffffffffffffull);
	gcrtc->hdr_mode = drm_property_create_range(ddev, DRM_MODE_PROP_RANGE, "hdr_mode", 0, 0xffffffffffffffffull);
	if (!gcrtc->hdr_info || !gcrtc->hdr_mode) {
		gf_printk(GF_DRV_ERROR, "create crtc hdr_mode property fail\n");
		return;
	}
	hdr_info.u64_value = 0;
	hdr_info.mem.hdr_en_0 = 0;
	hdr_info.mem.hdr_en_1 = 0;
	hdr_info.mem.cs_in_0 = hdr_cs_type_srgb;
	hdr_info.mem.tf_in_0 = hdr_tf_type_srgb;
	hdr_info.mem.cs_in_1 = hdr_cs_type_srgb;
	hdr_info.mem.tf_in_1 = hdr_tf_type_srgb;
	hdr_info.mem.max_lum_in_0 = 0;
	hdr_info.mem.max_lum_in_1 = 0;
	hdr_mode.u64_value = 0;
	hdr_mode.mem.cs_out = hdr_cs_type_srgb;
	hdr_mode.mem.tf_out = hdr_tf_type_srgb;
	hdr_mode.mem.min_lum_1_10000 = 0;
	hdr_mode.mem.max_lum = 100;
	hdr_mode.mem.max_fall = 50;
	drm_object_attach_property(&crtc->base, gcrtc->hdr_info, hdr_info.u64_value);
	drm_object_attach_property(&crtc->base, gcrtc->hdr_mode, hdr_mode.u64_value);
}

static void crtc_reset(struct drm_crtc *crtc)
{
	struct crtc_state_t *gstate = dcrtc_state_to_gcrtc(crtc->state);
	property_crtc_hdr_info_s hdr_info;
	property_crtc_hdr_mode_s hdr_mode;

	if (gstate)
	{
		crtc->funcs->atomic_destroy_state(crtc, crtc->state);
		crtc->state = NULL;
		gstate = NULL;
	}

	gstate = gf_calloc(sizeof(struct crtc_state_t));

	if (gstate) {
		hdr_info.u64_value = 0;
		hdr_info.mem.hdr_en_0 = 0;
		hdr_info.mem.hdr_en_1 = 0;
		hdr_info.mem.cs_in_0 = hdr_cs_type_srgb;
		hdr_info.mem.tf_in_0 = hdr_tf_type_srgb;
		hdr_info.mem.cs_in_1 = hdr_cs_type_srgb;
		hdr_info.mem.tf_in_1 = hdr_tf_type_srgb;
		hdr_info.mem.max_lum_in_0 = 0;
		hdr_info.mem.max_lum_in_1 = 0;
		hdr_mode.u64_value = 0;
		hdr_mode.mem.cs_out = hdr_cs_type_srgb;
		hdr_mode.mem.tf_out = hdr_tf_type_srgb;
		hdr_mode.mem.min_lum_1_10000 = 0;
		hdr_mode.mem.max_lum = 100;
		hdr_mode.mem.max_fall = 50;

		gstate->hdr_info_u64 = hdr_info.u64_value;
		gstate->hdr_mode_u64 = hdr_mode.u64_value;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 20, 0)
		__drm_atomic_helper_crtc_reset(crtc, &gstate->base);
#else
		crtc->state = kzalloc(sizeof(*crtc->state), GFP_KERNEL);
		if (crtc->state)
			crtc->state->crtc = crtc;
#endif
		drm_crtc_vblank_reset(crtc);
#ifndef __VIRTAUL_VBLANK__
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 20, 0)
		drm_crtc_set_max_vblank_count(crtc, 0xFFFF);
#else
		crtc->dev->max_vblank_count = 0xFFFF;
#endif
#endif
	} else {
		gf_printk(GF_DRV_ERROR, "crtc_reset, malloc failed\n");
	}
}

static int crtc_atomic_set_property(struct drm_crtc *crtc, struct drm_crtc_state *state, struct drm_property *property, uint64_t val)
{
	struct crtc_t *gcrtc = dcrtc_to_gcrtc(crtc);
	struct crtc_state_t *gstate = dcrtc_state_to_gcrtc(state);
	int ret = 0;
	bool replaced = false;

	if (property == gcrtc->hdr_info) {
		gstate->hdr_info_u64 = val;
	} else if (property == gcrtc->hdr_mode) {
		gstate->hdr_mode_u64 = val;
	} else {
		gf_printk(GF_DRV_ERROR, "unknown crtc property");
		ret = -EINVAL;
	}

	return ret;
}

static int crtc_atomic_get_property(struct drm_crtc *crtc, const struct drm_crtc_state *state, struct drm_property *property, uint64_t *val)
{
	struct crtc_t *gcrtc = dcrtc_to_gcrtc(crtc);
	struct crtc_state_t *gstate = dcrtc_state_to_gcrtc(state);
	int ret = 0;

	if (property == gcrtc->hdr_info) {
		*val = gstate->hdr_info_u64;
	} else if (property == gcrtc->hdr_mode) {
		*val = gstate->hdr_mode_u64;
	} else {
		gf_printk(GF_DRV_ERROR, "unknown crtc property");
		ret = -EINVAL;
	}

	return 0;
}

static void crtc_atomic_destroy_state(struct drm_crtc *crtc, struct drm_crtc_state *state)
{
	struct crtc_state_t *gstate = dcrtc_state_to_gcrtc(state);

	__drm_atomic_helper_crtc_destroy_state(state);

	if (gstate)
		gf_free(gstate);
}

static struct drm_crtc_state* crtc_atomic_duplicate_state(struct drm_crtc *crtc)
{
	struct crtc_t *gcrtc = dcrtc_to_gcrtc(crtc);
	struct crtc_state_t *state, *old_state;

	old_state = dcrtc_state_to_gcrtc(crtc->state);
	if (old_state)
		state = kmemdup(old_state, sizeof(struct crtc_state_t), GFP_KERNEL);
	else
		state = kmemdup(crtc->state, sizeof(struct crtc_state_t), GFP_KERNEL);

	if (!state)
		return NULL;

	__drm_atomic_helper_crtc_duplicate_state(crtc, &state->base);

	return &state->base;
}

static void plane_install_property(struct plane_t *gplane, bool enable_cts_property)
{
	struct drm_plane *plane = &gplane->base;
	struct drm_device *ddev = plane->dev;

	if (plane->type == DRM_PLANE_TYPE_OVERLAY || plane->type == DRM_PLANE_TYPE_PRIMARY) {
		if (plane->type == DRM_PLANE_TYPE_OVERLAY && enable_cts_property) {
			gplane->key_mode = drm_property_create_enum(ddev, DRM_MODE_PROP_ENUM, "key_mode", key_mode_type, ARRAY_SIZE(key_mode_type));
			gplane->kp_ks = drm_property_create_range(ddev, DRM_MODE_PROP_RANGE, "kp_ks", 0, 0xFF);
			gplane->key_sel = drm_property_create_enum(ddev, DRM_MODE_PROP_ENUM, "key_sel", key_select_type, ARRAY_SIZE(key_select_type));
			gplane->alpha_sel = drm_property_create_enum(ddev, DRM_MODE_PROP_ENUM, "alpha_sel", alpha_select_type, ARRAY_SIZE(alpha_select_type));
			gplane->invert_alpha = drm_property_create_range(ddev, DRM_MODE_PROP_RANGE, "invert_alpha", 0, 1);

			drm_plane_create_alpha_property(plane);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,7,0)
			drm_plane_create_blend_mode_property(plane, BIT(DRM_MODE_BLEND_PIXEL_NONE) | BIT(DRM_MODE_BLEND_PREMULTI) | BIT(DRM_MODE_BLEND_COVERAGE));
#endif

			if (!gplane->key_mode || !gplane->kp_ks ||
				!gplane->key_sel || !gplane->alpha_sel || !gplane->invert_alpha) {
				gf_printk(GF_DRV_ERROR, "create overlay property fail\n");
				return;
			}

			drm_object_attach_property(&plane->base, gplane->key_mode, DISP_BLEND_B_ALPHA_BLENDING);
			drm_object_attach_property(&plane->base, gplane->kp_ks, 0);

			drm_object_attach_property(&plane->base, gplane->alpha_sel, 0);
			drm_object_attach_property(&plane->base, gplane->key_sel, 0);
			drm_object_attach_property(&plane->base, gplane->invert_alpha, 0);
		}

		gplane->key_value = drm_property_create_range(ddev, DRM_MODE_PROP_RANGE, "key value", 0, 0xFFFFFFFF);
		if (!gplane->key_value) {
			gf_printk(GF_DRV_ERROR, "create key value property fail\n");
			return;
		}
		drm_object_attach_property(&plane->base, gplane->key_value, 0);

		gplane->hdr_dg_lut = drm_property_create(ddev, DRM_MODE_PROP_BLOB, "hdr_dg_lut", 0);
		if (!gplane->hdr_dg_lut) {
			gf_printk(GF_DRV_ERROR, "create plane hdr_info property fail\n");
			return;
		}
		drm_object_attach_property(&plane->base, gplane->hdr_dg_lut, 0);

		drm_plane_create_color_properties(
			plane,
			BIT(DRM_COLOR_YCBCR_BT601) |
			BIT(DRM_COLOR_YCBCR_BT709) |
			BIT(DRM_COLOR_YCBCR_BT2020),
			BIT(DRM_COLOR_YCBCR_LIMITED_RANGE) |
			BIT(DRM_COLOR_YCBCR_FULL_RANGE),
			DRM_COLOR_YCBCR_BT709, DRM_COLOR_YCBCR_LIMITED_RANGE);
	} else {
		gplane->cursor_type = drm_property_create_enum(ddev, DRM_MODE_PROP_ENUM, "cursor_type", cursor_type, ARRAY_SIZE(cursor_type));
		if (!gplane->cursor_type) {
			gf_printk(GF_DRV_ERROR, "create cursor type property fail\n");
			return;
		}
		drm_object_attach_property(&plane->base, gplane->cursor_type, DISP_CS_TYPE_COLOR_ALPHA);
	}

	gplane->sgnt_param = drm_property_create(ddev, DRM_MODE_PROP_BLOB, "sgnt_param", 0);
	if (!gplane->sgnt_param) {
		gf_printk(GF_DRV_ERROR, "create plane sgnt_param property fail\n");
		return;
	}
	drm_object_attach_property(&plane->base, gplane->sgnt_param, 0);
}

static void plane_reset(struct drm_plane *plane)
{
	struct plane_state_t *gstate = dplane_state_to_gplane(plane->state);

	if (gstate)
	{
		plane->funcs->atomic_destroy_state(plane, plane->state);
		plane->state = NULL;
		gstate = NULL;
	}

	gstate = gf_calloc(sizeof(struct plane_state_t));

	if (gstate) {
		if (plane->type == DRM_PLANE_TYPE_PRIMARY || plane->type == DRM_PLANE_TYPE_OVERLAY) {
			gstate->key_value = 0;
			if (plane->type == DRM_PLANE_TYPE_OVERLAY) {
				gstate->key_mode = DISP_BLEND_B_ALPHA_BLENDING;
				gstate->kp_ks = 0;
				gstate->alpha_sel = 0;
				gstate->key_sel = 0;
				gstate->invert_alpha = 0;
				if (gstate->hdr_dg_lut_blob) {
					drm_property_blob_put(gstate->hdr_dg_lut_blob);
					gstate->hdr_dg_lut_blob = NULL;
				}
			}
		} else {
			gstate->cursor_type = DISP_CS_TYPE_COLOR_ALPHA;
		}

		if (gstate->sgnt_param_blob) {
			drm_property_blob_put(gstate->sgnt_param_blob);
			gstate->sgnt_param_blob = NULL;
		}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 20, 0)
		__drm_atomic_helper_plane_reset(plane, &gstate->base);
#else
		//see 4.19 kernel dm_drm_plane_reset
		plane->state = &gstate->base;
		if (plane->state) {
			plane->state->plane = plane;
			plane->state->rotation = DRM_MODE_ROTATE_0;

			/* Reset the alpha value to fully opaque if it matters */
			if (plane->alpha_property)
				plane->state->alpha = plane->alpha_property->values[1];
		}
#endif

	} else {
		gf_printk(GF_DRV_ERROR, "plane_reset, malloc failed\n");
	}
}

static int plane_atomic_set_property(struct drm_plane *plane, struct drm_plane_state *state, struct drm_property *property, uint64_t val)
{
	struct plane_t *gplane = dplane_to_gplane(plane);
	struct plane_state_t *gstate = dplane_state_to_gplane(state);
	int ret = 0;
	bool replaced = false;

	if (property == gplane->key_mode) {
		gstate->key_mode = val;
	} else if (property == gplane->kp_ks) {
		gstate->kp_ks = val;
	} else if (property == gplane->alpha_sel) {
		gstate->alpha_sel = val;
	} else if (property == gplane->key_sel) {
		gstate->key_sel = val;
	} else if (property == gplane->invert_alpha) {
		gstate->invert_alpha = val;
	} else if (property == gplane->cursor_type) {
		gstate->cursor_type = val;
	} else if (property == gplane->key_value) {
		gstate->key_value = val;
	} else if (property == gplane->hdr_dg_lut) {
		ret = atomic_replace_property_blob_from_id(plane->dev,
				&gstate->hdr_dg_lut_blob,
				val, 2 * HDR_DEGAMMA_LUT_LENGTH * sizeof(unsigned int), -1,
				&replaced);
	} else if (property == gplane->sgnt_param) {
		ret = atomic_replace_property_blob_from_id(plane->dev,
				&gstate->sgnt_param_blob,
				val, sizeof(property_sgnt_param_s), -1,
				&replaced);
	} else {
		gf_printk(GF_DRV_ERROR, "unknown plane property");
		ret = -EINVAL;
	}

	return ret;
}

static int plane_atomic_get_property(struct drm_plane *plane, const struct drm_plane_state *state, struct drm_property *property, uint64_t *val)
{
	struct plane_t *gplane = dplane_to_gplane(plane);
	struct plane_state_t *gstate = dplane_state_to_gplane(state);
	int ret = 0;

	if (property == gplane->key_mode) {
		*val = gstate->key_mode;
	} else if (property == gplane->kp_ks) {
		*val = gstate->kp_ks;
	} else if (property == gplane->alpha_sel) {
		*val = gstate->alpha_sel;
	} else if (property == gplane->key_sel) {
		*val = gstate->key_sel;
	} else if (property == gplane->invert_alpha) {
		*val = gstate->invert_alpha;
	} else if (property == gplane->cursor_type) {
		*val = gstate->cursor_type;
	} else if (property == gplane->key_value) {
		*val = gstate->key_value;
	} else if (property == gplane->hdr_dg_lut) {
		*val = (gstate->hdr_dg_lut_blob) ? gstate->hdr_dg_lut_blob->base.id : 0;
	} else if (property == gplane->sgnt_param) {
		*val = (gstate->sgnt_param_blob) ? gstate->sgnt_param_blob->base.id : 0;
	} else {
		gf_printk(GF_DRV_ERROR, "unknown plane property");
		ret = -EINVAL;
	}

	return ret;
}

static void plane_atomic_destroy_state(struct drm_plane *plane, struct drm_plane_state *state)
{
	struct plane_state_t *gstate = dplane_state_to_gplane(state);

	if (gstate)
	{
		__drm_atomic_helper_plane_destroy_state(state);

		if (gstate->hdr_dg_lut_blob)
			drm_property_blob_put(gstate->hdr_dg_lut_blob);

		if (gstate->sgnt_param_blob)
			drm_property_blob_put(gstate->sgnt_param_blob);

		gf_free(gstate);
	}
}

static struct drm_plane_state* plane_atomic_duplicate_state(struct drm_plane *plane)
{
	struct plane_t *gplane = dplane_to_gplane(plane);
	struct plane_state_t *state, *old_state;

	old_state = dplane_state_to_gplane(plane->state);
	if (old_state)
		state = kmemdup(old_state, sizeof(struct plane_state_t), GFP_KERNEL);
	else
		state = kmemdup(plane->state, sizeof(struct plane_state_t), GFP_KERNEL);

	if (!state)
		return NULL;

	__drm_atomic_helper_plane_duplicate_state(plane, &state->base);

	if (state->hdr_dg_lut_blob)
		drm_property_blob_get(state->hdr_dg_lut_blob);

	if (state->sgnt_param_blob)
		drm_property_blob_get(state->sgnt_param_blob);

	return &state->base;
}

static DISP_FB_FMT dformat_to_gformat(struct drm_format_info dformat)
{
	if (dformat.format == DRM_FORMAT_RGB565)
		return DISP_FB_FMT_R5G6B5;
	else if (dformat.format == DRM_FORMAT_ARGB8888)
		return DISP_FB_FMT_A8R8G8B8;
	else if (dformat.format == DRM_FORMAT_ABGR8888)
		return DISP_FB_FMT_A8B8G8R8;
	else if (dformat.format == DRM_FORMAT_XRGB8888)
		return DISP_FB_FMT_X8R8G8B8;
	else if (dformat.format == DRM_FORMAT_XBGR8888)
		return DISP_FB_FMT_X8B8G8R8;
	else if (dformat.format == DRM_FORMAT_XRGB2101010)
		return DISP_FB_FMT_X2R10G10B10;
	else if (dformat.format == DRM_FORMAT_XBGR2101010)
		return DISP_FB_FMT_X2B10G10R10;
	else if (dformat.format == DRM_FORMAT_ARGB2101010)
		return DISP_FB_FMT_A2R10G10B10;
	else if (dformat.format == DRM_FORMAT_ABGR2101010)
		return DISP_FB_FMT_A2B10G10R10;
	else if (dformat.format == DRM_FORMAT_YUYV)
		return DISP_FB_FMT_CRYCB422_16BIT;
	else if (dformat.format == DRM_FORMAT_UYVY)
		return DISP_FB_FMT_YCBCR422_16BIT;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
	else if (dformat.format == DRM_FORMAT_XVYU2101010 || dformat.format == DRM_FORMAT_Y410)
		return DISP_FB_FMT_CRYCB2101010_32BIT;
	else if (dformat.format == DRM_FORMAT_Y210)
		return DISP_FB_FMT_CRYCB422_20BIT;
	else if (dformat.format == DRM_FORMAT_Y212)
		return DISP_FB_FMT_CRYCB422_24BIT;
	else if (dformat.format == DRM_FORMAT_Y216)
		return DISP_FB_FMT_CRYCB422_32BIT;
	else if (dformat.format  == DRM_FORMAT_AYUV || dformat.format  == DRM_FORMAT_XYUV8888)
		return DISP_FB_FMT_YCBCR8888_32BIT;
	else if (dformat.format == DRM_FORMAT_ARGB16161616F)
		return DISP_FB_FMT_A16R16G16B16_FLOAT;
	else if (dformat.format == DRM_FORMAT_ABGR16161616F)
		return DISP_FB_FMT_A16B16G16R16_FLOAT;
#endif
	else {
		gf_printk(GF_DRV_ERROR, "unknown drm format, check again\n");
		return -1;
	}
}

//only for color key mode, 8bpc
static unsigned int transfer_key(unsigned int key, DISP_FB_FMT fmt, DISP_BLEND mode)
{
	unsigned short channel_1 = 0, channel_2 = 0, channel_3 = 0, channel_4 = 0;

	if (mode != DISP_BLEND_A_COLOR_KEY && mode != DISP_BLEND_B_COLOR_KEY) {
		return key;
	}

	if (fmt == DISP_FB_FMT_A2R10G10B10 ||
			fmt == DISP_FB_FMT_A2B10G10R10 ||
			fmt == DISP_FB_FMT_X2R10G10B10 ||
			fmt == DISP_FB_FMT_X2B10G10R10 ||
			fmt == DISP_FB_FMT_CRYCB2101010_32BIT ||
			fmt == DISP_FB_FMT_YCBCR2101010_32BIT) {
		return key;
	}

	if (fmt == DISP_FB_FMT_R5G6B5) {
		channel_1 = key & 0x1F;
		channel_1 = ((channel_1) | (channel_1 << 5)) & 0x3FF;
		channel_2 = (key >> 5) & 0x3F;
		channel_2 = (((channel_2 & 0x3C) >> 2) | (channel_2 << 4)) & 0x3FF;
		channel_3 = (key >> 11) & 0x1F;
		channel_3 = ((channel_3) | (channel_3 << 5)) & 0x3FF;

		return (channel_1 | (channel_2 << 10) | (channel_3 << 20));
	} else if (fmt == DISP_FB_FMT_A8R8G8B8 ||
			fmt == DISP_FB_FMT_A8B8G8R8 ||
			fmt == DISP_FB_FMT_X8R8G8B8 ||
			fmt == DISP_FB_FMT_X8B8G8R8 ||
			fmt == DISP_FB_FMT_CRYCB8888_32BIT ||
			fmt == DISP_FB_FMT_YCBCR8888_32BIT) {
		channel_1 = key & 0xFF;
		channel_1 = (((channel_1 & 0xC0) >> 6) | (channel_1 << 2)) & 0x3FF;
		channel_2 = (key >> 8) & 0xFF;
		channel_2 = (((channel_2 & 0xC0) >> 6) | (channel_2 << 2)) & 0x3FF;
		channel_3 = (key >> 16) & 0xFF;
		channel_3 = (((channel_3 & 0xC0) >> 6) | (channel_3 << 2)) & 0x3FF;

		return (channel_1 | (channel_2 << 10) | (channel_3 << 20));
	} else if (fmt == DISP_FB_FMT_CRYCB422_16BIT) {//should be [31:0] Y Cb Cr: 10:10:10
		channel_2 = (key >> 8) & 0xFF;
		channel_2 = (((channel_2 & 0xC0) >> 6) | (channel_2 << 2)) & 0x3FF;
		channel_3 = (key >> 16) & 0xFF;
		channel_3 = (((channel_3 & 0xC0) >> 6) | (channel_3 << 2)) & 0x3FF;
		channel_4 = (key >> 24) & 0xFF;
		channel_4 = (((channel_4 & 0xC0) >> 6) | (channel_4 << 2)) & 0x3FF;

		return (channel_4 | (channel_2 << 10) | (channel_3 << 20));
	} else if (fmt == DISP_FB_FMT_YCBCR422_16BIT) {//should be [31:0] Y Cb Cr: 10:10:10
		channel_1 = key & 0xFF;
		channel_1 = (((channel_1 & 0xC0) >> 6) | (channel_1 << 2)) & 0x3FF;
		channel_2 = (key >> 8) & 0xFF;
		channel_2 = (((channel_2 & 0xC0) >> 6) | (channel_2 << 2)) & 0x3FF;
		channel_3 = (key >> 16) & 0xFF;
		channel_3 = (((channel_3 & 0xC0) >> 6) | (channel_3 << 2)) & 0x3FF;

		return (channel_3 | (channel_1 << 10) | (channel_2 << 20));
	} else if (fmt == DISP_FB_FMT_CRYCB422_20BIT ||
			fmt == DISP_FB_FMT_YCBCR422_20BIT ||
			fmt == DISP_FB_FMT_CRYCB422_24BIT ||
			fmt == DISP_FB_FMT_YCBCR422_24BIT ||
			fmt == DISP_FB_FMT_CRYCB422_32BIT ||
			fmt == DISP_FB_FMT_YCBCR422_32BIT) {
		gf_error("check with hw the bit map, sw not support now\n");
		return key;
	} else {
		gf_error("color key not support this format\n");
		return key;
	}
}

static int get_compress_property(struct lisuan_gem_bo *gbo, disp_fb_t *pdisp_fb)
{
	int flip_mode = DISP_FLIP_ENABLE;
	pdisp_fb->flag.compress = 0x1;

	if (gbo->cf_node) {
		pdisp_fb->bl_index = gbo->cf_node->cell_index;
	} else {
		flip_mode = DISP_FLIP_DISABLE;
		gf_error("%s, cf_node null, get compress property fail\n", __func__);
	}

	pdisp_fb->range_type = gbo->compressFormat;

	return flip_mode;
}

static void plane_hdr_update(struct drm_plane *plane,
            pdisp_hdr_module_t pdisp_hdr,
            struct plane_state_t *g_pstate,
            struct crtc_state_t *g_cstate)
{
	struct plane_t *gplane = dplane_to_gplane(plane);
	property_crtc_hdr_info_s hdr_info;
	property_crtc_hdr_mode_s hdr_mode;
	HdrMgr_t *phdr_mgr = gplane->phdr_mgr;
	unsigned int *lut_y = NULL, *lut_s = NULL;
	bool is_overlay = false;

	is_overlay = (gplane->index == DISP_OVERLAY_PLANE) ? true : false;
	if (gplane->index == DISP_PRIMARY_PLANE || is_overlay) {
		hdr_info.u64_value = g_cstate->hdr_info_u64;
		hdr_mode.u64_value = g_cstate->hdr_mode_u64;
		if(g_pstate->hdr_dg_lut_blob && g_pstate->hdr_dg_lut_blob->data &&
			g_pstate->hdr_dg_lut_blob->length == 2 * HDR_DEGAMMA_LUT_LENGTH * sizeof(unsigned int)) {
			lut_y = g_pstate->hdr_dg_lut_blob->data;
			lut_s = (unsigned int*)g_pstate->hdr_dg_lut_blob->data + HDR_DEGAMMA_LUT_LENGTH;
		}

		phdr_mgr->hdrEnable = is_overlay ? hdr_info.mem.hdr_en_1 : hdr_info.mem.hdr_en_0;
		phdr_mgr->satType = HdrSaturationType_0_25;
		phdr_mgr->csIn = is_overlay ? hdr_info.mem.cs_in_1 : hdr_info.mem.cs_in_0;
		phdr_mgr->csOut = hdr_mode.mem.cs_out;
		phdr_mgr->tfIn = is_overlay ? hdr_info.mem.tf_in_1 : hdr_info.mem.tf_in_0;
		phdr_mgr->tfOut = hdr_mode.mem.tf_out;
		phdr_mgr->minLout_1_10000 = hdr_mode.mem.min_lum_1_10000;
		phdr_mgr->maxLout = hdr_mode.mem.max_lum;
		phdr_mgr->maxFall = hdr_mode.mem.max_fall;
		phdr_mgr->maxLin = is_overlay ? hdr_info.mem.max_lum_in_1 : hdr_info.mem.max_lum_in_0;
		if (lut_y) {
			gf_memcpy(phdr_mgr->degammaLutY, lut_y, sizeof(phdr_mgr->degammaLutY));
		}
		if (lut_s) {
			gf_memcpy(phdr_mgr->degammaLutS, lut_s, sizeof(phdr_mgr->degammaLutS));
		}
		HdrUpdate(phdr_mgr, pdisp_hdr);
		//HdrDump(phdr_mgr, pdisp_hdr);
	}
}

static int plane_disp_implement_standard_prepare(struct drm_plane *plane,
            struct drm_framebuffer *pdrm_fb,
            pdisp_plane_update_para_t pdisp_p,
            struct drm_plane_state *new_plane_state)
{
	struct plane_t *gplane = dplane_to_gplane(plane);
	struct plane_state_t *gstate = new_plane_state ?
		dplane_state_to_gplane(new_plane_state) : dplane_state_to_gplane(plane->state);
	struct crtc_t *gcrtc = new_plane_state ?
		dcrtc_to_gcrtc(new_plane_state->crtc) : dcrtc_to_gcrtc(plane->state->crtc);
	struct crtc_state_t *g_cstate = NULL;
	struct drm_framebuffer *drm_fb = NULL;
	struct lisuan_gem_bo *gbo;
	struct lisuan_vm_node *vm_node;
	property_sgnt_param_s* psgnt_param = NULL;
	int idx = 0;

	if (!pdisp_p) {
		gf_error("%s, pdisp_p NULL!\n", __func__);
		return -1;
	}
	if (new_plane_state && !gcrtc) {
		/* drm will disable plane twice
		 * The second disable check, atomic state will have no crtc, use cur crtc instead, to get crtc_index */
		gcrtc = dcrtc_to_gcrtc(plane->state->crtc);
	}
	if (!gcrtc) {
		/*
			sometime wayland may give a "crtc 0" & "fb 0" property to plane	(see drm_pending_state_apply_atomic)
			for disable some planes, we should skip prepare and check for it
		*/
		//gf_error("%s, gcrtc NULL!\n", __func__);
		return -1;
	}

	if (DISP_CURSOR_PLANE == gplane->index && (plane->state->crtc_x < 0 || plane->state->crtc_y < 0))
	{
		/* Skip negative cursor position as HW not support. */
		return -1;
	}

	pdisp_p->crtc_index = gcrtc->index;
	pdisp_p->plane_index = gplane->index;
	pdisp_p->src.x = plane->state->src_x >> 16; pdisp_p->src.y = plane->state->src_y >> 16;
	pdisp_p->src.w = plane->state->src_w >> 16; pdisp_p->src.h = plane->state->src_h >> 16;
	pdisp_p->dst.x = plane->state->crtc_x; pdisp_p->dst.y = plane->state->crtc_y;
	pdisp_p->dst.w = plane->state->crtc_w; pdisp_p->dst.h = plane->state->crtc_h;

	drm_fb = pdrm_fb ? pdrm_fb : plane->state->fb;
	if (drm_fb) {
		pdisp_p->flip_mode = DISP_FLIP_ENABLE;

		gbo = gem_to_gf_bo(drm_fb->obj[0]);
		if (gbo && gbo->vm_node) {
			pdisp_p->fb.address = gbo->vm_node->offset;
			//gf_info("%s, gem:%x, from tgid:%d, address = 0x%x\n", __func__,
			//	gbo->handle, gbo->ls_file?gbo->ls_file->tgid:0, pdisp_p->fb.address);
		} else {
			gf_info("%s gbo[%llx] or vm_node[%llx] NULL!\n", __func__, gbo, gbo ? gbo->vm_node : 0);
		}

		pdisp_p->fb.format = dformat_to_gformat(*drm_fb->format);

		if (pdisp_p->fb.format == DISP_FB_FMT_R5G6B5 ||
			pdisp_p->fb.format == DISP_FB_FMT_CRYCB422_16BIT ||
			pdisp_p->fb.format == DISP_FB_FMT_YCBCR422_16BIT) {
			pdisp_p->fb.bit_cnt = 16;
		} else if (pdisp_p->fb.format == DISP_FB_FMT_A16R16G16B16_FLOAT
				|| pdisp_p->fb.format == DISP_FB_FMT_A16B16G16R16_FLOAT) {
			pdisp_p->fb.bit_cnt = 64;
		} else {
			pdisp_p->fb.bit_cnt = 32;
		}

		pdisp_p->fb.pitch = drm_fb->pitches[0];
		pdisp_p->fb.width = drm_fb->width;
		pdisp_p->fb.height = drm_fb->height;
		pdisp_p->fb.align_width = drm_fb->width;

		if ((gbo->compressFormat != 0) && (pdisp_p->flip_mode == DISP_FLIP_ENABLE)) {
			pdisp_p->flip_mode = get_compress_property(gbo, &pdisp_p->fb);
		}
	} else {
		pdisp_p->flip_mode = DISP_FLIP_DISABLE;
	}

/*in factually, primary use window key, overlay use B overlay A alpha blending*/
	if (gplane->index == DISP_OVERLAY_PLANE) {
		pdisp_p->blend.blend_mode = gstate->key_mode;
		pdisp_p->blend.factor.kp_kmix = (gstate->kp_ks >> 4) & 0xF;
		pdisp_p->blend.factor.ks_kt = gstate->kp_ks & 0xF;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
		if (plane->state->pixel_blend_mode == DRM_MODE_BLEND_PREMULTI) {
			pdisp_p->blend.pre_multi = 1;
		} else if (plane->state->pixel_blend_mode == DRM_MODE_BLEND_COVERAGE) {
			pdisp_p->blend.pre_multi = 0;
		}
#endif

		pdisp_p->blend.alpha_sel = gstate->alpha_sel;
		pdisp_p->blend.key_sel = gstate->key_sel;
		pdisp_p->blend.invert_a = gstate->invert_alpha;
		//the plane->state->alpha means plane alpha, 0-0xFFFF, our plane alpha is 0-0xFF
		//if used, we should map 0-0xFFFFF to 0-0xFF
		pdisp_p->blend.plane_alpha = plane->state->alpha >> 8;
		pdisp_p->blend.key_value = transfer_key(gstate->key_value, pdisp_p->fb.format, pdisp_p->blend.blend_mode);
	} else if (gplane->index == DISP_PRIMARY_PLANE){
		pdisp_p->blend.blend_mode = DISP_BLEND_B_WINDOW_KEY;
		pdisp_p->blend.factor.kp_kmix = 0;
		pdisp_p->blend.factor.ks_kt = 8;
		pdisp_p->blend.key_sel = DISP_KEY_SEL_B_KEY;//default output B key
		pdisp_p->blend.alpha_sel = DISP_ALP_SEL_B_ALPHA;//default output B alpha
		pdisp_p->blend.key_value = transfer_key(gstate->key_value, pdisp_p->fb.format, pdisp_p->blend.blend_mode);
	} else {
		pdisp_p->cs_type = gstate->cursor_type;
		pdisp_p->mono_bg = 0;
		pdisp_p->mono_fg = 0xffffff;
	}

	g_cstate = dcrtc_state_to_gcrtc(gcrtc->base.state);
	plane_hdr_update(plane, &pdisp_p->hdr_module, gstate, g_cstate);

	// sgnt golden
	if (gstate->sgnt_param_blob && gstate->sgnt_param_blob->data &&
			gstate->sgnt_param_blob->length == sizeof(property_sgnt_param_s)) {
		psgnt_param = (property_sgnt_param_s*)gstate->sgnt_param_blob->data;
		pdisp_p->sgnt_golden[0] = psgnt_param->golden[0];
		pdisp_p->sgnt_golden[1] = psgnt_param->golden[1];
		pdisp_p->sgnt_golden[2] = psgnt_param->golden[2];
		pdisp_p->sgnt_golden[3] = psgnt_param->golden[3];
	}

	return 0;
}

static int plane_disp_implement(struct drm_plane *plane,
            pdisp_plane_update_para_t pdisp_param,
            unsigned char is_update)
{
	struct plane_t *gplane = dplane_to_gplane(plane);
	return dm_plane_implement(gplane->adapter, pdisp_param, is_update);
}

static int disable_plane(struct drm_plane *plane,
			     struct drm_modeset_acquire_ctx *ctx)
{
	struct plane_t *gplane = dplane_to_gplane(plane);
	disp_plane_update_para_t disp_param;
	int ret = 0;

	gf_memset(&disp_param, 0, sizeof(disp_plane_update_para_t));
	disp_param.crtc_index = gf_get_last_bit_index(plane->possible_crtcs);
	disp_param.plane_index = gplane->index;
	disp_param.flip_mode = DISP_FLIP_DISABLE;

	ret = plane_disp_implement(plane, &disp_param, false);
	if (0 != ret) {
		gf_error("%s, check, failed\n", __func__);
		return ret;
	}
	return plane_disp_implement(plane, &disp_param, true);
}

static struct drm_plane_funcs plane_func = {
	.reset = plane_reset,
	.destroy = drm_plane_cleanup,
	.update_plane = drm_atomic_helper_update_plane,
	.disable_plane = disable_plane,
	.atomic_set_property = plane_atomic_set_property,
	.atomic_get_property = plane_atomic_get_property,
	.atomic_destroy_state = plane_atomic_destroy_state,
	.atomic_duplicate_state = plane_atomic_duplicate_state,
};

static int plane_prepare_fb(struct drm_plane *plane, struct drm_plane_state *new_state)
{
	int ret = -1;
	struct plane_t *gplane = dplane_to_gplane(plane);
	//wait fence if buffer from gpu

	if (!plane || !new_state)
		return 0;

#if DRM_VERSION_CODE >= KERNEL_VERSION(5,15,0)
	ret = drm_gem_plane_helper_prepare_fb(plane, new_state);
#else
	ret =  drm_gem_fb_prepare_fb(plane, new_state);
#endif

	return ret;
}

#if DRM_VERSION_CODE >= KERNEL_VERSION(5,15,0)
static int plane_atomic_check(struct drm_plane *plane, struct drm_atomic_state *state)
#else
static int plane_atomic_check(struct drm_plane *plane, struct drm_plane_state *state)
#endif
{
	disp_plane_update_para_t disp_param;
	int ret = 0;
	struct drm_plane_state *new_plane_state =
#if DRM_VERSION_CODE >= KERNEL_VERSION(5,15,0)
		drm_atomic_get_new_plane_state(state, plane);
#else
		state;
#endif

	gf_memset(&disp_param, 0, sizeof(disp_plane_update_para_t));
	ret = plane_disp_implement_standard_prepare(plane, NULL, &disp_param, new_plane_state);
	if (ret < 0) {
		//return -EINVAL;
	}
	return plane_disp_implement(plane, &disp_param, false);
}

#if DRM_VERSION_CODE >= KERNEL_VERSION(5,15,0)
void plane_atomic_update(struct drm_plane *plane, struct drm_atomic_state *state)
#else
void plane_atomic_update(struct drm_plane *plane, struct drm_plane_state *state)
#endif
{
	disp_plane_update_para_t disp_param;
	int ret = 0;
	struct drm_plane_state *new_plane_state =
#if DRM_VERSION_CODE >= KERNEL_VERSION(5,15,0)
			drm_atomic_get_new_plane_state(state, plane);
#else
			state;
#endif

	gf_memset(&disp_param, 0, sizeof(disp_plane_update_para_t));
	ret = plane_disp_implement_standard_prepare(plane, NULL, &disp_param, new_plane_state);
	if (ret < 0) {
		// do nothing.
	}
	else {
		plane_disp_implement(plane, &disp_param, true);
	}
}

#if DRM_VERSION_CODE >= KERNEL_VERSION(5,15,0)
void plane_atomic_disable(struct drm_plane *plane,
			       struct drm_atomic_state *old_state)
#else
void plane_atomic_disable(struct drm_plane *plane,
                               struct drm_plane_state *old_state)
#endif
{
	struct plane_t *gplane = dplane_to_gplane(plane);
	disp_plane_update_para_t disp_param;

	gf_memset(&disp_param, 0, sizeof(disp_plane_update_para_t));
	disp_param.crtc_index = gplane->crtc_index;
	disp_param.plane_index = gplane->index;
	disp_param.flip_mode = DISP_FLIP_DISABLE;
	plane_disp_implement(plane, &disp_param, true);
}

static struct drm_plane_helper_funcs plane_helper = {
	.prepare_fb = plane_prepare_fb,
	.atomic_check = plane_atomic_check,
	.atomic_update = plane_atomic_update,
	.atomic_disable = plane_atomic_disable,
};

static unsigned int plane_format[] = {
//16 bpp rgb
	DRM_FORMAT_RGB565,
//32 bpp rgb
	DRM_FORMAT_ARGB8888,
	DRM_FORMAT_ABGR8888,
	DRM_FORMAT_XRGB8888,
	DRM_FORMAT_XBGR8888,
	DRM_FORMAT_XRGB2101010,
	DRM_FORMAT_XBGR2101010,
	DRM_FORMAT_ARGB2101010,
	DRM_FORMAT_ABGR2101010,
//
	DRM_FORMAT_YUYV,
	DRM_FORMAT_UYVY,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
	DRM_FORMAT_Y210,
	DRM_FORMAT_Y212,
	DRM_FORMAT_Y216,
	DRM_FORMAT_AYUV,
	DRM_FORMAT_XYUV8888,
	DRM_FORMAT_Y410,
	DRM_FORMAT_XVYU2101010,
	DRM_FORMAT_ARGB16161616F,
	DRM_FORMAT_ABGR16161616F,
#endif
};

static unsigned int cursor_format[] = {
	DRM_FORMAT_ARGB8888,
};

static u32 crtc_get_vblank_counter(struct drm_crtc *crtc)
{
	struct crtc_t *gcrtc = dcrtc_to_gcrtc(crtc);
	struct display_adapter *adapter = gcrtc->adapter;

	return dm_crtc_get_vblank_counter(adapter, gcrtc->index);
}

static int crtc_enable_vblank(struct drm_crtc *crtc)
{
	struct crtc_t *gcrtc = dcrtc_to_gcrtc(crtc);
	struct display_adapter *adapter = gcrtc->adapter;

	return dm_crtc_enable_vblank(adapter, gcrtc->index, 1);
}

static void crtc_disable_vblank(struct drm_crtc *crtc)
{
	struct crtc_t *gcrtc = dcrtc_to_gcrtc(crtc);
	struct display_adapter *adapter = gcrtc->adapter;

	dm_crtc_enable_vblank(adapter, gcrtc->index, 0);
}

int gf_kms_cursor_set(struct drm_crtc *crtc, struct drm_file *file_priv,
			  uint32_t handle, uint32_t width, uint32_t height)
{
	struct crtc_t *gcrtc = dcrtc_to_gcrtc(crtc);
	struct drm_plane *dplane;
	struct plane_t *gplane = NULL;
	struct drm_gem_object *obj;
	disp_plane_update_para_t disp_param;
	int ret = 0;

	obj = drm_gem_object_lookup(file_priv, handle);
	if(!obj)
	{
		return -1;
	}

	if(width > 256 || height > 256)
	{
		gf_error("%s, cursor too large\n", __func__);
		return -1;
	}

	gcrtc->legacy_cursor.gem_obj = obj;
	gcrtc->legacy_cursor.width = width;
	gcrtc->legacy_cursor.height = height;

	if (!handle) {			//disable cursor
		gplane = &gcrtc->planes[1];		//get cursor plane, cursor plane cannot get from crtc->cursor since it may be NULL in legacy mode
		if(!gplane)
		{
			gf_error("%s, get gplane NULL\n", __func__);
			return -1;
		}

		dplane = &gplane->base;
		gf_memset(&disp_param, 0, sizeof(disp_plane_update_para_t));
		disp_param.flip_mode = DISP_FLIP_DISABLE;
		ret = plane_disp_implement(dplane, &disp_param, true);
		return ret;
	}

	return 0;
}

int gf_kms_cursor_set2(struct drm_crtc *crtc, struct drm_file *file_priv,
			   uint32_t handle, uint32_t width, uint32_t height,
			   int32_t hot_x, int32_t hot_y)
{
	//hot_x, hot_y used by virtualized display hardware, ignore
	return gf_kms_cursor_set(crtc, file_priv, handle, width, height);
}

int gf_kms_cursor_move(struct drm_crtc *crtc, int x, int y)
{

	struct crtc_t *gcrtc = dcrtc_to_gcrtc(crtc);
	struct drm_plane *dplane;
	struct plane_t *gplane;
	disp_plane_update_para_t disp_param;
	struct lisuan_gem_bo *gbo;
	struct drm_gem_object *obj;
	int ret = 0;

	if(x < 0)
		x = 0;
	if(y < 0)
		y = 0;

	gplane = &gcrtc->planes[1];		//get cursor plane, cursor plane cannot get from crtc->cursor since it may be NULL
	if(!gplane)
	{
		gf_error("%s, gplane NULL\n", __func__);
		return -1;
	}
	dplane = &gplane->base;

	obj = gcrtc->legacy_cursor.gem_obj;
	if(!obj)
	{
		return -1;
	}
	gbo = gem_to_gf_bo(obj);

	gf_memset(&disp_param, 0, sizeof(disp_plane_update_para_t));
	disp_param.fb.address = gbo->vm_node->offset;
	disp_param.flip_mode = DISP_FLIP_ENABLE;
	disp_param.fb.format = DISP_FB_FMT_A8R8G8B8;
	disp_param.fb.bit_cnt = 32;
	disp_param.fb.pitch = gcrtc->legacy_cursor.width * 4;
	disp_param.fb.width = gcrtc->legacy_cursor.width;
	disp_param.fb.height = gcrtc->legacy_cursor.height;

	disp_param.src.x = 0;
	disp_param.src.y = 0;
	disp_param.src.w = gcrtc->legacy_cursor.width;
	disp_param.src.h = gcrtc->legacy_cursor.height;
	disp_param.dst.w = gcrtc->legacy_cursor.width;
	disp_param.dst.h = gcrtc->legacy_cursor.height;

	disp_param.dst.x = x;
	disp_param.dst.y = y;
	disp_param.crtc_index = gcrtc->index;
	disp_param.plane_index = DISP_CURSOR_PLANE;
	disp_param.cs_type = DISP_CS_TYPE_COLOR_ALPHA;
	disp_param.flip_mode = DISP_FLIP_ENABLE;

	ret = plane_disp_implement(dplane, &disp_param, true);

	return ret;
}

static struct drm_crtc_funcs crtc_func = {
	.reset = crtc_reset,
	.destroy = drm_crtc_cleanup,
	.atomic_set_property = crtc_atomic_set_property,
	.atomic_get_property = crtc_atomic_get_property,
#ifdef __VIRTAUL_VBLANK__
	.enable_vblank = gf_kms_virt_vblank_enable,
	.disable_vblank = gf_kms_virt_vblank_disable,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 7, 0)
	.get_vblank_timestamp = gf_kms_virt_get_vblank_timestamp,
#endif
#else
	.get_vblank_counter = crtc_get_vblank_counter,
	.enable_vblank = crtc_enable_vblank,
	.disable_vblank = crtc_disable_vblank,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 7, 0)
	.get_vblank_timestamp = drm_crtc_vblank_helper_get_vblank_timestamp,
#endif
#endif
	.atomic_duplicate_state = crtc_atomic_duplicate_state,
	.atomic_destroy_state = crtc_atomic_destroy_state,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
	.get_crc_sources	= gf_kms_get_crc_sources,
	.set_crc_source		= gf_kms_set_crc_source,
	.verify_crc_source	= gf_kms_verify_crc_source,
#endif

//legacy interface
	.set_config = drm_atomic_helper_set_config,
	.page_flip = drm_atomic_helper_page_flip,
	.cursor_set = gf_kms_cursor_set,
	.cursor_set2 = gf_kms_cursor_set2,
	.cursor_move = gf_kms_cursor_move,
};

static void type_gf_to_drm(pdisp_display_mode_t gf_mode, struct drm_display_mode *mode)
{
	int vaild_bit = 0;

	//must init type
	mode->type = 0;

	vaild_bit = gf_mode->type & DISP_MODE_TYPE_MASK;

	if (vaild_bit == 1) {
		mode->type |= DRM_MODE_TYPE_BUILTIN;
	}

	if (vaild_bit == 3) {
		mode->type |= DRM_MODE_TYPE_CLOCK_C;
	}

	if (vaild_bit == 5) {
		mode->type |= DRM_MODE_TYPE_CRTC_C;
	}

	if (gf_mode->type & DISP_MODE_TYPE_PREFERRED) {
		mode->type |= DRM_MODE_TYPE_PREFERRED;
	}

	if (gf_mode->type & DISP_MODE_TYPE_DEFAULT) {
		mode->type |= DRM_MODE_TYPE_DEFAULT;
	}

	if (gf_mode->type & DISP_MODE_TYPE_USERDEF) {
		mode->type |= DRM_MODE_TYPE_USERDEF;
	}

	if (gf_mode->type & DISP_MODE_TYPE_DRIVER) {
		mode->type |= DRM_MODE_TYPE_DRIVER;
	}
}

static void flags_gf_to_drm(pdisp_display_mode_t gf_mode, struct drm_display_mode *mode)
{
	int vaild_bit = 0;

	//must init flags
	mode->flags = 0;

	if (gf_mode->flags & DISP_MODE_FLAG_PHSYNC) {
		mode->flags |= DRM_MODE_FLAG_PHSYNC;
	}

	if (gf_mode->flags & DISP_MODE_FLAG_NHSYNC) {
		mode->flags |= DRM_MODE_FLAG_NHSYNC;
	}

	if (gf_mode->flags & DISP_MODE_FLAG_PVSYNC) {
		mode->flags |= DRM_MODE_FLAG_PVSYNC;
	}

	if (gf_mode->flags & DISP_MODE_FLAG_NVSYNC) {
		mode->flags |= DRM_MODE_FLAG_NVSYNC;
	}

	if (gf_mode->flags & DISP_MODE_FLAG_INTERLACE) {
		mode->flags |= DRM_MODE_FLAG_INTERLACE;
	}

	if (gf_mode->flags & DISP_MODE_FLAG_DBLSCAN) {
		mode->flags |= DRM_MODE_FLAG_DBLSCAN;
	}

	if (gf_mode->flags & DISP_MODE_FLAG_CSYNC) {
		mode->flags |= DRM_MODE_FLAG_CSYNC;
	}

	if (gf_mode->flags & DISP_MODE_FLAG_PCSYNC) {
		mode->flags |= DRM_MODE_FLAG_PCSYNC;
	}

	if (gf_mode->flags & DISP_MODE_FLAG_NCSYNC) {
		mode->flags |= DRM_MODE_FLAG_NCSYNC;
	}

	if (gf_mode->flags & DISP_MODE_FLAG_HSKEW) {
		mode->flags |= DRM_MODE_FLAG_HSKEW;
	}

	if (gf_mode->flags & DISP_MODE_FLAG_BCAST) {
		mode->flags |= DRM_MODE_FLAG_BCAST;
	}

	if (gf_mode->flags & DISP_MODE_FLAG_PIXMUX) {
		mode->flags |= DRM_MODE_FLAG_PIXMUX;
	}

	if (gf_mode->flags & DISP_MODE_FLAG_DBLCLK) {
		mode->flags |= DRM_MODE_FLAG_DBLCLK;
	}

	if (gf_mode->flags & DISP_MODE_FLAG_CLKDIV2) {
		mode->flags |= DRM_MODE_FLAG_CLKDIV2;
	}

	if (gf_mode->flags & DISP_MODE_FLAG_CLKDIV2) {
		mode->flags |= DRM_MODE_FLAG_CLKDIV2;
	}

	vaild_bit = (gf_mode->flags & DISP_MODE_FLAG_3D_MASK) >> 14;

	if (vaild_bit == 0) {
		mode->flags |= DRM_MODE_FLAG_3D_NONE;
	}

	if (vaild_bit == 1) {
		mode->flags |= DRM_MODE_FLAG_3D_FRAME_PACKING;
	}

	if (vaild_bit == 2) {
		mode->flags |= DRM_MODE_FLAG_3D_FIELD_ALTERNATIVE;
	}

	if (vaild_bit == 3) {
		mode->flags |= DRM_MODE_FLAG_3D_LINE_ALTERNATIVE;
	}

	if (vaild_bit == 4) {
		mode->flags |= DRM_MODE_FLAG_3D_SIDE_BY_SIDE_FULL;
	}

	if (vaild_bit == 5) {
		mode->flags |= DRM_MODE_FLAG_3D_L_DEPTH;
	}

	if (vaild_bit == 6) {
		mode->flags |= DRM_MODE_FLAG_3D_L_DEPTH_GFX_GFX_DEPTH;
	}

	if (vaild_bit == 7) {
		mode->flags |= DRM_MODE_FLAG_3D_TOP_AND_BOTTOM;
	}

	if (vaild_bit == 8) {
		mode->flags |= DRM_MODE_FLAG_3D_SIDE_BY_SIDE_HALF;
	}

	vaild_bit = (gf_mode->flags & DISP_MODE_FLAG_PIC_AR_MASK) >> 19;

	if (vaild_bit == 0) {
		mode->flags |= DRM_MODE_FLAG_PIC_AR_NONE;
	}

	if (vaild_bit == 1) {
		mode->flags |= DRM_MODE_FLAG_PIC_AR_4_3;
	}

	if (vaild_bit == 2) {
		mode->flags |= DRM_MODE_FLAG_PIC_AR_16_9;
	}

	if (vaild_bit == 3) {
		mode->flags |= DRM_MODE_FLAG_PIC_AR_64_27;
	}

	if (vaild_bit == 4) {
		mode->flags |= DRM_MODE_FLAG_PIC_AR_256_135;
	}
}

static void mode_gf_to_drm(pdisp_display_mode_t gf_mode, struct drm_display_mode *mode)
{
	mode->clock = gf_mode->clock;
	mode->hdisplay = gf_mode->hdisplay;
	mode->hsync_start = gf_mode->hsync_start;
	mode->hsync_end = gf_mode->hsync_end;
	mode->htotal = gf_mode->htotal;
	mode->hskew = gf_mode->hskew;
	mode->vdisplay = gf_mode->vdisplay;
	mode->vsync_start = gf_mode->vsync_start;
	mode->vsync_end = gf_mode->vsync_end;
	mode->vtotal = gf_mode->vtotal;
	mode->vscan = gf_mode->vscan;

	type_gf_to_drm(gf_mode, mode);
	flags_gf_to_drm(gf_mode, mode);
	mode->picture_aspect_ratio = gf_mode->picture_aspect_ratio;
}

static bool mode_drm_to_gf(pdisp_display_mode_t gf_mode, struct drm_display_mode *mode, struct connector_t *connector)
{
	int i = 0;
	int mode_num = connector->mode_num;

	for (i = 0; i < mode_num; i++) {
		if (mode->hdisplay != connector->modes[i].hdisplay) {
			continue;
		}

		if (mode->vdisplay != connector->modes[i].vdisplay) {
			continue;
		}

		if (mode->clock != (connector->modes[i].clock)) {
			continue;
		}

		if (mode->htotal != (connector->modes[i].htotal)) {
			continue;
		}

		if (mode->vtotal != (connector->modes[i].vtotal)) {
			continue;
		}

		gf_memcpy(gf_mode, &(connector->modes[i]), sizeof(disp_display_mode_t));
		return true;
	}

	return false;
}

int crtc_mode_set(struct drm_crtc *crtc, struct drm_display_mode *mode,
			struct drm_display_mode *adjusted_mode, int x, int y,
			struct drm_framebuffer *old_fb)
{
	int connector_index = -1;
	struct encoder_t *gencoder;
	struct connector_t *gconnector;
	struct drm_encoder *dencoder;
	struct crtc_t *gcrtc = dcrtc_to_gcrtc(crtc);
	struct display_adapter *adapter = gcrtc->adapter;
	disp_display_mode_t gmode = {0};
	disp_display_mode_t ghw_mode = {0};
	bool find_mode, find_hw_mode;

	drm_for_each_encoder_mask(dencoder, crtc->dev, crtc->state->encoder_mask) {
		gencoder = dencoder_to_gencoder(dencoder);
		connector_index = gencoder->connector_index;
		break;
	}

	if ((connector_index) < 0 || (connector_index > 3)) {
		gf_printk(GF_DRV_ERROR, "crtc_mode_set set a error index\n");
		return -1;
	}

	gconnector = &(adapter->connectors[connector_index]);
	find_mode = mode_drm_to_gf(&gmode, mode, gconnector);
	find_hw_mode = mode_drm_to_gf(&ghw_mode, adjusted_mode, gconnector);
	if (!find_mode || !find_hw_mode)
	{
		gf_printk(GF_DRV_ERROR, "crtc_mode_set, wh[%u %u] clk[%d], wh[%u %u] clk[%d]\n",
			mode->hdisplay, mode->vdisplay, mode->clock,
			adjusted_mode->hdisplay, adjusted_mode->vdisplay, adjusted_mode->clock);
		return -1;
	}

	return dm_crtc_set_mode(adapter, gcrtc, &gmode, &ghw_mode);
}

void crtc_mode_set_nofb(struct drm_crtc *crtc)
{
	int connector_index = -1;
	struct encoder_t *gencoder;
	struct connector_t *gconnector;
	struct drm_encoder *dencoder;
	struct crtc_t *gcrtc = dcrtc_to_gcrtc(crtc);
	struct display_adapter *adapter = gcrtc->adapter;
	disp_display_mode_t gmode = {0};
	disp_display_mode_t ghw_mode = {0};
	bool find_mode, find_hw_mode;

	drm_for_each_encoder_mask(dencoder, crtc->dev, crtc->state->encoder_mask) {
		gencoder = dencoder_to_gencoder(dencoder);
		connector_index = gencoder->connector_index;
		break;
	}

	if ((connector_index) < 0 || (connector_index > 3)) {
		gf_printk(GF_DRV_ERROR, "crtc_mode_set set a error index\n");
	} else {
		gconnector = &(adapter->connectors[connector_index]);
		find_mode = mode_drm_to_gf(&gmode, &crtc->state->mode, gconnector);
		find_hw_mode = mode_drm_to_gf(&ghw_mode, &crtc->state->adjusted_mode, gconnector);

		if (!find_mode || !find_hw_mode)
		{
			gf_printk(GF_DRV_ERROR, "crtc_mode_set_nofb, wh[%u %u] clk[%d], wh[%u %u] clk[%d]\n",
				crtc->state->mode.hdisplay, crtc->state->mode.vdisplay, crtc->state->mode.clock,
				crtc->state->adjusted_mode.hdisplay, crtc->state->adjusted_mode.vdisplay, crtc->state->adjusted_mode.clock);
			return;
		}

		dm_crtc_set_mode(adapter, gcrtc, &gmode, &ghw_mode);
	}
}

#if DRM_VERSION_CODE >= KERNEL_VERSION(5,15,0)
void crtc_atomic_enable(struct drm_crtc *crtc, struct drm_atomic_state *state)
#else
void crtc_atomic_enable(struct drm_crtc *crtc, struct drm_crtc_state *state)
#endif
{
	struct crtc_t *gcrtc = dcrtc_to_gcrtc(crtc);
	struct display_adapter *adapter = gcrtc->adapter;

	dm_crtc_dpms(adapter, gcrtc, true);
	drm_crtc_vblank_on(crtc);
}

static uint32_t CONVERT_S2_12(int64_t x)
{
	uint32_t value = 0;
	uint32_t flag = 0;

	if (x < 0) {
		flag = 1;
		x = 0 - x;
	}

	value = x >> (54 - 9);
	if (flag == 1) {
		value = (uint32_t)(1 << 14) - value;
	}

	return value;
}

#define MAX_COLOR_LEGACY_LUT_ENTRIES 256

#if DRM_VERSION_CODE >= KERNEL_VERSION(5,15,0)
static void crtc_atomic_flush(struct drm_crtc *crtc, struct drm_atomic_state *state)
#else
static void crtc_atomic_flush(struct drm_crtc *crtc, struct drm_crtc_state *state)
#endif
{
	struct crtc_t *gcrtc = dcrtc_to_gcrtc(crtc);
	struct drm_device *dev = crtc->dev;
	struct drm_pending_vblank_event *event = crtc->state->event;
	struct drm_color_ctm *ctm;
	disp_csc_para_t csc_para = {0};
	struct drm_color_lut *dlut;
	int dlut_size = 0;
	disp_gamma_info_t gamma_info = {0};
	int i = 0;
	unsigned long flags;

	if (event) {
		crtc->state->event = NULL;

		spin_lock_irqsave(&crtc->dev->event_lock, flags);
		if (drm_crtc_vblank_get(crtc) == 0)
			drm_crtc_arm_vblank_event(crtc, event);
		else
			drm_crtc_send_vblank_event(crtc, event);
		spin_unlock_irqrestore(&crtc->dev->event_lock, flags);
	}

	if (crtc->state->color_mgmt_changed) {
		
		if (crtc->state->ctm) {//set csc color enhance
			gf_error("should check: why enable programming csc but no valid csc matrix given\n");
			/*ctm = crtc->state->ctm->data;
			csc_para.flag.bprogrammable = 1;
			csc_para.flag.bbypass = 0;
			csc_para.crtc_index = gcrtc->index;
			csc_para.brightness = 255;//not change brightness in ctm_mgmt_changed
			csc_para.coef_1 = CONVERT_S2_12(ctm->matrix[0]);
			csc_para.coef_2 = CONVERT_S2_12(ctm->matrix[1]);
			csc_para.coef_3 = CONVERT_S2_12(ctm->matrix[2]);
			csc_para.coef_4 = CONVERT_S2_12(ctm->matrix[3]);
			csc_para.coef_5 = CONVERT_S2_12(ctm->matrix[4]);
			csc_para.coef_6 = CONVERT_S2_12(ctm->matrix[5]);
			csc_para.coef_7 = CONVERT_S2_12(ctm->matrix[6]);
			csc_para.coef_8 = CONVERT_S2_12(ctm->matrix[7]);
			csc_para.coef_9 = CONVERT_S2_12(ctm->matrix[8]);

			dm_pipe_set_csc(gcrtc->adapter, &csc_para);*/
		}

		if (crtc->state->gamma_lut) {//set gamma
			dlut = (struct drm_color_lut *)crtc->state->gamma_lut->data;
			dlut_size = drm_color_lut_size(crtc->state->gamma_lut);

			if (dlut_size != MAX_COLOR_LEGACY_LUT_ENTRIES) {
				gf_error("error gamma size %d\n", dlut_size);
				return;
			}

			gamma_info.crtc_index = gcrtc->index;
			gamma_info.lut_length = dlut_size;
			gamma_info.gamma_bit = 8;
			gamma_info.gamma_enable = 1;

			gamma_info.data.pgamma_ramp_rgb = (pdisp_gamma_ramp_rgb_t)gf_calloc(sizeof(disp_gamma_ramp_rgb_t));
			for (i = 0; i < dlut_size; i++) {
				gamma_info.data.pgamma_ramp_rgb->Red[i] = dlut[i].red >> 8;
				gamma_info.data.pgamma_ramp_rgb->Green[i] = dlut[i].green >> 8;
				gamma_info.data.pgamma_ramp_rgb->Blue[i] = dlut[i].blue >> 8;
			}

			dm_pipe_set_gamma(gcrtc->adapter, &gamma_info);
			gf_free(gamma_info.data.pgamma_ramp_rgb);
			gamma_info.data.pgamma_ramp_rgb = NULL;
		}
	}
}

#if DRM_VERSION_CODE >= KERNEL_VERSION(5,15,0)
void crtc_atomic_disable(struct drm_crtc *crtc, struct drm_atomic_state *state)
#else
void crtc_atomic_disable(struct drm_crtc *crtc, struct drm_crtc_state *state)
#endif
{
	struct crtc_t *gcrtc = dcrtc_to_gcrtc(crtc);
	struct display_adapter *adapter = gcrtc->adapter;
#if DRM_VERSION_CODE >= KERNEL_VERSION(5,15,0)
		struct drm_crtc_state *old_state = drm_atomic_get_old_crtc_state(state, crtc);
#else
		struct drm_crtc_state *old_state = drm_atomic_get_old_crtc_state(state->state, crtc);
#endif


	drm_atomic_helper_disable_planes_on_crtc(old_state, false);
	drm_crtc_vblank_off(crtc);
	dm_crtc_dpms(adapter, gcrtc, false);
}

#define DRM_SCANOUTPOS_VALID        (1 << 0)
#define DRM_SCANOUTPOS_IN_VBLANK    (1 << 1)
#define DRM_SCANOUTPOS_ACCURATE     (1 << 2)
#define USE_REAL_VBLANKSTART            (1 << 30)
#define GET_DISTANCE_TO_VBLANKSTART     (1 << 31)

#if DRM_VERSION_CODE >= KERNEL_VERSION(5,7,0)
static bool crtc_get_scanout_position(struct drm_crtc *crtc,
				     bool in_vblank_irq, int *vpos, int *hpos,
				     ktime_t *stime, ktime_t *etime,
				     const struct drm_display_mode *mode)
{
	struct crtc_t *gcrtc = dcrtc_to_gcrtc(crtc);
	int in_hblank = 0, in_vblank = 0, ret = 0, status = 0;
	int hp = 0, vp = 0;

    if (stime) {
        *stime = ktime_get();
    }

    status = dm_crtc_get_hv_position(gcrtc->adapter, gcrtc->index, &hp, &vp, &in_hblank, &in_vblank);

    if (etime) {
        *etime = ktime_get();
    }

    if (!status) {
        if (hpos) {
            *hpos = in_hblank ? hp - (int)mode->crtc_htotal : hp;
        }

        if (vpos) {
            *vpos = in_vblank ? vp - (int)mode->crtc_vtotal : vp;
        }

        ret = DRM_SCANOUTPOS_VALID | DRM_SCANOUTPOS_ACCURATE;
        if (in_vblank) {
            ret |= DRM_SCANOUTPOS_IN_VBLANK;
        }

        //gf_info("scanout_position: hb[%d]vb[%d] hp[%d]vp[%d] -> hp[%d]vp[%d], ret[%d]\n",
          //  in_hblank, in_vblank, hp, vp, *hpos, *vpos, ret);
    }

    return  ret;
}
#endif

static void
add_modifier(uint64_t **mods, uint64_t *size, uint64_t *cap, uint64_t mod)
{
	if (!*mods)
		return;

	if (*cap - *size < 1) {
		uint64_t new_cap = *cap * 2;
		uint64_t *new_mods = gf_calloc(new_cap * sizeof(uint64_t));

		if (!new_mods) {
			gf_free(*mods);
			*mods = NULL;
			return;
		}

		gf_memcpy(new_mods, *mods, sizeof(uint64_t) * *size);
		gf_free(*mods);
		*mods = new_mods;
		*cap = new_cap;
	}

	(*mods)[*size] = mod;
	*size += 1;
}

static int
get_plane_modifiers(const struct lisuan_device *dev, unsigned int plane_type, uint64_t **mods)
{
	uint64_t size = 0, capacity = 128;
	*mods = NULL;

	*mods = gf_calloc(capacity * sizeof(uint64_t));

	if (plane_type == DRM_PLANE_TYPE_CURSOR) {
		add_modifier(mods, &size, &capacity, DRM_FORMAT_MOD_LINEAR);
		add_modifier(mods, &size, &capacity, DRM_FORMAT_MOD_INVALID);
		return *mods ? 0 : -ENOMEM;
	}

	//not support TILE surface
	add_modifier(mods, &size, &capacity, LISUAN_DRM_FMT_MOD_LINEAR_COMPRESS);
	add_modifier(mods, &size, &capacity, LISUAN_DRM_FMT_MOD_LINEAR_NONECOMPRESS);
	add_modifier(mods, &size, &capacity, DRM_FORMAT_MOD_LINEAR);
	/* INVALID marks the end of the list. */
	add_modifier(mods, &size, &capacity, DRM_FORMAT_MOD_INVALID);

	if (!*mods)
		return -ENOMEM;

	return 0;
}

static struct drm_crtc_helper_funcs crtc_helper = {
	.mode_fixup = NULL,
	.atomic_enable = crtc_atomic_enable,
	.atomic_flush = crtc_atomic_flush,
	.atomic_disable = crtc_atomic_disable,
	.mode_set = crtc_mode_set,
	.mode_set_nofb = crtc_mode_set_nofb,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 7, 0)		//in older kernel, get_scanout_position can be found in drm_driver
	.get_scanout_position = crtc_get_scanout_position,
#endif
};

static int gf_kms_crtc_init(struct display_adapter *adapter)
{
	struct drm_device *ddev = adapter->gdevice->ddev;
	struct crtc_t *crtcs;
	struct crtc_t *crtc = NULL;
	struct plane_t *planes;
	struct plane_t *plane;
	enum drm_plane_type type;
	struct drm_plane *primary = NULL, *cursor = NULL, *overlay = NULL;
	unsigned int *format, format_num;
	int i = 0, j = 0;
	int ret = 0;

	adapter->crtcs = gf_calloc(adapter->crtc_num * sizeof(struct crtc_t));
	crtcs = adapter->crtcs;
	if (!crtcs) {
		gf_printk(GF_DRV_ERROR, "alloc crtc fail\n");
		return -1;
	}

	for (i = 0; i < adapter->crtc_num; i++) {
		crtc = &crtcs[i];
		crtc->index = i;
		crtc->dpms = 0;
		crtc->adapter = adapter;
		crtc->planes = gf_calloc(adapter->plane_num * sizeof(struct plane_t));
		planes = crtc->planes;
		for (j = 0; j < adapter->plane_num; j++) {
			uint64_t *modifiers = NULL;
			plane = &planes[j];
			plane->crtc_index = crtc->index;
			plane->adapter = adapter;
			if (j == 0) {
				plane->index = DISP_PRIMARY_PLANE;
				plane->phdr_mgr = gf_calloc(sizeof(HdrMgr_t));
				type = DRM_PLANE_TYPE_PRIMARY;
				format = plane_format;
				format_num = ARRAY_SIZE(plane_format); 
				primary = &plane->base;
			} else if (j == 1) {
				plane->index = DISP_CURSOR_PLANE;
				type = DRM_PLANE_TYPE_CURSOR;
				format = cursor_format;
				format_num = ARRAY_SIZE(cursor_format);
				cursor = &plane->base;
			} else {
				plane->index = DISP_OVERLAY_PLANE;
				plane->phdr_mgr = gf_calloc(sizeof(HdrMgr_t));
				type = DRM_PLANE_TYPE_OVERLAY;
				format = plane_format;
				format_num = ARRAY_SIZE(plane_format); 
				overlay = &plane->base;
			}
			ret = get_plane_modifiers(adapter->gdevice, type, &modifiers);
			if (ret)
				return ret;

#if DRM_VERSION_CODE >= KERNEL_VERSION(5,18,0)
#ifdef __VIRTAUL_VBLANK__
			ddev->mode_config.fb_modifiers_not_supported = true;
#else
			if(!modifiers)
				ddev->mode_config.fb_modifiers_not_supported = true;
#endif
#endif
			ddev->mode_config.async_page_flip = true;
			drm_universal_plane_init(ddev, &plane->base, 0xff, 
					&plane_func, format, format_num,
#ifdef __VIRTAUL_VBLANK__
					NULL,
#else
					modifiers,
#endif
					type, "gf plane");

			drm_plane_helper_add(&plane->base, &plane_helper);

			plane_install_property(plane, adapter->gdevice->hwctl.display_cts || adapter->gdevice->hwctl.dp_cts);
			gf_free(modifiers);
		}
		drm_crtc_init_with_planes(ddev, &crtc->base, primary, cursor, &crtc_func, "gf crtc");
		drm_crtc_helper_add(&crtc->base, &crtc_helper);
		
		if (primary)
			primary->possible_crtcs =  drm_crtc_mask(&crtc->base);
		if (overlay)
			overlay->possible_crtcs = drm_crtc_mask(&crtc->base);
		if (cursor)
			cursor->possible_crtcs = drm_crtc_mask(&crtc->base);

		crtc_install_property(crtc);
		drm_crtc_enable_color_mgmt(&crtc->base, 0,
					true, MAX_COLOR_LEGACY_LUT_ENTRIES);//only color enhance and gamma, no degamma
		drm_mode_crtc_set_gamma_size(&crtc->base, MAX_COLOR_LEGACY_LUT_ENTRIES);

#ifdef __VIRTAUL_VBLANK__
		gf_kms_virt_vblank_init(adapter, i);
#endif
	}
#if DRM_VERSION_CODE < KERNEL_VERSION(5,15,0) || (DRM_VERSION_CODE < KERNEL_VERSION(6,7,0) && IS_ENABLED(CONFIG_DRM_LEGACY))
	ddev->irq_enabled = true;
#endif

	drm_vblank_init(ddev, adapter->crtc_num);

	return 0;
}

static int gf_kms_crtc_deinit(struct display_adapter *adapter)
{
	struct crtc_t *crtcs = adapter->crtcs;
	struct crtc_t *crtc;
	struct plane_t *planes;
	int i = 0;

	if (!crtcs)
		return -1;

	for (i = 0; i < adapter->crtc_num; i++) {
		crtc = &crtcs[i];
		planes = crtc->planes;
		if (planes) {
			gf_free(planes);
			crtc->planes = NULL;
		}
	}

	gf_free(crtcs);
	adapter->crtcs = NULL;

	return 0;
}

static const struct drm_prop_enum_list connector_color_format[] = {
	{DISP_COLOR_FORMAT_TYPE_RGB, "rgb"},
	{DISP_COLOR_FORMAT_TYPE_YUV444, "yuv444"},
	{DISP_COLOR_FORMAT_TYPE_YUV422, "yuv422"},
	{DISP_COLOR_FORMAT_TYPE_YUV420, "yuv420"},
};

static const struct drm_prop_enum_list connector_color_depth[] = {
	{DISP_COLOR_DEPTH_6, "6 bpc"},
	{DISP_COLOR_DEPTH_8, "8 bpc"},
	{DISP_COLOR_DEPTH_10, "10 bpc"},
	{DISP_COLOR_DEPTH_12, "12 bpc"},
	{DISP_COLOR_DEPTH_16, "16 bpc"},
};

static const struct drm_prop_enum_list connector_colorimetry[] = {
	{DISP_COLORIMETRY_SRGB, "srgb"},
	{DISP_COLORIMETRY_YCC_NONE, "ycc"},
	{DISP_COLORIMETRY_YCC_SMPTE_170M, "ycc smpte 170m"},
	{DISP_COLORIMETRY_YCC_BT709, "ycc bt709"},
	{DISP_COLORIMETRY_XVYCC_601, "xvycc 601"},
	{DISP_COLORIMETRY_XVYCC_709, "xvycc 709"},
	{DISP_COLORIMETRY_SYCC_601, "sycc 601"},
	{DISP_COLORIMETRY_YCC_ADOBE_601, "ycc adobe 601"},
	{DISP_COLORIMETRY_RGB_ADOBE, "rgb adobe"},
	{DISP_COLORIMETRY_CYCC_BT2020, "cycc 2020"},
	{DISP_COLORIMETRY_YCC_BT2020, "ycc 2020"},
	{DISP_COLORIMETRY_RGB_BT2020, "rgb 2020"},
	{DISP_COLORIMETRY_RGB_DCI, "rgb dci"},
	{DISP_COLORIMETRY_RGB_DCI_D65, "rgb dci d65"},
};

static const struct drm_prop_enum_list connector_cts_dp_link_rate[] = {
	{DISP_DP_LINK_RATE_TYPE_1_62, "link_rate_1_62"},
	{DISP_DP_LINK_RATE_TYPE_2_7,  "link_rate_2_7"},
	{DISP_DP_LINK_RATE_TYPE_5_4,  "link_rate_5_4"},
	{DISP_DP_LINK_RATE_TYPE_8_1,  "link_rate_8_1"},
};

static const struct drm_prop_enum_list connector_cts_dp_async_clock[] = {
	{DISP_DP_ASYNC_CLOCK_ASYNC, "async"},
	{DISP_DP_ASYNC_CLOCK_SYNC, "sync"},
};

static const struct drm_prop_enum_list connector_cts_dp_range_type[] = {
	{DISP_RANGE_TYPE_VESA, "vesa"},
	{DISP_RANGE_TYPE_CEA, "cea"},
};

static const struct drm_prop_enum_list connector_cts_dp_ycbcr_coefficients[] = {
	{DISP_YUV_COEFFICIENTS_ITU601, "yuv_601"},
	{DISP_YUV_COEFFICIENTS_ITU709, "yuv_709"},
};

static const struct drm_prop_enum_list connector_cts_dp_enhanced_frame[] = {
	{DISP_DP_ENHANCED_FRAME_MODE_NORMAL, "normal"},
	{DISP_DP_ENHANCED_FRAME_MODE_ENHANCED, "enhanced"},
};

static void connector_install_property(struct connector_t *gconnector)
{
	struct drm_connector *connector = &gconnector->base;
	struct drm_device *ddev = connector->dev;

	gconnector->output_signal = drm_property_create_enum(ddev, DRM_MODE_PROP_ENUM, "output signal",
		connector_color_format, ARRAY_SIZE(connector_color_format));
	gconnector->color_depth = drm_property_create_enum(ddev, DRM_MODE_PROP_ENUM, "output color depth",
		connector_color_depth, ARRAY_SIZE(connector_color_depth));
	gconnector->colorimetry = drm_property_create_enum(ddev, DRM_MODE_PROP_ENUM, "output colorimetry",
		connector_colorimetry, ARRAY_SIZE(connector_colorimetry));

	// cts dp
	gconnector->dp_cts_valid_bits = drm_property_create_range(ddev, DRM_MODE_PROP_RANGE, "dp valid_flag", 0, 0xffff);
	gconnector->dp_link_rate = drm_property_create_enum(ddev, DRM_MODE_PROP_ENUM, "dp link_rate",
		connector_cts_dp_link_rate, ARRAY_SIZE(connector_cts_dp_link_rate));
	gconnector->dp_lane_count = drm_property_create_range(ddev, DRM_MODE_PROP_RANGE, "dp lane_count", 0, 4);
	gconnector->dp_tps = drm_property_create_range(ddev, DRM_MODE_PROP_RANGE, "dp tps", 0, 4);
	gconnector->dp_async_clock = drm_property_create_enum(ddev, DRM_MODE_PROP_ENUM, "dp async_clock",
		connector_cts_dp_async_clock, ARRAY_SIZE(connector_cts_dp_async_clock));
	gconnector->dp_range_type = drm_property_create_enum(ddev, DRM_MODE_PROP_ENUM, "dp range_type",
		connector_cts_dp_range_type, ARRAY_SIZE(connector_cts_dp_range_type));
	gconnector->dp_ycbcr_coefficients = drm_property_create_enum(ddev, DRM_MODE_PROP_ENUM, "dp ycbcr_coefficients",
		connector_cts_dp_ycbcr_coefficients, ARRAY_SIZE(connector_cts_dp_ycbcr_coefficients));
	gconnector->dp_enhanced_frame = drm_property_create_enum(ddev, DRM_MODE_PROP_ENUM, "dp enhanced_frame",
		connector_cts_dp_enhanced_frame, ARRAY_SIZE(connector_cts_dp_enhanced_frame));

	if (!gconnector->output_signal || !gconnector->color_depth || !gconnector->colorimetry
		|| !gconnector->dp_cts_valid_bits || !gconnector->dp_link_rate || !gconnector->dp_lane_count
		|| !gconnector->dp_tps || !gconnector->dp_async_clock || !gconnector->dp_range_type
		|| !gconnector->dp_ycbcr_coefficients || !gconnector->dp_enhanced_frame) {
		gf_printk(GF_DRV_ERROR, "create properties one or more failed: [%#x, %#x, %#x], [%#x, %#x, %#x], [%#x, %#x, %#x], [%#x, %#x]\n",
			gconnector->output_signal, gconnector->color_depth, gconnector->colorimetry,
			gconnector->dp_cts_valid_bits, gconnector->dp_link_rate, gconnector->dp_lane_count,
			gconnector->dp_tps, gconnector->dp_async_clock, gconnector->dp_range_type,
			gconnector->dp_ycbcr_coefficients, gconnector->dp_enhanced_frame);
		return;
	}

	drm_object_attach_property(&connector->base, gconnector->output_signal, DISP_COLOR_FORMAT_TYPE_RGB);
	drm_object_attach_property(&connector->base, gconnector->color_depth, DISP_COLOR_DEPTH_8);
	drm_object_attach_property(&connector->base, gconnector->colorimetry, DISP_COLORIMETRY_SRGB);

	drm_object_attach_property(&connector->base, gconnector->dp_cts_valid_bits, 0);
	drm_object_attach_property(&connector->base, gconnector->dp_link_rate, DISP_DP_LINK_RATE_TYPE_1_62);
	drm_object_attach_property(&connector->base, gconnector->dp_lane_count, 0);
	drm_object_attach_property(&connector->base, gconnector->dp_tps, 2);
	drm_object_attach_property(&connector->base, gconnector->dp_async_clock, DISP_DP_ASYNC_CLOCK_ASYNC);
	drm_object_attach_property(&connector->base, gconnector->dp_range_type, DISP_RANGE_TYPE_VESA);
	drm_object_attach_property(&connector->base, gconnector->dp_ycbcr_coefficients, DISP_YUV_COEFFICIENTS_ITU601);
	drm_object_attach_property(&connector->base, gconnector->dp_enhanced_frame, DISP_DP_ENHANCED_FRAME_MODE_NORMAL);
}

static void connector_reset(struct drm_connector *connector)
{
	struct connector_state_t *gstate = dconnector_state_to_gconnector(connector->state);

	if (connector->state)
	{
		connector->funcs->atomic_destroy_state(connector, connector->state);
		connector->state = NULL;
		gstate = NULL;
	}

	gstate = gf_calloc(sizeof(struct connector_state_t));

	if (gstate) {
		gstate->output_signal = DISP_COLOR_FORMAT_TYPE_RGB;
		gstate->color_depth = DISP_COLOR_DEPTH_8;
		gstate->colorimetry = DISP_COLORIMETRY_SRGB;

		gstate->cts_dp.valid_bits = 0;
		gstate->cts_dp.link_rate = DISP_DP_LINK_RATE_TYPE_1_62;
		gstate->cts_dp.lane_count = 0;
		gstate->cts_dp.tps = 2;
		gstate->cts_dp.async_clock = DISP_DP_ASYNC_CLOCK_ASYNC;
		gstate->cts_dp.range_type = DISP_RANGE_TYPE_VESA;
		gstate->cts_dp.ycbcr_coefficients = DISP_YUV_COEFFICIENTS_ITU601;
		gstate->cts_dp.enhanced_frame = DISP_DP_ENHANCED_FRAME_MODE_NORMAL;

		__drm_atomic_helper_connector_reset(connector, &gstate->base);
	}
	else {
		gf_printk(GF_DRV_ERROR, "connector_reset, malloc failed\n");
	}
}

static int connector_atomic_set_property(struct drm_connector *connector, struct drm_connector_state *state, struct drm_property *property, uint64_t val)
{
	struct connector_t *gconnector = dconnector_to_gconnector(connector);
	struct connector_state_t *gstate = dconnector_state_to_gconnector(state);
	int ret = 0;

	if (gconnector->color_depth == property) {
		gstate->color_depth = val;
		ret = 0;
	} else if (gconnector->output_signal == property) {
		gstate->output_signal = val;
		ret = 0;
	} else if (gconnector->colorimetry == property) {
		gstate->colorimetry = val;
		ret = 0;
	} else if (gconnector->dp_cts_valid_bits == property) {
		gstate->cts_dp.valid_bits = val;
		ret = 0;
	} else if (gconnector->dp_link_rate == property) {
		gstate->cts_dp.link_rate = val;
		ret = 0;
	} else if (gconnector->dp_lane_count == property) {
		gstate->cts_dp.lane_count = val;
		ret = 0;
	} else if (gconnector->dp_tps == property) {
		gstate->cts_dp.tps = val;
		ret = 0;
	} else if (gconnector->dp_async_clock == property) {
		gstate->cts_dp.async_clock = val;
		ret = 0;
	} else if (gconnector->dp_range_type == property) {
		gstate->cts_dp.range_type = val;
		ret = 0;
	} else if (gconnector->dp_ycbcr_coefficients == property) {
		gstate->cts_dp.ycbcr_coefficients = val;
		ret = 0;
	} else if (gconnector->dp_enhanced_frame == property) {
		gstate->cts_dp.enhanced_frame = val;
		ret = 0;
	} else {
		gf_printk(GF_DRV_ERROR, "unknown connector property\n");
		ret = -EINVAL;
	}

	return ret;
}

static int connector_atomic_get_property(struct drm_connector *connector, const struct drm_connector_state *state, struct drm_property *property, uint64_t *val)
{
	struct connector_t *gconnector = dconnector_to_gconnector(connector);
	struct connector_state_t *gstate = dconnector_state_to_gconnector(state);
	int ret = 0;

	if (gconnector->color_depth == property) {
		*val = gstate->color_depth;
		ret = 0;
	} else if (gconnector->output_signal == property) {
		*val = gstate->output_signal;
		ret = 0;
	} else if (gconnector->colorimetry == property) {
		*val = gstate->colorimetry;
		ret = 0;
	} else if (gconnector->dp_cts_valid_bits == property) {
		*val = gstate->cts_dp.valid_bits;
		ret = 0;
	} else if (gconnector->dp_link_rate == property) {
		*val = gstate->cts_dp.link_rate;
		ret = 0;
	} else if (gconnector->dp_lane_count == property) {
		*val = gstate->cts_dp.lane_count;
		ret = 0;
	} else if (gconnector->dp_tps == property) {
		*val = gstate->cts_dp.tps;
		ret = 0;
	} else if (gconnector->dp_async_clock == property) {
		*val = gstate->cts_dp.async_clock;
		ret = 0;
	} else if (gconnector->dp_range_type == property) {
		*val = gstate->cts_dp.range_type;
		ret = 0;
	} else if (gconnector->dp_ycbcr_coefficients == property) {
		*val = gstate->cts_dp.ycbcr_coefficients;
		ret = 0;
	} else if (gconnector->dp_enhanced_frame == property) {
		*val = gstate->cts_dp.enhanced_frame;
		ret = 0;
	} else {
		gf_printk(GF_DRV_ERROR, "unknown connector property\n");
		ret = -EINVAL;
	}

	return ret;
}

static struct drm_connector_state* connector_atomic_duplicate_state(struct drm_connector *connector)
{
	struct connector_t *gconnector = dconnector_to_gconnector(connector);
	struct connector_state_t *gstate, *old_state;
	
	old_state = dconnector_state_to_gconnector(connector->state);

	if (old_state)
		gstate = kmemdup(old_state, sizeof(struct connector_state_t), GFP_KERNEL);
	else
		gstate = kmemdup(connector->state, sizeof(struct connector_state_t), GFP_KERNEL);
	if (!gstate)
		return NULL;

	__drm_atomic_helper_connector_duplicate_state(connector, &gstate->base);
	return &gstate->base;
}

static void connector_atomic_destroy_state(struct drm_connector *connector, struct drm_connector_state *state)
{
	struct connector_state_t *gstate = dconnector_state_to_gconnector(state);

	__drm_atomic_helper_connector_destroy_state(state);

	if (gstate)
		gf_free(gstate);
}

static enum drm_connector_status connector_detect(struct drm_connector *connector)
{
	struct connector_t *gconnector = dconnector_to_gconnector(connector);
	struct display_adapter *adapter = gconnector->adapter;
	int to_detect = 1 << gconnector->index;
	int to_fake = adapter->gdevice->hwctl.fake_monitor_mask;
	int detected = 0;
	enum drm_connector_status ret = connector_status_disconnected;
	int dm_ret = 0;
	int idx = 0;

	//gf_info("connector_detect, real detect %d\n", gconnector->index);

	adapter->detected_mask &= ~to_detect;
	mutex_lock(&gconnector->ddc_mutex.lock);
	dm_ret = dm_detect_connector(adapter, to_detect, to_fake, &detected);
	if (dm_ret) {
		gf_info("connector_detect[%u] dm detect failed ret[%d]\n", connector->index, dm_ret);
		mutex_unlock(&gconnector->ddc_mutex.lock);
		return connector_status_connected;
	}
	mutex_unlock(&gconnector->ddc_mutex.lock);

	ret = (detected & to_detect) ? (connector_status_connected) : (connector_status_disconnected);
	gf_printk(GF_DRV_DEBUG, "connector_detect[%u] ret[%d]\n", connector->index, ret);

	if (connector_status_connected == ret) {
		dm_ret = dm_connector_get_edid(adapter, gconnector);
		if (dm_ret) {
			gf_info("connector_detect[%u] get edid failed ret[%d]\n", connector->index, dm_ret);
			return connector_status_disconnected;
		} else {
			dm_ret = dm_connector_get_modelist(adapter, gconnector);
			if (dm_ret) {
				gf_info("connector_detect[%u] get modelist failed ret[%d]\n", connector->index, dm_ret);
				return connector_status_disconnected;
			}
		}

		// On systems such as UOS, DPMS remains enabled after plug out
		// When reconnecting same screen, there may no link-training, 
		// If the display does not send an Interrupt Request (IRQ), the screen will not light up, so manual IRQ triggering is required
		if (connector->state && connector->state->crtc && connector->state->crtc->state->active) {
			gf_info("connector_detect[%u]: CRTC is Active when connector detect, need link training check\n", connector->index);
			dm_notify_irq_hpd(adapter, connector->index);
		}
	}

	adapter->detected_mask |= detected;
	return ret;
}

static enum drm_connector_status connector_detect_ex(struct drm_connector *connector, bool force)
{
    disp_get_hpd_state_t hpd_state;
    struct connector_t *gconnector = dconnector_to_gconnector(connector);
    struct display_adapter *adapter = gconnector->adapter;

    //gf_info("connector_detect_ex, to detect %d, force %d, detected_mask %d\n", gconnector->index, force, adapter->detected_mask);

    /* Ignore force, only real detect after hpd out */
    //if (force || adapter->gdevice->hwctl.dp_cts) // CTS force real detect
    //    return connector_detect(connector);

    if (adapter->detected_mask & (1 << gconnector->index)) // detected before
    {
        hpd_state.connector_index = gconnector->index;
        hpd_state.hpd_state = DISP_HPD_TYPE_IDLE;
        dm_get_hpd_state(adapter, &hpd_state);

        if (DISP_HPD_TYPE_PLUG_IN == hpd_state.hpd_state || DISP_HPD_TYPE_EVENT == hpd_state.hpd_state)
        {   // detected before -> hpd in now
            //gf_info("connector_detect_ex, skip detect %d\n", gconnector->index);
            return connector_status_connected;
        }
    }

    return connector_detect(connector);
}

int connector_later_register(struct drm_connector *connector)
{
	struct connector_t *gconnector = dconnector_to_gconnector(connector);
	int ret = 0;

#if defined(CONFIG_DEBUG_FS)
	connector_debugfs_init(gconnector);
#endif

	return ret;
}

void connector_early_unregister(struct drm_connector *connector)
{

}



static struct drm_connector_funcs connector_func = {
	.reset = connector_reset,
	.detect = connector_detect_ex,
	.destroy = drm_connector_cleanup,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.atomic_set_property = connector_atomic_set_property,
	.atomic_get_property = connector_atomic_get_property,
	.atomic_destroy_state = connector_atomic_destroy_state,
	.atomic_duplicate_state = connector_atomic_duplicate_state,
	.late_register = connector_later_register,
	.early_unregister = connector_early_unregister,
};

static int connector_get_modes(struct drm_connector *connector)
{
	struct connector_t *gconnector = dconnector_to_gconnector(connector);
	struct display_adapter *adapter = gconnector->adapter;
	pdisp_display_mode_t gf_mode;
	struct drm_display_mode *mode;
	int mode_num = 0, i = 0;

	mode_num = gconnector->mode_num;

	for (i = 0; i < mode_num; i++) {
		gf_mode = &gconnector->modes[i];
		mode = gf_calloc(sizeof(struct drm_display_mode));//will be free in connnector destroy func
		mode_gf_to_drm(gf_mode, mode);
		drm_mode_probed_add(connector, mode);
	}

	drm_connector_update_edid_property(connector, (struct edid*)gconnector->edid);

	return mode_num;
}

#if DRM_VERSION_CODE >= KERNEL_VERSION(6,15,0)
static enum drm_mode_status connector_mode_valid(struct drm_connector *connector, const struct drm_display_mode *mode)
#else
static enum drm_mode_status connector_mode_valid(struct drm_connector *connector, struct drm_display_mode *mode)
#endif
{
	return MODE_OK;
}

#if DRM_VERSION_CODE >= KERNEL_VERSION(5,15,0)
	static struct drm_encoder* connector_atomic_best_encoder(struct drm_connector *connector, 
												struct drm_atomic_state *state)
#else
	static struct drm_encoder* connector_atomic_best_encoder(struct drm_connector *connector, 
												struct drm_connector_state *state)
#endif
{
	struct drm_encoder *encoder;
	int pos=0;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 5, 0)
	WARN_ON(hweight32(connector->possible_encoders) > 1);
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 5, 0)
	drm_connector_for_each_possible_encoder(connector, encoder)
#else
	drm_connector_for_each_possible_encoder(connector, encoder, pos)
#endif
		return encoder;

	return NULL;
}

static bool connector_atomic_check_cts(struct cts_dp_state_t *snew, struct cts_dp_state_t *sold)
{
	bool changed = false;

	if (!snew->valid_bits && !sold->valid_bits) // not valid, ignore
		changed = false;
	else if (snew->valid_bits != sold->valid_bits)
		changed = true;
	else {
		changed |= (snew->link_rate != sold->link_rate);
		changed |= (snew->lane_count != sold->lane_count);
		changed |= (snew->tps != sold->tps);
		changed |= (snew->async_clock != sold->async_clock);
		changed |= (snew->range_type != sold->range_type);
		changed |= (snew->ycbcr_coefficients != sold->ycbcr_coefficients);
		changed |= (snew->enhanced_frame != sold->enhanced_frame);
	}

	return changed;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
static int connector_atomic_check(struct drm_connector *connector, struct drm_atomic_state *state)
#else
static int connector_atomic_check(struct drm_connector *connector, struct drm_connector_state *conn_state)
#endif
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0)
	struct drm_atomic_state *state = conn_state->state;
#endif

	struct drm_connector_state *new_con_state = drm_atomic_get_new_connector_state(state, connector);
	struct drm_connector_state *old_con_state = drm_atomic_get_old_connector_state(state, connector);
	struct connector_state_t *gstate = dconnector_state_to_gconnector(new_con_state);
	struct connector_state_t *gstate_old = dconnector_state_to_gconnector(old_con_state);
	struct drm_crtc_state *new_crtc_state;

	if (new_con_state->crtc) {
		new_crtc_state = drm_atomic_get_new_crtc_state(state, new_con_state->crtc);
		if (gstate->output_signal != gstate_old->output_signal ||
			gstate->color_depth != gstate_old->color_depth ||
			gstate->colorimetry != gstate_old->colorimetry ||
			connector_atomic_check_cts(&gstate->cts_dp, &gstate_old->cts_dp)
			) {
			/* If specific property changed, reset mode & dpms */
			new_crtc_state->connectors_changed = true;
			new_crtc_state->mode_changed = true;
		}
	}

	return 0;
}

static struct drm_connector_helper_funcs connector_helper = {
	.get_modes = connector_get_modes,
	.mode_valid = connector_mode_valid,
	.atomic_best_encoder = connector_atomic_best_encoder,
	.atomic_check = connector_atomic_check,
};

static struct drm_encoder_funcs encoder_func = {
	.destroy = drm_encoder_cleanup,
};

static void encoder_dpms(struct drm_encoder *encoder, int mode)
{
	struct encoder_t *gencoder = dencoder_to_gencoder(encoder);
	struct connector_t *gconnector = &gencoder->adapter->connectors[gencoder->index];

	gconnector->ddc_func.encoder_dpms(gconnector, (bool)(mode == DRM_MODE_DPMS_ON) ? true : false);
}

static bool is_adapter_mode(const struct drm_display_mode *mode, struct connector_t *connector)
{
	bool ret = false;
	int index = 0;
	int mode_num = connector->mode_num;

	for (index = 0; index < mode_num; index++) {
		if (connector->modes[index].flags & (DISP_MODE_FLAG_ADAPTER_SOURCE)) {
			if (mode->hdisplay != connector->modes[index].hdisplay) {
				continue;
			}

			if (mode->vdisplay != connector->modes[index].vdisplay) {
				continue;
    		}

    			if (mode->clock != connector->modes[index].clock) {
				continue;
    		}

			ret = true;
			break;
		}
	}

	return ret;
}

static bool encoder_mode_fixup(struct drm_encoder *encoder,
                        const struct drm_display_mode *mode,
                        struct drm_display_mode *adjusted_mode)
{
	int index_mode = 0;
	int index_connector = 0;
	int mode_num = 0;
	struct encoder_t *gencoder;
	struct display_adapter *adapter;
	struct connector_t *connector;

	gencoder = dencoder_to_gencoder(encoder);
	adapter  = gencoder->adapter;

	connector = &(adapter->connectors[gencoder->connector_index]);
	mode_num  = connector->mode_num;

	if (is_adapter_mode(mode, connector)) {
		gf_printk(GF_DRV_DEBUG, "adapter mode need fixup!\n");

		for (index_mode = 0; index_mode < mode_num; index_mode++) {
			if(connector->modes[index_mode].type & (DISP_MODE_TYPE_PREFERRED)) {
				mode_gf_to_drm(&(connector->modes[index_mode]), adjusted_mode);
				break;
			}	
		}
	} else {
		gf_memcpy(adjusted_mode, (struct drm_display_mode *)mode, sizeof(struct drm_display_mode));
	}

	return true;
}
void encoder_mode_set(struct drm_encoder *encoder,
			 struct drm_display_mode *mode,
			 struct drm_display_mode *adjusted_mode)
{
	struct encoder_t *gencoder = dencoder_to_gencoder(encoder);
	struct connector_t *gconnector = &gencoder->adapter->connectors[gencoder->index];

	gconnector->ddc_func.encoder_set_mode(gconnector);
}

static void encoder_disable(struct drm_encoder *encoder)
{
	struct encoder_t *gencoder = dencoder_to_gencoder(encoder);
	struct connector_t *gconnector = &gencoder->adapter->connectors[gencoder->index];

	gconnector->ddc_func.encoder_dpms(gconnector, false);
}

static void encoder_enable(struct drm_encoder *encoder)
{
	struct encoder_t *gencoder = dencoder_to_gencoder(encoder);
	struct connector_t *gconnector = &gencoder->adapter->connectors[gencoder->index];

	gconnector->ddc_func.encoder_dpms(gconnector, true);
}

static struct drm_encoder_helper_funcs encoder_helper = {
	.dpms = encoder_dpms,
	.mode_fixup = encoder_mode_fixup,
	.mode_set = encoder_mode_set,
	.disable = encoder_disable,
	.enable = encoder_enable
};

static int ddc_irq_event(struct connector_t *connector)
{
	int ret = 0;
	mutex_lock(&connector->ddc_mutex.lock);
	ret = dm_notify_irq_hpd(connector->adapter, connector->index);
	mutex_unlock(&connector->ddc_mutex.lock);
	return ret;
}

static int ddc_encoder_set_mode(struct connector_t *connector)
{
	int ret = 0;
	mutex_lock(&connector->ddc_mutex.lock);
	ret = dm_encoder_set_mode(connector->adapter, &connector->adapter->encoders[connector->index]);
	mutex_unlock(&connector->ddc_mutex.lock);
	return ret;
}

static int ddc_encoder_dpms(struct connector_t *connector, bool enable)
{
	int ret = 0;
	mutex_lock(&connector->ddc_mutex.lock);
	ret = dm_encoder_dpms(connector->adapter, &connector->adapter->encoders[connector->index], enable);
	mutex_unlock(&connector->ddc_mutex.lock);
	return ret;
}

static int ddc_backlight_query(struct connector_t *connector, pdisp_backlight_query_t pquery)
{
	int ret = 0;
	mutex_lock(&connector->ddc_mutex.lock);
	ret = dm_backlight_query(connector->adapter, pquery);
	mutex_unlock(&connector->ddc_mutex.lock);
	return ret;
}

static int ddc_backlight_set(struct connector_t *connector, pdisp_backlight_set_t pset)
{
	int ret = 0;
	mutex_lock(&connector->ddc_mutex.lock);
	ret = dm_backlight_set(connector->adapter, pset);
	mutex_unlock(&connector->ddc_mutex.lock);
	return ret;
}

static int ddc_psr_query(struct connector_t *connector, pdisp_psr_query_t pquery)
{
	int ret = 0;
	mutex_lock(&connector->ddc_mutex.lock);
	ret = dm_psr_query(connector->adapter, pquery);
	mutex_unlock(&connector->ddc_mutex.lock);
	return ret;
}

static int ddc_psr_set(struct connector_t *connector, pdisp_psr_set_t pset)
{
	int ret = 0;
	mutex_lock(&connector->ddc_mutex.lock);
	ret = dm_psr_set(connector->adapter, pset);
	mutex_unlock(&connector->ddc_mutex.lock);
	return ret;
}

static int ddc_aux_transfer(struct connector_t *connector, pdisp_aux_para_t paux)
{
	int ret = 0;
	mutex_lock(&connector->ddc_mutex.lock);
	ret = dm_aux_transfer(connector->adapter, paux);
	mutex_unlock(&connector->ddc_mutex.lock);
	return ret;
}

static int ddc_aux_readb(struct connector_t *connector, unsigned int offset, unsigned char *val)
{
	disp_aux_para_t aux_para;

	aux_para.op = DISP_DP_NATIVE_AUX_READ;
	aux_para.connector_index = connector->index;
	aux_para.addr = offset;
	aux_para.pbuf = val;
	aux_para.size = 1;

	return ddc_aux_transfer(connector, &aux_para);
}

static int ddc_aux_writeb(struct connector_t *connector, unsigned int offset, unsigned char val)
{
	disp_aux_para_t aux_para;

	aux_para.op = DISP_DP_NATIVE_AUX_WRITE;
	aux_para.connector_index = connector->index;
	aux_para.addr = offset;
	aux_para.pbuf = &val;
	aux_para.size = 1;

	return ddc_aux_transfer(connector, &aux_para);
}

static void gf_kms_connector_ddc_init(struct connector_t *connector)
{
	mutex_init(&connector->ddc_mutex.lock);

	connector->ddc_func.irq_event = ddc_irq_event;
	connector->ddc_func.encoder_set_mode = ddc_encoder_set_mode;
	connector->ddc_func.encoder_dpms = ddc_encoder_dpms;
	connector->ddc_func.backlight_query = ddc_backlight_query;
	connector->ddc_func.backlight_set =ddc_backlight_set;
	connector->ddc_func.psr_query = ddc_psr_query;
	connector->ddc_func.psr_set = ddc_psr_set;
	connector->ddc_func.aux_transfer = ddc_aux_transfer;
	connector->ddc_func.aux_readb = ddc_aux_readb;
	connector->ddc_func.aux_writeb = ddc_aux_writeb;
}

static void gf_kms_connector_ddc_deinit(struct connector_t *connector)
{
	mutex_destroy(&connector->ddc_mutex.lock);
}

static int gf_kms_connector_init(struct display_adapter *adapter)
{
	struct drm_device *ddev = adapter->gdevice->ddev;
	struct connector_t *connectors;
	struct encoder_t *encoders;
	struct connector_t *connector;
	struct encoder_t *encoder;
	int connector_index = 0, encoder_index = 0;
	int connector_num = 0, encoder_num = 0;
	int connector_type;
	int encoder_type;
	int support = adapter->connector_support;
	int i = 0, temp = 0;

	connector_num = encoder_num = gf_get_bit_num(support);
	adapter->connectors = gf_calloc(connector_num * sizeof(struct connector_t));
	connectors = adapter->connectors;
	if (!connectors)
		return -1;

	adapter->encoders = gf_calloc(encoder_num * sizeof(struct encoder_t));
	encoders = adapter->encoders;
	if (!encoders)
		return -1;

	while (support) {
		temp = GF_GET_LAST_BIT(support);
		connector_index = encoder_index = gf_get_last_bit_index(temp);
		connector = &connectors[i];
		connector->adapter = adapter;
		connector->index = connector_index;
		gf_kms_connector_ddc_init(connector);
		if (connector_index < 4) { // DISP_ENCODER_NUM_DP - 4 DP, 0 HDMI
			connector_type = DRM_MODE_CONNECTOR_DisplayPort;
			encoder_type = DRM_MODE_ENCODER_TMDS;
		} else {
			connector_type = DRM_MODE_CONNECTOR_HDMIA;
			encoder_type = DRM_MODE_ENCODER_TMDS;
		}

		drm_connector_init(ddev, &connector->base, &connector_func, connector_type);
		drm_connector_helper_add(&connector->base, &connector_helper);
		connector_install_property(connector);

		connector->base.polled = DRM_CONNECTOR_POLL_HPD;

		encoder = &encoders[i];
		encoder->connector_index = i;
		encoder->adapter = adapter;
		encoder->index = encoder_index;

		drm_encoder_init(ddev, &encoder->base, &encoder_func, encoder_type, "gf encoder");
		drm_encoder_helper_add(&encoder->base, &encoder_helper);

		drm_connector_attach_encoder(&connector->base, &encoder->base);
		support &= (~(temp));
		encoder->base.possible_crtcs = drm_crtc_mask(&adapter->crtcs[i].base);
		
		i++;
	}

	adapter->connector_num = connector_num;

	return 0;
}

static int gf_kms_connector_deinit(struct display_adapter *adapter)
{
	struct connector_t *connector;
	int i = 0;

	if (adapter->connectors) {
		for (i = 0; i < adapter->connector_num; i++) {
			connector = &adapter->connectors[i];
			gf_kms_connector_ddc_deinit(connector);
			if (connector->modes) {
				gf_free(connector->modes);
				connector->modes = NULL;
			}
		}
		
		gf_free(adapter->connectors);
		adapter->connectors = NULL;
	}

	if (adapter->encoders) {
		gf_free(adapter->encoders);
		adapter->encoders = NULL;
	}

	return 0;
}
#if DRM_VERSION_CODE >= KERNEL_VERSION(6,17,0)
struct drm_framebuffer * drm_mode_fb_create(struct drm_device *dev,
					 struct drm_file *file_priv,
					 const struct drm_format_info *info,
					 const struct drm_mode_fb_cmd2 *mode_cmd)
{
#else
struct drm_framebuffer * drm_mode_fb_create(struct drm_device *dev,
					 struct drm_file *file_priv,
					 const struct drm_mode_fb_cmd2 *mode_cmd)
{
    const struct drm_format_info *info;
#endif
	struct drm_gem_object * gem;
	struct lisuan_gem_bo *gbo;
	int i;

#ifdef _DEBUG_
#if DRM_VERSION_CODE < KERNEL_VERSION(6,17,0)
	info = drm_get_format_info(dev, mode_cmd);
#endif
	if (!info) {
		gf_info("Failed to get FB format info\n");
		return ERR_PTR(-EINVAL);;
	}
	for (i = 0; i < info->num_planes; i++) {
		gem = drm_gem_object_lookup(file_priv, mode_cmd->handles[i]);
		gbo = gem_to_gf_bo(gem);

		gf_info("fb(0x%x) gem:0x%x, offset:0x%llx, from tgid:%d, bypass:%d, present:%d, cp fmt:%d, slot index:%d, modifier:0x%llx\n", 
			mode_cmd->handles[i], 
			gbo->handle, gbo->vm_node->offset, gbo->ls_file?gbo->ls_file->tgid:0,
			gbo->flag.byPass, gbo->is_present,
			gbo->compressFormat, gbo->cf_node?gbo->cf_node->cell_index:0, mode_cmd->modifier[i]);

		drm_gem_object_put_compat(gem);
	}
#endif
#if DRM_VERSION_CODE >= KERNEL_VERSION(6,17,0)
	return drm_gem_fb_create(dev, file_priv, info, mode_cmd);
#else
	return drm_gem_fb_create(dev, file_priv, mode_cmd);
#endif
}

static struct drm_mode_config_funcs mode_config_func = {
	.fb_create = drm_mode_fb_create,
	.atomic_check = drm_atomic_helper_check,
	.atomic_commit = drm_atomic_helper_commit,
};

int gf_kms_init(struct lisuan_device *gdevice)
{
	struct display_adapter *adapter;
	struct drm_device *ddev = gdevice->ddev;
	int ret = 0;

	gdevice->adapter = gf_calloc(sizeof(struct display_adapter));
	adapter = gdevice->adapter;
	if (!adapter)
		return -1;

	adapter->gdevice = gdevice;
	adapter->mmio_base = gdevice->mmio_base;
	adapter->rom_base = gdevice->rom_base;
	adapter->detected_mask = 0;
	atomic_set(&adapter->hpd_mask, 0);
	adapter->flag.bits.sgnt_enable = gdevice->hwctl.enable_sgnt ? 1 : 0;

	ret = dm_init(adapter);
	if (ret) {
		gf_printk(GF_DRV_ERROR, "display manager init fail\n");
		return ret;
	}

	ret = dm_query_resource(adapter);
	if (ret) {
		gf_printk(GF_DRV_ERROR, "query display resource fail\n");
		return ret;
	}

	drm_mode_config_init(ddev);
	ddev->mode_config.max_width = 3840 * 4;
	ddev->mode_config.max_height = 2160 * 4;
	ddev->mode_config.funcs = &mode_config_func;

	ret = gf_kms_crtc_init(adapter);
	if (ret) {
		gf_printk(GF_DRV_ERROR, "crtc init fail\n");
		return -1;
	}

	ret = gf_kms_connector_init(adapter);
	if (ret) {
		gf_printk(GF_DRV_ERROR, "connector init fail");
		return -1;
	}

	/* We need to coordinate the hotplugs with the asynchronous */
	drm_kms_helper_poll_init(ddev);

	drm_mode_config_reset(ddev);

	return ret;
}

int gf_kms_resume(struct lisuan_device *gdevice)
{
    struct display_adapter *adapter;
    int ret = 0;

    adapter = gdevice->adapter;
    dm_hw_init(adapter);

    return 0;
}

int gf_kms_deinit(struct lisuan_device *gdevice)
{
	struct display_adapter *adapter = gdevice->adapter;
	int ret = 0;

	drm_kms_helper_poll_fini(gdevice->ddev);

	gf_kms_connector_deinit(adapter);

	gf_kms_crtc_deinit(adapter);

	ret = dm_deinit(adapter);

	gf_free(adapter);
	gdevice->adapter = NULL;

	return ret;
}
