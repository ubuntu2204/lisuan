/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#include "disp_pll.h"
#include "disp_device.h"
#include "disp_reg.h"
#include "PLLCTL_register.h"
#include "CR_register.h"

/* Wait time: 500 / 27MHz -> 1/54 ms, 50us is double enough */
#define PLL_WAIT_TIMEOUT_TEN_MICROSECOND 5

/* VCO frequency min 4000MHz max 8000MHz, Pre-divide by 4, get min 1000 max 2000 */
#define PLL_VCO_MIN 1000
#define PLL_VCO_MAX 2000

/* 2^24 precision for pll divf fraction */
#define PLL_DIVFF_PRECISION 0x1000000

#define PLL_REF_CLK_MHZ 27
#define PLLOUT_MIN_KHZ 7500
#define PLLOUT_MIN_KHZ_1T4P 30000

#define PLL_MHZ_TO_HZ_MULTI_BASE 1000000

#define pll_write_field(crtc_index, reg_name, reg_start, num, ...) \
{ \
    reg32_write_field(reg_name, (reg_start * 4 + pdd->reg_off.pll[crtc_index]), num, __VA_ARGS__) \
}

#define pll_read_field(crtc_index, reg_name, reg_start, num, ...) \
{ \
    reg32_read_field(reg_name, (reg_start * 4 + pdd->reg_off.pll[crtc_index]), num, __VA_ARGS__) \
}

#define disp_print(level, log, ...) disp_print_ex(DISP_PRINT_MASK_PLL, level, log, ##__VA_ARGS__)

/* pllout table policy:
 * divf_int prefer use min match divq.
 * divf_fra prefer use middle pllout*divq value.
 *
 * pllout & divq need to follow:
 *  a. 1000 < reg_divq * pllout < 2000. (4000 / pllout < reg_divq * 4 < 8000 / pllout)
 *  b. reg_divq * 4 need be unsigned int. (DIVQ = reg_divq / 4)
 *  c. Dpe pll reg_divq * 4 further limit to Multiples of 4 (or 2 only when reg_divq=0),
 *      That means reg_divq must be unsigned int or 0.5(only when reg_divq=0).
 *  d. pllout * reg_divq / refclk(27), This value divisible or not -> divf use int or fraction
 * Final divf reg value: divfi + divff/0x1000000 = pllout * divq * (divr+1) / 27 - 1
 */
