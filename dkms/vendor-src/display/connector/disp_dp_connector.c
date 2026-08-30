/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#include "disp_dp_connector.h"
#include "disp_device.h"
#include "disp_dp_aux.h"
#include "disp_dp_phy.h"
#include "disp_reg.h"
#include "DP_register.h"
#include "disp_pwm.h"

#define DISP_DP_RETRY_I2CAUX_TIMES 16
#define DP_I2C_STOP -1
#define DP_I2C_REPEAT -2
#define disp_print(level, log, ...) disp_print_ex(DISP_PRINT_MASK_CONNECTOR_DP, level, log, ##__VA_ARGS__)
#define disp_print_addition_file(level, log, ...) \
{ \
    disp_print_ex(DISP_PRINT_MASK_CONNECTOR_DP, level, log, ##__VA_ARGS__); \
    disp_print_to_file_ex(pdd, DISP_PRINT_MASK_CONNECTOR_DP, level, log, ##__VA_ARGS__); \
}

static disp_u8_t disp_dp_is_edp(pdisp_connector_t pdisp_connector)
{
    /* How to identify eDP:
     *
     * DPCD: (unreliable)
     * 0x00D: eDP_CONFIGURATION_CAP, always 0 for external DP.
        bit0: ALTERNATE_SCRAMBLER_RESET_CAPABLE, eDP alternate scrambler reset value of FFFEh
        bit3: DPCD_DISPLAY_CONTROL_CAPABLE, dpcd 0x700 ~ 0x7ff enable
     *
     * Linux code:
     * intel_dp_is_port_edp: check VBT or specific vendor/device ID
     */
    disp_print(DDM_TRACE, "disp_dp_is_edp, default FALSE, pdisp_connector[%#x]\n", pdisp_connector);
    return DISP_FALSE;
}

// backlight
static disp_u8_t disp_edp_support_backlight(pdisp_connector_t pdisp_connector)
{
    pdisp_edp_info_t pedp = (pdisp_edp_info_t)pdisp_connector->disp_info.edp_info;

    if (!pedp)
        return DISP_FALSE;

    return (pedp->support_backlight_pwm_pin || pedp->support_tcon_backlight_adjustment) ? DISP_TRUE : DISP_FALSE;
}

static disp_void_t disp_edp_backlight_dump(pdisp_connector_t pdisp_connector)
{
    pdisp_edp_info_t pedp = (pdisp_edp_info_t)pdisp_connector->disp_info.edp_info;
    struct disp_edp_backlight_info *bl = &pedp->backlight;

    disp_print(DDM_INFO, "backlight caps: pwm_pin_support[%u], pwm_aux_support[%u]\n",
        pedp->support_backlight_pwm_pin, pedp->support_tcon_backlight_adjustment);
    disp_print(DDM_INFO, "\ten: aux[%u] axu_pwm[%u] pwm_duty[%u] pwm_freq[%u] aux_freq[%u] dbc[%u] regional[%u]\n",
        bl->aux_enable, bl->aux_pwm_enable, bl->pwm_duty_enable, bl->pwm_freq_enable, bl->aux_freq_enable, bl->dbc_enable, bl->regional_enable);
    disp_print(DDM_INFO, "\ten: smooth[%u] nits[%u], prefer: aux[%u] dbc[%u] regional[%u]\n",
        pedp->backlight.smooth_enable, pedp->backlight.nits_enable, bl->prefer_mode, bl->prefer_dbc, bl->prefer_regional);
    disp_print(DDM_INFO, "\tprefer: Pn_min[%u], Pn_max[%u], Pn_cur[%u], FP_min[%u], FP_max[%u]\n",
        bl->min_pn, bl->max_pn, bl->pwmgen_bit_count, bl->min_freq_divider, bl->max_freq_divider);
    disp_print(DDM_INFO, "\tdbc_cur: min[%u], max[%u]\n", bl->dbc_min_set, bl->dbc_max_set);
    disp_print(DDM_INFO, "\tregional: x[%u], y[%u]\n", bl->regional_x, bl->regional_y);
}

static DISP_STATUS disp_edp_backlight_init(pdisp_connector_t pdisp_connector)
{
    pdisp_edp_info_t pedp = (pdisp_edp_info_t)pdisp_connector->disp_info.edp_info;
    pdisp_dp_sink_caps_t psink_caps = (pdisp_dp_sink_caps_t)pdisp_connector->disp_info.dp_sink_caps;

    // prefer policy
    pedp->backlight.smooth_enable = 0;
    pedp->backlight.nits_enable = 0;
    pedp->backlight.prefer_mode = DP_EDP_BACKLIGHT_CONTROL_MODE_PWM;
    pedp->backlight.prefer_dbc = DISP_FALSE;
    pedp->backlight.prefer_regional = DISP_FALSE;

    // get specific info from vbt
    if (pedp->support_backlight_pwm_pin)
    {
        pedp->backlight.pwm_duty_enable = 1;
        pedp->backlight.pwm_freq_enable = 1;
        //pedp->backlight.prefer_freq_divider // Backlight Frequency = 27MHz / (F × P)
    }

    // get specific info from dpcd
    pedp->support_tcon_backlight_adjustment = disp_edp_backlight_supported(psink_caps->edp_dpcd);
    if (pedp->support_tcon_backlight_adjustment)
        disp_edp_backlight_aux_get_caps((struct disp_dp_aux *)pdisp_connector->ddc, &pedp->backlight, psink_caps->edp_dpcd);
    if (pedp->support_tcon_backlight_adjustment && !pedp->backlight.aux_enable)
    {
        disp_print(DDM_WARNING, "disp_edp_backlight_init, backlight[%u] support but invalid aux[%u]!\n",
            pedp->support_tcon_backlight_adjustment, pedp->backlight.aux_enable);
        pedp->support_tcon_backlight_adjustment = 0; // invalid, disable backlight
    }

    // decide mode: aux/pwm/product
    if (pedp->support_backlight_pwm_pin && pedp->support_tcon_backlight_adjustment)
    {
        if (pedp->backlight.aux_enable && DP_EDP_BACKLIGHT_CONTROL_MODE_DPCD == pedp->backlight.prefer_mode)
            pedp->backlight.set_mode = DP_EDP_BACKLIGHT_CONTROL_MODE_DPCD;
        else if (pedp->backlight.aux_pwm_enable && DP_EDP_BACKLIGHT_CONTROL_MODE_PRODUCT == pedp->backlight.prefer_mode)
            pedp->backlight.set_mode = DP_EDP_BACKLIGHT_CONTROL_MODE_PRODUCT;
        else // aux not possible or prefer pwm -> pwm mode
            pedp->backlight.set_mode = DP_EDP_BACKLIGHT_CONTROL_MODE_PWM;
    }
    else if (pedp->support_backlight_pwm_pin) // only pwm possible
        pedp->backlight.set_mode = DP_EDP_BACKLIGHT_CONTROL_MODE_PWM;
    else if (pedp->support_tcon_backlight_adjustment) // only aux enable
        pedp->backlight.set_mode = DP_EDP_BACKLIGHT_CONTROL_MODE_DPCD;
    else // pwm & aux both not support
        disp_print(DDM_WARNING, "disp_edp_backlight_init, backlight not possible!\n");

    /* desire freq:
     * Visual persistence: so it is good to set freq as higher as possible.
     * inductive howling: human could perceived 20Hz ~ 20kHz.
     * It's almost impossible to set above 20kHz, so it could not set too high.
     * Normally set to 200Hz.
     */
    if (!pedp->backlight.max_freq_divider)
        pedp->backlight.max_freq_divider = 0x3ffff; // at least 103Hz
    if (!pedp->backlight.min_freq_divider)
        pedp->backlight.min_freq_divider = 1; // at least 27MHz

    return DISP_OK;
}

static DISP_STATUS disp_edp_backlight_get_caps(pdisp_connector_t pdisp_connector, disp_backlight_query_parma_t* ppram)
{
    pdisp_edp_info_t pedp = (pdisp_edp_info_t)pdisp_connector->disp_info.edp_info;

    if (disp_edp_support_backlight(pdisp_connector))
    {
        ppram->caps.level = 1;
        ppram->caps.freq = (pedp->backlight.pwm_freq_enable || pedp->backlight.aux_freq_enable) ? 1 : 0;
        ppram->caps.smooth = pedp->backlight.smooth_enable;
        ppram->caps.adaptive = pedp->backlight.dbc_enable;
        ppram->caps.nits = pedp->backlight.nits_enable;
    }
    else
    {
        ppram->caps.level = 0;
    }

    return DISP_OK;
}

static DISP_STATUS disp_edp_backlight_set_level(pdisp_connector_t pdisp_connector,
    DISP_BACKLIGHT_LEVEL_TYPE level_type, disp_u32_t level, disp_u8_t enable)
{
    pdisp_edp_info_t pedp = (pdisp_edp_info_t)pdisp_connector->disp_info.edp_info;
    disp_u32_t duty, aux_base;

    if (!disp_edp_support_backlight(pdisp_connector))
        return DISP_ENSUPPORT;
    if ((enable && level_type != DISP_BACKLIGHT_LEVEL_THOUSANDS_PERCENT)
        || (enable && level > DISP_BACKLIGHT_PERCENT_VALUE_MAX))
    {
        disp_print(DDM_WARNING, "disp_edp_backlight_set_level invalid param! pedp[%llx] enable[%u] level_type[%d] level[%u]\n",
            pedp, level_type, level);
        return DISP_EINVAL;
    }

    switch (pedp->backlight.set_mode)
    {
    case DP_EDP_BACKLIGHT_CONTROL_MODE_PWM:
        if (enable)
        {
            duty = (disp_u32_t)((disp_u64_t)level * DISP_BACKLIGHT_PWM_DUTY_MAX / DISP_BACKLIGHT_PERCENT_VALUE_MAX);
            disp_set_backlight_pwm_set_duty(pdisp_connector->disp_device, duty);
        }
        disp_set_backlight_pwm_enable_duty(pdisp_connector->disp_device, enable);
        break;
    case DP_EDP_BACKLIGHT_CONTROL_MODE_DPCD:
        if (enable)
        {
            aux_base = pedp->backlight.lsb_reg_used ? DISP_BACKLIGHT_AUX_LSB_MAX : DISP_BACKLIGHT_AUX_MSB_MAX;
            pedp->backlight.cur_level = (disp_u16_t)((disp_u64_t)level * aux_base / DISP_BACKLIGHT_PERCENT_VALUE_MAX);
            disp_edp_backlight_aux_set_level(pdisp_connector->ddc, &pedp->backlight);
        }
        pedp->backlight.set_aux_backlight = enable;
        disp_edp_backlight_aux_set_mode(pdisp_connector->ddc, &pedp->backlight);
        disp_edp_backlight_aux_enable(pdisp_connector->ddc, &pedp->backlight);
        break;
    case DP_EDP_BACKLIGHT_CONTROL_MODE_PRODUCT:
        /* level_percent = pwm_level_percent * aux_level_percent;
         * policy: set pwm 100%, only adjust aux
         */
        if (enable)
        {
            aux_base = pedp->backlight.lsb_reg_used ? DISP_BACKLIGHT_AUX_LSB_MAX : DISP_BACKLIGHT_AUX_MSB_MAX;
            pedp->backlight.cur_level = (disp_u16_t)((disp_u64_t)level * aux_base / DISP_BACKLIGHT_PERCENT_VALUE_MAX);
            disp_set_backlight_pwm_set_duty(pdisp_connector->disp_device, DISP_BACKLIGHT_PWM_DUTY_MAX);
            disp_edp_backlight_aux_set_level(pdisp_connector->ddc, &pedp->backlight);
        }
        pedp->backlight.set_aux_backlight = enable;
        disp_set_backlight_pwm_enable_duty(pdisp_connector->disp_device, enable);
        disp_edp_backlight_aux_set_mode(pdisp_connector->ddc, &pedp->backlight);
        disp_edp_backlight_aux_enable(pdisp_connector->ddc, &pedp->backlight);
        break;
    default:
        disp_print(DDM_WARNING, "disp_edp_backlight_set_level invalid mode[%d]!\n", pedp->backlight.set_mode);
        return DISP_ESRCH;
    }

    

    return DISP_OK;
}

static DISP_STATUS disp_edp_backlight_get_level(pdisp_connector_t pdisp_connector,
    DISP_BACKLIGHT_LEVEL_TYPE level_type, pdisp_u32_t plevel, pdisp_u8_t penable)
{
    pdisp_edp_info_t pedp = (pdisp_edp_info_t)pdisp_connector->disp_info.edp_info;
    disp_u8_t duty_en;
    disp_u32_t duty, aux_base;

    if (!disp_edp_support_backlight(pdisp_connector))
        return DISP_ENSUPPORT;
    if (level_type != DISP_BACKLIGHT_LEVEL_THOUSANDS_PERCENT)
    {
        disp_print(DDM_WARNING, "disp_edp_backlight_get_level invalid param! pedp[%llx], level_type[%d]\n", pedp, level_type);
        return DISP_EINVAL;
    }

    switch (pedp->backlight.set_mode)
    {
    case DP_EDP_BACKLIGHT_CONTROL_MODE_PWM:
        disp_get_backlight_pwm_state(pdisp_connector->disp_device, &duty_en, &duty, DISP_NULL, DISP_NULL);
        if (penable) *penable = duty_en;
        if (plevel)  *plevel  = (disp_u32_t)((disp_u64_t)duty * DISP_BACKLIGHT_PERCENT_VALUE_MAX / DISP_BACKLIGHT_PWM_DUTY_MAX);
        break;
    case DP_EDP_BACKLIGHT_CONTROL_MODE_DPCD:
        disp_edp_backlight_aux_get_brightness(pdisp_connector->ddc, &pedp->backlight);
        aux_base = pedp->backlight.lsb_reg_used ? DISP_BACKLIGHT_AUX_LSB_MAX : DISP_BACKLIGHT_AUX_MSB_MAX;
        if (penable) *penable = pedp->backlight.set_mode;
        if (plevel)  *plevel  = (disp_u32_t)((disp_u64_t)pedp->backlight.cur_level * DISP_BACKLIGHT_PERCENT_VALUE_MAX / aux_base);
        break;
    case DP_EDP_BACKLIGHT_CONTROL_MODE_PRODUCT:
        disp_get_backlight_pwm_state(pdisp_connector->disp_device, &duty_en, &duty, DISP_NULL, DISP_NULL);
        disp_edp_backlight_aux_get_brightness(pdisp_connector->ddc, &pedp->backlight);

        if (!duty_en || pedp->backlight.set_mode != DP_EDP_BACKLIGHT_CONTROL_MODE_PRODUCT)
        {
            disp_print(DDM_WARNING, "disp_edp_backlight_get_level, expect product_mode, not match: duty_en[%u] mode[%u]!\n",
                duty_en, pedp->backlight.set_mode);
        }
        aux_base = pedp->backlight.lsb_reg_used ? DISP_BACKLIGHT_AUX_LSB_MAX : DISP_BACKLIGHT_AUX_MSB_MAX;
        if (penable) *penable = (duty_en || pedp->backlight.set_mode);
        if (plevel)  *plevel  = (disp_u32_t)((disp_u64_t)duty * pedp->backlight.cur_level *
            DISP_BACKLIGHT_PERCENT_VALUE_MAX / aux_base / DISP_BACKLIGHT_PWM_DUTY_MAX);
        break;
    default:
        disp_print(DDM_WARNING, "disp_edp_backlight_get_level invalid mode[%d]!\n", pedp->backlight.set_mode);
        break;
    }

    return DISP_OK;
}

static DISP_STATUS disp_edp_backlight_set_freq(pdisp_connector_t pdisp_connector, disp_u8_t enable, disp_u32_t desire_freq_hz)
{
    pdisp_edp_info_t pedp = (pdisp_edp_info_t)pdisp_connector->disp_info.edp_info;
    pdisp_dp_sink_caps_t psink_caps = (pdisp_dp_sink_caps_t)pdisp_connector->disp_info.dp_sink_caps;
    disp_u32_t freq_divider;

    if (!disp_edp_support_backlight(pdisp_connector))
        return DISP_ENSUPPORT;

    freq_divider = (disp_u32_t)((disp_u64_t)DP_EDP_BACKLIGHT_FREQ_BASE_KHZ * 1000 / desire_freq_hz);
    if (freq_divider < pedp->backlight.min_freq_divider || freq_divider > pedp->backlight.max_freq_divider)
    {
        disp_print(DDM_WARNING, "disp_edp_backlight_set_freq, freq check failed! desire_freq_hz[%u], min_divider[%u], max_divider[%u]\n",
            desire_freq_hz, pedp->backlight.min_freq_divider, pedp->backlight.max_freq_divider);
        return DISP_EINVAL;
    }

    if (pedp->support_backlight_pwm_pin) // prefer use pwm freq
    {
        if (enable)
            disp_set_backlight_pwm_set_freq(pdisp_connector->disp_device, freq_divider);
        disp_set_backlight_pwm_enable_freq(pdisp_connector->disp_device, enable ? DISP_TRUE : DISP_FALSE);
    }
    else if (pedp->backlight.aux_freq_enable) // use aux control
    {
        if (enable) // drm policy: check Pn and make sure within 25 percent desire freq 
            disp_edp_backlight_aux_probe_freq_within_25percent_desire(pdisp_connector->ddc, &pedp->backlight, desire_freq_hz, psink_caps->edp_dpcd);
        pedp->backlight.set_freq_aux = enable;
        disp_edp_backlight_aux_enable_freq(pdisp_connector->ddc, &pedp->backlight);
    }
    else
    {
        disp_print(DDM_WARNING, "disp_edp_backlight_set_freq, both pwm & aux not support!\n");
        return DISP_ENSUPPORT;
    }

    return DISP_OK;
}

static DISP_STATUS disp_edp_backlight_get_freq(pdisp_connector_t pdisp_connector, pdisp_u8_t penable, pdisp_u32_t pfreq_hz)
{
    pdisp_edp_info_t pedp = (pdisp_edp_info_t)pdisp_connector->disp_info.edp_info;
    disp_u32_t freq_divider, freq_p, freq_f;
    disp_u8_t freq_en;

    if (!disp_edp_support_backlight(pdisp_connector))
        return DISP_ENSUPPORT;

    if (pedp->support_backlight_pwm_pin) // prefer use pwm freq
    {
        disp_get_backlight_pwm_state(pdisp_connector->disp_device, DISP_NULL, DISP_NULL, &freq_en, &freq_divider);
        if (penable)  *penable  = freq_en ? DISP_TRUE : DISP_FALSE;
        if (pfreq_hz) *pfreq_hz = (disp_u32_t)((disp_u64_t)DP_EDP_BACKLIGHT_FREQ_BASE_KHZ * 1000 / freq_divider);
    }
    else if (pedp->backlight.aux_freq_enable) // use aux control
    {
        disp_edp_backlight_aux_get_freq(pdisp_connector->ddc, &pedp->backlight);
        freq_p = 1 << pedp->backlight.pwmgen_bit_count;
        freq_f = pedp->backlight.pwm_freq_pre_divider ? pedp->backlight.pwm_freq_pre_divider : 1;
        freq_divider = freq_p * freq_f;
        if (penable)  *penable  = pedp->backlight.set_freq_aux ? DISP_TRUE : DISP_FALSE;
        if (pfreq_hz) *pfreq_hz = (disp_u32_t)((disp_u64_t)DP_EDP_BACKLIGHT_FREQ_BASE_KHZ * 1000 / freq_divider);
    }
    else
    {
        disp_print(DDM_WARNING, "disp_edp_backlight_get_freq, both pwm & aux not support!\n");
        return DISP_ENSUPPORT;
    }

    return DISP_OK;
}

static DISP_STATUS disp_edp_backlight_set_adaptive(pdisp_connector_t pdisp_connector,
    disp_u8_t enable, disp_u32_t min, disp_u32_t max)
{
    pdisp_edp_info_t pedp = (pdisp_edp_info_t)pdisp_connector->disp_info.edp_info;

    if (!disp_edp_support_backlight(pdisp_connector)
        || !pedp->backlight.dbc_enable)
        return DISP_ENSUPPORT;

    if (enable && (max > DISP_BACKLIGHT_PERCENT_VALUE_MAX || min > max))
    {
        disp_print(DDM_WARNING, "disp_edp_backlight_set_adaptive invalid min[%x] max[%x]\n", min, max);
        return DISP_EINVAL;
    }

    if (enable)
    {
        pedp->backlight.dbc_min_set = (disp_u8_t)(min * DISP_BACKLIGHT_ADAPTIVE_VALUE_MAX / DISP_BACKLIGHT_PERCENT_VALUE_MAX);
        pedp->backlight.dbc_max_set = (disp_u8_t)(max * DISP_BACKLIGHT_ADAPTIVE_VALUE_MAX / DISP_BACKLIGHT_PERCENT_VALUE_MAX);
        disp_edp_backlight_aux_set_adaptive(pdisp_connector->ddc, &pedp->backlight);
    }
    pedp->backlight.set_dbc = enable ? DISP_TRUE : DISP_FALSE;
    disp_edp_backlight_aux_enable_adaptive(pdisp_connector->ddc, &pedp->backlight);

    return DISP_OK;
}

static DISP_STATUS disp_edp_backlight_get_adaptive(pdisp_connector_t pdisp_connector,
    pdisp_u8_t penable, pdisp_u32_t pmin, pdisp_u32_t pmax)
{
    pdisp_edp_info_t pedp = (pdisp_edp_info_t)pdisp_connector->disp_info.edp_info;

    if (!disp_edp_support_backlight(pdisp_connector)
        || !pedp->backlight.dbc_enable)
        return DISP_ENSUPPORT;

    disp_edp_backlight_aux_get_adaptive(pdisp_connector->ddc, &pedp->backlight);
    *penable = pedp->backlight.set_dbc;
    *pmin = (disp_u32_t)pedp->backlight.dbc_min_set * 50; // %5 -> 0.1%
    *pmax = (disp_u32_t)pedp->backlight.dbc_max_set * 50;

    return DISP_OK;
}

static DISP_STATUS disp_edp_backlight_query(pdisp_connector_t pdisp_connector, pdisp_backlight_query_t pquery)
{
    if (!disp_edp_support_backlight(pdisp_connector) && pquery->type != DISP_BACKLIGHT_QUERY_CAPS)
    {
        disp_print(DDM_WARNING, "disp_edp_backlight_query not support!\n");
        return DISP_ENSUPPORT;
    }

    switch(pquery->type)
    {
    case DISP_BACKLIGHT_QUERY_CAPS:
        return disp_edp_backlight_get_caps(pdisp_connector, &pquery->param);
    case DISP_BACKLIGHT_QUERY_LEVLE:
        return disp_edp_backlight_get_level(pdisp_connector, pquery->param.level.type, &pquery->param.level.level_val, &pquery->param.level.enable);
    case DISP_BACKLIGHT_QUERY_FREQ:
        return disp_edp_backlight_get_freq(pdisp_connector, &pquery->param.freq.enable, &pquery->param.freq.freq_val);
    case DISP_BACKLIGHT_QUERY_ADAPTIVE:
        if (DISP_BACKLIGHT_LEVEL_THOUSANDS_PERCENT == pquery->param.adaptive.type)
        {
            return disp_edp_backlight_get_adaptive(pdisp_connector,
                &pquery->param.adaptive.enable,
                &pquery->param.adaptive.adaptive_min_level,
                &pquery->param.adaptive.adaptive_max_level);
        }
        else
        {
            disp_print(DDM_WARNING, "disp_backlight_query, invalid adaptive level type[%d]\n", pquery->param.adaptive.type);
            return -DISP_EINVAL;
        }
        break;
    default:
        break;
    }

    disp_print(DDM_WARNING, "disp_backlight_query, invalid type[%d]\n", pquery->type);
    return -DISP_EINVAL;
}

static DISP_STATUS disp_edp_backlight_set(pdisp_connector_t pdisp_connector, pdisp_backlight_set_t pset)
{
    DISP_STATUS ret = DISP_OK;

    if (!disp_edp_support_backlight(pdisp_connector))
    {
        disp_print(DDM_WARNING, "disp_edp_backlight_set not support!\n");
        return DISP_ENSUPPORT;
    }

    if (pset->flag.disable_level)
        ret |=  disp_edp_backlight_set_level(pdisp_connector, 0, 0, DISP_FALSE);
    else if (pset->flag.set_level)
        ret |=  disp_edp_backlight_set_level(pdisp_connector, pset->level_type, pset->level_val, DISP_TRUE);

    if (pset->flag.disable_freq)
        ret |= disp_edp_backlight_set_freq(pdisp_connector, DISP_FALSE, 0);
    else if (pset->flag.set_freq)
        ret |= disp_edp_backlight_set_freq(pdisp_connector, DISP_TRUE, pset->freq_val);

    // set adaptive
    if (pset->flag.disable_adaptive)
        ret |= disp_edp_backlight_set_adaptive(pdisp_connector, DISP_FALSE, 0, 0);
    else if (pset->flag.set_adaptive)
        ret |= disp_edp_backlight_set_adaptive(pdisp_connector, DISP_TRUE, pset->adaptive_min_level, pset->adaptive_min_level);

    if (pset->flag.set_smooth)
    {
        // how to set smooth ?
    }

    return ret;
}

// PSR
static DISP_STATUS disp_edp_psr_init(pdisp_connector_t pdisp_connector)
{
    pdisp_device_t pdd = pdisp_connector->disp_device;
    pdisp_encoder_t pdisp_encoder = &pdd->disp_encoder[pdisp_connector->index];
    pdisp_edp_info_t pedp = (pdisp_edp_info_t)pdisp_connector->disp_info.edp_info;
    pdisp_dp_sink_caps_t psink_caps = (pdisp_dp_sink_caps_t)pdisp_connector->disp_info.dp_sink_caps;
    disp_u8_t byte_read = 0, byte_read_1 = 0;
    disp_u32_t psr_revision=0, psr_length=0;

    // prefer policy
    pedp->psr.prefer_frame_start_vsync = 0; // default vden
    pedp->psr.prefer_crc_on = 0; // default no crc
    pedp->psr.prefer_main_link_remain = 0; // default main link off
    pedp->psr.prefer_sw_sdp = 0; // default hw auto psr sdp

    pedp->psr.set_su_y = 0; // G100 not support su
    pedp->psr.set_crc_err_irq = pedp->psr.prefer_crc_on ? 1 : 0; // If enable crc, set aux to enable irq

    // get specific info from dpcd
    byte_read = psink_caps->psr_dpcd[DP_PSR_SUPPORT - DP_PSR_SUPPORT];
    if (byte_read != DP_PSR_IS_SUPPORTED && byte_read != DP_PSR2_IS_SUPPORTED && byte_read != DP_PSR2_WITH_Y_COORD_IS_SUPPORTED)
    {
        pedp->support_psr = 0;
        pedp->psr.version = 0;
    }
    else
    {
        pedp->support_psr = 1;
        pedp->psr.version = byte_read;

        byte_read = psink_caps->psr_dpcd[DP_PSR_CAPS - DP_PSR_SUPPORT];
        pedp->psr.caps_exit_no_lt = (byte_read & DP_PSR_NO_TRAIN_ON_EXIT) ? 1 : 0;
        pedp->psr.setup_time_us = disp_dp_psr_setup_time(psink_caps->psr_dpcd);
        pedp->psr.caps_su_y = (byte_read & DP_PSR2_SU_Y_COORDINATE_REQUIRED) ? 1 : 0;
        pedp->psr.coord_granularity = (byte_read & DP_PSR2_SU_GRANULARITY_REQUIRED) ? 1 : 0;

        byte_read = psink_caps->psr_dpcd[DP_PSR2_SU_X_GRANULARITY - DP_PSR_SUPPORT];
        byte_read_1 = psink_caps->psr_dpcd[DP_PSR2_SU_X_GRANULARITY + 1 - DP_PSR_SUPPORT];
        pedp->psr.su_x_granularity = (disp_u16_t)byte_read | ((disp_u16_t)byte_read_1 << 8);

        byte_read = psink_caps->psr_dpcd[DP_PSR2_SU_Y_GRANULARITY - DP_PSR_SUPPORT];
        byte_read_1 = psink_caps->psr_dpcd[DP_PSR2_SU_Y_GRANULARITY + 1 - DP_PSR_SUPPORT];
        pedp->psr.su_y_granularity = (disp_u16_t)byte_read | ((disp_u16_t)byte_read_1 << 8);
    }

    if (pedp->psr.prefer_sw_sdp)
    {   // Disable HW auto psr vsc
        dp_write_field(pdisp_encoder->index, Reg_Dpt_Reg0e, Register_Dpt_Reg0e_Start, 3,
            Vsc_Revision, 0,
            Vsc_Valid_Num, 0,
            Vsc_Db0, 0);
    }
    else
    {
        /* G100 HW auto psr sdp only valid first 8 byte vsc payload, the follow byte tie 0.
         * So not support colorimetry, psr2 could choose, but su must not enable.
         */
        switch(pedp->psr.version)
        {
        case 3:
        case 2:
            psr_revision = DP_SDP_VSC_TYPE_3D_PSR2;
            psr_length = DP_SDP_VSC_LENGTH_3D_PSR2;
            break;
        case 1:
        default:
            psr_revision = DP_SDP_VSC_TYPE_3D_PSR;
            psr_length = DP_SDP_VSC_LENGTH_3D_PSR;
            break;
        }
        dp_write_field(pdisp_encoder->index, Reg_Dpt_Reg0e, Register_Dpt_Reg0e_Start, 3,
            Vsc_Revision, psr_revision,
            Vsc_Valid_Num, psr_length,
            Vsc_Db0, 0); // Assert no 3D
    }

    return DISP_OK;
}

static disp_void_t disp_edp_psr_dump(pdisp_connector_t pdisp_connector)
{
    pdisp_edp_info_t pedp = (pdisp_edp_info_t)pdisp_connector->disp_info.edp_info;
    struct disp_edp_psr_info *psr = &pedp->psr;

    if (!pedp->support_psr || !psr->version)
        disp_print(DDM_INFO, "psr not support");
    else
    {
        disp_print(DDM_INFO, "psr caps: ver[%u] setup_time_us[%d], exit_no_lt[%u] su_need_coord[%u]granularity[%u][%u %u], ",
            psr->version, psr->setup_time_us, psr->caps_exit_no_lt, psr->caps_su_y,
            psr->coord_granularity, psr->su_x_granularity, psr->su_y_granularity);
    }
}

static disp_u8_t disp_edp_psr_get_version(pdisp_connector_t pdisp_connector)
{
    pdisp_edp_info_t pedp = (pdisp_edp_info_t)pdisp_connector->disp_info.edp_info;

    if (!pedp || !pedp->support_psr)
        return 0;

    return pedp->psr.version;
}

static DISP_STATUS disp_edp_psr_query(pdisp_connector_t pdisp_connector, pdisp_psr_query_t pquery)
{
    if (!pdisp_connector || !pquery)
    {
        disp_print(DDM_WARNING, "disp_edp_psr_query, param NULL! pdisp_connector[%llx], pquery[%llx]\n", pdisp_connector, pquery);
        return DISP_EINVAL;
    }

    pquery->psr_version = disp_edp_psr_get_version(pdisp_connector);
    return DISP_OK;
}

static disp_void_t disp_edp_psr_prepare(pdisp_connector_t pdisp_connector)
{
    pdisp_device_t pdd = pdisp_connector->disp_device;
    pdisp_edp_info_t pedp = (pdisp_edp_info_t)pdisp_connector->disp_info.edp_info;
    pdisp_crtc_t pdisp_crtc = &pdd->disp_crtc[pdisp_connector->index];
    pdisp_display_mode_t phw_mode = &pdisp_crtc->property.hw_mode;
    disp_u64_t frame_n_cmp_left=0, frame_n_cmp_right=0;
    disp_u32_t setup_time_us=0, sdp_to_active_pixel=0;

    /* If sdp_to_active_time >= setup_time, frame N(Psr_Enter_Mode set 0). Otherwise, frame N+1.
     * sdp_to_active_us >= setup_time_us
     * fresh_rate = clk_khz * 1000 / htotal / vtotal
     * ==> 1000*1000/fresh_rate * sdp_to_active_pixel/vtotal >= setup_time_us
     * ==> htotal * sdp_to_active_pixel * 1000 >= clk_khz * setup_time_us
     */

    setup_time_us = pedp->psr.setup_time_us;
    if (pedp->psr.prefer_frame_start_vsync)
        sdp_to_active_pixel = phw_mode->vtotal - phw_mode->vsync_start; // vsync + vbp
    else
        sdp_to_active_pixel = phw_mode->vtotal - phw_mode->vdisplay; // vfp + vsync + vbp

    frame_n_cmp_left = (disp_u64_t)phw_mode->htotal * sdp_to_active_pixel * 1000;
    frame_n_cmp_right = (disp_u64_t)phw_mode->clock * setup_time_us;
    pedp->psr.set_frame_next = (frame_n_cmp_left >= frame_n_cmp_right) ? 0 : 1;
}

static DISP_STATUS disp_edp_psr_set(pdisp_connector_t pdisp_connector, pdisp_psr_set_t pset)
{
    pdisp_device_t pdd = DISP_NULL;
    pdisp_encoder_t pencoder = DISP_NULL;
    pdisp_edp_info_t pedp = DISP_NULL;
    DISP_STATUS ret = DISP_OK;

    if (!pdisp_connector || !pset)
    {
        disp_print(DDM_WARNING, "disp_edp_psr_set, param NULL! pdisp_connector[%llx], pset[%llx]\n", pdisp_connector, pset);
        return DISP_EINVAL;
    }
    if (!disp_edp_psr_get_version(pdisp_connector))
    {
        disp_print(DDM_WARNING, "disp_edp_psr_set, psr not support!\n");
        return -DISP_ENREADY;
    }
    if (pencoder->state.power_state != DISP_POWER_ON)
    {
        disp_print(DDM_WARNING, "disp_edp_psr_set, encoder not power on yet!\n");
        return -DISP_ENREADY;
    }

    pdd = pdisp_connector->disp_device;
    pencoder = &pdd->disp_encoder[pdisp_connector->index];
    pedp = (pdisp_edp_info_t)pdisp_connector->disp_info.edp_info;

    if (!pencoder || !pencoder->func || !pencoder->func->edp_psr_set)
    {
        disp_print(DDM_WARNING, "disp_edp_psr_set, func missing!\n");
        return DISP_ENSUPPORT;
    }

    if (DISP_PSR_SLEEP == pset->psr_type)
    {
        // TODO: HW config for power off

        ret = disp_dp_dpcd_writeb(pdisp_connector->ddc, DP_SET_POWER, DP_SET_POWER_D3_AUX_ON); // or DP_SET_POWER_D3
        if(ret < 0)
        {
            disp_print(DDM_ERROR, "dpcd set power off fail\n");
        }
    }

    if (DISP_PSR_ENTRY == pset->psr_type)
    {
        // Prepare parameter for set aux & set HW
        disp_edp_psr_prepare(pdisp_connector);

        // set aux before set HW
        pedp->psr.set_active = 1;
        if (!disp_edp_psr_aux_is_enable(pdisp_connector->ddc))
            disp_edp_psr_aux_set(pdisp_connector->ddc, &pedp->psr);
    }

    // set HW
    ret = pencoder->func->edp_psr_set(pencoder, pset->psr_type);
    if (ret)
        return ret;

    return ret;
}

static disp_void_t disp_update_edp_info(pdisp_connector_t pdisp_connector)
{
    // backlight init
    disp_edp_backlight_init(pdisp_connector);
    disp_edp_backlight_dump(pdisp_connector);

    // psr init
    disp_edp_psr_init(pdisp_connector);
    disp_edp_psr_dump(pdisp_connector);
}

static disp_u32_t disp_dp_read_dsc_caps(pdisp_connector_t pdisp_connector)
{
    struct disp_dp_aux *paux = (struct disp_dp_aux *)pdisp_connector->ddc;
    pdisp_dp_sink_caps_t psink_caps = (pdisp_dp_sink_caps_t)pdisp_connector->disp_info.dp_sink_caps;
    disp_s32_t ret = 0;

    /* DSC limit DP1.4, no dpcd limit. */
    //if(psink_caps->dpcd[DP_DPCD_REV] >= 0x14 || psink_caps->edp_dpcd[0] >= DP_EDP_14)
    {
        ret = disp_dp_dpcd_read(paux, DP_DSC_SUPPORT, psink_caps->dsc_dpcd, sizeof(psink_caps->dsc_dpcd));
        if(ret < 0)
        {
            disp_print(DDM_WARNING, "Failed to read DP_DSC_SUPPORT DPCD register\n");
        }

        /* FEC is supported only on DP 1.4 */
        if(!disp_dp_is_edp(pdisp_connector))
        {
            ret = disp_dp_dpcd_read(paux, DP_FEC_CAPABILITY, &psink_caps->fec_capable, 1);
            if(ret < 0)
            {
                disp_print(DDM_WARNING, "Failed to read FEC DPCD register\n");
            }
        }
    }

    return ret;
}

static disp_u32_t disp_dp_read_edp_caps(pdisp_connector_t pdisp_connector)
{
    struct disp_dp_aux *paux = (struct disp_dp_aux *)pdisp_connector->ddc;
    pdisp_dp_sink_caps_t psink_caps = (pdisp_dp_sink_caps_t)pdisp_connector->disp_info.dp_sink_caps;

    return disp_dp_dpcd_read(paux, DP_EDP_DPCD_REV, psink_caps->edp_dpcd, sizeof(psink_caps->edp_dpcd));
}

static disp_u32_t disp_dp_read_psr_caps(pdisp_connector_t pdisp_connector)
{
    struct disp_dp_aux *paux = (struct disp_dp_aux *)pdisp_connector->ddc;
    pdisp_dp_sink_caps_t psink_caps = (pdisp_dp_sink_caps_t)pdisp_connector->disp_info.dp_sink_caps;

    return disp_dp_dpcd_read(paux, DP_PSR_SUPPORT, psink_caps->psr_dpcd, sizeof(psink_caps->psr_dpcd));
}

static DISP_STATUS dp_get_sink_caps(pdisp_connector_t pdisp_connector, disp_u8_t is_retry)
{
    struct disp_dp_aux *paux = (struct disp_dp_aux *)pdisp_connector->ddc;
    pdisp_dp_sink_caps_t pcaps = (pdisp_dp_sink_caps_t)pdisp_connector->disp_info.dp_sink_caps;
    pdisp_device_t pdd = (pdisp_device_t)pdisp_connector->disp_device;
    disp_u32_t log_level = DDM_TRACE;

    if (!is_retry)
    {
        disp_memset(pcaps, 0, sizeof(disp_dp_sink_caps_t));

        disp_dp_read_dpcd_caps(paux, pcaps->dpcd);

        disp_dp_read_downstream_info(pdisp_connector->ddc, pcaps->dpcd, pcaps->downstream_ports);
        if(pdd->flag.info.dp_cts)
        {
            disp_dp_read_sink_status(pdisp_connector, pcaps->sink_status);
        }
        disp_dp_read_dsc_caps(pdisp_connector);

        if(disp_dp_is_edp(pdisp_connector))
        {
            disp_dp_read_edp_caps(pdisp_connector);
            disp_dp_read_psr_caps(pdisp_connector);
        }
    }
    else
    {
        if (pdisp_connector->flag.info.dpcd_get_failed)
            disp_dp_read_dpcd_caps(paux, pcaps->dpcd);

        if (pdisp_connector->flag.info.dpcd_get_dsc_failed)
            disp_dp_read_dsc_caps(pdisp_connector);
    }

    pdisp_connector->flag.info.dpcd_get_failed = (!pcaps->dpcd[0] || !pcaps->dpcd[1] || !pcaps->dpcd[2]) ? 1 : 0;
    pdisp_connector->flag.info.dpcd_get_dsc_failed = (!pcaps->fec_capable || !pcaps->dsc_dpcd[0]) ? 1 : 0;

    disp_print(log_level, "dpcd: %x %x %x %x, %x %x %x %x, %x %x %x %x, %x %x %x %x\n",
        pcaps->dpcd[0], pcaps->dpcd[1], pcaps->dpcd[2], pcaps->dpcd[3],
        pcaps->dpcd[4], pcaps->dpcd[5], pcaps->dpcd[6], pcaps->dpcd[7],
        pcaps->dpcd[8], pcaps->dpcd[9], pcaps->dpcd[10], pcaps->dpcd[11],
        pcaps->dpcd[12], pcaps->dpcd[13], pcaps->dpcd[14], pcaps->dpcd[15]);
    disp_print(log_level, "downstream_ports: %x %x %x %x, %x %x %x %x, %x %x %x %x, %x %x %x %x\n",
        pcaps->downstream_ports[0], pcaps->downstream_ports[1], pcaps->downstream_ports[2], pcaps->downstream_ports[3],
        pcaps->downstream_ports[4], pcaps->downstream_ports[5], pcaps->downstream_ports[6], pcaps->downstream_ports[7],
        pcaps->downstream_ports[8], pcaps->downstream_ports[9], pcaps->downstream_ports[10], pcaps->downstream_ports[11],
        pcaps->downstream_ports[12], pcaps->downstream_ports[13], pcaps->downstream_ports[14], pcaps->downstream_ports[15]);
    disp_print(log_level, "sink_status: %x %x %x %x %x\n",
        pcaps->sink_status[0], pcaps->sink_status[1], pcaps->sink_status[2], pcaps->sink_status[3], pcaps->sink_status[4]);
    disp_print(log_level, "fec: %x\n", pcaps->fec_capable);
    disp_print(log_level, "dsc: %x %x %x %x, %x %x %x %x, %x %x %x %x, %x %x %x %x\n",
        pcaps->dsc_dpcd[0], pcaps->dsc_dpcd[1], pcaps->dsc_dpcd[2], pcaps->dsc_dpcd[3], pcaps->dsc_dpcd[4], pcaps->dsc_dpcd[5],
        pcaps->dsc_dpcd[6], pcaps->dsc_dpcd[7], pcaps->dsc_dpcd[8], pcaps->dsc_dpcd[9], pcaps->dsc_dpcd[10],
        pcaps->dsc_dpcd[11], pcaps->dsc_dpcd[12], pcaps->dsc_dpcd[13], pcaps->dsc_dpcd[14], pcaps->dsc_dpcd[15]);

    return DISP_OK;
}

static disp_u32_t dp_get_throughput_mode_mps(disp_u8_t dpcd_throughput_mode)
{
    if(0 == dpcd_throughput_mode || dpcd_throughput_mode >= 16)
    {
        disp_print(DDM_WARNING, "warning, dpcd_throughput_mode[%#x] invalid\n", dpcd_throughput_mode);
        return 0;
    }

    switch(dpcd_throughput_mode)
    {
        case 1:
            return 340;
        case 15:
            return 170;
        default:
            return (disp_u32_t)dpcd_throughput_mode * 50 + 300;
    }
}

static disp_void_t dp_update_dsc_caps(pdisp_connector_t pdisp_connector)
{
    pdisp_dp_sink_caps_t pcaps = (pdisp_dp_sink_caps_t)pdisp_connector->disp_info.dp_sink_caps;
    pdisp_dsc_caps_t pdsc_caps = &pdisp_connector->disp_info.dsc_sink_caps;
    disp_u8_t rc_buf_blk_bits = 0, index = 0;
    disp_u32_t rc_buf_size_kb = 0;

    disp_memset(pdsc_caps, 0, sizeof(disp_dsc_caps_t));
    pdsc_caps->flag.bits.fec_caps = disp_dp_sink_supports_fec(pcaps->fec_capable);
    pdsc_caps->flag.bits.dsc_caps = disp_dp_sink_supports_dsc(pcaps->dsc_dpcd);
    pdsc_caps->flag.bits.dp = 1;

    if (1 != (pcaps->dsc_dpcd[DP_DSC_REV - DP_DSC_SUPPORT] & DP_DSC_MAJOR_MASK))
    {
        disp_print(DDM_INFO, "dsc not support dpcd[%#x]\n", pcaps->dsc_dpcd[DP_DSC_REV - DP_DSC_SUPPORT]);
        pdsc_caps->flag.bits.dsc_caps = 0;
        return;
    }
    if (1 == ((pcaps->dsc_dpcd[DP_DSC_REV - DP_DSC_SUPPORT] & DP_DSC_MINOR_MASK) >> DP_DSC_MINOR_SHIFT))
        pdsc_caps->flag.bits.dsc_ver_1_1 = 1;
    else if (2 == ((pcaps->dsc_dpcd[DP_DSC_REV - DP_DSC_SUPPORT] & DP_DSC_MINOR_MASK) >> DP_DSC_MINOR_SHIFT))
        pdsc_caps->flag.bits.dsc_ver_1_2 = 1;

    rc_buf_blk_bits = (pcaps->dsc_dpcd[DP_DSC_RC_BUF_BLK_SIZE - DP_DSC_SUPPORT] & 0x3);
    rc_buf_size_kb = pcaps->dsc_dpcd[DP_DSC_RC_BUF_SIZE - DP_DSC_SUPPORT] + 1;
    for (index = 0; index < rc_buf_blk_bits; ++index) // blk: 0/1/2/3 -> 1/4/16/64 KB
    {
        rc_buf_size_kb *= 4;
    }
    pdsc_caps->min_rc_buf_size_kb = (disp_u16_t)rc_buf_size_kb;

    if (pcaps->dsc_dpcd[DP_DSC_SLICE_CAP_1 - DP_DSC_SUPPORT] & DP_DSC_1_PER_DP_DSC_SINK)
        pdsc_caps->flag.bits.slice_num_1 = 1;
    if (pcaps->dsc_dpcd[DP_DSC_SLICE_CAP_1 - DP_DSC_SUPPORT] & DP_DSC_2_PER_DP_DSC_SINK)
        pdsc_caps->flag.bits.slice_num_2 = 1;
    if (pcaps->dsc_dpcd[DP_DSC_SLICE_CAP_1 - DP_DSC_SUPPORT] & DP_DSC_4_PER_DP_DSC_SINK)
        pdsc_caps->flag.bits.slice_num_4 = 1;
    if (pcaps->dsc_dpcd[DP_DSC_SLICE_CAP_1 - DP_DSC_SUPPORT] & DP_DSC_6_PER_DP_DSC_SINK)
        pdsc_caps->flag.bits.slice_num_6 = 1;
    if (pcaps->dsc_dpcd[DP_DSC_SLICE_CAP_1 - DP_DSC_SUPPORT] & DP_DSC_8_PER_DP_DSC_SINK)
        pdsc_caps->flag.bits.slice_num_8 = 1;
    if (pcaps->dsc_dpcd[DP_DSC_SLICE_CAP_1 - DP_DSC_SUPPORT] & DP_DSC_10_PER_DP_DSC_SINK)
        pdsc_caps->flag.bits.slice_num_10 = 1;
    if (pcaps->dsc_dpcd[DP_DSC_SLICE_CAP_1 - DP_DSC_SUPPORT] & DP_DSC_12_PER_DP_DSC_SINK)
        pdsc_caps->flag.bits.slice_num_12 = 1;
    if (pcaps->dsc_dpcd[DP_DSC_SLICE_CAP_2 - DP_DSC_SUPPORT] & DP_DSC_16_PER_DP_DSC_SINK)
        pdsc_caps->flag.bits.slice_num_16 = 1;
    if (pcaps->dsc_dpcd[DP_DSC_SLICE_CAP_2 - DP_DSC_SUPPORT] & DP_DSC_20_PER_DP_DSC_SINK)
        pdsc_caps->flag.bits.slice_num_20 = 1;
    if (pcaps->dsc_dpcd[DP_DSC_SLICE_CAP_2 - DP_DSC_SUPPORT] & DP_DSC_24_PER_DP_DSC_SINK)
        pdsc_caps->flag.bits.slice_num_24 = 1;

    pdsc_caps->max_linebuf_depth = disp_dp_dsc_sink_line_buf_depth(pcaps->dsc_dpcd);
    pdsc_caps->flag.bits.block_prediction =
        (pcaps->dsc_dpcd[DP_DSC_BLK_PREDICTION_SUPPORT - DP_DSC_SUPPORT] & DP_DSC_BLK_PREDICTION_IS_SUPPORTED);

    // DP_DSC_MAX_BITS_PER_PIXEL_LOW & HI, max bpp DP2.1 should support, not use yet

    pdsc_caps->flag.bits.rgb = (pcaps->dsc_dpcd[DP_DSC_DEC_COLOR_FORMAT_CAP - DP_DSC_SUPPORT] & DP_DSC_RGB) ? 1 : 0;
    pdsc_caps->flag.bits.yuv_444 = (pcaps->dsc_dpcd[DP_DSC_DEC_COLOR_FORMAT_CAP - DP_DSC_SUPPORT] & DP_DSC_YCbCr444) ? 1 : 0;
    pdsc_caps->flag.bits.yuv_simple422 = (pcaps->dsc_dpcd[DP_DSC_DEC_COLOR_FORMAT_CAP - DP_DSC_SUPPORT] & DP_DSC_YCbCr422_Simple) ? 1 : 0;
    pdsc_caps->flag.bits.yuv_native422 = (pcaps->dsc_dpcd[DP_DSC_DEC_COLOR_FORMAT_CAP - DP_DSC_SUPPORT] & DP_DSC_YCbCr422_Native) ? 1 : 0;
    pdsc_caps->flag.bits.yuv_native420 = (pcaps->dsc_dpcd[DP_DSC_DEC_COLOR_FORMAT_CAP - DP_DSC_SUPPORT] & DP_DSC_YCbCr420_Native) ? 1 : 0;

    pdsc_caps->flag.bits.bpc8 = (pcaps->dsc_dpcd[DP_DSC_DEC_COLOR_DEPTH_CAP - DP_DSC_SUPPORT] & DP_DSC_8_BPC) ? 1 : 0;
    pdsc_caps->flag.bits.bpc10 = (pcaps->dsc_dpcd[DP_DSC_DEC_COLOR_DEPTH_CAP - DP_DSC_SUPPORT] & DP_DSC_10_BPC) ? 1 : 0;
    pdsc_caps->flag.bits.bpc12 = (pcaps->dsc_dpcd[DP_DSC_DEC_COLOR_DEPTH_CAP - DP_DSC_SUPPORT] & DP_DSC_12_BPC) ? 1 : 0;

    pdsc_caps->max_throughput_mode0_mps = dp_get_throughput_mode_mps(
        pcaps->dsc_dpcd[DP_DSC_PEAK_THROUGHPUT - DP_DSC_SUPPORT] & DP_DSC_THROUGHPUT_MODE_0_MASK);
    pdsc_caps->max_throughput_mode1_mps = dp_get_throughput_mode_mps(
        (pcaps->dsc_dpcd[DP_DSC_PEAK_THROUGHPUT - DP_DSC_SUPPORT] & DP_DSC_THROUGHPUT_MODE_1_MASK) >> DP_DSC_THROUGHPUT_MODE_1_SHIFT);

    pdsc_caps->max_slice_width = disp_dp_dsc_sink_max_slice_width(pcaps->dsc_dpcd);
    pdsc_caps->min_slice_height = 108;
    pdsc_caps->min_increment_of_bpp = disp_dp_dsc_sink_min_increment_bpp(pcaps->dsc_dpcd);
}

disp_s32_t disp_dp_set_sink_dsc(pdisp_connector_t pdisp_connector, disp_u8_t enable)
{
    struct disp_dp_aux *paux = (struct disp_dp_aux *)pdisp_connector->ddc;
    disp_s32_t ret = 0;

    ret = disp_dp_dpcd_writeb(paux, DP_DSC_ENABLE, enable ? DP_DECOMPRESSION_EN : 0);
    if(ret < 0)
    {
        disp_print(DDM_ERROR, "dpcd set dsc fail\n");
    }

    return ret;
}

disp_s32_t disp_dp_set_sink_fec(pdisp_connector_t pdisp_connector, disp_u8_t enable)
{
    struct disp_dp_aux *paux = (struct disp_dp_aux *)pdisp_connector->ddc;
    disp_s32_t ret = 0;

    ret = disp_dp_dpcd_writeb(paux, DP_FEC_CONFIGURATION, enable ? DP_FEC_READY : 0);
    if(ret < 0)
    {
        disp_print(DDM_ERROR, "dpcd set fec_ready fail\n");
    }

    return ret;
}

disp_s32_t disp_dp_set_main_link_coding(pdisp_connector_t pdisp_connector)
{
    struct disp_dp_aux *paux = (struct disp_dp_aux *)pdisp_connector->ddc;
    disp_s32_t ret = 0;

    ret = disp_dp_dpcd_writeb(paux, DP_MAIN_LINK_CHANNEL_CODING_SET, DP_SET_ANSI_8B10B);

    if(ret < 0)
    {
        disp_print(DDM_ERROR, "dpcd set DP_MAIN_LINK_CHANNEL_CODING_SET fail\n");
    }

    return ret;
}

disp_s32_t disp_dp_set_sink_power_state(pdisp_connector_t pdisp_connector, disp_u8_t enable)
{
    struct disp_dp_aux *paux = (struct disp_dp_aux *)pdisp_connector->ddc;
    pdisp_device_t pdd = pdisp_connector->disp_device;
    disp_s32_t ret = 0;

    ret = disp_dp_dpcd_writeb(paux, DP_SET_POWER, enable ? DP_SET_POWER_D0 : DP_SET_POWER_D3);

    pdisp_connector->flag.info.dpcd_set_power_failed = (ret < 0) ? 1 : 0;
    if(ret < 0)
    {
        disp_print_addition_file(DDM_ERROR, "%u dpcd set DP_SET_POWER fail\n", pdisp_connector->index);
    }

    return ret;
}

disp_void_t disp_dp_set_sink_ssc(pdisp_connector_t pdisp_connector, disp_u8_t ssc_on)
{
    struct disp_dp_aux *paux = (struct disp_dp_aux *)pdisp_connector->ddc;
    disp_s32_t ret;
    disp_u8_t reg_value = 0;

    ret = disp_dp_dpcd_read(paux, DP_DOWNSPREAD_CTRL, &reg_value, 1);
    if (ret < 0)
    {
        disp_print(DDM_ERROR, "disp_dp_set_sink_ssc read failed!\n");
    }

    if (ssc_on)
        reg_value |= DP_SPREAD_AMP_0_5;
    else
        reg_value &= ~DP_SPREAD_AMP_0_5;

    ret = disp_dp_dpcd_writeb(paux, DP_DOWNSPREAD_CTRL, reg_value);
    if(ret < 0)
    {
        disp_print(DDM_ERROR, "disp_dp_set_sink_ssc write failed! reg_value[%x]\n", reg_value);
    }
}

static DISP_HPD_TYPE disp_dp_get_hpd_status(pdisp_connector_t pdisp_connector)
{
    pdisp_device_t pdd = pdisp_connector->disp_device;
    disp_u32_t hpd_status = 0;

    dp_read_field(pdisp_connector->index, Reg_Dpt_Reg29, Register_Dpt_Reg29_Start, 1,
        Hpd_Sts_O, &hpd_status);

    switch(hpd_status)
    {
        case DPT_REG29_HPD_STS_O_HPD_IDLE:
            return DISP_HPD_TYPE_IDLE;
        case DPT_REG29_HPD_STS_O_HPD_PLUG_IN:
            return DISP_HPD_TYPE_PLUG_IN;
        case DPT_REG29_HPD_STS_O_HPD_EVENT:
            return DISP_HPD_TYPE_EVENT;
        case DPT_REG29_HPD_STS_O_PHD_PLUGOUT:
            return DISP_HPD_TYPE_PLUG_OUT;
        default:
            break;
    }

    disp_print(DDM_WARNING, "disp_dp_get_hpd_status, invalid hpd_status[%u]\n", hpd_status);
    return DISP_HPD_TYPE_IDLE;
}

static DISP_STATUS disp_dp_get_i2c_status(pdisp_connector_t pdisp_connector, pdisp_get_i2c_status_t pdisp_i2c_status)
{
    struct disp_dp_aux *pdisp_dp_aux = (struct disp_dp_aux *)pdisp_connector->ddc;

    pdisp_i2c_status->i2c_nack_count = pdisp_dp_aux->i2c_nack_count;
    pdisp_i2c_status->i2c_defer_count = pdisp_dp_aux->i2c_defer_count;

    return DISP_OK;
}

disp_void_t dp_free_sync_set_sink(pdisp_connector_t pdisp_connector, disp_u8_t enable)
{
    struct disp_dp_aux *paux = (struct disp_dp_aux *)pdisp_connector->ddc;
    disp_s32_t ret;
    disp_u8_t reg_value = 0;

    ret = disp_dp_dpcd_read(paux, DP_DOWNSPREAD_CTRL, &reg_value, 1);
    if (ret < 0)
    {
        disp_print(DDM_ERROR, "dp_free_sync_set_sink read failed!\n");
    }

    if (enable)
        reg_value |= DP_MSA_TIMING_PAR_IGNORE_EN;
    else
        reg_value &= ~DP_MSA_TIMING_PAR_IGNORE_EN;

    ret = disp_dp_dpcd_writeb(paux, DP_DOWNSPREAD_CTRL, reg_value);
    if(ret < 0)
    {
        disp_print(DDM_ERROR, "dp_free_sync_set_sink write failed! reg_value[%x]\n", reg_value);
    }
}

static DISP_STATUS dp_update_disp_info_from_dpcd(pdisp_connector_t pdisp_connector)
{
    pdisp_device_t pdd = pdisp_connector->disp_device;
    pdisp_encoder_t pencoder = &pdd->disp_encoder[pdisp_connector->index];
    pdisp_dp_sink_caps_t pcaps = (pdisp_dp_sink_caps_t)pdisp_connector->disp_info.dp_sink_caps;
    disp_u8_t idx = 0, temp = 0;

    dp_update_dsc_caps(pdisp_connector);
    if (disp_dp_is_edp(pdisp_connector))
    {
        disp_update_edp_info(pdisp_connector);
    }

    pencoder->policy.custom.dp.max_possible_link_rate =
        pcaps->dpcd[DP_MAX_LINK_RATE] > pencoder->caps.priv.dp.source_max_link_rate ?
        pencoder->caps.priv.dp.source_max_link_rate : pcaps->dpcd[DP_MAX_LINK_RATE];
    pencoder->policy.custom.dp.max_possible_lane_count =
        disp_dp_max_lane_count(pcaps->dpcd) > pencoder->caps.priv.dp.source_max_lane_count ?
        pencoder->caps.priv.dp.source_max_lane_count : disp_dp_max_lane_count(pcaps->dpcd);
    pencoder->policy.custom.dp.max_possible_bandwidth_kbyte = (disp_u32_t)27000 *
        pencoder->policy.custom.dp.max_possible_link_rate *
        pencoder->policy.custom.dp.max_possible_lane_count;

    pencoder->policy.flag.bits.free_sync_sink_support = disp_dp_sink_can_do_video_without_timing_msa(pcaps->dpcd) ? DISP_TRUE : DISP_FALSE;
    /* 0x2207 0x2214 may need check, as DP v2.1 */
    disp_print(DDM_TRACE, "free sync dpcd_support[%u]\n", pencoder->policy.flag.bits.free_sync_sink_support);

    if ((pcaps->dpcd[DP_DOWNSTREAMPORT_PRESENT] & DP_DWN_STRM_PORT_PRESENT) &&
        (pcaps->dpcd[DP_DOWN_STREAM_PORT_COUNT] & DP_PORT_COUNT_MASK))
    {
        pencoder->state.priv.dp.flag.bits.dfp_branch_device = 1;
        for (idx = 0; idx < 4; idx++)
        {
            pencoder->state.priv.dp.dfp[idx].type = pcaps->downstream_ports[idx * 4] & DP_DS_PORT_TYPE_MASK;
            pencoder->state.priv.dp.dfp[idx].hpd_aware = (pcaps->downstream_ports[idx * 4] & DP_DS_PORT_HPD) ? 1 : 0;
            switch (pencoder->state.priv.dp.dfp[idx].type)
            {
            case DP_DS_PORT_TYPE_HDMI:
                temp = pcaps->downstream_ports[idx * 4 + 1];
                pencoder->state.priv.dp.dfp[idx].max_tdms_clk_khz = (disp_s32_t)temp * 2500;
                temp = pcaps->downstream_ports[idx * 4 + 2] & DP_DS_MAX_BPC_MASK;
                pencoder->state.priv.dp.dfp[idx].max_bpc = (3 == temp) ? 16 : temp * 2 + 8; // 0-8, 1-10, 2-12, 3-16
                pencoder->state.priv.dp.dfp[idx].pixel_cap = pcaps->downstream_ports[idx * 4 + 3];
                break;
            case DP_DS_PORT_TYPE_DP:
            case DP_DS_PORT_TYPE_VGA:
            case DP_DS_PORT_TYPE_DVI:
            case DP_DS_PORT_TYPE_NON_EDID:
            case DP_DS_PORT_TYPE_DP_DUALMODE:
            case DP_DS_PORT_TYPE_WIRELESS:
            default:
                // TODO: handle dfp type
                break;
            }
        }
        disp_print(DDM_INFO, "dp branch device, dfp_type/hpd_aware/bpc/clk [%x %u %u %d] [%x %u %u %d] [%x %u %u %d] [%x %u %u %d]\n",
            pencoder->state.priv.dp.dfp[0].type, pencoder->state.priv.dp.dfp[0].hpd_aware, pencoder->state.priv.dp.dfp[0].max_bpc, pencoder->state.priv.dp.dfp[0].max_tdms_clk_khz,
            pencoder->state.priv.dp.dfp[1].type, pencoder->state.priv.dp.dfp[1].hpd_aware, pencoder->state.priv.dp.dfp[1].max_bpc, pencoder->state.priv.dp.dfp[1].max_tdms_clk_khz,
            pencoder->state.priv.dp.dfp[2].type, pencoder->state.priv.dp.dfp[2].hpd_aware, pencoder->state.priv.dp.dfp[2].max_bpc, pencoder->state.priv.dp.dfp[2].max_tdms_clk_khz,
            pencoder->state.priv.dp.dfp[3].type, pencoder->state.priv.dp.dfp[3].hpd_aware, pencoder->state.priv.dp.dfp[3].max_bpc, pencoder->state.priv.dp.dfp[3].max_tdms_clk_khz);

        /* TODO: do some limit for branch device. (bpc, max_tdms_clock, frl_bw)
         * Total 4 dfp: 0x80 ~ 0x83 ... 0x8c ~ 0x8f.
         */
    }
    else
    {
        pencoder->state.priv.dp.flag.bits.dfp_branch_device = 0;
    }

    return DISP_OK;
}

disp_u32_t dp_probe_edid_block_ddc2(pdisp_connector_t pdisp_connector);
static DISP_CONNECT_STATE dp_connector_detect(pdisp_connector_t pdisp_connector)
{
    pdisp_device_t pdd = pdisp_connector->disp_device;
    pdisp_encoder_t pencoder = &pdd->disp_encoder[pdisp_connector->index];
    disp_u32_t hpd_status = 0;

    if(pdd->flag.info.pz1_capture_pipe)
    {
        if(DISP_TRUE == pdisp_connector->state.bfake)
            return DISP_CONNECTTED;
        return DISP_DISCONNECTED;
    }

    if(pdd->flag.info.pz1_config)
        disp_dp_phy_pz1_setting(&pencoder->phy);

    hpd_status = disp_dp_get_hpd_status(pdisp_connector);
    if(hpd_status != DPT_REG29_HPD_STS_O_HPD_PLUG_IN && hpd_status != DPT_REG29_HPD_STS_O_HPD_EVENT)
    {
        disp_print(DDM_INFO, "dp[%d] hpd status is not plug_in/event, hpd_status[%u]\n",
            pdisp_connector->index, hpd_status);
        return DISP_DISCONNECTED;
    }

    pdisp_connector->flag.uint = 0; // clear all flags
    if (DISP_OK != dp_get_sink_caps(pdisp_connector, DISP_FALSE))
    {
        disp_print_addition_file(DDM_ERROR, "%u aux get sink caps fail\n", pdisp_connector->index);
        return DISP_DISCONNECTED;
    }

    if (pdisp_connector->flag.info.dpcd_get_failed)
    {
        disp_dp_set_sink_power_state(pdisp_connector, DISP_TRUE);
        if (pdisp_connector->flag.info.dpcd_set_power_failed)
        {   // Dpcd not available, determined to be a cheater.
            pdisp_connector->flag.info.cheater = 1;
            pencoder->policy.custom.dp.max_possible_link_rate = DP_LINK_BW_8_1;
            pencoder->policy.custom.dp.max_possible_lane_count = 4;
            pencoder->policy.custom.dp.max_possible_bandwidth_kbyte = 27000 * DP_LINK_BW_8_1 * 4;
            return DISP_CONNECTTED;
        }
        else
        {
            dp_get_sink_caps(pdisp_connector, DISP_FALSE);
        }
    }

    // Fake connector not try aux_i2c to get edid. For dvo & pz1
    if(DISP_TRUE == pdisp_connector->state.bfake)
        return DISP_CONNECTTED;

    if (disp_connector_probe_edid(pdisp_connector))
        pdisp_connector->flag.info.use_safe_mode = 1;

    dp_probe_edid_block_ddc2(pdisp_connector);

    /* As DP spec we should set power first, but some monitor could not read back edid after D3->D0.
     * So we read dpcd & edid first, if not touch, then set power and try again.
     */
    disp_dp_set_sink_power_state(pdisp_connector, DISP_TRUE);

    dp_get_sink_caps(pdisp_connector, DISP_TRUE);
    if (DISP_OK != dp_update_disp_info_from_dpcd(pdisp_connector))
    {
        disp_print_addition_file(DDM_ERROR, "%u dpcd check failed!\n", pdisp_connector->index);
        return DISP_DISCONNECTED;
    }

    if (pdisp_connector->flag.info.use_safe_mode)
    {
        if (disp_connector_probe_edid(pdisp_connector))
            pdisp_connector->flag.info.use_safe_mode = 1;
        else
            pdisp_connector->flag.info.use_safe_mode = 0;
    }

    return DISP_CONNECTTED;
}

static disp_s32_t dp_i2c_over_aux(struct disp_dp_aux *aux, disp_u8_t request, disp_u32_t address, void *buffer, disp_u32_t size)
{
    struct disp_dp_aux_msg msg;
    disp_u32_t level = DDM_TRACE;

    disp_memset(&msg, 0, sizeof(msg));
    msg.address = address;
    msg.request = request;
    msg.buffer = buffer;
    msg.size = size;

    aux->transfer(aux, &msg);

    if (0 == msg.reply) // Ack
    {
        return msg.read_back_size;
    }
    else if (DP_AUX_HW_REPLY_TIMEOUT & msg.reply)
    {
        disp_print(level, "dp_i2c_over_aux, aux timeout! req[%x] addr[%x]\n", request, address);
        return DP_I2C_STOP;
    }
    else if (DP_AUX_HW_REPLY_BUSY & msg.reply)
    {
        disp_print(level, "dp_i2c_over_aux, aux timeout! req[%x] addr[%x]\n", request, address);
        return DP_I2C_STOP;
    }
    else if (DP_AUX_I2C_REPLY_NACK & msg.reply)
    {
        disp_print(level, "dp_i2c_over_aux, i2c nack! req[%x] addr[%x]\n", request, address);
        return DP_I2C_STOP;
    }
    else if (DP_AUX_NATIVE_REPLY_NACK & msg.reply)
    {
        disp_print(level, "dp_i2c_over_aux, aux nack! req[%x] addr[%x]\n", request, address);
        return DP_I2C_STOP;
    }
    else if (DP_AUX_I2C_REPLY_DEFER & msg.reply)
    {
        disp_print(level, "dp_i2c_over_aux, i2c defer! req[%x] addr[%x]\n", request, address);
        disp_delay_us(1000);
        return DP_I2C_REPEAT;
    }
    else if (DP_AUX_NATIVE_REPLY_DEFER & msg.reply)
    {
        disp_print(level, "dp_i2c_over_aux, aux defer! req[%x] addr[%x]\n", request, address);
        disp_delay_us(1000);
        return DP_I2C_REPEAT;
    }

    disp_print(level, "dp_i2c_over_aux, unexpected reply[%x] req[%x] addr[%x]\n", msg.reply, request, address);
    return DP_I2C_STOP;
}

static disp_s32_t dp_set_segment_and_ddc(pdisp_connector_t pdisp_connector, disp_u8_t segment, disp_u8_t start)
{
    disp_u8_t request = DP_AUX_I2C_WRITE | DP_AUX_I2C_MOT;
    disp_u8_t buf_seg = segment;
    disp_u8_t buf_offset = start;
    disp_s32_t ret = 0, retries = 5;
    disp_s32_t max_i2c_aux_retry = DISP_DP_RETRY_I2CAUX_TIMES;

    // set seg
    ret = 0; retries = 0;
    do {
        request = DP_AUX_I2C_WRITE | DP_AUX_I2C_MOT;
        if (DP_I2C_STOP == ret)
            dp_i2c_over_aux(pdisp_connector->ddc, DP_AUX_I2C_WRITE, DDC_ADDR, &buf_seg, 0);
        else if (DP_I2C_REPEAT == ret)
            request |= DP_AUX_I2C_WRITE_STATUS_UPDATE;
        ret = dp_i2c_over_aux(pdisp_connector->ddc, request, DDC_SEGMENT_ADDR, &buf_seg, 1);
        retries++;
    } while (ret < 0 && retries < max_i2c_aux_retry);
    if (ret < 0)
    {
        disp_print(DDM_TRACE, "ddc segment write failed[%d]!\n", ret);
        dp_i2c_over_aux(pdisp_connector->ddc, DP_AUX_I2C_WRITE, DDC_ADDR, &buf_seg, 0);
        //return ret; // Not wait segment success, direct try set ddc, probably read back block 0/1.
    }

    // set ddc
    ret = 0; retries = 0;
    do {
        request = DP_AUX_I2C_WRITE | DP_AUX_I2C_MOT;
        if (DP_I2C_STOP == ret)
            dp_i2c_over_aux(pdisp_connector->ddc, DP_AUX_I2C_WRITE, DDC_ADDR, &buf_seg, 0);
        else if (DP_I2C_REPEAT == ret)
            request |= DP_AUX_I2C_WRITE_STATUS_UPDATE;
        ret = dp_i2c_over_aux(pdisp_connector->ddc, request, DDC_ADDR, &buf_offset, 1);
        retries++;
    } while (ret < 0 && retries < max_i2c_aux_retry);
    if (ret < 0)
    {
        disp_print(DDM_TRACE, "ddc address write failed!\n");
        dp_i2c_over_aux(pdisp_connector->ddc, DP_AUX_I2C_WRITE, DDC_ADDR, &buf_seg, 0);
        return ret;
    }

    return 0;
}

static disp_u32_t dp_probe_edid_block(pdisp_connector_t pdisp_connector, pdisp_u8_t buf, disp_u8_t block, disp_u16_t len)
{
    disp_u8_t segment = block >> 1;
    disp_u8_t start = block * EDID_LENGTH;
    int ret = 0, retries = 5;
    disp_u8_t request = 0;
    disp_u32_t have_read = 0, req_len = 0;
    disp_u32_t max_req_len = 16;
    disp_s32_t max_i2c_aux_retry = DISP_DP_RETRY_I2CAUX_TIMES;
    DISP_HPD_TYPE hpd_state = DISP_HPD_TYPE_IDLE;

    dp_set_segment_and_ddc(pdisp_connector, segment, start);

    ret = 0;
    retries = 0;
    req_len = DISP_MIN(len - have_read, max_req_len);
    request = DP_AUX_I2C_READ | DP_AUX_I2C_MOT;
    while (have_read < len && retries < max_i2c_aux_retry)
    {
        ret = dp_i2c_over_aux(pdisp_connector->ddc, request, DDC_ADDR, buf + have_read, req_len);
        if (DP_I2C_STOP == ret)
        {
            hpd_state = disp_dp_get_hpd_status(pdisp_connector);
            if (DISP_HPD_TYPE_IDLE == hpd_state || DISP_HPD_TYPE_PLUG_OUT == hpd_state)
            {
                retries = max_i2c_aux_retry;
                break;
            }
            else
            {
                dp_i2c_over_aux(pdisp_connector->ddc, DP_AUX_I2C_READ, DDC_ADDR, buf, 0);
                dp_set_segment_and_ddc(pdisp_connector, segment, block * EDID_LENGTH + (disp_u8_t)have_read);
                retries++; // Keep req_len
            }
        }
        else if (DP_I2C_REPEAT == ret)
        {
            retries++; // Keep req_len
        }
        else
        {
            have_read += ret;
            if (ret < (disp_s32_t)req_len)
            {
                disp_print(DDM_TRACE, "probe_edid_block, ack but few byte[%d < %u].\n", ret, req_len);
                req_len = req_len - ret;
                if (0 == ret)
                    retries++;
                else
                    retries = 0;
            }
            else
            {
                req_len = DISP_MIN(len - have_read, max_req_len);
                retries = 0;
            }
        }
    }

    if (have_read < len)
    {
        disp_print(DDM_TRACE, "probe_edid_block[%u], have_read[%u] < len[%u], hpd_state[%d]\n",
            block, have_read, len, hpd_state);
    }
    dp_i2c_over_aux(pdisp_connector->ddc, DP_AUX_I2C_READ, DDC_ADDR, buf, 0);

    return have_read;
}

disp_u32_t dp_probe_edid_block_ddc2(pdisp_connector_t pdisp_connector)
{
    disp_u8_t request = DP_AUX_I2C_WRITE | DP_AUX_I2C_MOT;
    disp_u8_t buf_seg = 0, buf_offset = 0, try_read_cnt = 0;
    disp_u8_t buf[EDID_LENGTH] = { 0 };
    disp_u32_t have_read = 0, max_req_len = 16, req_len = 0, len = EDID_LENGTH, idx = 0;
    disp_s32_t ret = 0;

    /* Not found yet that monitor could access ddc2, refine ddc2 if meet. */

    ret = dp_i2c_over_aux(pdisp_connector->ddc, request, DDC_SEGMENT_ADDR, &buf_seg, 1);
    if (ret < 0)
    {
        disp_print(DDM_TRACE, "dp_probe_edid_block_ddc2 set seg failed[%d]!\n", ret);
        dp_i2c_over_aux(pdisp_connector->ddc, DP_AUX_I2C_WRITE, DDC_ADDR2, &buf_seg, 0);
    }

    ret = dp_i2c_over_aux(pdisp_connector->ddc, request, DDC_ADDR2, &buf_offset, 1);
    if (ret < 0)
    {
        disp_print(DDM_TRACE, "dp_probe_edid_block_ddc2 set ddc2 offset failed[%d]!\n", ret);
        dp_i2c_over_aux(pdisp_connector->ddc, DP_AUX_I2C_WRITE, DDC_ADDR2, &buf_seg, 0);
    }
    else
    {
        request = DP_AUX_I2C_READ | DP_AUX_I2C_MOT;
        have_read = 0;

        // Check EEPROM empty read.
        ret = dp_i2c_over_aux(pdisp_connector->ddc, request, DDC_ADDR2, buf + have_read, max_req_len);
        if (ret == (disp_s32_t)max_req_len)
        {
            try_read_cnt = 0;
            for (idx = 0; idx < (disp_u32_t)ret; idx++)
            {
                if (0x4d == buf[idx])
                    try_read_cnt++;
            }
            if (try_read_cnt == max_req_len)
            {
                disp_print(DDM_KEY, "dp_probe_edid_block_ddc2, ignore empty read.\n");
                return 0;
            }
        }
        else
        {
            disp_print(DDM_TRACE, "dp_probe_edid_block_ddc2, read failed[%d]!\n", ret);
            return 0;
        }
        try_read_cnt = 1;
        have_read = ret;

        // Read left block data.
        while (have_read < len)
        {
            req_len = DISP_MIN(len - have_read, max_req_len);
            ret = dp_i2c_over_aux(pdisp_connector->ddc, request, DDC_ADDR2, buf + have_read, req_len);
            if (ret < 0)
                break;
            else
                have_read += ret;

            try_read_cnt++;
        }

        disp_print(DDM_KEY, "dp_probe_edid_block_ddc2 access OK, have_read[%u] try_read_cnt[%u]:\n", have_read, try_read_cnt);
        for (idx = 0; idx < EDID_LENGTH; idx += 16)
        {
            disp_print(DDM_KEY, "\t0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x"
                " 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",
                buf[idx], buf[idx + 1], buf[idx + 2], buf[idx + 3],
                buf[idx + 4], buf[idx + 5], buf[idx + 6], buf[idx + 7],
                buf[idx + 8], buf[idx + 9], buf[idx + 10], buf[idx + 11],
                buf[idx + 12], buf[idx + 13], buf[idx + 14], buf[idx + 15]);
        }
    }

    return have_read;
}


#define DP_2CHANNEL_EQ_BITS (DP_CHANNEL_EQ_BITS | (DP_CHANNEL_EQ_BITS << 4))

disp_u8_t dp_hpd_need_link_maintenance(pdisp_connector_t pdisp_connector, disp_u8_t lane_count)
{
    pdisp_dp_sink_caps_t psink_caps = (pdisp_dp_sink_caps_t)pdisp_connector->disp_info.dp_sink_caps;
    disp_u8_t dp_lane0_1_status;
    disp_u8_t dp_lane2_3_status;
    disp_u8_t dp_lane_align;
    disp_u8_t irq_vector;

    irq_vector = psink_caps->sink_status[1];
    dp_lane0_1_status = psink_caps->sink_status[2];
    dp_lane2_3_status = psink_caps->sink_status[3];
    dp_lane_align = psink_caps->sink_status[4];

    if(~dp_lane_align & DP_INTERLANE_ALIGN_DONE)
    {
        disp_print(DDM_INFO, "dp_lane_align need LT\n");
        return DISP_TRUE;
    }

    if(lane_count == 4)
    {
        if((~dp_lane0_1_status & DP_2CHANNEL_EQ_BITS) || (~dp_lane2_3_status & DP_2CHANNEL_EQ_BITS))
        {
            disp_print(DDM_INFO, "dp_lane0_1_status or dp_lane2_3_status need LT\n");
            return DISP_TRUE;
        }
    }
    else if(lane_count == 2)
    {
        if(~dp_lane0_1_status & DP_2CHANNEL_EQ_BITS)
        {
            disp_print(DDM_INFO, "dp_lane0_1_status need LT\n");
            return DISP_TRUE;
        }
    }
    else if(lane_count == 1)
    {
        if(~dp_lane0_1_status & DP_CHANNEL_EQ_BITS)
        {
            disp_print(DDM_INFO, "dp_lane0_1_status need LT\n");
            return DISP_TRUE;
        }
    }
    else
    {
        disp_print(DDM_INFO, "lane count read error, not 1/2/4\n");
    }

    return DISP_FALSE;
}

disp_u8_t dp_hpd_need_read_edid(pdisp_connector_t pdisp_connector, disp_u8_t auto_test_req)
{
    pdisp_dp_sink_caps_t psink_caps = (pdisp_dp_sink_caps_t)pdisp_connector->disp_info.dp_sink_caps;
    disp_u8_t irq_vector;
    disp_u8_t dp_lane_align;

    irq_vector = psink_caps->sink_status[1];
    dp_lane_align = psink_caps->sink_status[4];

    if((irq_vector & DP_AUTOMATED_TEST_REQUEST) && (auto_test_req == DP_TEST_LINK_EDID_READ))
    {
        disp_print(DDM_INFO, "auto test need read edid\n");
        return DISP_TRUE;
    }

    if(dp_lane_align & DP_DOWNSTREAM_PORT_STATUS_CHANGED)
    {
        disp_print(DDM_INFO, "dp_lane_align need read edid\n");
        return DISP_TRUE;
    }

    return DISP_FALSE;
}

static disp_void_t disp_dp_check_status(pdisp_dp_sink_caps_t pcaps)
{
    // DPCD 0x201: DEVICE_SERVICE_IRQ_VECTOR
    disp_u8_t irq_vec = pcaps->sink_status[1];
    disp_u8_t lane_align_status = pcaps->sink_status[4];
    disp_u8_t sink_status = pcaps->sink_status[5];

    //DPCD 0x201
    if (irq_vec & DP_REMOTE_CONTROL_COMMAND_PENDING)
        disp_print(DDM_INFO, "Remote control command pending (DPCD 0x201 bit1)\n");

    if (irq_vec & DP_CP_IRQ)
        disp_print(DDM_INFO, "Content Protection (HDCP) IRQ asserted (DPCD 0x201 bit3)\n");

    if (irq_vec & DP_MCCS_IRQ)
        disp_print(DDM_INFO, "MCCS command/response available (DPCD 0x201 bit4)\n");

    if (irq_vec & DP_DOWN_REP_MSG_RDY)
        disp_print(DDM_INFO, "Downstream reply message ready (DPCD 0x201 bit4)\n");

    if (irq_vec & DP_UP_REQ_MSG_RDY)
        disp_print(DDM_INFO, "Upstream request message ready (DPCD 0x201 bit5)\n");

    if (irq_vec & DP_SINK_SPECIFIC_IRQ)
        disp_print(DDM_INFO, "Vendor specific irq set (DPCD 0x201 bit6)\n");

    // DPCD 0x204-0x205
    if (lane_align_status & DP_LINK_STATUS_UPDATED)
        disp_print(DDM_INFO, "Link Status and Adjust Request updated since the last read (DPCD 0x204 bit7)\n");

    if ((sink_status & DP_RECEIVE_PORT_0_STATUS) == 0)        //bit0 0:out of sync 1:in sync
        disp_print(DDM_INFO, "Port 0 Sink device is out of synchronization (DPCD 0x205 bit0)\n");

    if ((sink_status & DP_RECEIVE_PORT_1_STATUS) == 0)        //bit1 0:out of sync 1:in sync
        disp_print(DDM_INFO, "Port 1 Sink device is out of synchronization (DPCD 0x205 bit1)\n");

}

static DISP_STATUS disp_dp_irq_hpd_handle(pdisp_connector_t pdisp_connector)
{
    struct disp_dp_aux *paux = (struct disp_dp_aux *)pdisp_connector->ddc;
    pdisp_dp_sink_caps_t pcaps = (pdisp_dp_sink_caps_t)pdisp_connector->disp_info.dp_sink_caps;
    pdisp_device_t pdd = pdisp_connector->disp_device;
    pdisp_encoder_t pdisp_encoder = &pdd->disp_encoder[pdisp_connector->index];
    disp_s32_t ret = 0;
    disp_u8_t reg_value = 0;
    disp_u8_t auto_test_req = 0;
    disp_u8_t irq_vector = 0;
    disp_u8_t lane_count = pdisp_encoder->state.priv.dp.lane_count;
    disp_u8_t irq_vector_handled = 0;
    disp_u8_t test_lane_count = 0, test_link_rate = 0;
    disp_u8_t need_lr = DISP_FALSE;

    disp_dp_read_sink_status(pdisp_connector, pcaps->sink_status);

    disp_print(DDM_INFO, "sink_status[%d]: %x %x %x %x %x %x\n", pdisp_connector->index, pcaps->sink_status[0],
        pcaps->sink_status[1], pcaps->sink_status[2], pcaps->sink_status[3], pcaps->sink_status[4], pcaps->sink_status[5]);

    irq_vector = pcaps->sink_status[1];

    if(irq_vector & DP_AUTOMATED_TEST_REQUEST)
    {
        ret = disp_dp_dpcd_read(paux, DP_TEST_REQUEST, &auto_test_req, 1);
        if (ret < 0)
            disp_print(DDM_WARNING, "disp_dp_irq_hpd_handle, read DP_PSR_ERROR_STATUS error[%u]\n", ret);
    }

    // two case need link re-training, test request or link maintenance need
    if((irq_vector & DP_AUTOMATED_TEST_REQUEST) && (auto_test_req == DP_TEST_LINK_TRAINING))
    {
        ret = disp_dp_dpcd_read(paux, DP_TEST_LINK_RATE, &test_link_rate, 1);
        ret |= disp_dp_dpcd_read(paux, DP_TEST_LANE_COUNT, &test_lane_count, 1);
        if (ret < 0)
            disp_print(DDM_WARNING, "disp_dp_irq_hpd_handle, read DP_PSR_ERROR_STATUS error[%u]\n", ret);
        else
        {
            disp_print(DDM_KEY, "disp_dp_irq_hpd_handle, LT request: test link rate[%x], test lane count[%x]\n",
                test_link_rate, test_lane_count);
            pdisp_encoder->policy.custom.dp.link_rate = test_link_rate;
            pdisp_encoder->policy.custom.dp.lane_count = test_lane_count;
            need_lr |= DISP_TRUE;
            irq_vector_handled |= DP_AUTOMATED_TEST_REQUEST;
        }
    }

    need_lr |= dp_hpd_need_link_maintenance(pdisp_connector, lane_count);

    if (need_lr)
    {
        /* Some irq lt request too close with hpd_in, skip lt if not set timing yet. */
        if (pdd->disp_crtc[pdisp_connector->index].property.hw_mode.clock)
        {
            if (!pdisp_encoder->state.power_state) // First lt, need prepare.
                pdisp_encoder->func->dpms_prepare(pdisp_encoder);
            pdisp_encoder->func->dpms(pdisp_encoder, DISP_POWER_ON);
        }
        else
            disp_print(DDM_WARNING, "not set timing yet! skip this lt request\n");
    }

    // here process edid read irq, also test request or link maintenance need
    if(dp_hpd_need_read_edid(pdisp_connector, auto_test_req))
    {
        ret = disp_connector_probe_edid(pdisp_connector);
        if (ret < 0)
            disp_print(DDM_WARNING, "disp_dp_irq_hpd_handle, probe edid failed\n", ret);
        else if(irq_vector & DP_AUTOMATED_TEST_REQUEST)
        {
            irq_vector_handled |= DP_AUTOMATED_TEST_REQUEST;
        }
    }

    if (irq_vector & 0x4)
        disp_print(DDM_KEY, "CP irq unhandles\n");

    if (irq_vector & 0x40)
        disp_print(DDM_KEY, "Sink specific irq unhandled\n");

    //check other sink status bits
    disp_dp_check_status(pcaps);
    
    if (disp_edp_psr_get_version(pdisp_connector))
    {
        ret = disp_dp_dpcd_read(paux, DP_PSR_ERROR_STATUS, &reg_value, 1);
        if (ret < 0)
            disp_print(DDM_WARNING, "disp_dp_irq_hpd_handle, read DP_PSR_ERROR_STATUS error[%u]\n", ret);
        if (reg_value)
            disp_print(DDM_KEY, "IRQ_HPD need handle: DP_PSR_ERROR_STATUS[%u]\n", reg_value);

        ret = disp_dp_dpcd_read(paux, DP_PSR_ESI, &reg_value, 1);
        if (ret < 0)
            disp_print(DDM_WARNING, "disp_dp_irq_hpd_handle, read DP_PSR_ESI error[%u]\n", ret);
        if (reg_value)
            disp_print(DDM_KEY, "IRQ_HPD need handle: DP_PSR_ESI[%u]\n", reg_value);
    }

    ret = disp_dp_dpcd_read(paux, DP_SINK_DEVICE_PR_AND_FRAME_LOCK_STATUS, &reg_value, 1);
    if (ret < 0)
        disp_print(DDM_WARNING, "disp_dp_irq_hpd_handle, read DP_SINK_DEVICE_PR_AND_FRAME_LOCK_STATUS error[%u]\n", ret);
    if ((reg_value & DP_SINK_DEVICE_PANEL_REPLAY_STATUS_MASK) == 0x7)
        disp_print(DDM_KEY, "IRQ_HPD need handle: DP Sink device internal error! reg[%u]\n", reg_value);

    //handle finish, here write irq vector to dpcd if need
    if(irq_vector_handled)
    {
        ret = disp_dp_dpcd_write(paux, DP_DEVICE_SERVICE_IRQ_VECTOR, &irq_vector_handled, 1);
        if(ret < 0)
            disp_print(DDM_WARNING, "disp_dp_irq_hpd_handle, write DP_DEVICE_SERVICE_IRQ_VECTOR error[%u]\n", ret);
    }

    return DISP_OK;
}

static struct disp_connector_func dp_connector_func =
{
    .detect = dp_connector_detect,
    .probe_edid_block = dp_probe_edid_block,
    .get_hpd_status = disp_dp_get_hpd_status,
    .get_i2c_status = disp_dp_get_i2c_status,
    .backlight_query = disp_edp_backlight_query,
    .backlight_set = disp_edp_backlight_set,
    .psr_query = disp_edp_psr_query,
    .psr_set = disp_edp_psr_set,
    .irq_hpd_handle = disp_dp_irq_hpd_handle,
};

disp_void_t disp_dp_connector_sw_init(pdisp_void_t pdisp_connector)
{
    pdisp_connector_t pdp_connector = (pdisp_connector_t)pdisp_connector;

    pdp_connector->ddc = disp_malloc(sizeof(struct disp_dp_aux));

    if(!pdp_connector->ddc)
    {
        disp_print(DDM_ERROR, "malloc aux failed\n");
        return;
    }

    disp_dp_aux_init(pdp_connector);

    pdp_connector->pfunc = &dp_connector_func;

    pdp_connector->disp_info.dp_sink_caps = disp_malloc(sizeof(disp_dp_sink_caps_t));
    if(!pdp_connector->disp_info.dp_sink_caps)
        disp_print(DDM_ERROR, "disp_dp_connector_sw_init, alloc dp sink caps failed\n");

    if (disp_dp_is_edp(pdp_connector))
    {
        pdp_connector->disp_info.edp_info = disp_malloc(sizeof(disp_edp_info_t));
        if(!pdp_connector->disp_info.edp_info)
            disp_print(DDM_ERROR, "disp_dp_connector_sw_init, alloc edp_info failed\n");
    }
}

disp_void_t disp_dp_connector_sw_deinit(pdisp_void_t pdisp_connector)
{
    pdisp_connector_t pdp_connector = (pdisp_connector_t)pdisp_connector;

    if (pdp_connector->disp_info.edp_info)
    {
        disp_free(pdp_connector->disp_info.edp_info);
        pdp_connector->disp_info.edp_info = DISP_NULL;
    }

    if (pdp_connector->disp_info.dp_sink_caps)
    {
        disp_free(pdp_connector->disp_info.dp_sink_caps);
        pdp_connector->disp_info.dp_sink_caps = DISP_NULL;
    }

    pdp_connector->pfunc = DISP_NULL;

    if(pdp_connector->ddc)
    {
        disp_free(pdp_connector->ddc);
        pdp_connector->ddc = DISP_NULL;
    }
}
