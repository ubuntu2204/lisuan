/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#include "display.h"
#include "disp_callback.h"
#include "disp_device.h"
#include "disp_edid.h"
#include "disp_crtc.h"
#include "disp_encoder.h"
#include "disp_mode.h"
#include "disp_intr.h"
#include "disp_dp_aux.h"
#include "disp_dp_connector.h"
#include "disp_dhi.h"
#include "disp_vbt.h"

#define disp_print(level, log, ...) disp_print_ex(DISP_PRINT_MASK_COMMON, level, log, ##__VA_ARGS__)
#define disp_print_addition_file(level, log, ...) \
{ \
    disp_print_ex(DISP_PRINT_MASK_COMMON, level, log, ##__VA_ARGS__); \
    disp_print_to_file_ex(pdd, DISP_PRINT_MASK_COMMON, level, log, ##__VA_ARGS__); \
}
#define disp_print_pure_addition_file(level, log, ...) \
{ \
    disp_print_pure_ex(DISP_PRINT_MASK_COMMON, level, log, ##__VA_ARGS__); \
    disp_print_pure_to_file_ex(pdd, DISP_PRINT_MASK_COMMON, level, log, ##__VA_ARGS__); \
}


DISP_DLLEXPORTS DISP_STATUS display_device_hw_init(pdisp_void_t pdisplay_device)
{
    pdisp_device_t pdd = pdisplay_device;
    DISP_STATUS ret = disp_device_hw_init(pdd);
    if(ret)
    {
        disp_print(DDM_ERROR, "display device hw init fail\n");
    }

    return ret;
}

DISP_DLLEXPORTS DISP_STATUS display_device_init(pdisp_device_init_t pdisp_device_init)
{
    DISP_STATUS ret = -DISP_EPRIV_UNKNOWN;
    pdisp_device_t dd = DISP_NULL;

    if((ret = disp_set_callback(pdisp_device_init->pdisp_callback)) != DISP_OK)
    {
        return ret;
    }

    dd = (pdisp_device_t)disp_malloc(sizeof(disp_device_t));
    if(!dd)
    {
        disp_print(DDM_ERROR, "display_device_init, dd alloc failed\n");
        return -DISP_ENOMEM;
    }
    disp_memset(dd, 0, sizeof(disp_device_t));

    dd->disp_adapter = pdisp_device_init->pdisplay_adapter;
    dd->device_id = pdisp_device_init->device_id;
    dd->flag.info.pz1_config = pdisp_device_init->flag.bits.pz1_config;
    dd->flag.info.pz1_capture_pipe = pdisp_device_init->flag.bits.pz1_capture_pipe;
    dd->flag.info.dvo_enable = pdisp_device_init->flag.bits.dvo_enable;
    dd->flag.info.prefer_free_sync = pdisp_device_init->flag.bits.prefer_free_sync;
    dd->flag.info.dp_cts = pdisp_device_init->flag.bits.dp_cts;
    dd->flag.info.mode_8k = pdisp_device_init->flag.bits.mode_8k;
    dd->flag.info.hau_sw_csb = pdisp_device_init->flag.bits.hau_sw_csb;
    dd->flag.info.hau_sw_info = pdisp_device_init->flag.bits.hau_sw_info;
    dd->flag.info.hau_asp_8ch = pdisp_device_init->flag.bits.hau_asp_8ch;
    pdisp_device_init->pdisplay_device = (pdisp_void_t)dd;

    //disp_check_rom_header(dd);
    ret = disp_device_sw_init(dd);
    if(ret)
    {
        disp_print(DDM_ERROR, "display device sw init fail\n");
        return ret;
    }

    ret = disp_device_hw_init(dd);
    if(ret)
    {
        disp_print(DDM_ERROR, "display device hw init fail\n");
        return ret;
    }

    disp_print(DDM_INFO, "display device init successfully, pz1_capture_pipe[%u]\n", dd->flag.info.pz1_capture_pipe);

    return ret;
}

DISP_DLLEXPORTS DISP_STATUS display_device_deinit(pdisp_void_t pdisp_device)
{
    DISP_STATUS ret = -DISP_EPRIV_UNKNOWN;
    pdisp_device_t pdd = (pdisp_device_t)pdisp_device;

    ret = disp_device_hw_deinit(pdd);
    
    ret = disp_device_sw_deinit(pdd);

    if(pdd)
    {
        disp_free(pdd);
    }

    /* Do not clear callback as multi-adapter NULL cb use issue.
     * It may be better that move cb to display_device_t in the future.
     */
    //disp_clear_callback();

    return ret;
}

DISP_DLLEXPORTS DISP_STATUS display_query_resource(pdisp_void_t pdisp_device, pdisp_query_resource_t pdisp_query_resource)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_device;

    if((!pdd) || (!pdisp_query_resource))
        return -DISP_EINVAL;

    if(pdd->device_id == DISP_DEVICE_G100)
    {
        pdisp_query_resource->crtc_num = DISP_CRTC_NUM;
        pdisp_query_resource->plane_num = DISP_OVERLAY_PLANE_NUM + 2;//overlay plane + primary + cursor
        pdisp_query_resource->pu_support = DISP_TRUE;

        pdisp_query_resource->connector_support = disp_device_get_support_connector(pdd);

        return DISP_OK;
    }
    else
    {
        disp_print(DDM_WARNING, "unknown device id\n");
        return -DISP_EPRIV_DEVICE_ID;
    }
}

static disp_u32_t disp_last_bit_index(disp_u32_t value)
{
    disp_u32_t temp = 0x1;
    disp_u32_t i;

    for(i = 0; i < 32; i++)
    {
        if(value & temp )
        {
            break;
        }
        temp <<= 1; 
    }

    return i;
}

disp_u8_t fake_edid_hdr[384] =
{
    0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x05, 0xe3, 0x01, 0xb2, 0x40, 0x01, 0x00, 0x00,
    0x17, 0x20, 0x01, 0x04, 0xb5, 0x3e, 0x22, 0x78, 0x3f, 0xa4, 0x15, 0xad, 0x50, 0x45, 0xa2, 0x26,
    0x0e, 0x50, 0x54, 0xbf, 0xef, 0x00, 0xd1, 0xc0, 0x81, 0x80, 0x31, 0x68, 0x31, 0x7c, 0x45, 0x68,
    0x45, 0x7c, 0x61, 0x68, 0x61, 0x7c, 0x4d, 0xd0, 0x00, 0xa0, 0xf0, 0x70, 0x3e, 0x80, 0x30, 0x40,
    0x35, 0x00, 0x6d, 0x55, 0x21, 0x00, 0x00, 0x1a, 0x00, 0x00, 0x00, 0xff, 0x00, 0x53, 0x41, 0x47,
    0x4e, 0x36, 0x4a, 0x41, 0x30, 0x30, 0x30, 0x33, 0x32, 0x30, 0x00, 0x00, 0x00, 0xfc, 0x00, 0x55,
    0x32, 0x38, 0x47, 0x32, 0x58, 0x5f, 0x44, 0x0a, 0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0xfd,
    0x00, 0x30, 0x90, 0xff, 0xff, 0x8c, 0x01, 0x0a, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x02, 0xbb,

    0x02, 0x03, 0x32, 0xf1, 0x4c, 0x01, 0x03, 0x05, 0x14, 0x04, 0x13, 0x1f, 0x12, 0x02, 0x11, 0x90,
    0x3f, 0x23, 0x09, 0x07, 0x07, 0x83, 0x01, 0x00, 0x00, 0xe3, 0x05, 0xe3, 0x01, 0xe6, 0x06, 0x07,
    0x01, 0x62, 0x62, 0x00, 0x6d, 0x1a, 0x00, 0x00, 0x02, 0x01, 0x30, 0x90, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x40, 0xe7, 0x00, 0x6a, 0xa0, 0xa0, 0x67, 0x50, 0x08, 0x20, 0x98, 0x04, 0x6d, 0x55,
    0x21, 0x00, 0x00, 0x1a, 0x6f, 0xc2, 0x00, 0xa0, 0xa0, 0xa0, 0x55, 0x50, 0x30, 0x20, 0x35, 0x00,
    0x6d, 0x55, 0x21, 0x00, 0x00, 0x1e, 0x4d, 0x6c, 0x80, 0xa0, 0x70, 0x70, 0x3e, 0x80, 0x30, 0x20,
    0x3a, 0x00, 0x6d, 0x55, 0x21, 0x00, 0x00, 0x1a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x36,

    0x70, 0x12, 0x79, 0x00, 0x00, 0x03, 0x01, 0x64, 0xf2, 0x07, 0x02, 0x04, 0xff, 0x0e, 0x3f, 0x01,
    0x5f, 0x80, 0x7f, 0x00, 0x6f, 0x08, 0x3d, 0x00, 0x02, 0x00, 0x04, 0x00, 0xa4, 0xa0, 0x01, 0x84,
    0xff, 0x0e, 0x9f, 0x00, 0x2f, 0x80, 0x3f, 0x00, 0x6f, 0x08, 0x3d, 0x00, 0x02, 0x00, 0x04, 0x00,
    0x30, 0x5b, 0x01, 0x04, 0xff, 0x0e, 0x9f, 0x00, 0x2f, 0x80, 0x3f, 0x00, 0x6f, 0x08, 0x3d, 0x00,
    0x02, 0x00, 0x04, 0x00, 0x1b, 0x27, 0x01, 0x04, 0xff, 0x0e, 0x9f, 0x00, 0x2f, 0x80, 0x3f, 0x00,
    0x6f, 0x08, 0x3d, 0x00, 0x02, 0x00, 0x04, 0x00, 0x64, 0x04, 0x01, 0x04, 0xff, 0x0e, 0x9f, 0x00,
    0x2f, 0x80, 0x3f, 0x00, 0x6f, 0x08, 0x3d, 0x00, 0x02, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4c, 0x90,
};

disp_u8_t safe_edid[128] = {
    0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0x04, 0xa5, 0x3e, 0x22, 0x78, 0x00, 0xa4, 0x15, 0xad, 0x50, 0x45, 0xa2, 0x26,
    0x0e, 0x50, 0x54, 0x21, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xee,
};

DISP_DLLEXPORTS DISP_STATUS display_detect_connector(pdisp_void_t pdisp_device, pdisp_detect_connector_t pdisp_detect_connector)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_device;
    disp_u8_t fake_connector = 0, to_detect = 0, support = 0, temp = 0, detected = 0;
    pdisp_connector_t pdisp_connector = DISP_NULL;
    disp_u8_t connector_index = 0;
    struct disp_connector_func* pfunc = DISP_NULL;
    disp_u32_t mode_num = 0;
    disp_u8_t edid_cnt = 0;

    if((!pdd) || (!pdisp_detect_connector))
        return -DISP_EINVAL;
    
    support = disp_device_get_support_connector(pdd);
    fake_connector = support & pdisp_detect_connector->connector_to_fake;
    to_detect = support & pdisp_detect_connector->connector_to_detect & (~fake_connector);

    while(fake_connector)
    {
        temp = GET_LAST_BIT(fake_connector);

        fake_connector &= (~(temp));

        if((connector_index = (disp_u8_t)disp_last_bit_index(temp)) < DISP_CONNECTOR_NUM)
        {
            pdisp_connector = &pdd->disp_connector[connector_index];
            pdisp_connector->state.bfake = DISP_TRUE;

            // fake dp use detect to set phy
            pfunc = pdisp_connector->pfunc;
            if(pfunc && pfunc->detect)
            {
                pdisp_connector->state.connected = pfunc->detect(pdisp_connector);
                if(pdisp_connector->state.connected != DISP_CONNECTTED)
                {
                    disp_print(DDM_WARNING, "display_detect_connector, fake detect failed, index[%u], type[%d], connected[%d]\n",
                        connector_index, pdisp_connector->type, pdisp_connector->state.connected);
                }
            }
            else
            {
                disp_print(DDM_WARNING, "display_detect_connector, fake detect no func! index[%d] type[%d]\n", pdisp_connector->index, pdisp_connector->type);
            }

            edid_cnt = disp_edid_ext_count((pedid_t)fake_edid_hdr) + 1;
            if (edid_cnt > DISP_EDID_MAX_LENGTH / EDID_LENGTH)
            {
                disp_print(DDM_INFO, "fake edid more than 4 block, force 4\n");
                edid_cnt = DISP_EDID_MAX_LENGTH / EDID_LENGTH;
            }
            disp_memset(pdisp_connector->pedid, 0, DISP_EDID_MAX_LENGTH);
            disp_memcpy(pdisp_connector->pedid, fake_edid_hdr, EDID_LENGTH * edid_cnt);
            pdisp_connector->edid_block_cnt = edid_cnt;

            detected |= temp;
            if(pdisp_connector->mode_list.modes)
            {
                disp_free(pdisp_connector->mode_list.modes);
                pdisp_connector->mode_list.num = 0;
                pdisp_connector->mode_list.modes = DISP_NULL;
                pdisp_connector->native_mode = DISP_NULL;
                pdisp_connector->max_clk_khz = 0;
            }

            if (disp_add_edid_info(pdisp_connector, (pedid_t)pdisp_connector->pedid) < 0)
            {
                disp_print(DDM_WARNING, "display_detect_connector, fake invalid edid, conn[%u]\n", connector_index);
                return -DISP_EINVAL;
            }
            mode_num = disp_add_edid_modes(pdisp_connector, (pedid_t)pdisp_connector->pedid, DISP_FALSE);
            pdisp_connector->mode_list.modes = disp_malloc(mode_num * sizeof(disp_display_mode_t));
            if(!pdisp_connector->mode_list.modes)
            {
                disp_print(DDM_ERROR, "display_detect_connector, fake alloc mode list failed, conn[%u]\n", connector_index);
                return -DISP_ENOMEM;
            }
            disp_add_edid_modes(pdisp_connector, (pedid_t)pdisp_connector->pedid, DISP_TRUE);
            disp_add_edid_modes_extension(pdisp_connector, (pedid_t)pdisp_connector->pedid);
            pdd->disp_encoder[pdisp_connector->index].policy.flag.bits.hdr_metadata =
                (pdisp_connector->disp_info.hdr_sink_metadata.data.hdmi_type1.eotf &
                BIT(DISP_EOTF_SMPTE_ST2084)) ? 1 : 0;
        }
        else
        {
            disp_print(DDM_WARNING, "wrong connector index by fake connector\n");
        }
    }

    temp = 0;
    while(to_detect)
    {
        temp = GET_LAST_BIT(to_detect);

        to_detect &= (~(temp));

        if((connector_index = (disp_u8_t)disp_last_bit_index(temp)) < DISP_CONNECTOR_NUM)
        {
            pdisp_connector = &pdd->disp_connector[connector_index];
            pdisp_connector->state.bfake = DISP_FALSE;
            pfunc = pdisp_connector->pfunc;

            if(pfunc->detect)
            {
                pdisp_connector->state.connected = pfunc->detect(pdisp_connector);
                if(pdisp_connector->state.connected  == DISP_CONNECTTED)
                {
                    detected |= temp;

                    if(pdisp_connector->flag.info.use_safe_mode || pdisp_connector->flag.info.cheater)
                    {
                        disp_print(DDM_KEY, "detect %u %s\n", pdisp_connector->index,
                            pdisp_connector->flag.info.cheater ? "cheater" : "safe mode");
                        pdisp_connector->edid_block_cnt = 1;
                        disp_memcpy(pdisp_connector->pedid, safe_edid, sizeof(safe_edid));
                    }

                    if(pdisp_connector->mode_list.modes)
                    {
                        disp_free(pdisp_connector->mode_list.modes);
                        pdisp_connector->mode_list.num = 0;
                        pdisp_connector->mode_list.modes = DISP_NULL;
                        pdisp_connector->native_mode = DISP_NULL;
                        pdisp_connector->max_clk_khz = 0;
                    }

                    if (disp_add_edid_info(pdisp_connector, (pedid_t)pdisp_connector->pedid) < 0)
                    {
                        disp_print(DDM_WARNING, "display_detect_connector, detect invalid edid, conn[%u]\n", connector_index);
                        return -DISP_EINVAL;
                    }
                    mode_num = disp_add_edid_modes(pdisp_connector, (pedid_t)pdisp_connector->pedid, DISP_FALSE);
                    pdisp_connector->mode_list.modes = disp_malloc(mode_num * sizeof(disp_display_mode_t));
                    if(!pdisp_connector->mode_list.modes)
                    {
                        disp_print(DDM_ERROR, "display_detect_connector, detect alloc mode list failed, conn[%u]\n", connector_index);
                        return -DISP_ENOMEM;
                    }
                    disp_add_edid_modes(pdisp_connector, (pedid_t)pdisp_connector->pedid, DISP_TRUE);
                    if (!pdisp_connector->flag.info.use_safe_mode) // No need addition modes when safe edid.
                        disp_add_edid_modes_extension(pdisp_connector, (pedid_t)pdisp_connector->pedid);
                    pdd->disp_encoder[pdisp_connector->index].policy.flag.bits.hdr_metadata =
                        (pdisp_connector->disp_info.hdr_sink_metadata.data.hdmi_type1.eotf &
                        BIT(DISP_EOTF_SMPTE_ST2084)) ? 1 : 0;
                    if(pdd->flag.info.dp_cts)
                        disp_dp_send_real_edid_checksum(pdisp_connector->ddc, pdisp_connector->edid_origin_csum);
                }
            }
            else
            {
                disp_print(DDM_WARNING, "connector %d have not detect func\n", connector_index);
            }
        }
        else
        {
            disp_print(DDM_WARNING, "wrong connector index by detect connector\n");
        }
    }

    pdisp_detect_connector->connector_detected = detected;

    return DISP_OK;
}

DISP_DLLEXPORTS DISP_STATUS display_get_modelist(pdisp_void_t pdisp_device, pdisp_mode_list_t pdisp_mode_list)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_device;
    pdisp_connector_t pdisp_connector = DISP_NULL;
    disp_u8_t connector_index = 0;

    if(!pdd || !pdisp_mode_list)
    {
        return -DISP_EINVAL;
    }

    if((connector_index = pdisp_mode_list->connector_index) >= DISP_CONNECTOR_NUM)
    {
        return -DISP_EINVAL;
    }

    pdisp_connector = &pdd->disp_connector[connector_index];

    pdisp_mode_list->num = pdisp_connector->mode_list.num; 
    
    if(pdisp_mode_list->modes)
        disp_memcpy(pdisp_mode_list->modes, pdisp_connector->mode_list.modes, 
                     pdisp_mode_list->num * sizeof(disp_display_mode_t));

    if(pdisp_mode_list->num != 0)
        return DISP_OK;
    else
        return -DISP_EPRIV_INEDID;
}

DISP_DLLEXPORTS DISP_STATUS display_get_edid(pdisp_void_t pdisp_device, pdisp_get_edid_t pdisp_get_edid)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_device;
    pdisp_connector_t pdisp_connector = DISP_NULL;
    disp_u8_t connector_index = 0;

    if(!pdd || !pdisp_get_edid)
    {
        return -DISP_EINVAL;
    }

    if((connector_index = pdisp_get_edid->connector_index) >= DISP_CONNECTOR_NUM)
    {
        return -DISP_EINVAL;
    }
    
    pdisp_connector = &pdd->disp_connector[connector_index];
    if(0 == pdisp_connector->edid_block_cnt)
    {
        disp_print(DDM_WARNING, "display_get_edid, edid not ready, edid_block_cnt[%u]\n", pdisp_connector->edid_block_cnt);
        return -DISP_ENREADY;
    }

    pdisp_get_edid->edid_size = pdisp_connector->edid_block_cnt * EDID_LENGTH;
    if(pdisp_get_edid->buf)
        disp_memcpy(pdisp_get_edid->buf, pdisp_connector->pedid, DISP_MIN(pdisp_get_edid->buf_size, EDID_LENGTH * (disp_u32_t)pdisp_connector->edid_block_cnt));

    return DISP_OK;
}

