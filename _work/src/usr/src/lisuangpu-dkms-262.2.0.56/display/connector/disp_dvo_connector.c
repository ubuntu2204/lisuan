/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#include "disp_dvo_connector.h"
#include "disp_callback.h"

#define disp_print(level, log, ...) disp_print_ex(DISP_PRINT_MASK_CONNECTOR_DVO, level, log, ##__VA_ARGS__)

static DISP_CONNECT_STATE dvo_connector_detect(pdisp_connector_t pconnector)
{
    if (!pconnector->state.bfake)
    {
        disp_print(DDM_ERROR, "dvo_connector_detect index[%d], unexpected: dvo should use fake edid!\n", pconnector->index);
        return DISP_DISCONNECTED;
    }

    return DISP_CONNECTTED;
}

static struct disp_connector_func dvo_connector_func =
{
    .detect = dvo_connector_detect,
};

disp_void_t disp_dvo_connector_sw_init(pdisp_connector_t pconnector)
{
    pconnector->pfunc = &dvo_connector_func;
}

disp_void_t disp_dvo_connector_sw_deinit(pdisp_connector_t pconnector)
{
    pconnector->pfunc = DISP_NULL;
}
