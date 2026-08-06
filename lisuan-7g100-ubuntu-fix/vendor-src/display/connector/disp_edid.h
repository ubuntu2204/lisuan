/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#ifndef __DISP_EDID_H__
#define __DISP_EDID_H__

/*
 * duplicate from the release linux kernel 5.13.12
 */

#include "display.h"
#include "disp_device.h"
#include "disp_hdmi_encoder.h"

#define EDID_LENGTH 128
#define DDC_ADDR    0x50
#define DDC_ADDR2   0x52 /* E-DDC 1.2 - where DisplayID can hide */
#define DDC_SEGMENT_ADDR 0x30

#define CEA_EXT       0x02
#define VTB_EXT       0x10
#define DI_EXT        0x40
#define LS_EXT        0x50
#define MI_EXT        0x60
#define DISPLAYID_EXT 0x70

typedef struct _est_timings_t
{
    disp_u8_t t1;
    disp_u8_t t2;
    disp_u8_t mfg_rsvd;
}est_timings_t, *pest_timings_t;

/* 00=16:10, 01=4:3, 10=5:4, 11=16:9 */
#define EDID_TIMING_ASPECT_SHIFT 6
#define EDID_TIMING_ASPECT_MASK  (0x3 << EDID_TIMING_ASPECT_SHIFT)

/* need to add 60 */
#define EDID_TIMING_VFREQ_SHIFT  0
#define EDID_TIMING_VFREQ_MASK   (0x3f << EDID_TIMING_VFREQ_SHIFT)

typedef struct _std_timing_t
{
    disp_u8_t hsize; /* need to multiply by 8 then add 248 */
    disp_u8_t vfreq_aspect;
}std_timing_t, *pstd_timing_t;

#define DISP_EDID_PT_HSYNC_POSITIVE (1 << 1)
#define DISP_EDID_PT_VSYNC_POSITIVE (1 << 2)
#define DISP_EDID_PT_SEPARATE_SYNC  (3 << 3)
#define DISP_EDID_PT_STEREO         (1 << 5)
#define DISP_EDID_PT_INTERLACED     (1 << 7)

/* If detailed data is pixel timing */
typedef struct _detailed_pixel_timing_t
{
    disp_u8_t hactive_lo;
    disp_u8_t hblank_lo;
    disp_u8_t hactive_hblank_hi;
    disp_u8_t vactive_lo;
    disp_u8_t vblank_lo;
    disp_u8_t vactive_vblank_hi;
    disp_u8_t hsync_offset_lo;
    disp_u8_t hsync_pulse_width_lo;
    disp_u8_t vsync_offset_pulse_width_lo;
    disp_u8_t hsync_vsync_offset_pulse_width_hi;
    disp_u8_t width_mm_lo;
    disp_u8_t height_mm_lo;
    disp_u8_t width_height_mm_hi;
    disp_u8_t hborder;
    disp_u8_t vborder;
    disp_u8_t misc;
}detailed_pixel_timing_t, *pdetailed_pixel_timing_t;

/* If it's not pixel timing, it'll be one of the below */
typedef struct _detailed_data_string_t
{
    disp_u8_t str[13];
}detailed_data_string_t, *pdetailed_data_string_t;

#define DISP_EDID_DEFAULT_GTF_SUPPORT_FLAG   0x00
#define DISP_EDID_RANGE_LIMITS_ONLY_FLAG     0x01
#define DISP_EDID_SECONDARY_GTF_SUPPORT_FLAG 0x02
#define DISP_EDID_CVT_SUPPORT_FLAG           0x04