DISP_DLLEXPORTS DISP_STATUS display_crtc_dpms(pdisp_void_t pdisp_device, disp_crtc_dpms_t crtc_dpms)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_device;
    disp_u8_t crtc_index = crtc_dpms.crtc_index;
    pdisp_crtc_t pdisp_crtc = DISP_NULL;

    disp_print(DDM_INFO, "crtc %d dpms %d\n", crtc_index, crtc_dpms.enable);

    if(crtc_index > DISP_CRTC_NUM)
        return -DISP_EINVAL;

    pdisp_crtc = &pdd->disp_crtc[crtc_index];

    return disp_crtc_dpms(pdisp_crtc, &crtc_dpms);
}

DISP_DLLEXPORTS DISP_STATUS display_crtc_set_mode(pdisp_void_t pdisp_device, disp_crtc_set_mode_t crtc_set_mode)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_device;
    disp_u8_t crtc_index = crtc_set_mode.index;
    pdisp_crtc_t pdisp_crtc = DISP_NULL;

    disp_print_addition_file(DDM_KEY, "crtc %d set mode\n", crtc_index);
    disp_print_pure_addition_file(DDM_KEY, "\tsrc mode: " DISP_MODE_FMT "\n", DISP_MODE_ARG(&crtc_set_mode.mode));
    disp_print_pure_addition_file(DDM_KEY, "\thw  mode: " DISP_MODE_FMT "\n", DISP_MODE_ARG(&crtc_set_mode.hw_mode));
    disp_print_pure_addition_file(DDM_KEY, "\tproperty: output_signal[%u] bpc[%u] colorimetry[%d]\n",
        crtc_set_mode.output_signal, crtc_set_mode.bpc, crtc_set_mode.colorimetry);

    if(crtc_index > DISP_CRTC_NUM)
        return -DISP_EINVAL;

    pdisp_crtc = &pdd->disp_crtc[crtc_index];

    return disp_crtc_set_mode(pdisp_crtc, crtc_set_mode);
}

