#include <drv/adc.h>
#include <drv/common.h>
#include <csi_kernel.h>
#include "reg_lsadc_type.h"
#include "reg_syscfg.h"
#include "field_manipulate.h"
#include "sdk_config.h"
#include "systick.h"
#include <math.h>

#ifndef CONFIG_KERNEL_NONE
#define CSI_INTRPT_ENTER() aos_kernel_intrpt_enter()
#define CSI_INTRPT_EXIT()  aos_kernel_intrpt_exit()
#else
#define CSI_INTRPT_ENTER()
#define CSI_INTRPT_EXIT()
#endif

static uint16_t adc_count = 0;
static bool ch_set_flag = false;

uint32_t CSI_ADC_MSP_Init(void *inst,uint32_t idx);
void CSI_ADC_MSP_DeInit(uint32_t idx);


csi_error_t csi_adc_init(csi_adc_t *adc, uint32_t idx)
{
    uint32_t tmp_ccr = 0U;

    adc->dev.idx = idx;
    adc->dev.reg_base = CSI_ADC_MSP_Init(adc,idx);
    reg_adc_t *reg = (reg_adc_t *)adc->dev.reg_base;

    SET_BIT(reg->CR2, ADC_BINBUF_MASK | ADC_BINRES_MASK);

    tmp_ccr = FIELD_BUILD(ADC_VRPS,4) | FIELD_BUILD(ADC_BP,1) | FIELD_BUILD(ADC_VRBUFEN,1) | FIELD_BUILD(ADC_VCMEN,1) | FIELD_BUILD(ADC_VREFEN,1);
     tmp_ccr |= FIELD_BUILD(ADC_MSBCAL, 2)  |
                FIELD_BUILD(ADC_LPCTL, 1)   |
                FIELD_BUILD(ADC_GCALV, 0)   |
                FIELD_BUILD(ADC_OCALV, 0)   |
	            FIELD_BUILD(ADC_CKDIV, 0x00000005U); //32 div

    MODIFY_REG(reg->CCR,
                ADC_MSBCAL_MASK  |
                ADC_VRPS_MASK    |
                ADC_VRBUFEN_MASK |
                ADC_BP_MASK      |
                ADC_VCMEN_MASK   |
                ADC_LPCTL_MASK   |
                ADC_GCALV_MASK   |
                ADC_OCALV_MASK   |
                ADC_CKDIV_MASK,
                tmp_ccr);

    ch_set_flag = false;
    return CSI_OK;

}

void csi_adc_uninit(csi_adc_t *adc)
{
    reg_adc_t *reg = (reg_adc_t *)adc->dev.reg_base;

    CLEAR_BIT(reg->CR1, (ADC_RAWDEN_MASK  | ADC_JAWDEN_MASK  | ADC_DISCNUM_MASK | 
                         ADC_JDISCEN_MASK | ADC_RDISCEN_MASK | ADC_JAUTO_MASK   | 
                         ADC_AWDSGL_MASK  | ADC_SCAN_MASK    | ADC_JEOCIE_MASK  |   
                         ADC_AWDIE_MASK   | ADC_REOCIE_MASK  | ADC_AWDCH_MASK    ));    
    /* Reset register CR2 */
    CLEAR_BIT(reg->CR2, (ADC_ALIGN_MASK   | ADC_DMAEN_MASK   |        
                                    ADC_CONT_MASK   |  ADC_ADEN_MASK   ));  
    /* Reset register SMPR1 */
    CLEAR_BIT(reg->SMPR1, 0xFFFFFF);
    /* Reset register JOFR1 */
    CLEAR_BIT(reg->JOFR1, ADC_JOFF1_MASK);
    /* Reset register JOFR2 */
    CLEAR_BIT(reg->JOFR2, ADC_JOFF2_MASK);
    /* Reset register JOFR3 */
    CLEAR_BIT(reg->JOFR3, ADC_JOFF3_MASK);
    /* Reset register JOFR4 */
    CLEAR_BIT(reg->JOFR4, ADC_JOFF4_MASK);
    /* Reset register HTR */
    CLEAR_BIT(reg->HTR, ADC_HT_MASK);
    /* Reset register LTR */
    CLEAR_BIT(reg->LTR, ADC_LT_MASK);
    /* Reset register SQR1 */
    CLEAR_BIT(reg->RSQR1, ADC_RSQ1_MASK | ADC_RSQ2_MASK | ADC_RSQ3_MASK | ADC_RSQ4_MASK | ADC_RSQ5_MASK | 
                          ADC_RSQ6_MASK | ADC_RSQ7_MASK | ADC_RSQ8_MASK  );
    /* Reset register SQR2 */
    CLEAR_BIT(reg->RSQR2, ADC_RSQ9_MASK | ADC_RSQ10_MASK | ADC_RSQ11_MASK | ADC_RSQ12_MASK);
    /* Reset register JSQR */
    CLEAR_BIT(reg->JSQR, ADC_JSQ1_MASK | ADC_JSQ2_MASK | ADC_JSQ3_MASK | ADC_JSQ4_MASK);

    CLEAR_BIT(SYSCFG->PMU_TRIM,SYSCFG_EN_BAT_DET_MASK);

    CSI_ADC_MSP_DeInit(adc->dev.idx);

    ch_set_flag = false;
}

