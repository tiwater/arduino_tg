#include <drv/spi.h>
#include <drv/common.h>
#include <csi_kernel.h>
#include "reg_spi_type.h"
#include "field_manipulate.h"
#include "sdk_config.h"
#include "systick.h"
#include "spi_msp.h"
#include <cpu.h>
#include "platform.h"
#ifndef CONFIG_KERNEL_NONE
#define CSI_INTRPT_ENTER() aos_kernel_intrpt_enter()
#define CSI_INTRPT_EXIT()  aos_kernel_intrpt_exit()
#else
#define CSI_INTRPT_ENTER()
#define CSI_INTRPT_EXIT()
#endif

#define SPI_DR_REG_WRITE_ATOMIC(reg, spi_str, type)    do{                                                             \
                                                          uint32_t cpu_stat = enter_critical();                        \
                                                          *(type *)&(reg)->DR = *((type *)(spi_str)->tx_data);         \
                                                          (spi_str)->tx_data += sizeof(type);                          \
                                                          (spi_str)->tx_size--;                                        \
                                                          exit_critical(cpu_stat);                                     \
                                                       }while(0)

#define SPI_DEFAULT_TIMEOUT 100U

uint32_t CSI_SPI_MSP_Init(void *inst,uint32_t idx);
void CSI_SPI_MSP_DeInit(uint32_t idx);

static csi_error_t SPI_EndRxTransaction(csi_spi_t *spi, uint32_t Timeout, uint32_t Tickstart);
static csi_error_t SPI_EndRxTxTransaction(csi_spi_t *spi, uint32_t Timeout, uint32_t Tickstart);
static csi_error_t csi_spi_rxisr_8bit(csi_spi_t *spi, void *data, uint32_t size);
static csi_error_t csi_spi_rxisr_16bit(csi_spi_t *spi, void *data, uint32_t size);
static csi_error_t csi_spi_txisr_8bit(csi_spi_t *spi, const void *data, uint32_t size);
static csi_error_t csi_spi_txisr_16bit(csi_spi_t *spi, const void *data, uint32_t size);
static csi_error_t csi_spi_2linerxisr_16bit(csi_spi_t *spi, void *data, uint32_t size);
static csi_error_t csi_spi_2linetxisr_16bit(csi_spi_t *spi, const void *data, uint32_t size);
static csi_error_t csi_spi_2linerxisr_8bit(csi_spi_t *spi, void *data, uint32_t size);
static csi_error_t csi_spi_2linetxisr_8bit(csi_spi_t *spi, const void *data, uint32_t size);

static volatile uint8_t txrx_2line_flag = 0;

csi_error_t csi_spi_init(csi_spi_t *spi, uint32_t idx)
{
    spi->dev.idx = idx;
    spi->dev.reg_base = CSI_SPI_MSP_Init(spi,idx);
    reg_spi_t *reg = (reg_spi_t *)spi->dev.reg_base;
    WRITE_REG(reg->CR2, SPI_CR2_SSOE_MASK);
    return CSI_OK;
}

void csi_spi_uninit(csi_spi_t *spi)
{
    CSI_SPI_MSP_DeInit(spi->dev.idx);
}

csi_error_t csi_spi_attach_callback(csi_spi_t *spi, void *callback, void *arg)
{
    spi->callback = callback;
    spi->arg = arg;

    return CSI_OK;

}

void  csi_spi_detach_callback(csi_spi_t *spi)
{
    spi->callback = NULL;
    spi->arg = NULL;
}

csi_error_t csi_spi_mode(csi_spi_t *spi, csi_spi_mode_t mode)
{
  reg_spi_t *reg = (reg_spi_t *)spi->dev.reg_base;

  if(SPI_MASTER == mode)
  {
    SET_BIT(reg->CR1,SPI_CR1_MSTR_MASK | SPI_CR1_SSI_MASK);
  }
  else
  {
    CLEAR_BIT(reg->CR1,SPI_CR1_MSTR_MASK);
  }
  return CSI_OK;
}

csi_error_t csi_spi_cp_format(csi_spi_t *spi, csi_spi_cp_format_t format)
{
    reg_spi_t *reg = (reg_spi_t *)spi->dev.reg_base; 

    switch(format)
    {
    case SPI_FORMAT_CPOL0_CPHA0:
        CLEAR_BIT(reg->CR1,SPI_CR1_CPOL_MASK | SPI_CR1_CPHA_MASK);
    break;
    case SPI_FORMAT_CPOL0_CPHA1:
        MODIFY_REG(reg->CR1,SPI_CR1_CPOL_MASK | SPI_CR1_CPHA_MASK,SPI_CR1_CPHA_MASK);
    break;
    case SPI_FORMAT_CPOL1_CPHA0:
        MODIFY_REG(reg->CR1,SPI_CR1_CPOL_MASK | SPI_CR1_CPHA_MASK,SPI_CR1_CPOL_MASK);
    break;
    case SPI_FORMAT_CPOL1_CPHA1:
        SET_BIT(reg->CR1,SPI_CR1_CPOL_MASK | SPI_CR1_CPHA_MASK);
    break;
    default:
        CLEAR_BIT(reg->CR1,SPI_CR1_CPOL_MASK | SPI_CR1_CPHA_MASK);
    break;
    }
  return CSI_OK;
}

