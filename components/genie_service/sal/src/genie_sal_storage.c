/*
 * Copyright (C) 2018-2021 Alibaba Group Holding Limited
 */

#include "genie_mesh_internal.h"

#ifdef CONFIG_GENIE_MESH_PORT_YOC

#include "drv/spiflash.h"
#include "dut/hal/common.h"
#include "dut/hal/ble.h"

#define FLASH_ALIGN_MASK ~(sizeof(uint32_t) - 1)
#define FLASH_ALIGN sizeof(uint32_t)

//#define MAC_PARAMS_OFFSET (0)
//#define MAC_PARAMS_SIZE (0x10)

//#define RSV_PARAMS_OFFSET (MAC_PARAMS_OFFSET + MAC_PARAMS_SIZE)
//#define RSV_PARAMS_SIZE (0x10)

//#define FREQ_PARAMS_OFFSET (RSV_PARAMS_OFFSET + RSV_PARAMS_SIZE)
//#define FREQ_PARAMS_SIZE (0x8)

//#define TRIPLE_OFFSET (FREQ_PARAMS_OFFSET + FREQ_PARAMS_SIZE)
#define TRIPLE_OFFSET (0)
#define TRIPLE_SIZE (32)

#define GROUP_ADDR_OFFSET (TRIPLE_OFFSET + TRIPLE_SIZE)
#define GROUP_ADDR_SIZE (18) //CONFIG_BT_MESH_MODEL_GROUP_COUNT * 2 + 2

#define SN_PARAMS_OFFSET (GROUP_ADDR_OFFSET + GROUP_ADDR_SIZE)
#define SN_PARAMS_SIZE (32)

//#define OTP_TOTAL_DATA_SIZE (MAC_PARAMS_SIZE + RSV_PARAMS_SIZE + FREQ_PARAMS_SIZE + TRIPLE_SIZE + GROUP_ADDR_SIZE + SN_PARAMS_SIZE)
#define OTP_TOTAL_DATA_SIZE (TRIPLE_SIZE + GROUP_ADDR_SIZE + SN_PARAMS_SIZE)

int32_t hal_flash_read_otp(uint32_t off_set, void *out_buf, uint32_t out_buf_len)
{
    return dut_hal_factorydata_read(off_set, out_buf, out_buf_len);
}

int32_t hal_flash_write_otp(uint32_t off_set, const void *in_buf, uint32_t in_buf_len)
{
    int32_t retval = 0;

    if (off_set + in_buf_len > OTP_TOTAL_DATA_SIZE)
    {
        printf("param err\n");
        return -1;
    }

    retval = dut_hal_factorydata_store(off_set, (uint8_t *)in_buf, in_buf_len);

    return retval;
}

int32_t hal_flash_read_xtalcap_params(void *out_buf, uint32_t out_buf_len)
{
    uint32_t xtal_cap = 0;
    int ret = -1;
    if (out_buf_len != sizeof(uint32_t))
    {
        return -1;
    }

    ret = dut_hal_xtalcap_get(&xtal_cap);

    memcpy(out_buf, &xtal_cap, sizeof(uint32_t));

    return ret;
}

int32_t hal_flash_write_xtalcap_params(const void *in_buf, uint32_t in_buf_len)
{
    if (in_buf_len != sizeof(uint32_t))
    {
        return -1;
    }

    return dut_hal_xtalcap_store(*((uint32_t *)in_buf));
}

int32_t hal_flash_read_sn_params(void *out_buf, uint32_t out_buf_len)
{
    return hal_flash_read_otp(SN_PARAMS_OFFSET, out_buf, out_buf_len);
}

int32_t hal_flash_write_sn_params(const void *in_buf, uint32_t in_buf_len)
{
    return hal_flash_write_otp(SN_PARAMS_OFFSET, in_buf, in_buf_len);
}

int32_t hal_flash_read_mac_params(void *out_buf, uint32_t out_buf_len)
{
    if (!out_buf || out_buf_len < 6)
        return -1;

    return dut_hal_mac_get((uint8_t *)out_buf);
}

int32_t hal_flash_write_mac_params(const void *in_buf, uint32_t in_buf_len)
{
    if (!in_buf || in_buf_len != 6)
        return -1;

    return dut_hal_mac_store((uint8_t *)in_buf);
}

int32_t hal_flash_read_triples(void *out_buf, uint32_t out_buf_len)
{
    return hal_flash_read_otp(TRIPLE_OFFSET, out_buf, out_buf_len);
}

int32_t hal_flash_write_triples(const void *in_buf, uint32_t in_buf_len)
{
    return hal_flash_write_otp(TRIPLE_OFFSET, in_buf, in_buf_len);
}

int32_t hal_flash_read_group_addr(void *out_buf, uint32_t out_buf_len)
{
    return hal_flash_read_otp(GROUP_ADDR_OFFSET, out_buf, out_buf_len);
}

int32_t hal_flash_write_group_addr(const void *in_buf, uint32_t in_buf_len)
{
    return hal_flash_write_otp(GROUP_ADDR_OFFSET, in_buf, in_buf_len);
}

int32_t hal_flash_addr2offset(hal_partition_t *in_partition, uint32_t *off_set, uint32_t addr)
{ //partition_start_addr
    hal_logic_partition_t partition_info;
    int ret;
    for (int i = 0; i < HAL_PARTITION_MAX; i++)
    {
        ret = hal_flash_info_get(i, &partition_info);
        if (ret)
            continue;

        if ((addr >= partition_info.partition_start_addr) && (addr < (partition_info.partition_start_addr + partition_info.partition_length)))
        {
            *in_partition = i;
            *off_set = addr - partition_info.partition_start_addr;
            return 0;
        }
    }
    *in_partition = -1;
    *off_set = 0;
    return -1;
}
#endif
