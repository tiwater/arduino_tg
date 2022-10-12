#ifndef __SIG_MODEL_H__
#define __SIG_MODEL_H__

#define ATTR_TYPE_GENERIC_ONOFF 0x0100
#define ATTR_TYPE_LIGHTNESS 0x0121
#define ATTR_TYPE_COLOR_TEMPERATURE 0x0122
#define ATTR_TYPE_SENCE 0xF004
#define ATTR_TYPE_ADJ_STEP 0xF00B
#define ATTR_TYPE_LIGHT_PREVIEW 0xF030

#define SIG_MODEL_OP_DATA_FLAG 0xFF

#define MESH_SIGMODEL_ELEM_COUNT (1)

typedef enum
{
    TYPE_PRESENT = 0,
    TYPE_TARGET,
    TYPE_NUM,
} E_VALUE_TYPE;

typedef struct
{
#ifdef CONFIG_MESH_MODEL_GEN_ONOFF_SRV
    u8_t onoff[TYPE_NUM];
#endif

#ifdef CONFIG_MESH_MODEL_LIGHTNESS_SRV
    u16_t lightness[TYPE_NUM];
#endif

#ifdef CONFIG_MESH_MODEL_CTL_SRV
    u16_t color_temperature[TYPE_NUM];
#endif

#ifdef CONFIG_MESH_MODEL_SCENE_SRV
    u16_t scene_number[TYPE_NUM];
#endif

    u8_t delay; //unit:5ms
    struct k_timer delay_timer;
#ifdef CONFIG_MESH_MODEL_TRANS
    u8_t trans; //unit:100ms

    u32_t trans_start_time;
    u32_t trans_end_time;

    struct k_timer trans_timer;
#endif
} sig_model_state_t;

typedef struct
{
#ifdef CONFIG_MESH_MODEL_GEN_ONOFF_SRV
    u8_t last_onoff;
#endif
#ifdef CONFIG_MESH_MODEL_LIGHTNESS_SRV
    u16_t last_lightness;
#endif
#ifdef CONFIG_MESH_MODEL_CTL_SRV
    u16_t last_color_temperature;
#endif
#ifdef CONFIG_MESH_MODEL_SCENE_SRV
    u16_t last_scene_number;
#endif
} __packed sig_model_powerup_t;

#ifdef CONFIG_BT_MESH_NPS_OPT
typedef struct
{
    u16_t op_status;
    /** AppKey Index to encrypt the message with. */
    u16_t app_idx;
    /** Remote address. */
    u16_t addr;
    /** Destination address of a received message */
    u16_t recv_dst;
    /* op timestamp */
    u32_t op_time_ms;
    /** is need packet all registered scene number */
    bool all_scene_register;
    /** registered status code */
    u8_t scene_status_code;
} sig_model_ctx_t;
#endif

typedef struct
{
    u8_t element_id;
    sig_model_state_t state;
    sig_model_powerup_t powerup;
#ifdef CONFIG_BT_MESH_NPS_OPT
    sig_model_ctx_t ctx;
#endif
} sig_model_element_state_t;

#ifdef CONFIG_MESH_MODEL_GEN_ONOFF_SRV
#include "sig_model_onoff_srv.h"
#include "sig_model_bind_ops.h"
#endif
#ifdef CONFIG_MESH_MODEL_LIGHTNESS_SRV
#include "sig_model_lightness_srv.h"
#endif
#ifdef CONFIG_MESH_MODEL_CTL_SRV
#include "sig_model_light_ctl_srv.h"
#endif

#ifdef CONFIG_MESH_MODEL_SCENE_SRV
#include "sig_model_scene.h"
#endif

#include "sig_model_transition.h"
#include "sig_model_opcode.h"
#include "sig_model_event.h"

#endif
