/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#ifndef __DISP_DP_AUX_H__
#define __DISP_DP_AUX_H__

#include "display.h"
#include "disp_edid.h"

/**
 * this dpcd defination duplicated from linux kernel(version 5.17), drm_dp_helper.h 
*/

/* MSA (Main Stream Attribute) MISC bits (as MISC1<<8|MISC0) */
#define DP_MSA_MISC_SYNC_CLOCK            (1 << 0)
#define DP_MSA_MISC_INTERLACE_VTOTAL_EVEN    (1 << 8)
#define DP_MSA_MISC_STEREO_NO_3D        (0 << 9)
#define DP_MSA_MISC_STEREO_PROG_RIGHT_EYE    (1 << 9)
#define DP_MSA_MISC_STEREO_PROG_LEFT_EYE    (3 << 9)
/* bits per component for non-RAW */
#define DP_MSA_MISC_6_BPC            (0 << 5)
#define DP_MSA_MISC_8_BPC            (1 << 5)
#define DP_MSA_MISC_10_BPC            (2 << 5)
#define DP_MSA_MISC_12_BPC            (3 << 5)
#define DP_MSA_MISC_16_BPC            (4 << 5)
/* bits per component for RAW */
#define DP_MSA_MISC_RAW_6_BPC            (1 << 5)
#define DP_MSA_MISC_RAW_7_BPC            (2 << 5)
#define DP_MSA_MISC_RAW_8_BPC            (3 << 5)
#define DP_MSA_MISC_RAW_10_BPC            (4 << 5)
#define DP_MSA_MISC_RAW_12_BPC            (5 << 5)
#define DP_MSA_MISC_RAW_14_BPC            (6 << 5)
#define DP_MSA_MISC_RAW_16_BPC            (7 << 5)
/* pixel encoding/colorimetry format */
#define _DP_MSA_MISC_COLOR(misc1_7, misc0_21, misc0_3, misc0_4) \
    ((misc1_7) << 15 | (misc0_4) << 4 | (misc0_3) << 3 | ((misc0_21) << 1))
#define DP_MSA_MISC_COLOR_RGB            _DP_MSA_MISC_COLOR(0, 0, 0, 0)
#define DP_MSA_MISC_COLOR_CEA_RGB        _DP_MSA_MISC_COLOR(0, 0, 1, 0)
#define DP_MSA_MISC_COLOR_RGB_WIDE_FIXED    _DP_MSA_MISC_COLOR(0, 3, 0, 0)
#define DP_MSA_MISC_COLOR_RGB_WIDE_FLOAT    _DP_MSA_MISC_COLOR(0, 3, 0, 1)
#define DP_MSA_MISC_COLOR_Y_ONLY        _DP_MSA_MISC_COLOR(1, 0, 0, 0)
#define DP_MSA_MISC_COLOR_RAW            _DP_MSA_MISC_COLOR(1, 1, 0, 0)
#define DP_MSA_MISC_COLOR_YCBCR_422_BT601    _DP_MSA_MISC_COLOR(0, 1, 1, 0)
#define DP_MSA_MISC_COLOR_YCBCR_422_BT709    _DP_MSA_MISC_COLOR(0, 1, 1, 1)
#define DP_MSA_MISC_COLOR_YCBCR_444_BT601    _DP_MSA_MISC_COLOR(0, 2, 1, 0)
#define DP_MSA_MISC_COLOR_YCBCR_444_BT709    _DP_MSA_MISC_COLOR(0, 2, 1, 1)
#define DP_MSA_MISC_COLOR_XVYCC_422_BT601    _DP_MSA_MISC_COLOR(0, 1, 0, 0)
#define DP_MSA_MISC_COLOR_XVYCC_422_BT709    _DP_MSA_MISC_COLOR(0, 1, 0, 1)
#define DP_MSA_MISC_COLOR_XVYCC_444_BT601    _DP_MSA_MISC_COLOR(0, 2, 0, 0)
#define DP_MSA_MISC_COLOR_XVYCC_444_BT709    _DP_MSA_MISC_COLOR(0, 2, 0, 1)
#define DP_MSA_MISC_COLOR_OPRGB            _DP_MSA_MISC_COLOR(0, 0, 1, 1)
#define DP_MSA_MISC_COLOR_DCI_P3        _DP_MSA_MISC_COLOR(0, 3, 1, 0)
#define DP_MSA_MISC_COLOR_COLOR_PROFILE        _DP_MSA_MISC_COLOR(0, 3, 1, 1)
#define DP_MSA_MISC_COLOR_VSC_SDP        (1 << 14)

#define DP_AUX_MAX_PAYLOAD_BYTES    16

#define DP_AUX_I2C_WRITE        0x0
#define DP_AUX_I2C_READ            0x1
#define DP_AUX_I2C_WRITE_STATUS_UPDATE    0x2
#define DP_AUX_I2C_MOT            0x4
#define DP_AUX_NATIVE_WRITE        0x8
#define DP_AUX_NATIVE_READ        0x9

#define DP_AUX_NATIVE_REPLY_ACK        (0x0 << 0)
#define DP_AUX_NATIVE_REPLY_NACK    (0x1 << 0)
#define DP_AUX_NATIVE_REPLY_DEFER    (0x2 << 0)
#define DP_AUX_NATIVE_REPLY_MASK    (0x3 << 0)

#define DP_AUX_I2C_REPLY_ACK        (0x0 << 2)
#define DP_AUX_I2C_REPLY_NACK        (0x1 << 2)
#define DP_AUX_I2C_REPLY_DEFER        (0x2 << 2)
#define DP_AUX_I2C_REPLY_MASK        (0x3 << 2)

#define DP_AUX_HW_REPLY_ACK         (0x0 << 4)
#define DP_AUX_HW_REPLY_TIMEOUT     (0x1 << 4)
#define DP_AUX_HW_REPLY_BUSY        (0x2 << 4)
#define DP_AUX_HW_REPLY_MASK        (0x3 << 4)

/* DPCD Field Address Mapping */

/* Receiver Capability */
#define DP_DPCD_REV                         0x000
# define DP_DPCD_REV_10                     0x10
# define DP_DPCD_REV_11                     0x11
# define DP_DPCD_REV_12                     0x12
# define DP_DPCD_REV_13                     0x13
# define DP_DPCD_REV_14                     0x14

#define DP_MAX_LINK_RATE                    0x001

#define DP_MAX_LANE_COUNT                   0x002
# define DP_MAX_LANE_COUNT_MASK            0x1f
# define DP_TPS3_SUPPORTED            (1 << 6) /* 1.2 */
# define DP_ENHANCED_FRAME_CAP            (1 << 7)

#define DP_MAX_DOWNSPREAD                   0x003
# define DP_MAX_DOWNSPREAD_0_5            (1 << 0)
# define DP_STREAM_REGENERATION_STATUS_CAP  (1 << 1) /* 2.0 */
# define DP_NO_AUX_HANDSHAKE_LINK_TRAINING  (1 << 6)
# define DP_TPS4_SUPPORTED                  (1 << 7)

#define DP_NORP                             0x004

#define DP_DOWNSTREAMPORT_PRESENT           0x005
# define DP_DWN_STRM_PORT_PRESENT           (1 << 0)
# define DP_DWN_STRM_PORT_TYPE_MASK         0x06
# define DP_DWN_STRM_PORT_TYPE_DP           (0 << 1)
# define DP_DWN_STRM_PORT_TYPE_ANALOG       (1 << 1)
# define DP_DWN_STRM_PORT_TYPE_TMDS         (2 << 1)
# define DP_DWN_STRM_PORT_TYPE_OTHER        (3 << 1)
# define DP_FORMAT_CONVERSION               (1 << 3)
# define DP_DETAILED_CAP_INFO_AVAILABLE        (1 << 4) /* DPI */

#define DP_MAIN_LINK_CHANNEL_CODING         0x006
# define DP_CAP_ANSI_8B10B            (1 << 0)
# define DP_CAP_ANSI_128B132B               (1 << 1) /* 2.0 */

#define DP_DOWN_STREAM_PORT_COUNT        0x007
# define DP_PORT_COUNT_MASK            0x0f
# define DP_MSA_TIMING_PAR_IGNORED        (1 << 6) /* eDP */
# define DP_OUI_SUPPORT                (1 << 7)

#define DP_RECEIVE_PORT_0_CAP_0            0x008
# define DP_LOCAL_EDID_PRESENT            (1 << 1)
# define DP_ASSOCIATED_TO_PRECEDING_PORT    (1 << 2)

#define DP_RECEIVE_PORT_0_BUFFER_SIZE        0x009

#define DP_RECEIVE_PORT_1_CAP_0            0x00a
#define DP_RECEIVE_PORT_1_BUFFER_SIZE       0x00b

#define DP_I2C_SPEED_CAP            0x00c    /* DPI */
# define DP_I2C_SPEED_1K            0x01
# define DP_I2C_SPEED_5K            0x02
# define DP_I2C_SPEED_10K            0x04
# define DP_I2C_SPEED_100K            0x08
# define DP_I2C_SPEED_400K            0x10
# define DP_I2C_SPEED_1M            0x20

#define DP_EDP_CONFIGURATION_CAP            0x00d   /* XXX 1.2? */
# define DP_ALTERNATE_SCRAMBLER_RESET_CAP   (1 << 0)
# define DP_FRAMING_CHANGE_CAP            (1 << 1)
# define DP_DPCD_DISPLAY_CONTROL_CAPABLE     (1 << 3) /* edp v1.2 or higher */

#define DP_TRAINING_AUX_RD_INTERVAL             0x00e   /* XXX 1.2? */
# define DP_TRAINING_AUX_RD_MASK                0x7F    /* DP 1.3 */
# define DP_EXTENDED_RECEIVER_CAP_FIELD_PRESENT    (1 << 7) /* DP 1.3 */

#define DP_ADAPTER_CAP                0x00f   /* 1.2 */
# define DP_FORCE_LOAD_SENSE_CAP        (1 << 0)
# define DP_ALTERNATE_I2C_PATTERN_CAP        (1 << 1)

#define DP_SUPPORTED_LINK_RATES            0x010 /* eDP 1.4 */
# define DP_MAX_SUPPORTED_RATES             8        /* 16-bit little-endian */

/* Multiple stream transport */
#define DP_FAUX_CAP                0x020   /* 1.2 */
# define DP_FAUX_CAP_1                (1 << 0)

#define DP_SINK_VIDEO_FALLBACK_FORMATS      0x020   /* 2.0 */
# define DP_FALLBACK_1024x768_60HZ_24BPP    (1 << 0)
# define DP_FALLBACK_1280x720_60HZ_24BPP    (1 << 1)
# define DP_FALLBACK_1920x1080_60HZ_24BPP   (1 << 2)

#define DP_MSTM_CAP                0x021   /* 1.2 */
# define DP_MST_CAP                (1 << 0)
# define DP_SINGLE_STREAM_SIDEBAND_MSG      (1 << 1) /* 2.0 */

#define DP_NUMBER_OF_AUDIO_ENDPOINTS        0x022   /* 1.2 */

/* AV_SYNC_DATA_BLOCK                                  1.2 */
#define DP_AV_GRANULARITY            0x023
# define DP_AG_FACTOR_MASK            (0xf << 0)
# define DP_AG_FACTOR_3MS            (0 << 0)
# define DP_AG_FACTOR_2MS            (1 << 0)
# define DP_AG_FACTOR_1MS            (2 << 0)
# define DP_AG_FACTOR_500US            (3 << 0)
# define DP_AG_FACTOR_200US            (4 << 0)
# define DP_AG_FACTOR_100US            (5 << 0)
# define DP_AG_FACTOR_10US            (6 << 0)
# define DP_AG_FACTOR_1US            (7 << 0)
# define DP_VG_FACTOR_MASK            (0xf << 4)
# define DP_VG_FACTOR_3MS            (0 << 4)
# define DP_VG_FACTOR_2MS            (1 << 4)
# define DP_VG_FACTOR_1MS            (2 << 4)
# define DP_VG_FACTOR_500US            (3 << 4)
# define DP_VG_FACTOR_200US            (4 << 4)
# define DP_VG_FACTOR_100US            (5 << 4)

#define DP_AUD_DEC_LAT0                0x024
#define DP_AUD_DEC_LAT1                0x025

#define DP_AUD_PP_LAT0                0x026
#define DP_AUD_PP_LAT1                0x027

#define DP_VID_INTER_LAT            0x028

#define DP_VID_PROG_LAT                0x029

#define DP_REP_LAT                0x02a

#define DP_AUD_DEL_INS0                0x02b
#define DP_AUD_DEL_INS1                0x02c
#define DP_AUD_DEL_INS2                0x02d
/* End of AV_SYNC_DATA_BLOCK */

#define DP_RECEIVER_ALPM_CAP            0x02e   /* eDP 1.4 */
# define DP_ALPM_CAP                (1 << 0)

#define DP_SINK_DEVICE_AUX_FRAME_SYNC_CAP   0x02f   /* eDP 1.4 */
# define DP_AUX_FRAME_SYNC_CAP            (1 << 0)

#define DP_GUID                    0x030   /* 1.2 */

#define DP_DSC_SUPPORT                      0x060   /* DP 1.4 */
# define DP_DSC_DECOMPRESSION_IS_SUPPORTED  (1 << 0)

#define DP_DSC_REV                          0x061
# define DP_DSC_MAJOR_MASK                  (0xf << 0)
# define DP_DSC_MINOR_MASK                  (0xf << 4)
# define DP_DSC_MAJOR_SHIFT                 0
# define DP_DSC_MINOR_SHIFT                 4

#define DP_DSC_RC_BUF_BLK_SIZE              0x062
# define DP_DSC_RC_BUF_BLK_SIZE_1           0x0
# define DP_DSC_RC_BUF_BLK_SIZE_4           0x1
# define DP_DSC_RC_BUF_BLK_SIZE_16          0x2
# define DP_DSC_RC_BUF_BLK_SIZE_64          0x3

#define DP_DSC_RC_BUF_SIZE                  0x063

#define DP_DSC_SLICE_CAP_1                  0x064
# define DP_DSC_1_PER_DP_DSC_SINK           (1 << 0)
# define DP_DSC_2_PER_DP_DSC_SINK           (1 << 1)
# define DP_DSC_4_PER_DP_DSC_SINK           (1 << 3)
# define DP_DSC_6_PER_DP_DSC_SINK           (1 << 4)
# define DP_DSC_8_PER_DP_DSC_SINK           (1 << 5)
# define DP_DSC_10_PER_DP_DSC_SINK          (1 << 6)
# define DP_DSC_12_PER_DP_DSC_SINK          (1 << 7)

