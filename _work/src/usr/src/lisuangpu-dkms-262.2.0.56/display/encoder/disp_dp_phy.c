/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#include "disp_dp_phy.h"
#include "disp_device.h"
#include "disp_reg.h"
#include "DP_register.h"
#include "disp_dp_aux.h"
#include "disp_dp_connector.h"

#define disp_print(level, log, ...) disp_print_ex(DISP_PRINT_MASK_DP_PHY, level, log, ##__VA_ARGS__)

dp_phy_pin_config_t dp_1p62_ss_phy_pin_config = {
	.pma_cmn_refclk_mode = 0x0, 
	.pma_cmn_refclk_int_mode = 0x0, 
	.pma_cmn_refclk_dig_div = 0x0, 
	.pma_cmn_refclk_dig_sel = 0x1, 
	.pma_cmn_pll0_refclk_sel = 0x1, 
	.pma_cmn_pll1_refclk_sel = 0x1, 
	.phy_lnn_00_mode = 0x3, 
	.phy_link_cfg_ln_n_1 = 0x1, 
	.phy_link_cfg_ln_n_1_SLV_LANE = 0x0, 
	.pma_xcvr_standard_mode_ln_n_0 = 0x0, 
	.pma_xcvr_data_width_ln_n_0 = 0x5, 
	.pma_tx_deemphasis_ln_n_0 = 0x0, 
	.pma_tx_vmargin_ln_n_0 = 0x0, 
	.pma_tx_low_power_swing_en_ln_n_0 = 0x0, 
};

dp_phy_reg_config_t dp_1p62_ss_phy_reg_config = {
	.pHY_PLL_CFG = 0x0, 
	.pHY_PMA_ISO_PLL_CTRL0 = 0xb, 
	.pHY_PMA_ISO_PLL_CTRL1 = 0x2224, 
};

dp_pma_reg_config_t dp_1p62_ss_pma_reg_config = {
	.cMN_SSM_BANDGAP_TMR = 0x1, 
	.cMN_SSM_BIAS_TMR = 0x1b, 
	.cMN_PLLSM0_PLLEN_TMR = 0xb, 
	.cMN_PLLSM0_PLLPRE_TMR = 0x36, 
	.cMN_PLLSM0_PLLLOCK_TMR = 0x10e, 
	.cMN_PLLSM1_PLLEN_TMR = 0xb, 
	.cMN_PLLSM1_PLLPRE_TMR = 0x36, 
	.cMN_PLLSM1_PLLLOCK_TMR = 0x10e, 
	.cMN_BGCAL_INIT_TMR = 0x87, 
	.cMN_BGCAL_ITER_TMR = 0x87, 
	.cMN_IBCAL_INIT_TMR = 0x1b, 
	.cMN_IBCAL_ITER_TMR = 0x13, 
	.cMN_TXPUCAL_INIT_TMR = 0x21, 
	.cMN_TXPUCAL_ITER_TMR = 0x7, 
	.cMN_TXPDCAL_INIT_TMR = 0x21, 
	.cMN_TXPDCAL_ITER_TMR = 0x7, 
	.cMN_RXCAL_INIT_TMR = 0x32a, 
	.cMN_RXCAL_ITER_TMR = 0x7, 
	.cMN_SD_CAL_INIT_TMR = 0x7, 
	.cMN_SD_CAL_ITER_TMR = 0x3, 
	.cMN_SD_CAL_REFTIM_START = 0x10, 
	.cMN_SD_CAL_PLLCNT_START = 0x139, 
	.dRV_DIAG_LANE_FCM_EN_TO = 0x21c, 
	.dRV_DIAG_LANE_FCM_EN_MGN_TMR = 0xa2, 
	.rX_SDCAL0_INIT_TMR = 0x1b, 
	.rX_SDCAL0_ITER_TMR = 0x87, 
	.rX_SDCAL1_INIT_TMR = 0x1b, 
	.rX_SDCAL1_ITER_TMR = 0x87, 
	.tX_RCVDET_ST_TMR = 0xa8c, 
	.cMN_PDIAG_PLL0_CLK_SEL_M0 = 0xf01, 
	.cMN_PDIAG_PLL0_CLK_SEL_M1 = 0x400, 
	.cMN_PDIAG_PLL1_CLK_SEL_M0 = 0x400, 
	.xCVR_DIAG_HSCLK_SEL = 0x0, 
	.xCVR_DIAG_HSCLK_DIV = 0x2, 
	.xCVR_DIAG_PLLDRC_CTRL = 0x11, 
	.xCVR_DIAG_HSCLK_SEL_TRUE = 0x0, 
	.xCVR_DIAG_HSCLK_DIV_TRUE = 0x1, 
	.xCVR_DIAG_PLLDRC_CTRL_TRUE = 0x12, 
	.cMN_PLL0_DSM_DIAG_M0 = 0x4, 
	.cMN_PLL1_DSM_DIAG_M0 = 0x4, 
	.cMN_DIAG_GPANA_1 = 0xc00, 
	.cMN_PDIAG_PLL0_ITRIM_M0 = 0x7f, 
	.cMN_PDIAG_PLL1_ITRIM_M0 = 0x7f, 
	.cMN_PDIAG_PLL0_CP_PADJ_M0 = 0xb17, 
	.cMN_PDIAG_PLL1_CP_PADJ_M0 = 0xb17, 
	.cMN_PDIAG_PLL0_CP_IADJ_M0 = 0xe01, 
	.cMN_PDIAG_PLL1_CP_IADJ_M0 = 0xe01, 
	.cMN_PDIAG_PLL0_FILT_PADJ_M0 = 0xd05, 
	.cMN_PDIAG_PLL1_FILT_PADJ_M0 = 0xd05, 
	.cMN_PLL0_DSM_FBH_OVRD_M0 = 0x24, 
	.cMN_PLL1_DSM_FBH_OVRD_M0 = 0x1c, 
	.cMN_PLL0_DSM_FBL_OVRD_M0 = 0x6, 
	.cMN_PLL1_DSM_FBL_OVRD_M0 = 0x4, 
	.cMN_PLL0_INTDIV_M0 = 0x168, 
	.cMN_PLL1_INTDIV_M0 = 0x128, 
	.cMN_PLL0_FRACDIVL_M0 = 0x0, 
	.cMN_PLL1_FRACDIVL_M0 = 0x4bda, 
	.cMN_PLL0_FRACDIVH_M0 = 0x2, 
	.cMN_PLL1_FRACDIVH_M0 = 0x2, 
	.cMN_PLL0_HIGH_THR_M0 = 0xf0, 
	.cMN_PLL1_HIGH_THR_M0 = 0xc6, 
	.cMN_PDIAG_PLL0_CTRL_M0 = 0x1002, 
	.cMN_PDIAG_PLL1_CTRL_M0 = 0x1002, 
	.cMN_PLL0_SS_CTRL1_M0 = 0x1, 
	.cMN_PLL1_SS_CTRL1_M0 = 0x1, 
	.cMN_PLL0_SS_CTRL2_M0 = 0x440, 
	.cMN_PLL1_SS_CTRL2_M0 = 0x33f, 
	.cMN_PLL0_SS_CTRL3_M0 = 0x67, 
	.cMN_PLL1_SS_CTRL3_M0 = 0x6f, 
	.cMN_PLL0_SS_CTRL4_M0 = 0x4, 
	.cMN_PLL1_SS_CTRL4_M0 = 0x4, 
	.cMN_PLL0_VCOCAL_INIT_TMR = 0x10e, 
	.cMN_PLL1_VCOCAL_INIT_TMR = 0x10e, 
	.cMN_PLL0_VCOCAL_ITER_TMR = 0x5, 
	.cMN_PLL1_VCOCAL_ITER_TMR = 0x5, 
	.cMN_PLL0_VCOCAL_REFTIM_START = 0x357, 
	.cMN_PLL1_VCOCAL_REFTIM_START = 0x357, 
	.cMN_PLL0_VCOCAL_PLLCNT_START = 0x355, 
	.cMN_PLL1_VCOCAL_PLLCNT_START = 0x355, 
	.cMN_PLL0_VCOCAL_TCTRL = 0x3, 
	.cMN_PLL1_VCOCAL_TCTRL = 0x3, 
	.cMN_PLL0_LOCK_REFCNT_START = 0xd7, 
	.cMN_PLL1_LOCK_REFCNT_START = 0xd7, 
	.cMN_PLL0_LOCK_PLLCNT_START = 0xd6, 
	.cMN_PLL1_LOCK_PLLCNT_START = 0xd6, 
	.cMN_PLL0_LOCK_PLLCNT_THR = 0x5, 
	.cMN_PLL1_LOCK_PLLCNT_THR = 0x5, 
	.tX_PSC_A0 = 0xfb, 
	.tX_PSC_A2 = 0x4aa, 
	.tX_PSC_A3 = 0x4aa, 
	.rX_PSC_A0 = 0x0, 
	.rX_PSC_A2 = 0x0, 
	.rX_PSC_A3 = 0x0, 
	.rX_PSC_CAL = 0x0, 
	.xCVR_DIAG_BIDI_CTRL = 0xf, 
	.rX_REE_GCSM1_CTRL = 0x0, 
	.rX_REE_GCSM2_CTRL = 0x0, 
	.rX_REE_PERGCSM_CTRL = 0x0, 
	.tX_TXCC_CTRL = 0x8a4, 
	.tX_TXCC_CTRL_DP_OR_EDP_DP_DE_DP_1 = 0x8a4, 
	.tX_TXCC_CTRL_DP_OR_EDP_DP_DE_DP_2 = 0x8a4, 
	.tX_TXCC_CTRL_DP_OR_EDP_DP_DE_DP_3 = 0x8a4, 
	.dRV_DIAG_TX_DRV = 0x3, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_DP_DE_DP_1 = 0x3, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_DP_DE_DP_2 = 0x3, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_DP_DE_DP_3 = 0x3, 
	.tX_TXCC_MGNFS_MULT_000 = 0x2a, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_0_DE_DP_1 = 0x1f, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_0_DE_DP_2 = 0x12, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_0_DE_DP_3 = 0x0, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_1_DE_DP_0 = 0x1f, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_1_DE_DP_1 = 0x13, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_1_DE_DP_2 = 0x0, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_2_DE_DP_0 = 0x13, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_2_DE_DP_1 = 0x0, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_3_DE_DP_0 = 0x0, 
	.tX_TXCC_CPOST_MULT_00 = 0x0, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_0_DE_DP_1 = 0x14, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_0_DE_DP_2 = 0x20, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_0_DE_DP_3 = 0x2a, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_1_DE_DP_0 = 0x0, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_1_DE_DP_1 = 0x12, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_1_DE_DP_2 = 0x1f, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_2_DE_DP_0 = 0x0, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_2_DE_DP_1 = 0x13, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_3_DE_DP_0 = 0x0, 
	.tX_TXCC_CTRL_DP_OR_EDP_EDP_VDIFF_DP_LOW_DE_DP_0 = 0x8a4, 
	.tX_TXCC_CTRL_DP_OR_EDP_EDP_VDIFF_DP_LOW_DE_DP_1 = 0x8a4, 
	.tX_TXCC_CTRL_DP_OR_EDP_EDP_VDIFF_DP_LOW_DE_DP_2 = 0x8a4, 
	.tX_TXCC_CTRL_DP_OR_EDP_EDP_VDIFF_DP_LOW_DE_DP_3 = 0x8a4, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_EDP_VDIFF_DP_LOW_DE_DP_0 = 0x3, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_EDP_VDIFF_DP_LOW_DE_DP_1 = 0x3, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_EDP_VDIFF_DP_LOW_DE_DP_2 = 0x3, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_EDP_VDIFF_DP_LOW_DE_DP_3 = 0x3, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_0_DE_DP_0 = 0x2f, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_0_DE_DP_1 = 0x2a, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_0_DE_DP_2 = 0x26, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_0_DE_DP_3 = 0x22, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_1_DE_DP_0 = 0x2b, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_1_DE_DP_1 = 0x27, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_1_DE_DP_2 = 0x23, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_2_DE_DP_0 = 0x27, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_2_DE_DP_1 = 0x23, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_3_DE_DP_0 = 0x23, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_0_DE_DP_0 = 0x0, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_0_DE_DP_1 = 0xd, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_0_DE_DP_2 = 0x15, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_0_DE_DP_3 = 0x1b, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_1_DE_DP_0 = 0x0, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_1_DE_DP_1 = 0xb, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_1_DE_DP_2 = 0x12, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_2_DE_DP_0 = 0x0, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_2_DE_DP_1 = 0x9, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_3_DE_DP_0 = 0x0, 
	.tX_TXCC_CTRL_DP_OR_EDP_EDP_VDIFF_DP_HIGH_DE_DP_0 = 0x8a4, 
	.tX_TXCC_CTRL_DP_OR_EDP_EDP_VDIFF_DP_HIGH_DE_DP_1 = 0x8a4, 
	.tX_TXCC_CTRL_DP_OR_EDP_EDP_VDIFF_DP_HIGH_DE_DP_2 = 0x8a4, 
	.tX_TXCC_CTRL_DP_OR_EDP_EDP_VDIFF_DP_HIGH_DE_DP_3 = 0x8a4, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_EDP_VDIFF_DP_HIGH_DE_DP_0 = 0x3, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_EDP_VDIFF_DP_HIGH_DE_DP_1 = 0x3, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_EDP_VDIFF_DP_HIGH_DE_DP_2 = 0x3, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_EDP_VDIFF_DP_HIGH_DE_DP_3 = 0x3, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_0_DE_DP_0 = 0x27, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_0_DE_DP_1 = 0x23, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_0_DE_DP_2 = 0x1e, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_0_DE_DP_3 = 0x1a, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_1_DE_DP_0 = 0x23, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_1_DE_DP_1 = 0x1e, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_1_DE_DP_2 = 0x1a, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_2_DE_DP_0 = 0x1e, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_2_DE_DP_1 = 0x1a, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_3_DE_DP_0 = 0x1a, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_0_DE_DP_0 = 0x0, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_0_DE_DP_1 = 0x9, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_0_DE_DP_2 = 0x10, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_0_DE_DP_3 = 0x15, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_1_DE_DP_0 = 0x0, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_1_DE_DP_1 = 0x8, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_1_DE_DP_2 = 0xe, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_2_DE_DP_0 = 0x0, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_2_DE_DP_1 = 0x7, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_3_DE_DP_0 = 0x0, 
	.cMN_DIAG_BIAS_OVRD1 = 0x7400, 
};

// Add following lines manually.

dp_pma_reg_swing_emphasis_config_t dp_1p62_ss_swing_0_emphasis_0 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x002A, 
    .tX_TXCC_CPOST_MULT_00 = 0x0000,
};

dp_pma_reg_swing_emphasis_config_t dp_1p62_ss_swing_0_emphasis_1 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x001F, 
    .tX_TXCC_CPOST_MULT_00 = 0x0014,
};

dp_pma_reg_swing_emphasis_config_t dp_1p62_ss_swing_0_emphasis_2 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x0012, 
    .tX_TXCC_CPOST_MULT_00 = 0x0020,
};

dp_pma_reg_swing_emphasis_config_t dp_1p62_ss_swing_0_emphasis_3 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x0000, 
    .tX_TXCC_CPOST_MULT_00 = 0x002A,
};

dp_pma_reg_swing_emphasis_config_t dp_1p62_ss_swing_1_emphasis_0 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x001F, 
    .tX_TXCC_CPOST_MULT_00 = 0x0000,
};

dp_pma_reg_swing_emphasis_config_t dp_1p62_ss_swing_1_emphasis_1 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x0013, 
    .tX_TXCC_CPOST_MULT_00 = 0x0012,
};

dp_pma_reg_swing_emphasis_config_t dp_1p62_ss_swing_1_emphasis_2 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x0000, 
    .tX_TXCC_CPOST_MULT_00 = 0x001F,
};

dp_pma_reg_swing_emphasis_config_t dp_1p62_ss_swing_2_emphasis_0 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x0013, 
    .tX_TXCC_CPOST_MULT_00 = 0x0000,
};

dp_pma_reg_swing_emphasis_config_t dp_1p62_ss_swing_2_emphasis_1 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x0000, 
    .tX_TXCC_CPOST_MULT_00 = 0x0013,
};

dp_pma_reg_swing_emphasis_config_t dp_1p62_ss_swing_3_emphasis_0 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x0000, 
    .tX_TXCC_CPOST_MULT_00 = 0x0000,
};

dp_phy_swing_emphasis_t dp_1p62_ss_swing_emphasis_config = {
    .swing_0_emphasis_0 = &dp_1p62_ss_swing_0_emphasis_0,
    .swing_0_emphasis_1 = &dp_1p62_ss_swing_0_emphasis_1,
    .swing_0_emphasis_2 = &dp_1p62_ss_swing_0_emphasis_2,
    .swing_0_emphasis_3 = &dp_1p62_ss_swing_0_emphasis_3,
    .swing_1_emphasis_0 = &dp_1p62_ss_swing_1_emphasis_0,
    .swing_1_emphasis_1 = &dp_1p62_ss_swing_1_emphasis_1,
    .swing_1_emphasis_2 = &dp_1p62_ss_swing_1_emphasis_2,
    .swing_2_emphasis_0 = &dp_1p62_ss_swing_2_emphasis_0,
    .swing_2_emphasis_1 = &dp_1p62_ss_swing_2_emphasis_1,
    .swing_3_emphasis_0 = &dp_1p62_ss_swing_3_emphasis_0,
};

dp_phy_config_t dp_1p62_ss_phy_config = {
    .phy_pin_config = &dp_1p62_ss_phy_pin_config,
    .phy_reg_config = &dp_1p62_ss_phy_reg_config,
    .pma_reg_config = &dp_1p62_ss_pma_reg_config,
    .swing_emphasis_config = &dp_1p62_ss_swing_emphasis_config,
};

dp_phy_pin_config_t dp_2p7_ss_phy_pin_config = {
	.pma_cmn_refclk_mode = 0x0, 
	.pma_cmn_refclk_int_mode = 0x0, 
	.pma_cmn_refclk_dig_div = 0x0, 
	.pma_cmn_refclk_dig_sel = 0x1, 
	.pma_cmn_pll0_refclk_sel = 0x1, 
	.pma_cmn_pll1_refclk_sel = 0x1, 
	.phy_lnn_00_mode = 0x3, 
	.phy_link_cfg_ln_n_1 = 0x1, 
	.phy_link_cfg_ln_n_1_SLV_LANE = 0x0, 
	.pma_xcvr_standard_mode_ln_n_0 = 0x0, 
	.pma_xcvr_data_width_ln_n_0 = 0x5, 
	.pma_tx_deemphasis_ln_n_0 = 0x0, 
	.pma_tx_vmargin_ln_n_0 = 0x0, 
	.pma_tx_low_power_swing_en_ln_n_0 = 0x0, 
};

dp_phy_reg_config_t dp_2p7_ss_phy_reg_config = {
	.pHY_PLL_CFG = 0x0, 
	.pHY_PMA_ISO_PLL_CTRL0 = 0xb, 
	.pHY_PMA_ISO_PLL_CTRL1 = 0x2224, 
};

dp_pma_reg_config_t dp_2p7_ss_pma_reg_config = {
	.cMN_SSM_BANDGAP_TMR = 0x1, 
	.cMN_SSM_BIAS_TMR = 0x1b, 
	.cMN_PLLSM0_PLLEN_TMR = 0xb, 
	.cMN_PLLSM0_PLLPRE_TMR = 0x36, 
	.cMN_PLLSM0_PLLLOCK_TMR = 0x10e, 
	.cMN_PLLSM1_PLLEN_TMR = 0xb, 
	.cMN_PLLSM1_PLLPRE_TMR = 0x36, 
	.cMN_PLLSM1_PLLLOCK_TMR = 0x10e, 
	.cMN_BGCAL_INIT_TMR = 0x87, 
	.cMN_BGCAL_ITER_TMR = 0x87, 
	.cMN_IBCAL_INIT_TMR = 0x1b, 
	.cMN_IBCAL_ITER_TMR = 0x13, 
	.cMN_TXPUCAL_INIT_TMR = 0x21, 
	.cMN_TXPUCAL_ITER_TMR = 0x7, 
	.cMN_TXPDCAL_INIT_TMR = 0x21, 
	.cMN_TXPDCAL_ITER_TMR = 0x7, 
	.cMN_RXCAL_INIT_TMR = 0x32a, 
	.cMN_RXCAL_ITER_TMR = 0x7, 
	.cMN_SD_CAL_INIT_TMR = 0x7, 
	.cMN_SD_CAL_ITER_TMR = 0x3, 
	.cMN_SD_CAL_REFTIM_START = 0x10, 
	.cMN_SD_CAL_PLLCNT_START = 0x139, 
	.dRV_DIAG_LANE_FCM_EN_TO = 0x21c, 
	.dRV_DIAG_LANE_FCM_EN_MGN_TMR = 0xa2, 
	.rX_SDCAL0_INIT_TMR = 0x1b, 
	.rX_SDCAL0_ITER_TMR = 0x87, 
	.rX_SDCAL1_INIT_TMR = 0x1b, 
	.rX_SDCAL1_ITER_TMR = 0x87, 
	.tX_RCVDET_ST_TMR = 0xa8c, 
	.cMN_PDIAG_PLL0_CLK_SEL_M0 = 0x701, 
	.cMN_PDIAG_PLL0_CLK_SEL_M1 = 0x400, 
	.cMN_PDIAG_PLL1_CLK_SEL_M0 = 0x400, 
	.xCVR_DIAG_HSCLK_SEL = 0x0, 
	.xCVR_DIAG_HSCLK_DIV = 0x1, 
	.xCVR_DIAG_PLLDRC_CTRL = 0x11, 
	.xCVR_DIAG_HSCLK_SEL_TRUE = 0x0, 
	.xCVR_DIAG_HSCLK_DIV_TRUE = 0x1, 
	.xCVR_DIAG_PLLDRC_CTRL_TRUE = 0x12, 
	.cMN_PLL0_DSM_DIAG_M0 = 0x4, 
	.cMN_PLL1_DSM_DIAG_M0 = 0x4, 
	.cMN_DIAG_GPANA_1 = 0xc00, 
	.cMN_PDIAG_PLL0_ITRIM_M0 = 0x3f, 
	.cMN_PDIAG_PLL1_ITRIM_M0 = 0x7f, 
	.cMN_PDIAG_PLL0_CP_PADJ_M0 = 0xb17, 
	.cMN_PDIAG_PLL1_CP_PADJ_M0 = 0xb17, 
	.cMN_PDIAG_PLL0_CP_IADJ_M0 = 0xe01, 
	.cMN_PDIAG_PLL1_CP_IADJ_M0 = 0xe01, 
	.cMN_PDIAG_PLL0_FILT_PADJ_M0 = 0xd05, 
	.cMN_PDIAG_PLL1_FILT_PADJ_M0 = 0xd05, 
	.cMN_PLL0_DSM_FBH_OVRD_M0 = 0x24, 
	.cMN_PLL1_DSM_FBH_OVRD_M0 = 0x1c, 
	.cMN_PLL0_DSM_FBL_OVRD_M0 = 0x6, 
	.cMN_PLL1_DSM_FBL_OVRD_M0 = 0x4, 
	.cMN_PLL0_INTDIV_M0 = 0x190, 
	.cMN_PLL1_INTDIV_M0 = 0x128, 
	.cMN_PLL0_FRACDIVL_M0 = 0x0, 
	.cMN_PLL1_FRACDIVL_M0 = 0x4bda, 
	.cMN_PLL0_FRACDIVH_M0 = 0x2, 
	.cMN_PLL1_FRACDIVH_M0 = 0x2, 
	.cMN_PLL0_HIGH_THR_M0 = 0x10c, 
	.cMN_PLL1_HIGH_THR_M0 = 0xc6, 
	.cMN_PDIAG_PLL0_CTRL_M0 = 0x1002, 
	.cMN_PDIAG_PLL1_CTRL_M0 = 0x1002, 
	.cMN_PLL0_SS_CTRL1_M0 = 0x1, 
	.cMN_PLL1_SS_CTRL1_M0 = 0x1, 
	.cMN_PLL0_SS_CTRL2_M0 = 0x46c, 
	.cMN_PLL1_SS_CTRL2_M0 = 0x33f, 
	.cMN_PLL0_SS_CTRL3_M0 = 0x6e, 
	.cMN_PLL1_SS_CTRL3_M0 = 0x6f, 
	.cMN_PLL0_SS_CTRL4_M0 = 0x4, 
	.cMN_PLL1_SS_CTRL4_M0 = 0x4, 
	.cMN_PLL0_VCOCAL_INIT_TMR = 0x10e, 
	.cMN_PLL1_VCOCAL_INIT_TMR = 0x10e, 
	.cMN_PLL0_VCOCAL_ITER_TMR = 0x5, 
	.cMN_PLL1_VCOCAL_ITER_TMR = 0x5, 
	.cMN_PLL0_VCOCAL_REFTIM_START = 0x357, 
	.cMN_PLL1_VCOCAL_REFTIM_START = 0x357, 
	.cMN_PLL0_VCOCAL_PLLCNT_START = 0x355, 
	.cMN_PLL1_VCOCAL_PLLCNT_START = 0x355, 
	.cMN_PLL0_VCOCAL_TCTRL = 0x3, 
	.cMN_PLL1_VCOCAL_TCTRL = 0x3, 
	.cMN_PLL0_LOCK_REFCNT_START = 0xd7, 
	.cMN_PLL1_LOCK_REFCNT_START = 0xd7, 
	.cMN_PLL0_LOCK_PLLCNT_START = 0xd6, 
	.cMN_PLL1_LOCK_PLLCNT_START = 0xd6, 
	.cMN_PLL0_LOCK_PLLCNT_THR = 0x5, 
	.cMN_PLL1_LOCK_PLLCNT_THR = 0x5, 
	.tX_PSC_A0 = 0xfb, 
	.tX_PSC_A2 = 0x4aa, 
	.tX_PSC_A3 = 0x4aa, 
	.rX_PSC_A0 = 0x0, 
	.rX_PSC_A2 = 0x0, 
	.rX_PSC_A3 = 0x0, 
	.rX_PSC_CAL = 0x0, 
	.xCVR_DIAG_BIDI_CTRL = 0xf, 
	.rX_REE_GCSM1_CTRL = 0x0, 
	.rX_REE_GCSM2_CTRL = 0x0, 
	.rX_REE_PERGCSM_CTRL = 0x0, 
	.tX_TXCC_CTRL = 0x8a4, 
	.tX_TXCC_CTRL_DP_OR_EDP_DP_DE_DP_1 = 0x8a4, 
	.tX_TXCC_CTRL_DP_OR_EDP_DP_DE_DP_2 = 0x8a4, 
	.tX_TXCC_CTRL_DP_OR_EDP_DP_DE_DP_3 = 0x8a4, 
	.dRV_DIAG_TX_DRV = 0x3, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_DP_DE_DP_1 = 0x3, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_DP_DE_DP_2 = 0x3, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_DP_DE_DP_3 = 0x3, 
	.tX_TXCC_MGNFS_MULT_000 = 0x2a, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_0_DE_DP_1 = 0x1f, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_0_DE_DP_2 = 0x12, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_0_DE_DP_3 = 0x0, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_1_DE_DP_0 = 0x1f, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_1_DE_DP_1 = 0x13, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_1_DE_DP_2 = 0x0, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_2_DE_DP_0 = 0x13, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_2_DE_DP_1 = 0x0, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_3_DE_DP_0 = 0x0, 
	.tX_TXCC_CPOST_MULT_00 = 0x0, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_0_DE_DP_1 = 0x14, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_0_DE_DP_2 = 0x20, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_0_DE_DP_3 = 0x2a, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_1_DE_DP_0 = 0x0, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_1_DE_DP_1 = 0x12, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_1_DE_DP_2 = 0x1f, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_2_DE_DP_0 = 0x0, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_2_DE_DP_1 = 0x13, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_3_DE_DP_0 = 0x0, 
	.tX_TXCC_CTRL_DP_OR_EDP_EDP_VDIFF_DP_LOW_DE_DP_0 = 0x8a4, 
	.tX_TXCC_CTRL_DP_OR_EDP_EDP_VDIFF_DP_LOW_DE_DP_1 = 0x8a4, 
	.tX_TXCC_CTRL_DP_OR_EDP_EDP_VDIFF_DP_LOW_DE_DP_2 = 0x8a4, 
	.tX_TXCC_CTRL_DP_OR_EDP_EDP_VDIFF_DP_LOW_DE_DP_3 = 0x8a4, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_EDP_VDIFF_DP_LOW_DE_DP_0 = 0x3, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_EDP_VDIFF_DP_LOW_DE_DP_1 = 0x3, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_EDP_VDIFF_DP_LOW_DE_DP_2 = 0x3, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_EDP_VDIFF_DP_LOW_DE_DP_3 = 0x3, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_0_DE_DP_0 = 0x2f, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_0_DE_DP_1 = 0x2a, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_0_DE_DP_2 = 0x26, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_0_DE_DP_3 = 0x22, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_1_DE_DP_0 = 0x2b, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_1_DE_DP_1 = 0x27, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_1_DE_DP_2 = 0x23, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_2_DE_DP_0 = 0x27, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_2_DE_DP_1 = 0x23, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_3_DE_DP_0 = 0x23, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_0_DE_DP_0 = 0x0, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_0_DE_DP_1 = 0xd, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_0_DE_DP_2 = 0x15, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_0_DE_DP_3 = 0x1b, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_1_DE_DP_0 = 0x0, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_1_DE_DP_1 = 0xb, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_1_DE_DP_2 = 0x12, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_2_DE_DP_0 = 0x0, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_2_DE_DP_1 = 0x9, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_3_DE_DP_0 = 0x0, 
	.tX_TXCC_CTRL_DP_OR_EDP_EDP_VDIFF_DP_HIGH_DE_DP_0 = 0x8a4, 
	.tX_TXCC_CTRL_DP_OR_EDP_EDP_VDIFF_DP_HIGH_DE_DP_1 = 0x8a4, 
	.tX_TXCC_CTRL_DP_OR_EDP_EDP_VDIFF_DP_HIGH_DE_DP_2 = 0x8a4, 
	.tX_TXCC_CTRL_DP_OR_EDP_EDP_VDIFF_DP_HIGH_DE_DP_3 = 0x8a4, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_EDP_VDIFF_DP_HIGH_DE_DP_0 = 0x3, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_EDP_VDIFF_DP_HIGH_DE_DP_1 = 0x3, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_EDP_VDIFF_DP_HIGH_DE_DP_2 = 0x3, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_EDP_VDIFF_DP_HIGH_DE_DP_3 = 0x3, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_0_DE_DP_0 = 0x27, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_0_DE_DP_1 = 0x23, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_0_DE_DP_2 = 0x1e, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_0_DE_DP_3 = 0x1a, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_1_DE_DP_0 = 0x23, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_1_DE_DP_1 = 0x1e, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_1_DE_DP_2 = 0x1a, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_2_DE_DP_0 = 0x1e, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_2_DE_DP_1 = 0x1a, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_3_DE_DP_0 = 0x1a, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_0_DE_DP_0 = 0x0, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_0_DE_DP_1 = 0x9, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_0_DE_DP_2 = 0x10, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_0_DE_DP_3 = 0x15, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_1_DE_DP_0 = 0x0, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_1_DE_DP_1 = 0x8, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_1_DE_DP_2 = 0xe, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_2_DE_DP_0 = 0x0, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_2_DE_DP_1 = 0x7, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_3_DE_DP_0 = 0x0, 
	.cMN_DIAG_BIAS_OVRD1 = 0x7400, 
};

