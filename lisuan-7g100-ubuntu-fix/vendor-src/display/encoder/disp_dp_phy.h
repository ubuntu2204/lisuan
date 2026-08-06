/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#ifndef __DISP_DP_PHY_H__
#define __DISP_DP_PHY_H__

#include "display.h"

#define DP_SWING_LEVEL_0        0x0
#define DP_SWING_LEVEL_1        0x1
#define DP_SWING_LEVEL_2        0x2
#define DP_SWING_LEVEL_3        0x3
#define DP_EMPHASIS_LEVEL_0     0x0
#define DP_EMPHASIS_LEVEL_1     0x1
#define DP_EMPHASIS_LEVEL_2     0x2
#define DP_EMPHASIS_LEVEL_3     0x3

//=== Cadence Torrent Register APB Address ===================================================================================================================
// Note: Address bits 15:9 = 01_m_nnnn. General lane addressing : m = multi-write, nnnn = lane number. Transceiver And Transmitter Components Address Space.
//       bits 15:9 = 01_0_0000: Transmitter lane 0.
//       bits 15:9 = 01_0_0001: Transmitter lane 1.
//       bits 15:9 = 01_0_0010: Transmitter lane 2.
//       bits 15:9 = 01_0_0011: Transmitter lane 3.
//       bits 15:9 = 01_1_0000: Transceiver and transmitter write only to the same register in all the lanes of a link where lane 0 is the master lane.
//       bits 15:9 = 01_1_0001: Transceiver and transmitter write only to the same register in all the lanes of a link where lane 1 is the master lane.
//       bits 15:9 = 01_1_0010: Transceiver and transmitter write only to the same register in all the lanes of a link where lane 2 is the master lane.
//       bits 15:9 = 01_1_0011: Transceiver and transmitter write only to the same register in all the lanes of a link where lane 3 is the master lane.
// Note: Address bits 15:9 = 10_m_nnnn. General lane addressing : m = multi-write, nnnn = lane number. Receive Components Address Space.
//       bits 15:9 = 10_0_0000: Receiver lane 0.
//       bits 15:9 = 10_0_0001: Receiver lane 1.
//       bits 15:9 = 10_0_0010: Receiver lane 2.
//       bits 15:9 = 10_0_0011: Receiver lane 3.
//       bits 15:9 = 10_1_0000: Receiver write only to the same register in all the lanes of a link where lane 0 is the master lane.
//       bits 15:9 = 10_1_0001: Receiver write only to the same register in all the lanes of a link where lane 1 is the master lane.
//       bits 15:9 = 10_1_0010: Receiver write only to the same register in all the lanes of a link where lane 2 is the master lane.
//       bits 15:9 = 10_1_0011: Receiver write only to the same register in all the lanes of a link where lane 3 is the master lane.

// Note: Reference clock rate related registers.
// PHY and PCS Configuration and Control	
// Startup State Machine Registers	
#define PMA_Reg_Addr_CMN_SSM_BANDGAP_TMR                0x0021    // 00_0_0000 : 0001 : 00001	
#define PMA_Reg_Addr_CMN_SSM_BIAS_TMR                   0x0022    // 00_0_0000 : 0001 : 00010	
// PLL 0 Control State Machine Registers	            
#define PMA_Reg_Addr_CMN_PLLSM0_PLLEN_TMR               0x0029    // 00_0_0000 : 0001 : 01001	
#define PMA_Reg_Addr_CMN_PLLSM0_PLLPRE_TMR              0x002A    // 00_0_0000 : 0001 : 01010	
#define PMA_Reg_Addr_CMN_PLLSM0_PLLLOCK_TMR             0x002C    // 00_0_0000 : 0001 : 01100	
// PLL 1 Control State Machine Registers	            
#define PMA_Reg_Addr_CMN_PLLSM1_PLLEN_TMR               0x0031    // 00_0_0000 : 0001 : 10001
#define PMA_Reg_Addr_CMN_PLLSM1_PLLPRE_TMR              0x0032    // 00_0_0000 : 0001 : 10010 
#define PMA_Reg_Addr_CMN_PLLSM1_PLLLOCK_TMR             0x0034    // 00_0_0000 : 0001 : 10100
// Bandgap calibration registers        
#define PMA_Reg_Addr_CMN_BGCAL_INIT_TMR                 0x0064    // 00_0_0000 : 0011 : 00100
#define PMA_Reg_Addr_CMN_BGCAL_ITER_TMR                 0x0065    // 00_0_0000 : 0011 : 00101
// External bias current calibration registers      
#define PMA_Reg_Addr_CMN_IBCAL_INIT_TMR                 0x0074    // 00_0_0000 : 0011 : 10100
#define PMA_Reg_Addr_CMN_IBCAL_ITER_TMR                 0x0075    // 00_0_0000 : 0011 : 10101
// Resitor calibration registers        
#define PMA_Reg_Addr_CMN_TXPUCAL_INIT_TMR               0x0104    // 00_0_0000 : 1000 : 00100
#define PMA_Reg_Addr_CMN_TXPUCAL_ITER_TMR               0x0105    // 00_0_0000 : 1000 : 00101
#define PMA_Reg_Addr_CMN_TXPDCAL_INIT_TMR               0x010C    // 00_0_0000 : 1000 : 01100
#define PMA_Reg_Addr_CMN_TXPDCAL_ITER_TMR               0x010D    // 00_0_0000 : 1000 : 01101
#define PMA_Reg_Addr_CMN_RXCAL_INIT_TMR                 0x0114    // 00_0_0000 : 1000 : 10100 
#define PMA_Reg_Addr_CMN_RXCAL_ITER_TMR                 0x0115    // 00_0_0000 : 1000 : 10101
// Signal detect clock calibration registers        
#define PMA_Reg_Addr_CMN_SD_CAL_INIT_TMR                0x0124    // 00_0_0000 : 1001 : 00100
#define PMA_Reg_Addr_CMN_SD_CAL_ITER_TMR                0x0125    // 00_0_0000 : 1001 : 00101 
#define PMA_Reg_Addr_CMN_SD_CAL_REFTIM_START            0x0126    // 00_0_0000 : 1001 : 00110 
#define PMA_Reg_Addr_CMN_SD_CAL_PLLCNT_START            0x0128    // 00_0_0000 : 1001 : 01000
// Fast establishment of common mode registers
// These registers are only required for PCIe standard programming.
#define PMA_Reg_Addr_Lane0_DRV_DIAG_LANE_FCM_EN_TO      0x40C0    // 01_0_0000 : 0110 : 00000
#define PMA_Reg_Addr_Lane1_DRV_DIAG_LANE_FCM_EN_TO      0x42C0    // 01_0_0001 : 0110 : 00000
#define PMA_Reg_Addr_Lane2_DRV_DIAG_LANE_FCM_EN_TO      0x44C0    // 01_0_0010 : 0110 : 00000
#define PMA_Reg_Addr_Lane3_DRV_DIAG_LANE_FCM_EN_TO      0x46C0    // 01_0_0011 : 0110 : 00000

