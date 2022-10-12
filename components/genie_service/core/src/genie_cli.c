/*
 * Copyright (C) 2018-2020 Alibaba Group Holding Limited
 */

#include "genie_mesh_internal.h"

extern uint8_t g_mesh_log_mode;

static bool send_mesg_working = false;
static uint32_t send_mesg_total_cnt = 0;
static uint32_t send_mesg_timeout_cnt = 0;
static aos_timer_t mesg_send_timer;

#define MAC_LEN (6)
#define TIMER_SEND_MODE (255)
#define TIMEOUT_SEND_MODE (254)
#define USER_TIMER_SEND_MODE (253)
#ifndef SYSINFO_PRODUCT_MODEL
#define SYSINFO_PRODUCT_MODEL "GENIE_IOT_DEVICE"
#endif

static void _get_triple(char *pwbuf, int blen, int argc, char **argv)
{
    uint8_t i;
    genie_triple_t genie_triple;
    genie_storage_status_e ret;

    memset(&genie_triple, 0, sizeof(genie_triple_t));

    ret = genie_triple_read(&genie_triple.pid, genie_triple.mac, genie_triple.key);
    if (ret != GENIE_STORAGE_SUCCESS)
    {
        GENIE_LOG_ERR("read triple fail(%d)", ret);
        return;
    }

    printf("%d ", (unsigned int)genie_triple.pid);

    for (i = 0; i < 16; i++)
    {
        printf("%02x", genie_triple.key[i]);
    }

    printf(" ");
    for (i = 0; i < 6; i++)
    {
        printf("%02x", genie_triple.mac[i]);
    }
    printf("\n");
}

static int genie_cli_set_triple(char *pwbuf, int blen, int argc, char **argv)
{
    uint32_t pid;
    uint8_t mac[MAC_LEN], read_mac[MAC_LEN];
    uint8_t key[16];
    uint8_t ret;

    if (argc != 4)
    {
        GENIE_LOG_ERR("para err");
        return -1;
    }

    //pro_id
    pid = atol(argv[1]);
    if (pid == 0)
    {
        GENIE_LOG_ERR("pid err");
        return -1;
    }
    //key
    if (strlen(argv[2]) != 32)
    {
        GENIE_LOG_ERR("key len err");
        return -1;
    }
    ret = stringtohex(argv[2], key, 16);
    if (ret == 0)
    {
        GENIE_LOG_ERR("key format err");
        return -1;
    }

    //addr
    if (strlen(argv[3]) != 12)
    {
        GENIE_LOG_ERR("mac len err");
        return -1;
    }
    ret = stringtohex(argv[3], mac, MAC_LEN);
    if (ret == 0)
    {
        GENIE_LOG_ERR("mac format err");
        return -1;
    }

    hal_flash_write_mac_params(mac, MAC_LEN);
    memset(read_mac, 0xFF, MAC_LEN);
    hal_flash_read_mac_params(read_mac, MAC_LEN);
    if (memcmp(mac, read_mac, MAC_LEN))
    {
        GENIE_LOG_ERR("write mac fail");
        return -1;
    }

    ret = genie_triple_write(&pid, mac, key);

    return ret;
}

static void _set_triple(char *pwbuf, int blen, int argc, char **argv)
{
    genie_cli_set_triple(pwbuf, blen, argc, argv);
    _get_triple(pwbuf, blen, argc, argv);
}