#define DP_DSC_LINE_BUF_BIT_DEPTH           0x065
# define DP_DSC_LINE_BUF_BIT_DEPTH_MASK     (0xf << 0)
# define DP_DSC_LINE_BUF_BIT_DEPTH_9        0x0
# define DP_DSC_LINE_BUF_BIT_DEPTH_10       0x1
# define DP_DSC_LINE_BUF_BIT_DEPTH_11       0x2
# define DP_DSC_LINE_BUF_BIT_DEPTH_12       0x3
# define DP_DSC_LINE_BUF_BIT_DEPTH_13       0x4
# define DP_DSC_LINE_BUF_BIT_DEPTH_14       0x5
# define DP_DSC_LINE_BUF_BIT_DEPTH_15       0x6
# define DP_DSC_LINE_BUF_BIT_DEPTH_16       0x7
# define DP_DSC_LINE_BUF_BIT_DEPTH_8        0x8

#define DP_DSC_BLK_PREDICTION_SUPPORT       0x066
# define DP_DSC_BLK_PREDICTION_IS_SUPPORTED (1 << 0)

#define DP_DSC_MAX_BITS_PER_PIXEL_LOW       0x067   /* eDP 1.4 */

#define DP_DSC_MAX_BITS_PER_PIXEL_HI        0x068   /* eDP 1.4 */
# define DP_DSC_MAX_BITS_PER_PIXEL_HI_MASK  (0x3 << 0)
# define DP_DSC_MAX_BITS_PER_PIXEL_HI_SHIFT 8

#define DP_DSC_DEC_COLOR_FORMAT_CAP         0x069
# define DP_DSC_RGB                         (1 << 0)
# define DP_DSC_YCbCr444                    (1 << 1)
# define DP_DSC_YCbCr422_Simple             (1 << 2)
# define DP_DSC_YCbCr422_Native             (1 << 3)
# define DP_DSC_YCbCr420_Native             (1 << 4)

#define DP_DSC_DEC_COLOR_DEPTH_CAP          0x06A
# define DP_DSC_8_BPC                       (1 << 1)
# define DP_DSC_10_BPC                      (1 << 2)
# define DP_DSC_12_BPC                      (1 << 3)

#define DP_DSC_PEAK_THROUGHPUT              0x06B
# define DP_DSC_THROUGHPUT_MODE_0_MASK      (0xf << 0)
# define DP_DSC_THROUGHPUT_MODE_0_SHIFT     0
# define DP_DSC_THROUGHPUT_MODE_0_UNSUPPORTED 0
# define DP_DSC_THROUGHPUT_MODE_0_340       (1 << 0)
# define DP_DSC_THROUGHPUT_MODE_0_400       (2 << 0)
# define DP_DSC_THROUGHPUT_MODE_0_450       (3 << 0)
# define DP_DSC_THROUGHPUT_MODE_0_500       (4 << 0)
# define DP_DSC_THROUGHPUT_MODE_0_550       (5 << 0)
# define DP_DSC_THROUGHPUT_MODE_0_600       (6 << 0)
# define DP_DSC_THROUGHPUT_MODE_0_650       (7 << 0)
# define DP_DSC_THROUGHPUT_MODE_0_700       (8 << 0)
# define DP_DSC_THROUGHPUT_MODE_0_750       (9 << 0)
# define DP_DSC_THROUGHPUT_MODE_0_800       (10 << 0)
# define DP_DSC_THROUGHPUT_MODE_0_850       (11 << 0)
# define DP_DSC_THROUGHPUT_MODE_0_900       (12 << 0)
# define DP_DSC_THROUGHPUT_MODE_0_950       (13 << 0)
# define DP_DSC_THROUGHPUT_MODE_0_1000      (14 << 0)
# define DP_DSC_THROUGHPUT_MODE_0_170       (15 << 0) /* 1.4a */
# define DP_DSC_THROUGHPUT_MODE_1_MASK      (0xf << 4)
# define DP_DSC_THROUGHPUT_MODE_1_SHIFT     4
# define DP_DSC_THROUGHPUT_MODE_1_UNSUPPORTED 0
# define DP_DSC_THROUGHPUT_MODE_1_340       (1 << 4)
# define DP_DSC_THROUGHPUT_MODE_1_400       (2 << 4)
# define DP_DSC_THROUGHPUT_MODE_1_450       (3 << 4)
# define DP_DSC_THROUGHPUT_MODE_1_500       (4 << 4)
# define DP_DSC_THROUGHPUT_MODE_1_550       (5 << 4)
# define DP_DSC_THROUGHPUT_MODE_1_600       (6 << 4)
# define DP_DSC_THROUGHPUT_MODE_1_650       (7 << 4)
# define DP_DSC_THROUGHPUT_MODE_1_700       (8 << 4)
# define DP_DSC_THROUGHPUT_MODE_1_750       (9 << 4)
# define DP_DSC_THROUGHPUT_MODE_1_800       (10 << 4)
# define DP_DSC_THROUGHPUT_MODE_1_850       (11 << 4)
# define DP_DSC_THROUGHPUT_MODE_1_900       (12 << 4)
# define DP_DSC_THROUGHPUT_MODE_1_950       (13 << 4)
# define DP_DSC_THROUGHPUT_MODE_1_1000      (14 << 4)
# define DP_DSC_THROUGHPUT_MODE_1_170       (15 << 4)

#define DP_DSC_MAX_SLICE_WIDTH              0x06C
#define DP_DSC_MIN_SLICE_WIDTH_VALUE        2560
#define DP_DSC_SLICE_WIDTH_MULTIPLIER       320

#define DP_DSC_SLICE_CAP_2                  0x06D
# define DP_DSC_16_PER_DP_DSC_SINK          (1 << 0)
# define DP_DSC_20_PER_DP_DSC_SINK          (1 << 1)
# define DP_DSC_24_PER_DP_DSC_SINK          (1 << 2)

#define DP_DSC_BITS_PER_PIXEL_INC           0x06F
# define DP_DSC_BITS_PER_PIXEL_INC_MASK     0x7
# define DP_DSC_BITS_PER_PIXEL_1_16         0x0
# define DP_DSC_BITS_PER_PIXEL_1_8          0x1
# define DP_DSC_BITS_PER_PIXEL_1_4          0x2
# define DP_DSC_BITS_PER_PIXEL_1_2          0x3
# define DP_DSC_BITS_PER_PIXEL_1            0x4

#define DP_PSR_SUPPORT                      0x070   /* XXX 1.2? */
# define DP_PSR_IS_SUPPORTED                1
# define DP_PSR2_IS_SUPPORTED            2        /* eDP 1.4 */
# define DP_PSR2_WITH_Y_COORD_IS_SUPPORTED  3        /* eDP 1.4a */

#define DP_PSR_CAPS                         0x071   /* XXX 1.2? */
# define DP_PSR_NO_TRAIN_ON_EXIT            1
# define DP_PSR_SETUP_TIME_330              (0 << 1)
# define DP_PSR_SETUP_TIME_275              (1 << 1)
# define DP_PSR_SETUP_TIME_220              (2 << 1)
# define DP_PSR_SETUP_TIME_165              (3 << 1)
# define DP_PSR_SETUP_TIME_110              (4 << 1)
# define DP_PSR_SETUP_TIME_55               (5 << 1)
# define DP_PSR_SETUP_TIME_0                (6 << 1)
# define DP_PSR_SETUP_TIME_MASK             (7 << 1)
# define DP_PSR_SETUP_TIME_SHIFT            1
# define DP_PSR2_SU_Y_COORDINATE_REQUIRED   (1 << 4)  /* eDP 1.4a */
# define DP_PSR2_SU_GRANULARITY_REQUIRED    (1 << 5)  /* eDP 1.4b */

#define DP_PSR2_SU_X_GRANULARITY        0x072 /* eDP 1.4b */
#define DP_PSR2_SU_Y_GRANULARITY        0x074 /* eDP 1.4b */

/*
 * 0x80-0x8f describe downstream port capabilities, but there are two layouts
 * based on whether DP_DETAILED_CAP_INFO_AVAILABLE was set.  If it was not,
 * each port's descriptor is one byte wide.  If it was set, each port's is
 * four bytes wide, starting with the one byte from the base info.  As of
 * DP interop v1.1a only VGA defines additional detail.
 */

/* offset 0 */
#define DP_DOWNSTREAM_PORT_0            0x80
# define DP_DS_PORT_TYPE_MASK            (7 << 0)
# define DP_DS_PORT_TYPE_DP            0
# define DP_DS_PORT_TYPE_VGA            1
# define DP_DS_PORT_TYPE_DVI            2
# define DP_DS_PORT_TYPE_HDMI            3
# define DP_DS_PORT_TYPE_NON_EDID        4
# define DP_DS_PORT_TYPE_DP_DUALMODE        5
# define DP_DS_PORT_TYPE_WIRELESS           6
# define DP_DS_PORT_HPD                (1 << 3)
# define DP_DS_NON_EDID_MASK            (0xf << 4)
# define DP_DS_NON_EDID_720x480i_60        (1 << 4)
# define DP_DS_NON_EDID_720x480i_50        (2 << 4)
# define DP_DS_NON_EDID_1920x1080i_60        (3 << 4)
# define DP_DS_NON_EDID_1920x1080i_50        (4 << 4)
# define DP_DS_NON_EDID_1280x720_60        (5 << 4)
# define DP_DS_NON_EDID_1280x720_50        (7 << 4)
/* offset 1 for VGA is maximum megapixels per second / 8 */
/* offset 1 for DVI/HDMI is maximum TMDS clock in Mbps / 2.5 */
/* offset 2 for VGA/DVI/HDMI */
# define DP_DS_MAX_BPC_MASK                (3 << 0)
# define DP_DS_8BPC                    0
# define DP_DS_10BPC                    1
# define DP_DS_12BPC                    2
# define DP_DS_16BPC                    3
/* HDMI2.1 PCON FRL CONFIGURATION */
# define DP_PCON_MAX_FRL_BW                 (7 << 2)
# define DP_PCON_MAX_0GBPS                  (0 << 2)
# define DP_PCON_MAX_9GBPS                  (1 << 2)
# define DP_PCON_MAX_18GBPS                 (2 << 2)
# define DP_PCON_MAX_24GBPS                 (3 << 2)
# define DP_PCON_MAX_32GBPS                 (4 << 2)
# define DP_PCON_MAX_40GBPS                 (5 << 2)
# define DP_PCON_MAX_48GBPS                 (6 << 2)
# define DP_PCON_SOURCE_CTL_MODE            (1 << 5)

/* offset 3 for DVI */
# define DP_DS_DVI_DUAL_LINK            (1 << 1)
# define DP_DS_DVI_HIGH_COLOR_DEPTH        (1 << 2)
/* offset 3 for HDMI */
# define DP_DS_HDMI_FRAME_SEQ_TO_FRAME_PACK (1 << 0)
# define DP_DS_HDMI_YCBCR422_PASS_THROUGH   (1 << 1)
# define DP_DS_HDMI_YCBCR420_PASS_THROUGH   (1 << 2)
# define DP_DS_HDMI_YCBCR444_TO_422_CONV    (1 << 3)
# define DP_DS_HDMI_YCBCR444_TO_420_CONV    (1 << 4)

/*
 * VESA DP-to-HDMI PCON Specification adds caps for colorspace
 * conversion in DFP cap DPCD 83h. Sec6.1 Table-3.
 * Based on the available support the source can enable
 * color conversion by writing into PROTOCOL_COVERTER_CONTROL_2
 * DPCD 3052h.
 */
# define DP_DS_HDMI_BT601_RGB_YCBCR_CONV    (1 << 5)
# define DP_DS_HDMI_BT709_RGB_YCBCR_CONV    (1 << 6)
# define DP_DS_HDMI_BT2020_RGB_YCBCR_CONV   (1 << 7)

#define DP_MAX_DOWNSTREAM_PORTS            0x10

/* DP Forward error Correction Registers */
#define DP_FEC_CAPABILITY            0x090    /* 1.4 */
# define DP_FEC_CAPABLE                (1 << 0)
# define DP_FEC_UNCORR_BLK_ERROR_COUNT_CAP  (1 << 1)
# define DP_FEC_CORR_BLK_ERROR_COUNT_CAP    (1 << 2)
# define DP_FEC_BIT_ERROR_COUNT_CAP        (1 << 3)
#define DP_FEC_CAPABILITY_1            0x091   /* 2.0 */

/**
 * remove DP-HDMI2.1 PCON 
*/

/* DP Extended DSC Capabilities */
#define DP_DSC_BRANCH_OVERALL_THROUGHPUT_0  0x0a0   /* DP 1.4a SCR */
#define DP_DSC_BRANCH_OVERALL_THROUGHPUT_1  0x0a1
#define DP_DSC_BRANCH_MAX_LINE_WIDTH        0x0a2

/* DFP Capability Extension */
#define DP_DFP_CAPABILITY_EXTENSION_SUPPORT    0x0a3    /* 2.0 */

/* Link Configuration */
#define    DP_LINK_BW_SET                    0x100
# define DP_LINK_RATE_TABLE            0x00    /* eDP 1.4 */
# define DP_LINK_BW_1_62            0x06
# define DP_LINK_BW_2_7                0x0a
# define DP_LINK_BW_5_4                0x14    /* 1.2 */
# define DP_LINK_BW_8_1                0x1e    /* 1.4 */
# define DP_LINK_BW_10                      0x01    /* 2.0 128b/132b Link Layer */
# define DP_LINK_BW_13_5                    0x04    /* 2.0 128b/132b Link Layer */
# define DP_LINK_BW_20                      0x02    /* 2.0 128b/132b Link Layer */

#define DP_LANE_COUNT_SET                0x101
# define DP_LANE_COUNT_MASK            0x0f
# define DP_LANE_COUNT_ENHANCED_FRAME_EN    (1 << 7)

#define DP_TRAINING_PATTERN_SET                0x102
# define DP_TRAINING_PATTERN_DISABLE        0
# define DP_TRAINING_PATTERN_1            1
# define DP_TRAINING_PATTERN_2            2
# define DP_TRAINING_PATTERN_3            3        /* 1.2 */
# define DP_TRAINING_PATTERN_4              7       /* 1.4 */
# define DP_TRAINING_PATTERN_MASK        0x3
# define DP_TRAINING_PATTERN_MASK_1_4        0xf