#define PMA_Reg_Addr_Lane0_DRV_DIAG_LANE_FCM_EN_MGN_TMR 0x40C2    // 01_0_0000 : 0110 : 00010
#define PMA_Reg_Addr_Lane1_DRV_DIAG_LANE_FCM_EN_MGN_TMR 0x42C2    // 01_0_0001 : 0110 : 00010
#define PMA_Reg_Addr_Lane2_DRV_DIAG_LANE_FCM_EN_MGN_TMR 0x44C2    // 01_0_0010 : 0110 : 00010
#define PMA_Reg_Addr_Lane3_DRV_DIAG_LANE_FCM_EN_MGN_TMR 0x46C2    // 01_0_0011 : 0110 : 00010
// Signal detect calibration registers                                                                 
#define PMA_Reg_Addr_Lane0_RX_SDCAL0_INIT_TMR           0x8044    // 10_0_0000 : 0010 : 00100
#define PMA_Reg_Addr_Lane1_RX_SDCAL0_INIT_TMR           0x8244    // 10_0_0001 : 0010 : 00100
#define PMA_Reg_Addr_Lane2_RX_SDCAL0_INIT_TMR           0x8444    // 10_0_0010 : 0010 : 00100
#define PMA_Reg_Addr_Lane3_RX_SDCAL0_INIT_TMR           0x8644    // 10_0_0011 : 0010 : 00100
                                                                                                    
#define PMA_Reg_Addr_Lane0_RX_SDCAL0_ITER_TMR           0x8045    // 10_0_0000 : 0010 : 00101
#define PMA_Reg_Addr_Lane1_RX_SDCAL0_ITER_TMR           0x8245    // 10_0_0001 : 0010 : 00101
#define PMA_Reg_Addr_Lane2_RX_SDCAL0_ITER_TMR           0x8445    // 10_0_0010 : 0010 : 00101
#define PMA_Reg_Addr_Lane3_RX_SDCAL0_ITER_TMR           0x8645    // 10_0_0011 : 0010 : 00101
                                                                                                                                  
#define PMA_Reg_Addr_Lane0_RX_SDCAL1_INIT_TMR           0x804C    // 10_0_0000 : 0010 : 01100
#define PMA_Reg_Addr_Lane1_RX_SDCAL1_INIT_TMR           0x824C    // 10_0_0001 : 0010 : 01100
#define PMA_Reg_Addr_Lane2_RX_SDCAL1_INIT_TMR           0x844C    // 10_0_0010 : 0010 : 01100
#define PMA_Reg_Addr_Lane3_RX_SDCAL1_INIT_TMR           0x864C    // 10_0_0011 : 0010 : 01100
                                                                                                                                  
#define PMA_Reg_Addr_Lane0_RX_SDCAL1_ITER_TMR           0x804D    // 10_0_0000 : 0010 : 01101
#define PMA_Reg_Addr_Lane1_RX_SDCAL1_ITER_TMR           0x824D    // 10_0_0001 : 0010 : 01101
#define PMA_Reg_Addr_Lane2_RX_SDCAL1_ITER_TMR           0x844D    // 10_0_0010 : 0010 : 01101
#define PMA_Reg_Addr_Lane3_RX_SDCAL1_ITER_TMR           0x864D    // 10_0_0011 : 0010 : 01101
// Receiver detect calibration registers                                                                  
#define PMA_Reg_Addr_Lane0_TX_RCVDET_ST_TMR             0x4123    // 01_0_0000 : 1001 : 00011
#define PMA_Reg_Addr_Lane1_TX_RCVDET_ST_TMR             0x4323    // 01_0_0001 : 1001 : 00011
#define PMA_Reg_Addr_Lane2_TX_RCVDET_ST_TMR             0x4523    // 01_0_0010 : 1001 : 00011
#define PMA_Reg_Addr_Lane3_TX_RCVDET_ST_TMR             0x4723    // 01_0_0011 : 1001 : 00011
// Link configuration related registers.                                                                 
#define PHY_Reg_Addr_PHY_PLL_CFG                        0xC00E    // 16'b11_0_0_0000_0000_1110
#define PMA_Reg_Addr_CMN_PDIAG_PLL0_CLK_SEL_M0          0x01A1    // 00_0_0000 : 1101 : 00001
#define PMA_Reg_Addr_CMN_PDIAG_PLL0_CLK_SEL_M1          0x01B1    // 00_0_0000 : 1101 : 10001
#define PMA_Reg_Addr_CMN_PDIAG_PLL1_CLK_SEL_M0          0x01C1    // 00_0_0000 : 1110 : 00001
                                                                                                  
#define PMA_Reg_Addr_Lane0_XCVR_DIAG_HSCLK_SEL          0x40E6    // 01_0_0000 : 0111 : 00110
#define PMA_Reg_Addr_Lane1_XCVR_DIAG_HSCLK_SEL          0x42E6    // 01_0_0001 : 0111 : 00110
#define PMA_Reg_Addr_Lane2_XCVR_DIAG_HSCLK_SEL          0x44E6    // 01_0_0010 : 0111 : 00110
#define PMA_Reg_Addr_Lane3_XCVR_DIAG_HSCLK_SEL          0x46E6    // 01_0_0011 : 0111 : 00110
                                                                                                      