#ifdef CONFIG_GENIE_MESH_PORT_YOC
static void xtalcap_handle(char *pwbuf, int blen, int argc, char **argv)
{
    int xtalcap = 0;

    if (!strncmp(argv[1], "set", 3))
    {
        xtalcap = atoi((char *)(argv[2]));
        if (xtalcap < 0 || xtalcap > 64)
        {
            GENIE_LOG_ERR("input int value[0,64]");
            return;
        }

        if (hal_flash_write_xtalcap_params(&xtalcap, sizeof(xtalcap)) != 0)
        {
            GENIE_LOG_ERR("write freq failed");
            return;
        }
    }
    else if (!strncmp(argv[1], "get", 3))
    {
        hal_flash_read_xtalcap_params(&xtalcap, sizeof(xtalcap));
        GENIE_LOG_INFO("xtalcap:%d", xtalcap);
    }
}
#else
static void freq_offset_handle(char *pwbuf, int blen, int argc, char **argv)
{
    int freq_offset = 0;

    if (!strncmp(argv[1], "set", 3))
    {
        freq_offset = atoi((char *)(argv[2]));
        if (freq_offset == 0 && strncmp(argv[2], "0", 1))
        {
            GENIE_LOG_ERR("input int value[-200,200]");
            return;
        }

        if (freq_offset < -200 || freq_offset > 200)
        {
            GENIE_LOG_ERR("input range [-200,200]");
            return;
        }

        //Convert for match TG7100B
        if (freq_offset % 10 == freq_offset % 20)
        {
            freq_offset = freq_offset - freq_offset % 10;
        }
        else
        {
            freq_offset = freq_offset - freq_offset % 10;
            if (freq_offset < 0)
            {
                freq_offset -= 10;
            }
            else
            {
                freq_offset += 10;
            }
        }

        freq_offset >>= 2;

        if (hal_flash_write_freq_params(&freq_offset, sizeof(freq_offset)) != 0)
        {
            GENIE_LOG_ERR("write freq failed");
            return;
        }
    }
    else if (!strncmp(argv[1], "get", 3))
    {
        hal_flash_read_freq_params(&freq_offset, sizeof(freq_offset));
        if ((freq_offset & 0xFF) == 0xFF)
        {
            GENIE_LOG_INFO("default freq offset:%d", -80); //RF_PHY_FREQ_FOFF_N80KHZ
        }
        else
        {
            GENIE_LOG_INFO("freq offset:%d", freq_offset * 4);
        }
    }
}
#endif

#ifdef SUPPORT_SN_NUMBER
static void sn_number_handle(char *pwbuf, int blen, int argc, char **argv)
{
    int32_t ret = 0;
    uint8_t count = 0;
    uint8_t read_sn[24];

    if (!strncmp(argv[1], "set", 3))
    {
        if (argc != 3)
        {
            GENIE_LOG_ERR("param err");
            return;
        }
        count = strlen(argv[2]);
        if (count != CUSTOM_SN_LEN)
        {
            GENIE_LOG_ERR("sn len error");
            return;
        }
        ret = hal_flash_write_sn_params(argv[2], count);
        if (ret != 0)
        {
            GENIE_LOG_ERR("write sn failed(%d)", ret);
            return;
        }
        memset(read_sn, 0, sizeof(read_sn));
        hal_flash_read_sn_params(read_sn, count);
        if (memcmp(argv[2], read_sn, count))
        {
            GENIE_LOG_ERR("write sn failed");
            return;
        }
    }
    else if (!strncmp(argv[1], "get", 3))
    {
        memset(read_sn, 0, sizeof(read_sn));
        ret = hal_flash_read_sn_params(read_sn, CUSTOM_SN_LEN);
        if (ret != 0)
        {
            GENIE_LOG_ERR("read sn failed(%d)", ret);
            return;
        }
        if ((read_sn[0] & 0xFF) == 0xFF)
        {
            GENIE_LOG_INFO("SN:");
            return;
        }
        GENIE_LOG_INFO("SN:%s", read_sn);
    }
}
#endif

static void _reboot_handle(char *pwbuf, int blen, int argc, char **argv)
{
    aos_reboot();
}

