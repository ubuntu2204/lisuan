/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <linux/vga_switcheroo.h>
#include <linux/version.h>

#include <drm/amdgpu_drm.h>
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_print.h>
#include <drm/drm_gem.h>
#include <drm/drm_modeset_helper.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0)
#include <drm/drm_damage_helper.h>
#endif
#include "lisuan_drv.h"
#include "lisuan_device.h"
#include "lisuan_fb.h"

static struct drm_client_buffer *lisuan_framebuffer_init(struct drm_fb_helper *fb_helper, struct drm_fb_helper_surface_size *sizes,
							struct drm_mode_fb_cmd2 *mode_cmd)
{
#if DRM_VERSION_CODE >= KERNEL_VERSION(6,19,0)
	return  drm_client_buffer_create_dumb(&fb_helper->client, sizes->surface_width, sizes->surface_height, mode_cmd->pixel_format);
#else
	return drm_client_framebuffer_create(&fb_helper->client, sizes->surface_width, sizes->surface_height, mode_cmd->pixel_format);
#endif
}

#if DRM_VERSION_CODE >= KERNEL_VERSION(6,6,0)
static void drm_fb_helper_cfb_fillrect_self(struct fb_info *info,
				const struct fb_fillrect *rect)
{
	cfb_fillrect(info, rect);
	drm_fb_helper_damage_area(info, rect->dx, rect->dy, rect->width, rect->height);
}

static void drm_fb_helper_cfb_copyarea_self(struct fb_info *info,
				const struct fb_copyarea *area)
{
	cfb_copyarea(info, area);
	drm_fb_helper_damage_area(info, area->dx, area->dy, area->width, area->height);
}

static void drm_fb_helper_cfb_imageblit_self(struct fb_info *info,
				 const struct fb_image *image)
{
	cfb_imageblit(info, image);
	drm_fb_helper_damage_area(info, image->dx, image->dy, image->width, image->height);
}
#endif			 

static struct fb_ops lisuanfb_ops = {
	.owner = THIS_MODULE,
	DRM_FB_HELPER_DEFAULT_OPS,
#if DRM_VERSION_CODE >= KERNEL_VERSION(6,6,0)
	.fb_fillrect = drm_fb_helper_cfb_fillrect_self,
	.fb_copyarea = drm_fb_helper_cfb_copyarea_self,
	.fb_imageblit = drm_fb_helper_cfb_imageblit_self,
#else
	.fb_fillrect = drm_fb_helper_cfb_fillrect,
	.fb_copyarea = drm_fb_helper_cfb_copyarea,
	.fb_imageblit = drm_fb_helper_cfb_imageblit,
#endif
};

static const struct drm_framebuffer_funcs lisuan_fb_funcs = {
	.destroy = drm_gem_fb_destroy,
	.create_handle = drm_gem_fb_create_handle,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,0,0)
	.dirty = drm_atomic_helper_dirtyfb,
#endif
};


int lisuanfb_probe(struct drm_fb_helper *helper, struct drm_fb_helper_surface_size *sizes)
{
	struct drm_device *ddev = helper->dev;
	struct lisuan_device *gfdev = (struct lisuan_device *)ddev->dev_private;
	struct lisuan_fbdev *rfbdev = gfdev->gfbdev;
	struct fb_info *info;
	struct drm_framebuffer *fb = NULL;
	struct drm_mode_fb_cmd2 mode_cmd;
	int ret;
#if DRM_VERSION_CODE >= KERNEL_VERSION(6,17,0)
	const struct drm_format_info *format_info;
#endif

	mode_cmd.width = sizes->surface_width;
	mode_cmd.height = sizes->surface_height;
	mode_cmd.pitches[0] = ALIGN(mode_cmd.width * DIV_ROUND_UP(sizes->surface_bpp, 8), 64);
	mode_cmd.pixel_format = drm_mode_legacy_fb_format(sizes->surface_bpp, sizes->surface_depth);

#if DRM_VERSION_CODE >= KERNEL_VERSION(6,19,0)
	info = helper->info;
#elif DRM_VERSION_CODE >= KERNEL_VERSION(6,6,0)
	info = drm_fb_helper_alloc_info(helper);
#else
	info = drm_fb_helper_alloc_fbi(helper);
#endif
	if (IS_ERR(info)) {
		ret = PTR_ERR(info);
		return ret;
	}

	rfbdev->buffer = lisuan_framebuffer_init(helper, sizes, &mode_cmd);
	if (!rfbdev->buffer) {
		gf_error("fbdev: internal buffer create fail\n");
		return -1;
	}

	rfbdev->base.obj[0] = rfbdev->buffer->gem;
#if DRM_VERSION_CODE >= KERNEL_VERSION(6,17,0)
	format_info = drm_get_format_info(ddev, mode_cmd.pixel_format,
							    mode_cmd.modifier[0]);
	drm_helper_mode_fill_fb_struct(ddev,&rfbdev->base,format_info,&mode_cmd);
#else
	drm_helper_mode_fill_fb_struct(ddev,&rfbdev->base,&mode_cmd);
#endif
	ret = drm_framebuffer_init(ddev, &rfbdev->base, &lisuan_fb_funcs);
	if(ret)
	{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,6,0)
		drm_dbg_kms(ddev, "Failed to init gem fb:%d\n",ret);
#else
		drm_dbg(DRM_UT_DP, "Failed to init gem fb:%d\n",ret);
#endif

		rfbdev->base.obj[0]=NULL;
		return ret;
	}
	fb = rfbdev->buffer->fb;
	helper->fb  = fb;
	info->fbops = &lisuanfb_ops;
	info->screen_size = fb->height * fb->pitches[0];
	info->fix.smem_len = info->screen_size;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,4,0)
	drm_fb_helper_fill_info(info, helper, sizes);
