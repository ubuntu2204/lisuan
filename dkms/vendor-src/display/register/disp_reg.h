#ifndef __DISP_REG_H__
#define __DISP_REG_H__

#include "display.h"
#include "disp_callback.h"

#define CR_SHARE_OFFSET  0x5100
#define CR_A_OFFSET      0x5180
#define CR_B_OFFSET      0x5380
#define CR_C_OFFSET      0x5580
#define CR_D_OFFSET      0x5780
#define SR_SHARE_OFFSET  0x5000
#define SR_A_OFFSET      0x5010
#define SR_B_OFFSET      0x5210
#define SR_C_OFFSET      0x5410
#define SR_D_OFFSET      0x5610
#define PLNS_A_OFFSET    0x9000
#define PLNS_B_OFFSET    0xa000
#define PLNS_C_OFFSET    0xb000
#define PLNS_D_OFFSET    0xc000
#define PIPE_A_OFFSET    0x9600
#define PIPE_B_OFFSET    0xa600
#define PIPE_C_OFFSET    0xb600
#define PIPE_D_OFFSET    0xc600
#define DP_A_OFFSET      0x9700
#define DP_B_OFFSET      0xa700
#define DP_C_OFFSET      0xb700
#define DP_D_OFFSET      0xc700
#define DSC_A_OFFSET     0x9a00
#define DSC_B_OFFSET     0xaa00
#define DHI_OFFSET       0xba00
#define HAU_OFFSET       0xca00
#define PLLCTL_A_OFFSET  0x9d00
#define PLLCTL_B_OFFSET  0xad00
#define PLLCTL_C_OFFSET  0xbd00
#define PLLCTL_D_OFFSET  0xcd00
#define INTR_OFFSET      0xad20
#define PWM_OFFSET       0xbd20
#define PWRCTL_OFFSET    0xcd20
#define HDA_FUNC_OFFSET  0x0000

#define PPA_DP_EPHY_A_OFFSET 0x440000
#define PPA_DP_EPHY_B_OFFSET 0x460000
#define PPA_DP_EPHY_C_OFFSET 0x480000
#define PPA_DP_EPHY_D_OFFSET 0x4a0000

#define reg32_write_field_1(pdd, reg_name, reg_start, field_1_name, field_1_value) \
{ \
	reg_name reg; \
	reg.uint = disp_read_u32(pdd, reg_start); \
	reg.reg.field_1_name = field_1_value; \
	disp_write_u32(pdd, reg_start, reg.uint); \
}

#define reg32_write_field_2(pdd, reg_name, reg_start, field_1_name, field_1_value, f2, v2) \
{ \
	reg_name reg; \
	reg.uint = disp_read_u32(pdd, reg_start); \
	reg.reg.field_1_name = field_1_value; \
	reg.reg.f2 = v2; \
	disp_write_u32(pdd, reg_start, reg.uint); \
}

#define reg32_write_field_3(pdd, reg_name, reg_start, field_1_name, field_1_value, f2, v2, f3, v3) \
{ \
	reg_name reg; \
	reg.uint = disp_read_u32(pdd, reg_start); \
	reg.reg.field_1_name = field_1_value; \
	reg.reg.f2 = v2; \
	reg.reg.f3 = v3; \
	disp_write_u32(pdd, reg_start, reg.uint); \
}

#define reg32_write_field_4(pdd, reg_name, reg_start, field_1_name, field_1_value, f2, v2, f3, v3, f4, v4) \
{ \
	reg_name reg; \
	reg.uint = disp_read_u32(pdd, reg_start); \
	reg.reg.field_1_name = field_1_value; \
	reg.reg.f2 = v2; \
	reg.reg.f3 = v3; \
	reg.reg.f4 = v4; \
	disp_write_u32(pdd, reg_start, reg.uint); \
}

#define reg32_write_field_5(pdd, reg_name, reg_start, field_1_name, field_1_value, f2, v2, f3, v3, f4, v4, f5, v5) \
{ \
	reg_name reg; \
	reg.uint = disp_read_u32(pdd, reg_start); \
	reg.reg.field_1_name = field_1_value; \
	reg.reg.f2= v2; \
	reg.reg.f3 = v3; \
	reg.reg.f4 = v4; \
	reg.reg.f5 = v5; \
	disp_write_u32(pdd, reg_start, reg.uint); \
}

