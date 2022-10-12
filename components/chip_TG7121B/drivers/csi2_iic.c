#include <drv/iic.h>
#include <drv/common.h>
#include <csi_kernel.h>
#include "reg_i2c_type.h"
#include "field_manipulate.h"
#include "i2c_misc.h"
#include "sdk_config.h"
#include "systick.h"
#include "cpu.h"
#ifndef CONFIG_KERNEL_NONE
#define CSI_INTRPT_ENTER() aos_kernel_intrpt_enter()
#define CSI_INTRPT_EXIT()  aos_kernel_intrpt_exit()
#else
#define CSI_INTRPT_ENTER()
#define CSI_INTRPT_EXIT()
#endif

#define I2C_TIMEOUT_BUSY_FLAG     25U         /*!< Timeout 25 ms             */
#define I2C_CLOCK   (SDK_PCLK_MHZ*1000000)

#define __CSI_I2C_ENABLE_IT(__REG__, __INTERRUPT__)   SET_BIT((__REG__)->IER,(__INTERRUPT__))
#define __CSI_I2C_DISABLE_IT(__REG__, __INTERRUPT__)  SET_BIT((__REG__)->IDR,(__INTERRUPT__))
#define __CSI_I2C_GET_CR2_FLAG(__REG__, __INTERRUPT__)  ((((__REG__)->CR2 & (__INTERRUPT__)) == (__INTERRUPT__)) ? true : false)
#define __CSI_I2C_GET_FLAG(__REG__, __FLAG__) (((((__REG__)->SR) & (__FLAG__)) == (__FLAG__)) ? true : false)
#define __CSI_I2C_CLEAR_FLAG(__REG__, __FLAG__)    SET_BIT((__REG__)->CFR, (__FLAG__))
#define __CSI_I2C_CLEAR_IF(__REG__, __FLAG__)      SET_BIT((__REG__)->ICR, (__FLAG__))
#define __CSI_I2C_CLEAR_SR(__REG__)                SET_BIT((__REG__)->CFR, 0x3F38) 
#define __CSI_I2C_ENABLE(__REG__)                  SET_BIT((__REG__)->CR1, I2C_CR1_PE_MASK)
#define __CSI_I2C_DISABLE(__REG__)                 CLEAR_BIT((__REG__)->CR1, I2C_CR1_PE_MASK)

#define __CSI_I2C_CHECK_FLAG(__ISR__, __FLAG__)         ((((__ISR__) & (__FLAG__)) == (__FLAG__)) ? true : false)
#define __CSI_I2C_CHECK_IT_SOURCE(__CR1__, __IT__)      ((((__CR1__) & (__IT__)) == (__IT__)) ? true : false)

#define CSI_I2C_ERROR_NONE              0x00000000    /*!< No error              */
#define CSI_I2C_ERROR_BERR              0x00000001    /*!< BERR error            */
#define CSI_I2C_ERROR_ARLO              0x00000002    /*!< ARLO error            */
#define CSI_I2C_ERROR_NACKF             0x00000004    /*!< NACK error            */
#define CSI_I2C_ERROR_OVR               0x00000008    /*!< OVR error             */
#define CSI_I2C_ERROR_DMA               0x00000010    /*!< DMA transfer error    */
#define CSI_I2C_ERROR_TIMEOUT           0x00000020    /*!< Timeout Error         */
#define CSI_I2C_ERROR_SIZE              0x00000040    /*!< Size Management error */
#define CSI_I2C_ERROR_DMA_PARAM         0x00000080    /*!< DMA Parameter Error   */

enum
{
  CSI_I2C_STATE_RESET             = 0x00,   /*!< Peripheral is not yet Initialized         */
  CSI_I2C_STATE_READY             = 0x20,   /*!< Peripheral Initialized and ready for use  */
  CSI_I2C_STATE_BUSY              = 0x24,   /*!< An internal process is ongoing            */
  CSI_I2C_STATE_BUSY_TX           = 0x21,   /*!< Data Transmission process is ongoing      */
  CSI_I2C_STATE_BUSY_RX           = 0x22,   /*!< Data Reception process is ongoing         */
  CSI_I2C_STATE_LISTEN            = 0x28,   /*!< Address Listen Mode is ongoing            */
  CSI_I2C_STATE_BUSY_TX_LISTEN    = 0x29,   /*!< Address Listen Mode and Data Transmission
                                                 process is ongoing                         */
  CSI_I2C_STATE_BUSY_RX_LISTEN    = 0x2A,   /*!< Address Listen Mode and Data Reception
                                                 process is ongoing                         */
  CSI_I2C_STATE_ABORT             = 0x60,   /*!< Abort user request ongoing                */
  CSI_I2C_STATE_TIMEOUT           = 0xA0,   /*!< Timeout state                             */
  CSI_I2C_STATE_ERROR             = 0xE0    /*!< Error                                     */

};

static uint32_t iic_global_error_code = 0;

uint32_t CSI_IIC_MSP_Init(void *inst,uint32_t idx);
void CSI_IIC_MSP_DeInit(uint32_t idx);

