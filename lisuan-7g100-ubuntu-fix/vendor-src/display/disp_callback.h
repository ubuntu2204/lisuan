/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#ifndef __DISP_CALLBACK_H__
#define __DISP_CALLBACK_H__

//here not need add relative path
#include "display.h"

#ifdef __DISP_GOP__
/*1:va_list in stdarg.h and VA_LIST in edk2 env are not compatiable
  2:need use EFIAPI to quilify the edk interface*/
#include "../../edk2/MdePkg/Include/Base.h"

#if defined(__loongarch64)
#include "../../edk2/MdePkg/Include/LoongArch64/ProcessorBind.h"
#else
#include "../../edk2/MdePkg/Include/X64/ProcessorBind.h"
#endif

#else
/*used for va_list/va_start/va_end*/
#ifdef __linux__

#include <linux/version.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
#include <linux/stdarg.h>
#else
#include <stdarg.h>
#endif

#else
#include <stdarg.h>
#endif
#define EFIAPI
#define VA_LIST va_list
#define VA_START va_start
#define VA_END va_end
#endif

#define DISP_PRINT_BUF_SIZE 256

// Print Mask
#define DISP_PRINT_MASK_CRTC            (1ul << 0)
#define DISP_PRINT_MASK_PLANE           (1ul << 1)
#define DISP_PRINT_MASK_PIPE            (1ul << 2)
#define DISP_PRINT_MASK_PLL             (1ul << 3)
#define DISP_PRINT_MASK_DHI             (1ul << 4)
#define DISP_PRINT_MASK_HDR             (1ul << 5)
#define DISP_PRINT_MASK_ENCODER         (1ul << 6)
#define DISP_PRINT_MASK_ENCODER_DP      (1ul << 7)
#define DISP_PRINT_MASK_ENCODER_HDMI    (1ul << 8)
#define DISP_PRINT_MASK_ENCODER_DVO     (1ul << 9)
#define DISP_PRINT_MASK_DP_PHY          (1ul << 10)
#define DISP_PRINT_MASK_DSC             (1ul << 11)
#define DISP_PRINT_MASK_CONNECTOR       (1ul << 12)
#define DISP_PRINT_MASK_CONNECTOR_DP    (1ul << 13)
#define DISP_PRINT_MASK_CONNECTOR_HDMI  (1ul << 14)
#define DISP_PRINT_MASK_CONNECTOR_DVO   (1ul << 15)
#define DISP_PRINT_MASK_EDID            (1ul << 16)
#define DISP_PRINT_MASK_DISPLAYID       (1ul << 17)
#define DISP_PRINT_MASK_MODE            (1ul << 18)
#define DISP_PRINT_MASK_DP_AUX          (1ul << 19)
#define DISP_PRINT_MASK_PWM             (1ul << 20)
#define DISP_PRINT_MASK_HDAUDIO         (1ul << 21)
#define DISP_PRINT_MASK_VBT             (1ul << 22)
#define DISP_PRINT_MASK_INTR            (1ul << 23)
#define DISP_PRINT_MASK_DEVICE          (1ul << 24)
#define DISP_PRINT_MASK_COMMON          (1ul << 31)

#define DDM_ERROR       0ul
#define DDM_WARNING     1ul
#define DDM_KEY         2ul
#define DDM_INFO        3ul
#define DDM_TRACE       4ul

typedef disp_s32_t EFIAPI (*disp_fn_vsnprintf_t)(pdisp_s8_t buf, disp_u32_t size, const pdisp_s8_t fmt, VA_LIST Marker);
typedef disp_void_t (*disp_fn_print_t)(disp_u32_t print_level, pdisp_s8_t message);
typedef disp_void_t (*disp_fn_print_to_file_t)(pdisp_void_t pdisp_adapter, disp_u32_t print_level, pdisp_s8_t message);

typedef pdisp_void_t (*disp_fn_malloc_t)(disp_u32_t size);
typedef disp_void_t (*disp_fn_free_t)(pdisp_void_t address);

typedef pdisp_void_t (*disp_fn_memset_t)(pdisp_void_t pBuf, disp_u8_t value, disp_u32_t length);
typedef pdisp_void_t (*disp_fn_memcpy_t)(pdisp_void_t pBuf1, pdisp_void_t pBuf2, disp_u32_t length);
typedef disp_u32_t (*disp_fn_memcmp_t)(pdisp_void_t pBuf1, pdisp_void_t pBuf2, disp_u32_t length);

