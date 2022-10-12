/*
 * Copyright (C) 2015-2020 Alibaba Group Holding Limited
 */

#include "genie_sal.h"
#include "genie_mesh_api.h"

#include "drv_light.h"
#include <board_config.h>
#include <pin_name.h>

#include "light.h"
#include "light_driver.h"

static pwm_port_func_t pwm_channel_config[] = {
    {PIN_LED_R, PIN_LED_R_CHANNEL, PWM_LED_R_PORT},
    {PIN_LED_G, PIN_LED_G_CHANNEL, PWM_LED_G_PORT},
};

static pwm_dev_t pmw_light[ARRAY_SIZE(pwm_channel_config)];

static led_light_cfg_t led_config[] = {
#if defined(BOARD_TG7121B_EVB)
    LED_LIGHT_MODEL(GENIE_COLD_WARM_LIGHT, &pwm_channel_config[0], HIGH_LIGHT, &pmw_light[0], ARRAY_SIZE(pwm_channel_config)), /* &pmw_light[0], */
#else
    LED_LIGHT_MODEL(GENIE_COLD_WARM_LIGHT, &pwm_channel_config[0], LOW_LIGHT, &pmw_light[0], ARRAY_SIZE(pwm_channel_config)), /* &pmw_light[0], */
#endif
};

void light_driver_init(void)
{
    led_light_init(led_config);
}

void light_driver_update(uint8_t onoff, uint16_t lightness, uint16_t temperature)
{
    if (GENIE_COLD_WARM_LIGHT == led_config[0].pin_mode)
    {
        struct genie_cold_warm_op led_op = {
            .power_switch = onoff,
            .actual = lightness,
            .temperature = temperature,
        };
        led_light_control((void *)(&led_op));
    }
    else if (ON_OFF_LIGHT == led_config[0].pin_mode)
    {
        struct genie_on_off_op led_op;
        led_op.power_switch = onoff;
        led_light_control((void *)(&led_op));
    }
    else if (RGB_LIGHT == led_config[0].pin_mode)
    {
        struct genie_rgb_op led_op;
        for (int i = 0; i < LED_RGB_CHANNEL_MAX; i++)
        {
            led_op.rgb_config[i].power_switch = onoff;
            led_op.rgb_config[i].led_actual = 0xff;
        }
        led_light_control((void *)(&led_op));
    }
}