typedef struct _detailed_data_monitor_range_t
{
    disp_u8_t min_vfreq;
    disp_u8_t max_vfreq;
    disp_u8_t min_hfreq_khz;
    disp_u8_t max_hfreq_khz;
    disp_u8_t pixel_clock_mhz; /* need to multiply by 10 */
    disp_u8_t flags;
    union 
    {
        struct 
        {
            disp_u8_t reserved;
            disp_u8_t hfreq_start_khz; /* need to multiply by 2 */
            disp_u8_t c; /* need to divide by 2 */
            disp_u8_t m[2];
            disp_u8_t k;
            disp_u8_t j; /* need to divide by 2 */
        }gtf2;
        struct 
        {
            disp_u8_t version;
            disp_u8_t data1; /* high 6 bits: extra clock resolution */
            disp_u8_t data2; /* plus low 2 of above: max hactive */
            disp_u8_t supported_aspects;
            disp_u8_t flags; /* preferred aspect and blanking support */
            disp_u8_t supported_scalings;
            disp_u8_t preferred_refresh;
        }cvt;
    } formula;
}detailed_data_monitor_range_t, *pdetailed_data_monitor_range_t;

typedef struct _detailed_data_wpindex_t
{
    disp_u8_t white_yx_lo; /* Lower 2 bits each */
    disp_u8_t white_x_hi;
    disp_u8_t white_y_hi;
    disp_u8_t gamma; /* need to divide by 100 then add 1 */
}detailed_data_wpindex_t, *pdetailed_data_wpindex_t;

typedef struct _detailed_data_color_point_t 
{
    disp_u8_t windex1;
    detailed_data_wpindex_t wpindex1;
    disp_u8_t windex2;
    detailed_data_wpindex_t wpindex2;
    disp_u8_t resv[3];
}detailed_data_color_point_t, *pdetailed_data_color_point;

typedef struct _cvt_timing_t 
{
    disp_u8_t code[3];
}cvt_timing_t, *pcvt_timing_t;

typedef struct _detailed_cvt_timing_t 
{
    cvt_timing_t cvt[4];
    disp_u8_t resv;
}detailed_cvt_timing_t;

typedef struct _detailed_std_timing_t
{
    std_timing_t timings[6];
    disp_u8_t resv;
}detailed_std_timing_t;

typedef struct _detailed_non_pixel_t 
{
    disp_u8_t pad1;
    disp_u8_t type; /* ff=serial, fe=string, fd=monitor range, fc=monitor name
            fb=color point data, fa=standard timing data,
            f9=undefined, f8=mfg. reserved */
    disp_u8_t pad2;
    union 
    {
        detailed_data_string_t str;
        detailed_data_monitor_range_t range;
        detailed_data_color_point_t color;
        detailed_std_timing_t timings;
        detailed_cvt_timing_t cvt;
    } data;
}detailed_non_pixel_t, *pdetailed_non_pixel_t;

#define EDID_DETAIL_EST_TIMINGS 0xf7
#define EDID_DETAIL_CVT_3BYTE 0xf8
#define EDID_DETAIL_COLOR_MGMT_DATA 0xf9
#define EDID_DETAIL_STD_MODES 0xfa
#define EDID_DETAIL_MONITOR_CPDATA 0xfb
#define EDID_DETAIL_MONITOR_NAME 0xfc
#define EDID_DETAIL_MONITOR_RANGE 0xfd
#define EDID_DETAIL_MONITOR_STRING 0xfe
#define EDID_DETAIL_MONITOR_SERIAL 0xff

typedef struct _detailed_timing_t 
{
    disp_u8_t pixel_clock_low; /* need to multiply by 10 KHz */
    disp_u8_t pixel_clock_high;
    union
    {
        detailed_pixel_timing_t pixel_data;
        detailed_non_pixel_t other_data;
    } data;
}detailed_timing_t, *pdetailed_timing_t;