typedef disp_u8_t (*disp_fn_read_u8_t)(pdisp_void_t pdisp_adapter, disp_u32_t offset);
typedef disp_void_t (*disp_fn_write_u8_t)(pdisp_void_t pdisp_adapter, disp_u32_t offset, disp_u8_t data);
typedef disp_u16_t (*disp_fn_read_u16_t)(pdisp_void_t pdisp_adapter, disp_u32_t offset);
typedef disp_void_t (*disp_fn_write_u16_t)(pdisp_void_t pdisp_adapter, disp_u32_t offset, disp_u16_t data);
typedef disp_u32_t (*disp_fn_read_u32_t)(pdisp_void_t pdisp_adapter, disp_u32_t offset);
typedef disp_void_t (*disp_fn_write_u32_t)(pdisp_void_t pdisp_adapter, disp_u32_t offset, disp_u32_t data);
typedef disp_u8_t (*disp_fn_read_rom_byte)(pdisp_void_t pdisp_adapter, disp_u64_t offset);

typedef disp_s32_t (*disp_fn_strcmp_t)(pdisp_u8_t s1, pdisp_u8_t s2);
typedef pdisp_u8_t (*disp_fn_strcpy_t)(pdisp_u8_t s1, pdisp_u8_t s2);
typedef disp_s32_t (*disp_fn_strncmp_t)(pdisp_u8_t s1, pdisp_u8_t s2, disp_u32_t size);
typedef disp_void_t (*disp_fn_delay_us_t)(disp_u64_t value);
typedef disp_u64_t (*disp_fn_do_div)(disp_u64_t a, disp_u64_t b);

DISP_STATUS disp_set_callback(pdisp_callback_t pdisp_callback);
DISP_STATUS disp_clear_callback(void);

disp_s32_t EFIAPI disp_snprintf(pdisp_s8_t dst_str, disp_u32_t size, pdisp_s8_t format, ...);
disp_void_t EFIAPI disp_print_ex(disp_u32_t mask, disp_u32_t level, pdisp_s8_t log, ...);
disp_void_t EFIAPI disp_print_pure_ex(disp_u32_t mask, disp_u32_t level, pdisp_s8_t log, ...);
disp_void_t EFIAPI disp_print_to_file_ex(pdisp_void_t pdisp_device, disp_u32_t mask, disp_u32_t level, pdisp_s8_t log, ...);
disp_void_t EFIAPI disp_print_pure_to_file_ex(pdisp_void_t pdisp_device, disp_u32_t mask, disp_u32_t level, pdisp_s8_t log, ...);

pdisp_void_t disp_malloc(disp_u32_t size);
disp_void_t disp_free(pdisp_void_t address);

pdisp_void_t disp_memset(pdisp_void_t pBuf, disp_u8_t value, disp_u32_t length);
pdisp_void_t disp_memcpy(pdisp_void_t pBuf1, pdisp_void_t pBuf2, disp_u32_t length);
disp_u32_t disp_memcmp(pdisp_void_t pBuf1, pdisp_void_t pBuf2, disp_u32_t length);

disp_u8_t disp_read_u8(pdisp_void_t pdisp_adapter, disp_u32_t offset);
disp_void_t disp_write_u8(pdisp_void_t pdisp_adapter, disp_u32_t offset, disp_u8_t data);
disp_u16_t disp_read_u16(pdisp_void_t pdisp_device, disp_u32_t offset);
disp_void_t disp_write_u16(pdisp_void_t pdisp_device, disp_u32_t offset, disp_u16_t data);
disp_u32_t disp_read_u32(pdisp_void_t pdisp_adapter, disp_u32_t offset);
disp_void_t disp_write_u32(pdisp_void_t pdisp_adapter, disp_u32_t offset, disp_u32_t data);

disp_s32_t disp_strcmp(pdisp_u8_t s1, pdisp_u8_t s2);
pdisp_u8_t disp_strcpy(pdisp_u8_t s1, pdisp_u8_t s2);
disp_s32_t disp_strncmp(pdisp_u8_t s1, pdisp_u8_t s2, disp_u32_t size);

disp_void_t disp_delay_us(disp_u64_t value);
disp_u64_t disp_do_div(disp_u64_t a, disp_u64_t b);
disp_u8_t disp_read_rom_byte(pdisp_void_t pdisp_device, disp_u64_t offset);

#endif