// Add following lines manually.

dp_pma_reg_swing_emphasis_config_t dp_2p7_ss_swing_0_emphasis_0 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x002A, 
    .tX_TXCC_CPOST_MULT_00 = 0x0000,
};

dp_pma_reg_swing_emphasis_config_t dp_2p7_ss_swing_0_emphasis_1 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x001F, 
    .tX_TXCC_CPOST_MULT_00 = 0x0014,
};

dp_pma_reg_swing_emphasis_config_t dp_2p7_ss_swing_0_emphasis_2 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x0012, 
    .tX_TXCC_CPOST_MULT_00 = 0x0020,
};

dp_pma_reg_swing_emphasis_config_t dp_2p7_ss_swing_0_emphasis_3 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x0000, 
    .tX_TXCC_CPOST_MULT_00 = 0x002A,
};

dp_pma_reg_swing_emphasis_config_t dp_2p7_ss_swing_1_emphasis_0 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x001F, 
    .tX_TXCC_CPOST_MULT_00 = 0x0000,
};

dp_pma_reg_swing_emphasis_config_t dp_2p7_ss_swing_1_emphasis_1 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x0013, 
    .tX_TXCC_CPOST_MULT_00 = 0x0012,
};

dp_pma_reg_swing_emphasis_config_t dp_2p7_ss_swing_1_emphasis_2 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x0000, 
    .tX_TXCC_CPOST_MULT_00 = 0x001F,
};

dp_pma_reg_swing_emphasis_config_t dp_2p7_ss_swing_2_emphasis_0 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x0013, 
    .tX_TXCC_CPOST_MULT_00 = 0x0000,
};

dp_pma_reg_swing_emphasis_config_t dp_2p7_ss_swing_2_emphasis_1 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x0000, 
    .tX_TXCC_CPOST_MULT_00 = 0x0013,
};

dp_pma_reg_swing_emphasis_config_t dp_2p7_ss_swing_3_emphasis_0 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x0000, 
    .tX_TXCC_CPOST_MULT_00 = 0x0000,
};

dp_phy_swing_emphasis_t dp_2p7_ss_swing_emphasis_config = {
    .swing_0_emphasis_0 = &dp_2p7_ss_swing_0_emphasis_0,
    .swing_0_emphasis_1 = &dp_2p7_ss_swing_0_emphasis_1,
    .swing_0_emphasis_2 = &dp_2p7_ss_swing_0_emphasis_2,
    .swing_0_emphasis_3 = &dp_2p7_ss_swing_0_emphasis_3,
    .swing_1_emphasis_0 = &dp_2p7_ss_swing_1_emphasis_0,
    .swing_1_emphasis_1 = &dp_2p7_ss_swing_1_emphasis_1,
    .swing_1_emphasis_2 = &dp_2p7_ss_swing_1_emphasis_2,
    .swing_2_emphasis_0 = &dp_2p7_ss_swing_2_emphasis_0,
    .swing_2_emphasis_1 = &dp_2p7_ss_swing_2_emphasis_1,
    .swing_3_emphasis_0 = &dp_2p7_ss_swing_3_emphasis_0,
};

dp_phy_config_t dp_2p7_ss_phy_config = {
    .phy_pin_config = &dp_2p7_ss_phy_pin_config,
    .phy_reg_config = &dp_2p7_ss_phy_reg_config,
    .pma_reg_config = &dp_2p7_ss_pma_reg_config,
    .swing_emphasis_config = &dp_2p7_ss_swing_emphasis_config,
};

dp_phy_pin_config_t dp_5p4_ss_phy_pin_config = {
	.pma_cmn_refclk_mode = 0x0, 
	.pma_cmn_refclk_int_mode = 0x0, 
	.pma_cmn_refclk_dig_div = 0x0, 
	.pma_cmn_refclk_dig_sel = 0x1, 
	.pma_cmn_pll0_refclk_sel = 0x1, 
	.pma_cmn_pll1_refclk_sel = 0x1, 
	.phy_lnn_00_mode = 0x3, 
	.phy_link_cfg_ln_n_1 = 0x1, 
	.phy_link_cfg_ln_n_1_SLV_LANE = 0x0, 
	.pma_xcvr_standard_mode_ln_n_0 = 0x0, 
	.pma_xcvr_data_width_ln_n_0 = 0x5, 
	.pma_tx_deemphasis_ln_n_0 = 0x0, 
	.pma_tx_vmargin_ln_n_0 = 0x0, 
	.pma_tx_low_power_swing_en_ln_n_0 = 0x0, 
};

dp_phy_reg_config_t dp_5p4_ss_phy_reg_config = {
	.pHY_PLL_CFG = 0x0, 
	.pHY_PMA_ISO_PLL_CTRL0 = 0xb, 
	.pHY_PMA_ISO_PLL_CTRL1 = 0x2224, 
};

dp_pma_reg_config_t dp_5p4_ss_pma_reg_config = {
	.cMN_SSM_BANDGAP_TMR = 0x1, 
	.cMN_SSM_BIAS_TMR = 0x1b, 
	.cMN_PLLSM0_PLLEN_TMR = 0xb, 
	.cMN_PLLSM0_PLLPRE_TMR = 0x36, 
	.cMN_PLLSM0_PLLLOCK_TMR = 0x10e, 
	.cMN_PLLSM1_PLLEN_TMR = 0xb, 
	.cMN_PLLSM1_PLLPRE_TMR = 0x36, 
	.cMN_PLLSM1_PLLLOCK_TMR = 0x10e, 
	.cMN_BGCAL_INIT_TMR = 0x87, 
	.cMN_BGCAL_ITER_TMR = 0x87, 
	.cMN_IBCAL_INIT_TMR = 0x1b, 
	.cMN_IBCAL_ITER_TMR = 0x13, 
	.cMN_TXPUCAL_INIT_TMR = 0x21, 
	.cMN_TXPUCAL_ITER_TMR = 0x7, 
	.cMN_TXPDCAL_INIT_TMR = 0x21, 
	.cMN_TXPDCAL_ITER_TMR = 0x7, 
	.cMN_RXCAL_INIT_TMR = 0x32a, 
	.cMN_RXCAL_ITER_TMR = 0x7, 
	.cMN_SD_CAL_INIT_TMR = 0x7, 
	.cMN_SD_CAL_ITER_TMR = 0x3, 
	.cMN_SD_CAL_REFTIM_START = 0x10, 
	.cMN_SD_CAL_PLLCNT_START = 0x139, 
	.dRV_DIAG_LANE_FCM_EN_TO = 0x21c, 
	.dRV_DIAG_LANE_FCM_EN_MGN_TMR = 0xa2, 
	.rX_SDCAL0_INIT_TMR = 0x1b, 
	.rX_SDCAL0_ITER_TMR = 0x87, 
	.rX_SDCAL1_INIT_TMR = 0x1b, 
	.rX_SDCAL1_ITER_TMR = 0x87, 
	.tX_RCVDET_ST_TMR = 0xa8c, 
	.cMN_PDIAG_PLL0_CLK_SEL_M0 = 0x601, 
	.cMN_PDIAG_PLL0_CLK_SEL_M1 = 0x400, 
	.cMN_PDIAG_PLL1_CLK_SEL_M0 = 0x400, 
	.xCVR_DIAG_HSCLK_SEL = 0x0, 
	.xCVR_DIAG_HSCLK_DIV = 0x0, 
	.xCVR_DIAG_PLLDRC_CTRL = 0x11, 
	.xCVR_DIAG_HSCLK_SEL_TRUE = 0x0, 
	.xCVR_DIAG_HSCLK_DIV_TRUE = 0x1, 
	.xCVR_DIAG_PLLDRC_CTRL_TRUE = 0x12, 
	.cMN_PLL0_DSM_DIAG_M0 = 0x4, 
	.cMN_PLL1_DSM_DIAG_M0 = 0x4, 
	.cMN_DIAG_GPANA_1 = 0xc00, 
	.cMN_PDIAG_PLL0_ITRIM_M0 = 0x3f, 
	.cMN_PDIAG_PLL1_ITRIM_M0 = 0x7f, 
	.cMN_PDIAG_PLL0_CP_PADJ_M0 = 0xb17, 
	.cMN_PDIAG_PLL1_CP_PADJ_M0 = 0xb17, 
	.cMN_PDIAG_PLL0_CP_IADJ_M0 = 0xe01, 
	.cMN_PDIAG_PLL1_CP_IADJ_M0 = 0xe01, 
	.cMN_PDIAG_PLL0_FILT_PADJ_M0 = 0xd05, 
	.cMN_PDIAG_PLL1_FILT_PADJ_M0 = 0xd05, 
	.cMN_PLL0_DSM_FBH_OVRD_M0 = 0x24, 
	.cMN_PLL1_DSM_FBH_OVRD_M0 = 0x1c, 
	.cMN_PLL0_DSM_FBL_OVRD_M0 = 0x6, 
	.cMN_PLL1_DSM_FBL_OVRD_M0 = 0x4, 
	.cMN_PLL0_INTDIV_M0 = 0x190, 
	.cMN_PLL1_INTDIV_M0 = 0x128, 
	.cMN_PLL0_FRACDIVL_M0 = 0x0, 
	.cMN_PLL1_FRACDIVL_M0 = 0x4bda, 
	.cMN_PLL0_FRACDIVH_M0 = 0x2, 
	.cMN_PLL1_FRACDIVH_M0 = 0x2, 
	.cMN_PLL0_HIGH_THR_M0 = 0x10c, 
	.cMN_PLL1_HIGH_THR_M0 = 0xc6, 
	.cMN_PDIAG_PLL0_CTRL_M0 = 0x1002, 
	.cMN_PDIAG_PLL1_CTRL_M0 = 0x1002, 
	.cMN_PLL0_SS_CTRL1_M0 = 0x1, 
	.cMN_PLL1_SS_CTRL1_M0 = 0x1, 
	.cMN_PLL0_SS_CTRL2_M0 = 0x46c, 
	.cMN_PLL1_SS_CTRL2_M0 = 0x33f, 
	.cMN_PLL0_SS_CTRL3_M0 = 0x6e, 
	.cMN_PLL1_SS_CTRL3_M0 = 0x6f, 
	.cMN_PLL0_SS_CTRL4_M0 = 0x4, 
	.cMN_PLL1_SS_CTRL4_M0 = 0x4, 
	.cMN_PLL0_VCOCAL_INIT_TMR = 0x10e, 
	.cMN_PLL1_VCOCAL_INIT_TMR = 0x10e, 
	.cMN_PLL0_VCOCAL_ITER_TMR = 0x5, 
	.cMN_PLL1_VCOCAL_ITER_TMR = 0x5, 
	.cMN_PLL0_VCOCAL_REFTIM_START = 0x357, 
	.cMN_PLL1_VCOCAL_REFTIM_START = 0x357, 
	.cMN_PLL0_VCOCAL_PLLCNT_START = 0x355, 
	.cMN_PLL1_VCOCAL_PLLCNT_START = 0x355, 
	.cMN_PLL0_VCOCAL_TCTRL = 0x3, 
	.cMN_PLL1_VCOCAL_TCTRL = 0x3, 
	.cMN_PLL0_LOCK_REFCNT_START = 0xd7, 
	.cMN_PLL1_LOCK_REFCNT_START = 0xd7, 
	.cMN_PLL0_LOCK_PLLCNT_START = 0xd6, 
	.cMN_PLL1_LOCK_PLLCNT_START = 0xd6, 
	.cMN_PLL0_LOCK_PLLCNT_THR = 0x5, 
	.cMN_PLL1_LOCK_PLLCNT_THR = 0x5, 
	.tX_PSC_A0 = 0xfb, 
	.tX_PSC_A2 = 0x4aa, 
	.tX_PSC_A3 = 0x4aa, 
	.rX_PSC_A0 = 0x0, 
	.rX_PSC_A2 = 0x0, 
	.rX_PSC_A3 = 0x0, 
	.rX_PSC_CAL = 0x0, 
	.xCVR_DIAG_BIDI_CTRL = 0xf, 
	.rX_REE_GCSM1_CTRL = 0x0, 
	.rX_REE_GCSM2_CTRL = 0x0, 
	.rX_REE_PERGCSM_CTRL = 0x0, 
	.tX_TXCC_CTRL = 0x8a4, 
	.tX_TXCC_CTRL_DP_OR_EDP_DP_DE_DP_1 = 0x8a4, 
	.tX_TXCC_CTRL_DP_OR_EDP_DP_DE_DP_2 = 0x8a4, 
	.tX_TXCC_CTRL_DP_OR_EDP_DP_DE_DP_3 = 0x8a4, 
	.dRV_DIAG_TX_DRV = 0x3, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_DP_DE_DP_1 = 0x3, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_DP_DE_DP_2 = 0x3, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_DP_DE_DP_3 = 0x3, 
	.tX_TXCC_MGNFS_MULT_000 = 0x2a, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_0_DE_DP_1 = 0x1f, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_0_DE_DP_2 = 0x12, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_0_DE_DP_3 = 0x0, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_1_DE_DP_0 = 0x1f, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_1_DE_DP_1 = 0x13, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_1_DE_DP_2 = 0x0, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_2_DE_DP_0 = 0x13, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_2_DE_DP_1 = 0x0, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_3_DE_DP_0 = 0x0, 
	.tX_TXCC_CPOST_MULT_00 = 0x0, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_0_DE_DP_1 = 0x14, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_0_DE_DP_2 = 0x20, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_0_DE_DP_3 = 0x2a, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_1_DE_DP_0 = 0x0, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_1_DE_DP_1 = 0x12, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_1_DE_DP_2 = 0x1f, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_2_DE_DP_0 = 0x0, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_2_DE_DP_1 = 0x13, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_3_DE_DP_0 = 0x0, 
	.tX_TXCC_CTRL_DP_OR_EDP_EDP_VDIFF_DP_LOW_DE_DP_0 = 0x8a4, 
	.tX_TXCC_CTRL_DP_OR_EDP_EDP_VDIFF_DP_LOW_DE_DP_1 = 0x8a4, 
	.tX_TXCC_CTRL_DP_OR_EDP_EDP_VDIFF_DP_LOW_DE_DP_2 = 0x8a4, 
	.tX_TXCC_CTRL_DP_OR_EDP_EDP_VDIFF_DP_LOW_DE_DP_3 = 0x8a4, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_EDP_VDIFF_DP_LOW_DE_DP_0 = 0x3, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_EDP_VDIFF_DP_LOW_DE_DP_1 = 0x3, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_EDP_VDIFF_DP_LOW_DE_DP_2 = 0x3, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_EDP_VDIFF_DP_LOW_DE_DP_3 = 0x3, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_0_DE_DP_0 = 0x2f, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_0_DE_DP_1 = 0x2a, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_0_DE_DP_2 = 0x26, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_0_DE_DP_3 = 0x22, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_1_DE_DP_0 = 0x2b, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_1_DE_DP_1 = 0x27, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_1_DE_DP_2 = 0x23, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_2_DE_DP_0 = 0x27, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_2_DE_DP_1 = 0x23, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_3_DE_DP_0 = 0x23, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_0_DE_DP_0 = 0x0, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_0_DE_DP_1 = 0xd, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_0_DE_DP_2 = 0x15, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_0_DE_DP_3 = 0x1b, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_1_DE_DP_0 = 0x0, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_1_DE_DP_1 = 0xb, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_1_DE_DP_2 = 0x12, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_2_DE_DP_0 = 0x0, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_2_DE_DP_1 = 0x9, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_3_DE_DP_0 = 0x0, 
	.tX_TXCC_CTRL_DP_OR_EDP_EDP_VDIFF_DP_HIGH_DE_DP_0 = 0x8a4, 
	.tX_TXCC_CTRL_DP_OR_EDP_EDP_VDIFF_DP_HIGH_DE_DP_1 = 0x8a4, 
	.tX_TXCC_CTRL_DP_OR_EDP_EDP_VDIFF_DP_HIGH_DE_DP_2 = 0x8a4, 
	.tX_TXCC_CTRL_DP_OR_EDP_EDP_VDIFF_DP_HIGH_DE_DP_3 = 0x8a4, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_EDP_VDIFF_DP_HIGH_DE_DP_0 = 0x3, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_EDP_VDIFF_DP_HIGH_DE_DP_1 = 0x3, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_EDP_VDIFF_DP_HIGH_DE_DP_2 = 0x3, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_EDP_VDIFF_DP_HIGH_DE_DP_3 = 0x3, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_0_DE_DP_0 = 0x27, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_0_DE_DP_1 = 0x23, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_0_DE_DP_2 = 0x1e, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_0_DE_DP_3 = 0x1a, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_1_DE_DP_0 = 0x23, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_1_DE_DP_1 = 0x1e, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_1_DE_DP_2 = 0x1a, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_2_DE_DP_0 = 0x1e, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_2_DE_DP_1 = 0x1a, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_3_DE_DP_0 = 0x1a, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_0_DE_DP_0 = 0x0, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_0_DE_DP_1 = 0x9, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_0_DE_DP_2 = 0x10, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_0_DE_DP_3 = 0x15, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_1_DE_DP_0 = 0x0, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_1_DE_DP_1 = 0x8, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_1_DE_DP_2 = 0xe, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_2_DE_DP_0 = 0x0, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_2_DE_DP_1 = 0x7, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_3_DE_DP_0 = 0x0, 
	.cMN_DIAG_BIAS_OVRD1 = 0x7400, 
};

// Add following lines manually.

dp_pma_reg_swing_emphasis_config_t dp_5p4_ss_swing_0_emphasis_0 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x002A, 
    .tX_TXCC_CPOST_MULT_00 = 0x0000,
};

dp_pma_reg_swing_emphasis_config_t dp_5p4_ss_swing_0_emphasis_1 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x001F, 
    .tX_TXCC_CPOST_MULT_00 = 0x0014,
};

dp_pma_reg_swing_emphasis_config_t dp_5p4_ss_swing_0_emphasis_2 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x0012, 
    .tX_TXCC_CPOST_MULT_00 = 0x0020,
};

dp_pma_reg_swing_emphasis_config_t dp_5p4_ss_swing_0_emphasis_3 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x0000, 
    .tX_TXCC_CPOST_MULT_00 = 0x002A,
};

dp_pma_reg_swing_emphasis_config_t dp_5p4_ss_swing_1_emphasis_0 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x001F, 
    .tX_TXCC_CPOST_MULT_00 = 0x0000,
};

dp_pma_reg_swing_emphasis_config_t dp_5p4_ss_swing_1_emphasis_1 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x0013, 
    .tX_TXCC_CPOST_MULT_00 = 0x0012,
};

dp_pma_reg_swing_emphasis_config_t dp_5p4_ss_swing_1_emphasis_2 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x0000, 
    .tX_TXCC_CPOST_MULT_00 = 0x001F,
};

dp_pma_reg_swing_emphasis_config_t dp_5p4_ss_swing_2_emphasis_0 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x0013, 
    .tX_TXCC_CPOST_MULT_00 = 0x0000,
};

dp_pma_reg_swing_emphasis_config_t dp_5p4_ss_swing_2_emphasis_1 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x0000, 
    .tX_TXCC_CPOST_MULT_00 = 0x0013,
};

dp_pma_reg_swing_emphasis_config_t dp_5p4_ss_swing_3_emphasis_0 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x0000, 
    .tX_TXCC_CPOST_MULT_00 = 0x0000,
};

dp_phy_swing_emphasis_t dp_5p4_ss_swing_emphasis_config = {
    .swing_0_emphasis_0 = &dp_5p4_ss_swing_0_emphasis_0,
    .swing_0_emphasis_1 = &dp_5p4_ss_swing_0_emphasis_1,
    .swing_0_emphasis_2 = &dp_5p4_ss_swing_0_emphasis_2,
    .swing_0_emphasis_3 = &dp_5p4_ss_swing_0_emphasis_3,
    .swing_1_emphasis_0 = &dp_5p4_ss_swing_1_emphasis_0,
    .swing_1_emphasis_1 = &dp_5p4_ss_swing_1_emphasis_1,
    .swing_1_emphasis_2 = &dp_5p4_ss_swing_1_emphasis_2,
    .swing_2_emphasis_0 = &dp_5p4_ss_swing_2_emphasis_0,
    .swing_2_emphasis_1 = &dp_5p4_ss_swing_2_emphasis_1,
    .swing_3_emphasis_0 = &dp_5p4_ss_swing_3_emphasis_0,
};

dp_phy_config_t dp_5p4_ss_phy_config = {
    .phy_pin_config = &dp_5p4_ss_phy_pin_config,
    .phy_reg_config = &dp_5p4_ss_phy_reg_config,
    .pma_reg_config = &dp_5p4_ss_pma_reg_config,
    .swing_emphasis_config = &dp_5p4_ss_swing_emphasis_config,
};

dp_phy_pin_config_t dp_8p1_ss_phy_pin_config = {
	.pma_cmn_refclk_mode = 0x0, 
	.pma_cmn_refclk_int_mode = 0x0, 
	.pma_cmn_refclk_dig_div = 0x0, 
	.pma_cmn_refclk_dig_sel = 0x1, 
	.pma_cmn_pll0_refclk_sel = 0x1, 
	.pma_cmn_pll1_refclk_sel = 0x1, 
	.phy_lnn_00_mode = 0x3, 
	.phy_link_cfg_ln_n_1 = 0x1, 
	.phy_link_cfg_ln_n_1_SLV_LANE = 0x0, 
	.pma_xcvr_standard_mode_ln_n_0 = 0x0, 
	.pma_xcvr_data_width_ln_n_0 = 0x5, 
	.pma_tx_deemphasis_ln_n_0 = 0x0, 
	.pma_tx_vmargin_ln_n_0 = 0x0, 
	.pma_tx_low_power_swing_en_ln_n_0 = 0x0, 
};

dp_phy_reg_config_t dp_8p1_ss_phy_reg_config = {
	.pHY_PLL_CFG = 0x0, 
	.pHY_PMA_ISO_PLL_CTRL0 = 0xb, 
	.pHY_PMA_ISO_PLL_CTRL1 = 0x2224, 
};