#define DISP_EDID_INPUT_SERRATION_VSYNC (1 << 0)
#define DISP_EDID_INPUT_SYNC_ON_GREEN   (1 << 1)
#define DISP_EDID_INPUT_COMPOSITE_SYNC  (1 << 2)
#define DISP_EDID_INPUT_SEPARATE_SYNCS  (1 << 3)
#define DISP_EDID_INPUT_BLANK_TO_BLACK  (1 << 4)
#define DISP_EDID_INPUT_VIDEO_LEVEL     (3 << 5)
#define DISP_EDID_INPUT_DIGITAL         (1 << 7)
#define DISP_EDID_DIGITAL_DEPTH_MASK    (7 << 4) /* 1.4 */
#define DISP_EDID_DIGITAL_DEPTH_UNDEF   (0 << 4) /* 1.4 */
#define DISP_EDID_DIGITAL_DEPTH_6       (1 << 4) /* 1.4 */
#define DISP_EDID_DIGITAL_DEPTH_8       (2 << 4) /* 1.4 */
#define DISP_EDID_DIGITAL_DEPTH_10      (3 << 4) /* 1.4 */
#define DISP_EDID_DIGITAL_DEPTH_12      (4 << 4) /* 1.4 */
#define DISP_EDID_DIGITAL_DEPTH_14      (5 << 4) /* 1.4 */
#define DISP_EDID_DIGITAL_DEPTH_16      (6 << 4) /* 1.4 */
#define DISP_EDID_DIGITAL_DEPTH_RSVD    (7 << 4) /* 1.4 */
#define DISP_EDID_DIGITAL_TYPE_MASK     (7 << 0) /* 1.4 */
#define DISP_EDID_DIGITAL_TYPE_UNDEF    (0 << 0) /* 1.4 */
#define DISP_EDID_DIGITAL_TYPE_DVI      (1 << 0) /* 1.4 */
#define DISP_EDID_DIGITAL_TYPE_HDMI_A   (2 << 0) /* 1.4 */
#define DISP_EDID_DIGITAL_TYPE_HDMI_B   (3 << 0) /* 1.4 */
#define DISP_EDID_DIGITAL_TYPE_MDDI     (4 << 0) /* 1.4 */
#define DISP_EDID_DIGITAL_TYPE_DP       (5 << 0) /* 1.4 */
#define DISP_EDID_DIGITAL_DFP_1_X       (1 << 0) /* 1.3 */

#define DISP_EDID_FEATURE_DEFAULT_GTF       (1 << 0)
#define DISP_EDID_FEATURE_PREFERRED_TIMING  (1 << 1)
#define DISP_EDID_FEATURE_STANDARD_COLOR    (1 << 2)
/* If analog */
#define DISP_EDID_FEATURE_DISPLAY_TYPE      (3 << 3) /* 00=mono, 01=rgb, 10=non-rgb, 11=unknown */
/* If digital */
#define DISP_EDID_FEATURE_COLOR_MASK        (3 << 3)
#define DISP_EDID_FEATURE_RGB               (0 << 3)
#define DISP_EDID_FEATURE_RGB_YCRCB444      (1 << 3)
#define DISP_EDID_FEATURE_RGB_YCRCB422      (2 << 3)
#define DISP_EDID_FEATURE_RGB_YCRCB         (3 << 3) /* both 4:4:4 and 4:2:2 */

#define DISP_EDID_FEATURE_PM_ACTIVE_OFF     (1 << 5)
#define DISP_EDID_FEATURE_PM_SUSPEND        (1 << 6)
#define DISP_EDID_FEATURE_PM_STANDBY        (1 << 7)

#define DISP_EDID_HDMI_DC_48                (1 << 6)
#define DISP_EDID_HDMI_DC_36                (1 << 5)
#define DISP_EDID_HDMI_DC_30                (1 << 4)
#define DISP_EDID_HDMI_DC_Y444              (1 << 3)

/* YCBCR 420 deep color modes */
#define DISP_EDID_YCBCR420_DC_48            (1 << 2)
#define DISP_EDID_YCBCR420_DC_36            (1 << 1)
#define DISP_EDID_YCBCR420_DC_30            (1 << 0)
#define DISP_EDID_YCBCR420_DC_MASK (DISP_EDID_YCBCR420_DC_48 | \
                    DISP_EDID_YCBCR420_DC_36 | \
                    DISP_EDID_YCBCR420_DC_30)

/* HDMI 2.1 additional fields */
#define DISP_EDID_MAX_FRL_RATE_MASK         0xf0
#define DISP_EDID_FAPA_START_LOCATION       (1 << 0)
#define DISP_EDID_ALLM                      (1 << 1)
#define DISP_EDID_FVA                       (1 << 2)

/* Deep Color specific */
#define DISP_EDID_DC_30BIT_420            (1 << 0)
#define DISP_EDID_DC_36BIT_420            (1 << 1)
#define DISP_EDID_DC_48BIT_420            (1 << 2)

