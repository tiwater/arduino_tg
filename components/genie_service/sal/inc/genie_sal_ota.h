/*
 * Copyright (C) 2015-2018 Alibaba Group Holding Limited
 */

#ifndef __GENIE_SAL_OTA_H__
#define __GENIE_SAL_OTA_H__
#include <stdbool.h>

#ifndef CONFIG_AIS_TOTAL_FRAME
#define CONFIG_AIS_TOTAL_FRAME 16
#endif

typedef struct genie_sal_ota_ctx_s
{
    uint8_t image_type;
    uint32_t image_size;
    uint16_t image_crc16;
    uint8_t ota_ready;
} genie_sal_ota_ctx_t;

/**
 * @brief finish ota and reboot the device.
 */
void genie_sal_ota_reboot(void);

/**
 * @brief check the dfu image.
 * @param[in] the image type.
 * @param[out] the crc of image.
 * @return the result of checksum.
 */
unsigned char genie_sal_ota_check_checksum(short image_id, unsigned short *crc16_output);

/**
 * @brief write dfu data.
 * @param[in] the image type.
 * @param[in] the offset of flash.
 * @param[in] the length of data.
 * @param[in] the writting data.
 * @return the current runing partition.
 */
int genie_sal_ota_update_image(uint8_t image_type, uint32_t offset, uint32_t length, uint8_t *buffer);

uint32_t genie_sal_ota_get_max_partition_size(void);

#ifdef CONFIG_GENIE_OTA_PINGPONG
/**
 * @brief get the current runing partition.
 * @return the current runing partition.
 */
uint8_t genie_sal_ota_get_program_image(void);

/**
 * @brief switch the running partition, without reboot.
 * @param[in] the partition which switch to.
 * @return the runing partition when next boot.
 */
uint8_t genie_sal_ota_change_image_id(uint8_t target_id);
#endif

int genie_sal_ota_erase(void);

/*This api mainly for light product,if light is on we shouldn't reboot by ota*/
bool genie_sal_ota_is_allow_reboot(void);

int32_t genie_sal_ota_update_boot_params(genie_sal_ota_ctx_t sal_ota_ctx);

#endif
