/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#include "gf_kms_dm.h"
#include "display.h"
#include "gf_os_interface.h"
#include "lisuan_device.h"

static unsigned char read_u8(struct display_adapter *adapter, unsigned int offset)
{
	return gf_read_mmio8(adapter->mmio_base + offset);
}

static void write_u8(struct display_adapter *adapter, unsigned int offset, unsigned char value)
{
	gf_write_mmio8(adapter->mmio_base + offset, value);
}

static unsigned short read_u16(struct display_adapter *adapter, unsigned int offset)
{
	return gf_read_mmio16(adapter->mmio_base + offset);
}

static void write_u16(struct display_adapter *adapter, unsigned int offset, unsigned short value)
{
	gf_write_mmio16(adapter->mmio_base + offset, value);
}

static unsigned int read_u32(struct display_adapter *adapter, unsigned int offset)
{
	return gf_read_mmio32(adapter->mmio_base + offset);
}

static void write_u32(struct display_adapter *adapter, unsigned int offset, unsigned int value)
{
	gf_write_mmio32(adapter->mmio_base + offset, value);
}

static void dm_udelay(unsigned long long usecs_num)
{
	unsigned long long msecs = usecs_num;
	unsigned long usecs = do_div(msecs, 1000);

	if(msecs != 0)mdelay(msecs);
	if(usecs != 0)udelay(usecs);
}

static void dm_printk(unsigned int msgtype, const char *fmt, ...)
{
	char buffer[256];
	va_list marker;

	va_start(marker, fmt);
	vsnprintf(buffer, 256, fmt, marker);
	va_end(marker);

	printk(KERN_INFO KERN_CONT "%s", buffer);
}

unsigned char dm_read_rom_byte(struct display_adapter *adapter, int offset)
{
	return *(unsigned char*)(adapter->rom_base + offset);
}

int dm_hw_init(struct display_adapter *adapter)
{
    int ret = display_device_hw_init(adapter->display_device);
    return (ret != DISP_OK) ? -1 : 0;
}

int dm_init(struct display_adapter *adapter)
{
	struct lisuan_device *gdevice = adapter->gdevice;
	disp_device_init_t disp_device_init = {0};
	disp_callback_t callback = {0};
	int ret = 0;

	disp_device_init.pdisplay_adapter = adapter;
	disp_device_init.device_id = DISP_DEVICE_G100;
	disp_device_init.flag.val = 0;
	disp_device_init.flag.bits.pz1_config = gdevice->hwctl.pz1;
	disp_device_init.flag.bits.pz1_capture_pipe = gdevice->hwctl.no_encoder;
	disp_device_init.flag.bits.dp_cts = gdevice->hwctl.dp_cts;
	disp_device_init.flag.bits.mode_8k = gdevice->hwctl.mode_8k;
	disp_device_init.flag.bits.hau_asp_8ch = 1;

	callback.pdisp_fn_vsnprintf = (void*)vsnprintf;
	callback.pdisp_fn_print = (void*)dm_printk;
	callback.pdisp_fn_malloc = (void*)gf_calloc;
	callback.pdisp_fn_free = (void*)gf_free;
	callback.pdisp_fn_memset = (void*)gf_memset;
	callback.pdisp_fn_memcpy = (void*)gf_memcpy;
	callback.pdisp_fn_memcmp = (void*)gf_memcmp;
	callback.pdisp_fn_read_u8 = (void*)read_u8;
	callback.pdisp_fn_write_u8 = (void*)write_u8;
	callback.pdisp_fn_read_u16 = (void*)read_u16;
	callback.pdisp_fn_write_u16 = (void*)write_u16;
	callback.pdisp_fn_read_u32 = (void*)read_u32;
	callback.pdisp_fn_write_u32 = (void*)write_u32;
	callback.pdisp_fn_strcmp = (void*)strcmp;
	callback.pdisp_fn_strcpy = (void*)strcpy;
	callback.pdisp_fn_strncmp = (void*)strncmp;
	callback.pdisp_fn_delay_us = (void*)dm_udelay;
	callback.pdisp_fn_read_rom_byte = (void*)dm_read_rom_byte;

	disp_device_init.pdisp_callback = &callback;

	ret = display_device_init(&disp_device_init);
	if (ret != DISP_OK)
		return -1;

	adapter->display_device = disp_device_init.pdisplay_device;

	return ret;
}

int dm_deinit(struct display_adapter *adapter)
{
	if (adapter->display_device)
		return display_device_deinit(adapter->display_device);

	return -1;
}

