/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#ifndef __DISP_HDMI_ENCODER_H__
#define __DISP_HDMI_ENCODER_H__

#include "display.h"
#include "disp_encoder.h"

//include/linux/hdmi.h
typedef enum _HDMI_PACKET_TYPE 
{
    HDMI_PACKET_TYPE_NULL = 0x00,
    HDMI_PACKET_TYPE_AUDIO_CLOCK_REGEN = 0x01,
    HDMI_PACKET_TYPE_AUDIO_SAMPLE = 0x02,
    HDMI_PACKET_TYPE_GENERAL_CONTROL = 0x03,
    HDMI_PACKET_TYPE_ACP = 0x04,
    HDMI_PACKET_TYPE_ISRC1 = 0x05,
    HDMI_PACKET_TYPE_ISRC2 = 0x06,
    HDMI_PACKET_TYPE_ONE_BIT_AUDIO_SAMPLE = 0x07,
    HDMI_PACKET_TYPE_DST_AUDIO = 0x08,
    HDMI_PACKET_TYPE_HBR_AUDIO_STREAM = 0x09,
    HDMI_PACKET_TYPE_GAMUT_METADATA = 0x0a,
    /* + HDMI_INFOFRAME_TYPE */
}HDMI_PACKET_TYPE;

typedef enum _HDMI_INFOFRAME_TYPE 
{
    HDMI_INFOFRAME_TYPE_VENDOR = 0x81,
    HDMI_INFOFRAME_TYPE_AVI = 0x82,
    HDMI_INFOFRAME_TYPE_SPD = 0x83,
    HDMI_INFOFRAME_TYPE_AUDIO = 0x84,
    HDMI_INFOFRAME_TYPE_MPEG = 0x85,
    HDMI_INFOFRAME_TYPE_VBI = 0x86, // NTSC VBI
    HDMI_INFOFRAME_TYPE_DRM = 0x87,
}HDMI_INFOFRAME_TYPE;

#define HDMI_IEEE_OUI 0x000c03
#define HDMI_FORUM_IEEE_OUI 0xc45dd8
#define HDMI_INFOFRAME_HEADER_SIZE  4 // header + checksum
#define HDMI_AVI_INFOFRAME_VER      2 // hdmi2.0 ver2, CEA-861-G ver4
#define HDMI_AVI_INFOFRAME_SIZE    13 // ver2/ver3 - 13, ver4 - 14
#define HDMI_SPD_INFOFRAME_VER      1
#define HDMI_SPD_INFOFRAME_SIZE    25
#define HDMI_AUDIO_INFOFRAME_VER    1
#define HDMI_AUDIO_INFOFRAME_SIZE  10
#define HDMI_DRM_INFOFRAME_VER      1
#define HDMI_DRM_INFOFRAME_SIZE    26
#define HDMI_VENDOR_INFOFRAME_VER   1
#define HDMI_VENDOR_INFOFRAME_SIZE  4
#define HDMI_VENDOR_INFOFRAME_FORUM_VER 1

