/*
 * Copyright (C) 2019-2022 Alibaba Group Holding Limited
 */

#include <stdio.h>
#include <stdint.h>
#include "dimmer.h"
//#include <hal/hal.h>
#include <aos/hal/pwm.h>
#include <board_config.h>
#include <pinmux.h>
#include <drv_light.h>

#if (CURVE_FITTING)
/* curve fitting table */
/* choose a better table for your hardware or replace with your own data */
const float fitting_table[] = {
 0.0       , 0.0       , 0.0       , 0.0       , 0.0       , 0.0,
 0.0       , 0.0       , 0.0       , 0.0       , 0.0       , 0.0,
 0.00392157, 0.00392157, 0.00392157, 0.00392157, 0.00392157, 0.00392157,
 0.00392157, 0.00392157, 0.00784314, 0.00784314, 0.00784314, 0.00784314,
 0.00784314, 0.00784314, 0.01176471, 0.01176471, 0.01176471, 0.01176471,
 0.01568628, 0.01568628, 0.01568628, 0.01568628, 0.01960784, 0.01960784,
 0.01960784, 0.01960784, 0.02352941, 0.02352941, 0.02352941, 0.02745098,
 0.02745098, 0.02745098, 0.03137255, 0.03137255, 0.03137255, 0.03529412,
 0.03529412, 0.03529412, 0.03921569, 0.03921569, 0.04313726, 0.04313726,
 0.04313726, 0.04705882, 0.04705882, 0.05098039, 0.05098039, 0.05490196,
 0.05490196, 0.05882353, 0.05882353, 0.0627451 , 0.0627451 , 0.06666667,
 0.06666667, 0.07058824, 0.07058824, 0.07450981, 0.07450981, 0.07843138,
 0.07843138, 0.08235294, 0.08235294, 0.08627451, 0.09019608, 0.09019608,
 0.09411765, 0.09411765, 0.09803922, 0.10196079, 0.10196079, 0.10588235,
 0.10980392, 0.10980392, 0.11372549, 0.11764706, 0.11764706, 0.12156863,
 0.1254902 , 0.1254902 , 0.12941177, 0.13333334, 0.13725491, 0.13725491,
 0.14117648, 0.14509805, 0.14901961, 0.14901961, 0.15294118, 0.15686275,
 0.16078432, 0.16470589, 0.16470589, 0.16862746, 0.17254902, 0.1764706,
 0.18039216, 0.18431373, 0.18431373, 0.1882353 , 0.19215687, 0.19607843,
 0.2       , 0.20392157, 0.20784314, 0.21176471, 0.21568628, 0.21960784,
 0.21960784, 0.22352941, 0.22745098, 0.23137255, 0.23529412, 0.23921569,
 0.24313726, 0.24705882, 0.2509804 , 0.25490198, 0.25882354, 0.2627451,
 0.26666668, 0.27058825, 0.27450982, 0.2784314 , 0.28627452, 0.2901961,
 0.29411766, 0.29803923, 0.3019608 , 0.30588236, 0.30980393, 0.3137255,
 0.31764707, 0.32156864, 0.32941177, 0.33333334, 0.3372549 , 0.34117648,
 0.34509805, 0.34901962, 0.35686275, 0.36078432, 0.3647059 , 0.36862746,
 0.37254903, 0.38039216, 0.38431373, 0.3882353 , 0.39215687, 0.4,
 0.40392157, 0.40784314, 0.4117647 , 0.41960785, 0.42352942, 0.42745098,
 0.43529412, 0.4392157 , 0.44313726, 0.4509804 , 0.45490196, 0.45882353,
 0.46666667, 0.47058824, 0.4745098 , 0.48235294, 0.4862745 , 0.49411765,
 0.49803922, 0.5019608 , 0.50980395, 0.5137255 , 0.52156866, 0.5254902,
 0.53333336, 0.5372549 , 0.54509807, 0.54901963, 0.5568628 , 0.56078434,
 0.5686275 , 0.57254905, 0.5803922 , 0.58431375, 0.5921569 , 0.59607846,
 0.6039216 , 0.60784316, 0.6156863 , 0.61960787, 0.627451  , 0.63529414,
 0.6392157 , 0.64705884, 0.6509804 , 0.65882355, 0.6666667 , 0.67058825,
 0.6784314 , 0.6862745 , 0.6901961 , 0.69803923, 0.7058824 , 0.70980394,
 0.7176471 , 0.7254902 , 0.7294118 , 0.7372549 , 0.74509805, 0.7529412,
 0.75686276, 0.7647059 , 0.77254903, 0.78039217, 0.78431374, 0.7921569,
 0.8       , 0.80784315, 0.8117647 , 0.81960785, 0.827451  , 0.8352941,
 0.84313726, 0.8509804 , 0.85490197, 0.8627451 , 0.87058824, 0.8784314,
 0.8862745 , 0.89411765, 0.9019608 , 0.9098039 , 0.9137255 , 0.92156863,
 0.92941177, 0.9372549 , 0.94509804, 0.9529412 , 0.9607843 , 0.96862745,
 0.9764706 , 0.9843137 , 0.99215686, 1.0
};
#endif

