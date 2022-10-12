#include <drv/common.h>
#include <drv/spiflash.h>
#include "spi_flash.h"
#include "reg_base_addr.h"
#define FLASH_NAME "LS_SPI_FLASH"

csi_error_t csi_spiflash_qspi_init(csi_spiflash_t *spiflash, uint32_t qspi_idx, void *spi_cs_callback)
{
    return CSI_OK;
}

csi_error_t csi_spiflash_get_flash_info(csi_spiflash_t *spiflash, csi_spiflash_info_t *flash_info)
{
    flash_info->flash_name = FLASH_NAME;
    spi_flash_read_id((uint8_t *)&flash_info->flash_id);
    flash_info->flash_size = 1<<(flash_info->flash_id>>16&0xff);
    flash_info->xip_addr = FLASH_BASE_ADDR;
    flash_info->sector_size = FLASH_SECTOR_SIZE;
    flash_info->page_size = FLASH_PAGE_SIZE;
    return CSI_OK;
}

int32_t csi_spiflash_program(csi_spiflash_t *spiflash, uint32_t offset, const void *data, uint32_t size)
{
    uint32_t current = offset;
    uint16_t length;
    if(current % 256)
    {
        length = size > 256 - current % 256 ? 256 - current % 256 : size;
    }else
    {
        length = 0;
    }
    if(length)
    {
        spi_flash_quad_page_program(current - FLASH_BASE_ADDR,(void *)data,length);
        size -= length;
        current += length;
        data = (uint8_t *)data + length; 
    }
    while(size)
    {
        length = size > 256 ? 256 : size;
        spi_flash_quad_page_program(current - FLASH_BASE_ADDR,(void *)data,length);
        size -= length;
        current += length;
        data = (uint8_t *)data + length; 
    }
    return size;
}

int32_t csi_spiflash_read(csi_spiflash_t *spiflash, uint32_t offset, void *data, uint32_t size)
{
    spi_flash_quad_io_read(offset - FLASH_BASE_ADDR,data,size);
    return size;
}

csi_error_t csi_spiflash_erase(csi_spiflash_t *spiflash, uint32_t offset, uint32_t size)
{
    while(size)
    {
        spi_flash_sector_erase(offset);
        if(size > FLASH_SECTOR_SIZE)
        {
            size -= FLASH_SECTOR_SIZE;
            offset += FLASH_SECTOR_SIZE;
        }else
        {
            break;
        }
    }
    return CSI_OK;
}
