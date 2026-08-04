/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#include "disp_dhi.h"
#include "disp_device.h"
#include "disp_callback.h"
#include "disp_reg.h"

#include "DHI_register.h"
#include "CR_SHARE_register.h"
#include "SR_SHARE_register.h"
#include "PLNS_register.h"
#include "SR_register.h"

#define dhi_write_field(reg_name, reg_start, num, ...) \
    reg32_write_field(reg_name, (reg_start * 4 + pdd->reg_off.dhi), num, __VA_ARGS__)

#define dhi_read_field(reg_name, reg_start, num, ...) \
    reg32_read_field(reg_name, (reg_start * 4 + pdd->reg_off.dhi), num, __VA_ARGS__)

#define disp_print(level, log, ...) disp_print_ex(DISP_PRINT_MASK_DHI, level, log, ##__VA_ARGS__)
#define disp_print_pure(level, log, ...) disp_print_pure_ex(DISP_PRINT_MASK_DHI, level, log, ##__VA_ARGS__)

disp_void_t disp_register_unlock(pdisp_void_t pdisp_device, disp_u8_t unlock)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_device;

    sr_share_write_field(Reg_Sr_08, Register_Sr_08_Start, 1,
        Sr_Pr, (unlock ? 0x96 : 0)); //unlock sr

    cr_share_write_field(Reg_Cr_25, Register_Cr_25_Start, 1,
        Cr_Pr, (unlock ? 0x53 : 0)); //unlock cr

    cr_share_write_field(Reg_Cr_1e, Register_Cr_1e_Start, 8,
        Ns_Htg_Pr, !unlock,
        Ns_Hde_Pr, !unlock,
        Ns_Vtg_Pr, !unlock,
        Ns_Vde_Pr, !unlock,
        Ns_Gr05_Sr_Pr, !unlock,
        Ns_Sr0103_Pr, !unlock,
        Ns_Sr01_89dc_Pr, !unlock,
        Ns_Vdeh_Pr, !unlock);

    cr_share_write_field(Reg_Cr_1f, Register_Cr_1f_Start, 3,
        Lut_Pr, !unlock,
        Ns_Misc_Cs_Pr, !unlock,
        Ns_Ar_Pal_Pr, !unlock);
}

disp_void_t disp_dhi_hw_init(pdisp_void_t pdisp_device)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_device;
    disp_print(DDM_TRACE, "tofill %#x\n", pdd);

    //reg32_write_field(Reg_Dhi_Mmio_0, Register_Dhi_Mmio_0_Start, 1,
    //    Epl_Contreq, DHI_MMIO_0_EPL_CONTREQ_ENABLE);
}

disp_void_t disp_dhi_hw_deinit(pdisp_void_t pdisp_device)
{
    disp_print(DDM_TRACE, "tofill %#x\n", pdisp_device);
}

