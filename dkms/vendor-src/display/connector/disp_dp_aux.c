/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#include "disp_dp_aux.h"
#include "disp_connector.h"
#include "disp_reg.h"
#include "DP_register.h"
#include "disp_encoder.h"

/**
 * this aux funcs duplicated from linux kernel(version 5.17), drm_dp_helper.c 
*/

#define disp_print(level, log, ...) disp_print_ex(DISP_PRINT_MASK_DP_AUX, level, log, ##__VA_ARGS__)

#define AUX_REPLY_TIMEOUT_WAIT_US 500 /* AUX Transaction Response/Reply Timeouts, no less than 400 us */
#define AUX_REPLY_TIMEOUT_WAIT_WAKE_US 3500 /* Wake from low power, no less than 3200 us */

struct dp_aux_backlight {
    struct disp_dp_aux *aux;
    struct disp_edp_backlight_info info;
    disp_u8_t enabled;
};

static disp_u8_t dp_link_status(disp_u8_t link_status[DP_LINK_STATUS_SIZE], disp_s32_t r)
{
    return link_status[r - DP_LANE0_1_STATUS];
}

static disp_u8_t dp_get_lane_status(disp_u8_t link_status[DP_LINK_STATUS_SIZE],
                 disp_s32_t lane)
{
    disp_s32_t i = DP_LANE0_1_STATUS + (lane >> 1);
    disp_s32_t s = (lane & 1) * 4;
    disp_u8_t l = dp_link_status(link_status, i);

    return (l >> s) & 0xf;
}

static disp_u8_t dp_hpd_ok(pdisp_connector_t pconnector)
{
    pdisp_device_t pdd = pconnector->disp_device;
    disp_u32_t plug_status = 0;

    dp_read_field(pconnector->index, Reg_Dpt_Reg29, Register_Dpt_Reg29_Start, 1,
        Hpd_Sts_O, &plug_status);

    if (DPT_REG29_HPD_STS_O_HPD_PLUG_IN == plug_status ||
        DPT_REG29_HPD_STS_O_HPD_EVENT == plug_status)
    {
        return DISP_TRUE;
    }

    return DISP_FALSE;
}

disp_u8_t disp_dp_channel_eq_ok(disp_u8_t link_status[DP_LINK_STATUS_SIZE],
              disp_s32_t lane_count)
{
    disp_u8_t lane_align;
    disp_u8_t lane_status;
    disp_s32_t lane;

    lane_align = dp_link_status(link_status,
                    DP_LANE_ALIGN_STATUS_UPDATED);
    if ((lane_align & DP_INTERLANE_ALIGN_DONE) == 0)
        return DISP_FALSE ;
    for (lane = 0; lane < lane_count; lane++) {
        lane_status = dp_get_lane_status(link_status, lane);
        if ((lane_status & DP_CHANNEL_EQ_BITS) != DP_CHANNEL_EQ_BITS)
            return DISP_FALSE ;
    }
    return DISP_TRUE;
}

disp_u8_t disp_dp_clock_recovery_ok(disp_u8_t link_status[DP_LINK_STATUS_SIZE],
                  disp_s32_t lane_count)
{
    disp_s32_t lane;
    disp_u8_t lane_status;

    for (lane = 0; lane < lane_count; lane++) {
        lane_status = dp_get_lane_status(link_status, lane);
        if ((lane_status & DP_LANE_CR_DONE) == 0)
            return DISP_FALSE ;
    }
    return DISP_TRUE;
}

disp_u8_t disp_dp_get_adjust_request_voltage(disp_u8_t link_status[DP_LINK_STATUS_SIZE],
                     disp_s32_t lane)
{
    disp_s32_t i = DP_ADJUST_REQUEST_LANE0_1 + (lane >> 1);
    disp_s32_t s = ((lane & 1) ?
         DP_ADJUST_VOLTAGE_SWING_LANE1_SHIFT :
         DP_ADJUST_VOLTAGE_SWING_LANE0_SHIFT);
    disp_u8_t l = dp_link_status(link_status, i);

    return ((l >> s) & 0x3) << DP_TRAIN_VOLTAGE_SWING_SHIFT;
}

disp_u8_t disp_dp_get_adjust_request_pre_emphasis(disp_u8_t link_status[DP_LINK_STATUS_SIZE],
                      disp_s32_t lane)
{
    disp_s32_t i = DP_ADJUST_REQUEST_LANE0_1 + (lane >> 1);
    disp_s32_t s = ((lane & 1) ?
         DP_ADJUST_PRE_EMPHASIS_LANE1_SHIFT :
         DP_ADJUST_PRE_EMPHASIS_LANE0_SHIFT);
    disp_u8_t l = dp_link_status(link_status, i);

    return ((l >> s) & 0x3) << DP_TRAIN_PRE_EMPHASIS_SHIFT;
}

/* DP 2.0 128b/132b */
disp_u8_t disp_dp_get_adjust_tx_ffe_preset(disp_u8_t link_status[DP_LINK_STATUS_SIZE],
                   disp_s32_t lane)
{
    disp_s32_t i = DP_ADJUST_REQUEST_LANE0_1 + (lane >> 1);
    disp_s32_t s = ((lane & 1) ?
         DP_ADJUST_TX_FFE_PRESET_LANE1_SHIFT :
         DP_ADJUST_TX_FFE_PRESET_LANE0_SHIFT);
    disp_u8_t l = dp_link_status(link_status, i);

    return (l >> s) & 0xf;
}

static disp_s32_t __8b10b_clock_recovery_delay_us(disp_u8_t rd_interval)
{
    if (rd_interval > 4)
        disp_print(DDM_INFO, "invalid AUX interval 0x%02x (max 4)\n",rd_interval);

    if (rd_interval == 0)
        return 100;

    return rd_interval * 4 * 1000;
}

static disp_s32_t __8b10b_channel_eq_delay_us(disp_u8_t rd_interval)
{
    if (rd_interval == 0)
        return 400;
    else if (rd_interval > 4)
    {
        disp_print(DDM_WARNING, "invalid AUX interval 0x%02x (max 4)\n", rd_interval);
        return 16000; // use max instead
    }

    return rd_interval * 4 * 1000;
}

static disp_s32_t __128b132b_channel_eq_delay_us(disp_u8_t rd_interval)
{
    switch (rd_interval) {
    default:
        disp_print(DDM_INFO, "invalid AUX interval 0x%02x\n", rd_interval);
        return 400;
    case DP_128B132B_TRAINING_AUX_RD_INTERVAL_400_US:
        return 400;
    case DP_128B132B_TRAINING_AUX_RD_INTERVAL_4_MS:
        return 4000;
    case DP_128B132B_TRAINING_AUX_RD_INTERVAL_8_MS:
        return 8000;
    case DP_128B132B_TRAINING_AUX_RD_INTERVAL_12_MS:
        return 12000;
    case DP_128B132B_TRAINING_AUX_RD_INTERVAL_16_MS:
        return 16000;
    case DP_128B132B_TRAINING_AUX_RD_INTERVAL_32_MS:
        return 32000;
    case DP_128B132B_TRAINING_AUX_RD_INTERVAL_64_MS:
        return 64000;
    }
}

disp_s32_t disp_8b10b_channel_eq_delay_us(disp_u8_t rd_interval)
{
    return __8b10b_channel_eq_delay_us(rd_interval);
}

/*
 * The link training delays are different for:
 *
 *  - Clock recovery vs. channel equalization
 *  - DPRX vs. LTTPR
 *  - 128b/132b vs. 8b/10b
 *  - DPCD rev 1.3 vs. later
 *
 * Get the correct delay in us, reading DPCD if necessary.
 */
static disp_s32_t __read_delay(struct disp_dp_aux *aux, disp_u8_t dpcd[DP_RECEIVER_CAP_SIZE],
            enum disp_dp_phy dp_phy, disp_u8_t uhbr, disp_u8_t cr)
{
    disp_s32_t (*parse)(disp_u8_t rd_interval);
    disp_u32_t offset;
    disp_u8_t rd_interval, mask;

    if (dp_phy == DP_PHY_DPRX) {
        if (uhbr) {
            if (cr)
                return 100;

            offset = DP_128B132B_TRAINING_AUX_RD_INTERVAL;
            mask = DP_128B132B_TRAINING_AUX_RD_INTERVAL_MASK;
            parse = __128b132b_channel_eq_delay_us;
        } else {
            if (cr && dpcd[DP_DPCD_REV] >= DP_DPCD_REV_14)
                return 100;

            offset = DP_TRAINING_AUX_RD_INTERVAL;
            mask = DP_TRAINING_AUX_RD_MASK;
            if (cr)
                parse = __8b10b_clock_recovery_delay_us;
            else
                parse = __8b10b_channel_eq_delay_us;
        }
    } else {
        if (uhbr) {
            offset = DP_128B132B_TRAINING_AUX_RD_INTERVAL_PHY_REPEATER(dp_phy);
            mask = DP_128B132B_TRAINING_AUX_RD_INTERVAL_MASK;
            parse = __128b132b_channel_eq_delay_us;
        } else {
            if (cr)
                return 100;

            offset = DP_TRAINING_AUX_RD_INTERVAL_PHY_REPEATER(dp_phy);
            mask = DP_TRAINING_AUX_RD_MASK;
            parse = __8b10b_channel_eq_delay_us;
        }
    }

    if (offset < DP_RECEIVER_CAP_SIZE) {
        rd_interval = dpcd[offset];
    } else {
        if (disp_dp_dpcd_readb(aux, offset, &rd_interval) != 1) {
            disp_print(DDM_WARNING, "failed rd interval read\n");
            /* arbitrary default delay */
            return 400;
        }
    }

    return parse(rd_interval & mask);
}

disp_s32_t disp_dp_read_clock_recovery_delay(struct disp_dp_aux *aux, disp_u8_t dpcd[DP_RECEIVER_CAP_SIZE],
                     enum disp_dp_phy dp_phy, disp_u8_t uhbr)
{
    return __read_delay(aux, dpcd, dp_phy, uhbr, DISP_TRUE);
}

disp_s32_t disp_dp_read_channel_eq_delay(struct disp_dp_aux *aux, disp_u8_t dpcd[DP_RECEIVER_CAP_SIZE],
                 enum disp_dp_phy dp_phy, disp_u8_t uhbr)
{
    return __read_delay(aux, dpcd, dp_phy, uhbr, DISP_FALSE );
}

void disp_dp_link_train_clock_recovery_delay(disp_u8_t dpcd[DP_RECEIVER_CAP_SIZE])
{
    disp_u8_t rd_interval = dpcd[DP_TRAINING_AUX_RD_INTERVAL] &
        DP_TRAINING_AUX_RD_MASK;
    disp_s32_t delay_us;

    if (dpcd[DP_DPCD_REV] >= DP_DPCD_REV_14)
        delay_us = 100;
    else
        delay_us = __8b10b_clock_recovery_delay_us(rd_interval);

    disp_delay_us(delay_us);
}

static void __disp_dp_link_train_channel_eq_delay(disp_u8_t rd_interval)
{
    disp_s32_t delay_us = __8b10b_channel_eq_delay_us(rd_interval);

    disp_delay_us(delay_us);
}

void disp_dp_link_train_channel_eq_delay(disp_u8_t dpcd[DP_RECEIVER_CAP_SIZE])
{
    __disp_dp_link_train_channel_eq_delay(
                         dpcd[DP_TRAINING_AUX_RD_INTERVAL] &
                         DP_TRAINING_AUX_RD_MASK);
}

void disp_dp_lttpr_link_train_clock_recovery_delay(void)
{
    disp_delay_us(100);
}

static disp_u8_t dp_lttpr_phy_cap(disp_u8_t phy_cap[DP_LTTPR_PHY_CAP_SIZE], disp_s32_t r)
{
    return phy_cap[r - DP_TRAINING_AUX_RD_INTERVAL_PHY_REPEATER1];
}

void disp_dp_lttpr_link_train_channel_eq_delay(disp_u8_t phy_cap[DP_LTTPR_PHY_CAP_SIZE])
{
    disp_u8_t interval = dp_lttpr_phy_cap(phy_cap,
                       DP_TRAINING_AUX_RD_INTERVAL_PHY_REPEATER1) &
              DP_TRAINING_AUX_RD_MASK;

    __disp_dp_link_train_channel_eq_delay(interval);
}