/* DPCD 1.1 only. For DPCD >= 1.2 see per-lane DP_LINK_QUAL_LANEn_SET */
# define DP_LINK_QUAL_PATTERN_11_DISABLE    (0 << 2)
# define DP_LINK_QUAL_PATTERN_11_D10_2        (1 << 2)
# define DP_LINK_QUAL_PATTERN_11_ERROR_RATE (2 << 2)
# define DP_LINK_QUAL_PATTERN_11_PRBS7        (3 << 2)
# define DP_LINK_QUAL_PATTERN_11_MASK        (3 << 2)

# define DP_RECOVERED_CLOCK_OUT_EN        (1 << 4)
# define DP_LINK_SCRAMBLING_DISABLE        (1 << 5)

# define DP_SYMBOL_ERROR_COUNT_BOTH        (0 << 6)
# define DP_SYMBOL_ERROR_COUNT_DISPARITY    (1 << 6)
# define DP_SYMBOL_ERROR_COUNT_SYMBOL        (2 << 6)
# define DP_SYMBOL_ERROR_COUNT_MASK        (3 << 6)

#define DP_TRAINING_LANE0_SET            0x103
#define DP_TRAINING_LANE1_SET            0x104
#define DP_TRAINING_LANE2_SET            0x105
#define DP_TRAINING_LANE3_SET            0x106

# define DP_TRAIN_VOLTAGE_SWING_MASK        0x3
# define DP_TRAIN_VOLTAGE_SWING_SHIFT        0
# define DP_TRAIN_MAX_SWING_REACHED        (1 << 2)
# define DP_TRAIN_VOLTAGE_SWING_LEVEL_0 (0 << 0)
# define DP_TRAIN_VOLTAGE_SWING_LEVEL_1 (1 << 0)
# define DP_TRAIN_VOLTAGE_SWING_LEVEL_2 (2 << 0)
# define DP_TRAIN_VOLTAGE_SWING_LEVEL_3 (3 << 0)

# define DP_TRAIN_PRE_EMPHASIS_MASK        (3 << 3)
# define DP_TRAIN_PRE_EMPH_LEVEL_0        (0 << 3)
# define DP_TRAIN_PRE_EMPH_LEVEL_1        (1 << 3)
# define DP_TRAIN_PRE_EMPH_LEVEL_2        (2 << 3)
# define DP_TRAIN_PRE_EMPH_LEVEL_3        (3 << 3)

# define DP_TRAIN_PRE_EMPHASIS_SHIFT        3
# define DP_TRAIN_MAX_PRE_EMPHASIS_REACHED  (1 << 5)

# define DP_TX_FFE_PRESET_VALUE_MASK        (0xf << 0) /* 2.0 128b/132b Link Layer */

#define DP_DOWNSPREAD_CTRL            0x107
# define DP_SPREAD_AMP_0_5            (1 << 4)
# define DP_MSA_TIMING_PAR_IGNORE_EN        (1 << 7) /* eDP */

#define DP_MAIN_LINK_CHANNEL_CODING_SET        0x108
# define DP_SET_ANSI_8B10B            (1 << 0)
# define DP_SET_ANSI_128B132B               (1 << 1)

#define DP_I2C_SPEED_CONTROL_STATUS        0x109   /* DPI */
/* bitmask as for DP_I2C_SPEED_CAP */

#define DP_EDP_CONFIGURATION_SET            0x10a   /* XXX 1.2? */
# define DP_ALTERNATE_SCRAMBLER_RESET_ENABLE (1 << 0)
# define DP_FRAMING_CHANGE_ENABLE        (1 << 1)
# define DP_PANEL_SELF_TEST_ENABLE        (1 << 7)

#define DP_LINK_QUAL_LANE0_SET            0x10b   /* DPCD >= 1.2 */
#define DP_LINK_QUAL_LANE1_SET            0x10c
#define DP_LINK_QUAL_LANE2_SET            0x10d
#define DP_LINK_QUAL_LANE3_SET            0x10e
# define DP_LINK_QUAL_PATTERN_DISABLE        0
# define DP_LINK_QUAL_PATTERN_D10_2        1
# define DP_LINK_QUAL_PATTERN_ERROR_RATE    2
# define DP_LINK_QUAL_PATTERN_PRBS7        3
# define DP_LINK_QUAL_PATTERN_80BIT_CUSTOM  4
# define DP_LINK_QUAL_PATTERN_CP2520_PAT_1  5
# define DP_LINK_QUAL_PATTERN_CP2520_PAT_2  6
# define DP_LINK_QUAL_PATTERN_CP2520_PAT_3  7
/* DP 2.0 UHBR10, UHBR13.5, UHBR20 */
# define DP_LINK_QUAL_PATTERN_128B132B_TPS1 0x08
# define DP_LINK_QUAL_PATTERN_128B132B_TPS2 0x10
# define DP_LINK_QUAL_PATTERN_PRSBS9        0x18
# define DP_LINK_QUAL_PATTERN_PRSBS11       0x20
# define DP_LINK_QUAL_PATTERN_PRSBS15       0x28
# define DP_LINK_QUAL_PATTERN_PRSBS23       0x30
# define DP_LINK_QUAL_PATTERN_PRSBS31       0x38
# define DP_LINK_QUAL_PATTERN_CUSTOM        0x40
# define DP_LINK_QUAL_PATTERN_SQUARE        0x48

#define DP_TRAINING_LANE0_1_SET2        0x10f
#define DP_TRAINING_LANE2_3_SET2        0x110
# define DP_LANE02_POST_CURSOR2_SET_MASK    (3 << 0)
# define DP_LANE02_MAX_POST_CURSOR2_REACHED (1 << 2)
# define DP_LANE13_POST_CURSOR2_SET_MASK    (3 << 4)
# define DP_LANE13_MAX_POST_CURSOR2_REACHED (1 << 6)

#define DP_MSTM_CTRL                0x111   /* 1.2 */
# define DP_MST_EN                (1 << 0)
# define DP_UP_REQ_EN                (1 << 1)
# define DP_UPSTREAM_IS_SRC            (1 << 2)

#define DP_AUDIO_DELAY0                0x112   /* 1.2 */
#define DP_AUDIO_DELAY1                0x113
#define DP_AUDIO_DELAY2                0x114

#define DP_LINK_RATE_SET            0x115   /* eDP 1.4 */
# define DP_LINK_RATE_SET_SHIFT            0
# define DP_LINK_RATE_SET_MASK            (7 << 0)

#define DP_RECEIVER_ALPM_CONFIG            0x116   /* eDP 1.4 */
# define DP_ALPM_ENABLE                (1 << 0)
# define DP_ALPM_LOCK_ERROR_IRQ_HPD_ENABLE  (1 << 1)

#define DP_SINK_DEVICE_AUX_FRAME_SYNC_CONF  0x117   /* eDP 1.4 */
# define DP_AUX_FRAME_SYNC_ENABLE        (1 << 0)
# define DP_IRQ_HPD_ENABLE            (1 << 1)

#define DP_UPSTREAM_DEVICE_DP_PWR_NEED        0x118   /* 1.2 */
# define DP_PWR_NOT_NEEDED            (1 << 0)

#define DP_FEC_CONFIGURATION            0x120    /* 1.4 */
# define DP_FEC_READY                (1 << 0)
# define DP_FEC_ERR_COUNT_SEL_MASK        (7 << 1)
# define DP_FEC_ERR_COUNT_DIS            (0 << 1)
# define DP_FEC_UNCORR_BLK_ERROR_COUNT        (1 << 1)
# define DP_FEC_CORR_BLK_ERROR_COUNT        (2 << 1)
# define DP_FEC_BIT_ERROR_COUNT            (3 << 1)
# define DP_FEC_LANE_SELECT_MASK        (3 << 4)
# define DP_FEC_LANE_0_SELECT            (0 << 4)
# define DP_FEC_LANE_1_SELECT            (1 << 4)
# define DP_FEC_LANE_2_SELECT            (2 << 4)
# define DP_FEC_LANE_3_SELECT            (3 << 4)

#define DP_AUX_FRAME_SYNC_VALUE            0x15c   /* eDP 1.4 */
# define DP_AUX_FRAME_SYNC_VALID        (1 << 0)

#define DP_DSC_ENABLE                       0x160   /* DP 1.4 */
# define DP_DECOMPRESSION_EN                (1 << 0)
#define DP_DSC_CONFIGURATION                0x161    /* DP 2.0 */

#define DP_PSR_EN_CFG                0x170   /* XXX 1.2? */
# define DP_PSR_ENABLE                BIT(0)
# define DP_PSR_MAIN_LINK_ACTIVE        BIT(1)
# define DP_PSR_CRC_VERIFICATION        BIT(2)
# define DP_PSR_FRAME_CAPTURE            BIT(3)
# define DP_PSR_SU_REGION_SCANLINE_CAPTURE    BIT(4) /* eDP 1.4a */
# define DP_PSR_IRQ_HPD_WITH_CRC_ERRORS        BIT(5) /* eDP 1.4a */
# define DP_PSR_ENABLE_PSR2            BIT(6) /* eDP 1.4a */

#define DP_ADAPTER_CTRL                0x1a0
# define DP_ADAPTER_CTRL_FORCE_LOAD_SENSE   (1 << 0)

#define DP_BRANCH_DEVICE_CTRL            0x1a1
# define DP_BRANCH_DEVICE_IRQ_HPD        (1 << 0)

#define DP_PAYLOAD_ALLOCATE_SET            0x1c0
#define DP_PAYLOAD_ALLOCATE_START_TIME_SLOT 0x1c1
#define DP_PAYLOAD_ALLOCATE_TIME_SLOT_COUNT 0x1c2

/* Link/Sink Device Status */
#define DP_SINK_COUNT                0x200
/* prior to 1.2 bit 7 was reserved mbz */
# define DP_GET_SINK_COUNT(x)            ((((x) & 0x80) >> 1) | ((x) & 0x3f))
# define DP_SINK_CP_READY            (1 << 6)

#define DP_DEVICE_SERVICE_IRQ_VECTOR        0x201
# define DP_REMOTE_CONTROL_COMMAND_PENDING  (1 << 0)
# define DP_AUTOMATED_TEST_REQUEST        (1 << 1)
# define DP_CP_IRQ                (1 << 2)
# define DP_MCCS_IRQ                (1 << 3)
# define DP_DOWN_REP_MSG_RDY            (1 << 4) /* 1.2 MST */
# define DP_UP_REQ_MSG_RDY            (1 << 5) /* 1.2 MST */
# define DP_SINK_SPECIFIC_IRQ            (1 << 6)

#define DP_LANE0_1_STATUS            0x202
#define DP_LANE2_3_STATUS            0x203
# define DP_LANE_CR_DONE            (1 << 0)
# define DP_LANE_CHANNEL_EQ_DONE        (1 << 1)
# define DP_LANE_SYMBOL_LOCKED            (1 << 2)

#define DP_CHANNEL_EQ_BITS (DP_LANE_CR_DONE |        \
                DP_LANE_CHANNEL_EQ_DONE |    \
                DP_LANE_SYMBOL_LOCKED)

#define DP_LANE_ALIGN_STATUS_UPDATED        0x204

#define DP_INTERLANE_ALIGN_DONE            (1 << 0)
#define DP_POST_LT_ADJ_REQ_IN_PROGRESS     (1 << 1)
#define DP_DOWNSTREAM_PORT_STATUS_CHANGED   (1 << 6)
#define DP_LINK_STATUS_UPDATED            (1 << 7)

#define DP_SINK_STATUS                0x205
# define DP_RECEIVE_PORT_0_STATUS        (1 << 0)
# define DP_RECEIVE_PORT_1_STATUS        (1 << 1)
# define DP_STREAM_REGENERATION_STATUS      (1 << 2) /* 2.0 */
# define DP_INTRA_HOP_AUX_REPLY_INDICATION    (1 << 3) /* 2.0 */

#define DP_ADJUST_REQUEST_LANE0_1        0x206
#define DP_ADJUST_REQUEST_LANE2_3        0x207
# define DP_ADJUST_VOLTAGE_SWING_LANE0_MASK  0x03
# define DP_ADJUST_VOLTAGE_SWING_LANE0_SHIFT 0
# define DP_ADJUST_PRE_EMPHASIS_LANE0_MASK   0x0c
# define DP_ADJUST_PRE_EMPHASIS_LANE0_SHIFT  2
# define DP_ADJUST_VOLTAGE_SWING_LANE1_MASK  0x30
# define DP_ADJUST_VOLTAGE_SWING_LANE1_SHIFT 4
# define DP_ADJUST_PRE_EMPHASIS_LANE1_MASK   0xc0
# define DP_ADJUST_PRE_EMPHASIS_LANE1_SHIFT  6

/* DP 2.0 128b/132b Link Layer */
# define DP_ADJUST_TX_FFE_PRESET_LANE0_MASK  (0xf << 0)
# define DP_ADJUST_TX_FFE_PRESET_LANE0_SHIFT 0
# define DP_ADJUST_TX_FFE_PRESET_LANE1_MASK  (0xf << 4)
# define DP_ADJUST_TX_FFE_PRESET_LANE1_SHIFT 4

#define DP_ADJUST_REQUEST_POST_CURSOR2      0x20c
# define DP_ADJUST_POST_CURSOR2_LANE0_MASK  0x03
# define DP_ADJUST_POST_CURSOR2_LANE0_SHIFT 0
# define DP_ADJUST_POST_CURSOR2_LANE1_MASK  0x0c
# define DP_ADJUST_POST_CURSOR2_LANE1_SHIFT 2
# define DP_ADJUST_POST_CURSOR2_LANE2_MASK  0x30
# define DP_ADJUST_POST_CURSOR2_LANE2_SHIFT 4
# define DP_ADJUST_POST_CURSOR2_LANE3_MASK  0xc0
# define DP_ADJUST_POST_CURSOR2_LANE3_SHIFT 6

#define DP_TEST_REQUEST                0x218
# define DP_TEST_LINK_TRAINING            (1 << 0)
# define DP_TEST_LINK_VIDEO_PATTERN        (1 << 1)
# define DP_TEST_LINK_EDID_READ            (1 << 2)
# define DP_TEST_LINK_PHY_TEST_PATTERN        (1 << 3) /* DPCD >= 1.1 */
# define DP_TEST_LINK_FAUX_PATTERN        (1 << 4) /* DPCD >= 1.2 */
# define DP_TEST_LINK_AUDIO_PATTERN         (1 << 5) /* DPCD >= 1.2 */
# define DP_TEST_LINK_AUDIO_DISABLED_VIDEO  (1 << 6) /* DPCD >= 1.2 */