static disp_void_t disp_check_underflow_epl(pdisp_device_t pdd, disp_s32_t index, disp_u8_t is_overlay)
{
    disp_u8_t vsync_flip[2];
    Reg_Plns_Mmio_4 plns_4;
    Reg_Plns_Mmio_5 plns_5;
    Reg_Plns_Mmio_6 plns_6;
    Reg_Plns_Mmio_7 plns_7;
    Reg_Plns_Mmio_10 plns_10;
    Reg_Plns_Mmio_11 plns_11;
    Reg_Plns_Mmio_12 plns_12;
    Reg_Plns_Mmio_13 plns_13;
    Reg_Plns_Mmio_14 plns_14;
    Reg_Plns_Mmio_15 plns_15;
    //Reg_Plns_Mmio_16 plns_16;
    Reg_Plns_Mmio_17 plns_17;

    sr_read_field(index, Reg_Sr_2f, Register_Sr_2f_Start, 2,
        Epl0_Vsync_On_Flip, &vsync_flip[0],
        Epl1_Vsync_On_Flip, &vsync_flip[1]);

    plns_4.uint = disp_read_u32(pdd, pdd->reg_off.plns[index] + Register_Plns_Mmio_4_Start * 4);
    plns_5.uint = disp_read_u32(pdd, pdd->reg_off.plns[index] + Register_Plns_Mmio_5_Start * 4);
    plns_6.uint = disp_read_u32(pdd, pdd->reg_off.plns[index] + Register_Plns_Mmio_6_Start * 4);
    plns_7.uint = disp_read_u32(pdd, pdd->reg_off.plns[index] + Register_Plns_Mmio_7_Start * 4);
    plns_10.uint = disp_read_u32(pdd, pdd->reg_off.plns[index] + pdd->reg_off.layer_plns[is_overlay] + Register_Plns_Mmio_10_Start * 4);
    plns_11.uint = disp_read_u32(pdd, pdd->reg_off.plns[index] + pdd->reg_off.layer_plns[is_overlay] + Register_Plns_Mmio_11_Start * 4);
    plns_12.uint = disp_read_u32(pdd, pdd->reg_off.plns[index] + pdd->reg_off.layer_plns[is_overlay] + Register_Plns_Mmio_12_Start * 4);
    plns_13.uint = disp_read_u32(pdd, pdd->reg_off.plns[index] + pdd->reg_off.layer_plns[is_overlay] + Register_Plns_Mmio_13_Start * 4);
    plns_14.uint = disp_read_u32(pdd, pdd->reg_off.plns[index] + pdd->reg_off.layer_plns[is_overlay] + Register_Plns_Mmio_14_Start * 4);
    plns_15.uint = disp_read_u32(pdd, pdd->reg_off.plns[index] + pdd->reg_off.layer_plns[is_overlay] + Register_Plns_Mmio_15_Start * 4);
    //plns_16.uint = disp_read_u32(pdd, pdd->reg_off.plns[index] + pdd->reg_off.layer_plns[is_overlay] + Register_Plns_Mmio_16_Start * 4);
    plns_17.uint = disp_read_u32(pdd, pdd->reg_off.plns[index] + pdd->reg_off.layer_plns[is_overlay] + Register_Plns_Mmio_17_Start * 4);

    disp_print(DDM_ERROR, "Epl_Underflow[%u], scaler: sel[%u] hvEn[%u %u] hvRatio[%x %x] hvCostap[%u %u]vDup[%u]hwRatio[%u]ratioPlus[%u]\n",
        index, plns_4.reg.Scl_Src_Sel, plns_4.reg.H_En, plns_4.reg.V_En, plns_6.reg.H_Ratio, plns_5.reg.V_Ratio,
        plns_4.reg.Hcostap, plns_4.reg.Vcostap, plns_4.reg.Vdup, plns_4.reg.Hw_Ratio, plns_4.reg.Ratio_Plus);
    disp_print_pure(DDM_ERROR, "\telp[%u]: vsync_flip[%u] en[%u] WH[%u %u] XY[%u %u] fmt[%u]order[%u] "
        "pxlOff[%u] stride[%u] pxlAddr[%x]baseOff[%x] compress[%u]type[%u]index[%x] cscBypass[%u]fmt[%u]\n",
        is_overlay, vsync_flip[is_overlay], plns_10.reg.Epl_En, plns_14.reg.Epl_Width, plns_14.reg.Epl_Height,
        plns_15.reg.Epl_Xstart, plns_15.reg.Epl_Ystart, plns_10.reg.Epl_Fmt, plns_10.reg.Epl_Dat_Order,
        plns_10.reg.Epl_Pxl_Off, plns_10.reg.Epl_Stride, plns_11.reg.Epl_Base_Adr, plns_12.reg.Epl_Baseoff,
        plns_10.reg.Epl_Compress_En, plns_13.reg.Epl_Cl_Idx, plns_13.reg.Epl_Cl_Type,
        plns_17.reg.Epl_Csc_Bypass, plns_17.reg.Epl_Csc_In_Fmt);
}