int dm_query_resource(struct display_adapter *adapter)
{
	disp_query_resource_t resource = {0};
	int ret = 0;

	ret = display_query_resource(adapter->display_device, &resource);

	if (ret != DISP_OK) {
		gf_printk(GF_DRV_ERROR, "get display resource fail\n");
		return ret;
	}

	adapter->crtc_num = resource.crtc_num;
	adapter->plane_num = resource.plane_num;
	adapter->pu_support = resource.pu_support;
	adapter->connector_support = resource.connector_support;

	return 0;
}

int dm_detect_connector(struct display_adapter *adapter, int to_detect, int to_fake, int* detected)
{
	disp_detect_connector_t detect_connector = {0};
	int ret = 0;

	detect_connector.connector_to_detect = to_detect;
	detect_connector.connector_to_fake = to_fake;

	ret = display_detect_connector(adapter->display_device, &detect_connector);
	if (ret != DISP_OK) {
		gf_printk(GF_DRV_ERROR, "detect conenctor 0x%x fail\n", to_detect);
		return ret;
	}

	*detected = detect_connector.connector_detected;
	return ret;
}

int dm_connector_get_modelist(struct display_adapter *adapter, struct connector_t *connector)
{
	disp_mode_list_t mode_list = {0};
	int ret = 0;

	if (!adapter || !connector)
		return -1;

	gf_memset(&mode_list, 0, sizeof(disp_mode_list_t));

	mode_list.connector_index = connector->index;

	ret = display_get_modelist(adapter->display_device, &mode_list);
	if (ret != DISP_OK) {
		gf_printk(GF_DRV_INFO, "get mode num fail\n");
		return ret;
	}

	connector->mode_num = mode_list.num;
	//find where to free later
	if (connector->modes) {
		gf_free(connector->modes);
		connector->modes = NULL;
	}

	connector->modes = gf_calloc(connector->mode_num * sizeof(disp_display_mode_t));

	mode_list.modes = connector->modes;
	ret = display_get_modelist(adapter->display_device, &mode_list);
	if (ret != DISP_OK) {
		gf_printk(GF_DRV_ERROR, "get mode fail\n");
		return ret;
	}

	return ret;
}

int dm_connector_get_edid(struct display_adapter *adapter, struct connector_t *connector)
{
	disp_get_edid_t get_edid = {0};
	int ret = 0;

	get_edid.connector_index = connector->index;
	get_edid.buf = connector->edid;
	get_edid.buf_size = 512;

	ret = display_get_edid(adapter->display_device, &get_edid);
	if (ret != DISP_OK)
		gf_printk(GF_DRV_ERROR, "get edid fail\n");

	return ret;
}

int dm_encoder_dpms(struct display_adapter *adapter, struct encoder_t *encoder, bool on)
{
	disp_encoder_dpms_t dpms = {0};
	disp_codec_para_t codec_para = {0};
	int ret = 0;

	dpms.index = encoder->index;
	dpms.enable = on ? DISP_POWER_ON : DISP_POWER_OFF;

	ret = display_encoder_dpms(adapter->display_device, dpms);

	if (ret != DISP_OK) {
		gf_printk(GF_DRV_ERROR, "encoder dpms fail\n");
	}

	codec_para.power = on ? DISP_POWER_ON : DISP_POWER_OFF;
	codec_para.encoder_index = encoder->index;
	codec_para.present = 1;
	codec_para.eld_valid = on;

	ret = display_codec_dpms(adapter->display_device, &codec_para);

	if (ret != DISP_OK) {
		gf_printk(GF_DRV_ERROR, "codec dpms fail\n");
	}

	return ret;
}

int dm_encoder_set_mode(struct display_adapter *adapter, struct encoder_t *encoder)
{
	disp_encoder_set_mode_t set_mode = {0};
	int ret = 0;

	set_mode.index = encoder->index;
	ret = display_encoder_set_mode(adapter->display_device, set_mode);

	if (ret != DISP_OK)
		gf_printk(GF_DRV_ERROR, "encoder set mode fail\n");

	return ret;
}

int dm_crtc_dpms(struct display_adapter *adapter, struct crtc_t *crtc, bool on)
{
	disp_crtc_dpms_t crtc_dpms = {0};
	int ret = 0;

	crtc_dpms.crtc_index = crtc->index;
	crtc_dpms.enable = on;
	crtc_dpms.sgnt_global_enable = adapter->flag.bits.sgnt_enable;

	ret = display_crtc_dpms(adapter->display_device, crtc_dpms);
	if (ret != DISP_OK)
		gf_printk(GF_DRV_ERROR, "crtc dpms fail");

	crtc->dpms = on;
	return ret;
}