disp_u8_t disp_dp_link_rate_to_bw_code(disp_s32_t link_rate)
{
    switch (link_rate) {
    case 1000000:
        return DP_LINK_BW_10;
    case 1350000:
        return DP_LINK_BW_13_5;
    case 2000000:
        return DP_LINK_BW_20;
    default:
        /* Spec says link_bw = link_rate / 0.27Gbps */
        return (disp_u8_t)(link_rate / 27000);
    }
}

disp_s32_t disp_dp_bw_code_to_link_rate(disp_u8_t link_bw)
{
    switch (link_bw) {
    case DP_LINK_BW_10:
        return 1000000;
    case DP_LINK_BW_13_5:
        return 1350000;
    case DP_LINK_BW_20:
        return 2000000;
    default:
        /* Spec says link_rate = link_bw * 0.27Gbps */
        return link_bw * 27000;
    }
}

static inline void
disp_dp_dump_access(disp_u8_t request, disp_u32_t offset, void *buffer, disp_s32_t ret)
{
    char *arrow = request == DP_AUX_NATIVE_READ ? "->" : "<-";

    if (ret > 0)
    {   /* G100 A0 TODO: %*ph will cause Call Trace log. PZ1 is OK, why real chip not work? Check gcc version. */
        //disp_print(DDM_TRACE, "0x%05x AUX %s (ret=%3d) %*ph\n", offset, arrow, ret, DISP_MIN(ret, 20), buffer);
        disp_print(DDM_TRACE, "0x%05x AUX %s (ret=%3d) buf[%p]\n", offset, arrow, ret, buffer);
    }
    else
        disp_print(DDM_TRACE, "0x%05x AUX %s (ret=%3d)\n", offset, arrow, ret);

}

static disp_s32_t disp_dp_dpcd_access(struct disp_dp_aux *aux, disp_u8_t request,
                  disp_u32_t offset, void *buffer, disp_u32_t size)
{
    pdisp_connector_t pconn = (pdisp_connector_t)aux->pconnector;
    pdisp_device_t pdd = (pdisp_device_t)pconn->disp_device;
    struct disp_dp_aux_msg msg;
    disp_u32_t retry = 0;
    disp_u32_t max_retry = 16;
    disp_s32_t have_read = 0;
    disp_s32_t ret = 0;

    disp_memset(&msg, 0, sizeof(msg));
    msg.address = offset;
    msg.request = request;
    msg.buffer = buffer;
    msg.size = size;

    do {
        ret = aux->transfer(aux, &msg);
        if ((DP_AUX_HW_REPLY_BUSY & msg.reply)
            || (DP_AUX_HW_REPLY_TIMEOUT & msg.reply))
        {
            if (!dp_hpd_ok(pconn))
            {
                retry = max_retry;
                ret = -DISP_EIO;
                break;
            }
            else
            {
                /* CTS analyzer default support low power and need wait 3200 us when aux timeout.
                 * Extension monitor 400 us is enough.
                 * Refine it later to support low power monitor.
                 */
                if (pdd->flag.info.dp_cts)
                {
                    disp_delay_us(AUX_REPLY_TIMEOUT_WAIT_WAKE_US);
                }
                else
                {
                    disp_delay_us(AUX_REPLY_TIMEOUT_WAIT_US);
                }
                retry++;
            }
        }
        else if (msg.reply) // Nack or Defer
        {
            retry++;
        }
        else if (ret == (disp_s32_t)size - have_read) // Ack and size match
        {
            ret = size;
            break;
        }
        else if (ret > 0) // Ack but few
        {
            have_read += ret;
            msg.address = offset + have_read;
            msg.buffer = (pdisp_u8_t)buffer + have_read;
            msg.size = size - have_read;
            retry = 0;
        }
        else // Ack and 0 read back
        {
            retry++;
        }
    } while (retry < max_retry);

    if (retry >= max_retry) // Retry timeout
    {
        disp_print(DDM_WARNING, "disp_dp_dpcd_access 0x%u failed ret[%d] reply[0x%x] retry[%u]\n",
            offset, ret, msg.reply, retry);
        ret = -DISP_EIO;
    }
    return ret;
}

disp_s32_t disp_dp_dpcd_read(struct disp_dp_aux *aux, disp_u32_t offset,
             void *buffer, disp_u32_t size)
{
    disp_s32_t ret;

    /*
     * HP ZR24w corrupts the first DPCD access after entering power save
     * mode. Eg. on a read, the entire buffer will be filled with the same
     * byte. Do a throw away read to avoid corrupting anything we care
     * about. Afterwards things will work correctly until the monitor
     * gets woken up and subsequently re-enters power save mode.
     *
     * The user pressing any button on the monitor is enough to wake it
     * up, so there is no particularly good place to do the workaround.
     * We just have to do it before any DPCD access and hope that the
     * monitor doesn't power down exactly after the throw away read.
     */
    //ret = disp_dp_dpcd_access(aux, DP_AUX_NATIVE_READ, DP_DPCD_REV,
    //                buffer, 1);
    //if (ret != 1)
    //    goto out;

    ret = disp_dp_dpcd_access(aux, DP_AUX_NATIVE_READ, offset, buffer, size);

    disp_dp_dump_access(DP_AUX_NATIVE_READ, offset, buffer, ret);
    return ret;
}

disp_s32_t  disp_dp_dpcd_write(struct disp_dp_aux *aux, disp_u32_t offset,
              void *buffer, disp_u32_t size)
{
    disp_s32_t ret;

    ret = disp_dp_dpcd_access(aux, DP_AUX_NATIVE_WRITE, offset,
                    buffer, size);

    disp_dp_dump_access(DP_AUX_NATIVE_WRITE, offset, buffer, ret);
    return ret;
}

disp_s32_t  disp_dp_i2c_read(struct disp_dp_aux *aux, disp_u32_t offset,
              void *buffer, disp_u32_t size)
{
    disp_s32_t ret;

    ret = disp_dp_dpcd_access(aux, DP_AUX_I2C_READ, offset,
                    buffer, size);

    disp_dp_dump_access(DP_AUX_I2C_READ, offset, buffer, ret);
    return ret;
}

disp_u32_t disp_dp_dpcd_transfer(pdisp_connector_t pdisp_connector, pdisp_aux_para_t paux_para)
{
    struct disp_dp_aux *aux = (struct disp_dp_aux*)pdisp_connector->ddc;
    disp_s32_t ret = 0;

    if(paux_para->op == DISP_DP_NATIVE_AUX_WRITE)
    {
        ret = disp_dp_dpcd_write(aux, paux_para->addr, paux_para->pbuf, paux_para->size);
    }
    else if(paux_para->op == DISP_DP_NATIVE_AUX_READ)
    {
        ret = disp_dp_dpcd_read(aux, paux_para->addr, paux_para->pbuf, paux_para->size);
    }
    else if(paux_para->op == DISP_DP_I2C_OVER_AUX_READ)
    {
        ret = disp_dp_i2c_read(aux, paux_para->addr, paux_para->pbuf, paux_para->size);
    }

    return ret;
}

disp_s32_t disp_dp_dpcd_read_link_status(struct disp_dp_aux *aux,
                 disp_u8_t status[DP_LINK_STATUS_SIZE])
{
    return disp_dp_dpcd_read(aux, DP_LANE0_1_STATUS, status,
                DP_LINK_STATUS_SIZE);
}

disp_s32_t disp_dp_dpcd_read_phy_link_status(struct disp_dp_aux *aux,
                     enum disp_dp_phy dp_phy,
                     disp_u8_t link_status[DP_LINK_STATUS_SIZE])
{
    disp_s32_t ret;

    if (dp_phy == DP_PHY_DPRX) {
        ret = disp_dp_dpcd_read(aux,
                       DP_LANE0_1_STATUS,
                       link_status,
                       DP_LINK_STATUS_SIZE);

        if (ret < 0)
            return ret;

        if (ret != DP_LINK_STATUS_SIZE)
            disp_print(DDM_WARNING, "ret != DP_LINK_STATUS_SIZE\n");

        return 0;
    }

    ret = disp_dp_dpcd_read(aux,
                   DP_LANE0_1_STATUS_PHY_REPEATER(dp_phy),
                   link_status,
                   DP_LINK_STATUS_SIZE - 1);

    if (ret < 0)
        return ret;

    if(ret != DP_LINK_STATUS_SIZE - 1)
        disp_print(DDM_WARNING, "warning: ret != DP_LINK_STATUS_SIZE - 1\n");

    /* Convert the LTTPR to the sink PHY link status layout */
    disp_memcpy(&link_status[DP_SINK_STATUS - DP_LANE0_1_STATUS + 1],
        &link_status[DP_SINK_STATUS - DP_LANE0_1_STATUS],
        DP_LINK_STATUS_SIZE - (DP_SINK_STATUS - DP_LANE0_1_STATUS) - 1);
    link_status[DP_SINK_STATUS - DP_LANE0_1_STATUS] = 0;

    return 0;
}

static disp_u8_t is_edid_digital_input_dp(edid_t *edid)
{
    return edid && edid->revision >= 4 &&
        edid->input & DISP_EDID_INPUT_DIGITAL &&
        (edid->input & DISP_EDID_DIGITAL_TYPE_MASK) == DISP_EDID_DIGITAL_TYPE_DP;
}

disp_u8_t disp_dp_downstream_is_type(disp_u8_t dpcd[DP_RECEIVER_CAP_SIZE],
                   disp_u8_t port_cap[4], disp_u8_t type)
{
    return disp_dp_is_branch(dpcd) &&
        dpcd[DP_DPCD_REV] >= 0x11 &&
        (port_cap[0] & DP_DS_PORT_TYPE_MASK) == type;
}

disp_u8_t disp_dp_downstream_is_tmds(disp_u8_t dpcd[DP_RECEIVER_CAP_SIZE],
                   disp_u8_t port_cap[4],
                   edid_t *edid)
{
    if (dpcd[DP_DPCD_REV] < 0x11) {
        switch (dpcd[DP_DOWNSTREAMPORT_PRESENT] & DP_DWN_STRM_PORT_TYPE_MASK) {
        case DP_DWN_STRM_PORT_TYPE_TMDS:
            return DISP_TRUE;
        default:
            return DISP_FALSE ;
        }
    }

    switch (port_cap[0] & DP_DS_PORT_TYPE_MASK) {
    case DP_DS_PORT_TYPE_DP_DUALMODE:
        if (is_edid_digital_input_dp(edid))
            return DISP_FALSE;
        return DISP_FALSE;
    case DP_DS_PORT_TYPE_DVI:
    case DP_DS_PORT_TYPE_HDMI:
        return DISP_TRUE;
    default:
        return DISP_FALSE ;
    }
}

/**
 * disp_dp_send_real_edid_checksum() - send back real edid checksum value
 * @aux: DisplayPort AUX channel
 * @real_edid_checksum: real edid checksum for the last block
 *
 * Returns:
 * DISP_TRUE on success
 */
disp_u8_t disp_dp_send_real_edid_checksum(struct disp_dp_aux *aux,
                    disp_u8_t real_edid_checksum)
{
    disp_u8_t link_edid_read = 0, auto_test_req = 0, test_resp = 0;

    if (disp_dp_dpcd_read(aux, DP_DEVICE_SERVICE_IRQ_VECTOR,
                 &auto_test_req, 1) < 1) {
        disp_print(DDM_WARNING, "DPCD failed read at register 0x%x\n", DP_DEVICE_SERVICE_IRQ_VECTOR);
        return DISP_FALSE ;
    }
    auto_test_req &= DP_AUTOMATED_TEST_REQUEST;

    if (disp_dp_dpcd_read(aux, DP_TEST_REQUEST, &link_edid_read, 1) < 1) {
        disp_print(DDM_WARNING, "DPCD failed read at register 0x%x\n", DP_TEST_REQUEST);
        return DISP_FALSE ;
    }
    link_edid_read &= DP_TEST_LINK_EDID_READ;

    if (!auto_test_req || !link_edid_read) {
        disp_print(DDM_WARNING, "Source DUT does not support TEST_EDID_READ\n");
        return DISP_FALSE ;
    }

    if (disp_dp_dpcd_write(aux, DP_DEVICE_SERVICE_IRQ_VECTOR,
                  &auto_test_req, 1) < 1) {
        disp_print(DDM_WARNING, "DPCD failed write at register 0x%x\n", DP_DEVICE_SERVICE_IRQ_VECTOR);
        return DISP_FALSE ;
    }

    /* send back checksum for the last edid extension block data */
    if (disp_dp_dpcd_write(aux, DP_TEST_EDID_CHECKSUM,
                  &real_edid_checksum, 1) < 1) {
        disp_print(DDM_WARNING, "DPCD failed write at register 0x%x\n", DP_TEST_EDID_CHECKSUM);
        return DISP_FALSE ;
    }

    test_resp |= DP_TEST_EDID_CHECKSUM_WRITE;
    if (disp_dp_dpcd_write(aux, DP_TEST_RESPONSE, &test_resp, 1) < 1) {
        disp_print(DDM_WARNING, "DPCD failed write at register 0x%x\n", DP_TEST_RESPONSE);
        return DISP_FALSE ;
    }

    return DISP_TRUE;
}