static void genie_cli_print_sysinfo(void)
{
    uint8_t write_mac[MAC_LEN];
    char appver[16];

    genie_version_appver_string_get(appver, 16);

    printf("\r\nDEVICE:%s\r\n", CONFIG_BT_DEVICE_NAME);
    printf("APP VER:%s\r\n", appver);
    printf("GenieSDK:V%s\r\n", genie_version_sysver_get());
    printf("PROUDUCT:%s\r\n", SYSINFO_PRODUCT_MODEL);
    memset(write_mac, 0xFF, MAC_LEN);
    hal_flash_read_mac_params(write_mac, MAC_LEN);
    printf("MAC:%02X:%02X:%02X:%02X:%02X:%02X\r\n", write_mac[0], write_mac[1], write_mac[2], write_mac[3], write_mac[4], write_mac[5]);
}

static void _get_sw_info(char *pwbuf, int blen, int argc, char **argv)
{
    genie_cli_print_sysinfo();
}

static void _get_mm_info(char *pwbuf, int blen, int argc, char **argv)
{
#if RHINO_CONFIG_MM_DEBUG
#ifdef CONFIG_GENIE_MESH_PORT_YOC
    int total = 0, used = 0, mfree = 0, peak = 0;
    aos_get_mminfo(&total, &used, &mfree, &peak);
    printf("                   total      used      free      peak \r\n");
    printf("memory usage: %10d%10d%10d%10d\r\n\r\n",
           total, used, mfree, peak);
#else
    extern uint32_t dump_mm_info_used(void);
    dump_mm_info_used();
#endif
#endif
}

static uint8_t retry_mode = 0;
static uint8_t *p_payload = NULL;
static genie_transport_payload_param_t *p_transport_payload_param = NULL;

static void mesg_send_timer_cb(void *time, void *args)
{
    if (p_transport_payload_param != NULL && send_mesg_working == true)
    {
        if (0 == genie_transport_send_payload(p_transport_payload_param))
        {
            send_mesg_total_cnt++;
        }
    }
}

static int send_mesg_cb(void *p_params, transport_result_e result_e)
{
    if (retry_mode == TIMEOUT_SEND_MODE) //This is timeout send mode
    {
        if (mesg_send_timer.hdl == NULL)
        {
            aos_timer_new(&mesg_send_timer, mesg_send_timer_cb, p_transport_payload_param, 10, 0);
        }
        aos_timer_stop(&mesg_send_timer);
        aos_timer_start(&mesg_send_timer);

        if (result_e == SEND_RESULT_TIMEOUT)
        {
            send_mesg_timeout_cnt++;
            GENIE_LOG_INFO("timeout");
        }
    }

    GENIE_LOG_INFO("total cnt:%ld timeout cnt:%ld", send_mesg_total_cnt, send_mesg_timeout_cnt);

    return 0;
}