static disp_pll_table_t g_pllout_table[] =
{
    // divf_int match: 1188000000(594000000 * 2) 1782000000(594000000 * 3)
    // sel_div[0] divq[2] divr[0] range[3] divf[43.0]
    {594000000, 0, 2, 0, 3, 43, 0},

    // divf_int match: 1485000000(371250000 * 4)
    // sel_div[0] divq[4] divr[0] range[3] divf[54.0]
    {371250000, 0, 4, 0, 3, 54, 0},

    // divf_int match: 1188000000(297000000 * 4) 1485000000(297000000 * 5) 1782000000(297000000 * 6)
    // sel_div[0] divq[4] divr[0] range[3] divf[43.0]
    {297000000, 0, 4, 0, 3, 43, 0},

    // divf_int match: 1485000000(185625000 * 8)
    // sel_div[0] divq[8] divr[0] range[3] divf[54.0]
    {185625000, 0, 8, 0, 3, 54, 0},

    // divf_int match: 1188000000(148500000 * 8) 1485000000(148500000 * 10) 1782000000(148500000 * 12)
    // sel_div[0] divq[8] divr[0] range[3] divf[43.0]
    {148500000, 0, 8, 0, 3, 43, 0},

    // divf_int match: 1188000000(99000000 * 12) 1485000000(99000000 * 15) 1782000000(99000000 * 18)
    // sel_div[0] divq[12] divr[0] range[3] divf[43.0]
    {99000000, 0, 12, 0, 3, 43, 0},

    // divf_int match: 1188000000(74250000 * 16) 1485000000(74250000 * 20) 1782000000(74250000 * 24)
    // sel_div[0] divq[16] divr[0] range[3] divf[43.0]
    {74250000, 0, 16, 0, 3, 43, 0},

    // divf_int match: 1188000000(37125000 * 32) 1485000000(37125000 * 40) 1782000000(37125000 * 48)
    // sel_div[0] divq[32] divr[0] range[3] divf[43.0]
    {37125000, 0, 32, 0, 3, 43, 0},

    // divf_int match: 1080000000(33750000 * 32) 1215000000(33750000 * 36) 1350000000(33750000 * 40) ... 1890000000(33750000 * 56)
    // sel_div[0] divq[32] divr[0] range[3] divf[39.0]
    {33750000, 0, 32, 0, 3, 39, 0},

    // divf_int match: 1026000000(27000000 * 38) 1053000000(27000000 * 39) 1080000000(27000000 * 40) ... 1998000000(27000000 * 74)
    // sel_div[0] divq[38] divr[0] range[3] divf[37.0]
    {27000000, 0, 38, 0, 3, 37, 0},

    // divf_int match: 1026000000(21375000 * 48) 1539000000(21375000 * 72)
    // sel_div[0] divq[48] divr[0] range[3] divf[37.0]
    {21375000, 0, 48, 0, 3, 37, 0},

    // divf_int match: 1188000000(18562500 * 64) 1485000000(18562500 * 80) 1782000000(18562500 * 96)
    // sel_div[0] divq[64] divr[0] range[3] divf[43.0]
    {18562500, 0, 64, 0, 3, 43, 0},

    // divf_int match: 1890000000(19687500 * 96)
    // sel_div[0] divq[96] divr[0] range[3] divf[69.0]
    {19687500, 0, 96, 0, 3, 69, 0},

    // divf_int match: 1350000000(18750000 * 72)
    // sel_div[0] divq[72] divr[0] range[3] divf[49.0]
    {18750000, 0, 72, 0, 3, 49, 0},

    // divf_int match: 1755000000(16250000 * 108)
    // sel_div[0] divq[108] divr[0] range[3] divf[64.0]
    {16250000, 0, 108, 0, 3, 64, 0},

    // divf_int match: 1188000000(12375000 * 96) 1485000000(12375000 * 120) 1782000000(12375000 * 144)
    // sel_div[0] divq[96] divr[0] range[3] divf[43.0]
    {12375000, 0, 96, 0, 3, 43, 0},

    // divf_int match: 1350000000(12500000 * 108)
    // sel_div[0] divq[108] divr[0] range[3] divf[49.0]
    {12500000, 0, 108, 0, 3, 49, 0},

    // divf_int match: 1080000000(10000000 * 108) 1350000000(10000000 * 135) 1620000000(10000000 * 162) 1890000000(10000000 * 189)
    // sel_div[0] divq[108] divr[0] range[3] divf[39.0]
    {10000000, 0, 108, 0, 3, 39, 0},

    // divf_int match: 1026000000(9000000 * 114) 1053000000(9000000 * 117) 1080000000(9000000 * 120) ... 1998000000(9000000 * 222)
    // sel_div[0] divq[114] divr[0] range[3] divf[37.0]
    {9000000, 0, 114, 0, 3, 37, 0},

    // divf_int match: 1134000000(7875000 * 144) 1323000000(7875000 * 168) 1512000000(7875000 * 192) 1701000000(7875000 * 216) 1890000000(7875000 * 240)
    // sel_div[0] divq[144] divr[0] range[3] divf[41.0]
    {7875000, 0, 144, 0, 3, 41, 0},

    // divf_int match: 1134000000(7560000 * 150) 1323000000(7560000 * 175) 1512000000(7560000 * 200) 1701000000(7560000 * 225) 1890000000(7560000 * 250)
    // sel_div[0] divq[150] divr[0] range[3] divf[41.0]
    {7560000, 0, 150, 0, 3, 41, 0},

    // divf_int match: 1026000000(27000000 * 38) 1053000000(27000000 * 39) 1080000000(27000000 * 40) ... 1998000000(27000000 * 74)
    // sel_div[2] divq[38] divr[0] range[3] divf[37.0]
    {27000000, 2, 38, 0, 3, 37, 0},

    // clk_khz[146250] pllout_hz[36562500] sel_div[0] divq[48] divr[0] range[3] divf[64 - 0]
    {36562500, 0, 48, 0, 3, 64, 0},

    // clk_khz[106500] pllout_hz[26625000] sel_div[0] divq[72] divr[0] range[3] divf[70 - 0]
    {26625000, 0, 72, 0, 3, 70, 0},

/* fraction pll */

    // fraction match: pixel_clk_khz 533250, pllout_mhz 133.3125
    // fraction match: pixel_clk_khz 497750, pllout_mhz 124.4375
    // fraction match: pixel_clk_khz 277250, pllout_mhz 69.3125
    // fraction match: pixel_clk_khz 115500, pllout_mhz 28.875

    // clk_khz[533120] pllout_hz[133280000] sel_div[0] divq[11] divr[3] range[0] divf[216 - 3305732]
    {133280000, 0, 11, 3, 0, 216, 3305732},

    // clk_khz[262750] pllout_hz[65687500] sel_div[0] divq[22] divr[3] range[0] divf[213 - 1553445]
    {65687500, 0, 22, 3, 0, 213, 1553445},

    // clk_khz[241500] pllout_hz[60375000] sel_div[0] divq[24] divr[3] range[0] divf[213 - 11184810]
    {60375000, 0, 24, 3, 0, 213, 11184810},

    // clk_khz[83500] pllout_hz[20875000] sel_div[0] divq[71] divr[3] range[0] divf[218 - 9631364]
    {20875000, 0, 71, 3, 0, 218, 9631364},

    // clk_khz[57284] pllout_hz[14321000] sel_div[0] divq[104] divr[3] range[0] divf[219 - 10896491]
    {14321000, 0, 104, 3, 0, 219, 10896491},

    // clk_khz[25175] pllout_hz[25175000] sel_div[2] divq[59] divr[3] range[0] divf[219 - 807791]
    //{25175000, 2, 69, 3, 0, 256, 5778818}, // G100 A0 golden
    {25175000, 2, 59, 3, 0, 219, 807791},

    // clk_khz[28320] pllout_hz[28320000] sel_div[2] divq[52] divr[3] range[0] divf[217 - 2833485]
    {28320000, 2, 52, 3, 0, 217, 2833485}
};