DISP_DLLEXPORTS DISP_STATUS display_encoder_dpms(pdisp_void_t pdisp_device, disp_encoder_dpms_t encoder_dpms)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_device;
    pdisp_encoder_t pdisp_encoder = DISP_NULL;

    if(!pdd)
    {
        disp_print(DDM_WARNING, "pdisp_device is NULL\n");
        return -DISP_EINVAL;
    }

    disp_print(DDM_TRACE, "encoder %d dpms %d\n", encoder_dpms.index, encoder_dpms.enable);

    if(encoder_dpms.index >= DISP_ENCODER_NUM)
    {
        disp_print(DDM_WARNING, "dpms encoder[%u] index out of range\n", encoder_dpms.index);
        return -DISP_EINVAL;
    }

    pdisp_encoder = &pdd->disp_encoder[encoder_dpms.index];

    return disp_encoder_dpms(pdisp_encoder, encoder_dpms.enable);
}

DISP_DLLEXPORTS DISP_STATUS display_encoder_set_mode(pdisp_void_t pdisp_device, disp_encoder_set_mode_t encoder_set_mode)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_device;
    pdisp_encoder_t pdisp_encoder = DISP_NULL;

    if(!pdd)
    {
        disp_print(DDM_WARNING, "pdisp_device is NULL\n");
        return -DISP_EINVAL;
    }

    disp_print(DDM_TRACE, "encoder %d set mode\n", encoder_set_mode.index);

    if(encoder_set_mode.index >= DISP_ENCODER_NUM)
    {
        disp_print(DDM_WARNING, "encoder index out of range\n");
        return -DISP_EINVAL;
    }

    pdisp_encoder = &pdd->disp_encoder[encoder_set_mode.index];

    return disp_encoder_set_mode(pdisp_encoder);
}