#define PMA_Reg_Addr_Lane0_XCVR_DIAG_HSCLK_DIV          0x40E7    // 01_0_0000 : 0111 : 00111
#define PMA_Reg_Addr_Lane1_XCVR_DIAG_HSCLK_DIV          0x42E7    // 01_0_0001 : 0111 : 00111
#define PMA_Reg_Addr_Lane2_XCVR_DIAG_HSCLK_DIV          0x44E7    // 01_0_0010 : 0111 : 00111
#define PMA_Reg_Addr_Lane3_XCVR_DIAG_HSCLK_DIV          0x46E7    // 01_0_0011 : 0111 : 00111
                                                                                                      
#define PMA_Reg_Addr_Lane0_XCVR_DIAG_PLLDRC_CTRL        0x40E5    // 01_0_0000 : 0111 : 00101
#define PMA_Reg_Addr_Lane1_XCVR_DIAG_PLLDRC_CTRL        0x42E5    // 01_0_0001 : 0111 : 00101
#define PMA_Reg_Addr_Lane2_XCVR_DIAG_PLLDRC_CTRL        0x44E5    // 01_0_0010 : 0111 : 00101
#define PMA_Reg_Addr_Lane3_XCVR_DIAG_PLLDRC_CTRL        0x46E5    // 01_0_0011 : 0111 : 00101
// PLL related registers.                                                                                                   
// General PLL settings for all rates                                                                                                   
#define PMA_Reg_Addr_CMN_PLL0_DSM_DIAG_M0               0x0094    // 00_0_0000 : 0100 : 10100
#define PMA_Reg_Addr_CMN_PLL1_DSM_DIAG_M0               0x00D4    // 00_0_0000 : 0110 : 10100
// Analog PLL settings                                                                                                        
#define PMA_Reg_Addr_CMN_DIAG_GPANA_1                   0x01F1    // 00_0_0000 : 1111 : 10001
#define PMA_Reg_Addr_CMN_PDIAG_PLL0_ITRIM_M0            0x01A3    // 00_0_0000 : 1101 : 00011
#define PMA_Reg_Addr_CMN_PDIAG_PLL1_ITRIM_M0            0x01C3    // 00_0_0000 : 1110 : 00011
#define PMA_Reg_Addr_CMN_PDIAG_PLL0_CP_PADJ_M0          0x01A4    // 00_0_0000 : 1101 : 00100
#define PMA_Reg_Addr_CMN_PDIAG_PLL1_CP_PADJ_M0          0x01C4    // 00_0_0000 : 1110 : 00100
#define PMA_Reg_Addr_CMN_PDIAG_PLL0_CP_IADJ_M0          0x01A5    // 00_0_0000 : 1101 : 00101
#define PMA_Reg_Addr_CMN_PDIAG_PLL1_CP_IADJ_M0          0x01C5    // 00_0_0000 : 1110 : 00101
#define PMA_Reg_Addr_CMN_PDIAG_PLL0_FILT_PADJ_M0        0x01A6    // 00_0_0000 : 1101 : 00110
#define PMA_Reg_Addr_CMN_PDIAG_PLL1_FILT_PADJ_M0        0x01C6    // 00_0_0000 : 1110 : 00110
// Integer mode feedback divider settings.      
#define PMA_Reg_Addr_CMN_PLL0_DSM_FBH_OVRD_M0           0x0095    // 00_0_0000 : 0100 : 10101
#define PMA_Reg_Addr_CMN_PLL1_DSM_FBH_OVRD_M0           0x00D5    // 00_0_0000 : 0110 : 10101
#define PMA_Reg_Addr_CMN_PLL0_DSM_FBL_OVRD_M0           0x0096    // 00_0_0000 : 0100 : 10110
#define PMA_Reg_Addr_CMN_PLL1_DSM_FBL_OVRD_M0           0x00D6    // 00_0_0000 : 0110 : 10110
// Fractional mode feedback divider settings.       
#define PMA_Reg_Addr_CMN_PLL0_INTDIV_M0                 0x0090    // 00_0_0000 : 0100 : 10000
#define PMA_Reg_Addr_CMN_PLL1_INTDIV_M0                 0x00D0    // 00_0_0000 : 0110 : 10000
#define PMA_Reg_Addr_CMN_PLL0_FRACDIVL_M0               0x0091    // 00_0_0000 : 0100 : 10001
#define PMA_Reg_Addr_CMN_PLL1_FRACDIVL_M0               0x00D1    // 00_0_0000 : 0110 : 10001
#define PMA_Reg_Addr_CMN_PLL0_FRACDIVH_M0               0x0092    // 00_0_0000 : 0100 : 10010 
#define PMA_Reg_Addr_CMN_PLL1_FRACDIVH_M0               0x00D2    // 00_0_0000 : 0110 : 10010 
#define PMA_Reg_Addr_CMN_PLL0_HIGH_THR_M0               0x0093    // 00_0_0000 : 0100 : 10011 
#define PMA_Reg_Addr_CMN_PLL1_HIGH_THR_M0               0x00D3    // 00_0_0000 : 0110 : 10011 
// PLL control register settings.       
#define PMA_Reg_Addr_CMN_PDIAG_PLL0_CTRL_M0             0x01A0    // 00_0_0000 : 1101 : 00000
#define PMA_Reg_Addr_CMN_PDIAG_PLL1_CTRL_M0             0x01C0    // 00_0_0000 : 1110 : 00000
// Spread spectrum settings.
// When enabled this is approximately 5000 PPM between 30 KHz and 33 KHz
#define PMA_Reg_Addr_CMN_PLL0_SS_CTRL1_M0               0x0098    // 00_0_0000 : 0100 : 11000
#define PMA_Reg_Addr_CMN_PLL1_SS_CTRL1_M0               0x00D8    // 00_0_0000 : 0110 : 11000
#define PMA_Reg_Addr_CMN_PLL0_SS_CTRL2_M0               0x0099    // 00_0_0000 : 0100 : 11001
#define PMA_Reg_Addr_CMN_PLL1_SS_CTRL2_M0               0x00D9    // 00_0_0000 : 0110 : 11001
#define PMA_Reg_Addr_CMN_PLL0_SS_CTRL3_M0               0x009A    // 00_0_0000 : 0100 : 11010
#define PMA_Reg_Addr_CMN_PLL1_SS_CTRL3_M0               0x00DA    // 00_0_0000 : 0110 : 11010
#define PMA_Reg_Addr_CMN_PLL0_SS_CTRL4_M0               0x009B    // 00_0_0000 : 0100 : 11011
#define PMA_Reg_Addr_CMN_PLL1_SS_CTRL4_M0               0x00DB    // 00_0_0000 : 0110 : 11011
// VCO calibration settings.        
#define PMA_Reg_Addr_CMN_PLL0_VCOCAL_INIT_TMR           0x0084    // 00_0_0000 : 0100 : 00100
#define PMA_Reg_Addr_CMN_PLL1_VCOCAL_INIT_TMR           0x00C4    // 00_0_0000 : 0110 : 00100
#define PMA_Reg_Addr_CMN_PLL0_VCOCAL_ITER_TMR           0x0085    // 00_0_0000 : 0100 : 00101
#define PMA_Reg_Addr_CMN_PLL1_VCOCAL_ITER_TMR           0x00C5    // 00_0_0000 : 0110 : 00101
#define PMA_Reg_Addr_CMN_PLL0_VCOCAL_REFTIM_START       0x0086    // 00_0_0000 : 0100 : 00110
#define PMA_Reg_Addr_CMN_PLL1_VCOCAL_REFTIM_START       0x00C6    // 00_0_0000 : 0110 : 00110
#define PMA_Reg_Addr_CMN_PLL0_VCOCAL_PLLCNT_START       0x0088    // 00_0_0000 : 0100 : 01000
#define PMA_Reg_Addr_CMN_PLL1_VCOCAL_PLLCNT_START       0x00C8    // 00_0_0000 : 0110 : 01000
#define PMA_Reg_Addr_CMN_PLL0_VCOCAL_TCTRL              0x0082    // 00_0_0000 : 0100 : 00010
#define PMA_Reg_Addr_CMN_PLL1_VCOCAL_TCTRL              0x00C2    // 00_0_0000 : 0110 : 00010
// PLL lock detect settings     
#define PMA_Reg_Addr_CMN_PLL0_LOCK_REFCNT_START         0x009C    // 00_0_0000 : 0100 : 11100
#define PMA_Reg_Addr_CMN_PLL1_LOCK_REFCNT_START         0x00DC    // 00_0_0000 : 0110 : 11100
#define PMA_Reg_Addr_CMN_PLL0_LOCK_PLLCNT_START         0x009E    // 00_0_0000 : 0100 : 11110 
#define PMA_Reg_Addr_CMN_PLL1_LOCK_PLLCNT_START         0x00DE    // 00_0_0000 : 0110 : 11110
#define PMA_Reg_Addr_CMN_PLL0_LOCK_PLLCNT_THR           0x009F    // 00_0_0000 : 0100 : 11111
#define PMA_Reg_Addr_CMN_PLL1_LOCK_PLLCNT_THR           0x00DF    // 00_0_0000 : 0110 : 11111

