/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#include <linux/version.h>

#include <drm/drm_simple_kms_helper.h>
#include <drm/drm_vblank.h>
#include <drm/drm_gem.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_drv.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_file.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_ioctl.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,8,0)
#include <drm/drm_managed.h>
#endif
#if DRM_VERSION_CODE >= KERNEL_VERSION(5,1,0)
#include <drm/drm_probe_helper.h>
#else
#include <drm/drm_crtc_helper.h>
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,0,0)
#include <drm/drm_gem_shmem_helper.h>
#else
#include <drm/drm_gem.h>
#endif
#include <drm/drm_plane_helper.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_edid.h>
#include "gf_os_interface.h"
#include "gf_kms_pipe_crc.h"

#include "lisuan_device.h"
#include "gf_kms_virt.h"

#define XRES_DEF  1024
#define YRES_DEF   768

#define XRES_MAX  3840
#define YRES_MAX  2160

#define VIRT_CRTC_NUM  1
#define VIRT_ENCODER_NUM VIRT_CRTC_NUM
#define VIRT_CONNECTOR_NUM VIRT_CRTC_NUM

#ifndef DRM_PLANE_HELPER_NO_SCALING
#define DRM_PLANE_HELPER_NO_SCALING (1<<16)
#endif


/**
 * DOC: gf_kms_virt
 *
 * The gf vkms interface provides a virtual KMS interface for several use
 * cases: devices without display hardware, platforms where the actual display
 * hardware is not useful (e.g., servers), SR-IOV virtual functions, device
 * emulation/simulation, and device bring up prior to display hardware being
 * usable. We previously emulated a legacy KMS interface, but there was a desire
 * to move to the atomic KMS interface. The vkms driver did everything we
 * needed, but we wanted KMS support natively in the driver without buffer
 * sharing and the ability to support an instance of VKMS per device. We first
 * looked at splitting vkms into a stub driver and a helper module that other
 * drivers could use to implement a virtual display, but this strategy ended up
 * being messy due to driver specific callbacks needed for buffer management.
 * Ultimately, it proved easier to import the vkms code as it mostly used core
 * drm helpers anyway.
 */

