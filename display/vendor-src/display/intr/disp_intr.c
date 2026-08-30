/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#include "disp_intr.h"
#include "INTR_register.h"
#include "disp_reg.h"
#include "disp_callback.h"
#include "disp_device.h"

#define intr_write(reg_start, value) \
    disp_write_u32(pdd, reg_start * 4 + pdd->reg_off.intr, value)

#define intr_read(reg_start) \
    disp_read_u32(pdd, reg_start * 4 + pdd->reg_off.intr)

#define intr_write_field(reg_name, reg_start, num, ...) \
{ \
    reg32_write_field(reg_name, (reg_start * 4 + pdd->reg_off.intr), num, __VA_ARGS__) \
}

#define intr_read_field(reg_name, reg_start, num, ...) \
{ \
    reg32_read_field(reg_name, (reg_start * 4 + pdd->reg_off.intr), num, __VA_ARGS__) \
}

#define disp_print(level, log, ...) disp_print_ex(DISP_PRINT_MASK_INTR, level, log, ##__VA_ARGS__)

static disp_u32_t disp_uint_to_reg_uint(disp_intr_t disp_intr)
{
    Reg_Intr_Mmio_0 reg_intr;
    reg_intr.uint = 0;

    reg_intr.reg.Int_Vgairq_En = disp_intr.info.Vgairq;
    reg_intr.reg.Int_Vsynca_En = disp_intr.info.Vsynca;
    reg_intr.reg.Int_Vsyncb_En = disp_intr.info.Vsyncb;
    reg_intr.reg.Int_Vsyncc_En = disp_intr.info.Vsyncc;
    reg_intr.reg.Int_Vsyncd_En = disp_intr.info.Vsyncd;
    reg_intr.reg.Int_Codec1_En = disp_intr.info.Codec1;
    reg_intr.reg.Int_Dpa_Hpd_En = disp_intr.info.Dpa_Hpd;
    reg_intr.reg.Int_Dpa_Ephy_En = disp_intr.info.Dpa_Ephy;
    reg_intr.reg.Int_Dpa_Linktraining_En = disp_intr.info.Dpa_Linktraining;
    reg_intr.reg.Int_Dpb_Hpd_En = disp_intr.info.Dpb_Hpd;
    reg_intr.reg.Int_Dpb_Ephy_En = disp_intr.info.Dpb_Ephy;
    reg_intr.reg.Int_Dpb_Linktraining_En = disp_intr.info.Dpb_Linktraining;
    reg_intr.reg.Int_Dpc_Hpd_En = disp_intr.info.Dpc_Hpd;
    reg_intr.reg.Int_Dpc_Ephy_En = disp_intr.info.Dpc_Ephy;
    reg_intr.reg.Int_Dpc_Linktraining_En = disp_intr.info.Dpc_Linktraining;
    reg_intr.reg.Int_Dpd_Hpd_En = disp_intr.info.Dpd_Hpd;
    reg_intr.reg.Int_Dpd_Ephy_En = disp_intr.info.Dpd_Ephy;
    reg_intr.reg.Int_Dpd_Linktraining_En = disp_intr.info.Dpd_Linktraining;
    reg_intr.reg.Int_Pipea_Sgnt_En = disp_intr.info.Pipea_Sgnt;
    reg_intr.reg.Int_Pipeb_Sgnt_En = disp_intr.info.Pipeb_Sgnt;
    reg_intr.reg.Int_Pipec_Sgnt_En = disp_intr.info.Pipec_Sgnt;
    reg_intr.reg.Int_Piped_Sgnt_En = disp_intr.info.Piped_Sgnt;

    return reg_intr.uint;
}

