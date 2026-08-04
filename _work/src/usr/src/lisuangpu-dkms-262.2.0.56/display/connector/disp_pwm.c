/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#include "disp_device.h"
#include "disp_reg.h"
#include "PWM_register.h"

#define pwm_write_field(reg_name, reg_start, num, ...) \
{ \
    reg32_write_field(reg_name, (reg_start * 4 + pdd->reg_off.pwm), num, __VA_ARGS__) \
}

#define pwm_read_field(reg_name, reg_start, num, ...) \
{ \
    reg32_read_field(reg_name, (reg_start * 4 + pdd->reg_off.pwm), num, __VA_ARGS__) \
}

#define disp_print(level, log, ...) disp_print_ex(DISP_PRINT_MASK_PWM, level, log, ##__VA_ARGS__)

disp_void_t disp_get_backlight_pwm_state(pdisp_device_t pdd,
    pdisp_u8_t pduty_en, pdisp_u32_t pduty, pdisp_u8_t pfreq_en, pdisp_u32_t pfreq)
{
    disp_u32_t duty, freq, duty_en, freq_en;

    pwm_read_field(Reg_Pwm0_Mmio_0, Register_Pwm0_Mmio_0_Start, 2,
        Pwm0_Duty, &duty,
        Pwm0_En, &duty_en);
    pwm_read_field(Reg_Pwm0_Mmio_1, Register_Pwm0_Mmio_1_Start, 2,
        Pwm0_Freq, &freq,
        En_Pwm_Out, &freq_en);

    if (pduty_en)   *pduty_en   = (disp_u8_t)duty_en;
    if (pduty)      *pduty      = duty;
    if (pfreq_en)   *pfreq_en   = (disp_u8_t)freq_en;
    if (pfreq)      *pfreq      = freq;
}

disp_void_t disp_set_backlight_pwm_set_duty(pdisp_device_t pdd, disp_u32_t duty)
{
    pwm_write_field(Reg_Pwm0_Mmio_0, Register_Pwm0_Mmio_0_Start, 1,
        Pwm0_Duty, duty);
}

disp_void_t disp_set_backlight_pwm_enable_duty(pdisp_device_t pdd, disp_u8_t enable)
{
    pwm_write_field(Reg_Pwm0_Mmio_0, Register_Pwm0_Mmio_0_Start, 1,
        Pwm0_En, enable);
}

disp_void_t disp_set_backlight_pwm_set_freq(pdisp_device_t pdd, disp_u32_t freq)
{
    pwm_write_field(Reg_Pwm0_Mmio_1, Register_Pwm0_Mmio_1_Start, 1,
        Pwm0_Freq, freq);
}

disp_void_t disp_set_backlight_pwm_enable_freq(pdisp_device_t pdd, disp_u8_t enable)
{
    pwm_write_field(Reg_Pwm0_Mmio_1, Register_Pwm0_Mmio_1_Start, 1,
        En_Pwm_Out, enable);
}
