/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#include "display.h"
#include "disp_device.h"
#include "disp_codec.h"
#include "disp_reg.h"
#include "CR_SHARE_register.h"
#include "HAU_register.h"
#include "disp_hw_common.h"

#define codec_write_field(reg_name, reg_start, num, ...) \
{ \
    reg32_write_field(reg_name, (reg_start * 4 + pdd->reg_off.hau), num, __VA_ARGS__) \
}

#define codec_read_field(reg_name, reg_start, num, ...) \
{ \
    reg32_read_field(reg_name, (reg_start * 4 + pdd->reg_off.hau), num, __VA_ARGS__) \
}

#define disp_print(level, log, ...) disp_print_ex(DISP_PRINT_MASK_HDAUDIO, level, log, __VA_ARGS__)

static disp_void_t codec_write_u32(pdisp_device_t pdd, disp_u32_t reg_start, disp_u32_t val)
{
    disp_write_u32(pdd, (reg_start * 4 + pdd->reg_off.hau), val);
}

static disp_u32_t codec_read_u32(pdisp_device_t pdd, disp_u32_t reg_start)
{
    return disp_read_u32(pdd, (reg_start * 4 + pdd->reg_off.hau));
}

struct clock_ratio_table_t sample_rate_27_25_ratio[] = {
//sample rate 27M 25M
    {44100,    1795868992, 1939538511},
    {48000,    1954687338, 2111062325},
    {22050,     897934496, 969769255},
    {14700,     598622997, 646512836},
    {11025,     448967248, 484884627},
    {8820,     359173798, 387907701},
    {7350,      299311498, 323256417},
    {6300,      256552713, 277076930},
    {5512,      224463262, 242420322},
    {88200,    3591737984, 3879077022},
    {29400,    1197245994, 1293025673},
    {17640,     718347596, 775815403},
    {12600,     513105426, 554153860},
    {132300,   5387606976, 5818615534},
    {66150,    2693803488, 2909307767},
    {33075,    1346901744, 1454653883},
    {26460,    1077521395, 1163723106},
    {18900,     769658139, 831230790},
    {16537,     673430510, 727304950},
    {176400,   7183475968, 7758154045},
    {58800,    2394491989, 2586051348},
    {35280,    1436695193, 1551630808},
    {25200,    1026210852, 1108307720},
    {24000,     977343669, 1055531162},
    {16000,     651562446, 703687441},
    {12000,     488671834, 527765580},
    {9600,      390937467, 422212464},
    {8000,      325781223, 351843720},
    {6857,      279235230, 301574048},
    {6000,      244335917, 263882790},
    {96000,    3909374676, 4222124650},
    {32000,    1303124892, 1407374883},
    {19200,     781874935, 844424929},
    {13714,     558470461, 603148097},
    {144000,   5864062014, 6333186975},
    {72000,    2932031007, 3166593487},
    {36000,    1466015503, 1583296743},
    {28800,    1172812402, 1266637394},
    {20571,     837705692, 904722147},
    {18000,     733007751, 791648371},
    {192000,   7818749353, 8444249301},
    {64000,    2606249784, 2814749766},
    {38400,    1563749870, 1688849859},
    {27428,    1116940923, 1206296196},
};

/**
* there are 2 methods to get eld data in linux (indel)hda driver
* 1. get eld data from controller via verbs, f2fh
* 2. get eld data via drm component, that's direct communication
*/
static disp_void_t update_eld_fifo(pdisp_codec_converter_t pconverter, disp_u8_t bupdate)
{
    pdisp_device_t pdd = (pdisp_device_t)pconverter->disp_device;
    disp_fifo_para_t lut_fifo = {0};
    pdisp_connector_t pdisp_connector = &pdd->disp_connector[pconverter->encoder_index];

    cr_share_write_field(Reg_Cr_1f, Register_Cr_1f_Start, 1,
        Lut_Pr, 0);

    lut_fifo.fifo_select = 7 + pconverter->encoder_index;
    lut_fifo.size = MAX_ELD_BYTES / 4;
    lut_fifo.data = (pdisp_u32_t)disp_malloc(lut_fifo.size * 4);

    disp_memset(lut_fifo.data, 0, lut_fifo.size * 4);
    if (bupdate)
    {
        disp_prepare_eld_fifo(&lut_fifo, pdisp_connector->audio_info.eld);
    }

    disp_write_fifo(pdd, &lut_fifo);

    disp_free(lut_fifo.data);
    lut_fifo.data = DISP_NULL;

    cr_share_write_field(Reg_Cr_1f, Register_Cr_1f_Start, 1,
        Lut_Pr, 1);
}