/* VRR specific */
#define DISP_EDID_CNMVRR                (1 << 3)
#define DISP_EDID_CINEMA_VRR            (1 << 4)
#define DISP_EDID_MDELTA                (1 << 5)
#define DISP_EDID_VRR_MAX_UPPER_MASK        0xc0
#define DISP_EDID_VRR_MAX_LOWER_MASK        0xff
#define DISP_EDID_VRR_MIN_MASK              0x3f

/* DSC specific */
#define DISP_EDID_DSC_10BPC             (1 << 0)
#define DISP_EDID_DSC_12BPC             (1 << 1)
#define DISP_EDID_DSC_16BPC             (1 << 2)
#define DISP_EDID_DSC_ALL_BPP           (1 << 3)
#define DISP_EDID_DSC_NATIVE_420        (1 << 6)
#define DISP_EDID_DSC_1P2               (1 << 7)
#define DISP_EDID_DSC_MAX_FRL_RATE_MASK     0xf0
#define DISP_EDID_DSC_MAX_SLICES             0xf
#define DISP_EDID_DSC_TOTAL_CHUNK_KBYTES    0x3f

/* ELD Header Block */
#define DISP_ELD_HEADER_BLOCK_SIZE    4

#define DISP_ELD_VER                    0
#define DISP_ELD_VER_SHIFT              3
#define DISP_ELD_VER_MASK     (0x1f << 3)
#define DISP_ELD_VER_CEA861D     (2 << 3) /* supports 861D or below */
#define DISP_ELD_VER_CANNED   (0x1f << 3)

#define DISP_ELD_BASELINE_ELD_LEN    2    /* in dwords! */

/* ELD Baseline Block for ELD_Ver == 2 */
#define DISP_ELD_CEA_EDID_VER_MNL    4
#define DISP_ELD_CEA_EDID_VER_SHIFT    5
#define DISP_ELD_CEA_EDID_VER_MASK    (7 << 5)
#define DISP_ELD_CEA_EDID_VER_NONE    (0 << 5)
#define DISP_ELD_CEA_EDID_VER_CEA861    (1 << 5)
#define DISP_ELD_CEA_EDID_VER_CEA861A    (2 << 5)
#define DISP_ELD_CEA_EDID_VER_CEA861BCD    (3 << 5)
#define DISP_ELD_MNL_SHIFT        0
#define DISP_ELD_MNL_MASK        (0x1f << 0)

#define DISP_ELD_SAD_COUNT_CONN_TYPE    5
#define DISP_ELD_SAD_COUNT_SHIFT    4
#define DISP_ELD_SAD_COUNT_MASK        (0xf << 4)
#define DISP_ELD_CONN_TYPE_SHIFT    2
#define DISP_ELD_CONN_TYPE_MASK        (3 << 2)
#define DISP_ELD_CONN_TYPE_HDMI        (0 << 2)
#define DISP_ELD_CONN_TYPE_DP        (1 << 2)
#define DISP_ELD_SUPPORTS_AI        (1 << 1)
#define DISP_ELD_SUPPORTS_HDCP        (1 << 0)

#define DISP_ELD_AUD_SYNCH_DELAY        6    /* in units of 2 ms */
#define DISP_ELD_AUD_SYNCH_DELAY_MAX    0xfa    /* 500 ms */

#define DISP_ELD_SPEAKER            7
#define DISP_ELD_SPEAKER_MASK        0x7f
#define DISP_ELD_SPEAKER_RLRC        (1 << 6)
#define DISP_ELD_SPEAKER_FLRC        (1 << 5)
#define DISP_ELD_SPEAKER_RC        (1 << 4)
#define DISP_ELD_SPEAKER_RLR        (1 << 3)
#define DISP_ELD_SPEAKER_FC        (1 << 2)
#define DISP_ELD_SPEAKER_LFE        (1 << 1)
#define DISP_ELD_SPEAKER_FLR        (1 << 0)

#define DISP_ELD_PORT_ID            8    /* offsets 8..15 inclusive */
#define DISP_ELD_PORT_ID_LEN        8