dp_pma_reg_config_t dp_8p1_ss_pma_reg_config = {
	.cMN_SSM_BANDGAP_TMR = 0x1, 
	.cMN_SSM_BIAS_TMR = 0x1b, 
	.cMN_PLLSM0_PLLEN_TMR = 0xb, 
	.cMN_PLLSM0_PLLPRE_TMR = 0x36, 
	.cMN_PLLSM0_PLLLOCK_TMR = 0x10e, 
	.cMN_PLLSM1_PLLEN_TMR = 0xb, 
	.cMN_PLLSM1_PLLPRE_TMR = 0x36, 
	.cMN_PLLSM1_PLLLOCK_TMR = 0x10e, 
	.cMN_BGCAL_INIT_TMR = 0x87, 
	.cMN_BGCAL_ITER_TMR = 0x87, 
	.cMN_IBCAL_INIT_TMR = 0x1b, 
	.cMN_IBCAL_ITER_TMR = 0x13, 
	.cMN_TXPUCAL_INIT_TMR = 0x21, 
	.cMN_TXPUCAL_ITER_TMR = 0x7, 
	.cMN_TXPDCAL_INIT_TMR = 0x21, 
	.cMN_TXPDCAL_ITER_TMR = 0x7, 
	.cMN_RXCAL_INIT_TMR = 0x32a, 
	.cMN_RXCAL_ITER_TMR = 0x7, 
	.cMN_SD_CAL_INIT_TMR = 0x7, 
	.cMN_SD_CAL_ITER_TMR = 0x3, 
	.cMN_SD_CAL_REFTIM_START = 0x10, 
	.cMN_SD_CAL_PLLCNT_START = 0x139, 
	.dRV_DIAG_LANE_FCM_EN_TO = 0x21c, 
	.dRV_DIAG_LANE_FCM_EN_MGN_TMR = 0xa2, 
	.rX_SDCAL0_INIT_TMR = 0x1b, 
	.rX_SDCAL0_ITER_TMR = 0x87, 
	.rX_SDCAL1_INIT_TMR = 0x1b, 
	.rX_SDCAL1_ITER_TMR = 0x87, 
	.tX_RCVDET_ST_TMR = 0xa8c, 
	.cMN_PDIAG_PLL0_CLK_SEL_M0 = 0x200, 
	.cMN_PDIAG_PLL0_CLK_SEL_M1 = 0x400, 
	.cMN_PDIAG_PLL1_CLK_SEL_M0 = 0x400, 
	.xCVR_DIAG_HSCLK_SEL = 0x0, 
	.xCVR_DIAG_HSCLK_DIV = 0x0, 
	.xCVR_DIAG_PLLDRC_CTRL = 0x11, 
	.xCVR_DIAG_HSCLK_SEL_TRUE = 0x0, 
	.xCVR_DIAG_HSCLK_DIV_TRUE = 0x1, 
	.xCVR_DIAG_PLLDRC_CTRL_TRUE = 0x12, 
	.cMN_PLL0_DSM_DIAG_M0 = 0x4, 
	.cMN_PLL1_DSM_DIAG_M0 = 0x4, 
	.cMN_DIAG_GPANA_1 = 0xc00, 
	.cMN_PDIAG_PLL0_ITRIM_M0 = 0x7f, 
	.cMN_PDIAG_PLL1_ITRIM_M0 = 0x7f, 
	.cMN_PDIAG_PLL0_CP_PADJ_M0 = 0xb17, 
	.cMN_PDIAG_PLL1_CP_PADJ_M0 = 0xb17, 
	.cMN_PDIAG_PLL0_CP_IADJ_M0 = 0xe01, 
	.cMN_PDIAG_PLL1_CP_IADJ_M0 = 0xe01, 
	.cMN_PDIAG_PLL0_FILT_PADJ_M0 = 0xd05, 
	.cMN_PDIAG_PLL1_FILT_PADJ_M0 = 0xd05, 
	.cMN_PLL0_DSM_FBH_OVRD_M0 = 0x24, 
	.cMN_PLL1_DSM_FBH_OVRD_M0 = 0x1c, 
	.cMN_PLL0_DSM_FBL_OVRD_M0 = 0x6, 
	.cMN_PLL1_DSM_FBL_OVRD_M0 = 0x4, 
	.cMN_PLL0_INTDIV_M0 = 0x12c, 
	.cMN_PLL1_INTDIV_M0 = 0x128, 
	.cMN_PLL0_FRACDIVL_M0 = 0x0, 
	.cMN_PLL1_FRACDIVL_M0 = 0x4bda, 
	.cMN_PLL0_FRACDIVH_M0 = 0x2, 
	.cMN_PLL1_FRACDIVH_M0 = 0x2, 
	.cMN_PLL0_HIGH_THR_M0 = 0xc8, 
	.cMN_PLL1_HIGH_THR_M0 = 0xc6, 
	.cMN_PDIAG_PLL0_CTRL_M0 = 0x1002, 
	.cMN_PDIAG_PLL1_CTRL_M0 = 0x1002, 
	.cMN_PLL0_SS_CTRL1_M0 = 0x1, 
	.cMN_PLL1_SS_CTRL1_M0 = 0x1, 
	.cMN_PLL0_SS_CTRL2_M0 = 0x351, 
	.cMN_PLL1_SS_CTRL2_M0 = 0x33f, 
	.cMN_PLL0_SS_CTRL3_M0 = 0x6e, 
	.cMN_PLL1_SS_CTRL3_M0 = 0x6f, 
	.cMN_PLL0_SS_CTRL4_M0 = 0x4, 
	.cMN_PLL1_SS_CTRL4_M0 = 0x4, 
	.cMN_PLL0_VCOCAL_INIT_TMR = 0x10e, 
	.cMN_PLL1_VCOCAL_INIT_TMR = 0x10e, 
	.cMN_PLL0_VCOCAL_ITER_TMR = 0x5, 
	.cMN_PLL1_VCOCAL_ITER_TMR = 0x5, 
	.cMN_PLL0_VCOCAL_REFTIM_START = 0x357, 
	.cMN_PLL1_VCOCAL_REFTIM_START = 0x357, 
	.cMN_PLL0_VCOCAL_PLLCNT_START = 0x355, 
	.cMN_PLL1_VCOCAL_PLLCNT_START = 0x355, 
	.cMN_PLL0_VCOCAL_TCTRL = 0x3, 
	.cMN_PLL1_VCOCAL_TCTRL = 0x3, 
	.cMN_PLL0_LOCK_REFCNT_START = 0xd7, 
	.cMN_PLL1_LOCK_REFCNT_START = 0xd7, 
	.cMN_PLL0_LOCK_PLLCNT_START = 0xd6, 
	.cMN_PLL1_LOCK_PLLCNT_START = 0xd6, 
	.cMN_PLL0_LOCK_PLLCNT_THR = 0x5, 
	.cMN_PLL1_LOCK_PLLCNT_THR = 0x5, 
	.tX_PSC_A0 = 0xfb, 
	.tX_PSC_A2 = 0x4aa, 
	.tX_PSC_A3 = 0x4aa, 
	.rX_PSC_A0 = 0x0, 
	.rX_PSC_A2 = 0x0, 
	.rX_PSC_A3 = 0x0, 
	.rX_PSC_CAL = 0x0, 
	.xCVR_DIAG_BIDI_CTRL = 0xf, 
	.rX_REE_GCSM1_CTRL = 0x0, 
	.rX_REE_GCSM2_CTRL = 0x0, 
	.rX_REE_PERGCSM_CTRL = 0x0, 
	.tX_TXCC_CTRL = 0x8a4, 
	.tX_TXCC_CTRL_DP_OR_EDP_DP_DE_DP_1 = 0x8a4, 
	.tX_TXCC_CTRL_DP_OR_EDP_DP_DE_DP_2 = 0x8a4, 
	.tX_TXCC_CTRL_DP_OR_EDP_DP_DE_DP_3 = 0x8a4, 
	.dRV_DIAG_TX_DRV = 0x3, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_DP_DE_DP_1 = 0x3, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_DP_DE_DP_2 = 0x3, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_DP_DE_DP_3 = 0x3, 
	.tX_TXCC_MGNFS_MULT_000 = 0x2a, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_0_DE_DP_1 = 0x1f, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_0_DE_DP_2 = 0x12, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_0_DE_DP_3 = 0x0, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_1_DE_DP_0 = 0x1f, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_1_DE_DP_1 = 0x13, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_1_DE_DP_2 = 0x0, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_2_DE_DP_0 = 0x13, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_2_DE_DP_1 = 0x0, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_3_DE_DP_0 = 0x0, 
	.tX_TXCC_CPOST_MULT_00 = 0x0, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_0_DE_DP_1 = 0x14, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_0_DE_DP_2 = 0x20, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_0_DE_DP_3 = 0x2a, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_1_DE_DP_0 = 0x0, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_1_DE_DP_1 = 0x12, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_1_DE_DP_2 = 0x1f, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_2_DE_DP_0 = 0x0, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_2_DE_DP_1 = 0x13, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_3_DE_DP_0 = 0x0, 
	.tX_TXCC_CTRL_DP_OR_EDP_EDP_VDIFF_DP_LOW_DE_DP_0 = 0x8a4, 
	.tX_TXCC_CTRL_DP_OR_EDP_EDP_VDIFF_DP_LOW_DE_DP_1 = 0x8a4, 
	.tX_TXCC_CTRL_DP_OR_EDP_EDP_VDIFF_DP_LOW_DE_DP_2 = 0x8a4, 
	.tX_TXCC_CTRL_DP_OR_EDP_EDP_VDIFF_DP_LOW_DE_DP_3 = 0x8a4, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_EDP_VDIFF_DP_LOW_DE_DP_0 = 0x3, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_EDP_VDIFF_DP_LOW_DE_DP_1 = 0x3, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_EDP_VDIFF_DP_LOW_DE_DP_2 = 0x3, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_EDP_VDIFF_DP_LOW_DE_DP_3 = 0x3, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_0_DE_DP_0 = 0x2f, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_0_DE_DP_1 = 0x2a, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_0_DE_DP_2 = 0x26, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_0_DE_DP_3 = 0x22, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_1_DE_DP_0 = 0x2b, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_1_DE_DP_1 = 0x27, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_1_DE_DP_2 = 0x23, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_2_DE_DP_0 = 0x27, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_2_DE_DP_1 = 0x23, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_3_DE_DP_0 = 0x23, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_0_DE_DP_0 = 0x0, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_0_DE_DP_1 = 0xd, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_0_DE_DP_2 = 0x15, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_0_DE_DP_3 = 0x1b, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_1_DE_DP_0 = 0x0, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_1_DE_DP_1 = 0xb, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_1_DE_DP_2 = 0x12, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_2_DE_DP_0 = 0x0, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_2_DE_DP_1 = 0x9, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_3_DE_DP_0 = 0x0, 
	.tX_TXCC_CTRL_DP_OR_EDP_EDP_VDIFF_DP_HIGH_DE_DP_0 = 0x8a4, 
	.tX_TXCC_CTRL_DP_OR_EDP_EDP_VDIFF_DP_HIGH_DE_DP_1 = 0x8a4, 
	.tX_TXCC_CTRL_DP_OR_EDP_EDP_VDIFF_DP_HIGH_DE_DP_2 = 0x8a4, 
	.tX_TXCC_CTRL_DP_OR_EDP_EDP_VDIFF_DP_HIGH_DE_DP_3 = 0x8a4, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_EDP_VDIFF_DP_HIGH_DE_DP_0 = 0x3, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_EDP_VDIFF_DP_HIGH_DE_DP_1 = 0x3, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_EDP_VDIFF_DP_HIGH_DE_DP_2 = 0x3, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_EDP_VDIFF_DP_HIGH_DE_DP_3 = 0x3, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_0_DE_DP_0 = 0x27, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_0_DE_DP_1 = 0x23, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_0_DE_DP_2 = 0x1e, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_0_DE_DP_3 = 0x1a, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_1_DE_DP_0 = 0x23, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_1_DE_DP_1 = 0x1e, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_1_DE_DP_2 = 0x1a, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_2_DE_DP_0 = 0x1e, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_2_DE_DP_1 = 0x1a, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_3_DE_DP_0 = 0x1a, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_0_DE_DP_0 = 0x0, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_0_DE_DP_1 = 0x9, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_0_DE_DP_2 = 0x10, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_0_DE_DP_3 = 0x15, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_1_DE_DP_0 = 0x0, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_1_DE_DP_1 = 0x8, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_1_DE_DP_2 = 0xe, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_2_DE_DP_0 = 0x0, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_2_DE_DP_1 = 0x7, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_3_DE_DP_0 = 0x0, 
	.cMN_DIAG_BIAS_OVRD1 = 0x7400, 
};

// Add following lines manually.

dp_pma_reg_swing_emphasis_config_t dp_8p1_ss_swing_0_emphasis_0 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x002A, 
    .tX_TXCC_CPOST_MULT_00 = 0x0000,
};

dp_pma_reg_swing_emphasis_config_t dp_8p1_ss_swing_0_emphasis_1 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x001F, 
    .tX_TXCC_CPOST_MULT_00 = 0x0014,
};

dp_pma_reg_swing_emphasis_config_t dp_8p1_ss_swing_0_emphasis_2 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x0012, 
    .tX_TXCC_CPOST_MULT_00 = 0x0020,
};

dp_pma_reg_swing_emphasis_config_t dp_8p1_ss_swing_0_emphasis_3 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x0000, 
    .tX_TXCC_CPOST_MULT_00 = 0x002A,
};

dp_pma_reg_swing_emphasis_config_t dp_8p1_ss_swing_1_emphasis_0 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x001F, 
    .tX_TXCC_CPOST_MULT_00 = 0x0000,
};

dp_pma_reg_swing_emphasis_config_t dp_8p1_ss_swing_1_emphasis_1 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x0013, 
    .tX_TXCC_CPOST_MULT_00 = 0x0012,
};

dp_pma_reg_swing_emphasis_config_t dp_8p1_ss_swing_1_emphasis_2 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x0000, 
    .tX_TXCC_CPOST_MULT_00 = 0x001F,
};

dp_pma_reg_swing_emphasis_config_t dp_8p1_ss_swing_2_emphasis_0 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x0013, 
    .tX_TXCC_CPOST_MULT_00 = 0x0000,
};

dp_pma_reg_swing_emphasis_config_t dp_8p1_ss_swing_2_emphasis_1 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x0000, 
    .tX_TXCC_CPOST_MULT_00 = 0x0013,
};

dp_pma_reg_swing_emphasis_config_t dp_8p1_ss_swing_3_emphasis_0 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x0000, 
    .tX_TXCC_CPOST_MULT_00 = 0x0000,
};

dp_phy_swing_emphasis_t dp_8p1_ss_swing_emphasis_config = {
    .swing_0_emphasis_0 = &dp_8p1_ss_swing_0_emphasis_0,
    .swing_0_emphasis_1 = &dp_8p1_ss_swing_0_emphasis_1,
    .swing_0_emphasis_2 = &dp_8p1_ss_swing_0_emphasis_2,
    .swing_0_emphasis_3 = &dp_8p1_ss_swing_0_emphasis_3,
    .swing_1_emphasis_0 = &dp_8p1_ss_swing_1_emphasis_0,
    .swing_1_emphasis_1 = &dp_8p1_ss_swing_1_emphasis_1,
    .swing_1_emphasis_2 = &dp_8p1_ss_swing_1_emphasis_2,
    .swing_2_emphasis_0 = &dp_8p1_ss_swing_2_emphasis_0,
    .swing_2_emphasis_1 = &dp_8p1_ss_swing_2_emphasis_1,
    .swing_3_emphasis_0 = &dp_8p1_ss_swing_3_emphasis_0,
};

dp_phy_config_t dp_8p1_ss_phy_config = {
    .phy_pin_config = &dp_8p1_ss_phy_pin_config,
    .phy_reg_config = &dp_8p1_ss_phy_reg_config,
    .pma_reg_config = &dp_8p1_ss_pma_reg_config,
    .swing_emphasis_config = &dp_8p1_ss_swing_emphasis_config,
};

dp_phy_pin_config_t dp_1p62_phy_pin_config = {
	.pma_cmn_refclk_mode = 0x0, 
	.pma_cmn_refclk_int_mode = 0x0, 
	.pma_cmn_refclk_dig_div = 0x0, 
	.pma_cmn_refclk_dig_sel = 0x1, 
	.pma_cmn_pll0_refclk_sel = 0x1, 
	.pma_cmn_pll1_refclk_sel = 0x1, 
	.phy_lnn_00_mode = 0x3, 
	.phy_link_cfg_ln_n_1 = 0x1, 
	.phy_link_cfg_ln_n_1_SLV_LANE = 0x0, 
	.pma_xcvr_standard_mode_ln_n_0 = 0x0, 
	.pma_xcvr_data_width_ln_n_0 = 0x5, 
	.pma_tx_deemphasis_ln_n_0 = 0x0, 
	.pma_tx_vmargin_ln_n_0 = 0x0, 
	.pma_tx_low_power_swing_en_ln_n_0 = 0x0, 
};

dp_phy_reg_config_t dp_1p62_phy_reg_config = {
	.pHY_PLL_CFG = 0x0, 
	.pHY_PMA_ISO_PLL_CTRL0 = 0xb, 
	.pHY_PMA_ISO_PLL_CTRL1 = 0x2224, 
};

dp_pma_reg_config_t dp_1p62_pma_reg_config = {
	.cMN_SSM_BANDGAP_TMR = 0x1, 
	.cMN_SSM_BIAS_TMR = 0x1b, 
	.cMN_PLLSM0_PLLEN_TMR = 0xb, 
	.cMN_PLLSM0_PLLPRE_TMR = 0x36, 
	.cMN_PLLSM0_PLLLOCK_TMR = 0x29, 
	.cMN_PLLSM1_PLLEN_TMR = 0xb, 
	.cMN_PLLSM1_PLLPRE_TMR = 0x36, 
	.cMN_PLLSM1_PLLLOCK_TMR = 0x10e, 
	.cMN_BGCAL_INIT_TMR = 0x87, 
	.cMN_BGCAL_ITER_TMR = 0x87, 
	.cMN_IBCAL_INIT_TMR = 0x1b, 
	.cMN_IBCAL_ITER_TMR = 0x13, 
	.cMN_TXPUCAL_INIT_TMR = 0x21, 
	.cMN_TXPUCAL_ITER_TMR = 0x7, 
	.cMN_TXPDCAL_INIT_TMR = 0x21, 
	.cMN_TXPDCAL_ITER_TMR = 0x7, 
	.cMN_RXCAL_INIT_TMR = 0x32a, 
	.cMN_RXCAL_ITER_TMR = 0x7, 
	.cMN_SD_CAL_INIT_TMR = 0x7, 
	.cMN_SD_CAL_ITER_TMR = 0x3, 
	.cMN_SD_CAL_REFTIM_START = 0x10, 
	.cMN_SD_CAL_PLLCNT_START = 0x139, 
	.dRV_DIAG_LANE_FCM_EN_TO = 0x21c, 
	.dRV_DIAG_LANE_FCM_EN_MGN_TMR = 0xa2, 
	.rX_SDCAL0_INIT_TMR = 0x1b, 
	.rX_SDCAL0_ITER_TMR = 0x87, 
	.rX_SDCAL1_INIT_TMR = 0x1b, 
	.rX_SDCAL1_ITER_TMR = 0x87, 
	.tX_RCVDET_ST_TMR = 0xa8c, 
	.cMN_PDIAG_PLL0_CLK_SEL_M0 = 0xf01, 
	.cMN_PDIAG_PLL0_CLK_SEL_M1 = 0x400, 
	.cMN_PDIAG_PLL1_CLK_SEL_M0 = 0x400, 
	.xCVR_DIAG_HSCLK_SEL = 0x0, 
	.xCVR_DIAG_HSCLK_DIV = 0x2, 
	.xCVR_DIAG_PLLDRC_CTRL = 0x11, 
	.xCVR_DIAG_HSCLK_SEL_TRUE = 0x0, 
	.xCVR_DIAG_HSCLK_DIV_TRUE = 0x1, 
	.xCVR_DIAG_PLLDRC_CTRL_TRUE = 0x12, 
	.cMN_PLL0_DSM_DIAG_M0 = 0x4004, 
	.cMN_PLL1_DSM_DIAG_M0 = 0x4, 
	.cMN_DIAG_GPANA_1 = 0xc00, 
	.cMN_PDIAG_PLL0_ITRIM_M0 = 0x7f, 
	.cMN_PDIAG_PLL1_ITRIM_M0 = 0x7f, 
	.cMN_PDIAG_PLL0_CP_PADJ_M0 = 0x31, 
	.cMN_PDIAG_PLL1_CP_PADJ_M0 = 0xb17, 
	.cMN_PDIAG_PLL0_CP_IADJ_M0 = 0xf07, 
	.cMN_PDIAG_PLL1_CP_IADJ_M0 = 0xe01, 
	.cMN_PDIAG_PLL0_FILT_PADJ_M0 = 0x100, 
	.cMN_PDIAG_PLL1_FILT_PADJ_M0 = 0xd05, 
	.cMN_PLL0_DSM_FBH_OVRD_M0 = 0x76, 
	.cMN_PLL1_DSM_FBH_OVRD_M0 = 0x1c, 
	.cMN_PLL0_DSM_FBL_OVRD_M0 = 0x36, 
	.cMN_PLL1_DSM_FBL_OVRD_M0 = 0x4, 
	.cMN_PLL0_INTDIV_M0 = 0x0, 
	.cMN_PLL1_INTDIV_M0 = 0x128, 
	.cMN_PLL0_FRACDIVL_M0 = 0x0, 
	.cMN_PLL1_FRACDIVL_M0 = 0x4bda, 
	.cMN_PLL0_FRACDIVH_M0 = 0x0, 
	.cMN_PLL1_FRACDIVH_M0 = 0x2, 
	.cMN_PLL0_HIGH_THR_M0 = 0x0, 
	.cMN_PLL1_HIGH_THR_M0 = 0xc6, 
	.cMN_PDIAG_PLL0_CTRL_M0 = 0x1012, 
	.cMN_PDIAG_PLL1_CTRL_M0 = 0x1002, 
	.cMN_PLL0_SS_CTRL1_M0 = 0x2, 
	.cMN_PLL1_SS_CTRL1_M0 = 0x2, 
	.cMN_PLL0_SS_CTRL2_M0 = 0x0, 
	.cMN_PLL1_SS_CTRL2_M0 = 0x0, 
	.cMN_PLL0_SS_CTRL3_M0 = 0x0, 
	.cMN_PLL1_SS_CTRL3_M0 = 0x0, 
	.cMN_PLL0_SS_CTRL4_M0 = 0x0, 
	.cMN_PLL1_SS_CTRL4_M0 = 0x0, 
	.cMN_PLL0_VCOCAL_INIT_TMR = 0x10e, 
	.cMN_PLL1_VCOCAL_INIT_TMR = 0x10e, 
	.cMN_PLL0_VCOCAL_ITER_TMR = 0x5, 
	.cMN_PLL1_VCOCAL_ITER_TMR = 0x5, 
	.cMN_PLL0_VCOCAL_REFTIM_START = 0x357, 
	.cMN_PLL1_VCOCAL_REFTIM_START = 0x357, 
	.cMN_PLL0_VCOCAL_PLLCNT_START = 0x357, 
	.cMN_PLL1_VCOCAL_PLLCNT_START = 0x357, 
	.cMN_PLL0_VCOCAL_TCTRL = 0x3, 
	.cMN_PLL1_VCOCAL_TCTRL = 0x3, 
	.cMN_PLL0_LOCK_REFCNT_START = 0xd7, 
	.cMN_PLL1_LOCK_REFCNT_START = 0xd7, 
	.cMN_PLL0_LOCK_PLLCNT_START = 0xd7, 
	.cMN_PLL1_LOCK_PLLCNT_START = 0xd7, 
	.cMN_PLL0_LOCK_PLLCNT_THR = 0x3, 
	.cMN_PLL1_LOCK_PLLCNT_THR = 0x3, 
	// .tX_PSC_A0 = 0xfb, 
	// .tX_PSC_A2 = 0x4aa, 
	// .tX_PSC_A3 = 0x4aa, 
    .tX_PSC_A0 = 0xfb, 
	.tX_PSC_A2 = 0x4aa, 
	.tX_PSC_A3 = 0x4aa, 
	.rX_PSC_A0 = 0x0, 
	.rX_PSC_A2 = 0x0, 
	.rX_PSC_A3 = 0x0, 
	.rX_PSC_CAL = 0x0, 
	.xCVR_DIAG_BIDI_CTRL = 0xf, 
	.rX_REE_GCSM1_CTRL = 0x0, 
	.rX_REE_GCSM2_CTRL = 0x0, 
	.rX_REE_PERGCSM_CTRL = 0x0, 
	.tX_TXCC_CTRL = 0x8a4, 
	.tX_TXCC_CTRL_DP_OR_EDP_DP_DE_DP_1 = 0x8a4, 
	.tX_TXCC_CTRL_DP_OR_EDP_DP_DE_DP_2 = 0x8a4, 
	.tX_TXCC_CTRL_DP_OR_EDP_DP_DE_DP_3 = 0x8a4, 
	.dRV_DIAG_TX_DRV = 0x3, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_DP_DE_DP_1 = 0x3, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_DP_DE_DP_2 = 0x3, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_DP_DE_DP_3 = 0x3, 
	.tX_TXCC_MGNFS_MULT_000 = 0x2a, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_0_DE_DP_1 = 0x1f, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_0_DE_DP_2 = 0x12, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_0_DE_DP_3 = 0x0, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_1_DE_DP_0 = 0x1f, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_1_DE_DP_1 = 0x13, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_1_DE_DP_2 = 0x0, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_2_DE_DP_0 = 0x13, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_2_DE_DP_1 = 0x0, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_3_DE_DP_0 = 0x0, 
	.tX_TXCC_CPOST_MULT_00 = 0x0, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_0_DE_DP_1 = 0x14, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_0_DE_DP_2 = 0x20, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_0_DE_DP_3 = 0x2a, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_1_DE_DP_0 = 0x0, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_1_DE_DP_1 = 0x12, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_1_DE_DP_2 = 0x1f, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_2_DE_DP_0 = 0x0, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_2_DE_DP_1 = 0x13, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_3_DE_DP_0 = 0x0, 
	.tX_TXCC_CTRL_DP_OR_EDP_EDP_VDIFF_DP_LOW_DE_DP_0 = 0x8a4, 
	.tX_TXCC_CTRL_DP_OR_EDP_EDP_VDIFF_DP_LOW_DE_DP_1 = 0x8a4, 
	.tX_TXCC_CTRL_DP_OR_EDP_EDP_VDIFF_DP_LOW_DE_DP_2 = 0x8a4, 
	.tX_TXCC_CTRL_DP_OR_EDP_EDP_VDIFF_DP_LOW_DE_DP_3 = 0x8a4, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_EDP_VDIFF_DP_LOW_DE_DP_0 = 0x3, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_EDP_VDIFF_DP_LOW_DE_DP_1 = 0x3, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_EDP_VDIFF_DP_LOW_DE_DP_2 = 0x3, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_EDP_VDIFF_DP_LOW_DE_DP_3 = 0x3, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_0_DE_DP_0 = 0x2f, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_0_DE_DP_1 = 0x2a, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_0_DE_DP_2 = 0x26, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_0_DE_DP_3 = 0x22, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_1_DE_DP_0 = 0x2b, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_1_DE_DP_1 = 0x27, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_1_DE_DP_2 = 0x23, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_2_DE_DP_0 = 0x27, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_2_DE_DP_1 = 0x23, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_3_DE_DP_0 = 0x23, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_0_DE_DP_0 = 0x0, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_0_DE_DP_1 = 0xd, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_0_DE_DP_2 = 0x15, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_0_DE_DP_3 = 0x1b, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_1_DE_DP_0 = 0x0, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_1_DE_DP_1 = 0xb, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_1_DE_DP_2 = 0x12, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_2_DE_DP_0 = 0x0, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_2_DE_DP_1 = 0x9, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_3_DE_DP_0 = 0x0, 
	.tX_TXCC_CTRL_DP_OR_EDP_EDP_VDIFF_DP_HIGH_DE_DP_0 = 0x8a4, 
	.tX_TXCC_CTRL_DP_OR_EDP_EDP_VDIFF_DP_HIGH_DE_DP_1 = 0x8a4, 
	.tX_TXCC_CTRL_DP_OR_EDP_EDP_VDIFF_DP_HIGH_DE_DP_2 = 0x8a4, 
	.tX_TXCC_CTRL_DP_OR_EDP_EDP_VDIFF_DP_HIGH_DE_DP_3 = 0x8a4, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_EDP_VDIFF_DP_HIGH_DE_DP_0 = 0x3, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_EDP_VDIFF_DP_HIGH_DE_DP_1 = 0x3, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_EDP_VDIFF_DP_HIGH_DE_DP_2 = 0x3, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_EDP_VDIFF_DP_HIGH_DE_DP_3 = 0x3, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_0_DE_DP_0 = 0x27, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_0_DE_DP_1 = 0x23, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_0_DE_DP_2 = 0x1e, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_0_DE_DP_3 = 0x1a, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_1_DE_DP_0 = 0x23, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_1_DE_DP_1 = 0x1e, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_1_DE_DP_2 = 0x1a, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_2_DE_DP_0 = 0x1e, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_2_DE_DP_1 = 0x1a, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_3_DE_DP_0 = 0x1a, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_0_DE_DP_0 = 0x0, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_0_DE_DP_1 = 0x9, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_0_DE_DP_2 = 0x10, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_0_DE_DP_3 = 0x15, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_1_DE_DP_0 = 0x0, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_1_DE_DP_1 = 0x8, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_1_DE_DP_2 = 0xe, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_2_DE_DP_0 = 0x0, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_2_DE_DP_1 = 0x7, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_3_DE_DP_0 = 0x0, 
	.cMN_DIAG_BIAS_OVRD1 = 0x7400, 
};

// Add following lines manually.

dp_pma_reg_swing_emphasis_config_t dp_1p62_swing_0_emphasis_0 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x002A, 
    .tX_TXCC_CPOST_MULT_00 = 0x0000,
};

dp_pma_reg_swing_emphasis_config_t dp_1p62_swing_0_emphasis_1 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x001F, 
    .tX_TXCC_CPOST_MULT_00 = 0x0014,
};

dp_pma_reg_swing_emphasis_config_t dp_1p62_swing_0_emphasis_2 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x0012, 
    .tX_TXCC_CPOST_MULT_00 = 0x0020,
};

dp_pma_reg_swing_emphasis_config_t dp_1p62_swing_0_emphasis_3 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x0000, 
    .tX_TXCC_CPOST_MULT_00 = 0x002A,
};

dp_pma_reg_swing_emphasis_config_t dp_1p62_swing_1_emphasis_0 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x001F, 
    .tX_TXCC_CPOST_MULT_00 = 0x0000,
};

dp_pma_reg_swing_emphasis_config_t dp_1p62_swing_1_emphasis_1 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x0013, 
    .tX_TXCC_CPOST_MULT_00 = 0x0012,
};

dp_pma_reg_swing_emphasis_config_t dp_1p62_swing_1_emphasis_2 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x0000, 
    .tX_TXCC_CPOST_MULT_00 = 0x001F,
};

dp_pma_reg_swing_emphasis_config_t dp_1p62_swing_2_emphasis_0 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x0013, 
    .tX_TXCC_CPOST_MULT_00 = 0x0000,
};

dp_pma_reg_swing_emphasis_config_t dp_1p62_swing_2_emphasis_1 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x0000, 
    .tX_TXCC_CPOST_MULT_00 = 0x0013,
};

dp_pma_reg_swing_emphasis_config_t dp_1p62_swing_3_emphasis_0 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x0000, 
    .tX_TXCC_CPOST_MULT_00 = 0x0000,
};

dp_phy_swing_emphasis_t dp_1p62_swing_emphasis_config = {
    .swing_0_emphasis_0 = &dp_1p62_swing_0_emphasis_0,
    .swing_0_emphasis_1 = &dp_1p62_swing_0_emphasis_1,
    .swing_0_emphasis_2 = &dp_1p62_swing_0_emphasis_2,
    .swing_0_emphasis_3 = &dp_1p62_swing_0_emphasis_3,
    .swing_1_emphasis_0 = &dp_1p62_swing_1_emphasis_0,
    .swing_1_emphasis_1 = &dp_1p62_swing_1_emphasis_1,
    .swing_1_emphasis_2 = &dp_1p62_swing_1_emphasis_2,
    .swing_2_emphasis_0 = &dp_1p62_swing_2_emphasis_0,
    .swing_2_emphasis_1 = &dp_1p62_swing_2_emphasis_1,
    .swing_3_emphasis_0 = &dp_1p62_swing_3_emphasis_0,
};