static disp_void_t update_eld_status(pdisp_codec_converter_t pconverter)
{
    pdisp_device_t pdd = (pdisp_device_t)pconverter->disp_device;
    disp_u32_t unsol_tag = 0, unsol_res = 0;
    disp_u32_t pin_sense = pconverter->eld_valid | (pconverter->present << 1);
    disp_u32_t power_on = pconverter->eld_valid ? 1 : 0;

    if(pconverter->encoder_index == 0)
    {
        codec_write_field(Reg_Hau_Reg67, Register_Hau_Reg67_Start, 3,
            Pin0_Pinsense, pin_sense, // bit 0 is eld valid, bit 1 is pin sense detect
            Enable_Dip_Pin0, 1,
            Hdaudio_En_Pin0, 1);
        codec_write_field(Reg_Hau_Reg4, Register_Hau_Reg4_Start, 2,
            Ratio_Auto_En_0, 1, // hw ratio
            Ratio_Mult_0, 0); // No multi, 5 -> 0
    }
    else if(pconverter->encoder_index == 1)
    {
        codec_write_field(Reg_Hau_Reg77, Register_Hau_Reg77_Start, 3,
            Pin1_Pinsense, pin_sense,
            Enable_Dip_Pin1, 1,
            Hdaudio_En_Pin1, 1);
        codec_write_field(Reg_Hau_Reg13, Register_Hau_Reg13_Start, 2,
            Ratio_Auto_En_1, 1,
            Ratio_Mult_1, 0);
    }
    else if(pconverter->encoder_index == 2)
    {
        codec_write_field(Reg_Hau_Reg87, Register_Hau_Reg87_Start, 3,
            Pin2_Pinsense, pin_sense,
            Enable_Dip_Pin2, 1,
            Hdaudio_En_Pin2, 1);
        codec_write_field(Reg_Hau_Reg111, Register_Hau_Reg111_Start, 2,
            Ratio_Auto_En_2, 1,
            Ratio_Mult_2, 0);
    }
    else
    {
        codec_write_field(Reg_Hau_Reg97, Register_Hau_Reg97_Start, 3,
            Pin3_Pinsense, pin_sense,
            Enable_Dip_Pin3, 1,
            Hdaudio_En_Pin3, 1);
        codec_write_field(Reg_Hau_Reg120, Register_Hau_Reg120_Start, 2,
            Ratio_Auto_En_3, 1,
            Ratio_Mult_3, 0);
    }

    codec_write_field(Reg_Hau_Reg27, Register_Hau_Reg27_Start, 1,
        Sw_Unsolid_Req, 1); //send unsolicited response

    if(pconverter->encoder_index == 0)
    {
        codec_write_field(Reg_Hau_Reg3, Register_Hau_Reg3_Start, 2,
            Link_Pos_Readmod_0, 1, // 0-codec have send to monitor, 1-memory have read
            Enable_Converter_Channel_0, 1);
        codec_read_field(Reg_Hau_Reg109, Register_Hau_Reg109_Start, 1,
            Pin0_Unsolid_Enable, &unsol_tag);//bit7: enable, bit 5:0: tag
    }
    else if(pconverter->encoder_index == 1)
    {
        codec_write_field(Reg_Hau_Reg12, Register_Hau_Reg12_Start, 2,
            Link_Pos_Readmod_1, 1,
            Enable_Converter_Channel_1, 1);
        codec_read_field(Reg_Hau_Reg109, Register_Hau_Reg109_Start, 1,
            Pin1_Unsolid_Enable, &unsol_tag);//bit7: enable, bit 5:0: tag
    }
    else if(pconverter->encoder_index == 2)
    {
        codec_write_field(Reg_Hau_Reg110, Register_Hau_Reg110_Start, 2,
            Link_Pos_Readmod_2, 1,
            Enable_Converter_Channel_2, 1);
        codec_read_field(Reg_Hau_Reg109, Register_Hau_Reg109_Start, 1,
            Pin2_Unsolid_Enable, &unsol_tag);//bit7: enable, bit 5:0: tag
    }
    else
    {
        codec_write_field(Reg_Hau_Reg119, Register_Hau_Reg119_Start, 2,
            Link_Pos_Readmod_3, 1,
            Enable_Converter_Channel_3, 1);
        codec_read_field(Reg_Hau_Reg109, Register_Hau_Reg109_Start, 1,
            Pin3_Unsolid_Enable, &unsol_tag);//bit7: enable, bit 5:0: tag
    }

    disp_print(DDM_TRACE, "enable unsolicited response = 0x%x\n", unsol_tag);
    unsol_tag &= 0x3F;
    unsol_res = unsol_tag << 26;
    codec_write_field(Reg_Hau_Reg28, Register_Hau_Reg28_Start, 1,
        Sw_Resp, unsol_res);

    //enable codec interrupt
    if (pconverter->encoder_index == 0)
    {
        codec_write_field(Reg_Hau_Reg102, Register_Hau_Reg102_Start, 4,
            set_cov0_strm_fmt, power_on,
            set_cov0_chan_stm_id, power_on,
            set_cov0_channel_count, power_on,
            set_pin0_asp_slot_map, power_on);
    }
    else if (pconverter->encoder_index == 1)
    {
        codec_write_field(Reg_Hau_Reg102, Register_Hau_Reg102_Start, 4,
            set_cov1_strm_fmt, power_on,
            set_cov1_chan_stm_id, power_on,
            set_cov1_channel_count, power_on,
            set_pin1_asp_slot_map, power_on);
    }
    else if (pconverter->encoder_index == 2)
    {
        codec_write_field(Reg_Hau_Reg103, Register_Hau_Reg103_Start, 4,
            set_cov2_strm_fmt, power_on,
            set_cov2_chan_stm_id, power_on,
            set_cov2_channel_count, power_on,
            set_pin2_asp_slot_map, power_on);
    }
    else
    {
        codec_write_field(Reg_Hau_Reg103, Register_Hau_Reg103_Start, 4,
            set_cov3_strm_fmt, power_on,
            set_cov3_chan_stm_id, power_on,
            set_cov3_channel_count, power_on,
            set_pin3_asp_slot_map, power_on);
    }
}