csi_error_t csi_spi_frame_len(csi_spi_t *spi, csi_spi_frame_len_t length)
{
    reg_spi_t *reg = (reg_spi_t *)spi->dev.reg_base;

    MODIFY_REG(reg->CR2,SPI_CR2_DS_MASK,(length - 1)<<SPI_CR2_DS_POS);
  return CSI_OK;
}

uint32_t csi_spi_baud(csi_spi_t *spi, uint32_t baud)
{
    uint8_t div = 2;
    uint32_t freq;
    reg_spi_t *reg = (reg_spi_t *)spi->dev.reg_base;

    // for( i=0;i<7;i++)
    // {
    //     div = 2^(i+1);
    //     if((SDK_PCLK_MHZ*1000000/div) <= baud)
    //       break;
    // }
    // if(i < 2)
    // {
    //   i=2;
    //   div = 2^(i+1);
    // }
    // i = 0x7; // TODO: for test!!!

    if (baud > SDK_PCLK_MHZ * 1000000 / 8)
    {
        baud = SDK_PCLK_MHZ * 1000000 / 8;
    }
    
    freq = (SDK_PCLK_MHZ * 1000000) / (1 << (div + 1));
    while (freq > baud && div <= 7) // search a freq less than or equal to baud
    {
        div++;
        freq = (SDK_PCLK_MHZ * 1000000) / (1 << (div + 1));
    }
    if (div > 7)
    {
        div = 7;
        freq = (SDK_PCLK_MHZ * 1000000) / 256;
    }
    
    MODIFY_REG(reg->CR1,SPI_CR1_BR_MASK,div << SPI_CR1_BR_POS);
    return freq;
}

static bool sci_spi_flag_poll(va_list va)
{
    csi_spi_t *spi = va_arg(va,csi_spi_t *);
    reg_spi_t *reg = (reg_spi_t *)spi->dev.reg_base;

    uint32_t flag = va_arg(va,uint32_t);

    if((reg->SR & flag) == flag)
    {
      return true;
    }
    else
    {
      return false;
    }
}
static bool sci_spi_flag_poll1(va_list va)
{
    csi_spi_t *spi = va_arg(va,csi_spi_t *);
    reg_spi_t *reg = (reg_spi_t *)spi->dev.reg_base;

    uint32_t flag = va_arg(va,uint32_t);

    if ((reg->SR & flag) == flag)
    {
      return false;
    }
    else
    {
      return true;
    }
}
static bool sci_spi_flag_poll2(va_list va)
{
    csi_spi_t *spi = va_arg(va,csi_spi_t *);
    reg_spi_t *reg = (reg_spi_t *)spi->dev.reg_base;

    uint32_t flag1 = va_arg(va,uint32_t);
    uint32_t flag2 = va_arg(va,uint32_t);
    if (((reg->SR & flag1) == flag1)  || ((reg->SR & flag2) == flag2))
    {
      return true;
    }
    else
    {
      return false;
    }
}

