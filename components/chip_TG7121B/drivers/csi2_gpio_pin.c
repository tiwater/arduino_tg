#include <drv/gpio_pin.h>
#include <drv/gpio.h>   
#include "reg_lsgpio_type.h"    
#include "reg_lsgpio.h"
#include "field_manipulate.h"   
#include "reg_syscfg.h"
#include "io_config.h"

#define CONFIG_PIN_NUM 36

static csi_gpio_pin_t *pin_manage[CONFIG_PIN_NUM];

csi_error_t csi_gpio_pin_init(csi_gpio_pin_t *pin, pin_name_t pin_name)
{   
    pin_manage[pin_name] = pin;
    pin->pin_idx = pin_name;
    pin->callback = NULL;
    pin->arg = NULL;
    return CSI_OK;
}


void csi_gpio_pin_uninit(csi_gpio_pin_t *pin)
{
    uint8_t pin_num = pin->pin_idx;
    io_cfg_disable(pin_num);
    pin->pin_idx  = 0;
    pin->callback = NULL;
    pin->arg = NULL;
    pin_manage[pin_num] = NULL;
}

void csi_gpio_pin_write(csi_gpio_pin_t *pin, csi_gpio_pin_state_t value)
{
    io_write_pin(pin->pin_idx, value);
}

csi_gpio_pin_state_t csi_gpio_pin_read(csi_gpio_pin_t *pin)
{
    return io_read_pin(pin->pin_idx);
}

void csi_gpio_pin_toggle(csi_gpio_pin_t *pin)
{
    io_toggle_pin(pin->pin_idx);
}

csi_error_t csi_gpio_pin_dir(csi_gpio_pin_t *pin, csi_gpio_dir_t dir)
{
    if(dir == GPIO_DIRECTION_INPUT)
    {
        io_cfg_input(pin->pin_idx);
    }else
    {
        io_cfg_output(pin->pin_idx);
    }   
    return CSI_OK;
}


csi_error_t csi_gpio_pin_mode(csi_gpio_pin_t *pin, csi_gpio_mode_t mode)
{   
    switch(mode){
        case GPIO_MODE_PULLNONE:
            io_pull_write(pin->pin_idx,IO_PULL_DISABLE);
            break;

        case GPIO_MODE_PULLUP:
            io_pull_write(pin->pin_idx,IO_PULL_UP);
            break;

        case GPIO_MODE_PULLDOWN:
            io_pull_write(pin->pin_idx,IO_PULL_DOWN);
            break;
        
        case GPIO_MODE_OPEN_DRAIN:
            io_cfg_opendrain(pin->pin_idx);
            break;
        
        case GPIO_MODE_PUSH_PULL:
            io_cfg_pushpull(pin->pin_idx);
            break;

        default:
            return CSI_UNSUPPORTED;
    }
    return CSI_OK;
}

csi_error_t csi_gpio_pin_irq_mode(csi_gpio_pin_t *pin, csi_gpio_irq_mode_t mode)
{
    csi_error_t ret = CSI_OK;

    switch(mode)
    {
        case GPIO_IRQ_MODE_RISING_EDGE:
            io_exti_config(pin->pin_idx,INT_EDGE_RISING);
            break;

        case GPIO_IRQ_MODE_FALLING_EDGE:
            io_exti_config(pin->pin_idx,INT_EDGE_FALLING);
            break;

        default:
            ret = CSI_UNSUPPORTED;
    }
    return ret;
}

csi_error_t csi_gpio_pin_irq_enable(csi_gpio_pin_t *pin, bool enable)
{
    if(enable == true)
    {
        io_cfg_input(pin->pin_idx);
        io_ext_intrp_enable(pin->pin_idx);
    }else
    {
        io_ext_intrp_disable(pin->pin_idx);
    }
    return CSI_OK;
}

csi_error_t csi_gpio_pin_attach_callback(csi_gpio_pin_t *pin, void *callback, void *arg)
{
    pin->callback = callback;
    pin->arg = arg;

    return CSI_OK;
}

void io_exti_callback(uint8_t pin)
{
    if(pin_manage[pin])
    {
        if(pin_manage[pin]->callback)
        {
            pin_manage[pin]->callback(pin_manage[pin], pin_manage[pin]->arg);
        }
    }
}