DISP_DLLEXPORTS DISP_STATUS display_plane_check(pdisp_void_t pdisp_device, pdisp_plane_update_para_t pupdate_para)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_device;
    pdisp_crtc_t pdisp_crtc = DISP_NULL;

    if(!pdd || !pupdate_para)
    {
        return -DISP_EINVAL;
    }

    if(pupdate_para->crtc_index > DISP_CRTC_NUM)
        return -DISP_EINVAL;

    pdisp_crtc = &pdd->disp_crtc[pupdate_para->crtc_index];

    return disp_crtc_plane_check(pdisp_crtc, pupdate_para);
}

DISP_DLLEXPORTS DISP_STATUS display_plane_update(pdisp_void_t pdisp_device, pdisp_plane_update_para_t pupdate_para)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_device;
    pdisp_crtc_t pdisp_crtc = DISP_NULL;

    disp_print(DDM_TRACE, "display_plane_update, crtc[%d], plane[%d], format[%d], bpp[%u]\n",
        pupdate_para->crtc_index, pupdate_para->plane_index, pupdate_para->fb.format, pupdate_para->fb.bit_cnt);

    if(!pdd || !pupdate_para)
    {
        return -DISP_EINVAL;
    }

    if(pupdate_para->crtc_index > DISP_CRTC_NUM) {
        return -DISP_EINVAL;
    }

    pdisp_crtc = &pdd->disp_crtc[pupdate_para->crtc_index];
    
    return disp_crtc_plane_update(pdisp_crtc, pupdate_para);
}