static disp_void_t disp_codec_force_8ch_data(pdisp_codec_converter_t pconverter)
{
    pdisp_device_t pdd = (pdisp_device_t)pconverter->disp_device;
    disp_u32_t offset = (Register_Hau_Reg65_Start + pconverter->encoder_index * 10); // 65 75 85 95
    disp_u32_t pin_slot_map = 0x01010101;

    codec_write_u32(pdd, offset, pin_slot_map);
}

disp_void_t disp_codec_hw_init(pdisp_void_t pdisp_device)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_device;
    disp_u8_t index = 0;

    /* Duration_48k = refclk / 48k
     * refclk 25M -> 27M, So Duration_48k: 520(520.8333) -> 562(562.5)
     */
    codec_write_field(Reg_Hau_Reg106, Register_Hau_Reg106_Start, 1,
        Duration_48k, 562);

    codec_write_field(Reg_Hau_Reg0, Register_Hau_Reg0_Start, 1,
        Clk_Sel, 0); // 0-27M, 1-25M

    for (index = 0; index < DISP_ENCODER_NUM; index++)
    {
        update_eld_fifo(pdd->disp_encoder[index].pcodec_converter, DISP_FALSE);
    }
}

disp_void_t disp_codec_hw_deinit(pdisp_void_t pdisp_device)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_device;
    disp_u8_t index = 0;

    for (index = 0; index < DISP_ENCODER_NUM; index++)
    {
        update_eld_fifo(pdd->disp_encoder[index].pcodec_converter, DISP_FALSE);
    }
}

disp_void_t disp_codec_dpms(pdisp_void_t pdisp_device, pdisp_codec_para_t hdac_para)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_device;
    pdisp_codec_converter_t pconverter = pdd->disp_encoder[hdac_para->encoder_index].pcodec_converter;

    pconverter->present = hdac_para->present;
    pconverter->eld_valid = hdac_para->eld_valid;

    update_eld_fifo(pconverter, hdac_para->power ? DISP_TRUE : DISP_FALSE);
    update_eld_status(pconverter);
}

