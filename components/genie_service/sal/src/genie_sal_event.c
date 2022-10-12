/*
 * Copyright (C) 2019-2020 Alibaba Group Holding Limited
 */

#include "genie_mesh_internal.h"

#ifdef CONFIG_GENIE_MESH_PORT_YOC
#include "port/mesh_event_port.h"

static void genie_mesh_model_event_cb(mesh_model_event_e event, void *p_arg)
{
    genie_event_e genie_evt;
    switch (event) {
    case BT_MESH_MODEL_EVT_PROV_START:
    {
        genie_evt = GENIE_EVT_SDK_MESH_PROV_START;
    }
    break;
    case BT_MESH_MODEL_EVT_PROV_DATA:
    {
        genie_evt = GENIE_EVT_SDK_MESH_PROV_DATA;
    }
    break;

    case BT_MESH_MODEL_EVT_APPKEY_ADD:
    {
        genie_mesh_load_group_addr();
        genie_evt = GENIE_EVT_SDK_APPKEY_ADD;
    }
    break;

    case BT_MESH_MODEL_EVT_APPKEY_UPDATE:
    {
        genie_evt = GENIE_EVT_SDK_APPKEY_UPDATE;
    }
    break;

    case BT_MESH_MODEL_EVT_APPKEY_DEL:
    {
        genie_evt = GENIE_EVT_SDK_APPKEY_DEL;
    }
    break;

    case BT_MESH_MODEL_EVT_NETKEY_ADD:
    {
        genie_evt = GENIE_EVT_SDK_NETKEY_ADD;
    }
    break;

    case BT_MESH_MODEL_EVT_NETKEY_UPDATE:
    {
        genie_evt = GENIE_EVT_SDK_NETKEY_UPDATE;
    }
    break;

    case BT_MESH_MODEL_EVT_NETKEY_DEL:
    {
        genie_evt = GENIE_EVT_SDK_NETKEY_DEL;
    }
    break;

    case BT_MESH_MODEL_EVT_NODE_RESET_OP:
    {
        genie_evt = GENIE_EVT_SW_RESET;
    }
    break;
#if 0
    case BT_MESH_MODEL_EVT_SEQ_UPDATE:
    {
        genie_evt = GENIE_EVT_SDK_SEQ_UPDATE;
    }
    break;
#endif
    default:
        return;
    }

    return genie_event(genie_evt, p_arg);
}

int genie_sal_event_init(void)
{
    return bt_mesh_event_register(genie_mesh_model_event_cb);
}
#endif