static disp_u8_t disp_dp_downstream_port_count(disp_u8_t dpcd[DP_RECEIVER_CAP_SIZE])
{
    disp_u8_t port_count = dpcd[DP_DOWN_STREAM_PORT_COUNT] & DP_PORT_COUNT_MASK;

    if (dpcd[DP_DOWNSTREAMPORT_PRESENT] & DP_DETAILED_CAP_INFO_AVAILABLE && port_count > 4)
        port_count = 4;

    return port_count;
}

static disp_s32_t disp_dp_read_extended_dpcd_caps(struct disp_dp_aux *aux,
                      disp_u8_t dpcd[DP_RECEIVER_CAP_SIZE])
{
    disp_u8_t dpcd_ext[DP_RECEIVER_CAP_SIZE];
    disp_s32_t ret;

    /*
     * Prior to DP1.3 the bit represented by
     * DP_EXTENDED_RECEIVER_CAP_FIELD_PRESENT was reserved.
     * If it is set DP_DPCD_REV at 0000h could be at a value less than
     * the DISP_TRUE capability of the panel. The only way to check is to
     * then compare 0000h and 2200h.
     */
    if (!(dpcd[DP_TRAINING_AUX_RD_INTERVAL] &
          DP_EXTENDED_RECEIVER_CAP_FIELD_PRESENT))
        return 0;

    ret = disp_dp_dpcd_read(aux, DP_DP13_DPCD_REV, &dpcd_ext,
                   sizeof(dpcd_ext));
    if (ret < 0)
        return ret;
    if (ret != sizeof(dpcd_ext))
        return -DISP_EIO;

    if (dpcd[DP_DPCD_REV] > dpcd_ext[DP_DPCD_REV]) {
        disp_print(DDM_WARNING,
                "Extended DPCD rev less than base DPCD rev (%d > %d)\n",
                dpcd[DP_DPCD_REV], dpcd_ext[DP_DPCD_REV]);
        return 0;
    }

    if (!disp_memcmp(dpcd, dpcd_ext, sizeof(dpcd_ext)))
        return 0;

    //disp_print(DDM_TRACE, "Base DPCD 0x%x: %*ph\n", DP_RECEIVER_CAP_SIZE, dpcd);

    disp_memcpy(dpcd, dpcd_ext, sizeof(dpcd_ext));

    return 0;
}

/**
 * disp_dp_read_dpcd_caps() - read DPCD caps and extended DPCD caps if
 * available
 * @aux: DisplayPort AUX channel
 * @dpcd: Buffer to store the resulting DPCD in
 *
 * Attempts to read the base DPCD caps for @aux. Additionally, this function
 * checks for and reads the extended DPRX caps (%DP_DP13_DPCD_REV) if
 * present.
 *
 * Returns: %0 if the DPCD was read successfully, negative error code
 * otherwise.
 */
disp_s32_t disp_dp_read_dpcd_caps(struct disp_dp_aux *aux,
              disp_u8_t dpcd[DP_RECEIVER_CAP_SIZE])
{
    disp_s32_t ret;

    ret = disp_dp_dpcd_read(aux, DP_DPCD_REV, dpcd, DP_RECEIVER_CAP_SIZE);
    if (ret != DP_RECEIVER_CAP_SIZE || dpcd[DP_DPCD_REV] == 0)
    {
        disp_print(DDM_WARNING, "dpcd read not expect: dpcd_rev[%x] read_size[%d]\n",
            dpcd[DP_DPCD_REV], ret);
        return -DISP_EIO;
    }

    ret = disp_dp_read_extended_dpcd_caps(aux, dpcd);
    if (ret != DP_RECEIVER_CAP_SIZE || dpcd[DP_DPCD_REV] == 0)
    {
        disp_print(DDM_WARNING, "dpcd ext_rev not support, read_size[%d]\n", ret);
    }

    return ret;
}

/**
 * disp_dp_read_downstream_info() - read DPCD downstream port info if available
 * @aux: DisplayPort AUX channel
 * @dpcd: A cached copy of the port's DPCD
 * @downstream_ports: buffer to store the downstream port info in
 *
 * See also:
 * disp_dp_downstream_max_clock()
 * disp_dp_downstream_max_bpc()
 *
 * Returns: 0 if either the downstream port info was read successfully or
 * there was no downstream info to read, or a negative error code otherwise.
 */
disp_s32_t disp_dp_read_downstream_info(struct disp_dp_aux *aux,
                disp_u8_t dpcd[DP_RECEIVER_CAP_SIZE],
                disp_u8_t downstream_ports[DP_MAX_DOWNSTREAM_PORTS])
{
    disp_s32_t ret;
    disp_u8_t len;

    disp_memset(downstream_ports, 0, DP_MAX_DOWNSTREAM_PORTS);

    /* No downstream info to read */
    if (!disp_dp_is_branch(dpcd) || dpcd[DP_DPCD_REV] == DP_DPCD_REV_10)
        return 0;

    /* Some branches advertise having 0 downstream ports, despite also advertising they have a
     * downstream port present. The DP spec isn't clear on if this is allowed or not, but since
     * some branches do it we need to handle it regardless.
     */
    len = disp_dp_downstream_port_count(dpcd);
    if (!len)
        return 0;

    if (dpcd[DP_DOWNSTREAMPORT_PRESENT] & DP_DETAILED_CAP_INFO_AVAILABLE)
        len *= 4;

    ret = disp_dp_dpcd_read(aux, DP_DOWNSTREAM_PORT_0, downstream_ports, len);
    if (ret < 0)
        return ret;
    if (ret != len)
        return -DISP_EIO;

    //disp_print(DDM_TRACE, "DPCD DFP: %*ph\n", downstream_ports);

    return 0;
}

/**
 * disp_dp_downstream_max_dotclock() - extract downstream facing port max dot clock
 * @dpcd: DisplayPort configuration data
 * @port_cap: port capabilities
 *
 * Returns: Downstream facing port max dot clock in kHz on success,
 * or 0 if max clock not defined
 */
disp_s32_t disp_dp_downstream_max_dotclock(disp_u8_t dpcd[DP_RECEIVER_CAP_SIZE],
                   disp_u8_t port_cap[4])
{
    if (!disp_dp_is_branch(dpcd))
        return 0;

    if (dpcd[DP_DPCD_REV] < 0x11)
        return 0;

    switch (port_cap[0] & DP_DS_PORT_TYPE_MASK) {
    case DP_DS_PORT_TYPE_VGA:
        if ((dpcd[DP_DOWNSTREAMPORT_PRESENT] & DP_DETAILED_CAP_INFO_AVAILABLE) == 0)
            return 0;
        return port_cap[1] * 8000;
    default:
        return 0;
    }
}

/**
 * disp_dp_downstream_max_tmds_clock() - extract downstream facing port max TMDS clock
 * @dpcd: DisplayPort configuration data
 * @port_cap: port capabilities
 * @edid: EDID
 *
 * Returns: HDMI/DVI downstream facing port max TMDS clock in kHz on success,
 * or 0 if max TMDS clock not defined
 */
disp_s32_t disp_dp_downstream_max_tmds_clock(disp_u8_t dpcd[DP_RECEIVER_CAP_SIZE],
                     disp_u8_t port_cap[4],
                     edid_t *edid)
{
    if (!disp_dp_is_branch(dpcd))
        return 0;

    if (dpcd[DP_DPCD_REV] < 0x11) {
        switch (dpcd[DP_DOWNSTREAMPORT_PRESENT] & DP_DWN_STRM_PORT_TYPE_MASK) {
        case DP_DWN_STRM_PORT_TYPE_TMDS:
            return 165000;
        default:
            return 0;
        }
    }

    switch (port_cap[0] & DP_DS_PORT_TYPE_MASK) {
    case DP_DS_PORT_TYPE_DP_DUALMODE:
        if (is_edid_digital_input_dp(edid))
            return 0;
        return 0;
        /*
         * It's left up to the driver to check the
         * DP dual mode adapter's max TMDS clock.
         *
         * Unfortunately it looks like branch devices
         * may not fordward that the DP dual mode i2c
         * access so we just usually get i2c nak :(
         */
    case DP_DS_PORT_TYPE_HDMI:
         /*
          * We should perhaps assume 165 MHz when detailed cap
          * info is not available. But looks like many typical
          * branch devices fall into that category and so we'd
          * probably end up with users complaining that they can't
          * get high resolution modes with their favorite dongle.
          *
          * So let's limit to 300 MHz instead since DPCD 1.4
          * HDMI 2.0 DFPs are required to have the detailed cap
          * info. So it's more likely we're dealing with a HDMI 1.4
          * compatible* device here.
          */
        if ((dpcd[DP_DOWNSTREAMPORT_PRESENT] & DP_DETAILED_CAP_INFO_AVAILABLE) == 0)
            return 300000;
        return port_cap[1] * 2500;
    case DP_DS_PORT_TYPE_DVI:
        if ((dpcd[DP_DOWNSTREAMPORT_PRESENT] & DP_DETAILED_CAP_INFO_AVAILABLE) == 0)
            return 165000;
        /* FIXME what to do about DVI dual link? */
        return port_cap[1] * 2500;
    default:
        return 0;
    }
}

/**
 * disp_dp_downstream_min_tmds_clock() - extract downstream facing port min TMDS clock
 * @dpcd: DisplayPort configuration data
 * @port_cap: port capabilities
 * @edid: EDID
 *
 * Returns: HDMI/DVI downstream facing port min TMDS clock in kHz on success,
 * or 0 if max TMDS clock not defined
 */
disp_s32_t disp_dp_downstream_min_tmds_clock(disp_u8_t dpcd[DP_RECEIVER_CAP_SIZE],
                     disp_u8_t port_cap[4],
                     edid_t *edid)
{
    if (!disp_dp_is_branch(dpcd))
        return 0;

    if (dpcd[DP_DPCD_REV] < 0x11) {
        switch (dpcd[DP_DOWNSTREAMPORT_PRESENT] & DP_DWN_STRM_PORT_TYPE_MASK) {
        case DP_DWN_STRM_PORT_TYPE_TMDS:
            return 25000;
        default:
            return 0;
        }
    }

    switch (port_cap[0] & DP_DS_PORT_TYPE_MASK) {
    case DP_DS_PORT_TYPE_DP_DUALMODE:
        if (is_edid_digital_input_dp(edid))
            return 0;
        return 0;
    case DP_DS_PORT_TYPE_DVI:
    case DP_DS_PORT_TYPE_HDMI:
        /*
         * Unclear whether the protocol converter could
         * utilize pixel replication. Assume it won't.
         */
        return 25000;
    default:
        return 0;
    }
}

/**
 * disp_dp_downstream_max_bpc() - extract downstream facing port max
 *                               bits per component
 * @dpcd: DisplayPort configuration data
 * @port_cap: downstream facing port capabilities
 * @edid: EDID
 *
 * Returns: Max bpc on success or 0 if max bpc not defined
 */
disp_s32_t disp_dp_downstream_max_bpc(disp_u8_t dpcd[DP_RECEIVER_CAP_SIZE],
                  disp_u8_t port_cap[4],
                  edid_t *edid)
{
    if (!disp_dp_is_branch(dpcd))
        return 0;

    if (dpcd[DP_DPCD_REV] < 0x11) {
        switch (dpcd[DP_DOWNSTREAMPORT_PRESENT] & DP_DWN_STRM_PORT_TYPE_MASK) {
        case DP_DWN_STRM_PORT_TYPE_DP:
            return 0;
        default:
            return 8;
        }
    }

    switch (port_cap[0] & DP_DS_PORT_TYPE_MASK) {
    case DP_DS_PORT_TYPE_DP:
        return 0;
    case DP_DS_PORT_TYPE_DP_DUALMODE:
        if (is_edid_digital_input_dp(edid))
            return 0;
        return 0;
    case DP_DS_PORT_TYPE_HDMI:
    case DP_DS_PORT_TYPE_DVI:
    case DP_DS_PORT_TYPE_VGA:
        if ((dpcd[DP_DOWNSTREAMPORT_PRESENT] & DP_DETAILED_CAP_INFO_AVAILABLE) == 0)
            return 8;

        switch (port_cap[2] & DP_DS_MAX_BPC_MASK) {
        case DP_DS_8BPC:
            return 8;
        case DP_DS_10BPC:
            return 10;
        case DP_DS_12BPC:
            return 12;
        case DP_DS_16BPC:
            return 16;
        default:
            return 8;
        }
        break;
    default:
        return 8;
    }
}