static unsigned char crtc_set_mode_check_bpc(unsigned char bpc_mask)
{
	unsigned char bpc = 8;

	if (bpc_mask & (1 << DISP_COLOR_DEPTH_6))
		bpc = 8;
	else if (bpc_mask & (1 << DISP_COLOR_DEPTH_6))
		bpc = 6;
	else if (bpc_mask & (1 << DISP_COLOR_DEPTH_10))
		bpc = 10;
	else if (bpc_mask & (1 << DISP_COLOR_DEPTH_12))
		bpc = 12;
	else if (bpc_mask & (1 << DISP_COLOR_DEPTH_16))
		bpc = 16;
	else
		gf_printk(GF_DRV_INFO, "crtc_set_mode_check_bpc, hw_mode invalid bpc_mask[%x]!\n", bpc_mask);

	return bpc;
}

static void crtc_set_mode_check(pdisp_crtc_set_mode_t pset_mode, pdisp_display_mode_t hw_mode)
{
	unsigned char fmt_mask = hw_mode->output_format_mask;
	unsigned char mask_444 = hw_mode->output_444_bpc_mask;
	unsigned char mask_422 = hw_mode->output_422_bpc_mask;
	unsigned char mask_420 = hw_mode->output_420_bpc_mask;

	if (pset_mode->output_signal != DISP_COLOR_FORMAT_RGB444 ||
		pset_mode->bpc != 8 || pset_mode->colorimetry != DISP_COLORIMETRY_SRGB) {
		return; // CTS test mode, no need adjust.
	} else if (!fmt_mask) {
		gf_printk(GF_DRV_INFO, "crtc_set_mode_check, hw_mode no valid output_fmt!\n");
		return;
	} else if (fmt_mask & DISP_COLOR_FORMAT_RGB444) {
		pset_mode->bpc = crtc_set_mode_check_bpc(mask_444);
	} else if (!(fmt_mask & DISP_COLOR_FORMAT_RGB444)) {
		if (fmt_mask & DISP_COLOR_FORMAT_YCRCB444) {
			pset_mode->output_signal = DISP_COLOR_FORMAT_YCRCB444;
			pset_mode->bpc = crtc_set_mode_check_bpc(mask_444);
		} else if (fmt_mask & DISP_COLOR_FORMAT_YCRCB422) {
			pset_mode->output_signal = DISP_COLOR_FORMAT_YCRCB422;
			pset_mode->bpc = crtc_set_mode_check_bpc(mask_422);
		} else if (fmt_mask & DISP_COLOR_FORMAT_YCRCB420) {
			pset_mode->output_signal = DISP_COLOR_FORMAT_YCRCB420;
			pset_mode->bpc = crtc_set_mode_check_bpc(mask_420);
		} else {
			gf_printk(GF_DRV_INFO, "crtc_set_mode_check, hw_mode invalid fmt_mask[%x]!\n", fmt_mask);
		}
	}
}

