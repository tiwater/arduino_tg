/*
 * Copyright (C) 2019-2022 Alibaba Group Holding Limited
 */

#ifndef _DIMMER_H_
#define _DIMMER_H_

#if defined(__cplusplus)  /* For C++ compiler support */
extern "C" {
#endif

#define CW_MODE     0
#define CCT_MODE    1

#define RED_CAL_FACTOR      1
#define GREEN_CAL_FACTOR    1
#define BLUE_CAL_FACTOR     1
#define WARM_CAL_FACTOR     1
#define COLD_CAL_FACTOR     1

/* PWM output frequency */
#define PWM_FRE_RED     16000
#define PWM_FRE_GREEN   16000
#define PWM_FRE_BLUE    16000
#define PWM_FRE_COLD    16000
#define PWM_FRE_WARM    16000
/* Dynamic adjust max/min brightness and temperature by HardWare characteristics */
#define MIN_BRIGHTNESS  0
#define MAX_BRIGHTNESS  100
#define MIN_TEMPERATURE 2000
#define MAX_TEMPERATURE 7000
#define CURVE_FITTING      1
#define CW_HW_CONFIG    CW_MODE

#define DEF_ADC_RESOLUTION       16
#define DEF_SAMPLE_CHANNELS      1
#define DEF_ADC_SAMPLE_CH        10
#define DEF_FREQ_GPIO_PIN        13
#define SAMPLES_PER_FRAME        128
#define DEF_SAMPLE_RATE          4000
#define RESAMPLE_FACTOR          1
#define DEF_SAMPLE_RATE_MEASURE  0

typedef enum _LED_HW_TYPES {
    LED_TYPE_C = 0,
    LED_TYPE_CW,
    LED_TYPE_RGB,
    LED_TYPE_RGBC,
    LED_TYPE_RGBCW,
    LED_TYPE_UNKNOWN
} led_hw_type_t;

typedef enum _LED_POWER_TYPES {
    LED_POWER_HALF = 0,
    LED_POWER_FULL,
    LED_POWER_UNKNOWN
} led_power_type_t;

typedef struct _LED_TYPES {
    led_hw_type_t type:4;
    led_power_type_t power:4;
} led_type_t;

typedef enum _LED_TYPES_GPIO_VALUE {
    LED_TYPE_RGB_GPIO_VALUE = 0,          /* gpio7:5:4  ==  0:0:0 */
    LED_TYPE_C_GPIO_VALUE,                /* gpio7:5:4  ==  0:0:1 */
    LED_TYPE_CW_GPIO_VALUE,               /* gpio7:5:4  ==  0:1:0 */
    LED_TYPE_RGBC_GPIO_VALUE,             /* gpio7:5:4  ==  0:1:1 */
    LED_TYPE_RGBCW_GPIO_VALUE,            /* gpio7:5:4  ==  1:0:0 */
    LED_TYPE_RGBCW_FULL_POWER_GPIO_VALUE, /* gpio7:5:4  ==  1:0:1 */
    LED_TYPE_RESERVED_GPIO_VALUE,         /* gpio7:5:4  ==  1:1:0 */
    LED_TYPE_CW_FULL_POWER_GPIO_VALUE,    /* gpio7:5:4  ==  1:1:1 */
    LED_TYPE_MAX_GPIO_VALUE
} led_type_gpio_value_t;

int dimmer_init(void);
void dimmer_deinit(void);

led_type_t dimmer_get_type(void);

int dimmer_set_color(uint8_t red, uint8_t green, uint8_t blue, uint8_t cold, uint8_t warm);

#if defined(__cplusplus)  /* For C++ compiler support */
}
#endif

#endif /* End of  _DIMMER_H_  */
