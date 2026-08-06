/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#include "disp_device.h"
#include "disp_reg.h"
#include "PLNS_register.h"

#ifndef __DISP_GOP__

#define plane_hdr_write_field(crtc_index, plane_index, reg_name, reg_start, num, ...) \
{ \
    reg32_write_field(reg_name, (reg_start * 4 + pdd->reg_off.plns[crtc_index] + pdd->reg_off.layer_hdr[plane_index]), num, __VA_ARGS__) \
}

#define disp_print(level, log, ...) disp_print_ex(DISP_PRINT_MASK_HDR, level, log, ##__VA_ARGS__)
#define disp_print_pure(level, log, ...) disp_print_pure_ex(DISP_PRINT_MASK_HDR, level, log, ##__VA_ARGS__)

disp_u8_t disp_hdr_check(pdisp_void_t pplane, pdisp_hdr_module_t pnew, disp_u8_t hdr_module_valid)
{
    pdisp_plane_t pdisp_plane = (pdisp_plane_t)pplane;
    pdisp_hdr_module_t pold_module = &pdisp_plane->hdr_module;

    if (hdr_module_valid && (disp_memcmp(pold_module, pnew, sizeof(disp_hdr_module_t)) != 0))
    {
        disp_memcpy(pold_module, pnew, sizeof(disp_hdr_module_t));
        pdisp_plane->flag.bits.flush_hdr_module = 1;
        return DISP_TRUE;
    }

    pdisp_plane->flag.bits.flush_hdr_module = 0;
    return DISP_FALSE;
}