int dm_crtc_set_mode(struct display_adapter *adapter, struct crtc_t *crtc,
                pdisp_display_mode_t mode, pdisp_display_mode_t hw_mode)
{
	disp_crtc_set_mode_t set_mode = {0};
	disp_cts_para_t cts_para = {0};
	struct connector_t *gconnector = NULL;
	struct connector_state_t *gstate = NULL;
	int ret = 0;

	set_mode.index = crtc->index;
	gf_memcpy(&set_mode.mode, mode, sizeof(disp_display_mode_t));
	gf_memcpy(&set_mode.hw_mode, hw_mode, sizeof(disp_display_mode_t));
	set_mode.output_signal = DISP_COLOR_FORMAT_RGB444;
	set_mode.colorimetry = 0;//for default SDR colorimetry
	set_mode.bpc = 8;

	gconnector = &(adapter->connectors[crtc->index]);
	gstate = dconnector_state_to_gconnector(gconnector->base.state);
	switch(gstate->output_signal)
	{
	    case DISP_COLOR_FORMAT_TYPE_RGB:
			set_mode.output_signal = DISP_COLOR_FORMAT_RGB444;
			break;
	    case DISP_COLOR_FORMAT_TYPE_YUV444:
			set_mode.output_signal = DISP_COLOR_FORMAT_YCRCB444;
			break;
	    case DISP_COLOR_FORMAT_TYPE_YUV422:
			set_mode.output_signal = DISP_COLOR_FORMAT_YCRCB422;
			break;
	    case DISP_COLOR_FORMAT_TYPE_YUV420:
			set_mode.output_signal = DISP_COLOR_FORMAT_YCRCB420;
			break;
	    default:
			gf_printk(GF_DRV_ERROR, "dm_crtc_set_mode, invalid output_signal[%u], use rgb\n", gstate->output_signal);
			set_mode.output_signal = DISP_COLOR_FORMAT_RGB444;
			break;
	}
	switch(gstate->color_depth)
	{
	    case DISP_COLOR_DEPTH_6:
			set_mode.bpc = 6;
			break;
	    case DISP_COLOR_DEPTH_8:
			set_mode.bpc = 8;
			break;
	    case DISP_COLOR_DEPTH_10:
			set_mode.bpc = 10;
			break;
	    case DISP_COLOR_DEPTH_12:
			set_mode.bpc = 12;
			break;
		case DISP_COLOR_DEPTH_16:
			set_mode.bpc = 16;
			break;
	    default:
			gf_printk(GF_DRV_ERROR, "dm_crtc_set_mode, invalid color_depth[%u], use 8bpc\n", gstate->color_depth);
			set_mode.bpc = 8;
			break;
	}
	set_mode.colorimetry = gstate->colorimetry;

	crtc_set_mode_check(&set_mode, hw_mode);

	ret = display_crtc_set_mode(adapter->display_device, set_mode);

	if(gstate->cts_dp.valid_bits)
	{
		cts_para.connector_index = crtc->index;
		cts_para.type = DISP_CTS_TYPE_DP;
		cts_para.para.dp.valid_bits = gstate->cts_dp.valid_bits;
		cts_para.para.dp.link_rate = gstate->cts_dp.link_rate;
		cts_para.para.dp.lane_count = gstate->cts_dp.lane_count;
		cts_para.para.dp.tps = gstate->cts_dp.tps;
		cts_para.para.dp.async_clock = gstate->cts_dp.async_clock;
		cts_para.para.dp.range_type = gstate->cts_dp.range_type;
		cts_para.para.dp.ycbcr_coefficients = gstate->cts_dp.ycbcr_coefficients;
		cts_para.para.dp.enhanced_frame = gstate->cts_dp.enhanced_frame;
		if(DISP_OK != display_set_cts(adapter->display_device, &cts_para))
			gf_printk(GF_DRV_ERROR, "dm_crtc_set_mode, display_set_cts failed\n");
	}

	if (ret != DISP_OK)
		gf_printk(GF_DRV_ERROR, "crtc set mode fail\n");

	return ret;
}

int dm_plane_implement(struct display_adapter *adapter, pdisp_plane_update_para_t param, unsigned char is_update)
{
	DISP_STATUS status = DISP_OK;

	if (!param) {
		gf_error("%s, param NULL!\n", __func__);
		return -1;
	}

	if (is_update) {
		status = display_plane_update(adapter->display_device, param);
	} else {
		status = display_plane_check(adapter->display_device, param);
	}

	if (status != DISP_OK) {
		gf_error("%s, %s fail\n", __func__, is_update ? "update" : "check");
		return -1;
	}

	return 0;
}

unsigned int dm_crtc_get_vblank_counter(struct display_adapter *adapter, int crtc_index)
{
	disp_itg_counter_t itg_cnt = {0};

	itg_cnt.crtc = crtc_index;
	itg_cnt.flag.bGetFrameCnt = 1;

	if (DISP_OK != display_get_itg_counter(adapter->display_device, &itg_cnt)) {
		gf_printk(GF_DRV_ERROR, "get frame count fail\n");
		return -1;
	}

	return itg_cnt.Value[DISP_COUNTER_FRAME];
}

int dm_crtc_enable_vblank(struct display_adapter *adapter, int crtc_index, bool enable)
{
	disp_intr_t intr = {0};

	if (crtc_index == 0) {
		intr.info.Vsynca = 1;
	} else if (crtc_index == 1) {
		intr.info.Vsyncb = 1;
	} else if (crtc_index == 2) {
		intr.info.Vsyncc = 1;
	} else {
		intr.info.Vsyncd = 1;
	}

	if (DISP_OK != display_intr_enable(adapter->display_device, enable, intr)) {
		gf_printk(GF_DRV_ERROR, "enable/disable vblank fail\n");
		return -1;
	}

	//gf_printk(GF_DRV_INFO, "%s vblank on crtc %d\n", enable ? "enable" : "disable", crtc_index);

	return 0;
}

