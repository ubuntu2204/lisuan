/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#include "lisuan_device.h"
#include "gf_irq.h"
#include "gf_kms_dm.h"
#include "gf_kms_pipe_crc.h"
#include "gf_os_interface.h"
#include "gf_hw_misc.h"

#include "subsys.h"
#include "vcx_normal_driver.h"
#include "vcx_vcmd_driver.h"
#include "gf_kmd_event.h"
#include "lisuan_trace.h"

#include <drm/drm_edid.h>
#include <drm/drm_fb_helper.h>

#define MIN_VECS 2
#define MAX_VECS 2

void gf_irq_reset(struct lisuan_device *ls_dev)
{
//TODO:maybe same with the preinstall, disable the hw interrupt,reset the hw bit

	disp_intr_t disp_intr = { 0 };

	//clear dpe interrupt status first
	dm_intr_get_status(ls_dev->adapter);

	disp_intr.info.Vsynca = 1;
	disp_intr.info.Vsyncb = 1;
	disp_intr.info.Vsyncc = 1;
	disp_intr.info.Vsyncd = 1;
	disp_intr.info.Codec1 = 1;
	disp_intr.info.Dpa_Hpd = 1;
	disp_intr.info.Dpa_Ephy = 1;
	disp_intr.info.Dpa_Linktraining = 1;
	disp_intr.info.Dpb_Hpd = 1;
	disp_intr.info.Dpb_Ephy = 1;
	disp_intr.info.Dpb_Linktraining = 1;
	disp_intr.info.Dpc_Hpd = 1;
	disp_intr.info.Dpc_Ephy = 1;
	disp_intr.info.Dpc_Linktraining = 1;
	disp_intr.info.Dpd_Hpd = 1;
	disp_intr.info.Dpd_Ephy = 1;
	disp_intr.info.Dpd_Linktraining = 1;

	gf_interrupt_disable(ls_dev->mmio_base, ls_dev->is_virtfn);
	dm_intr_enable(ls_dev->adapter, 0, disp_intr.intr);
}

static void gf_dp_read_status(struct lisuan_device *gdev, int connector_index)
{
	unsigned char val;
	struct connector_t *gconnector = &gdev->adapter->connectors[connector_index];
	//read 200-205h for ack hpd, 218h for test request type

	gconnector->ddc_func.aux_readb(gconnector, 0x200, &val);
	//gf_info("read success, 200h val 0x%x\n", val);
	gdev->adapter->connectors[connector_index].dp_hpd_status.sink_count = val;

	gconnector->ddc_func.aux_readb(gconnector, 0x201, &val);
	//gf_info("read success, 201h val 0x%x\n", val);
	gdev->adapter->connectors[connector_index].dp_hpd_status.irq_vector = val;

	gconnector->ddc_func.aux_readb(gconnector, 0x202, &val);
	//gf_info("read success, 202h val 0x%x\n", val);
	gdev->adapter->connectors[connector_index].dp_hpd_status.dp_lane0_1_status = val;

	gconnector->ddc_func.aux_readb(gconnector, 0x203, &val);
	//gf_info("read success, 203h val 0x%x\n", val);
	gdev->adapter->connectors[connector_index].dp_hpd_status.dp_lane2_3_status = val;

	gconnector->ddc_func.aux_readb(gconnector, 0x204, &val);
	//gf_info("read success, 204h val 0x%x\n", val);
	gdev->adapter->connectors[connector_index].dp_hpd_status.dp_lane_align = val;

	gconnector->ddc_func.aux_readb(gconnector, 0x205, &val);
	//gf_info("read success, 205h val 0x%x\n", val);
	gdev->adapter->connectors[connector_index].dp_hpd_status.dp_sink_status = val;

	gconnector->ddc_func.aux_readb(gconnector, 0x218, &val);
	//gf_info("read success, 218h val 0x%x\n", val);
	gdev->adapter->connectors[connector_index].dp_hpd_status.auto_test_req = val;

	//read 2h for sink lane count use for link training maintenance, see autotest_need_link_training
	gconnector->ddc_func.aux_readb(gconnector, 0x2, &val);
	//gf_info("read success, 2h val 0x%x\n", val);
	gdev->adapter->connectors[connector_index].dp_hpd_status.lane_count = val & 0xF;

}

