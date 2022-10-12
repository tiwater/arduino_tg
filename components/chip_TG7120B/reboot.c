/*
 * Copyright (C) 2019-2020 Alibaba Group Holding Limited
 */

/******************************************************************************
 * @file     reboot.c
 * @brief    source file for the reboot
 * @version  V1.0
 * @date     04. April 2019
 * @vendor   csky
 * @chip     pangu
 ******************************************************************************/

#include <soc.h>
#include <drv/wdt.h>
#include <mcu_phy_bumbee.h>
#include "jump_function.h"

#define RST_FROM_ROM_BOOT  0
#define RST_FROM_APP_RST_HANDLER 1

extern size_t cpu_intrpt_save(void);
extern void sram_reset_handler(void);
extern void set_sleep_flag(int flag);

__attribute__((section(".__sram.code"))) void config_reset_handler(int rstMod)
{
    if(rstMod==RST_FROM_ROM_BOOT)
    {
        set_sleep_flag(0);
    }
    else
    {
	    JUMP_FUNCTION(WAKEUP_PROCESS) = (uint32_t)sram_reset_handler;
		extern void trap_c(uint32_t *regs);
		extern void sram_HardFault_Handler(void);
		JUMP_FUNCTION(CK802_TRAP_C) = (uint32_t)&sram_HardFault_Handler;           //register trap irq handler
        set_sleep_flag(1);
    }
}

__attribute__((section(".__sram.code"))) void drv_reboot(int cmd)
{
    csi_irq_save();

    AP_PCR->CACHE_BYPASS = 1;
//    hal_flash_write_disable();
//    AP_SPIF->config = 0;
    AP_PCR->CACHE_RST = 0;

    // config reset path , from rom or app 
    config_reset_handler(cmd);
    /**
        config reset casue as RSTC_WARM_NDWC
        reset path walkaround dwc
    */
    AP_AON->SLEEP_R[0]=4;


    *(volatile uint32_t *) 0x40000004 = 0x00;

    /* waiting for reboot */
    while (1);
}