dp_phy_config_t dp_1p62_phy_config = {
    .phy_pin_config = &dp_1p62_phy_pin_config,
    .phy_reg_config = &dp_1p62_phy_reg_config,
    .pma_reg_config = &dp_1p62_pma_reg_config,
    .swing_emphasis_config = &dp_1p62_swing_emphasis_config,
};

dp_phy_pin_config_t dp_2p7_phy_pin_config = {
	.pma_cmn_refclk_mode = 0x0, 
	.pma_cmn_refclk_int_mode = 0x0, 
	.pma_cmn_refclk_dig_div = 0x0, 
	.pma_cmn_refclk_dig_sel = 0x1, 
	.pma_cmn_pll0_refclk_sel = 0x1, 
	.pma_cmn_pll1_refclk_sel = 0x1, 
	.phy_lnn_00_mode = 0x3, 
	.phy_link_cfg_ln_n_1 = 0x1, 
	.phy_link_cfg_ln_n_1_SLV_LANE = 0x0, 
	.pma_xcvr_standard_mode_ln_n_0 = 0x0, 
	.pma_xcvr_data_width_ln_n_0 = 0x5, 
	.pma_tx_deemphasis_ln_n_0 = 0x0, 
	.pma_tx_vmargin_ln_n_0 = 0x0, 
	.pma_tx_low_power_swing_en_ln_n_0 = 0x0, 
};

dp_phy_reg_config_t dp_2p7_phy_reg_config = {
	.pHY_PLL_CFG = 0x0, 
	.pHY_PMA_ISO_PLL_CTRL0 = 0xb, 
	.pHY_PMA_ISO_PLL_CTRL1 = 0x2224, 
};

dp_pma_reg_config_t dp_2p7_pma_reg_config = {
	.cMN_SSM_BANDGAP_TMR = 0x1, 
	.cMN_SSM_BIAS_TMR = 0x1b, 
	.cMN_PLLSM0_PLLEN_TMR = 0xb, 
	.cMN_PLLSM0_PLLPRE_TMR = 0x36, 
	.cMN_PLLSM0_PLLLOCK_TMR = 0x29, 
	.cMN_PLLSM1_PLLEN_TMR = 0xb, 
	.cMN_PLLSM1_PLLPRE_TMR = 0x36, 
	.cMN_PLLSM1_PLLLOCK_TMR = 0x10e, 
	.cMN_BGCAL_INIT_TMR = 0x87, 
	.cMN_BGCAL_ITER_TMR = 0x87, 
	.cMN_IBCAL_INIT_TMR = 0x1b, 
	.cMN_IBCAL_ITER_TMR = 0x13, 
	.cMN_TXPUCAL_INIT_TMR = 0x21, 
	.cMN_TXPUCAL_ITER_TMR = 0x7, 
	.cMN_TXPDCAL_INIT_TMR = 0x21, 
	.cMN_TXPDCAL_ITER_TMR = 0x7, 
	.cMN_RXCAL_INIT_TMR = 0x32a, 
	.cMN_RXCAL_ITER_TMR = 0x7, 
	.cMN_SD_CAL_INIT_TMR = 0x7, 
	.cMN_SD_CAL_ITER_TMR = 0x3, 
	.cMN_SD_CAL_REFTIM_START = 0x10, 
	.cMN_SD_CAL_PLLCNT_START = 0x139, 
	.dRV_DIAG_LANE_FCM_EN_TO = 0x21c, 
	.dRV_DIAG_LANE_FCM_EN_MGN_TMR = 0xa2, 
	.rX_SDCAL0_INIT_TMR = 0x1b, 
	.rX_SDCAL0_ITER_TMR = 0x87, 
	.rX_SDCAL1_INIT_TMR = 0x1b, 
	.rX_SDCAL1_ITER_TMR = 0x87, 
	.tX_RCVDET_ST_TMR = 0xa8c, 
	.cMN_PDIAG_PLL0_CLK_SEL_M0 = 0x701, 
	.cMN_PDIAG_PLL0_CLK_SEL_M1 = 0x400, 
	.cMN_PDIAG_PLL1_CLK_SEL_M0 = 0x400, 
	.xCVR_DIAG_HSCLK_SEL = 0x0, 
	.xCVR_DIAG_HSCLK_DIV = 0x1, 
	.xCVR_DIAG_PLLDRC_CTRL = 0x11, 
	.xCVR_DIAG_HSCLK_SEL_TRUE = 0x0, 
	.xCVR_DIAG_HSCLK_DIV_TRUE = 0x1, 
	.xCVR_DIAG_PLLDRC_CTRL_TRUE = 0x12, 
	.cMN_PLL0_DSM_DIAG_M0 = 0x4004, 
	.cMN_PLL1_DSM_DIAG_M0 = 0x4, 
	.cMN_DIAG_GPANA_1 = 0xc00, 
	.cMN_PDIAG_PLL0_ITRIM_M0 = 0x3f, 
	.cMN_PDIAG_PLL1_ITRIM_M0 = 0x7f, 
	.cMN_PDIAG_PLL0_CP_PADJ_M0 = 0x35, 
	.cMN_PDIAG_PLL1_CP_PADJ_M0 = 0xb17, 
	.cMN_PDIAG_PLL0_CP_IADJ_M0 = 0xf07, 
	.cMN_PDIAG_PLL1_CP_IADJ_M0 = 0xe01, 
	.cMN_PDIAG_PLL0_FILT_PADJ_M0 = 0x100, 
	.cMN_PDIAG_PLL1_FILT_PADJ_M0 = 0xd05, 
	.cMN_PLL0_DSM_FBH_OVRD_M0 = 0x82, 
	.cMN_PLL1_DSM_FBH_OVRD_M0 = 0x1c, 
	.cMN_PLL0_DSM_FBL_OVRD_M0 = 0x3e, 
	.cMN_PLL1_DSM_FBL_OVRD_M0 = 0x4, 
	.cMN_PLL0_INTDIV_M0 = 0x0, 
	.cMN_PLL1_INTDIV_M0 = 0x128, 
	.cMN_PLL0_FRACDIVL_M0 = 0x0, 
	.cMN_PLL1_FRACDIVL_M0 = 0x4bda, 
	.cMN_PLL0_FRACDIVH_M0 = 0x0, 
	.cMN_PLL1_FRACDIVH_M0 = 0x2, 
	.cMN_PLL0_HIGH_THR_M0 = 0x0, 
	.cMN_PLL1_HIGH_THR_M0 = 0xc6, 
	.cMN_PDIAG_PLL0_CTRL_M0 = 0x1012, 
	.cMN_PDIAG_PLL1_CTRL_M0 = 0x1002, 
	.cMN_PLL0_SS_CTRL1_M0 = 0x2, 
	.cMN_PLL1_SS_CTRL1_M0 = 0x2, 
	.cMN_PLL0_SS_CTRL2_M0 = 0x0, 
	.cMN_PLL1_SS_CTRL2_M0 = 0x0, 
	.cMN_PLL0_SS_CTRL3_M0 = 0x0, 
	.cMN_PLL1_SS_CTRL3_M0 = 0x0, 
	.cMN_PLL0_SS_CTRL4_M0 = 0x0, 
	.cMN_PLL1_SS_CTRL4_M0 = 0x0, 
	.cMN_PLL0_VCOCAL_INIT_TMR = 0x10e, 
	.cMN_PLL1_VCOCAL_INIT_TMR = 0x10e, 
	.cMN_PLL0_VCOCAL_ITER_TMR = 0x5, 
	.cMN_PLL1_VCOCAL_ITER_TMR = 0x5, 
	.cMN_PLL0_VCOCAL_REFTIM_START = 0x357, 
	.cMN_PLL1_VCOCAL_REFTIM_START = 0x357, 
	.cMN_PLL0_VCOCAL_PLLCNT_START = 0x357, 
	.cMN_PLL1_VCOCAL_PLLCNT_START = 0x357, 
	.cMN_PLL0_VCOCAL_TCTRL = 0x3, 
	.cMN_PLL1_VCOCAL_TCTRL = 0x3, 
	.cMN_PLL0_LOCK_REFCNT_START = 0xd7, 
	.cMN_PLL1_LOCK_REFCNT_START = 0xd7, 
	.cMN_PLL0_LOCK_PLLCNT_START = 0xd7, 
	.cMN_PLL1_LOCK_PLLCNT_START = 0xd7, 
	.cMN_PLL0_LOCK_PLLCNT_THR = 0x3, 
	.cMN_PLL1_LOCK_PLLCNT_THR = 0x3, 
	.tX_PSC_A0 = 0xfb, 
	.tX_PSC_A2 = 0x4aa, 
	.tX_PSC_A3 = 0x4aa, 
	.rX_PSC_A0 = 0x0, 
	.rX_PSC_A2 = 0x0, 
	.rX_PSC_A3 = 0x0, 
	.rX_PSC_CAL = 0x0, 
	.xCVR_DIAG_BIDI_CTRL = 0xf, 
	.rX_REE_GCSM1_CTRL = 0x0, 
	.rX_REE_GCSM2_CTRL = 0x0, 
	.rX_REE_PERGCSM_CTRL = 0x0, 
	.tX_TXCC_CTRL = 0x8a4, 
	.tX_TXCC_CTRL_DP_OR_EDP_DP_DE_DP_1 = 0x8a4, 
	.tX_TXCC_CTRL_DP_OR_EDP_DP_DE_DP_2 = 0x8a4, 
	.tX_TXCC_CTRL_DP_OR_EDP_DP_DE_DP_3 = 0x8a4, 
	.dRV_DIAG_TX_DRV = 0x3, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_DP_DE_DP_1 = 0x3, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_DP_DE_DP_2 = 0x3, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_DP_DE_DP_3 = 0x3, 
	.tX_TXCC_MGNFS_MULT_000 = 0x2a, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_0_DE_DP_1 = 0x1f, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_0_DE_DP_2 = 0x12, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_0_DE_DP_3 = 0x0, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_1_DE_DP_0 = 0x1f, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_1_DE_DP_1 = 0x13, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_1_DE_DP_2 = 0x0, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_2_DE_DP_0 = 0x13, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_2_DE_DP_1 = 0x0, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_3_DE_DP_0 = 0x0, 
	.tX_TXCC_CPOST_MULT_00 = 0x0, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_0_DE_DP_1 = 0x14, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_0_DE_DP_2 = 0x20, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_0_DE_DP_3 = 0x2a, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_1_DE_DP_0 = 0x0, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_1_DE_DP_1 = 0x12, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_1_DE_DP_2 = 0x1f, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_2_DE_DP_0 = 0x0, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_2_DE_DP_1 = 0x13, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_3_DE_DP_0 = 0x0, 
	.tX_TXCC_CTRL_DP_OR_EDP_EDP_VDIFF_DP_LOW_DE_DP_0 = 0x8a4, 
	.tX_TXCC_CTRL_DP_OR_EDP_EDP_VDIFF_DP_LOW_DE_DP_1 = 0x8a4, 
	.tX_TXCC_CTRL_DP_OR_EDP_EDP_VDIFF_DP_LOW_DE_DP_2 = 0x8a4, 
	.tX_TXCC_CTRL_DP_OR_EDP_EDP_VDIFF_DP_LOW_DE_DP_3 = 0x8a4, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_EDP_VDIFF_DP_LOW_DE_DP_0 = 0x3, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_EDP_VDIFF_DP_LOW_DE_DP_1 = 0x3, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_EDP_VDIFF_DP_LOW_DE_DP_2 = 0x3, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_EDP_VDIFF_DP_LOW_DE_DP_3 = 0x3, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_0_DE_DP_0 = 0x2f, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_0_DE_DP_1 = 0x2a, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_0_DE_DP_2 = 0x26, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_0_DE_DP_3 = 0x22, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_1_DE_DP_0 = 0x2b, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_1_DE_DP_1 = 0x27, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_1_DE_DP_2 = 0x23, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_2_DE_DP_0 = 0x27, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_2_DE_DP_1 = 0x23, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_3_DE_DP_0 = 0x23, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_0_DE_DP_0 = 0x0, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_0_DE_DP_1 = 0xd, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_0_DE_DP_2 = 0x15, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_0_DE_DP_3 = 0x1b, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_1_DE_DP_0 = 0x0, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_1_DE_DP_1 = 0xb, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_1_DE_DP_2 = 0x12, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_2_DE_DP_0 = 0x0, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_2_DE_DP_1 = 0x9, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_3_DE_DP_0 = 0x0, 
	.tX_TXCC_CTRL_DP_OR_EDP_EDP_VDIFF_DP_HIGH_DE_DP_0 = 0x8a4, 
	.tX_TXCC_CTRL_DP_OR_EDP_EDP_VDIFF_DP_HIGH_DE_DP_1 = 0x8a4, 
	.tX_TXCC_CTRL_DP_OR_EDP_EDP_VDIFF_DP_HIGH_DE_DP_2 = 0x8a4, 
	.tX_TXCC_CTRL_DP_OR_EDP_EDP_VDIFF_DP_HIGH_DE_DP_3 = 0x8a4, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_EDP_VDIFF_DP_HIGH_DE_DP_0 = 0x3, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_EDP_VDIFF_DP_HIGH_DE_DP_1 = 0x3, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_EDP_VDIFF_DP_HIGH_DE_DP_2 = 0x3, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_EDP_VDIFF_DP_HIGH_DE_DP_3 = 0x3, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_0_DE_DP_0 = 0x27, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_0_DE_DP_1 = 0x23, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_0_DE_DP_2 = 0x1e, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_0_DE_DP_3 = 0x1a, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_1_DE_DP_0 = 0x23, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_1_DE_DP_1 = 0x1e, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_1_DE_DP_2 = 0x1a, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_2_DE_DP_0 = 0x1e, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_2_DE_DP_1 = 0x1a, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_3_DE_DP_0 = 0x1a, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_0_DE_DP_0 = 0x0, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_0_DE_DP_1 = 0x9, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_0_DE_DP_2 = 0x10, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_0_DE_DP_3 = 0x15, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_1_DE_DP_0 = 0x0, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_1_DE_DP_1 = 0x8, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_1_DE_DP_2 = 0xe, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_2_DE_DP_0 = 0x0, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_2_DE_DP_1 = 0x7, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_3_DE_DP_0 = 0x0, 
	.cMN_DIAG_BIAS_OVRD1 = 0x7400, 
};

// Add following lines manually.

dp_pma_reg_swing_emphasis_config_t dp_2p7_swing_0_emphasis_0 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x002A, 
    .tX_TXCC_CPOST_MULT_00 = 0x0000,
};

dp_pma_reg_swing_emphasis_config_t dp_2p7_swing_0_emphasis_1 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x001F, 
    .tX_TXCC_CPOST_MULT_00 = 0x0014,
};

dp_pma_reg_swing_emphasis_config_t dp_2p7_swing_0_emphasis_2 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x0012, 
    .tX_TXCC_CPOST_MULT_00 = 0x0020,
};

dp_pma_reg_swing_emphasis_config_t dp_2p7_swing_0_emphasis_3 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x0000, 
    .tX_TXCC_CPOST_MULT_00 = 0x002A,
};

dp_pma_reg_swing_emphasis_config_t dp_2p7_swing_1_emphasis_0 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x001F, 
    .tX_TXCC_CPOST_MULT_00 = 0x0000,
};

dp_pma_reg_swing_emphasis_config_t dp_2p7_swing_1_emphasis_1 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x0013, 
    .tX_TXCC_CPOST_MULT_00 = 0x0012,
};

dp_pma_reg_swing_emphasis_config_t dp_2p7_swing_1_emphasis_2 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x0000, 
    .tX_TXCC_CPOST_MULT_00 = 0x001F,
};

dp_pma_reg_swing_emphasis_config_t dp_2p7_swing_2_emphasis_0 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x0013, 
    .tX_TXCC_CPOST_MULT_00 = 0x0000,
};

dp_pma_reg_swing_emphasis_config_t dp_2p7_swing_2_emphasis_1 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x0000, 
    .tX_TXCC_CPOST_MULT_00 = 0x0013,
};

dp_pma_reg_swing_emphasis_config_t dp_2p7_swing_3_emphasis_0 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x0000, 
    .tX_TXCC_CPOST_MULT_00 = 0x0000,
};

dp_phy_swing_emphasis_t dp_2p7_swing_emphasis_config = {
    .swing_0_emphasis_0 = &dp_2p7_swing_0_emphasis_0,
    .swing_0_emphasis_1 = &dp_2p7_swing_0_emphasis_1,
    .swing_0_emphasis_2 = &dp_2p7_swing_0_emphasis_2,
    .swing_0_emphasis_3 = &dp_2p7_swing_0_emphasis_3,
    .swing_1_emphasis_0 = &dp_2p7_swing_1_emphasis_0,
    .swing_1_emphasis_1 = &dp_2p7_swing_1_emphasis_1,
    .swing_1_emphasis_2 = &dp_2p7_swing_1_emphasis_2,
    .swing_2_emphasis_0 = &dp_2p7_swing_2_emphasis_0,
    .swing_2_emphasis_1 = &dp_2p7_swing_2_emphasis_1,
    .swing_3_emphasis_0 = &dp_2p7_swing_3_emphasis_0,
};

dp_phy_config_t dp_2p7_phy_config = {
    .phy_pin_config = &dp_2p7_phy_pin_config,
    .phy_reg_config = &dp_2p7_phy_reg_config,
    .pma_reg_config = &dp_2p7_pma_reg_config,
    .swing_emphasis_config = &dp_2p7_swing_emphasis_config,
};

dp_phy_pin_config_t dp_5p4_phy_pin_config = {
	.pma_cmn_refclk_mode = 0x0, 
	.pma_cmn_refclk_int_mode = 0x0, 
	.pma_cmn_refclk_dig_div = 0x0, 
	.pma_cmn_refclk_dig_sel = 0x1, 
	.pma_cmn_pll0_refclk_sel = 0x1, 
	.pma_cmn_pll1_refclk_sel = 0x1, 
	.phy_lnn_00_mode = 0x3, 
	.phy_link_cfg_ln_n_1 = 0x1, 
	.phy_link_cfg_ln_n_1_SLV_LANE = 0x0, 
	.pma_xcvr_standard_mode_ln_n_0 = 0x0, 
	.pma_xcvr_data_width_ln_n_0 = 0x5, 
	.pma_tx_deemphasis_ln_n_0 = 0x0, 
	.pma_tx_vmargin_ln_n_0 = 0x0, 
	.pma_tx_low_power_swing_en_ln_n_0 = 0x0, 
};

dp_phy_reg_config_t dp_5p4_phy_reg_config = {
	.pHY_PLL_CFG = 0x0, 
	.pHY_PMA_ISO_PLL_CTRL0 = 0xb, 
	.pHY_PMA_ISO_PLL_CTRL1 = 0x2224, 
};

dp_pma_reg_config_t dp_5p4_pma_reg_config = {
	.cMN_SSM_BANDGAP_TMR = 0x1, 
	.cMN_SSM_BIAS_TMR = 0x1b, 
	.cMN_PLLSM0_PLLEN_TMR = 0xb, 
	.cMN_PLLSM0_PLLPRE_TMR = 0x36, 
	.cMN_PLLSM0_PLLLOCK_TMR = 0x29, 
	.cMN_PLLSM1_PLLEN_TMR = 0xb, 
	.cMN_PLLSM1_PLLPRE_TMR = 0x36, 
	.cMN_PLLSM1_PLLLOCK_TMR = 0x10e, 
	.cMN_BGCAL_INIT_TMR = 0x87, 
	.cMN_BGCAL_ITER_TMR = 0x87, 
	.cMN_IBCAL_INIT_TMR = 0x1b, 
	.cMN_IBCAL_ITER_TMR = 0x13, 
	.cMN_TXPUCAL_INIT_TMR = 0x21, 
	.cMN_TXPUCAL_ITER_TMR = 0x7, 
	.cMN_TXPDCAL_INIT_TMR = 0x21, 
	.cMN_TXPDCAL_ITER_TMR = 0x7, 
	.cMN_RXCAL_INIT_TMR = 0x32a, 
	.cMN_RXCAL_ITER_TMR = 0x7, 
	.cMN_SD_CAL_INIT_TMR = 0x7, 
	.cMN_SD_CAL_ITER_TMR = 0x3, 
	.cMN_SD_CAL_REFTIM_START = 0x10, 
	.cMN_SD_CAL_PLLCNT_START = 0x139, 
	.dRV_DIAG_LANE_FCM_EN_TO = 0x21c, 
	.dRV_DIAG_LANE_FCM_EN_MGN_TMR = 0xa2, 
	.rX_SDCAL0_INIT_TMR = 0x1b, 
	.rX_SDCAL0_ITER_TMR = 0x87, 
	.rX_SDCAL1_INIT_TMR = 0x1b, 
	.rX_SDCAL1_ITER_TMR = 0x87, 
	.tX_RCVDET_ST_TMR = 0xa8c, 
	.cMN_PDIAG_PLL0_CLK_SEL_M0 = 0x601, 
	.cMN_PDIAG_PLL0_CLK_SEL_M1 = 0x400, 
	.cMN_PDIAG_PLL1_CLK_SEL_M0 = 0x400, 
	.xCVR_DIAG_HSCLK_SEL = 0x0, 
	.xCVR_DIAG_HSCLK_DIV = 0x0, 
	.xCVR_DIAG_PLLDRC_CTRL = 0x11, 
	.xCVR_DIAG_HSCLK_SEL_TRUE = 0x0, 
	.xCVR_DIAG_HSCLK_DIV_TRUE = 0x1, 
	.xCVR_DIAG_PLLDRC_CTRL_TRUE = 0x12, 
	.cMN_PLL0_DSM_DIAG_M0 = 0x4004, 
	.cMN_PLL1_DSM_DIAG_M0 = 0x4, 
	.cMN_DIAG_GPANA_1 = 0xc00, 
	.cMN_PDIAG_PLL0_ITRIM_M0 = 0x3f, 
	.cMN_PDIAG_PLL1_ITRIM_M0 = 0x7f, 
	.cMN_PDIAG_PLL0_CP_PADJ_M0 = 0x35, 
	.cMN_PDIAG_PLL1_CP_PADJ_M0 = 0xb17, 
	.cMN_PDIAG_PLL0_CP_IADJ_M0 = 0xf07, 
	.cMN_PDIAG_PLL1_CP_IADJ_M0 = 0xe01, 
	.cMN_PDIAG_PLL0_FILT_PADJ_M0 = 0x100, 
	.cMN_PDIAG_PLL1_FILT_PADJ_M0 = 0xd05, 
	.cMN_PLL0_DSM_FBH_OVRD_M0 = 0x82, 
	.cMN_PLL1_DSM_FBH_OVRD_M0 = 0x1c, 
	.cMN_PLL0_DSM_FBL_OVRD_M0 = 0x3e, 
	.cMN_PLL1_DSM_FBL_OVRD_M0 = 0x4, 
	.cMN_PLL0_INTDIV_M0 = 0x0, 
	.cMN_PLL1_INTDIV_M0 = 0x128, 
	.cMN_PLL0_FRACDIVL_M0 = 0x0, 
	.cMN_PLL1_FRACDIVL_M0 = 0x4bda, 
	.cMN_PLL0_FRACDIVH_M0 = 0x0, 
	.cMN_PLL1_FRACDIVH_M0 = 0x2, 
	.cMN_PLL0_HIGH_THR_M0 = 0x0, 
	.cMN_PLL1_HIGH_THR_M0 = 0xc6, 
	.cMN_PDIAG_PLL0_CTRL_M0 = 0x1012, 
	.cMN_PDIAG_PLL1_CTRL_M0 = 0x1002, 
	.cMN_PLL0_SS_CTRL1_M0 = 0x2, 
	.cMN_PLL1_SS_CTRL1_M0 = 0x2, 
	.cMN_PLL0_SS_CTRL2_M0 = 0x0, 
	.cMN_PLL1_SS_CTRL2_M0 = 0x0, 
	.cMN_PLL0_SS_CTRL3_M0 = 0x0, 
	.cMN_PLL1_SS_CTRL3_M0 = 0x0, 
	.cMN_PLL0_SS_CTRL4_M0 = 0x0, 
	.cMN_PLL1_SS_CTRL4_M0 = 0x0, 
	.cMN_PLL0_VCOCAL_INIT_TMR = 0x10e, 
	.cMN_PLL1_VCOCAL_INIT_TMR = 0x10e, 
	.cMN_PLL0_VCOCAL_ITER_TMR = 0x5, 
	.cMN_PLL1_VCOCAL_ITER_TMR = 0x5, 
	.cMN_PLL0_VCOCAL_REFTIM_START = 0x357, 
	.cMN_PLL1_VCOCAL_REFTIM_START = 0x357, 
	.cMN_PLL0_VCOCAL_PLLCNT_START = 0x357, 
	.cMN_PLL1_VCOCAL_PLLCNT_START = 0x357, 
	.cMN_PLL0_VCOCAL_TCTRL = 0x3, 
	.cMN_PLL1_VCOCAL_TCTRL = 0x3, 
	.cMN_PLL0_LOCK_REFCNT_START = 0xd7, 
	.cMN_PLL1_LOCK_REFCNT_START = 0xd7, 
	.cMN_PLL0_LOCK_PLLCNT_START = 0xd7, 
	.cMN_PLL1_LOCK_PLLCNT_START = 0xd7, 
	.cMN_PLL0_LOCK_PLLCNT_THR = 0x3, 
	.cMN_PLL1_LOCK_PLLCNT_THR = 0x3, 
	.tX_PSC_A0 = 0xfb, 
	.tX_PSC_A2 = 0x4aa, 
	.tX_PSC_A3 = 0x4aa, 
	.rX_PSC_A0 = 0x0, 
	.rX_PSC_A2 = 0x0, 
	.rX_PSC_A3 = 0x0, 
	.rX_PSC_CAL = 0x0, 
	.xCVR_DIAG_BIDI_CTRL = 0xf, 
	.rX_REE_GCSM1_CTRL = 0x0, 
	.rX_REE_GCSM2_CTRL = 0x0, 
	.rX_REE_PERGCSM_CTRL = 0x0, 
	.tX_TXCC_CTRL = 0x8a4, 
	.tX_TXCC_CTRL_DP_OR_EDP_DP_DE_DP_1 = 0x8a4, 
	.tX_TXCC_CTRL_DP_OR_EDP_DP_DE_DP_2 = 0x8a4, 
	.tX_TXCC_CTRL_DP_OR_EDP_DP_DE_DP_3 = 0x8a4, 
	.dRV_DIAG_TX_DRV = 0x3, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_DP_DE_DP_1 = 0x3, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_DP_DE_DP_2 = 0x3, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_DP_DE_DP_3 = 0x3, 
	.tX_TXCC_MGNFS_MULT_000 = 0x2a, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_0_DE_DP_1 = 0x1f, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_0_DE_DP_2 = 0x12, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_0_DE_DP_3 = 0x0, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_1_DE_DP_0 = 0x1f, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_1_DE_DP_1 = 0x13, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_1_DE_DP_2 = 0x0, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_2_DE_DP_0 = 0x13, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_2_DE_DP_1 = 0x0, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_3_DE_DP_0 = 0x0, 
	.tX_TXCC_CPOST_MULT_00 = 0x0, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_0_DE_DP_1 = 0x14, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_0_DE_DP_2 = 0x20, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_0_DE_DP_3 = 0x2a, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_1_DE_DP_0 = 0x0, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_1_DE_DP_1 = 0x12, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_1_DE_DP_2 = 0x1f, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_2_DE_DP_0 = 0x0, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_2_DE_DP_1 = 0x13, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_3_DE_DP_0 = 0x0, 
	.tX_TXCC_CTRL_DP_OR_EDP_EDP_VDIFF_DP_LOW_DE_DP_0 = 0x8a4, 
	.tX_TXCC_CTRL_DP_OR_EDP_EDP_VDIFF_DP_LOW_DE_DP_1 = 0x8a4, 
	.tX_TXCC_CTRL_DP_OR_EDP_EDP_VDIFF_DP_LOW_DE_DP_2 = 0x8a4, 
	.tX_TXCC_CTRL_DP_OR_EDP_EDP_VDIFF_DP_LOW_DE_DP_3 = 0x8a4, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_EDP_VDIFF_DP_LOW_DE_DP_0 = 0x3, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_EDP_VDIFF_DP_LOW_DE_DP_1 = 0x3, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_EDP_VDIFF_DP_LOW_DE_DP_2 = 0x3, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_EDP_VDIFF_DP_LOW_DE_DP_3 = 0x3, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_0_DE_DP_0 = 0x2f, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_0_DE_DP_1 = 0x2a, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_0_DE_DP_2 = 0x26, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_0_DE_DP_3 = 0x22, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_1_DE_DP_0 = 0x2b, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_1_DE_DP_1 = 0x27, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_1_DE_DP_2 = 0x23, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_2_DE_DP_0 = 0x27, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_2_DE_DP_1 = 0x23, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_3_DE_DP_0 = 0x23, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_0_DE_DP_0 = 0x0, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_0_DE_DP_1 = 0xd, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_0_DE_DP_2 = 0x15, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_0_DE_DP_3 = 0x1b, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_1_DE_DP_0 = 0x0, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_1_DE_DP_1 = 0xb, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_1_DE_DP_2 = 0x12, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_2_DE_DP_0 = 0x0, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_2_DE_DP_1 = 0x9, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_3_DE_DP_0 = 0x0, 
	.tX_TXCC_CTRL_DP_OR_EDP_EDP_VDIFF_DP_HIGH_DE_DP_0 = 0x8a4, 
	.tX_TXCC_CTRL_DP_OR_EDP_EDP_VDIFF_DP_HIGH_DE_DP_1 = 0x8a4, 
	.tX_TXCC_CTRL_DP_OR_EDP_EDP_VDIFF_DP_HIGH_DE_DP_2 = 0x8a4, 
	.tX_TXCC_CTRL_DP_OR_EDP_EDP_VDIFF_DP_HIGH_DE_DP_3 = 0x8a4, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_EDP_VDIFF_DP_HIGH_DE_DP_0 = 0x3, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_EDP_VDIFF_DP_HIGH_DE_DP_1 = 0x3, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_EDP_VDIFF_DP_HIGH_DE_DP_2 = 0x3, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_EDP_VDIFF_DP_HIGH_DE_DP_3 = 0x3, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_0_DE_DP_0 = 0x27, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_0_DE_DP_1 = 0x23, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_0_DE_DP_2 = 0x1e, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_0_DE_DP_3 = 0x1a, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_1_DE_DP_0 = 0x23, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_1_DE_DP_1 = 0x1e, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_1_DE_DP_2 = 0x1a, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_2_DE_DP_0 = 0x1e, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_2_DE_DP_1 = 0x1a, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_3_DE_DP_0 = 0x1a, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_0_DE_DP_0 = 0x0, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_0_DE_DP_1 = 0x9, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_0_DE_DP_2 = 0x10, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_0_DE_DP_3 = 0x15, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_1_DE_DP_0 = 0x0, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_1_DE_DP_1 = 0x8, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_1_DE_DP_2 = 0xe, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_2_DE_DP_0 = 0x0, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_2_DE_DP_1 = 0x7, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_3_DE_DP_0 = 0x0, 
	.cMN_DIAG_BIAS_OVRD1 = 0x7400, 
};