#define reg32_write_field_6(pdd, reg_name, reg_start, field_1_name, field_1_value, f2, v2, f3, v3, f4, v4, f5, v5, f6, v6) \
{ \
	reg_name reg; \
	reg.uint = disp_read_u32(pdd, reg_start); \
	reg.reg.field_1_name = field_1_value; \
	reg.reg.f2 = v2; \
	reg.reg.f3 = v3; \
	reg.reg.f4 = v4; \
	reg.reg.f5 = v5; \
	reg.reg.f6 = v6; \
	disp_write_u32(pdd, reg_start, reg.uint); \
}

#define reg32_write_field_7(pdd, reg_name, reg_start, field_1_name, field_1_value, f2, v2, f3, v3, f4, v4, f5, v5, f6, v6, f7, v7) \
{ \
	reg_name reg; \
	reg.uint = disp_read_u32(pdd, reg_start); \
	reg.reg.field_1_name = field_1_value; \
	reg.reg.f2 = v2; \
	reg.reg.f3 = v3; \
	reg.reg.f4 = v4; \
	reg.reg.f5 = v5; \
	reg.reg.f6 = v6; \
	reg.reg.f7 = v7; \
	disp_write_u32(pdd, reg_start, reg.uint); \
}

#define reg32_write_field_8(pdd, reg_name, reg_start, field_1_name, field_1_value, f2, v2, f3, v3, f4, v4, f5, v5, f6, v6, f7, v7, f8, v8) \
{ \
	reg_name reg; \
	reg.uint = disp_read_u32(pdd, reg_start); \
	reg.reg.field_1_name = field_1_value; \
	reg.reg.f2 = v2; \
	reg.reg.f3 = v3; \
	reg.reg.f4 = v4; \
	reg.reg.f5 = v5; \
	reg.reg.f6 = v6; \
	reg.reg.f7 = v7; \
	reg.reg.f8 = v8; \
	disp_write_u32(pdd, reg_start, reg.uint); \
}

#define reg32_write_field_9(pdd, reg_name, reg_start, field_1_name, field_1_value, f2, v2, f3, v3, f4, v4, f5, v5, f6, v6, f7, v7, f8, v8, f9, v9) \
{ \
	reg_name reg; \
	reg.uint = disp_read_u32(pdd, reg_start); \
	reg.reg.field_1_name = field_1_value; \
	reg.reg.f2 = v2; \
	reg.reg.f3 = v3; \
	reg.reg.f4 = v4; \
	reg.reg.f5 = v5; \
	reg.reg.f6 = v6; \
	reg.reg.f7 = v7; \
	reg.reg.f8 = v8; \
	reg.reg.f9 = v9; \
	disp_write_u32(pdd, reg_start, reg.uint); \
}
#define reg32_write_field_10(pdd, reg_name, reg_start, field_1_name, field_1_value, f2, v2, f3, v3, f4, v4, f5, v5, f6, v6, f7, v7, f8, v8, f9, v9, f10, v10) \
{ \
	reg_name reg; \
	reg.uint = disp_read_u32(pdd, reg_start); \
	reg.reg.field_1_name = field_1_value; \
	reg.reg.f2 = v2; \
	reg.reg.f3 = v3; \
	reg.reg.f4 = v4; \
	reg.reg.f5 = v5; \
	reg.reg.f6 = v6; \
	reg.reg.f7 = v7; \
	reg.reg.f8 = v8; \
	reg.reg.f9 = v9; \
	reg.reg.f10 = v10; \
	disp_write_u32(pdd, reg_start, reg.uint); \
}

#define reg32_write_field_11(pdd, reg_name, reg_start, field_1_name, field_1_value, f2, v2, f3, v3, f4, v4, f5, v5, f6, v6, f7, v7, f8, v8, f9, v9, f10, v10, f11, v11) \
{ \
	reg_name reg; \
	reg.uint = disp_read_u32(pdd, reg_start); \
	reg.reg.field_1_name = field_1_value; \
	reg.reg.f2 = v2; \
	reg.reg.f3 = v3; \
	reg.reg.f4 = v4; \
	reg.reg.f5 = v5; \
	reg.reg.f6 = v6; \
	reg.reg.f7 = v7; \
	reg.reg.f8 = v8; \
	reg.reg.f9 = v9; \
	reg.reg.f10 = v10; \
	reg.reg.f11 = v11; \
	disp_write_u32(pdd, reg_start, reg.uint); \
}