int32_t csi_spi_send(csi_spi_t *spi, const void *data, uint32_t size, uint32_t timeout)
{
  uint32_t tickstart = systick_get_value();
  uint32_t txallowed = 1;
  uint32_t Timeout = timeout * SDK_PCLK_MHZ * 1000;
  reg_spi_t *reg = (reg_spi_t *)spi->dev.reg_base;

  spi->tx_data = (uint8_t *)data;
  spi->tx_size = size;

  /* Check if the SPI is already enabled */
  if ((reg->CR1 & SPI_CR1_SPE_MASK) != SPI_CR1_SPE_MASK)
  {
    /* Enable SPI peripheral */
    SET_BIT(reg->CR1, SPI_CR1_SPE_MASK);
  }

  csi_spi_frame_len_t frame_len = ((reg->CR2&SPI_CR2_DS_MASK) == SPI_CR2_DS_MASK)? SPI_FRAME_LEN_16:SPI_FRAME_LEN_8;
  csi_spi_mode_t spi_mode = ((reg->CR1&SPI_CR1_MSTR_MASK) == SPI_CR1_MSTR_MASK)? SPI_MASTER:SPI_SLAVE;

  /* Transmit data in 16 Bit mode */
  if (frame_len == SPI_FRAME_LEN_16)
  {
    if (spi_mode == SPI_SLAVE) 
    {
      *(uint16_t *)&reg->DR = *((uint16_t *)spi->tx_data);
      spi->tx_data += sizeof(uint16_t);
      spi->tx_size--;
    }
    /* Transmit data in 16 Bit mode */
    while (spi->tx_size > 0U)
    {
        if(systick_poll_timeout(tickstart,Timeout,sci_spi_flag_poll,spi,SPI_SR_TXE_MASK))
        {
            return CSI_TIMEOUT;
        }
        else
        {
            *(uint16_t *)&reg->DR = *((uint16_t *)spi->tx_data);
            spi->tx_data += sizeof(uint16_t);
            spi->tx_size--;
        }
    }
  }
  /* Transmit data in 8 Bit mode */
  else
  {
    if(spi->tx_size >0)
    {
      *(uint8_t *)&reg->DR = *((uint8_t *)spi->tx_data);
      spi->tx_data += sizeof(uint8_t);
      spi->tx_size--;
      txallowed = 0U;
    }
    while (spi->tx_size > 0U)
    {
        if(systick_poll_timeout(tickstart,Timeout,sci_spi_flag_poll2,spi,SPI_SR_TXE_MASK,SPI_SR_RXNE_MASK))
        {
            return CSI_TIMEOUT;
        }
        else
        {
            if (((reg->SR & SPI_SR_TXE_MASK) == SPI_SR_TXE_MASK) && (spi->tx_size > 0U) && (txallowed == 1U))
            {
                // DELAY_US(1000);
                *(uint8_t *)&reg->DR =  *((uint8_t *)spi->tx_data);
                spi->tx_data += 1;
                spi->tx_size--;
                txallowed = 0U;
            }
              /* Check RXNE flag */
            if ((reg->SR & SPI_SR_RXNE_MASK) == SPI_SR_RXNE_MASK)
            {
                txallowed = reg->DR; //clear RXNE flag
                txallowed = 1U;
            }
        }

    }
  }

  /* Check the end of the transaction */
  if (SPI_EndRxTxTransaction(spi, timeout, tickstart) != CSI_OK)
  {
      return CSI_TIMEOUT;
  }
  return size - spi->tx_size;

}

csi_error_t csi_spi_send_async(csi_spi_t *spi, const void *data, uint32_t size)
{
  reg_spi_t *reg = (reg_spi_t *)spi->dev.reg_base;

  spi->tx_data = (uint8_t *)data;
  spi->tx_size = size;

  SET_BIT(reg->ICR, SPI_IER_TXEIE_MASK);
  SET_BIT(reg->IER, SPI_IER_TXEIE_MASK);

  /* Check if the SPI is already enabled */
  if ((reg->CR1 & SPI_CR1_SPE_MASK) != SPI_CR1_SPE_MASK)
  {
    /* Enable SPI peripheral */
    SET_BIT(reg->CR1, SPI_CR1_SPE_MASK);
  }

  csi_spi_frame_len_t frame_len = ((reg->CR2&SPI_CR2_DS_MASK) == SPI_CR2_DS_MASK)? SPI_FRAME_LEN_16:SPI_FRAME_LEN_8;

  if (frame_len == SPI_FRAME_LEN_16)
  {
    spi->send = csi_spi_txisr_16bit;
  }
  else
  {
    spi->send = csi_spi_txisr_8bit;
  }

  /* Transmit and Receive data in 16 Bit mode */
  if (frame_len == SPI_FRAME_LEN_16)
  {
      /* Check TXE flag */
      if((reg->SR & SPI_SR_TXE_MASK) == SPI_SR_TXE_MASK) 
      {
        SPI_DR_REG_WRITE_ATOMIC(reg, spi, uint16_t);
        // *(uint16_t *)&reg->DR = *((uint16_t *)spi->tx_data);
        // spi->tx_data += sizeof(uint16_t);
        // spi->tx_size--;
			}

  }
  /* Transmit and Receive data in 8 Bit mode */
  else
  {
      /* Check TXE flag */
      if((reg->SR & SPI_SR_TXE_MASK) == SPI_SR_TXE_MASK) 
      {
        SPI_DR_REG_WRITE_ATOMIC(reg, spi, uint8_t);
        // *(uint8_t *)&reg->DR = (*spi->tx_data);
        // spi->tx_data++;
        // spi->tx_size--;
			}
  }
	
  return CSI_OK;  
}

