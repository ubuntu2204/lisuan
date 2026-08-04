/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#include "disp_hdmi_connector.h"
#include "disp_device.h"

#define disp_print(level, log, ...) disp_print_ex(DISP_PRINT_MASK_CONNECTOR_HDMI, level, log, ##__VA_ARGS__)

static DISP_CONNECT_STATE hdmi_connector_detect(pdisp_connector_t pdisp_connector)
{
    if(DISP_TRUE == pdisp_connector->state.bfake)
        return DISP_CONNECTTED;

    /* TODO: implement hdmi_probe_edid_block that add i2c module to support hdmi ddc read/write. */

    return DISP_DISCONNECTED;
}

static struct disp_connector_func hdmi_connector_func =
{
    .detect = hdmi_connector_detect,
};

disp_void_t disp_hdmi_connector_sw_init(pdisp_void_t pdisp_connector)
{
    pdisp_connector_t phdmi_connector = (pdisp_connector_t)pdisp_connector;

    phdmi_connector->pfunc = &hdmi_connector_func;
}

disp_void_t disp_hdmi_connector_sw_deinit(pdisp_void_t pdisp_connector)
{
    pdisp_connector_t phdmi_connector = (pdisp_connector_t)pdisp_connector;

    phdmi_connector->pfunc = DISP_NULL;
}
