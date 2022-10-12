/*
 * Copyright (C) 2018-2021 Alibaba Group Holding Limited
 */

#define BT_DBG_ENABLED IS_ENABLED(CONFIG_BT_MESH_DEBUG_MODEL)
#include "genie_mesh_internal.h"

struct bt_mesh_model_pub g_scene_setup_pub = {};

static genie_scene_data_t genie_scene_data[MESH_SIGMODEL_ELEM_COUNT][GENIE_SCENE_NUMBER_MAX] = {0};

static int _scene_setup_load_preset_data(void);

static int _is_scene_data_exist(uint8_t element_id, uint16_t scene_number)
{
    int index = 0;

    for (index = 0; index < GENIE_SCENE_NUMBER_MAX; index++)
    {
        if (genie_scene_data[element_id][index].occupied && genie_scene_data[element_id][index].scene_number == scene_number && scene_number > GENIE_SCENE_INVALID_NUMBER)
        {
            return index;
        }
    }

    return -1;
}

static int scene_find_position(uint8_t element_id)
{
    int index = 0;

    for (index = 0; index < GENIE_SCENE_NUMBER_MAX; index++)
    {
        if (!genie_scene_data[element_id][index].occupied)
        {
            return index;
        }
    }

    return -1;
}

static int _scene_setup_store_data(void)
{
    genie_storage_write_userdata(GFI_MESH_SCENE_DATA, (uint8_t *)genie_scene_data, GENIE_SCENE_STORE_MAX * sizeof(genie_scene_data_t));

    return 0;
}

int genie_scene_setup_load_data(void)
{
    memset(genie_scene_data, 0, sizeof(genie_scene_data));

    genie_storage_read_userdata(GFI_MESH_SCENE_DATA, (uint8_t *)genie_scene_data, GENIE_SCENE_STORE_MAX * sizeof(genie_scene_data_t));

    _scene_setup_load_preset_data();

    return 0;
}

int genie_scene_setup_get_scene_data(uint8_t element_id, uint16_t scene_number, genie_scene_data_t *p_scene_data)
{
    int index = -1;

    if (!p_scene_data || scene_number == 0)
    {
        return -1;
    }

    index = _is_scene_data_exist(element_id, scene_number);
    if (index >= 0)
    {
        memcpy(p_scene_data, &genie_scene_data[element_id][index], sizeof(genie_scene_data_t));
        return 0;
    }

    return -1;
}

static int do_scene_setup_store(uint8_t element_id, genie_scene_data_t *p_scene_data)
{
    int index = -1;

    index = _is_scene_data_exist(element_id, p_scene_data->scene_number);
    if (index != -1)
    {
        if (memcmp(&genie_scene_data[element_id][index], p_scene_data, sizeof(genie_scene_data_t)))
        {
            memcpy(&genie_scene_data[element_id][index], p_scene_data, sizeof(genie_scene_data_t));
            if (_scene_setup_store_data() == 0)
            {
                return GENIE_SCENE_REGISTER_STATUS_SUCCESS;
            }
        }
        else
        {
            GENIE_LOG_WARN("scene info dup");
            return GENIE_SCENE_REGISTER_STATUS_SUCCESS;
        }
    }
    else
    {
        index = scene_find_position(element_id);
        GENIE_LOG_ERR("scene store pos:%d", index);
        if (index == -1) //is full
        {
            GENIE_LOG_ERR("scene store is full");
            return GENIE_SCENE_REGISTER_STATUS_FULL;
        }
        else
        {
            memcpy(&genie_scene_data[element_id][index], p_scene_data, sizeof(genie_scene_data_t));
            if (_scene_setup_store_data() == 0)
            {
                return GENIE_SCENE_REGISTER_STATUS_SUCCESS;
            }
        }
    }

    return GENIE_SCENE_REGISTER_STATUS_UNKOWN;
}

