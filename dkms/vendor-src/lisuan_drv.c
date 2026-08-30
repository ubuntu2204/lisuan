/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#include <drm/drm_ioctl.h>
#include <drm/drm_drv.h>
#include <linux/pci.h>
#include <linux/dma-buf.h>
#include <drm/drm_fb_helper.h>
#include <linux/iommu.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/vgaarb.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <drm/drm_vblank.h>
#include <drm/drm_modeset_helper.h>
#include <drm/drm_atomic_helper.h>

#include "lisuan_scheduler.h"
#include "lisuan_drm.h"
#include "lisuan_context.h"
#include "lisuan_sync.h"
#include "lisuan_device.h"
#include "lisuan_gem.h"
#include "lisuan_mailbox.h"
#include "lisuan_vm.h"
#include "lisuan_drv.h"
#include "lisuan_procid.h"
#include "gf_os_interface.h"
#include "lisuan_ring.h"
#include "gf_irq.h"
#include "lisuan_fb.h"
#include "gf_debugfs.h"
#include "gf_hw_misc.h"
#include "gf_ringbuffer.h"
#include "gf_kms_virt.h"
#include "gf_kmd_event.h"
#include "log_control.h"
#include "gf_kms_dm.h"
#include "VERSION"

#include "subsys.h"
//#include "vcx_driver.h"
#include "vcx_normal_driver.h"
#include "vcx_vcmd_driver.h"

#ifndef DRM_VERSION_CODE
#define DRM_VERSION_CODE LINUX_VERSION_CODE
#endif

#if DRM_VERSION_CODE >= KERNEL_VERSION(6,12,0)
#include <linux/aperture.h>
#elif DRM_VERSION_CODE >= KERNEL_VERSION(5,15,0)
#include <drm/drm_aperture.h>
#endif

unsigned int print_level = 2;
module_param(print_level, int, 0600);
MODULE_PARM_DESC(print_level, "print debug info level");

int fbdev_enable = 1;
module_param(fbdev_enable, int, 0600);
MODULE_PARM_DESC(fbdev_enable, "enable fb dev");

int bypass_only = 0;
module_param(bypass_only, int, 0600);
MODULE_PARM_DESC(bypass_only, "enable bypass only");

unsigned int mhb_disable_mask = 0x0;//0x3E
module_param(mhb_disable_mask, int, 0600);
MODULE_PARM_DESC(mhb_disable_mask, "mhb disable mask");

unsigned int cluster_slice_mask = 0xFFFFFF;
module_param(cluster_slice_mask, int, 0600);
MODULE_PARM_DESC(cluster_slice_mask, "cluster slice mask");

int recovery_enable = 1;
module_param(recovery_enable, int, 0600);
MODULE_PARM_DESC(recovery_enable, "recover hw when hw hang");

unsigned int enable_clock_gating = 1;
module_param(enable_clock_gating, int, 0600);
MODULE_PARM_DESC(enable_clock_gating, "clock gating contrl");

int sgnt_enable = 0;
module_param(sgnt_enable, int, 0600);
MODULE_PARM_DESC(sgnt_enable, "dpe sgnt intr control");

unsigned int no_gfx = 0;
module_param(no_gfx, int, 0600);
MODULE_PARM_DESC(no_gfx, "no gfx");

unsigned int no_gce = 0;
module_param(no_gce, int, 0600);
MODULE_PARM_DESC(no_gce, "no gce");

unsigned int no_vem = 0;
module_param(no_vem, int, 0600);
MODULE_PARM_DESC(no_vem, "no vem");

unsigned int no_video = 0;
module_param(no_video, int, 0600);
MODULE_PARM_DESC(no_video, "no video");

unsigned int cpu_update_pte = 0;
module_param(cpu_update_pte, int, 0600);
MODULE_PARM_DESC(cpu_update_pte, "cpu update pte");

unsigned int paging_rb = 1;
module_param(paging_rb, int, 0600);
MODULE_PARM_DESC(paging_rb, "paging rb");

unsigned int dump_rb_cmd = 0;
module_param(dump_rb_cmd, int, 0600);
MODULE_PARM_DESC(dump_rb_cmd, "dump rb cmd");

unsigned int skip_clr = 0;
module_param(skip_clr, int, 0600);
MODULE_PARM_DESC(skip_clr, "skip kmd allocation clear");

unsigned int enable_gl2 = 0;
module_param(enable_gl2, int, 0600);
MODULE_PARM_DESC(enable_gl2, "enable kmd cmd gl2");

unsigned int validation_chip = 0;
module_param(validation_chip, int, 0600);
MODULE_PARM_DESC(validation_chip, "validation chip");

unsigned int dp_cts = 0;
module_param(dp_cts, int, 0600);
MODULE_PARM_DESC(dp_cts, "dp cts test");

unsigned int display_cts = 0;
module_param(display_cts, int, 0600);
MODULE_PARM_DESC(display_cts, "pipe cts test");

unsigned int vram_only = 0;
module_param(vram_only, int, 0600);
MODULE_PARM_DESC(vram_only, "use vram only");

unsigned int pz1 = 0;
module_param(pz1, int, 0600);
MODULE_PARM_DESC(pz1, "pz1 config");

unsigned int no_encoder = 0;
module_param(no_encoder, int, 0600);
MODULE_PARM_DESC(no_encoder, "no encoder");

unsigned int mode_8k = 0;
module_param(mode_8k, int, 0600);
MODULE_PARM_DESC(mode_8k, "mode 8k");

unsigned int fake_monitor_mask = 0;
module_param(fake_monitor_mask, int, 0600);
MODULE_PARM_DESC(fake_monitor_mask, "fake monitor mask");

unsigned int lisuan_debug = 0;
module_param(lisuan_debug, int, 0600);
MODULE_PARM_DESC(lisuan_debug, "Enable debug output, where each bit enables a debug category.\n"
"\t\tBit 0 (0x01)  will enable VM_DEBUG\n"
"\t\tBit 1 (0x02)  will enable VMID_DEBUG\n"
"\t\tBit 2 (0x04)  will enable GEM_DEBUG\n"
"\t\tBit 3 (0x08)  will enable JOB_DEBUG\n"
"\t\tBit 4 (0x10)  will enable PM_DEBUG\n"
"\t\tBit 5 (0x20)  will enable MB_DEBUG\n"
"\t\tBit 6 (0x40)  will enable DMA_FENCE\n");

unsigned int endebug = 0;
module_param(endebug, int, 0600);
MODULE_PARM_DESC(endebug, "enable debug mode");


unsigned int en_new_cl_clr = 0;
module_param(en_new_cl_clr, int, 0600);
MODULE_PARM_DESC(en_new_cl_clr, "enable new cl clear");

static int xx_HANTRO_IOCH_GET_VCMD_ENABLE(struct drm_device *dev, void *data,
		       struct drm_file *file_priv)
{
	if (0)
		return xx_HANTRO_IOCH_GET_VCMD_ENABLE_normal(dev, data, file_priv);
	else
		return xx_HANTRO_IOCH_GET_VCMD_ENABLE_vcmd(dev, data, file_priv);
}

static int xx_HANTRO_IOCH_GET_MMU_ENABLE(struct drm_device *dev, void *data,
		       struct drm_file *file_priv)
{
	if (0)
		return xx_HANTRO_IOCH_GET_MMU_ENABLE_normal(dev, data, file_priv);
	else
		return xx_HANTRO_IOCH_GET_MMU_ENABLE_vcmd(dev, data, file_priv);
}

