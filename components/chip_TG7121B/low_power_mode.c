#include <stdbool.h>
#ifndef CONFIG_KERNEL_NONE
#include <aos/aos.h>
#include "k_api.h"
#include "platform.h"
#include "io_config.h"
#include "sleep.h"
#include "reg_syscfg.h"
#include "tg7121b_lpm.h"

bool mac_sleep();
void deep_sleep();
extern bool os_tick_updated;
void board_console_init();
void board_console_deinit();
static bool sleep_flag;
static wakeup_cb_t s_wakeup_cb = NULL;

void tg7121b_sleep_enable(void)
{
    printf("tg7121 sleep\r\n");
    sleep_flag = true;
}

void tg7121b_sleep_disable(void)
{
    printf("tg7121 wakeup\r\n");
    sleep_flag = false;
}

void tg7121b_standby_enable(void)
{
    struct deep_sleep_wakeup wakeup;

    memset(&wakeup, 0, sizeof(wakeup));
    wakeup.pb15 = 1;
    wakeup.pb15_rising_edge = 0;

    enter_deep_sleep_mode_lvl2_lvl3(&wakeup);
}

uint32_t tg7121b_get_bootup_reason(void)
{
    uint32_t reason = SYSCFG->BKD[0];
    //printf("get bt reason:0x%08x\n", reason);
    return reason;
}

int32_t tg7121b_set_bootup_reason(uint32_t reason)
{
    SYSCFG->BKD[0] = reason;
    //printf("set bt reason:0x%08x\n", reason);
    return 0;
}

void krhino_idle_hook()
{
    CPSR_ALLOC();
    RHINO_CRITICAL_ENTER();
    if (sleep_flag == false)
    {
        RHINO_CRITICAL_EXIT();
        return;
    }
    board_console_deinit();
    if (os_tick_updated && mac_sleep())
    {
        os_tick_updated = false;
        deep_sleep();
    }
    board_console_init();

    if (s_wakeup_cb)
    {
        s_wakeup_cb(NULL);
    }

    RHINO_CRITICAL_EXIT();
}

int32_t os_sleep_duration_get()
{
    int32_t suspend_tick = aos_kernel_suspend();

    return (uint32_t)OSTICK_HS_STEP_INC(CONFIG_SYSTICK_HZ, (uint32_t)suspend_tick);
}

void tg7121b_io_wakeup_handler_unregister()
{
    s_wakeup_cb = NULL;
}

void tg7121b_io_wakeup_handler_register(wakeup_cb_t wakeup_cb)
{
    s_wakeup_cb = wakeup_cb;
}
#endif
