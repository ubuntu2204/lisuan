/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#ifndef DISP_DISPLAYID_H
#define DISP_DISPLAYID_H

#include "display.h"

typedef struct _edid_t edid_t;

typedef struct _disp_edid_t {
    /* Size allocated for edid */
    disp_u64_t size;
    const edid_t *edid;
} disp_edid_t;

#define VESA_IEEE_OUI                0x3a0292

/* DisplayID Structure versions */
#define DISPLAY_ID_STRUCTURE_VER_20        0x20

/* DisplayID Structure v1r2 Data Blocks */
#define DATA_BLOCK_PRODUCT_ID            0x00
#define DATA_BLOCK_DISPLAY_PARAMETERS        0x01
#define DATA_BLOCK_COLOR_CHARACTERISTICS    0x02
#define DATA_BLOCK_TYPE_1_DETAILED_TIMING    0x03
#define DATA_BLOCK_TYPE_2_DETAILED_TIMING    0x04
#define DATA_BLOCK_TYPE_3_SHORT_TIMING        0x05
#define DATA_BLOCK_TYPE_4_DMT_TIMING        0x06
#define DATA_BLOCK_VESA_TIMING            0x07
#define DATA_BLOCK_CEA_TIMING            0x08
#define DATA_BLOCK_VIDEO_TIMING_RANGE        0x09
#define DATA_BLOCK_PRODUCT_SERIAL_NUMBER    0x0a
#define DATA_BLOCK_GP_ASCII_STRING        0x0b
#define DATA_BLOCK_DISPLAY_DEVICE_DATA        0x0c
#define DATA_BLOCK_INTERFACE_POWER_SEQUENCING    0x0d
#define DATA_BLOCK_TRANSFER_CHARACTERISTICS    0x0e
#define DATA_BLOCK_DISPLAY_INTERFACE        0x0f
#define DATA_BLOCK_STEREO_DISPLAY_INTERFACE    0x10
#define DATA_BLOCK_TILED_DISPLAY        0x12
#define DATA_BLOCK_VENDOR_SPECIFIC        0x7f
#define DATA_BLOCK_CTA                0x81

/* DisplayID Structure v2r0 Data Blocks */
#define DATA_BLOCK_2_PRODUCT_ID            0x20
#define DATA_BLOCK_2_DISPLAY_PARAMETERS        0x21
#define DATA_BLOCK_2_TYPE_7_DETAILED_TIMING    0x22
#define DATA_BLOCK_2_TYPE_8_ENUMERATED_TIMING    0x23
#define DATA_BLOCK_2_TYPE_9_FORMULA_TIMING    0x24
#define DATA_BLOCK_2_DYNAMIC_VIDEO_TIMING    0x25
#define DATA_BLOCK_2_DISPLAY_INTERFACE_FEATURES    0x26
#define DATA_BLOCK_2_STEREO_DISPLAY_INTERFACE    0x27
#define DATA_BLOCK_2_TILED_DISPLAY_TOPOLOGY    0x28
#define DATA_BLOCK_2_CONTAINER_ID        0x29
#define DATA_BLOCK_2_VENDOR_SPECIFIC        0x7e
#define DATA_BLOCK_2_CTA_DISPLAY_ID        0x81

/* DisplayID Structure v1r2 Product Type */
#define PRODUCT_TYPE_EXTENSION            0
#define PRODUCT_TYPE_TEST            1
#define PRODUCT_TYPE_PANEL            2
#define PRODUCT_TYPE_MONITOR            3
#define PRODUCT_TYPE_TV                4
#define PRODUCT_TYPE_REPEATER            5
#define PRODUCT_TYPE_DIRECT_DRIVE        6

/* DisplayID Structure v2r0 Display Product Primary Use Case (~Product Type) */
#define PRIMARY_USE_EXTENSION            0
#define PRIMARY_USE_TEST            1
#define PRIMARY_USE_GENERIC            2
#define PRIMARY_USE_TV                3
#define PRIMARY_USE_DESKTOP_PRODUCTIVITY    4
#define PRIMARY_USE_DESKTOP_GAMING        5
#define PRIMARY_USE_PRESENTATION        6
#define PRIMARY_USE_HEAD_MOUNTED_VR        7
#define PRIMARY_USE_HEAD_MOUNTED_AR        8

typedef struct _displayid_header_t {
    disp_u8_t rev;
    disp_u8_t bytes;
    disp_u8_t prod_id;
    disp_u8_t ext_count;
} displayid_header_t;

typedef struct _displayid_block_t {
    disp_u8_t tag;
    disp_u8_t rev;
    disp_u8_t num_bytes;
} displayid_block_t;

typedef struct _displayid_tiled_block_t {
    displayid_block_t base;
    disp_u8_t tile_cap;
    disp_u8_t topo[3];
    disp_u8_t tile_size[4];
    disp_u8_t tile_pixel_bezel[5];
    disp_u8_t topology_id[8];
} displayid_tiled_block_t;

typedef struct _displayid_detailed_timings_1_t {
    disp_u8_t pixel_clock[3];
    disp_u8_t flags;
    disp_u8_t hactive[2];
    disp_u8_t hblank[2];
    disp_u8_t hsync[2];
    disp_u8_t hsw[2];
    disp_u8_t vactive[2];
    disp_u8_t vblank[2];
    disp_u8_t vsync[2];
    disp_u8_t vsw[2];
} displayid_detailed_timings_1_t;

typedef struct _displayid_detailed_timing_block_t {
    displayid_block_t base;
    //displayid_detailed_timings_1_t timings[];
    //displayid_detailed_timings_1_t *timings;
    displayid_detailed_timings_1_t timings[1];
} displayid_detailed_timing_block_t;

#define DISPLAYID_VESA_MSO_OVERLAP    GENMASK(3, 0)
#define DISPLAYID_VESA_MSO_MODE        GENMASK(6, 5)

typedef struct _displayid_vesa_vendor_specific_block_t {
    displayid_block_t base;
    disp_u8_t oui[3];
    disp_u8_t data_structure_type;
    disp_u8_t mso;
} displayid_vesa_vendor_specific_block_t;

/*
 * DisplayID iteration.
 *
 * Do not access directly, this is private.
 */
typedef struct _displayid_iter_t {
    const disp_edid_t *disp_edid;

    const disp_u8_t *section;
    disp_s32_t length;
    disp_s32_t idx;
    disp_s32_t ext_index;

    disp_u8_t version;
    disp_u8_t primary_use;
} displayid_iter_t;

disp_void_t displayid_iter_edid_begin(const disp_edid_t *disp_edid, displayid_iter_t *iter);
const displayid_block_t *displayid_iter_next(displayid_iter_t *iter);
#define displayid_iter_for_each(__block, __iter) \
    while (((__block) = displayid_iter_next(__iter)))
disp_void_t displayid_iter_end(displayid_iter_t *iter);

disp_u8_t displayid_version(const displayid_iter_t *iter);
disp_u8_t displayid_primary_use(const displayid_iter_t *iter);

#endif