struct drm_ioctl_desc lisuan_ioctls_drm[] = {
	DRM_IOCTL_DEF_DRV(GF_QUERY_CARD_INFO, lisuan_query_card_info_ioctl, DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(GF_GEM_CREATE, lisuan_gem_create_ioctl, DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(GF_GEM_MAP_VA, lisuan_gem_map_gpuva_ioctl, DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(GF_GEM_FREE_VA, lisuan_gem_free_gpuva_ioctl, DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(GF_GEM_MAKE_RESIDENT, lisuan_make_resident_ioctl, DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(GF_CTX_ALLOC, lisuan_ctx_alloc_ioctl, DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(GF_CTX_FREE, lisuan_ctx_free_ioctl, DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(GF_SYNC_CREATE, lisuan_sync_create_ioctl, DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(GF_CS, lisuan_cs_ioctl, DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(GF_SYNC_SIGNAL, lisuan_sync_signal_ioctl, DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(GF_SYNC_WAIT, lisuan_sync_wait_ioctl, DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(GF_SYNC_DESTROY, lisuan_sync_destroy_ioctl, DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(GF_GEM_MAP_OFFSET, lisuan_gem_map_offset_ioctl, DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(GF_GET_CELLINDEX, lisuan_get_cellindex_ioctl, DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(GF_GEM_QUERY_INFOS, lisuan_gem_query_infos_ioctl, DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(GF_RESERVE_GPUVA, lisuan_sparse_reserve_gpuva_ioctl, DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(GF_UPDATE_GPUVA, lisuan_sparse_update_gpuva_ioctl, DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(GF_QUERY_CLOCK_CALIBRATION, lisuan_query_clock_calibration_ioctl, DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(GF_GET_KMDFMT, lisuan_get_kmdfmt_ioctl, DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(GF_GEM_IS_BUSY, lisuan_gem_is_busy_ioctl, DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(GF_ALLOC_CELL_INDEX, lisuan_alloc_cell_index_ioctl, DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(GF_QUERY_VRAM_INFO, lisuan_query_vram_info_ioctl, DRM_AUTH | DRM_RENDER_ALLOW),


	DRM_IOCTL_DEF_DRV(GF_GET_VEM_INFO, lisuan_get_vem_info_ioctl, DRM_AUTH | DRM_RENDER_ALLOW),

	DRM_IOCTL_DEF_DRV(HANTRO_VCMD_IOCH_GET_CMDBUF_PARAMETER, xx_HANTRO_VCMD_IOCH_GET_CMDBUF_PARAMETER, DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(HANTRO_VCMD_IOCH_GET_VCMD_PARAMETER, xx_HANTRO_VCMD_IOCH_GET_VCMD_PARAMETER, DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(HANTRO_VCMD_IOCH_RESERVE_CMDBUF, xx_HANTRO_VCMD_IOCH_RESERVE_CMDBUF, DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(HANTRO_VCMD_IOCH_LINK_RUN_CMDBUF, xx_HANTRO_VCMD_IOCH_LINK_RUN_CMDBUF, DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(HANTRO_VCMD_IOCH_WAIT_CMDBUF, xx_HANTRO_VCMD_IOCH_WAIT_CMDBUF, DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(HANTRO_VCMD_IOCH_RELEASE_CMDBUF, xx_HANTRO_VCMD_IOCH_RELEASE_CMDBUF, DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(HANTRO_VCMD_IOCH_POLLING_CMDBUF, xx_HANTRO_VCMD_IOCH_POLLING_CMDBUF, DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(HANTRO_VCMD_IOCH_PUSH_SLICE_REG, xx_HANTRO_VCMD_IOCH_PUSH_SLICE_REG, DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(HANTRO_VCMD_IOCH_ACQUIRE_MC_TOKEN, xx_HANTRO_VCMD_IOCH_ACQUIRE_MC_TOKEN, DRM_AUTH | DRM_RENDER_ALLOW),

	DRM_IOCTL_DEF_DRV(HANTRODEC_IOCGHWIOSIZE, xx_HANTRODEC_IOCGHWIOSIZE, DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(HANTRODEC_IOC_MC_OFFSETS, xx_HANTRODEC_IOC_MC_OFFSETS, DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(HANTRODEC_IOC_MC_CORES, xx_HANTRODEC_IOC_MC_CORES, DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(HANTRODEC_IOCS_DEC_PUSH_REG, xx_HANTRODEC_IOCS_DEC_PUSH_REG, DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(HANTRODEC_IOCS_DEC_WRITE_REG, xx_HANTRODEC_IOCS_DEC_WRITE_REG, DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(HANTRODEC_IOCS_DEC_WRITE_APBFILTER_REG, xx_HANTRODEC_IOCS_DEC_WRITE_APBFILTER_REG, DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(HANTRODEC_IOCS_DEC_PULL_REG, xx_HANTRODEC_IOCS_DEC_PULL_REG, DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(HANTRODEC_IOCS_DEC_READ_REG, xx_HANTRODEC_IOCS_DEC_READ_REG, DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(HANTRODEC_IOCH_DEC_RESERVE, xx_HANTRODEC_IOCH_DEC_RESERVE, DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(HANTRODEC_IOCT_DEC_RELEASE, xx_HANTRODEC_IOCT_DEC_RELEASE, DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(HANTRODEC_IOCX_DEC_WAIT, xx_HANTRODEC_IOCX_DEC_WAIT, DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(HANTRODEC_IOCG_CORE_WAIT, xx_HANTRODEC_IOCG_CORE_WAIT, DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(HANTRODEC_IOX_ASIC_ID, xx_HANTRODEC_IOX_ASIC_ID, DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(HANTRODEC_IOCG_CORE_ID, xx_HANTRODEC_IOCG_CORE_ID, DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(HANTRODEC_IOX_ASIC_BUILD_ID, xx_HANTRODEC_IOX_ASIC_BUILD_ID, DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(HANTRODEC_IOX_SUBSYS, xx_HANTRODEC_IOX_SUBSYS, DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(HANTRODEC_IOCX_POLL, xx_HANTRODEC_IOCX_POLL, DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(HANTRODEC_IOC_APBFILTER_CONFIG, xx_HANTRODEC_IOC_APBFILTER_CONFIG, DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(HANTRODEC_IOC_AXIFE_CONFIG, xx_HANTRODEC_IOC_AXIFE_CONFIG, DRM_AUTH | DRM_RENDER_ALLOW),

	DRM_IOCTL_DEF_DRV(HANTRO_IOCH_GET_CMDBUF_PARAMETER, xx_HANTRO_IOCH_GET_CMDBUF_PARAMETER, DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(HANTRO_IOCH_GET_VCMD_PARAMETER, xx_HANTRO_IOCH_GET_VCMD_PARAMETER, DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(HANTRO_IOCH_RESERVE_CMDBUF, xx_HANTRO_IOCH_RESERVE_CMDBUF, DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(HANTRO_IOCH_LINK_RUN_CMDBUF, xx_HANTRO_IOCH_LINK_RUN_CMDBUF, DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(HANTRO_IOCH_WAIT_CMDBUF, xx_HANTRO_IOCH_WAIT_CMDBUF, DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(HANTRO_IOCH_RELEASE_CMDBUF, xx_HANTRO_IOCH_RELEASE_CMDBUF, DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(HANTRO_IOCH_POLLING_CMDBUF, xx_HANTRO_IOCH_POLLING_CMDBUF, DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(HANTRO_IOCH_GET_VCMD_ENABLE, xx_HANTRO_IOCH_GET_VCMD_ENABLE, DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(HANTRO_IOCH_GET_MMU_ENABLE, xx_HANTRO_IOCH_GET_MMU_ENABLE, DRM_AUTH | DRM_RENDER_ALLOW),

	DRM_IOCTL_DEF_DRV(HANTRO_IOCG_HWOFFSET, xx_HANTRO_IOCG_HWOFFSET, DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(HANTRO_IOCG_HWIOSIZE, xx_HANTRO_IOCG_HWIOSIZE, DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(HANTRO_IOCG_SRAMOFFSET, xx_HANTRO_IOCG_SRAMOFFSET, DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(HANTRO_IOCG_SRAMEIOSIZE, xx_HANTRO_IOCG_SRAMEIOSIZE, DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(HANTRO_IOCH_ENC_RESERVE, xx_HANTRO_IOCH_ENC_RESERVE, DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(HANTRO_IOCH_ENC_RELEASE, xx_HANTRO_IOCH_ENC_RELEASE, DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(HANTRO_IOCG_CORE_NUM, xx_HANTRO_IOCG_CORE_NUM, DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(HANTRO_IOCG_CORE_INFO, xx_HANTRO_IOCG_CORE_INFO, DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(HANTRO_IOCG_CORE_WAIT, xx_HANTRO_IOCG_CORE_WAIT, DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(HANTRO_IOCG_ANYCORE_WAIT, xx_HANTRO_IOCG_ANYCORE_WAIT, DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(HANTRO_IOCG_ANYCORE_WAIT_POLLING, xx_HANTRO_IOCG_ANYCORE_WAIT_POLLING, DRM_AUTH | DRM_RENDER_ALLOW),
};

static void lisuan_get_misc_cfg(struct lisuan_device *ls_dev)
{
	ls_dev->hwctl.use_vram = vram_only;
	ls_dev->hwctl.cpu_update_ptes = cpu_update_pte;
	ls_dev->hwctl.validation_chip = validation_chip;
	ls_dev->hwctl.no_gfx = no_gfx;
	ls_dev->hwctl.no_gce = no_gce;
	ls_dev->hwctl.no_vem = no_vem;
	ls_dev->hwctl.dp_cts = dp_cts;
	ls_dev->hwctl.display_cts = display_cts;
	ls_dev->hwctl.no_video = no_video;
	ls_dev->hwctl.paging_rb = paging_rb;
	ls_dev->hwctl.skip_clr = skip_clr || (ls_dev->hwctl.no_gfx && ls_dev->hwctl.no_gce);
	ls_dev->hwctl.enable_gl2 = enable_gl2;
	ls_dev->hwctl.pz1 = pz1;
	ls_dev->hwctl.no_encoder = no_encoder;
	ls_dev->hwctl.mode_8k = mode_8k;
	ls_dev->hwctl.fake_monitor_mask = fake_monitor_mask;
	ls_dev->hwctl.support_wc = true;
	ls_dev->hwctl.endebug = endebug;

#if defined(__arm64__) || defined(__aarch64__) || defined(CONFIG_SW64) || defined(__loongarch__)
	//from drm_arch_can_wc_memory
	ls_dev->hwctl.support_wc = false;
#endif
	ls_dev->hang_timeout = (ls_dev->hwctl.validation_chip ? msecs_to_jiffies(120000) : msecs_to_jiffies(5000));

	ls_dev->hwctl.use_byPass = bypass_only;
	ls_dev->hwctl.test_local_mem = 0;
	ls_dev->hwctl.skip_invalidate = 0;
	ls_dev->hwctl.recovery_enable = recovery_enable;
	ls_dev->hwctl.iommu_enabled = 1;//iommu_present(ls_dev->pdev->dev.bus);
	ls_dev->hwctl.enable_local_huge_page = 1;
	ls_dev->hwctl.enable_hw_cg = enable_clock_gating;
	ls_dev->hwctl.enable_sgnt = sgnt_enable;
	ls_dev->hwctl.en_new_cl_clr = en_new_cl_clr;
	ls_dev->dump_rb_cmd = dump_rb_cmd;
   
	gf_debug("iommu %d, recovery:%d, clock gating:%d, use bypass:%d, vram only:%d\n", 
		ls_dev->hwctl.iommu_enabled, ls_dev->hwctl.recovery_enable, ls_dev->hwctl.enable_hw_cg, ls_dev->hwctl.use_byPass, ls_dev->hwctl.use_vram);
	gf_info("cpu update pte:%d or use paging rb:%d, skip clr:%d, enable gl2:%d, support wc:%d, new clr:%d\n", 
		ls_dev->hwctl.cpu_update_ptes, ls_dev->hwctl.paging_rb, ls_dev->hwctl.skip_clr, ls_dev->hwctl.enable_gl2, ls_dev->hwctl.support_wc, ls_dev->hwctl.en_new_cl_clr);
	gf_debug("no gfx:%d, no gce:%d, no vem:%d\n", ls_dev->hwctl.no_gfx, ls_dev->hwctl.no_gce, ls_dev->hwctl.no_vem);
}

static void lisuan_device_init(struct lisuan_device *ls_dev, struct drm_device *dev, unsigned long flags)
{
	struct pci_dev *pdev = ls_dev->pdev;
	unsigned short command = 0;

	ls_dev->dev = dev->dev;

	mutex_init(&ls_dev->file_lock.lock);
	mutex_init(&ls_dev->mailbox_lock.lock);
	INIT_LIST_HEAD(&ls_dev->file_list);
	pci_set_master(pdev);

	ls_dev->fbdev = fbdev_enable;

	//enable IO space for QT/FPGA validation(boot as none primary, will not enable IO space)
	pci_read_config_word(pdev, 0x4, &command);
	if (!(command & 1)) {
		pci_write_config_word(pdev, 0x4, (command | 1));
	}
#if defined(CONFIG_VGA_ARB)
	vga_set_legacy_decoding(pdev, VGA_RSRC_NONE);
#endif

	if (dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(40))) {
		gf_info("No suitable DMA configuration\n");
		ls_dev->hwctl.iommu_enabled = FALSE;
	}

	ls_dev->ring_fence_context = dma_fence_context_alloc(RB_INDEX_LAST);

	ls_dev->rb_count = RB_INDEX_LAST;

	atomic_set(&ls_dev->checked_hang, 0);
	atomic_set(&ls_dev->pf_req_reset, 0);
}

static int lisuan_gem_map(struct lisuan_file *ls_file, struct vm_area_struct *vma)
{
	struct lisuan_gem_bo *buffer = gem_to_gf_bo(vma->vm_private_data);
#if DRM_VERSION_CODE >= KERNEL_VERSION(6,6,0)
	vm_flags_set(vma, VM_PFNMAP | VM_IO | VM_DONTEXPAND | VM_DONTDUMP);
#else
	vma->vm_flags |= VM_PFNMAP | VM_IO | VM_DONTEXPAND | VM_DONTDUMP;
#endif

	atomic_inc(&buffer->unpagable);
	switch (buffer->domain) {
	case dom_visual_vram:
		lisuan_gem_map_vmarea(ls_file, buffer, vma);
		break;
	case dom_system_pages:
	case dom_snoop_system_pages:
		lisuan_gem_map_system_pages(ls_file, buffer, vma);
		break;
	}

	return 0;
}

static int hantroenc_init(struct lisuan_device *gf_device)
{
	if (0)//(vcmd_supported == 0)
		return hantroenc_normal_init(gf_device);
	else
		return hantroenc_vcmd_init(gf_device);
}

static void hantroenc_cleanup(struct lisuan_device *gf_device)
{
	if (0)//(vcmd_supported == 0)
		hantroenc_normal_cleanup(gf_device);
	else
		hantroenc_vcmd_cleanup(gf_device);
}


int lisuan_kms_drm_load(struct lisuan_device *ls_dev, unsigned long flags)
{
    struct lisuan_pci_info pci_info = {0};

	ls_dev->ddev->dev_private = ls_dev;

	lisuan_get_misc_cfg(ls_dev);

	lisuan_init_pci_device_info(ls_dev);

	lisuan_device_init(ls_dev, ls_dev->ddev, 0);

	lisuan_init_memory_manager(ls_dev);

	//init the scheduler 
	lisuan_init_ring(ls_dev);

	gf_init_cmd_template(&ls_dev->rb_cmd_template, ls_dev->slc_mask);

    if(!ls_dev->hwctl.no_gce || !ls_dev->hwctl.no_gfx){
    	gf_init_gpu_state(ls_dev->sched[RB_INDEX_GFX]);	//use the GFX engine to init
    }

	lisuan_init_procid_mgr(ls_dev);

	//TODO irq init & debugfs
	tasklet_init(&ls_dev->fenceirq_work, (void*)lisuan_sched_fence_irq_worklet, (unsigned long)ls_dev);
	tasklet_disable(&ls_dev->fenceirq_work);
	tasklet_enable(&ls_dev->fenceirq_work);	
	INIT_WORK(&ls_dev->therm_work, lisuan_mailbox_get_therm_worker);
	
	if (ls_dev->is_virtfn) {
		INIT_WORK(&ls_dev->mailbox_work, lisuan_mailbox_worker);
	}
	lisuan_init_ring_fence_thread(ls_dev);

	if (!ls_dev->hwctl.no_video) {
		//gf_hantro_reset(gf_dev);
		hantrodec_init(ls_dev);
		hantroenc_init(ls_dev);

		INIT_WORK(&ls_dev->dec0_vcmd_work, lisuan_dec0_vcmd_worker);
		INIT_WORK(&ls_dev->dec1_vcmd_work, lisuan_dec1_vcmd_worker);
		INIT_WORK(&ls_dev->enc0_vcmd_work, lisuan_enc0_vcmd_worker);
		INIT_WORK(&ls_dev->im_vcmd_work, lisuan_im_vcmd_worker);
	}

	if (ls_dev->is_virtfn) 
		gf_kms_virt_init(ls_dev);
	else 
		gf_kms_init(ls_dev);
	
	gf_irq_init(ls_dev);

	lisuan_query_pci_msi_info(ls_dev,&pci_info);
	gf_msi_init(ls_dev,ls_dev->pdev, ls_dev->mmio_base,
                pci_info.pos,
                pci_info.address_hi,
                pci_info.address_lo,
                pci_info.msi_ctl,
                pci_info.data,
                pci_info.adr_64bit,
                pci_info.msi_enable,
                pci_info.msi_mm_en,
                ls_dev->is_virtfn,
				ls_dev->pdev->msi_cap);

	gf_irq_postinstall(ls_dev);

#if LINUX_VERSION_CODE <= KERNEL_VERSION(5, 4, 0)
	drm_fbdev_generic_setup(ls_dev->ddev, 32);
#else
	if (ls_dev->fbdev)
		lisuan_fbdev_init(ls_dev);
#endif

	ls_dev->debugfs = NULL;

	//gf_kmdEvent_log_control_init();
	gf_kmdEvent_log_init(ls_dev);

	gf_gpuusage_init(ls_dev);

	gf_map_cf_buffer(ls_dev->mmio_base);

	return 0;
}

int lisuan_drm_load(struct drm_device *ddev, unsigned long flags)
{
	return 0;
}

static void lisuan_drm_unload(struct drm_device *ddev)
{
}

extern const struct dma_buf_ops lisuan_dmabuf_ops;

static struct drm_gem_object *lisuan_dmabuf_create_obj(struct drm_device *dev, struct dma_buf *dma_buf)
{
	struct lisuan_device *ls_dev = (struct lisuan_device *)dev->dev_private;
	struct lisuan_gem_bo *gbo = (struct lisuan_gem_bo *)gf_kvcalloc(1, sizeof(struct lisuan_gem_bo), GFP_KERNEL);
	struct drm_gem_object *gem = tbo_to_drm_gem_bo(&gbo->tbo);
	struct dma_buf_attachment *attach = NULL;
	struct sg_table * st = NULL;

	int ret = 0;

	gbo->first_map = 1;
	gbo->domain = (ls_dev->hwctl.support_wc?dom_system_pages:dom_snoop_system_pages);
	gbo->prefer_flags.cpu_visible = 1;

	gbo->size = 
	gbo->size_org = dma_buf->size;
	gbo->align = 
	gbo->align_org = ls_dev->sys_page_size;

#if DRM_VERSION_CODE >= KERNEL_VERSION(5,4,0)
	gem->resv = dma_buf->resv;
#else
	gbo->tbo.resv = dma_buf->resv;
#endif

	ret = lisuan_alloc_gem_bo(ls_dev, NULL, gbo);
	if(ret){
		gf_error("dmabuf gem create failed:%d\n", ret);
		gf_kvfree(gbo);
		return ERR_PTR(ret);
	}

	attach = dma_buf_attach(dma_buf, dev->dev);
	if (IS_ERR(attach)){
		drm_gem_object_put(gem);
		return ERR_CAST(attach);
	}

	st = dma_buf_map_attachment(attach,
			       DMA_BIDIRECTIONAL);
	if (IS_ERR(st))
		goto fail_attach;

	get_dma_buf(dma_buf);
	gem->import_attach = attach;

	gbo->tbo.sg = st;
	gbo->flag.pa_exist = 1;
	gbo->flag.snoop = ((gbo->domain == dom_snoop_system_pages) ? 1 : 0);
	gbo->flag.vram = 0;
	gbo->flag.nsec = 1;

	atomic64_inc(&gbo->resident_cnt);

	return gem;
fail_attach:
	dma_buf_detach(dma_buf, attach);
	drm_gem_object_put(gem);
	return ERR_PTR(-EINVAL);
}

struct drm_gem_object *lisuan_gem_prime_import(struct drm_device *dev, struct dma_buf *dma_buf)
{
	struct drm_gem_object *geo;
	struct lisuan_gem_bo *buffer;

	struct lisuan_device *ls_dev = (struct lisuan_device *)dev->dev_private;

	if (dma_buf->ops == &lisuan_dmabuf_ops)
	{
		geo = dma_buf->priv;
		buffer = gem_to_gf_bo(geo);

		if(geo->dev == dev){
			gem_info("gem import:0x%p(0x%x), create at %d, cur tid:%d-%s, is_present:%d, bypass:%d, handcnt:%d\n",
				buffer, buffer->handle, buffer->ls_file->tgid, (unsigned long long)current->tgid, current->comm,
				buffer->is_present, buffer->flag.byPass, geo->handle_count);

			drm_gem_object_get(geo);
			return geo;
		}
	}

	geo = lisuan_dmabuf_create_obj(dev, dma_buf);

	return geo;
}

void *lisuan_gem_vmap(struct drm_gem_object *base)
{
	struct lisuan_device *ls_dev = (struct lisuan_device *)base->dev->dev_private;
	struct lisuan_gem_bo *gem = gem_to_gf_bo(base);
	void * addr  = NULL;

	if(gem->domain == dom_invisual_vram)
		return NULL;

	atomic_inc(&gem->unpagable);

	if(gem->domain < dom_invisual_vram) 
		addr = lisuan_map_vram_node(ls_dev, __get_gem_buffer_mm_node(gem), gem->flag.byPass);
	else
		addr = lisuan_map_system_pages(ls_dev, gem->flag.snoop, gem->size, gem->tbo.sg);

	return addr;
}

#if DRM_VERSION_CODE >= KERNEL_VERSION(6,6,0)
int lisuan_gem_prime_vmap(struct drm_gem_object *base, struct iosys_map *map)
{
	struct lisuan_gem_bo *buffer = gem_to_gf_bo(base);
	void *addr = lisuan_gem_vmap(base);

	if(buffer->domain < dom_system_pages)
		iosys_map_set_vaddr_iomem(map, addr);
	else
		iosys_map_set_vaddr(map, addr);

	return addr?0:-ENOMEM;
}
#elif  DRM_VERSION_CODE >= KERNEL_VERSION(5,15,0)
int lisuan_gem_prime_vmap(struct drm_gem_object *base, struct dma_buf_map *map)
{
	struct lisuan_gem_bo *buffer = gem_to_gf_bo(base);
	void *addr = lisuan_gem_vmap(base);

	if(buffer->domain < dom_system_pages)
		dma_buf_map_set_vaddr_iomem(map, addr);
	else
		dma_buf_map_set_vaddr(map, addr);

	return addr?0:-ENOMEM;
}
#else
void *lisuan_gem_prime_vmap(struct drm_gem_object *base)
{
	struct lisuan_gem_bo *buffer = gem_to_gf_bo(base);
	void *addr = lisuan_gem_vmap(base);

	return addr;
}
#endif

#if DRM_VERSION_CODE >= KERNEL_VERSION(6,6,0)
void lisuan_gem_prime_vunmap(struct drm_gem_object *base, struct iosys_map *map)
{
	struct lisuan_gem_bo *gem = gem_to_gf_bo(base);

	if(gem->domain == dom_invisual_vram)
		return;

	
	if (gem->domain < dom_invisual_vram)
		iounmap(map->vaddr_iomem);
	else
		vunmap(map->vaddr);

	atomic_dec(&gem->unpagable);
	iosys_map_clear(map);
}
#elif DRM_VERSION_CODE >= KERNEL_VERSION(5,15,0)
void lisuan_gem_prime_vunmap(struct drm_gem_object *base, struct dma_buf_map *map)
{
	struct lisuan_gem_bo *gem = gem_to_gf_bo(base);

	if(gem->domain == dom_invisual_vram)
		return;

	if (gem->domain < dom_invisual_vram)
		iounmap(map->vaddr_iomem);
	else
		vunmap(map->vaddr);

	atomic_dec(&gem->unpagable);
	dma_buf_map_clear(map);
}
#else
void lisuan_gem_prime_vunmap(struct drm_gem_object *base, void *vaddr)
{
	struct lisuan_gem_bo *gem = gem_to_gf_bo(base);

	if(gem->domain == dom_invisual_vram)
		return;

	if (gem->domain < dom_invisual_vram)
		iounmap(vaddr);
	else
		vunmap(vaddr);

	atomic_dec(&gem->unpagable);
}
#endif

int lisuan_dumb_create(struct drm_file *filp, struct drm_device *dev, struct drm_mode_create_dumb *args)
{
	unsigned int width = args->width, bpp = args->bpp;
	struct lisuan_device *ls_device = (struct lisuan_device *)dev->dev_private;
	struct lisuan_file *ls_file = (struct lisuan_file *)filp->driver_priv;

	struct lisuan_gem_bo *buffer = (struct lisuan_gem_bo *)gf_kvcalloc(1, sizeof(struct lisuan_gem_bo), GFP_KERNEL);
	int ret = 0;
	unsigned int height = args->height;
	struct lisuan_mman *mmgr = NULL;
	struct lisuan_vm_node *va_node = NULL;

	args->pitch = ((width * bpp + 2047) / 2048) * 2048 / 8;	//2kbits alignment size
	args->size = height * args->pitch;

	buffer->domain = dom_visual_vram;//force in bypass for bring up
	buffer->size = ALIGN64_SIZE(args->size, ls_device->sys_page_size);
	buffer->align = 256;
	buffer->flag.byPass = 1;
	buffer->flag.nsec = 1;
	atomic_set(&buffer->unpagable, 1);
	buffer->priority = gem_priority_normal;
	buffer->flag.vram = (buffer->domain <= dom_invisual_vram) ? 1 : 0;
	buffer->flag.snoop = ((buffer->domain == dom_snoop_system_pages) ? 1 : 0);

	ret = lisuan_gem_alloc_oneshot(ls_device, ls_file, buffer, 0);
	if(ret)
		goto fail;
	args->handle = buffer->handle;
	return ret;

fail:
	if(buffer){
		gf_kvfree(buffer);
	}
	return ret;
}

int lisuan_dumb_mmap_offset(struct drm_file *filp, struct drm_device *dev, uint32_t handle, uint64_t * offset)
{
	struct drm_gem_object *geo;

	geo = drm_gem_object_lookup(filp, handle);
	if (geo == NULL) {
		return -ENOENT;
	}

	drm_gem_create_mmap_offset(geo);
	*offset = drm_vma_node_offset_addr(&geo->vma_node);

	drm_gem_object_put_compat(geo);

	return 0;
}

long lisuan_drm_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	long ret = 0;
	return ret;
}

int lisuan_drm_do_internal_buf_mmap(struct vm_area_struct *vma, struct mmap_args *args)
{
    if(!vma || !args || args->domain == dom_invisual_vram)
        return -ENOENT;

    vma_config(vma, args->snoop);

    if(args->domain < dom_invisual_vram){
        if (remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff, vma->vm_end - vma->vm_start, vma->vm_page_prot)) {
                return -EAGAIN;
        }
    } else {
        struct sg_table *table = args->sg_table;
        struct scatterlist *sg = table->sgl;
        unsigned long vm_start = vma->vm_start, pfn;

        while (sg) {
            if (sg_page(sg)) {
                pfn = page_to_pfn(sg_page(sg));
                if (remap_pfn_range(vma, vm_start, pfn, sg->length, vma->vm_page_prot)) {
                    return -EAGAIN;
                }
                vm_start += sg->length;
            }
            sg = sg_next(sg);
        }
    }

    return 0;
}

int lisuan_drm_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct drm_file *file = (struct drm_file *)filp->private_data;
	struct lisuan_file *ls_file = (struct lisuan_file *)file->driver_priv;
	struct lisuan_device *ls_dev = ls_file->ls_dev;

	int ret = 0;

	if (!ls_dev)
		return -EINVAL;

	//only used for the cf node map umd cpu_va
	if (ls_file->gbo_cf) {
		struct mmap_args *args = (struct mmap_args *)ls_file->gbo_cf;

		return lisuan_drm_do_internal_buf_mmap(vma, args);
	}

	//only used for the cf node map umd cpu_va
	if (ls_file->vem_rb_map) {
		struct mmap_args *args = (struct mmap_args *)ls_file->vem_rb_map;

		return lisuan_drm_do_internal_buf_mmap(vma, args);
	}

	if (ls_file->dec_vcmd_map) {
		struct mmap_args *args = (struct mmap_args *)ls_file->dec_vcmd_map;

		return lisuan_drm_do_internal_buf_mmap(vma, args);
	}

	if (ls_file->enc_vcmd_map) {
		struct mmap_args *args = (struct mmap_args *)ls_file->enc_vcmd_map;

		return lisuan_drm_do_internal_buf_mmap(vma, args);
	}

	ret = drm_gem_mmap(filp, vma);

	if (ret < 0) {
		gf_error("lisuan: drm_gem_mmap failed, ret=%d vma=[%lx-%lx] pgoff=0x%lx pages=%lu\n",
				ret, vma->vm_start, vma->vm_end, vma->vm_pgoff, (unsigned long)vma_pages(vma));
		return ret;
	}

	return lisuan_gem_map(ls_file, vma);
}

static void lisuan_pci_shutdown(struct pci_dev *pdev)
{
}

static void gf_wait_ring_idle(struct lisuan_ring *sched)
{
	while(atomic64_read(&sched->last_submit_fence_id)  >  atomic64_read(&sched->returned_fence_id))
	{
		pm_info("sched is busy %p\n",sched);
		udelay(10);
	}
}
static void lisuan_wait_all_rb_idle(struct lisuan_device *ls_dev)
{
	int i;
	for (i = 0; i < ls_dev->rb_count; i++) {
		if (ls_dev->sched[i])
			gf_wait_ring_idle(ls_dev->sched[i]);
	}
}

static void lisuan_pm_save_cf_buf(struct lisuan_device *ls_dev, struct lisuan_gem_bo *cfbuf)
{
	struct lisuan_gem_bo * tmp;
	void * cpu_va = NULL;
	int ret = 0;

	tmp = gf_kvcalloc(1, sizeof(struct lisuan_gem_bo), GFP_KERNEL);
	
	tmp->size = cfbuf->size;
	tmp->align = cfbuf->align;
	tmp->domain = (ls_dev->hwctl.support_wc?dom_system_pages:dom_snoop_system_pages);
	tmp->flag.byPass = 1;
	tmp->pm_discard = 1;
	atomic_set(&tmp->unpagable, 1);
	tmp->priority = gem_priority_high;
	tmp->flag.vram = (tmp->domain <= dom_invisual_vram) ? 1 : 0;
	tmp->flag.snoop = ((tmp->domain == dom_snoop_system_pages) ? 1 : 0);
	
	ret = lisuan_gem_alloc_oneshot(ls_dev, NULL, tmp, 1);
	
	if(ret){
		gem_info("evict cf buffer create tmp gem failed:%d\n", ret);
		goto fail;
	}

	cpu_va = lisuan_map_system_pages(ls_dev, 0, tmp->size, tmp->tbo.sg);

	gf_memcpy(cpu_va, cfbuf->virt_addres, cfbuf->size);
	
	cfbuf->tbo.sg = tmp->tbo.sg;
	tmp->tbo.sg = NULL;

	cfbuf->evicted = true;

fail:
	if(tmp)
		lisuan_gem_free_oneshot(ls_dev, tmp);
}

static void lisuan_pm_restore_cf_buf(struct lisuan_device *ls_dev, struct lisuan_gem_bo *cfbuf)
{
	void * cpu_va = NULL;

	gf_assert(cfbuf && cfbuf->tbo.sg && cfbuf->size);
	cpu_va = lisuan_map_system_pages(ls_dev, 0, cfbuf->size, cfbuf->tbo.sg);

	gf_memcpy(cfbuf->virt_addres, cpu_va, cfbuf->size);

	vunmap(cpu_va);
	lisuan_free_system_pages(ls_dev, 0, cfbuf->tbo.sg, 0, cfbuf->size);

	cfbuf->tbo.sg = NULL;
	cfbuf->evicted = false;
}
static bool lisuan_can_freeze(struct lisuan_scheduler *sched)
{
	int i;
	struct lisuan_sched_entity *entity;
	struct lisuan_sched_rq *rq;

	for (i = DRM_SCHED_PRIORITY_MAX - 1; i >= DRM_SCHED_PRIORITY_NORMAL; i--) 
	{
		rq = &sched->sched_rq[i];
		list_for_each_entry(entity, &rq->entities, list) 
		{
			spin_lock(&entity->rq_lock);
			if (!lisuan_sched_entity_is_idle(entity))
			{
				spin_unlock(&entity->rq_lock);
				return false;
			}
			spin_unlock(&entity->rq_lock);
		}
	}

	return true;
}
static void gf_wait_job_empty(struct lisuan_ring *sched)
{
	while(!lisuan_can_freeze(&sched->base))
	{
		pm_info("job is not empty %p\n",sched);
		udelay(10);
	}
}
static void lisuan_wait_all_job_empty(struct lisuan_device *ls_dev)
{
	int i;
	for (i = 0; i < ls_dev->rb_count; i++) {
		if (ls_dev->sched[i])
			gf_wait_job_empty(ls_dev->sched[i]);
	}
}

static void lisuan_pm_suspend(struct lisuan_device *ls_dev)
{
	int i = 0;
	struct drm_atomic_state *state;

	/*suspend fb*/
	drm_mode_config_helper_suspend(ls_dev->ddev);
	
	pci_save_state(ls_dev->pdev);

	/*wait hw idle*/
	lisuan_wait_all_job_empty(ls_dev);
	lisuan_wait_all_rb_idle(ls_dev);
	gf_info("suspend wait hw idle done 1\n");

	//lisuan_pm_save_cf_buf(ls_dev, ls_dev->cf.buf);
	lisuan_gem_evict_all(ls_dev);

	pm_info("evict pt buffer at last time %lx", ls_dev->pt.buf);
	lisuan_gem_evict(ls_dev, ls_dev->pt.buf, true, true);
	gf_info("evict all gem\n");

	/*wait evict job done*/
	lisuan_wait_all_job_empty(ls_dev);
	lisuan_wait_all_rb_idle(ls_dev);

	tasklet_disable(&ls_dev->fenceirq_work);
	gf_info("suspend wait hw idle done 2\n");
}

static void lisuan_pm_resume_recover_gem_by_cpu(struct lisuan_device *ls_dev, struct lisuan_gem_bo *ptbuf)
{
	unsigned int domain = 0;
	struct lisuan_mman * mmgr = NULL;
	void * cpu_va = NULL;

	if (!ptbuf)
		return;

	/*we should revover pt by cpu at begining of resume*/
	domain = (ptbuf->domain <= dom_invisual_vram)?ptbuf->domain:dom_system_pages;
	mmgr = &ls_dev->mmgr[domain-1];

	gf_assert(ptbuf && ptbuf->tbo.sg && ptbuf->size);
	cpu_va = lisuan_map_system_pages(ls_dev, 0, ptbuf->size, ptbuf->tbo.sg);

	gf_memcpy(ptbuf->virt_addres, cpu_va, ptbuf->size);

	vunmap(cpu_va);
	lisuan_free_system_pages(ls_dev, 0, ptbuf->tbo.sg, 0, ptbuf->size);

	spin_lock(&mmgr->lock);
	list_move_tail(&ptbuf->item, &mmgr->list_lru[ptbuf->priority]);
	spin_unlock(&mmgr->lock);

	ptbuf->tbo.sg = NULL;
	ptbuf->evicted = false;
}

static void lisuan_set_all_vm_dirty(struct lisuan_device *ls_dev)
{
	struct lisuan_file *ls_file;

	mutex_lock(&ls_dev->file_lock.lock);

	list_for_each_entry(ls_file, &ls_dev->file_list, dev_object) {
		if (ls_file) {
			ls_file->vm.is_L0_dirty = 0xFF;
			ls_file->vm.is_L1_dirty = 0xFF;
			ls_file->vm.is_L2_dirty = 0xFF;
		}
	}
	mutex_unlock(&ls_dev->file_lock.lock);
}


static void lisuan_pm_gem_resident_all(struct lisuan_device *ls_dev)
{
	struct lisuan_gem_bo * gem, *tmp;
	int ret = 0, i = 0;

	mutex_lock(&ls_dev->lock.lock);
	for(i = gem_priority_unused; i < gem_priority_count; i++)
	{
		//recover bypass gem first
		list_for_each_entry_safe(gem, tmp, &ls_dev->list_lru[i], item)
		{
			if(!gem->pm_discard && gem->flag.byPass)
			{
				pm_info("make resident gem %p by gpu\r\n", gem);
				ret = lisuan_gem_resident(ls_dev, gem, true, true);

				if(ret){
					gf_error("resident gem:%x from tgid-pid:%d-%d, failed:%d\n",
						gem->handle,
						gem->ls_file?gem->ls_file->tgid:ls_dev->vm_ctx->tgid,
						gem->ls_file?gem->ls_file->pid:ls_dev->vm_ctx->pid,
						ret);
				}
			}
		}
		//then recover none bypass gem
		list_for_each_entry_safe(gem, tmp, &ls_dev->list_lru[i], item)
		{
			if(!gem->pm_discard && !gem->flag.byPass)
			{
				pm_info("make resident none bypass gem %p by gpu\r\n", gem);
				ret = lisuan_gem_resident(ls_dev, gem, true, true);
				if(ret){
					gf_error("resident none bypass gem:%x from tgid-pid:%d-%d, failed:%d\n",
						gem->handle,
						gem->ls_file?gem->ls_file->tgid:ls_dev->vm_ctx->tgid,
						gem->ls_file?gem->ls_file->pid:ls_dev->vm_ctx->pid,
						ret);
				}
			}
		}
	}

	mutex_unlock(&ls_dev->lock.lock);
}

static void lisuan_pm_resume(struct lisuan_device *ls_dev)
{
	int i = 0;
	struct lisuan_cl_info cl_info;
	struct lisuan_pci_info pci_info = {0};
	uint32_t retry = 0;

	pci_set_power_state(ls_dev->pdev, PCI_D0);
	
	if (pci_enable_device(ls_dev->pdev)) {
		pm_info("pci_enable_device failed 0x%lx \r\n", ls_dev);
		return;
	}
	pci_restore_state(ls_dev->pdev);

	/* ensure bus-master for DMA */
	pci_set_master(ls_dev->pdev);

	/* code for dpe*/
	while(0xdeadbeef != gf_read_mmio32(ls_dev->mmio_base + 0x22cc28))
	{
		udelay(1000);
		retry++;
		if(retry>1000)
		{
		pm_info("wait DMC init timeout 0x%lx \r\n", ls_dev);
		return;
		}
	}
	lisuan_query_cl_info(ls_dev,&cl_info);

	gf_hw_mhb_init(ls_dev->mmio_base,
		ls_dev->vram_size / 1024 / 1024,
		cl_info.cl_base,
		cl_info.cl_size,
		ls_dev->mhu_disable_mask,
		ls_dev->mhb_num,
		ls_dev->is_virtfn);

	gf_map_cf_buffer(ls_dev->mmio_base);

	gf_pci_mem_config_init(ls_dev->mmio_base, ls_dev->hwctl.enable_hw_cg, ls_dev->is_virtfn);

	lisuan_query_pci_msi_info(ls_dev,&pci_info);

	//gf_interrupt_disable(ls_dev->mmio_base, ls_dev->is_virtfn);
	gf_irq_reset(ls_dev);
	gf_msi_init(ls_dev, ls_dev->pdev,ls_dev->mmio_base,
				pci_info.pos,
				pci_info.address_hi,
				pci_info.address_lo,
				pci_info.msi_ctl,
				pci_info.data,
				pci_info.adr_64bit,
				pci_info.msi_enable,
				pci_info.msi_mm_en,
				ls_dev->is_virtfn,
				ls_dev->pdev->msi_cap);

	gf_kms_resume(ls_dev);
	gf_irq_postinstall(ls_dev);

	tasklet_enable(&ls_dev->fenceirq_work);

	lisuan_pm_resume_recover_gem_by_cpu(ls_dev, ls_dev->pt.buf);
	lisuan_pm_resume_recover_gem_by_cpu(ls_dev, ls_dev->dec_bufObj);
	lisuan_vm_reinit_bypass_pt(ls_dev);

	for (i = 0; i < ls_dev->rb_count; i++)
		gf_scheduler_reinit(ls_dev, i, ls_dev->sched[i]);

	gf_init_gpu_state(ls_dev->sched[0]);

	//lisuan_pm_restore_cf_buf(ls_dev, ls_dev->cf.buf);
	lisuan_pm_gem_resident_all(ls_dev);
	
	gf_info("resume lisuan_pm_gem_resident_all\n");
	lisuan_set_all_vm_dirty(ls_dev);

	if (!ls_dev->hwctl.no_video) {
		hantro_dec_resume(ls_dev);
		hantro_enc_resume(ls_dev);
	}

	lisuan_wait_all_job_empty(ls_dev);
	lisuan_wait_all_rb_idle(ls_dev);
	gf_info("resume wait idle done\n");

	drm_mode_config_helper_resume(ls_dev->ddev);
	gf_info("resume drm_mode_config_helper_resume\n");
}

static long lisuan_drm_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct drm_file *file_priv = filp->private_data;
	struct drm_device *dev;
	struct lisuan_device *ls_device;
	
	long ret;
	dev = file_priv->minor->dev;
	ret = pm_runtime_get_sync(dev->dev);
	if (ret < 0)
		goto out;
	ls_device = (struct lisuan_device *)dev->dev_private;

	while(ls_device->is_virtfn && !atomic_read(&ls_device->pf_finish_reset)){
		gf_msleep(5);
	}

	ret = drm_ioctl(filp, cmd, arg);

	pm_runtime_mark_last_busy(dev->dev);
out:
	pm_runtime_put_autosuspend(dev->dev);
	return ret;
}

static int lisuan_pm_op_suspend(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct drm_device *drm_dev = pci_get_drvdata(pdev);
	struct lisuan_device *ls_dev=(struct lisuan_device *)drm_dev->dev_private;

	gf_info("pci-dev(%x:%x) pm suspend\n", pdev->vendor, pdev->device);

	lisuan_pm_suspend(ls_dev);
	return 0;
}
static int lisuan_pm_op_resume(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct drm_device *drm_dev = pci_get_drvdata(pdev);
	struct lisuan_device *ls_dev=(struct lisuan_device *)drm_dev->dev_private;

	gf_info("pci-dev(%x:%x) pm resume\n", pdev->vendor, pdev->device);

	lisuan_pm_resume(ls_dev);
	return 0;
}

static int lisuan_pm_op_freeze(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct drm_device *drm_dev = pci_get_drvdata(pdev);
	struct lisuan_device *ls_dev=(struct lisuan_device *)drm_dev->dev_private;

	gf_info("pci-dev(%x:%x) pm freeze\n", pdev->vendor, pdev->device);

	lisuan_pm_suspend(ls_dev);
	return 0;
}

static int lisuan_pm_op_thaw(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct drm_device *drm_dev = pci_get_drvdata(pdev);
	struct lisuan_device *ls_dev=(struct lisuan_device *)drm_dev->dev_private;

	gf_info("pci-dev(%x:%x) pm thaw\n", pdev->vendor, pdev->device);

	return 0;
}

static int lisuan_pm_op_poweroff(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);

	gf_info("pci-dev(%x:%x) pm poweroff\n", pdev->vendor, pdev->device);

	return 0;
}

static int lisuan_pm_op_restore(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct drm_device *drm_dev = pci_get_drvdata(pdev);
	struct lisuan_device *ls_dev=(struct lisuan_device *)drm_dev->dev_private;

	gf_info("pci-dev(%x:%x) pm restore\n", pdev->vendor, pdev->device);

	lisuan_pm_resume(ls_dev);

	return 0;
}

static int lisuan_pm_op_runtime_suspend(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);

	gf_info("pci-dev(%x:%x) pm runtime suspend\n", pdev->vendor, pdev->device);

	return 0;
}

static int lisuan_pm_op_runtime_resume(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);

	gf_info("pci-dev(%x:%x) pm runtime resume\n", pdev->vendor, pdev->device);

	return 0;
}
static int lisuan_pm_op_runtime_idle(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);

	gf_info("pci-dev(%x:%x) pm runtime idle\n", pdev->vendor, pdev->device);

	return 0;
}

static const struct dev_pm_ops lisuan_pm_ops = {
	.suspend = lisuan_pm_op_suspend,
	.resume = lisuan_pm_op_resume,
	.freeze = lisuan_pm_op_freeze,
	.thaw = lisuan_pm_op_thaw,
	.poweroff = lisuan_pm_op_poweroff,
	.restore = lisuan_pm_op_restore,
	.runtime_suspend = lisuan_pm_op_runtime_suspend,
	.runtime_resume = lisuan_pm_op_runtime_resume,
	.runtime_idle = lisuan_pm_op_runtime_idle,
};

static const struct file_operations lisuan_driver_fops = {
	.owner = THIS_MODULE,
	.open = drm_open,
	.release = drm_release,
	.unlocked_ioctl = lisuan_drm_ioctl,
	.mmap = lisuan_drm_mmap,
	.poll = drm_poll,
	.read = drm_read,
#if defined(HAVE_COMPAT_IOCTL)
	.compat_ioctl = lisuan_drm_compat_ioctl,
#endif
#if DRM_VERSION_CODE >= KERNEL_VERSION(6,12,0)
	.fop_flags = FOP_UNSIGNED_OFFSET,
#endif
};

#if DRM_VERSION_CODE < KERNEL_VERSION(5,7,0)

#define DRM_SCANOUTPOS_VALID        (1 << 0)
#define DRM_SCANOUTPOS_IN_VBLANK    (1 << 1)
#define DRM_SCANOUTPOS_ACCURATE     (1 << 2)
#define USE_REAL_VBLANKSTART		(1 << 30)
#define GET_DISTANCE_TO_VBLANKSTART	(1 << 31)
static bool crtc_get_scanout_position(struct drm_device *dev, unsigned int pipe,
				     bool in_vblank_irq, int *vpos, int *hpos,
				     ktime_t *stime, ktime_t *etime,
				     const struct drm_display_mode *mode)
{
	struct drm_crtc *crtc = drm_crtc_from_index(dev, pipe);
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

int lisuan_gem_dmabuf_mmap(struct dma_buf *dma_buf, struct vm_area_struct *vma)
{
	struct drm_gem_object *obj = dma_buf->priv;
	struct lisuan_gem_bo *buffer = gem_to_gf_bo(obj);
	struct lisuan_file * ls_file = buffer->ls_file;
#if DRM_VERSION_CODE >= KERNEL_VERSION(6,6,0)
	vm_flags_set(vma, VM_PFNMAP | VM_IO | VM_DONTEXPAND | VM_DONTDUMP);
#else
	vma->vm_flags |= VM_PFNMAP | VM_IO | VM_DONTEXPAND | VM_DONTDUMP;
#endif

	atomic_inc(&buffer->unpagable);
	switch (buffer->domain) {
	case dom_visual_vram:
		lisuan_gem_map_vmarea(ls_file, buffer, vma);
		break;
	case dom_invisual_vram:
		gf_error("mapping invisible memory\n");
		break;
	case dom_system_pages:
	case dom_snoop_system_pages:
		lisuan_gem_map_system_pages(ls_file, buffer, vma);
		break;
	}

	return 0;
}

#if DRM_VERSION_CODE < KERNEL_VERSION(5,5,0)
extern struct vm_operations_struct gf_vm_ops;
#endif
static struct pci_device_id pciidlist[] = {
	{ 0x4c54, 0x5000, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
	{ 0x4c54, 0x5001, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
	{ 0x4c54, 0x5002, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
	{ 0x4c54, 0x5003, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
	{ 0x4c54, 0x5004, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
	{ 0x4c54, 0x5005, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
	{ 0x4c54, 0x5006, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
	{ 0x4c54, 0x0100, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
	{ 0, 0, 0 },
};

static struct drm_driver lisuan_drm_driver = {
#if DRM_VERSION_CODE >= KERNEL_VERSION(5,4,0)
	.driver_features = DRIVER_MODESET | DRIVER_RENDER | DRIVER_GEM | DRIVER_ATOMIC | DRIVER_SYNCOBJ | DRIVER_SYNCOBJ_TIMELINE,
#elif DRM_VERSION_CODE >= KERNEL_VERSION(5,2,0)
	.driver_features = DRIVER_MODESET | DRIVER_RENDER | DRIVER_GEM | DRIVER_ATOMIC | DRIVER_SYNCOBJ | DRIVER_SYNCOBJ_TIMELINE | DRIVER_PRIME,
#else
	.driver_features = DRIVER_MODESET | DRIVER_RENDER | DRIVER_GEM | DRIVER_ATOMIC | DRIVER_SYNCOBJ | DRIVER_PRIME | DRIVER_LEGACY,
#endif
	.open = lisuan_drm_open,
	.postclose = lisuan_drm_postclose,
#if DRM_VERSION_CODE < KERNEL_VERSION(6,12,0)
	.lastclose = lisuan_drm_last_close,
#endif
	.ioctls = lisuan_ioctls_drm,
	.num_ioctls = ARRAY_SIZE(lisuan_ioctls_drm),
	.dumb_create = lisuan_dumb_create,
	.dumb_map_offset = lisuan_dumb_mmap_offset,
	.fops = &lisuan_driver_fops,
	.prime_handle_to_fd = drm_gem_prime_handle_to_fd,
	.prime_fd_to_handle = drm_gem_prime_fd_to_handle,
	.gem_prime_import = lisuan_gem_prime_import,

	.load = lisuan_drm_load,
	.unload = lisuan_drm_unload,

#if DRM_VERSION_CODE >= KERNEL_VERSION(6,12,0)
#ifdef CONFIG_DRM_FBDEV_EMULATION
	.fbdev_probe = lisuanfb_probe,
#else
	.fbdev_probe = NULL,
#endif
#endif
	//.gem_free_object_unlocked = lisuan_gem_free_buffer,    
	//.dumb_destroy = drm_gem_dumb_destroy,

#ifdef CONFIG_DEBUG_FS
	.debugfs_init = gf_debugfs_init,
#endif

	.name = DRIVER_NAME,
	.desc = DRIVER_DESC,
#if DRM_VERSION_CODE < KERNEL_VERSION(6,12,0)
	.date = DRIVER_DATE,
#endif
#if DRM_VERSION_CODE < KERNEL_VERSION(5,0,0)
	.gem_free_object = lisuan_gem_object_free,
	.gem_open_object = lisuan_gem_object_open,
	.gem_close_object = lisuan_gem_object_close,
	.gem_vm_ops = &gf_vm_ops,
	.gem_prime_vmap = lisuan_gem_prime_vmap,
	.gem_prime_vunmap = lisuan_gem_prime_vunmap,
	.gem_prime_export = lisuan_gem_object_export,
#endif
	.major = DRM_DRIVER_MAJOR,
	.minor = DRM_DRIVER_MINOR,
	.patchlevel = DRM_DRIVER_PATCHLEVEL,
#if DRM_VERSION_CODE < KERNEL_VERSION(5,7,0)
	.get_scanout_position = crtc_get_scanout_position,
	.get_vblank_timestamp = drm_calc_vbltimestamp_from_scanoutpos,
#endif
};

#if DRM_VERSION_CODE < KERNEL_VERSION(4,20,0)
static int lisuan_kick_out_firmware_fb(struct pci_dev *pdev)
{
	struct apertures_struct *ap;
	bool primary = false;

	ap = alloc_apertures(1);
	if (!ap)
		return -ENOMEM;

	ap->ranges[0].base = pci_resource_start(pdev, 0);
	ap->ranges[0].size = pci_resource_len(pdev, 0);

#ifdef CONFIG_X86
	primary = pdev->resource[PCI_ROM_RESOURCE].flags & IORESOURCE_ROM_SHADOW;
#endif
	drm_fb_helper_remove_conflicting_framebuffers(ap, "lisuan", primary);
	kfree(ap);

	return 0;
}
#endif

int lisuan_pci_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	struct drm_device *ddev;
	int ret = 0, retry = 0, i;
	struct lisuan_device *ls_dev;

	ls_dev = (struct lisuan_device *)kvcalloc(1, sizeof(struct lisuan_device), GFP_KERNEL);

	/* Get rid of things like offb */

#if DRM_VERSION_CODE >= KERNEL_VERSION(6,12,0)
	ret = aperture_remove_conflicting_pci_devices(pdev, lisuan_drm_driver.name);
#elif DRM_VERSION_CODE >= KERNEL_VERSION(5,15,0)
	ret = drm_aperture_remove_conflicting_pci_framebuffers(pdev, &lisuan_drm_driver);
#elif DRM_VERSION_CODE >= KERNEL_VERSION(5,5,0)
	ret = drm_fb_helper_remove_conflicting_pci_framebuffers(pdev, "lisuan");
#elif DRM_VERSION_CODE >= KERNEL_VERSION(4,20,0)
	ret = drm_fb_helper_remove_conflicting_pci_framebuffers(pdev, 0, "lisuan");
#else
	ret = lisuan_kick_out_firmware_fb(pdev);
#endif
	if (ret)
		return ret;

	ddev = drm_dev_alloc(&lisuan_drm_driver, &pdev->dev);
	if (IS_ERR(ddev))
		return PTR_ERR(ddev);

	ret = pci_enable_device(pdev);
	if (ret) {
		drm_dev_put(ddev);
		return ret;
	}

	ls_dev->pdev = pdev;
	ls_dev->ddev = ddev;

	for(i = gem_priority_unused; i < gem_priority_count; i++)
	{
		INIT_LIST_HEAD(&ls_dev->list_lru[i]);
	}

	mutex_init(&ls_dev->lock.lock);

#if DRM_VERSION_CODE < KERNEL_VERSION(5,15,0)
	ddev->pdev = pdev;
#endif

	pci_set_drvdata(pdev, ddev);
	lisuan_kms_drm_load(ls_dev, ent->driver_data);
	atomic_set(&ls_dev->pf_finish_reset, 1);

retry_init:
	ret = drm_dev_register(ddev, ent->driver_data);
	if (ret == -EAGAIN && ++retry <= 3) {
		gf_info("retry init %d\n", retry);
		/* Don't request EX mode too frequently which is attacking */
		gf_msleep(5000);
		goto retry_init;
	} else if (ret) {
		pci_disable_device(pdev);
		drm_dev_put(ddev);
		return ret;
	}
	return 0;
}

static void lisuan_pci_remove(struct pci_dev *pdev)
{
	struct drm_device *drm_dev = pci_get_drvdata(pdev);
	struct lisuan_device *ls_device=(struct lisuan_device *)drm_dev->dev_private;
	int i=0;

	while(ls_device->is_virtfn && 
		!atomic_read(&ls_device->pf_finish_reset)){
		gf_msleep(5);
	}

	lisuan_fini_ring(ls_device);
	lisuan_fini_ring_fence_thread(ls_device);

	if (ls_device->fbdev)
		lisuan_fbdev_fini(ls_device);

	if (!ls_device->hwctl.no_video) {
		hantroenc_cleanup(ls_device);
		hantrodec_cleanup(ls_device);
	}

	lisuan_fini_memory_manager(ls_device);

	lisuan_fini_procid_mgr(ls_device);

	gf_deinit_cmd_template(&ls_device->rb_cmd_template);

	gf_kmdEvent_log_exit(ls_device);
	//gf_kmdEvent_log_control_exit();

	gf_gpuusage_exit(ls_device);

	//TODO irq & kms fini
	gf_irq_fini(ls_device);

	if (ls_device->is_virtfn)
		gf_kms_virt_deinit(ls_device);
	else
		gf_kms_deinit(ls_device);

	if (ls_device->mmio_base)
		iounmap(ls_device->mmio_base);

	if (ls_device->rom_base)
		iounmap(ls_device->rom_base);

	mutex_destroy(&ls_device->file_lock.lock);
	mutex_destroy(&ls_device->mailbox_lock.lock);
	mutex_destroy(&ls_device->lock.lock);

	drm_dev_unplug(drm_dev);

	pci_disable_device(pdev);

	pci_set_drvdata(pdev, NULL);

	kvfree(ls_device);
}

static struct pci_driver lisuan_pci_driver = {
	.name = "lisuan",
	.id_table = pciidlist,
	.probe = lisuan_pci_probe,
	.remove = lisuan_pci_remove,
	.shutdown = lisuan_pci_shutdown,
	.driver.pm = &lisuan_pm_ops,
};

#if DRM_VERSION_CODE < KERNEL_VERSION(5,17,0)
static bool ls_nomodeset = false;
static int __init lisuan_nomodeset(char *str)
{
	ls_nomodeset = true;
    gf_info("lisuan_nomodeset enabled\n");
	return 1;
}
__setup("nomodeset", lisuan_nomodeset);
#endif

static int __init lisuan_init(void)
{
	int ret = 0;

#if DRM_VERSION_CODE >= KERNEL_VERSION(5,17,0)
	if (drm_firmware_drivers_only())
		return -EINVAL;
#else
	if (ls_nomodeset)
		return -EINVAL;
#endif

	gf_info("VERSION:%s", PACKAGE_VERSION);

	ret = lisuan_sync_init();
	if (ret)
		goto error_sync;

	ret = lisuan_sched_fence_slab_init();
	if (ret)
		goto error_fence;
	ret = lisuan_ring_fence_init();
	if (ret)
		goto error_ring_fence;

	return pci_register_driver(&lisuan_pci_driver);

error_ring_fence:
	lisuan_sched_fence_slab_fini();

error_fence:
	lisuan_sync_fini();
error_sync:
	return ret;
}

static void __exit lisuan_exit(void)
{

	pci_unregister_driver(&lisuan_pci_driver);

	lisuan_ring_fence_init();
	lisuan_sched_fence_slab_fini();
	lisuan_sync_fini();

	gf_debug("exit gf gpu\n");
}

module_init(lisuan_init);
module_exit(lisuan_exit);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