/* Compliance test status bits  */
#define LS_DP_RESOLUTION_SHIFT_MASK	0
#define LS_DP_RESOLUTION_PREFERRED	(1 << LS_DP_RESOLUTION_SHIFT_MASK)
#define LS_DP_RESOLUTION_STANDARD	(2 << LS_DP_RESOLUTION_SHIFT_MASK)
#define LS_DP_RESOLUTION_FAILSAFE	(3 << LS_DP_RESOLUTION_SHIFT_MASK)

static unsigned char connector_dp_autotest_video_pattern(struct connector_t *gconnector)
{
	struct drm_connector *connector = &gconnector->base;
	struct connector_state_t *gconnector_state = dconnector_state_to_gconnector(connector->state);

	gconnector_state->compliance.test_active = true;

	return 0;
}

static void gf_hpd_event_handle(struct lisuan_device *gdev, int connector_index)
{
	unsigned char irq_vector = 0;
	unsigned char auto_test_req, dp_lane_align;
	int ret = 0, index = 0;
	struct connector_t *gconnector = &gdev->adapter->connectors[connector_index];

	gconnector->ddc_func.irq_event(gconnector);
	connector_dp_autotest_video_pattern(gconnector);

	return;
}

static void gf_hpd_set_state(struct lisuan_device *gdev, disp_intr_t disp_intr)
{
	int hpd_irq_mask = 0;

	if (!gdev->adapter->connectors || gdev->adapter->connector_num < 4)
	{
		gf_error("gf_hpd_set_state, invalid connector, connector_num[%d]\n",
			gdev->adapter->connector_num);
		return;
	}

	//gf_info ("%s, intr[0x%x]\n", __func__, disp_intr.intr);

	hpd_irq_mask |= disp_intr.info.Dpa_Hpd ? (1 << 0) : 0;
	hpd_irq_mask |= disp_intr.info.Dpb_Hpd ? (1 << 1) : 0;
	hpd_irq_mask |= disp_intr.info.Dpc_Hpd ? (1 << 2) : 0;
	hpd_irq_mask |= disp_intr.info.Dpd_Hpd ? (1 << 3) : 0;

	atomic_or(hpd_irq_mask, &gdev->adapter->hpd_mask);
}

static void gf_hpd_handle(struct lisuan_device *gdev)
{
	disp_get_hpd_state_t hpd = {0};
	int hpd_handle_mask = 0, hpd_in_out_mask = 0, drm_connector_changed = 0, index = 0;

	while ((index = atomic_cmpxchg(&gdev->adapter->hpd_mask, hpd_handle_mask, 0)) != hpd_handle_mask)
	{
		hpd_handle_mask = index;
	}

	for (index = 0; index < gdev->adapter->connector_num; ++index) {
		if (hpd_handle_mask & (1 << index))
		{
			hpd.connector_index = index;
			hpd.hpd_state = DISP_HPD_TYPE_IDLE;
			dm_get_hpd_state(gdev->adapter, &hpd);

			if (DISP_HPD_TYPE_EVENT == hpd.hpd_state) {
				gf_info("hpd %d irq event\n", index);
				gf_hpd_event_handle(gdev, index);
			} else if (DISP_HPD_TYPE_PLUG_IN == hpd.hpd_state || DISP_HPD_TYPE_PLUG_OUT == hpd.hpd_state) {
				gf_info("hpd %d %s\n", index, DISP_HPD_TYPE_PLUG_IN == hpd.hpd_state ? "in" : "out");
				//this interface for specific connector is defined in newer kernel
				//drm_connector_helper_hpd_irq_event(&gdev->adapter->connectors[index].base);
				hpd_in_out_mask |= (1 << index);
			}
		}
	}

	if (hpd_in_out_mask)
	{
		drm_connector_changed = drm_helper_hpd_irq_event(gdev->ddev);
		gf_debug("gf_hpd_handle, hpd_handle_mask[%x] hpd_in_out_mask[%x] drm_connector_changed[%u]\n",
			hpd_handle_mask, hpd_in_out_mask, drm_connector_changed);

		/* Patch for KylinV10 keyboard switch tty: alt + ctrl + F1(F2/F3/F4...)
		 * Desktop work on drm, is OK. fb connector state need extra update.
		 * Fb update when drm_device master exit, so we could update it in lastclose(drm_driver) through fb client hotplug.
		 * But legacy version will not call lastclose, no chance to wait master exit.
		 * So, ignore master state, direct update fb for legacy version.
		 */
#if DRM_VERSION_CODE < KERNEL_VERSION(5,7,0)
		drm_fb_helper_hotplug_event (gdev->ddev->fb_helper);
#endif
	}
}