static void _send_msg(char *pwbuf, int blen, int argc, char **argv)
{
    uint8_t count;
    uint8_t ret = 0;
    uint8_t opid = 0;

    if (argc == 2 && !strncmp(argv[1], "stop", 4))
    {
        send_mesg_working = false;
        send_mesg_timeout_cnt = 0;
        send_mesg_total_cnt = 0;
        GENIE_LOG_INFO("send stop");

        aos_timer_stop(&mesg_send_timer);
        aos_timer_free(&mesg_send_timer);

        if (p_payload)
        {
            hal_free(p_payload);
            p_payload = NULL;
        }

        if (p_transport_payload_param)
        {
            hal_free(p_transport_payload_param);
            p_transport_payload_param = NULL;
        }

        return;
    }

    if (argc != 5)
    {
        GENIE_LOG_ERR("param err");
        return;
    }

    if (strlen(argv[3]) != 4)
    {
        GENIE_LOG_ERR("addr len error");
        return;
    }

    if (stringtohex(argv[1], &opid, 1) > 0)
    {
        retry_mode = atoi(argv[2]);
        count = strlen(argv[4]) >> 1;

        if (p_payload == NULL)
        {
            p_payload = hal_malloc(count);
            if (p_payload == NULL)
            {
                GENIE_LOG_ERR("malloc(%d) fail", count);
                return;
            }
        }
        else //remalloc
        {
            hal_free(p_payload);
            p_payload = NULL;
            p_payload = hal_malloc(count);
            if (p_payload == NULL)
            {
                GENIE_LOG_ERR("malloc(%d) fail", count);
                return;
            }
        }

        ret = stringtohex(argv[4], p_payload, count);
        if (ret == 0)
        {
            hal_free(p_payload);
            p_payload = NULL;
            return;
        }

        if (p_transport_payload_param == NULL)
        {
            p_transport_payload_param = hal_malloc(sizeof(genie_transport_payload_param_t));
            if (p_transport_payload_param == NULL)
            {
                hal_free(p_payload);
                p_payload = NULL;
                GENIE_LOG_ERR("malloc(%d) fail", sizeof(genie_transport_payload_param_t));
                return;
            }
        }

        memset(p_transport_payload_param, 0, sizeof(genie_transport_payload_param_t));
        p_transport_payload_param->opid = opid;
        p_transport_payload_param->p_payload = p_payload;
        p_transport_payload_param->payload_len = count;
        p_transport_payload_param->retry_cnt = retry_mode;
        p_transport_payload_param->result_cb = send_mesg_cb;

        ret = stringtohex(argv[3], (uint8_t *)&p_transport_payload_param->dst_addr, 2);
        if (ret == 0)
        {
            hal_free(p_payload);
            p_payload = NULL;
            hal_free(p_transport_payload_param);
            p_transport_payload_param = NULL;
            return;
        }

        //swap addr
        uint8_t upper = p_transport_payload_param->dst_addr >> 8;
        p_transport_payload_param->dst_addr = upper | (p_transport_payload_param->dst_addr << 8);

        if (retry_mode == TIMER_SEND_MODE) //This is auto send mode
        {
            send_mesg_working = true;

            p_transport_payload_param->retry_cnt = 0;
            if (mesg_send_timer.hdl == NULL)
            {
                aos_timer_new(&mesg_send_timer, mesg_send_timer_cb, p_transport_payload_param, 1000, 1);
            }
            aos_timer_stop(&mesg_send_timer);
            aos_timer_start(&mesg_send_timer);
            return;
        }
        else if (retry_mode == USER_TIMER_SEND_MODE && p_transport_payload_param->payload_len > 1) //This is auto send mode
        {
            send_mesg_working = true;

            p_transport_payload_param->retry_cnt = 0;
            if (mesg_send_timer.hdl == NULL)
            {
                int interval = p_transport_payload_param->p_payload[0];
                if (interval == 0)
                {
                    interval = 1;
                }

                interval = interval * 100; //Unit is 100ms
                //skip p_transport_payload_param->p_payload[0]
                p_transport_payload_param->p_payload = &p_transport_payload_param->p_payload[1];
                p_transport_payload_param->payload_len -= 1;
                aos_timer_new(&mesg_send_timer, mesg_send_timer_cb, p_transport_payload_param, interval, 1);
            }
            aos_timer_stop(&mesg_send_timer);
            aos_timer_start(&mesg_send_timer);
            return;
        }
        else if (retry_mode == TIMEOUT_SEND_MODE) //This is timeout send mode
        {
            send_mesg_working = true;
            p_transport_payload_param->retry_cnt = 0;
            genie_transport_send_payload(p_transport_payload_param);
            return;
        }

        genie_transport_send_payload(p_transport_payload_param);

        if (p_payload)
        {
            hal_free(p_payload);
            p_payload = NULL;
        }

        if (p_transport_payload_param)
        {
            hal_free(p_transport_payload_param);
            p_transport_payload_param = NULL;
        }
    }
}

