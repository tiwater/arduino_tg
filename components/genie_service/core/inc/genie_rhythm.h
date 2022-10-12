/*
 * Copyright (C) 2019-2022 Alibaba Group Holding Limited
 */
#ifndef _GENIE_RHYTHM_H_
#define _GENIE_RHYTHM_H_

#include <net/buf.h>
#include "net.h"

#define GENIE_RHYTHM_UART_END_FLAG 0xFDFD
#define GENIE_RHYTHM_BIN_DATA_FLAG 0xA0A0
#define GENIE_RHYTHM_BIN_CMD_FLAG 0xA0B0
#define GENIE_RHYTHM_INVALID_ID 0xFFFF

#define GENIE_RHYTHM_GROUP_ADDR 0xc1ff

typedef enum _genie_rhythm_adv_type
{
    GENIE_RHYTHM_ADV_TYPE_GEN = 1,
    GENIE_RHYTHM_ADV_TYPE_EXT,
    GENIE_RHYTHM_ADV_TYPE_NONE
} genie_rhythm_adv_type_e;

typedef enum _genie_rhythm_ctl_type
{
    GENIE_RHYTHM_CTL_TYPE_CMD = 0,
    GENIE_RHYTHM_CTL_TYPE_DATA,
    GENIE_RHYTHM_CTL_TYPE_NONE
} genie_rhythm_send_ctl_type_e;

typedef enum _genie_rhythm_cmd_type
{
    GENIE_RHYTHM_CMD_START = 1,
    GENIE_RHYTHM_CMD_STOP,
    GENIE_CMD_RHYTHM_NONE
} genie_rhythm_cmd_type_e;

typedef struct _genie_rhythm_cmd
{
    uint16_t flag;
    uint16_t id;
    uint8_t cmd; /* 1:start, 2:stop */
} genie_rhythm_cmd_t;

typedef struct _genie_rhythm_data
{
    uint16_t flag;
    uint16_t id;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t cold;
    uint8_t warm;
} genie_rhythm_data_t;

void genie_rhythm_recv_init(void);
uint8_t genie_rhythm_send_stat(void);
int genie_rhythm_recv_msg(u8_t ctl_op, struct bt_mesh_net_rx *rx, struct net_buf_simple *buf);

#endif /* _GENIE_RHYTHM_H_ */