static void gf_vsync_handle_by_index(struct drm_device *ddev, int crtc_index)
{
	struct lisuan_device *gdev = ddev->dev_private;
	struct drm_crtc *crtc;

	crtc = drm_crtc_from_index(gdev->ddev, crtc_index);
	drm_crtc_handle_vblank(crtc);
	gf_kms_crtc_get_crc(crtc);
	dm_check_underflow(gdev->adapter);
	gf_record_evt_vsync(gdev,crtc_index);
	trace_lisuan_vsync_index(crtc_index);
	// gf_gpuusage_vsync(crtc, crtc_index);
}

static irqreturn_t gf_multi_vsync_handle_a(int irq, void *arg)
{
	struct drm_device *ddev = (struct drm_device *)arg;
	struct lisuan_device *gdev = ddev->dev_private;

	dm_intr_clear_vsync_status(gdev->adapter, 0);
	gf_vsync_handle_by_index(ddev, 0);

	return IRQ_HANDLED;
}

static irqreturn_t gf_multi_vsync_handle_b(int irq, void *arg)
{
	struct drm_device *ddev = (struct drm_device *)arg;
	struct lisuan_device *gdev = ddev->dev_private;

	dm_intr_clear_vsync_status(gdev->adapter, 1);

	gf_vsync_handle_by_index(ddev, 1);

	return IRQ_HANDLED;
}

static irqreturn_t gf_multi_vsync_handle_c(int irq, void *arg)
{
	struct drm_device *ddev = (struct drm_device *)arg;
	struct lisuan_device *gdev = ddev->dev_private;

	dm_intr_clear_vsync_status(gdev->adapter, 2);

	gf_vsync_handle_by_index(ddev, 2);

	return IRQ_HANDLED;
}

static irqreturn_t gf_multi_vsync_handle_d(int irq, void *arg)
{
	struct drm_device *ddev = (struct drm_device *)arg;
	struct lisuan_device *gdev = ddev->dev_private;

	dm_intr_clear_vsync_status(gdev->adapter, 3);

	gf_vsync_handle_by_index(ddev, 3);

	return IRQ_HANDLED;
}

static void gf_vsync_handle(struct lisuan_device *gdev, disp_intr_t disp_intr)
{
	if (disp_intr.info.Vsynca) {
		gf_vsync_handle_by_index(gdev->ddev, 0);
	}

	if (disp_intr.info.Vsyncb) {
		gf_vsync_handle_by_index(gdev->ddev, 1);
	}

	if (disp_intr.info.Vsyncc) {
		gf_vsync_handle_by_index(gdev->ddev, 2);
	}

	if (disp_intr.info.Vsyncd) {
		gf_vsync_handle_by_index(gdev->ddev, 3);
	}
}