static int _scene_setup_store_analyze(struct bt_mesh_model *p_model, u16_t src_addr, struct net_buf_simple *p_buf)
{
    uint8_t msglen = 0;
    genie_scene_data_t scene_data = {0};
    sig_model_element_state_t *p_elem = NULL;

    if (!p_model || !p_buf)
    {
        return GENIE_SCENE_REGISTER_STATUS_UNKOWN;
    }

    p_elem = p_model->user_data;

    msglen = p_buf->len;
    if (msglen < 2 || msglen > 8)
    {
        BT_ERR("scene store len(%d) outside", msglen);
        return GENIE_SCENE_REGISTER_STATUS_INVALID;
    }

    genie_transport_src_addr_set(src_addr);

    memset(&scene_data, 0, sizeof(genie_scene_data_t));

    scene_data.scene_number = net_buf_simple_pull_le16(p_buf);

    if (msglen > 2)
    {
        scene_data.onoff = net_buf_simple_pull_u8(p_buf);
    }

    if (msglen > 4)
    {
        scene_data.lightness = net_buf_simple_pull_le16(p_buf);
    }

    if (msglen > 6)
    {
        scene_data.color_temperature = net_buf_simple_pull_le16(p_buf);
    }

    scene_data.occupied = 1;

    GENIE_LOG_INFO("scene(0x%04x)", scene_data.scene_number);
    GENIE_LOG_INFO("scene onoff(0x%02x)", scene_data.onoff);
    GENIE_LOG_INFO("scene lightness(0x%04x)", scene_data.lightness);
    GENIE_LOG_INFO("scene temperature(0x%04x)", scene_data.color_temperature);

    return do_scene_setup_store(p_elem->element_id, &scene_data);
}

static void _scene_setup_store_unack(struct bt_mesh_model *p_model,
                                     struct bt_mesh_msg_ctx *p_ctx,
                                     struct net_buf_simple *p_buf)
{
    int status_code = 0;
    sig_model_msg msg;
    sig_model_element_state_t *p_elem_state = NULL;

    memset(&msg, 0, sizeof(sig_model_msg));
    msg.opcode = OP_GENERIC_SCENE_STORE;
    if (p_buf != NULL)
    {
        msg.len = p_buf->len;
        msg.data = (u8_t *)p_buf->data;
    }

    p_elem_state = (sig_model_element_state_t *)p_model->user_data;
    status_code = _scene_setup_store_analyze(p_model, p_ctx->addr, p_buf);

    if (status_code != GENIE_SCENE_REGISTER_STATUS_UNKOWN)
    {
        if (p_elem_state != NULL)
        {
            msg.element_id = p_elem_state->element_id;
        }
        sig_model_event(SIG_MODEL_EVT_GENERIC_MESG, (void *)&msg);
    }
}

static void _scene_setup_store(struct bt_mesh_model *p_model,
                               struct bt_mesh_msg_ctx *p_ctx,
                               struct net_buf_simple *p_buf)
{
    int status_code = 0;
    sig_model_msg msg;
    sig_model_element_state_t *p_elem_state = NULL;

    memset(&msg, 0, sizeof(sig_model_msg));
    msg.opcode = OP_GENERIC_SCENE_STORE;
    if (p_buf != NULL)
    {
        msg.len = p_buf->len;
        msg.data = (u8_t *)p_buf->data;
    }

    p_elem_state = (sig_model_element_state_t *)p_model->user_data;
    status_code = _scene_setup_store_analyze(p_model, p_ctx->addr, p_buf);

    if (status_code != GENIE_SCENE_REGISTER_STATUS_UNKOWN)
    {
        if (p_elem_state != NULL)
        {
            msg.element_id = p_elem_state->element_id;
        }
        sig_model_event(SIG_MODEL_EVT_GENERIC_MESG, (void *)&msg);
    }

#ifndef CONFIG_GENIE_MESH_NO_AUTO_REPLY
#ifdef CONFIG_BT_MESH_NPS_OPT
    if (!BT_MESH_ADDR_IS_GROUP(p_ctx->recv_dst))
    {
        genie_scene_register_status(p_model, p_ctx, status_code, false);
    }
    else
    {
        if (p_elem_state != NULL)
        {
            p_elem_state->ctx.op_status = OP_GENERIC_SCENE_REGISTER_STATUS;
            p_elem_state->ctx.recv_dst = p_ctx->recv_dst;
            p_elem_state->ctx.addr = p_ctx->addr;
            p_elem_state->ctx.app_idx = p_ctx->app_idx;
            p_elem_state->ctx.all_scene_register = false;
            p_elem_state->ctx.scene_status_code = status_code;
            p_elem_state->ctx.op_time_ms = k_uptime_get_32();

            sig_model_event(SIG_MODEL_EVT_ANALYZE_MSG, p_elem_state);
        }
    }
#else
    genie_scene_register_status(p_model, p_ctx, status_code, false);
#endif
#endif
}

