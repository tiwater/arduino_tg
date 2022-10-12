/*
 * Copyright (C) 2018-2020 Alibaba Group Holding Limited
 */

#define BT_DBG_ENABLED IS_ENABLED(CONFIG_BT_MESH_DEBUG_MODEL)
#include "genie_mesh_internal.h"

struct bt_mesh_model_pub g_scene_pub = {};

static void _scene_prepare_buf(struct bt_mesh_model *p_model, struct net_buf_simple *p_msg, uint8_t scene_status, bool is_ack)
{
    sig_model_state_t *p_state = &((sig_model_element_state_t *)p_model->user_data)->state;

    bt_mesh_model_msg_init(p_msg, OP_GENERIC_SCENE_STATUS);
    net_buf_simple_add_u8(p_msg, scene_status);
    net_buf_simple_add_le16(p_msg, p_state->scene_number[TYPE_PRESENT]);
    net_buf_simple_add_le16(p_msg, p_state->scene_number[TYPE_TARGET]);

#ifdef CONFIG_MESH_MODEL_TRANS
    uint8_t remain_time_byte = sig_model_transition_get_remain_time_byte(p_state, is_ack);
    net_buf_simple_add_u8(p_msg, remain_time_byte);
#endif
}

static void _scene_status(struct bt_mesh_model *p_model, struct bt_mesh_msg_ctx *p_ctx, uint8_t scene_status, bool is_ack)
{
    struct net_buf_simple *p_msg = NET_BUF_SIMPLE(1 + 1 + 2 + 2 + 1 + 4); //Opcode + StatusCode + Present + Target + Trans

    GENIE_LOG_INFO("addr(0x%04x)", bt_mesh_model_elem(p_model)->addr);

    _scene_prepare_buf(p_model, p_msg, scene_status, is_ack);
    p_ctx->send_ttl = GENIE_TRANSPORT_DEFAULT_TTL;
    if (bt_mesh_model_send(p_model, p_ctx, p_msg, NULL, NULL))
    {
        BT_ERR("Unable to send scene Status");
    }
    BT_DBG("Success!!!");
}

static void _scene_register_prepare_buf(struct bt_mesh_model *p_model, struct net_buf_simple *p_msg, uint8_t status_code, bool is_all)
{
    uint8_t element_id = 0;

    sig_model_state_t *p_state = &((sig_model_element_state_t *)p_model->user_data)->state;

    bt_mesh_model_msg_init(p_msg, OP_GENERIC_SCENE_REGISTER_STATUS);
    net_buf_simple_add_u8(p_msg, status_code);
    net_buf_simple_add_le16(p_msg, p_state->scene_number[TYPE_PRESENT]);

    if (is_all)
    {
        element_id = bt_mesh_elem_find_id(bt_mesh_model_elem(p_model));
        genie_scene_add_registered_scene_number(element_id, p_msg);
    }
}

void genie_scene_register_status(struct bt_mesh_model *p_model, struct bt_mesh_msg_ctx *p_ctx, uint8_t status_code, bool is_all)
{
    struct net_buf_simple *p_msg = NET_BUF_SIMPLE(2 + 1 + 2 + 2 * GENIE_SCENE_STORE_MAX + 4); //Opcode + StatusCode + Current Scene Number + Registered Scene Numbers + Transport CRC

    BT_DBG("addr(0x%04x)", bt_mesh_model_elem(p_model)->addr);

    _scene_register_prepare_buf(p_model, p_msg, status_code, is_all);
    p_ctx->send_ttl = GENIE_TRANSPORT_DEFAULT_TTL;
    if (bt_mesh_model_send(p_model, p_ctx, p_msg, NULL, NULL))
    {
        BT_ERR("Unable to send scene Status");
    }
    BT_DBG("Success!!!");
}