static void gf_sgnt_set_state(struct lisuan_device *gdev, disp_intr_t disp_intr)
{
	gdev->adapter->flag.bits.sgnt_intr_a = disp_intr.info.Pipea_Sgnt ? 1 : 0;
	gdev->adapter->flag.bits.sgnt_intr_b = disp_intr.info.Pipeb_Sgnt ? 1 : 0;
	gdev->adapter->flag.bits.sgnt_intr_c = disp_intr.info.Pipec_Sgnt ? 1 : 0;
	gdev->adapter->flag.bits.sgnt_intr_d = disp_intr.info.Piped_Sgnt ? 1 : 0;
}

static void gf_sgnt_clear_state(struct lisuan_device *gdev, int hpd_bits)
{
	gdev->adapter->flag.bits.sgnt_intr_a = 0;
	gdev->adapter->flag.bits.sgnt_intr_b = 0;
	gdev->adapter->flag.bits.sgnt_intr_c = 0;
	gdev->adapter->flag.bits.sgnt_intr_d = 0;
}

static void gf_sgnt_handle(struct lisuan_device *gdev)
{
	unsigned int crc[8] = {0};
	int idx_pipe = 0, sgnt_mask = 0;
	unsigned int frame_cnt;

	/* Sgnt intr check need: sgnt_en && sgnt_intr_en
	 * Sgnt intr check at vsync (4T after vde descending edge)
	 * Sgnt intr trigger when mismatch between Sgnt_Gold_x and Sgnt_x
	 */
	sgnt_mask |= gdev->adapter->flag.bits.sgnt_intr_a ? (1 << 0) : 0;
	sgnt_mask |= gdev->adapter->flag.bits.sgnt_intr_b ? (1 << 1) : 0;
	sgnt_mask |= gdev->adapter->flag.bits.sgnt_intr_c ? (1 << 2) : 0;
	sgnt_mask |= gdev->adapter->flag.bits.sgnt_intr_d ? (1 << 3) : 0;
	gf_info("%s, sig intr bits[%d %d %d %d]\n",
		__func__, gdev->adapter->flag.bits.sgnt_intr_a, gdev->adapter->flag.bits.sgnt_intr_b,
		gdev->adapter->flag.bits.sgnt_intr_c, gdev->adapter->flag.bits.sgnt_intr_d);

	for (idx_pipe = 0; idx_pipe < gdev->adapter->crtc_num; idx_pipe++) {
		if ((1 << idx_pipe) & sgnt_mask) { // only print mismatch sgnt intr
			frame_cnt = dm_crtc_get_vblank_counter(gdev->adapter, idx_pipe);
			dm_pipe_get_sgnt_value(gdev->adapter, idx_pipe, crc, 8);
			gf_error("idx[%d], frame_cnt[%u], golden_dpe[0x%x 0x%x 0x%x 0x%x][0x%x 0x%x 0x%x 0x%x]\n",
				idx_pipe, frame_cnt, crc[0], crc[1], crc[2], crc[3], crc[4], crc[5], crc[6], crc[7]);
		}
	}
}

