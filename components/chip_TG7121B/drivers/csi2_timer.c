#include <drv/common.h>
#include <drv/timer.h>
#include "app_config.h"
#include "reg_timer_type.h"

/* The counter of a timer instance is disabled only if all the CCx and CCxN
   channels have been disabled */
#define TIM_CCER_CCxE_MASK  ((uint32_t)(TIMER_CCER_CC1E | TIMER_CCER_CC2E | TIMER_CCER_CC3E | TIMER_CCER_CC4E))
#define TIM_CCER_CCxNE_MASK ((uint32_t)(TIMER_CCER_CC1NE | TIMER_CCER_CC2NE | TIMER_CCER_CC3NE))

uint32_t CSI_TIM_MSP_Init(void *inst,uint32_t idx,bool pwm_mode);
void CSI_TIM_MSP_DeInit(uint32_t idx,bool pwm_mode);

csi_error_t csi_timer_init(csi_timer_t *timer, uint32_t idx)
{
    timer->dev.idx = idx;
    timer->dev.reg_base = CSI_TIM_MSP_Init(timer,idx,false);
    reg_timer_t *reg = (reg_timer_t *)timer->dev.reg_base;

    reg->CR1 |= TIMER_CR1_URS; 
    reg->EGR = TIMER_EGR_UG;

    return CSI_OK;
}

csi_error_t csi_timer_start(csi_timer_t *timer, uint32_t timeout_us)
{
    reg_timer_t *reg = (reg_timer_t *)timer->dev.reg_base;
    if(timeout_us > 65536){
        reg->PSC = SDK_HCLK_MHZ*1000 - 1;
        reg->ARR = timeout_us/1000 - 1;
    }
    else{
        reg->PSC = SDK_HCLK_MHZ - 1;
        reg->ARR = timeout_us - 1;
    }

    reg->IDR &= ~TIMER_IDR_UIE_MASK;
    reg->IER |= TIMER_IDR_UIE_MASK;

    reg->CR1 |= TIMER_CR1_CEN;

    return CSI_OK;
}

void csi_timer_stop(csi_timer_t *timer)
{
    reg_timer_t *reg = (reg_timer_t *)timer->dev.reg_base;
    reg->IDR &= ~TIMER_CR1_CEN;
    if((reg->CCER & TIM_CCER_CCxE_MASK)==0)
    {
        if((reg->CCER & TIM_CCER_CCxNE_MASK)==0)
        {
            reg->CR1 &= ~(TIMER_CR1_CEN);
        }
    }
}

uint32_t csi_timer_get_remaining_value(csi_timer_t *timer)
{
    reg_timer_t *reg = (reg_timer_t *)timer->dev.reg_base;
    return ((reg->ARR) - (reg->CNT));
}

uint32_t csi_timer_get_load_value(csi_timer_t *timer)
{
    reg_timer_t *reg = (reg_timer_t *)timer->dev.reg_base;
    return reg->ARR;
}

bool csi_timer_is_running(csi_timer_t *timer)
{
    return true;
}

csi_error_t csi_timer_attach_callback(csi_timer_t *timer, void *callback, void *arg)
{
    timer->callback = callback;
    timer->arg = arg;

    return CSI_OK;
}

void csi_timer_detach_callback(csi_timer_t *timer)
{
    timer->callback = NULL;
}

void csi_timer_uninit(csi_timer_t *timer)
{
    CSI_TIM_MSP_DeInit(timer->dev.idx,false);
}

void csi_timer_isr(csi_timer_t *timer)
{
    reg_timer_t *reg = (reg_timer_t *)timer->dev.reg_base;
    if((reg->RIF & TIMER_RIF_UIF)!=0)
    {
        if((reg->IVS & TIMER_IDR_UIE_MASK) == TIMER_IDR_UIE_MASK)
        {
            reg->ICR |= TIMER_IDR_UIE_MASK;
            timer->callback(timer,NULL);
        }
    }
}