DISP_STATUS disp_hdr_flush(pdisp_void_t pplane)
{
    pdisp_plane_t pdisp_plane = (pdisp_plane_t)pplane;
    pdisp_device_t pdd = (pdisp_device_t)pdisp_plane->disp_device;
    pdisp_hdr_module_t phdr = &pdisp_plane->hdr_module;

    // hdr en
    plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_30, Register_Plns_Mmio_30_Start, 1,
        Hdr_En, phdr->hdr_en);

    if (phdr->hdr_en)
    {
        // degamma
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_31, Register_Plns_Mmio_31_Start, 1,
            Hdr_Dg_Luty_00, phdr->dg_lut_y[0]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_32, Register_Plns_Mmio_32_Start, 1,
            Hdr_Dg_Luty_01, phdr->dg_lut_y[1]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_33, Register_Plns_Mmio_33_Start, 1,
            Hdr_Dg_Luty_02, phdr->dg_lut_y[2]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_34, Register_Plns_Mmio_34_Start, 1,
            Hdr_Dg_Luty_03, phdr->dg_lut_y[3]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_35, Register_Plns_Mmio_35_Start, 1,
            Hdr_Dg_Luty_04, phdr->dg_lut_y[4]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_36, Register_Plns_Mmio_36_Start, 1,
            Hdr_Dg_Luty_05, phdr->dg_lut_y[5]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_37, Register_Plns_Mmio_37_Start, 1,
            Hdr_Dg_Luty_06, phdr->dg_lut_y[6]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_38, Register_Plns_Mmio_38_Start, 1,
            Hdr_Dg_Luty_07, phdr->dg_lut_y[7]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_39, Register_Plns_Mmio_39_Start, 1,
            Hdr_Dg_Luty_08, phdr->dg_lut_y[8]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_3a, Register_Plns_Mmio_3a_Start, 1,
            Hdr_Dg_Luty_09, phdr->dg_lut_y[9]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_3b, Register_Plns_Mmio_3b_Start, 1,
            Hdr_Dg_Luty_10, phdr->dg_lut_y[10]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_3c, Register_Plns_Mmio_3c_Start, 1,
            Hdr_Dg_Luty_11, phdr->dg_lut_y[11]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_3d, Register_Plns_Mmio_3d_Start, 1,
            Hdr_Dg_Luty_12, phdr->dg_lut_y[12]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_3e, Register_Plns_Mmio_3e_Start, 1,
            Hdr_Dg_Luty_13, phdr->dg_lut_y[13]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_3f, Register_Plns_Mmio_3f_Start, 1,
            Hdr_Dg_Luty_14, phdr->dg_lut_y[14]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_40, Register_Plns_Mmio_40_Start, 1,
            Hdr_Dg_Luty_15, phdr->dg_lut_y[15]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_41, Register_Plns_Mmio_41_Start, 1,
            Hdr_Dg_Luty_16, phdr->dg_lut_y[16]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_42, Register_Plns_Mmio_42_Start, 1,
            Hdr_Dg_Luty_17, phdr->dg_lut_y[17]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_43, Register_Plns_Mmio_43_Start, 1,
            Hdr_Dg_Luty_18, phdr->dg_lut_y[18]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_44, Register_Plns_Mmio_44_Start, 1,
            Hdr_Dg_Luty_19, phdr->dg_lut_y[19]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_45, Register_Plns_Mmio_45_Start, 1,
            Hdr_Dg_Luty_20, phdr->dg_lut_y[20]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_46, Register_Plns_Mmio_46_Start, 1,
            Hdr_Dg_Luty_21, phdr->dg_lut_y[21]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_47, Register_Plns_Mmio_47_Start, 1,
            Hdr_Dg_Luty_22, phdr->dg_lut_y[22]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_48, Register_Plns_Mmio_48_Start, 1,
            Hdr_Dg_Luty_23, phdr->dg_lut_y[23]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_49, Register_Plns_Mmio_49_Start, 1,
            Hdr_Dg_Luty_24, phdr->dg_lut_y[24]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_4a, Register_Plns_Mmio_4a_Start, 1,
            Hdr_Dg_Luty_25, phdr->dg_lut_y[25]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_4b, Register_Plns_Mmio_4b_Start, 1,
            Hdr_Dg_Luty_26, phdr->dg_lut_y[26]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_4c, Register_Plns_Mmio_4c_Start, 1,
            Hdr_Dg_Luty_27, phdr->dg_lut_y[27]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_4d, Register_Plns_Mmio_4d_Start, 1,
            Hdr_Dg_Luty_28, phdr->dg_lut_y[28]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_4e, Register_Plns_Mmio_4e_Start, 1,
            Hdr_Dg_Luty_29, phdr->dg_lut_y[29]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_4f, Register_Plns_Mmio_4f_Start, 1,
            Hdr_Dg_Luty_30, phdr->dg_lut_y[30]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_50, Register_Plns_Mmio_50_Start, 1,
            Hdr_Dg_Luty_31, phdr->dg_lut_y[31]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_51, Register_Plns_Mmio_51_Start, 1,
            Hdr_Dg_Luty_32, phdr->dg_lut_y[32]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_52, Register_Plns_Mmio_52_Start, 1,
            Hdr_Dg_Luty_33, phdr->dg_lut_y[33]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_53, Register_Plns_Mmio_53_Start, 1,
            Hdr_Dg_Luty_34, phdr->dg_lut_y[34]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_54, Register_Plns_Mmio_54_Start, 1,
            Hdr_Dg_Luty_35, phdr->dg_lut_y[35]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_55, Register_Plns_Mmio_55_Start, 1,
            Hdr_Dg_Luty_36, phdr->dg_lut_y[36]);

        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_56, Register_Plns_Mmio_56_Start, 1,
            Hdr_Dg_Luts_00, phdr->dg_lut_s[0]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_57, Register_Plns_Mmio_57_Start, 1,
            Hdr_Dg_Luts_01, phdr->dg_lut_s[1]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_58, Register_Plns_Mmio_58_Start, 1,
            Hdr_Dg_Luts_02, phdr->dg_lut_s[2]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_59, Register_Plns_Mmio_59_Start, 1,
            Hdr_Dg_Luts_03, phdr->dg_lut_s[3]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_5a, Register_Plns_Mmio_5a_Start, 1,
            Hdr_Dg_Luts_04, phdr->dg_lut_s[4]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_5b, Register_Plns_Mmio_5b_Start, 1,
            Hdr_Dg_Luts_05, phdr->dg_lut_s[5]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_5c, Register_Plns_Mmio_5c_Start, 1,
            Hdr_Dg_Luts_06, phdr->dg_lut_s[6]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_5d, Register_Plns_Mmio_5d_Start, 1,
            Hdr_Dg_Luts_07, phdr->dg_lut_s[7]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_5e, Register_Plns_Mmio_5e_Start, 1,
            Hdr_Dg_Luts_08, phdr->dg_lut_s[8]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_5f, Register_Plns_Mmio_5f_Start, 1,
            Hdr_Dg_Luts_09, phdr->dg_lut_s[9]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_60, Register_Plns_Mmio_60_Start, 1,
            Hdr_Dg_Luts_10, phdr->dg_lut_s[10]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_61, Register_Plns_Mmio_61_Start, 1,
            Hdr_Dg_Luts_11, phdr->dg_lut_s[11]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_62, Register_Plns_Mmio_62_Start, 1,
            Hdr_Dg_Luts_12, phdr->dg_lut_s[12]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_63, Register_Plns_Mmio_63_Start, 1,
            Hdr_Dg_Luts_13, phdr->dg_lut_s[13]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_64, Register_Plns_Mmio_64_Start, 1,
            Hdr_Dg_Luts_14, phdr->dg_lut_s[14]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_65, Register_Plns_Mmio_65_Start, 1,
            Hdr_Dg_Luts_15, phdr->dg_lut_s[15]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_66, Register_Plns_Mmio_66_Start, 1,
            Hdr_Dg_Luts_16, phdr->dg_lut_s[16]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_67, Register_Plns_Mmio_67_Start, 1,
            Hdr_Dg_Luts_17, phdr->dg_lut_s[17]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_68, Register_Plns_Mmio_68_Start, 1,
            Hdr_Dg_Luts_18, phdr->dg_lut_s[18]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_69, Register_Plns_Mmio_69_Start, 1,
            Hdr_Dg_Luts_19, phdr->dg_lut_s[19]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_6a, Register_Plns_Mmio_6a_Start, 1,
            Hdr_Dg_Luts_20, phdr->dg_lut_s[20]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_6b, Register_Plns_Mmio_6b_Start, 1,
            Hdr_Dg_Luts_21, phdr->dg_lut_s[21]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_6c, Register_Plns_Mmio_6c_Start, 1,
            Hdr_Dg_Luts_22, phdr->dg_lut_s[22]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_6d, Register_Plns_Mmio_6d_Start, 1,
            Hdr_Dg_Luts_23, phdr->dg_lut_s[23]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_6e, Register_Plns_Mmio_6e_Start, 1,
            Hdr_Dg_Luts_24, phdr->dg_lut_s[24]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_6f, Register_Plns_Mmio_6f_Start, 1,
            Hdr_Dg_Luts_25, phdr->dg_lut_s[25]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_70, Register_Plns_Mmio_70_Start, 1,
            Hdr_Dg_Luts_26, phdr->dg_lut_s[26]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_71, Register_Plns_Mmio_71_Start, 1,
            Hdr_Dg_Luts_27, phdr->dg_lut_s[27]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_72, Register_Plns_Mmio_72_Start, 1,
            Hdr_Dg_Luts_28, phdr->dg_lut_s[28]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_73, Register_Plns_Mmio_73_Start, 1,
            Hdr_Dg_Luts_29, phdr->dg_lut_s[29]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_74, Register_Plns_Mmio_74_Start, 1,
            Hdr_Dg_Luts_30, phdr->dg_lut_s[30]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_75, Register_Plns_Mmio_75_Start, 1,
            Hdr_Dg_Luts_31, phdr->dg_lut_s[31]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_76, Register_Plns_Mmio_76_Start, 1,
            Hdr_Dg_Luts_32, phdr->dg_lut_s[32]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_77, Register_Plns_Mmio_77_Start, 1,
            Hdr_Dg_Luts_33, phdr->dg_lut_s[33]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_78, Register_Plns_Mmio_78_Start, 1,
            Hdr_Dg_Luts_34, phdr->dg_lut_s[34]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_79, Register_Plns_Mmio_79_Start, 1,
            Hdr_Dg_Luts_35, phdr->dg_lut_s[35]);

        // hdr sat
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_7a, Register_Plns_Mmio_7a_Start, 3,
            Hdr_Tm_Rgb2y_Kr, phdr->sat_rgb_2_y[0],
            Hdr_Tm_Rgb2y_Kg, phdr->sat_rgb_2_y[1],
            Hdr_Tm_Sat_En, phdr->sat_en);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_7b, Register_Plns_Mmio_7b_Start, 1,
            Hdr_Tm_Rgb2y_Kb, phdr->sat_rgb_2_y[2]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_7c, Register_Plns_Mmio_7c_Start, 3,
            Hdr_Tm_Luty_00, phdr->sat_lut_y[0],
            Hdr_Tm_Luty_01, phdr->sat_lut_y[1],
            Hdr_Tm_Luty_02, phdr->sat_lut_y[2]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_7d, Register_Plns_Mmio_7d_Start, 3,
            Hdr_Tm_Luty_03, phdr->sat_lut_y[3],
            Hdr_Tm_Luty_04, phdr->sat_lut_y[4],
            Hdr_Tm_Luty_05, phdr->sat_lut_y[5]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_7e, Register_Plns_Mmio_7e_Start, 3,
            Hdr_Tm_Luty_06, phdr->sat_lut_y[6],
            Hdr_Tm_Luty_07, phdr->sat_lut_y[7],
            Hdr_Tm_Luty_08, phdr->sat_lut_y[8]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_7f, Register_Plns_Mmio_7f_Start, 3,
            Hdr_Tm_Luty_09, phdr->sat_lut_y[9],
            Hdr_Tm_Luty_10, phdr->sat_lut_y[10],
            Hdr_Tm_Luty_11, phdr->sat_lut_y[11]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_80, Register_Plns_Mmio_80_Start, 1,
            Hdr_Tm_Luty_12, phdr->sat_lut_y[12]);

        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_81, Register_Plns_Mmio_81_Start, 2,
            Hdr_Tm_Luts_00, phdr->sat_lut_s[0],
            Hdr_Tm_Luts_01, phdr->sat_lut_s[1]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_82, Register_Plns_Mmio_82_Start, 2,
            Hdr_Tm_Luts_02, phdr->sat_lut_s[2],
            Hdr_Tm_Luts_03, phdr->sat_lut_s[3]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_83, Register_Plns_Mmio_83_Start, 2,
            Hdr_Tm_Luts_04, phdr->sat_lut_s[4],
            Hdr_Tm_Luts_05, phdr->sat_lut_s[5]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_84, Register_Plns_Mmio_84_Start, 2,
            Hdr_Tm_Luts_06, phdr->sat_lut_s[6],
            Hdr_Tm_Luts_07, phdr->sat_lut_s[7]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_85, Register_Plns_Mmio_85_Start, 2,
            Hdr_Tm_Luts_08, phdr->sat_lut_s[8],
            Hdr_Tm_Luts_09, phdr->sat_lut_s[9]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_86, Register_Plns_Mmio_86_Start, 2,
            Hdr_Tm_Luts_10, phdr->sat_lut_s[10],
            Hdr_Tm_Luts_11, phdr->sat_lut_s[11]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_87, Register_Plns_Mmio_87_Start, 1,
            Hdr_Dg_Stretch_A, phdr->dg_stretch_a);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_88, Register_Plns_Mmio_88_Start, 1,
            Hdr_Dg_Stretch_B, (disp_u32_t)phdr->dg_stretch_b);

        // csc
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_89, Register_Plns_Mmio_89_Start, 2,
            Hdr_Csc_M_00, phdr->rgb_2_rgb_m[0],
            Hdr_Csc_M_01, phdr->rgb_2_rgb_m[1]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_8a, Register_Plns_Mmio_8a_Start, 2,
            Hdr_Csc_M_02, phdr->rgb_2_rgb_m[2],
            Hdr_Csc_M_10, phdr->rgb_2_rgb_m[3]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_8b, Register_Plns_Mmio_8b_Start, 2,
            Hdr_Csc_M_11, phdr->rgb_2_rgb_m[4],
            Hdr_Csc_M_12, phdr->rgb_2_rgb_m[5]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_8c, Register_Plns_Mmio_8c_Start, 2,
            Hdr_Csc_M_20, phdr->rgb_2_rgb_m[6],
            Hdr_Csc_M_21, phdr->rgb_2_rgb_m[7]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_8d, Register_Plns_Mmio_8d_Start, 1,
            Hdr_Csc_M_22, phdr->rgb_2_rgb_m[8]);

        // gamma
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_8e, Register_Plns_Mmio_8e_Start, 2,
            Hdr_G_Luty_00, phdr->g_lut_y[0],
            Hdr_G_Luty_01, phdr->g_lut_y[1]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_8f, Register_Plns_Mmio_8f_Start, 2,
            Hdr_G_Luty_02, phdr->g_lut_y[2],
            Hdr_G_Luty_03, phdr->g_lut_y[3]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_90, Register_Plns_Mmio_90_Start, 2,
            Hdr_G_Luty_04, phdr->g_lut_y[4],
            Hdr_G_Luty_05, phdr->g_lut_y[5]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_91, Register_Plns_Mmio_91_Start, 2,
            Hdr_G_Luty_06, phdr->g_lut_y[6],
            Hdr_G_Luty_07, phdr->g_lut_y[7]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_92, Register_Plns_Mmio_92_Start, 2,
            Hdr_G_Luty_08, phdr->g_lut_y[8],
            Hdr_G_Luty_09, phdr->g_lut_y[9]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_93, Register_Plns_Mmio_93_Start, 2,
            Hdr_G_Luty_10, phdr->g_lut_y[10],
            Hdr_G_Luty_11, phdr->g_lut_y[11]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_94, Register_Plns_Mmio_94_Start, 2,
            Hdr_G_Luty_12, phdr->g_lut_y[12],
            Hdr_G_Luty_13, phdr->g_lut_y[13]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_95, Register_Plns_Mmio_95_Start, 2,
            Hdr_G_Luty_14, phdr->g_lut_y[14],
            Hdr_G_Luty_15, phdr->g_lut_y[15]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_96, Register_Plns_Mmio_96_Start, 2,
            Hdr_G_Luty_16, phdr->g_lut_y[16],
            Hdr_G_Luty_17, phdr->g_lut_y[17]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_97, Register_Plns_Mmio_97_Start, 2,
            Hdr_G_Luty_18, phdr->g_lut_y[18],
            Hdr_G_Luty_19, phdr->g_lut_y[19]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_98, Register_Plns_Mmio_98_Start, 2,
            Hdr_G_Luty_20, phdr->g_lut_y[20],
            Hdr_G_Luty_21, phdr->g_lut_y[21]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_99, Register_Plns_Mmio_99_Start, 2,
            Hdr_G_Luty_22, phdr->g_lut_y[22],
            Hdr_G_Luty_23, phdr->g_lut_y[23]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_9a, Register_Plns_Mmio_9a_Start, 2,
            Hdr_G_Luty_24, phdr->g_lut_y[24],
            Hdr_G_Luty_25, phdr->g_lut_y[25]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_9b, Register_Plns_Mmio_9b_Start, 2,
            Hdr_G_Luty_26, phdr->g_lut_y[26],
            Hdr_G_Luty_27, phdr->g_lut_y[27]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_9c, Register_Plns_Mmio_9c_Start, 2,
            Hdr_G_Luty_28, phdr->g_lut_y[28],
            Hdr_G_Luty_29, phdr->g_lut_y[29]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_9d, Register_Plns_Mmio_9d_Start, 2,
            Hdr_G_Luty_30, phdr->g_lut_y[30],
            Hdr_G_Luty_31, phdr->g_lut_y[31]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_9e, Register_Plns_Mmio_9e_Start, 1,
            Hdr_G_Luty_32, phdr->g_lut_y[32]);

        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_9f, Register_Plns_Mmio_9f_Start, 1,
            Hdr_G_Luts_00, phdr->g_lut_s[0]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_A0, Register_Plns_Mmio_A0_Start, 1,
            Hdr_G_Luts_01, phdr->g_lut_s[1]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_A1, Register_Plns_Mmio_A1_Start, 1,
            Hdr_G_Luts_02, phdr->g_lut_s[2]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_A2, Register_Plns_Mmio_A2_Start, 1,
            Hdr_G_Luts_03, phdr->g_lut_s[3]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_A3, Register_Plns_Mmio_A3_Start, 1,
            Hdr_G_Luts_04, phdr->g_lut_s[4]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_A4, Register_Plns_Mmio_A4_Start, 1,
            Hdr_G_Luts_05, phdr->g_lut_s[5]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_A5, Register_Plns_Mmio_A5_Start, 1,
            Hdr_G_Luts_06, phdr->g_lut_s[6]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_A6, Register_Plns_Mmio_A6_Start, 1,
            Hdr_G_Luts_07, phdr->g_lut_s[7]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_A7, Register_Plns_Mmio_A7_Start, 1,
            Hdr_G_Luts_08, phdr->g_lut_s[8]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_A8, Register_Plns_Mmio_A8_Start, 1,
            Hdr_G_Luts_09, phdr->g_lut_s[9]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_A9, Register_Plns_Mmio_A9_Start, 1,
            Hdr_G_Luts_10, phdr->g_lut_s[10]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_Aa, Register_Plns_Mmio_Aa_Start, 1,
            Hdr_G_Luts_11, phdr->g_lut_s[11]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_Ab, Register_Plns_Mmio_Ab_Start, 1,
            Hdr_G_Luts_12, phdr->g_lut_s[12]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_Ac, Register_Plns_Mmio_Ac_Start, 1,
            Hdr_G_Luts_13, phdr->g_lut_s[13]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_Ad, Register_Plns_Mmio_Ad_Start, 1,
            Hdr_G_Luts_14, phdr->g_lut_s[14]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_Ae, Register_Plns_Mmio_Ae_Start, 1,
            Hdr_G_Luts_15, phdr->g_lut_s[15]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_Af, Register_Plns_Mmio_Af_Start, 1,
            Hdr_G_Luts_16, phdr->g_lut_s[16]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_B0, Register_Plns_Mmio_B0_Start, 1,
            Hdr_G_Luts_17, phdr->g_lut_s[17]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_B1, Register_Plns_Mmio_B1_Start, 1,
            Hdr_G_Luts_18, phdr->g_lut_s[18]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_B2, Register_Plns_Mmio_B2_Start, 1,
            Hdr_G_Luts_19, phdr->g_lut_s[19]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_B3, Register_Plns_Mmio_B3_Start, 1,
            Hdr_G_Luts_20, phdr->g_lut_s[20]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_B4, Register_Plns_Mmio_B4_Start, 1,
            Hdr_G_Luts_21, phdr->g_lut_s[21]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_B5, Register_Plns_Mmio_B5_Start, 1,
            Hdr_G_Luts_22, phdr->g_lut_s[22]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_B6, Register_Plns_Mmio_B6_Start, 1,
            Hdr_G_Luts_23, phdr->g_lut_s[23]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_B7, Register_Plns_Mmio_B7_Start, 1,
            Hdr_G_Luts_24, phdr->g_lut_s[24]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_B8, Register_Plns_Mmio_B8_Start, 1,
            Hdr_G_Luts_25, phdr->g_lut_s[25]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_B9, Register_Plns_Mmio_B9_Start, 1,
            Hdr_G_Luts_26, phdr->g_lut_s[26]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_Ba, Register_Plns_Mmio_Ba_Start, 1,
            Hdr_G_Luts_27, phdr->g_lut_s[27]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_Bb, Register_Plns_Mmio_Bb_Start, 1,
            Hdr_G_Luts_28, phdr->g_lut_s[28]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_Bc, Register_Plns_Mmio_Bc_Start, 1,
            Hdr_G_Luts_29, phdr->g_lut_s[29]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_Bd, Register_Plns_Mmio_Bd_Start, 1,
            Hdr_G_Luts_30, phdr->g_lut_s[30]);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_Be, Register_Plns_Mmio_Be_Start, 1,
            Hdr_G_Luts_31, phdr->g_lut_s[31]);

        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_Bf, Register_Plns_Mmio_Bf_Start, 1,
            Hdr_G_Stretch_A, phdr->g_stretch_a);
        plane_hdr_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_C0, Register_Plns_Mmio_C0_Start, 1,
            Hdr_G_Stretch_B, (disp_u32_t)phdr->g_stretch_b);
    }

    return DISP_OK;
}