static disp_void_t disp_pll_generate(pdisp_pll_para_t ppll_param)
{
    disp_u8_t use_int_divf = DISP_FALSE, use_sel_div = DISP_FALSE, reg_sel_div = 0, dvo = 0;
    disp_u32_t reg_divq, reg_divr, reg_range, reg_divfi, reg_divff;
    disp_u32_t cur_pllout_multi_divq, idx;
    disp_u32_t pixel_clk_khz = ppll_param->pixel_clk_khz;
    disp_u64_t pllout_hz, temp_u64; // make sure enough precision

    if (pixel_clk_khz < PLLOUT_MIN_KHZ)
    {
        disp_print(DDM_WARNING, "disp_pll: invalid pixel_clk_khz[%u] !\n", pixel_clk_khz);
    }

    if (ppll_param->flags.bits.dvo)
    {
        /* DVO will use internal reg to set 1t1p, Sel_Div will force 0 latter. */
        dvo = 1;
        reg_sel_div = 2;
        pllout_hz = (disp_u64_t)pixel_clk_khz * 1000;
    }
    else if (ppll_param->dsc_en)
    {
        reg_sel_div = 0;
        if (pixel_clk_khz < PLLOUT_MIN_KHZ_1T4P)
        {   /* HW assert dsc use large mode, not design dsc sel_div. */
            disp_print(DDM_WARNING, "disp_pll: HW not support dsc sel_div! pixel_clk_khz[%u]\n", pixel_clk_khz);
        }

        /* horizontal slice num 1, 2, 4(8) -> PS/line 1, 2, 4 -> PLLOUT /1, /2, /4 */
        pllout_hz = (disp_u64_t)pixel_clk_khz * 1000;
        switch (ppll_param->dsc_slice)
        {
        case 1:
        case 2:
        case 4:
            pllout_hz /= ppll_param->dsc_slice;
            break;
        case 8:
            pllout_hz /= 4;
            break;
        default:
            disp_print(DDM_WARNING, "disp_pll: dsc_en but invalid dsc_slice[%u] !\n", ppll_param->dsc_slice);
            break;
        }
    }
    else
    {
        use_sel_div = (pixel_clk_khz < PLLOUT_MIN_KHZ_1T4P) ? DISP_TRUE : DISP_FALSE;
        reg_sel_div = use_sel_div ? 2 : 0;
        pllout_hz = use_sel_div ? (disp_u64_t)pixel_clk_khz * 1000 : (disp_u64_t)pixel_clk_khz * 1000 / 4;
    }

    // Try match pll in table
    for (idx = 0; idx < sizeof(g_pllout_table) / sizeof(disp_pll_table_t); idx++)
    {
        if (g_pllout_table[idx].pllout_hz == pllout_hz && g_pllout_table[idx].sel_div == reg_sel_div)
        {
            ppll_param->sel_div = g_pllout_table[idx].sel_div;
            ppll_param->divq = g_pllout_table[idx].divq;
            ppll_param->divr = g_pllout_table[idx].divr;
            ppll_param->range = g_pllout_table[idx].range;
            ppll_param->divfi = g_pllout_table[idx].divfi;
            ppll_param->divff = g_pllout_table[idx].divff;
            if (dvo)
            {   // Dvo force Sel_Div 0
                ppll_param->sel_div = 0;
            }
            return;
        }
    }

    // start from 38*27(1026)
    for (cur_pllout_multi_divq = 1026; cur_pllout_multi_divq < PLL_VCO_MAX; cur_pllout_multi_divq += PLL_REF_CLK_MHZ)
    {
        temp_u64 = (disp_u64_t)cur_pllout_multi_divq * PLL_MHZ_TO_HZ_MULTI_BASE;
        reg_divq = (disp_u32_t)(temp_u64 / pllout_hz);
        if (temp_u64 == reg_divq * pllout_hz)
        {
            use_int_divf = DISP_TRUE;
            break;
        }
    }

    if (use_int_divf)
    {
        // int divf
        reg_divr = 0;
        reg_range = 3;
        reg_divfi = cur_pllout_multi_divq / PLL_REF_CLK_MHZ - 1; // divr + 1 == 1, no need to div
        reg_divff = 0;
    }
    else
    {
        if (PLL_VCO_MAX * PLL_MHZ_TO_HZ_MULTI_BASE < pllout_hz) // pllout need <= 2000M
        {
            /* Limited by HW caps, mode that pixel clk more than 4000MHz could not come here.
             * That too large mode should filtered out before when detect mode.
             * When 0 reg_divq happened, do not care if pllout could be int_divf.
             * Too large mode example: pixel_clk[9504MHz] pllout[2376MHz]
             */
            reg_divq = 0;
            disp_print(DDM_WARNING, "disp_pll: reg_divq 0 case! pixel_clk_khz[%u], pllout_hz[%llu], reg_divq 0 %s enough!\n",
                pixel_clk_khz, pllout_hz, ((disp_u32_t)PLL_VCO_MAX * PLL_MHZ_TO_HZ_MULTI_BASE * 2 < pllout_hz) ? "still not" : "");
        }

        // Fraction force
        reg_divr = 3;
        reg_range = 0;

        // Choose middle divq value: reg_divq = (divq_min + divq_max) / 2
        reg_divq = (disp_u32_t)((disp_u64_t)PLL_MHZ_TO_HZ_MULTI_BASE * (PLL_VCO_MIN + PLL_VCO_MAX) / (pllout_hz * 2));
        reg_divq += (pllout_hz * reg_divq < PLL_VCO_MIN * PLL_MHZ_TO_HZ_MULTI_BASE) ? 1 : 0;

        // divf = pll * divq * (divr + 1) / 27M - 1 = pllout_hz * reg_divq * (reg_divr + 1) / (PLL_REF_CLK_MHZ * PLL_MHZ_TO_HZ_MULTI_BASE) - 1
        reg_divfi = (disp_u32_t)(pllout_hz * reg_divq * (reg_divr + 1) / (PLL_REF_CLK_MHZ * PLL_MHZ_TO_HZ_MULTI_BASE) - 1);
        reg_divff = (disp_u32_t)(pllout_hz * reg_divq * (reg_divr + 1) * PLL_DIVFF_PRECISION /
            (PLL_REF_CLK_MHZ * PLL_MHZ_TO_HZ_MULTI_BASE) - PLL_DIVFF_PRECISION) & (PLL_DIVFF_PRECISION - 1);
    }

    ppll_param->sel_div = reg_sel_div;
    ppll_param->divq = reg_divq;
    ppll_param->divr = reg_divr;
    ppll_param->range = reg_range;
    ppll_param->divfi = reg_divfi;
    ppll_param->divff = reg_divff;

    disp_print(DDM_INFO, "disp_pll: clk_khz[%u] pllout_hz[%llu] sel_div[%u]"
        " divq[%u] divr[%u] range[%u] divf[%u - %u]\n", pixel_clk_khz, pllout_hz,
        ppll_param->sel_div, ppll_param->divq, ppll_param->divr, ppll_param->range,
        ppll_param->divfi, ppll_param->divff);
}

