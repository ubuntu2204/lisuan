/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#include "disp_callback.h"
#include "disp_device.h"

disp_callback_t g_callback = {0};
#ifdef __DDM_PRINT_LEVEL__
disp_u32_t g_print_level = __DDM_PRINT_LEVEL__;
#else
disp_u32_t g_print_level = DDM_KEY;
#endif
disp_u32_t g_print_mask =
    DISP_PRINT_MASK_CRTC |
    DISP_PRINT_MASK_PLANE |
    DISP_PRINT_MASK_PIPE |
    DISP_PRINT_MASK_PLL |
    DISP_PRINT_MASK_DHI |
    DISP_PRINT_MASK_HDR |
    DISP_PRINT_MASK_ENCODER |
    DISP_PRINT_MASK_ENCODER_DP |
    DISP_PRINT_MASK_ENCODER_HDMI |
    DISP_PRINT_MASK_ENCODER_DVO |
    DISP_PRINT_MASK_DP_PHY |
    DISP_PRINT_MASK_DSC |
    DISP_PRINT_MASK_CONNECTOR |
    DISP_PRINT_MASK_CONNECTOR_DP |
    DISP_PRINT_MASK_CONNECTOR_HDMI |
    DISP_PRINT_MASK_CONNECTOR_DVO |
    DISP_PRINT_MASK_EDID |
    DISP_PRINT_MASK_DISPLAYID |
    DISP_PRINT_MASK_MODE |
    //DISP_PRINT_MASK_DP_AUX | // too many aux failed when DP-HDMI adapter retry aux
    DISP_PRINT_MASK_PWM |
    DISP_PRINT_MASK_HDAUDIO |
    DISP_PRINT_MASK_VBT |
    DISP_PRINT_MASK_INTR |
    DISP_PRINT_MASK_DEVICE |
    DISP_PRINT_MASK_COMMON;

DISP_STATUS disp_set_callback(pdisp_callback_t pdisp_callback)
{
    if(!pdisp_callback)
        return -DISP_EINVAL;

    if((pdisp_callback->pdisp_fn_vsnprintf == DISP_NULL) ||
        (pdisp_callback->pdisp_fn_print == DISP_NULL))
        return -DISP_EINVAL;

    g_callback.pdisp_fn_vsnprintf = pdisp_callback->pdisp_fn_vsnprintf;
    g_callback.pdisp_fn_print = pdisp_callback->pdisp_fn_print;

    if (//!pdisp_callback->pdisp_fn_log_to_file
        !pdisp_callback->pdisp_fn_malloc
        || !pdisp_callback->pdisp_fn_free
        || !pdisp_callback->pdisp_fn_memset
        || !pdisp_callback->pdisp_fn_memcpy
        || !pdisp_callback->pdisp_fn_memcmp
        || !pdisp_callback->pdisp_fn_read_u8
        || !pdisp_callback->pdisp_fn_write_u8
        || !pdisp_callback->pdisp_fn_read_u16
        || !pdisp_callback->pdisp_fn_write_u16
        || !pdisp_callback->pdisp_fn_read_u32
        || !pdisp_callback->pdisp_fn_write_u32
        || !pdisp_callback->pdisp_fn_strcmp
        || !pdisp_callback->pdisp_fn_strcpy
        || !pdisp_callback->pdisp_fn_strncmp
        || !pdisp_callback->pdisp_fn_delay_us
        || !pdisp_callback->pdisp_fn_read_rom_byte)
    {
        ((disp_fn_print_t)g_callback.pdisp_fn_print)(0, "DDM_ERR: some callback funcation not set\n");
        return -DISP_EINVAL;
    }

    g_callback = *pdisp_callback;

    return DISP_OK;
}

DISP_STATUS disp_clear_callback(void)
{
    if(g_callback.pdisp_fn_memset == DISP_NULL)
    {
        return -DISP_EINVAL;
    }

    disp_memset(&g_callback, 0, sizeof(disp_callback_t));

    return DISP_OK;
}