static disp_void_t disp_check_underflow_cs(pdisp_device_t pdd, disp_s32_t index)
{
    disp_u8_t vsync_flip;
    Reg_Plns_Mmio_9 plns_9;
    Reg_Plns_Mmio_A plns_a;
    //Reg_Plns_Mmio_B plns_b;
    Reg_Plns_Mmio_C plns_c;
    Reg_Plns_Mmio_D plns_d;
    Reg_Plns_Mmio_E plns_e;
    //Reg_Plns_Mmio_F plns_f;

    sr_read_field(index, Reg_Sr_2f, Register_Sr_2f_Start, 1, Cs_Vsync_On_Flip, &vsync_flip);
    plns_9.uint = disp_read_u32(pdd, pdd->reg_off.plns[index] + Register_Plns_Mmio_9_Start * 4);
    plns_a.uint = disp_read_u32(pdd, pdd->reg_off.plns[index] + Register_Plns_Mmio_A_Start * 4);
    //plns_b.uint = disp_read_u32(pdd, pdd->reg_off.plns[index] + Register_Plns_Mmio_B_Start * 4);
    plns_c.uint = disp_read_u32(pdd, pdd->reg_off.plns[index] + Register_Plns_Mmio_C_Start * 4);
    plns_d.uint = disp_read_u32(pdd, pdd->reg_off.plns[index] + Register_Plns_Mmio_D_Start * 4);
    plns_e.uint = disp_read_u32(pdd, pdd->reg_off.plns[index] + Register_Plns_Mmio_E_Start * 4);
    //plns_f.uint = disp_read_u32(pdd, pdd->reg_off.plns[index] + Register_Plns_Mmio_F_Start * 4);

    disp_print(DDM_ERROR, "Cs_Underflow[%u]: vsync_flip[%u] en[%u] type[%u] WH[%u %u] XY[%u %u] bg[%x]fg[%x] addr[%x]\n",
        index, vsync_flip, plns_9.reg.Cs_En, plns_9.reg.Cs_Type, plns_9.reg.Cs_Width, plns_9.reg.Cs_Height,
        plns_a.reg.Cs_Scrn_Xpos, plns_a.reg.Cs_Scrn_Ypos, plns_d.reg.Cs_Bg_Color, plns_e.reg.Cs_Fg_Color,
        plns_c.reg.Cs_Base_Adr);
}

disp_void_t disp_check_underflow(pdisp_void_t pdisp_device)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_device;
    Reg_Dhi_Mmio_12 dhi_mmio_12 = {0};

    dhi_mmio_12.uint = disp_read_u32(pdd, pdd->reg_off.dhi + Register_Dhi_Mmio_12_Start * 4);
    // write 1 to clear
    disp_write_u32(pdd, pdd->reg_off.dhi + Register_Dhi_Mmio_12_Start * 4, dhi_mmio_12.uint);

    if (dhi_mmio_12.reg.A_Epl0_Underflow)
        disp_check_underflow_epl(pdd, 0, DISP_FALSE);
    if (dhi_mmio_12.reg.A_Epl1_Underflow)
        disp_check_underflow_epl(pdd, 0, DISP_TRUE);
    if (dhi_mmio_12.reg.B_Epl0_Underflow)
        disp_check_underflow_epl(pdd, 1, DISP_FALSE);
    if (dhi_mmio_12.reg.B_Epl1_Underflow)
        disp_check_underflow_epl(pdd, 1, DISP_TRUE);
    if (dhi_mmio_12.reg.C_Epl0_Underflow)
        disp_check_underflow_epl(pdd, 2, DISP_FALSE);
    if (dhi_mmio_12.reg.C_Epl1_Underflow)
        disp_check_underflow_epl(pdd, 2, DISP_TRUE);
    if (dhi_mmio_12.reg.D_Epl0_Underflow)
        disp_check_underflow_epl(pdd, 3, DISP_FALSE);
    if (dhi_mmio_12.reg.D_Epl1_Underflow)
        disp_check_underflow_epl(pdd, 3, DISP_TRUE);

    if (dhi_mmio_12.reg.A_Cs_Underflow)
        disp_check_underflow_cs(pdd, 0);
    if (dhi_mmio_12.reg.B_Cs_Underflow)
        disp_check_underflow_cs(pdd, 1);
    if (dhi_mmio_12.reg.C_Cs_Underflow)
        disp_check_underflow_cs(pdd, 2);
    if (dhi_mmio_12.reg.D_Cs_Underflow)
        disp_check_underflow_cs(pdd, 3);

    if(dhi_mmio_12.reg.D_Vga_Underflow)
        disp_print(DDM_ERROR, "D_Vga_Underflow\n");
    if(dhi_mmio_12.reg.C_Vga_Underflow)
        disp_print(DDM_ERROR, "C_Vga_Underflow\n");
    if(dhi_mmio_12.reg.B_Vga_Underflow)
        disp_print(DDM_ERROR, "B_Vga_Underflow\n");
    if(dhi_mmio_12.reg.A_Vga_Underflow)
        disp_print(DDM_ERROR, "A_Vga_Underflow\n");
}