#define DISP_ELD_MANUFACTURER_NAME0    16
#define DISP_ELD_MANUFACTURER_NAME1    17

#define DISP_ELD_PRODUCT_CODE0        18
#define DISP_ELD_PRODUCT_CODE1        19

#define DISP_ELD_MONITOR_NAME_STRING    20    /* offsets 20..(20+mnl-1) inclusive */

#define DISP_ELD_CEA_SAD(mnl, sad)    (20 + (mnl) + 3 * (sad))

typedef struct _edid_t {
    disp_u8_t header[8];
    /* Vendor & product info */
    disp_u8_t mfg_id[2];
    disp_u8_t prod_code[2];
    disp_u8_t serial[4]; /* FIXME: byte order */
    disp_u8_t mfg_week;
    disp_u8_t mfg_year;
    /* EDID version */
    disp_u8_t version;
    disp_u8_t revision;
    /* Display info: */
    disp_u8_t input;
    disp_u8_t width_cm;
    disp_u8_t height_cm;
    disp_u8_t gamma;
    disp_u8_t features;
    /* Color characteristics */
    disp_u8_t red_green_lo;
    disp_u8_t blue_white_lo;
    disp_u8_t red_x;
    disp_u8_t red_y;
    disp_u8_t green_x;
    disp_u8_t green_y;
    disp_u8_t blue_x;
    disp_u8_t blue_y;
    disp_u8_t white_x;
    disp_u8_t white_y;
    /* Est. timings and mfg rsvd timings*/
    est_timings_t established_timings;
    /* Standard timings 1-8*/
    std_timing_t standard_timings[8];
    /* Detailing timings 1-4 */
    detailed_timing_t detailed_timings[4];
    /* Number of 128 byte ext. blocks */
    disp_u8_t extensions;
    /* Checksum */
    disp_u8_t checksum;
}edid_t, *pedid_t;

#define EDID_PRODUCT_ID(e) ((e)->prod_code[0] | ((e)->prod_code[1] << 8))

disp_u8_t disp_edid_ext_count(pedid_t edid);
disp_s32_t disp_edid_to_sad(pedid_t edid, pcea_sad_t *ppsads);
disp_s32_t disp_edid_to_speaker_allocation(pedid_t edid, disp_u8_t **sadb);
disp_s32_t disp_av_sync_delay(pdisp_connector_t pdisp_connector,
              pdisp_display_mode_t mode);

disp_u8_t disp_edid_are_equal(pedid_t edid1, pedid_t edid2);
/***
disp_s32_t
disp_hdmi_infoframe_set_hdr_metadata(struct hdmi_disp_infoframe *frame,
                    struct disp_connector_state *conn_state);
***/
/**
 * disp_eld_mnl - Get ELD monitor name length in bytes.
 * @eld: pointer to an eld memory structure with mnl set
 */
static inline disp_s32_t disp_eld_mnl(disp_u8_t *eld)
{
    return (eld[DISP_ELD_CEA_EDID_VER_MNL] & DISP_ELD_MNL_MASK) >> DISP_ELD_MNL_SHIFT;
}

/**
 * disp_eld_sad - Get ELD SAD structures.
 * @eld: pointer to an eld memory structure with sad_count set
 */
static inline disp_u8_t *disp_eld_sad(disp_u8_t *eld)
{
    disp_u32_t ver, mnl;

    ver = (eld[DISP_ELD_VER] & DISP_ELD_VER_MASK) >> DISP_ELD_VER_SHIFT;
    if (ver != 2 && ver != 31)
        return (disp_u8_t *)DISP_NULL;

    mnl = disp_eld_mnl(eld);
    if (mnl > 16)
        return (disp_u8_t *)DISP_NULL;

    return eld + DISP_ELD_CEA_SAD(mnl, 0);
}

/**
 * disp_eld_sad_count - Get ELD SAD count.
 * @eld: pointer to an eld memory structure with sad_count set
 */