int32_t csi_spi_receive(csi_spi_t *spi, void *data, uint32_t size, uint32_t timeout)
{
  uint32_t tickstart = systick_get_value();
  uint32_t Timeout = timeout * SDK_PCLK_MHZ * 1000;
  reg_spi_t *reg = (reg_spi_t *)spi->dev.reg_base;

  csi_spi_mode_t spi_mode = ((reg->CR1&SPI_CR1_MSTR_MASK) == SPI_CR1_MSTR_MASK)? SPI_MASTER:SPI_SLAVE;

  if (spi_mode == SPI_MASTER)
  {
    return csi_spi_send_receive(spi, data, data, size, timeout);
  }

  /* Set the transaction information */
  spi->rx_data = (uint8_t *)data;
  spi->rx_size = size;

  /* Check if the SPI is already enabled */
  if ((reg->CR1 & SPI_CR1_SPE_MASK) != SPI_CR1_SPE_MASK)
  {
    /* Enable SPI peripheral */
    SET_BIT(reg->CR1, SPI_CR1_SPE_MASK);
  }

  csi_spi_frame_len_t frame_len = ((reg->CR2&SPI_CR2_DS_MASK) == SPI_CR2_DS_MASK)? SPI_FRAME_LEN_16:SPI_FRAME_LEN_8;

  /* Receive data in 8 Bit mode */
  if (frame_len == SPI_FRAME_LEN_8)
  {
    /* Transfer loop */
    while (spi->rx_size > 0U)
    {
      /* Check the RXNE flag */
     if(systick_poll_timeout(tickstart,Timeout,sci_spi_flag_poll,spi,SPI_SR_RXNE_MASK))
      {
          return CSI_TIMEOUT;
      }
      else
      {
        (* (uint8_t *)spi->rx_data) = *(uint8_t *)&reg->DR;
        spi->rx_data += sizeof(uint8_t);
        spi->rx_size--;
      }
    }
  }
  else
  {
    /* Transfer loop */
    while (spi->rx_size > 0U)
    {

     if(systick_poll_timeout(tickstart,Timeout,sci_spi_flag_poll,spi,SPI_SR_RXNE_MASK))
      {
          return CSI_TIMEOUT;
      }
      else
      {
          /* read the received data */
        (* (uint16_t *)spi->rx_data) = *(uint16_t *)&reg->DR;
        spi->rx_data += sizeof(uint16_t);
        spi->rx_size--;
      }
    }
  }

  /* Check the end of the transaction */
  if (SPI_EndRxTransaction(spi, timeout, tickstart) != CSI_OK)
  {
      return CSI_TIMEOUT;
  }
  return size - spi->rx_size;

}


csi_error_t csi_spi_receive_async(csi_spi_t *spi, void *data, uint32_t size)
{
  reg_spi_t *reg = (reg_spi_t *)spi->dev.reg_base;

  spi->rx_data = (uint8_t *)data;
  spi->rx_size = size;

  csi_spi_mode_t spi_mode = ((reg->CR1&SPI_CR1_MSTR_MASK) == SPI_CR1_MSTR_MASK)? SPI_MASTER:SPI_SLAVE;

  if (spi_mode == SPI_MASTER)
  {
    return csi_spi_send_receive_async(spi, data, data, size);
  }

  /* Set the transaction information */
  spi->rx_data = (uint8_t *)data;
  spi->rx_size = size;

  csi_spi_frame_len_t frame_len = ((reg->CR2&SPI_CR2_DS_MASK) == SPI_CR2_DS_MASK)? SPI_FRAME_LEN_16:SPI_FRAME_LEN_8;

  if (frame_len == SPI_FRAME_LEN_16)
  {
    spi->receive = csi_spi_rxisr_16bit;
  }
  else
  {
    spi->receive = csi_spi_rxisr_8bit;
  }

  /* Enable TXE and ERR interrupt */
  SET_BIT(reg->ICR, SPI_IER_RXNEIE_MASK );
  SET_BIT(reg->IER, SPI_IER_RXNEIE_MASK );

  /* Check if the SPI is already enabled */
  if ((reg->CR1 & SPI_CR1_SPE_MASK) != SPI_CR1_SPE_MASK)
  {
    /* Enable SPI peripheral */
    SET_BIT(reg->CR1, SPI_CR1_SPE_MASK);
  }

  return CSI_OK; 
}


