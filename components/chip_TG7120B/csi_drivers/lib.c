/*
 * Copyright (C) 2019-2020 Alibaba Group Holding Limited
 */

/******************************************************************************
 * @file     lib.c
 * @brief    source file for the lib
 * @version  V1.0
 * @date     02. June 2017
 ******************************************************************************/

#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <soc.h>
#include <csi_core.h>
#include <drv/usart.h>
#include "rom_sym_def.h"
#include <clock.h>

extern uint32_t csi_coret_get_load(void);
extern uint32_t csi_coret_get_value(void);

extern int32_t csi_usart_putchar(usart_handle_t handle, uint8_t ch);
extern int32_t csi_usart_getchar(usart_handle_t handle, uint8_t *ch);
#ifndef SYSTEM_CLOCK
#define  SYSTEM_CLOCK        (48000000)
#endif
//static void _mdelay(void)
//{
//    uint32_t load = csi_coret_get_load();
//    uint32_t start = csi_coret_get_value();
//    uint32_t cur;
//    uint32_t cnt = (SYSTEM_CLOCK / 1000);
//
//    while (1) {
//        cur = csi_coret_get_value();
//
//        if (start > cur) {
//            if (start - cur >= cnt) {
//                return;
//            }
//        } else {
//            if (cur + load - start > cnt) {
//                return;
//            }
//        }
//    }
//}

void mdelay(uint32_t ms)
{
    if (ms == 0) {
        return;
    }

    while (ms--) {
        WaitUs(1000);
    }
}

void udelay(uint32_t us)
{
    if (us == 0) {
        return;
    }

    while (us--) {
        WaitUs(1);
    }
}