/**
 * disp_dp_downstream_420_passthrough() - determine downstream facing port
 *                                       YCbCr 4:2:0 pass-through capability
 * @dpcd: DisplayPort configuration data
 * @port_cap: downstream facing port capabilities
 *
 * Returns: whether the downstream facing port can pass through YCbCr 4:2:0
 */
disp_u8_t disp_dp_downstream_420_passthrough(disp_u8_t dpcd[DP_RECEIVER_CAP_SIZE],
                       disp_u8_t port_cap[4])
{
    if (!disp_dp_is_branch(dpcd))
        return DISP_FALSE ;

    if (dpcd[DP_DPCD_REV] < 0x13)
        return DISP_FALSE ;

    switch (port_cap[0] & DP_DS_PORT_TYPE_MASK) {
    case DP_DS_PORT_TYPE_DP:
        return DISP_TRUE;
    case DP_DS_PORT_TYPE_HDMI:
        if ((dpcd[DP_DOWNSTREAMPORT_PRESENT] & DP_DETAILED_CAP_INFO_AVAILABLE) == 0)
            return DISP_FALSE ;

        return port_cap[3] & DP_DS_HDMI_YCBCR420_PASS_THROUGH;
    default:
        return DISP_FALSE ;
    }
}

/**
 * disp_dp_downstream_444_to_420_conversion() - determine downstream facing port
 *                                             YCbCr 4:4:4->4:2:0 conversion capability
 * @dpcd: DisplayPort configuration data
 * @port_cap: downstream facing port capabilities
 *
 * Returns: whether the downstream facing port can convert YCbCr 4:4:4 to 4:2:0
 */
disp_u8_t disp_dp_downstream_444_to_420_conversion(disp_u8_t dpcd[DP_RECEIVER_CAP_SIZE],
                         disp_u8_t port_cap[4])
{
    if (!disp_dp_is_branch(dpcd))
        return DISP_FALSE ;

    if (dpcd[DP_DPCD_REV] < 0x13)
        return DISP_FALSE ;

    switch (port_cap[0] & DP_DS_PORT_TYPE_MASK) {
    case DP_DS_PORT_TYPE_HDMI:
        if ((dpcd[DP_DOWNSTREAMPORT_PRESENT] & DP_DETAILED_CAP_INFO_AVAILABLE) == 0)
            return DISP_FALSE ;

        return port_cap[3] & DP_DS_HDMI_YCBCR444_TO_420_CONV;
    default:
        return DISP_FALSE ;
    }
}

/**
 * disp_dp_downstream_rgb_to_ycbcr_conversion() - determine downstream facing port
 *                                               RGB->YCbCr conversion capability
 * @dpcd: DisplayPort configuration data
 * @port_cap: downstream facing port capabilities
 * @color_spc: Colorspace for which conversion cap is sought
 *
 * Returns: whether the downstream facing port can convert RGB->YCbCr for a given
 * colorspace.
 */
disp_u8_t disp_dp_downstream_rgb_to_ycbcr_conversion(disp_u8_t dpcd[DP_RECEIVER_CAP_SIZE],
                           disp_u8_t port_cap[4],
                           disp_u8_t color_spc)
{
    if (!disp_dp_is_branch(dpcd))
        return DISP_FALSE ;

    if (dpcd[DP_DPCD_REV] < 0x13)
        return DISP_FALSE ;

    switch (port_cap[0] & DP_DS_PORT_TYPE_MASK) {
    case DP_DS_PORT_TYPE_HDMI:
        if ((dpcd[DP_DOWNSTREAMPORT_PRESENT] & DP_DETAILED_CAP_INFO_AVAILABLE) == 0)
            return DISP_FALSE ;

        return port_cap[3] & color_spc;
    default:
        return DISP_FALSE ;
    }
}

/**
 * disp_dp_downstream_mode() - return a mode for downstream facing port
 * @dev: DRM device
 * @dpcd: DisplayPort configuration data
 * @port_cap: port capabilities
 *
 * Provides a suitable mode for downstream facing ports without EDID.
 *
 * Returns: A new disp_display_mode on success or DISP_NULL on failure
 */
pdisp_display_mode_t disp_dp_downstream_mode(disp_u8_t dpcd[DP_RECEIVER_CAP_SIZE],
               disp_u8_t port_cap[4])

{
    disp_u8_t vic;

    if (!disp_dp_is_branch(dpcd))
        return DISP_NULL;

    if (dpcd[DP_DPCD_REV] < 0x11)
        return DISP_NULL;

    switch (port_cap[0] & DP_DS_PORT_TYPE_MASK) {
    case DP_DS_PORT_TYPE_NON_EDID:
        switch (port_cap[0] & DP_DS_NON_EDID_MASK) {
        case DP_DS_NON_EDID_720x480i_60:
            vic = 6;
            break;
        case DP_DS_NON_EDID_720x480i_50:
            vic = 21;
            break;
        case DP_DS_NON_EDID_1920x1080i_60:
            vic = 5;
            break;
        case DP_DS_NON_EDID_1920x1080i_50:
            vic = 20;
            break;
        case DP_DS_NON_EDID_1280x720_60:
            vic = 4;
            break;
        case DP_DS_NON_EDID_1280x720_50:
            vic = 19;
            break;
        default:
            return DISP_NULL;
        }
        return disp_display_mode_from_cea_vic(vic);
    default:
        return DISP_NULL;
    }
}

/**
 * disp_dp_downstream_id() - identify branch device
 * @aux: DisplayPort AUX channel
 * @id: DisplayPort branch device id
 *
 * Returns branch device id on success or DISP_NULL on failure
 */
disp_s32_t disp_dp_downstream_id(struct disp_dp_aux *aux, char id[6])
{
    return disp_dp_dpcd_read(aux, DP_BRANCH_ID, id, 6);
}

/**
 * disp_dp_subconnector_type() - get DP branch device type
 * @dpcd: DisplayPort configuration data
 * @port_cap: port capabilities
 */
DISP_MODE_SUBCONNECTOR_TYPE
disp_dp_subconnector_type(disp_u8_t dpcd[DP_RECEIVER_CAP_SIZE],
             disp_u8_t port_cap[4])
{
    disp_s32_t type;
    if (!disp_dp_is_branch(dpcd))
        return DISP_MODE_SUBCONNECTOR_NATIVE;
    /* DP 1.0 approach */
    if (dpcd[DP_DPCD_REV] == DP_DPCD_REV_10) {
        type = dpcd[DP_DOWNSTREAMPORT_PRESENT] &
               DP_DWN_STRM_PORT_TYPE_MASK;

        switch (type) {
        case DP_DWN_STRM_PORT_TYPE_TMDS:
            /* Can be HDMI or DVI-D, DVI-D is a safer option */
            return DISP_MODE_SUBCONNECTOR_DVID;
        case DP_DWN_STRM_PORT_TYPE_ANALOG:
            /* Can be VGA or DVI-A, VGA is more popular */
            return DISP_MODE_SUBCONNECTOR_VGA;
        case DP_DWN_STRM_PORT_TYPE_DP:
            return DISP_MODE_SUBCONNECTOR_DISPLAYPORT;
        case DP_DWN_STRM_PORT_TYPE_OTHER:
        default:
            return DISP_MODE_SUBCONNECTOR_UNKNOWN;
        }
    }
    type = port_cap[0] & DP_DS_PORT_TYPE_MASK;

    switch (type) {
    case DP_DS_PORT_TYPE_DP:
    case DP_DS_PORT_TYPE_DP_DUALMODE:
        return DISP_MODE_SUBCONNECTOR_DISPLAYPORT;
    case DP_DS_PORT_TYPE_VGA:
        return DISP_MODE_SUBCONNECTOR_VGA;
    case DP_DS_PORT_TYPE_DVI:
        return DISP_MODE_SUBCONNECTOR_DVID;
    case DP_DS_PORT_TYPE_HDMI:
        return DISP_MODE_SUBCONNECTOR_HDMIA;
    case DP_DS_PORT_TYPE_WIRELESS:
        return DISP_MODE_SUBCONNECTOR_WIRELESS;
    case DP_DS_PORT_TYPE_NON_EDID:
    default:
        return DISP_MODE_SUBCONNECTOR_UNKNOWN;
    }
}

/**
 * disp_dp_read_sink_count_cap() - Check whether a given connector has a valid sink
 * count
 * @connector: The DRM connector to check
 * @dpcd: A cached copy of the connector's DPCD RX capabilities
 * @desc: A cached copy of the connector's DP descriptor
 *
 * See also: disp_dp_read_sink_count()
 *
 * Returns: %DISP_TRUE if the (e)DP connector has a valid sink count that should
 * be probed, %DISP_FALSE  otherwise.
 */
disp_u8_t disp_dp_read_sink_count_cap(disp_connector_t *connector,
                disp_u8_t dpcd[DP_RECEIVER_CAP_SIZE],
                struct disp_dp_desc *desc)
{
    /* Some eDP panels don't set a valid value for the sink count */
    return connector->type != DISP_CONNECTOR_eDP &&
        dpcd[DP_DPCD_REV] >= DP_DPCD_REV_11 &&
        dpcd[DP_DOWNSTREAMPORT_PRESENT] & DP_DWN_STRM_PORT_PRESENT &&
        !disp_dp_has_quirk(desc, DP_DPCD_QUIRK_NO_SINK_COUNT);
}

/**
 * disp_dp_read_sink_count() - Retrieve the sink count for a given sink
 * @aux: The DP AUX channel to use
 *
 * See also: disp_dp_read_sink_count_cap()
 *
 * Returns: The current sink count reported by @aux, or a negative error code
 * otherwise.
 */
disp_s32_t disp_dp_read_sink_count(struct disp_dp_aux *aux)
{
    disp_u8_t count;
    disp_s32_t ret;

    ret = disp_dp_dpcd_readb(aux, DP_SINK_COUNT, &count);
    if (ret < 0)
        return ret;
    if (ret != 1)
        return -DISP_EIO;

    return DP_GET_SINK_COUNT(count);
}

/**
 * disp_dp_read_lt_status() - Read dpcd 0x200 - 0x205
 * DP 1.4 CTS requires read 0x200 - 0x205 before link training
 */
disp_s32_t disp_dp_read_sink_status(pdisp_connector_t pdisp_connector, disp_u8_t status[DP_SINK_STATUS_SIZE])
{
    struct disp_dp_aux *aux = (struct disp_dp_aux *)pdisp_connector->ddc;
    disp_s32_t ret;

    ret = disp_dp_dpcd_read(aux, DP_SINK_COUNT, status, DP_SINK_STATUS_SIZE);
    if (ret != DP_SINK_STATUS_SIZE)
    {
        disp_print(DDM_WARNING, "disp_dp_read_lt_status, read_size[%d]\n", ret);
        return -1;
    }

    return 0;
}

static disp_s32_t disp_dp_aux_get_crc(struct disp_dp_aux *aux, disp_u8_t *crc)
{
    disp_u8_t buf, count;
    disp_s32_t ret;

    ret = disp_dp_dpcd_readb(aux, DP_TEST_SINK, &buf);
    if (ret < 0)
        return ret;

    if (!(buf & DP_TEST_SINK_START))
        disp_print(DDM_INFO, "! buf & DP_TEST_SINK_START\n");

    ret = disp_dp_dpcd_readb(aux, DP_TEST_SINK_MISC, &buf);
    if (ret < 0)
        return ret;

    count = buf & DP_TEST_COUNT_MASK;
    if (count == aux->crc_count)
        return -DISP_EAGAIN; /* No CRC yet */

    aux->crc_count = count;

    /*
     * At DP_TEST_CRC_R_CR, there's 6 bytes containing CRC data, 2 bytes
     * per component (RGB or CrYCb).
     */
    ret = disp_dp_dpcd_read(aux, DP_TEST_CRC_R_CR, crc, 6);
    if (ret < 0)
        return ret;

    return 0;
}

