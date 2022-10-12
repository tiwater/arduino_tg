/*
 * Copyright (C) 2018-2020 Alibaba Group Holding Limited
 */

#ifndef __GENIE_MODEL_SCENE_H__
#define __GENIE_MODEL_SCENE_H__

#include "sig_model_scene_data.h"

#define GENIE_SCENE_PRESET_STORE_MAX (9)
#define GENIE_SCENE_STORE_MAX (24)
#define GENIE_SCENE_NUMBER_MAX (GENIE_SCENE_STORE_MAX + GENIE_SCENE_PRESET_STORE_MAX)

#define GENIE_SCENE_INVALID_NUMBER (0)

typedef enum _genie_scene_e
{
    GENIE_SCENE_NORMAL,
    GENIE_SCENE_READ = 3,
    GENIE_SCENE_CINEMA,
    GENIE_SCENE_WARM,
    GENIE_SCENE_NIGHT
} genie_scene_e;

typedef enum _genie_scene_register_status_e
{
    GENIE_SCENE_REGISTER_STATUS_SUCCESS,
    GENIE_SCENE_REGISTER_STATUS_FULL,
    GENIE_SCENE_REGISTER_STATUS_NOT_FOUND,
    GENIE_SCENE_REGISTER_STATUS_INVALID,
    GENIE_SCENE_REGISTER_STATUS_UNKOWN,
} genie_scene_register_status_e;

typedef struct _genie_scene_data_s
{
    uint8_t occupied;
    uint8_t onoff;
    uint16_t scene_number;
    uint16_t lightness;
    uint16_t color_temperature;
} __packed genie_scene_data_t;

#define SCENE_DEFAULT GENIE_SCENE_NORMAL

extern struct bt_mesh_model_pub g_scene_pub;
extern const struct bt_mesh_model_op g_scene_op[];
#define MESH_MODEL_SCENE_SRV(_user_data) BT_MESH_MODEL(BT_MESH_MODEL_ID_SCENE_SRV, g_scene_op, &g_scene_pub, _user_data)

extern struct bt_mesh_model_pub g_scene_setup_pub;
extern const struct bt_mesh_model_op g_scene_setup_op[];
#define MESH_MODEL_SCENE_SETUP_SRV(_user_data) BT_MESH_MODEL(BT_MESH_MODEL_ID_SCENE_SETUP_SRV, g_scene_setup_op, &g_scene_setup_pub, _user_data)

extern void genie_scene_register_status(struct bt_mesh_model *p_model, struct bt_mesh_msg_ctx *p_ctx, uint8_t status_code, bool is_all);
extern int genie_scene_setup_load_data(void);
extern int genie_scene_add_registered_scene_number(uint8_t element_id, struct net_buf_simple *p_msg);
extern int genie_scene_packet_registered_data(uint8_t element_id, uint8_t *payload, uint8_t payload_len);
extern int genie_scene_setup_get_scene_data(uint8_t element_id, uint16_t scene_number, genie_scene_data_t *p_scene_data);

#endif
