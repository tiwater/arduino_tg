/*
 * Copyright (C) 2019-2020 Alibaba Group Holding Limited
 */

#include "genie_mesh_internal.h"

#define GENIE_MIN(a, b) (a) < (b) ? (a) : (b)

static ATOMIC_DEFINE(indicate_flags, SIG_MODEL_INDICATE_FLAGS);

static sig_model_event_e sig_model_event_handle_delay_start(sig_model_element_state_t *p_elem)
{
#ifdef CONFIG_MESH_MODEL_TRANS
    sig_model_transition_timer_stop(p_elem);
#endif

    k_timer_start(&p_elem->state.delay_timer, p_elem->state.delay * 5);

    return SIG_MODEL_EVT_NONE;
}

static sig_model_event_e sig_model_event_handle_delay_end(sig_model_element_state_t *p_elem)
{
#ifdef CONFIG_MESH_MODEL_TRANS
    u32_t cur_time = k_uptime_get();
#endif

    p_elem->state.delay = 0;

#ifdef CONFIG_MESH_MODEL_TRANS
    if (p_elem->state.trans == 0 || cur_time >= p_elem->state.trans_end_time)
    {
        sig_model_transition_state_reset(p_elem);
        return SIG_MODEL_EVT_ACTION_DONE;
    }
    else
    {
        return SIG_MODEL_EVT_TRANS_START;
    }
#else
    return SIG_MODEL_EVT_ACTION_DONE;
#endif
}

#ifdef CONFIG_MESH_MODEL_TRANS
static sig_model_event_e sig_model_event_handle_trans_start(sig_model_element_state_t *p_elem)
{
    u32_t cur_time = k_uptime_get();

    sig_model_transition_timer_stop(p_elem);

    //check time
    if (cur_time >= p_elem->state.trans_end_time - SIG_MODEL_TRANSITION_INTERVAL)
    {
        return SIG_MODEL_EVT_TRANS_END;
    }
    else
    {
        //start cycle
        k_timer_start(&p_elem->state.trans_timer, SIG_MODEL_TRANSITION_INTERVAL);
        BT_DBG("start trans %p", &p_elem->state.trans_timer);

        return SIG_MODEL_EVT_NONE;
    }
}

static sig_model_event_e sig_model_event_handle_trans_cycle(sig_model_element_state_t *p_elem)
{
    if (sig_model_transition_update(p_elem) == 0)
    {
        p_elem->state.trans = 0;
    }

    return SIG_MODEL_EVT_NONE;
}

static sig_model_event_e sig_model_event_handle_trans_end(sig_model_element_state_t *p_elem)
{
    //clear paras
    sig_model_transition_state_reset(p_elem);
    //action done
    return SIG_MODEL_EVT_ACTION_DONE;
}
#endif

static sig_model_event_e sig_model_event_handle_analyze_msg(sig_model_element_state_t *p_elem)
{
#ifdef CONFIG_MESH_MODEL_TRANS
    if (p_elem->state.trans || p_elem->state.delay)
    {
        if (p_elem->state.delay)
        {
            return SIG_MODEL_EVT_DELAY_START;
        }
        else
        {
            return SIG_MODEL_EVT_TRANS_START;
        }
    }
#endif

    return SIG_MODEL_EVT_ACTION_DONE;
}