csi_error_t csi_iic_init(csi_iic_t *iic, uint32_t idx)
{
    iic->dev.idx = idx;
    iic->dev.reg_base = CSI_IIC_MSP_Init(iic,idx);
    // reg_i2c_t *reg = (reg_i2c_t *)iic->dev.reg_base;
    // reg->CR1 |= I2C_CR1_PE_MASK;
    return CSI_OK;
}

void csi_iic_uninit(csi_iic_t *iic)
{
    reg_i2c_t *reg = (reg_i2c_t *)iic->dev.reg_base;
    reg->CR1 &= ~I2C_CR1_PE_MASK;
    CSI_IIC_MSP_DeInit(iic->dev.idx);
}

csi_error_t csi_iic_mode(csi_iic_t *iic, csi_iic_mode_t mode)
{
    iic->mode = mode;
    return CSI_OK; 
}

csi_error_t csi_iic_addr_mode(csi_iic_t *iic, csi_iic_addr_mode_t addr_mode)
{
    reg_i2c_t *reg = (reg_i2c_t *)iic->dev.reg_base;
    __CSI_I2C_DISABLE(reg);
    if (IIC_ADDRESS_7BIT == addr_mode)
    {
        reg->CR2 &= ~I2C_CR2_SADD10_MASK;
    }
    else
    {
        reg->CR2 |= I2C_CR2_SADD10_MASK;
    }
    
    return CSI_OK;
}

csi_error_t csi_iic_speed(csi_iic_t *iic, csi_iic_speed_t speed)
{
    reg_i2c_t *reg = (reg_i2c_t *)iic->dev.reg_base;
    __CSI_I2C_DISABLE(reg);
    uint32_t freq, period;
    uint8_t scll, sclh, sdadel, scldel;
    uint8_t presc = 0;
    switch (speed)
    {
    case IIC_BUS_SPEED_STANDARD:
        freq = 100*1000; // 100KHz
        break;
    case IIC_BUS_SPEED_FAST:
        freq = 400*1000; // 400KHz
        break;
    case IIC_BUS_SPEED_FAST_PLUS:
    case IIC_BUS_SPEED_HIGH:
        freq = 1000*1000; // 1MHz
        break;
    // case IIC_BUS_SPEED_HIGH:
    //     freq = 3400*1000; // 3.4MHz
    //     break;
    default:
        freq = 100*1000; // 100KHz for default
        break;
    }

    do
    {
        presc = (presc + 1) * 2 - 1;
        period = (SDK_PCLK_MHZ*1000000) / (presc + 1)/ freq;
    } while (period/2 > 255);

    scll = period / 2;
    sclh = period - scll;
    sdadel = sclh / 4;
    scldel = sclh / 4;
    if (sdadel > 0xf)
    {
        sdadel = 0xf;
    }
    else if (sdadel < 1)
    {
        sdadel = 1;
    }
    
    if (scldel > 0xf)
    {
        scldel = 0xf;
    }
    else if (scldel < 1)
    {
        scldel = 1;
    }

    if (scll == 0)
    {
        scll = 1;
    }
    if (sclh == 0)
    {
        sclh = 1;
    }

    reg->TIMINGR &= ~I2C_TIMINGR_PRESC_MASK;
    MODIFY_REG(reg->TIMINGR, I2C_TIMINGR_PRESC_MASK, (presc & 0xf) << I2C_TIMINGR_PRESC_POS);
    reg->TIMINGR &= ~(I2C_TIMINGR_SCLH_MASK | I2C_TIMINGR_SCLL_MASK | I2C_TIMINGR_SDADEL_MASK | I2C_TIMINGR_SCLDEL_MASK);
    reg->TIMINGR |= (sclh - 1) << I2C_TIMINGR_SCLH_POS | (scll - 1) << I2C_TIMINGR_SCLL_POS | sdadel << I2C_TIMINGR_SDADEL_POS | scldel << I2C_TIMINGR_SCLDEL_POS;
    return CSI_OK;
}

// csi_error_t csi_iic_own_addr(csi_iic_t *iic, uint32_t own_addr);

static bool i2c_CR2flag_poll(va_list va)
{
    reg_i2c_t *hi2c = va_arg(va,reg_i2c_t *);
    uint32_t flag = va_arg(va,uint32_t);
    if (__CSI_I2C_GET_CR2_FLAG(hi2c, flag))
    {
        return false;
    }
    else
    {
        return true;
    }
}

static bool i2c_IsAcknowledgeFailed(reg_i2c_t *reg)
{
    bool status = true;
    if (__CSI_I2C_GET_FLAG(reg, I2C_FLAG_NACK))
    {
        __CSI_I2C_CLEAR_FLAG(reg, I2C_FLAG_NACK);
        iic_global_error_code |= CSI_I2C_ERROR_NACKF;
        status = false;
    }
    return status;
}