#else
	info->par = helper;
	drm_fb_helper_fill_fix(info, fb->pitches[0], fb->format->depth);
	drm_fb_helper_fill_var(info, helper, sizes->surface_depth, sizes->surface_bpp);
#endif

	info->screen_buffer = lisuan_gem_vmap(rfbdev->buffer->gem);
	info->screen_base = info->screen_buffer;

	//clear a black screen after vmap, or garbage was left in there.
	if (info->screen_buffer) {
		gf_memset(info->screen_buffer, 0, info->screen_size);
	}

	return 0;
}


#if DRM_VERSION_CODE >= KERNEL_VERSION(6,6,0)
static int lisuanfb_dirty(struct drm_fb_helper *helper, struct drm_clip_rect *clip)
{
	if (!(clip->x1 < clip->x2 && clip->y1 < clip->y2))
		return 0;

	if (helper->fb->funcs->dirty)
		return helper->fb->funcs->dirty(helper->fb, NULL, 0, 0, clip, 1);

	return 0;
}
#endif

static const struct drm_fb_helper_funcs lisuan_fb_helper_funcs = {
#if DRM_VERSION_CODE < KERNEL_VERSION(6,12,0)
	.fb_probe = lisuanfb_probe,
#endif

#if DRM_VERSION_CODE >= KERNEL_VERSION(6,6,0)
	.fb_dirty = lisuanfb_dirty,
#endif
};

#if DRM_VERSION_CODE >= KERNEL_VERSION(5,7,0)
static void lisuan_fbdev_generic_client_unregister(struct drm_client_dev *client)
{
	struct drm_fb_helper *fb_helper = drm_fb_helper_from_client(client);
#if DRM_VERSION_CODE >= KERNEL_VERSION(6,2,1)
	if (fb_helper->info) {
		drm_fb_helper_unregister_info(fb_helper);
	} else {
		drm_client_release(&fb_helper->client);
		drm_fb_helper_unprepare(fb_helper);
		kfree(fb_helper);
	}
#elif DRM_VERSION_CODE < KERNEL_VERSION(6,2,0)
	if (fb_helper)
		drm_fb_helper_unregister_fbi(fb_helper);
#else
	if (fb_helper && fb_helper->fbdev)
		unregister_framebuffer(fb_helper->fbdev);
#endif
}
#if DRM_VERSION_CODE >= KERNEL_VERSION(6,19,0)
static int lisuan_fbdev_generic_client_restore(struct drm_client_dev *client, bool force)
#else
static int lisuan_fbdev_generic_client_restore(struct drm_client_dev *client)
#endif
{
#if DRM_VERSION_CODE >= KERNEL_VERSION(6,19,0)
	struct drm_fb_helper *fb_helper = drm_fb_helper_from_client(client);
	drm_fb_helper_restore_fbdev_mode_unlocked(fb_helper, force);
#else
	drm_fb_helper_lastclose(client->dev);
#endif
	return 0;
}

