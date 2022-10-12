/*
 * Copyright (C) 2017-2019 Alibaba Group Holding Limited
 */


/******************************************************************************
 * @file     system.c
 * @brief    CSI Device System Source File
 * @version  V1.0
 * @date     10. April 2019
 ******************************************************************************/

#include <soc.h>
#include <drv/irq.h>
#include <drv/pin.h>
#include "platform.h"
#include <stdbool.h>
#include "systick.h"
#include "lstrng.h"
#ifndef CONFIG_KERNEL_NONE
#include <aos/aos.h>
#include <yoc/yoc.h>
#include <devices/devicelist.h>
#include "aos/cli.h"
#include "k_api.h"
#include "common.h"
#include "io_config.h"
#include "app_init.h"
#include "calc_acc.h"
#include "platform.h"
#include "yoc/partition.h"
#include "cmsis_gcc.h"
#include "ls_dbg.h"
#define TAG  "SYSTEM"
bool os_tick_updated;
void mdelay(uint32_t ms)
{
    DELAY_US(ms*1000);
}

void krhino_tick_proc(void);
void sys_init_ll(void);
void ble_ll_init(void);
void ble_ll_isr(void);
void ke_timer_os_wrapper_init(void (*start)(void **,uint32_t),void (*stop)(void **),void (*tick_handler)(),void (*wakeup_compensate)(uint32_t));
uint32_t ke_timer_os_tick_inc(uint32_t hs,uint32_t hus,bool compensation);
void ke_timer_os_tick_start(uint32_t hs,uint32_t hus);
void os_wrapped_timer_callback(uint32_t param);
void eif_init(void *read,void *write,void *flow_on,void *flow_off);
void ll_hci_read(uint8_t *bufptr, uint32_t size, void (*callback)(void *,uint8_t), void* dummy);
void ll_hci_write(uint8_t *bufptr, uint32_t size, void (*callback)(void *,uint8_t), void* dummy);
void ll_hci_flow_on(void);
bool ll_hci_flow_off(void);
void ble_le5010_register(void);
void hci_h4_driver_init(void);
void ble_ll_task_event_set(void);
int dut_hal_mac_get(uint8_t addr[6]);
int32_t os_sleep_duration_get();
extern int32_t (*os_sleep_duration_get_fn)();

static void timer_callback(void *timer,void *arg)
{
    os_wrapped_timer_callback((uint32_t)arg);
    aos_timer_free(arg);
}

static void timer_start(void **p_timer_hdl,uint32_t delay)
{
    aos_timer_new((aos_timer_t *)p_timer_hdl,timer_callback,p_timer_hdl,delay,0);
}

static void timer_stop(void **timer_hdl)
{
    aos_timer_stop((aos_timer_t *)timer_hdl);
    aos_timer_free((aos_timer_t *)timer_hdl);
}

static void os_tick_handler()
{
    uint32_t tick_inc = ke_timer_os_tick_inc(OSTICK_HS_INC(CONFIG_SYSTICK_HZ),OSTICK_HUS_INC(CONFIG_SYSTICK_HZ),false);
    while(tick_inc--)
    {
        krhino_tick_proc();
    }
    os_tick_updated = true;
}

static void wakeup_compensation(uint32_t hus)
{
    uint32_t ticks = CEILING(hus,2*1000000/CONFIG_SYSTICK_HZ);
    aos_kernel_resume(ticks);
    ke_timer_os_tick_inc(OSTICK_HS_STEP_INC(CONFIG_SYSTICK_HZ,ticks),OSTICK_HUS_STEP_INC(CONFIG_SYSTICK_HZ,ticks),true);
}

void ll_stack_reset_hook()
{
    ke_timer_os_tick_start(OSTICK_HS_INC(CONFIG_SYSTICK_HZ),OSTICK_HUS_INC(CONFIG_SYSTICK_HZ));
}

void ll_get_mac_addr(uint8_t *buf)
{
    uint8_t addr[6];
    dut_hal_mac_get(addr);
    buf[0] = addr[5];
    buf[1] = addr[4];
    buf[2] = addr[3];
    buf[3] = addr[2];
    buf[4] = addr[1];
    buf[5] = addr[0];
}