static irqreturn_t gf_irq_handler(int irq, void *arg)
{
	struct drm_device *ddev = (struct drm_device *)arg;
	struct lisuan_device *gdev = ddev->dev_private;
	disp_intr_t disp_intr = { 0 };
	struct irq_Int_Reg1 shadow_reg1 = { 0 };
	struct irq_Int_Reg1 reg1 = { 0 };
	struct irq_Int_Reg2 shadow_reg2 = { 0 };
	int retry = 0;

	gf_get_hw_irq(gdev->mmio_base, &shadow_reg1, &reg1, &shadow_reg2);
	if(gdev->vec_num == 1 && reg1.uint != shadow_reg1.uint)
	{
		retry = 1;
	}

	if (gdev->vec_num == 1 && shadow_reg1.Phb == 1) {
		//Fence interrupt
		tasklet_schedule(&gdev->fenceirq_work);
	} else{
		shadow_reg1.Phb = 0;
	}

	if (shadow_reg2.Pf_To_Vf_Irq && gdev->is_virtfn) {
		schedule_work(&gdev->mailbox_work);
	}

	if (shadow_reg1.MailBox) {
		gf_error("Therm Hit High Temperature Threshold\n");
		gf_clear_mail_box_irq_flag(gdev->mmio_base);
		schedule_work(&gdev->therm_work);
	}

	if (shadow_reg1.Dpe) {
		disp_intr.intr = dm_intr_get_status(gdev->adapter);
		if (disp_intr.info.Vsynca || disp_intr.info.Vsyncb || disp_intr.info.Vsyncc || disp_intr.info.Vsyncd) {
			gf_vsync_handle(gdev, disp_intr);
		}

		if (disp_intr.info.Dpa_Hpd || disp_intr.info.Dpb_Hpd || disp_intr.info.Dpc_Hpd || disp_intr.info.Dpd_Hpd) {
			gf_hpd_set_state(gdev, disp_intr);
			schedule_work(&gdev->hpd_work);
		}

		if (disp_intr.info.Codec1) {
			schedule_work(&gdev->hdaudio_work);
		}

		if (disp_intr.info.Pipea_Sgnt || disp_intr.info.Pipeb_Sgnt || disp_intr.info.Pipec_Sgnt || disp_intr.info.Piped_Sgnt) {
			gf_sgnt_set_state(gdev, disp_intr);
			schedule_work(&gdev->sgnt_work);
		}
	}

	if (shadow_reg1.Mhu_PageError) {
		gf_handle_mhb_irq(gdev->mmio_base, gdev->is_virtfn, gdev->virtfn_id, gdev->mhb_num);
	}

	if (shadow_reg1.vdec_0_va0) {
		lisuan_dec_vcmd_clear_irq_status(0, gdev->vcmd_mgr);
		schedule_work(&gdev->dec0_vcmd_work);
	}
	if (shadow_reg1.vdec_1_va0) {
		lisuan_dec_vcmd_clear_irq_status(1, gdev->vcmd_mgr);
		schedule_work(&gdev->dec1_vcmd_work);
	}

	if (shadow_reg1.venc_va0) {
		lisuan_enc_vcmd_clear_irq_status(0, gdev->enc_vcmd_mgr);
		schedule_work(&gdev->enc0_vcmd_work);
	}
	if (shadow_reg1.venc_va4) {
		lisuan_enc_vcmd_clear_irq_status(1, gdev->enc_vcmd_mgr);
		schedule_work(&gdev->im_vcmd_work);
	}

	gf_clear_hw_irq(gdev->mmio_base, &shadow_reg1, &shadow_reg2, gdev->is_virtfn);

	//vf do not has vsync, not try again
	if(retry && !gdev->is_virtfn)
		gf_irq_handler(irq, arg);

	return IRQ_HANDLED;
}

static irqreturn_t gf_irq_handler_fence(int irq, void *arg)
{
	//TODO:fence interrupt handler
	struct drm_device *ddev = (struct drm_device *)arg;
	struct lisuan_device *gdev = ddev->dev_private;

	if(gdev->vec_num > 1){
		tasklet_schedule(&gdev->fenceirq_work);
		gf_clear_fence_irq(gdev->mmio_base);
	}
	return IRQ_HANDLED;
}