static disp_u32_t reg_uint_to_disp_uint(disp_u32_t reg_uint)
{
    Reg_Intr_Mmio_0 reg_intr;
    disp_intr_t disp_intr;

    disp_intr.intr = 0;
    reg_intr.uint = reg_uint;

    disp_intr.info.Vgairq = reg_intr.reg.Int_Vgairq_En;
    disp_intr.info.Vsynca = reg_intr.reg.Int_Vsynca_En;
    disp_intr.info.Vsyncb = reg_intr.reg.Int_Vsyncb_En;
    disp_intr.info.Vsyncc = reg_intr.reg.Int_Vsyncc_En;
    disp_intr.info.Vsyncd = reg_intr.reg.Int_Vsyncd_En;
    disp_intr.info.Codec1 = reg_intr.reg.Int_Codec1_En;
    disp_intr.info.Dpa_Hpd = reg_intr.reg.Int_Dpa_Hpd_En;
    disp_intr.info.Dpa_Ephy = reg_intr.reg.Int_Dpa_Ephy_En;
    disp_intr.info.Dpa_Linktraining = reg_intr.reg.Int_Dpa_Linktraining_En;
    disp_intr.info.Dpb_Hpd = reg_intr.reg.Int_Dpb_Hpd_En;
    disp_intr.info.Dpb_Ephy = reg_intr.reg.Int_Dpb_Ephy_En;
    disp_intr.info.Dpb_Linktraining = reg_intr.reg.Int_Dpb_Linktraining_En;
    disp_intr.info.Dpc_Hpd = reg_intr.reg.Int_Dpc_Hpd_En;
    disp_intr.info.Dpc_Ephy = reg_intr.reg.Int_Dpc_Ephy_En;
    disp_intr.info.Dpc_Linktraining = reg_intr.reg.Int_Dpc_Linktraining_En;
    disp_intr.info.Dpd_Hpd = reg_intr.reg.Int_Dpd_Hpd_En;
    disp_intr.info.Dpd_Ephy = reg_intr.reg.Int_Dpd_Ephy_En;
    disp_intr.info.Dpd_Linktraining = reg_intr.reg.Int_Dpd_Linktraining_En;
    disp_intr.info.Pipea_Sgnt = reg_intr.reg.Int_Pipea_Sgnt_En;
    disp_intr.info.Pipeb_Sgnt = reg_intr.reg.Int_Pipeb_Sgnt_En;
    disp_intr.info.Pipec_Sgnt = reg_intr.reg.Int_Pipec_Sgnt_En;
    disp_intr.info.Piped_Sgnt = reg_intr.reg.Int_Piped_Sgnt_En;

    return disp_intr.intr;
}

disp_void_t disp_intr_enable(pdisp_void_t pdevice, disp_u8_t enable, disp_intr_t disp_intr)
{
    disp_u32_t intr = 0, old_status = 0;
    pdisp_device_t pdd = (pdisp_device_t)pdevice;

    intr = disp_uint_to_reg_uint(disp_intr);

//there is 2 dpe interrupt:
//one go to mcu(intr 2), it is enabled by Register_Intr_Mmio_2_Start
//the other go to pcie(intr 1), it is enabled by register in PBU
    if(enable)
    {
        old_status = intr_read(Register_Intr_Mmio_0_Start);
        intr_write(Register_Intr_Mmio_0_Start, old_status | intr);

        old_status = intr_read(Register_Intr_Mmio_1_Start);
        intr_write(Register_Intr_Mmio_1_Start, old_status | intr);
    }
    else
    {
        old_status = intr_read(Register_Intr_Mmio_0_Start);
        intr_write(Register_Intr_Mmio_0_Start, old_status & (~intr));

        old_status = intr_read(Register_Intr_Mmio_1_Start);
        intr_write(Register_Intr_Mmio_1_Start, old_status & (~intr));
    }
}

disp_u32_t disp_intr_get_status(pdisp_void_t pdevice)
{
    pdisp_device_t pdd = pdevice;
    disp_u32_t status = 0;
    Reg_Intr_Mmio_3 reg_intr = {0};

//write 1 to clear interrupt status
    status = intr_read(Register_Intr_Mmio_3_Start);

    if(pdd->flag.info.multi_vsync_enable)
    {
        reg_intr.reg.Int_Vsynca = 1;
        reg_intr.reg.Int_Vsyncb = 1;
        reg_intr.reg.Int_Vsyncc = 1;
        reg_intr.reg.Int_Vsyncd = 1;

        status = status & (~reg_intr.uint);
    }

    intr_write(Register_Intr_Mmio_3_Start, status);

    return reg_uint_to_disp_uint(status);
}

disp_void_t disp_intr_clear_vsync_status(pdisp_void_t pdevice, disp_s32_t crtc_index)
{
    pdisp_device_t pdd = pdevice;

    if(pdd->flag.info.multi_vsync_enable)
    {
        if (crtc_index == 0)
        {
            intr_write_field(Reg_Intr_Mmio_3, Register_Intr_Mmio_3_Start, 1,
                Int_Vsynca, 1);
        }
        else if(crtc_index == 1)
        {
            intr_write_field(Reg_Intr_Mmio_3, Register_Intr_Mmio_3_Start, 1,
                Int_Vsyncb, 1);
        }
        else if(crtc_index == 2)
        {
            intr_write_field(Reg_Intr_Mmio_3, Register_Intr_Mmio_3_Start, 1,
                Int_Vsyncc, 1);
        }
        else if(crtc_index == 3)
        {
            intr_write_field(Reg_Intr_Mmio_3, Register_Intr_Mmio_3_Start, 1,
                Int_Vsyncd, 1);
        }
        else
        {
            disp_print(DDM_WARNING, "wrong crtc index %d", crtc_index);
        }
    }
    else
    {
        disp_print(DDM_WARNING, "why come here when disable multi sync\n");
    }
}

disp_u32_t disp_intr_multi_sync_enable(pdisp_void_t pdevice, disp_u8_t enable)
{
    pdisp_device_t pdd = pdevice;

    intr_write_field(Reg_Intr_Mmio_5, Register_Intr_Mmio_5_Start, 1,
        En_Multi_Msi, enable);

    pdd->flag.info.multi_vsync_enable = enable;

    return 0;
}