#ifdef CONFIG_BT_MESH_NPS_OPT
static void _group_send_msg(char *pwbuf, int blen, int argc, char **argv)
{
    uint8_t count;
    uint8_t ret = 0;
    uint8_t tid;
    uint8_t dst_addr[2];
    uint16_t opcode;
    uint8_t *data = NULL;
    struct bt_mesh_model *p_model = NULL;
    struct net_buf_simple *msg = NULL;
    struct bt_mesh_msg_ctx ctx;
    struct bt_mesh_elem *p_primary_elem = NULL;
    static uint8_t last_onoff = 0xff;
    static uint32_t last_time_ms = 0;
    uint32_t cur_time_ms, diff_ms;

    if (argc != 3)
    {
        GENIE_LOG_ERR("param err");
        return;
    }

    if (strlen(argv[1]) != 4)
    {
        GENIE_LOG_ERR("addr len error");
        return;
    }

    ret = stringtohex(argv[1], dst_addr, 2);
    if (ret == 0)
    {
        GENIE_LOG_ERR("addr param error");
        return;
    }

    count = strlen(argv[2]) >> 1;
    if (count < 3)
    {
        GENIE_LOG_ERR("data len err");
        return;
    }

    data = hal_malloc(count);
    if (data == NULL)
    {
        GENIE_LOG_ERR("malloc(%d) failed", count);
        return;
    }

    ret = stringtohex(argv[2], data, count);
    if (ret == 0)
    {
        hal_free(data);
        data = NULL;
        GENIE_LOG_ERR("data param err");
        return;
    }

    tid = genie_transport_gen_tid();
    p_primary_elem = genie_mesh_get_primary_element();
    opcode = (data[0] << 8) + data[1];
    if ((opcode == OP_GENERIC_ONOFF_SET) || (opcode == OP_GENERIC_ONOFF_SET_UNACK))
    {
        p_model = bt_mesh_model_find(p_primary_elem, BT_MESH_MODEL_ID_GEN_ONOFF_SRV);
        if (count > 3)
        {
            data[3] = tid;
        }
        if (last_onoff != 0xff)
        {
            data[2] = (last_onoff == 0) ? 1 : 0;
        }
        last_onoff = data[2];
    }
    else if ((opcode == OP_GENERIC_LIGHTNESS_SET) || (opcode == OP_GENERIC_LIGHTNESS_SET_UNACK))
    {
        p_model = bt_mesh_model_find(p_primary_elem, BT_MESH_MODEL_ID_LIGHT_LIGHTNESS_SRV);
        if (count > 4)
        {
            data[4] = tid;
        }
    }
    else if ((opcode == OP_GENERIC_LIGHT_CTL_SET) || (opcode == OP_GENERIC_LIGHT_CTL_SET_UNACK))
    {
        p_model = bt_mesh_model_find(p_primary_elem, BT_MESH_MODEL_ID_LIGHT_CTL_SRV);
        if (count > 8)
        {
            data[8] = tid;
        }
    }
    else if ((opcode == OP_GENERIC_SCENE_RECALL) || (opcode == OP_GENERIC_SCENE_RECALL_UNACK))
    {
        p_model = bt_mesh_model_find(p_primary_elem, BT_MESH_MODEL_ID_SCENE_SRV);
        if (count > 4)
        {
            data[4] = tid;
        }
    }
    else
    {
        p_model = p_primary_elem->models;
    }

    if (p_model == NULL)
    {
        hal_free(data);
        data = NULL;
        GENIE_LOG_ERR("not find model");
        return;
    }

    memset(&ctx, 0, sizeof(struct bt_mesh_msg_ctx));
    ctx.app_idx = 0;
    ctx.net_idx = 0;
    ctx.addr = (dst_addr[0] << 8) + dst_addr[1];
    ctx.send_ttl = GENIE_TRANSPORT_DEFAULT_TTL;
    ctx.send_rel = 0;

    msg = NET_BUF_SIMPLE(GENIE_MODEL_MTU + 4);
    if (msg == NULL)
    {
        hal_free(data);
        data = NULL;
        GENIE_LOG_ERR("get msg net_buf failed");
        return;
    }

    net_buf_simple_init(msg, 0);
    net_buf_simple_add_mem(msg, data, count);

    cur_time_ms = k_uptime_get_32();
    if ((last_time_ms > 0) && (cur_time_ms > last_time_ms))
    {
        diff_ms = cur_time_ms - last_time_ms;
    }
    else
    {
        diff_ms = 0;
    }
    last_time_ms = cur_time_ms;

    printf("[%u]gmesg, diff_ms:%u, len:%u, data:%s\n", cur_time_ms, diff_ms, msg->len, bt_hex(msg->data, msg->len));
    if (bt_mesh_model_send(p_model, &ctx, msg, NULL, NULL))
    {
        GENIE_LOG_ERR("mesh model send fail");
    }

    hal_free(data);
    data = NULL;

    return;
}
#endif

