#ifndef _VCX_VCMD_DRIVER_H
#define _VCX_VCMD_DRIVER_H
#ifdef __cplusplus
extern "C" {
#endif

#include "lisuan_device.h"

/******************************************************************************
 * VCMD Command Definitions
 ******************************************************************************/
#define UNSIGNED_OPCODE(A)		  ((u32)(A))
#define OPCODE_MASK				  (UNSIGNED_OPCODE(0x1F) << 27)
#define OPCODE_WREG               (UNSIGNED_OPCODE(0x01) << 27)
#define OPCODE_END                (UNSIGNED_OPCODE(0x02) << 27)
#define OPCODE_NOP                (UNSIGNED_OPCODE(0x03) << 27)
#define OPCODE_RREG               (UNSIGNED_OPCODE(0x16) << 27)
#define OPCODE_INT                (UNSIGNED_OPCODE(0x18) << 27)
#define OPCODE_JMP                (UNSIGNED_OPCODE(0x19) << 27)
#define OPCODE_STALL              (UNSIGNED_OPCODE(0x09) << 27)
#define OPCODE_CLRINT             (UNSIGNED_OPCODE(0x1a) << 27)
#define OPCODE_JMP_RDY0           (UNSIGNED_OPCODE(0x19) << 27)
#define OPCODE_JMP_RDY1           ((UNSIGNED_OPCODE(0x19) << 27) | (1 << 26))
#define OPCODE_M2M                (UNSIGNED_OPCODE(0x1b) << 27)
#define OPCODE_MSET               (UNSIGNED_OPCODE(0x1c) << 27)
#define OPCODE_M2MP               (UNSIGNED_OPCODE(0x1d) << 27)
#define JMP_IE_1                  BIT(25)
#define JMP_RDY_1                 BIT(26)

int xx_HANTRO_IOCH_GET_VCMD_ENABLE_vcmd(struct drm_device *dev, void *data,
		       struct drm_file *file_priv);

int xx_HANTRO_IOCH_GET_MMU_ENABLE_vcmd(struct drm_device *dev, void *data,
		       struct drm_file *file_priv);

int xx_HANTRO_IOCH_GET_CMDBUF_PARAMETER(struct drm_device *dev, void *data,
		       struct drm_file *file_priv);

int xx_HANTRO_IOCH_GET_VCMD_PARAMETER(struct drm_device *dev, void *data,
		       struct drm_file *file_priv);

int xx_HANTRO_IOCH_RESERVE_CMDBUF(struct drm_device *dev, void *data,
		       struct drm_file *file_priv);

int xx_HANTRO_IOCH_LINK_RUN_CMDBUF(struct drm_device *dev, void *data,
		       struct drm_file *file_priv);

int xx_HANTRO_IOCH_WAIT_CMDBUF(struct drm_device *dev, void *data,
		       struct drm_file *file_priv);

int xx_HANTRO_IOCH_RELEASE_CMDBUF(struct drm_device *dev, void *data,
		       struct drm_file *file_priv);

int xx_HANTRO_IOCH_POLLING_CMDBUF(struct drm_device *dev, void *data,
		       struct drm_file *file_priv);

int hantrovcmd_open(struct lisuan_device *gf_device, struct drm_file *filp);
int hantrovcmd_release(struct lisuan_device *gf_device, struct drm_file *filp);
int hantroenc_vcmd_init(struct lisuan_device *gf_device);
void hantroenc_vcmd_cleanup(struct lisuan_device *gf_device);
void hantro_enc_resume(struct lisuan_device *gf_device);
int hantroenc_vcmd_isr(int irq, void *vvcmd_mgr, int is_internal);
void lisuan_enc_vcmd_clear_irq_status(int irq, void *vvcmd_mgr);
void lisuan_enc0_vcmd_worker(struct work_struct *work);
void lisuan_im_vcmd_worker(struct work_struct *work);
int gf_enc_link_and_run(struct drm_device *dev, void *data, struct drm_file *file_priv, unsigned long long ring_fence_value, int procid);
#ifdef __cplusplus
}
#endif
#endif