static bool i2c_WaitOnMasterAddressFlagUntilTimeout(reg_i2c_t *reg, uint32_t Flag, uint32_t Timeout, uint32_t Tickstart)
{
    bool status = true;
    uint32_t timeout = SYSTICK_MS2TICKS(Timeout);
    if (Timeout != 0xffffffff)
    {
        if(systick_poll_timeout(Tickstart, timeout, i2c_CR2flag_poll, reg, Flag))
        {
            status = false;
        }
    }
    else
    {
        while (__CSI_I2C_GET_CR2_FLAG(reg, Flag))
        {
            if (!i2c_IsAcknowledgeFailed(reg))
            {
                status = false;
                break;
            }
        }
    }   
   return status;
}

static bool i2c_flagandstop_poll(va_list va)
{
    bool status = false;
    reg_i2c_t *reg = va_arg(va, reg_i2c_t *);
    uint32_t flag = va_arg(va,uint32_t);
    if (__CSI_I2C_GET_FLAG(reg, flag))
    {
        status = true;
    }
    else
    {
        if (__CSI_I2C_GET_FLAG(reg, I2C_SR_STOPF_MASK))
        {
            __CSI_I2C_CLEAR_FLAG(reg, I2C_CFR_STOPCF_MASK);
            iic_global_error_code = CSI_I2C_ERROR_BERR;

            status = true;
        }
    }
    return status;
}

static bool i2c_WaitOnRXNEFlagUntilTimeout(reg_i2c_t *reg, uint32_t Timeout, uint32_t Tickstart)
{
    bool status = true;
    uint32_t timeout = SYSTICK_MS2TICKS(Timeout);

    if (Timeout != 0xffffffff)
    {
        if(systick_poll_timeout(Tickstart, timeout, i2c_flagandstop_poll, reg, I2C_SR_RXNE_MASK))
        {
            iic_global_error_code |= CSI_I2C_ERROR_TIMEOUT;
            status = false;
        }
        if(iic_global_error_code == CSI_I2C_ERROR_BERR)
        {
            // iic_global_error_code |= CSI_I2C_ERROR_NONE;
            iic_global_error_code = CSI_I2C_ERROR_NONE; 
            status = false;
        }
    }
    else
    {
        while (!__CSI_I2C_GET_FLAG(reg, I2C_SR_RXNE_MASK))
        {
            if (!i2c_IsAcknowledgeFailed(reg))
            {
                status = false;
                break;
            }
        }
    }  
    return status;
}

static bool i2c_flagandnack_poll(va_list va)
{
    bool status = false;
    reg_i2c_t *hi2c = va_arg(va, reg_i2c_t *);
    uint32_t flag = va_arg(va, uint32_t);
    if (__CSI_I2C_GET_FLAG(hi2c, flag))
    {
        status =  true;
    }
    else
    {
        if (!i2c_IsAcknowledgeFailed(hi2c))
        {
            status = true;
        }
    }
    return status;
}

static bool i2c_WaitOnTXEFlagUntilTimeout(reg_i2c_t *reg, uint32_t Timeout, uint32_t Tickstart)
{
    bool status = true;
    uint32_t timeout = SYSTICK_MS2TICKS(Timeout);

    if (Timeout != 0xffffffff)
    {
        if(systick_poll_timeout(Tickstart, timeout, i2c_flagandnack_poll, reg, I2C_FLAG_TXE))
        {
            iic_global_error_code |= CSI_I2C_ERROR_TIMEOUT;
            status = false;
        }
        if(iic_global_error_code == CSI_I2C_ERROR_NACKF)
        {
            status = false;
        }
    }
    else
    {
        while (!__CSI_I2C_GET_FLAG(reg, I2C_FLAG_TXE))
        {
            if (!i2c_IsAcknowledgeFailed(reg))
            {
                status = false;
            }
        }
    }
    return status;
}

static bool i2c_MasterRequestRead(reg_i2c_t *reg, uint16_t DevAddress, uint32_t Timeout, uint32_t Tickstart)
{
    bool status = true;
    CLEAR_BIT(reg->CR2, I2C_CR2_NACK_MASK);
    if (READ_BIT(reg->CR2, I2C_CR2_SADD10_MASK) == 0) // 7-bits slave address
    {
        // CLEAR_BIT(reg->CR2, I2C_CR2_SADD10_MASK);
        MODIFY_REG(reg->CR2, I2C_CR2_RD_WEN_MASK | I2C_CR2_SADD1_7_MASK, I2C_CR2_RD_WEN_MASK | ((DevAddress << 1) & 0xFE));
    }
    else
    {
        // SET_BIT(reg->CR2, I2C_CR2_SADD10_MASK);
        MODIFY_REG(reg->CR2, I2C_CR2_RD_WEN_MASK | I2C_CR2_SADD0_MASK | I2C_CR2_SADD1_7_MASK | I2C_CR2_SADD8_9_MASK, I2C_CR2_RD_WEN_MASK | (DevAddress & 0x3FF));
        CLEAR_BIT(reg->CR2, I2C_CR2_HEAD10R_MASK);
    }
    SET_BIT(reg->CR2, I2C_CR2_START_MASK); // for first frame
    if (!i2c_WaitOnMasterAddressFlagUntilTimeout(reg, I2C_CR2_START_MASK, Timeout, Tickstart))
    {
        status = false;
    }
    return status;
}

