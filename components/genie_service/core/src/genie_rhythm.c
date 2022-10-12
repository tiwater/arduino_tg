/*
 * Copyright (C) 2018-2021 Alibaba Group Holding Limited
 */

#define BT_DBG_ENABLED IS_ENABLED(CONFIG_BT_MESH_DEBUG_ADV)
#include "genie_mesh_internal.h"

static uint8_t rhythm_recv_stat = 0;
static aos_timer_t rhythm_recv_timer;
static uint16_t last_rhythm_cmd_id = GENIE_RHYTHM_INVALID_ID;
static uint16_t last_rhythm_data_id = GENIE_RHYTHM_INVALID_ID;

#if 0
static void _rhythm_recv_scan_cb(const struct bt_le_scan_recv_info *info,
                                 struct net_buf_simple *buf)
{
    u8_t adv_type;

    adv_type = info->adv_type;
    if (adv_type != BT_LE_ADV_NONCONN_IND)
    {
        return;
    }

    // GENIE_LOG_DBG("len %u: %s", buf->len, bt_hex(buf->data, buf->len));
    while (buf->len > 1)
    {
        struct net_buf_simple_state state;
        u8_t len = 0, type = 0;

        len = net_buf_simple_pull_u8(buf);
        if (len == 0)
        {
            return;
        }

        if (len > buf->len)
        {
            // GENIE_LOG_WARN("AD malformed");
            return;
        }

        net_buf_simple_save(buf, &state);
        type = net_buf_simple_pull_u8(buf);

        buf->len = len - 1;
        if (adv_type == BT_LE_ADV_NONCONN_IND)
        {
            switch (type)
            {
            case BT_DATA_MESH_MESSAGE:
                bt_mesh_net_recv(buf, info->rssi, BT_MESH_NET_IF_ADV);
                break;

            default:
                break;
            }
        }
        net_buf_simple_restore(buf, &state);
        net_buf_simple_pull(buf, len);
    }
}

static struct bt_le_scan_cb genie_rhythm_recv_scan_cb = {
    .recv = _rhythm_recv_scan_cb,
};
#endif

static void _rhythm_recv_start(void)
{
    if (rhythm_recv_stat == 0)
    {
        rhythm_recv_stat = 1;
        bt_mesh_adv_disable();
        dimmer_init();
        aos_timer_start(&rhythm_recv_timer);
    }
}

static void _rhythm_recv_stop(void)
{
    if (rhythm_recv_stat == 1)
    {
        rhythm_recv_stat = 0;
        dimmer_deinit();
        aos_timer_stop(&rhythm_recv_timer);
    }
}

static void _rhythm_recv_timer_cb(void *p_timer, void *args)
{
    static uint16_t save_data_id = GENIE_RHYTHM_INVALID_ID;

    if ((last_rhythm_data_id == save_data_id) && (save_data_id != GENIE_RHYTHM_INVALID_ID))
    {
        _rhythm_recv_stop();
        save_data_id = 0;
    }
    else
    {
        save_data_id = last_rhythm_data_id;
        aos_timer_stop(&rhythm_recv_timer);
        aos_timer_start(&rhythm_recv_timer);
    }
}

