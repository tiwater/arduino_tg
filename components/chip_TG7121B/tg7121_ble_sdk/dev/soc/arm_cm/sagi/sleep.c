#include "reg_v33_rg.h"
#include "ARMCM3.h"
#include "sleep.h"
#include "reg_sysc_awo.h"
#include "spi_flash.h"
#include "field_manipulate.h"
#include "platform.h"
#include "io_config.h"
#define ISR_VECTOR_ADDR ((uint32_t *)(0x400000))

static uint32_t sleep_msp;
void cpu_sleep_asm(void);
void cpu_recover_asm(void);
XIP_BANNED void cpu_sleep_recover_init()
{
    V33_RG->SFT_CTRL0F = (uint32_t)cpu_recover_asm >> 1;
}

XIP_BANNED void store_msp()
{ 
    sleep_msp = __get_MSP();
}

XIP_BANNED void restore_msp()
{
    __set_MSP(sleep_msp);
}

XIP_BANNED void before_wfi()
{
    //switch to hsi clock
    REG_FIELD_WR(SYSC_AWO->PD_AWO_CLK_CTRL,SYSC_AWO_CLK_SEL_HBUS_L0,1);//24M
    DELAY_US(3);
    REG_FIELD_WR(SYSC_AWO->PD_AWO_ANA0,SYSC_AWO_AWO_EN_DPLL,0);
}

XIP_BANNED void after_wfi()
{

    pll_enable();
    clk_switch();
}

XIP_BANNED void power_down_config()
{
    cpu_sleep_recover_init(); 
    REG_FIELD_WR(SYSC_AWO->PIN_SEL0,SYSC_AWO_QSPI0_EN,0);
    SYSC_AWO->IO[2].PUPD=0XFc0F03f0;
    SYSC_AWO->PWR_CTRL0 =
      FIELD_BUILD(SYSC_AWO_PD_EN_SEC, 0x1)
    | FIELD_BUILD(SYSC_AWO_PD_EN_BLE, 0x1)
    | FIELD_BUILD(SYSC_AWO_PD_EN_PER, 0x1)
    | FIELD_BUILD(SYSC_AWO_PD_EN_SRAM, 0x30)
    | FIELD_BUILD(SYSC_AWO_PU_EN_SEC, 0x1)
    | FIELD_BUILD(SYSC_AWO_PU_EN_BLE, 0x1)
    | FIELD_BUILD(SYSC_AWO_PU_EN_PER, 0x1)
    | FIELD_BUILD(SYSC_AWO_PU_EN_SRAM, 0x3f);
    SYSC_AWO->PWR_CTRL2 =
      FIELD_BUILD(SYSC_AWO_PD_SRAM_DS_EN, 1)
    | FIELD_BUILD(SYSC_AWO_PMU_CLK_DLY, 0x3F)
    | FIELD_BUILD(SYSC_AWO_PMU_FSM_DLY, 0xF)
    | FIELD_BUILD(SYSC_AWO_PWR_SW_STB, 0x31)
    | FIELD_BUILD(SYSC_AWO_MEM_DS_STB, 0x0);
    SYSC_AWO->PWR_CTRL3 =
      FIELD_BUILD(SYSC_AWO_V33_SLP_EN, 0x1)
    | FIELD_BUILD(SYSC_AWO_MODE_CLK_ONLY, 0x0);
    V33_RG->TRIM0 =
      FIELD_BUILD(V33_RG_SPI_CODE_L,0x0);
    V33_RG->TRIM1 =
      FIELD_BUILD(V33_RG_SPI_CODE_H, 0x0)
    | FIELD_BUILD(V33_RG_RCO_I_ADJ, 0x0);
    V33_RG->TRIM2 =
      FIELD_BUILD(V33_RG_PD_AMIC, 0x0)
    | FIELD_BUILD(V33_RG_LATCH_GPIO, 0)
    | FIELD_BUILD(V33_RG_PD_ADC12, 0x1)
    | FIELD_BUILD(V33_RG_PD_GPIO, 0x0)
    | FIELD_BUILD(V33_RG_BOR_EN, 0x0)
    | FIELD_BUILD(V33_RG_BAT_DTCT_EN, 0x0)
    | FIELD_BUILD(V33_RG_PD_BIM, 0x1);
    V33_RG->TRIM3 =
      FIELD_BUILD(V33_RG_PD_GPIO_SEL, 0x0)
    | FIELD_BUILD(V33_RG_PD_ADC24, 0x1)
    | FIELD_BUILD(V33_RG_LPLDO_TRIM, 0x4)
    | FIELD_BUILD(V33_RG_PD_TK, 0x1)
    | FIELD_BUILD(V33_RG_PD_LCD, 0x0);
    V33_RG->WKUP_CTRL0 = 
      FIELD_BUILD(V33_RG_WKUP_MSK, 0x08);
    V33_RG->SWD_IO_WKUP_EN = 
      FIELD_BUILD(V33_RG_SWD_IO_WKUP_EN, 0x0);
    V33_RG->PWR_CTRL0 =
      FIELD_BUILD(V33_RG_LPLDO_PD_EN, 0)
    | FIELD_BUILD(V33_RG_HPLDO_PD_EN, 0x1)
    | FIELD_BUILD(V33_RG_HSE_BUF_PD_EN, 0x1)
    | FIELD_BUILD(V33_RG_HSE_BUF_PU_EN, 0x1)
    | FIELD_BUILD(V33_RG_HS_GATE_PD_EN, 0x1)
    | FIELD_BUILD(V33_RG_HS_GATE_PU_EN, 0x1)
    | FIELD_BUILD(V33_RG_DCDC_PDPU_EN, 0x0)
    | FIELD_BUILD(V33_RG_DCDC_PDPU_MD, 0x0);
    V33_RG->PWR_CTRL1 =
      FIELD_BUILD(V33_RG_LSI_PD_EN, 0)
    | FIELD_BUILD(V33_RG_LSE_PD_EN, 0x1)
    | FIELD_BUILD(V33_RG_LSE_PU_EN, 0x1)
    | FIELD_BUILD(V33_RG_HSI_PD_EN, 0x1)
    | FIELD_BUILD(V33_RG_HSI_PU_EN, 0x1)
    | FIELD_BUILD(V33_RG_HSE_PD_EN, 0x1)
    | FIELD_BUILD(V33_RG_HSE_PU_EN, 0x1);
    SCB->SCR |= (1<<2);
}