// Add following lines manually.

dp_pma_reg_swing_emphasis_config_t dp_5p4_swing_0_emphasis_0 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x002A, 
    .tX_TXCC_CPOST_MULT_00 = 0x0000,
};

dp_pma_reg_swing_emphasis_config_t dp_5p4_swing_0_emphasis_1 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x001F, 
    .tX_TXCC_CPOST_MULT_00 = 0x0014,
};

dp_pma_reg_swing_emphasis_config_t dp_5p4_swing_0_emphasis_2 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x0012, 
    .tX_TXCC_CPOST_MULT_00 = 0x0020,
};

dp_pma_reg_swing_emphasis_config_t dp_5p4_swing_0_emphasis_3 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x0000, 
    .tX_TXCC_CPOST_MULT_00 = 0x002A,
};

dp_pma_reg_swing_emphasis_config_t dp_5p4_swing_1_emphasis_0 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x001F, 
    .tX_TXCC_CPOST_MULT_00 = 0x0000,
};

dp_pma_reg_swing_emphasis_config_t dp_5p4_swing_1_emphasis_1 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x0013, 
    .tX_TXCC_CPOST_MULT_00 = 0x0012,
};

dp_pma_reg_swing_emphasis_config_t dp_5p4_swing_1_emphasis_2 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x0000, 
    .tX_TXCC_CPOST_MULT_00 = 0x001F,
};

dp_pma_reg_swing_emphasis_config_t dp_5p4_swing_2_emphasis_0 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x0013, 
    .tX_TXCC_CPOST_MULT_00 = 0x0000,
};

dp_pma_reg_swing_emphasis_config_t dp_5p4_swing_2_emphasis_1 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x0000, 
    .tX_TXCC_CPOST_MULT_00 = 0x0013,
};

dp_pma_reg_swing_emphasis_config_t dp_5p4_swing_3_emphasis_0 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x0000, 
    .tX_TXCC_CPOST_MULT_00 = 0x0000,
};

dp_phy_swing_emphasis_t dp_5p4_swing_emphasis_config = {
    .swing_0_emphasis_0 = &dp_5p4_swing_0_emphasis_0,
    .swing_0_emphasis_1 = &dp_5p4_swing_0_emphasis_1,
    .swing_0_emphasis_2 = &dp_5p4_swing_0_emphasis_2,
    .swing_0_emphasis_3 = &dp_5p4_swing_0_emphasis_3,
    .swing_1_emphasis_0 = &dp_5p4_swing_1_emphasis_0,
    .swing_1_emphasis_1 = &dp_5p4_swing_1_emphasis_1,
    .swing_1_emphasis_2 = &dp_5p4_swing_1_emphasis_2,
    .swing_2_emphasis_0 = &dp_5p4_swing_2_emphasis_0,
    .swing_2_emphasis_1 = &dp_5p4_swing_2_emphasis_1,
    .swing_3_emphasis_0 = &dp_5p4_swing_3_emphasis_0,
};

dp_phy_config_t dp_5p4_phy_config = {
    .phy_pin_config = &dp_5p4_phy_pin_config,
    .phy_reg_config = &dp_5p4_phy_reg_config,
    .pma_reg_config = &dp_5p4_pma_reg_config,
    .swing_emphasis_config = &dp_5p4_swing_emphasis_config,
};

dp_phy_pin_config_t dp_8p1_phy_pin_config = {
	.pma_cmn_refclk_mode = 0x0, 
	.pma_cmn_refclk_int_mode = 0x0, 
	.pma_cmn_refclk_dig_div = 0x0, 
	.pma_cmn_refclk_dig_sel = 0x1, 
	.pma_cmn_pll0_refclk_sel = 0x1, 
	.pma_cmn_pll1_refclk_sel = 0x1, 
	.phy_lnn_00_mode = 0x3, 
	.phy_link_cfg_ln_n_1 = 0x1, 
	.phy_link_cfg_ln_n_1_SLV_LANE = 0x0, 
	.pma_xcvr_standard_mode_ln_n_0 = 0x0, 
	.pma_xcvr_data_width_ln_n_0 = 0x5, 
	.pma_tx_deemphasis_ln_n_0 = 0x0, 
	.pma_tx_vmargin_ln_n_0 = 0x0, 
	.pma_tx_low_power_swing_en_ln_n_0 = 0x0, 
};

dp_phy_reg_config_t dp_8p1_phy_reg_config = {
	.pHY_PLL_CFG = 0x0, 
	.pHY_PMA_ISO_PLL_CTRL0 = 0xb, 
	.pHY_PMA_ISO_PLL_CTRL1 = 0x2224, 
};

dp_pma_reg_config_t dp_8p1_pma_reg_config = {
	.cMN_SSM_BANDGAP_TMR = 0x1, 
	.cMN_SSM_BIAS_TMR = 0x1b, 
	.cMN_PLLSM0_PLLEN_TMR = 0xb, 
	.cMN_PLLSM0_PLLPRE_TMR = 0x36, 
	.cMN_PLLSM0_PLLLOCK_TMR = 0x29, 
	.cMN_PLLSM1_PLLEN_TMR = 0xb, 
	.cMN_PLLSM1_PLLPRE_TMR = 0x36, 
	.cMN_PLLSM1_PLLLOCK_TMR = 0x10e, 
	.cMN_BGCAL_INIT_TMR = 0x87, 
	.cMN_BGCAL_ITER_TMR = 0x87, 
	.cMN_IBCAL_INIT_TMR = 0x1b, 
	.cMN_IBCAL_ITER_TMR = 0x13, 
	.cMN_TXPUCAL_INIT_TMR = 0x21, 
	.cMN_TXPUCAL_ITER_TMR = 0x7, 
	.cMN_TXPDCAL_INIT_TMR = 0x21, 
	.cMN_TXPDCAL_ITER_TMR = 0x7, 
	.cMN_RXCAL_INIT_TMR = 0x32a, 
	.cMN_RXCAL_ITER_TMR = 0x7, 
	.cMN_SD_CAL_INIT_TMR = 0x7, 
	.cMN_SD_CAL_ITER_TMR = 0x3, 
	.cMN_SD_CAL_REFTIM_START = 0x10, 
	.cMN_SD_CAL_PLLCNT_START = 0x139, 
	.dRV_DIAG_LANE_FCM_EN_TO = 0x21c, 
	.dRV_DIAG_LANE_FCM_EN_MGN_TMR = 0xa2, 
	.rX_SDCAL0_INIT_TMR = 0x1b, 
	.rX_SDCAL0_ITER_TMR = 0x87, 
	.rX_SDCAL1_INIT_TMR = 0x1b, 
	.rX_SDCAL1_ITER_TMR = 0x87, 
	.tX_RCVDET_ST_TMR = 0xa8c, 
	.cMN_PDIAG_PLL0_CLK_SEL_M0 = 0x200, 
	.cMN_PDIAG_PLL0_CLK_SEL_M1 = 0x400, 
	.cMN_PDIAG_PLL1_CLK_SEL_M0 = 0x400, 
	.xCVR_DIAG_HSCLK_SEL = 0x0, 
	.xCVR_DIAG_HSCLK_DIV = 0x0, 
	.xCVR_DIAG_PLLDRC_CTRL = 0x11, 
	.xCVR_DIAG_HSCLK_SEL_TRUE = 0x0, 
	.xCVR_DIAG_HSCLK_DIV_TRUE = 0x1, 
	.xCVR_DIAG_PLLDRC_CTRL_TRUE = 0x12, 
	.cMN_PLL0_DSM_DIAG_M0 = 0x4004, 
	.cMN_PLL1_DSM_DIAG_M0 = 0x4, 
	.cMN_DIAG_GPANA_1 = 0xc00, 
	.cMN_PDIAG_PLL0_ITRIM_M0 = 0x7f, 
	.cMN_PDIAG_PLL1_ITRIM_M0 = 0x7f, 
	.cMN_PDIAG_PLL0_CP_PADJ_M0 = 0x31, 
	.cMN_PDIAG_PLL1_CP_PADJ_M0 = 0xb17, 
	.cMN_PDIAG_PLL0_CP_IADJ_M0 = 0xf07, 
	.cMN_PDIAG_PLL1_CP_IADJ_M0 = 0xe01, 
	.cMN_PDIAG_PLL0_FILT_PADJ_M0 = 0x100, 
	.cMN_PDIAG_PLL1_FILT_PADJ_M0 = 0xd05, 
	.cMN_PLL0_DSM_FBH_OVRD_M0 = 0x62, 
	.cMN_PLL1_DSM_FBH_OVRD_M0 = 0x1c, 
	.cMN_PLL0_DSM_FBL_OVRD_M0 = 0x2c, 
	.cMN_PLL1_DSM_FBL_OVRD_M0 = 0x4, 
	.cMN_PLL0_INTDIV_M0 = 0x0, 
	.cMN_PLL1_INTDIV_M0 = 0x128, 
	.cMN_PLL0_FRACDIVL_M0 = 0x0, 
	.cMN_PLL1_FRACDIVL_M0 = 0x4bda, 
	.cMN_PLL0_FRACDIVH_M0 = 0x0, 
	.cMN_PLL1_FRACDIVH_M0 = 0x2, 
	.cMN_PLL0_HIGH_THR_M0 = 0x0, 
	.cMN_PLL1_HIGH_THR_M0 = 0xc6, 
	.cMN_PDIAG_PLL0_CTRL_M0 = 0x1012, 
	.cMN_PDIAG_PLL1_CTRL_M0 = 0x1002, 
	.cMN_PLL0_SS_CTRL1_M0 = 0x2, 
	.cMN_PLL1_SS_CTRL1_M0 = 0x2, 
	.cMN_PLL0_SS_CTRL2_M0 = 0x0, 
	.cMN_PLL1_SS_CTRL2_M0 = 0x0, 
	.cMN_PLL0_SS_CTRL3_M0 = 0x0, 
	.cMN_PLL1_SS_CTRL3_M0 = 0x0, 
	.cMN_PLL0_SS_CTRL4_M0 = 0x0, 
	.cMN_PLL1_SS_CTRL4_M0 = 0x0, 
	.cMN_PLL0_VCOCAL_INIT_TMR = 0x10e, 
	.cMN_PLL1_VCOCAL_INIT_TMR = 0x10e, 
	.cMN_PLL0_VCOCAL_ITER_TMR = 0x5, 
	.cMN_PLL1_VCOCAL_ITER_TMR = 0x5, 
	.cMN_PLL0_VCOCAL_REFTIM_START = 0x357, 
	.cMN_PLL1_VCOCAL_REFTIM_START = 0x357, 
	.cMN_PLL0_VCOCAL_PLLCNT_START = 0x357, 
	.cMN_PLL1_VCOCAL_PLLCNT_START = 0x357, 
	.cMN_PLL0_VCOCAL_TCTRL = 0x3, 
	.cMN_PLL1_VCOCAL_TCTRL = 0x3, 
	.cMN_PLL0_LOCK_REFCNT_START = 0xd7, 
	.cMN_PLL1_LOCK_REFCNT_START = 0xd7, 
	.cMN_PLL0_LOCK_PLLCNT_START = 0xd7, 
	.cMN_PLL1_LOCK_PLLCNT_START = 0xd7, 
	.cMN_PLL0_LOCK_PLLCNT_THR = 0x3, 
	.cMN_PLL1_LOCK_PLLCNT_THR = 0x3, 
	.tX_PSC_A0 = 0xfb, 
	.tX_PSC_A2 = 0x4aa, 
	.tX_PSC_A3 = 0x4aa, 
	.rX_PSC_A0 = 0x0, 
	.rX_PSC_A2 = 0x0, 
	.rX_PSC_A3 = 0x0, 
	.rX_PSC_CAL = 0x0, 
	.xCVR_DIAG_BIDI_CTRL = 0xf, 
	.rX_REE_GCSM1_CTRL = 0x0, 
	.rX_REE_GCSM2_CTRL = 0x0, 
	.rX_REE_PERGCSM_CTRL = 0x0, 
	.tX_TXCC_CTRL = 0x8a4, 
	.tX_TXCC_CTRL_DP_OR_EDP_DP_DE_DP_1 = 0x8a4, 
	.tX_TXCC_CTRL_DP_OR_EDP_DP_DE_DP_2 = 0x8a4, 
	.tX_TXCC_CTRL_DP_OR_EDP_DP_DE_DP_3 = 0x8a4, 
	.dRV_DIAG_TX_DRV = 0x3, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_DP_DE_DP_1 = 0x3, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_DP_DE_DP_2 = 0x3, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_DP_DE_DP_3 = 0x3, 
	.tX_TXCC_MGNFS_MULT_000 = 0x2a, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_0_DE_DP_1 = 0x1f, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_0_DE_DP_2 = 0x12, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_0_DE_DP_3 = 0x0, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_1_DE_DP_0 = 0x1f, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_1_DE_DP_1 = 0x13, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_1_DE_DP_2 = 0x0, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_2_DE_DP_0 = 0x13, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_2_DE_DP_1 = 0x0, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_3_DE_DP_0 = 0x0, 
	.tX_TXCC_CPOST_MULT_00 = 0x0, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_0_DE_DP_1 = 0x14, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_0_DE_DP_2 = 0x20, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_0_DE_DP_3 = 0x2a, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_1_DE_DP_0 = 0x0, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_1_DE_DP_1 = 0x12, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_1_DE_DP_2 = 0x1f, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_2_DE_DP_0 = 0x0, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_2_DE_DP_1 = 0x13, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_3_DE_DP_0 = 0x0, 
	.tX_TXCC_CTRL_DP_OR_EDP_EDP_VDIFF_DP_LOW_DE_DP_0 = 0x8a4, 
	.tX_TXCC_CTRL_DP_OR_EDP_EDP_VDIFF_DP_LOW_DE_DP_1 = 0x8a4, 
	.tX_TXCC_CTRL_DP_OR_EDP_EDP_VDIFF_DP_LOW_DE_DP_2 = 0x8a4, 
	.tX_TXCC_CTRL_DP_OR_EDP_EDP_VDIFF_DP_LOW_DE_DP_3 = 0x8a4, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_EDP_VDIFF_DP_LOW_DE_DP_0 = 0x3, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_EDP_VDIFF_DP_LOW_DE_DP_1 = 0x3, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_EDP_VDIFF_DP_LOW_DE_DP_2 = 0x3, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_EDP_VDIFF_DP_LOW_DE_DP_3 = 0x3, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_0_DE_DP_0 = 0x2f, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_0_DE_DP_1 = 0x2a, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_0_DE_DP_2 = 0x26, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_0_DE_DP_3 = 0x22, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_1_DE_DP_0 = 0x2b, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_1_DE_DP_1 = 0x27, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_1_DE_DP_2 = 0x23, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_2_DE_DP_0 = 0x27, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_2_DE_DP_1 = 0x23, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_3_DE_DP_0 = 0x23, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_0_DE_DP_0 = 0x0, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_0_DE_DP_1 = 0xd, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_0_DE_DP_2 = 0x15, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_0_DE_DP_3 = 0x1b, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_1_DE_DP_0 = 0x0, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_1_DE_DP_1 = 0xb, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_1_DE_DP_2 = 0x12, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_2_DE_DP_0 = 0x0, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_2_DE_DP_1 = 0x9, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_3_DE_DP_0 = 0x0, 
	.tX_TXCC_CTRL_DP_OR_EDP_EDP_VDIFF_DP_HIGH_DE_DP_0 = 0x8a4, 
	.tX_TXCC_CTRL_DP_OR_EDP_EDP_VDIFF_DP_HIGH_DE_DP_1 = 0x8a4, 
	.tX_TXCC_CTRL_DP_OR_EDP_EDP_VDIFF_DP_HIGH_DE_DP_2 = 0x8a4, 
	.tX_TXCC_CTRL_DP_OR_EDP_EDP_VDIFF_DP_HIGH_DE_DP_3 = 0x8a4, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_EDP_VDIFF_DP_HIGH_DE_DP_0 = 0x3, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_EDP_VDIFF_DP_HIGH_DE_DP_1 = 0x3, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_EDP_VDIFF_DP_HIGH_DE_DP_2 = 0x3, 
	.dRV_DIAG_TX_DRV_DP_OR_EDP_EDP_VDIFF_DP_HIGH_DE_DP_3 = 0x3, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_0_DE_DP_0 = 0x27, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_0_DE_DP_1 = 0x23, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_0_DE_DP_2 = 0x1e, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_0_DE_DP_3 = 0x1a, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_1_DE_DP_0 = 0x23, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_1_DE_DP_1 = 0x1e, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_1_DE_DP_2 = 0x1a, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_2_DE_DP_0 = 0x1e, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_2_DE_DP_1 = 0x1a, 
	.tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_3_DE_DP_0 = 0x1a, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_0_DE_DP_0 = 0x0, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_0_DE_DP_1 = 0x9, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_0_DE_DP_2 = 0x10, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_0_DE_DP_3 = 0x15, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_1_DE_DP_0 = 0x0, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_1_DE_DP_1 = 0x8, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_1_DE_DP_2 = 0xe, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_2_DE_DP_0 = 0x0, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_2_DE_DP_1 = 0x7, 
	.tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_3_DE_DP_0 = 0x0, 
	.cMN_DIAG_BIAS_OVRD1 = 0x7400, 
};

// Add following lines manually.

dp_pma_reg_swing_emphasis_config_t dp_8p1_swing_0_emphasis_0 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x002A, 
    .tX_TXCC_CPOST_MULT_00 = 0x0000,
};

dp_pma_reg_swing_emphasis_config_t dp_8p1_swing_0_emphasis_1 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x001F, 
    .tX_TXCC_CPOST_MULT_00 = 0x0014,
};

dp_pma_reg_swing_emphasis_config_t dp_8p1_swing_0_emphasis_2 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x0012, 
    .tX_TXCC_CPOST_MULT_00 = 0x0020,
};

dp_pma_reg_swing_emphasis_config_t dp_8p1_swing_0_emphasis_3 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x0000, 
    .tX_TXCC_CPOST_MULT_00 = 0x002A,
};

dp_pma_reg_swing_emphasis_config_t dp_8p1_swing_1_emphasis_0 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x001F, 
    .tX_TXCC_CPOST_MULT_00 = 0x0000,
};

dp_pma_reg_swing_emphasis_config_t dp_8p1_swing_1_emphasis_1 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x0013, 
    .tX_TXCC_CPOST_MULT_00 = 0x0012,
};

dp_pma_reg_swing_emphasis_config_t dp_8p1_swing_1_emphasis_2 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x0000, 
    .tX_TXCC_CPOST_MULT_00 = 0x001F,
};

dp_pma_reg_swing_emphasis_config_t dp_8p1_swing_2_emphasis_0 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x0013, 
    .tX_TXCC_CPOST_MULT_00 = 0x0000,
};

dp_pma_reg_swing_emphasis_config_t dp_8p1_swing_2_emphasis_1 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x0000, 
    .tX_TXCC_CPOST_MULT_00 = 0x0013,
};

dp_pma_reg_swing_emphasis_config_t dp_8p1_swing_3_emphasis_0 = {
    .tX_TXCC_CTRL = 0x08A4, 
    .dRV_DIAG_TX_DRV = 0x0003,
    .tX_TXCC_MGNFS_MULT_000 = 0x0000, 
    .tX_TXCC_CPOST_MULT_00 = 0x0000,
};

dp_phy_swing_emphasis_t dp_8p1_swing_emphasis_config = {
    .swing_0_emphasis_0 = &dp_8p1_swing_0_emphasis_0,
    .swing_0_emphasis_1 = &dp_8p1_swing_0_emphasis_1,
    .swing_0_emphasis_2 = &dp_8p1_swing_0_emphasis_2,
    .swing_0_emphasis_3 = &dp_8p1_swing_0_emphasis_3,
    .swing_1_emphasis_0 = &dp_8p1_swing_1_emphasis_0,
    .swing_1_emphasis_1 = &dp_8p1_swing_1_emphasis_1,
    .swing_1_emphasis_2 = &dp_8p1_swing_1_emphasis_2,
    .swing_2_emphasis_0 = &dp_8p1_swing_2_emphasis_0,
    .swing_2_emphasis_1 = &dp_8p1_swing_2_emphasis_1,
    .swing_3_emphasis_0 = &dp_8p1_swing_3_emphasis_0,
};

dp_phy_config_t dp_8p1_phy_config = {
    .phy_pin_config = &dp_8p1_phy_pin_config,
    .phy_reg_config = &dp_8p1_phy_reg_config,
    .pma_reg_config = &dp_8p1_pma_reg_config,
    .swing_emphasis_config = &dp_8p1_swing_emphasis_config,
};

static disp_void_t dp_write_u32(pdisp_device_t pdd, disp_u8_t dp_index, disp_u32_t reg_start, disp_u32_t val)
{
    disp_write_u32(pdd, (reg_start * 4 + pdd->reg_off.dp[dp_index]), val);
}

static disp_u32_t dp_read_u32(pdisp_device_t pdd, disp_u8_t dp_index, disp_u32_t reg_start)
{
    return disp_read_u32(pdd, reg_start * 4 + pdd->reg_off.dp[dp_index]);
}

static disp_void_t dp_apb_ephy_write(struct dp_phy_t* phy, disp_u32_t addr, disp_u32_t data)
{
    pdisp_device_t pdd = phy->pdisp_deivce;
    Reg_Dpt_Reg7a reg7a = {0};
    disp_u32_t loop_count = 0, apb_req = 0;

    dp_write_field(phy->index, Reg_Dpt_Reg75, Register_Dpt_Reg75_Start, 2,
        Ephy_Apb_Add, addr,
        Ephy_Apb_Wdat, data);

    dp_write_field(phy->index, Reg_Dpt_Reg79, Register_Dpt_Reg79_Start, 2,
        Ephy_Apb_Type, 1,
        Ephy_Apb_Req, 1);

    disp_delay_us(10);
    dp_read_field(phy->index, Reg_Dpt_Reg79, Register_Dpt_Reg79_Start, 1,
        Ephy_Apb_Req, &apb_req);
    while (apb_req && loop_count < 10) {
        disp_delay_us(100);
        dp_read_field(phy->index, Reg_Dpt_Reg79, Register_Dpt_Reg79_Start, 1,
            Ephy_Apb_Req, &apb_req);
        loop_count++;
    }
    if (loop_count == 10)
        disp_print(DDM_WARNING, "apb write wait_req timeout, addr = 0x%X, data = 0x%X\n", addr, data);

    loop_count = 0;
    reg7a.uint = dp_read_u32(pdd, phy->index, Register_Dpt_Reg7a_Start);
    while (!reg7a.reg.Ephy_Apb_Ready_O && loop_count < 10) {
        disp_delay_us(100);
        reg7a.uint = dp_read_u32(pdd, phy->index, Register_Dpt_Reg7a_Start);
        loop_count++;
    }
    if (loop_count == 10)
        disp_print(DDM_WARNING, "apb write wait_ready timeout, addr = 0x%X, data = 0x%X\n", addr, data);
}

disp_u32_t dp_apb_ephy_read(struct dp_phy_t* phy, disp_u32_t addr)
{
    pdisp_device_t pdd = phy->pdisp_deivce;
    Reg_Dpt_Reg7a reg7a = {0};
    disp_u32_t loop_count = 0, apb_req = 0;

    dp_write_field(phy->index, Reg_Dpt_Reg75, Register_Dpt_Reg75_Start, 1,
        Ephy_Apb_Add, addr);

    dp_write_field(phy->index, Reg_Dpt_Reg79, Register_Dpt_Reg79_Start, 2,
        Ephy_Apb_Type, 0,
        Ephy_Apb_Req, 1);

    disp_delay_us(10);
    dp_read_field(phy->index, Reg_Dpt_Reg79, Register_Dpt_Reg79_Start, 1,
        Ephy_Apb_Req, &apb_req);
    while (apb_req && loop_count < 10) {
        disp_delay_us(100);
        dp_read_field(phy->index, Reg_Dpt_Reg79, Register_Dpt_Reg79_Start, 1,
            Ephy_Apb_Req, &apb_req);
        loop_count++;
    }
    if (loop_count == 10)
        disp_print(DDM_WARNING, "apb read wait_req timeout, addr = 0x%X\n", addr);

    reg7a.uint = dp_read_u32(pdd, phy->index, Register_Dpt_Reg7a_Start);
    while (!reg7a.reg.Ephy_Apb_Ready_O && loop_count < 10) {
        disp_delay_us(100);
        reg7a.uint = dp_read_u32(pdd, phy->index, Register_Dpt_Reg7a_Start);
        loop_count++;
    }
    if (loop_count == 10)
        disp_print(DDM_WARNING, "apb read wait_ready timeout, addr = 0x%X\n", addr);

    return reg7a.reg.Ephy_Apb_Rdata_O;
}

static disp_void_t dp_ppa_ephy_write(pdisp_device_t pdd, disp_u8_t dp_index, disp_u32_t reg_start, disp_u16_t val)
{
    disp_write_u16(pdd, pdd->reg_off.ppa[dp_index] + reg_start * 2, val);
}

static disp_u16_t dp_ppa_ephy_read(pdisp_device_t pdd, disp_u8_t dp_index, disp_u32_t reg_start)
{
    return disp_read_u16(pdd, pdd->reg_off.ppa[dp_index] + reg_start * 2);
}

static disp_void_t dp_ephy_write(struct dp_phy_t* phy, disp_u32_t addr, disp_u32_t data)
{
#ifdef __DISP_EPHY_APB__
    dp_apb_ephy_write(phy, addr, data);
#else
    pdisp_device_t pdd = phy->pdisp_deivce;
    dp_ppa_ephy_write(pdd, phy->index, addr, (disp_u16_t)data);
#endif
}

disp_u32_t dp_ephy_read(struct dp_phy_t* phy, disp_u32_t addr)
{
#ifdef __DISP_EPHY_APB__
    return dp_apb_ephy_read(phy, addr);
#else
    pdisp_device_t pdd = phy->pdisp_deivce;
    return (disp_u32_t)dp_ppa_ephy_read(pdd, phy->index, addr);
#endif
}

