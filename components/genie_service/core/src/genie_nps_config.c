/*
 * Copyright (C) 2018-2021 Alibaba Group Holding Limited
 */

#define BT_DBG_ENABLED IS_ENABLED(CONFIG_BT_MESH_DEBUG_ADV)
#include "genie_mesh_internal.h"

static genie_nps_config_t g_nps_config;

uint16_t genie_nps_config_group_delay_max_get(void)
{
    int ret = 0;
    int read_len = sizeof(uint16_t);
    uint16_t delay = 0;

    if (g_nps_config.group_delay_max == 0)
    {
        ret = aos_kv_get("group_delay_max", &delay, &read_len);
        if (ret != 0)
        {
            delay = GENIE_NPS_CONFIG_GROUP_DELAY_MAX;
        }
        GENIE_LOG_INFO("get group_delay_max:%u", delay);
        g_nps_config.group_delay_max = delay;
    }

    return g_nps_config.group_delay_max;
}

static int genie_nps_config_group_delay_max_set(uint16_t delay)
{
    int ret = 0;

    if (g_nps_config.group_delay_max != delay)
    {
        ret = aos_kv_set("group_delay_max", &delay, sizeof(uint16_t), 0);
        if (ret != 0)
        {
            GENIE_LOG_ERR("kv set failed, err:%d", ret);
            return ret;
        }
        GENIE_LOG_INFO("set group_delay_max:%u", delay);
        g_nps_config.group_delay_max = delay;
    }

    return 0;
}

uint16_t genie_nps_config_group_delay_min_get(void)
{
    int ret = 0;
    int read_len = sizeof(uint16_t);
    uint16_t delay = 0;

    if (g_nps_config.group_delay_min == 0)
    {
        ret = aos_kv_get("group_delay_min", &delay, &read_len);
        if (ret != 0)
        {
            delay = GENIE_NPS_CONFIG_GROUP_DELAY_MIN;
        }
        GENIE_LOG_INFO("get group_delay_min:%u", delay);
        g_nps_config.group_delay_min = delay;
    }

    return g_nps_config.group_delay_min;
}

static int genie_nps_config_group_delay_min_set(uint16_t delay)
{
    int ret = 0;

    if (g_nps_config.group_delay_min != delay)
    {
        ret = aos_kv_set("group_delay_min", &delay, sizeof(uint16_t), 0);
        if (ret != 0)
        {
            GENIE_LOG_ERR("kv set failed, err:%d", ret);
            return ret;
        }
        GENIE_LOG_INFO("set group_delay_min:%u", delay);
        g_nps_config.group_delay_min = delay;
    }

    return 0;
}

uint16_t genie_nps_config_boot_interval_get(void)
{
    int ret = 0;
    int read_len = sizeof(uint16_t);
    uint16_t interval = 0;

    if (g_nps_config.boot_interval == 0)
    {
        ret = aos_kv_get("boot_interval", &interval, &read_len);
        if (ret != 0)
        {
            interval = GENIE_NPS_CONFIG_BOOT_INTERVAL;
        }
        GENIE_LOG_INFO("get boot_interval:%u", interval);
        g_nps_config.boot_interval = interval;
    }

    return g_nps_config.boot_interval;
}

static int genie_nps_config_boot_interval_set(uint16_t interval)
{
    int ret = 0;

    if (g_nps_config.boot_interval != interval)
    {
        ret = aos_kv_set("boot_interval", &interval, sizeof(uint16_t), 0);
        if (ret != 0)
        {
            GENIE_LOG_ERR("kv set failed, err:%d", ret);
            return ret;
        }
        GENIE_LOG_INFO("set boot_interval:%u", interval);
        g_nps_config.boot_interval = interval;
    }

    return 0;
}

uint16_t genie_nps_config_per_interval_get(void)
{
    int ret = 0;
    int read_len = sizeof(uint16_t);
    uint16_t interval = 0;

    if (g_nps_config.per_interval == 0)
    {
        ret = aos_kv_get("per_interval", &interval, &read_len);
        if (ret != 0)
        {
            interval = GENIE_NPS_CONFIG_PER_INTERVAL;
        }
        GENIE_LOG_INFO("get per_interval:%u", interval);
        g_nps_config.per_interval = interval;
    }

    return g_nps_config.per_interval;
}

static int genie_nps_config_per_interval_set(uint16_t interval)
{
    int ret = 0;

    if (g_nps_config.per_interval != interval)
    {
        ret = aos_kv_set("per_interval", &interval, sizeof(uint16_t), 0);
        if (ret != 0)
        {
            GENIE_LOG_ERR("kv set failed, err:%d", ret);
            return ret;
        }
        GENIE_LOG_INFO("set per_interval:%u", interval);
        g_nps_config.per_interval = interval;
    }

    return 0;
}

uint8_t genie_nps_config_send_ttl_get(void)
{
    int ret = 0;
    int read_len = sizeof(uint8_t);
    uint8_t ttl = 0;

    if (g_nps_config.send_ttl == 0)
    {
        ret = aos_kv_get("send_ttl", &ttl, &read_len);
        if (ret != 0)
        {
            ttl = GENIE_NPS_CONFIG_SEND_TTL;
        }
        GENIE_LOG_DBG("get send_ttl:%u", ttl);
        g_nps_config.send_ttl = ttl;
    }

    return g_nps_config.send_ttl;
}

