#ifndef _HANTRO_NORMAL_DRIVER_H
#define _HANTRO_NORMAL_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lisuan_device.h"

int hantroenc_normal_init(struct lisuan_device *gf_device);
void hantroenc_normal_cleanup(struct lisuan_device *gf_device);
int hantroenc_open(struct lisuan_device *gf_device, struct drm_file *filp);
int hantroenc_release(struct lisuan_device *gf_device, struct drm_file *filp);

int hantroenc_isr(void *vdev);

int xx_HANTRO_IOCH_GET_VCMD_ENABLE_normal(struct drm_device *dev, void *data,
			struct drm_file *file_priv);

int xx_HANTRO_IOCH_GET_MMU_ENABLE_normal(struct drm_device *dev, void *data,
			struct drm_file *file_priv);

int xx_HANTRO_IOCG_HWOFFSET(struct drm_device *dev, void *data,
		       struct drm_file *file_priv);

int xx_HANTRO_IOCG_HWIOSIZE(struct drm_device *dev, void *data,
		       struct drm_file *file_priv);

int xx_HANTRO_IOCG_SRAMOFFSET(struct drm_device *dev, void *data,
		       struct drm_file *file_priv);

int xx_HANTRO_IOCG_SRAMEIOSIZE(struct drm_device *dev, void *data,
		       struct drm_file *file_priv);

int xx_HANTRO_IOCG_CORE_NUM(struct drm_device *dev, void *data,
		       struct drm_file *file_priv);

int xx_HANTRO_IOCG_CORE_INFO(struct drm_device *dev, void *data,
		       struct drm_file *file_priv);

int xx_HANTRO_IOCH_ENC_RESERVE(struct drm_device *dev, void *data,
		       struct drm_file *file_priv);

int xx_HANTRO_IOCH_ENC_RELEASE(struct drm_device *dev, void *data,
		       struct drm_file *file_priv);

int xx_HANTRO_IOCG_CORE_WAIT(struct drm_device *dev, void *data,
		       struct drm_file *file_priv);

int xx_HANTRO_IOCG_ANYCORE_WAIT_POLLING(struct drm_device *dev, void *data,
		       struct drm_file *file_priv);

int xx_HANTRO_IOCG_ANYCORE_WAIT(struct drm_device *dev, void *data,
		       struct drm_file *file_priv);

#ifdef __cplusplus
}
#endif
#endif