int32_t csi_spi_send_receive(csi_spi_t *spi, const void *data_out, void *data_in, uint32_t size, uint32_t timeout)
{
  uint16_t             initial_TxXferCount;
  uint32_t             txallowed = 1U;
  uint32_t tickstart = systick_get_value();
  uint32_t Timeout = timeout * SDK_PCLK_MHZ * 1000;
  reg_spi_t *reg = (reg_spi_t *)spi->dev.reg_base;

  /* Init temporary variables */

  initial_TxXferCount = size;

  /* Set the transaction information */
  spi->rx_data  = (uint8_t *)data_in;
  spi->rx_size = size;

  spi->tx_data  = (uint8_t *)data_out;
  spi->tx_size = size;

  /* Check if the SPI is already enabled */
  if ((reg->CR1 & SPI_CR1_SPE_MASK) != SPI_CR1_SPE_MASK)
  {
    /* Enable SPI peripheral */
    SET_BIT(reg->CR1, SPI_CR1_SPE_MASK);
  }

  csi_spi_frame_len_t frame_len = ((reg->CR2&SPI_CR2_DS_MASK) == SPI_CR2_DS_MASK)? SPI_FRAME_LEN_16:SPI_FRAME_LEN_8;
  csi_spi_mode_t spi_mode = ((reg->CR1&SPI_CR1_MSTR_MASK) == SPI_CR1_MSTR_MASK)? SPI_MASTER:SPI_SLAVE;

  /* Transmit and Receive data in 16 Bit mode */
  if (frame_len == SPI_FRAME_LEN_16)
  {
    if ((spi_mode == SPI_SLAVE) || (initial_TxXferCount == 0x01U))
    {
      *(uint16_t *)&reg->DR = *((uint16_t *)spi->tx_data);
      spi->tx_data += sizeof(uint16_t);
      spi->tx_size--;
    }
    while ((spi->tx_size > 0U) || (spi->rx_size > 0U))
    {
        if(systick_poll_timeout(tickstart,Timeout,sci_spi_flag_poll2,spi,SPI_SR_TXE_MASK,SPI_SR_RXNE_MASK))
        {
            return CSI_TIMEOUT;
        }
        else
        {
            /* Check TXE flag */
            if (((reg->SR & SPI_SR_TXE_MASK) == SPI_SR_TXE_MASK)  && (spi->tx_size > 0U) )
            {
               while((REG_FIELD_RD(reg->SR,SPI_SR_TXFLV) != 0X0F)&&(spi->tx_size > 0U))
               {
                   *(uint16_t *)&reg->DR = *((uint16_t *)spi->tx_data);
                   spi->tx_data += sizeof(uint16_t);
                   spi->tx_size--;
               }
            }
              /* Check RXNE flag */
            if (((reg->SR & SPI_SR_RXNE_MASK) == SPI_SR_RXNE_MASK) && (spi->rx_size > 0U))
            {
                *((uint16_t *)spi->rx_data) = *(uint16_t *)&reg->DR;
                spi->rx_data += sizeof(uint16_t);
                spi->rx_size--;
            }
        }
    }
  }
  /* Transmit and Receive data in 8 Bit mode */
  else
  {
    if ((spi_mode == SPI_SLAVE) || (initial_TxXferCount == 0x01U))
    {
      *(( uint8_t *)&reg->DR) = (*spi->tx_data);
      spi->tx_data += sizeof(uint8_t);
      spi->tx_size--;
      txallowed = 0U;
    }
    while ((spi->tx_size > 0U) || (spi->rx_size > 0U))
    {
        if(systick_poll_timeout(tickstart,Timeout,sci_spi_flag_poll2,spi,SPI_SR_TXE_MASK,SPI_SR_RXNE_MASK))
        {
            return CSI_TIMEOUT;
        }
        else
        {
            /* Check TXE flag */
            if (((reg->SR & SPI_SR_TXE_MASK) == SPI_SR_TXE_MASK) && (spi->tx_size > 0U) && (txallowed == 1U))
            {
                *(( uint8_t *)&reg->DR) = (*spi->tx_data);
                spi->tx_data += sizeof(uint8_t);
                spi->tx_size--;
                /* Next Data is a reception (Rx). Tx not allowed */
                txallowed = 0U;

            }
            /* Check RXNE flag */
            if (((reg->SR & SPI_SR_RXNE_MASK) == SPI_SR_RXNE_MASK) && (spi->rx_size > 0U))
            {
                *((uint8_t *)spi->rx_data) = *(uint8_t *)&reg->DR;
                spi->rx_data += sizeof(uint8_t);
                spi->rx_size--;
                /* Next Data is a Transmission (Tx). Tx is allowed */
                txallowed = 1U;
            }
        }
    }
  }

  /* Check the end of the transaction */
  if (SPI_EndRxTxTransaction(spi, timeout, tickstart) != CSI_OK)
  {
    return CSI_TIMEOUT;
  }

  return size - spi->rx_size;  
}

