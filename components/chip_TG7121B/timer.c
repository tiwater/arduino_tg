/*
 * Copyright (C) 2019-2020 Alibaba Group Holding Limited
 */

#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include "csi_core.h"
#include "drv/timer.h"
#ifdef CONFIG_CSI_V2
#include "drv/porting.h"
#endif

extern int32_t drv_get_cpu_freq(int idx);
extern uint32_t csi_coret_get_value(void);
extern uint32_t csi_coret_get_load(void);
extern long long aos_now_ms(void);

#ifndef CONFIG_CLOCK_BASETIME
#define CONFIG_CLOCK_BASETIME 1003939200
#endif


/*
 * return : the coretim register count in a tick(calculate by TICK_PER_SEC)
 */

static uint32_t coretim_getpass(void)
{
    uint32_t cvalue;
    int      value;

    uint32_t loadtime;
#ifdef __arm__
    loadtime = SysTick->LOAD;
#else
    loadtime = csi_coret_get_load();
#endif

#ifdef __arm__
    cvalue = SysTick->VAL;
#else
    cvalue = csi_coret_get_value();
#endif
    value = loadtime - cvalue;

    return value > 0 ? value : 0;
}

int coretimspec(struct timespec *ts)
{
    uint32_t msecs;
    uint32_t secs;
    uint32_t nsecs = 0;

    if (ts == NULL) {
        return -1;
    }

    while (1) {
        uint64_t clk1, clk2;
        uint32_t pass1, pass2;

        clk1 = aos_now_ms();
        pass1 = coretim_getpass();
        clk2 = aos_now_ms();
        pass2 = coretim_getpass();

        if (clk1 == clk2 && pass2 > pass1) {
            msecs = clk1;
#ifdef CONFIG_CSI_V2
            nsecs = pass2 * (NSEC_PER_SEC / soc_get_cur_cpu_freq());
#else            
            nsecs = pass2 * (NSEC_PER_SEC / drv_get_cpu_freq(0));
#endif            
            nsecs = 0;
            break;
        }
    }

    secs = msecs / MSEC_PER_SEC;

    nsecs += (msecs - (secs * MSEC_PER_SEC)) * NSEC_PER_MSEC;

    if (nsecs > NSEC_PER_SEC) {
        nsecs -= NSEC_PER_SEC;
        secs += 1;
    }

    ts->tv_sec = (time_t)secs;
    ts->tv_nsec = (long)nsecs;
    return 0;
}

