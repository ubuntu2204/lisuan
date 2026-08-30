/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#include "display.h"
#include "disp_callback.h"
#include "disp_vbt.h"

#define disp_print(level, log, ...) disp_print_ex(DISP_PRINT_MASK_VBT, level, log, __VA_ARGS__)

disp_s8_t disp_check_rom_header(pdisp_void_t pdisp_device)
{
    if (disp_read_rom_byte(pdisp_device, 0)!= 0x55 || disp_read_rom_byte(pdisp_device,1)!= 0xAA) 
    {
        disp_print(DDM_ERROR, "rom sgnt error, should be 0x55 0xAA, now is 0x%x 0x%x\n"
            , disp_read_rom_byte(pdisp_device,0), disp_read_rom_byte(pdisp_device,1));
        return 0;
    }
    else
    {
        disp_print(DDM_INFO, "rom sgnt byte right: 0x%x 0x%x\n"
            , disp_read_rom_byte(pdisp_device,0), disp_read_rom_byte(pdisp_device,1));
        return -1;
    }
}