static void genie_system_reset(char *pwbuf, int blen, int argc, char **argv)
{
    genie_event(GENIE_EVT_HW_RESET_START, NULL);
}

#if 0
static void genie_log_onoff(char *pwbuf, int blen, int argc, char **argv)
{
    if (argc != 2)
    {
        GENIE_LOG_ERR("para err");
        return;
    }

    if (!strncmp(argv[1], "on", 2))
    {
        g_mesh_log_mode = 1;
    }
    else if (!strncmp(argv[1], "off", 3))
    {
        g_mesh_log_mode = 0;
    }
    else
    {
        GENIE_LOG_ERR("para err");
        return;
    }

    return;
}
#endif

#ifdef CONFIG_GENIE_RHYTHM
extern int dimmer_set_color(uint8_t red, uint8_t green, uint8_t blue, uint8_t cold, uint8_t warm);
static void rhythm_rgb_test_handle(char *pwbuf, int blen, int argc, char **argv)
{
    uint8_t red, green, blue;

    if (argc != 4)
    {
        GENIE_LOG_ERR("para err");
        return;
    }

    red = atoi((char *)(argv[1]));
    green = atoi((char *)(argv[2]));

    blue = atoi((char *)(argv[3]));

    GENIE_LOG_INFO("rgb:(%d,%d,%d)", red, green, blue);
    dimmer_set_color(red, green, blue, 0, 0);
}
#endif

/*[Genie begin] add by wenbing.cwb at 2021-04-29*/
#ifdef CONFIG_BT_MESH_CTRL_RELAY
static void genie_cr_log_onoff(char *pwbuf, int blen, int argc, char **argv)
{
    if (argc != 2)
    {
        GENIE_LOG_ERR("para err");
        return;
    }

    if (!strncmp(argv[1], "on", 2))
    {
        ctrl_relay_log_set(CTRL_RELAY_LOG_ON);
    }
    else if (!strncmp(argv[1], "off", 3))
    {
        ctrl_relay_log_set(CTRL_RELAY_LOG_OFF);
    }
    else
    {
        GENIE_LOG_ERR("para err");
    }
    return;
}
#endif
/*[Genie end] add by wenbing.cwb at 2021-04-29*/

