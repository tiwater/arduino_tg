/*
 * Copyright (C) 2018-2021 Alibaba Group Holding Limited
 */

#ifndef __GENIE_ULTRA_PROV_H__
#define __GENIE_ULTRA_PROV_H__

#define GENIE_ULTRA_PROV_ADV_TYPE (0xFF)
#define GENIE_ULTRA_PROV_FIXED_BYTE (0x0D)

#define RESEND_DATA_TO_PROVISIONER_TIMEOUT K_MSEC(500)
#define SEND_DATA_TO_PROVISIONER_TIMEOUT K_SECONDS(3)
#define RESEND_DATA_TO_PROVISIONER_TIMES (SEND_DATA_TO_PROVISIONER_TIMEOUT / RESEND_DATA_TO_PROVISIONER_TIMEOUT)

#define ULTRA_PROV_RESEND_ENABLE

#define ULTRA_PROV_CACHE_SIZE (10)
#define ULTRA_PROV_DEDUPLICATE_DURATION (3000) //unit ms

typedef enum _genie_ultra_prov_type_e
{
    GENIE_ULTRA_PROV_TYPE_RANDOM,
    GENIE_ULTRA_PROV_TYPE_COMFIRM,
    GENIE_ULTRA_PROV_TYPE_PROV_DATA,
    GENIE_ULTRA_PROV_TYPE_PROV_COMPLETE,
    GENIE_ULTRA_PROV_TYPE_PROV_FAILED = 0xF0,
} genie_ultra_prov_type_e;

enum _ultra_prov_failed_reason_e
{
    ULTRA_PROV_SUCCESS,
    ULTRA_PROV_FAILED_NO_MEM,
    ULTRA_PROV_FAILED_SHA256,
    ULTRA_PROV_FAILED_MISMATCH_MAC,
    ULTRA_PROV_FAILED_WAIT_PROV_DATA_TIMEOUT,
    ULTRA_PROV_FAILED_WAIT_COMPLETE_ACK_TIMEOUT,
};

enum _state_expect_e
{
    STATE_EXPECT_NONE,
    STATE_EXPECT_PROV_DATA,
    STATE_EXPECT_PROV_COMPLETE_ACK,
};

typedef struct _genie_ultra_prov_ctx_s
{
    uint8_t random_a[8];
    uint8_t random_b[8];
    uint8_t expect;
#ifdef ULTRA_PROV_RESEND_ENABLE
    uint8_t attempts;
    struct k_delayed_work timer;
#endif
} genie_ultra_prov_ctx_t;

typedef struct _ultra_prov_cache_s
{
    uint16_t crc16;
    uint32_t time;
} genie_ultra_prov_cache_t;

int genie_ultra_prov_init(void);
int genie_ultra_prov_done(uint8_t reason);
int genie_ultra_prov_deinit(void);

int genie_ultra_prov_handle(uint8_t frame_type, void *frame_buf);

#endif