// Note: Registers and pins specific to Display Port / eDP lanes.
// Power state controller registers
#define PMA_Reg_Addr_Lane0_TX_PSC_A0                    0x4100    // 01_0_0000 : 1000 : 00000
#define PMA_Reg_Addr_Lane0_TX_PSC_A2                    0x4102    // 01_0_0000 : 1000 : 00010
#define PMA_Reg_Addr_Lane0_TX_PSC_A3                    0x4103    // 01_0_0000 : 1000 : 00011
#define PMA_Reg_Addr_Lane0_RX_PSC_A0                    0x8000    // 10_0_0000 : 0000 : 00000
#define PMA_Reg_Addr_Lane0_RX_PSC_A2                    0x8002    // 10_0_0000 : 0000 : 00010
#define PMA_Reg_Addr_Lane0_RX_PSC_A3                    0x8003    // 10_0_0000 : 0000 : 00011
#define PMA_Reg_Addr_Lane0_RX_PSC_CAL                   0x8006    // 10_0_0000 : 0000 : 00110
                                                                                          
#define PMA_Reg_Addr_Lane1_TX_PSC_A0                    0x4300    // 01_0_0001 : 1000 : 00000
#define PMA_Reg_Addr_Lane1_TX_PSC_A2                    0x4302    // 01_0_0001 : 1000 : 00010
#define PMA_Reg_Addr_Lane1_TX_PSC_A3                    0x4303    // 01_0_0001 : 1000 : 00011
#define PMA_Reg_Addr_Lane1_RX_PSC_A0                    0x8200    // 10_0_0001 : 0000 : 00000
#define PMA_Reg_Addr_Lane1_RX_PSC_A2                    0x8202    // 10_0_0001 : 0000 : 00010
#define PMA_Reg_Addr_Lane1_RX_PSC_A3                    0x8203    // 10_0_0001 : 0000 : 00011
#define PMA_Reg_Addr_Lane1_RX_PSC_CAL                   0x8206    // 10_0_0001 : 0000 : 00110
                                                                                                
#define PMA_Reg_Addr_Lane2_TX_PSC_A0                    0x4500    // 01_0_0010 : 1000 : 00000
#define PMA_Reg_Addr_Lane2_TX_PSC_A2                    0x4502    // 01_0_0010 : 1000 : 00010
#define PMA_Reg_Addr_Lane2_TX_PSC_A3                    0x4503    // 01_0_0010 : 1000 : 00011
#define PMA_Reg_Addr_Lane2_RX_PSC_A0                    0x8400    // 10_0_0010 : 0000 : 00000
#define PMA_Reg_Addr_Lane2_RX_PSC_A2                    0x8402    // 10_0_0010 : 0000 : 00010
#define PMA_Reg_Addr_Lane2_RX_PSC_A3                    0x8403    // 10_0_0010 : 0000 : 00011
#define PMA_Reg_Addr_Lane2_RX_PSC_CAL                   0x8406    // 10_0_0010 : 0000 : 00110
                                                                                                 