static sig_model_event_e sig_model_handle_action_done(sig_model_element_state_t *p_elem)
{
#ifdef CONFIG_MESH_MODEL_GEN_ONOFF_SRV
    BT_DBG("onoff cur(%d) tar(%d)", p_elem->state.onoff[TYPE_PRESENT], p_elem->state.onoff[TYPE_TARGET]);

    if (p_elem->state.onoff[TYPE_PRESENT] != p_elem->state.onoff[TYPE_TARGET])
    {
        p_elem->state.onoff[TYPE_PRESENT] = p_elem->state.onoff[TYPE_TARGET];
        atomic_set_bit(indicate_flags, SIG_MODEL_INDICATE_GEN_ONOFF);
    }
#endif

#ifdef CONFIG_MESH_MODEL_LIGHTNESS_SRV
    BT_DBG("lightness cur(%04x) tar(%04x)", p_elem->state.lightness[TYPE_PRESENT], p_elem->state.lightness[TYPE_TARGET]);

    if (p_elem->state.lightness[TYPE_PRESENT] != p_elem->state.lightness[TYPE_TARGET])
    {
        p_elem->state.lightness[TYPE_PRESENT] = p_elem->state.lightness[TYPE_TARGET];
        atomic_set_bit(indicate_flags, SIG_MODEL_INDICATE_GEN_LIGHTNESS);
    }
#endif

#ifdef CONFIG_MESH_MODEL_CTL_SRV
    BT_DBG("color_temperature cur(%04x) tar(%04x)", p_elem->state.color_temperature[TYPE_PRESENT], p_elem->state.color_temperature[TYPE_TARGET]);

    if (p_elem->state.color_temperature[TYPE_PRESENT] != p_elem->state.color_temperature[TYPE_TARGET])
    {
        p_elem->state.color_temperature[TYPE_PRESENT] = p_elem->state.color_temperature[TYPE_TARGET];
        atomic_set_bit(indicate_flags, SIG_MODEL_INDICATE_GEN_CTL);
    }
#endif

#ifdef CONFIG_MESH_MODEL_SCENE_SRV
    BT_DBG("scene cur(%04x) tar(%04x)", p_elem->state.scene_number[TYPE_PRESENT], p_elem->state.scene_number[TYPE_TARGET]);

    if (p_elem->state.scene_number[TYPE_PRESENT] != p_elem->state.scene_number[TYPE_TARGET])
    {
        p_elem->state.scene_number[TYPE_PRESENT] = p_elem->state.scene_number[TYPE_TARGET];
        atomic_set_bit(indicate_flags, SIG_MODEL_INDICATE_GEN_SCENE);
    }
#endif

    if (bt_mesh_is_provisioned() && (genie_mesh_get_init_state() == GENIE_MESH_INIT_STATE_NORMAL_BOOT)) //until to random delay done
    {
#ifdef CONFIG_BT_MESH_NPS_OPT
        if (BT_MESH_ADDR_IS_GROUP(p_elem->ctx.recv_dst))
        {
            BT_DBG("group ctrl report sig status");
            return SIG_MODEL_EVT_REPORT_STATUS;
        }
        else
        {
            p_elem->ctx.op_status = 0;
            atomic_set_bit(indicate_flags, SIG_MODEL_INDICATE_FLAGS);
            return SIG_MODEL_EVT_NONE;
        }
#endif

        return SIG_MODEL_EVT_INDICATE;
    }

    return SIG_MODEL_EVT_NONE;
}

