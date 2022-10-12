/*
 * Copyright (C) 2019-2020 Alibaba Group Holding Limited
 */

#ifndef __BOARD_CONFIG_H__
#define __BOARD_CONFIG_H__

#include <stdint.h>
//#include "pinmux.h"
#include "soc.h"
#include "pwm_config.h"

#define CONSOLE_UART_IDX  0

#define CONSOLE_TXD                0// PA10
#define CONSOLE_RXD                0// PA11
#define CONSOLE_TXD_FUNC           0// PA10_UART0_TX
#define CONSOLE_RXD_FUNC           0// PA11_UART0_RX

// i2c
#define EXAMPLE_IIC_IDX            0// 1
#define EXAMPLE_PIN_IIC_SDA        0// PC1
#define EXAMPLE_PIN_IIC_SCL        0// PC0
#define EXAMPLE_PIN_IIC_SDA_FUNC   0// PC1_I2C1_SDA
#define EXAMPLE_PIN_IIC_SCL_FUNC   0// PC0_I2C1_SCL

// adc
#define EXAMPLE_ADC_CH0            0// PA8
#define EXAMPLE_ADC_CH0_FUNC       0// PA8_ADC_A0
#define EXAMPLE_ADC_CH12           0// PA26
#define EXAMPLE_ADC_CH12_FUNC      0// PA26_ADC_A12

#define PIN_LED_R  PA01
#define PIN_LED_G  PA07
#define PIN_LED_B  PB08
#define PIN_LED_C  PB09
#define PIN_LED_W  PB11

#define PIN_LED_R_CHANNEL  PWM0_FUNC
#define PIN_LED_G_CHANNEL  PWM8_FUNC
#define PIN_LED_B_CHANNEL  PWM5_FUNC
#define PIN_LED_C_CHANNEL  PWM4_FUNC
#define PIN_LED_W_CHANNEL  PWM3_FUNC

#define PWM_LED_R_PORT     0
#define PWM_LED_G_PORT     8
#define PWM_LED_B_PORT     5
#define PWM_LED_C_PORT     4
#define PWM_LED_W_PORT     3

#endif