#define PMA_Reg_Addr_Lane3_TX_PSC_A0                    0x4700    // 01_0_0011 : 1000 : 00000
#define PMA_Reg_Addr_Lane3_TX_PSC_A2                    0x4702    // 01_0_0011 : 1000 : 00010
#define PMA_Reg_Addr_Lane3_TX_PSC_A3                    0x4703    // 01_0_0011 : 1000 : 00011
#define PMA_Reg_Addr_Lane3_RX_PSC_A0                    0x8600    // 10_0_0011 : 0000 : 00000
#define PMA_Reg_Addr_Lane3_RX_PSC_A2                    0x8602    // 10_0_0011 : 0000 : 00010
#define PMA_Reg_Addr_Lane3_RX_PSC_A3                    0x8603    // 10_0_0011 : 0000 : 00011
#define PMA_Reg_Addr_Lane3_RX_PSC_CAL                   0x8606    // 10_0_0011 : 0000 : 00110
// Transceiver control and diagnostic registers                                                                                             
#define PMA_Reg_Addr_Lane0_XCVR_DIAG_BIDI_CTRL          0x407A    // 01_0_0000 : 0111 : 01010
#define PMA_Reg_Addr_Lane1_XCVR_DIAG_BIDI_CTRL          0x427A    // 01_0_0001 : 0111 : 01010
#define PMA_Reg_Addr_Lane2_XCVR_DIAG_BIDI_CTRL          0x447A    // 01_0_0010 : 0111 : 01010
#define PMA_Reg_Addr_Lane3_XCVR_DIAG_BIDI_CTRL          0x467A    // 01_0_0011 : 0111 : 01010
// Receiver equalizer engine registers                                                                                                    
#define PMA_Reg_Addr_Lane0_RX_REE_GCSM1_CTRL            0x8108    // 10_0_0000 : 1000 : 01000
#define PMA_Reg_Addr_Lane0_RX_REE_GCSM2_CTRL            0x8110    // 10_0_0000 : 1000 : 10000
#define PMA_Reg_Addr_Lane0_RX_REE_PERGCSM_CTRL          0x8118    // 10_0_0000 : 1000 : 11000
                                                                                                  
#define PMA_Reg_Addr_Lane1_RX_REE_GCSM1_CTRL            0x8308    // 10_0_0001 : 1000 : 01000
#define PMA_Reg_Addr_Lane1_RX_REE_GCSM2_CTRL            0x8310    // 10_0_0001 : 1000 : 10000
#define PMA_Reg_Addr_Lane1_RX_REE_PERGCSM_CTRL          0x8318    // 10_0_0001 : 1000 : 11000
                                                                                               
#define PMA_Reg_Addr_Lane2_RX_REE_GCSM1_CTRL            0x8508    // 10_0_0010 : 1000 : 01000
#define PMA_Reg_Addr_Lane2_RX_REE_GCSM2_CTRL            0x8510    // 10_0_0010 : 1000 : 10000
#define PMA_Reg_Addr_Lane2_RX_REE_PERGCSM_CTRL          0x8518    // 10_0_0010 : 1000 : 11000
                                                                                                 
#define PMA_Reg_Addr_Lane3_RX_REE_GCSM1_CTRL            0x8708    // 10_0_0011 : 1000 : 01000
#define PMA_Reg_Addr_Lane3_RX_REE_GCSM2_CTRL            0x8710    // 10_0_0011 : 1000 : 10000
#define PMA_Reg_Addr_Lane3_RX_REE_PERGCSM_CTRL          0x8718    // 10_0_0011 : 1000 : 11000
                                                                                              
// Note: Special Display port swing level and emphasis level change instructions.
#define PMA_Reg_Addr_Lane0_TX_TXCC_CTRL                 0x4040    // 01_0_0000 : 0010 : 00000                                           
#define PMA_Reg_Addr_Lane0_DRV_DIAG_TX_DRV              0x40C6    // 01_0_0000 : 0110 : 00110 
#define PMA_Reg_Addr_Lane0_TX_TXCC_MGNFS_MULT_000       0x4050    // 01_0_0000 : 0010 : 10000                                                    
#define PMA_Reg_Addr_Lane0_TX_TXCC_CPOST_MULT_00        0x404C    // 01_0_0000 : 0010 : 01100    

#define PMA_Reg_Addr_Lane1_TX_TXCC_CTRL                 0x4240    // 01_0_0001 : 0010 : 00000                                           
#define PMA_Reg_Addr_Lane1_DRV_DIAG_TX_DRV              0x42C6    // 01_0_0001 : 0110 : 00110 
#define PMA_Reg_Addr_Lane1_TX_TXCC_MGNFS_MULT_000       0x4250    // 01_0_0001 : 0010 : 10000                                                    
#define PMA_Reg_Addr_Lane1_TX_TXCC_CPOST_MULT_00        0x424C    // 01_0_0001 : 0010 : 01100   

#define PMA_Reg_Addr_Lane2_TX_TXCC_CTRL                 0x4440    // 01_0_0010 : 0010 : 00000                                           
#define PMA_Reg_Addr_Lane2_DRV_DIAG_TX_DRV              0x44C6    // 01_0_0010 : 0110 : 00110 
#define PMA_Reg_Addr_Lane2_TX_TXCC_MGNFS_MULT_000       0x4450    // 01_0_0010 : 0010 : 10000                                                    
#define PMA_Reg_Addr_Lane2_TX_TXCC_CPOST_MULT_00        0x444C    // 01_0_0010 : 0010 : 01100 

#define PMA_Reg_Addr_Lane3_TX_TXCC_CTRL                 0x4640    // 01_0_0011 : 0010 : 00000                                           
#define PMA_Reg_Addr_Lane3_DRV_DIAG_TX_DRV              0x46C6    // 01_0_0011 : 0110 : 00110 
#define PMA_Reg_Addr_Lane3_TX_TXCC_MGNFS_MULT_000       0x4650    // 01_0_0011 : 0010 : 10000                                                    
#define PMA_Reg_Addr_Lane3_TX_TXCC_CPOST_MULT_00        0x464C    // 01_0_0011 : 0010 : 01100 