static disp_void_t disp_codec_irq_prepare(pdisp_device_t pdd, Reg_Hau_Reg104 hau_104, Reg_Hau_Reg105 hau_105)
{
    pdisp_codec_converter_t pconverter[4];
    Reg_Hau_Reg33 hau_33;
    disp_s32_t idx = 0;
    disp_u32_t log_level = DDM_TRACE;

    for (idx = 0; idx < DISP_ENCODER_NUM; idx++)
    {
        pconverter[idx] = pdd->disp_encoder[idx].pcodec_converter;
    }

    if (hau_104.reg.set_cov0_strm_fmt || hau_104.reg.set_cov1_strm_fmt)
    {
        codec_write_field(Reg_Hau_Reg27, Register_Hau_Reg27_Start, 1,
            Read_Out_Sel, 10);
        codec_read_field(Reg_Hau_Reg33, Register_Hau_Reg33_Start, 1,
            Codec_Read_Out, &hau_33.uint);

        if(hau_104.reg.set_cov0_strm_fmt)
        {
            pconverter[0]->stream_format.u16 = hau_33.uint & 0xFFFF;
            pconverter[0]->flags.bits.stream_format_valid = 1;
            disp_print(log_level, "convert 0 stream format = 0x%x\n", pconverter[0]->stream_format.u16);
        }

        if(hau_104.reg.set_cov1_strm_fmt)
        {
            pconverter[1]->stream_format.u16 = hau_33.uint >> 16;
            pconverter[1]->flags.bits.stream_format_valid = 1;
            disp_print(log_level, "convert 1 stream format = 0x%x\n", pconverter[1]->stream_format.u16);
        }
    }
    if (hau_105.reg.set_cov2_strm_fmt || hau_105.reg.set_cov3_strm_fmt)
    {
        codec_write_field(Reg_Hau_Reg27, Register_Hau_Reg27_Start, 1,
            Read_Out_Sel, 9);
        codec_read_field(Reg_Hau_Reg33, Register_Hau_Reg33_Start, 1,
            Codec_Read_Out, &hau_33.uint);

        if (hau_105.reg.set_cov2_strm_fmt)
        {
            pconverter[2]->stream_format.u16 = hau_33.uint & 0xFFFF;
            pconverter[2]->flags.bits.stream_format_valid = 1;
            disp_print(log_level, "convert 2 stream format = 0x%x\n", pconverter[2]->stream_format.u16);
        }
        else
        {
            pconverter[3]->stream_format.u16 = hau_33.uint >> 16;
            pconverter[3]->flags.bits.stream_format_valid = 1;
            disp_print(log_level, "convert 3 stream format = 0x%x\n", pconverter[1]->stream_format.u16);
        }
    }

    if (hau_104.reg.set_conv0_channel_count || hau_104.reg.set_conv1_channel_count ||
        hau_105.reg.set_conv2_channel_count || hau_105.reg.set_conv3_channel_count)
    {
        codec_write_field(Reg_Hau_Reg27, Register_Hau_Reg27_Start, 1,
            Read_Out_Sel, 11);
        codec_read_field(Reg_Hau_Reg33, Register_Hau_Reg33_Start, 1,
            Codec_Read_Out, &hau_33.uint);

        if (hau_104.reg.set_conv0_channel_count)
        {
            pconverter[0]->channel_count = (hau_33.uint & 0xFF) + 1;
            pconverter[0]->flags.bits.channel_count_valid = 1;
            disp_print(log_level, "convert 0 channel count = %d\n", pconverter[0]->channel_count);
        }
        if (hau_104.reg.set_conv1_channel_count)
        {
            pconverter[1]->channel_count = ((hau_33.uint >> 8) & 0xFF) + 1;
            pconverter[1]->flags.bits.channel_count_valid = 1;
            disp_print(log_level, "convert 1 channel count = %d\n", pconverter[1]->channel_count);
        }
        if (hau_105.reg.set_conv2_channel_count)
        {
            pconverter[2]->channel_count = ((hau_33.uint >> 16) & 0xFF) + 1;
            pconverter[2]->flags.bits.channel_count_valid = 1;
            disp_print(log_level, "convert 2 channel count = %d\n", pconverter[2]->channel_count);
        }
        if (hau_105.reg.set_conv3_channel_count)
        {
            pconverter[3]->channel_count = ((hau_33.uint >> 24) & 0xFF) + 1;
            pconverter[3]->flags.bits.channel_count_valid = 1;
            disp_print(log_level, "convert 3 channel count = %d\n", pconverter[3]->channel_count);
        }
    }

    if (hau_104.reg.set_cov0_chan_stm_id || hau_104.reg.set_cov1_chan_stm_id ||
        hau_105.reg.set_cov2_chan_stm_id || hau_105.reg.set_cov3_chan_stm_id)
    {
        codec_write_field(Reg_Hau_Reg27, Register_Hau_Reg27_Start, 1,
            Read_Out_Sel, 1);
        codec_read_field(Reg_Hau_Reg33, Register_Hau_Reg33_Start, 1,
            Codec_Read_Out, &hau_33.uint);

        if (hau_104.reg.set_cov0_chan_stm_id)
        {
            pconverter[0]->stream_id = hau_33.uint & 0xFF;
            pconverter[0]->flags.bits.stream_id_valid = 1;
            disp_print(log_level, "convert 0 stream id = %d\n", pconverter[0]->stream_id);
        }
        if (hau_104.reg.set_cov1_chan_stm_id)
        {
            pconverter[1]->stream_id = (hau_33.uint >> 8) & 0xFF;
            pconverter[1]->flags.bits.stream_id_valid = 1;
            disp_print(log_level, "convert 1 stream id = %d\n", pconverter[1]->stream_id);
        }
        if (hau_105.reg.set_cov2_chan_stm_id)
        {
            pconverter[2]->stream_id = (hau_33.uint >> 16) & 0xFF;
            pconverter[2]->flags.bits.stream_id_valid = 1;
            disp_print(log_level, "convert 2 stream id = %d\n", pconverter[2]->stream_id);
        }
        if (hau_105.reg.set_cov3_chan_stm_id)
        {
            pconverter[3]->stream_id = (hau_33.uint >> 24) & 0xFF;
            pconverter[3]->flags.bits.stream_id_valid = 1;
            disp_print(log_level, "convert 3 stream id = %d\n", pconverter[3]->stream_id);
        }
    }

    pconverter[0]->flags.bits.asp_slot_map_valid = hau_104.reg.set_pin0_asp_slot_map ? 1 : 0;
    pconverter[1]->flags.bits.asp_slot_map_valid = hau_104.reg.set_pin1_asp_slot_map ? 1 : 0;
    pconverter[2]->flags.bits.asp_slot_map_valid = hau_105.reg.set_pin2_asp_slot_map ? 1 : 0;
    pconverter[3]->flags.bits.asp_slot_map_valid = hau_105.reg.set_pin3_asp_slot_map ? 1 : 0;

    // TODO: power, pin
}

