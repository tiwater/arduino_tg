/*
 * Copyright (C) 2018-2021 Alibaba Group Holding Limited
 */

#include "genie_mesh_internal.h"

#ifdef TG7100CEVB
#include <hal_boot2.h>
#endif

#ifdef CONFIG_GENIE_MESH_PORT_YOC
#include <yoc/partition.h>
#endif

#ifdef TG7100CEVB
#define OTA_IMAGE_RESERVE_SIZE (0)
#else
#define OTA_IMAGE_RESERVE_SIZE (0x1000 << 1) //TG7100B TG712XB
#endif

static unsigned short image_crc16 = 0;

static uint16_t genie_ota_crc16_ccitt(uint8_t const *p_data, uint32_t size, uint16_t const *p_crc)
{
    uint8_t b = 0;
    uint16_t crc = (p_crc == NULL) ? 0xFFFF : *p_crc;

    if (p_crc == NULL)
    {
        b = 0;
    }

    for (uint32_t i = 0; i < size; i++)
    {
        for (uint8_t j = 0; j < 8; j++)
        {
            b = ((p_data[i] << j) & 0x80) ^ ((crc & 0x8000) >> 8);
            crc <<= 1;
            if (b != 0)
            {
                crc ^= 0x1021;
            }
        }
    }

    return crc;
}

static uint32_t hal_ota_get_partition_length(void)
{
#ifdef CONFIG_GENIE_MESH_PORT_YOC
    partition_t ota_partition_handle = -1;
    partition_info_t *ota_partition_info = NULL;

    ota_partition_handle = partition_open("misc");

    if (ota_partition_handle < 0)
    {
        printf("ota open fail:%d\n", ota_partition_handle);
        return 0;
    }

    ota_partition_info = partition_info_get(ota_partition_handle);
    if (ota_partition_info == NULL)
    {
        printf("ota get info fail\n");
        partition_close(ota_partition_handle);
        return 0;
    }

    partition_close(ota_partition_handle);

    return ota_partition_info->length;
#else
    hal_logic_partition_t *partition_info = NULL;

    partition_info = hal_flash_get_info(HAL_PARTITION_OTA_TEMP);
    if (partition_info == NULL)
    {
        printf("ota read partition fail\n");
        return 0;
    }
    else
    {
        return partition_info->partition_length;
    }
#endif

    return 0;
}

static int hal_ota_flash_read(uint32_t offset, uint8_t *buffer, uint32_t buffer_len)
{
#ifdef CONFIG_GENIE_MESH_PORT_YOC
    int ret = -1;
    partition_t ota_partition_handle = -1;

    ota_partition_handle = partition_open("misc");

    if (ota_partition_handle < 0)
    {
        printf("ota open fail:%d\n", ota_partition_handle);
        return 0;
    }

    ret = partition_read(ota_partition_handle, offset, (void *)buffer, buffer_len);
    if (ret < 0)
    {
        printf("ota read fail\n");
        partition_close(ota_partition_handle);
        return -1;
    }

    partition_close(ota_partition_handle);
    return 0;
#else
    int ret = -1;

    ret = hal_flash_read(HAL_PARTITION_OTA_TEMP, &offset, (void *)buffer, buffer_len);
    if (ret < 0)
    {
        printf("ota read fail\n");
        return -1;
    }

    return 0;
#endif
}

static uint32_t hal_ota_flash_write(uint32_t offset, uint8_t *buffer, uint32_t buffer_len)
{
#ifdef CONFIG_GENIE_MESH_PORT_YOC
    int ret = -1;
    partition_t ota_partition_handle = -1;

    ota_partition_handle = partition_open("misc");

    if (ota_partition_handle < 0)
    {
        printf("ota open fail:%d\n", ota_partition_handle);
        return 0;
    }

    ret = partition_write(ota_partition_handle, offset, (void *)buffer, buffer_len);
    if (ret < 0)
    {
        printf("ota write fail\n");
        partition_close(ota_partition_handle);
        return -1;
    }

    partition_close(ota_partition_handle);
    return 0;
#else
    int ret = -1;

    ret = hal_flash_write(HAL_PARTITION_OTA_TEMP, &offset, (void *)buffer, buffer_len);
    if (ret < 0)
    {
        printf("ota write fail\n");
        return -1;
    }

    return 0;
#endif
}