int dm_crtc_get_hv_position(struct display_adapter *adapter, int crtc_index, int *hpos, int *vpos, int *in_hblank, int *in_vblank)
{
    disp_itg_counter_t itg_cnt = {0};

	itg_cnt.crtc = crtc_index;
	itg_cnt.flag.bGetPixelCnt = 1;
	itg_cnt.flag.bGetLineCnt = 1;

	if (DISP_OK != display_get_itg_counter(adapter->display_device, &itg_cnt)) {
		gf_printk(GF_DRV_ERROR, "get hv position fail\n");
		return -1;
	}

	*hpos = itg_cnt.Value[DISP_COUNTER_PIXEL];
	*vpos = itg_cnt.Value[DISP_COUNTER_LINE];
	*in_vblank = itg_cnt.bInVblank;
	*in_hblank = itg_cnt.bInHblank;

	return 0;
}

int dm_intr_enable(struct display_adapter *adapter, bool enable, int intr_bit)
{
	disp_intr_t intr = {0};

	intr.intr = intr_bit;

	if (DISP_OK != display_intr_enable(adapter->display_device, enable, intr)) {
		gf_printk(GF_DRV_ERROR, "display intr enable fail\n");
		return -1;
	}

	return 0;
}

int dm_intr_get_status(struct display_adapter *adapter)
{
	disp_intr_t intr = {0};

	if (DISP_OK != display_intr_get_status(adapter->display_device, &intr)) {
		gf_printk(GF_DRV_ERROR, "display get intr status fail\n");
		return -1;
	}

	return intr.intr;
}

int dm_intr_multi_vsync_enable(struct display_adapter *adapter, bool enable)
{
	if (DISP_OK != display_intr_multi_vsync_enable(adapter->display_device, enable)) {
		gf_printk(GF_DRV_ERROR, "dm_intr_multi_vsync_enable fail\n");
		return -1;
	}

	return 0;
}

int dm_intr_clear_vsync_status(struct display_adapter *adapter, int crtc_index)
{
	if (DISP_OK != display_intr_clear_vsync_status(adapter->display_device, crtc_index)) {
		gf_printk(GF_DRV_ERROR, "dm_intr_clear_vsync_status fail\n");
		return -1;
	}

	return 0;
}

int dm_pipe_set_crc(struct display_adapter *adapter, struct crtc_t *gcrtc)
{
	struct pipe_crc_t *pipe_crc = &gcrtc->pipe_crc;
	disp_sig_set_t sig_set = {0};

	sig_set.benable = pipe_crc->crc_enabled;
	sig_set.crtc_index = gcrtc->index;
	sig_set.isr.x = pipe_crc->window_x_start;
	sig_set.isr.y = pipe_crc->window_y_start;
	sig_set.isr.w = pipe_crc->window_x_end - pipe_crc->window_x_start;
	sig_set.isr.h = pipe_crc->window_y_end - pipe_crc->window_y_start;

	if (DISP_OK != display_set_signature(adapter->display_device, sig_set)) {
		gf_printk(GF_DRV_ERROR, "set signature fail\n");
		return -1;
	}

	return 0;
}

int dm_pipe_get_crc(struct display_adapter *adapter, struct crtc_t *gcrtc,
					uint32_t *crc_0, uint32_t *crc_1, uint32_t *crc_2, uint32_t *crc_3)
{
	disp_sig_get_t sig_get = {0};

	sig_get.crtc_index = gcrtc->index;

	if (DISP_OK != display_get_signature(adapter->display_device, &sig_get)) {
		gf_printk(GF_DRV_ERROR, "get signature fail\n");
		return -1;
	}

	*crc_0 = sig_get.crc[0];
	*crc_1 = sig_get.crc[1];
	*crc_2 = sig_get.crc[2];
	*crc_3 = sig_get.crc[3];

	return 0;
}