DISP_DLLEXPORTS DISP_STATUS display_plane_get_addr(pdisp_void_t pdisp_device, pdisp_get_fb_addr_t paddr)
{
    if(!pdisp_device || !paddr)
    {
        return -DISP_EINVAL;
    }
    if(paddr->crtc_index > DISP_CRTC_NUM)
    {
        return -DISP_EINVAL;
    }
    if(paddr->plane_index == DISP_CURSOR_PLANE) 
    {
        disp_print(DDM_INFO, "display_plane_get_addr, no need to get cursor addr, plane_index[%d]\n", paddr->plane_index);
        return -DISP_EINVAL;
    }

    return disp_plane_get_addr(pdisp_device, paddr);
}

DISP_DLLEXPORTS DISP_STATUS display_intr_enable(pdisp_void_t pdisp_device, disp_u8_t enable, disp_intr_t intr)
{
    if(!pdisp_device)
        return -DISP_EINVAL;

    disp_intr_enable(pdisp_device, enable, intr);

    return DISP_OK;
}

DISP_DLLEXPORTS DISP_STATUS display_intr_get_status(pdisp_void_t pdisp_device, pdisp_intr_t pintr)
{
    if(!pdisp_device || !pintr)
        return -DISP_EINVAL;

    pintr->intr = disp_intr_get_status(pdisp_device);

    return DISP_OK;
}

DISP_DLLEXPORTS DISP_STATUS display_intr_multi_vsync_enable(pdisp_void_t pdisp_device, disp_u8_t enable)
{
    if(!pdisp_device)
        return -DISP_EINVAL;

    disp_intr_multi_sync_enable(pdisp_device, enable);

    return DISP_OK;
}

