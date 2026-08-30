/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#ifndef __DISP_CODEC_H__
#define __DISP_CODEC_H__

#include "display.h"

typedef struct _disp_stream_format_t
{
    disp_u32_t sample_rate;
    disp_u32_t channel_num;

    //CEA 861 G table 35
    union
    {
        struct
        {
            disp_u32_t map_slot_0 :4;
            disp_u32_t map_slot_1 :4;
            disp_u32_t map_slot_2 :4;
            disp_u32_t map_slot_3 :4;
            disp_u32_t map_slot_4 :4;
            disp_u32_t map_slot_5 :4;
            disp_u32_t map_slot_6 :4;
            disp_u32_t map_slot_7 :4;
            disp_u32_t map_slot_8 :4;
        }map;
        disp_u32_t value;
    }channel_allocation;
}disp_stream_format_t, *pdisp_stream_format_t;

struct clock_ratio_table_t {
	disp_u32_t sample_rate;
	disp_u64_t colck_ratio_27;//27M ref
    disp_u64_t colck_ratio_25;//25M ref
};

typedef enum _disp_csb_sample_frequency_e
{
    disp_csb_sample_frequency_22k05 = 4,
    disp_csb_sample_frequency_44k1 = 0,
    disp_csb_sample_frequency_88k2 = 8,
    disp_csb_sample_frequency_176k4 = 12,

    disp_csb_sample_frequency_24k = 6,
    disp_csb_sample_frequency_48k = 2,
    disp_csb_sample_frequency_96k = 10,
    disp_csb_sample_frequency_192k = 14,

    disp_csb_sample_frequency_not_indicated = 1,
    disp_csb_sample_frequency_32k = 3,
    disp_csb_sample_frequency_768k = 9,
} disp_csb_sample_frequency_e;

typedef enum _disp_csb_sample_word_length_e
{
    disp_csb_sample_word_length_bits_16 = 2,
    disp_csb_sample_word_length_bits_18 = 4,
    disp_csb_sample_word_length_bits_19 = 8,
    disp_csb_sample_word_length_bits_20 = 10,
    disp_csb_sample_word_length_bits_17 = 12,

    disp_csb_sample_word_length_bits_20_max24 = 3,
    disp_csb_sample_word_length_bits_22 = 5,
    disp_csb_sample_word_length_bits_23 = 9,
    disp_csb_sample_word_length_bits_24 = 11,
    disp_csb_sample_word_length_bits_21 = 13,
} disp_csb_sample_word_length_e;

typedef enum _disp_csb_clock_accuracy_e
{
    disp_csb_clock_accuracy_level_2 = 0,
    disp_csb_clock_accuracy_level_1 = 1,
    disp_csb_clock_accuracy_level_3 = 2,
    disp_csb_clock_accuracy_not_matched = 3,
} disp_csb_clock_accuracy_e;

// 192 bits channel status
typedef union _disp_codec_channel_status_t
{
    struct {
        struct {
            disp_u8_t not_consumer_use          : 1;
            disp_u8_t not_linear_pcm            : 1;
            disp_u8_t no_copy_right             : 1;
            disp_u8_t pre_emphasis              : 3;
            disp_u8_t mode_reserved             : 2;
        } byte0;
        disp_u8_t category_code;
        struct {
            disp_u8_t source                    : 4;
            disp_u8_t channel                   : 4;
        } byte2;
        struct {
            disp_u8_t sample_frequency          : 4;
            disp_u8_t clock_accuracy            : 4;
        } byte3;
        struct {
            disp_u8_t sample_word_length        : 4;
            disp_u8_t origin_sample_frequency   : 4;
        } byte4;
        disp_u8_t cgms_a;
        disp_u8_t reserved;
        disp_u8_t reserved1;
    } bits;

    disp_u8_t  u8[24];
    disp_u32_t u32[6];
} disp_codec_channel_status_t, *pdisp_codec_channel_status_t;

typedef enum _disp_codec_stream_bits_e
{
    disp_codec_stream_bits_8  = 0,
    disp_codec_stream_bits_16 = 1,
    disp_codec_stream_bits_20 = 2,
    disp_codec_stream_bits_24 = 3,
    disp_codec_stream_bits_32 = 4,
} disp_codec_stream_bits_e;

typedef struct _disp_codec_converter_t
{
    pdisp_void_t disp_device;
    disp_u8_t encoder_index;
    disp_u8_t present;      //sink is connected, and support audio
    disp_u8_t eld_valid;    //sink is connected, and power on

    disp_u8_t channel_count;
    disp_u8_t stream_id;
    union {
        struct {
            disp_u16_t channel_num          : 4;
            disp_u16_t sample_bits          : 4; // 8/16/20/24/32
            disp_u16_t sample_rate_div      : 3;
            disp_u16_t sample_rate_multi    : 3;
            disp_u16_t sample_rate_base     : 1; // 0-48k, 1-44.1k
            disp_u16_t non_pcm              : 1;
        } bits;
        disp_u16_t u16;
    } stream_format;

    disp_codec_channel_status_t csb;

    union {
        struct {
            disp_u32_t channel_count_valid  : 1;
            disp_u32_t stream_id_valid      : 1;
            disp_u32_t stream_format_valid  : 1;
            disp_u32_t asp_slot_map_valid   : 1;
            disp_u32_t sw_csb_on            : 1;
            disp_u32_t sw_sdp_on            : 1;
            disp_u32_t reserved             :26;
        } bits;
        disp_u32_t val;
    } flags;
} disp_codec_converter_t, *pdisp_codec_converter_t;

disp_void_t disp_codec_hw_init(pdisp_void_t pdisp_device);
disp_void_t disp_codec_hw_deinit(pdisp_void_t pdisp_device);

disp_void_t disp_codec_dpms(pdisp_void_t pdisp_device, pdisp_codec_para_t hdac_para);
disp_void_t disp_codec_irq(pdisp_void_t pdisp_device);

#endif
