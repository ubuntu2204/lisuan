/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#ifndef __LISUAN_FB_H_
#define __LISUAN_FB_H_

#include <drm/drm_framebuffer.h>

struct lisuan_device;
struct lisuan_fbdev {
	struct lisuan_device *device;
	struct drm_fb_helper helper;
	struct drm_client_buffer *buffer;
	struct drm_framebuffer base;
};
int lisuanfb_probe(struct drm_fb_helper *helper, struct drm_fb_helper_surface_size *sizes);
int lisuan_fbdev_init(struct lisuan_device *gfdev);
void lisuan_fbdev_fini(struct lisuan_device *gfdev);

#endif