void gf_irq_postinstall(struct lisuan_device *dev_priv)
{
	//TODO:enable the hw interrupt,set the hw bit
	disp_intr_t disp_intr = { 0 };

	disp_intr.info.Vsynca = 1;
	disp_intr.info.Vsyncb = 1;
	disp_intr.info.Vsyncc = 1;
	disp_intr.info.Vsyncd = 1;
	disp_intr.info.Codec1 = 1;
	disp_intr.info.Dpa_Hpd = 1;
	disp_intr.info.Dpa_Ephy = 1;
	disp_intr.info.Dpa_Linktraining = 1;
	disp_intr.info.Dpb_Hpd = 1;
	disp_intr.info.Dpb_Ephy = 1;
	disp_intr.info.Dpb_Linktraining = 1;
	disp_intr.info.Dpc_Hpd = 1;
	disp_intr.info.Dpc_Ephy = 1;
	disp_intr.info.Dpc_Linktraining = 1;
	disp_intr.info.Dpd_Hpd = 1;
	disp_intr.info.Dpd_Ephy = 1;
	disp_intr.info.Dpd_Linktraining = 1;

	dm_intr_enable(dev_priv->adapter, 1, disp_intr.intr);

/*
* multi vsync enable: vsync a use vector 3, vysnc b use vector 4, vsync c use vector 5, vsync d use vector 6
* multi sync disbale: vsync a/b/c/d merged into the overall dpe interrupt
* we need tell this to dpe hw, then dpe will select send the interrupt to ppa directly(enable) or send to dpe(disable)
*/
	if (dev_priv->vec_num >= 8) {
		dm_intr_multi_vsync_enable(dev_priv->adapter, 1);
	} else {
		dm_intr_multi_vsync_enable(dev_priv->adapter, 0);
	}

	gf_interrupt_enable(dev_priv->mmio_base, dev_priv->is_virtfn);
}

static void kms_hdaudio_worker(struct work_struct *work)
{
	struct lisuan_device *dev_priv = container_of(work, struct lisuan_device, hdaudio_work);

	dm_hda_handle(dev_priv->adapter);
}

static void kms_hpd_worker(struct work_struct *work)
{
	struct lisuan_device *dev_priv = container_of(work, struct lisuan_device, hpd_work);

	gf_hpd_handle(dev_priv);
}

static void kms_sgnt_worker(struct work_struct *work)
{
	struct lisuan_device *dev_priv = container_of(work, struct lisuan_device, sgnt_work);

	gf_sgnt_handle(dev_priv);
}

static void gf_kms_init_work(struct lisuan_device *dev_priv)
{
	INIT_WORK(&dev_priv->hdaudio_work, kms_hdaudio_worker);
	INIT_WORK(&dev_priv->hpd_work, kms_hpd_worker);
	INIT_WORK(&dev_priv->sgnt_work, kms_sgnt_worker);
}

static void gf_kms_cancel_work(struct lisuan_device *dev_priv)
{
	if (work_pending(&dev_priv->hdaudio_work))
		cancel_work_sync(&dev_priv->hdaudio_work);
	if (work_pending(&dev_priv->hpd_work))
		cancel_work_sync(&dev_priv->hpd_work);
	if (work_pending(&dev_priv->sgnt_work))
		cancel_work_sync(&dev_priv->sgnt_work);
}

void gf_irq_init(struct lisuan_device *dev_priv)
{
	struct drm_device *ddev = dev_priv->ddev;
	int force_single_vec = 0;
#ifdef __loongarch64
	// On certain LoongArch CPUs (like 3d6000), we need to force limit single vector mode
	// Otherwise, the gf_irq_handler will not run correctly, which leads to a vblank wait timeout
	struct cpuinfo_loongarch *c = &current_cpu_data;
	unsigned int id = c->processor_id;

	if((id & PRID_PRODUCT_MASK) == 0x10)		//Loongson 3C6000 processor_id: 0x14d010
	{
		force_single_vec = 1;
		gf_info("Loongson Processor 3C6000 detect, force single vector mode\n");
	}
#endif
	gf_kms_init_work(dev_priv);

	if(force_single_vec)
	{
		dev_priv->vec_num = pci_alloc_irq_vectors(dev_priv->pdev, 1, 1, PCI_IRQ_MSI);
	}
	else
	{
		dev_priv->vec_num = pci_alloc_irq_vectors(dev_priv->pdev, MIN_VECS, MAX_VECS, PCI_IRQ_MSI);
		if (dev_priv->vec_num <= 0) {
			dev_priv->vec_num = pci_alloc_irq_vectors(dev_priv->pdev, 1, 1, PCI_IRQ_MSI);
		}
	}

	if(dev_priv->vec_num <= 0)
	{
		gf_error("pci_alloc_irq_vectors error\n");
		return;
	}
	gf_debug("irq vec:%d\n", dev_priv->vec_num);
	gf_irq_install(dev_priv);

}

