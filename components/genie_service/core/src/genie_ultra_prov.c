/*
 * Copyright (C) 2018-2021 Alibaba Group Holding Limited
 */

#define BT_DBG_ENABLED IS_ENABLED(CONFIG_BT_MESH_DEBUG_PROV)
#include "genie_mesh_internal.h"

#define BUF_TIMEOUT K_MSEC(400)
#define BT_MESH_ADV(buf) (*(struct bt_mesh_adv **)net_buf_user_data(buf))

static genie_ultra_prov_cache_t prov_cache[ULTRA_PROV_CACHE_SIZE];
static genie_ultra_prov_ctx_t genie_ultra_prov_ctx;

static int ultra_prov_send_comfirm_device(void);

static int genie_ultra_prov_send(uint8_t type, uint8_t *p_msg, uint8_t len)
{
    struct net_buf *buf;

#ifdef CONFIG_GENIE_MESH_PORT_YOC
    buf = bt_mesh_adv_create(0, 0, BUF_TIMEOUT);
#else
    buf = bt_mesh_adv_create(0, 0, 0, BUF_TIMEOUT);
#endif
    if (!buf)
    {
        GENIE_LOG_ERR("Out of provisioning buffers");
        return -ENOBUFS;
    }

    BT_MESH_ADV(buf)->tiny_adv = 1;

    net_buf_add_be16(buf, CONFIG_MESH_VENDOR_COMPANY_ID);
    //VID
    net_buf_add_u8(buf, GENIE_ULTRA_PROV_FIXED_BYTE);
    net_buf_add_u8(buf, type);
    net_buf_add_mem(buf, p_msg, len);

    GENIE_LOG_INFO("ultra prov send:%s", bt_hex(buf->data, buf->len));

    bt_mesh_adv_send(buf, NULL, NULL);
    net_buf_unref(buf);

    return 0;
}

int genie_ultra_prov_done(uint8_t reason)
{
    uint8_t reason_pdu[3];
    genie_triple_t *p_genie_triple = genie_triple_get();

    if (reason > 0)
    {
        GENIE_LOG_ERR("ultra prov fail reason:%d", reason);
        reason_pdu[0] = p_genie_triple->mac[4];
        reason_pdu[1] = p_genie_triple->mac[5];
        reason_pdu[2] = reason;
        genie_ultra_prov_send(GENIE_ULTRA_PROV_TYPE_PROV_FAILED, reason_pdu, 3); //notify provisioner prov failed
    }

    genie_ultra_prov_deinit();

    return 0;
}

#ifdef ULTRA_PROV_RESEND_ENABLE
static void ultra_prov_timeout(struct k_work *work)
{
    GENIE_LOG_WARN("ultra prov timeout,expect:%d", genie_ultra_prov_ctx.expect);
    switch (genie_ultra_prov_ctx.expect)
    {
    case STATE_EXPECT_PROV_DATA:
    {
        if (++genie_ultra_prov_ctx.attempts < RESEND_DATA_TO_PROVISIONER_TIMES)
        {
            ultra_prov_send_comfirm_device(); //resend comfirm device to provisioner
            k_delayed_work_submit(&genie_ultra_prov_ctx.timer, RESEND_DATA_TO_PROVISIONER_TIMEOUT);
        }
        else
        {
            genie_ultra_prov_done(ULTRA_PROV_FAILED_WAIT_PROV_DATA_TIMEOUT);
        }
    }
    break;
    case STATE_EXPECT_PROV_COMPLETE_ACK:
    {
        if (++genie_ultra_prov_ctx.attempts < RESEND_DATA_TO_PROVISIONER_TIMES)
        {
            genie_triple_t *p_genie_triple = genie_triple_get();

            genie_ultra_prov_send(GENIE_ULTRA_PROV_TYPE_PROV_COMPLETE, p_genie_triple->mac, 6); //resend prov complete
            k_delayed_work_submit(&genie_ultra_prov_ctx.timer, RESEND_DATA_TO_PROVISIONER_TIMEOUT);
        }
        else
        {
            genie_ultra_prov_done(ULTRA_PROV_FAILED_WAIT_COMPLETE_ACK_TIMEOUT);
        }
    }
    break;
    default:
    {
        BT_ERR("unexpect state:%d", genie_ultra_prov_ctx.expect);
    }
    break;
    }
}
#endif