#define PMA_Reg_Addr_PHY_PMA_PLL_RAW_CTRL               0xE003    // 16’b11_1_0_0000_0000_0011
#define PMA_Reg_Addr_PHY_PMA_CMN_CTRL2                  0xE001    // 16’b11_1_0_0000_0000_0001

#define PMA_Reg_Addr_Lane0_TX_DIAG_ACYA                 0x41E7    // 01_0_0000 : 1111 : 00111
#define PMA_Reg_Addr_Lane1_TX_DIAG_ACYA                 0x43E7    // 01_0_0001 : 1111 : 00111
#define PMA_Reg_Addr_Lane2_TX_DIAG_ACYA                 0x45E7    // 01_0_0010 : 1111 : 00111
#define PMA_Reg_Addr_Lane3_TX_DIAG_ACYA                 0x47E7    // 01_0_0011 : 1111 : 00111

typedef struct _dp_phy_pin_config_t { 
	unsigned short pma_cmn_refclk_mode; 
	unsigned short pma_cmn_refclk_int_mode; 
	unsigned short pma_cmn_refclk_dig_div; 
	unsigned short pma_cmn_refclk_dig_sel; 
	unsigned short pma_cmn_pll0_refclk_sel; 
	unsigned short pma_cmn_pll1_refclk_sel; 
	unsigned short phy_lnn_00_mode; 
	unsigned short phy_link_cfg_ln_n_1; 
	unsigned short phy_link_cfg_ln_n_1_SLV_LANE; 
	unsigned short pma_xcvr_standard_mode_ln_n_0; 
	unsigned short pma_xcvr_data_width_ln_n_0; 
	unsigned short pma_tx_deemphasis_ln_n_0; 
	unsigned short pma_tx_vmargin_ln_n_0; 
	unsigned short pma_tx_low_power_swing_en_ln_n_0; 
} dp_phy_pin_config_t;

typedef struct _dp_phy_reg_config_t { 
	unsigned short pHY_PLL_CFG; 
	unsigned short pHY_PMA_ISO_PLL_CTRL0; // not needed for DP.
	unsigned short pHY_PMA_ISO_PLL_CTRL1; // not needed for DP. 
} dp_phy_reg_config_t;

