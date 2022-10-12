#ifndef __GENIE_SAL_OS_H__
#define __GENIE_SAL_OS_H__

#include <errno.h>

#ifdef CONFIG_GENIE_MESH_PORT_YOC
#define hal_malloc malloc
#define hal_free   free

#include <aos/hal/timer.h>
#include <aos/hal/gpio.h>
#include <aos/hal/flash.h>
#include <aos/hal/pwm.h>
#include <aos/hal/uart.h>
#include <aos/hal/adc.h>
#include <aos/cli.h>
#include <aos/aos.h>
#include <aos/kv.h>
#else
#include <hal/hal.h>
#endif

#endif
