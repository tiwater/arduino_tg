/*
 * Copyright (C) 2019-2022 Alibaba Group Holding Limited
 */
#ifndef _GENIE_NPS_CONFIG_H_
#define _GENIE_NPS_CONFIG_H_

typedef struct _genie_nps_config
{
    uint8_t cr_enable;
    uint8_t send_ttl;
    uint16_t group_delay_min;
    uint16_t group_delay_max;
    uint16_t boot_interval;
    uint16_t per_interval;
    uint16_t adv_duration;
} genie_nps_config_t;

#define GENIE_NPS_CONFIG_SEND_TTL (3)
#define GENIE_NPS_CONFIG_ADV_DURATION 120
#define GENIE_NPS_CONFIG_GROUP_DELAY_MIN (200)  //ms
#define GENIE_NPS_CONFIG_GROUP_DELAY_MAX (8000) //ms
#define GENIE_NPS_CONFIG_BOOT_INTERVAL (10000)  //ms
#define GENIE_NPS_CONFIG_PER_INTERVAL (30)     //s
#define GENIE_NPS_CONFIG_APPEND_DELAY (20000)   //ms
#define GENIE_NPS_CONFIG_PER_DELAY_MIN (1000) //ms
#define GENIE_NPS_GUARD_TIME (12000)           //ms
#define GENIE_NPS_CONFIG_CR_ENABLE (1)

uint16_t genie_nps_config_group_delay_min_get(void);
uint16_t genie_nps_config_group_delay_max_get(void);
uint16_t genie_nps_config_boot_interval_get(void);
uint16_t genie_nps_config_per_interval_get(void);
uint16_t genie_nps_config_adv_duration_get(void);
uint8_t genie_nps_config_send_ttl_get(void);
uint8_t genie_nps_config_cr_enable_get(void);
int genie_nps_config_msg_recv(u8_t ctl_op, struct bt_mesh_net_rx *rx, struct net_buf_simple *buf);
void genie_nps_config_init(void);

#endif /* _GENIE_NPS_CONFIG_H_ */