static sig_model_event_e sig_model_handle_indicate(sig_model_element_state_t *p_elem)
{
    uint8_t payload[SIG_MODEL_INDICATE_PAYLOAD_MAX_LEN];
    uint8_t payload_len = 0;
    genie_transport_model_param_t transport_model_param = {0};

    if (p_elem == NULL)
    {
        return SIG_MODEL_EVT_NONE;
    }

#ifdef CONFIG_MESH_MODEL_GEN_ONOFF_SRV
    if (atomic_test_and_clear_bit(indicate_flags, SIG_MODEL_INDICATE_GEN_ONOFF))
    {
        payload[payload_len++] = ATTR_TYPE_GENERIC_ONOFF & 0xff;
        payload[payload_len++] = (ATTR_TYPE_GENERIC_ONOFF >> 8) & 0xff;
        payload[payload_len++] = p_elem->state.onoff[TYPE_PRESENT];
    }
#endif
#ifdef CONFIG_MESH_MODEL_LIGHTNESS_SRV
    if (atomic_test_and_clear_bit(indicate_flags, SIG_MODEL_INDICATE_GEN_LIGHTNESS))
    {
        payload[payload_len++] = ATTR_TYPE_LIGHTNESS & 0xff;
        payload[payload_len++] = (ATTR_TYPE_LIGHTNESS >> 8) & 0xff;
        payload[payload_len++] = p_elem->state.lightness[TYPE_PRESENT] & 0xff;
        payload[payload_len++] = (p_elem->state.lightness[TYPE_PRESENT] >> 8) & 0xff;
    }
#endif
#ifdef CONFIG_MESH_MODEL_CTL_SRV
    if (atomic_test_and_clear_bit(indicate_flags, SIG_MODEL_INDICATE_GEN_CTL))
    {
        payload[payload_len++] = ATTR_TYPE_COLOR_TEMPERATURE & 0xff;
        payload[payload_len++] = (ATTR_TYPE_COLOR_TEMPERATURE >> 8) & 0xff;
        payload[payload_len++] = p_elem->state.color_temperature[TYPE_PRESENT] & 0xff;
        payload[payload_len++] = (p_elem->state.color_temperature[TYPE_PRESENT] >> 8) & 0xff;
    }
#endif
#ifdef CONFIG_MESH_MODEL_SCENE_SRV
    if (atomic_test_and_clear_bit(indicate_flags, SIG_MODEL_INDICATE_GEN_SCENE))
    {
        payload[payload_len++] = ATTR_TYPE_SENCE & 0xff;
        payload[payload_len++] = (ATTR_TYPE_SENCE >> 8) & 0xff;
        payload[payload_len++] = p_elem->state.scene_number[TYPE_PRESENT] & 0xff;
        payload[payload_len++] = (p_elem->state.scene_number[TYPE_PRESENT] >> 8) & 0xff;
    }
#endif

    if (payload_len > 0)
    {
        memset(&transport_model_param, 0, sizeof(genie_transport_model_param_t));
        transport_model_param.opid = VENDOR_OP_ATTR_INDICATE;
        transport_model_param.data = payload;
        transport_model_param.len = GENIE_MIN(SIG_MODEL_INDICATE_PAYLOAD_MAX_LEN, payload_len);
        transport_model_param.p_elem = bt_mesh_elem_find_by_id(p_elem->element_id);
        transport_model_param.retry_period = GENIE_TRANSPORT_EACH_PDU_TIMEOUT * genie_transport_get_seg_count(transport_model_param.len);
        transport_model_param.retry = GENIE_TRANSPORT_DEFAULT_RETRY_COUNT;
        transport_model_param.app_idx = BT_MESH_KEY_UNUSED;

        genie_transport_send_model(&transport_model_param);
    }

    return SIG_MODEL_EVT_NONE;
}

#ifdef CONFIG_BT_MESH_NPS_OPT
#define RANDOM_DELAY_HAS_MAC 1
uint32_t random_delay_time_get(uint32_t scope)
{
    uint8_t rand = 0;
    uint32_t delay_time = 0;
#if RANDOM_DELAY_HAS_MAC
    uint8_t mac_byte = 0;
    uint8_t mac_mod = 0;
    uint8_t mod = 3;
    uint32_t scope_unit = 0;
    genie_triple_t *p_genie_triple = NULL;

    p_genie_triple = genie_triple_get();
    if (p_genie_triple != NULL)
    {
        mac_byte = p_genie_triple->mac[GENIE_TRIPLE_MAC_SIZE - 1];
        mac_mod = mac_byte % mod;
        scope_unit = scope / mod;
        bt_rand(&rand, 1);
        delay_time = (mac_mod * scope_unit) + (scope_unit * rand / 255);
        GENIE_LOG_INFO("delay_time:%u, rand:%u, scope_unit:%u, mac_mod:%u", delay_time, rand, scope_unit, mac_mod);
    }
    else
#endif
    {
        bt_rand(&rand, 1);
        delay_time = scope * rand / 255;
        GENIE_LOG_INFO("delay_time:%u, rand:%u, scope:%u", delay_time, rand, scope);
    }

    return delay_time;
}