DISP_DLLEXPORTS DISP_STATUS display_intr_clear_vsync_status(pdisp_void_t pdisp_device, disp_s32_t crtc_index)
{
    if(!pdisp_device)
	    return -DISP_EINVAL;

    disp_intr_clear_vsync_status(pdisp_device, crtc_index);

    return DISP_OK;
}

DISP_DLLEXPORTS DISP_STATUS display_get_itg_counter(pdisp_void_t pdisp_device, pdisp_itg_counter_t pitg_cnt)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_device;
    pdisp_crtc_t pdisp_crtc = DISP_NULL;

    if(!pdd || !pitg_cnt)
        return -DISP_EINVAL;

    if(pitg_cnt->crtc > DISP_CRTC_NUM)
        return -DISP_EINVAL;

    pdisp_crtc = &pdd->disp_crtc[pitg_cnt->crtc];

    return disp_pipe_get_itg_counter(&pdisp_crtc->pipe, pitg_cnt);
}

DISP_DLLEXPORTS DISP_STATUS display_set_signature(pdisp_void_t pdisp_device, disp_sig_set_t sig_set)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_device;
    pdisp_crtc_t pdisp_crtc = DISP_NULL;
    pdisp_pipe_t pdisp_pipe = DISP_NULL;

    if(!pdd)
        return -DISP_EINVAL;

    if(sig_set.crtc_index > DISP_CRTC_NUM)
        return -DISP_EINVAL;

    pdisp_crtc = &pdd->disp_crtc[sig_set.crtc_index];
    pdisp_pipe = &pdisp_crtc->pipe;

    return disp_pipe_set_sig(pdisp_pipe, &sig_set);
}

DISP_DLLEXPORTS DISP_STATUS display_get_signature(pdisp_void_t pdisp_device, pdisp_sig_get_t psig_get)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_device;
    pdisp_crtc_t pdisp_crtc = DISP_NULL;

    if(!pdd || !psig_get)
        return -DISP_EINVAL;

    if(psig_get->crtc_index > DISP_CRTC_NUM)
        return -DISP_EINVAL;

    pdisp_crtc = &pdd->disp_crtc[psig_get->crtc_index];

    return disp_pipe_get_sig(&pdisp_crtc->pipe, psig_get);
}

DISP_DLLEXPORTS DISP_STATUS display_get_sgnt_golden(pdisp_void_t pdisp_device, pdisp_sig_get_t psig_get)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_device;
    pdisp_crtc_t pdisp_crtc = DISP_NULL;

    if(!pdd || !psig_get)
        return -DISP_EINVAL;

    if(psig_get->crtc_index > DISP_CRTC_NUM)
        return -DISP_EINVAL;

    pdisp_crtc = &pdd->disp_crtc[psig_get->crtc_index];
    return disp_pipe_get_sig_golden(&pdisp_crtc->pipe, psig_get);
}

DISP_DLLEXPORTS DISP_STATUS display_set_gamma(pdisp_void_t pdisp_device, pdisp_gamma_info_t pgamma_info)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_device;
    pdisp_crtc_t pdisp_crtc = DISP_NULL;

    if(!pdd || !pgamma_info)
        return -DISP_EINVAL;

    if(pgamma_info->crtc_index > DISP_CRTC_NUM)
        return -DISP_EINVAL;

    pdisp_crtc = &pdd->disp_crtc[pgamma_info->crtc_index];

    disp_print(DDM_TRACE, "display_set_gamma: crtc_index=%u, gamma_bit=%u, lut_length=%u\n",
        pgamma_info->crtc_index, pgamma_info->gamma_bit, pgamma_info->lut_length);

    disp_pipe_set_lut(&pdisp_crtc->pipe, pgamma_info);

    return DISP_OK;
}

DISP_DLLEXPORTS DISP_STATUS display_set_csc(pdisp_void_t pdisp_device, pdisp_csc_para_t pcsc_para)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_device;

    pdisp_crtc_t pdisp_crtc = DISP_NULL;

    if(!pdd || !pcsc_para)
        return -DISP_EINVAL;

    if(pcsc_para->crtc_index > DISP_CRTC_NUM)
        return -DISP_EINVAL;

    pdisp_crtc = &pdd->disp_crtc[pcsc_para->crtc_index];

    disp_pipe_set_csc(&pdisp_crtc->pipe, pcsc_para);

    return DISP_OK;
}

DISP_DLLEXPORTS DISP_STATUS display_aux_transfer(pdisp_void_t pdisp_device, pdisp_aux_para_t paux_para)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_device;
    pdisp_connector_t pdisp_connector = DISP_NULL;

    if(!pdd || !paux_para)
        return -DISP_EINVAL;

    if(paux_para->connector_index > DISP_CONNECTOR_NUM)
        return -DISP_EINVAL;

    pdisp_connector =  &pdd->disp_connector[paux_para->connector_index];
    if(pdisp_connector->type != DISP_CONNECTOR_DP)//remain eDP
        return -DISP_EINVAL;

    disp_dp_dpcd_transfer(pdisp_connector, paux_para);

    return DISP_OK;
}