#ifdef CONFIG_GENIE_MESH_SCENE_SHARE
void genie_scene_share_send(char *pwbuf, int blen, int argc, char **argv)
{
    int err = 0;
    uint8_t tid;
    uint8_t *data = NULL;
    uint8_t count = 0;
    uint8_t ret = 0;
    struct net_buf_simple *pmsg = NULL;
    char mac_str[13];

    memset(mac_str, 0, sizeof(mac_str));
    if (argc == 3)
    {
        strncpy(mac_str, argv[2], 12);
    }

    count = strlen(argv[1]) >> 1;
    if (count < 5)
    {
        GENIE_LOG_ERR("data param err");
        return;
    }

    data = hal_malloc(count);
    if (data == NULL)
    {
        GENIE_LOG_ERR("malloc(%d) failed", count);
        return;
    }

    ret = stringtohex(argv[1], data, count);
    if (ret == 0)
    {
        hal_free(data);
        data = NULL;
        return;
    }

    tid = genie_transport_gen_tid();
    data[4] = tid;

    GENIE_LOG_INFO("[%u]scene_share send, len:%u raw_data:%s", k_uptime_get_32(), count, bt_hex(data, count));
    pmsg = NET_BUF_SIMPLE(CONFIG_BT_MESH_RX_SDU_MAX - 4);
    if (pmsg == NULL)
    {
        GENIE_LOG_ERR("get net_buf failed");
        hal_free(data);
        data = NULL;
        return;
    }
    net_buf_simple_init(pmsg, 0);
    net_buf_simple_add_mem(pmsg, data, count);

    if (strlen(mac_str) > 0)
    {
        err = genie_scene_share_msg_encrypt(mac_str, pmsg);
    }
    else
    {
        err = genie_scene_share_msg_encrypt(NULL, pmsg);
    }

    if (err != 0)
    {
        GENIE_LOG_ERR("data encrypt, err:%d", err);
        hal_free(data);
        data = NULL;
        return;
    }

    // GENIE_LOG_INFO("[%u]scene_share send, len:%u en_data:%s", k_uptime_get_32(), pmsg->len, bt_hex(pmsg->data, pmsg->len));
    err = bt_mesh_ctl_send_ext(TRANS_CTL_OP_SCENE_SHARE, BT_MESH_ADDR_GENIE_ALL_NODES, 0, pmsg->data, pmsg->len, NULL, NULL);
    if (err != 0)
    {
        GENIE_LOG_ERR("ctl send ext failed, err:%d", err);
    }

    hal_free(data);
    data = NULL;
    return;
}
#endif

static const struct cli_command genie_cmds[] = {
    {"get_tt", "get tri truple", _get_triple},
    {"set_tt", "set_tt pid key mac", _set_triple},
#ifdef CONFIG_GENIE_MESH_PORT_YOC
    {"xtalcap", "xtalcap set 16|xtalcap get", xtalcap_handle},
#else
    {"freq", "freq set -80|freq get", freq_offset_handle},
#endif
#ifdef SUPPORT_SN_NUMBER
    {"sn", "sn set <value>|sn get", sn_number_handle},
#endif
    {"reboot", "reboot", _reboot_handle},
    {"reset", "reset system", genie_system_reset},
#if 0
    {"log", "log on|off", genie_log_onoff},
#endif
/*[Genie begin] add by wenbing.cwb at 2021-04-29*/
#ifdef CONFIG_BT_MESH_CTRL_RELAY
    {"cr_log", "cr_log on|off", genie_cr_log_onoff},
#endif
    /*[Genie end] add by wenbing.cwb at 2021-04-29*/
    {"get_info", "get sw info", _get_sw_info},
    {"mm_info", "get mm info", _get_mm_info},
    {"mesg", "mesg d4 1 f000 010203", _send_msg},
#ifdef CONFIG_GENIE_RHYTHM
    {"rgb_test", "rgb_test r g b", rhythm_rgb_test_handle},
#endif
#ifdef CONFIG_BT_MESH_NPS_OPT
    {"gmesg", "gmesg c000 820201064100", _group_send_msg},
#endif
#ifdef CONFIG_GENIE_MESH_SCENE_SHARE
    /* opcode:0x8243 scene_number:0x2711 tid:0x80 trans_time:0x41 delay:0x00 mac */
    //{"scene_share", "scene_share 82431127804100 66e22b26b505", genie_scene_share_send},
#endif
};

void genie_cli_init(void)
{
#ifdef CONFIG_AOS_CLI
    aos_cli_register_commands(&genie_cmds[0], sizeof(genie_cmds) / sizeof(genie_cmds[0]));
#endif

    genie_cli_print_sysinfo();
}