static sig_model_event_e sig_model_handle_report_status_single(sig_model_element_state_t *p_elem, uint8_t delay)
{
    uint8_t payload[SIG_MODEL_INDICATE_PAYLOAD_MAX_LEN];
    uint8_t payload_len = 0;
    uint16_t random_time = 0;
    uint16_t delay_max = 0, delay_min = 0;
    genie_transport_model_param_t transport_model_param;

    if (p_elem == NULL)
    {
        return SIG_MODEL_EVT_NONE;
    }

    memset(payload, 0, SIG_MODEL_INDICATE_PAYLOAD_MAX_LEN);
    memset(&transport_model_param, 0, sizeof(genie_transport_model_param_t));
    transport_model_param.p_elem = bt_mesh_elem_find_by_id(p_elem->element_id);
    transport_model_param.app_idx = BT_MESH_KEY_UNUSED;

    if (p_elem->ctx.op_status == OP_GENERIC_ONOFF_STATUS)
    {
#ifdef CONFIG_MESH_MODEL_GEN_ONOFF_SRV
        transport_model_param.p_model = bt_mesh_model_find(transport_model_param.p_elem, BT_MESH_MODEL_ID_GEN_ONOFF_SRV);
        payload[payload_len++] = (p_elem->ctx.op_status >> 8) & 0xff;
        payload[payload_len++] = p_elem->ctx.op_status & 0xff;
        payload[payload_len++] = p_elem->state.onoff[TYPE_PRESENT];
#endif
    }
    else if (p_elem->ctx.op_status == OP_GENERIC_LIGHTNESS_STATUS)
    {
#ifdef CONFIG_MESH_MODEL_LIGHTNESS_SRV
        transport_model_param.p_model = bt_mesh_model_find(transport_model_param.p_elem, BT_MESH_MODEL_ID_LIGHT_LIGHTNESS_SRV);
        payload[payload_len++] = (p_elem->ctx.op_status >> 8) & 0xff;
        payload[payload_len++] = p_elem->ctx.op_status & 0xff;
        payload[payload_len++] = p_elem->state.lightness[TYPE_PRESENT];
        payload[payload_len++] = (p_elem->state.lightness[TYPE_PRESENT] >> 8) & 0xff;
#endif
    }
    else if (p_elem->ctx.op_status == OP_GENERIC_LIGHT_CTL_STATUS)
    {
#ifdef CONFIG_MESH_MODEL_CTL_SRV
        transport_model_param.p_model = bt_mesh_model_find(transport_model_param.p_elem, BT_MESH_MODEL_ID_LIGHT_CTL_SRV);
        payload[payload_len++] = (p_elem->ctx.op_status >> 8) & 0xff;
        payload[payload_len++] = p_elem->ctx.op_status & 0xff;
        payload[payload_len++] = p_elem->state.lightness[TYPE_PRESENT];
        payload[payload_len++] = (p_elem->state.lightness[TYPE_PRESENT] >> 8) & 0xff;
        payload[payload_len++] = p_elem->state.color_temperature[TYPE_PRESENT] & 0xff;
        payload[payload_len++] = (p_elem->state.color_temperature[TYPE_PRESENT] >> 8) & 0xff;
#endif
    }
    else if (p_elem->ctx.op_status == OP_GENERIC_SCENE_STATUS)
    {
#ifdef CONFIG_MESH_MODEL_SCENE_SRV
        transport_model_param.p_model = bt_mesh_model_find(transport_model_param.p_elem, BT_MESH_MODEL_ID_SCENE_SRV);
        payload[payload_len++] = p_elem->ctx.op_status & 0xff;
        payload[payload_len++] = p_elem->ctx.scene_status_code; //Register status code
        payload[payload_len++] = p_elem->state.scene_number[TYPE_PRESENT] & 0xff;
        payload[payload_len++] = (p_elem->state.scene_number[TYPE_PRESENT] >> 8) & 0xff;
        payload[payload_len++] = p_elem->state.scene_number[TYPE_TARGET] & 0xff;
        payload[payload_len++] = (p_elem->state.scene_number[TYPE_TARGET] >> 8) & 0xff;
#ifdef CONFIG_MESH_MODEL_TRANS
        payload[payload_len++] = sig_model_transition_get_remain_time_byte(&p_elem->state, 0);
#endif
#endif
    }
    else if (p_elem->ctx.op_status == OP_GENERIC_SCENE_REGISTER_STATUS)
    {
#ifdef CONFIG_MESH_MODEL_SCENE_SRV
        transport_model_param.p_model = bt_mesh_model_find(transport_model_param.p_elem, BT_MESH_MODEL_ID_SCENE_SETUP_SRV);
        payload[payload_len++] = (p_elem->ctx.op_status >> 8) & 0xff;
        payload[payload_len++] = p_elem->ctx.op_status & 0xff;
        payload[payload_len++] = p_elem->ctx.scene_status_code; //Register status code
        payload[payload_len++] = p_elem->state.scene_number[TYPE_PRESENT] & 0xff;
        payload[payload_len++] = (p_elem->state.scene_number[TYPE_PRESENT] >> 8) & 0xff;

        if (p_elem->ctx.all_scene_register)
        {
            payload_len += genie_scene_packet_registered_data(p_elem->element_id, &payload[payload_len], SIG_MODEL_INDICATE_PAYLOAD_MAX_LEN - payload_len);
        }
#endif
    }

    if (payload_len > 0)
    {
        transport_model_param.opid = SIG_MODEL_OP_DATA_FLAG;
        transport_model_param.app_idx = p_elem->ctx.app_idx;
        transport_model_param.dst_addr = p_elem->ctx.addr;
        transport_model_param.ttl = genie_nps_config_send_ttl_get();
        transport_model_param.xmit = BT_MESH_ADV_XMIT_FLAG;
        transport_model_param.data = payload;
        transport_model_param.len = GENIE_MIN(SIG_MODEL_INDICATE_PAYLOAD_MAX_LEN, payload_len);

        if (delay == 1)
        {
            delay_max = genie_nps_config_group_delay_max_get();
            delay_min = genie_nps_config_group_delay_min_get();
            random_time = delay_min + random_delay_time_get(delay_max - delay_min);
            GENIE_LOG_INFO("opcode:0x%04x, rand delay:%u", p_elem->ctx.op_status, random_time);
            transport_model_param.retry_period = random_time;
            if (random_time > 0)
            {
                transport_model_param.retry = 0;
            }
            else
            {
                transport_model_param.retry = 1;
            }
        }
        else
        {
            transport_model_param.retry_period = 0;
            transport_model_param.retry = 1;
        }

        genie_transport_send_model(&transport_model_param);

        p_elem->ctx.op_status = 0;
        atomic_set_bit(indicate_flags, SIG_MODEL_INDICATE_FLAGS);
    }

    return SIG_MODEL_EVT_NONE;
}