int dm_pipe_get_sgnt_value(struct display_adapter *adapter, uint32_t crtc_index, uint32_t *psgnt, uint32_t sgnt_len)
{
	disp_sig_get_t sig_get = {0};

	if (sgnt_len < 8) {
		gf_printk(GF_DRV_ERROR, "dm_pipe_get_sgnt_value, sgnt_len[%u] need at least 8!\n", sgnt_len);
		return -1;
	}

	sig_get.crtc_index = crtc_index;

	if (DISP_OK != display_get_sgnt_golden(adapter->display_device, &sig_get)) {
		gf_printk(GF_DRV_ERROR, "get sgnt_golden fail\n");
		return -1;
	}
	psgnt[0] = sig_get.crc[0];
	psgnt[1] = sig_get.crc[1];
	psgnt[2] = sig_get.crc[2];
	psgnt[3] = sig_get.crc[3];

	if (DISP_OK != display_get_signature(adapter->display_device, &sig_get)) {
		gf_printk(GF_DRV_ERROR, "get signature fail\n");
		return -1;
	}
	psgnt[4] = sig_get.crc[0];
	psgnt[5] = sig_get.crc[1];
	psgnt[6] = sig_get.crc[2];
	psgnt[7] = sig_get.crc[3];

	return 0;
}

int dm_pipe_set_csc(struct display_adapter *adpter, pdisp_csc_para_t pcsc_para)
{
	if (DISP_OK != display_set_csc(adpter->display_device, pcsc_para)) {
		gf_printk(GF_DRV_ERROR, "set csc fail\n");
		return -1;
	}

	return 0;
}

int dm_pipe_set_gamma(struct display_adapter *adpter, pdisp_gamma_info_t pgamma_info)
{
	if (DISP_OK != display_set_gamma(adpter->display_device, pgamma_info)) {
		gf_printk(GF_DRV_ERROR, "set gamma fail\n");
		return -1;
	}

	return 0;
}

int dm_aux_transfer(struct display_adapter *adapter, pdisp_aux_para_t paux_para)
{
	if (DISP_OK != display_aux_transfer(adapter->display_device, paux_para)) {
		gf_printk(GF_DRV_ERROR, "aux transfer fail\n");
		return -1;
	}

	return 0;
}

int dm_get_hpd_state(struct display_adapter *adapter, pdisp_get_hpd_state_t phpd_state)
{
	if (DISP_OK != display_get_hpd_state(adapter->display_device, phpd_state)) {
		gf_printk(GF_DRV_ERROR, "dm_get_hpd_state fail\n");
		return -1;
	}

	return 0;
}

int dm_connector_get_i2c_status(struct display_adapter *adapter, pdisp_get_i2c_status_t pdisp_i2c_status)
{
	if (DISP_OK != display_get_i2c_status(adapter->display_device, pdisp_i2c_status)) {
		gf_printk(GF_DRV_ERROR, "dm_connector_get_i2c_status fail\n");
		return -1;
	}

	return 0;
}

int dm_check_underflow(struct display_adapter *adapter)
{
    if (DISP_OK != display_check_underflow(adapter->display_device)) {
        gf_printk(GF_DRV_ERROR, "dm_check_underflow fail\n");
        return -1;
    }

    return 0;
}

int dm_notify_irq_hpd(struct display_adapter *adapter, disp_u8_t connector_index)
{
    if (DISP_OK != display_notify_irq_hpd(adapter->display_device, connector_index)) {
        gf_printk(GF_DRV_ERROR, "dm_notify_irq_hpd fail\n");
        return -1;
    }

    return 0;
}

int dm_backlight_query(struct display_adapter *adapter, pdisp_backlight_query_t pquery)
{
    if (DISP_OK != display_backlight_query(adapter->display_device, pquery)) {
        gf_printk(GF_DRV_ERROR, "dm_backlight_query fail\n");
        return -1;
    }

    return 0;
}

int dm_backlight_set(struct display_adapter *adapter, pdisp_backlight_set_t pset)
{
    if (DISP_OK != display_backlight_set(adapter->display_device, pset)) {
        gf_printk(GF_DRV_ERROR, "dm_backlight_set fail\n");
        return -1;
    }

    return 0;
}

int dm_psr_query(struct display_adapter *adapter, pdisp_psr_query_t pquery)
{
    if (DISP_OK != display_psr_query(adapter->display_device, pquery)) {
        gf_printk(GF_DRV_ERROR, "dm_psr_query fail\n");
        return -1;
    }

    return 0;
}

int dm_psr_set(struct display_adapter *adapter, pdisp_psr_set_t pset)
{
    if (DISP_OK != display_psr_set(adapter->display_device, pset)) {
        gf_printk(GF_DRV_ERROR, "dm_psr_set fail\n");
        return -1;
    }

    return 0;
}

int dm_hda_handle(struct display_adapter *adapter)
{
    if (DISP_OK != display_hda_handle(adapter->display_device)) {
        gf_printk(GF_DRV_ERROR, "dm_hda_handle fail\n");
        return -1;
    }

    return 0;
}