static disp_void_t disp_codec_csb_converter_set(pdisp_codec_converter_t pconverter)
{
    disp_u8_t sw_csb_on = DISP_FALSE;
    disp_csb_sample_frequency_e csb_sample_frequency = disp_csb_sample_frequency_48k;

    if (pconverter->stream_format.bits.non_pcm)
    {
        disp_print(DDM_INFO, "disp_codec[%u]: non_pcm, unexpected!\n", pconverter->encoder_index);
    }
    else if (pconverter->stream_format.bits.channel_num + 1 > 2)
    {
        disp_print(DDM_INFO, "disp_codec[%u]: channel_num %u > 2, unexpected!\n",
            pconverter->encoder_index, pconverter->stream_format.bits.channel_num + 1);
    }
    else if (pconverter->stream_format.bits.sample_rate_multi && pconverter->stream_format.bits.sample_rate_div)
    {
        disp_print(DDM_INFO, "disp_codec[%u]: sample_rate multi[%u] and div[%u], unexpected!\n", pconverter->encoder_index,
            pconverter->stream_format.bits.sample_rate_multi, pconverter->stream_format.bits.sample_rate_div);
    }
    else if (pconverter->stream_format.bits.sample_rate_multi == 2 ||
        pconverter->stream_format.bits.sample_rate_multi > 3 ||
        pconverter->stream_format.bits.sample_rate_div > 1)
    {
        disp_print(DDM_INFO, "disp_codec[%u]: sample_rate multi[%u] and div[%u], not support!\n", pconverter->encoder_index,
            pconverter->stream_format.bits.sample_rate_multi, pconverter->stream_format.bits.sample_rate_div);
    }
    else
    {
        // default value
        sw_csb_on = DISP_TRUE;
        pconverter->csb.bits.byte0.no_copy_right = 1;
        pconverter->csb.bits.byte3.clock_accuracy = disp_csb_clock_accuracy_level_2;

        if (pconverter->stream_format.bits.sample_rate_base) // 44.1k
        {
            if (1 == pconverter->stream_format.bits.sample_rate_multi)
            {
                csb_sample_frequency = disp_csb_sample_frequency_88k2;
            }
            else if (3 == pconverter->stream_format.bits.sample_rate_multi)
            {
                csb_sample_frequency = disp_csb_sample_frequency_176k4;
            }
            else if (pconverter->stream_format.bits.sample_rate_div)
            {
                csb_sample_frequency = disp_csb_sample_frequency_22k05;
            }
            else
            {
                csb_sample_frequency = disp_csb_sample_frequency_44k1;
            }
        }
        else // 48k
        {
            if (1 == pconverter->stream_format.bits.sample_rate_multi)
            {
                csb_sample_frequency = disp_csb_sample_frequency_96k;
            }
            else if (3 == pconverter->stream_format.bits.sample_rate_multi)
            {
                csb_sample_frequency = disp_csb_sample_frequency_192k;
            }
            else if (pconverter->stream_format.bits.sample_rate_div)
            {
                csb_sample_frequency = disp_csb_sample_frequency_24k;
            }
            else
            {
                csb_sample_frequency = disp_csb_sample_frequency_48k;
            }
        }
        pconverter->csb.bits.byte3.sample_frequency = csb_sample_frequency;

        switch (pconverter->stream_format.bits.sample_bits)
        {
        case disp_codec_stream_bits_16:
            pconverter->csb.bits.byte4.sample_word_length = disp_csb_sample_word_length_bits_16;
            break;
        case disp_codec_stream_bits_20:
            pconverter->csb.bits.byte4.sample_word_length = disp_csb_sample_word_length_bits_20;
            break;
        case disp_codec_stream_bits_24:
            pconverter->csb.bits.byte4.sample_word_length = disp_csb_sample_word_length_bits_24;
            break;
        case disp_codec_stream_bits_8:
        case disp_codec_stream_bits_32:
        default:
            disp_print(DDM_INFO, "disp_codec[%u]: sample bits[%u] not support!\n",
                pconverter->encoder_index, pconverter->stream_format.bits.sample_bits);
            sw_csb_on = DISP_FALSE;
            break;
        }
    }

    disp_print(DDM_TRACE, "disp_codec[%u]: sw_csb_on[%u] sample rate[%x] len[%x], csb[%x %x %x %x %x %x]\n",
        pconverter->encoder_index, sw_csb_on,
        pconverter->csb.bits.byte3.sample_frequency,
        pconverter->csb.bits.byte4.sample_word_length,
        pconverter->csb.u32[0], pconverter->csb.u32[1], pconverter->csb.u32[2],
        pconverter->csb.u32[3], pconverter->csb.u32[4], pconverter->csb.u32[5]);

    pconverter->flags.bits.sw_csb_on |= sw_csb_on ? 1 : 0;
}