#if 0
static sig_model_event_e sig_model_report_status_union(sig_model_element_state_t *p_elem, uint8_t flags)
{
    uint8_t payload[SIG_MODEL_INDICATE_PAYLOAD_MAX_LEN];
    uint8_t payload_len = 0;
    genie_transport_model_param_t transport_model_param = {0};

    if (p_elem == NULL)
    {
        return SIG_MODEL_EVT_NONE;
    }

    if ((flags == SIG_MODEL_REPORT_STATUS_ONE) || (flags == SIG_MODEL_REPORT_STATUS_HB))
    {
#ifdef CONFIG_MESH_MODEL_GEN_ONOFF_SRV
        payload[payload_len++] = ATTR_TYPE_GENERIC_ONOFF & 0xff;
        payload[payload_len++] = (ATTR_TYPE_GENERIC_ONOFF >> 8) & 0xff;
        payload[payload_len++] = p_elem->state.onoff[TYPE_PRESENT];
#endif

#ifdef CONFIG_MESH_MODEL_LIGHTNESS_SRV
        payload[payload_len++] = ATTR_TYPE_LIGHTNESS & 0xff;
        payload[payload_len++] = (ATTR_TYPE_LIGHTNESS >> 8) & 0xff;
        payload[payload_len++] = p_elem->state.lightness[TYPE_PRESENT] & 0xff;
        payload[payload_len++] = (p_elem->state.lightness[TYPE_PRESENT] >> 8) & 0xff;
#endif
    }

    if (flags == SIG_MODEL_REPORT_STATUS_TWO)
    {
#ifdef CONFIG_MESH_MODEL_CTL_SRV
        payload[payload_len++] = ATTR_TYPE_COLOR_TEMPERATURE & 0xff;
        payload[payload_len++] = (ATTR_TYPE_COLOR_TEMPERATURE >> 8) & 0xff;
        payload[payload_len++] = p_elem->state.color_temperature[TYPE_PRESENT] & 0xff;
        payload[payload_len++] = (p_elem->state.color_temperature[TYPE_PRESENT] >> 8) & 0xff;
#endif

#ifdef CONFIG_MESH_MODEL_SCENE_SRV
        payload[payload_len++] = ATTR_TYPE_SENCE & 0xff;
        payload[payload_len++] = (ATTR_TYPE_SENCE >> 8) & 0xff;
        payload[payload_len++] = p_elem->state.scene_number[TYPE_PRESENT] & 0xff;
        payload[payload_len++] = (p_elem->state.scene_number[TYPE_PRESENT] >> 8) & 0xff;
#endif
    }

    if (payload_len > 0)
    {
        memset(&transport_model_param, 0, sizeof(genie_transport_model_param_t));
        transport_model_param.opid = VENDOR_OP_ATTR_STATUS;
        transport_model_param.data = payload;
        transport_model_param.len = GENIE_MIN(SIG_MODEL_INDICATE_PAYLOAD_MAX_LEN, payload_len);
        transport_model_param.p_elem = bt_mesh_elem_find_by_id(p_elem->element_id);
        transport_model_param.ttl = genie_nps_config_send_ttl_get();
        transport_model_param.xmit = BT_MESH_ADV_XMIT_FLAG;
        transport_model_param.retry_period = 0;
        transport_model_param.retry = 1;
        transport_model_param.app_idx = BT_MESH_KEY_UNUSED;
        
        genie_transport_send_model(&transport_model_param);
    }

    return SIG_MODEL_EVT_NONE;
}
#endif