disp_u32_t dp_aux_wait_ready(struct disp_dp_aux *aux)
{
    pdisp_connector_t pconnector = aux->pconnector;
    pdisp_device_t pdd = pconnector->disp_device;
    disp_u32_t aux_req = 0, loop_count = 0;

    if (!dp_hpd_ok(pconnector))
    {
        disp_print(DDM_WARNING, "wait aux: hpd status not ok\n");
        return DISP_FALSE;
    }

    dp_read_field(pconnector->index, Reg_Dpt_Reg5, Register_Dpt_Reg5_Start, 1,
        Aux_Req, &aux_req);

    while (aux_req && loop_count < 10) {
        disp_delay_us(aux->aux_delay_us);
        dp_read_field(pconnector->index, Reg_Dpt_Reg5, Register_Dpt_Reg5_Start, 1,
            Aux_Req, &aux_req);
        loop_count++;
    }

    if (loop_count == 10)
    {
        disp_print(DDM_WARNING, "Aux_Req is not 0\n");
        return DISP_FALSE;
    }

    return DISP_TRUE;
}

DISP_STATUS dp_aux_check_reply_status(struct disp_dp_aux *aux, struct disp_dp_aux_msg *msg)
{
    pdisp_connector_t pconnector = aux->pconnector;
    pdisp_device_t pdd = pconnector->disp_device;
    disp_u32_t i = 0, ready = 0, aux_valid = 0, aux_com = 0, timeout = 0;

    for(i = 0; i < 100; i++)
    {
        dp_read_field(pconnector->index, Reg_Dpt_Reg5, Register_Dpt_Reg5_Start, 2,
            Aux_Val_Flag, &aux_valid,
            Sw_Aux_Timeout_Flag, &timeout);
        if (aux_valid) {
            ready = 1;
            break;
        }

        if (timeout) {
            disp_print(DDM_WARNING, "Sw_Aux_Timeout_Flag\n");
            msg->reply = DP_AUX_HW_REPLY_TIMEOUT; // Aux_Com_O not ready yet.
            return -DISP_ETIMEDOUT;
        }

        disp_delay_us(aux->aux_delay_us);
    }

    if(ready)
    {
        dp_read_field(pconnector->index, Reg_Dpt_Reg29, Register_Dpt_Reg29_Start, 1,
            Aux_Com_O, &aux_com);
        msg->reply = (disp_u8_t)aux_com;

        return DISP_OK;
    }

    msg->reply = DP_AUX_HW_REPLY_BUSY;
    return -DISP_EBUSY;
}

static disp_s32_t aux_transfer(pdisp_void_t aux, struct disp_dp_aux_msg *msg)
{
    struct disp_dp_aux *paux = aux; 
    pdisp_connector_t pconnector = paux->pconnector;
    pdisp_device_t pdd = pconnector->disp_device;
    disp_s32_t ret = 0;
    disp_u32_t v[4] = {0};

    if(DISP_TRUE != dp_aux_wait_ready(paux))
    {
        msg->reply = DP_AUX_HW_REPLY_TIMEOUT;
        return -DISP_EIO;
    }

    switch (msg->request & ~DP_AUX_I2C_MOT) {
        case DP_AUX_NATIVE_WRITE:
        case DP_AUX_I2C_WRITE:
        case DP_AUX_I2C_WRITE_STATUS_UPDATE:
            disp_memcpy(v, msg->buffer, msg->size);
            dp_write_field(pconnector->index, Reg_Dpt_Reg7, Register_Dpt_Reg7_Start, 1,
                Aux_Dat_0, v[0]);
            dp_write_field(pconnector->index, Reg_Dpt_Reg8, Register_Dpt_Reg8_Start, 1,
                Aux_Dat_1, v[1]);
            dp_write_field(pconnector->index, Reg_Dpt_Reg9, Register_Dpt_Reg9_Start, 1,
                Aux_Dat_2, v[2]);
            dp_write_field(pconnector->index, Reg_Dpt_Reg0a, Register_Dpt_Reg0a_Start, 1,
                Aux_Dat_3, v[3]);
        break;

        default:
        break;
    }

    //Aux_Num = the bytes behind address
    //native read, address + length(1 byte) = 1
    //native write, address + length(1 byte) + data(N byte) = size + 1
    if (!msg->size) // size 0, stop read/write
    {
        dp_write_field(pconnector->index, Reg_Dpt_Reg5, Register_Dpt_Reg5_Start, 1,
            Aux_Num, 0);
    }
    else if(DP_AUX_NATIVE_WRITE == (msg->request & ~DP_AUX_I2C_MOT) ||
        DP_AUX_I2C_WRITE == (msg->request & ~DP_AUX_I2C_MOT))
    {
        dp_write_field(pconnector->index, Reg_Dpt_Reg5, Register_Dpt_Reg5_Start, 1,
            Aux_Num, msg->size + 1);
    }
    else if(DP_AUX_NATIVE_READ == (msg->request & ~DP_AUX_I2C_MOT) ||
        DP_AUX_I2C_READ == (msg->request & ~DP_AUX_I2C_MOT))
    {
        dp_write_field(pconnector->index, Reg_Dpt_Reg5, Register_Dpt_Reg5_Start, 1,
            Aux_Num, 1);
    }
    else
    {
        disp_print(DDM_WARNING, "unexpected request[%x]\n", msg->request);
    }

    dp_write_field(pconnector->index, Reg_Dpt_Reg6, Register_Dpt_Reg6_Start, 3,
        Aux_Len, msg->size ? msg->size - 1 : 0,
        Aux_Com, msg->request, 
        Aux_Add, msg->address);

    dp_write_field(pconnector->index, Reg_Dpt_Reg5, Register_Dpt_Reg5_Start, 3,
        Aux_Req, 1,
        Aux_Val_Flag, 0,
        Sw_Aux_Timeout_Flag, 0);

    //here should return read back size, but on pz1, it can only read 3 bytes once request
    if(DISP_OK == dp_aux_check_reply_status(paux, msg))
    {
        ret = msg->size;
    }
    else
    {
        return -DISP_EIO;
    }

    switch (msg->request & ~DP_AUX_I2C_MOT) {
        case DP_AUX_NATIVE_READ:
        case DP_AUX_I2C_READ:
            dp_read_field(pconnector->index, Reg_Dpt_Reg2b, Register_Dpt_Reg2b_Start, 1,
                Aux_Rdat_0, &v[0]);
            dp_read_field(pconnector->index, Reg_Dpt_Reg2c, Register_Dpt_Reg2c_Start, 1,
                Aux_Rdat_1, &v[1]);
            dp_read_field(pconnector->index, Reg_Dpt_Reg2d, Register_Dpt_Reg2d_Start, 1,
                Aux_Rdat_2, &v[2]);
            dp_read_field(pconnector->index, Reg_Dpt_Reg2e, Register_Dpt_Reg2e_Start, 1,
                Aux_Rdat_3, &v[3]);
            disp_memcpy(msg->buffer, v, msg->size);

            dp_read_field(pconnector->index, Reg_Dpt_Reg29, Register_Dpt_Reg29_Start, 1,
                Aux_Len_O, &(msg->read_back_size));
            msg->read_back_size--;
            if (msg->read_back_size < msg->size)
                ret = msg->read_back_size;
        break;

        default:
        break;
    }

    return ret;
}

void disp_dp_aux_init(pdisp_connector_t pdp_connector)
{
    struct disp_dp_aux *aux = (struct disp_dp_aux*)pdp_connector->ddc;
    pdisp_device_t pdd = pdp_connector->disp_device;
//    mutex_init(&aux->hw_mutex);

    aux->pconnector = pdp_connector;
    aux->transfer = aux_transfer;
    aux->aux_delay_us = 100;
    if(pdd->flag.info.pz1_config)
        aux->aux_delay_us *= 10000; // 25M -> 0.0025M
}

#define PSR_SETUP_TIME(x) [DP_PSR_SETUP_TIME_ ## x >> DP_PSR_SETUP_TIME_SHIFT] = (x)

/**
 * disp_dp_psr_setup_time() - PSR setup in time usec
 * @psr_cap: PSR capabilities from DPCD
 *
 * Returns:
 * PSR setup time for the panel in microseconds,  negative
 * error code on failure.
 */
disp_s32_t disp_dp_psr_setup_time(disp_u8_t psr_cap[EDP_PSR_RECEIVER_CAP_SIZE])
{
    static disp_u16_t psr_setup_time_us[] = {
        PSR_SETUP_TIME(330),
        PSR_SETUP_TIME(275),
        PSR_SETUP_TIME(220),
        PSR_SETUP_TIME(165),
        PSR_SETUP_TIME(110),
        PSR_SETUP_TIME(55),
        PSR_SETUP_TIME(0),
    };
    disp_s32_t i;

    i = (psr_cap[1] & DP_PSR_SETUP_TIME_MASK) >> DP_PSR_SETUP_TIME_SHIFT;
    if (i >= ARRAY_SIZE(psr_setup_time_us))
        return -DISP_EINVAL;

    return psr_setup_time_us[i];
}

#undef PSR_SETUP_TIME

disp_s32_t disp_dp_start_crc(struct disp_dp_aux *aux)
{
    disp_u8_t buf;
    disp_s32_t ret;

    ret = disp_dp_dpcd_readb(aux, DP_TEST_SINK, &buf);
    if (ret < 0)
        return ret;

    ret = disp_dp_dpcd_writeb(aux, DP_TEST_SINK, buf | DP_TEST_SINK_START);
    if (ret < 0)
        return ret;

    return 0;
}

disp_s32_t disp_dp_stop_crc(struct disp_dp_aux *aux)
{
    disp_u8_t buf;
    disp_s32_t ret;

    ret = disp_dp_dpcd_readb(aux, DP_TEST_SINK, &buf);
    if (ret < 0)
        return ret;

    ret = disp_dp_dpcd_writeb(aux, DP_TEST_SINK, buf & ~DP_TEST_SINK_START);
    if (ret < 0)
        return ret;

    return 0;
}

struct dpcd_quirk {
    disp_u8_t oui[3];
    disp_u8_t device_id[6];
    disp_u8_t is_branch;
    disp_u32_t quirks;
};

#define OUI(first, second, third) { (first), (second), (third) }
#define DEVICE_ID(first, second, third, fourth, fifth, sixth) \
    { (first), (second), (third), (fourth), (fifth), (sixth) }

#define DEVICE_ID_ANY    DEVICE_ID(0, 0, 0, 0, 0, 0)

static struct dpcd_quirk dpcd_quirk_list[] = {
    /* Analogix 7737 needs reduced M and N at HBR2 link rates */
    { OUI(0x00, 0x22, 0xb9), DEVICE_ID_ANY, DISP_TRUE, BIT(DP_DPCD_QUIRK_CONSTANT_N) },
    /* LG LP140WF6-SPM1 eDP panel */
    { OUI(0x00, 0x22, 0xb9), DEVICE_ID('s', 'i', 'v', 'a', 'r', 'T'), DISP_FALSE , BIT(DP_DPCD_QUIRK_CONSTANT_N) },
    /* Apple panels need some additional handling to support PSR */
    { OUI(0x00, 0x10, 0xfa), DEVICE_ID_ANY, DISP_FALSE , BIT(DP_DPCD_QUIRK_NO_PSR) },
    /* CH7511 seems to leave SINK_COUNT zeroed */
    { OUI(0x00, 0x00, 0x00), DEVICE_ID('C', 'H', '7', '5', '1', '1'), DISP_FALSE , BIT(DP_DPCD_QUIRK_NO_SINK_COUNT) },
    /* Synaptics DP1.4 MST hubs can support DSC without virtual DPCD */
    { OUI(0x90, 0xCC, 0x24), DEVICE_ID_ANY, DISP_TRUE, BIT(DP_DPCD_QUIRK_DSC_WITHOUT_VIRTUAL_DPCD) },
    /* Apple MacBookPro 2017 15 inch eDP Retina panel reports too low DP_MAX_LINK_RATE */
    { OUI(0x00, 0x10, 0xfa), DEVICE_ID(101, 68, 21, 101, 98, 97), DISP_FALSE , BIT(DP_DPCD_QUIRK_CAN_DO_MAX_LINK_RATE_3_24_GBPS) },
};