static disp_void_t disp_pll_flush_common_reg(pdisp_crtc_t pdisp_crtc, pdisp_pll_para_t ppll_param)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_crtc->disp_device;

    pll_write_field(pdisp_crtc->index, Reg_Pllctl_Mmio_1, Register_Pllctl_Mmio_1_Start, 2,
        Divff, ppll_param->divff,
        Divq,  ppll_param->divq);
    pll_write_field(pdisp_crtc->index, Reg_Pllctl_Mmio_2, Register_Pllctl_Mmio_2_Start, 3,
        Divr,  ppll_param->divr,
        Divfi, ppll_param->divfi,
        Range, ppll_param->range);
    cr_write_field(pdisp_crtc->index, Reg_Cr_81, Register_Cr_81_Start, 1,
        Sel_Div, ppll_param->sel_div);
}

static disp_void_t disp_pll_flush_new(pdisp_crtc_t pdisp_crtc, pdisp_pll_para_t ppll_param)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_crtc->disp_device;
    disp_u32_t wait_time_ten_microsecond = 0;
    disp_u32_t reg_newdiv = 0;

    // 1. set common reg
    disp_pll_flush_common_reg(pdisp_crtc, ppll_param);

    // 2. set Newdiv
    reg_newdiv = 1;
    pll_write_field(pdisp_crtc->index, Reg_Pllctl_Mmio_0, Register_Pllctl_Mmio_0_Start, 1,
        Newdiv, reg_newdiv);

    // 3. wait Newdiv 1 -> 0
    while (wait_time_ten_microsecond < PLL_WAIT_TIMEOUT_TEN_MICROSECOND && reg_newdiv)
    {
        disp_delay_us(10);
        pll_read_field(pdisp_crtc->index, Reg_Pllctl_Mmio_0, Register_Pllctl_Mmio_0_Start, 1,
            Newdiv, &reg_newdiv);
        wait_time_ten_microsecond++;
    }

    if (reg_newdiv)
    {
        disp_print(DDM_WARNING, "disp_pll: wait Newdiv timeout!\n");
    }
}

