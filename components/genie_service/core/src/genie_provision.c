/*
 * Copyright (C) 2018-2021 Alibaba Group Holding Limited
 */

#define BT_DBG_ENABLED IS_ENABLED(CONFIG_BT_MESH_DEBUG_PROV)
#include "genie_mesh_internal.h"

static struct k_timer pbadv_timeout_timer;
static struct k_timer prov_timeout_timer;

static genie_provision_state_e provision_state = GENIE_PROVISION_UNPROV;
static uint8_t genie_uuid[GENIE_PROVISON_UUID_LEN];

static void genie_provision_set_silent_flag(void)
{
    genie_uuid[13] |= UNPROV_ADV_FEATURE1_SILENT_ADV;
}

void genie_provision_clear_silent_flag(void)
{
    genie_uuid[13] &= UNPROV_ADV_FEATURE1_SILENT_UNMASK;
}

int genie_provision_set_state(genie_provision_state_e state)
{
    provision_state = state;

    return 0;
}

genie_provision_state_e genie_provision_get_state(void)
{
    return provision_state;
}

uint8_t *genie_provision_get_uuid(void)
{
    int i = 0;
    genie_triple_t *p_genie_triple = NULL;

    p_genie_triple = genie_triple_get();

    // all fields in uuid should be in little-endian
    // CID: Taobao
    genie_uuid[0] = CONFIG_MESH_VENDOR_COMPANY_ID & 0xFF;
    genie_uuid[1] = (CONFIG_MESH_VENDOR_COMPANY_ID >> 8) & 0xFF;

    // PID
    // Bit0~Bit3: 0001 (broadcast version)
    // Bit4：1 (one secret pre device)
    // Bit5: 1 (OTA support)
    // Bit6~Bit7: 01 (00:4.0 01:4.2 10:5.0 11:>5.0)
    genie_uuid[2] = 0x71;

    // Product ID
    for (i = 0; i < 4; i++)
    {
        genie_uuid[3 + i] = (p_genie_triple->pid >> (i << 3)) & 0xFF;
    }

    // mac addr (device name)
    for (i = 0; i < GENIE_TRIPLE_MAC_SIZE; i++)
    {
        genie_uuid[7 + i] = p_genie_triple->mac[GENIE_TRIPLE_MAC_SIZE - 1 - i];
    }

    genie_uuid[13] = UNPROV_ADV_FEATURE1_UUID_VERSION;

    genie_uuid[14] = genie_sal_provision_get_module_type();

#ifdef CONFIG_GENIE_ULTRA_PROV
    genie_uuid[14] |= UNPROV_ADV_FEATURE2_ULTRA_PROV_FLAG;
    genie_uuid[14] |= UNPROV_ADV_FEATURE2_AUTH_FLAG;
#else
    genie_uuid[14] |= UNPROV_ADV_FEATURE2_AUTH_FLAG;
#endif

    GENIE_LOG_INFO("uuid: %s", bt_hex(genie_uuid, GENIE_PROVISON_UUID_LEN));

    return genie_uuid;
}

int genie_provision_get_saved_data(genie_provision_t *p_genie_provision)
{
    genie_storage_status_e ret = GENIE_STORAGE_SUCCESS;

    if (p_genie_provision == NULL)
    {
        return -1;
    }

    memset(p_genie_provision, 0, sizeof(genie_provision_t));

    ret = genie_storage_read_addr(&p_genie_provision->addr);
    if (ret != GENIE_STORAGE_SUCCESS)
    {
        return ret;
    }

    ret = genie_storage_read_seq(&p_genie_provision->seq);
    if (ret != GENIE_STORAGE_SUCCESS)
    {
        return ret;
    }

    ret = genie_storage_read_devkey(p_genie_provision->devkey);
    if (ret != GENIE_STORAGE_SUCCESS)
    {
        return ret;
    }

    ret = genie_storage_read_netkey(&p_genie_provision->netkey);
    if (ret != GENIE_STORAGE_SUCCESS)
    {
        return ret;
    }

    ret = genie_storage_read_appkey(0, &p_genie_provision->appkey);
    if (ret != GENIE_STORAGE_SUCCESS)
    {
        return ret;
    }

    return 0;
}

static void _genie_pbadv_timer_cb(void *p_timer, void *args)
{
    genie_event(GENIE_EVT_SDK_MESH_PBADV_TIMEOUT, NULL);
}

void genie_provision_pbadv_timer_start(uint32_t prov_timeout)
{
    static uint8_t inited = 0;

    if (!inited)
    {
        k_timer_init(&pbadv_timeout_timer, _genie_pbadv_timer_cb, NULL);
        inited = 1;
    }

    k_timer_start(&pbadv_timeout_timer, prov_timeout);
}

void genie_provision_pbadv_timer_stop(void)
{
    k_timer_stop(&pbadv_timeout_timer);
}

void genie_provision_start_slient_pbadv(void)
{
    genie_provision_set_silent_flag();
    //bt_mesh_prov_disable(BT_MESH_PROV_GATT | BT_MESH_PROV_ADV);
    extern void genie_set_silent_unprov_beacon_interval(bool is_silent);
    genie_set_silent_unprov_beacon_interval(true);
    bt_mesh_prov_enable(BT_MESH_PROV_ADV);
}

static void _genie_prov_timer_cb(void *p_timer, void *args)
{
    genie_event(GENIE_EVT_SDK_MESH_PROV_TIMEOUT, NULL);
}

void genie_provision_prov_timer_start(void)
{
    static uint8_t inited = 0;

    if (!inited)
    {
        k_timer_init(&prov_timeout_timer, _genie_prov_timer_cb, NULL);
        inited = 1;
    }
    k_timer_start(&prov_timeout_timer, MESH_PROVISIONING_TIMEOUT);
}

void genie_provision_prov_timer_stop(void)
{
    k_timer_stop(&prov_timeout_timer);
}