csi_error_t csi_adc_set_buffer(csi_adc_t *adc, uint32_t *data, uint32_t num)
{
    adc->data = data;
    adc->num = num;
    return CSI_OK;
}

csi_error_t csi_adc_start(csi_adc_t *adc)
{
    reg_adc_t *reg = (reg_adc_t *)adc->dev.reg_base;

    if((reg->CR2 & ADC_ADEN_MASK) != ADC_ADEN_MASK)
        SET_BIT(reg->CR2, ADC_ADEN_MASK);
    WRITE_REG(reg->SFCR, ADC_REOS_MASK | ADC_REOC_MASK);
    SET_BIT(reg->CR2, ADC_RTRIG_MASK);    
    while ((reg->SR & ADC_REOC_MASK) != ADC_REOC_MASK)
    {
        //printf("SR:%d",reg->SR);  
         
    }
    WRITE_REG(reg->SFCR, ADC_REOS_MASK | ADC_REOC_MASK);    
    return CSI_OK;
}

csi_error_t csi_adc_start_async(csi_adc_t *adc)
{
    reg_adc_t *reg = (reg_adc_t *)adc->dev.reg_base;

    SET_BIT(reg->CR2, ADC_ADEN_MASK);
    WRITE_REG(reg->SFCR, ADC_REOS_MASK | ADC_REOC_MASK);
    SET_BIT(reg->CR1, ADC_REOCIE_MASK);
    SET_BIT(reg->CR2, ADC_RTRIG_MASK);

     if((reg->CR2 & ADC_CONT_MASK) == ADC_CONT_MASK)
        adc_count = 0;
    return CSI_OK;
}

csi_error_t csi_adc_stop(csi_adc_t *adc)
{
    reg_adc_t *reg = (reg_adc_t *)adc->dev.reg_base;

    CLEAR_BIT(reg->CR2, ADC_ADEN_MASK);
    return CSI_OK;
}

csi_error_t csi_adc_stop_async(csi_adc_t *adc)
{
    reg_adc_t *reg = (reg_adc_t *)adc->dev.reg_base;

    CLEAR_BIT(reg->CR2, ADC_ADEN_MASK);
    CLEAR_BIT(reg->CR1, ADC_REOCIE_MASK);
    return CSI_OK;
}

csi_error_t csi_adc_channel_enable(csi_adc_t *adc, uint8_t ch_id, bool is_enable)
{
    reg_adc_t *reg = (reg_adc_t *)adc->dev.reg_base;
    uint8_t i,id,num = (reg->SQLR & 0x07);
    uint64_t volatile u64,RSQR_back,RSQR = (((uint64_t)(reg->RSQR2))<<32) | reg->RSQR1;

    if(ch_id >11)
        return CSI_ERROR;

    if(ch_id == 10)
    {
        SET_BIT(SYSCFG->PMU_TRIM,SYSCFG_EN_BAT_DET_MASK);
    }

    if(is_enable == true)
    {
        if(ch_set_flag == false)
        {
            MODIFY_REG(reg->RSQR1 ,0xFFFFFFFF,ch_id);
            ch_set_flag = true;
            return CSI_OK;
        }

        if(num == 11)
            return CSI_ERROR;
        for(i=0;i<=num;i++)
        {
            id = 0x0F & (RSQR>>(4*i));
            if(ch_id == id)
                return CSI_OK;
            if(id > ch_id)
            {
                break;

            }
        }
        if(i == 0)
        {
            RSQR<<=4;
            RSQR |= ch_id;
        }
        else
        {
            RSQR_back = RSQR;
            RSQR_back<<=4;
            u64 = (pow(2,4*(i+1))-1);
            RSQR_back &= ~u64;
            RSQR_back |= (uint64_t)ch_id<<(4*i);
            u64 = (uint64_t)(pow(2,4*i)-1);        
            RSQR &= u64;
            RSQR |=RSQR_back;
        }
        num ++;
        MODIFY_REG(reg->SQLR,ADC_RSQL_MASK,num);
        MODIFY_REG(reg->RSQR1 ,0xFFFFFFFF,RSQR);
        MODIFY_REG(reg->RSQR2 ,0xFFFFFFFF,RSQR>>32);
    }
    else
    {
        for(i=0;i<=num;i++)
        {
            id = 0x0F & (RSQR>>(4*i));
            if(ch_id == id)
                break;

        }
        if(num == 0)
        {
            RSQR = 0;
            if(i == 0)
            {
                ch_set_flag = false;
            }
        }
        else if(i<=num)
        {
            if(i == 0)
            {
                RSQR>>=4;
            }
            else
            {
                RSQR_back = RSQR;
                RSQR_back>>=4;
                u64 = (pow(2,4*i)-1);
                RSQR_back &= ~u64;
                u64 = (pow(2,4*i)-1);
                RSQR &= u64;
                RSQR |=RSQR_back;
            }
            num --;
        }
        else
            return CSI_ERROR;
        MODIFY_REG(reg->SQLR,ADC_RSQL_MASK,num);
        MODIFY_REG(reg->RSQR1 ,0xFFFFFFFF,RSQR);
        MODIFY_REG(reg->RSQR2 ,0xFFFFFFFF,RSQR>>32);            

    }
    return CSI_OK;
}