static disp_void_t disp_pll_flush_reset(pdisp_crtc_t pdisp_crtc, pdisp_pll_para_t ppll_param)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_crtc->disp_device;
    disp_u32_t reg_pll_lock = 0, wait_time_ten_microsecond = 0;

    // 0. set common reg // No need second reset when first start. But not work when start again.
    //disp_pll_flush_common_reg(pdisp_crtc, ppll_param);

    // 1. reset
    pll_write_field(pdisp_crtc->index, Reg_Pllctl_Mmio_0, Register_Pllctl_Mmio_0_Start, 3,
        Reset, 1,
        Hreset_Mode, 0, // Clear legacy version GOP that enabled Hreset_Mode
        Enaout, 1);

    // 2. set common reg
    disp_pll_flush_common_reg(pdisp_crtc, ppll_param);

    // 3. reset clear
    disp_delay_us(10);
    pll_write_field(pdisp_crtc->index, Reg_Pllctl_Mmio_0, Register_Pllctl_Mmio_0_Start, 1,
        Reset, 0);

    // 4. wait pll_lock 0 -> 1
    while (wait_time_ten_microsecond < PLL_WAIT_TIMEOUT_TEN_MICROSECOND && !reg_pll_lock)
    {
        disp_delay_us(10);
        pll_read_field(pdisp_crtc->index, Reg_Pllctl_Mmio_3, Register_Pllctl_Mmio_3_Start, 1,
            Pllctl_Lock_St, &reg_pll_lock);
        wait_time_ten_microsecond++;
    }
    if (!reg_pll_lock)
    {
        disp_print(DDM_ERROR, "disp_pll: Pllctl_Lock_St wait timeout!\n");
    }