typedef struct _dp_pma_reg_config_t { 
	unsigned short cMN_SSM_BANDGAP_TMR; 
	unsigned short cMN_SSM_BIAS_TMR; 
	unsigned short cMN_PLLSM0_PLLEN_TMR; 
	unsigned short cMN_PLLSM0_PLLPRE_TMR; 
	unsigned short cMN_PLLSM0_PLLLOCK_TMR; 
	unsigned short cMN_PLLSM1_PLLEN_TMR; 
	unsigned short cMN_PLLSM1_PLLPRE_TMR; 
	unsigned short cMN_PLLSM1_PLLLOCK_TMR; 
	unsigned short cMN_BGCAL_INIT_TMR; 
	unsigned short cMN_BGCAL_ITER_TMR; 
	unsigned short cMN_IBCAL_INIT_TMR; 
	unsigned short cMN_IBCAL_ITER_TMR; 
	unsigned short cMN_TXPUCAL_INIT_TMR; 
	unsigned short cMN_TXPUCAL_ITER_TMR; 
	unsigned short cMN_TXPDCAL_INIT_TMR; 
	unsigned short cMN_TXPDCAL_ITER_TMR; 
	unsigned short cMN_RXCAL_INIT_TMR; 
	unsigned short cMN_RXCAL_ITER_TMR; 
	unsigned short cMN_SD_CAL_INIT_TMR; 
	unsigned short cMN_SD_CAL_ITER_TMR; 
	unsigned short cMN_SD_CAL_REFTIM_START; 
	unsigned short cMN_SD_CAL_PLLCNT_START; 
	unsigned short dRV_DIAG_LANE_FCM_EN_TO; 
	unsigned short dRV_DIAG_LANE_FCM_EN_MGN_TMR; 
	unsigned short rX_SDCAL0_INIT_TMR; 
	unsigned short rX_SDCAL0_ITER_TMR; 
	unsigned short rX_SDCAL1_INIT_TMR; 
	unsigned short rX_SDCAL1_ITER_TMR; 
	unsigned short tX_RCVDET_ST_TMR; 
	unsigned short cMN_PDIAG_PLL0_CLK_SEL_M0; 
	unsigned short cMN_PDIAG_PLL0_CLK_SEL_M1; 
	unsigned short cMN_PDIAG_PLL1_CLK_SEL_M0; 
	unsigned short xCVR_DIAG_HSCLK_SEL; 
	unsigned short xCVR_DIAG_HSCLK_DIV; 
	unsigned short xCVR_DIAG_PLLDRC_CTRL; 
	unsigned short xCVR_DIAG_HSCLK_SEL_TRUE; 
	unsigned short xCVR_DIAG_HSCLK_DIV_TRUE; 
	unsigned short xCVR_DIAG_PLLDRC_CTRL_TRUE; 
	unsigned short cMN_PLL0_DSM_DIAG_M0; 
	unsigned short cMN_PLL1_DSM_DIAG_M0; 
	unsigned short cMN_DIAG_GPANA_1; 
	unsigned short cMN_PDIAG_PLL0_ITRIM_M0; 
	unsigned short cMN_PDIAG_PLL1_ITRIM_M0; 
	unsigned short cMN_PDIAG_PLL0_CP_PADJ_M0; 
	unsigned short cMN_PDIAG_PLL1_CP_PADJ_M0; 
	unsigned short cMN_PDIAG_PLL0_CP_IADJ_M0; 
	unsigned short cMN_PDIAG_PLL1_CP_IADJ_M0; 
	unsigned short cMN_PDIAG_PLL0_FILT_PADJ_M0; 
	unsigned short cMN_PDIAG_PLL1_FILT_PADJ_M0; 
	unsigned short cMN_PLL0_DSM_FBH_OVRD_M0; 
	unsigned short cMN_PLL1_DSM_FBH_OVRD_M0; 
	unsigned short cMN_PLL0_DSM_FBL_OVRD_M0; 
	unsigned short cMN_PLL1_DSM_FBL_OVRD_M0; 
	unsigned short cMN_PLL0_INTDIV_M0; 
	unsigned short cMN_PLL1_INTDIV_M0; 
	unsigned short cMN_PLL0_FRACDIVL_M0; 
	unsigned short cMN_PLL1_FRACDIVL_M0; 
	unsigned short cMN_PLL0_FRACDIVH_M0; 
	unsigned short cMN_PLL1_FRACDIVH_M0; 
	unsigned short cMN_PLL0_HIGH_THR_M0; 
	unsigned short cMN_PLL1_HIGH_THR_M0; 
	unsigned short cMN_PDIAG_PLL0_CTRL_M0; 
	unsigned short cMN_PDIAG_PLL1_CTRL_M0; 
	unsigned short cMN_PLL0_SS_CTRL1_M0; 
	unsigned short cMN_PLL1_SS_CTRL1_M0; 
	unsigned short cMN_PLL0_SS_CTRL2_M0; 
	unsigned short cMN_PLL1_SS_CTRL2_M0; 
	unsigned short cMN_PLL0_SS_CTRL3_M0; 
	unsigned short cMN_PLL1_SS_CTRL3_M0; 
	unsigned short cMN_PLL0_SS_CTRL4_M0; 
	unsigned short cMN_PLL1_SS_CTRL4_M0; 
	unsigned short cMN_PLL0_VCOCAL_INIT_TMR; 
	unsigned short cMN_PLL1_VCOCAL_INIT_TMR; 
	unsigned short cMN_PLL0_VCOCAL_ITER_TMR; 
	unsigned short cMN_PLL1_VCOCAL_ITER_TMR; 
	unsigned short cMN_PLL0_VCOCAL_REFTIM_START; 
	unsigned short cMN_PLL1_VCOCAL_REFTIM_START; 
	unsigned short cMN_PLL0_VCOCAL_PLLCNT_START; 
	unsigned short cMN_PLL1_VCOCAL_PLLCNT_START; 
	unsigned short cMN_PLL0_VCOCAL_TCTRL; 
	unsigned short cMN_PLL1_VCOCAL_TCTRL; 
	unsigned short cMN_PLL0_LOCK_REFCNT_START; 
	unsigned short cMN_PLL1_LOCK_REFCNT_START; 
	unsigned short cMN_PLL0_LOCK_PLLCNT_START; 
	unsigned short cMN_PLL1_LOCK_PLLCNT_START; 
	unsigned short cMN_PLL0_LOCK_PLLCNT_THR; 
	unsigned short cMN_PLL1_LOCK_PLLCNT_THR; 
	unsigned short tX_PSC_A0; 
	unsigned short tX_PSC_A2; 
	unsigned short tX_PSC_A3; 
	unsigned short rX_PSC_A0; 
	unsigned short rX_PSC_A2; 
	unsigned short rX_PSC_A3; 
	unsigned short rX_PSC_CAL; 
	unsigned short xCVR_DIAG_BIDI_CTRL; 
	unsigned short rX_REE_GCSM1_CTRL; 
	unsigned short rX_REE_GCSM2_CTRL; 
	unsigned short rX_REE_PERGCSM_CTRL; 
	unsigned short tX_TXCC_CTRL; 
	unsigned short tX_TXCC_CTRL_DP_OR_EDP_DP_DE_DP_1; 
	unsigned short tX_TXCC_CTRL_DP_OR_EDP_DP_DE_DP_2; 
	unsigned short tX_TXCC_CTRL_DP_OR_EDP_DP_DE_DP_3; 
	unsigned short dRV_DIAG_TX_DRV; 
	unsigned short dRV_DIAG_TX_DRV_DP_OR_EDP_DP_DE_DP_1; 
	unsigned short dRV_DIAG_TX_DRV_DP_OR_EDP_DP_DE_DP_2; 
	unsigned short dRV_DIAG_TX_DRV_DP_OR_EDP_DP_DE_DP_3; 
	unsigned short tX_TXCC_MGNFS_MULT_000; 
	unsigned short tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_0_DE_DP_1; 
	unsigned short tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_0_DE_DP_2; 
	unsigned short tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_0_DE_DP_3; 
	unsigned short tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_1_DE_DP_0; 
	unsigned short tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_1_DE_DP_1; 
	unsigned short tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_1_DE_DP_2; 
	unsigned short tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_2_DE_DP_0; 
	unsigned short tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_2_DE_DP_1; 
	unsigned short tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_DP_SL_DP_3_DE_DP_0; 
	unsigned short tX_TXCC_CPOST_MULT_00; 
	unsigned short tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_0_DE_DP_1; 
	unsigned short tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_0_DE_DP_2; 
	unsigned short tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_0_DE_DP_3; 
	unsigned short tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_1_DE_DP_0; 
	unsigned short tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_1_DE_DP_1; 
	unsigned short tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_1_DE_DP_2; 
	unsigned short tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_2_DE_DP_0; 
	unsigned short tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_2_DE_DP_1; 
	unsigned short tX_TXCC_CPOST_MULT_00_DP_OR_EDP_DP_SL_DP_3_DE_DP_0; 
	unsigned short tX_TXCC_CTRL_DP_OR_EDP_EDP_VDIFF_DP_LOW_DE_DP_0; 
	unsigned short tX_TXCC_CTRL_DP_OR_EDP_EDP_VDIFF_DP_LOW_DE_DP_1; 
	unsigned short tX_TXCC_CTRL_DP_OR_EDP_EDP_VDIFF_DP_LOW_DE_DP_2; 
	unsigned short tX_TXCC_CTRL_DP_OR_EDP_EDP_VDIFF_DP_LOW_DE_DP_3; 
	unsigned short dRV_DIAG_TX_DRV_DP_OR_EDP_EDP_VDIFF_DP_LOW_DE_DP_0; 
	unsigned short dRV_DIAG_TX_DRV_DP_OR_EDP_EDP_VDIFF_DP_LOW_DE_DP_1; 
	unsigned short dRV_DIAG_TX_DRV_DP_OR_EDP_EDP_VDIFF_DP_LOW_DE_DP_2; 
	unsigned short dRV_DIAG_TX_DRV_DP_OR_EDP_EDP_VDIFF_DP_LOW_DE_DP_3; 
	unsigned short tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_0_DE_DP_0; 
	unsigned short tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_0_DE_DP_1; 
	unsigned short tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_0_DE_DP_2; 
	unsigned short tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_0_DE_DP_3; 
	unsigned short tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_1_DE_DP_0; 
	unsigned short tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_1_DE_DP_1; 
	unsigned short tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_1_DE_DP_2; 
	unsigned short tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_2_DE_DP_0; 
	unsigned short tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_2_DE_DP_1; 
	unsigned short tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_3_DE_DP_0; 
	unsigned short tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_0_DE_DP_0; 
	unsigned short tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_0_DE_DP_1; 
	unsigned short tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_0_DE_DP_2; 
	unsigned short tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_0_DE_DP_3; 
	unsigned short tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_1_DE_DP_0; 
	unsigned short tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_1_DE_DP_1; 
	unsigned short tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_1_DE_DP_2; 
	unsigned short tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_2_DE_DP_0; 
	unsigned short tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_2_DE_DP_1; 
	unsigned short tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_LOW_SL_DP_3_DE_DP_0; 
	unsigned short tX_TXCC_CTRL_DP_OR_EDP_EDP_VDIFF_DP_HIGH_DE_DP_0; 
	unsigned short tX_TXCC_CTRL_DP_OR_EDP_EDP_VDIFF_DP_HIGH_DE_DP_1; 
	unsigned short tX_TXCC_CTRL_DP_OR_EDP_EDP_VDIFF_DP_HIGH_DE_DP_2; 
	unsigned short tX_TXCC_CTRL_DP_OR_EDP_EDP_VDIFF_DP_HIGH_DE_DP_3; 
	unsigned short dRV_DIAG_TX_DRV_DP_OR_EDP_EDP_VDIFF_DP_HIGH_DE_DP_0; 
	unsigned short dRV_DIAG_TX_DRV_DP_OR_EDP_EDP_VDIFF_DP_HIGH_DE_DP_1; 
	unsigned short dRV_DIAG_TX_DRV_DP_OR_EDP_EDP_VDIFF_DP_HIGH_DE_DP_2; 
	unsigned short dRV_DIAG_TX_DRV_DP_OR_EDP_EDP_VDIFF_DP_HIGH_DE_DP_3; 
	unsigned short tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_0_DE_DP_0; 
	unsigned short tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_0_DE_DP_1; 
	unsigned short tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_0_DE_DP_2; 
	unsigned short tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_0_DE_DP_3; 
	unsigned short tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_1_DE_DP_0; 
	unsigned short tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_1_DE_DP_1; 
	unsigned short tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_1_DE_DP_2; 
	unsigned short tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_2_DE_DP_0; 
	unsigned short tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_2_DE_DP_1; 
	unsigned short tX_TXCC_MGNFS_MULT_000_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_3_DE_DP_0; 
	unsigned short tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_0_DE_DP_0; 
	unsigned short tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_0_DE_DP_1; 
	unsigned short tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_0_DE_DP_2; 
	unsigned short tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_0_DE_DP_3; 
	unsigned short tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_1_DE_DP_0; 
	unsigned short tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_1_DE_DP_1; 
	unsigned short tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_1_DE_DP_2; 
	unsigned short tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_2_DE_DP_0; 
	unsigned short tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_2_DE_DP_1; 
	unsigned short tX_TXCC_CPOST_MULT_00_DP_OR_EDP_EDP_VDIFF_DP_HIGH_SL_DP_3_DE_DP_0; 
	unsigned short cMN_DIAG_BIAS_OVRD1; 
} dp_pma_reg_config_t;