static disp_void_t disp_codec_csb_converter_flush(pdisp_codec_converter_t pconverter)
{
    pdisp_device_t pdd = (pdisp_device_t)pconverter->disp_device;
    disp_u8_t sw_csb_on = pconverter->flags.bits.sw_csb_on ? DISP_TRUE : DISP_FALSE;

    switch (pconverter->encoder_index)
    {
    case 0:
        codec_write_field(Reg_Hau_Reg3, Register_Hau_Reg3_Start, 1,
            Channel_Sts_Sel_0, sw_csb_on ? 3 : 0);
        if (sw_csb_on)
        {
            codec_write_field(Reg_Hau_Reg6, Register_Hau_Reg6_Start, 1,
                Channel_Sts_0_Part0, pconverter->csb.u32[0]);
            codec_write_field(Reg_Hau_Reg7, Register_Hau_Reg7_Start, 1,
                Channel_Sts_0_Part1, pconverter->csb.u32[1]);
            codec_write_field(Reg_Hau_Reg8, Register_Hau_Reg8_Start, 1,
                Channel_Sts_0_Part2, pconverter->csb.u32[2]);
            codec_write_field(Reg_Hau_Reg9, Register_Hau_Reg9_Start, 1,
                Channel_Sts_0_Part3, pconverter->csb.u32[3]);
            codec_write_field(Reg_Hau_Reg10, Register_Hau_Reg10_Start, 1,
                Channel_Sts_0_Part4, pconverter->csb.u32[4]);
            codec_write_field(Reg_Hau_Reg11, Register_Hau_Reg11_Start, 1,
                Channel_Sts_0_Part5, pconverter->csb.u32[5]);
        }
        break;

    case 1:
        codec_write_field(Reg_Hau_Reg12, Register_Hau_Reg12_Start, 1,
            Channel_Sts_Sel_1, sw_csb_on ? 3 : 0);
        if (sw_csb_on)
        {
            codec_write_field(Reg_Hau_Reg15, Register_Hau_Reg15_Start, 1,
                Channel_Sts_1_Part0, pconverter->csb.u32[0]);
            codec_write_field(Reg_Hau_Reg16, Register_Hau_Reg16_Start, 1,
                Channel_Sts_1_Part1, pconverter->csb.u32[1]);
            codec_write_field(Reg_Hau_Reg17, Register_Hau_Reg17_Start, 1,
                Channel_Sts_1_Part2, pconverter->csb.u32[2]);
            codec_write_field(Reg_Hau_Reg18, Register_Hau_Reg18_Start, 1,
                Channel_Sts_1_Part3, pconverter->csb.u32[3]);
            codec_write_field(Reg_Hau_Reg19, Register_Hau_Reg19_Start, 1,
                Channel_Sts_1_Part4, pconverter->csb.u32[4]);
            codec_write_field(Reg_Hau_Reg20, Register_Hau_Reg20_Start, 1,
                Channel_Sts_1_Part5, pconverter->csb.u32[5]);
        }
        break;

    case 2:
        codec_write_field(Reg_Hau_Reg110, Register_Hau_Reg110_Start, 1,
            Channel_Sts_Sel_2, sw_csb_on ? 3 : 0);
        if (sw_csb_on)
        {
            codec_write_field(Reg_Hau_Reg113, Register_Hau_Reg113_Start, 1,
                Channel_Sts_2_Part0, pconverter->csb.u32[0]);
            codec_write_field(Reg_Hau_Reg114, Register_Hau_Reg114_Start, 1,
                Channel_Sts_2_Part1, pconverter->csb.u32[1]);
            codec_write_field(Reg_Hau_Reg115, Register_Hau_Reg115_Start, 1,
                Channel_Sts_2_Part2, pconverter->csb.u32[2]);
            codec_write_field(Reg_Hau_Reg116, Register_Hau_Reg116_Start, 1,
                Channel_Sts_2_Part3, pconverter->csb.u32[3]);
            codec_write_field(Reg_Hau_Reg117, Register_Hau_Reg117_Start, 1,
                Channel_Sts_2_Part4, pconverter->csb.u32[4]);
            codec_write_field(Reg_Hau_Reg118, Register_Hau_Reg118_Start, 1,
                Channel_Sts_2_Part5, pconverter->csb.u32[5]);
        }
        break;

    case 3:
        codec_write_field(Reg_Hau_Reg119, Register_Hau_Reg119_Start, 1,
            Channel_Sts_Sel_3, sw_csb_on ? 3 : 0);
        if (sw_csb_on)
        {
            codec_write_field(Reg_Hau_Reg122, Register_Hau_Reg122_Start, 1,
                Channel_Sts_3_Part0, pconverter->csb.u32[0]);
            codec_write_field(Reg_Hau_Reg123, Register_Hau_Reg123_Start, 1,
                Channel_Sts_3_Part1, pconverter->csb.u32[1]);
            codec_write_field(Reg_Hau_Reg124, Register_Hau_Reg124_Start, 1,
                Channel_Sts_3_Part2, pconverter->csb.u32[2]);
            codec_write_field(Reg_Hau_Reg125, Register_Hau_Reg125_Start, 1,
                Channel_Sts_3_Part3, pconverter->csb.u32[3]);
            codec_write_field(Reg_Hau_Reg126, Register_Hau_Reg126_Start, 1,
                Channel_Sts_3_Part4, pconverter->csb.u32[4]);
            codec_write_field(Reg_Hau_Reg127, Register_Hau_Reg127_Start, 1,
                Channel_Sts_3_Part5, pconverter->csb.u32[5]);
        }
        break;

    default:
        break;
    }
}