led_type_t g_dimmer_led_type = {
    .type = LED_TYPE_RGB,
    .power = LED_POWER_HALF
};

static uint8_t pwm_inited = 0;
static pwm_dev_t pwm_r;
static pwm_dev_t pwm_g;
static pwm_dev_t pwm_b;
static pwm_dev_t pwm_c;
static pwm_dev_t pwm_w;

static pwm_port_func_t pwm_channel[] = {
    {PIN_LED_R, PIN_LED_R_CHANNEL, PWM_LED_R_PORT},
    {PIN_LED_G, PIN_LED_G_CHANNEL, PWM_LED_G_PORT},
    {PIN_LED_B, PIN_LED_B_CHANNEL, PWM_LED_B_PORT},
    {PIN_LED_C, PIN_LED_C_CHANNEL, PWM_LED_C_PORT},
    {PIN_LED_W, PIN_LED_W_CHANNEL, PWM_LED_W_PORT},
};

int dimmer_init(void)
{
    if (pwm_inited == 1)
    {
        return 0;
    }
    drv_pinmux_config(pwm_channel[0].pin, pwm_channel[0].pin_func);
    pwm_r.port = pwm_channel[0].port;
    pwm_r.config.freq = PWM_FRE_RED;
    pwm_r.config.duty_cycle = 0.0;
    hal_pwm_init(&pwm_r);
    hal_pwm_start(&pwm_r);

    drv_pinmux_config(pwm_channel[1].pin, pwm_channel[1].pin_func);
    pwm_g.port = pwm_channel[1].port;
    pwm_g.config.freq = PWM_FRE_GREEN;
    pwm_g.config.duty_cycle = 0.0;
    hal_pwm_init(&pwm_g);
    hal_pwm_start(&pwm_g);

    drv_pinmux_config(pwm_channel[2].pin, pwm_channel[2].pin_func);
    pwm_b.port = pwm_channel[2].port;
    pwm_b.config.freq = PWM_FRE_BLUE;
    pwm_b.config.duty_cycle = 0.0;
    hal_pwm_init(&pwm_b);
    hal_pwm_start(&pwm_b);

    if ((LED_TYPE_RGB != g_dimmer_led_type.type))
    {
        drv_pinmux_config(pwm_channel[3].pin, pwm_channel[3].pin_func);
        pwm_c.port = pwm_channel[3].port;
        pwm_c.config.freq = PWM_FRE_COLD;
        pwm_c.config.duty_cycle = 0.0;
        hal_pwm_init(&pwm_c);
        hal_pwm_start(&pwm_c);
    }
    if (((LED_TYPE_CW == g_dimmer_led_type.type) || (LED_TYPE_RGBCW == g_dimmer_led_type.type)))
    {
        drv_pinmux_config(pwm_channel[4].pin, pwm_channel[4].pin_func);
        pwm_w.port = pwm_channel[4].port;
        pwm_w.config.freq = PWM_FRE_WARM;
        pwm_w.config.duty_cycle = 0.0;
        hal_pwm_init(&pwm_w);
        hal_pwm_start(&pwm_w);
    }

    pwm_inited = 1;
    return dimmer_set_color(0, 0, 0, 0, 0);
}