/* for pz1 only:
 *
 * 13 phy registers, 16bit per one register
 * phy0 phy3 phy12 must set in PZ1, real chip not need
 *
 * phy0 : dpcd ver[15:8], max_link_rate[7:0]
 * phy1 : rcvr_eDP_config_caps[15:8], rcvr_training_aux_rd_interval[7:0]
 * phy2 : aux_read_len_size[15:8], rcvr_adapter_caps[7:0]
 * phy3 : rcvr_final_cr_link_rate[15:8], rcvr_final_eq_link_rate[7:0]
 * phy4 : rcvr_final_cr_adj_lane0[15:12], rcvr_final_cr_adj_lane1[11:8],
 *      rcvr_final_cr_adj_lane2[7:4], rcvr_final_cr_adj_lane3[3:0]
 * phy5 : rcvr_final_eq_adj_lane 0 ~ 4
 * phy6 : store_raw_data[15], rcvr_unpack_mode[14:10], rcvr_max_lane_count[9:5],
 *      rcvr_MSA_timing_par_ignore[4], aux_ctrl_in[2:0]
 * phy7 : DSC_bits_per_pixel[9:0]
 * phy8 : DSC_ver_minor[15:12], DSC_ver_major[11:8], DSC_thru_mode_1[7:4], DSC_thru_mode_0[3:0]
 * phy9 : DSC_rc_buf_size[15:8], DSC_slice_cap_1[7:0]
 * phy10: DSC_rc_buf_block_size[15:14], DSC_link_buf_depth[13:10], DSC_block_pred[9],
 *      DSC_color_depth[8:6], DSC_color_fmt[5:0]
 * phy11: DSC_slice_width[15:8], DSC_slice_cap_2[6:4], DSC_bpp_inc[2:0]
 * phy12: pll_enable(0-capture show pipe, 1-capture show dp)[15], reskew_in_enable[14],
 *      link_rate[5:4], FEC_force_decode_in[1], hot_plug_event[0]
 */
disp_void_t disp_dp_phy_pz1_setting(struct dp_phy_t *phy)
{
    disp_u32_t phy_3 = 0, idx = 0;
    pdisp_device_t pdd = phy->pdisp_deivce;
    disp_u32_t link_rate = pdd->disp_encoder[phy->index].policy.custom.dp.link_rate;

    phy_3 = (link_rate) ? ((link_rate << 8) | link_rate) : ((DP_LINK_BW_1_62 << 8) | DP_LINK_BW_1_62);
    dp_ephy_write(phy, 3, phy_3);

    // pz1: clear all PHY_PMA_PLL_RAW_CTRL, reset VSB show
    for(idx = 0; idx < DISP_ENCODER_NUM; ++idx)
        dp_ephy_write(&pdd->disp_encoder[idx].phy, 0xE003, 0);

    // dsc force for pz1 HW config not work completely issue
    dp_ephy_write(phy, 9, (127 << 8) | 0x2b); // rc_buf 128, slice 1/2/4/8
    // rc_block 64KB, max_linebuf 16, bpc8/10/12, RGB/yuv444
    dp_ephy_write(phy, 10, (0x3 << 14) | (7 << 10) | (0x7 << 6) | 0x3);
    dp_ephy_write(phy, 11, 0x8 << 8); // slice_width 8

    disp_print(DDM_TRACE, "ephy 3 = 0x%x\n", dp_ephy_read(phy, 3));
}

disp_void_t disp_dp_phy_pz1_capture_show_dp(struct dp_phy_t *phy, disp_u8_t enable)
{
    disp_u32_t phy_12 = PZ1_PHY_CAPTURE_HPD_EVENT | PZ1_PHY_CAPTURE_RESKEW;
    phy_12 |= enable ? PZ1_PHY_CAPTURE_SHOW_DP : 0;

    dp_ephy_write(phy, 12, phy_12);
}

disp_void_t disp_dp_phy_reset(struct dp_phy_t* phy, disp_u8_t reset)
{
    disp_u32_t reg_field_value_0 = 0, reg_field_value_1 = 0, reg_field_value_2 = 0, reg_field_value_3 = 0;
    pdisp_device_t pdd = phy->pdisp_deivce;

    if (reset)
    {
        dp_write_field(phy->index, Reg_Dpt_Reg79, Register_Dpt_Reg79_Start, 1,
            Ephy_Reset_N, 0);      
        dp_write_field(phy->index, Reg_Dpt_Reg73, Register_Dpt_Reg73_Start, 4,
            Phy_L00_Reset_N, 0,
            Phy_L01_Reset_N, 0,
            Phy_L02_Reset_N, 0,
            Phy_L03_Reset_N, 0);
    }
    else // clear reset
    {
        switch (phy->lane_count)
        {
            case 1:
                reg_field_value_0 = 1;
                break;
            case 2:
                reg_field_value_0 = 1;
                reg_field_value_1 = 1;
                break;
            case 4:
            default:
                reg_field_value_0 = 1;
                reg_field_value_1 = 1;
                reg_field_value_2 = 1;
                reg_field_value_3 = 1;
                break;
        }

        dp_write_field(phy->index, Reg_Dpt_Reg73, Register_Dpt_Reg73_Start, 4,
            Phy_L00_Reset_N, reg_field_value_0,
            Phy_L01_Reset_N, reg_field_value_1,
            Phy_L02_Reset_N, reg_field_value_2,
            Phy_L03_Reset_N, reg_field_value_3);
        dp_write_field(phy->index, Reg_Dpt_Reg79, Register_Dpt_Reg79_Start, 1,
            Ephy_Reset_N, 1);
    }

    disp_print(DDM_TRACE, "disp_dp_phy_reset. DpIndex = 0x%x, reset = 0x%x.\n", phy->index, reset);
    dp_read_field(phy->index, Reg_Dpt_Reg79, Register_Dpt_Reg79_Start, 1,
        Ephy_Reset_N, &reg_field_value_0);
    disp_print(DDM_TRACE, "Read Reg_Dpt_Reg79.Ephy_Reset_N [%x]\n", reg_field_value_0);
    dp_read_field(phy->index, Reg_Dpt_Reg73, Register_Dpt_Reg73_Start, 4,
        Phy_L00_Reset_N, &reg_field_value_0,
        Phy_L01_Reset_N, &reg_field_value_1,
        Phy_L02_Reset_N, &reg_field_value_2,
        Phy_L03_Reset_N, &reg_field_value_3);
    disp_print(DDM_TRACE, "Read Reg_Dpt_Reg73.phy_Reset_N [%x %x %x %x]\n", 
        reg_field_value_0, reg_field_value_1, reg_field_value_2, reg_field_value_3);
}

static disp_void_t dp_phy_aux_init(struct dp_phy_t* phy)
{
    pdisp_device_t pdd = phy->pdisp_deivce;
    disp_s32_t retry = 10;
    disp_u32_t phy_aux_ready = 0;

    dp_write_field(phy->index, Reg_Dpt_Reg0, Register_Dpt_Reg0_Start, 1,
        Aux_2m_Duration, 26); // 24-25M, 26-27M ref clk

    dp_write_field(phy->index, Reg_Dpt_Reg3, Register_Dpt_Reg3_Start, 1,
        Interval_1us, 26); // for 27M ref clk

    // the config value for DP aux tx pre sync length (the value must be odd).
    dp_write_field(phy->index, Reg_Dpt_Reg4, Register_Dpt_Reg4_Start, 1,
        Pre_Sync_Len, 61); // (default is 61) (DP 1.4: 26~32; eDP 1.4: 18~24).

    dp_write_field(phy->index, Reg_Dpt_Reg5, Register_Dpt_Reg5_Start, 4,
        Sync_Max, 0x3F, // disable check max value.
        Sync_Min, 0x10,
        Aux_Wait_Duration, 0x0A, // DP 1.4 bus park time > 10ns.
        En_Auxlong_Time_Hpd_Irq, 1);

    dp_write_field(phy->index, Reg_Dpt_Reg5, Register_Dpt_Reg5_Start, 1,
        Aux_Ephy_Start, 1);

    dp_read_field(phy->index, Reg_Dpt_Reg29, Register_Dpt_Reg29_Start, 1,
        Aux_Ephy_Ready_O, &phy_aux_ready);
    while (!phy_aux_ready && retry)
    {
        disp_delay_us (1000);
        dp_read_field(phy->index, Reg_Dpt_Reg29, Register_Dpt_Reg29_Start, 1,
            Aux_Ephy_Ready_O, &phy_aux_ready);
        retry--;
    }
    if (!phy_aux_ready)
    {
        disp_print(DDM_WARNING, "Aux_Ephy_Ready_O 0!\n");
    }

    dp_write_field(phy->index, Reg_Dpt_Reg78, Register_Dpt_Reg78_Start, 6,
        Bg_Start_Adj, 1,
        Rx_Deglitch_Filter, 3,
        Rx_Hyst_Lvl, 7, // 3'b011 -> 3'b111 for more stable
        Rx_Offset_Disable, 0,
        Term_Highz_En, 0,
        Term_Seg_En, 0x0D);

    dp_write_field(phy->index, Reg_Dpt_Reg79, Register_Dpt_Reg79_Start, 2,
        Tx_Cur_Ctrl, 0x10, // 5’b01110: 367/169 mVpp (DP default).
        Tx_Slew, 3);
}

disp_void_t disp_dp_phy_init(struct dp_phy_t* phy)
{
    pdisp_device_t pdd = phy->pdisp_deivce;

    // In DP CTS Test, lt_max_bandwith_policy should be 0, since CTS require frist reduce linkrate
    dp_write_field(phy->index, Reg_Dpt_Reg0, Register_Dpt_Reg0_Start, 4,
        Audio_En, 0,
        En_Audio_Timestamp, 0,
        Hpd_Min_Mode, 2, // 2 - 0.5ms (default).
        Lt_Max_Bandwith_Policy, (pdd->flag.info.dp_cts ? 0 : 1));

    // HPD will also be enabled, as same PHY with aux.
    dp_phy_aux_init(phy);

    dp_write_field(phy->index, Reg_Dpt_Reg4, Register_Dpt_Reg4_Start, 1,
        Fec_En, 0); // FEC disable

    dp_write_field(phy->index, Reg_Dpt_Reg0e, Register_Dpt_Reg0e_Start, 1,
        Msa_Inster_Dely, 0);

    dp_write_field(phy->index, Reg_Dpt_Reg23, Register_Dpt_Reg23_Start, 1,
        Link_Video_En, 0); // Video Disable.
}

static disp_void_t dp_phy_apb_reset(struct dp_phy_t* phy, disp_u8_t reset)
{
    pdisp_device_t pdd = phy->pdisp_deivce;
    dp_write_field(phy->index, Reg_Dpt_Reg79, Register_Dpt_Reg79_Start, 1,
        Apb_Preset_N, !reset); // 0-reset, 1-clear
}

static disp_void_t dp_phy_pin_reg_set(struct dp_phy_t* phy)
{
    disp_u32_t dp_index = phy->index;
    pdisp_device_t pdd = phy->pdisp_deivce;

    disp_print(DDM_TRACE, "dp_phy_pin_reg_set. index = 0x%x. ssc = 0x%x.\n", dp_index, phy->ssc_en);

    dp_write_field(dp_index, Reg_Dpt_Reg6c, Register_Dpt_Reg6c_Start, 1,
        Pma_Tx_Elec_Idle_Ln_0, 0);
    dp_write_field(dp_index, Reg_Dpt_Reg6d, Register_Dpt_Reg6d_Start, 1,
        Pma_Tx_Elec_Idle_Ln_1, 0);
    dp_write_field(dp_index, Reg_Dpt_Reg6e, Register_Dpt_Reg6e_Start, 1,
        Pma_Tx_Elec_Idle_Ln_2, 0);
    dp_write_field(dp_index, Reg_Dpt_Reg6f, Register_Dpt_Reg6f_Start, 1,
        Pma_Tx_Elec_Idle_Ln_3, 0);
    
    dp_write_field(dp_index, Reg_Dpt_Reg71, Register_Dpt_Reg71_Start, 2,
        Pma_Fullrt_Div_Ln_0, 0,
        Pma_Fullrt_Div_Ln_1, 0);
    dp_write_field(dp_index, Reg_Dpt_Reg72, Register_Dpt_Reg72_Start, 2,
        Pma_Fullrt_Div_Ln_2, 0,
        Pma_Fullrt_Div_Ln_3, 0);
    
    dp_write_field(dp_index, Reg_Dpt_Reg73, Register_Dpt_Reg73_Start, 4,
        Pma_Cmn_Clock_Stop_Req, 0,
        Tx_Reduced_Swing, 0,
        Pma_Refclk_Rcv_En_Override, 0,
        Pma_Xcvr_Power_State, 0);

    dp_write_field(dp_index, Reg_Dpt_Reg73, Register_Dpt_Reg73_Start, 1,
        Pll_Ssc_En, phy->ssc_en ? 0x1 : 0);

    dp_write_field(dp_index, Reg_Dpt_Reg73, Register_Dpt_Reg73_Start, 1,
        Pma_Xcvr_Pllclk_En, 0);
}

static disp_void_t dp_phy_rate0_reg_set(struct dp_phy_t* phy, dp_phy_config_t* phy_config)
{
    disp_u32_t dp_index = phy->index;
    pdisp_device_t pdd = phy->pdisp_deivce;
    
    dp_write_field(dp_index, Reg_Dpt_Reg39, Register_Dpt_Reg39_Start, 2,
        Rate0_Cmn_Pdiag_Pll0_Clk_Sel_M0, phy_config->pma_reg_config->cMN_PDIAG_PLL0_CLK_SEL_M0,
        Rate0_Cmn_Pllsm0_Plllock_Tmr, phy_config->pma_reg_config->cMN_PLLSM0_PLLLOCK_TMR);

    dp_write_field(dp_index, Reg_Dpt_Reg3a, Register_Dpt_Reg3a_Start, 2,
        Rate0_Cmn_Pdiag_Pll0_Cp_Padj_M0, phy_config->pma_reg_config->cMN_PDIAG_PLL0_CP_PADJ_M0,
        Rate0_Cmn_Pdiag_Pll0_Cp_Iadj_M0, phy_config->pma_reg_config->cMN_PDIAG_PLL0_CP_IADJ_M0);

    dp_write_field(dp_index, Reg_Dpt_Reg3b, Register_Dpt_Reg3b_Start, 2,
        Rate0_Cmn_Pdiag_Pll0_Filt_Padj_M0, phy_config->pma_reg_config->cMN_PDIAG_PLL0_FILT_PADJ_M0,
        Rate0_Cmn_Pdiag_Pll0_Ctrl_M0, phy_config->pma_reg_config->cMN_PDIAG_PLL0_CTRL_M0);

    dp_write_field(dp_index, Reg_Dpt_Reg3c, Register_Dpt_Reg3c_Start, 2,
        Rate0_Cmn_Pll0_Dsm_Diag_M0, phy_config->pma_reg_config->cMN_PLL0_DSM_DIAG_M0,
        Rate0_Cmn_Pdiag_Pll0_Itrim_M0, phy_config->pma_reg_config->cMN_PDIAG_PLL0_ITRIM_M0);

    dp_write_field(dp_index, Reg_Dpt_Reg3d, Register_Dpt_Reg3d_Start, 2,
        Rate0_Cmn_Pll0_Fracdivl_M0, phy_config->pma_reg_config->cMN_PLL0_FRACDIVL_M0,
        Rate0_Cmn_Pll0_Fracdivh_M0, phy_config->pma_reg_config->cMN_PLL0_FRACDIVH_M0);

    dp_write_field(dp_index, Reg_Dpt_Reg3e, Register_Dpt_Reg3e_Start, 2,
        Rate0_Cmn_Pll0_Intdiv_M0, phy_config->pma_reg_config->cMN_PLL0_INTDIV_M0,
        Rate0_Cmn_Pll0_High_Thr_M0, phy_config->pma_reg_config->cMN_PLL0_HIGH_THR_M0);

    dp_write_field(dp_index, Reg_Dpt_Reg3f, Register_Dpt_Reg3f_Start, 2,
        Rate0_Cmn_Pll0_Lock_Pllcnt_Thr, phy_config->pma_reg_config->cMN_PLL0_LOCK_PLLCNT_THR,
        Rate0_Cmn_Pll0_Dsm_Fbl_Ovrd_M0, phy_config->pma_reg_config->cMN_PLL0_DSM_FBL_OVRD_M0);

    dp_write_field(dp_index, Reg_Dpt_Reg40, Register_Dpt_Reg40_Start, 2,
        Rate0_Cmn_Pll0_Vcocal_Init_Tmr, phy_config->pma_reg_config->cMN_PLL0_VCOCAL_INIT_TMR,
        Rate0_Cmn_Pll0_Dsm_Fbh_Ovrd_M0, phy_config->pma_reg_config->cMN_PLL0_DSM_FBH_OVRD_M0);

    dp_write_field(dp_index, Reg_Dpt_Reg41, Register_Dpt_Reg41_Start, 2,
        Rate0_Cmn_Pll0_Ss_Ctrl2_M0, phy_config->pma_reg_config->cMN_PLL0_SS_CTRL2_M0,
        Rate0_Cmn_Pll0_Ss_Ctrl1_M0, phy_config->pma_reg_config->cMN_PLL0_SS_CTRL1_M0);

    dp_write_field(dp_index, Reg_Dpt_Reg42, Register_Dpt_Reg42_Start, 2,
        Rate0_Cmn_Pll0_Ss_Ctrl4_M0, phy_config->pma_reg_config->cMN_PLL0_SS_CTRL4_M0,
        Rate0_Cmn_Pll0_Ss_Ctrl3_M0, phy_config->pma_reg_config->cMN_PLL0_SS_CTRL3_M0);

    dp_write_field(dp_index, Reg_Dpt_Reg43, Register_Dpt_Reg43_Start, 2,
        Rate0_Cmn_Pll0_Vcocal_Pllcnt_Start, phy_config->pma_reg_config->cMN_PLL0_VCOCAL_PLLCNT_START,
        Rate0_Cmn_Pll0_Vcocal_Iter_Tmr, phy_config->pma_reg_config->cMN_PLL0_VCOCAL_ITER_TMR);

    dp_write_field(dp_index, Reg_Dpt_Reg44, Register_Dpt_Reg44_Start, 2,
        Rate0_Cmn_Pll0_Vcocal_Tctrl, phy_config->pma_reg_config->cMN_PLL0_VCOCAL_TCTRL,
        Rate0_Cmn_Pll0_Vcocal_Reftim_Start, phy_config->pma_reg_config->cMN_PLL0_VCOCAL_REFTIM_START);

    dp_write_field(dp_index, Reg_Dpt_Reg69, Register_Dpt_Reg69_Start, 1,
        Rate0_Xcvr_Diag_Hsclk_Div, phy_config->pma_reg_config->xCVR_DIAG_HSCLK_DIV);
}

static disp_void_t dp_phy_rate1_reg_set(struct dp_phy_t* phy, dp_phy_config_t* phy_config)
{
    disp_u32_t dp_index = phy->index;
    pdisp_device_t pdd = phy->pdisp_deivce;
    
    dp_write_field(dp_index, Reg_Dpt_Reg45, Register_Dpt_Reg45_Start, 2,
        Rate1_Cmn_Pdiag_Pll0_Clk_Sel_M0, phy_config->pma_reg_config->cMN_PDIAG_PLL0_CLK_SEL_M0,
        Rate1_Cmn_Pllsm0_Plllock_Tmr, phy_config->pma_reg_config->cMN_PLLSM0_PLLLOCK_TMR);

    dp_write_field(dp_index, Reg_Dpt_Reg46, Register_Dpt_Reg46_Start, 2,
        Rate1_Cmn_Pdiag_Pll0_Cp_Padj_M0, phy_config->pma_reg_config->cMN_PDIAG_PLL0_CP_PADJ_M0,
        Rate1_Cmn_Pdiag_Pll0_Cp_Iadj_M0, phy_config->pma_reg_config->cMN_PDIAG_PLL0_CP_IADJ_M0);

    dp_write_field(dp_index, Reg_Dpt_Reg47, Register_Dpt_Reg47_Start, 2,
        Rate1_Cmn_Pdiag_Pll0_Filt_Padj_M0, phy_config->pma_reg_config->cMN_PDIAG_PLL0_FILT_PADJ_M0,
        Rate1_Cmn_Pdiag_Pll0_Ctrl_M0, phy_config->pma_reg_config->cMN_PDIAG_PLL0_CTRL_M0);

    dp_write_field(dp_index, Reg_Dpt_Reg48, Register_Dpt_Reg48_Start, 2,
        Rate1_Cmn_Pll0_Dsm_Diag_M0, phy_config->pma_reg_config->cMN_PLL0_DSM_DIAG_M0,
        Rate1_Cmn_Pdiag_Pll0_Itrim_M0, phy_config->pma_reg_config->cMN_PDIAG_PLL0_ITRIM_M0);

    dp_write_field(dp_index, Reg_Dpt_Reg49, Register_Dpt_Reg49_Start, 2,
        Rate1_Cmn_Pll0_Fracdivl_M0, phy_config->pma_reg_config->cMN_PLL0_FRACDIVL_M0,
        Rate1_Cmn_Pll0_Fracdivh_M0, phy_config->pma_reg_config->cMN_PLL0_FRACDIVH_M0);

    dp_write_field(dp_index, Reg_Dpt_Reg4a, Register_Dpt_Reg4a_Start, 2,
        Rate1_Cmn_Pll0_Intdiv_M0, phy_config->pma_reg_config->cMN_PLL0_INTDIV_M0,
        Rate1_Cmn_Pll0_High_Thr_M0, phy_config->pma_reg_config->cMN_PLL0_HIGH_THR_M0);

    dp_write_field(dp_index, Reg_Dpt_Reg4b, Register_Dpt_Reg4b_Start, 2,
        Rate1_Cmn_Pll0_Lock_Pllcnt_Thr, phy_config->pma_reg_config->cMN_PLL0_LOCK_PLLCNT_THR,
        Rate1_Cmn_Pll0_Dsm_Fbl_Ovrd_M0, phy_config->pma_reg_config->cMN_PLL0_DSM_FBL_OVRD_M0);

    dp_write_field(dp_index, Reg_Dpt_Reg4c, Register_Dpt_Reg4c_Start, 2,
        Rate1_Cmn_Pll0_Vcocal_Init_Tmr, phy_config->pma_reg_config->cMN_PLL0_VCOCAL_INIT_TMR,
        Rate1_Cmn_Pll0_Dsm_Fbh_Ovrd_M0, phy_config->pma_reg_config->cMN_PLL0_DSM_FBH_OVRD_M0);

    dp_write_field(dp_index, Reg_Dpt_Reg4d, Register_Dpt_Reg4d_Start, 2,
        Rate1_Cmn_Pll0_Ss_Ctrl2_M0, phy_config->pma_reg_config->cMN_PLL0_SS_CTRL2_M0,
        Rate1_Cmn_Pll0_Ss_Ctrl1_M0, phy_config->pma_reg_config->cMN_PLL0_SS_CTRL1_M0);

    dp_write_field(dp_index, Reg_Dpt_Reg4e, Register_Dpt_Reg4e_Start, 2,
        Rate1_Cmn_Pll0_Ss_Ctrl4_M0, phy_config->pma_reg_config->cMN_PLL0_SS_CTRL4_M0,
        Rate1_Cmn_Pll0_Ss_Ctrl3_M0, phy_config->pma_reg_config->cMN_PLL0_SS_CTRL3_M0);

    dp_write_field(dp_index, Reg_Dpt_Reg4f, Register_Dpt_Reg4f_Start, 2,
        Rate1_Cmn_Pll0_Vcocal_Pllcnt_Start, phy_config->pma_reg_config->cMN_PLL0_VCOCAL_PLLCNT_START,
        Rate1_Cmn_Pll0_Vcocal_Iter_Tmr, phy_config->pma_reg_config->cMN_PLL0_VCOCAL_ITER_TMR);

    dp_write_field(dp_index, Reg_Dpt_Reg50, Register_Dpt_Reg50_Start, 2,
        Rate1_Cmn_Pll0_Vcocal_Tctrl, phy_config->pma_reg_config->cMN_PLL0_VCOCAL_TCTRL,
        Rate1_Cmn_Pll0_Vcocal_Reftim_Start, phy_config->pma_reg_config->cMN_PLL0_VCOCAL_REFTIM_START);

    dp_write_field(dp_index, Reg_Dpt_Reg69, Register_Dpt_Reg69_Start, 1,
        Rate1_Xcvr_Diag_Hsclk_Div, phy_config->pma_reg_config->xCVR_DIAG_HSCLK_DIV);
}

static disp_void_t dp_phy_rate2_reg_set(struct dp_phy_t* phy, dp_phy_config_t* phy_config)
{
    disp_u32_t dp_index = phy->index;
    pdisp_device_t pdd = phy->pdisp_deivce;

    dp_write_field(dp_index, Reg_Dpt_Reg51, Register_Dpt_Reg51_Start, 2,
        Rate2_Cmn_Pdiag_Pll0_Clk_Sel_M0, phy_config->pma_reg_config->cMN_PDIAG_PLL0_CLK_SEL_M0,
        Rate2_Cmn_Pllsm0_Plllock_Tmr, phy_config->pma_reg_config->cMN_PLLSM0_PLLLOCK_TMR);

    dp_write_field(dp_index, Reg_Dpt_Reg52, Register_Dpt_Reg52_Start, 2,
        Rate2_Cmn_Pdiag_Pll0_Cp_Padj_M0, phy_config->pma_reg_config->cMN_PDIAG_PLL0_CP_PADJ_M0,
        Rate2_Cmn_Pdiag_Pll0_Cp_Iadj_M0, phy_config->pma_reg_config->cMN_PDIAG_PLL0_CP_IADJ_M0);

    dp_write_field(dp_index, Reg_Dpt_Reg53, Register_Dpt_Reg53_Start, 2,
        Rate2_Cmn_Pdiag_Pll0_Filt_Padj_M0, phy_config->pma_reg_config->cMN_PDIAG_PLL0_FILT_PADJ_M0,
        Rate2_Cmn_Pdiag_Pll0_Ctrl_M0, phy_config->pma_reg_config->cMN_PDIAG_PLL0_CTRL_M0);

    dp_write_field(dp_index, Reg_Dpt_Reg54, Register_Dpt_Reg54_Start, 2,
        Rate2_Cmn_Pll0_Dsm_Diag_M0, phy_config->pma_reg_config->cMN_PLL0_DSM_DIAG_M0,
        Rate2_Cmn_Pdiag_Pll0_Itrim_M0, phy_config->pma_reg_config->cMN_PDIAG_PLL0_ITRIM_M0);

    dp_write_field(dp_index, Reg_Dpt_Reg55, Register_Dpt_Reg55_Start, 2,
        Rate2_Cmn_Pll0_Fracdivl_M0, phy_config->pma_reg_config->cMN_PLL0_FRACDIVL_M0,
        Rate2_Cmn_Pll0_Fracdivh_M0, phy_config->pma_reg_config->cMN_PLL0_FRACDIVH_M0);

    dp_write_field(dp_index, Reg_Dpt_Reg56, Register_Dpt_Reg56_Start, 2,
        Rate2_Cmn_Pll0_Intdiv_M0, phy_config->pma_reg_config->cMN_PLL0_INTDIV_M0,
        Rate2_Cmn_Pll0_High_Thr_M0, phy_config->pma_reg_config->cMN_PLL0_HIGH_THR_M0);

    dp_write_field(dp_index, Reg_Dpt_Reg57, Register_Dpt_Reg57_Start, 2,
        Rate2_Cmn_Pll0_Lock_Pllcnt_Thr, phy_config->pma_reg_config->cMN_PLL0_LOCK_PLLCNT_THR,
        Rate2_Cmn_Pll0_Dsm_Fbl_Ovrd_M0, phy_config->pma_reg_config->cMN_PLL0_DSM_FBL_OVRD_M0);

    dp_write_field(dp_index, Reg_Dpt_Reg58, Register_Dpt_Reg58_Start, 2,
        Rate2_Cmn_Pll0_Vcocal_Init_Tmr, phy_config->pma_reg_config->cMN_PLL0_VCOCAL_INIT_TMR,
        Rate2_Cmn_Pll0_Dsm_Fbh_Ovrd_M0, phy_config->pma_reg_config->cMN_PLL0_DSM_FBH_OVRD_M0);

    dp_write_field(dp_index, Reg_Dpt_Reg59, Register_Dpt_Reg59_Start, 2,
        Rate2_Cmn_Pll0_Ss_Ctrl2_M0, phy_config->pma_reg_config->cMN_PLL0_SS_CTRL2_M0,
        Rate2_Cmn_Pll0_Ss_Ctrl1_M0, phy_config->pma_reg_config->cMN_PLL0_SS_CTRL1_M0);

    dp_write_field(dp_index, Reg_Dpt_Reg5a, Register_Dpt_Reg5a_Start, 2,
        Rate2_Cmn_Pll0_Ss_Ctrl4_M0, phy_config->pma_reg_config->cMN_PLL0_SS_CTRL4_M0,
        Rate2_Cmn_Pll0_Ss_Ctrl3_M0, phy_config->pma_reg_config->cMN_PLL0_SS_CTRL3_M0);

    dp_write_field(dp_index, Reg_Dpt_Reg5b, Register_Dpt_Reg5b_Start, 2,
        Rate2_Cmn_Pll0_Vcocal_Pllcnt_Start, phy_config->pma_reg_config->cMN_PLL0_VCOCAL_PLLCNT_START,
        Rate2_Cmn_Pll0_Vcocal_Iter_Tmr, phy_config->pma_reg_config->cMN_PLL0_VCOCAL_ITER_TMR);

    dp_write_field(dp_index, Reg_Dpt_Reg5c, Register_Dpt_Reg5c_Start, 2,
        Rate2_Cmn_Pll0_Vcocal_Tctrl, phy_config->pma_reg_config->cMN_PLL0_VCOCAL_TCTRL,
        Rate2_Cmn_Pll0_Vcocal_Reftim_Start, phy_config->pma_reg_config->cMN_PLL0_VCOCAL_REFTIM_START);

    dp_write_field(dp_index, Reg_Dpt_Reg6a, Register_Dpt_Reg6a_Start, 1,
        Rate2_Xcvr_Diag_Hsclk_Div, phy_config->pma_reg_config->xCVR_DIAG_HSCLK_DIV);
}