#define HDMI_INFOFRAME_SIZE(type)    \
    (HDMI_INFOFRAME_HEADER_SIZE + HDMI_ ## type ## _INFOFRAME_SIZE)

typedef struct _hdmi_any_infoframe_t
{
    HDMI_INFOFRAME_TYPE type;
    disp_u8_t version;
    disp_u8_t length;
}hdmi_any_infoframe_t, *phdmi_any_infoframe_t;

typedef enum _HDMI_COLORSPACE_TYPE 
{
    HDMI_COLORSPACE_RGB,
    HDMI_COLORSPACE_YUV422,
    HDMI_COLORSPACE_YUV444,
    HDMI_COLORSPACE_YUV420,
    HDMI_COLORSPACE_RESERVED4,
    HDMI_COLORSPACE_RESERVED5,
    HDMI_COLORSPACE_RESERVED6,
    HDMI_COLORSPACE_IDO_DEFINED,
}HDMI_COLORSPACE_TYPE;

typedef enum _HDMI_COLORIMETRY_TYPE 
{
    HDMI_COLORIMETRY_NONE,
    HDMI_COLORIMETRY_ITU_601,
    HDMI_COLORIMETRY_ITU_709,
    HDMI_COLORIMETRY_EXTENDED,
}HDMI_COLORIMETRY_TYPE;

typedef enum _HDMI_EXTENDED_COLORIMETRY_TYPE 
{
    HDMI_EXTENDED_COLORIMETRY_XV_YCC_601 = 0,
    HDMI_EXTENDED_COLORIMETRY_XV_YCC_709,
    HDMI_EXTENDED_COLORIMETRY_S_YCC_601,
    HDMI_EXTENDED_COLORIMETRY_OPYCC_601,    // AdobeYCC601
    HDMI_EXTENDED_COLORIMETRY_OPRGB,        // AdobeRGB

    /* The following EC values are only defined in CEA-861-F. */
    HDMI_EXTENDED_COLORIMETRY_BT2020_CONST_LUM,         // ITU-R BT.2020 Y'cC'bcC'rc
    HDMI_EXTENDED_COLORIMETRY_BT2020,                   // ITU-R BT.2020 R’G’B’  or  ITU-R BT.2020 Y'C'bC'r
    HDMI_EXTENDED_COLORIMETRY_ADDITIONAL_EXTENSION = 7, // Reserved for RGB additional extension

    HDMI_EXTENDED_COLORIMETRY_ADDITIONAL_D65,       // Extension: DCI-P3 R’G’B’ (D65)
    HDMI_EXTENDED_COLORIMETRY_ADDITIONAL_THEATER,   // Extension: DCI-P3 R’G’B’ (theater) origin DCI-P3
}HDMI_EXTENDED_COLORIMETRY_TYPE;

typedef enum _HDMI_BAR_DATA_PRESENT_TYPE 
{
    HDMI_BAR_DATA_PRESENT_NONE,
    HDMI_BAR_DATA_PRESENT_VERT,
    HDMI_BAR_DATA_PRESENT_HORIZ,
    HDMI_BAR_DATA_PRESENT_BOTH,
}HDMI_BAR_DATA_PRESENT_TYPE;

typedef enum _HDMI_SCAN_MODE_TYPE 
{
    HDMI_SCAN_MODE_NONE,
    HDMI_SCAN_MODE_OVERSCAN,
    HDMI_SCAN_MODE_UNDERSCAN,
    HDMI_SCAN_MODE_RESERVED,
}HDMI_SCAN_MODE_TYPE;

typedef enum _HDMI_PICTURE_ASPECT_TYPE 
{
    HDMI_PICTURE_ASPECT_NONE,
    HDMI_PICTURE_ASPECT_4_3,
    HDMI_PICTURE_ASPECT_16_9,
    HDMI_PICTURE_ASPECT_64_27,
    HDMI_PICTURE_ASPECT_256_135,
    HDMI_PICTURE_ASPECT_RESERVED,
}HDMI_PICTURE_ASPECT_TYPE;

typedef enum _HDMI_ACTIVE_ASPECT_TYPE 
{
    HDMI_ACTIVE_ASPECT_16_9_TOP = 2,
    HDMI_ACTIVE_ASPECT_14_9_TOP = 3,
    HDMI_ACTIVE_ASPECT_16_9_CENTER = 4,
    HDMI_ACTIVE_ASPECT_PICTURE = 8,
    HDMI_ACTIVE_ASPECT_4_3 = 9,
    HDMI_ACTIVE_ASPECT_16_9 = 10,
    HDMI_ACTIVE_ASPECT_14_9 = 11,
    HDMI_ACTIVE_ASPECT_4_3_SP_14_9 = 13,
    HDMI_ACTIVE_ASPECT_16_9_SP_14_9 = 14,
    HDMI_ACTIVE_ASPECT_16_9_SP_4_3 = 15,
}HDMI_ACTIVE_ASPECT_TYPE;

typedef enum _HDMI_QUANTIZATION_RANGE_TYPE 
{
    HDMI_QUANTIZATION_RANGE_DEFAULT,
    HDMI_QUANTIZATION_RANGE_LIMITED,
    HDMI_QUANTIZATION_RANGE_FULL,
    HDMI_QUANTIZATION_RANGE_RESERVED,
}HDMI_QUANTIZATION_RANGE_TYPE;

/* non-uniform picture scaling */
typedef enum _HDMI_NUPS_TYPE 
{
    HDMI_NUPS_UNKNOWN,
    HDMI_NUPS_HORIZONTAL,
    HDMI_NUPS_VERTICAL,
    HDMI_NUPS_BOTH,
}HDMI_NUPS_TYPE;

typedef enum _HDMI_YCC_QUANTIZATION_RANGE_TYPE 
{
    HDMI_YCC_QUANTIZATION_RANGE_LIMITED,
    HDMI_YCC_QUANTIZATION_RANGE_FULL,
    HDMI_YCC_QUANTIZATION_RANGE_Reserved,
}HDMI_YCC_QUANTIZATION_RANGE_TYPE;

typedef enum _HDMI_CONTENT_TYPE 
{
    HDMI_CONTENT_TYPE_GRAPHICS,
    HDMI_CONTENT_TYPE_PHOTO,
    HDMI_CONTENT_TYPE_CINEMA,
    HDMI_CONTENT_TYPE_GAME,
}HDMI_CONTENT_TYPE;

typedef struct _hdmi_avi_infoframe_t
{
    HDMI_INFOFRAME_TYPE type;
    disp_u8_t version;
    disp_u8_t length;
    HDMI_COLORSPACE_TYPE colorspace;
    disp_u8_t active_aspect_valid;
    HDMI_BAR_DATA_PRESENT_TYPE bar_type;
    HDMI_SCAN_MODE_TYPE scan_mode;
    HDMI_COLORIMETRY_TYPE colorimetry;
    HDMI_PICTURE_ASPECT_TYPE picture_aspect;
    HDMI_ACTIVE_ASPECT_TYPE active_aspect;
    disp_u8_t itc;
    HDMI_EXTENDED_COLORIMETRY_TYPE extended_colorimetry;
    HDMI_QUANTIZATION_RANGE_TYPE quantization_range;
    HDMI_NUPS_TYPE nups;
    disp_u8_t video_code;
    HDMI_YCC_QUANTIZATION_RANGE_TYPE ycc_quantization_range;
    HDMI_CONTENT_TYPE content_type;
    disp_u8_t pixel_repeat; // 0-noRepeat, 1-repeat2, 2-repeat3 ...
    disp_u16_t top_bar;
    disp_u16_t bottom_bar;
    disp_u16_t left_bar;
    disp_u16_t right_bar;
}hdmi_avi_infoframe_t, *phdmi_avi_infoframe_t;

DISP_STATUS hdmi_avi_infoframe_init(phdmi_avi_infoframe_t pframe);
DISP_STATUS hdmi_avi_infoframe_pack(phdmi_avi_infoframe_t pframe, pdisp_void_t pbuffer, disp_u32_t size);
DISP_STATUS hdmi_avi_infoframe_pack_only(phdmi_avi_infoframe_t pframe, pdisp_void_t pbuffer, disp_u32_t size);
DISP_STATUS hdmi_avi_infoframe_check(phdmi_avi_infoframe_t pframe);
DISP_STATUS hdmi_avi_infoframe_unpack(phdmi_avi_infoframe_t pframe, pdisp_void_t pbuffer, disp_u32_t size);
DISP_STATUS hdmi_avi_infoframe_construct(pdisp_void_t pdisp_encoder, phdmi_avi_infoframe_t pframe);

typedef enum _HDMI_STATIC_METADATA_TYPE
{
    HDMI_STATIC_METADATA_TYPE1 = 0,
    HDMI_STATIC_METADATA_RESERVED, // 1 ~ 7
}HDMI_STATIC_METADATA_TYPE;

/* Dynamic Range and Mastering InfoFrame as per CTA 861.G spec */
typedef struct _hdmi_drm_infoframe_t
{
    HDMI_INFOFRAME_TYPE type;
    disp_u8_t version;
    disp_u8_t length;
    DISP_EOTF_TYPE eotf;
    HDMI_STATIC_METADATA_TYPE metadata_type;
    disp_u16xy_t display_primaries[3];  // ref CTA-861-G: unit 0.00002, [0x0, 0xC350] -> [0.0, 1.0]
    disp_u16xy_t white_point;           // unit 0.00002
    disp_u16_t max_display_mastering_luminance; // 1 nits
    disp_u16_t min_display_mastering_luminance; // 1/10000 nits
    disp_u16_t max_content_ligh_level;          // 1 nits
    disp_u16_t max_frame_average_light_level;   // 1 nits
}hdmi_drm_infoframe_t, *phdmi_drm_infoframe_t;

DISP_STATUS hdmi_drm_infoframe_pack(phdmi_drm_infoframe_t pframe, pdisp_void_t pbuffer, disp_u32_t size);
DISP_STATUS hdmi_drm_infoframe_unpack(phdmi_drm_infoframe_t pframe, pdisp_void_t pbuffer, disp_u32_t size);
DISP_STATUS hdmi_drm_infoframe_construct(pdisp_void_t pdisp_encoder, phdmi_drm_infoframe_t pframe);

typedef enum _HDMI_SPD_SDI_TYPE
{
    HDMI_SPD_SDI_UNKNOWN,
    HDMI_SPD_SDI_DSTB,
    HDMI_SPD_SDI_DVDP,
    HDMI_SPD_SDI_DVHS,
    HDMI_SPD_SDI_HDDVR,
    HDMI_SPD_SDI_DVC,
    HDMI_SPD_SDI_DSC,
    HDMI_SPD_SDI_VCD,
    HDMI_SPD_SDI_GAME,
    HDMI_SPD_SDI_PC,
    HDMI_SPD_SDI_BD,
    HDMI_SPD_SDI_SACD,
    HDMI_SPD_SDI_HDDVD,
    HDMI_SPD_SDI_PMP,
}HDMI_SPD_SDI_TYPE;

/* Source Product Description */
typedef struct _hdmi_spd_infoframe_t 
{
    HDMI_INFOFRAME_TYPE type;
    disp_u8_t version;
    disp_u8_t length;
    disp_s8_t vendor[8];
    disp_s8_t product[16];
    HDMI_SPD_SDI_TYPE sdi;
}hdmi_spd_infoframe_t, *phdmi_spd_infoframe_t;

DISP_STATUS hdmi_spd_infoframe_pack(phdmi_spd_infoframe_t pframe, pdisp_void_t pbuffer, disp_u32_t size);
DISP_STATUS hdmi_spd_infoframe_unpack(phdmi_spd_infoframe_t pframe, pdisp_void_t pbuffer, disp_u32_t size);
DISP_STATUS hdmi_spd_infoframe_construct(pdisp_void_t pdisp_encoder, phdmi_spd_infoframe_t pframe);

typedef enum _HDMI_AUDIO_CODING_TYPE
{
    HDMI_AUDIO_CODING_TYPE_STREAM = 0,
    HDMI_AUDIO_CODING_TYPE_PCM,
    HDMI_AUDIO_CODING_TYPE_AC3,
    HDMI_AUDIO_CODING_TYPE_MPEG1,
    HDMI_AUDIO_CODING_TYPE_MP3,
    HDMI_AUDIO_CODING_TYPE_MPEG2,
    HDMI_AUDIO_CODING_TYPE_AAC_LC,
    HDMI_AUDIO_CODING_TYPE_DTS,
    HDMI_AUDIO_CODING_TYPE_ATRAC,
    HDMI_AUDIO_CODING_TYPE_DSD,
    HDMI_AUDIO_CODING_TYPE_EAC3,
    HDMI_AUDIO_CODING_TYPE_DTS_HD,
    HDMI_AUDIO_CODING_TYPE_MLP,
    HDMI_AUDIO_CODING_TYPE_DST,
    HDMI_AUDIO_CODING_TYPE_WMA_PRO,
    HDMI_AUDIO_CODING_TYPE_CXT, // 15 - Coding Extension Type
}HDMI_AUDIO_CODING_TYPE;

typedef enum _HDMI_AUDIO_SAMPLE_SIZE_TYPE
{
    HDMI_AUDIO_SAMPLE_SIZE_STREAM,
    HDMI_AUDIO_SAMPLE_SIZE_16,
    HDMI_AUDIO_SAMPLE_SIZE_20,
    HDMI_AUDIO_SAMPLE_SIZE_24,
}HDMI_AUDIO_SAMPLE_SIZE_TYPE;

typedef enum _HDMI_AUDIO_SAMPLE_FREQUENCY_TYPE
{
    HDMI_AUDIO_SAMPLE_FREQUENCY_STREAM,
    HDMI_AUDIO_SAMPLE_FREQUENCY_32000,
    HDMI_AUDIO_SAMPLE_FREQUENCY_44100,
    HDMI_AUDIO_SAMPLE_FREQUENCY_48000,
    HDMI_AUDIO_SAMPLE_FREQUENCY_88200,
    HDMI_AUDIO_SAMPLE_FREQUENCY_96000,
    HDMI_AUDIO_SAMPLE_FREQUENCY_176400,
    HDMI_AUDIO_SAMPLE_FREQUENCY_192000,
}HDMI_AUDIO_SAMPLE_FREQUENCY_TYPE;

typedef enum _HDMI_AUDIO_CODING_TYPE_EXT_TYPE
{
    /* Refer to Audio Coding Type (CT) field in Data Byte 1 */
    HDMI_AUDIO_CODING_TYPE_EXT_CT = 0,

    /*
     * The next three CXT values are defined in CEA-861-E only.
     * They do not exist in older versions, and in CEA-861-F they are
     * defined as 'Not in use'.
     */
    HDMI_AUDIO_CODING_TYPE_EXT_HE_AAC,
    HDMI_AUDIO_CODING_TYPE_EXT_HE_AAC_V2,
    HDMI_AUDIO_CODING_TYPE_EXT_MPEG_SURROUND,

    /* The following CXT values are only defined in CEA-861-F. */
    HDMI_AUDIO_CODING_TYPE_EXT_MPEG4_HE_AAC = 4,
    HDMI_AUDIO_CODING_TYPE_EXT_MPEG4_HE_AAC_V2,
    HDMI_AUDIO_CODING_TYPE_EXT_MPEG4_AAC_LC,
    HDMI_AUDIO_CODING_TYPE_EXT_DRA,
    HDMI_AUDIO_CODING_TYPE_EXT_MPEG4_HE_AAC_SURROUND,
    HDMI_AUDIO_CODING_TYPE_EXT_MPEG4_AAC_LC_SURROUND = 10,
    HDMI_AUDIO_CODING_TYPE_EXT_MPEG_H_3D_AUDIO,
    HDMI_AUDIO_CODING_TYPE_EXT_AC_4,
    HDMI_AUDIO_CODING_TYPE_EXT_L_PCM_3D_AUDIO,

    HDMI_AUDIO_CODING_TYPE_EXT_RESERVED // 14 15 reserved
}HDMI_AUDIO_CODING_TYPE_EXT_TYPE;

typedef enum _HDMI_LFE_PLAYBACK_LEVEL_TYPE
{
    HDMI_LFE_PLAYBACK_LEVEL_UNKNOW = 0,
    HDMI_LFE_PLAYBACK_LEVEL_0,      // 0 dB playback
    HDMI_LFE_PLAYBACK_LEVEL_10,     // 10+ dB playback
    HDMI_LFE_PLAYBACK_LEVEL_RESERVED,
}HDMI_LFE_PLAYBACK_LEVEL_TYPE;

typedef struct _hdmi_audio_infoframe_t
{
    HDMI_INFOFRAME_TYPE type;
    disp_u8_t version;
    disp_u8_t length;
    disp_u8_t channels; // val = channel + 1, 0 - stream
    HDMI_AUDIO_CODING_TYPE coding_type;
    HDMI_AUDIO_SAMPLE_SIZE_TYPE sample_size;
    HDMI_AUDIO_SAMPLE_FREQUENCY_TYPE sample_frequency;
    HDMI_AUDIO_CODING_TYPE_EXT_TYPE coding_type_ext;
    disp_u8_t channel_allocation; /* CEA-861-G: Table 35 */
    disp_u8_t level_shift_value;  // 0 ~ 15 db
    disp_u8_t downmix_inhibit;
    HDMI_LFE_PLAYBACK_LEVEL_TYPE lfe_playback_level;
}hdmi_audio_infoframe_t, *phdmi_audio_infoframe_t;

DISP_STATUS hdmi_audio_infoframe_init(phdmi_audio_infoframe_t pframe);
DISP_STATUS hdmi_audio_infoframe_pack(phdmi_audio_infoframe_t pframe, pdisp_void_t pbuffer, disp_u32_t size);
DISP_STATUS hdmi_audio_infoframe_unpack(phdmi_audio_infoframe_t pframe, pdisp_void_t pbuffer, disp_u32_t size);
DISP_STATUS hdmi_audio_infoframe_construct(pdisp_void_t pdisp_encoder, phdmi_audio_infoframe_t pframe);

typedef enum _HDMI_VIDEO_FORMAT_TYPE
{
    HDMI_VIDEO_FORMAT_NO_ADDITION = 0,
    HDMI_VIDEO_FORMAT_EXTENDED_RESOLUTION,
    HDMI_VIDEO_FORMAT_3D_STRUCTURE,
    HDMI_VIDEO_FORMAT_RESERVED,
}HDMI_VIDEO_FORMAT_TYPE;

// vendor 3D
#define HDMI_3D_STRUCTURE_FLAG_3D_VALID         (1<<0)
#define HDMI_3D_STRUCTURE_FLAG_ADDITION         (1<<1)
#define HDMI_3D_STRUCTURE_FLAG_DISPARITY        (1<<2)
#define HDMI_3D_STRUCTURE_FLAG_META             (1<<3)
#define HDMI_3D_STRUCTURE_FLAG_3D_DUAL_VIEW     (1<<4)

typedef enum _HDMI_3D_STRUCTURE_TYPE
{
    HDMI_3D_STRUCTURE_INVALID = -1,
    HDMI_3D_STRUCTURE_FRAME_PACKING = 0,
    HDMI_3D_STRUCTURE_FIELD_ALTERNATIVE,
    HDMI_3D_STRUCTURE_LINE_ALTERNATIVE,
    HDMI_3D_STRUCTURE_SIDE_BY_SIDE_FULL,
    HDMI_3D_STRUCTURE_L_DEPTH,
    HDMI_3D_STRUCTURE_L_DEPTH_GFX_GFX_DEPTH,
    HDMI_3D_STRUCTURE_TOP_AND_BOTTOM,
    HDMI_3D_STRUCTURE_RESERVED_1,
    HDMI_3D_STRUCTURE_SIDE_BY_SIDE_HALF = 8,
}HDMI_3D_STRUCTURE_TYPE;

typedef enum _HDMI_3D_VIEW_DEPENDENCY_TYPE
{
    HDMI_3D_VIEW_DEPENDENCY_NONE = 0,
    HDMI_3D_VIEW_DEPENDENCY_RIGHT, // The right view originates from an independently coded view
    HDMI_3D_VIEW_DEPENDENCY_LEFT,
    HDMI_3D_VIEW_DEPENDENCY_BOTH,
}HDMI_3D_VIEW_DEPENDENCY_TYPE;

typedef enum _HDMI_3D_VIEW_PREFERRED_2D_VIEW_TYPE
{
    HDMI_3D_VIEW_PREFERRED_2D_VIEW_NONE = 0,
    HDMI_3D_VIEW_PREFERRED_2D_VIEW_RIGHT, // Use the right 3D view for 2D viewing
    HDMI_3D_VIEW_PREFERRED_2D_VIEW_LEFT,
    HDMI_3D_VIEW_PREFERRED_2D_VIEW_NO_CARE,
}HDMI_3D_VIEW_PREFERRED_2D_VIEW_TYPE;

typedef struct _hdmi_vendor_infoframe_t
{
    HDMI_INFOFRAME_TYPE type;
    disp_u8_t version;
    disp_u8_t length;
    disp_u32_t ieee_code;

    HDMI_VIDEO_FORMAT_TYPE video_format;
    disp_u8_t hdmi_vic;
    HDMI_3D_STRUCTURE_TYPE s3d_struct;
    disp_u8_t s3d_ext_data; // 3D_Ext_Data field in hdmi spec 1.4
}hdmi_vendor_infoframe_t, *phdmi_vendor_infoframe_t;

typedef struct _hdmi_forum_vendor_infoframe_t
{
    HDMI_INFOFRAME_TYPE type;
    disp_u8_t version;
    disp_u8_t length;
    disp_u32_t ieee_code;

    disp_u8_t forum_version;
    disp_u8_t s3d_flag; // HDMI_3D_STRUCTURE_FLAG_3D_VALID ...
    disp_u8_t disparty_len;
    disp_u8_t meta_len;
    HDMI_3D_STRUCTURE_TYPE s3d_struct;
    disp_u8_t s3d_ext_data; // not actual use in spec, see hdmi 1.4 3D_Ext_Data field
    HDMI_3D_VIEW_DEPENDENCY_TYPE view_dependency;
    HDMI_3D_VIEW_PREFERRED_2D_VIEW_TYPE view_preferred;
}hdmi_forum_vendor_infoframe_t, *phdmi_forum_vendor_infoframe_t;

typedef union _hdmi_vendor_any_infoframe_t
{
    struct
    {
        HDMI_INFOFRAME_TYPE type;
        disp_u8_t version;
        disp_u8_t length;
        disp_u32_t ieee_code;
    } any;
    hdmi_vendor_infoframe_t hdmi;
    hdmi_forum_vendor_infoframe_t hdmi_forum;
}hdmi_vendor_any_infoframe_t, *phdmi_vendor_any_infoframe_t;

DISP_STATUS hdmi_vendor_infoframe_init(phdmi_vendor_infoframe_t pframe);
DISP_STATUS hdmi_vendor_infoframe_pack(phdmi_vendor_infoframe_t pframe, pdisp_void_t pbuffer, disp_u32_t size);
DISP_STATUS hdmi_vendor_infoframe_pack_only(phdmi_vendor_infoframe_t pframe, pdisp_void_t pbuffer, disp_u32_t size);
DISP_STATUS hdmi_vendor_infoframe_check(phdmi_vendor_infoframe_t pframe);
DISP_STATUS hdmi_vendor_infoframe_unpack(phdmi_vendor_infoframe_t pframe, pdisp_void_t pbuffer, disp_u32_t size);
DISP_STATUS hdmi_vendor_infoframe_construct(pdisp_void_t pdisp_encoder, phdmi_vendor_infoframe_t pframe);
DISP_STATUS hdmi_forum_vendor_infoframe_pack(phdmi_forum_vendor_infoframe_t pframe, pdisp_void_t pbuffer, disp_u32_t size);
DISP_STATUS hdmi_forum_vendor_infoframe_unpack(phdmi_forum_vendor_infoframe_t pframe, pdisp_void_t pbuffer, disp_u32_t size);
DISP_STATUS hdmi_forum_vendor_infoframe_construct(pdisp_void_t pdisp_encoder, phdmi_forum_vendor_infoframe_t pframe);

/**
 * union hdmi_infoframe - overall union of all abstract infoframe representations
 * @any: generic infoframe
 * @avi: avi infoframe
 * @spd: spd infoframe
 * @vendor: union of all vendor infoframes
 * @audio: audio infoframe
 * @drm: Dynamic Range and Mastering infoframe
 *
 * This is used by the generic pack function. This works since all infoframes
 * have the same header which also indicates which type of infoframe should be
 * packed.
 */
typedef union _hdmi_infoframe_t
{
    hdmi_any_infoframe_t any;
    hdmi_avi_infoframe_t avi;
    hdmi_spd_infoframe_t spd;
    hdmi_vendor_any_infoframe_t vendor;
    hdmi_audio_infoframe_t audio;
    hdmi_drm_infoframe_t drm;
}hdmi_infoframe_t, *phdmi_infoframe_t;

DISP_STATUS hdmi_infoframe_pack(phdmi_infoframe_t pframe, pdisp_void_t pbuffer, disp_u32_t size);
DISP_STATUS hdmi_infoframe_pack_only(phdmi_infoframe_t pframe, pdisp_void_t pbuffer, disp_u32_t size);
DISP_STATUS hdmi_infoframe_check(phdmi_infoframe_t pframe);
DISP_STATUS hdmi_infoframe_unpack(phdmi_infoframe_t pframe, pdisp_void_t pbuffer, disp_u32_t size);
DISP_STATUS hdmi_infoframe_construct(pdisp_void_t pdisp_encoder, phdmi_infoframe_t pframe);
DISP_STATUS hdmi_infoframe_set(pdisp_void_t pbuffer, disp_u32_t size);

DISP_STATUS hdmi_encoder_set_infoframe(pdisp_encoder_t pdisp_encoder, DISP_POWER_STATE state);
disp_void_t disp_hdmi_encoder_sw_init(pdisp_void_t pdisp_encoder);

#endif