static int do_scene_setup_delete(uint8_t element_id, uint16_t scene_number)
{
    int index = -1;

    index = _is_scene_data_exist(element_id, scene_number);
    if (index != -1)
    {
        memset(&genie_scene_data[element_id][index], 0, sizeof(genie_scene_data_t));
        _scene_setup_store_data();
    }

    return GENIE_SCENE_REGISTER_STATUS_SUCCESS;
}

static int _scene_setup_delete_analyze(struct bt_mesh_model *p_model,
                                       u16_t src_addr, struct net_buf_simple *p_buf)
{
    u16_t scene_number = 0;
    sig_model_element_state_t *p_elem = NULL;

    if (!p_model || !p_buf)
    {
        return GENIE_SCENE_REGISTER_STATUS_UNKOWN;
    }

    p_elem = (sig_model_element_state_t *)p_model->user_data;

    if (p_buf->len < 2 && p_buf->len > 3)
    {
        BT_ERR("scene store len(%d) outside", p_buf->len);
        return GENIE_SCENE_REGISTER_STATUS_INVALID;
    }

    genie_transport_src_addr_set(src_addr);
    scene_number = net_buf_simple_pull_le16(p_buf);

    GENIE_LOG_INFO("del scene(0x%02x)", scene_number);

    return do_scene_setup_delete(p_elem->element_id, scene_number);
}

static void _scene_setup_delete_unack(struct bt_mesh_model *p_model,
                                      struct bt_mesh_msg_ctx *p_ctx,
                                      struct net_buf_simple *p_buf)
{
    sig_model_msg msg;
    genie_scene_register_status_e status_code = GENIE_SCENE_REGISTER_STATUS_SUCCESS;
    sig_model_element_state_t *p_elem_state = NULL;

    memset(&msg, 0, sizeof(sig_model_msg));
    msg.opcode = OP_GENERIC_SCENE_STORE;
    if (p_buf != NULL)
    {
        msg.len = p_buf->len;
        msg.data = (u8_t *)p_buf->data;
    }

    p_elem_state = (sig_model_element_state_t *)p_model->user_data;
    status_code = _scene_setup_delete_analyze(p_model, p_ctx->addr, p_buf);

    if (status_code != GENIE_SCENE_REGISTER_STATUS_UNKOWN)
    {
        if (p_elem_state != NULL)
        {
            msg.element_id = p_elem_state->element_id;
        }

        sig_model_event(SIG_MODEL_EVT_GENERIC_MESG, (void *)&msg);
    }
}

static void _scene_setup_delete(struct bt_mesh_model *p_model,
                                struct bt_mesh_msg_ctx *p_ctx,
                                struct net_buf_simple *p_buf)
{
    sig_model_msg msg;
    genie_scene_register_status_e status_code = GENIE_SCENE_REGISTER_STATUS_SUCCESS;
    sig_model_element_state_t *p_elem_state = NULL;

    memset(&msg, 0, sizeof(sig_model_msg));
    msg.opcode = OP_GENERIC_SCENE_STORE;
    if (p_buf != NULL)
    {
        msg.len = p_buf->len;
        msg.data = (u8_t *)p_buf->data;
    }

    p_elem_state = (sig_model_element_state_t *)p_model->user_data;
    status_code = _scene_setup_delete_analyze(p_model, p_ctx->addr, p_buf);

    if (status_code != GENIE_SCENE_REGISTER_STATUS_UNKOWN)
    {
        if (p_elem_state != NULL)
        {
            msg.element_id = p_elem_state->element_id;
        }
        sig_model_event(SIG_MODEL_EVT_GENERIC_MESG, (void *)&msg);
    }

#ifndef CONFIG_GENIE_MESH_NO_AUTO_REPLY
#ifdef CONFIG_BT_MESH_NPS_OPT
    if (!BT_MESH_ADDR_IS_GROUP(p_ctx->recv_dst))
    {
        genie_scene_register_status(p_model, p_ctx, status_code, false);
    }
    else
    {
        if (p_elem_state != NULL)
        {
            p_elem_state->ctx.op_status = OP_GENERIC_SCENE_REGISTER_STATUS;
            p_elem_state->ctx.recv_dst = p_ctx->recv_dst;
            p_elem_state->ctx.addr = p_ctx->addr;
            p_elem_state->ctx.app_idx = p_ctx->app_idx;
            p_elem_state->ctx.all_scene_register = false;
            p_elem_state->ctx.scene_status_code = status_code;
            p_elem_state->ctx.op_time_ms = k_uptime_get_32();

            sig_model_event(SIG_MODEL_EVT_ANALYZE_MSG, p_elem_state);
        }
    }
#else
    genie_scene_register_status(p_model, p_ctx, status_code, false);
#endif
#endif
}