static int lisuan_fbdev_generic_client_hotplug(struct drm_client_dev *client)
{
	struct drm_fb_helper *fb_helper = drm_fb_helper_from_client(client);
	struct drm_device *dev = client->dev;
	int ret;

	if (dev->fb_helper)
		return drm_fb_helper_hotplug_event(dev->fb_helper);

	ret = drm_fb_helper_init(dev, fb_helper);
	if (ret)
		goto err_drm_err;

	if (!drm_drv_uses_atomic_modeset(dev))
		drm_helper_disable_unused_functions(dev);
#if DRM_VERSION_CODE >= KERNEL_VERSION(6,3,0)
	ret = drm_fb_helper_initial_config(fb_helper);
#else
	ret = drm_fb_helper_initial_config(fb_helper, 32);
#endif
	if (ret)
		goto err_drm_fb_helper_fini;

	return 0;

err_drm_fb_helper_fini:
	drm_fb_helper_fini(fb_helper);
err_drm_err:
	drm_err(dev, "fbdev: Failed to setup generic emulation (ret=%d)\n", ret);
	return ret;
}

static const struct drm_client_funcs lisuan_fbdev_client_funcs = {
	.owner		= THIS_MODULE,
	.unregister	= lisuan_fbdev_generic_client_unregister,
	.restore	= lisuan_fbdev_generic_client_restore,
	.hotplug	= lisuan_fbdev_generic_client_hotplug,
};
#endif

int lisuan_fbdev_init(struct lisuan_device *gfdev)
{
	int ret=0;
	int bpp_sel = 32;
	struct lisuan_fbdev *gfbdev = kzalloc(sizeof(struct lisuan_fbdev), GFP_KERNEL);
	if (!gfbdev)
		return -ENOMEM;
	gfbdev->device = gfdev;
	gfdev->gfbdev = gfbdev;

#if DRM_VERSION_CODE >= KERNEL_VERSION(6,6,0)
	drm_fb_helper_prepare(gfdev->ddev, &gfbdev->helper, bpp_sel, &lisuan_fb_helper_funcs);
#else
	drm_fb_helper_prepare(gfdev->ddev, &gfbdev->helper, &lisuan_fb_helper_funcs);
#endif

#if DRM_VERSION_CODE >= KERNEL_VERSION(5,7,0)
	ret = drm_fb_helper_init(gfdev->ddev, &gfbdev->helper);
	ret = drm_client_init(gfdev->ddev, &gfbdev->helper.client, "lisuan_fb_helper", &lisuan_fbdev_client_funcs);
	drm_client_register(&gfbdev->helper.client);
#elif DRM_VERSION_CODE >= KERNEL_VERSION(5,0,0)
	ret = drm_fb_helper_init(gfdev->ddev, &gfbdev->helper, gfdev->adapter->connector_num);
	ret = drm_client_init(gfdev->ddev, &gfbdev->helper.client, "drm_fb_helper", NULL);
	drm_fb_helper_single_add_all_connectors(&gfbdev->helper);
#else
	ret = drm_fb_helper_init(gfdev->ddev, &gfbdev->helper, gfdev->adapter->crtc_num);
	ret = drm_client_init(gfdev->ddev, &gfbdev->helper.client, "drm_fb_helper", NULL);
	drm_fb_helper_single_add_all_connectors(&gfbdev->helper);
#endif
	

	if (ret) {
		gf_error("drm_fb_helper_init error ret = %d\n", ret);
		kfree(gfbdev);
		return ret;
	}

#if DRM_VERSION_CODE >= KERNEL_VERSION(6,3,0)
	drm_fb_helper_initial_config(&gfbdev->helper);
#else
	drm_fb_helper_initial_config(&gfbdev->helper, bpp_sel);
#endif
	return 0;

}

static int lisuan_fbdev_destroy(struct drm_device *dev, struct lisuan_fbdev *rfbdev)
{
	int i;
#if DRM_VERSION_CODE >= KERNEL_VERSION(6,6,0)
	drm_fb_helper_unregister_info(&rfbdev->helper);
#else
	drm_fb_helper_unregister_fbi(&rfbdev->helper);
#endif

	if (rfbdev->base.obj[0]) {
#if DRM_VERSION_CODE >= KERNEL_VERSION(6,6,0)
		iounmap(rfbdev->helper.info->screen_buffer);
#else
		iounmap(rfbdev->helper.fbdev->screen_buffer);
#endif
		for (i = 0; i < rfbdev->base.format->num_planes; i++)
			drm_gem_object_put_compat(rfbdev->base.obj[0]);
		rfbdev->base.obj[0] = NULL;
		drm_framebuffer_unregister_private(&rfbdev->base);
		drm_framebuffer_cleanup(&rfbdev->base);
	}
	drm_fb_helper_fini(&rfbdev->helper);

	return 0;
}

void lisuan_fbdev_fini(struct lisuan_device *gfdev)
{
	if (!gfdev->gfbdev)
		return;

	lisuan_fbdev_destroy(gfdev->ddev, gfdev->gfbdev);
	kfree(gfdev->gfbdev);
	gfdev->gfbdev = NULL;

}