static disp_void_t dp_phy_rate3_reg_set(struct dp_phy_t* phy, dp_phy_config_t* phy_config)
{
    disp_u32_t dp_index = phy->index;
    pdisp_device_t pdd = phy->pdisp_deivce;

    dp_write_field(dp_index, Reg_Dpt_Reg5d, Register_Dpt_Reg5d_Start, 2,
        Rate3_Cmn_Pdiag_Pll0_Clk_Sel_M0, phy_config->pma_reg_config->cMN_PDIAG_PLL0_CLK_SEL_M0,
        Rate3_Cmn_Pllsm0_Plllock_Tmr, phy_config->pma_reg_config->cMN_PLLSM0_PLLLOCK_TMR);

    dp_write_field(dp_index, Reg_Dpt_Reg5e, Register_Dpt_Reg5e_Start, 2,
        Rate3_Cmn_Pdiag_Pll0_Cp_Padj_M0, phy_config->pma_reg_config->cMN_PDIAG_PLL0_CP_PADJ_M0,
        Rate3_Cmn_Pdiag_Pll0_Cp_Iadj_M0, phy_config->pma_reg_config->cMN_PDIAG_PLL0_CP_IADJ_M0);

    dp_write_field(dp_index, Reg_Dpt_Reg5f, Register_Dpt_Reg5f_Start, 2,
        Rate3_Cmn_Pdiag_Pll0_Filt_Padj_M0, phy_config->pma_reg_config->cMN_PDIAG_PLL0_FILT_PADJ_M0,
        Rate3_Cmn_Pdiag_Pll0_Ctrl_M0, phy_config->pma_reg_config->cMN_PDIAG_PLL0_CTRL_M0);

    dp_write_field(dp_index, Reg_Dpt_Reg60, Register_Dpt_Reg60_Start, 2,
        Rate3_Cmn_Pll0_Dsm_Diag_M0, phy_config->pma_reg_config->cMN_PLL0_DSM_DIAG_M0,
        Rate3_Cmn_Pdiag_Pll0_Itrim_M0, phy_config->pma_reg_config->cMN_PDIAG_PLL0_ITRIM_M0);

    dp_write_field(dp_index, Reg_Dpt_Reg61, Register_Dpt_Reg61_Start, 2,
        Rate3_Cmn_Pll0_Fracdivl_M0, phy_config->pma_reg_config->cMN_PLL0_FRACDIVL_M0,
        Rate3_Cmn_Pll0_Fracdivh_M0, phy_config->pma_reg_config->cMN_PLL0_FRACDIVH_M0);

    dp_write_field(dp_index, Reg_Dpt_Reg62, Register_Dpt_Reg62_Start, 2,
        Rate3_Cmn_Pll0_Intdiv_M0, phy_config->pma_reg_config->cMN_PLL0_INTDIV_M0,
        Rate3_Cmn_Pll0_High_Thr_M0, phy_config->pma_reg_config->cMN_PLL0_HIGH_THR_M0);

    dp_write_field(dp_index, Reg_Dpt_Reg63, Register_Dpt_Reg63_Start, 2,
        Rate3_Cmn_Pll0_Lock_Pllcnt_Thr, phy_config->pma_reg_config->cMN_PLL0_LOCK_PLLCNT_THR,
        Rate3_Cmn_Pll0_Dsm_Fbl_Ovrd_M0, phy_config->pma_reg_config->cMN_PLL0_DSM_FBL_OVRD_M0);

    dp_write_field(dp_index, Reg_Dpt_Reg64, Register_Dpt_Reg64_Start, 2,
        Rate3_Cmn_Pll0_Vcocal_Init_Tmr, phy_config->pma_reg_config->cMN_PLL0_VCOCAL_INIT_TMR,
        Rate3_Cmn_Pll0_Dsm_Fbh_Ovrd_M0, phy_config->pma_reg_config->cMN_PLL0_DSM_FBH_OVRD_M0);

    dp_write_field(dp_index, Reg_Dpt_Reg65, Register_Dpt_Reg65_Start, 2,
        Rate3_Cmn_Pll0_Ss_Ctrl2_M0, phy_config->pma_reg_config->cMN_PLL0_SS_CTRL2_M0,
        Rate3_Cmn_Pll0_Ss_Ctrl1_M0, phy_config->pma_reg_config->cMN_PLL0_SS_CTRL1_M0);

    dp_write_field(dp_index, Reg_Dpt_Reg66, Register_Dpt_Reg66_Start, 2,
        Rate3_Cmn_Pll0_Ss_Ctrl4_M0, phy_config->pma_reg_config->cMN_PLL0_SS_CTRL4_M0,
        Rate3_Cmn_Pll0_Ss_Ctrl3_M0, phy_config->pma_reg_config->cMN_PLL0_SS_CTRL3_M0);

    dp_write_field(dp_index, Reg_Dpt_Reg67, Register_Dpt_Reg67_Start, 2,
        Rate3_Cmn_Pll0_Vcocal_Pllcnt_Start, phy_config->pma_reg_config->cMN_PLL0_VCOCAL_PLLCNT_START,
        Rate3_Cmn_Pll0_Vcocal_Iter_Tmr, phy_config->pma_reg_config->cMN_PLL0_VCOCAL_ITER_TMR);

    dp_write_field(dp_index, Reg_Dpt_Reg68, Register_Dpt_Reg68_Start, 2,
        Rate3_Cmn_Pll0_Vcocal_Tctrl, phy_config->pma_reg_config->cMN_PLL0_VCOCAL_TCTRL,
        Rate3_Cmn_Pll0_Vcocal_Reftim_Start, phy_config->pma_reg_config->cMN_PLL0_VCOCAL_REFTIM_START);

    dp_write_field(dp_index, Reg_Dpt_Reg6a, Register_Dpt_Reg6a_Start, 1,
        Rate3_Xcvr_Diag_Hsclk_Div, phy_config->pma_reg_config->xCVR_DIAG_HSCLK_DIV);
}

static disp_void_t dp_phy_ratex_reg_set(struct dp_phy_t* phy) 
{
    disp_print(DDM_TRACE, "dp_phy_ratex_reg_set. index = 0x%x, ssc_en = 0x%x.\n", phy->index, phy->ssc_en);

    dp_phy_rate0_reg_set(phy, &dp_1p62_ss_phy_config);
    dp_phy_rate1_reg_set(phy, &dp_2p7_ss_phy_config);
    dp_phy_rate2_reg_set(phy, &dp_5p4_ss_phy_config);
    dp_phy_rate3_reg_set(phy, &dp_8p1_ss_phy_config);
}

static disp_void_t dp_phy_swing_emphasis_reg_set(struct dp_phy_t* phy,
    dp_phy_swing_emphasis_t* swing_emphasis_config)
{
    disp_u32_t dp_index = phy->index;
    pdisp_device_t pdd = phy->pdisp_deivce;

    disp_u32_t swing0_dec0_txcc;
    disp_u32_t swing0_dec1_txcc;
    disp_u32_t swing0_dec2_txcc;
    disp_u32_t swing0_dec3_txcc;
    disp_u32_t swing1_dec0_txcc;
    disp_u32_t swing1_dec1_txcc;
    disp_u32_t swing1_dec2_txcc;
    disp_u32_t swing2_dec0_txcc;
    disp_u32_t swing2_dec1_txcc;
    disp_u32_t swing3_dec0_txcc;

    dp_write_field(dp_index, Reg_Dpt_Reg77, Register_Dpt_Reg77_Start, 2,
        Tx_Txcc_Ctrl, swing_emphasis_config->swing_0_emphasis_0->tX_TXCC_CTRL,
        Drv_Diag_Tx_Drv, swing_emphasis_config->swing_0_emphasis_0->dRV_DIAG_TX_DRV);

    // {MGNFS[15:0] ,CPOST[15:0]}
    swing0_dec0_txcc = (((swing_emphasis_config->swing_0_emphasis_0->tX_TXCC_MGNFS_MULT_000 << 16) & 0xFFFF0000) | (swing_emphasis_config->swing_0_emphasis_0->tX_TXCC_CPOST_MULT_00 & 0x0000FFFF)); 
    swing0_dec1_txcc = (((swing_emphasis_config->swing_0_emphasis_1->tX_TXCC_MGNFS_MULT_000 << 16) & 0xFFFF0000) | (swing_emphasis_config->swing_0_emphasis_1->tX_TXCC_CPOST_MULT_00 & 0x0000FFFF)); 
    swing0_dec2_txcc = (((swing_emphasis_config->swing_0_emphasis_2->tX_TXCC_MGNFS_MULT_000 << 16) & 0xFFFF0000) | (swing_emphasis_config->swing_0_emphasis_2->tX_TXCC_CPOST_MULT_00 & 0x0000FFFF)); 
    swing0_dec3_txcc = (((swing_emphasis_config->swing_0_emphasis_3->tX_TXCC_MGNFS_MULT_000 << 16) & 0xFFFF0000) | (swing_emphasis_config->swing_0_emphasis_3->tX_TXCC_CPOST_MULT_00 & 0x0000FFFF)); 
    swing1_dec0_txcc = (((swing_emphasis_config->swing_1_emphasis_0->tX_TXCC_MGNFS_MULT_000 << 16) & 0xFFFF0000) | (swing_emphasis_config->swing_1_emphasis_0->tX_TXCC_CPOST_MULT_00 & 0x0000FFFF)); 
    swing1_dec1_txcc = (((swing_emphasis_config->swing_1_emphasis_1->tX_TXCC_MGNFS_MULT_000 << 16) & 0xFFFF0000) | (swing_emphasis_config->swing_1_emphasis_1->tX_TXCC_CPOST_MULT_00 & 0x0000FFFF)); 
    swing1_dec2_txcc = (((swing_emphasis_config->swing_1_emphasis_2->tX_TXCC_MGNFS_MULT_000 << 16) & 0xFFFF0000) | (swing_emphasis_config->swing_1_emphasis_2->tX_TXCC_CPOST_MULT_00 & 0x0000FFFF));
    swing2_dec0_txcc = (((swing_emphasis_config->swing_2_emphasis_0->tX_TXCC_MGNFS_MULT_000 << 16) & 0xFFFF0000) | (swing_emphasis_config->swing_2_emphasis_0->tX_TXCC_CPOST_MULT_00 & 0x0000FFFF)); 
    swing2_dec1_txcc = (((swing_emphasis_config->swing_2_emphasis_1->tX_TXCC_MGNFS_MULT_000 << 16) & 0xFFFF0000) | (swing_emphasis_config->swing_2_emphasis_1->tX_TXCC_CPOST_MULT_00 & 0x0000FFFF));
    swing3_dec0_txcc = (((swing_emphasis_config->swing_3_emphasis_0->tX_TXCC_MGNFS_MULT_000 << 16) & 0xFFFF0000) | (swing_emphasis_config->swing_3_emphasis_0->tX_TXCC_CPOST_MULT_00 & 0x0000FFFF));   
    dp_write_field(dp_index, Reg_Dpt_Reg2f, Register_Dpt_Reg2f_Start, 1,
        Swing0_De0_Txcc, swing0_dec0_txcc);
    dp_write_field(dp_index, Reg_Dpt_Reg30, Register_Dpt_Reg30_Start, 1,
        Swing0_De1_Txcc, swing0_dec1_txcc);
    dp_write_field(dp_index, Reg_Dpt_Reg31, Register_Dpt_Reg31_Start, 1,
        Swing0_De2_Txcc, swing0_dec2_txcc);
    dp_write_field(dp_index, Reg_Dpt_Reg32, Register_Dpt_Reg32_Start, 1,
        Swing0_De3_Txcc, swing0_dec3_txcc);
    dp_write_field(dp_index, Reg_Dpt_Reg33, Register_Dpt_Reg33_Start, 1,
        Swing1_De0_Txcc, swing1_dec0_txcc);
    dp_write_field(dp_index, Reg_Dpt_Reg34, Register_Dpt_Reg34_Start, 1,
        Swing1_De1_Txcc, swing1_dec1_txcc);
    dp_write_field(dp_index, Reg_Dpt_Reg35, Register_Dpt_Reg35_Start, 1,
        Swing1_De2_Txcc, swing1_dec2_txcc);
    dp_write_field(dp_index, Reg_Dpt_Reg36, Register_Dpt_Reg36_Start, 1,
        Swing2_De0_Txcc, swing2_dec0_txcc);
    dp_write_field(dp_index, Reg_Dpt_Reg37, Register_Dpt_Reg37_Start, 1,
        Swing2_De1_Txcc, swing2_dec1_txcc);
    dp_write_field(dp_index, Reg_Dpt_Reg38, Register_Dpt_Reg38_Start, 1,
        Swing3_De0_Txcc, swing3_dec0_txcc);
}

static disp_void_t dp_phy_swing_emphasis_config(struct dp_phy_t* phy)
{
    dp_phy_swing_emphasis_t *swing_emphasis_config;

    disp_print(DDM_TRACE, "dp_phy_swing_emphasis_config. Index:0x%x, LinkRate:0x%x, SscStatus:0x%x.\n",
        phy->index, phy->link_rate, phy->ssc_en);

    switch (phy->link_rate)
    {
        case DP_LINK_BW_8_1:
            swing_emphasis_config = dp_8p1_ss_phy_config.swing_emphasis_config;
            break;
        case DP_LINK_BW_5_4:
            swing_emphasis_config = dp_5p4_ss_phy_config.swing_emphasis_config;
            break;
        case DP_LINK_BW_2_7:
            swing_emphasis_config = dp_2p7_ss_phy_config.swing_emphasis_config;
            break;
        case DP_LINK_BW_1_62:
        default:
            swing_emphasis_config = dp_1p62_ss_phy_config.swing_emphasis_config;
            break;
    }

    dp_phy_swing_emphasis_reg_set(phy, swing_emphasis_config);
}