#define DP_TEST_LINK_RATE            0x219
# define DP_LINK_RATE_162            (0x6)
# define DP_LINK_RATE_27            (0xa)

#define DP_TEST_LANE_COUNT            0x220

#define DP_TEST_PATTERN                0x221
# define DP_NO_TEST_PATTERN                 0x0
# define DP_COLOR_RAMP                      0x1
# define DP_BLACK_AND_WHITE_VERTICAL_LINES  0x2
# define DP_COLOR_SQUARE                    0x3

#define DP_TEST_H_TOTAL_HI                  0x222
#define DP_TEST_H_TOTAL_LO                  0x223

#define DP_TEST_V_TOTAL_HI                  0x224
#define DP_TEST_V_TOTAL_LO                  0x225

#define DP_TEST_H_START_HI                  0x226
#define DP_TEST_H_START_LO                  0x227

#define DP_TEST_V_START_HI                  0x228
#define DP_TEST_V_START_LO                  0x229

#define DP_TEST_HSYNC_HI                    0x22A
# define DP_TEST_HSYNC_POLARITY             (1 << 7)
# define DP_TEST_HSYNC_WIDTH_HI_MASK        (127 << 0)
#define DP_TEST_HSYNC_WIDTH_LO              0x22B

#define DP_TEST_VSYNC_HI                    0x22C
# define DP_TEST_VSYNC_POLARITY             (1 << 7)
# define DP_TEST_VSYNC_WIDTH_HI_MASK        (127 << 0)
#define DP_TEST_VSYNC_WIDTH_LO              0x22D

#define DP_TEST_H_WIDTH_HI                  0x22E
#define DP_TEST_H_WIDTH_LO                  0x22F

#define DP_TEST_V_HEIGHT_HI                 0x230
#define DP_TEST_V_HEIGHT_LO                 0x231

#define DP_TEST_MISC0                       0x232
# define DP_TEST_SYNC_CLOCK                 (1 << 0)
# define DP_TEST_COLOR_FORMAT_MASK          (3 << 1)
# define DP_TEST_COLOR_FORMAT_SHIFT         1
# define DP_COLOR_FORMAT_RGB                (0 << 1)
# define DP_COLOR_FORMAT_YCbCr422           (1 << 1)
# define DP_COLOR_FORMAT_YCbCr444           (2 << 1)
# define DP_TEST_DYNAMIC_RANGE_VESA         (0 << 3)
# define DP_TEST_DYNAMIC_RANGE_CEA          (1 << 3)
# define DP_TEST_YCBCR_COEFFICIENTS         (1 << 4)
# define DP_YCBCR_COEFFICIENTS_ITU601       (0 << 4)
# define DP_YCBCR_COEFFICIENTS_ITU709       (1 << 4)
# define DP_TEST_BIT_DEPTH_MASK             (7 << 5)
# define DP_TEST_BIT_DEPTH_SHIFT            5
# define DP_TEST_BIT_DEPTH_6                (0 << 5)
# define DP_TEST_BIT_DEPTH_8                (1 << 5)
# define DP_TEST_BIT_DEPTH_10               (2 << 5)
# define DP_TEST_BIT_DEPTH_12               (3 << 5)
# define DP_TEST_BIT_DEPTH_16               (4 << 5)

#define DP_TEST_MISC1                       0x233
# define DP_TEST_REFRESH_DENOMINATOR        (1 << 0)
# define DP_TEST_INTERLACED                 (1 << 1)

#define DP_TEST_REFRESH_RATE_NUMERATOR      0x234

#define DP_TEST_MISC0                       0x232

#define DP_TEST_CRC_R_CR            0x240
#define DP_TEST_CRC_G_Y                0x242
#define DP_TEST_CRC_B_CB            0x244

#define DP_TEST_SINK_MISC            0x246
# define DP_TEST_CRC_SUPPORTED            (1 << 5)
# define DP_TEST_COUNT_MASK            0xf

#define DP_PHY_TEST_PATTERN                 0x248
# define DP_PHY_TEST_PATTERN_SEL_MASK       0x7
# define DP_PHY_TEST_PATTERN_NONE           0x0
# define DP_PHY_TEST_PATTERN_D10_2          0x1
# define DP_PHY_TEST_PATTERN_ERROR_COUNT    0x2
# define DP_PHY_TEST_PATTERN_PRBS7          0x3
# define DP_PHY_TEST_PATTERN_80BIT_CUSTOM   0x4
# define DP_PHY_TEST_PATTERN_CP2520         0x5

#define DP_PHY_SQUARE_PATTERN                0x249

#define DP_TEST_HBR2_SCRAMBLER_RESET        0x24A
#define DP_TEST_80BIT_CUSTOM_PATTERN_7_0    0x250
#define    DP_TEST_80BIT_CUSTOM_PATTERN_15_8   0x251
#define    DP_TEST_80BIT_CUSTOM_PATTERN_23_16  0x252
#define    DP_TEST_80BIT_CUSTOM_PATTERN_31_24  0x253
#define    DP_TEST_80BIT_CUSTOM_PATTERN_39_32  0x254
#define    DP_TEST_80BIT_CUSTOM_PATTERN_47_40  0x255
#define    DP_TEST_80BIT_CUSTOM_PATTERN_55_48  0x256
#define    DP_TEST_80BIT_CUSTOM_PATTERN_63_56  0x257
#define    DP_TEST_80BIT_CUSTOM_PATTERN_71_64  0x258
#define    DP_TEST_80BIT_CUSTOM_PATTERN_79_72  0x259

#define DP_TEST_RESPONSE            0x260
# define DP_TEST_ACK                (1 << 0)
# define DP_TEST_NAK                (1 << 1)
# define DP_TEST_EDID_CHECKSUM_WRITE        (1 << 2)

#define DP_TEST_EDID_CHECKSUM            0x261

#define DP_TEST_SINK                0x270
# define DP_TEST_SINK_START            (1 << 0)
#define DP_TEST_AUDIO_MODE            0x271
#define DP_TEST_AUDIO_PATTERN_TYPE        0x272
#define DP_TEST_AUDIO_PERIOD_CH1        0x273
#define DP_TEST_AUDIO_PERIOD_CH2        0x274
#define DP_TEST_AUDIO_PERIOD_CH3        0x275
#define DP_TEST_AUDIO_PERIOD_CH4        0x276
#define DP_TEST_AUDIO_PERIOD_CH5        0x277
#define DP_TEST_AUDIO_PERIOD_CH6        0x278
#define DP_TEST_AUDIO_PERIOD_CH7        0x279
#define DP_TEST_AUDIO_PERIOD_CH8        0x27A

#define DP_FEC_STATUS                0x280    /* 1.4 */
# define DP_FEC_DECODE_EN_DETECTED        (1 << 0)
# define DP_FEC_DECODE_DIS_DETECTED        (1 << 1)

#define DP_FEC_ERROR_COUNT_LSB            0x0281    /* 1.4 */

#define DP_FEC_ERROR_COUNT_MSB            0x0282    /* 1.4 */
# define DP_FEC_ERROR_COUNT_MASK        0x7F
# define DP_FEC_ERR_COUNT_VALID            (1 << 7)

#define DP_PAYLOAD_TABLE_UPDATE_STATUS      0x2c0   /* 1.2 MST */
# define DP_PAYLOAD_TABLE_UPDATED           (1 << 0)
# define DP_PAYLOAD_ACT_HANDLED             (1 << 1)

#define DP_VC_PAYLOAD_ID_SLOT_1             0x2c1   /* 1.2 MST */
/* up to ID_SLOT_63 at 0x2ff */

/* Source Device-specific */
#define DP_SOURCE_OUI                0x300

/* Sink Device-specific */
#define DP_SINK_OUI                0x400

/* Branch Device-specific */
#define DP_BRANCH_OUI                0x500
#define DP_BRANCH_ID                        0x503
#define DP_BRANCH_REVISION_START            0x509
#define DP_BRANCH_HW_REV                    0x509
#define DP_BRANCH_SW_REV                    0x50A

/* Link/Sink Device Power Control */
#define DP_SET_POWER                        0x600
# define DP_SET_POWER_D0                    0x1
# define DP_SET_POWER_D3                    0x2
# define DP_SET_POWER_MASK                  0x3
# define DP_SET_POWER_D3_AUX_ON             0x5

/* eDP-specific */
#define DP_EDP_DPCD_REV                0x700    /* eDP 1.2 */
# define DP_EDP_11                0x00
# define DP_EDP_12                0x01
# define DP_EDP_13                0x02
# define DP_EDP_14                0x03
# define DP_EDP_14a                         0x04    /* eDP 1.4a */
# define DP_EDP_14b                         0x05    /* eDP 1.4b */

#define DP_EDP_GENERAL_CAP_1            0x701
# define DP_EDP_TCON_BACKLIGHT_ADJUSTMENT_CAP        (1 << 0)
# define DP_EDP_BACKLIGHT_PIN_ENABLE_CAP        (1 << 1)
# define DP_EDP_BACKLIGHT_AUX_ENABLE_CAP        (1 << 2)
# define DP_EDP_PANEL_SELF_TEST_PIN_ENABLE_CAP        (1 << 3)
# define DP_EDP_PANEL_SELF_TEST_AUX_ENABLE_CAP        (1 << 4)
# define DP_EDP_FRC_ENABLE_CAP                (1 << 5)
# define DP_EDP_COLOR_ENGINE_CAP            (1 << 6)
# define DP_EDP_SET_POWER_CAP                (1 << 7)

#define DP_EDP_BACKLIGHT_ADJUSTMENT_CAP     0x702
# define DP_EDP_BACKLIGHT_BRIGHTNESS_PWM_PIN_CAP    (1 << 0)
# define DP_EDP_BACKLIGHT_BRIGHTNESS_AUX_SET_CAP    (1 << 1)
# define DP_EDP_BACKLIGHT_BRIGHTNESS_BYTE_COUNT        (1 << 2)
# define DP_EDP_BACKLIGHT_AUX_PWM_PRODUCT_CAP        (1 << 3)
# define DP_EDP_BACKLIGHT_FREQ_PWM_PIN_PASSTHRU_CAP    (1 << 4)
# define DP_EDP_BACKLIGHT_FREQ_AUX_SET_CAP        (1 << 5)
# define DP_EDP_DYNAMIC_BACKLIGHT_CAP            (1 << 6)
# define DP_EDP_VBLANK_BACKLIGHT_UPDATE_CAP        (1 << 7)

#define DP_EDP_GENERAL_CAP_2            0x703
# define DP_EDP_OVERDRIVE_ENGINE_ENABLED        (1 << 0)

#define DP_EDP_GENERAL_CAP_3            0x704    /* eDP 1.4 */
# define DP_EDP_X_REGION_CAP_MASK            (0xf << 0)
# define DP_EDP_X_REGION_CAP_SHIFT            0
# define DP_EDP_Y_REGION_CAP_MASK            (0xf << 4)
# define DP_EDP_Y_REGION_CAP_SHIFT            4

#define DP_EDP_DISPLAY_CONTROL_REGISTER     0x720
# define DP_EDP_BACKLIGHT_ENABLE            (1 << 0)
# define DP_EDP_BLACK_VIDEO_ENABLE            (1 << 1)
# define DP_EDP_FRC_ENABLE                (1 << 2)
# define DP_EDP_COLOR_ENGINE_ENABLE            (1 << 3)
# define DP_EDP_VBLANK_BACKLIGHT_UPDATE_ENABLE        (1 << 7)

#define DP_EDP_BACKLIGHT_MODE_SET_REGISTER  0x721
# define DP_EDP_BACKLIGHT_CONTROL_MODE_MASK        (3 << 0)
# define DP_EDP_BACKLIGHT_CONTROL_MODE_PWM        (0 << 0)
# define DP_EDP_BACKLIGHT_CONTROL_MODE_PRESET        (1 << 0)
# define DP_EDP_BACKLIGHT_CONTROL_MODE_DPCD        (2 << 0)
# define DP_EDP_BACKLIGHT_CONTROL_MODE_PRODUCT        (3 << 0)
# define DP_EDP_BACKLIGHT_FREQ_PWM_PIN_PASSTHRU_ENABLE    (1 << 2)
# define DP_EDP_BACKLIGHT_FREQ_AUX_SET_ENABLE        (1 << 3)
# define DP_EDP_DYNAMIC_BACKLIGHT_ENABLE        (1 << 4)
# define DP_EDP_REGIONAL_BACKLIGHT_ENABLE        (1 << 5)
# define DP_EDP_UPDATE_REGION_BRIGHTNESS        (1 << 6) /* eDP 1.4 */

#define DP_EDP_BACKLIGHT_BRIGHTNESS_MSB     0x722
#define DP_EDP_BACKLIGHT_BRIGHTNESS_LSB     0x723

#define DP_EDP_PWMGEN_BIT_COUNT             0x724
#define DP_EDP_PWMGEN_BIT_COUNT_CAP_MIN     0x725
#define DP_EDP_PWMGEN_BIT_COUNT_CAP_MAX     0x726
# define DP_EDP_PWMGEN_BIT_COUNT_MASK       (0x1f << 0)

#define DP_EDP_BACKLIGHT_CONTROL_STATUS     0x727

#define DP_EDP_BACKLIGHT_FREQ_SET           0x728
# define DP_EDP_BACKLIGHT_FREQ_BASE_KHZ     27000

#define DP_EDP_BACKLIGHT_FREQ_CAP_MIN_MSB   0x72a
#define DP_EDP_BACKLIGHT_FREQ_CAP_MIN_MID   0x72b
#define DP_EDP_BACKLIGHT_FREQ_CAP_MIN_LSB   0x72c

#define DP_EDP_BACKLIGHT_FREQ_CAP_MAX_MSB   0x72d
#define DP_EDP_BACKLIGHT_FREQ_CAP_MAX_MID   0x72e
#define DP_EDP_BACKLIGHT_FREQ_CAP_MAX_LSB   0x72f

#define DP_EDP_DBC_MINIMUM_BRIGHTNESS_SET   0x732
#define DP_EDP_DBC_MAXIMUM_BRIGHTNESS_SET   0x733

#define DP_EDP_REGIONAL_BACKLIGHT_BASE      0x740    /* eDP 1.4 */
#define DP_EDP_REGIONAL_BACKLIGHT_0        0x741    /* eDP 1.4 */