static sig_model_event_e sig_model_report_compress_hb(sig_model_element_state_t *p_elem)
{
    uint8_t payload[20];
    uint8_t payload_len = 0;
    uint8_t type = 0;
    genie_transport_model_param_t transport_model_param;

    if (p_elem == NULL)
    {
        return SIG_MODEL_EVT_NONE;
    }

    memset(&transport_model_param, 0, sizeof(genie_transport_model_param_t));
    transport_model_param.p_elem = bt_mesh_elem_find_by_id(p_elem->element_id);
    transport_model_param.p_model = bt_mesh_model_find_vnd(transport_model_param.p_elem, CONFIG_MESH_VENDOR_COMPANY_ID, CONFIG_MESH_VENDOR_MODEL_SRV);

    memset(&payload, 0, sizeof(payload));
    payload[0] = VENDOR_OP_ATTR_HB_REPORT;
    payload_len += 2;

#ifdef CONFIG_MESH_MODEL_GEN_ONOFF_SRV
    type = type | 0x01;
    payload[payload_len++] = p_elem->state.onoff[TYPE_PRESENT];
#endif

#ifdef CONFIG_MESH_MODEL_LIGHTNESS_SRV
    type = type | 0x02;
    payload[payload_len++] = p_elem->state.lightness[TYPE_PRESENT];
    payload[payload_len++] = (p_elem->state.lightness[TYPE_PRESENT] >> 8) & 0xff;
#endif

#ifdef CONFIG_MESH_MODEL_CTL_SRV
    type = type | 0x04;
    payload[payload_len++] = p_elem->state.color_temperature[TYPE_PRESENT] & 0xff;
    payload[payload_len++] = (p_elem->state.color_temperature[TYPE_PRESENT] >> 8) & 0xff;
#endif

#ifdef CONFIG_MESH_MODEL_SCENE_SRV
    type = type | 0x08;
    payload[payload_len++] = p_elem->state.scene_number[TYPE_PRESENT] & 0xff;
    payload[payload_len++] = (p_elem->state.scene_number[TYPE_PRESENT] >> 8) & 0xff;
#endif

    payload[1] = type;
    if (payload_len > 2)
    {
        transport_model_param.opid = SIG_MODEL_OP_DATA_FLAG;
        transport_model_param.app_idx = BT_MESH_KEY_UNUSED;
        transport_model_param.dst_addr = BT_MESH_ADDR_TMALL_GENIE;
        transport_model_param.ttl = genie_nps_config_send_ttl_get();
        transport_model_param.xmit = BT_MESH_ADV_XMIT_FLAG;
        transport_model_param.data = payload;
        transport_model_param.len = payload_len;
        transport_model_param.retry_period = 0;
        transport_model_param.retry = 1;
        genie_transport_send_model(&transport_model_param);
    }

    return SIG_MODEL_EVT_NONE;
}