static bool i2c_MasterRequestWrite(reg_i2c_t *reg, uint16_t DevAddress, uint32_t Timeout, uint32_t Tickstart)
{
    bool status = true;
    uint32_t cpu_stat = enter_critical();
    if (READ_BIT(reg->CR2, I2C_CR2_SADD10_MASK) == 0) // 7-bits slave address
    {
        // CLEAR_BIT(reg->CR2, I2C_CR2_SADD10_MASK);
        MODIFY_REG(reg->CR2, I2C_CR2_RD_WEN_MASK | I2C_CR2_SADD1_7_MASK, (DevAddress << 1) & 0xFE);
    }
    else
    {
        // SET_BIT(reg->CR2, I2C_CR2_SADD10_MASK);
        MODIFY_REG(reg->CR2, I2C_CR2_RD_WEN_MASK | I2C_CR2_SADD0_MASK | I2C_CR2_SADD1_7_MASK | I2C_CR2_SADD8_9_MASK, DevAddress & 0x3FF);
        CLEAR_BIT(reg->CR2, I2C_CR2_HEAD10R_MASK);
    }
    SET_BIT(reg->CR2, I2C_CR2_START_MASK);
    exit_critical(cpu_stat);
    if (!i2c_WaitOnMasterAddressFlagUntilTimeout(reg, I2C_CR2_START_MASK, Timeout, Tickstart))
    {
        status = false;
    }

    return status;
}

static bool i2c_WaitOnTCRFlagUntilTimeout(reg_i2c_t *reg, uint32_t Timeout, uint32_t Tickstart)
{
    bool status = true;
    uint32_t timeout = SYSTICK_MS2TICKS(Timeout);

    if (Timeout != 0xffffffff)
    {
        if(systick_poll_timeout(Tickstart, timeout, i2c_flagandstop_poll, reg, I2C_SR_TCR_MASK))
        {
            iic_global_error_code |= CSI_I2C_ERROR_TIMEOUT;
            status = false;
        }
        if(iic_global_error_code == CSI_I2C_ERROR_BERR)
        {
            iic_global_error_code |= CSI_I2C_ERROR_TIMEOUT;
            status = false;
        }
    }
    else
    {
        while (!__CSI_I2C_GET_FLAG(reg, I2C_SR_TCR_MASK))
        {

        }
    }  
    return status;
}

int32_t csi_iic_master_send(csi_iic_t *iic, uint32_t devaddr, const void *data, uint32_t size, uint32_t timeout)
{
    uint16_t TxCount = 0;
    bool tx_flag_timeout = false;
    uint32_t size_left = size;
    if (IIC_MODE_MASTER == iic->mode)
    {
        do
        {
            reg_i2c_t *reg = (reg_i2c_t *)iic->dev.reg_base;
            uint32_t tickstart = systick_get_value();
            if (READ_BIT(reg->CR1, I2C_CR1_PE_MASK) != I2C_CR1_PE_MASK)
            {
                __CSI_I2C_ENABLE(reg);
            }  
            iic_global_error_code = CSI_I2C_ERROR_NONE;
            iic->state.writeable = 1;
            iic->state.readable = 0;
            iic->data = (uint8_t*)data;
            __CSI_I2C_CLEAR_SR(reg);
                
            CLEAR_BIT(reg->CR2, I2C_CR2_AUTOEND_MASK | I2C_CR2_RELOAD_MASK | I2C_CR2_NBYTES_MASK);
            if(size_left > 0xFF)
            {
                SET_BIT(reg->CR2, I2C_CR2_RELOAD_MASK | 0xFF0000);
            }
            else
            {
                SET_BIT(reg->CR2, (((uint32_t)size_left) << I2C_CR2_NBYTES_POS));
            }
            if (!i2c_MasterRequestWrite(reg, devaddr, timeout, tickstart))
            {
                __CSI_I2C_DISABLE(reg);
                break;
            }

            while (size_left > 0)
            {
                if (!i2c_WaitOnTXEFlagUntilTimeout(reg, timeout, tickstart))
                {
                    if (iic_global_error_code == CSI_I2C_ERROR_NACKF)
                    {
                        SET_BIT(reg->CR2, I2C_CR2_STOP_MASK);
                    }
                    __CSI_I2C_DISABLE(reg);
                    tx_flag_timeout = true;
                    break;
                }
                reg->TXDR = *(uint8_t*)iic->data;

                iic->data++;
                TxCount++;

                size_left--;
                    		
                if((TxCount == 255)&&(reg->CR2 & I2C_CR2_RELOAD_MASK))	
                {
                    if (!i2c_WaitOnTCRFlagUntilTimeout(reg, timeout, tickstart))
                    {
                        __CSI_I2C_DISABLE(reg);
                        tx_flag_timeout = true;
                        break;
                    }
                    else
                    {	
                        uint32_t cpu_stat = enter_critical();
                        TxCount = 0;	
                        if(size_left > 0xFF)		
                        {
                            MODIFY_REG(reg->CR2, I2C_CR2_NBYTES_MASK, 0xFF0000);
                        }
                        else
                        {
                            MODIFY_REG(reg->CR2, I2C_CR2_NBYTES_MASK, (((uint32_t)size_left) << I2C_CR2_NBYTES_POS));
                            CLEAR_BIT(reg->CR2, I2C_CR2_RELOAD_MASK);
                        }
                        __CSI_I2C_CLEAR_FLAG(reg, I2C_FLAG_TCR);
                        exit_critical(cpu_stat);
                    }
                }	
            }
            if (tx_flag_timeout)
            {
                break;
            }

            SET_BIT(reg->CR2, I2C_CR2_STOP_MASK);

            if (!i2c_WaitOnMasterAddressFlagUntilTimeout(reg, I2C_CR2_STOP_MASK, timeout, tickstart))
            {
                __CSI_I2C_DISABLE(reg);
                break;
            }
        } while (0);
        
    }
    iic->state.writeable = 0;
    iic->state.readable = 0;
    return size - size_left;
}