static int hal_ota_flash_erase(uint32_t offset, uint32_t erase_size)
{
#ifdef CONFIG_GENIE_MESH_PORT_YOC
    int ret = -1;
    partition_t ota_partition_handle = -1;
    partition_info_t *ota_partition_info = NULL;

    ota_partition_handle = partition_open("misc");

    if (ota_partition_handle < 0)
    {
        printf("ota open fail:%d\n", ota_partition_handle);
        return -1;
    }

    ota_partition_info = partition_info_get(ota_partition_handle);
    if (ota_partition_info == NULL)
    {
        printf("ota get info fail\n");
        partition_close(ota_partition_handle);
        return -1;
    }

    ret = partition_erase(ota_partition_handle, offset, (erase_size + ota_partition_info->sector_size - 1) / ota_partition_info->sector_size);
    if (ret < 0)
    {
        printf("ota erase fail\n");
        partition_close(ota_partition_handle);
        return -1;
    }

    partition_close(ota_partition_handle);
    return 0;
#else
    int ret = -1;

    ret = hal_flash_erase(HAL_PARTITION_OTA_TEMP, offset, erase_size);
    if (ret < 0)
    {
        printf("ota erase fail\n");
        return -1;
    }

    return 0;
#endif
}

void genie_sal_ota_reboot(void)
{
    aos_reboot();
}

int genie_sal_ota_erase(void)
{
#define COMPARE_SIEZ (32)
    int ret;
    uint32_t offset = OTA_IMAGE_RESERVE_SIZE;
    uint32_t partition_length = 0;
    uint8_t cmp_buf[COMPARE_SIEZ] = {0xFF};
    uint8_t read_buf[COMPARE_SIEZ] = {0};

    memset(cmp_buf, 0xFF, COMPARE_SIEZ);
    ret = hal_ota_flash_read(offset, (void *)read_buf, COMPARE_SIEZ);
    if (ret < 0)
    {
        return -1;
    }

    if (memcmp(read_buf, cmp_buf, COMPARE_SIEZ) == 0)
    {
        return 0;
    }

    printf("erase ota partition\n");
    partition_length = hal_ota_get_partition_length();

    if (partition_length < offset)
    {
        printf("ota partition length err\n");
        return -1;
    }

    /* For bootloader upgrade, we will reserve two sectors, then save the image */
    ret = hal_ota_flash_erase(offset, partition_length - offset);
    if (ret < 0)
    {
        return -1;
    }

    return 0;
}

uint32_t genie_sal_ota_get_max_partition_size(void)
{
    uint32_t partition_length = 0;

    partition_length = hal_ota_get_partition_length();

    if (partition_length > OTA_IMAGE_RESERVE_SIZE)
    {
        return partition_length - OTA_IMAGE_RESERVE_SIZE;
    }

    return 0;
}

int genie_sal_ota_update_image(uint8_t image_type, uint32_t offset, uint32_t length, uint8_t *buffer)
{
    if (offset == 0)
    {
        image_crc16 = genie_ota_crc16_ccitt(buffer, length, NULL);
    }
    else
    {
        image_crc16 = genie_ota_crc16_ccitt(buffer, length, &image_crc16);
    }

    offset += OTA_IMAGE_RESERVE_SIZE; //Reserve a sector

    hal_ota_flash_write(offset, buffer, length);

    return 0;
}

unsigned char genie_sal_ota_check_checksum(short image_id, unsigned short *crc16_output)
{
    *crc16_output = image_crc16;

    return 1;
}

int32_t genie_sal_ota_update_boot_params(genie_sal_ota_ctx_t sal_ota_ctx)
{
    if (sal_ota_ctx.ota_ready == 1)
    {
#ifdef TG7100CEVB
        uint8_t activeID;
        HALPartition_Entry_Config ptEntry;

        int offset = 0;
        char *r_buf;

        activeID = hal_boot2_get_active_partition();

        GENIE_LOG_INFO("tg7100c ota active id:%d", activeID);

        if (hal_boot2_get_active_entries(BOOT2_PARTITION_TYPE_FW, &ptEntry))
        {
            printf("tg7100c ota get active entries fail\n");
            return -1;
        }

        ptEntry.len = sal_ota_ctx.image_size;

        GENIE_LOG_INFO("tg7100c ota image size:%lu", ptEntry.len);
        hal_boot2_update_ptable(&ptEntry);
#endif
    }

    return 0;
}