DISP_DLLEXPORTS DISP_STATUS display_get_connector_info(pdisp_void_t pdisp_device, pdisp_connector_info_t pconnector_info)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_device;
    pdisp_connector_t pconnector = DISP_NULL;
    disp_u8_t connector_index = 0;

    if (!pdd || !pconnector_info)
        return -DISP_EINVAL;

    if ((connector_index = pconnector_info->connector_index) > DISP_CRTC_NUM)
        return -DISP_EINVAL;

    pconnector = &pdd->disp_connector[connector_index];
    pconnector_info->bpc = pconnector->disp_info.bpc;
    pconnector_info->color_formats = pconnector->disp_info.color_formats;
    pconnector_info->colorimetry_support = pconnector->disp_info.colorimetry_support;
    disp_memcpy(&pconnector_info->rgbw_position, pconnector->disp_info.rgbw_position, 4 * sizeof(disp_u16xy_t));
    disp_memcpy(&pconnector_info->hdr_static_metadata, &pconnector->disp_info.hdr_sink_metadata.data.hdmi_type1, sizeof(hdr_static_metadata_t));
    pconnector_info->free_sync_support = (pconnector->disp_info.monitor_range.min_vfreq
        && pconnector->disp_info.monitor_range.max_vfreq
        && pdd->disp_encoder[pconnector->index].policy.flag.bits.free_sync_sink_support)
        ? DISP_TRUE : DISP_FALSE;
    pconnector_info->min_vfreq = pconnector->disp_info.monitor_range.min_vfreq;
    pconnector_info->max_vfreq = pconnector->disp_info.monitor_range.max_vfreq;

    /* add new member according to driver */

    return DISP_OK;
}

DISP_DLLEXPORTS DISP_STATUS display_codec_dpms(pdisp_void_t pdisp_device, pdisp_codec_para_t hdac_para)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_device;

    if(!pdd || !hdac_para)
        return -DISP_EINVAL;

    if(hdac_para->encoder_index > DISP_ENCODER_NUM)
        return -DISP_EINVAL;

    disp_codec_dpms(pdisp_device, hdac_para);

    return DISP_OK;
}

DISP_DLLEXPORTS DISP_STATUS display_set_cts(pdisp_void_t pdisp_device, pdisp_cts_para_t pcts_para)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_device;
    pdisp_encoder_t pdisp_encoder = DISP_NULL;
    struct disp_dp_cts_data* pdp_cts = DISP_NULL;

    if(!pdd || !pcts_para)
        return -DISP_EINVAL;

    if(pcts_para->connector_index > DISP_CONNECTOR_NUM)
        return -DISP_EINVAL;

    pdisp_encoder = &pdd->disp_encoder[pcts_para->connector_index];
    switch(pcts_para->type)
    {
        case DISP_CTS_TYPE_DP:
            pdp_cts = &pdisp_encoder->policy.custom.dp.cts_data;
            pdp_cts->valid.value = pcts_para->para.dp.valid_bits;
            pdp_cts->link_rate = pcts_para->para.dp.link_rate;
            pdp_cts->lane_count = pcts_para->para.dp.lane_count;
            pdp_cts->tps = pcts_para->para.dp.tps;
            pdp_cts->async_clock = pcts_para->para.dp.async_clock;
            pdp_cts->range_type = pcts_para->para.dp.range_type;
            pdp_cts->ycbcr_coefficients = pcts_para->para.dp.ycbcr_coefficients;
            pdp_cts->enhanced_frame_mode = pcts_para->para.dp.enhanced_frame;
            pdisp_encoder->policy.custom.dp.flag.bits.cts_valid = pdp_cts->valid.value ? 1 : 0;
            disp_print(DDM_TRACE, "display_set_cts, valid[%#x] link/lane[%u, %u], tps/async/rt/coef/enh[%u, %u, %u, %u, %u]\n",
                pdp_cts->valid.value, pdp_cts->link_rate, pdp_cts->lane_count, pdp_cts->tps, pdp_cts->async_clock,
                pdp_cts->range_type, pdp_cts->ycbcr_coefficients, pdp_cts->enhanced_frame_mode);
            break;
        case DISP_CTS_TYPE_HDMI:
        default:
            disp_print(DDM_INFO, "display_set_cts, invalid type[%d]\n", pcts_para->type);
            break;
    }

    return DISP_OK;
}

DISP_DLLEXPORTS DISP_STATUS display_get_hpd_state(pdisp_void_t pdisp_device, pdisp_get_hpd_state_t phpd_state)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_device;
    pdisp_connector_t pconnector = DISP_NULL;

    if (!pdd || !phpd_state)
        return -DISP_EINVAL;

    if(phpd_state->connector_index >= DISP_CRTC_NUM)
    {
        disp_print(DDM_WARNING, "display_get_hpd_state, invalid connector_index[%#x]\n", phpd_state->connector_index);
        return -DISP_EINVAL;
    }

    pconnector = &pdd->disp_connector[phpd_state->connector_index];
    if(pconnector->pfunc && pconnector->pfunc->get_hpd_status)
        phpd_state->hpd_state = pconnector->pfunc->get_hpd_status(pconnector);
    else
        disp_print(DDM_WARNING, "display_get_hpd_state, func get_hpd_status NULL\n");

    return DISP_OK;
}

DISP_DLLEXPORTS DISP_STATUS display_get_i2c_status(pdisp_void_t pdisp_device, pdisp_get_i2c_status_t pdisp_get_i2c_status)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_device;
    pdisp_connector_t pconnector = DISP_NULL;

    if (!pdd || !pdisp_get_i2c_status)
        return -DISP_EINVAL;

    if (pdisp_get_i2c_status->connector_index > DISP_CONNECTOR_NUM)
        return -DISP_EINVAL;

    pconnector = &pdd->disp_connector[pdisp_get_i2c_status->connector_index];

    return disp_connector_get_i2c_status(pconnector, pdisp_get_i2c_status);
}