disp_s32_t EFIAPI disp_snprintf(pdisp_s8_t dst_str, disp_u32_t size, pdisp_s8_t format, ...)
{
    VA_LIST args;
    disp_s32_t ret = 0;

    VA_START(args, format);
    ret = ((disp_fn_vsnprintf_t)g_callback.pdisp_fn_vsnprintf)(dst_str, size, format, args);
    VA_END(args);

    return ret;
}

disp_void_t EFIAPI disp_print_pure_ex(disp_u32_t mask, disp_u32_t level, pdisp_s8_t log, ...)
{
    VA_LIST args;
    disp_s8_t buff[DISP_PRINT_BUF_SIZE] = {0};

    if(level <= g_print_level && (mask & g_print_mask))
    {
        VA_START(args, log);
        ((disp_fn_vsnprintf_t)g_callback.pdisp_fn_vsnprintf)(buff, DISP_PRINT_BUF_SIZE * 2 - 1, log, args);
        VA_END(args);

        ((disp_fn_print_t)g_callback.pdisp_fn_print)(0, buff);
    }
}

disp_void_t EFIAPI disp_print_ex(disp_u32_t mask, disp_u32_t level, pdisp_s8_t log, ...)
{
    disp_s8_t buff[DISP_PRINT_BUF_SIZE] = {0};
    disp_s8_t prefixed_buf[DISP_PRINT_BUF_SIZE] = {0};
    disp_s8_t prefix[5][8] = {"ERRO", "WARN", "_KEY", "INFO", "TRAC"}; // error, warning, key, info, trace
    VA_LIST args;

    if(level <= g_print_level && (mask & g_print_mask))
    {
        VA_START(args, log);
        ((disp_fn_vsnprintf_t)g_callback.pdisp_fn_vsnprintf)(buff, DISP_PRINT_BUF_SIZE - 1, log, args);
        VA_END(args);

        disp_snprintf(prefixed_buf, DISP_PRINT_BUF_SIZE - 1, "DDM_%s: %s", prefix[level], buff);
        ((disp_fn_print_t)g_callback.pdisp_fn_print)(0, prefixed_buf);
    }
}

disp_void_t EFIAPI disp_print_pure_to_file_ex(pdisp_void_t pdisp_device, disp_u32_t mask, disp_u32_t level, pdisp_s8_t log, ...)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_device;
    VA_LIST args;
    disp_s8_t buff[DISP_PRINT_BUF_SIZE] = {0};

    if(g_callback.pdisp_fn_print_to_file && level <= g_print_level && (mask & g_print_mask))
    {
        VA_START(args, log);
        ((disp_fn_vsnprintf_t)g_callback.pdisp_fn_vsnprintf)(buff, DISP_PRINT_BUF_SIZE * 2 - 1, log, args);
        VA_END(args);

        ((disp_fn_print_to_file_t)g_callback.pdisp_fn_print_to_file)(pdd->disp_adapter, level, buff);
    }
}

disp_void_t EFIAPI disp_print_to_file_ex(pdisp_void_t pdisp_device, disp_u32_t mask, disp_u32_t level, pdisp_s8_t log, ...)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_device;
    disp_s8_t buff[DISP_PRINT_BUF_SIZE] = {0};
    disp_s8_t prefixed_buf[DISP_PRINT_BUF_SIZE] = {0};
    disp_s8_t prefix[5][8] = {"ERRO", "WARN", "_KEY", "INFO", "TRAC"}; // error, warning, key, info, trace
    VA_LIST args;

    if (g_callback.pdisp_fn_print_to_file && level <= g_print_level && (mask & g_print_mask))
    {
        VA_START(args, log);
        ((disp_fn_vsnprintf_t)g_callback.pdisp_fn_vsnprintf)(buff, DISP_PRINT_BUF_SIZE - 1, log, args);
        VA_END(args);

        disp_snprintf(prefixed_buf, DISP_PRINT_BUF_SIZE - 1, "DDM_%s: %s", prefix[level], buff);
        ((disp_fn_print_to_file_t)g_callback.pdisp_fn_print_to_file)(pdd->disp_adapter, level, prefixed_buf);
    }
}

pdisp_void_t disp_malloc(disp_u32_t size)
{
    return ((disp_fn_malloc_t)g_callback.pdisp_fn_malloc)(size);
}