void platform_init()
{
    sys_init_ll();
    os_sleep_duration_get_fn = os_sleep_duration_get;
    eif_init(ll_hci_read,ll_hci_write,ll_hci_flow_on,ll_hci_flow_off);
    ke_timer_os_wrapper_init(timer_start,timer_stop,os_tick_handler,wakeup_compensation);
    ble_le5010_register();
    ble_ll_init();
    ke_timer_os_tick_start(OSTICK_HS_INC(CONFIG_SYSTICK_HZ),OSTICK_HUS_INC(CONFIG_SYSTICK_HZ));
    hci_h4_driver_init();
}

void ble_isr()
{
    aos_kernel_intrpt_enter();
    bool tick_only = (*(volatile uint32_t *)0x5000001c) == 0x40;
    ble_ll_isr();
    if(tick_only == false)
    {
        ble_ll_task_event_set();
    }
    aos_kernel_intrpt_exit();
}
/*
void hardfault_print(uint32_t *msp,uint32_t *psp,uint32_t lr)
{
    LOGE(TAG,"lr:%x",lr);
    LOGE(TAG,"%x,%x,%x,%x",msp[-4],msp[-3],msp[-2],msp[-1]);
    LOGE(TAG, "%x,%x,%x,%x,%x,%x,%x,%x",msp[0],msp[1],msp[2],msp[3],msp[4],msp[5],msp[6],msp[7]);
    LOGE(TAG, "%x,%x,%x,%x,%x,%x,%x,%x",psp[0],psp[1],psp[2],psp[3],psp[4],psp[5],psp[6],psp[7]);
}

__attribute__((naked)) void HardFault_Handler()
{
    __asm("push {r4-r7}":::);
    uint32_t *msp = &((uint32_t *)__get_MSP())[4];
    uint32_t *psp = (uint32_t *)__get_PSP();
    uint32_t lr;
    __asm ("mov %0,r14": :"r"(lr):);
    hardfault_print(msp,psp,lr);
    while(1);
}
*/

int bt_crypto_rand(void *buf, size_t len)
{
    uint8_t *pdata = buf;
    if (buf == NULL || len <= 0) {
        return -1;
    }
    uint32_t random32bit;
    CPSR_ALLOC();
    RHINO_CRITICAL_ENTER();
    HAL_TRNG_Init();
    while (len >= 4) {
        HAL_TRNG_GenerateRandomNumber(&random32bit);
        memcpy(pdata,&random32bit,sizeof(uint32_t));
        len -= 4;
        pdata += 4;
    }

    if (len) {
        HAL_TRNG_GenerateRandomNumber(&random32bit);
        memcpy(pdata, &random32bit, len);
    }
    HAL_TRNG_DeInit();
    RHINO_CRITICAL_EXIT();
    return 0;
}

#endif

__attribute__((weak)) void pre_main(void)
{
#if  (!defined(CONFIG_KERNEL_RHINO))  &&  (!defined(CONFIG_NUTTXMM_NONE))  &&  (!defined(CONFIG_KERNEL_FREERTOS))
    extern void mm_heap_initialize();
    mm_heap_initialize();
#endif
    extern void main(void);
    main();
}

__attribute__((weak)) void SystemInit(void)
{
#ifndef CONFIG_KERNEL_NONE
    __disable_irq();
#else
    __enable_irq();
#endif

//    SysTick_Config(48000);

}

void drv_reboot()
{
    platform_reset(0);
}

void hal_flash_init()
{
    
}

void soc_dcache_clean()
{

}

void soc_icache_invalid()
{
    
}

uint32_t csi_tick_get_ms()
{
    return SYSTICK_TICKS2MS(systick_get_value());
}

__attribute__((aligned(4))) const csi_pinmap_t adc_pinmap[] = {
    {PB12,           0,     0,           PIN_FUNC_ANA },
    {PB13,           0,     1,           PIN_FUNC_ANA },
    {PC00,           0,     2,           PIN_FUNC_ANA },
    {PC01,           0,     3,           PIN_FUNC_ANA },
    {PA00,           0,     4,           PIN_FUNC_ANA },
    {PA01,           0,     5,           PIN_FUNC_ANA },
    {PA02,           0,     6,           PIN_FUNC_ANA },
    {PA03,           0,     7,           PIN_FUNC_ANA },
    {PA04,           0,     8,           PIN_FUNC_ANA },
    {0xFC,           0,     9,           0xFC },
    {0xFD,           0,     10,          0xFD },
    {0xFFU,         0xFFU, 0xFFU,       0xFFU },
};