int32_t csi_iic_master_receive(csi_iic_t *iic, uint32_t devAddr, void *data, uint32_t size, uint32_t timeout)
{
    uint16_t RxCount = 0;
    bool rx_flag_timeout = false;
    uint32_t size_left = size;
    if (IIC_MODE_MASTER == iic->mode && size != 0)
    {
        do
        {
            reg_i2c_t *reg = (reg_i2c_t *)iic->dev.reg_base;
            uint32_t tickstart = systick_get_value();
            if (READ_BIT(reg->CR1, I2C_CR1_PE_MASK) != I2C_CR1_PE_MASK)
            {
                __CSI_I2C_ENABLE(reg);
            }
            iic_global_error_code = CSI_I2C_ERROR_NONE;
            iic->state.writeable = 0;
            iic->state.readable = 1;
            iic->data = (uint8_t*)data;

            __CSI_I2C_CLEAR_SR(reg);
            uint32_t cpu_stat = enter_critical();
            CLEAR_BIT(reg->CR2, I2C_CR2_AUTOEND_MASK | I2C_CR2_RELOAD_MASK | I2C_CR2_NBYTES_MASK);
            if (size_left > 255)
            {
                SET_BIT(reg->CR2, I2C_CR2_RELOAD_MASK | 0xFF0000);
            }
            else
            {
                SET_BIT(reg->CR2, size_left << I2C_CR2_NBYTES_POS);
            }
            exit_critical(cpu_stat);
            if (!i2c_MasterRequestRead(reg, devAddr, timeout, tickstart))
            {
                break;
            }
            if (size_left == 1U)
            {
                cpu_stat = enter_critical();
                SET_BIT(reg->CR2, I2C_CR2_NACK_MASK);
                SET_BIT(reg->CR2, I2C_CR2_STOP_MASK);
                exit_critical(cpu_stat);
            }
            while (size_left > 0)
            {
                if (!i2c_WaitOnRXNEFlagUntilTimeout(reg, timeout, tickstart))
                {
                    __CSI_I2C_DISABLE(reg);
                    rx_flag_timeout = true;
                    break;
                }
                *(uint8_t*)iic->data = (uint8_t)reg->RXDR;
                iic->data++;
                RxCount++;

                size_left--;
                
                if (size_left == 1U)
                {
                    cpu_stat = enter_critical();
                    SET_BIT(reg->CR2, I2C_CR2_NACK_MASK);
                    SET_BIT(reg->CR2, I2C_CR2_STOP_MASK);
                    exit_critical(cpu_stat);
                }

                if((RxCount == 255)&&(reg->CR2 & I2C_CR2_RELOAD_MASK))	
                {
                    if (!i2c_WaitOnTCRFlagUntilTimeout(reg, timeout, tickstart))
                    {
                        __CSI_I2C_DISABLE(reg);
                        rx_flag_timeout = true;
                        break;
                    }
                    else
                    {	
                        RxCount = 0;	
                        cpu_stat = enter_critical();
                        if(size_left > 0xFF)		
                        {
                            MODIFY_REG(reg->CR2, I2C_CR2_NBYTES_MASK, 0xFF0000);
                        }
                        else
                        {
                            MODIFY_REG(reg->CR2, I2C_CR2_NBYTES_MASK, (((uint32_t)size_left) << I2C_CR2_NBYTES_POS));
                            CLEAR_BIT(reg->CR2, I2C_CR2_RELOAD_MASK);
                        }	
                        __CSI_I2C_CLEAR_FLAG(reg, I2C_FLAG_TCR);	
                        exit_critical(cpu_stat);	
                    }
                }
            }
            if (rx_flag_timeout)
            {
                break;
            }

            CLEAR_BIT(reg->CR2, I2C_CR2_AUTOEND_MASK | I2C_CR2_RELOAD_MASK | I2C_CR2_NBYTES_MASK);

            if (!i2c_WaitOnMasterAddressFlagUntilTimeout(reg, I2C_CR2_STOP_MASK, timeout, tickstart))
            {
                __CSI_I2C_DISABLE(reg);
                break;
            }
            
        } while (0);
    }
    iic->state.writeable = 0;
    iic->state.readable = 0;
    return size - size_left;
}