void dimmer_deinit(void)
{
    if (pwm_inited == 1)
    {
        hal_pwm_stop(&pwm_r);
        hal_pwm_finalize(&pwm_r);
        hal_pwm_stop(&pwm_g);
        hal_pwm_finalize(&pwm_g);
        hal_pwm_stop(&pwm_b);
        hal_pwm_finalize(&pwm_b);
        if ((LED_TYPE_RGB != g_dimmer_led_type.type))
        {
            hal_pwm_stop(&pwm_c);
            hal_pwm_finalize(&pwm_c);
        }
        if (((LED_TYPE_CW == g_dimmer_led_type.type) || (LED_TYPE_RGBCW == g_dimmer_led_type.type)))
        {
            hal_pwm_stop(&pwm_w);
            hal_pwm_finalize(&pwm_w);
        }
        pwm_inited = 0;
    }
}

led_type_t dimmer_get_type(void)
{
    return g_dimmer_led_type;
}

int dimmer_set_color(uint8_t red, uint8_t green, uint8_t blue, uint8_t cold, uint8_t warm)
{
    float duty_cycle = 0.0;

    dimmer_init();

    // printf("%s red %d, green %d, blue %d, cold %d, warm %d\n", __func__, red, green, blue, cold, warm);
    if (LED_TYPE_CW < g_dimmer_led_type.type)
    {
#if (CURVE_FITTING)
        duty_cycle = fitting_table[red];
#else
        duty_cycle = 1.0 * RED_CAL_FACTOR * red / 255;
#endif
        if (pwm_r.config.duty_cycle != duty_cycle)
        {
            pwm_r.config.duty_cycle = duty_cycle;
            hal_pwm_para_chg(&pwm_r, pwm_r.config);
            // printf("pwm_r duty_cycle: %f\n", duty_cycle);
        }
#if (CURVE_FITTING)
        duty_cycle = fitting_table[green];
#else
        duty_cycle = 1.0 * GREEN_CAL_FACTOR * green / 255;
#endif
        if (pwm_g.config.duty_cycle != duty_cycle)
        {
            pwm_g.config.duty_cycle = duty_cycle;
            hal_pwm_para_chg(&pwm_g, pwm_g.config);
            // printf("pwm_g duty_cycle: %f\n", duty_cycle);
        }
#if (CURVE_FITTING)
        duty_cycle = fitting_table[blue];
#else
        duty_cycle = 1.0 * BLUE_CAL_FACTOR * blue / 255;
#endif
        if (pwm_b.config.duty_cycle != duty_cycle)
        {
            pwm_b.config.duty_cycle = duty_cycle;
            hal_pwm_para_chg(&pwm_b, pwm_b.config);
            // printf("pwm_b duty_cycle: %f\n", duty_cycle);
        }
    }
    if (((LED_TYPE_CW == g_dimmer_led_type.type) || (LED_TYPE_RGBCW == g_dimmer_led_type.type)))
    {
        if (cold + warm != 0)
        {
            //duty_cycle = 1.0 - (1.0 * cold/(cold + warm));
#if (CW_HW_CONFIG == CCT_MODE)
            duty_cycle = 1.0 * cold / (cold + warm);
#else
            duty_cycle = 1.0 * WARM_CAL_FACTOR * warm / 255;
#endif
        }
        else
        {
            duty_cycle = 0.0;
        }
        if (pwm_w.config.duty_cycle != duty_cycle)
        {
            pwm_w.config.duty_cycle = duty_cycle;
            hal_pwm_para_chg(&pwm_w, pwm_w.config);
        }
    }
    if ((LED_TYPE_RGB != g_dimmer_led_type.type))
    {
#if (CW_HW_CONFIG == CCT_MODE)
        duty_cycle = 1.0 * (cold + warm) / 255;
#else
        duty_cycle = 1.0 * COLD_CAL_FACTOR * cold / 255;
#endif
        if (pwm_c.config.duty_cycle != duty_cycle)
        {
            pwm_c.config.duty_cycle = duty_cycle;
            hal_pwm_para_chg(&pwm_c, pwm_c.config);
        }
    }
    return 0;
}