#define reg32_write_field_12(pdd, reg_name, reg_start, field_1_name, field_1_value, f2, v2, f3, v3, f4, v4, f5, v5, f6, v6, f7, v7, f8, v8, f9, v9, f10, v10, f11, v11, f12, v12) \
{ \
	reg_name reg; \
	reg.uint = disp_read_u32(pdd, reg_start); \
	reg.reg.field_1_name = field_1_value; \
	reg.reg.f2 = v2; \
	reg.reg.f3 = v3; \
	reg.reg.f4 = v4; \
	reg.reg.f5 = v5; \
	reg.reg.f6 = v6; \
	reg.reg.f7 = v7; \
	reg.reg.f8 = v8; \
	reg.reg.f9 = v9; \
	reg.reg.f10 = v10; \
	reg.reg.f11 = v11; \
	reg.reg.f12 = v12; \
	disp_write_u32(pdd, reg_start, reg.uint); \
}

#define reg32_read_field_1(pdd, reg_name, reg_start, field_1_name, pfield_1_value) \
{ \
	reg_name reg; \
	reg.uint = disp_read_u32(pdd, reg_start); \
	*pfield_1_value = reg.reg.field_1_name; \
}

#define reg32_read_field_2(pdd, reg_name, reg_start, field_1_name, pfield_1_value, f2, pv2) \
{ \
	reg_name reg; \
	reg.uint = disp_read_u32(pdd, reg_start); \
	*pfield_1_value = reg.reg.field_1_name; \
	*pv2 = reg.reg.f2; \
}

#define reg32_read_field_3(pdd, reg_name, reg_start, field_1_name, pfield_1_value, f2, pv2, f3, pv3) \
{ \
	reg_name reg; \
	reg.uint = disp_read_u32(pdd, reg_start); \
	*pfield_1_value = reg.reg.field_1_name; \
	*pv2 = reg.reg.f2; \
	*pv3 = reg.reg.f3; \
}

#define reg32_read_field_4(pdd, reg_name, reg_start, field_1_name, pfield_1_value, f2, pv2, f3, pv3, f4, pv4) \
{ \
	reg_name reg; \
	reg.uint = disp_read_u32(pdd, reg_start); \
	*pfield_1_value = reg.reg.field_1_name; \
	*pv2 = reg.reg.f2; \
	*pv3 = reg.reg.f3; \
	*pv4 = reg.reg.f4; \
}

#define reg32_read_field_5(pdd, reg_name, reg_start, field_1_name, pfield_1_value, f2, pv2, f3, pv3, f4, pv4, f5, pv5) \
{ \
	reg_name reg; \
	reg.uint = disp_read_u32(pdd, reg_start); \
	*pfield_1_value = reg.reg.field_1_name; \
	*pv2 = reg.reg.f2; \
	*pv3 = reg.reg.f3; \
	*pv4 = reg.reg.f4; \
	*pv5 = reg.reg.f5; \
}

#define reg32_read_field_6(pdd, reg_name, reg_start, field_1_name, pfield_1_value, f2, pv2, f3, pv3, f4, pv4, f5, pv5, f6, pv6) \
{ \
	reg_name reg; \
	reg.uint = disp_read_u32(pdd, reg_start); \
	*pfield_1_value = reg.reg.field_1_name; \
	*pv2 = reg.reg.f2; \
	*pv3 = reg.reg.f3; \
	*pv4 = reg.reg.f4; \
	*pv5 = reg.reg.f5; \
	*pv6 = reg.reg.f6; \
}

#define reg32_read_field_7(pdd, reg_name, reg_start, field_1_name, pfield_1_value, f2, pv2, f3, pv3, f4, pv4, f5, pv5, f6, pv6, f7, pv7) \
{ \
	reg_name reg; \
	reg.uint = disp_read_u32(pdd, reg_start); \
	*pfield_1_value = reg.reg.field_1_name; \
	*pv2 = reg.reg.f2; \
	*pv3 = reg.reg.f3; \
	*pv4 = reg.reg.f4; \
	*pv5 = reg.reg.f5; \
	*pv6 = reg.reg.f6; \
	*pv7 = reg.reg.f7; \
}