csi_error_t csi_spi_send_receive_async(csi_spi_t *spi, const void *data_out, void *data_in, uint32_t size)
{
    reg_spi_t *reg = (reg_spi_t *)spi->dev.reg_base;

    spi->tx_data  = (uint8_t *)data_out;
    spi->tx_size  = size;
    spi->rx_data  = (uint8_t *)data_in;
    spi->rx_size  = size;

    SET_BIT(reg->ICR, (SPI_IER_TXEIE_MASK | SPI_IER_RXNEIE_MASK ));
    SET_BIT(reg->IER, (SPI_IER_TXEIE_MASK | SPI_IER_RXNEIE_MASK )); 

    csi_spi_frame_len_t frame_len = ((reg->CR2&SPI_CR2_DS_MASK) == SPI_CR2_DS_MASK)? SPI_FRAME_LEN_16:SPI_FRAME_LEN_8;

    if (frame_len == SPI_FRAME_LEN_16)
    {
        spi->receive  = csi_spi_2linerxisr_16bit;
        spi->send     = csi_spi_2linetxisr_16bit;
    }
    else
    {
        spi->receive  = csi_spi_2linerxisr_8bit;
        spi->send     = csi_spi_2linetxisr_8bit;
    }

    /* Check if the SPI is already enabled */
    if ((reg->CR1 & SPI_CR1_SPE_MASK) != SPI_CR1_SPE_MASK)
    {
      /* Enable SPI peripheral */
      SET_BIT(reg->CR1, SPI_CR1_SPE_MASK);
    }
    /* Transmit and Receive data in 16 Bit mode */
    if (frame_len == SPI_FRAME_LEN_16)
    {
        /* Check TXE flag */
        if ((reg->SR & SPI_SR_TXE_MASK) == SPI_SR_TXE_MASK)
        {
          SPI_DR_REG_WRITE_ATOMIC(reg, spi, uint16_t);
          // *(uint16_t *)&reg->DR = *((uint16_t *)spi->tx_data);
          // spi->tx_data += sizeof(uint16_t);
          // spi->tx_size--;
        }

    }
    /* Transmit and Receive data in 8 Bit mode */
    else
    {
        /* Check TXE flag */
        if ((reg->SR & SPI_SR_TXE_MASK) == SPI_SR_TXE_MASK)
        {
          SPI_DR_REG_WRITE_ATOMIC(reg, spi, uint8_t);
          // *( uint8_t *)&reg->DR = (*spi->tx_data);
          // spi->tx_data++;
          // spi->tx_size--;
        }
    }
    return CSI_OK; 
}
static void csi_spi_closerx_isr(csi_spi_t *spi)
{
  reg_spi_t *reg = (reg_spi_t *)spi->dev.reg_base;

  /* Disable RXNE and ERR interrupt */
  SET_BIT(reg->IDR,(SPI_IER_RXNEIE_MASK | SPI_IER_MODFIE_MASK | SPI_IER_OVRIE_MASK));

  /* Check the end of the transaction */
  if (SPI_EndRxTransaction(spi, SPI_DEFAULT_TIMEOUT * (SDK_PCLK_MHZ*1000000 / 24U / 1000U), systick_get_value()) != CSI_OK)
  {
    if(spi->callback)
    {
        spi->callback(spi,SPI_EVENT_ERROR,spi->arg);
    }    
    return;
  }

  /* Call user Rx complete callback */
  if(spi->callback)
  {
      spi->callback(spi,SPI_EVENT_RECEIVE_COMPLETE,spi->arg);
  }
}