static int genie_scene_recall_update(sig_model_element_state_t *p_elem_state, u16_t target_scene_number)
{
    genie_scene_data_t genie_scene_data;

    memset(&genie_scene_data, 0, sizeof(genie_scene_data_t));
    if (0 == genie_scene_setup_get_scene_data(p_elem_state->element_id, target_scene_number, &genie_scene_data))
    {
#ifdef CONFIG_MESH_MODEL_GEN_ONOFF_SRV
        p_elem_state->state.onoff[TYPE_TARGET] = genie_scene_data.onoff;
#endif
#ifdef CONFIG_MESH_MODEL_LIGHTNESS_SRV
        if (genie_scene_data.lightness > 0)
        {
            p_elem_state->state.lightness[TYPE_TARGET] = genie_scene_data.lightness;
        }
#endif
#ifdef CONFIG_MESH_MODEL_CTL_SRV
        if (genie_scene_data.color_temperature >= COLOR_TEMPERATURE_MIN && genie_scene_data.color_temperature <= COLOR_TEMPERATURE_MAX)
        {
            p_elem_state->state.color_temperature[TYPE_TARGET] = genie_scene_data.color_temperature;
        }
#endif

        GENIE_LOG_INFO("onoff:%d lightness:0x%04x color:0x%04x", genie_scene_data.onoff, genie_scene_data.lightness, genie_scene_data.color_temperature);
        sig_model_event(SIG_MODEL_EVT_ANALYZE_MSG, p_elem_state);
    }
    else
    {
        GENIE_LOG_WARN("not found scene number:%d", target_scene_number);
#ifdef CONFIG_BT_MESH_NPS_OPT
        if (BT_MESH_ADDR_IS_GROUP(p_elem_state->ctx.recv_dst))
        {
            sig_model_event(SIG_MODEL_EVT_REPORT_STATUS, p_elem_state);
        }
#endif
    }

    return 0;
}

static E_MESH_ERROR_TYPE _scene_recall_analyze(struct bt_mesh_model *p_model,
                                               u16_t src_addr, struct net_buf_simple *p_buf)
{
    u16_t scene_number = 0;
    u8_t tid = 0;
#ifdef CONFIG_MESH_MODEL_TRANS
    u8_t transition_time = 0;
    u8_t delay = 0;
#endif
    sig_model_element_state_t *p_elem = NULL;

    if (!p_model || !p_buf)
        return MESH_ANALYZE_ARGS_ERROR;

    p_elem = p_model->user_data;

    if (p_buf->len != 3 && p_buf->len != 5)
    {
        BT_ERR("MESH_ANALYZE_SIZE_ERROR buf->len(%d)", p_buf->len);
        return MESH_ANALYZE_SIZE_ERROR;
    }

    scene_number = net_buf_simple_pull_le16(p_buf);
    tid = net_buf_simple_pull_u8(p_buf);

    if (genie_transport_check_tid(src_addr, tid, p_elem->element_id) != MESH_SUCCESS)
    {
        BT_ERR("MESH_TID_REPEAT src_addr(0x%04x) tid(0x%02x)", src_addr, tid);
        return MESH_TID_REPEAT;
    }
    genie_transport_src_addr_set(src_addr);

#ifdef CONFIG_MESH_MODEL_TRANS
    if (p_buf->len == 2)
    {
        transition_time = net_buf_simple_pull_u8(p_buf);
        delay = net_buf_simple_pull_u8(p_buf);
    }
    else
    {
        transition_time = 0;
        delay = 0;
    }

    if ((transition_time & 0x3F) == 0x3F)
    {
        BT_ERR("MESH_SET_TRANSTION_ERROR");
        return MESH_SET_TRANSTION_ERROR;
    }
#endif

    p_elem->state.scene_number[TYPE_TARGET] = scene_number;
    GENIE_LOG_INFO("scene num cur(%d) tar(%d)", p_elem->state.scene_number[TYPE_PRESENT], p_elem->state.scene_number[TYPE_TARGET]);
#ifdef CONFIG_MESH_MODEL_TRANS
    p_elem->state.trans = transition_time;
    p_elem->state.delay = delay;
    if (p_elem->state.trans)
    {
        p_elem->state.trans_start_time = k_uptime_get() + p_elem->state.delay * 5;
        p_elem->state.trans_end_time = p_elem->state.trans_start_time + sig_model_transition_get_transition_time(p_elem->state.trans);
    }
    GENIE_LOG_INFO("trans(0x%02x) delay(0x%02x)", p_elem->state.trans, p_elem->state.delay);
#endif

    GENIE_LOG_INFO("light scene(0x%02x)", p_elem->state.scene_number[TYPE_TARGET]);

    return MESH_SUCCESS;
}