#undef OUI

/*
 * Get a bit mask of DPCD quirks for the sink/branch device identified by
 * ident. The quirk data is shared but it's up to the drivers to act on the
 * data.
 *
 * For now, only the OUI (first three bytes) is used, but this may be extended
 * to device identification string and hardware/firmware revisions later.
 */
static disp_u32_t
disp_dp_get_quirks(struct disp_dp_dpcd_ident *ident, disp_u8_t is_branch)
{
    struct dpcd_quirk *quirk;
    disp_u32_t quirks = 0;
    disp_s32_t i;
    disp_u8_t any_device[] = DEVICE_ID_ANY;

    for (i = 0; i < ARRAY_SIZE(dpcd_quirk_list); i++) {
        quirk = &dpcd_quirk_list[i];

        if (quirk->is_branch != is_branch)
            continue;

        if (disp_memcmp(quirk->oui, ident->oui, sizeof(ident->oui)) != 0)
            continue;

        if (disp_memcmp(quirk->device_id, any_device, sizeof(any_device)) != 0 &&
            disp_memcmp(quirk->device_id, ident->device_id, sizeof(ident->device_id)) != 0)
            continue;

        quirks |= quirk->quirks;
    }

    return quirks;
}

#undef DEVICE_ID_ANY
#undef DEVICE_ID

disp_s32_t disp_dp_read_desc(struct disp_dp_aux *aux, struct disp_dp_desc *desc,
             disp_u8_t is_branch)
{
    struct disp_dp_dpcd_ident *ident = &desc->ident;
    disp_u32_t offset = is_branch ? DP_BRANCH_OUI : DP_SINK_OUI;
    disp_s32_t ret;

    ret = disp_dp_dpcd_read(aux, offset, ident, sizeof(*ident));
    if (ret < 0)
        return ret;

    desc->quirks = disp_dp_get_quirks(ident, is_branch);

    disp_print(DDM_TRACE,
            "DP: OUI %*phD dev-ID %*pE HW-rev %d.%d SW-rev %d.%d quirks 0x%04x\n",
            is_branch ? "branch" : "sink",
            (int)sizeof(ident->oui), ident->oui,
            ident->device_id, ident->hw_rev >> 4, ident->hw_rev & 0xf,
            ident->sw_major_rev, ident->sw_minor_rev, desc->quirks);

    return 0;
}

disp_u8_t disp_dp_dsc_sink_max_slice_count(disp_u8_t dsc_dpcd[DP_DSC_RECEIVER_CAP_SIZE],
                   disp_u8_t is_edp)
{
    disp_u8_t slice_cap1 = dsc_dpcd[DP_DSC_SLICE_CAP_1 - DP_DSC_SUPPORT];

    if (is_edp) {
        /* For eDP, register DSC_SLICE_CAPABILITIES_1 gives slice count */
        if (slice_cap1 & DP_DSC_4_PER_DP_DSC_SINK)
            return 4;
        if (slice_cap1 & DP_DSC_2_PER_DP_DSC_SINK)
            return 2;
        if (slice_cap1 & DP_DSC_1_PER_DP_DSC_SINK)
            return 1;
    } else {
        /* For DP, use values from DSC_SLICE_CAP_1 and DSC_SLICE_CAP2 */
        disp_u8_t slice_cap2 = dsc_dpcd[DP_DSC_SLICE_CAP_2 - DP_DSC_SUPPORT];

        if (slice_cap2 & DP_DSC_24_PER_DP_DSC_SINK)
            return 24;
        if (slice_cap2 & DP_DSC_20_PER_DP_DSC_SINK)
            return 20;
        if (slice_cap2 & DP_DSC_16_PER_DP_DSC_SINK)
            return 16;
        if (slice_cap1 & DP_DSC_12_PER_DP_DSC_SINK)
            return 12;
        if (slice_cap1 & DP_DSC_10_PER_DP_DSC_SINK)
            return 10;
        if (slice_cap1 & DP_DSC_8_PER_DP_DSC_SINK)
            return 8;
        if (slice_cap1 & DP_DSC_6_PER_DP_DSC_SINK)
            return 6;
        if (slice_cap1 & DP_DSC_4_PER_DP_DSC_SINK)
            return 4;
        if (slice_cap1 & DP_DSC_2_PER_DP_DSC_SINK)
            return 2;
        if (slice_cap1 & DP_DSC_1_PER_DP_DSC_SINK)
            return 1;
    }

    return 0;
}

disp_u8_t disp_dp_dsc_sink_line_buf_depth(disp_u8_t dsc_dpcd[DP_DSC_RECEIVER_CAP_SIZE])
{
    disp_u8_t line_buf_depth = dsc_dpcd[DP_DSC_LINE_BUF_BIT_DEPTH - DP_DSC_SUPPORT];

    switch (line_buf_depth & DP_DSC_LINE_BUF_BIT_DEPTH_MASK) {
    case DP_DSC_LINE_BUF_BIT_DEPTH_9:
        return 9;
    case DP_DSC_LINE_BUF_BIT_DEPTH_10:
        return 10;
    case DP_DSC_LINE_BUF_BIT_DEPTH_11:
        return 11;
    case DP_DSC_LINE_BUF_BIT_DEPTH_12:
        return 12;
    case DP_DSC_LINE_BUF_BIT_DEPTH_13:
        return 13;
    case DP_DSC_LINE_BUF_BIT_DEPTH_14:
        return 14;
    case DP_DSC_LINE_BUF_BIT_DEPTH_15:
        return 15;
    case DP_DSC_LINE_BUF_BIT_DEPTH_16:
        return 16;
    case DP_DSC_LINE_BUF_BIT_DEPTH_8:
        return 8;
    }

    return 0;
}

disp_s32_t disp_dp_dsc_sink_supported_input_bpcs(disp_u8_t dsc_dpcd[DP_DSC_RECEIVER_CAP_SIZE],
                     disp_u8_t dsc_bpc[3])
{
    disp_s32_t num_bpc = 0;
    disp_u8_t color_depth = dsc_dpcd[DP_DSC_DEC_COLOR_DEPTH_CAP - DP_DSC_SUPPORT];

    if (color_depth & DP_DSC_12_BPC)
        dsc_bpc[num_bpc++] = 12;
    if (color_depth & DP_DSC_10_BPC)
        dsc_bpc[num_bpc++] = 10;
    if (color_depth & DP_DSC_8_BPC)
        dsc_bpc[num_bpc++] = 8;

    return num_bpc;
}

disp_s32_t disp_dp_read_lttpr_common_caps(struct disp_dp_aux *aux,
                  disp_u8_t caps[DP_LTTPR_COMMON_CAP_SIZE])
{
    disp_s32_t ret;

    ret = disp_dp_dpcd_read(aux,
                   DP_LT_TUNABLE_PHY_REPEATER_FIELD_DATA_STRUCTURE_REV,
                   caps, DP_LTTPR_COMMON_CAP_SIZE);
    if (ret < 0)
        return ret;

    if (ret != DP_LTTPR_COMMON_CAP_SIZE)
        disp_print(DDM_WARNING, "ret != DP_LTTPR_COMMON_CAP_SIZE\n");

    return 0;
}

disp_s32_t disp_dp_read_lttpr_phy_caps(struct disp_dp_aux *aux,
                   enum disp_dp_phy dp_phy,
                   disp_u8_t caps[DP_LTTPR_PHY_CAP_SIZE])
{
    disp_s32_t ret;

    ret = disp_dp_dpcd_read(aux,
                   DP_TRAINING_AUX_RD_INTERVAL_PHY_REPEATER(dp_phy),
                   caps, DP_LTTPR_PHY_CAP_SIZE);
    if (ret < 0)
        return ret;

    if (ret != DP_LTTPR_PHY_CAP_SIZE)
        disp_print(DDM_WARNING, "ret != DP_LTTPR_PHY_CAP_SIZE\n");

    return 0;
}

static disp_u8_t dp_lttpr_common_cap(disp_u8_t caps[DP_LTTPR_COMMON_CAP_SIZE], disp_s32_t r)
{
    return caps[r - DP_LT_TUNABLE_PHY_REPEATER_FIELD_DATA_STRUCTURE_REV];
}

static inline disp_u32_t Ilog2(disp_u32_t v)
{
    disp_u32_t r;
    disp_u32_t shift;

    r = (v > 0xffff) << 4; v >>= r;
    shift = (v > 0xff) << 3; v >>= shift; r |= shift;
    shift = (v > 0xf) << 2; v >>= shift; r |= shift;
    shift = (v > 0x3) << 1; v >>= shift; r |= shift;
    r |= (v >> 1);
    return r;
}

disp_s32_t disp_dp_lttpr_count(disp_u8_t caps[DP_LTTPR_COMMON_CAP_SIZE])
{
    disp_u8_t count = dp_lttpr_common_cap(caps, DP_PHY_REPEATER_CNT);

    switch (get_bit_num(count)) {
    case 0:
        return 0;
    case 1:
        return 8 - Ilog2(count);
    case 8:
        return -DISP_ERANGE;
    default:
        return -DISP_EINVAL;
    }
}

disp_s32_t disp_dp_lttpr_max_link_rate(disp_u8_t caps[DP_LTTPR_COMMON_CAP_SIZE])
{
    disp_u8_t rate = dp_lttpr_common_cap(caps, DP_MAX_LINK_RATE_PHY_REPEATER);

    return disp_dp_bw_code_to_link_rate(rate);
}

disp_s32_t disp_dp_lttpr_max_lane_count(disp_u8_t caps[DP_LTTPR_COMMON_CAP_SIZE])
{
    disp_u8_t max_lanes = dp_lttpr_common_cap(caps, DP_MAX_LANE_COUNT_PHY_REPEATER);

    return max_lanes & DP_MAX_LANE_COUNT_MASK;
}

disp_u8_t
disp_dp_lttpr_voltage_swing_level_3_supported(disp_u8_t caps[DP_LTTPR_PHY_CAP_SIZE])
{
    disp_u8_t txcap = dp_lttpr_phy_cap(caps, DP_TRANSMITTER_CAPABILITY_PHY_REPEATER1);

    return txcap & DP_VOLTAGE_SWING_LEVEL_3_SUPPORTED;
}

disp_u8_t
disp_dp_lttpr_pre_emphasis_level_3_supported(disp_u8_t caps[DP_LTTPR_PHY_CAP_SIZE])
{
    disp_u8_t txcap = dp_lttpr_phy_cap(caps, DP_TRANSMITTER_CAPABILITY_PHY_REPEATER1);

    return txcap & DP_PRE_EMPHASIS_LEVEL_3_SUPPORTED;
}

disp_s32_t disp_dp_get_phy_test_pattern(struct disp_dp_aux *aux,
                struct disp_dp_phy_test_params *data)
{
    disp_s32_t err;
    disp_u8_t rate, lanes;

    err = disp_dp_dpcd_readb(aux, DP_TEST_LINK_RATE, &rate);
    if (err < 0)
        return err;
    data->link_rate = disp_dp_bw_code_to_link_rate(rate);

    err = disp_dp_dpcd_readb(aux, DP_TEST_LANE_COUNT, &lanes);
    if (err < 0)
        return err;
    data->num_lanes = lanes & DP_MAX_LANE_COUNT_MASK;

    if (lanes & DP_ENHANCED_FRAME_CAP)
        data->enhanced_frame_cap = DISP_TRUE;

    err = disp_dp_dpcd_readb(aux, DP_PHY_TEST_PATTERN, &data->phy_pattern);
    if (err < 0)
        return err;

    switch (data->phy_pattern) {
    case DP_PHY_TEST_PATTERN_80BIT_CUSTOM:
        err = disp_dp_dpcd_read(aux, DP_TEST_80BIT_CUSTOM_PATTERN_7_0,
                       &data->custom80, sizeof(data->custom80));
        if (err < 0)
            return err;

        break;
    case DP_PHY_TEST_PATTERN_CP2520:
        err = disp_dp_dpcd_read(aux, DP_TEST_HBR2_SCRAMBLER_RESET,
                       &data->hbr2_reset,
                       sizeof(data->hbr2_reset));
        if (err < 0)
            return err;
    }

    return 0;
}

disp_s32_t disp_dp_set_phy_test_pattern(struct disp_dp_aux *aux,
                struct disp_dp_phy_test_params *data, disp_u8_t dp_rev)
{
    disp_s32_t err, i;
    disp_u8_t link_config[2];
    disp_u8_t test_pattern;