disp_void_t disp_codec_set_audio_info(pdisp_codec_converter_t pconverter)
{
    disp_audio_infoframe_t audio;
    pdisp_device_t pdd = (pdisp_device_t)pconverter->disp_device;
    disp_u32_t enable_dip_pin = 0;
    disp_u8_t index = pconverter->encoder_index;

    disp_memset(&audio, 0, sizeof(disp_audio_infoframe_t));
    if (pconverter->channel_count > 2) // only support 2 channel
    {
        audio.valid = 0;
    }
    else
    {
        audio.valid = 1;
        audio.coding_type = 1; // L-PCM
        audio.channel_count = pconverter->channel_count;
        // Sample bits/frequency not indicated here, just keep bits 0.
    }

    // sw fifo or hw dip fifo
    enable_dip_pin = audio.valid ? 0 : 1;
    switch (index)
    {
    case 0:
        codec_write_field(Reg_Hau_Reg67, Register_Hau_Reg67_Start, 1,
            Enable_Dip_Pin0, enable_dip_pin);
        break;
    case 1:
        codec_write_field(Reg_Hau_Reg77, Register_Hau_Reg77_Start, 1,
            Enable_Dip_Pin1, enable_dip_pin);
        break;
    case 2:
        codec_write_field(Reg_Hau_Reg87, Register_Hau_Reg87_Start, 1,
            Enable_Dip_Pin2, enable_dip_pin);
        break;
    case 3:
        codec_write_field(Reg_Hau_Reg97, Register_Hau_Reg97_Start, 1,
            Enable_Dip_Pin3, enable_dip_pin);
        break;
    default:
        break;
    }

    /* Double check: may need security lock as different thread conflict. */
    pdd->disp_encoder[index].func->set_audio_info(&pdd->disp_encoder[index], &audio);
}