#if 0 // G100 A0 patch: Second reset for safe.
    pll_write_field(pdisp_crtc->index, Reg_Pllctl_Mmio_0, Register_Pllctl_Mmio_0_Start, 1,
        Reset, 1);
    disp_delay_us(10);
    pll_write_field(pdisp_crtc->index, Reg_Pllctl_Mmio_0, Register_Pllctl_Mmio_0_Start, 1,
        Reset, 0);
#endif
}

static disp_void_t disp_pll_dump(pdisp_s8_t pprefix, disp_u32_t level, pdisp_pll_para_t ppll_param)
{
    disp_print(level, "%s pixel_clk_khz[%u], new_div[%u] dscEn[%u]slice[%u] "
        "sel_div[%u] divq[%u] divr[%u] range[%u] divf[%u.%u]\n", pprefix ? pprefix : "",
        ppll_param->pixel_clk_khz, ppll_param->use_new_div, ppll_param->dsc_en, ppll_param->dsc_slice,
        ppll_param->sel_div, ppll_param->divq, ppll_param->divr, ppll_param->range, ppll_param->divfi, ppll_param->divff);
}

disp_void_t disp_pll_on(pdisp_void_t pdisp_crtc)
{
    disp_pll_para_t pll_param;
    pdisp_crtc_t pcrtc = (pdisp_crtc_t)pdisp_crtc;
    pdisp_device_t pdd = (pdisp_device_t)pcrtc->disp_device;
    pdisp_encoder_t pencoder = &pdd->disp_encoder[pcrtc->index];
    pdisp_dsc_t pdsc = (pdisp_dsc_t)pencoder->pdsc;

    disp_memset(&pll_param, 0, sizeof(disp_pll_para_t));

    pll_param.pixel_clk_khz = (disp_u32_t)pcrtc->property.hw_mode.clock;
    if(pdsc && pdsc->func.need_enable_dsc)
    {
        pll_param.dsc_en = pdsc->func.need_enable_dsc(pdsc);
        pll_param.dsc_slice = pdsc->policy.slice_num_hor;
    }
    pll_param.flags.bits.dvo = (DISP_ENCODER_DVO == pencoder->type) ? 1 : 0;
    disp_pll_generate(&pll_param);

    pll_param.use_new_div = DISP_FALSE; // HW not recommended new_div path
    if (pll_param.use_new_div)
        disp_pll_flush_new(pcrtc, &pll_param);
    else
        disp_pll_flush_reset(pcrtc, &pll_param);

    disp_pll_dump("\t", DDM_INFO, &pll_param);
}

disp_void_t disp_pll_off(pdisp_void_t pdisp_crtc)
{
    pdisp_crtc_t pcrtc = (pdisp_crtc_t)pdisp_crtc;
    pdisp_device_t pdd = (pdisp_device_t)pcrtc->disp_device;

    /* Stop pll will save power
     * There is another way to save power that is disabling Gate_En
     */

    pll_write_field(pcrtc->index, Reg_Pllctl_Mmio_0, Register_Pllctl_Mmio_0_Start, 1,
        Reset, 1);
}