int genie_scene_add_registered_scene_number(uint8_t element_id, struct net_buf_simple *p_msg)
{
    int index = 0;

    for (index = 0; index < GENIE_SCENE_NUMBER_MAX; index++)
    {
        if (genie_scene_data[element_id][index].occupied && genie_scene_data[element_id][index].scene_number > GENIE_SCENE_INVALID_NUMBER)
        {
            net_buf_simple_add_le16(p_msg, genie_scene_data[element_id][index].scene_number);
        }
    }

    return 0;
}

int genie_scene_packet_registered_data(uint8_t element_id, uint8_t *payload, uint8_t payload_len)
{
    int index = 0;
    int packeted_len = 0;

    for (index = 0; index < GENIE_SCENE_NUMBER_MAX; index++)
    {
        if (genie_scene_data[element_id][index].occupied && genie_scene_data[element_id][index].scene_number > GENIE_SCENE_INVALID_NUMBER)
        {
            *(payload + packeted_len) = genie_scene_data[element_id][index].scene_number & 0xFF;
            packeted_len += 1;
            if (packeted_len >= payload_len)
            {
                break;
            }
            *(payload + packeted_len) = (genie_scene_data[element_id][index].scene_number >> 8) & 0xFF;
            packeted_len += 1;
            if (packeted_len >= payload_len)
            {
                break;
            }
        }
    }

    return packeted_len;
}

const struct bt_mesh_model_op g_scene_setup_op[] = {
    {OP_GENERIC_SCENE_STORE, 2, _scene_setup_store},
    {OP_GENERIC_SCENE_STORE_UNACK, 2, _scene_setup_store_unack},
    {OP_GENERIC_SCENE_DELETE, 2, _scene_setup_delete},
    {OP_GENERIC_SCENE_DELETE_UNACK, 2, _scene_setup_delete_unack},

    BT_MESH_MODEL_OP_END,
};