csi_error_t csi_iic_master_send_async(csi_iic_t *iic, uint32_t devaddr, const void *data, uint32_t size)
{
    csi_error_t status = CSI_ERROR;
    if (IIC_MODE_MASTER == iic->mode)
    {
        reg_i2c_t *reg = (reg_i2c_t *)iic->dev.reg_base;
    #if 0
        uint32_t count = 0;
        uint32_tcount = I2C_TIMEOUT_BUSY_FLAG * (I2C_CLOCK / 25U / 1000);
        do
        {
            count--;
            if (count == 0)
            {
                // iic_global_error_code |= CSI_I2C_ERROR_TIMEOUT;
                status = CSI_TIMEOUT;
                return status;
            }
        }while (__CSI_I2C_GET_FLAG(reg, I2C_FLAG_BUSY));
    #endif
        if ((reg->CR1 & I2C_CR1_PE_MASK) != I2C_CR1_PE_MASK)
        {
            __CSI_I2C_ENABLE(reg);
        }
        iic_global_error_code = CSI_I2C_ERROR_NONE;

        iic->data = (uint8_t*)data;
        iic->size = size;
        iic->state.writeable = 1;
        iic->state.readable = 0;

        CLEAR_BIT(reg->CR2, I2C_CR2_AUTOEND_MASK | I2C_CR2_RELOAD_MASK | I2C_CR2_NBYTES_MASK);
        if(iic->size > 0xFF)		
        {
            SET_BIT(reg->CR2, I2C_CR2_RELOAD_MASK | 0xFF0000);
        }
        else
            SET_BIT(reg->CR2, I2C_CR2_AUTOEND_MASK | (((uint32_t)iic->size) << I2C_CR2_NBYTES_POS));

        if (READ_BIT(reg->CR2, I2C_CR2_SADD10_MASK) == 0) // 7-bits slave address
        {
            // CLEAR_BIT(reg->CR2, I2C_CR2_SADD10_MASK);
            MODIFY_REG(reg->CR2, I2C_CR2_RD_WEN_MASK | I2C_CR2_SADD1_7_MASK, (devaddr << 1) & 0xFE);
        }
        else
        {
            // SET_BIT(reg->CR2, I2C_CR2_SADD10_MASK);
            MODIFY_REG(reg->CR2, I2C_CR2_RD_WEN_MASK | I2C_CR2_SADD0_MASK | I2C_CR2_SADD1_7_MASK | I2C_CR2_SADD8_9_MASK, devaddr & 0x3FF);
            CLEAR_BIT(reg->CR2, I2C_CR2_HEAD10R_MASK);
        }
        SET_BIT(reg->CR2, I2C_CR2_START_MASK);

        __CSI_I2C_CLEAR_SR(reg);
        __CSI_I2C_CLEAR_IF(reg, I2C_IT_EVT | I2C_IT_TXE | I2C_IT_ERR | I2C_IT_TCR | I2C_IT_TC);
        __CSI_I2C_ENABLE_IT(reg, I2C_IT_EVT | I2C_IT_TXE | I2C_IT_ERR);
            
        if(iic->size > 0xFF)
        {
            __CSI_I2C_ENABLE_IT(reg, I2C_IT_TCR);
        }
        else
        {
            __CSI_I2C_ENABLE_IT(reg, I2C_IT_TC);
        }

        if ((iic->size > 0) && (__CSI_I2C_GET_FLAG(reg, I2C_FLAG_TXE)))
        {
            reg->TXDR = *iic->data;
            iic->data++;
            iic->size--;
        }
        status = CSI_OK;
    }
    return status;
}

