/*
 * Copyright (C) 2019-2020 Alibaba Group Holding Limited
 */


/******************************************************************************
 * @file     pinmux.c
 * @brief    source file for the pinmux
 * @version  V1.0
 * @date     02. June 2017
 * @vendor   csky
 * @chip     pangu
 ******************************************************************************/
#include <stdint.h>
#include <drv/pin.h>

void af_io_init(gpio_pin_t *pin,enum GPIO_AF af);
void set_gpio_mode(gpio_pin_t *pin);
void ana_func1_io_init(uint8_t ain);

csi_error_t csi_pin_set_mux(pin_name_t pin_name, pin_func_t pin_func)
{
    if(pin_func == PIN_FUNC_GPIO)
    {
        set_gpio_mode((gpio_pin_t *)&pin_name);
    }else if(pin_func == PIN_FUNC_ANA)
    {
        ana_func1_io_init(pin_name);
    }else if(pin_func <= AF_I2S_CLK)
    {
        af_io_init((gpio_pin_t *)&pin_name,pin_func);
    }
    return 0;
}

int32_t drv_pinmux_config(pin_name_t pin, pin_func_t pin_func)
{
    return csi_pin_set_mux(pin,pin_func);
}