#define DP_EDP_MSO_LINK_CAPABILITIES        0x7a4    /* eDP 1.4 */
# define DP_EDP_MSO_NUMBER_OF_LINKS_MASK    (7 << 0)
# define DP_EDP_MSO_NUMBER_OF_LINKS_SHIFT   0
# define DP_EDP_MSO_INDEPENDENT_LINK_BIT    (1 << 3)

/* remove Sideband MSG Buffers */

/* DPRX Event Status Indicator */
#define DP_SINK_COUNT_ESI            0x2002   /* 1.2 */
/* 0-5 sink count */
# define DP_SINK_COUNT_CP_READY             (1 << 6)

#define DP_DEVICE_SERVICE_IRQ_VECTOR_ESI0   0x2003   /* 1.2 */

#define DP_DEVICE_SERVICE_IRQ_VECTOR_ESI1   0x2004   /* 1.2 */
# define DP_RX_GTC_MSTR_REQ_STATUS_CHANGE    (1 << 0)
# define DP_LOCK_ACQUISITION_REQUEST         (1 << 1)
# define DP_CEC_IRQ                          (1 << 2)

#define DP_LINK_SERVICE_IRQ_VECTOR_ESI0     0x2005   /* 1.2 */
# define RX_CAP_CHANGED                      (1 << 0)
# define LINK_STATUS_CHANGED                 (1 << 1)
# define STREAM_STATUS_CHANGED               (1 << 2)
# define HDMI_LINK_STATUS_CHANGED            (1 << 3)
# define CONNECTED_OFF_ENTRY_REQUESTED       (1 << 4)

#define DP_PSR_ERROR_STATUS                 0x2006  /* XXX 1.2? */
# define DP_PSR_LINK_CRC_ERROR              (1 << 0)
# define DP_PSR_RFB_STORAGE_ERROR           (1 << 1)
# define DP_PSR_VSC_SDP_UNCORRECTABLE_ERROR (1 << 2) /* eDP 1.4 */

#define DP_PSR_ESI                          0x2007  /* XXX 1.2? */
# define DP_PSR_CAPS_CHANGE                 (1 << 0)

#define DP_PSR_STATUS                       0x2008  /* XXX 1.2? */
# define DP_PSR_SINK_INACTIVE               0
# define DP_PSR_SINK_ACTIVE_SRC_SYNCED      1
# define DP_PSR_SINK_ACTIVE_RFB             2
# define DP_PSR_SINK_ACTIVE_SINK_SYNCED     3
# define DP_PSR_SINK_ACTIVE_RESYNC          4
# define DP_PSR_SINK_INTERNAL_ERROR         7
# define DP_PSR_SINK_STATE_MASK             0x07

#define DP_SYNCHRONIZATION_LATENCY_IN_SINK        0x2009 /* edp 1.4 */
# define DP_MAX_RESYNC_FRAME_COUNT_MASK            (0xf << 0)
# define DP_MAX_RESYNC_FRAME_COUNT_SHIFT        0
# define DP_LAST_ACTUAL_SYNCHRONIZATION_LATENCY_MASK    (0xf << 4)
# define DP_LAST_ACTUAL_SYNCHRONIZATION_LATENCY_SHIFT    4

#define DP_LAST_RECEIVED_PSR_SDP        0x200a /* eDP 1.2 */
# define DP_PSR_STATE_BIT            (1 << 0) /* eDP 1.2 */
# define DP_UPDATE_RFB_BIT            (1 << 1) /* eDP 1.2 */
# define DP_CRC_VALID_BIT            (1 << 2) /* eDP 1.2 */
# define DP_SU_VALID                (1 << 3) /* eDP 1.4 */
# define DP_FIRST_SCAN_LINE_SU_REGION        (1 << 4) /* eDP 1.4 */
# define DP_LAST_SCAN_LINE_SU_REGION        (1 << 5) /* eDP 1.4 */
# define DP_Y_COORDINATE_VALID            (1 << 6) /* eDP 1.4a */

#define DP_RECEIVER_ALPM_STATUS            0x200b  /* eDP 1.4 */
# define DP_ALPM_LOCK_TIMEOUT_ERROR        (1 << 0)

#define DP_LANE0_1_STATUS_ESI                  0x200c /* status same as 0x202 */
#define DP_LANE2_3_STATUS_ESI                  0x200d /* status same as 0x203 */
#define DP_LANE_ALIGN_STATUS_UPDATED_ESI       0x200e /* status same as 0x204 */
#define DP_SINK_STATUS_ESI                     0x200f /* status same as 0x205 */

#define DP_SINK_DEVICE_PR_AND_FRAME_LOCK_STATUS 0x2022 /* DP 2.1 */
# define DP_SINK_DEVICE_PANEL_REPLAY_STATUS_MASK (0x7 << 0)
# define DP_SINK_FRAME_LOCKED_MASK  (0x3 << 3)
# define DP_SINK_FRAME_LOCKED_SHIFT 3
# define SINK_FRAME_LOCKED_STATUS_VALID (1 << 5)

/* Extended Receiver Capability: See DP_DPCD_REV for definitions */
#define DP_DP13_DPCD_REV                    0x2200

#define DP_DPRX_FEATURE_ENUMERATION_LIST    0x2210  /* DP 1.3 */
# define DP_GTC_CAP                    (1 << 0)  /* DP 1.3 */
# define DP_SST_SPLIT_SDP_CAP                (1 << 1)  /* DP 1.4 */
# define DP_AV_SYNC_CAP                    (1 << 2)  /* DP 1.3 */
# define DP_VSC_SDP_EXT_FOR_COLORIMETRY_SUPPORTED    (1 << 3)  /* DP 1.3 */
# define DP_VSC_EXT_VESA_SDP_SUPPORTED            (1 << 4)  /* DP 1.4 */
# define DP_VSC_EXT_VESA_SDP_CHAINING_SUPPORTED        (1 << 5)  /* DP 1.4 */
# define DP_VSC_EXT_CEA_SDP_SUPPORTED            (1 << 6)  /* DP 1.4 */
# define DP_VSC_EXT_CEA_SDP_CHAINING_SUPPORTED        (1 << 7)  /* DP 1.4 */

#define DP_128B132B_SUPPORTED_LINK_RATES       0x2215 /* 2.0 */
# define DP_UHBR10                             (1 << 0)
# define DP_UHBR20                             (1 << 1)
# define DP_UHBR13_5                           (1 << 2)

#define DP_128B132B_TRAINING_AUX_RD_INTERVAL                    0x2216 /* 2.0 */
# define DP_128B132B_TRAINING_AUX_RD_INTERVAL_MASK              0x7f
# define DP_128B132B_TRAINING_AUX_RD_INTERVAL_400_US            0x00
# define DP_128B132B_TRAINING_AUX_RD_INTERVAL_4_MS              0x01
# define DP_128B132B_TRAINING_AUX_RD_INTERVAL_8_MS              0x02
# define DP_128B132B_TRAINING_AUX_RD_INTERVAL_12_MS             0x03
# define DP_128B132B_TRAINING_AUX_RD_INTERVAL_16_MS             0x04
# define DP_128B132B_TRAINING_AUX_RD_INTERVAL_32_MS             0x05
# define DP_128B132B_TRAINING_AUX_RD_INTERVAL_64_MS             0x06

#define DP_TEST_264BIT_CUSTOM_PATTERN_7_0        0x2230
#define DP_TEST_264BIT_CUSTOM_PATTERN_263_256    0x2250

/* DSC Extended Capability Branch Total DSC Resources */
#define DP_DSC_SUPPORT_AND_DSC_DECODER_COUNT        0x2260    /* 2.0 */
# define DP_DSC_DECODER_COUNT_MASK            (0b111 << 5)
# define DP_DSC_DECODER_COUNT_SHIFT            5
#define DP_DSC_MAX_SLICE_COUNT_AND_AGGREGATION_0    0x2270    /* 2.0 */
# define DP_DSC_DECODER_0_MAXIMUM_SLICE_COUNT_MASK    (1 << 0)
# define DP_DSC_DECODER_0_AGGREGATION_SUPPORT_MASK    (0b111 << 1)
# define DP_DSC_DECODER_0_AGGREGATION_SUPPORT_SHIFT    1

/**
 * remove Protocol Converter Extension */

/* remove HDCP 1.3 and HDCP 2.2 */

/* LTTPR: Link Training (LT)-tunable PHY Repeaters */
#define DP_LT_TUNABLE_PHY_REPEATER_FIELD_DATA_STRUCTURE_REV 0xf0000 /* 1.3 */
#define DP_MAX_LINK_RATE_PHY_REPEATER                0xf0001 /* 1.4a */
#define DP_PHY_REPEATER_CNT                    0xf0002 /* 1.3 */
#define DP_PHY_REPEATER_MODE                    0xf0003 /* 1.3 */
#define DP_MAX_LANE_COUNT_PHY_REPEATER                0xf0004 /* 1.4a */
#define DP_Repeater_FEC_CAPABILITY                0xf0004 /* 1.4 */
#define DP_PHY_REPEATER_EXTENDED_WAIT_TIMEOUT            0xf0005 /* 1.4a */
#define DP_MAIN_LINK_CHANNEL_CODING_PHY_REPEATER        0xf0006 /* 2.0 */
# define DP_PHY_REPEATER_128B132B_SUPPORTED            (1 << 0)
/* See DP_128B132B_SUPPORTED_LINK_RATES for values */
#define DP_PHY_REPEATER_128B132B_RATES                0xf0007 /* 2.0 */

enum disp_dp_phy {
    DP_PHY_DPRX,

    DP_PHY_LTTPR1,
    DP_PHY_LTTPR2,
    DP_PHY_LTTPR3,
    DP_PHY_LTTPR4,
    DP_PHY_LTTPR5,
    DP_PHY_LTTPR6,
    DP_PHY_LTTPR7,
    DP_PHY_LTTPR8,

    DP_MAX_LTTPR_COUNT = DP_PHY_LTTPR8,
};

#define DP_PHY_LTTPR(i)                        (DP_PHY_LTTPR1 + (i))

#define __DP_LTTPR1_BASE                    0xf0010 /* 1.3 */
#define __DP_LTTPR2_BASE                    0xf0060 /* 1.3 */
#define DP_LTTPR_BASE(dp_phy) \
    (__DP_LTTPR1_BASE + (__DP_LTTPR2_BASE - __DP_LTTPR1_BASE) * \
        ((dp_phy) - DP_PHY_LTTPR1))

#define DP_LTTPR_REG(dp_phy, lttpr1_reg) \
    (DP_LTTPR_BASE(dp_phy) - DP_LTTPR_BASE(DP_PHY_LTTPR1) + (lttpr1_reg))

#define DP_TRAINING_PATTERN_SET_PHY_REPEATER1            0xf0010 /* 1.3 */
#define DP_TRAINING_PATTERN_SET_PHY_REPEATER(dp_phy) \
    DP_LTTPR_REG(dp_phy, DP_TRAINING_PATTERN_SET_PHY_REPEATER1)

#define DP_TRAINING_LANE0_SET_PHY_REPEATER1            0xf0011 /* 1.3 */
#define DP_TRAINING_LANE0_SET_PHY_REPEATER(dp_phy) \
    DP_LTTPR_REG(dp_phy, DP_TRAINING_LANE0_SET_PHY_REPEATER1)

#define DP_TRAINING_LANE1_SET_PHY_REPEATER1            0xf0012 /* 1.3 */
#define DP_TRAINING_LANE2_SET_PHY_REPEATER1            0xf0013 /* 1.3 */
#define DP_TRAINING_LANE3_SET_PHY_REPEATER1            0xf0014 /* 1.3 */
#define DP_TRAINING_AUX_RD_INTERVAL_PHY_REPEATER1        0xf0020 /* 1.4a */
#define DP_TRAINING_AUX_RD_INTERVAL_PHY_REPEATER(dp_phy)    \
    DP_LTTPR_REG(dp_phy, DP_TRAINING_AUX_RD_INTERVAL_PHY_REPEATER1)

#define DP_TRANSMITTER_CAPABILITY_PHY_REPEATER1            0xf0021 /* 1.4a */
# define DP_VOLTAGE_SWING_LEVEL_3_SUPPORTED            BIT(0)
# define DP_PRE_EMPHASIS_LEVEL_3_SUPPORTED            BIT(1)

#define DP_128B132B_TRAINING_AUX_RD_INTERVAL_PHY_REPEATER1  0xf0022 /* 2.0 */
#define DP_128B132B_TRAINING_AUX_RD_INTERVAL_PHY_REPEATER(dp_phy)    \
    DP_LTTPR_REG(dp_phy, DP_128B132B_TRAINING_AUX_RD_INTERVAL_PHY_REPEATER1)
/* see DP_128B132B_TRAINING_AUX_RD_INTERVAL for values */

#define DP_LANE0_1_STATUS_PHY_REPEATER1                0xf0030 /* 1.3 */
#define DP_LANE0_1_STATUS_PHY_REPEATER(dp_phy) \
    DP_LTTPR_REG(dp_phy, DP_LANE0_1_STATUS_PHY_REPEATER1)

#define DP_LANE2_3_STATUS_PHY_REPEATER1                0xf0031 /* 1.3 */

#define DP_LANE_ALIGN_STATUS_UPDATED_PHY_REPEATER1        0xf0032 /* 1.3 */
#define DP_ADJUST_REQUEST_LANE0_1_PHY_REPEATER1            0xf0033 /* 1.3 */
#define DP_ADJUST_REQUEST_LANE2_3_PHY_REPEATER1            0xf0034 /* 1.3 */
#define DP_SYMBOL_ERROR_COUNT_LANE0_PHY_REPEATER1        0xf0035 /* 1.3 */
#define DP_SYMBOL_ERROR_COUNT_LANE1_PHY_REPEATER1        0xf0037 /* 1.3 */
#define DP_SYMBOL_ERROR_COUNT_LANE2_PHY_REPEATER1        0xf0039 /* 1.3 */
#define DP_SYMBOL_ERROR_COUNT_LANE3_PHY_REPEATER1        0xf003b /* 1.3 */

#define __DP_FEC1_BASE                        0xf0290 /* 1.4 */
#define __DP_FEC2_BASE                        0xf0298 /* 1.4 */
#define DP_FEC_BASE(dp_phy) \
    (__DP_FEC1_BASE + ((__DP_FEC2_BASE - __DP_FEC1_BASE) * \
               ((dp_phy) - DP_PHY_LTTPR1)))

#define DP_FEC_REG(dp_phy, fec1_reg) \
    (DP_FEC_BASE(dp_phy) - DP_FEC_BASE(DP_PHY_LTTPR1) + fec1_reg)