#define reg32_read_field_8(pdd, reg_name, reg_start, field_1_name, pfield_1_value, f2, pv2, f3, pv3, f4, pv4, f5, pv5, f6, pv6, f7, pv7, f8, pv8) \
{ \
	reg_name reg; \
	reg.uint = disp_read_u32(pdd, reg_start); \
	*pfield_1_value = reg.reg.field_1_name; \
	*pv2 = reg.reg.f2; \
	*pv3 = reg.reg.f3; \
	*pv4 = reg.reg.f4; \
	*pv5 = reg.reg.f5; \
	*pv6 = reg.reg.f6; \
	*pv7 = reg.reg.f7; \
	*pv8 = reg.reg.f8; \
}

// fix macro expansion order problem between WIN & Linux
#define Expand(...) __VA_ARGS__

#define reg32_write_field(reg_name, reg_start, num, ...) \
	Expand(reg32_write_field_##num(pdd, reg_name, reg_start, __VA_ARGS__))

#define reg32_read_field(reg_name, reg_start, num, ...) \
	Expand(reg32_read_field_##num(pdd, reg_name, reg_start, __VA_ARGS__))

#define crtc_write_field(crtc_index, reg_name, reg_start, num, ...) \
{ \
    reg32_write_field(reg_name, (reg_start * 4 + pdd->reg_off.plns[crtc_index]), num, __VA_ARGS__) \
}

#define crtc_read_field(crtc_index, reg_name, reg_start, num, ...) \
{ \
    reg32_read_field(reg_name, (reg_start * 4 + pdd->reg_off.plns[crtc_index]), num, __VA_ARGS__) \
}

#define dp_write_field(dp_index, reg_name, reg_start, num, ...) \
{ \
    reg32_write_field(reg_name, (reg_start * 4 + pdd->reg_off.dp[dp_index]), num, __VA_ARGS__) \
}

#define dp_read_field(dp_index, reg_name, reg_start, num, ...) \
{ \
    reg32_read_field(reg_name, (reg_start * 4 + pdd->reg_off.dp[dp_index]), num, __VA_ARGS__) \
}

#define dsc_write_field(dsc_index, reg_name, reg_start, num, ...) \
{ \
    reg32_write_field(reg_name, (reg_start * 4 + pdd->reg_off.dsc[dsc_index]), num, __VA_ARGS__) \
}

#define dsc_read_field(dsc_index, reg_name, reg_start, num, ...) \
{ \
    reg32_read_field(reg_name, (reg_start * 4 + pdd->reg_off.dsc[dsc_index]), num, __VA_ARGS__) \
}

#define reg8_write_field_1(pdd, reg_name, reg_start, field_1_name, field_1_value) \
{ \
	reg_name reg; \
	reg.uchar = disp_read_u8(pdd, reg_start); \
	reg.reg.field_1_name = field_1_value; \
	disp_write_u8(pdd, reg_start, reg.uchar); \
}

#define reg8_write_field_2(pdd, reg_name, reg_start, field_1_name, field_1_value, f2, v2) \
{ \
	reg_name reg; \
	reg.uchar = disp_read_u8(pdd, reg_start); \
	reg.reg.field_1_name = field_1_value; \
	reg.reg.f2 = v2; \
	disp_write_u8(pdd, reg_start, reg.uchar); \
}

#define reg8_write_field_3(pdd, reg_name, reg_start, field_1_name, field_1_value, f2, v2, f3, v3) \
{ \
	reg_name reg; \
	reg.uchar = disp_read_u8(pdd, reg_start); \
	reg.reg.field_1_name = field_1_value; \
	reg.reg.f2 = v2; \
	reg.reg.f3 = v3; \
	disp_write_u8(pdd, reg_start, reg.uchar); \
}

#define reg8_write_field_4(pdd, reg_name, reg_start, field_1_name, field_1_value, f2, v2, f3, v3, f4, v4) \
{ \
	reg_name reg; \
	reg.uchar = disp_read_u8(pdd, reg_start); \
	reg.reg.field_1_name = field_1_value; \
	reg.reg.f2 = v2; \
	reg.reg.f3 = v3; \
	reg.reg.f4 = v4; \
	disp_write_u8(pdd, reg_start, reg.uchar); \
}

#define reg8_write_field_5(pdd, reg_name, reg_start, field_1_name, field_1_value, f2, v2, f3, v3, f4, v4, f5, v5) \
{ \
	reg_name reg; \
	reg.uchar = disp_read_u8(pdd, reg_start); \
	reg.reg.field_1_name = field_1_value; \
	reg.reg.f2 = v2; \
	reg.reg.f3 = v3; \
	reg.reg.f4 = v4; \
	reg.reg.f5 = v5; \
	disp_write_u8(pdd, reg_start, reg.uchar); \
}

#define reg8_write_field_6(pdd, reg_name, reg_start, field_1_name, field_1_value, f2, v2, f3, v3, f4, v4, f5, v5, f6, v6) \
{ \
	reg_name reg; \
	reg.uchar = disp_read_u8(pdd, reg_start); \
	reg.reg.field_1_name = field_1_value; \
	reg.reg.f2 = v2; \
	reg.reg.f3 = v3; \
	reg.reg.f4 = v4; \
	reg.reg.f5 = v5; \
	reg.reg.f6 = v6; \
	disp_write_u8(pdd, reg_start, reg.uchar); \
}

#define reg8_write_field_7(pdd, reg_name, reg_start, field_1_name, field_1_value, f2, v2, f3, v3, f4, v4, f5, v5, f6, v6, f7, v7) \
{ \
	reg_name reg; \
	reg.uchar = disp_read_u8(pdd, reg_start); \
	reg.reg.field_1_name = field_1_value; \
	reg.reg.f2 = v2; \
	reg.reg.f3 = v3; \
	reg.reg.f4 = v4; \
	reg.reg.f5 = v5; \
	reg.reg.f6 = v6; \
	reg.reg.f7 = v7; \
	disp_write_u8(pdd, reg_start, reg.uchar); \
}

#define reg8_write_field_8(pdd, reg_name, reg_start, field_1_name, field_1_value, f2, v2, f3, v3, f4, v4, f5, v5, f6, v6, f7, v7, f8, v8) \
{ \
	reg_name reg; \
	reg.uchar = disp_read_u8(pdd, reg_start); \
	reg.reg.field_1_name = field_1_value; \
	reg.reg.f2 = v2; \
	reg.reg.f3 = v3; \
	reg.reg.f4 = v4; \
	reg.reg.f5 = v5; \
	reg.reg.f6 = v6; \
	reg.reg.f7 = v7; \
	reg.reg.f8 = v8; \
	disp_write_u8(pdd, reg_start, reg.uchar); \
}

#define reg8_read_field_1(pdd, reg_name, reg_start, field_1_name, pfield_1_value) \
{ \
	reg_name reg; \
	reg.uchar = disp_read_u8(pdd, reg_start); \
	*pfield_1_value = reg.reg.field_1_name; \
}

#define reg8_read_field_2(pdd, reg_name, reg_start, field_1_name, pfield_1_value, f2, pv2) \
{ \
	reg_name reg; \
	reg.uchar = disp_read_u8(pdd, reg_start); \
	*pfield_1_value = reg.reg.field_1_name; \
	*pv2 = reg.reg.f2; \
}

#define reg8_read_field_3(pdd, reg_name, reg_start, field_1_name, pfield_1_value, f2, pv2, f3, pv3) \
{ \
	reg_name reg; \
	reg.uchar = disp_read_u8(pdd, reg_start); \
	*pfield_1_value = reg.reg.field_1_name; \
	*pv2 = reg.reg.f2; \
	*pv3 = reg.reg.f3; \
}

#define reg8_read_field_4(pdd, reg_name, reg_start, field_1_name, pfield_1_value, f2, pv2, f3, pv3, f4, pv4) \
{ \
	reg_name reg; \
	reg.uchar = disp_read_u8(pdd, reg_start); \
	*pfield_1_value = reg.reg.field_1_name; \
	*pv2 = reg.reg.f2; \
	*pv3 = reg.reg.f3; \
	*pv4 = reg.reg.f4; \
}

#define reg8_read_field_5(pdd, reg_name, reg_start, field_1_name, pfield_1_value, f2, pv2, f3, pv3, f4, pv4, f5, pv5) \
{ \
	reg_name reg; \
	reg.uchar = disp_read_u8(pdd, reg_start); \
	*pfield_1_value = reg.reg.field_1_name; \
	*pv2 = reg.reg.f2; \
	*pv3 = reg.reg.f3; \
	*pv4 = reg.reg.f4; \
	*pv5 = reg.reg.f5; \
}

#define reg8_read_field_6(pdd, reg_name, reg_start, field_1_name, pfield_1_value, f2, pv2, f3, pv3, f4, pv4, f5, pv5, f6, pv6) \
{ \
	reg_name reg; \
	reg.uchar = disp_read_u8(pdd, reg_start); \
	*pfield_1_value = reg.reg.field_1_name; \
	*pv2 = reg.reg.f2; \
	*pv3 = reg.reg.f3; \
	*pv4 = reg.reg.f4; \
	*pv5 = reg.reg.f5; \
	*pv6 = reg.reg.f6; \
}

#define reg8_read_field_7(pdd, reg_name, reg_start, field_1_name, pfield_1_value, f2, pv2, f3, pv3, f4, pv4, f5, pv5, f6, pv6, f7, pv7) \
{ \
	reg_name reg; \
	reg.uchar = disp_read_u8(pdd, reg_start); \
	*pfield_1_value = reg.reg.field_1_name; \
	*pv2 = reg.reg.f2; \
	*pv3 = reg.reg.f3; \
	*pv4 = reg.reg.f4; \
	*pv5 = reg.reg.f5; \
	*pv6 = reg.reg.f6; \
	*pv7 = reg.reg.f7; \
}

#define reg8_read_field_8(pdd, reg_name, reg_start, field_1_name, pfield_1_value, f2, pv2, f3, pv3, f4, pv4, f5, pv5, f6, pv6, f7, pv7, f8, pv8) \
{ \
	reg_name reg; \
	reg.uchar = disp_read_u8(pdd, reg_start); \
	*pfield_1_value = reg.reg.field_1_name; \
	*pv2 = reg.reg.f2; \
	*pv3 = reg.reg.f3; \
	*pv4 = reg.reg.f4; \
	*pv5 = reg.reg.f5; \
	*pv6 = reg.reg.f6; \
	*pv7 = reg.reg.f7; \
	*pv8 = reg.reg.f8; \
}

#define reg8_write(reg_name, reg_start, value) \
	disp_write_u8(pdd, reg_start, value)

#define reg8_read(reg_name, reg_start) \
	return disp_read_u8(pdd, reg_start)

#define reg8_write_field(reg_name, reg_start, num, ...) \
	Expand(reg8_write_field_##num(pdd, reg_name, reg_start, __VA_ARGS__))

#define reg8_read_field(reg_name, reg_start, num, ...) \
	Expand(reg8_read_field_##num(pdd, reg_name, reg_start, __VA_ARGS__))


#define sr_write_field(pipe_index, reg_name, reg_start, num, ...) \
{ \
    Expand(reg8_write_field_##num(pdd, reg_name, pdd->reg_off.sr[pipe_index] + reg_start, __VA_ARGS__)); \
}

#define cr_write_field(pipe_index, reg_name, reg_start, num, ...) \
{ \
    Expand(reg8_write_field_##num(pdd, reg_name, pdd->reg_off.cr[pipe_index] + reg_start, __VA_ARGS__)); \
}

#define sr_share_write_field(reg_name, reg_start, num, ...) \
{ \
	Expand(reg8_write_field_##num(pdd, reg_name, pdd->reg_off.sr_share + reg_start, __VA_ARGS__)); \
}

#define cr_share_write_field(reg_name, reg_start, num, ...) \
{ \
	Expand(reg8_write_field_##num(pdd, reg_name, pdd->reg_off.cr_share + reg_start, __VA_ARGS__)); \
}

#define sr_read_field(pipe_index, reg_name, reg_start, num, ...) \
{ \
    Expand(reg8_read_field_##num(pdd, reg_name, pdd->reg_off.sr[pipe_index] + reg_start, __VA_ARGS__)); \
}

#define cr_read_field(pipe_index, reg_name, reg_start, num, ...) \
{ \
    Expand(reg8_read_field_##num(pdd, reg_name, pdd->reg_off.cr[pipe_index] + reg_start, __VA_ARGS__)); \
}

#define sr_share_read_field(reg_name, reg_start, num, ...) \
{ \
	Expand(reg8_read_field_##num(pdd, reg_name, pdd->reg_off.sr_share + reg_start, __VA_ARGS__)); \
}

#define cr_share_read_field(reg_name, reg_start, num, ...) \
{ \
	Expand(reg8_read_field_##num(pdd, reg_name, pdd->reg_off.cr_share + reg_start, __VA_ARGS__)); \
}

#endif