static void _scene_get(struct bt_mesh_model *p_model,
                       struct bt_mesh_msg_ctx *p_ctx,
                       struct net_buf_simple *p_buf)
{
    sig_model_msg msg;
    sig_model_element_state_t *p_elem_state = NULL;

    memset(&msg, 0, sizeof(sig_model_msg));
    msg.opcode = OP_GENERIC_SCENE_GET;

    if (p_buf != NULL)
    {
        msg.len = p_buf->len;
        msg.data = (u8_t *)p_buf->data;
    }
    genie_transport_src_addr_set(p_ctx->addr);

    p_elem_state = (sig_model_element_state_t *)p_model->user_data;
    msg.element_id = p_elem_state->element_id;
    sig_model_event(SIG_MODEL_EVT_GENERIC_MESG, (void *)&msg);

#ifndef CONFIG_GENIE_MESH_NO_AUTO_REPLY
    _scene_status(p_model, p_ctx, GENIE_SCENE_REGISTER_STATUS_SUCCESS, 0);
#endif
}

static void _scene_recall(struct bt_mesh_model *p_model,
                          struct bt_mesh_msg_ctx *p_ctx,
                          struct net_buf_simple *p_buf)
{
    sig_model_msg msg;
    sig_model_element_state_t *p_elem_state = NULL;
    uint8_t scene_status_code = GENIE_SCENE_REGISTER_STATUS_SUCCESS;
    genie_scene_data_t genie_scene_data;

    memset(&msg, 0, sizeof(sig_model_msg));
    msg.opcode = OP_GENERIC_SCENE_RECALL;
    if (p_buf != NULL)
    {
        msg.len = p_buf->len;
        msg.data = (u8_t *)p_buf->data;
    }

    p_elem_state = (sig_model_element_state_t *)p_model->user_data;
    E_MESH_ERROR_TYPE ret = _scene_recall_analyze(p_model, p_ctx->addr, p_buf);

    BT_DBG("ret %d", ret);

    if (ret == MESH_SUCCESS)
    {
        if (p_elem_state != NULL)
        {
            msg.element_id = p_elem_state->element_id;
        }
        sig_model_event(SIG_MODEL_EVT_GENERIC_MESG, (void *)&msg);
    }

#ifndef CONFIG_GENIE_MESH_NO_AUTO_REPLY
    if (ret == MESH_SUCCESS || ret == MESH_TID_REPEAT)
    {
        memset(&genie_scene_data, 0, sizeof(genie_scene_data_t));
        if (0 != genie_scene_setup_get_scene_data(p_elem_state->element_id, p_elem_state->state.scene_number[TYPE_TARGET], &genie_scene_data))
        {
            scene_status_code = GENIE_SCENE_REGISTER_STATUS_NOT_FOUND;
        }

#ifdef CONFIG_BT_MESH_NPS_OPT
        if (p_elem_state != NULL)
        {
            p_elem_state->ctx.recv_dst = p_ctx->recv_dst;
            p_elem_state->ctx.addr = p_ctx->addr;
            p_elem_state->ctx.app_idx = p_ctx->app_idx;
            p_elem_state->ctx.op_time_ms = k_uptime_get_32();

            if (genie_transport_find_by_opcode(OP_GENERIC_ONOFF_STATUS) == 1)
            {
                p_elem_state->ctx.op_status = 0;
            }
            else
            {
                p_elem_state->ctx.op_status = OP_GENERIC_SCENE_STATUS;
                p_elem_state->ctx.scene_status_code = scene_status_code;

                if (ret == MESH_SUCCESS)
                {
                    genie_transport_remove_by_opid(SIG_MODEL_OP_DATA_FLAG);
                }
            }
        }

        if (!BT_MESH_ADDR_IS_GROUP(p_ctx->recv_dst))
        {
            _scene_status(p_model, p_ctx, scene_status_code, 1);
        }
#else
        _scene_status(p_model, p_ctx, scene_status_code, 1);
#endif
    }
#endif