#define DP_FEC_STATUS_PHY_REPEATER1                0xf0290 /* 1.4 */
#define DP_FEC_STATUS_PHY_REPEATER(dp_phy) \
    DP_FEC_REG(dp_phy, DP_FEC_STATUS_PHY_REPEATER1)

#define DP_FEC_ERROR_COUNT_PHY_REPEATER1                    0xf0291 /* 1.4 */
#define DP_FEC_CAPABILITY_PHY_REPEATER1                     0xf0294 /* 1.4a */

#define DP_LTTPR_MAX_ADD                    0xf02ff /* 1.4 */

#define DP_DPCD_MAX_ADD                        0xfffff /* 1.4 */

/* Repeater modes */
#define DP_PHY_REPEATER_MODE_TRANSPARENT            0x55    /* 1.3 */
#define DP_PHY_REPEATER_MODE_NON_TRANSPARENT            0xaa    /* 1.3 */

/* remove DP HDCP message start offsets in DPCD address space */

/* remove DP 1.2 Sideband message defines */

#define MODE_I2C_START    1
#define MODE_I2C_WRITE    2
#define MODE_I2C_READ    4
#define MODE_I2C_STOP    8

/*remove DP 1.2 MST PORTs - Section 2.5.1 v1.2a spec */

#define DP_LINK_CONSTANT_N_VALUE 0x8000
#define DP_LINK_STATUS_SIZE       6
disp_u8_t disp_dp_channel_eq_ok(disp_u8_t link_status[DP_LINK_STATUS_SIZE],
              int lane_count);
disp_u8_t disp_dp_clock_recovery_ok(disp_u8_t link_status[DP_LINK_STATUS_SIZE],
                  int lane_count);
disp_u8_t disp_dp_get_adjust_request_voltage(disp_u8_t link_status[DP_LINK_STATUS_SIZE],
                     int lane);
disp_u8_t disp_dp_get_adjust_request_pre_emphasis(disp_u8_t link_status[DP_LINK_STATUS_SIZE],
                      int lane);
disp_u8_t disp_dp_get_adjust_tx_ffe_preset(disp_u8_t link_status[DP_LINK_STATUS_SIZE],
                   int lane);

#define DP_BRANCH_OUI_HEADER_SIZE       0xc
#define DP_RECEIVER_CAP_SIZE            0x10
#define DP_DSC_RECEIVER_CAP_SIZE        0x10
#define EDP_PSR_RECEIVER_CAP_SIZE       2
#define EDP_DISPLAY_CTL_CAP_SIZE        5
#define DP_LTTPR_COMMON_CAP_SIZE        8
#define DP_LTTPR_PHY_CAP_SIZE           3
#define DP_SINK_STATUS_SIZE             0x6

struct disp_dp_aux_msg {
    unsigned int address;
    disp_u8_t request;
    disp_u8_t reply;
    disp_u8_t *buffer;
    disp_u32_t size;//limit to 16 bytes one request
    disp_u32_t read_back_size;
};

struct disp_dp_aux {
    pdisp_void_t pconnector;
    disp_s32_t (*transfer)(pdisp_void_t aux, struct disp_dp_aux_msg *msg);
    disp_u32_t i2c_nack_count;
    disp_u32_t i2c_defer_count;
    disp_u32_t crc_count;
    disp_u32_t aux_delay_us;
};

int disp_dp_read_clock_recovery_delay(struct disp_dp_aux *aux, disp_u8_t dpcd[DP_RECEIVER_CAP_SIZE],
                     enum disp_dp_phy dp_phy, disp_u8_t uhbr);
int disp_dp_read_channel_eq_delay(struct disp_dp_aux *aux, disp_u8_t dpcd[DP_RECEIVER_CAP_SIZE],
                 enum disp_dp_phy dp_phy, disp_u8_t uhbr);

void disp_dp_link_train_clock_recovery_delay(disp_u8_t dpcd[DP_RECEIVER_CAP_SIZE]);
void disp_dp_lttpr_link_train_clock_recovery_delay(void);
void disp_dp_link_train_channel_eq_delay(disp_u8_t dpcd[DP_RECEIVER_CAP_SIZE]);
void disp_dp_lttpr_link_train_channel_eq_delay(disp_u8_t caps[DP_LTTPR_PHY_CAP_SIZE]);

disp_u8_t disp_dp_link_rate_to_bw_code(int link_rate);
int disp_dp_bw_code_to_link_rate(disp_u8_t link_bw);

#define DP_SDP_AUDIO_TIMESTAMP        0x01
#define DP_SDP_AUDIO_STREAM        0x02
#define DP_SDP_EXTENSION        0x04 /* DP 1.1 */
#define DP_SDP_AUDIO_COPYMANAGEMENT    0x05 /* DP 1.2 */
#define DP_SDP_ISRC            0x06 /* DP 1.2 */
#define DP_SDP_VSC            0x07 /* DP 1.2 */
#define DP_SDP_CAMERA_GENERIC(i)    (0x08 + (i)) /* 0-7, DP 1.3 */
#define DP_SDP_PPS            0x10 /* DP 1.4 */
#define DP_SDP_VSC_EXT_VESA        0x20 /* DP 1.4 */
#define DP_SDP_VSC_EXT_CEA        0x21 /* DP 1.4 */
/* 0x80+ CEA-861 infoframe types */

#define EDP_SDP_HEADER_REVISION_MASK        0x1F
#define EDP_SDP_HEADER_VALID_PAYLOAD_BYTES    0x1F
#define DP_SDP_PPS_HEADER_PAYLOAD_BYTES_MINUS_1 0x7F

/**
 * struct dp_sdp - DP secondary data packet
 *
 * @header: DP secondary data packet header
 * @byte0: Secondary Data Packet ID
 * @byte1: Secondary Data Packet Type
 * @byte2: Secondary Data Packet Specific header, Byte 0
 * @byte3: Secondary Data packet Specific header, Byte 1
 *
 * @payload: DP secondaray data packet data blocks
 * VSC SDP Payload for PSR
 * byte[0]: Stereo Interface
 * byte[1]: 0 - PSR State; 1 - Update RFB; 2 - CRC Valid
 * byte[2]: CRC value bits 7:0 of the R or Cr component
 * byte[3]: CRC value bits 15:8 of the R or Cr component
 * byte[4]: CRC value bits 7:0 of the G or Y component
 * byte[5]: CRC value bits 15:8 of the G or Y component
 * byte[6]: CRC value bits 7:0 of the B or Cb component
 * byte[7]: CRC value bits 15:8 of the B or Cb component
 * byte[8] - byte[31]: Reserved
 * VSC SDP Payload for Pixel Encoding/Colorimetry Format
 * byte[0] - byte[15]: Reserved
 * byte[16]: Pixel Encoding and Colorimetry Formats
 * byte[17]: Dynamic Range and Component Bit Depth
 * byte[18]: Content Type
 * byte[19] - byte[31]: Reserved
 */
union dp_sdp_uint {
    struct {
        disp_u8_t b0;
        disp_u8_t b1;
        disp_u8_t b2;
        disp_u8_t b3;
    }byte;
    disp_u32_t uint;
};
#define DP_SDP_PAYLOAD_MAX_DATA_BYTES 32
struct dp_sdp {
    union dp_sdp_uint header;
    union {
        disp_u32_t u32[DP_SDP_PAYLOAD_MAX_DATA_BYTES/4];
        disp_u8_t u8[DP_SDP_PAYLOAD_MAX_DATA_BYTES];
    }payload;
    disp_u32_t sdp_len;
    disp_u32_t sdp_req; // 1-once, 2-every frame, 3-clear req
    disp_u32_t fifo_index;
};

/* 32 fifo(Sdp_Req_Addr 0~31)
 *
 * before fifo send, make sure fifo state is idle(Sdp_Tx_Status 0)
 * check state: Sdp_Tx_Status_0 or Sdp_Tx_Status_1
 *
 * before update every frame fifo, clear fifo and check state
 */
#define DP_SDP_FIFO_STATE_COUNT 16
#define DP_SDP_FIFO_STATE_MASK(index) (0x3 << (index * 2))
enum DP_FIFO_STATE_TYPE
{
    DP_FIFO_STATE_TYPE_IDLE = 0,
    DP_FIFO_STATE_TYPE_ONCE,
    DP_FIFO_STATE_TYPE_EVERY_FRAME,
};

#define EDP_VSC_PSR_STATE_ACTIVE    (1<<0)
#define EDP_VSC_PSR_UPDATE_RFB        (1<<1)
#define EDP_VSC_PSR_CRC_VALUES_VALID    (1<<2)

/**
 * enum dp_pixelformat - drm DP Pixel encoding formats
 *
 * This enum is used to indicate DP VSC SDP Pixel encoding formats.
 * It is based on DP 1.4 spec [Table 2-117: VSC SDP Payload for DB16 through
 * DB18]
 *
 * @DP_PIXELFORMAT_RGB: RGB pixel encoding format
 * @DP_PIXELFORMAT_YUV444: YCbCr 4:4:4 pixel encoding format
 * @DP_PIXELFORMAT_YUV422: YCbCr 4:2:2 pixel encoding format
 * @DP_PIXELFORMAT_YUV420: YCbCr 4:2:0 pixel encoding format
 * @DP_PIXELFORMAT_Y_ONLY: Y Only pixel encoding format
 * @DP_PIXELFORMAT_RAW: RAW pixel encoding format
 * @DP_PIXELFORMAT_RESERVED: Reserved pixel encoding format
 */
enum dp_pixelformat {
    DP_PIXELFORMAT_RGB = 0,
    DP_PIXELFORMAT_YUV444 = 0x1,
    DP_PIXELFORMAT_YUV422 = 0x2,
    DP_PIXELFORMAT_YUV420 = 0x3,
    DP_PIXELFORMAT_Y_ONLY = 0x4,
    DP_PIXELFORMAT_RAW = 0x5,
    DP_PIXELFORMAT_RESERVED = 0x6,
};

/**
 * enum dp_colorimetry - drm DP Colorimetry formats
 *
 * This enum is used to indicate DP VSC SDP Colorimetry formats.
 * It is based on DP 1.4 spec [Table 2-117: VSC SDP Payload for DB16 through
 * DB18] and a name of enum member follows DISP_MODE_COLORIMETRY definition.
 *
 * @DP_COLORIMETRY_DEFAULT: sRGB (IEC 61966-2-1) or
 *                          ITU-R BT.601 colorimetry format
 * @DP_COLORIMETRY_RGB_WIDE_FIXED: RGB wide gamut fixed point colorimetry format
 * @DP_COLORIMETRY_BT709_YCC: ITU-R BT.709 colorimetry format
 * @DP_COLORIMETRY_RGB_WIDE_FLOAT: RGB wide gamut floating point
 *                                 (scRGB (IEC 61966-2-2)) colorimetry format
 * @DP_COLORIMETRY_XVYCC_601: xvYCC601 colorimetry format
 * @DP_COLORIMETRY_OPRGB: OpRGB colorimetry format
 * @DP_COLORIMETRY_XVYCC_709: xvYCC709 colorimetry format
 * @DP_COLORIMETRY_DCI_P3_RGB: DCI-P3 (SMPTE RP 431-2) colorimetry format
 * @DP_COLORIMETRY_SYCC_601: sYCC601 colorimetry format
 * @DP_COLORIMETRY_RGB_CUSTOM: RGB Custom Color Profile colorimetry format
 * @DP_COLORIMETRY_OPYCC_601: opYCC601 colorimetry format
 * @DP_COLORIMETRY_BT2020_RGB: ITU-R BT.2020 R' G' B' colorimetry format
 * @DP_COLORIMETRY_BT2020_CYCC: ITU-R BT.2020 Y'c C'bc C'rc colorimetry format
 * @DP_COLORIMETRY_BT2020_YCC: ITU-R BT.2020 Y' C'b C'r colorimetry format
 */
enum dp_colorimetry {
    DP_COLORIMETRY_DEFAULT = 0,
    DP_COLORIMETRY_RGB_WIDE_FIXED = 0x1,
    DP_COLORIMETRY_BT709_YCC = 0x1,
    DP_COLORIMETRY_RGB_WIDE_FLOAT = 0x2,
    DP_COLORIMETRY_XVYCC_601 = 0x2,
    DP_COLORIMETRY_OPRGB = 0x3,
    DP_COLORIMETRY_XVYCC_709 = 0x3,
    DP_COLORIMETRY_DCI_P3_RGB = 0x4,
    DP_COLORIMETRY_SYCC_601 = 0x4,
    DP_COLORIMETRY_RGB_CUSTOM = 0x5,
    DP_COLORIMETRY_OPYCC_601 = 0x5,
    DP_COLORIMETRY_BT2020_RGB = 0x6,
    DP_COLORIMETRY_BT2020_CYCC = 0x6,
    DP_COLORIMETRY_BT2020_YCC = 0x7,
};

enum dp_bpc {
    DP_BPC_6 = 0,
    DP_BPC_8 = 1,
    DP_BPC_RAW_6 = 1,
    DP_BPC_10 = 2,
    DP_BPC_RAW_7 = 2,
    DP_BPC_12 = 3,
    DP_BPC_RAW_8 = 3,
    DP_BPC_16 = 4,
    DP_BPC_RAW_10 = 4,
    DP_BPC_RAW_12 = 5,
    DP_BPC_RAW_14 = 6,
    DP_BPC_RAW_16 = 7,
};

/**
 * enum dp_dynamic_range - drm DP Dynamic Range
 *
 * This enum is used to indicate DP VSC SDP Dynamic Range.
 * It is based on DP 1.4 spec [Table 2-117: VSC SDP Payload for DB16 through
 * DB18]
 *
 * @DP_DYNAMIC_RANGE_VESA: VESA range
 * @DP_DYNAMIC_RANGE_CTA: CTA range
 */
enum dp_dynamic_range {
    DP_DYNAMIC_RANGE_VESA = 0,
    DP_DYNAMIC_RANGE_CTA = 1,
};

/**
 * enum dp_content_type - drm DP Content Type
 *
 * This enum is used to indicate DP VSC SDP Content Types.
 * It is based on DP 1.4 spec [Table 2-117: VSC SDP Payload for DB16 through
 * DB18]
 * CTA-861-G defines content types and expected processing by a sink device
 *
 * @DP_CONTENT_TYPE_NOT_DEFINED: Not defined type
 * @DP_CONTENT_TYPE_GRAPHICS: Graphics type
 * @DP_CONTENT_TYPE_PHOTO: Photo type
 * @DP_CONTENT_TYPE_VIDEO: Video type
 * @DP_CONTENT_TYPE_GAME: Game type
 */