csi_error_t csi_adc_sampling_time(csi_adc_t *adc, uint16_t clock_num)
{
    reg_adc_t *reg = (reg_adc_t *)adc->dev.reg_base;
    uint8_t i;
    uint16_t clock;

    if(clock_num<2)
        clock = 0;
    else if (clock_num<4)
        clock = 1;
    else if (clock_num<15)
        clock = 2;
    else
        clock = 3;

    for(i=0;i<=11;i++)
    {
        MODIFY_REG(reg->SMPR1,ADC_SMP0_MASK<<(2*i),clock<<(2*i));        
    }
    return CSI_OK;    
}

csi_error_t csi_adc_continue_mode(csi_adc_t *adc, bool is_enable)
{
    reg_adc_t *reg = (reg_adc_t *)adc->dev.reg_base;

    if(is_enable == true)
    {
        SET_BIT(reg->CR2, ADC_CONT_MASK);
    }
    else
    {
        CLEAR_BIT(reg->CR2, ADC_CONT_MASK);
    }    
    return CSI_OK; 
}


uint32_t csi_adc_freq_div(csi_adc_t *adc, uint32_t div)
{
    reg_adc_t *reg = (reg_adc_t *)adc->dev.reg_base;
    uint32_t value;

    if(div<4)
        value = 1;
    else if (div<8)
        value = 2;
    else if (div<16)
        value = 3;
    else if (div<32)
        value = 4;
    else if (div<64)
        value = 5;
    else if (div<128)
        value = 6;
    else
        value = 7;
    MODIFY_REG(reg->CCR, ADC_CKDIV_MASK, value);
    return SDK_PCLK_MHZ*1000000/(2^value);
}

int32_t csi_adc_read(csi_adc_t *adc)
{
    reg_adc_t *reg = (reg_adc_t *)adc->dev.reg_base;

    return reg->RDR;    
}

csi_error_t csi_adc_attach_callback(csi_adc_t *adc, void *callback, void *arg)
{
    adc->callback = callback;
    adc->arg = arg;

    return CSI_OK;
}


void  csi_adc_detach_callback(csi_adc_t *adc)
{
    adc->callback = NULL;
    adc->arg = NULL;
}

void csi_adc_isr(csi_adc_t *adc)
{
    reg_adc_t *reg = (reg_adc_t *)adc->dev.reg_base;
    uint8_t num = (reg->SQLR & 0x07);

    /* ========== Check End of Conversion flag for regular group ========== */
    if((reg->CR1 & ADC_REOCIE_MASK) == ADC_REOCIE_MASK)
    {
        if ((reg->SR & ADC_REOC_MASK) == ADC_REOC_MASK)
        {
            /* Clear regular group conversion flag */
            WRITE_REG(reg->SFCR, ADC_RSTRTCC_MASK | ADC_REOCC_MASK);

            /* Determine whether any further conversion upcoming on group regular   */
            /* by external trigger, continuous mode or scan sequence on going.      */
            if(adc_count < adc->num)
            {
                adc->data[adc_count++] = reg->RDR;
            }
            else
            {
                if(adc->callback)
                {
                    adc->callback(adc,ADC_EVENT_ERROR,adc->arg);
                }  
            }
            if(adc_count > num)
            {
                adc_count = 0;
                if((reg->CR2 & ADC_CONT_MASK) == ADC_CONT_MASK)
                {
                    CLEAR_BIT(reg->CR1, (ADC_REOCIE_MASK | ADC_REOSIE_MASK));
                    if(adc->callback)
                    {
                        adc->callback(adc,ADC_EVENT_CONVERT_COMPLETE,adc->arg);
                    }
                    return;
                }
            }
            if((reg->CR2 & ADC_CONT_MASK) != ADC_CONT_MASK)
            {
                /* Disable ADC end of conversion interrupt on group regular */
                CLEAR_BIT(reg->CR1, (ADC_REOCIE_MASK | ADC_REOSIE_MASK));

                if(adc->callback)
                {
                    adc->callback(adc,ADC_EVENT_CONVERT_COMPLETE,adc->arg);
                }
            }
        }
    }    

}