    if (ret == MESH_SUCCESS)
    {
        genie_scene_recall_update(p_elem_state, p_elem_state->state.scene_number[TYPE_TARGET]);
    }
}

static void _scene_recall_unack(struct bt_mesh_model *p_model,
                                struct bt_mesh_msg_ctx *p_ctx,
                                struct net_buf_simple *p_buf)
{
    sig_model_msg msg;
    sig_model_element_state_t *p_elem_state = NULL;

    memset(&msg, 0, sizeof(sig_model_msg));
    msg.opcode = OP_GENERIC_SCENE_RECALL_UNACK;
    if (p_buf != NULL)
    {
        msg.len = p_buf->len;
        msg.data = (u8_t *)p_buf->data;
    }

    E_MESH_ERROR_TYPE ret = _scene_recall_analyze(p_model, p_ctx->addr, p_buf);

    if (ret == MESH_SUCCESS)
    {
        p_elem_state = (sig_model_element_state_t *)p_model->user_data;
        if (p_elem_state != NULL)
        {
            msg.element_id = p_elem_state->element_id;
#ifdef CONFIG_BT_MESH_NPS_OPT
            p_elem_state->ctx.op_status = 0;
            p_elem_state->ctx.recv_dst = p_ctx->recv_dst;
            p_elem_state->ctx.addr = p_ctx->addr;
            p_elem_state->ctx.app_idx = p_ctx->app_idx;
#endif
            sig_model_event(SIG_MODEL_EVT_GENERIC_MESG, (void *)&msg);
            genie_scene_recall_update(p_elem_state, p_elem_state->state.scene_number[TYPE_TARGET]);
        }
    }
}

static void _scene_register_get(struct bt_mesh_model *p_model,
                                struct bt_mesh_msg_ctx *p_ctx,
                                struct net_buf_simple *p_buf)
{
    sig_model_msg msg;
    sig_model_element_state_t *p_elem_state = NULL;

    memset(&msg, 0, sizeof(sig_model_msg));
    msg.opcode = OP_GENERIC_SCENE_REGISTER_GET;

    if (p_buf != NULL)
    {
        msg.len = p_buf->len;
        msg.data = (u8_t *)p_buf->data;
    }
    genie_transport_src_addr_set(p_ctx->addr);

    p_elem_state = (sig_model_element_state_t *)p_model->user_data;
    msg.element_id = p_elem_state->element_id;
    sig_model_event(SIG_MODEL_EVT_GENERIC_MESG, (void *)&msg);

#ifndef CONFIG_GENIE_MESH_NO_AUTO_REPLY
#ifdef CONFIG_BT_MESH_NPS_OPT
    if (!BT_MESH_ADDR_IS_GROUP(p_ctx->recv_dst))
    {
        genie_scene_register_status(p_model, p_ctx, GENIE_SCENE_REGISTER_STATUS_SUCCESS, true);
    }
    else
    {
        if (p_elem_state != NULL)
        {
            p_elem_state->ctx.op_status = OP_GENERIC_SCENE_REGISTER_STATUS;
            p_elem_state->ctx.recv_dst = p_ctx->recv_dst;
            p_elem_state->ctx.addr = p_ctx->addr;
            p_elem_state->ctx.app_idx = p_ctx->app_idx;
            p_elem_state->ctx.all_scene_register = true;
            p_elem_state->ctx.scene_status_code = GENIE_SCENE_REGISTER_STATUS_SUCCESS;
            p_elem_state->ctx.op_time_ms = k_uptime_get_32();

            sig_model_event(SIG_MODEL_EVT_ANALYZE_MSG, p_elem_state);
        }
    }
#else
    genie_scene_register_status(p_model, p_ctx, GENIE_SCENE_REGISTER_STATUS_SUCCESS, true);
#endif
#endif
}

const struct bt_mesh_model_op g_scene_op[] = {
    {OP_GENERIC_SCENE_GET, 0, _scene_get},
    {OP_GENERIC_SCENE_RECALL, 3, _scene_recall},
    {OP_GENERIC_SCENE_RECALL_UNACK, 3, _scene_recall_unack},
    {OP_GENERIC_SCENE_REGISTER_GET, 0, _scene_register_get},

    BT_MESH_MODEL_OP_END,
};