enum dp_content_type {
    DP_CONTENT_TYPE_NOT_DEFINED = 0x00,
    DP_CONTENT_TYPE_GRAPHICS = 0x01,
    DP_CONTENT_TYPE_PHOTO = 0x02,
    DP_CONTENT_TYPE_VIDEO = 0x03,
    DP_CONTENT_TYPE_GAME = 0x04,
};

enum dp_sdp_vsc_type {
    DP_SDP_VSC_TYPE_3D = 1,
    DP_SDP_VSC_TYPE_3D_PSR,     // eDPv1.3
    DP_SDP_VSC_TYPE_3D_PSR2,    // eDPv1.4
    DP_SDP_VSC_TYPE_3D_PSR2_Y,    // eDPv1.4
    DP_SDP_VSC_TYPE_3D_PSR2_COLORIMETRY,
    DP_SDP_VSC_TYPE_3D_PR,
    DP_SDP_VSC_TYPE_3D_PR2_COLORIMETRY,
};

#define DP_SDP_VSC_LENGTH_3D        0x1;
#define DP_SDP_VSC_LENGTH_3D_PSR    0x8;
#define DP_SDP_VSC_LENGTH_3D_PSR2   0xc;
#define DP_SDP_VSC_LENGTH_3D_PSR2_Y 0xe;
#define DP_SDP_VSC_LENGTH_3D_PR     0x10;
#define DP_SDP_VSC_LENGTH_3D_PSR2_COLORIMETRY   0x13;
#define DP_SDP_VSC_LENGTH_3D_PR2_COLORIMETRY    0x13;

/**
 * struct disp_dp_sdp_vsc - drm DP VSC SDP
 *
 * This structure represents a DP VSC SDP of drm
 * It is based on DP 1.4 spec [Table 2-116: VSC SDP Header Bytes] and
 * [Table 2-117: VSC SDP Payload for DB16 through DB18]
 *
 * @sdp_type: secondary-data packet type
 * @revision: revision number
 * @length: number of valid data bytes
 * @pixelformat: pixel encoding format
 * @colorimetry: colorimetry format
 * @bpc: bit per color
 * @dynamic_range: dynamic range information
 * @content_type: CTA-861-G defines content types and expected processing by a sink device
 */
struct disp_dp_sdp_vsc {
    disp_u8_t sdp_id;
    disp_u8_t sdp_type;
    enum dp_sdp_vsc_type revision;
    disp_u8_t length;

    disp_u8_t stereo;
    disp_u8_t psr_active    : 1;
    disp_u8_t psr_rfb       : 1;
    disp_u8_t psr_crc       : 1;
    disp_u8_t psr_su        : 1;
    disp_u8_t psr_su_first  : 1;
    disp_u8_t psr_su_last   : 1;
    disp_u8_t psr_y         : 1;
    disp_u16_t psr_crc_rgb[3];  // RGB
    disp_u16_t psr_su_x;    // X-Coordinate of Upper-Left Corner of Selective Update Region (Pixel Count)
    disp_u16_t psr_rect_w;  // Width of Rectangle (Pixel Count)
    disp_u16_t psr_su_y;    // Y-Coordinate of First Scan Line of the Selective Update Region
    enum dp_pixelformat pixelformat;
    enum dp_colorimetry colorimetry;
    enum dp_bpc bpc;
    enum dp_dynamic_range dynamic_range;
    enum dp_content_type content_type;
};

int disp_dp_psr_setup_time(disp_u8_t psr_cap[EDP_PSR_RECEIVER_CAP_SIZE]);

static inline int
disp_dp_max_link_rate(disp_u8_t dpcd[DP_RECEIVER_CAP_SIZE])
{
    return disp_dp_bw_code_to_link_rate(dpcd[DP_MAX_LINK_RATE]);
}

static inline disp_u8_t
disp_dp_max_lane_count(disp_u8_t dpcd[DP_RECEIVER_CAP_SIZE])
{
    return dpcd[DP_MAX_LANE_COUNT] & DP_MAX_LANE_COUNT_MASK;
}

static inline disp_u8_t
disp_dp_enhanced_frame_cap(disp_u8_t dpcd[DP_RECEIVER_CAP_SIZE])
{
    return dpcd[DP_DPCD_REV] >= 0x11 &&
        (dpcd[DP_MAX_LANE_COUNT] & DP_ENHANCED_FRAME_CAP);
}

static inline disp_u8_t
disp_dp_fast_training_cap(disp_u8_t dpcd[DP_RECEIVER_CAP_SIZE])
{
    return dpcd[DP_DPCD_REV] >= 0x11 &&
        (dpcd[DP_MAX_DOWNSPREAD] & DP_NO_AUX_HANDSHAKE_LINK_TRAINING);
}

static inline disp_u8_t
disp_dp_tps3_supported(disp_u8_t dpcd[DP_RECEIVER_CAP_SIZE])
{
    return dpcd[DP_DPCD_REV] >= 0x12 &&
        dpcd[DP_MAX_LANE_COUNT] & DP_TPS3_SUPPORTED;
}

static inline disp_u8_t
disp_dp_max_downspread(disp_u8_t dpcd[DP_RECEIVER_CAP_SIZE])
{
    return dpcd[DP_DPCD_REV] >= 0x11 ||
        dpcd[DP_MAX_DOWNSPREAD] & DP_MAX_DOWNSPREAD_0_5;
}

static inline disp_u8_t
disp_dp_tps4_supported(disp_u8_t dpcd[DP_RECEIVER_CAP_SIZE])
{
    return dpcd[DP_DPCD_REV] >= 0x14 &&
        dpcd[DP_MAX_DOWNSPREAD] & DP_TPS4_SUPPORTED;
}

static inline disp_u8_t
disp_dp_training_pattern_mask(disp_u8_t dpcd[DP_RECEIVER_CAP_SIZE])
{
    return (dpcd[DP_DPCD_REV] >= 0x14) ? DP_TRAINING_PATTERN_MASK_1_4 :
        DP_TRAINING_PATTERN_MASK;
}

static inline disp_u8_t
disp_dp_is_branch(disp_u8_t dpcd[DP_RECEIVER_CAP_SIZE])
{
    return dpcd[DP_DOWNSTREAMPORT_PRESENT] & DP_DWN_STRM_PORT_PRESENT;
}

static inline disp_u8_t
disp_dp_get_rd_interval(disp_u8_t dpcd[DP_RECEIVER_CAP_SIZE])
{
    return dpcd[DP_TRAINING_AUX_RD_INTERVAL] & DP_TRAINING_AUX_RD_MASK;
}

/* DP/eDP DSC support */
disp_u8_t disp_dp_dsc_sink_max_slice_count(disp_u8_t dsc_dpcd[DP_DSC_RECEIVER_CAP_SIZE],
                   disp_u8_t is_edp);
disp_u8_t disp_dp_dsc_sink_line_buf_depth(disp_u8_t dsc_dpcd[DP_DSC_RECEIVER_CAP_SIZE]);
int disp_dp_dsc_sink_supported_input_bpcs(disp_u8_t dsc_dpc[DP_DSC_RECEIVER_CAP_SIZE],
                     disp_u8_t dsc_bpc[3]);

static inline disp_u8_t
disp_dp_sink_supports_dsc(disp_u8_t dsc_dpcd[DP_DSC_RECEIVER_CAP_SIZE])
{
    return dsc_dpcd[DP_DSC_SUPPORT - DP_DSC_SUPPORT] &
        DP_DSC_DECOMPRESSION_IS_SUPPORTED;
}

static inline disp_u16_t
disp_edp_dsc_sink_output_bpp(disp_u8_t dsc_dpcd[DP_DSC_RECEIVER_CAP_SIZE])
{
    return dsc_dpcd[DP_DSC_MAX_BITS_PER_PIXEL_LOW - DP_DSC_SUPPORT] |
        (dsc_dpcd[DP_DSC_MAX_BITS_PER_PIXEL_HI - DP_DSC_SUPPORT] &
         DP_DSC_MAX_BITS_PER_PIXEL_HI_MASK <<
         DP_DSC_MAX_BITS_PER_PIXEL_HI_SHIFT);
}

static inline disp_u32_t
disp_dp_dsc_sink_max_slice_width(disp_u8_t dsc_dpcd[DP_DSC_RECEIVER_CAP_SIZE])
{
    /* Max Slicewidth = Number of Pixels * 320 */
    return (disp_u32_t)dsc_dpcd[DP_DSC_MAX_SLICE_WIDTH - DP_DSC_SUPPORT] *
        DP_DSC_SLICE_WIDTH_MULTIPLIER;
}

static inline disp_u8_t
disp_dp_dsc_sink_min_increment_bpp(disp_u8_t dsc_dpcd[DP_DSC_RECEIVER_CAP_SIZE])
{
    return dsc_dpcd[DP_DSC_BITS_PER_PIXEL_INC - DP_DSC_SUPPORT] & DP_DSC_BITS_PER_PIXEL_INC_MASK;
}

/* Forward Error Correction Support on DP 1.4 */
static inline disp_u8_t
disp_dp_sink_supports_fec(disp_u8_t fec_capable)
{
    return fec_capable & DP_FEC_CAPABLE;
}

static inline disp_u8_t
disp_dp_channel_coding_supported(disp_u8_t dpcd[DP_RECEIVER_CAP_SIZE])
{
    return dpcd[DP_MAIN_LINK_CHANNEL_CODING] & DP_CAP_ANSI_8B10B;
}

static inline disp_u8_t
disp_dp_alternate_scrambler_reset_cap(disp_u8_t dpcd[DP_RECEIVER_CAP_SIZE])
{
    return dpcd[DP_EDP_CONFIGURATION_CAP] &
            DP_ALTERNATE_SCRAMBLER_RESET_CAP;
}

/* Ignore MSA timing for Adaptive Sync support on DP 1.4 */
static inline disp_u8_t
disp_dp_sink_can_do_video_without_timing_msa(disp_u8_t dpcd[DP_RECEIVER_CAP_SIZE])
{
    return dpcd[DP_DOWN_STREAM_PORT_COUNT] &
        DP_MSA_TIMING_PAR_IGNORED;
}

/**
 * disp_edp_backlight_supported() - Check an eDP DPCD for VESA backlight support
 * @edp_dpcd: The DPCD to check
 *
 * Note that currently this function will return %false for panels which support various DPCD
 * backlight features but which require the brightness be set through PWM, and don't support setting
 * the brightness level via the DPCD.
 *
 * Returns: %True if @edp_dpcd indicates that VESA backlight controls are supported, %false
 * otherwise
 */
static inline disp_u8_t
disp_edp_backlight_supported(disp_u8_t edp_dpcd[EDP_DISPLAY_CTL_CAP_SIZE])
{
    return !!(edp_dpcd[1] & DP_EDP_TCON_BACKLIGHT_ADJUSTMENT_CAP);
}

/*
 * DisplayPort AUX channel
 */



disp_s32_t disp_dp_dpcd_read(struct disp_dp_aux *aux, unsigned int offset,
             void *buffer, disp_u32_t size);
disp_s32_t disp_dp_dpcd_write(struct disp_dp_aux *aux, unsigned int offset,
              void *buffer, disp_u32_t size);
disp_u32_t disp_dp_dpcd_transfer(pdisp_connector_t pdisp_connector, pdisp_aux_para_t paux_para);
disp_s32_t  disp_dp_i2c_read(struct disp_dp_aux *aux, disp_u32_t offset,
              void *buffer, disp_u32_t size);

/**
 * disp_dp_dpcd_readb() - read a single byte from the DPCD
 * @aux: DisplayPort AUX channel
 * @offset: address of the register to read
 * @valuep: location where the value of the register will be stored
 *
 * Returns the number of bytes transferred (1) on success, or a negative
 * error code on failure.
 */
static inline disp_s32_t disp_dp_dpcd_readb(struct disp_dp_aux *aux,
                    unsigned int offset, disp_u8_t *valuep)
{
    return disp_dp_dpcd_read(aux, offset, valuep, 1);
}

/**
 * disp_dp_dpcd_writeb() - write a single byte to the DPCD
 * @aux: DisplayPort AUX channel
 * @offset: address of the register to write
 * @value: value to write to the register
 *
 * Returns the number of bytes transferred (1) on success, or a negative
 * error code on failure.
 */
static inline disp_s32_t disp_dp_dpcd_writeb(struct disp_dp_aux *aux,
                     unsigned int offset, disp_u8_t value)
{
    return disp_dp_dpcd_write(aux, offset, &value, 1);
}

int disp_dp_read_dpcd_caps(struct disp_dp_aux *aux,
              disp_u8_t dpcd[DP_RECEIVER_CAP_SIZE]);

int disp_dp_dpcd_read_link_status(struct disp_dp_aux *aux,
                 disp_u8_t status[DP_LINK_STATUS_SIZE]);

int disp_dp_dpcd_read_phy_link_status(struct disp_dp_aux *aux,
                     enum disp_dp_phy dp_phy,
                     disp_u8_t link_status[DP_LINK_STATUS_SIZE]);

disp_u8_t disp_dp_send_real_edid_checksum(struct disp_dp_aux *aux,
                    disp_u8_t real_edid_checksum);

int disp_dp_read_downstream_info(struct disp_dp_aux *aux,
                disp_u8_t dpcd[DP_RECEIVER_CAP_SIZE],
                disp_u8_t downstream_ports[DP_MAX_DOWNSTREAM_PORTS]);
disp_u8_t disp_dp_downstream_is_type(disp_u8_t dpcd[DP_RECEIVER_CAP_SIZE],
                   disp_u8_t port_cap[4], disp_u8_t type);
disp_u8_t disp_dp_downstream_is_tmds(disp_u8_t dpcd[DP_RECEIVER_CAP_SIZE],
                   disp_u8_t port_cap[4],
                   edid_t *edid);
int disp_dp_downstream_max_dotclock(disp_u8_t dpcd[DP_RECEIVER_CAP_SIZE],
                   disp_u8_t port_cap[4]);
int disp_dp_downstream_max_tmds_clock(disp_u8_t dpcd[DP_RECEIVER_CAP_SIZE],
                     disp_u8_t port_cap[4],
                     edid_t *edid);
int disp_dp_downstream_min_tmds_clock(disp_u8_t dpcd[DP_RECEIVER_CAP_SIZE],
                     disp_u8_t port_cap[4],
                     edid_t *edid);