static int _scene_setup_load_preset_data(void)
{
    if (GENIE_SCENE_PRESET_STORE_MAX > 0)
    {
        genie_scene_data[0][GENIE_SCENE_STORE_MAX].occupied = 1;
        genie_scene_data[0][GENIE_SCENE_STORE_MAX].scene_number = MODE_1_SCENE_NUMBER;
        genie_scene_data[0][GENIE_SCENE_STORE_MAX].onoff = MODE_1_ONOFF;
        genie_scene_data[0][GENIE_SCENE_STORE_MAX].lightness = MODE_1_LIGHTNESS;
        genie_scene_data[0][GENIE_SCENE_STORE_MAX].color_temperature = MODE_1_COLOR_TEMPERATURE;
    }

    if (GENIE_SCENE_PRESET_STORE_MAX > 1)
    {
        genie_scene_data[0][GENIE_SCENE_STORE_MAX + 1].occupied = 1;
        genie_scene_data[0][GENIE_SCENE_STORE_MAX + 1].scene_number = MODE_2_SCENE_NUMBER;  /* Special scene, turn off */
        genie_scene_data[0][GENIE_SCENE_STORE_MAX + 1].onoff = MODE_2_ONOFF;
        //genie_scene_data[0][GENIE_SCENE_STORE_MAX + 1].lightness = MODE_2_LIGHTNESS;  /* Only Off,don't change lightness */
        //genie_scene_data[0][GENIE_SCENE_STORE_MAX + 1].color_temperature = MODE_2_COLOR_TEMPERATURE;  /* Only Off,don't change color temperature */
    }

    if (GENIE_SCENE_PRESET_STORE_MAX > 2)
    {
        genie_scene_data[0][GENIE_SCENE_STORE_MAX + 2].occupied = 1;
        genie_scene_data[0][GENIE_SCENE_STORE_MAX + 2].scene_number = MODE_3_SCENE_NUMBER;
        genie_scene_data[0][GENIE_SCENE_STORE_MAX + 2].onoff = MODE_3_ONOFF;
        genie_scene_data[0][GENIE_SCENE_STORE_MAX + 2].lightness = MODE_3_LIGHTNESS;
        genie_scene_data[0][GENIE_SCENE_STORE_MAX + 2].color_temperature = MODE_3_COLOR_TEMPERATURE;
    }

    if (GENIE_SCENE_PRESET_STORE_MAX > 3)
    {
        genie_scene_data[0][GENIE_SCENE_STORE_MAX + 3].occupied = 1;
        genie_scene_data[0][GENIE_SCENE_STORE_MAX + 3].scene_number = MODE_4_SCENE_NUMBER;
        genie_scene_data[0][GENIE_SCENE_STORE_MAX + 3].onoff = MODE_4_ONOFF;
        genie_scene_data[0][GENIE_SCENE_STORE_MAX + 3].lightness = MODE_4_LIGHTNESS;
        genie_scene_data[0][GENIE_SCENE_STORE_MAX + 3].color_temperature = MODE_4_COLOR_TEMPERATURE;
    }

    if (GENIE_SCENE_PRESET_STORE_MAX > 4)
    {
        genie_scene_data[0][GENIE_SCENE_STORE_MAX + 4].occupied = 1;
        genie_scene_data[0][GENIE_SCENE_STORE_MAX + 4].scene_number = MODE_5_SCENE_NUMBER;
        genie_scene_data[0][GENIE_SCENE_STORE_MAX + 4].onoff = MODE_5_ONOFF;
        genie_scene_data[0][GENIE_SCENE_STORE_MAX + 4].lightness = MODE_5_LIGHTNESS;
        genie_scene_data[0][GENIE_SCENE_STORE_MAX + 4].color_temperature = MODE_5_COLOR_TEMPERATURE;
    }

    if (GENIE_SCENE_PRESET_STORE_MAX > 5)
    {
        genie_scene_data[0][GENIE_SCENE_STORE_MAX + 5].occupied = 1;
        genie_scene_data[0][GENIE_SCENE_STORE_MAX + 5].scene_number = MODE_6_SCENE_NUMBER;
        genie_scene_data[0][GENIE_SCENE_STORE_MAX + 5].onoff = MODE_6_ONOFF;
        genie_scene_data[0][GENIE_SCENE_STORE_MAX + 5].lightness = MODE_6_LIGHTNESS;
        genie_scene_data[0][GENIE_SCENE_STORE_MAX + 5].color_temperature = MODE_6_COLOR_TEMPERATURE;
    }

    if (GENIE_SCENE_PRESET_STORE_MAX > 6)
    {
        genie_scene_data[0][GENIE_SCENE_STORE_MAX + 6].occupied = 1;
        genie_scene_data[0][GENIE_SCENE_STORE_MAX + 6].scene_number = MODE_7_SCENE_NUMBER;
        genie_scene_data[0][GENIE_SCENE_STORE_MAX + 6].onoff = MODE_7_ONOFF;
        genie_scene_data[0][GENIE_SCENE_STORE_MAX + 6].lightness = MODE_7_LIGHTNESS;
        genie_scene_data[0][GENIE_SCENE_STORE_MAX + 6].color_temperature = MODE_7_COLOR_TEMPERATURE;
    }

    if (GENIE_SCENE_PRESET_STORE_MAX > 7)
    {
        genie_scene_data[0][GENIE_SCENE_STORE_MAX + 7].occupied = 1;
        genie_scene_data[0][GENIE_SCENE_STORE_MAX + 7].scene_number = MODE_8_SCENE_NUMBER;
        genie_scene_data[0][GENIE_SCENE_STORE_MAX + 7].onoff = MODE_8_ONOFF;
        genie_scene_data[0][GENIE_SCENE_STORE_MAX + 7].lightness = MODE_8_LIGHTNESS;
        genie_scene_data[0][GENIE_SCENE_STORE_MAX + 7].color_temperature = MODE_8_COLOR_TEMPERATURE;
    }

    if (GENIE_SCENE_PRESET_STORE_MAX > 8)
    {
        genie_scene_data[0][GENIE_SCENE_STORE_MAX + 8].occupied = 1;
        genie_scene_data[0][GENIE_SCENE_STORE_MAX + 8].scene_number = MODE_9_SCENE_NUMBER;
        genie_scene_data[0][GENIE_SCENE_STORE_MAX + 8].onoff = MODE_9_ONOFF;
        genie_scene_data[0][GENIE_SCENE_STORE_MAX + 8].lightness = MODE_9_LIGHTNESS;
        genie_scene_data[0][GENIE_SCENE_STORE_MAX + 8].color_temperature = MODE_9_COLOR_TEMPERATURE;
    }

    return 0;
}