typedef struct _dp_pma_reg_swing_emphasis_config_t {
    unsigned short tX_TXCC_CTRL; 
	unsigned short dRV_DIAG_TX_DRV; 
	unsigned short tX_TXCC_MGNFS_MULT_000; 
	unsigned short tX_TXCC_CPOST_MULT_00; 
} dp_pma_reg_swing_emphasis_config_t;

typedef struct _dp_phy_swing_emphasis_t {
    dp_pma_reg_swing_emphasis_config_t* swing_0_emphasis_0;
    dp_pma_reg_swing_emphasis_config_t* swing_0_emphasis_1;
    dp_pma_reg_swing_emphasis_config_t* swing_0_emphasis_2;
    dp_pma_reg_swing_emphasis_config_t* swing_0_emphasis_3;
    dp_pma_reg_swing_emphasis_config_t* swing_1_emphasis_0;
    dp_pma_reg_swing_emphasis_config_t* swing_1_emphasis_1;
    dp_pma_reg_swing_emphasis_config_t* swing_1_emphasis_2;
    dp_pma_reg_swing_emphasis_config_t* swing_2_emphasis_0;
    dp_pma_reg_swing_emphasis_config_t* swing_2_emphasis_1;
    dp_pma_reg_swing_emphasis_config_t* swing_3_emphasis_0;
} dp_phy_swing_emphasis_t;

typedef struct _dp_phy_config_t {
    dp_phy_pin_config_t* phy_pin_config;
    dp_phy_reg_config_t* phy_reg_config;
    dp_pma_reg_config_t* pma_reg_config;
    dp_phy_swing_emphasis_t* swing_emphasis_config;
} dp_phy_config_t;

struct dp_phy_t
{
    pdisp_void_t pdisp_deivce;
    disp_u8_t index;

    disp_u8_t ssc_en;
    disp_u8_t link_rate;
    disp_u8_t lane_count;
};

disp_void_t disp_dp_phy_pz1_setting(struct dp_phy_t* phy);
disp_void_t disp_dp_phy_pz1_capture_show_dp(struct dp_phy_t *phy, disp_u8_t enable);
disp_void_t disp_dp_phy_reset(struct dp_phy_t* phy, disp_u8_t reset);
disp_void_t disp_dp_phy_init(struct dp_phy_t* phy);
disp_void_t disp_dp_phy_link_training (struct dp_phy_t* phy);
#endif