NOINLINE XIP_BANNED static void cpu_flash_deep_sleep_and_recover()
{
    spi_flash_xip_stop();
    spi_flash_deep_power_down();
    power_down_config();
    cpu_sleep_asm();
    SCB->CCR |= SCB_CCR_UNALIGN_TRP_Msk;
    SCB->VTOR = (uint32_t)ISR_VECTOR_ADDR;
    V33_RG->TRIM2 =
      FIELD_BUILD(V33_RG_PD_AMIC, 0x0)
    | FIELD_BUILD(V33_RG_LATCH_GPIO, 1)
    | FIELD_BUILD(V33_RG_PD_ADC12, 0x1)
    | FIELD_BUILD(V33_RG_PD_GPIO, 0)
    | FIELD_BUILD(V33_RG_BOR_EN, 0x0)
    | FIELD_BUILD(V33_RG_BAT_DTCT_EN, 0x0)
    | FIELD_BUILD(V33_RG_PD_BIM, 0x1);
    V33_RG->TRIM2 =
      FIELD_BUILD(V33_RG_PD_AMIC, 0x0)
    | FIELD_BUILD(V33_RG_LATCH_GPIO, 0)
    | FIELD_BUILD(V33_RG_PD_ADC12, 0x1)
    | FIELD_BUILD(V33_RG_PD_GPIO, 0)
    | FIELD_BUILD(V33_RG_BOR_EN, 0x0)
    | FIELD_BUILD(V33_RG_BAT_DTCT_EN, 0x0)
    | FIELD_BUILD(V33_RG_PD_BIM, 0x1);
    spi_flash_init();
    spi_flash_release_from_deep_power_down();
    DELAY_US(8);
    spi_flash_xip_start();
}

void deep_sleep()
{
    // systick_stop();
    cpu_flash_deep_sleep_and_recover();
    // systick_start();
}

XIP_BANNED void lvl2_lvl3_mode_prepare()
{
    power_down_config();
    REG_FIELD_WR(V33_RG->PWR_CTRL0,V33_RG_LPLDO_PD_EN,1);
    REG_FIELD_WR(V33_RG->PWR_CTRL1,V33_RG_LSI_PD_EN, 1);
}


XIP_BANNED void enter_deep_sleep_mode_lvl2_lvl3()
{
    spi_flash_xip_stop();
    spi_flash_deep_power_down();
    lvl2_lvl3_mode_prepare();     
    __WFI();
    while(1);
}