static void _rhythm_recv_data(struct bt_mesh_net_rx *rx, struct net_buf_simple *buf)
{
    genie_rhythm_data_t rhythm_data;
    uint32_t diff_ms = 0;
    static uint32_t last_time_ms = 0;
    static uint32_t cur_time_ms = 0;

    // printf("[%u]src 0x%04x dst 0x%04x recv_ttl %u rssi %d len:%u data:%s\n", k_uptime_get_32(), rx->ctx.addr,
    //    rx->ctx.recv_dst, rx->ctx.recv_ttl, rx->rssi, buf->len, bt_hex(buf->data, buf->len));

    if (buf->len < 7)
    {
        GENIE_LOG_ERR("invalid data, len:%u, data:%s", buf->len, bt_hex(buf->data, buf->len));
        return;
    }
    rhythm_data.id = net_buf_simple_pull_le16(buf);
    if (rhythm_data.id != last_rhythm_data_id)
    {
        _rhythm_recv_start();

        rhythm_data.red = net_buf_simple_pull_u8(buf);
        rhythm_data.green = net_buf_simple_pull_u8(buf);
        rhythm_data.blue = net_buf_simple_pull_u8(buf);
        rhythm_data.cold = net_buf_simple_pull_u8(buf);
        rhythm_data.warm = net_buf_simple_pull_u8(buf);
        dimmer_set_color(rhythm_data.red, rhythm_data.green, rhythm_data.blue, rhythm_data.cold, rhythm_data.warm);

        cur_time_ms = k_uptime_get_32();
        if (last_time_ms > 0)
        {
            diff_ms = cur_time_ms - last_time_ms;
        }
        else
        {
            diff_ms = 0;
        }
        last_time_ms = cur_time_ms;

        last_rhythm_data_id = rhythm_data.id;
        printf("[%u]id:%d, value:(%d,%d,%d), diff:%u\n", cur_time_ms, rhythm_data.id, rhythm_data.red, rhythm_data.green, rhythm_data.blue, diff_ms);
    }
}

static void _rhythm_recv_cmd(struct bt_mesh_net_rx *rx, struct net_buf_simple *buf)
{
    genie_rhythm_cmd_t rhythm_cmd;

    // printf("[%u]src 0x%04x dst 0x%04x recv_ttl %u rssi %d len:%u data:%s\n", k_uptime_get_32(), rx->ctx.addr,
    //    rx->ctx.recv_dst, rx->ctx.recv_ttl, rx->rssi, buf->len, bt_hex(buf->data, buf->len));

    if (buf->len < 3)
    {
        GENIE_LOG_ERR("invalid cmd, len:%u, data:%s", buf->len, bt_hex(buf->data, buf->len));
        return;
    }
    rhythm_cmd.id = net_buf_simple_pull_le16(buf);
    if (rhythm_cmd.id != last_rhythm_cmd_id)
    {
        rhythm_cmd.cmd = net_buf_simple_pull_u8(buf);
        if (rhythm_cmd.cmd == GENIE_RHYTHM_CMD_START)
        {
            _rhythm_recv_start();
        }
        else if (rhythm_cmd.cmd == GENIE_RHYTHM_CMD_STOP)
        {
            _rhythm_recv_stop();
        }

        last_rhythm_cmd_id = rhythm_cmd.id;
        printf("[%u]id:%d, cmd:%d\n", k_uptime_get_32(), rhythm_cmd.id, rhythm_cmd.cmd);
    }
}

int genie_rhythm_recv_msg(u8_t ctl_op, struct bt_mesh_net_rx *rx, struct net_buf_simple *buf)
{
    if (!rx->local_match)
    {
        GENIE_LOG_ERR("Dropping packet of no local match");
        return 0;
    }

    if (bt_mesh_elem_find(rx->ctx.addr))
    {
        GENIE_LOG_ERR("Dropping self packet");
        return -EBADMSG;
    }

    if (buf->len < 1)
    {
        GENIE_LOG_ERR("Too short message");
        return -EINVAL;
    }

    if (ctl_op == TRANS_CTL_OP_RHYTHM_DATA)
    {
        _rhythm_recv_data(rx, buf);
    }
    else if (ctl_op == TRANS_CTL_OP_RHYTHM_CMD)
    {
        _rhythm_recv_cmd(rx, buf);
    }
    else
    {
        GENIE_LOG_ERR("Dropping unknown opcode:%u packet", ctl_op);
        return -EBADMSG;
    }

    return 0;
}

uint8_t genie_rhythm_recv_stat(void)
{
    return rhythm_recv_stat;
}

void genie_rhythm_recv_init(void)
{
    printf("genie_rhythm_recv_init\n");
    // bt_le_scan_cb_register(&genie_rhythm_recv_scan_cb);

    aos_timer_new(&rhythm_recv_timer, _rhythm_recv_timer_cb, NULL, 30000, 0);
    aos_timer_stop(&rhythm_recv_timer);
    rhythm_recv_stat = 0;
}