    link_config[0] = disp_dp_link_rate_to_bw_code(data->link_rate);
    link_config[1] = data->num_lanes;
    if (data->enhanced_frame_cap)
        link_config[1] |= DP_LANE_COUNT_ENHANCED_FRAME_EN;
    err = disp_dp_dpcd_write(aux, DP_LINK_BW_SET, link_config, 2);
    if (err < 0)
        return err;

    test_pattern = data->phy_pattern;
    if (dp_rev < 0x12) {
        test_pattern = (test_pattern << 2) &
                   DP_LINK_QUAL_PATTERN_11_MASK;
        err = disp_dp_dpcd_writeb(aux, DP_TRAINING_PATTERN_SET,
                     test_pattern);
        if (err < 0)
            return err;
    } else {
        for (i = 0; i < data->num_lanes; i++) {
            err = disp_dp_dpcd_writeb(aux,
                         DP_LINK_QUAL_LANE0_SET + i,
                         test_pattern);
            if (err < 0)
                return err;
        }
    }

    return 0;
}


/**
 * remove pcon funcs
*/

disp_void_t disp_edp_backlight_aux_get_brightness(struct disp_dp_aux *aux, struct disp_edp_backlight_info *bl)
{
    disp_u32_t ret = 0;
    disp_u8_t reg_value;
    disp_u8_t buf[2];

    ret = disp_dp_dpcd_readb(aux, DP_EDP_BACKLIGHT_MODE_SET_REGISTER, &reg_value);
    if (ret != 1)
        disp_print(DDM_WARNING, "Failed to read backlight mode: %d\n", ret);
    bl->set_mode = (reg_value & DP_EDP_BACKLIGHT_CONTROL_MODE_MASK);

    if (bl->set_mode == DP_EDP_BACKLIGHT_CONTROL_MODE_DPCD || bl->set_mode == DP_EDP_BACKLIGHT_CONTROL_MODE_PRODUCT)
    {
        disp_u32_t size = 1 + bl->lsb_reg_used;

        ret = disp_dp_dpcd_read(aux, DP_EDP_BACKLIGHT_BRIGHTNESS_MSB, buf, size);
        if (ret != size)
            disp_print(DDM_WARNING, "Failed to read backlight level: %d\n", ret);

        if (bl->lsb_reg_used)
            bl->cur_level = (buf[0] << 8) | buf[1];
        else
            bl->cur_level = buf[0];
    }
}

disp_void_t disp_edp_backlight_aux_set_level(struct disp_dp_aux *aux, struct disp_edp_backlight_info *bl)
{
    disp_s32_t ret;
    disp_u8_t buf[2] = {0};

    if (bl->lsb_reg_used) {
        buf[0] = (bl->cur_level & 0xff00) >> 8;
        buf[1] = (bl->cur_level & 0x00ff);
    } else {
        buf[0] = (disp_u8_t)bl->cur_level;
    }

    ret = disp_dp_dpcd_write(aux, DP_EDP_BACKLIGHT_BRIGHTNESS_MSB, buf, sizeof(buf));
    if (ret != sizeof(buf))
        disp_print(DDM_WARNING, "%s: Failed to write aux backlight level: %d\n", ret);
}

disp_void_t disp_edp_backlight_aux_set_mode(struct disp_dp_aux *aux, struct disp_edp_backlight_info *bl)
{
    disp_u32_t ret;
    disp_u8_t reg_value;

    ret = disp_dp_dpcd_readb(aux, DP_EDP_BACKLIGHT_MODE_SET_REGISTER, &reg_value);
    if (ret != 1) {
        disp_print(DDM_WARNING, "Failed to read aux mode set register: %d\n", ret);
    }

    reg_value &= ~DP_EDP_BACKLIGHT_CONTROL_MODE_MASK;
    reg_value |= bl->set_mode & DP_EDP_BACKLIGHT_CONTROL_MODE_MASK;
    ret = disp_dp_dpcd_writeb(aux, DP_EDP_BACKLIGHT_MODE_SET_REGISTER, reg_value);
    if (ret != 1)
        disp_print(DDM_WARNING, "Failed to write aux control mode: %d\n", ret);
}

disp_void_t disp_edp_backlight_aux_enable(struct disp_dp_aux *aux, struct disp_edp_backlight_info *bl)
{
    disp_s32_t ret;
    disp_u8_t buf;

    ret = disp_dp_dpcd_readb(aux, DP_EDP_DISPLAY_CONTROL_REGISTER, &buf);
    if (ret != 1) {
        disp_print(DDM_WARNING, "Failed to read aux display control register: %d\n", ret);
    }
    if (bl->set_aux_backlight)
        buf |= DP_EDP_BACKLIGHT_ENABLE;
    else
        buf &= ~DP_EDP_BACKLIGHT_ENABLE;

    ret = disp_dp_dpcd_writeb(aux, DP_EDP_DISPLAY_CONTROL_REGISTER, buf);
    if (ret != 1)
        disp_print(DDM_WARNING, "Failed to write aux display control register: %d\n", ret);
}

disp_void_t disp_edp_backlight_aux_probe_freq_within_25percent_desire(struct disp_dp_aux *aux,
    struct disp_edp_backlight_info *bl, disp_u32_t driver_pwm_freq_hz, disp_u8_t edp_dpcd[EDP_DISPLAY_CTL_CAP_SIZE])
{
    disp_s32_t fxp, fxp_min, fxp_max, fxp_actual, f = 1;
    disp_s32_t ret;
    disp_u8_t pn, pn_min, pn_max;

    ret = disp_dp_dpcd_readb(aux, DP_EDP_PWMGEN_BIT_COUNT, &pn);
    if (ret != 1)
        disp_print(DDM_WARNING, "Failed to read pwmgen bit count cap: %d\n", ret);

    pn &= DP_EDP_PWMGEN_BIT_COUNT_MASK;
    bl->max = (1 << pn) - 1;
    if (!driver_pwm_freq_hz)
        return;

    /*
     * Set PWM Frequency divider to match desired frequency provided by the driver.
     * The PWM Frequency is calculated as 27Mhz / (F x P).
     * - Where F = PWM Frequency Pre-Divider value programmed by field 7:0 of the
     *             EDP_BACKLIGHT_FREQ_SET register (DPCD Address 00728h)
     * - Where P = 2^Pn, where Pn is the value programmed by field 4:0 of the
     *             EDP_PWMGEN_BIT_COUNT register (DPCD Address 00724h)
     */

    /* Find desired value of (F x P)
     * Note that, if F x P is out of supported range, the maximum value or minimum value will
     * applied automatically. So no need to check that.
     */
    fxp = (disp_s32_t)DIV_ROUND_CLOSEST(1000 * DP_EDP_BACKLIGHT_FREQ_BASE_KHZ, driver_pwm_freq_hz);

    /* Use highest possible value of Pn for more granularity of brightness adjustment while
     * satisfying the conditions below.
     * - Pn is in the range of Pn_min and Pn_max
     * - F is in the range of 1 and 255
     * - FxP is within 25% of desired value.
     *   Note: 25% is arbitrary value and may need some tweak.
     */
    ret = disp_dp_dpcd_readb(aux, DP_EDP_PWMGEN_BIT_COUNT_CAP_MIN, &pn_min);
    if (ret != 1)
        disp_print(DDM_WARNING, "Failed to read pwmgen bit count cap min: %d\n", ret);

    ret = disp_dp_dpcd_readb(aux, DP_EDP_PWMGEN_BIT_COUNT_CAP_MAX, &pn_max);
    if (ret != 1)
        disp_print(DDM_WARNING, "Failed to read pwmgen bit count cap max: %d\n", ret);
    pn_min &= DP_EDP_PWMGEN_BIT_COUNT_MASK;
    pn_max &= DP_EDP_PWMGEN_BIT_COUNT_MASK;

    /* Ensure frequency is within 25% of desired value */
    fxp_min = (disp_s32_t)DIV_ROUND_CLOSEST(fxp * 3, 4);
    fxp_max = (disp_s32_t)DIV_ROUND_CLOSEST(fxp * 5, 4);
    if (fxp_min < (1 << pn_min) || (255 << pn_max) < fxp_max)
    {
        disp_print(DDM_INFO, "Driver defined backlight frequency (%d) out of range\n", driver_pwm_freq_hz);
        return;
    }

    for (pn = pn_max; pn >= pn_min; pn--) {
        f = DIV_ROUND_CLOSEST(fxp, 1 << pn) < 1 ? 1 : (disp_s32_t)DIV_ROUND_CLOSEST(fxp, 1 << pn);
        f = DIV_ROUND_CLOSEST(fxp, 1 << pn) > 255 ? 255 : (disp_s32_t)DIV_ROUND_CLOSEST(fxp, 1 << pn);
        fxp_actual = f << pn;
        if (fxp_min <= fxp_actual && fxp_actual <= fxp_max)
            break;
    }

    ret = disp_dp_dpcd_writeb(aux, DP_EDP_PWMGEN_BIT_COUNT, pn);
    if (ret != 1)
        disp_print(DDM_WARNING, "Failed to write aux pwmgen bit count: %d\n", ret);
    bl->pwmgen_bit_count = pn;
    bl->max = (1 << pn) - 1;

    if (edp_dpcd[2] & DP_EDP_BACKLIGHT_FREQ_AUX_SET_CAP) {
        bl->pwm_freq_pre_divider = (disp_u8_t)f;
        disp_print(DDM_WARNING, "Using backlight frequency from driver (%dHz)\n", driver_pwm_freq_hz);
    }
}

disp_void_t disp_edp_backlight_aux_set_pwmgen_bit(struct disp_dp_aux *aux, struct disp_edp_backlight_info *bl)
{
    disp_u32_t ret;

    ret = disp_dp_dpcd_writeb(aux, DP_EDP_PWMGEN_BIT_COUNT, bl->pwmgen_bit_count);
    if (ret != 1)
        disp_print(DDM_WARNING, "Failed to write aux backlight pwmgen: %d\n", ret);
}

disp_void_t disp_edp_backlight_aux_set_pre_freq(struct disp_dp_aux *aux, struct disp_edp_backlight_info *bl)
{
    disp_u32_t ret;

    ret = disp_dp_dpcd_writeb(aux, DP_EDP_BACKLIGHT_FREQ_SET, bl->pwm_freq_pre_divider);
    if (ret != 1)
        disp_print(DDM_WARNING, "Failed to write aux backlight pre freq: %d\n", ret);
}

disp_void_t disp_edp_backlight_aux_enable_freq(struct disp_dp_aux *aux, struct disp_edp_backlight_info *bl)
{
    disp_u32_t ret;
    disp_u8_t reg_value;

    ret = disp_dp_dpcd_readb(aux, DP_EDP_BACKLIGHT_MODE_SET_REGISTER, &reg_value);
    if (ret != 1)
        disp_print(DDM_WARNING, "Failed to read aux mode set register: %d\n", ret);

    if (bl->set_freq_aux)
        reg_value |= DP_EDP_BACKLIGHT_FREQ_AUX_SET_ENABLE;
    else
        reg_value &= ~DP_EDP_BACKLIGHT_FREQ_AUX_SET_ENABLE;
    ret = disp_dp_dpcd_writeb(aux, DP_EDP_BACKLIGHT_MODE_SET_REGISTER, reg_value);
    if (ret != 1)
        disp_print(DDM_WARNING, "Failed to write aux backlight mode: %d\n", ret);
}

disp_void_t disp_edp_backlight_aux_get_freq(struct disp_dp_aux *aux, struct disp_edp_backlight_info *bl)
{
    disp_u32_t ret;
    disp_u8_t reg_value;

    ret = disp_dp_dpcd_readb(aux, DP_EDP_BACKLIGHT_MODE_SET_REGISTER, &reg_value);
    if (ret != 1)
        disp_print(DDM_WARNING, "Failed to read aux mode set register: %d\n", ret);
    bl->set_freq_aux = (reg_value & DP_EDP_BACKLIGHT_FREQ_AUX_SET_ENABLE) ? DISP_TRUE : DISP_FALSE;

    ret = disp_dp_dpcd_readb(aux, DP_EDP_PWMGEN_BIT_COUNT, &reg_value);
    if (ret != 1)
        disp_print(DDM_WARNING, "Failed to read aux backlight frequency: %d\n", ret);
    bl->pwmgen_bit_count = reg_value;

    ret = disp_dp_dpcd_readb(aux, DP_EDP_BACKLIGHT_FREQ_SET, &reg_value);
    if (ret != 1)
        disp_print(DDM_WARNING, "Failed to read aux backlight frequency: %d\n", ret);
    bl->pwm_freq_pre_divider = reg_value;
}