#define GENIE_POWERON_REPORT_OTA_VER 0
static aos_timer_t report_timer;
static uint8_t report_flag = SIG_MODEL_REPORT_STATUS_ONE;

static void sig_model_report_timer_cb(void *p_timer, void *args)
{
    sig_model_element_state_t *p_elem;
    uint16_t per_interval = 0;
    uint32_t delay_time = 0;

    aos_timer_stop(&report_timer);

    p_elem = (sig_model_element_state_t *)args;
    if (p_elem == NULL)
    {
        return;
    }

    if (report_flag == SIG_MODEL_REPORT_STATUS_ONE)
    {
        //sig_model_report_status_union(p_elem, report_flag);
        sig_model_report_compress_hb(p_elem);
        #if GENIE_POWERON_REPORT_OTA_VER
        report_flag = SIG_MODEL_REPORT_OTHER_DATA;
        delay_time = genie_nps_config_boot_interval_get() + random_delay_time_get(GENIE_NPS_CONFIG_APPEND_DELAY);
        aos_timer_change(&report_timer, delay_time);
        aos_timer_start(&report_timer);
        #else
        per_interval = genie_nps_config_per_interval_get();
        if (per_interval > 0)
        {
            delay_time = GENIE_NPS_CONFIG_PER_DELAY_MIN + random_delay_time_get(per_interval * 1000);
            GENIE_LOG_INFO("hb rand delay:%u", delay_time);
            report_flag = SIG_MODEL_REPORT_STATUS_HB;
            aos_timer_change(&report_timer, delay_time);
            aos_timer_start(&report_timer);
        }
        else
        {
            report_flag = SIG_MODEL_REPORT_FLAGS;
        }
        #endif
    }
    #if GENIE_POWERON_REPORT_OTA_VER
    else if (report_flag == SIG_MODEL_REPORT_OTHER_DATA)
    {
        // genie_mesh_report();
        #ifdef CONFIG_GENIE_OTA
        genie_ota_report_version();
        #endif
        per_interval = genie_nps_config_per_interval_get();
        if (per_interval > 0)
        {
            delay_time = GENIE_NPS_CONFIG_PER_DELAY_MIN + random_delay_time_get(per_interval * 1000);
            GENIE_LOG_INFO("hb rand delay:%u", delay_time);
            report_flag = SIG_MODEL_REPORT_STATUS_HB;
            aos_timer_change(&report_timer, delay_time);
            aos_timer_start(&report_timer);
        }
        else
        {
            report_flag = SIG_MODEL_REPORT_FLAGS;
        }
    }
    #endif
    else if (report_flag == SIG_MODEL_REPORT_STATUS_HB)
    {
        uint8_t delay_flg = 0;
        uint32_t diff_ms = 0;
        uint32_t cur_time_ms = k_uptime_get_32();

        per_interval = genie_nps_config_per_interval_get();
        if (per_interval > 0)
        {
            if (p_elem->ctx.op_time_ms > 0)
            {
                if (cur_time_ms > p_elem->ctx.op_time_ms)
                {
                    diff_ms = cur_time_ms - p_elem->ctx.op_time_ms;
                }
                else
                {
                    diff_ms = p_elem->ctx.op_time_ms - cur_time_ms;
                }
                if (diff_ms < GENIE_NPS_GUARD_TIME)
                {
                    delay_flg = 1;
                }
            }
            if (delay_flg == 1)
            {
                GENIE_LOG_INFO("hb delay, diff_ms:%u", diff_ms);
                aos_timer_change(&report_timer, GENIE_NPS_GUARD_TIME);
            }
            else
            {
                //sig_model_report_status_union(p_elem, report_flag);
                sig_model_report_compress_hb(p_elem);
                aos_timer_change(&report_timer, per_interval * 1000);
            }
            aos_timer_start(&report_timer);
        }
        else
        {
            report_flag = SIG_MODEL_REPORT_FLAGS;
        }
    }

    return;
}