static int genie_ultra_prov_get_auth(const uint8_t random_hex[16], const uint8_t key[16], uint8_t cfm[16])
{
    int ret = -1;

    ret = bt_mesh_prov_conf(key, random_hex, genie_crypto_get_auth(random_hex), cfm);

    //GENIE_LOG_INFO("cfm: %s", bt_hex(cfm, 16));

    return ret;
}

static int genie_ultra_prov_sha256(uint8_t *source, uint32_t len, uint8_t digest[32])
{
    int ret = -1;
    struct tc_sha256_state_struct sha256_ctx;

    ret = tc_sha256_init(&sha256_ctx);
    if (ret != TC_CRYPTO_SUCCESS)
    {
        GENIE_LOG_ERR("sha256 init fail\n");
        return -1;
    }

    ret = tc_sha256_update(&sha256_ctx, source, len);
    if (ret != TC_CRYPTO_SUCCESS)
    {
        GENIE_LOG_ERR("sha256 udpate fail\n");
        return -1;
    }

    ret = tc_sha256_final(digest, &sha256_ctx);
    if (ret != TC_CRYPTO_SUCCESS)
    {
        GENIE_LOG_ERR("sha256 final fail\n");
        return -1;
    }

    return 0;
}

static int genie_ultra_generate_comfirm_key(uint8_t comfirm_key[16])
{
    uint8_t random_a_str[17];
    uint8_t random_b_str[17];
    uint8_t sha256_digest[32];
    uint8_t comfirmkey_source[48] = {0};

    hextostring(genie_ultra_prov_ctx.random_a, (char *)random_a_str, 8);
    random_a_str[16] = 0;
    hextostring(genie_ultra_prov_ctx.random_b, (char *)random_b_str, 8);
    random_b_str[16] = 0;

    sprintf((char *)comfirmkey_source, "%s%sConfirmationKey", random_a_str, random_b_str);

    //GENIE_LOG_INFO("device confirm source:%s\n", comfirmkey_source);

    if (0 != genie_ultra_prov_sha256(comfirmkey_source, 47, sha256_digest))
    {
        genie_ultra_prov_done(ULTRA_PROV_FAILED_SHA256);
        return -1;
    }

    memcpy(comfirm_key, sha256_digest, 16);

    return 0;
}

static int ultra_prov_send_comfirm_device(void)
{
    uint8_t random_device[16];
    uint8_t comfirm_key[16];
    uint8_t comfirm_device[16];
    uint8_t comfirm_device_pdu[18];
    genie_triple_t *p_genie_triple = genie_triple_get();

    memset(comfirm_device, 0, sizeof(comfirm_device));
    memcpy(random_device, genie_ultra_prov_ctx.random_a, 8);
    memcpy(random_device + 8, genie_ultra_prov_ctx.random_b, 8);

    if (genie_ultra_generate_comfirm_key(comfirm_key) != 0)
    {
        genie_ultra_prov_done(ULTRA_PROV_FAILED_SHA256);
        return -1;
    }

    //buf+2 = random_a || random_b
    genie_ultra_prov_get_auth(random_device, comfirm_key, comfirm_device);

    comfirm_device_pdu[0] = p_genie_triple->mac[4];
    comfirm_device_pdu[1] = p_genie_triple->mac[5];
    memcpy(comfirm_device_pdu + 2, comfirm_device, 16);

    return genie_ultra_prov_send(GENIE_ULTRA_PROV_TYPE_COMFIRM, comfirm_device_pdu, 18);
}

static void genie_ultra_prov_recv_random(uint8_t *buf)
{
    genie_triple_t *p_genie_triple = genie_triple_get();

    if (buf[0] != p_genie_triple->mac[4] || buf[1] != p_genie_triple->mac[5])
    {
        //GENIE_LOG_WARN("recv random mac not match");
        return;
    }

    if (genie_ultra_prov_ctx.expect == STATE_EXPECT_PROV_DATA)
    {
        //GENIE_LOG_INFO("duplicate recv random");
        return;
    }

    GENIE_LOG_INFO("[1-->2]ultra prov recv random,expect:%d", genie_ultra_prov_ctx.expect);
    genie_event(GENIE_EVT_SDK_MESH_PROV_START, NULL);

    memcpy(genie_ultra_prov_ctx.random_a, buf + 2, 8);
    memcpy(genie_ultra_prov_ctx.random_b, buf + 2 + 8, 8);

    ultra_prov_send_comfirm_device();
    genie_ultra_prov_ctx.expect = STATE_EXPECT_PROV_DATA;

#ifdef ULTRA_PROV_RESEND_ENABLE
    genie_ultra_prov_ctx.attempts = 0;
    k_delayed_work_submit(&genie_ultra_prov_ctx.timer, RESEND_DATA_TO_PROVISIONER_TIMEOUT);
#endif
}