static inline disp_s32_t disp_eld_sad_count(disp_u8_t *eld)
{
    return (eld[DISP_ELD_SAD_COUNT_CONN_TYPE] & DISP_ELD_SAD_COUNT_MASK) >>
        DISP_ELD_SAD_COUNT_SHIFT;
}

/**
 * disp_eld_calc_baseline_block_size - Calculate baseline block size in bytes
 * @eld: pointer to an eld memory structure with mnl and sad_count set
 *
 * This is a helper for determining the payload size of the baseline block, in
 * bytes, for e.g. setting the Baseline_ELD_Len field in the ELD header block.
 */
static inline disp_s32_t disp_eld_calc_baseline_block_size(disp_u8_t *eld)
{
    return DISP_ELD_MONITOR_NAME_STRING - DISP_ELD_HEADER_BLOCK_SIZE +
        disp_eld_mnl(eld) + disp_eld_sad_count(eld) * 3;
}

/**
 * disp_eld_size - Get ELD size in bytes
 * @eld: pointer to a complete eld memory structure
 *
 * The returned value does not include the vendor block. It's vendor specific,
 * and comprises of the remaining bytes in the ELD memory buffer after
 * disp_eld_size() bytes of header and baseline block.
 *
 * The returned value is guaranteed to be a multiple of 4.
 */
static inline disp_s32_t disp_eld_size(disp_u8_t *eld)
{
    return DISP_ELD_HEADER_BLOCK_SIZE + eld[DISP_ELD_BASELINE_ELD_LEN] * 4;
}

/**
 * disp_eld_get_spk_alloc - Get speaker allocation
 * @eld: pointer to an ELD memory structure
 *
 * The returned value is the speakers mask. User has to use %DISP_ELD_SPEAKER
 * field definitions to identify speakers.
 */
static inline disp_u8_t disp_eld_get_spk_alloc(disp_u8_t *eld)
{
    return eld[DISP_ELD_SPEAKER] & DISP_ELD_SPEAKER_MASK;
}

/**
 * disp_eld_get_conn_type - Get device type hdmi/dp connected
 * @eld: pointer to an ELD memory structure
 *
 * The caller need to use %DISP_ELD_CONN_TYPE_HDMI or %DISP_ELD_CONN_TYPE_DP to
 * identify the display type connected.
 */
static inline disp_u8_t disp_eld_get_conn_type(disp_u8_t *eld)
{
    return eld[DISP_ELD_SAD_COUNT_CONN_TYPE] & DISP_ELD_CONN_TYPE_MASK;
}

disp_s32_t disp_add_edid_info(pdisp_connector_t pdisp_connector, pedid_t edid);
disp_s32_t disp_add_edid_modes(pdisp_connector_t pdisp_connector, pedid_t edid, disp_u8_t add_mode_to_modelist);
disp_s32_t disp_add_edid_modes_extension(pdisp_connector_t pdisp_connector, pedid_t edid);

disp_u8_t disp_match_cea_mode(pdisp_display_mode_t to_match);
disp_u8_t disp_detect_hdmi_monitor(pedid_t edid);
disp_u8_t disp_detect_monitor_audio(pedid_t edid);
HDMI_QUANTIZATION_RANGE_TYPE disp_default_rgb_quant_range(pdisp_display_mode_t mode);

disp_s32_t disp_edid_header_is_valid(disp_u8_t *raw_edid);
disp_u8_t disp_edid_block_valid(disp_u8_t *raw_edid, disp_u32_t block);
disp_u8_t disp_edid_is_valid(pedid_t edid);
disp_void_t disp_edid_get_monitor_name(pedid_t edid, pdisp_s8_t name,
                   disp_s32_t buflen);
disp_u8_t disp_mode_find_dmt(disp_s32_t hsize, disp_s32_t vsize, disp_s32_t fresh, disp_u8_t rb, pdisp_display_mode_t pmode);
pdisp_display_mode_t 
disp_display_mode_from_cea_vic(
                  disp_u8_t video_code);
disp_u8_t *disp_find_edid_extension(pedid_t edid,
                  disp_s32_t ext_id, disp_s32_t *ext_index);
disp_u8_t disp_edid_block_checksum(disp_u8_t *raw_edid);

#endif