void sig_model_report_poweron_status(sig_model_element_state_t *p_elem)
{
    if (p_elem == NULL)
    {
        return;
    }

    p_elem->ctx.app_idx = BT_MESH_KEY_UNUSED;
    p_elem->ctx.addr = BT_MESH_ADDR_TMALL_GENIE;

    report_flag = SIG_MODEL_REPORT_STATUS_ONE;
    if (report_timer.hdl != NULL)
    {
        aos_timer_stop(&report_timer);
        aos_timer_free(&report_timer);
    }
    aos_timer_new(&report_timer, sig_model_report_timer_cb, (void *)p_elem, 100, 0);

    return;
}
#endif

void sig_model_event_set_indicate(int indicate)
{
    atomic_set_bit(indicate_flags, indicate);
}

void sig_model_event(sig_model_event_e event, void *p_arg)
{
    sig_model_event_e next_event = SIG_MODEL_EVT_NONE;

    if (event != SIG_MODEL_EVT_NONE)
    {
#ifdef CONFIG_MESH_MODEL_TRANS
        if (event != SIG_MODEL_EVT_TRANS_CYCLE)
        {
            GENIE_LOG_INFO("SigE:%d", event);
        }
#else
        GENIE_LOG_INFO("SigE:%d", event);
#endif
    }

    switch (event)
    {
    case SIG_MODEL_EVT_INDICATE:
    {
        next_event = sig_model_handle_indicate((sig_model_element_state_t *)p_arg);
    }
    break;
#ifdef CONFIG_BT_MESH_NPS_OPT
    case SIG_MODEL_EVT_REPORT_STATUS:
    {
        next_event = sig_model_handle_report_status_single((sig_model_element_state_t *)p_arg, 1);
    }
    break;
#endif
    case SIG_MODEL_EVT_ACTION_DONE:
    {
        next_event = sig_model_handle_action_done((sig_model_element_state_t *)p_arg);
    }
    break;
    case SIG_MODEL_EVT_ANALYZE_MSG:
    {
        next_event = sig_model_event_handle_analyze_msg((sig_model_element_state_t *)p_arg);
    }
    break;

    case SIG_MODEL_EVT_DELAY_START:
    {
        next_event = sig_model_event_handle_delay_start((sig_model_element_state_t *)p_arg);
    }
    break;
    case SIG_MODEL_EVT_DELAY_END:
    {
        next_event = sig_model_event_handle_delay_end((sig_model_element_state_t *)p_arg);
    }
    break;
#ifdef CONFIG_MESH_MODEL_TRANS
    case SIG_MODEL_EVT_TRANS_START:
    {
        next_event = sig_model_event_handle_trans_start((sig_model_element_state_t *)p_arg);
    }
    break;
    case SIG_MODEL_EVT_TRANS_CYCLE:
    {
        next_event = sig_model_event_handle_trans_cycle((sig_model_element_state_t *)p_arg);
    }
    break;
    case SIG_MODEL_EVT_TRANS_END:
    {
        next_event = sig_model_event_handle_trans_end((sig_model_element_state_t *)p_arg);
    }
    break;
#endif
    case SIG_MODEL_EVT_GENERIC_MESG:
    {
        sig_model_msg *p_msg = (sig_model_msg *)p_arg;
        genie_down_msg(GENIE_DOWN_MESG_SIG_TYPE, p_msg->opcode, p_arg);
    }
    break;
    default:
        break;
    }

    if (next_event != SIG_MODEL_EVT_NONE)
    {
        sig_model_event(next_event, p_arg);
    }

    if (event == SIG_MODEL_EVT_ACTION_DONE)
    {
        genie_event(GENIE_EVT_USER_ACTION_DONE, p_arg);
    }
#ifdef CONFIG_MESH_MODEL_TRANS
    if (event == SIG_MODEL_EVT_TRANS_CYCLE)
    {
        genie_event(GENIE_EVT_USER_TRANS_CYCLE, p_arg);
    }
#endif
}