csi_error_t csi_iic_master_receive_async(csi_iic_t *iic, uint32_t devaddr, void *data, uint32_t size)
{
    csi_error_t status = CSI_ERROR;
    if (IIC_MODE_MASTER == iic->mode)
    {
        reg_i2c_t *reg = (reg_i2c_t *)iic->dev.reg_base;
    #if 0
        uint32_t count = I2C_TIMEOUT_BUSY_FLAG * (I2C_CLOCK / 25U / 1000);
        count = I2C_TIMEOUT_BUSY_FLAG * (I2C_CLOCK / 25U / 1000);
        do
        {
            count--;
            if (count == 0)
            {
                // iic_global_error_code |= CSI_I2C_ERROR_TIMEOUT;
                status = CSI_TIMEOUT;
                return status;
            }
        }while (__CSI_I2C_GET_FLAG(reg, I2C_FLAG_BUSY));
    #endif

        if ((reg->CR1 & I2C_CR1_PE_MASK) != I2C_CR1_PE_MASK)
        {
            __CSI_I2C_ENABLE(reg);
        }

        iic_global_error_code = CSI_I2C_ERROR_NONE;

        iic->data = data;
        iic->size = size;
        iic->state.writeable = 0;
        iic->state.readable = 1;

        if(iic->size == 1)
        {
            SET_BIT(reg->CR2, I2C_CR2_NACK_MASK);
        }
        else
        {
            CLEAR_BIT(reg->CR2, I2C_CR2_NACK_MASK);
        }

        CLEAR_BIT(reg->CR2, I2C_CR2_AUTOEND_MASK | I2C_CR2_RELOAD_MASK | I2C_CR2_NBYTES_MASK);

        if(iic->size > 0xFF)		
        {
            SET_BIT(reg->CR2, I2C_CR2_RELOAD_MASK | 0xFF0000);
        }
        else
        {
            SET_BIT(reg->CR2, I2C_CR2_AUTOEND_MASK | (((uint32_t)iic->size) << I2C_CR2_NBYTES_POS));
        }
        if (READ_BIT(reg->CR2, I2C_CR2_SADD10_MASK) == 0) // 7-bits slave address
        {
            // CLEAR_BIT(reg->CR2, I2C_CR2_SADD10_MASK);
            MODIFY_REG(reg->CR2, I2C_CR2_RD_WEN_MASK | I2C_CR2_SADD1_7_MASK,I2C_CR2_RD_WEN_MASK | ((devaddr << 1) & 0xFE));
        }
        else
        {
            // SET_BIT(reg->CR2, I2C_CR2_SADD10_MASK);
            MODIFY_REG(reg->CR2, I2C_CR2_RD_WEN_MASK | I2C_CR2_SADD0_MASK | I2C_CR2_SADD1_7_MASK | I2C_CR2_SADD8_9_MASK, I2C_CR2_RD_WEN_MASK | (devaddr&0x3FF));
            CLEAR_BIT(reg->CR2, I2C_CR2_HEAD10R_MASK);
        }
        SET_BIT(reg->CR2, I2C_CR2_START_MASK);

        __CSI_I2C_CLEAR_SR(reg);
        __CSI_I2C_CLEAR_IF(reg, I2C_IT_EVT | I2C_IT_RXNE | I2C_IT_ERR);
        __CSI_I2C_ENABLE_IT(reg, I2C_IT_EVT | I2C_IT_RXNE | I2C_IT_ERR);
            
        if(iic->size > 255)
        {
            __CSI_I2C_ENABLE_IT(reg, I2C_IT_TCR);
        }
        else
        {
            __CSI_I2C_ENABLE_IT(reg, I2C_IT_TC);
        }
        status = CSI_OK;
    }
    return status;
}

// int32_t csi_iic_mem_send(csi_iic_t *iic, uint32_t devaddr, uint16_t memaddr, csi_iic_mem_addr_size_t memaddr_size, const void *data, uint32_t size, uint32_t timeout);

// int32_t csi_iic_mem_receive(csi_iic_t *iic, uint32_t devaddr, uint16_t memaddr, csi_iic_mem_addr_size_t memaddr_size, void *data, uint32_t size, uint32_t timeout);

// int32_t csi_iic_slave_send(csi_iic_t *iic, const void *data, uint32_t size, uint32_t timeout);

// int32_t csi_iic_slave_receive(csi_iic_t *iic, void *data, uint32_t size, uint32_t timeout);

// csi_error_t csi_iic_slave_send_async(csi_iic_t *iic, const void *data, uint32_t size);

// csi_error_t csi_iic_slave_receive_async(csi_iic_t *iic, void *data, uint32_t size);

csi_error_t csi_iic_attach_callback(csi_iic_t *iic, void *callback, void *arg)
{
    iic->callback = (void (*)(csi_iic_t*, csi_iic_event_t, void*))callback;
    iic->arg = arg;
    return CSI_OK;
}

void csi_iic_detach_callback(csi_iic_t *iic)
{
    iic->callback = NULL;
    iic->arg = NULL;
}

// csi_error_t csi_iic_xfer_pending(csi_iic_t *iic, bool enable);

// csi_error_t csi_iic_link_dma(csi_iic_t *iic, csi_dma_ch_t *tx_dma, csi_dma_ch_t *rx_dma);

// csi_error_t csi_iic_get_state(csi_iic_t *iic, csi_state_t *state);

// csi_error_t csi_iic_enable_pm(csi_iic_t *iic);

// void csi_iic_disable_pm(csi_iic_t *iic);

static void i2c_MasterTransmit_TXE(csi_iic_t *iic)
{
    reg_i2c_t *reg = (reg_i2c_t *)iic->dev.reg_base;
    csi_state_t CurrentState = iic->state;
    if (1 == CurrentState.writeable)
    {
        if (iic->size == 0)
        {
            __CSI_I2C_DISABLE_IT(reg, I2C_IT_EVT | I2C_IT_TXE | I2C_IT_ERR | I2C_IT_TC | I2C_IT_TCR);

            CurrentState.writeable = 0;
            CurrentState.readable = 0;
            if (iic->callback != NULL)
            {
                iic->callback(iic, IIC_EVENT_SEND_COMPLETE, iic->arg);
                iic->callback = NULL;
                iic->arg = NULL;
            }
        }
        else
        {
            reg->TXDR = *iic->data;
            iic->data++;
            iic->size--;
        }
    }
}

