#ifndef __SIG_MODEL_EVENT_H__
#define __SIG_MODEL_EVENT_H__

#define SIG_MODEL_INDICATE_PAYLOAD_MAX_LEN (23) //23:Two segments

#ifdef CONFIG_BT_MESH_NPS_OPT
typedef enum _report_flag
{
    SIG_MODEL_REPORT_STATUS_ONE,
    SIG_MODEL_REPORT_STATUS_TWO,
    SIG_MODEL_REPORT_STATUS_HB,
    SIG_MODEL_REPORT_OTHER_DATA,
    SIG_MODEL_REPORT_FLAGS
} sig_model_report_flag_e;
#endif

typedef enum _indicate_flag
{
    SIG_MODEL_INDICATE_GEN_ONOFF,
    SIG_MODEL_INDICATE_GEN_LIGHTNESS,
    SIG_MODEL_INDICATE_GEN_CTL,
    SIG_MODEL_INDICATE_GEN_SCENE,
    SIG_MODEL_INDICATE_FLAGS
} sig_model_indicate_flag_e;

typedef enum
{
    SIG_MODEL_EVT_NONE = 0,
    SIG_MODEL_EVT_ANALYZE_MSG,
    SIG_MODEL_EVT_TIME_OUT,
    SIG_MODEL_EVT_DOWN_MSG,
    SIG_MODEL_EVT_ACTION_DONE,
    SIG_MODEL_EVT_INDICATE,

#ifdef CONFIG_BT_MESH_NPS_OPT
    SIG_MODEL_EVT_REPORT_STATUS,
#endif

    SIG_MODEL_EVT_DELAY_START = 10,
    SIG_MODEL_EVT_DELAY_END,

#ifdef CONFIG_MESH_MODEL_TRANS
    SIG_MODEL_EVT_TRANS_START,
    SIG_MODEL_EVT_TRANS_CYCLE,
    SIG_MODEL_EVT_TRANS_END,
#endif

    SIG_MODEL_EVT_GENERIC_MESG = 20,
} sig_model_event_e;

void sig_model_event_set_indicate(int indicate);

void sig_model_event(sig_model_event_e event, void *p_arg);

#ifdef CONFIG_BT_MESH_NPS_OPT
void sig_model_report_poweron_status(sig_model_element_state_t *p_elem);
#endif

#endif