static disp_void_t torrent_dp_main_link_phy_set(struct dp_phy_t* phy,
    dp_phy_config_t* phy_config, dp_pma_reg_swing_emphasis_config_t* swing_emphasis_config)
{
    disp_u32_t dp_index = phy->index;
    pdisp_device_t pdd = phy->pdisp_deivce;

    dp_phy_pin_config_t * phy_pin_config = phy_config->phy_pin_config;
    dp_phy_reg_config_t * phy_reg_config = phy_config->phy_reg_config;
    dp_pma_reg_config_t * pma_reg_config = phy_config->pma_reg_config;
    disp_u16_t cMN_PLL0_SS_CTRL1_M0 = 0x1;

    disp_print(DDM_TRACE, "torrent_dp_main_link_phy_set. DpIndex = 0x%x. SscStatus = 0x%x.\n", dp_index, phy->ssc_en);

    /* Cadence AE suggest: To avoid lt random failed by wave shake.
     * Both ssc or not use ssc_config, just set cMN_PLL0_SS_CTRL1_M0 0x2 to control disable ssc.
     */
    switch (phy->ssc_en)
    {
        case 1:
            cMN_PLL0_SS_CTRL1_M0 = 0x1;
            break;
        case 0:
        default:
            cMN_PLL0_SS_CTRL1_M0 = 0x2;
            break;
    }

    // PHY_PIN
    dp_write_field(dp_index, Reg_Dpt_Reg73, Register_Dpt_Reg73_Start, 9,
        Pma_Cmn_Refclk_Mode, phy_pin_config->pma_cmn_refclk_mode,
        Pma_Cmn_Refclk_Int_Mode, phy_pin_config->pma_cmn_refclk_int_mode,
        Pma_Cmn_Refclk_Dig_Sel, phy_pin_config->pma_cmn_refclk_dig_sel,
        Pma_Cmn_Refclk_Dig_Div, phy_pin_config->pma_cmn_refclk_dig_div,
        Pma_Cmn_Pll1_Refclk_Sel, phy_pin_config->pma_cmn_pll1_refclk_sel,
        Pma_Cmn_Pll0_Refclk_Sel, phy_pin_config->pma_cmn_pll0_refclk_sel,
        Phy_Link_Cfg_Ln_3, phy_pin_config->phy_link_cfg_ln_n_1_SLV_LANE,
        Phy_Link_Cfg_Ln_2, phy_pin_config->phy_link_cfg_ln_n_1_SLV_LANE,
        Phy_Link_Cfg_Ln_1, phy_pin_config->phy_link_cfg_ln_n_1_SLV_LANE);
    dp_write_field(dp_index, Reg_Dpt_Reg6c, Register_Dpt_Reg6c_Start, 4,
        Pma_Xcvr_Standard_Mode_Ln_0, phy_pin_config->pma_xcvr_standard_mode_ln_n_0,
        Pma_Xcvr_Data_Width_Ln_0, phy_pin_config->pma_xcvr_data_width_ln_n_0,
        Pma_Tx_Vmargin_Ln_0, phy_pin_config->pma_tx_vmargin_ln_n_0,
        Pma_Tx_Deemphasis_Ln_0, phy_pin_config->pma_tx_deemphasis_ln_n_0);
    dp_write_field(dp_index, Reg_Dpt_Reg6d, Register_Dpt_Reg6d_Start, 4,
        Pma_Xcvr_Standard_Mode_Ln_1, phy_pin_config->pma_xcvr_standard_mode_ln_n_0,
        Pma_Xcvr_Data_Width_Ln_1, phy_pin_config->pma_xcvr_data_width_ln_n_0,
        Pma_Tx_Vmargin_Ln_1, phy_pin_config->pma_tx_vmargin_ln_n_0,
        Pma_Tx_Deemphasis_Ln_1, phy_pin_config->pma_tx_deemphasis_ln_n_0);
    dp_write_field(dp_index, Reg_Dpt_Reg6e, Register_Dpt_Reg6e_Start, 4,
        Pma_Xcvr_Standard_Mode_Ln_2, phy_pin_config->pma_xcvr_standard_mode_ln_n_0,
        Pma_Xcvr_Data_Width_Ln_2, phy_pin_config->pma_xcvr_data_width_ln_n_0,
        Pma_Tx_Vmargin_Ln_2, phy_pin_config->pma_tx_vmargin_ln_n_0,
        Pma_Tx_Deemphasis_Ln_2, phy_pin_config->pma_tx_deemphasis_ln_n_0);
    dp_write_field(dp_index, Reg_Dpt_Reg6f, Register_Dpt_Reg6f_Start, 4,
        Pma_Xcvr_Standard_Mode_Ln_3, phy_pin_config->pma_xcvr_standard_mode_ln_n_0,
        Pma_Xcvr_Data_Width_Ln_3, phy_pin_config->pma_xcvr_data_width_ln_n_0,
        Pma_Tx_Vmargin_Ln_3, phy_pin_config->pma_tx_vmargin_ln_n_0,
        Pma_Tx_Deemphasis_Ln_3, phy_pin_config->pma_tx_deemphasis_ln_n_0);
    dp_write_field(dp_index, Reg_Dpt_Reg71, Register_Dpt_Reg70_Start, 2,
        Pma_Tx_Low_Power_Swing_En_Ln_1, phy_pin_config->pma_tx_low_power_swing_en_ln_n_0,
        Pma_Tx_Low_Power_Swing_En_Ln_0, phy_pin_config->pma_tx_low_power_swing_en_ln_n_0);
    dp_write_field(dp_index, Reg_Dpt_Reg72, Register_Dpt_Reg72_Start, 2,
        Pma_Tx_Low_Power_Swing_En_Ln_3, phy_pin_config->pma_tx_low_power_swing_en_ln_n_0,
        Pma_Tx_Low_Power_Swing_En_Ln_2, phy_pin_config->pma_tx_low_power_swing_en_ln_n_0);
    
    // PHY_REG
    dp_ephy_write(phy, PHY_Reg_Addr_PHY_PLL_CFG, phy_reg_config->pHY_PLL_CFG);

    // PMA_REG
    // Note: startup state machine registers.
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_SSM_BANDGAP_TMR,  			pma_reg_config->cMN_SSM_BANDGAP_TMR			);  
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_SSM_BIAS_TMR,  				pma_reg_config->cMN_SSM_BIAS_TMR			);   
    // Note: PLL 0 control state machine registers.
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_PLLSM0_PLLEN_TMR,  			pma_reg_config->cMN_PLLSM0_PLLEN_TMR		); 
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_PLLSM0_PLLPRE_TMR,  		pma_reg_config->cMN_PLLSM0_PLLPRE_TMR		); 
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_PLLSM0_PLLLOCK_TMR,			pma_reg_config->cMN_PLLSM0_PLLLOCK_TMR		); 
    // Note: PLL 1 control state machine registers.
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_PLLSM1_PLLEN_TMR,  			pma_reg_config->cMN_PLLSM1_PLLEN_TMR		); 
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_PLLSM1_PLLPRE_TMR,  		pma_reg_config->cMN_PLLSM1_PLLPRE_TMR		); 
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_PLLSM1_PLLLOCK_TMR, 		pma_reg_config->cMN_PLLSM1_PLLLOCK_TMR		);
    // Note: Bandgap calibration registers. 
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_BGCAL_INIT_TMR,  			pma_reg_config->cMN_BGCAL_INIT_TMR			); 
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_BGCAL_ITER_TMR,  			pma_reg_config->cMN_BGCAL_ITER_TMR			); 
    // Note: External bias current calibration registers.
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_IBCAL_INIT_TMR,  			pma_reg_config->cMN_IBCAL_INIT_TMR			); 
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_IBCAL_ITER_TMR,  			pma_reg_config->cMN_IBCAL_ITER_TMR			); 
    // Note: Resitor calibration registers.
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_TXPUCAL_INIT_TMR,  			pma_reg_config->cMN_TXPUCAL_INIT_TMR		);   
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_TXPUCAL_ITER_TMR,  			pma_reg_config->cMN_TXPUCAL_ITER_TMR		);   
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_TXPDCAL_INIT_TMR,  			pma_reg_config->cMN_TXPDCAL_INIT_TMR		);   
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_TXPDCAL_ITER_TMR,  			pma_reg_config->cMN_TXPDCAL_ITER_TMR		);   
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_RXCAL_INIT_TMR,  			pma_reg_config->cMN_RXCAL_INIT_TMR			); 
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_RXCAL_ITER_TMR,  			pma_reg_config->cMN_RXCAL_ITER_TMR			);  
    // Note: Signal detect clock calibration registers.
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_SD_CAL_INIT_TMR,  			pma_reg_config->cMN_SD_CAL_INIT_TMR			);  
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_SD_CAL_ITER_TMR,  			pma_reg_config->cMN_SD_CAL_ITER_TMR			);  
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_SD_CAL_REFTIM_START,		pma_reg_config->cMN_SD_CAL_REFTIM_START		);
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_SD_CAL_PLLCNT_START,		pma_reg_config->cMN_SD_CAL_PLLCNT_START		);
    // Note: Fast establishment of common mode registers.
    dp_ephy_write(phy, PMA_Reg_Addr_Lane0_DRV_DIAG_LANE_FCM_EN_TO,      pma_reg_config->dRV_DIAG_LANE_FCM_EN_TO		);
    dp_ephy_write(phy, PMA_Reg_Addr_Lane0_DRV_DIAG_LANE_FCM_EN_MGN_TMR,	pma_reg_config->dRV_DIAG_LANE_FCM_EN_MGN_TMR); 
    dp_ephy_write(phy, PMA_Reg_Addr_Lane1_DRV_DIAG_LANE_FCM_EN_TO,      pma_reg_config->dRV_DIAG_LANE_FCM_EN_TO		);
    dp_ephy_write(phy, PMA_Reg_Addr_Lane1_DRV_DIAG_LANE_FCM_EN_MGN_TMR,	pma_reg_config->dRV_DIAG_LANE_FCM_EN_MGN_TMR); 
    dp_ephy_write(phy, PMA_Reg_Addr_Lane2_DRV_DIAG_LANE_FCM_EN_TO,      pma_reg_config->dRV_DIAG_LANE_FCM_EN_TO		);
    dp_ephy_write(phy, PMA_Reg_Addr_Lane2_DRV_DIAG_LANE_FCM_EN_MGN_TMR,	pma_reg_config->dRV_DIAG_LANE_FCM_EN_MGN_TMR); 
    dp_ephy_write(phy, PMA_Reg_Addr_Lane3_DRV_DIAG_LANE_FCM_EN_TO,      pma_reg_config->dRV_DIAG_LANE_FCM_EN_TO		);
    dp_ephy_write(phy, PMA_Reg_Addr_Lane3_DRV_DIAG_LANE_FCM_EN_MGN_TMR,	pma_reg_config->dRV_DIAG_LANE_FCM_EN_MGN_TMR); 
    // Note: Signal detect calibration registers.
    dp_ephy_write(phy, PMA_Reg_Addr_Lane0_RX_SDCAL0_INIT_TMR,  		pma_reg_config->rX_SDCAL0_INIT_TMR			); 
    dp_ephy_write(phy, PMA_Reg_Addr_Lane0_RX_SDCAL0_ITER_TMR,  		pma_reg_config->rX_SDCAL0_ITER_TMR			); 
    dp_ephy_write(phy, PMA_Reg_Addr_Lane0_RX_SDCAL1_INIT_TMR,  		pma_reg_config->rX_SDCAL1_INIT_TMR			); 
    dp_ephy_write(phy, PMA_Reg_Addr_Lane0_RX_SDCAL1_ITER_TMR,  		pma_reg_config->rX_SDCAL1_ITER_TMR			); 
    dp_ephy_write(phy, PMA_Reg_Addr_Lane1_RX_SDCAL0_INIT_TMR,  		pma_reg_config->rX_SDCAL0_INIT_TMR			); 
    dp_ephy_write(phy, PMA_Reg_Addr_Lane1_RX_SDCAL0_ITER_TMR,  		pma_reg_config->rX_SDCAL0_ITER_TMR			); 
    dp_ephy_write(phy, PMA_Reg_Addr_Lane1_RX_SDCAL1_INIT_TMR,  		pma_reg_config->rX_SDCAL1_INIT_TMR			); 
    dp_ephy_write(phy, PMA_Reg_Addr_Lane1_RX_SDCAL1_ITER_TMR,  		pma_reg_config->rX_SDCAL1_ITER_TMR			); 
    dp_ephy_write(phy, PMA_Reg_Addr_Lane2_RX_SDCAL0_INIT_TMR,  		pma_reg_config->rX_SDCAL0_INIT_TMR			); 
    dp_ephy_write(phy, PMA_Reg_Addr_Lane2_RX_SDCAL0_ITER_TMR,  		pma_reg_config->rX_SDCAL0_ITER_TMR			); 
    dp_ephy_write(phy, PMA_Reg_Addr_Lane2_RX_SDCAL1_INIT_TMR,  		pma_reg_config->rX_SDCAL1_INIT_TMR			); 
    dp_ephy_write(phy, PMA_Reg_Addr_Lane2_RX_SDCAL1_ITER_TMR,  		pma_reg_config->rX_SDCAL1_ITER_TMR			); 
    dp_ephy_write(phy, PMA_Reg_Addr_Lane3_RX_SDCAL0_INIT_TMR,  		pma_reg_config->rX_SDCAL0_INIT_TMR			); 
    dp_ephy_write(phy, PMA_Reg_Addr_Lane3_RX_SDCAL0_ITER_TMR,  		pma_reg_config->rX_SDCAL0_ITER_TMR			); 
    dp_ephy_write(phy, PMA_Reg_Addr_Lane3_RX_SDCAL1_INIT_TMR,  		pma_reg_config->rX_SDCAL1_INIT_TMR			); 
    dp_ephy_write(phy, PMA_Reg_Addr_Lane3_RX_SDCAL1_ITER_TMR,  		pma_reg_config->rX_SDCAL1_ITER_TMR			); 
    // Note: Receiver detect calibration registers.
    dp_ephy_write(phy, PMA_Reg_Addr_Lane0_TX_RCVDET_ST_TMR,  		pma_reg_config->tX_RCVDET_ST_TMR			); 
    dp_ephy_write(phy, PMA_Reg_Addr_Lane1_TX_RCVDET_ST_TMR,  		pma_reg_config->tX_RCVDET_ST_TMR			);
    dp_ephy_write(phy, PMA_Reg_Addr_Lane2_TX_RCVDET_ST_TMR,  		pma_reg_config->tX_RCVDET_ST_TMR			);
    dp_ephy_write(phy, PMA_Reg_Addr_Lane3_TX_RCVDET_ST_TMR,  		pma_reg_config->tX_RCVDET_ST_TMR			);
    // Note: Link configuration related registers.
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_PDIAG_PLL0_CLK_SEL_M0,  	pma_reg_config->cMN_PDIAG_PLL0_CLK_SEL_M0	);  
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_PDIAG_PLL0_CLK_SEL_M1,  	pma_reg_config->cMN_PDIAG_PLL0_CLK_SEL_M1	);  
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_PDIAG_PLL1_CLK_SEL_M0,  	pma_reg_config->cMN_PDIAG_PLL1_CLK_SEL_M0	);  
    dp_ephy_write(phy, PMA_Reg_Addr_Lane0_XCVR_DIAG_HSCLK_SEL,  	pma_reg_config->xCVR_DIAG_HSCLK_SEL			); 
    dp_ephy_write(phy, PMA_Reg_Addr_Lane1_XCVR_DIAG_HSCLK_SEL,  	pma_reg_config->xCVR_DIAG_HSCLK_SEL			); 
    dp_ephy_write(phy, PMA_Reg_Addr_Lane2_XCVR_DIAG_HSCLK_SEL,  	pma_reg_config->xCVR_DIAG_HSCLK_SEL			); 
    dp_ephy_write(phy, PMA_Reg_Addr_Lane3_XCVR_DIAG_HSCLK_SEL,  	pma_reg_config->xCVR_DIAG_HSCLK_SEL			); 
    dp_ephy_write(phy, PMA_Reg_Addr_Lane0_XCVR_DIAG_HSCLK_DIV,  	pma_reg_config->xCVR_DIAG_HSCLK_DIV			);
    dp_ephy_write(phy, PMA_Reg_Addr_Lane1_XCVR_DIAG_HSCLK_DIV,  	pma_reg_config->xCVR_DIAG_HSCLK_DIV			); 
    dp_ephy_write(phy, PMA_Reg_Addr_Lane2_XCVR_DIAG_HSCLK_DIV,  	pma_reg_config->xCVR_DIAG_HSCLK_DIV			); 
    dp_ephy_write(phy, PMA_Reg_Addr_Lane3_XCVR_DIAG_HSCLK_DIV,  	pma_reg_config->xCVR_DIAG_HSCLK_DIV			);   
    dp_ephy_write(phy, PMA_Reg_Addr_Lane0_XCVR_DIAG_PLLDRC_CTRL,  	pma_reg_config->xCVR_DIAG_PLLDRC_CTRL		); 
    dp_ephy_write(phy, PMA_Reg_Addr_Lane1_XCVR_DIAG_PLLDRC_CTRL,  	pma_reg_config->xCVR_DIAG_PLLDRC_CTRL		); 
    dp_ephy_write(phy, PMA_Reg_Addr_Lane2_XCVR_DIAG_PLLDRC_CTRL,  	pma_reg_config->xCVR_DIAG_PLLDRC_CTRL		); 
    dp_ephy_write(phy, PMA_Reg_Addr_Lane3_XCVR_DIAG_PLLDRC_CTRL,  	pma_reg_config->xCVR_DIAG_PLLDRC_CTRL		); 
    // Note: PLL related registers.
    // Note: General PLL settings for all rates
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_PLL0_DSM_DIAG_M0,  			pma_reg_config->cMN_PLL0_DSM_DIAG_M0		); 
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_PLL1_DSM_DIAG_M0,  			pma_reg_config->cMN_PLL1_DSM_DIAG_M0		); 
    // Note: Analog PLL settings.
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_DIAG_GPANA_1,  				pma_reg_config->cMN_DIAG_GPANA_1			);  
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_PDIAG_PLL0_ITRIM_M0,  		pma_reg_config->cMN_PDIAG_PLL0_ITRIM_M0		);
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_PDIAG_PLL1_ITRIM_M0,  		pma_reg_config->cMN_PDIAG_PLL1_ITRIM_M0		);
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_PDIAG_PLL0_CP_PADJ_M0,  	pma_reg_config->cMN_PDIAG_PLL0_CP_PADJ_M0	);
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_PDIAG_PLL1_CP_PADJ_M0,  	pma_reg_config->cMN_PDIAG_PLL1_CP_PADJ_M0	);
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_PDIAG_PLL0_CP_IADJ_M0,  	pma_reg_config->cMN_PDIAG_PLL0_CP_IADJ_M0	);
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_PDIAG_PLL1_CP_IADJ_M0,  	pma_reg_config->cMN_PDIAG_PLL1_CP_IADJ_M0	);
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_PDIAG_PLL0_FILT_PADJ_M0, 	pma_reg_config->cMN_PDIAG_PLL0_FILT_PADJ_M0	);
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_PDIAG_PLL1_FILT_PADJ_M0, 	pma_reg_config->cMN_PDIAG_PLL1_FILT_PADJ_M0	);
    // Note: Integer mode feedback divider settings..
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_PLL0_DSM_FBH_OVRD_M0,  		pma_reg_config->cMN_PLL0_DSM_FBH_OVRD_M0	);
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_PLL1_DSM_FBH_OVRD_M0,  		pma_reg_config->cMN_PLL1_DSM_FBH_OVRD_M0	);
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_PLL0_DSM_FBL_OVRD_M0,  		pma_reg_config->cMN_PLL0_DSM_FBL_OVRD_M0	);
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_PLL1_DSM_FBL_OVRD_M0,  		pma_reg_config->cMN_PLL1_DSM_FBL_OVRD_M0	);
    // Note: Fractional mode feedback divider settings.
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_PLL0_INTDIV_M0,  			pma_reg_config->cMN_PLL0_INTDIV_M0			); 
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_PLL1_INTDIV_M0,  			pma_reg_config->cMN_PLL1_INTDIV_M0			); 
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_PLL0_FRACDIVL_M0,  			pma_reg_config->cMN_PLL0_FRACDIVL_M0		);
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_PLL1_FRACDIVL_M0,  			pma_reg_config->cMN_PLL1_FRACDIVL_M0		);
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_PLL0_FRACDIVH_M0,  			pma_reg_config->cMN_PLL0_FRACDIVH_M0		);
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_PLL1_FRACDIVH_M0,  			pma_reg_config->cMN_PLL1_FRACDIVH_M0		);
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_PLL0_HIGH_THR_M0,  			pma_reg_config->cMN_PLL0_HIGH_THR_M0		);
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_PLL1_HIGH_THR_M0,  			pma_reg_config->cMN_PLL1_HIGH_THR_M0		);
    // Note: PLL control register settings.
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_PDIAG_PLL0_CTRL_M0,  		pma_reg_config->cMN_PDIAG_PLL0_CTRL_M0		);
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_PDIAG_PLL1_CTRL_M0,  		pma_reg_config->cMN_PDIAG_PLL1_CTRL_M0		);
    // Note: Spread spectrum settings.
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_PLL0_SS_CTRL1_M0,  			cMN_PLL0_SS_CTRL1_M0		);
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_PLL1_SS_CTRL1_M0,  			pma_reg_config->cMN_PLL1_SS_CTRL1_M0		);
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_PLL0_SS_CTRL2_M0,  			pma_reg_config->cMN_PLL0_SS_CTRL2_M0		);
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_PLL1_SS_CTRL2_M0,  			pma_reg_config->cMN_PLL1_SS_CTRL2_M0		);
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_PLL0_SS_CTRL3_M0,  			pma_reg_config->cMN_PLL0_SS_CTRL3_M0		);
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_PLL1_SS_CTRL3_M0,  			pma_reg_config->cMN_PLL1_SS_CTRL3_M0		);
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_PLL0_SS_CTRL4_M0,  			pma_reg_config->cMN_PLL0_SS_CTRL4_M0		);
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_PLL1_SS_CTRL4_M0,  			pma_reg_config->cMN_PLL1_SS_CTRL4_M0		);
    // Note: VCO calibration settings.
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_PLL0_VCOCAL_INIT_TMR,  		pma_reg_config->cMN_PLL0_VCOCAL_INIT_TMR	);
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_PLL1_VCOCAL_INIT_TMR,  		pma_reg_config->cMN_PLL1_VCOCAL_INIT_TMR	);
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_PLL0_VCOCAL_ITER_TMR,  		pma_reg_config->cMN_PLL0_VCOCAL_ITER_TMR	);
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_PLL1_VCOCAL_ITER_TMR,  		pma_reg_config->cMN_PLL1_VCOCAL_ITER_TMR	);
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_PLL0_VCOCAL_REFTIM_START,  	pma_reg_config->cMN_PLL0_VCOCAL_REFTIM_START);
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_PLL1_VCOCAL_REFTIM_START,  	pma_reg_config->cMN_PLL1_VCOCAL_REFTIM_START);
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_PLL0_VCOCAL_PLLCNT_START,  	pma_reg_config->cMN_PLL0_VCOCAL_PLLCNT_START);
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_PLL1_VCOCAL_PLLCNT_START,  	pma_reg_config->cMN_PLL1_VCOCAL_PLLCNT_START);
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_PLL0_VCOCAL_TCTRL,  		pma_reg_config->cMN_PLL0_VCOCAL_TCTRL		); 
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_PLL1_VCOCAL_TCTRL,  		pma_reg_config->cMN_PLL1_VCOCAL_TCTRL		);
    // Note: PLL lock detect settings.
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_PLL0_LOCK_REFCNT_START,  	pma_reg_config->cMN_PLL0_LOCK_REFCNT_START	);  
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_PLL1_LOCK_REFCNT_START,  	pma_reg_config->cMN_PLL1_LOCK_REFCNT_START	);  
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_PLL0_LOCK_PLLCNT_START,  	pma_reg_config->cMN_PLL0_LOCK_PLLCNT_START	);  
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_PLL1_LOCK_PLLCNT_START,  	pma_reg_config->cMN_PLL1_LOCK_PLLCNT_START	);  
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_PLL0_LOCK_PLLCNT_THR,  		pma_reg_config->cMN_PLL0_LOCK_PLLCNT_THR	);
    dp_ephy_write(phy, PMA_Reg_Addr_CMN_PLL1_LOCK_PLLCNT_THR,  		pma_reg_config->cMN_PLL1_LOCK_PLLCNT_THR	);

    // Note: Registers and pins specific to Display Port / eDP lanes.
    dp_ephy_write(phy, 0x60ea, 0x000F); 
    // Note: Power state controller registers.
    dp_ephy_write(phy, PMA_Reg_Addr_Lane0_TX_PSC_A0,                pma_reg_config->tX_PSC_A0                   );
    dp_ephy_write(phy, PMA_Reg_Addr_Lane0_TX_PSC_A2,                pma_reg_config->tX_PSC_A2                   );
    dp_ephy_write(phy, PMA_Reg_Addr_Lane0_TX_PSC_A3,                pma_reg_config->tX_PSC_A3                   );
    dp_ephy_write(phy, PMA_Reg_Addr_Lane0_RX_PSC_A0,                pma_reg_config->rX_PSC_A0                   );
    dp_ephy_write(phy, PMA_Reg_Addr_Lane0_RX_PSC_A2,                pma_reg_config->rX_PSC_A2                   );
    dp_ephy_write(phy, PMA_Reg_Addr_Lane0_RX_PSC_A3,                pma_reg_config->rX_PSC_A3                   );
    dp_ephy_write(phy, PMA_Reg_Addr_Lane0_RX_PSC_CAL,               pma_reg_config->rX_PSC_CAL                  ); 
    dp_ephy_write(phy, PMA_Reg_Addr_Lane1_TX_PSC_A0,                pma_reg_config->tX_PSC_A0                   );
    dp_ephy_write(phy, PMA_Reg_Addr_Lane1_TX_PSC_A2,                pma_reg_config->tX_PSC_A2                   );
    dp_ephy_write(phy, PMA_Reg_Addr_Lane1_TX_PSC_A3,                pma_reg_config->tX_PSC_A3                   );
    dp_ephy_write(phy, PMA_Reg_Addr_Lane1_RX_PSC_A0,                pma_reg_config->rX_PSC_A0                   );
    dp_ephy_write(phy, PMA_Reg_Addr_Lane1_RX_PSC_A2,                pma_reg_config->rX_PSC_A2                   );
    dp_ephy_write(phy, PMA_Reg_Addr_Lane1_RX_PSC_A3,                pma_reg_config->rX_PSC_A3                   );
    dp_ephy_write(phy, PMA_Reg_Addr_Lane1_RX_PSC_CAL,               pma_reg_config->rX_PSC_CAL                  ); 
    dp_ephy_write(phy, PMA_Reg_Addr_Lane2_TX_PSC_A0,                pma_reg_config->tX_PSC_A0                   );
    dp_ephy_write(phy, PMA_Reg_Addr_Lane2_TX_PSC_A2,                pma_reg_config->tX_PSC_A2                   );
    dp_ephy_write(phy, PMA_Reg_Addr_Lane2_TX_PSC_A3,                pma_reg_config->tX_PSC_A3                   );
    dp_ephy_write(phy, PMA_Reg_Addr_Lane2_RX_PSC_A0,                pma_reg_config->rX_PSC_A0                   );
    dp_ephy_write(phy, PMA_Reg_Addr_Lane2_RX_PSC_A2,                pma_reg_config->rX_PSC_A2                   );
    dp_ephy_write(phy, PMA_Reg_Addr_Lane2_RX_PSC_A3,                pma_reg_config->rX_PSC_A3                   );
    dp_ephy_write(phy, PMA_Reg_Addr_Lane2_RX_PSC_CAL,               pma_reg_config->rX_PSC_CAL                  ); 
    dp_ephy_write(phy, PMA_Reg_Addr_Lane3_TX_PSC_A0,                pma_reg_config->tX_PSC_A0                   );
    dp_ephy_write(phy, PMA_Reg_Addr_Lane3_TX_PSC_A2,                pma_reg_config->tX_PSC_A2                   );
    dp_ephy_write(phy, PMA_Reg_Addr_Lane3_TX_PSC_A3,                pma_reg_config->tX_PSC_A3                   );
    dp_ephy_write(phy, PMA_Reg_Addr_Lane3_RX_PSC_A0,                pma_reg_config->rX_PSC_A0                   );
    dp_ephy_write(phy, PMA_Reg_Addr_Lane3_RX_PSC_A2,                pma_reg_config->rX_PSC_A2                   );
    dp_ephy_write(phy, PMA_Reg_Addr_Lane3_RX_PSC_A3,                pma_reg_config->rX_PSC_A3                   );
    dp_ephy_write(phy, PMA_Reg_Addr_Lane3_RX_PSC_CAL,               pma_reg_config->rX_PSC_CAL                  ); 
    // Note: Transceiver control and diagnostic registers.
    dp_ephy_write(phy, PMA_Reg_Addr_Lane0_XCVR_DIAG_BIDI_CTRL,      pma_reg_config->xCVR_DIAG_BIDI_CTRL         );  
    dp_ephy_write(phy, PMA_Reg_Addr_Lane1_XCVR_DIAG_BIDI_CTRL,      pma_reg_config->xCVR_DIAG_BIDI_CTRL         );  
    dp_ephy_write(phy, PMA_Reg_Addr_Lane2_XCVR_DIAG_BIDI_CTRL,      pma_reg_config->xCVR_DIAG_BIDI_CTRL         );  
    dp_ephy_write(phy, PMA_Reg_Addr_Lane3_XCVR_DIAG_BIDI_CTRL,      pma_reg_config->xCVR_DIAG_BIDI_CTRL         );  
    // Note: Receiver equalizer engine registers.       
    dp_ephy_write(phy, PMA_Reg_Addr_Lane0_RX_REE_GCSM1_CTRL,        pma_reg_config->rX_REE_GCSM1_CTRL           );
    dp_ephy_write(phy, PMA_Reg_Addr_Lane0_RX_REE_GCSM2_CTRL,        pma_reg_config->rX_REE_GCSM2_CTRL           );
    dp_ephy_write(phy, PMA_Reg_Addr_Lane0_RX_REE_PERGCSM_CTRL,      pma_reg_config->rX_REE_PERGCSM_CTRL         );                                          
    dp_ephy_write(phy, PMA_Reg_Addr_Lane1_RX_REE_GCSM1_CTRL,        pma_reg_config->rX_REE_GCSM1_CTRL           );
    dp_ephy_write(phy, PMA_Reg_Addr_Lane1_RX_REE_GCSM2_CTRL,        pma_reg_config->rX_REE_GCSM2_CTRL           );
    dp_ephy_write(phy, PMA_Reg_Addr_Lane1_RX_REE_PERGCSM_CTRL,      pma_reg_config->rX_REE_PERGCSM_CTRL         );                           
    dp_ephy_write(phy, PMA_Reg_Addr_Lane2_RX_REE_GCSM1_CTRL,        pma_reg_config->rX_REE_GCSM1_CTRL           );
    dp_ephy_write(phy, PMA_Reg_Addr_Lane2_RX_REE_GCSM2_CTRL,        pma_reg_config->rX_REE_GCSM2_CTRL           );
    dp_ephy_write(phy, PMA_Reg_Addr_Lane2_RX_REE_PERGCSM_CTRL,      pma_reg_config->rX_REE_PERGCSM_CTRL         );                                
    dp_ephy_write(phy, PMA_Reg_Addr_Lane3_RX_REE_GCSM1_CTRL,        pma_reg_config->rX_REE_GCSM1_CTRL           );
    dp_ephy_write(phy, PMA_Reg_Addr_Lane3_RX_REE_GCSM2_CTRL,        pma_reg_config->rX_REE_GCSM2_CTRL           );
    dp_ephy_write(phy, PMA_Reg_Addr_Lane3_RX_REE_PERGCSM_CTRL,      pma_reg_config->rX_REE_PERGCSM_CTRL         );

    // Note: Special Display port swing level and emphasis level change instructions.
    dp_ephy_write(phy, PMA_Reg_Addr_Lane0_TX_TXCC_CTRL,             swing_emphasis_config->tX_TXCC_CTRL          );
    dp_ephy_write(phy, PMA_Reg_Addr_Lane0_DRV_DIAG_TX_DRV,          swing_emphasis_config->dRV_DIAG_TX_DRV       );
    dp_ephy_write(phy, PMA_Reg_Addr_Lane0_TX_TXCC_MGNFS_MULT_000,   swing_emphasis_config->tX_TXCC_MGNFS_MULT_000); 
    dp_ephy_write(phy, PMA_Reg_Addr_Lane0_TX_TXCC_CPOST_MULT_00,    swing_emphasis_config->tX_TXCC_CPOST_MULT_00 ); 
    dp_ephy_write(phy, PMA_Reg_Addr_Lane1_TX_TXCC_CTRL,             swing_emphasis_config->tX_TXCC_CTRL          );
    dp_ephy_write(phy, PMA_Reg_Addr_Lane1_DRV_DIAG_TX_DRV,          swing_emphasis_config->dRV_DIAG_TX_DRV       );
    dp_ephy_write(phy, PMA_Reg_Addr_Lane1_TX_TXCC_MGNFS_MULT_000,   swing_emphasis_config->tX_TXCC_MGNFS_MULT_000); 
    dp_ephy_write(phy, PMA_Reg_Addr_Lane1_TX_TXCC_CPOST_MULT_00,    swing_emphasis_config->tX_TXCC_CPOST_MULT_00 );
    dp_ephy_write(phy, PMA_Reg_Addr_Lane2_TX_TXCC_CTRL,             swing_emphasis_config->tX_TXCC_CTRL          );
    dp_ephy_write(phy, PMA_Reg_Addr_Lane2_DRV_DIAG_TX_DRV,          swing_emphasis_config->dRV_DIAG_TX_DRV       );
    dp_ephy_write(phy, PMA_Reg_Addr_Lane2_TX_TXCC_MGNFS_MULT_000,   swing_emphasis_config->tX_TXCC_MGNFS_MULT_000); 
    dp_ephy_write(phy, PMA_Reg_Addr_Lane2_TX_TXCC_CPOST_MULT_00,    swing_emphasis_config->tX_TXCC_CPOST_MULT_00 );    
    dp_ephy_write(phy, PMA_Reg_Addr_Lane3_TX_TXCC_CTRL,             swing_emphasis_config->tX_TXCC_CTRL          );
    dp_ephy_write(phy, PMA_Reg_Addr_Lane3_DRV_DIAG_TX_DRV,          swing_emphasis_config->dRV_DIAG_TX_DRV       );
    dp_ephy_write(phy, PMA_Reg_Addr_Lane3_TX_TXCC_MGNFS_MULT_000,   swing_emphasis_config->tX_TXCC_MGNFS_MULT_000); 
    dp_ephy_write(phy, PMA_Reg_Addr_Lane3_TX_TXCC_CPOST_MULT_00,    swing_emphasis_config->tX_TXCC_CPOST_MULT_00 );
}

static disp_void_t torrent_dp_main_link_phy_config(struct dp_phy_t* phy, disp_u32_t Swing, disp_u32_t Emphasis)
{
    dp_phy_config_t * torrent_phy_config = DISP_NULL;
    dp_pma_reg_swing_emphasis_config_t * swing_emphasis_config = DISP_NULL;

    disp_print(DDM_TRACE, "torrent_dp_main_link_phy_config. DpIndex:0x%x, LinkRate:0x%x, SscStatus:0x%x, Swing:0x%x, Emphasis:0x%x.\n",
        phy->index, phy->link_rate, phy->ssc_en, Swing, Emphasis);

    switch (phy->link_rate)
    {
        case DP_LINK_BW_8_1:
            torrent_phy_config = &dp_8p1_ss_phy_config;
            break;
        case DP_LINK_BW_5_4:
            torrent_phy_config = &dp_5p4_ss_phy_config;
            break;
        case DP_LINK_BW_2_7:
            torrent_phy_config = &dp_2p7_ss_phy_config;
            break;
        case DP_LINK_BW_1_62:
        default:
            torrent_phy_config = &dp_1p62_ss_phy_config;
            break;
    }

    if ((Swing == DP_SWING_LEVEL_0) && (Emphasis = DP_EMPHASIS_LEVEL_0))
        swing_emphasis_config = torrent_phy_config->swing_emphasis_config->swing_0_emphasis_0;
    else if ((Swing == DP_SWING_LEVEL_0) && (Emphasis = DP_EMPHASIS_LEVEL_1))
        swing_emphasis_config = torrent_phy_config->swing_emphasis_config->swing_0_emphasis_1;
    else if ((Swing == DP_SWING_LEVEL_0) && (Emphasis = DP_EMPHASIS_LEVEL_2))
        swing_emphasis_config = torrent_phy_config->swing_emphasis_config->swing_0_emphasis_2;
    else if ((Swing == DP_SWING_LEVEL_0) && (Emphasis = DP_EMPHASIS_LEVEL_3))
        swing_emphasis_config = torrent_phy_config->swing_emphasis_config->swing_0_emphasis_3;
    else if ((Swing == DP_SWING_LEVEL_1) && (Emphasis = DP_EMPHASIS_LEVEL_0))
        swing_emphasis_config = torrent_phy_config->swing_emphasis_config->swing_1_emphasis_0;
    else if ((Swing == DP_SWING_LEVEL_1) && (Emphasis = DP_EMPHASIS_LEVEL_1))
        swing_emphasis_config = torrent_phy_config->swing_emphasis_config->swing_1_emphasis_1;
    else if ((Swing == DP_SWING_LEVEL_1) && (Emphasis = DP_EMPHASIS_LEVEL_2))
        swing_emphasis_config = torrent_phy_config->swing_emphasis_config->swing_1_emphasis_2;
    else if ((Swing == DP_SWING_LEVEL_2) && (Emphasis = DP_EMPHASIS_LEVEL_0))
        swing_emphasis_config = torrent_phy_config->swing_emphasis_config->swing_2_emphasis_0;
    else if ((Swing == DP_SWING_LEVEL_2) && (Emphasis = DP_EMPHASIS_LEVEL_1))
        swing_emphasis_config = torrent_phy_config->swing_emphasis_config->swing_2_emphasis_1;
    else if ((Swing == DP_SWING_LEVEL_3) && (Emphasis = DP_EMPHASIS_LEVEL_0))
        swing_emphasis_config = torrent_phy_config->swing_emphasis_config->swing_3_emphasis_0;

    torrent_dp_main_link_phy_set(phy, torrent_phy_config, swing_emphasis_config);
}

static disp_u8_t dp_phy_wait_active(struct dp_phy_t* phy)
{
    pdisp_device_t pdd = phy->pdisp_deivce;
    disp_u32_t ephy_active_o = 0;
    disp_u32_t retry = 100;

    disp_print(DDM_TRACE, "dp_phy_wait_active. Index = 0x%x.\n", phy->index);

    ephy_active_o = 1;
    dp_read_field(phy->index, Reg_Dpt_Reg7a, Register_Dpt_Reg7a_Start, 1,
        Ephy_Active_O, &ephy_active_o);
    while (!ephy_active_o && retry) 
    {
        disp_delay_us (100);
        dp_read_field(phy->index, Reg_Dpt_Reg7a, Register_Dpt_Reg7a_Start, 1,
            Ephy_Active_O, &ephy_active_o);
        retry--;
    }
    if (!ephy_active_o)
    {
        disp_print(DDM_WARNING, "DP EPHY wait active timeout!\n");
        return DISP_FALSE;
    }

    return DISP_TRUE;
}

disp_void_t disp_dp_phy_link_training (struct dp_phy_t* phy)
{
    pdisp_device_t pdd = phy->pdisp_deivce;
    disp_u32_t reg_field_value = 0;

    dp_write_field(phy->index, Reg_Dpt_Reg78, Register_Dpt_Reg78_Start, 1,
        Sw_Ctrl_Ephy, 0); // hw control signal.
    dp_write_field(phy->index, Reg_Dpt_Reg0, Register_Dpt_Reg0_Start, 1,
        Phy_Cts_Mode, 0);
    dp_write_field(phy->index, Reg_Dpt_Reg0f, Register_Dpt_Reg0f_Start, 1,
        Link_Quality_Pattern_En, 0);

    // ephy reset.
    disp_dp_phy_reset(phy, DISP_TRUE);
    dp_phy_apb_reset(phy, DISP_TRUE);

    dp_write_field(phy->index, Reg_Dpt_Reg79, Register_Dpt_Reg79_Start, 1,
        Ephy_Auto_Set, 1); // If "ephy_auto_set = 0", the function must be called and the register values must be checked carefully.
    dp_read_field(phy->index, Reg_Dpt_Reg79, Register_Dpt_Reg79_Start, 1,
        Ephy_Auto_Set, &reg_field_value);
    disp_print(DDM_TRACE, "Set ephy_auto_set = 1. Read Reg_Dpt_Reg79.Ephy_Auto_Set = 0x%x.\n", reg_field_value);

    // Program pin-related registers which are not set by "ephy_auto_set = 1".
    dp_phy_pin_reg_set(phy);

    // Set "rate*_" DPE Registers.
    dp_phy_ratex_reg_set(phy);

    // Set Swing/Emphasis DPE Registers.
    dp_phy_swing_emphasis_config(phy);

    // clear APB Reset.
    dp_phy_apb_reset(phy, DISP_FALSE);

    // Set an initial value for Torrent PHY. Mainly through APB bus to program PMA_REG.
    // Marked for EPHY Debug. Janice Yi. 20250525.
    torrent_dp_main_link_phy_config(phy, DP_SWING_LEVEL_0, DP_EMPHASIS_LEVEL_0);
    
    // clear ephy reset.
    disp_dp_phy_reset(phy, DISP_FALSE);

    // Wait ephy active.
    dp_phy_wait_active(phy);
}
