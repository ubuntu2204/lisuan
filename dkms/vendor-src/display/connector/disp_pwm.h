/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#ifndef __DISP_PWM_H__
#define __DISP_PWM_H__

#include "disp_device.h"

disp_void_t disp_get_backlight_pwm_state(pdisp_device_t pdd,
    pdisp_u8_t pduty_en, pdisp_u32_t pduty, pdisp_u8_t pfreq_en, pdisp_u32_t pfreq);
disp_void_t disp_set_backlight_pwm_set_duty(pdisp_device_t pdd, disp_u32_t duty);
disp_void_t disp_set_backlight_pwm_enable_duty(pdisp_device_t pdd, disp_u8_t enable);
disp_void_t disp_set_backlight_pwm_set_freq(pdisp_device_t pdd, disp_u32_t freq);
disp_void_t disp_set_backlight_pwm_enable_freq(pdisp_device_t pdd, disp_u8_t enable);

#endif