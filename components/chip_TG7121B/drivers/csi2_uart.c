#include <drv/uart.h>
#include <drv/common.h>
#include <csi_kernel.h>
#include "reg_uart_type.h"
#include "field_manipulate.h"
#include "uart_misc.h"
#include "sdk_config.h"
#include "systick.h"
#ifndef CONFIG_KERNEL_NONE
#define CSI_INTRPT_ENTER() aos_kernel_intrpt_enter()
#define CSI_INTRPT_EXIT()  aos_kernel_intrpt_exit()
#else
#define CSI_INTRPT_ENTER()
#define CSI_INTRPT_EXIT()
#endif
uint32_t CSI_UART_MSP_Init(void *inst,uint32_t idx);
void CSI_UART_MSP_DeInit(uint32_t idx);

csi_error_t csi_uart_init(csi_uart_t *uart, uint32_t idx)
{
    uart->dev.idx = idx;
    uart->dev.reg_base = CSI_UART_MSP_Init(uart,idx);
    reg_uart_t *reg = (reg_uart_t *)uart->dev.reg_base;
    reg->FCR = UART_FCR_TFRST_MASK | UART_FCR_RFRST_MASK | UART_FCR_FIFOEN_MASK;
    return CSI_OK;
}

void csi_uart_uninit(csi_uart_t *uart)
{
    CSI_UART_MSP_DeInit(uart->dev.idx);
}

csi_error_t csi_uart_format(csi_uart_t *uart, csi_uart_data_bits_t data_bits, csi_uart_parity_t parity, csi_uart_stop_bits_t stop_bits)
{
    reg_uart_t *reg = (reg_uart_t *)uart->dev.reg_base;
    reg->LCR = FIELD_BUILD(UART_LCR_DLS,data_bits)|FIELD_BUILD(UART_LCR_STOP,stop_bits)
                                  |FIELD_BUILD(UART_LCR_PS,parity)|FIELD_BUILD(UART_LCR_MSB,0)
                                  |FIELD_BUILD(UART_LCR_RXEN,1);
    return CSI_OK;
}

csi_error_t csi_uart_baud(csi_uart_t *uart, uint32_t baud)
{
    reg_uart_t *reg = (reg_uart_t *)uart->dev.reg_base;
    REG_FIELD_WR(reg->LCR,UART_LCR_BRWEN,1);
    reg->BRR = ((((SDK_PCLK_MHZ*1000000)<<4)/baud) +8)>>4;
    REG_FIELD_WR(reg->LCR,UART_LCR_BRWEN,0);
    return CSI_OK;
}

static bool csi_uart_rx_poll(csi_uart_t *uart)
{
    reg_uart_t *reg = (reg_uart_t *)uart->dev.reg_base;
    if(reg->SR & UART_SR_RFNE)
    {
        *uart->rx_data++ = reg->RBR; 
        uart->rx_size -= 1;
    }
    return uart->rx_size ? false : true;
}

static bool csi_uart_receive_poll(va_list arg)
{
    csi_uart_t *uart = va_arg(arg,csi_uart_t *);
    return csi_uart_rx_poll(uart);
}

int32_t csi_uart_receive(csi_uart_t *uart, void *data, uint32_t size, uint32_t timeout)
{
    uint32_t current = systick_get_value();
    uart->rx_data = data;
    uart->rx_size = size;
    if(timeout == 0xffffffff)
    {
        while(csi_uart_rx_poll(uart)==false);
    }else{
        systick_poll_timeout(current,timeout*SDK_HCLK_MHZ*1000,csi_uart_receive_poll,uart);
    }
    return size - uart->rx_size;
}

csi_error_t csi_uart_send_async(csi_uart_t *uart, const void *data, uint32_t size)
{
    uart->tx_data = (uint8_t *)data;
    uart->tx_size = size;
    reg_uart_t *reg = (reg_uart_t *)uart->dev.reg_base;
    REG_FIELD_WR(reg->FCR,UART_FCR_TXTL,UART_FIFO_TL_0);
    reg->ICR = UART_IT_TC| UART_IT_TXS;
    reg->IER = UART_IT_TXS;
    uart->tx_size -= 1;
    reg->TBR = *uart->tx_data++;
    if(uart->tx_size == 0)
    {
        reg->IER = UART_IT_TC;
    }
    return CSI_OK;
}

csi_error_t csi_uart_attach_callback(csi_uart_t *uart, void *callback, void *arg)
{
    uart->callback = callback;
    uart->arg = arg;
    reg_uart_t *reg = (reg_uart_t *)uart->dev.reg_base;
    reg->IER = UART_RXRD_MASK;
    return CSI_OK;
}

void csi_uart_isr(csi_uart_t *uart)
{
    CSI_INTRPT_ENTER();
    reg_uart_t *reg = (reg_uart_t *)uart->dev.reg_base;
    uint32_t ifm = reg->IFM;
    if(ifm & UART_IT_RXRD)
    {
        if(uart->callback)
        {
            uart->callback(uart,UART_EVENT_RECEIVE_FIFO_READABLE,uart->arg);
        }else
        {
            while(reg->SR & UART_SR_RFNE)
            {
            reg->RBR;
            }
        }
    }
    if(ifm & UART_IT_TXS)
    {
        reg->ICR = UART_IT_TXS;
        if(!REG_FIELD_RD(reg->FCR, UART_FCR_TXFL))
        {
            if (uart->tx_size == 0U)
            {
                reg->IDR = UART_IT_TXS;
                reg->IER = UART_IT_TC;
            }
            else
            {
                while(REG_FIELD_RD(reg->SR,  UART_SR_TFNF))
                {
                    if (uart->tx_size == 0U)
                    {
                        break;
                    }
                    uart->tx_size --;
                    reg->TBR = *uart->tx_data++;
                }
            }
        }
    }
    if(ifm & UART_IT_TC)
    {
        reg->ICR = UART_IT_TC;
        reg->IDR = UART_IT_TC;
        uart->callback(uart,UART_EVENT_SEND_COMPLETE,uart->arg);
    }
    CSI_INTRPT_EXIT();
}

void csi_uart_putc(csi_uart_t *uart, uint8_t ch)
{
    reg_uart_t *reg = (reg_uart_t *)uart->dev.reg_base;
    reg->TBR = ch;
    while (!(reg->SR & UART_SR_TEMT));
}
