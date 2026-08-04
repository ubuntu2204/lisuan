/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#include "disp_connector.h"
#include "disp_dp_connector.h"
#include "disp_hdmi_connector.h"
#include "disp_dvo_connector.h"
#include "disp_callback.h"
#include "disp_edid.h"

#define DISP_DP_RETRY_EDID_TIMES 16
#define disp_print(level, log, ...) disp_print_ex(DISP_PRINT_MASK_CONNECTOR, level, log, ##__VA_ARGS__)
#define disp_print_addition_file(level, log, ...) \
{ \
    disp_print_ex(DISP_PRINT_MASK_CONNECTOR, level, log, ##__VA_ARGS__); \
    disp_print_to_file_ex(pdd, DISP_PRINT_MASK_CONNECTOR, level, log, ##__VA_ARGS__); \
}

DISP_STATUS disp_connector_sw_init(pdisp_connector_t pdisp_connector)
{
    if (DISP_CONNECTOR_DVO == pdisp_connector->type)
    {
        disp_dvo_connector_sw_init(pdisp_connector);
    }
    else if (DISP_CONNECTOR_DP == pdisp_connector->type)
    {
        disp_dp_connector_sw_init(pdisp_connector);
    }
    else
    {
        disp_hdmi_connector_sw_init(pdisp_connector);
    }

    return DISP_OK;
}

DISP_STATUS disp_connector_sw_deinit(pdisp_connector_t pdisp_connector)
{
    if(!pdisp_connector)
        return -DISP_EINVAL;

    pdisp_connector->edid_block_cnt = 0;
    if (pdisp_connector->mode_list.modes)
        disp_free(pdisp_connector->mode_list.modes);
    pdisp_connector->mode_list.modes = DISP_NULL;
    pdisp_connector->mode_list.num = 0;

    if (DISP_CONNECTOR_DVO == pdisp_connector->type)
    {
        disp_dvo_connector_sw_deinit(pdisp_connector);
    }
    if (DISP_CONNECTOR_DP == pdisp_connector->type)
    {
        disp_dp_connector_sw_deinit(pdisp_connector);
    }
    else
    {
        disp_hdmi_connector_sw_deinit(pdisp_connector);
    }

    return DISP_OK;
}

static void disp_edid_block_dump(disp_s32_t block_idx, disp_u8_t *pEdid, disp_s8_t *prefix, disp_u8_t level)
{
    disp_u32_t idx = 0;
    disp_print(level, "%s: block_idx[%d] edid:\n", prefix, block_idx);
    for (idx = 0; idx < EDID_LENGTH; idx += 16)
    {
        disp_print(level, "\t0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x"
            " 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",
            pEdid[idx], pEdid[idx + 1], pEdid[idx + 2], pEdid[idx + 3],
            pEdid[idx + 4], pEdid[idx + 5], pEdid[idx + 6], pEdid[idx + 7],
            pEdid[idx + 8], pEdid[idx + 9], pEdid[idx + 10], pEdid[idx + 11],
            pEdid[idx + 12], pEdid[idx + 13], pEdid[idx + 14], pEdid[idx + 15]);
    }
}

static disp_s32_t disp_loop_probe_edid_block(pdisp_connector_t pdisp_connector,
    disp_s32_t block_index, pdisp_u8_t pedid, disp_s32_t loop_cnt)
{
    disp_u32_t read_back;
    disp_s32_t block_cnt = 0;

    while (loop_cnt)
    {
        disp_memset(pedid, 0, EDID_LENGTH);
        read_back = pdisp_connector->pfunc->probe_edid_block(pdisp_connector, pedid, (disp_u8_t)block_index, EDID_LENGTH);
        disp_edid_block_dump(block_index, pedid, "first", DDM_TRACE);
        if(EDID_LENGTH == read_back && disp_edid_block_valid(pedid, block_index))
        {
            block_cnt = (0 == block_index) ? (1 + pedid[0x7e]) : 0;
            disp_print(DDM_TRACE, "edid block 0 success!\n");
            break;
        }
        else
        {
            disp_print(DDM_TRACE, "edid block %d read failed, read_back[%u]\n", block_index, read_back);
            loop_cnt--;
            block_cnt = -1;
        }
    }

    if (block_cnt < 0)
    {
        pdisp_connector->edid_origin_csum = disp_edid_block_checksum(pedid);
        disp_print(DDM_WARNING, "edid block %d read timeout!\n", block_index);
        return -1;
    }

    return block_cnt;
}

DISP_STATUS disp_connector_probe_edid(pdisp_connector_t pdisp_connector)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_connector->disp_device;
    disp_u8_t edid[EDID_LENGTH];
    disp_s32_t loop_ret = 0, block_cnt = 0, idx = 0, loop_cnt = 0;

    loop_cnt = pdd->flag.info.dp_cts ? 1 : DISP_DP_RETRY_EDID_TIMES;

    loop_ret = disp_loop_probe_edid_block(pdisp_connector, 0, edid, loop_cnt);
    if (loop_ret > 0)
    {
        block_cnt = loop_ret;
    }
    else
    {
        disp_print_addition_file(DDM_WARNING, "%u edid block 0 read failed!\n", pdisp_connector->index);
        return -DISP_EIO;
    }

    /* Clear EDID & copy first block */
    if (block_cnt > DISP_EDID_MAX_LENGTH / EDID_LENGTH)
    {
        disp_print(DDM_INFO, "detect edid more than 4 block, force 4\n");
        block_cnt = DISP_EDID_MAX_LENGTH / EDID_LENGTH;
    }
    pdisp_connector->edid_block_cnt = (disp_u8_t)block_cnt;
    disp_memset(pdisp_connector->pedid, 0, DISP_EDID_MAX_LENGTH);
    disp_memcpy(pdisp_connector->pedid, edid, EDID_LENGTH);

    for (idx = 1; idx < block_cnt; idx++)
    {
        loop_ret = disp_loop_probe_edid_block(pdisp_connector, idx, edid, loop_cnt);
        if (loop_ret < 0)
        {   /* Release EDID if failed in ext. */
            disp_print_addition_file(DDM_WARNING, "%u edid block %d read failed!\n", pdisp_connector->index, idx);
            pdisp_connector->edid_block_cnt = 0;
            return -DISP_EIO;
        }
        else
        {
            disp_memcpy(pdisp_connector->pedid + idx * EDID_LENGTH, edid, EDID_LENGTH);
            disp_print(DDM_TRACE, "edid block %u success!\n", idx);
        }
    }

    pdisp_connector->edid_origin_csum = edid[EDID_LENGTH - 1];
    for (idx = 0; idx < block_cnt; idx++)
    {
        disp_edid_block_dump(idx, pdisp_connector->pedid + idx * EDID_LENGTH, "final", DDM_TRACE);
    }

    disp_print(DDM_TRACE, "edid success!\n");
    return DISP_OK;
}

DISP_STATUS disp_connector_get_i2c_status(pdisp_connector_t pdisp_connector, pdisp_get_i2c_status_t pdisp_get_i2c_status)
{
    if(pdisp_connector->pfunc->get_i2c_status)
    {
        return pdisp_connector->pfunc->get_i2c_status(pdisp_connector, pdisp_get_i2c_status);
    }
    else
    {
        return -DISP_ENODEV;
    }
}