static int genie_nps_config_send_ttl_set(uint8_t ttl)
{
    int ret = 0;

    if (g_nps_config.send_ttl != ttl)
    {
        ret = aos_kv_set("send_ttl", &ttl, sizeof(uint8_t), 0);
        if (ret != 0)
        {
            GENIE_LOG_ERR("kv set failed, err:%d", ret);
            return ret;
        }
        GENIE_LOG_INFO("set send_ttl:%u", ttl);
        g_nps_config.send_ttl = ttl;
    }

    return 0;
}

uint8_t genie_nps_config_cr_enable_get(void)
{
    int ret = 0;
    int read_len = sizeof(uint8_t);
    uint8_t enable = 0;

    ret = aos_kv_get("cr_enable", &enable, &read_len);
    if (ret != 0)
    {
        enable = GENIE_NPS_CONFIG_CR_ENABLE;
    }
    GENIE_LOG_INFO("get cr_enable:%u", enable);
    g_nps_config.cr_enable = enable;

    return enable;
}

static int genie_nps_config_cr_enable_set(uint8_t enable)
{
    int ret = 0;

    if (g_nps_config.cr_enable != enable)
    {
        ret = aos_kv_set("cr_enable", &enable, sizeof(uint8_t), 0);
        if (ret != 0)
        {
            GENIE_LOG_ERR("kv set failed, err:%d", ret);
            return ret;
        }
        GENIE_LOG_INFO("set cr_enable:%u", enable);
        g_nps_config.cr_enable = enable;
    }

    return 0;
}

uint16_t genie_nps_config_adv_duration_get(void)
{
    int ret = 0;
    int read_len = sizeof(uint16_t);
    uint16_t duration = 0;

    if (g_nps_config.adv_duration == 0)
    {
        ret = aos_kv_get("adv_duration", &duration, &read_len);
        if (ret != 0)
        {
            duration = GENIE_NPS_CONFIG_ADV_DURATION;
        }
        GENIE_LOG_INFO("get adv_duration:%u", duration);
        g_nps_config.adv_duration = duration;
    }

    return g_nps_config.adv_duration;
}

static int genie_nps_config_adv_duration_set(uint16_t duration)
{
    int ret = 0;

    if (g_nps_config.adv_duration != duration)
    {
        ret = aos_kv_set("adv_duration", &duration, sizeof(uint16_t), 0);
        if (ret != 0)
        {
            GENIE_LOG_ERR("kv set failed, err:%d", ret);
            return ret;
        }
        GENIE_LOG_INFO("set adv_duration:%u", duration);
        g_nps_config.adv_duration = duration;
    }

    return 0;
}

void genie_nps_config_init(void)
{
    memset(&g_nps_config, 0, sizeof(genie_nps_config_t));
}

int genie_nps_config_msg_recv(u8_t ctl_op, struct bt_mesh_net_rx *rx, struct net_buf_simple *buf)
{
    genie_nps_config_t nps_config;

    if (!rx->local_match)
    {
        return 0;
    }

    if (ctl_op == TRANS_CTL_OP_NPS_CONFIG)
    {
        GENIE_LOG_INFO("[%u]src 0x%04x dst 0x%04x recv_ttl %u rssi %d len:%u data:%s", k_uptime_get_32(), rx->ctx.addr,
                       rx->ctx.recv_dst, rx->ctx.recv_ttl, rx->rssi, buf->len, bt_hex(buf->data, buf->len));

        if (buf->len < 10)
        {
            return -EINVAL;
        }

        nps_config.cr_enable = net_buf_simple_pull_u8(buf);
        nps_config.send_ttl = net_buf_simple_pull_u8(buf);
        nps_config.group_delay_min = net_buf_simple_pull_be16(buf);
        nps_config.group_delay_max = net_buf_simple_pull_be16(buf);
        nps_config.boot_interval = net_buf_simple_pull_be16(buf);
        nps_config.per_interval = net_buf_simple_pull_u8(buf);
        nps_config.adv_duration = net_buf_simple_pull_u8(buf);

        GENIE_LOG_INFO("cr_enable:%u send_ttl:%u group_delay_min:%u group_delay_max:%u boot_interval:%d per_interval:%u adv_duration:%u", nps_config.cr_enable, nps_config.send_ttl,
                       nps_config.group_delay_min, nps_config.group_delay_max, nps_config.boot_interval, nps_config.per_interval, nps_config.adv_duration);

        genie_nps_config_cr_enable_set(nps_config.cr_enable);
        genie_nps_config_send_ttl_set(nps_config.send_ttl);
        genie_nps_config_group_delay_min_set(nps_config.group_delay_min);
        genie_nps_config_group_delay_max_set(nps_config.group_delay_max);
        genie_nps_config_boot_interval_set(nps_config.boot_interval);
        genie_nps_config_per_interval_set(nps_config.per_interval);
        genie_nps_config_adv_duration_set(nps_config.adv_duration);
    }
    else
    {
        return -EBADMSG;
    }

    return 0;
}