DISP_DLLEXPORTS DISP_STATUS display_check_underflow(pdisp_void_t pdisp_device)
{
    disp_check_underflow(pdisp_device);

    return DISP_OK;
}

DISP_DLLEXPORTS DISP_STATUS display_notify_irq_hpd(pdisp_void_t pdisp_device, disp_u8_t connector_index)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_device;
    pdisp_connector_t pconnector = DISP_NULL;

    if (!pdd)
        return -DISP_EINVAL;
    if(connector_index >= DISP_CONNECTOR_NUM)
    {
        disp_print(DDM_WARNING, "display_notify_irq_hpd, invalid connector_index[%#x]\n", connector_index);
        return -DISP_EINVAL;
    }

    pconnector = &pdd->disp_connector[connector_index];
    if(pconnector->pfunc && pconnector->pfunc->irq_hpd_handle)
        return pconnector->pfunc->irq_hpd_handle(pconnector);
    else
        disp_print(DDM_WARNING, "display_notify_irq_hpd, func NULL\n");

    return DISP_OK;
}

DISP_DLLEXPORTS DISP_STATUS display_backlight_query(pdisp_void_t pdisp_device, pdisp_backlight_query_t pquery)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_device;
    pdisp_connector_t pconnector = DISP_NULL;

    if (!pdd || !pquery)
        return -DISP_EINVAL;

    if(pquery->connector_index >= DISP_CONNECTOR_NUM)
    {
        disp_print(DDM_WARNING, "display_backlight_set, invalid connector_index[%#x]\n", pquery->connector_index);
        return -DISP_EINVAL;
    }

    pconnector = &pdd->disp_connector[pquery->connector_index];
    if(pconnector->pfunc && pconnector->pfunc->backlight_query)
        return pconnector->pfunc->backlight_query(pconnector, pquery);
    else
        disp_print(DDM_WARNING, "display_set_backlight, func backlight_query NULL\n");

    return DISP_OK;
}

DISP_DLLEXPORTS DISP_STATUS display_backlight_set(pdisp_void_t pdisp_device, pdisp_backlight_set_t pset)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_device;
    pdisp_connector_t pconnector = DISP_NULL;

    if (!pdd || !pset)
        return -DISP_EINVAL;

    if(pset->connector_index >= DISP_CONNECTOR_NUM)
    {
        disp_print(DDM_WARNING, "display_backlight_set, invalid connector_index[%#x]\n", pset->connector_index);
        return -DISP_EINVAL;
    }

    pconnector = &pdd->disp_connector[pset->connector_index];
    if(pconnector->pfunc && pconnector->pfunc->backlight_set)
        return pconnector->pfunc->backlight_set(pconnector, pset);
    else
        disp_print(DDM_WARNING, "display_set_backlight, func backlight_set NULL\n");

    return DISP_OK;
}

DISP_DLLEXPORTS DISP_STATUS display_psr_query(pdisp_void_t pdisp_device, pdisp_psr_query_t pquery)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_device;
    pdisp_connector_t pconnector = DISP_NULL;

    if (!pdd || !pquery)
        return -DISP_EINVAL;

    if(pquery->connector_index >= DISP_CONNECTOR_NUM)
    {
        disp_print(DDM_WARNING, "display_psr_set, invalid connector_index[%#x]\n", pquery->connector_index);
        return -DISP_EINVAL;
    }

    pconnector = &pdd->disp_connector[pquery->connector_index];
    if(pconnector->pfunc && pconnector->pfunc->psr_query)
        return pconnector->pfunc->psr_query(pconnector, pquery);
    else
        disp_print(DDM_WARNING, "display_set_psr, func psr_query NULL\n");

    return DISP_OK;
}

DISP_DLLEXPORTS DISP_STATUS display_psr_set(pdisp_void_t pdisp_device, pdisp_psr_set_t pset)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_device;
    pdisp_connector_t pconnector = DISP_NULL;

    if (!pdd || !pset)
        return -DISP_EINVAL;

    if(pset->connector_index >= DISP_CONNECTOR_NUM)
    {
        disp_print(DDM_WARNING, "display_psr_set, invalid connector_index[%#x]\n", pset->connector_index);
        return -DISP_EINVAL;
    }

    pconnector = &pdd->disp_connector[pset->connector_index];
    if(pconnector->pfunc && pconnector->pfunc->psr_set)
        return pconnector->pfunc->psr_set(pconnector, pset);
    else
        disp_print(DDM_WARNING, "display_set_psr, func psr_set NULL\n");

    return DISP_OK;
}

DISP_DLLEXPORTS DISP_STATUS display_hda_handle(pdisp_void_t pdisp_device)
{
    if (!pdisp_device)
        return -DISP_EINVAL;

    disp_codec_irq(pdisp_device);
    return DISP_OK;
}

#ifdef DISP_WIN
#include <wdm.h>
DISP_DLLEXPORTS NTSTATUS DriverEntry(DRIVER_OBJECT* pDriverObject, UNICODE_STRING* pRegistryPath)
{
    UNREFERENCED_PARAMETER(pDriverObject);
    UNREFERENCED_PARAMETER(pRegistryPath);
    return STATUS_SUCCESS;
}
#endif