int disp_dp_downstream_max_bpc(disp_u8_t dpcd[DP_RECEIVER_CAP_SIZE],
                  disp_u8_t port_cap[4],
                  edid_t *edid);
disp_u8_t disp_dp_downstream_420_passthrough(disp_u8_t dpcd[DP_RECEIVER_CAP_SIZE],
                       disp_u8_t port_cap[4]);
disp_u8_t disp_dp_downstream_444_to_420_conversion(disp_u8_t dpcd[DP_RECEIVER_CAP_SIZE],
                         disp_u8_t port_cap[4]);
pdisp_display_mode_t disp_dp_downstream_mode(disp_u8_t dpcd[DP_RECEIVER_CAP_SIZE],
                        disp_u8_t port_cap[4]);
int disp_dp_downstream_id(struct disp_dp_aux *aux, char id[6]);

DISP_MODE_SUBCONNECTOR_TYPE
disp_dp_subconnector_type(disp_u8_t dpcd[DP_RECEIVER_CAP_SIZE],
             disp_u8_t port_cap[4]);

struct disp_dp_desc;
disp_u8_t disp_dp_read_sink_count_cap(pdisp_connector_t connector,
                disp_u8_t dpcd[DP_RECEIVER_CAP_SIZE],
                struct disp_dp_desc *desc);
int disp_dp_read_sink_count(struct disp_dp_aux *aux);
disp_s32_t disp_dp_read_sink_status(pdisp_connector_t pdisp_connector, disp_u8_t status[DP_SINK_STATUS_SIZE]);

int disp_dp_read_lttpr_common_caps(struct disp_dp_aux *aux,
                  disp_u8_t caps[DP_LTTPR_COMMON_CAP_SIZE]);
int disp_dp_read_lttpr_phy_caps(struct disp_dp_aux *aux,
                   enum disp_dp_phy dp_phy,
                   disp_u8_t caps[DP_LTTPR_PHY_CAP_SIZE]);
int disp_dp_lttpr_count(disp_u8_t cap[DP_LTTPR_COMMON_CAP_SIZE]);
int disp_dp_lttpr_max_link_rate(disp_u8_t caps[DP_LTTPR_COMMON_CAP_SIZE]);
int disp_dp_lttpr_max_lane_count(disp_u8_t caps[DP_LTTPR_COMMON_CAP_SIZE]);
disp_u8_t disp_dp_lttpr_voltage_swing_level_3_supported(disp_u8_t caps[DP_LTTPR_PHY_CAP_SIZE]);
disp_u8_t disp_dp_lttpr_pre_emphasis_level_3_supported(disp_u8_t caps[DP_LTTPR_PHY_CAP_SIZE]);

void disp_dp_aux_init(pdisp_connector_t pdp_connector);

int disp_dp_start_crc(struct disp_dp_aux *aux);
int disp_dp_stop_crc(struct disp_dp_aux *aux);

struct disp_dp_dpcd_ident {
    disp_u8_t oui[3];
    disp_u8_t device_id[6];
    disp_u8_t hw_rev;
    disp_u8_t sw_major_rev;
    disp_u8_t sw_minor_rev;
};

/**
 * struct disp_dp_desc - DP branch/sink device descriptor
 * @ident: DP device identification from DPCD 0x400 (sink) or 0x500 (branch).
 * @quirks: Quirks; use disp_dp_has_quirk() to query for the quirks.
 */
struct disp_dp_desc {
    struct disp_dp_dpcd_ident ident;
    disp_u32_t quirks;
};

int disp_dp_read_desc(struct disp_dp_aux *aux, struct disp_dp_desc *desc,
             disp_u8_t is_branch);

/**
 * enum disp_dp_quirk - Display Port sink/branch device specific quirks
 *
 * Display Port sink and branch devices in the wild have a variety of bugs, try
 * to collect them here. The quirks are shared, but it's up to the drivers to
 * implement workarounds for them.
 */
enum disp_dp_quirk {
    /**
     * @DP_DPCD_QUIRK_CONSTANT_N:
     *
     * The device requires main link attributes Mvid and Nvid to be limited
     * to 16 bits. So will give a constant value (0x8000) for compatability.
     */
    DP_DPCD_QUIRK_CONSTANT_N,
    /**
     * @DP_DPCD_QUIRK_NO_PSR:
     *
     * The device does not support PSR even if reports that it supports or
     * driver still need to implement proper handling for such device.
     */
    DP_DPCD_QUIRK_NO_PSR,
    /**
     * @DP_DPCD_QUIRK_NO_SINK_COUNT:
     *
     * The device does not set SINK_COUNT to a non-zero value.
     * The driver should ignore SINK_COUNT during detection. Note that
     * disp_dp_read_sink_count_cap() automatically checks for this quirk.
     */
    DP_DPCD_QUIRK_NO_SINK_COUNT,
    /**
     * @DP_DPCD_QUIRK_DSC_WITHOUT_VIRTUAL_DPCD:
     *
     * The device supports MST DSC despite not supporting Virtual DPCD.
     * The DSC caps can be read from the physical aux instead.
     */
    DP_DPCD_QUIRK_DSC_WITHOUT_VIRTUAL_DPCD,
    /**
     * @DP_DPCD_QUIRK_CAN_DO_MAX_LINK_RATE_3_24_GBPS:
     *
     * The device supports a link rate of 3.24 Gbps (multiplier 0xc) despite
     * the DP_MAX_LINK_RATE register reporting a lower max multiplier.
     */
    DP_DPCD_QUIRK_CAN_DO_MAX_LINK_RATE_3_24_GBPS,
};

/**
 * disp_dp_has_quirk() - does the DP device have a specific quirk
 * @desc: Device descriptor filled by disp_dp_read_desc()
 * @quirk: Quirk to query for
 *
 * Return true if DP device identified by @desc has @quirk.
 */
static inline disp_u8_t
disp_dp_has_quirk(struct disp_dp_desc *desc, enum disp_dp_quirk quirk)
{
    return desc->quirks & (1 << quirk);
}

#define DISP_BACKLIGHT_PWM_DUTY_MAX 0x3ffff
#define DISP_BACKLIGHT_AUX_MSB_MAX 0xff
#define DISP_BACKLIGHT_AUX_LSB_MAX 0xffff
#define DISP_BACKLIGHT_PERCENT_VALUE_MAX 100000 // 1000 == 1%
#define DISP_BACKLIGHT_ADAPTIVE_VALUE_MAX 20 // 5%
//#define DISP_BACKLIGHT_NITS_VALUE_MAX 1000000 // 0.001 nit step, 1000 nit max

/**
 * struct disp_edp_backlight_info - Probed eDP backlight info struct
 * @pwmgen_bit_count: The pwmgen bit count
 * @pwm_freq_pre_divider: The PWM frequency pre-divider value being used for this backlight, if any
 * @max: The maximum backlight level that may be set
 * @lsb_reg_used: Do we also write values to the DP_EDP_BACKLIGHT_BRIGHTNESS_LSB register?
 * @aux_enable: Does the panel support the AUX enable cap?
 * @aux_set: Does the panel support setting the brightness through AUX?
 *
 * This structure contains various data about an eDP backlight, which can be populated by using
 * disp_edp_backlight_init().
 */
struct disp_edp_backlight_info {
    disp_u8_t pwmgen_bit_count; // Pn
    disp_u8_t pwm_freq_pre_divider; // F
    disp_u16_t max; // P, P = 2^Pn // what use ?

    disp_u8_t min_pn;
    disp_u8_t max_pn;

    // %5 step of normal backlight, up to 100%
    disp_u8_t dbc_min_set;
    disp_u8_t dbc_max_set;

    // regional
    disp_u8_t regional_x;
    disp_u8_t regional_y;

    // msb + lsb
    disp_u16_t cur_level;

    // 18 bit number, F * P
    disp_u32_t min_freq_divider;
    disp_u32_t max_freq_divider;

    disp_u8_t lsb_reg_used : 1;
    disp_u8_t aux_enable : 1;
    disp_u8_t aux_pwm_enable : 1; // backlight value = pwm_duty_cycle * MSB-LSB
    disp_u8_t pwm_duty_enable : 1;
    disp_u8_t pwm_freq_enable : 1;
    disp_u8_t aux_freq_enable : 1;
    disp_u8_t dbc_enable : 1; // dynamic backlight control
    disp_u8_t regional_enable : 1;
    disp_u8_t smooth_enable : 1; // linear level control ? OS caps, no aux caps to get
    disp_u8_t nits_enable : 1; // direct nits level, OS caps, no aux caps to get
    disp_u8_t prefer_dbc : 1;
    disp_u8_t prefer_regional : 1;
    disp_u8_t prefer_mode : 2; // DP_EDP_BACKLIGHT_CONTROL_MODE_MASK
    disp_u8_t set_mode : 2; // DP_EDP_BACKLIGHT_CONTROL_MODE_MASK
    disp_u8_t set_freq_aux : 1;
    disp_u8_t set_dbc : 1;
    disp_u8_t set_regional : 1;
    disp_u8_t set_aux_backlight : 1;
};

disp_void_t disp_edp_backlight_aux_get_brightness(struct disp_dp_aux *aux, struct disp_edp_backlight_info *bl);
disp_void_t disp_edp_backlight_aux_set_level(struct disp_dp_aux *aux, struct disp_edp_backlight_info *bl);
disp_void_t disp_edp_backlight_aux_set_mode(struct disp_dp_aux *aux, struct disp_edp_backlight_info *bl);
disp_void_t disp_edp_backlight_aux_enable(struct disp_dp_aux *aux, struct disp_edp_backlight_info *bl);
disp_void_t disp_edp_backlight_aux_probe_freq_within_25percent_desire(struct disp_dp_aux *aux,
    struct disp_edp_backlight_info *bl, disp_u32_t driver_pwm_freq_hz, disp_u8_t edp_dpcd[EDP_DISPLAY_CTL_CAP_SIZE]);
disp_void_t disp_edp_backlight_aux_set_pwmgen_bit(struct disp_dp_aux *aux, struct disp_edp_backlight_info *bl);
disp_void_t disp_edp_backlight_aux_set_pre_freq(struct disp_dp_aux *aux, struct disp_edp_backlight_info *bl);
disp_void_t disp_edp_backlight_aux_enable_freq(struct disp_dp_aux *aux, struct disp_edp_backlight_info *bl);
disp_void_t disp_edp_backlight_aux_get_freq(struct disp_dp_aux *aux, struct disp_edp_backlight_info *bl);
disp_void_t disp_edp_backlight_aux_set_adaptive(struct disp_dp_aux *aux, struct disp_edp_backlight_info *bl);
disp_void_t disp_edp_backlight_aux_enable_adaptive(struct disp_dp_aux *aux, struct disp_edp_backlight_info *bl);
disp_void_t disp_edp_backlight_aux_get_adaptive(struct disp_dp_aux *aux, struct disp_edp_backlight_info *bl);
disp_void_t disp_edp_backlight_aux_get_regional(struct disp_dp_aux *aux, struct disp_edp_backlight_info *bl);
disp_void_t disp_edp_backlight_aux_set_regional(struct disp_dp_aux *aux, struct disp_edp_backlight_info *bl);
disp_void_t disp_edp_backlight_aux_enable_regional(struct disp_dp_aux *aux, struct disp_edp_backlight_info *bl);
disp_void_t disp_edp_backlight_aux_get_caps(struct disp_dp_aux *aux,
    struct disp_edp_backlight_info *bl, disp_u8_t edp_dpcd[EDP_DISPLAY_CTL_CAP_SIZE]);

// psr
struct disp_edp_psr_info {
    // caps
    disp_u8_t version; // 0 means not support, valid version: 1/2/3
    disp_s32_t setup_time_us; // 0 55 110 ... 330
    disp_u16_t su_x_granularity;
    disp_u16_t su_y_granularity;

    // sw prefer flags
    disp_u8_t prefer_frame_start_vsync  : 1; // HW generate frame start: 0-vden, 1-vsync
    disp_u8_t prefer_crc_on             : 1;
    disp_u8_t prefer_main_link_remain   : 1;
    disp_u8_t prefer_capture_next_frame : 1;
    disp_u8_t prefer_sw_sdp             : 1; // 0-fifo psr sdp, 1-hw auto psr sdp
    // caps flags
    disp_u8_t caps_exit_no_lt   : 1; // no need training
    // caps flags eDP 1.4
    disp_u8_t caps_su_y         : 1; // Y-coordinate is required for PSR2 SU
    disp_u8_t coord_granularity : 1; // SU coordinates are required to adhere to the granularity specified
    // set flags
    disp_u8_t set_active        : 1; // call sink to be ready for vsc sdp
    disp_u8_t set_update_rfb    : 1;
    disp_u8_t set_frame_next    : 1; // 0-frame n, 1-frame n+1
    // set eDP 1.4
    disp_u8_t set_su_y          : 1; // Selective Update Region Scan Line Capture
    disp_u8_t set_crc_err_irq   : 1; // generate IRQ_HPD for crc error
};
disp_u8_t disp_edp_psr_aux_is_enable(struct disp_dp_aux *aux);
disp_void_t disp_edp_psr_aux_set(struct disp_dp_aux *aux, struct disp_edp_psr_info *ppsr);

/**
 * struct disp_dp_phy_test_params - DP Phy Compliance parameters
 * @link_rate: Requested Link rate from DPCD 0x219
 * @num_lanes: Number of lanes requested by sing through DPCD 0x220
 * @phy_pattern: DP Phy test pattern from DPCD 0x248
 * @hbr2_reset: DP HBR2_COMPLIANCE_SCRAMBLER_RESET from DCPD 0x24A and 0x24B
 * @custom80: DP Test_80BIT_CUSTOM_PATTERN from DPCDs 0x250 through 0x259
 * @enhanced_frame_cap: flag for enhanced frame capability.
 */
struct disp_dp_phy_test_params {
    int link_rate;
    disp_u8_t num_lanes;
    disp_u8_t phy_pattern;
    disp_u8_t hbr2_reset[2];
    disp_u8_t custom80[10];
    disp_u8_t enhanced_frame_cap;
};

int disp_dp_get_phy_test_pattern(struct disp_dp_aux *aux,
                struct disp_dp_phy_test_params *data);
int disp_dp_set_phy_test_pattern(struct disp_dp_aux *aux,
                struct disp_dp_phy_test_params *data, disp_u8_t dp_rev);
disp_s32_t disp_8b10b_channel_eq_delay_us(disp_u8_t rd_interval);
#endif