disp_void_t disp_codec_irq(pdisp_void_t pdisp_device)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_device;
    pdisp_codec_converter_t pconverter = DISP_NULL;
    Reg_Hau_Reg104 hau_104;
    Reg_Hau_Reg105 hau_105;
    disp_u32_t hau_off = pdd->reg_off.hau;
    disp_s32_t idx = 0;

    hau_104.uint = disp_read_u32(pdisp_device, hau_off + Register_Hau_Reg104_Start * 4);
    disp_write_u32(pdisp_device, hau_off + Register_Hau_Reg104_Start * 4, hau_104.uint);
    hau_105.uint = disp_read_u32(pdisp_device, hau_off + Register_Hau_Reg105_Start * 4);
    disp_write_u32(pdisp_device, hau_off + Register_Hau_Reg105_Start * 4, hau_105.uint);
    disp_print(DDM_TRACE, "disp_codec: irq %x %x\n", hau_104.uint, hau_105.uint);

    disp_codec_irq_prepare(pdd, hau_104, hau_105);
    for (idx = 0; idx < DISP_ENCODER_NUM; idx++)
    {
        pconverter = pdd->disp_encoder[idx].pcodec_converter;

        if (pconverter->flags.bits.stream_format_valid)
        {
            pconverter->flags.bits.stream_format_valid = 0;
            if (pdd->flag.info.hau_sw_csb)
            {
                disp_codec_csb_converter_set(pconverter);
                disp_codec_csb_converter_flush(pconverter);
            }
            if (pdd->flag.info.hau_sw_info)
            {
                disp_codec_set_audio_info(pconverter);
            }
        }

        if (pconverter->flags.bits.channel_count_valid)
        {
            pconverter->flags.bits.channel_count_valid = 0;
            // not use.
        }

        if (pconverter->flags.bits.stream_id_valid)
        {
            pconverter->flags.bits.stream_id_valid = 0;
            // not use.
        }

        if (pdd->flag.info.hau_asp_8ch
            && pconverter->flags.bits.asp_slot_map_valid
            && 2 == pconverter->channel_count
            //&& 1 == pconverter->stream_format.bits.channel_num // Not reliable as some AP may set 0 ch_num
            )
        {
            disp_print(DDM_TRACE, "disp_codec[%u]: irq force 8ch data, ch_cnt[%u] ch_num[%u]\n",
                pconverter->encoder_index, pconverter->channel_count, pconverter->stream_format.bits.channel_num);
            disp_codec_force_8ch_data(pconverter);
        }
    }
}
