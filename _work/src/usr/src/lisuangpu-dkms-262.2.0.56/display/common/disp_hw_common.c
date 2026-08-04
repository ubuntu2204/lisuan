/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#include "disp_device.h"
#include "disp_hw_common.h"
#include "disp_reg.h"
#include "CR_SHARE_register.h"

#define FIFO_INDEX_REG 0xCFF0
#define FIFO_DATA_REG 0xCFF4

#define disp_print(level, log, ...) disp_print_ex(DISP_PRINT_MASK_COMMON, level, log, ##__VA_ARGS__)

disp_void_t disp_prepare_gamma_fifo(pdisp_fifo_para_t pfifo, pdisp_gamma_info_t gamma_info)
{
    disp_u32_t i = 0;
    disp_u32_t temp;
    disp_u16_t R = 0, G = 0, B = 0;
    disp_u32_t size = gamma_info->lut_length;

    for(i = 0; i < size; i++)
    {
        if(gamma_info->gamma_bit == 10)
        {
            R = gamma_info->data.pgamma_ramp_rgb->Red[i] & 0x3FF;
            G = gamma_info->data.pgamma_ramp_rgb->Green[i] & 0x3FF;
            B = gamma_info->data.pgamma_ramp_rgb->Blue[i] & 0x3FF;
        }
        else if(gamma_info->gamma_bit == 8)
        {
            R = (gamma_info->data.pgamma_ramp_rgb->Red[i] & 0xFF) << 2 | ((gamma_info->data.pgamma_ramp_rgb->Red[i] & 0xC0) >> 6);
            G = (gamma_info->data.pgamma_ramp_rgb->Green[i] & 0xFF) << 2 | ((gamma_info->data.pgamma_ramp_rgb->Green[i] & 0xC0) >> 6);
            B = (gamma_info->data.pgamma_ramp_rgb->Blue[i] & 0xFF) << 2 | ((gamma_info->data.pgamma_ramp_rgb->Blue[i] & 0xC0) >> 6);
        }

        temp = (R << 20) | (G << 10) | (B);
        *(pfifo->data + i) = temp;
    }

    pfifo->size = size;
    pfifo->index = 0;
    pfifo->order = 0;
}

//every 32byte(256bit) is a set
//for every set, should write 32nd byte firstly, then 31nd byte, 1st byte last
//write 4 byte one time
//data 0 = D31 << 24 | D30 << 16 | D29 << 8 | D28
//data 1 = D27 << 24 | D26 << 16 | D25 << 8 | D24
//...
//data 7 = D3 << 24 | D2 << 16 | D1 << 8 | D0
#define FIFO_SET_SIZE 32
disp_void_t disp_prepare_eld_fifo(pdisp_fifo_para_t pfifo, pdisp_u8_t peld)
{
    disp_s32_t i = 0;
    disp_u32_t j = 0, set_num = MAX_ELD_BYTES / FIFO_SET_SIZE;

    for(j = 0; j < set_num; j++)
    {
        for(i = FIFO_SET_SIZE - 1; i >= 0;)
        {
            *(pfifo->data +  (FIFO_SET_SIZE - i - 1) / 4  + (j * FIFO_SET_SIZE / 4)) = peld[i - 3 + j * FIFO_SET_SIZE] |
                                                    (peld[i - 2 + j * FIFO_SET_SIZE] << 8) |
                                                    (peld[i - 1  + j * FIFO_SET_SIZE] << 16) |
                                                    (peld[i + j * FIFO_SET_SIZE] << 24);
            i -= 4;
        }
    }

    pfifo->size = MAX_ELD_BYTES / 4;
    pfifo->index = 0;
    pfifo->order = 0;
}

disp_void_t disp_write_fifo(pdisp_void_t pdisp_device, pdisp_fifo_para_t pfifo)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_device;
    disp_u32_t i = 0;

//need add lock later

    cr_share_write_field(Reg_Cr_28, Register_Cr_28_Start, 1,
        Lut_Info_Sel, pfifo->fifo_select);

    disp_write_u32(pdd, FIFO_INDEX_REG, pfifo->index);//index auto-increments

    if(pfifo->order == 0)
    {
        for(i = 0; i < pfifo->size; i++)
        {
            disp_write_u32(pdd, FIFO_DATA_REG, *(pfifo->data + i));
        }
    }
//unlock
}

disp_void_t disp_read_fifo(pdisp_void_t pdisp_device, pdisp_fifo_para_t pfifo)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_device;
    disp_u32_t i = 0;

    cr_share_write_field(Reg_Cr_28, Register_Cr_28_Start, 1,
        Lut_Info_Sel, pfifo->fifo_select);

    disp_write_u32(pdd, FIFO_INDEX_REG, pfifo->index);

    for(i = 0; i < pfifo->size; i++)
    {
        *(pfifo->data + i) = disp_read_u32(pdd, FIFO_DATA_REG);
    }
}