static void i2c_MasterReceive_RXNE(csi_iic_t *iic)
{
    reg_i2c_t *reg = (reg_i2c_t *)iic->dev.reg_base;
    csi_state_t CurrentState = iic->state;
    if (1 == CurrentState.readable)
    {
        if (iic->size != 0)
        {
            *iic->data = (uint8_t)reg->RXDR;
            iic->data++;
            iic->size--;

        }
        if (iic->size == 0) 
        {
            __CSI_I2C_DISABLE_IT(reg, I2C_IT_EVT | I2C_IT_RXNE | I2C_IT_ERR | I2C_IT_TC | I2C_IT_TCR);
                
            CurrentState.writeable = 0;
            CurrentState.readable = 0;

            if (iic->callback != NULL)
            {
                iic->callback(iic, IIC_EVENT_RECEIVE_COMPLETE, iic->arg);
                iic->callback = NULL;
                iic->arg = NULL;
            }
        }
    }
}

static void i2c_Master_TCR(csi_iic_t *iic)
{
    reg_i2c_t *reg = (reg_i2c_t *)iic->dev.reg_base;
    if (iic->size > 0xFF)
    {
        MODIFY_REG(reg->CR2, I2C_CR2_NBYTES_MASK, ((uint32_t)0x0000FF) << I2C_CR2_NBYTES_POS);
        SET_BIT(reg->CR2, I2C_CR2_RELOAD_MASK);

    }
    else 
    {
        MODIFY_REG(reg->CR2, I2C_CR2_NBYTES_MASK, ((uint32_t)iic->size) << I2C_CR2_NBYTES_POS);
        CLEAR_BIT(reg->CR2, I2C_CR2_RELOAD_MASK);
        SET_BIT(reg->CR2, I2C_CR2_AUTOEND_MASK);
    }
}

void csi_iic_isr(csi_iic_t *iic)
{
    reg_i2c_t *reg = (reg_i2c_t *)iic->dev.reg_base;
    uint32_t sr1itflags = reg->SR;
    uint32_t itsources  = reg->IVS;

    csi_state_t CurrentState = iic->state;
    // uint32_t error = HAL_I2C_ERROR_NONE;

    if(__CSI_I2C_CHECK_FLAG(sr1itflags, I2C_FLAG_NACK) && __CSI_I2C_CHECK_IT_SOURCE(itsources, I2C_IER_NACKIE_MASK))
    {
        __CSI_I2C_CLEAR_FLAG(reg, I2C_FLAG_NACK);

        __CSI_I2C_CLEAR_IF(reg, I2C_IER_NACKIE_MASK);

        if (CurrentState.writeable == 1)
        {
            if(iic->size != 0)
            {
                // error  |= HAL_I2C_ERROR_NACKF;
                if (iic->mode == IIC_MODE_MASTER)
                {
                    SET_BIT(reg->CR2, I2C_CR2_STOP_MASK);
                }
            }
        }
    }

    /* Check STOP flag */
    if (__CSI_I2C_CHECK_FLAG(sr1itflags, I2C_FLAG_STOPF))
    {
        __CSI_I2C_CLEAR_IF(reg, I2C_IER_STOPIE_MASK);	
    }

    /* Check ADDR flag */
    if (__CSI_I2C_CHECK_FLAG(sr1itflags, I2C_FLAG_ADDR))
    {
        __CSI_I2C_CLEAR_IF(reg, I2C_IER_ADDRIE_MASK);	
    }

    /*  Check TX flag -----------------------------------------------*/
    if (__CSI_I2C_CHECK_FLAG(sr1itflags, I2C_FLAG_TXE) && __CSI_I2C_CHECK_IT_SOURCE(itsources, I2C_IT_TXE))
    {
        if(iic->mode == IIC_MODE_MASTER)
        {
            i2c_MasterTransmit_TXE(iic);
        }
    }
    /* Check RXNE flag -----------------------------------------------*/
    if (__CSI_I2C_CHECK_FLAG(sr1itflags, I2C_FLAG_RXNE) && __CSI_I2C_CHECK_IT_SOURCE(itsources, I2C_IT_RXNE))
    {
        if(iic->mode == IIC_MODE_MASTER)
        {
            i2c_MasterReceive_RXNE(iic);
        }
    }
    /*  Check TCR flag -----------------------------------------------*/
    if (__CSI_I2C_CHECK_FLAG(sr1itflags, I2C_FLAG_TCR) && __CSI_I2C_CHECK_IT_SOURCE(itsources, I2C_IT_TCR))
    {
        __CSI_I2C_CLEAR_IF(reg, I2C_IER_TCRIE_MASK);	
        
        if(iic->mode  == IIC_MODE_MASTER)
        {
            i2c_Master_TCR(iic);
        }		
    }
    /*  Check TC flag -----------------------------------------------*/
    if (__CSI_I2C_CHECK_FLAG(sr1itflags, I2C_FLAG_TC) && __CSI_I2C_CHECK_IT_SOURCE(itsources, I2C_IT_TC))
    {
        __CSI_I2C_CLEAR_IF(reg, I2C_IER_TCIE_MASK);	
        // if(hi2c->XferCount != 0)
        // {
        //     error  |= HAL_I2C_ERROR_SIZE;
        // }
    }
}
