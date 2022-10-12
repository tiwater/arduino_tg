#include <drv/pwm.h>
#include "reg_timer_type.h"
#include "app_config.h"

#define TIM_CCER_CCxE_MASK  ((uint32_t)(TIMER_CCER_CC1E | TIMER_CCER_CC2E | TIMER_CCER_CC3E | TIMER_CCER_CC4E))
#define TIM_CCER_CCxNE_MASK ((uint32_t)(TIMER_CCER_CC1NE | TIMER_CCER_CC2NE | TIMER_CCER_CC3NE))

uint32_t CSI_TIM_MSP_Init(void *inst,uint32_t idx,bool pwm_mode);
void CSI_TIM_MSP_DeInit(uint32_t idx,bool pwm_mode);

csi_error_t csi_pwm_init(csi_pwm_t *pwm, uint32_t idx)
{
    pwm->dev.idx = idx;
    pwm->dev.reg_base = CSI_TIM_MSP_Init(pwm,idx,true);
    reg_timer_t *reg = (reg_timer_t *)pwm->dev.reg_base;

    reg->CR1 |= TIMER_CR1_URS; 
    reg->EGR = TIMER_EGR_UG;
    return CSI_OK;
}

void csi_pwm_uninit(csi_pwm_t *pwm)
{
    CSI_TIM_MSP_DeInit(pwm->dev.idx,true);
}

csi_error_t csi_pwm_out_config(csi_pwm_t *pwm,
                               uint32_t channel,
                               uint32_t period_us,
                               uint32_t pulse_width_us,
                               csi_pwm_polarity_t polarity)
{
    uint8_t oc_polarity;
    uint32_t tmpccrx;
    if(pwm->dev.idx > 3)
    {
        return CSI_ERROR;
    }

    if(polarity == PWM_POLARITY_HIGH)
    {
        oc_polarity = 0;
    }
    else
    {
        oc_polarity = TIMER_CCER_CC1P_MASK;
    }

    reg_timer_t *reg = (reg_timer_t *)pwm->dev.reg_base;

    if(period_us > 65536){
        reg->PSC = SDK_HCLK_MHZ*1000 - 1;
        reg->ARR = period_us/1000 - 1;
        tmpccrx = (period_us/1000)*pulse_width_us/period_us;
    }
    else{
        reg->PSC = SDK_HCLK_MHZ - 1;
        reg->ARR = period_us - 1;
        tmpccrx = period_us*pulse_width_us/period_us;
    }

    switch(channel)
    {
    case 0:
        reg->CCR1 = tmpccrx;
        reg->CCMR1 &= ~(TIMER_CCMR1_OC1M|TIMER_CCMR1_CC1S);
        reg->CCMR1 |= (TIMER_CCMR1_OC1M_2 | TIMER_CCMR1_OC1M_1);

        reg->CCER &= ~TIMER_CCER_CC1P;
        reg->CCER |= oc_polarity;

        reg->CCMR1 |= TIMER_CCMR1_OC1PE;
        reg->CCMR1 &= ~TIMER_CCMR1_OC1FE;
        reg->CCMR1 |= 0;
    break;
    case 1:
        reg->CCR2 = tmpccrx;
        reg->CCMR1 &= ~(TIMER_CCMR1_OC2M|TIMER_CCMR1_CC2S);
        reg->CCMR1 |= ((TIMER_CCMR1_OC1M_2 | TIMER_CCMR1_OC1M_1)<<8U);

        reg->CCER &= ~TIMER_CCER_CC2P;
        reg->CCER |= (oc_polarity<<4U);

        reg->CCMR1 |= TIMER_CCMR1_OC2PE;
        reg->CCMR1 &= ~TIMER_CCMR1_OC2FE;
        reg->CCMR1 |= (0 << 8U);
    break;
    case 2:
        reg->CCR3 = tmpccrx;
        reg->CCMR2 &= ~(TIMER_CCMR2_OC3M|TIMER_CCMR2_CC3S);
        reg->CCMR2 |= (TIMER_CCMR1_OC1M_2 | TIMER_CCMR1_OC1M_1);

        reg->CCER &= ~TIMER_CCER_CC3P;
        reg->CCER |= (oc_polarity<<8U);

        reg->CCMR2 |= TIMER_CCMR2_OC3PE;
        reg->CCMR2 &= ~TIMER_CCMR2_OC3FE;
        reg->CCMR2 |= 0;

    break;
    case 3:
        reg->CCR4 = tmpccrx;
        reg->CCMR2 &= ~(TIMER_CCMR2_OC4M|TIMER_CCMR2_CC4S);
        reg->CCMR2 |= (TIMER_CCMR1_OC1M_2 | TIMER_CCMR1_OC1M_1)<<8U;

        reg->CCER &= ~TIMER_CCER_CC4P;
        reg->CCER |= (oc_polarity<<12U);

        reg->CCMR2 |= TIMER_CCMR2_OC4PE;
        reg->CCMR2 &= ~TIMER_CCMR2_OC4FE;
        reg->CCMR2 |= 0;
    break;
    default:
    break;
    }

    return CSI_OK;
}

csi_error_t csi_pwm_out_start(csi_pwm_t *pwm, uint32_t channel)
{
    uint32_t tmp;
    reg_timer_t *reg = (reg_timer_t *)pwm->dev.reg_base;

    tmp = TIMER_CCER_CC1E << ((channel*4) & 0x1FU);
    reg->CCER &= ~tmp;
    reg->CCER |= (uint32_t)(1 << ((channel*4) & 0x1FU));

    if(pwm->dev.idx > 0)
    {
        reg->BDTR |= TIMER_BDTR_MOE;
    }

    reg->CR1 |= TIMER_CR1_CEN;
    return CSI_OK;
}

void csi_pwm_out_stop(csi_pwm_t *pwm, uint32_t channel)
{
    uint32_t tmp;
    reg_timer_t *reg = (reg_timer_t *)pwm->dev.reg_base;

    tmp = TIMER_CCER_CC1E << ((channel*4) & 0x1FU);
    reg->CCER &= ~tmp;
    reg->CCER |= (uint32_t)(0 << ((channel*4) & 0x1FU));

    if ((reg->CCER & TIM_CCER_CCxE_MASK) == 0UL) 
    { 
      if((reg->CCER & TIM_CCER_CCxNE_MASK) == 0UL)
      {
        reg->CR1 &= ~(TIMER_CR1_CEN);
        if(pwm->dev.idx > 0)
        {
            reg->BDTR &= ~(TIMER_BDTR_MOE);
        }
      }
    }
}