static void csi_spi_closetx_isr(csi_spi_t *spi)
{
  reg_spi_t *reg = (reg_spi_t *)spi->dev.reg_base;
  uint32_t count = SPI_DEFAULT_TIMEOUT * (SDK_PCLK_MHZ*1000000 / 24U / 1000U);

  /* Wait until TXE flag is set */
  do
  {
    if (count == 0U)
    {
      break;
    }
    count--;
  } while ((reg->SR & SPI_SR_TXE_MASK) != SPI_SR_TXE_MASK);

  SET_BIT(reg->IDR, (SPI_IER_TXEIE_MASK | SPI_IER_MODFIE_MASK | SPI_IER_OVRIE_MASK));

  /* Check the end of the transaction */
  if (SPI_EndRxTxTransaction(spi, SPI_DEFAULT_TIMEOUT * (SDK_PCLK_MHZ*1000000 / 24U / 1000U), systick_get_value()) != CSI_OK)
  {
    if(spi->callback)
    {
        spi->callback(spi,SPI_EVENT_ERROR,spi->arg);
    }    
    return;
  }

  if(spi->callback)
  {
      spi->callback(spi,SPI_EVENT_SEND_COMPLETE,spi->arg);
  }
}
static void csi_spi_closerxtx_isr(csi_spi_t *spi)
{
  reg_spi_t *reg = (reg_spi_t *)spi->dev.reg_base;
  uint32_t count = SPI_DEFAULT_TIMEOUT * (SDK_PCLK_MHZ*1000000 / 24U / 1000U);

  SET_BIT(reg->IDR, (SPI_IER_MODFIE_MASK | SPI_IER_OVRIE_MASK));
  /* Wait until TXE flag is set */
  do
  {
    if (count == 0U)
    {
      break;
    }
    count--;
  } while ((reg->SR & SPI_SR_TXE_MASK) != SPI_SR_TXE_MASK);

  /* Check the end of the transaction */
  if (SPI_EndRxTxTransaction(spi, SPI_DEFAULT_TIMEOUT * (SDK_PCLK_MHZ*1000000 / 24U / 1000U), systick_get_value()) != CSI_OK)
  {
    if(spi->callback)
    {
        spi->callback(spi,SPI_EVENT_ERROR,spi->arg);
    }    
    return;
  }

  if(spi->callback)
  {
      spi->callback(spi,SPI_EVENT_SEND_RECEIVE_COMPLETE,spi->arg);
  }
  txrx_2line_flag = 0;
}
static csi_error_t csi_spi_rxisr_8bit(csi_spi_t *spi, void *data, uint32_t size)
{
  reg_spi_t *reg = (reg_spi_t *)spi->dev.reg_base;

  *spi->rx_data = (*(uint8_t *)&reg->DR);
  spi->rx_data++;
  spi->rx_size--;

  if (spi->rx_size == 0U)
  {
    csi_spi_closerx_isr(spi);
  }
  return CSI_OK;
}
static csi_error_t csi_spi_rxisr_16bit(csi_spi_t *spi, void *data, uint32_t size)
{
  reg_spi_t *reg = (reg_spi_t *)spi->dev.reg_base;

  *((uint16_t *)spi->rx_data) = *(uint16_t *)&(reg->DR);
  spi->rx_data += sizeof(uint16_t);
  spi->rx_size--;

  if (spi->rx_size == 0U)
  {
    csi_spi_closerx_isr(spi);
  }
  return CSI_OK;
}
static csi_error_t csi_spi_txisr_8bit(csi_spi_t *spi, const void *data, uint32_t size)
{
  reg_spi_t *reg = (reg_spi_t *)spi->dev.reg_base;
  DELAY_US(1000);
  *(uint8_t *)&reg->DR = (*spi->tx_data);
  spi->tx_data++;
  spi->tx_size--;

  if (spi->tx_size == 0U)
  {
    csi_spi_closetx_isr(spi);
  }
  return CSI_OK;
}
static csi_error_t csi_spi_txisr_16bit(csi_spi_t *spi, const void *data, uint32_t size)
{
    reg_spi_t *reg = (reg_spi_t *)spi->dev.reg_base;
    *(uint16_t *)&reg->DR = *((uint16_t *)spi->tx_data);
    spi->tx_data += sizeof(uint16_t);
    spi->tx_size--;
    if (spi->tx_size == 0U)
    {
      csi_spi_closetx_isr(spi);
    }
    return CSI_OK;
}
static csi_error_t csi_spi_2linerxisr_16bit(csi_spi_t *spi, void *data, uint32_t size)
{
  reg_spi_t *reg = (reg_spi_t *)spi->dev.reg_base;

  txrx_2line_flag |= 1 << 1;
  /* Receive data in 16 Bit mode */
  *((uint16_t *)spi->rx_data) = *(uint16_t *)&(reg->DR);
  spi->rx_data += sizeof(uint16_t);
  spi->rx_size--;

  if (txrx_2line_flag == 3 && spi->tx_size > 0)
  {
    *(uint16_t *)&reg->DR = (*spi->tx_data);
    spi->tx_data++;
    spi->tx_size--;

    txrx_2line_flag = 0;
  }

  if (spi->rx_size == 0U)
  {

    /* Disable RXNE interrupt */
    SET_BIT(reg->IDR, SPI_IER_RXNEIE_MASK);

    if (spi->tx_size == 0U)
    {
      csi_spi_closerxtx_isr(spi);
    }
  }
  return CSI_OK;
}

static csi_error_t csi_spi_2linetxisr_16bit(csi_spi_t *spi, const void *data, uint32_t size)
{
  reg_spi_t *reg = (reg_spi_t *)spi->dev.reg_base;
  txrx_2line_flag |= 1 << 0;
  /* Transmit data in 16 Bit mode */
  if (txrx_2line_flag == 3 && spi->tx_size > 0)
  {
    *(uint16_t *)&reg->DR = (*(uint16_t*)spi->tx_data);
    spi->tx_data += sizeof(uint16_t);
    spi->tx_size--;

    txrx_2line_flag = 0;
  }
  /* Enable CRC Transmission */
  if (spi->tx_size == 0U)
  {

    /* Disable TXE interrupt */
    SET_BIT(reg->IDR, SPI_IER_TXEIE_MASK);

    if (spi->rx_size == 0U)
    {
      csi_spi_closerxtx_isr(spi);
    }
  }
  return CSI_OK;
}

static csi_error_t csi_spi_2linerxisr_8bit(csi_spi_t *spi, void *data, uint32_t size)
{
  reg_spi_t *reg = (reg_spi_t *)spi->dev.reg_base;

  txrx_2line_flag |= 1 << 1;

  /* Receive data in 8bit mode */
  *spi->rx_data = *((uint8_t *)&reg->DR);
  spi->rx_data++;
  spi->rx_size--;

  if (txrx_2line_flag == 3 && spi->tx_size > 0)
  {
    // DELAY_US(1000);
    *(uint8_t *)&reg->DR = (*spi->tx_data);
    spi->tx_data++;
    spi->tx_size--;

    txrx_2line_flag = 0;
  }

  /* Check end of the reception */
  if (spi->rx_size == 0U)
  {
    /* Disable RXNE  and ERR interrupt */
    SET_BIT(reg->IDR, SPI_IER_RXNEIE_MASK | SPI_IER_MODFIE_MASK | SPI_IER_OVRIE_MASK);

    if (spi->tx_size == 0U)
    {
      csi_spi_closerxtx_isr(spi);
    }
  }
  
  return CSI_OK;
}