static void genie_ultra_prov_recv_data(uint8_t *buf)
{
    uint8_t i = 0;
    uint16_t net_idx;
    uint8_t flags;
    uint32_t iv_index;
    uint16_t addr;
    uint8_t random_cloud[16] = {0};
    uint8_t comfirm_cloud_key[16] = {0};
    uint8_t comfirm_cloud[16] = {0};
    uint8_t device_key[16] = {0};
    uint8_t session_key[32] = {0};
    uint8_t session_source[43] = {0};
    genie_triple_t *p_genie_triple = genie_triple_get();

    if (genie_provision_get_state() != GENIE_PROVISION_START)
    {
        GENIE_LOG_WARN("I not in provisioning");
        return;
    }

    if (genie_ultra_prov_ctx.expect == STATE_EXPECT_PROV_COMPLETE_ACK)
    {
        GENIE_LOG_WARN("duplicate recv prov data");
        return;
    }

    GENIE_LOG_INFO("[2-->3]ultra prov recv prov data,expect:%d", genie_ultra_prov_ctx.expect);
    genie_ultra_generate_comfirm_key(comfirm_cloud_key);
    memcpy(random_cloud, genie_ultra_prov_ctx.random_b, 8);
    memcpy(random_cloud + 8, genie_ultra_prov_ctx.random_a, 8);
    genie_ultra_prov_get_auth(random_cloud, comfirm_cloud_key, comfirm_cloud);
    hextostring(comfirm_cloud, (char *)session_source, 16);
    //GENIE_LOG_INFO("comfirm_cloud:%s", bt_hex(comfirm_cloud, 16));
    sprintf((char *)session_source + 32, "SessionKey");

    if (0 != genie_ultra_prov_sha256(session_source, 42, session_key))
    {
        genie_ultra_prov_done(ULTRA_PROV_FAILED_SHA256);
        return;
    }
    //GENIE_LOG_INFO("session_key:%s", bt_hex(session_key, 22));
    for (i = 0; i < 22; i++)
    {
        buf[i] ^= session_key[i]; //decrypt prov data
    }

    //GENIE_LOG_INFO("prov data:%s", bt_hex(buf, 22));

    if (buf[0] != p_genie_triple->mac[4] || buf[1] != p_genie_triple->mac[5])
    {
        GENIE_LOG_WARN("prov data mac not match");
        return;
    }
    buf += 2;

    flags = buf[0];
    net_idx = 0;
    iv_index = buf[17];
    addr = sys_get_be16(&buf[18]);

    GENIE_LOG_INFO("[2-->3]prov data net_idx %u iv_index 0x%08x, addr 0x%04x", net_idx, iv_index, addr);

    genie_event(GENIE_EVT_SDK_MESH_PROV_DATA, &addr);
    GENIE_LOG_INFO("nk %s", bt_hex(buf + 1, 16));

    sprintf((char *)session_source + 32, "DeviceKey");

    if (0 != genie_ultra_prov_sha256(session_source, 41, session_key))
    {
        genie_ultra_prov_done(ULTRA_PROV_FAILED_SHA256);
        return;
    }

    memcpy(device_key, session_key, 16);

    genie_ultra_prov_send(GENIE_ULTRA_PROV_TYPE_PROV_COMPLETE, p_genie_triple->mac, 6);
    genie_ultra_prov_ctx.expect = STATE_EXPECT_PROV_COMPLETE_ACK;

#ifdef ULTRA_PROV_RESEND_ENABLE
    genie_ultra_prov_ctx.attempts = 0;
    k_delayed_work_submit(&genie_ultra_prov_ctx.timer, RESEND_DATA_TO_PROVISIONER_TIMEOUT);
#endif

#ifdef CONFIG_GENIE_MESH_PORT_YOC
    bt_mesh_provision(buf + 1, net_idx, flags, iv_index, addr, device_key);
#else
    bt_mesh_provision(buf + 1, net_idx, flags, iv_index, 0, addr, device_key);
#endif
}