static const u32 gf_kms_virt_formats[] = {
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
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
	DRM_FORMAT_YUYV,
	DRM_FORMAT_UYVY,
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

static enum hrtimer_restart gf_kms_virt_vblank_handle(struct hrtimer *timer)
{
	struct crtc_t *gcrtc = container_of(timer, struct crtc_t, vblank_hrtimer);
	struct drm_crtc *crtc = &gcrtc->base;
	struct drm_device *ddev = crtc->dev;
	struct drm_vblank_crtc *vblank = &ddev->vblank[drm_crtc_index(crtc)];
	u64 ret_overrun;
	bool ret;

	hrtimer_forward_now(timer, ktime_set(0, vblank->framedur_ns));

	ret = drm_crtc_handle_vblank(crtc);
	if (!ret)
		gf_printk(GF_DRV_ERROR, "drm_crtc_handle_vblank fail\n");

	gf_kms_crtc_get_crc(crtc);

	return HRTIMER_RESTART;
}

void gf_kms_virt_vblank_init(struct display_adapter *adapter, int crtc_index)
{
	struct crtc_t *gcrtc = &adapter->crtcs[crtc_index];
	struct hrtimer *v_timer = &gcrtc->vblank_hrtimer;

#if DRM_VERSION_CODE >= KERNEL_VERSION(6,15,0)
	hrtimer_setup(v_timer, gf_kms_virt_vblank_handle, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
#else
	hrtimer_init(v_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	v_timer->function = &gf_kms_virt_vblank_handle;
#endif
}

int gf_kms_virt_vblank_enable(struct drm_crtc *crtc)
{
	struct crtc_t *gcrtc = dcrtc_to_gcrtc(crtc);
	struct drm_device *ddev = crtc->dev;
	struct drm_vblank_crtc *vblank = &ddev->vblank[drm_crtc_index(crtc)];

	hrtimer_start(&gcrtc->vblank_hrtimer, ktime_set(0, vblank->framedur_ns), HRTIMER_MODE_REL);

	return 0;
}

void gf_kms_virt_vblank_disable(struct drm_crtc *crtc)
{
	struct crtc_t *gcrtc = dcrtc_to_gcrtc(crtc);
	struct drm_device *ddev = crtc->dev;
	struct drm_vblank_crtc *vblank = &ddev->vblank[drm_crtc_index(crtc)];

	hrtimer_cancel(&gcrtc->vblank_hrtimer);
}

bool gf_kms_virt_get_vblank_timestamp(struct drm_crtc *crtc, int *max_error, ktime_t *vblank_time, bool in_vblank_irq)
{
	struct crtc_t *gcrtc = dcrtc_to_gcrtc(crtc);
	struct drm_device *ddev = crtc->dev;
	struct drm_vblank_crtc *vblank = &ddev->vblank[drm_crtc_index(crtc)];

	*vblank_time = READ_ONCE(gcrtc->vblank_hrtimer.node.expires);

	*vblank_time -= ktime_set(0, vblank->framedur_ns);

	return true;
}

static const struct drm_crtc_funcs gf_kms_virt_crtc_funcs = {
	.set_config             = drm_atomic_helper_set_config,
	.destroy                = drm_crtc_cleanup,
	.page_flip              = drm_atomic_helper_page_flip,
	.reset                  = drm_atomic_helper_crtc_reset,
	.atomic_duplicate_state = drm_atomic_helper_crtc_duplicate_state,
	.atomic_destroy_state   = drm_atomic_helper_crtc_destroy_state,
	.enable_vblank		= gf_kms_virt_vblank_enable,
	.disable_vblank		= gf_kms_virt_vblank_disable,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 7, 0)
	.get_vblank_timestamp	= gf_kms_virt_get_vblank_timestamp,
#endif
};

#if DRM_VERSION_CODE >= KERNEL_VERSION(5,15,0)
static void gf_kms_virt_crtc_atomic_enable(struct drm_crtc *crtc,
					   struct drm_atomic_state *state)
#else
static void gf_kms_virt_crtc_atomic_enable(struct drm_crtc *crtc,
					   struct drm_crtc_state *state)
#endif
{
	drm_crtc_vblank_on(crtc);
}

#if DRM_VERSION_CODE >= KERNEL_VERSION(5,15,0)
static void gf_kms_virt_crtc_atomic_disable(struct drm_crtc *crtc,
					    struct drm_atomic_state *state)

#else
static void gf_kms_virt_crtc_atomic_disable(struct drm_crtc *crtc,
					    struct drm_crtc_state *state)

#endif
{
	drm_crtc_vblank_off(crtc);
}

#if DRM_VERSION_CODE >= KERNEL_VERSION(5,15,0)
static void gf_kms_virt_crtc_atomic_flush(struct drm_crtc *crtc,
					  struct drm_atomic_state *state)
#else
static void gf_kms_virt_crtc_atomic_flush(struct drm_crtc *crtc,
					  struct drm_crtc_state *state)
#endif
{
	unsigned long flags;
	if (crtc->state->event) {
		spin_lock_irqsave(&crtc->dev->event_lock, flags);

		if (drm_crtc_vblank_get(crtc) != 0)
			drm_crtc_send_vblank_event(crtc, crtc->state->event);
		else
			drm_crtc_arm_vblank_event(crtc, crtc->state->event);

		spin_unlock_irqrestore(&crtc->dev->event_lock, flags);

		crtc->state->event = NULL;
	}
}

static const struct drm_crtc_helper_funcs gf_kms_virt_crtc_helper_funcs = {
	.atomic_flush	= gf_kms_virt_crtc_atomic_flush,
	.atomic_enable	= gf_kms_virt_crtc_atomic_enable,
	.atomic_disable	= gf_kms_virt_crtc_atomic_disable,
};

static int gf_kms_virt_crtc_init(struct drm_device *dev, struct display_adapter *adapter, struct drm_crtc *crtc,
			  struct drm_plane *primary, struct drm_plane *cursor, int index)
{
	int ret;

	ret = drm_crtc_init_with_planes(dev, crtc, primary, cursor,
					&gf_kms_virt_crtc_funcs, NULL);
	if (ret) {
		gf_error("Failed to init CRTC\n");
		return ret;
	}

	drm_crtc_helper_add(crtc, &gf_kms_virt_crtc_helper_funcs);

	gf_kms_virt_vblank_init(adapter, index);

	return ret;
}

static const struct drm_connector_funcs gf_kms_virt_connector_funcs = {
	.fill_modes = drm_helper_probe_single_connector_modes,
	.destroy = drm_connector_cleanup,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

static int gf_kms_virt_conn_get_modes(struct drm_connector *connector)
{
	struct drm_device *dev = connector->dev;
	struct drm_display_mode *mode = NULL;
	unsigned i;
	static const struct mode_size {
		int w;
		int h;
	} common_modes[] = {
		{ 640,  480},
		{ 720,  480},
		{ 800,  600},
		{ 848,  480},
		{1024,  768},
		{1152,  768},
		{1280,  720},
		{1280,  800},
		{1280,  854},
		{1280,  960},
		{1280, 1024},
		{1440,  900},
		{1400, 1050},
		{1680, 1050},
		{1600, 1200},
		{1920, 1080},
		{1920, 1200},
		{2560, 1440},
		{4096, 3112},
		{3656, 2664},
		{3840, 2160},
		{4096, 2160},
	};

	for (i = 0; i < ARRAY_SIZE(common_modes); i++) {
		mode = drm_cvt_mode(dev, common_modes[i].w, common_modes[i].h, 60, false, false, false);
		drm_mode_probed_add(connector, mode);
	}

	drm_set_preferred_mode(connector, XRES_DEF, YRES_DEF);

	return ARRAY_SIZE(common_modes);
}

static const struct drm_connector_helper_funcs gf_kms_virt_conn_helper_funcs = {
	.get_modes    = gf_kms_virt_conn_get_modes,
};

static const struct drm_plane_funcs gf_kms_virt_plane_funcs = {
	.update_plane		= drm_atomic_helper_update_plane,
	.disable_plane		= drm_atomic_helper_disable_plane,
	.destroy		= drm_plane_cleanup,
	.reset			= drm_atomic_helper_plane_reset,
	.atomic_duplicate_state = drm_atomic_helper_plane_duplicate_state,
	.atomic_destroy_state	= drm_atomic_helper_plane_destroy_state,
};
	
#if DRM_VERSION_CODE >= KERNEL_VERSION(5,15,0)
static void gf_kms_virt_plane_atomic_update(struct drm_plane *plane,
					    struct drm_atomic_state *old_state)
#else
static void gf_kms_virt_plane_atomic_update(struct drm_plane *plane,
					    struct drm_plane_state *old_state)
#endif
{
	return;
}

#if DRM_VERSION_CODE >= KERNEL_VERSION(5,15,0)
static int gf_kms_virt_plane_atomic_check(struct drm_plane *plane,
					  struct drm_atomic_state *state)
#else
static int gf_kms_virt_plane_atomic_check(struct drm_plane *plane,
					  struct drm_plane_state *state)
#endif
{
	struct drm_plane_state *new_plane_state = 
#if DRM_VERSION_CODE >= KERNEL_VERSION(5,15,0)
		drm_atomic_get_new_plane_state(state, plane);
#else
		state;
#endif

	struct drm_crtc_state *crtc_state;
	int ret;

	if (!new_plane_state->fb || WARN_ON(!new_plane_state->crtc))
		return 0;

#if DRM_VERSION_CODE >= KERNEL_VERSION(5,15,0)
		crtc_state = drm_atomic_get_crtc_state(state, new_plane_state->crtc);
#else
		crtc_state = drm_atomic_get_crtc_state(new_plane_state->state, new_plane_state->crtc);
#endif

	if (IS_ERR(crtc_state))
		return PTR_ERR(crtc_state);

	ret = drm_atomic_helper_check_plane_state(new_plane_state, crtc_state,
						  DRM_PLANE_HELPER_NO_SCALING,
						  DRM_PLANE_HELPER_NO_SCALING,
						  false, true);
	if (ret != 0)
		return ret;

	/* for now primary plane must be visible and full screen */
	if (!new_plane_state->visible)
		return -EINVAL;

	return 0;
}

static int gf_kms_virt_prepare_fb(struct drm_plane *plane,
				  struct drm_plane_state *new_state)
{
	return 0;
}

static void gf_kms_virt_cleanup_fb(struct drm_plane *plane,
				   struct drm_plane_state *old_state)
{
	return;
}

static const struct drm_plane_helper_funcs gf_kms_virt_primary_helper_funcs = {
	.atomic_update		= gf_kms_virt_plane_atomic_update,
	.atomic_check		= gf_kms_virt_plane_atomic_check,
	.prepare_fb		= gf_kms_virt_prepare_fb,
	.cleanup_fb		= gf_kms_virt_cleanup_fb,
};

static struct drm_plane *gf_kms_virt_plane_init(struct drm_device *dev,
						enum drm_plane_type type,
						int index)
{
	struct drm_plane *plane;
	int ret;

	plane = kzalloc(sizeof(*plane), GFP_KERNEL);
	if (!plane)
		return ERR_PTR(-ENOMEM);

	ret = drm_universal_plane_init(dev, plane, 1 << index,
				       &gf_kms_virt_plane_funcs,
				       gf_kms_virt_formats,
				       ARRAY_SIZE(gf_kms_virt_formats),  
				       NULL, type, NULL);
	if (ret) {
		kfree(plane);
		return ERR_PTR(ret);
	}

	drm_plane_helper_add(plane, &gf_kms_virt_primary_helper_funcs);

	return plane;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 7, 0)
static const struct drm_encoder_funcs gf_kms_virt_encoder_funcs = {
		.destroy = drm_encoder_cleanup,
	};
#endif


static int gf_kms_virt_output_init(struct drm_device *dev,
			    struct display_adapter *adapter)
{
	struct drm_plane *primary = NULL, *cursor = NULL;
	struct drm_crtc *crtc = NULL;
	struct drm_connector *connector = NULL;
	struct drm_encoder *encoder = NULL;
	int i = 0;
	int ret;

	adapter->crtc_num = VIRT_CRTC_NUM;
	adapter->connector_num = VIRT_CONNECTOR_NUM;

	adapter->crtcs = gf_calloc(adapter->crtc_num*sizeof(struct crtc_t));
	adapter->encoders = gf_calloc(VIRT_ENCODER_NUM*sizeof(struct encoder_t));
	adapter->connectors = gf_calloc(adapter->connector_num*sizeof(struct connector_t));
	

	for (i = 0; i <adapter->crtc_num; i++) {
		adapter->crtcs[i].planes = gf_calloc(sizeof(struct plane_t));
		primary = &adapter->crtcs[i].planes->base;
		
		primary = gf_kms_virt_plane_init(dev, DRM_PLANE_TYPE_PRIMARY, i);
		if (IS_ERR(primary))
			return PTR_ERR(primary);
		crtc = &adapter->crtcs[i].base;
		
		ret = gf_kms_virt_crtc_init(dev, adapter, crtc, primary, cursor, i);
		if (ret)
			goto err_crtc;
	}
	
	drm_vblank_init(dev, adapter->crtc_num);

	for (i = 0; i < adapter->connector_num; i++) {
		connector = &adapter->connectors[i].base;
		encoder = &adapter->encoders[i].base;
		
		ret = drm_connector_init(dev, connector, &gf_kms_virt_connector_funcs,
				DRM_MODE_CONNECTOR_VIRTUAL);
		if (ret) {
			gf_error("Failed to init connector\n");
			goto err_connector;
		}
		
		drm_connector_helper_add(connector, &gf_kms_virt_conn_helper_funcs);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 7, 0)
		ret = drm_simple_encoder_init(dev, encoder, DRM_MODE_ENCODER_VIRTUAL);
#else
		ret = drm_encoder_init(dev, encoder, &gf_kms_virt_encoder_funcs,
			DRM_MODE_ENCODER_VIRTUAL, NULL);
#endif

		if (ret) {
			gf_error("Failed to init encoder\n");
			goto err_encoder;
		}

		ret = drm_connector_attach_encoder(connector, encoder);
		if (ret) {
			gf_error("Failed to attach connector to encoder\n");
			goto err_attach;
		}
		encoder->possible_crtcs = 1 << i;
	}
	
	drm_mode_config_reset(dev);

	return 0;

err_attach:
	drm_encoder_cleanup(encoder);

err_encoder:
	drm_connector_cleanup(connector);

err_connector:
	drm_crtc_cleanup(crtc);
    
err_crtc:
	drm_plane_cleanup(primary);

	return ret;
}

static int gf_kms_virt_output_deinit(struct display_adapter *adapter) 
{
	int i = 0;
	int ret = 0;

	if (adapter->crtcs) {
		for (i=0; i < adapter->crtc_num; i++) {
			if (adapter->crtcs[i].planes) {
				gf_free(adapter->crtcs[i].planes);
				adapter->crtcs[i].planes = NULL;
			}
		}

		gf_free(adapter->crtcs);
		adapter->crtcs = NULL;
	}

	if (adapter->connectors) {
		gf_free(adapter->connectors);
		adapter->connectors = NULL;
	}

	if (adapter->encoders) {
		gf_free(adapter->encoders);
		adapter->encoders = NULL;
	}

	return ret;			
}


const struct drm_mode_config_funcs gf_kms_virt_mode_funcs = {
	.fb_create = drm_gem_fb_create,
	.atomic_check = drm_atomic_helper_check,
	.atomic_commit = drm_atomic_helper_commit,
};

int gf_kms_virt_init(struct lisuan_device *gdevice)
{
	struct display_adapter *adapter;
	struct drm_device *ddev = gdevice->ddev;
	int ret = 0;

	gdevice->adapter = gf_calloc(sizeof(struct display_adapter));
	adapter = gdevice->adapter;
	if (!adapter)
		return -1;

	drm_mode_config_init(ddev);
	ddev->mode_config.max_width = XRES_MAX;
	ddev->mode_config.max_height = YRES_MAX;
	ddev->mode_config.funcs = &gf_kms_virt_mode_funcs;

#if DRM_VERSION_CODE >= KERNEL_VERSION(6,6,0)
	ddev->mode_config.fb_modifiers_not_supported = true;
#endif

	gf_kms_virt_output_init(ddev, adapter);
	
	/* We need to coordinate the hotplugs with the asynchronous */
	drm_kms_helper_poll_init(ddev);

	drm_mode_config_reset(ddev);

	return ret;
}

int gf_kms_virt_deinit(struct lisuan_device *gdevice)
{
	struct drm_device *ddev = gdevice->ddev;
	struct display_adapter *adapter = gdevice->adapter;
	int ret = 0;

	drm_kms_helper_poll_fini(gdevice->ddev);

	gf_kms_virt_output_deinit(adapter);

	gf_free(adapter);
	gdevice->adapter = NULL;

	return ret;
}