void gf_irq_fini(struct lisuan_device *dev_priv)
{
	gf_irq_uninstall(dev_priv);
	pci_free_irq_vectors(dev_priv->pdev);
	gf_kms_cancel_work(dev_priv);
}

int gf_irq_install(struct lisuan_device *ls_dev)
{
	int irq, ret, flag;
	ls_dev->installed = true;
	gf_irq_reset(ls_dev);

#ifdef __loongarch64
	// On the Loongson architecture, bit IRQF_NOBALANCING needs to be added to the interrupt flag
	// otherwise the interrupt address may be changed by other programs, such as irqbalance
	flag = IRQF_SHARED | IRQF_NOBALANCING;
#else
	flag = IRQF_SHARED;
#endif

	if (ls_dev->vec_num == 1) {
		irq = pci_irq_vector(ls_dev->pdev, 0);
		ret = request_irq(irq, gf_irq_handler, flag, ls_dev->ddev->driver->name, ls_dev->ddev);
		if (ret < 0) {
			ls_dev->installed = false;
			return ret;
		}
		ls_dev->irq[0] = irq;
	}

	if (ls_dev->vec_num >= 2) {
		irq = pci_irq_vector(ls_dev->pdev, 0);
		ret = request_irq(irq, gf_irq_handler_fence, flag, ls_dev->ddev->driver->name, ls_dev->ddev);
		if (ret < 0) {
			ls_dev->installed = false;
			return ret;
		}
		ls_dev->irq[0] = irq;

		irq = pci_irq_vector(ls_dev->pdev, 1);
		ret = request_irq(irq, gf_irq_handler, flag, ls_dev->ddev->driver->name, ls_dev->ddev);
		if (ret < 0) {
			ls_dev->installed = false;
			return ret;
		}
		ls_dev->irq[1] = irq;
	}

	if (ls_dev->vec_num >= 8) {
		irq = pci_irq_vector(ls_dev->pdev, 3);
		ret = request_irq(irq, gf_multi_vsync_handle_a, flag, ls_dev->ddev->driver->name, ls_dev->ddev);
		if (ret < 0) {
			ls_dev->installed = false;
			return ret;
		}
		ls_dev->irq[3] = irq;

		irq = pci_irq_vector(ls_dev->pdev, 4);
		ret = request_irq(irq, gf_multi_vsync_handle_b, flag, ls_dev->ddev->driver->name, ls_dev->ddev);
		if (ret < 0) {
			ls_dev->installed = false;
			return ret;
		}

		ls_dev->irq[4] = irq;
		irq = pci_irq_vector(ls_dev->pdev, 5);
		ret = request_irq(irq, gf_multi_vsync_handle_c, flag, ls_dev->ddev->driver->name, ls_dev->ddev);
		if (ret < 0) {
			ls_dev->installed = false;
			return ret;
		}
		ls_dev->irq[5] = irq;

		irq = pci_irq_vector(ls_dev->pdev, 6);
		ret = request_irq(irq, gf_multi_vsync_handle_d, flag, ls_dev->ddev->driver->name, ls_dev->ddev);
		if (ret < 0) {
			ls_dev->installed = false;
			return ret;
		}
		ls_dev->irq[6] = irq;
	}

	return 0;
}

void gf_irq_uninstall(struct lisuan_device *dev_priv)
{
	if (!dev_priv->installed)
		return;

	dev_priv->installed = false;

	free_irq(dev_priv->irq[0], dev_priv->ddev);
	
	if (dev_priv->vec_num >= 2)
		free_irq(dev_priv->irq[1], dev_priv->ddev);

	if (dev_priv->vec_num >= 8) {
		free_irq(dev_priv->irq[3], dev_priv->ddev);
		free_irq(dev_priv->irq[4], dev_priv->ddev);
		free_irq(dev_priv->irq[5], dev_priv->ddev);
		free_irq(dev_priv->irq[6], dev_priv->ddev);
	}

	gf_irq_reset(dev_priv);
}