disp_void_t disp_edp_backlight_aux_set_adaptive(struct disp_dp_aux *aux, struct disp_edp_backlight_info *bl)
{
    disp_u32_t ret;
    
    ret = disp_dp_dpcd_writeb(aux, DP_EDP_DBC_MINIMUM_BRIGHTNESS_SET, bl->dbc_min_set);
    if (ret != 1)
        disp_print(DDM_WARNING, "Failed to write aux adaptive min register: %d\n", ret);

    ret = disp_dp_dpcd_writeb(aux, DP_EDP_DBC_MAXIMUM_BRIGHTNESS_SET, bl->dbc_max_set);
    if (ret != 1)
        disp_print(DDM_WARNING, "Failed to write aux adaptive max register: %d\n", ret);
}

disp_void_t disp_edp_backlight_aux_enable_adaptive(struct disp_dp_aux *aux, struct disp_edp_backlight_info *bl)
{
    disp_u32_t ret;
    disp_u8_t reg_value;

    ret = disp_dp_dpcd_readb(aux, DP_EDP_BACKLIGHT_MODE_SET_REGISTER, &reg_value);
    if (ret != 1)
        disp_print(DDM_WARNING, "Failed to read aux mode set register: %d\n", ret);

    if (bl->set_dbc)
        reg_value |= DP_EDP_DYNAMIC_BACKLIGHT_ENABLE;
    else
        reg_value &= ~DP_EDP_DYNAMIC_BACKLIGHT_ENABLE;
    ret = disp_dp_dpcd_writeb(aux, DP_EDP_BACKLIGHT_MODE_SET_REGISTER, reg_value);
    if (ret != 1)
        disp_print(DDM_WARNING, "Failed to write aux mode set: %d\n", ret);
}

disp_void_t disp_edp_backlight_aux_get_adaptive(struct disp_dp_aux *aux, struct disp_edp_backlight_info *bl)
{
    disp_u32_t ret;
    disp_u8_t reg_value;

    ret = disp_dp_dpcd_readb(aux, DP_EDP_BACKLIGHT_MODE_SET_REGISTER, &reg_value);
    if (ret != 1)
        disp_print(DDM_WARNING, "Failed to read aux mode set register: %d\n", ret);
    bl->set_dbc  = (reg_value & DP_EDP_DYNAMIC_BACKLIGHT_ENABLE) ? DISP_TRUE : DISP_FALSE;

    ret = disp_dp_dpcd_readb(aux, DP_EDP_DBC_MINIMUM_BRIGHTNESS_SET, &reg_value);
    if (ret != 1)
        disp_print(DDM_WARNING, "Failed to read aux adaptive min register: %d\n", ret);
    bl->dbc_min_set = reg_value;

    ret = disp_dp_dpcd_readb(aux, DP_EDP_DBC_MAXIMUM_BRIGHTNESS_SET, &reg_value);
    if (ret != 1)
        disp_print(DDM_WARNING, "Failed to read aux adaptive max control register: %d\n", ret);
    bl->dbc_max_set = reg_value;
}

disp_void_t disp_edp_backlight_aux_get_regional(struct disp_dp_aux *aux, struct disp_edp_backlight_info *bl)
{
    disp_u32_t ret;
    disp_u8_t reg_value;

    ret = disp_dp_dpcd_readb(aux, DP_EDP_BACKLIGHT_MODE_SET_REGISTER, &reg_value);
    if (ret != 1)
        disp_print(DDM_WARNING, "Failed to read aux regional control register: %d\n", ret);
    bl->set_regional  = (reg_value & DP_EDP_REGIONAL_BACKLIGHT_ENABLE) ? DISP_TRUE : DISP_FALSE;

    /* todo: read base & region level
     * DP_EDP_REGIONAL_BACKLIGHT_BASE, DP_EDP_REGIONAL_BACKLIGHT_0 ~ DP_EDP_REGIONAL_BACKLIGHT_14
     */
}

disp_void_t disp_edp_backlight_aux_set_regional(struct disp_dp_aux *aux, struct disp_edp_backlight_info *bl)
{
    disp_print(DDM_INFO, "disp_edp_backlight_aux_set_regional, aux[%llx], bl[%llx]\n", aux, bl);
    /* todo: write base & region level
     * DP_EDP_REGIONAL_BACKLIGHT_BASE, DP_EDP_REGIONAL_BACKLIGHT_0 ~ DP_EDP_REGIONAL_BACKLIGHT_14
     */
}

disp_void_t disp_edp_backlight_aux_enable_regional(struct disp_dp_aux *aux, struct disp_edp_backlight_info *bl)
{
    disp_u32_t ret;
    disp_u8_t reg_value;

    ret = disp_dp_dpcd_readb(aux, DP_EDP_BACKLIGHT_MODE_SET_REGISTER, &reg_value);
    if (ret != 1)
        disp_print(DDM_WARNING, "Failed to read aux regional en register: %d\n", ret);

    if (bl->set_regional)
        reg_value |= DP_EDP_REGIONAL_BACKLIGHT_ENABLE;
    else
        reg_value &= ~DP_EDP_REGIONAL_BACKLIGHT_ENABLE;

    ret = disp_dp_dpcd_writeb(aux, DP_EDP_BACKLIGHT_MODE_SET_REGISTER, reg_value);
    if (ret != 1)
        disp_print(DDM_WARNING, "Failed to write aux regional en register: %d\n", ret);
}

disp_void_t disp_edp_backlight_aux_get_caps(struct disp_dp_aux *aux,
    struct disp_edp_backlight_info *bl, disp_u8_t edp_dpcd[EDP_DISPLAY_CTL_CAP_SIZE])
{
    disp_u32_t ret = 0;
    disp_u8_t msb, mid, lsb;

    if (edp_dpcd[1] & DP_EDP_BACKLIGHT_AUX_ENABLE_CAP)
        bl->aux_enable = DISP_TRUE;
    if (edp_dpcd[2] & DP_EDP_BACKLIGHT_BRIGHTNESS_PWM_PIN_CAP)
        bl->pwm_duty_enable = DISP_TRUE; // check with VBT
    if (edp_dpcd[2] & DP_EDP_BACKLIGHT_BRIGHTNESS_AUX_SET_CAP)
    {
        /* check here latter
         * Linux directly set aux_set, if aux enable
         * We may use policy to decide aux/pwm/aux_pwm
         */
        //bl->aux_set = DISP_TRUE;
        bl->aux_enable = DISP_TRUE;
    }
    if (edp_dpcd[2] & DP_EDP_BACKLIGHT_BRIGHTNESS_BYTE_COUNT)
        bl->lsb_reg_used = DISP_TRUE;
    if (edp_dpcd[2] & DP_EDP_BACKLIGHT_AUX_PWM_PRODUCT_CAP)
        bl->aux_pwm_enable = DISP_TRUE; // check with VBT
    if (edp_dpcd[2] & DP_EDP_BACKLIGHT_FREQ_PWM_PIN_PASSTHRU_CAP)
        bl->pwm_freq_enable = DISP_TRUE; // check with VBT
    if (edp_dpcd[2] & DP_EDP_BACKLIGHT_FREQ_AUX_SET_CAP)
    {
        bl->aux_freq_enable = DISP_TRUE;

        ret = disp_dp_dpcd_readb(aux, DP_EDP_PWMGEN_BIT_COUNT, &bl->pwmgen_bit_count);
        if (ret != 1)
            disp_print(DDM_WARNING, "Failed to read pwmgen_bit_count, ret=%d\n", ret);
        ret = disp_dp_dpcd_readb(aux, DP_EDP_PWMGEN_BIT_COUNT_CAP_MIN, &bl->min_pn);
        if (ret != 1)
            disp_print(DDM_WARNING, "Failed to read min_pn, ret=%d\n", ret);
        ret = disp_dp_dpcd_readb(aux, DP_EDP_PWMGEN_BIT_COUNT_CAP_MAX, &bl->max_pn);
        if (ret != 1)
            disp_print(DDM_WARNING, "Failed to read max_pn, ret=%d\n", ret);

        ret = disp_dp_dpcd_readb(aux, DP_EDP_BACKLIGHT_FREQ_CAP_MIN_MSB, &msb);
        if (ret != 1)
            disp_print(DDM_WARNING, "Failed to read freq min msb, ret=%d\n", ret);
        ret = disp_dp_dpcd_readb(aux, DP_EDP_BACKLIGHT_FREQ_CAP_MIN_MID, &mid);
        if (ret != 1)
            disp_print(DDM_WARNING, "Failed to read freq min mid, ret=%d\n", ret);
        ret = disp_dp_dpcd_readb(aux, DP_EDP_BACKLIGHT_FREQ_CAP_MIN_LSB, &lsb);
        if (ret != 1)
            disp_print(DDM_WARNING, "Failed to read freq min lsb, ret=%d\n", ret);
        bl->min_freq_divider = msb | (mid << 8) | (lsb << 16);

        ret = disp_dp_dpcd_readb(aux, DP_EDP_BACKLIGHT_FREQ_CAP_MAX_MSB, &msb);
        if (ret != 1)
            disp_print(DDM_WARNING, "Failed to read freq max msb, ret=%d\n", ret);
        ret = disp_dp_dpcd_readb(aux, DP_EDP_BACKLIGHT_FREQ_CAP_MAX_MID, &mid);
        if (ret != 1)
            disp_print(DDM_WARNING, "Failed to read freq max mid, ret=%d\n", ret);
        ret = disp_dp_dpcd_readb(aux, DP_EDP_BACKLIGHT_FREQ_CAP_MAX_LSB, &lsb);
        if (ret != 1)
            disp_print(DDM_WARNING, "Failed to read freq max lsb, ret=%d\n", ret);
        bl->max_freq_divider = msb | (mid << 8) | (lsb << 16);
    }
    if (edp_dpcd[2] & DP_EDP_DYNAMIC_BACKLIGHT_CAP)
        bl->dbc_enable = DISP_TRUE;
    if (edp_dpcd[2] & DP_EDP_VBLANK_BACKLIGHT_UPDATE_CAP)
    {
        bl->regional_x = (edp_dpcd[4] & 0xf) + 1;
        bl->regional_y = (edp_dpcd[4] >> 4) + 1;

        if (bl->regional_x == 1 && bl->regional_y == 1)
        {
            disp_print(DDM_INFO,
                "Backlight caps: only one region[%u x %u], disable regional\n",
                bl->regional_x, bl->regional_y);
        }
        else
            bl->regional_enable = DISP_TRUE;

        if (bl->regional_x * bl->regional_y > 15)
        {
            disp_print(DDM_INFO,
                "Backlight caps: too many region[%u x %u], aux only could not handle\n",
                bl->regional_x, bl->regional_y);
        }
    }
}

disp_u8_t disp_edp_psr_aux_is_enable(struct disp_dp_aux *aux)
{
    disp_s32_t ret = 0;
    disp_u8_t reg_value = 0;

    ret = disp_dp_dpcd_readb(aux, DP_PSR_EN_CFG, &reg_value);
    if (ret != 1)
        disp_print(DDM_WARNING, "disp_edp_psr_aux_set, dpcd write failed: %d\n", ret);

    return (reg_value & DP_PSR_ENABLE) ? 1 : 0;
}

disp_void_t disp_edp_psr_aux_set(struct disp_dp_aux *aux, struct disp_edp_psr_info *ppsr)
{
    disp_s32_t ret = 0;
    disp_u8_t reg_value = 0, psr2_protocol = 0;

    psr2_protocol = (1 == ppsr->version) ? 0 : 1;
    reg_value = ppsr->set_active | (ppsr->prefer_main_link_remain << 1) |
                (ppsr->prefer_crc_on << 2) |
                (ppsr->set_frame_next << 3) |
                (ppsr->set_su_y << 4) |
                (ppsr->set_crc_err_irq << 5) |
                (psr2_protocol << 6);
    ret = disp_dp_dpcd_writeb(aux, DP_PSR_EN_CFG, reg_value);
    if (ret != 1)
        disp_print(DDM_WARNING, "disp_edp_psr_aux_set, dpcd write failed: %d\n", ret);
}