disp_void_t disp_hdr_module_dump(pdisp_hdr_module_t phdr_info)
{
    disp_s32_t max_x_count = 8, level = DDM_TRACE, idx = 0;

    disp_print(level, "hdr module:\n");

    disp_print_pure(level, "\treg: hdr_en[%#x], sat_en[%#x], dg_stretch[%#x, %#x], g_stretch[%#x, %#x], rgb2Y[%#x, %#x, %#x]\n",
        phdr_info->hdr_en, phdr_info->sat_en, phdr_info->dg_stretch_a, phdr_info->dg_stretch_b, phdr_info->g_stretch_a,
        phdr_info->g_stretch_b, phdr_info->sat_rgb_2_y[0], phdr_info->sat_rgb_2_y[1], phdr_info->sat_rgb_2_y[2]);

    disp_print_pure(level, "\treg dg_lut_y:\n");
    for(idx = 0; idx < DISP_HDR_REG_DG_LUT_LEN; idx++)
    {
        disp_print_pure(level, "%u ", phdr_info->dg_lut_y[idx]);
        if (max_x_count - 1 == idx % max_x_count) disp_print_pure(level, "\n");
    }
    if (max_x_count - 1 != idx % max_x_count) disp_print_pure(level, "\n");

    disp_print_pure(level, "\treg dg_lut_s:\n");
    for(idx = 0; idx < DISP_HDR_REG_DG_LUT_LEN - 1; idx++)
    {
        disp_print_pure(level, "%u ", phdr_info->dg_lut_s[idx]);
        if (max_x_count - 1 == idx % max_x_count)
            disp_print_pure(level, "\n");
    }
    if (max_x_count - 1 != idx % max_x_count) disp_print_pure(level, "\n");

    disp_print_pure(level, "\tsat_lut_y:\n");
    for(idx = 0; idx < DISP_HDR_REG_SAT_LUT_LEN; idx++)
    {
        disp_print_pure(level, "%u ", phdr_info->sat_lut_y[idx]);
        if (max_x_count - 1 == idx % max_x_count) disp_print_pure(level, "\n");
    }
    if (max_x_count - 1 != idx % max_x_count) disp_print_pure(level, "\n");

    disp_print_pure(level, "\tsat_lut_s:\n");
    for(idx = 0; idx < DISP_HDR_REG_SAT_LUT_LEN - 1; idx++)
    {
        disp_print_pure(level, "%u ", phdr_info->sat_lut_s[idx]);
        if (max_x_count - 1 == idx % max_x_count) disp_print_pure(level, "\n");
    }
    if (max_x_count - 1 != idx % max_x_count) disp_print_pure(level, "\n");

    disp_print_pure(level, "\trgb_2_rgb_m:");
    for(idx = 0; idx < DISP_HDR_REG_RGB2RGB_LEN; idx++)
        disp_print_pure(level, "%u ", phdr_info->rgb_2_rgb_m[idx]);
    disp_print_pure(level, "\n");

    disp_print_pure(level, "\tg_lut_y:\n");
    for(idx = 0; idx < DISP_HDR_REG_G_LUT_LEN; idx++)
    {
        disp_print_pure(level, "%u ", phdr_info->g_lut_y[idx]);
        if (max_x_count - 1 == idx % max_x_count) disp_print_pure(level, "\n");
    }
    if (max_x_count - 1 != idx % max_x_count) disp_print_pure(level, "\n");

    disp_print_pure(level, "\tg_lut_y:\n");
    for(idx = 0; idx < DISP_HDR_REG_G_LUT_LEN - 1; idx++)
    {
        disp_print_pure(level, "%u ", phdr_info->g_lut_s[idx]);
        if (max_x_count - 1 == idx % max_x_count) disp_print_pure(level, "\n");
    }
    if (max_x_count - 1 != idx % max_x_count) disp_print_pure(level, "\n");
}

#else

disp_u8_t disp_hdr_check(pdisp_void_t pplane, pdisp_hdr_module_t pnew, disp_u8_t hdr_module_valid)
{
    return DISP_FALSE;
}
DISP_STATUS disp_hdr_flush(pdisp_void_t pplane)
{
    return DISP_OK;
}
disp_void_t disp_hdr_module_dump(pdisp_hdr_module_t phdr_info)
{
}

#endif