static csi_error_t csi_spi_2linetxisr_8bit(csi_spi_t *spi, const void *data, uint32_t size)
{
  reg_spi_t *reg = (reg_spi_t *)spi->dev.reg_base;

  txrx_2line_flag |= 1 << 0;

  if (txrx_2line_flag == 3 && spi->tx_size > 0)
  {
    // DELAY_US(1000);
    *(uint8_t *)&reg->DR = (*spi->tx_data);
    spi->tx_data++;
    spi->tx_size--;

    txrx_2line_flag = 0;
  }
  /* Check the end of the transmission */
  if (spi->tx_size == 0U)
  {
    /* Disable TXE interrupt */
    SET_BIT(reg->IDR, SPI_IER_TXEIE_MASK);

    if (spi->rx_size == 0U)
    {
      csi_spi_closerxtx_isr(spi);
    }
  }
  return CSI_OK;
}

void csi_spi_isr(csi_spi_t *spi)
{
  CSI_INTRPT_ENTER();
  reg_spi_t *reg = (reg_spi_t *)spi->dev.reg_base;
  uint32_t itsource = reg->IVS;
  uint32_t itflag   = reg->SR;

  /* SPI in mode Receiver ----------------------------------------------------*/
  if (((itflag & SPI_SR_OVR_MASK) != SPI_SR_OVR_MASK) &&
      ((itflag & SPI_SR_RXNE_MASK) == SPI_SR_RXNE_MASK) && ((itsource & SPI_IER_RXNEIE_MASK) == SPI_IER_RXNEIE_MASK))
  {
		  SET_BIT(reg->ICR,SPI_IER_RXNEIE_MASK);
      spi->receive(spi,NULL,0);

  }

  /* SPI in mode Transmitter -------------------------------------------------*/
  if (((itflag & SPI_SR_TXE_MASK) == SPI_SR_TXE_MASK)  && ((itsource & SPI_IER_TXEIE_MASK) == SPI_IER_TXEIE_MASK))
  {
		  SET_BIT(reg->ICR,SPI_IER_TXEIE_MASK);
      spi->send(spi,NULL,0);
  }
 
  /* SPI in Error Treatment --------------------------------------------------*/
  if (((itflag & SPI_SR_MODF_MASK) == SPI_SR_MODF_MASK) || ((itflag & SPI_SR_OVR_MASK) == SPI_SR_OVR_MASK) )
     //  && ((itsource & (SPI_IER_MODFIE_MASK | SPI_IER_OVRIE_MASK)) == (SPI_IER_MODFIE_MASK | SPI_IER_OVRIE_MASK)))
  {
    /* SPI Overrun error interrupt occurred ----------------------------------*/
    if((itflag & SPI_SR_OVR_MASK) == SPI_SR_OVR_MASK )
    {
      uint32_t tmpreg_ovr = 0x00U;
      tmpreg_ovr = reg->SR;
      tmpreg_ovr = reg->DR;
      tmpreg_ovr = tmpreg_ovr;

    }
    /* SPI Frame error interrupt occurred ------------------------------------*/

	  SET_BIT(reg->ICR,(SPI_IER_MODFIE_MASK | SPI_IER_OVRIE_MASK));
    return;
  }
}

static csi_error_t SPI_EndRxTransaction(csi_spi_t *spi,  uint32_t Timeout, uint32_t Tickstart)
{
  reg_spi_t *reg = (reg_spi_t *)spi->dev.reg_base;
  csi_spi_mode_t spi_mode = ((reg->CR1&SPI_CR1_MSTR_MASK) == SPI_CR1_MSTR_MASK)? SPI_MASTER:SPI_SLAVE;

  if (spi_mode == SPI_MASTER)
  {
    /* Wait the RXNE reset */
    if(systick_poll_timeout(Tickstart,Timeout,sci_spi_flag_poll1,spi,SPI_SR_RXNE_MASK))
    {
      return CSI_TIMEOUT;
    }
  }
  else
  {
    /* Control the BSY flag */
    if(systick_poll_timeout(Tickstart,Timeout,sci_spi_flag_poll1,spi,SPI_SR_BSY_MASK))
    {
      return CSI_TIMEOUT;
    }
  }
  return CSI_OK;
}

static csi_error_t SPI_EndRxTxTransaction(csi_spi_t *spi, uint32_t Timeout, uint32_t Tickstart)
{

  /* Control the BSY flag */
  if(systick_poll_timeout(Tickstart,Timeout,sci_spi_flag_poll1,spi,SPI_SR_BSY_MASK))
  {
    return CSI_TIMEOUT;
  }
  return CSI_OK;
}