static void genie_ultra_prov_recv_failed(uint8_t *buf)
{
    uint8_t reason = 0;
    genie_triple_t *p_genie_triple = genie_triple_get();

    if (buf[0] != p_genie_triple->mac[4] || buf[1] != p_genie_triple->mac[5])
    {
        return;
    }

    reason = buf[2];
    if (reason > 0)
    {
        GENIE_LOG_ERR("ultra prov fail reason:%d", reason);
    }

    genie_ultra_prov_deinit();
}

static int check_cache(uint8_t *data, u16_t len)
{
    static uint8_t cache_index = 0;
    uint16_t crc16 = 0;
    uint32_t cur_time = k_uptime_get();
    uint32_t expired = 0;
    int index = 0;

    crc16 = genie_crc16_compute(data, len, NULL);
    //printf("crc16:0x%04x data:%s\n", crc16, bt_hex(data, len));
    for (index = 0; index < ULTRA_PROV_CACHE_SIZE; index++)
    {
        if (prov_cache[index].crc16 == crc16)
        {
            expired = prov_cache[index].time + ULTRA_PROV_DEDUPLICATE_DURATION;
            if (cur_time < expired)
            {
                //printf("dup index:%d crc16:0x%04x time:%d\n", index, crc16, prov_cache[index].time);
                break;
            }
        }
    }

    if (index < ULTRA_PROV_CACHE_SIZE)
    {
        return 1;
    }
    else
    {
        prov_cache[cache_index].crc16 = crc16;
        prov_cache[cache_index].time = cur_time;
        cache_index++;
        cache_index %= ULTRA_PROV_CACHE_SIZE;

        return 0;
    }
}

int genie_ultra_prov_handle(uint8_t frame_type, void *frame_buf)
{
    uint16_t company = 0;
    uint8_t fixed_byte = 0;
    uint8_t prov_cmd = 0;
    struct net_buf_simple *buf = NULL;

    buf = (struct net_buf_simple *)frame_buf;
    if (frame_type != GENIE_ULTRA_PROV_ADV_TYPE || buf == NULL)
    {
        return -1;
    }

    company = net_buf_simple_pull_be16(buf);
    if (company == CONFIG_MESH_VENDOR_COMPANY_ID)
    {
        fixed_byte = net_buf_simple_pull_u8(buf);
        if (fixed_byte == GENIE_ULTRA_PROV_FIXED_BYTE)
        {
            if (check_cache(buf->data, buf->len))
            {
                GENIE_LOG_INFO("dup ultra prov data:%s", bt_hex(buf->data, buf->len));
                return -1;
            }

            prov_cmd = net_buf_simple_pull_u8(buf);
            switch (prov_cmd)
            {
            case GENIE_ULTRA_PROV_TYPE_RANDOM:
            {
                genie_ultra_prov_recv_random(buf->data);
            }
            break;
            case GENIE_ULTRA_PROV_TYPE_PROV_DATA:
            {
                genie_ultra_prov_recv_data(buf->data);
            }
            break;
            case GENIE_ULTRA_PROV_TYPE_PROV_FAILED:
            {
                genie_ultra_prov_recv_failed(buf->data);
            }
            break;
            default:
                break;
            }
        }
    }

    return 0;
}

int genie_ultra_prov_init(void)
{
    memset(&genie_ultra_prov_ctx, 0, sizeof(genie_ultra_prov_ctx_t));
#ifdef ULTRA_PROV_RESEND_ENABLE
    k_delayed_work_init(&genie_ultra_prov_ctx.timer, ultra_prov_timeout);
#endif
    return 0;
}

int genie_ultra_prov_deinit(void)
{
    genie_ultra_prov_ctx_t ultra_prov_reset;

    memset(prov_cache, 0, sizeof(prov_cache));
    memset(&ultra_prov_reset, 0, sizeof(genie_ultra_prov_ctx_t));
    if (!memcmp(&genie_ultra_prov_ctx, &ultra_prov_reset, sizeof(genie_ultra_prov_ctx_t)))
    {
        return 0; //deinit done
    }

#ifdef ULTRA_PROV_RESEND_ENABLE
    k_delayed_work_cancel(&genie_ultra_prov_ctx.timer);
#endif

    memset(&genie_ultra_prov_ctx, 0, sizeof(genie_ultra_prov_ctx_t));

    return 0;
}