disp_void_t disp_free(pdisp_void_t address)
{
    ((disp_fn_free_t)g_callback.pdisp_fn_free)(address);
}

pdisp_void_t disp_memset(pdisp_void_t pBuf, disp_u8_t value, disp_u32_t length)
{
    return ((disp_fn_memset_t)g_callback.pdisp_fn_memset)(pBuf, value, length);
}
pdisp_void_t disp_memcpy(pdisp_void_t pBuf1, pdisp_void_t pBuf2, disp_u32_t length)
{
    return ((disp_fn_memcpy_t)g_callback.pdisp_fn_memcpy)(pBuf1, pBuf2, length);
}

disp_u32_t disp_memcmp(pdisp_void_t pBuf1, pdisp_void_t pBuf2, disp_u32_t length)
{
    return ((disp_fn_memcmp_t)g_callback.pdisp_fn_memcmp)(pBuf1, pBuf2, length);
}

disp_u8_t disp_read_u8(pdisp_void_t pdisp_device, disp_u32_t offset)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_device;
    return ((disp_fn_read_u8_t)g_callback.pdisp_fn_read_u8)(pdd->disp_adapter, offset);
}

disp_void_t disp_write_u8(pdisp_void_t pdisp_device, disp_u32_t offset, disp_u8_t data)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_device;
    ((disp_fn_write_u8_t)g_callback.pdisp_fn_write_u8)(pdd->disp_adapter, offset, data);
}

disp_u32_t disp_read_u32(pdisp_void_t pdisp_device, disp_u32_t offset)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_device;
    return ((disp_fn_read_u32_t)g_callback.pdisp_fn_read_u32)(pdd->disp_adapter, offset);
}

disp_void_t disp_write_u32(pdisp_void_t pdisp_device, disp_u32_t offset, disp_u32_t data)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_device;
    ((disp_fn_write_u32_t)g_callback.pdisp_fn_write_u32)(pdd->disp_adapter, offset, data);
}

disp_u16_t disp_read_u16(pdisp_void_t pdisp_device, disp_u32_t offset)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_device;
    return ((disp_fn_read_u16_t)g_callback.pdisp_fn_read_u16)(pdd->disp_adapter, offset);
}

disp_void_t disp_write_u16(pdisp_void_t pdisp_device, disp_u32_t offset, disp_u16_t data)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_device;
    ((disp_fn_write_u16_t)g_callback.pdisp_fn_write_u16)(pdd->disp_adapter, offset, data);
}

disp_u8_t disp_read_rom_byte(pdisp_void_t pdisp_device, disp_u64_t offset)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_device;
    return ((disp_fn_read_rom_byte)g_callback.pdisp_fn_read_rom_byte)(pdd->disp_adapter, offset);
}

disp_s32_t disp_strcmp(pdisp_u8_t s1, pdisp_u8_t s2)
{
    return ((disp_fn_strcmp_t)g_callback.pdisp_fn_strcmp)(s1, s2);
}

pdisp_u8_t disp_strcpy(pdisp_u8_t s1, pdisp_u8_t s2)
{
    return ((disp_fn_strcpy_t)g_callback.pdisp_fn_strcpy)(s1, s2);
}

disp_s32_t disp_strncmp(pdisp_u8_t s1, pdisp_u8_t s2, disp_u32_t size)
{
    return ((disp_fn_strncmp_t)g_callback.pdisp_fn_strncmp)(s1, s2, size);
}

disp_void_t disp_delay_us(disp_u64_t value)
{
    ((disp_fn_delay_us_t)g_callback.pdisp_fn_delay_us)(value);
}

disp_u64_t disp_do_div(disp_u64_t a, disp_u64_t b)
{
    disp_u64_t ret = 0;

    if (g_callback.pdisp_fn_do_div != DISP_NULL) {
        ret = ((disp_fn_do_div)g_callback.pdisp_fn_do_div)(a, b);
    } else {
        if (b == 0) {
            ((disp_fn_print_t)g_callback.pdisp_fn_print)(0, "DDM_ERR: disp_do_div: divisor is 0\n");
        } else {
            ret = (a)/(b);
        }
    }

    return ret;
}
