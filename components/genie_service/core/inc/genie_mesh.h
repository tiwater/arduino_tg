/*
 * Copyright (C) 2018-2020 Alibaba Group Holding Limited
 */

#ifndef __GENIE_MESH_H__
#define __GENIE_MESH_H__

#define CONFIG_MESH_VENDOR_COMPANY_ID (0x01A8)
#define CONFIG_MESH_VENDOR_MODEL_SRV 0x0000
#define CONFIG_MESH_VENDOR_MODEL_CLI 0x0001

#define GENIE_MESH_INIT_PHARSE_II_HW_RESET_DELAY (2000) //Unit:ms
#ifdef CONFIG_BT_MESH_NPS_OPT
#define GENIE_MESH_INIT_PROVISIONED_DELAY_START_MIN (8 * 1000)  //Unit:ms
#define GENIE_MESH_INIT_PROVISIONED_DELAY_START_MAX (10 * 1000) //Unit:ms
#else
#define GENIE_MESH_INIT_PROVISIONED_DELAY_START_MIN (3 * 1000) //Unit:ms
#define GENIE_MESH_INIT_PROVISIONED_DELAY_START_MAX (6 * 1000) //Unit:ms
#endif

#ifdef CONFIG_BT_MESH_NPS_OPT
#define GENIE_MESH_INIT_PHARSE_II_DELAY_START_MIN (8000)      //Unit:ms
#define GENIE_MESH_INIT_PHARSE_II_DELAY_START_MAX (10 * 1000) //Unit:ms
#else
#define GENIE_MESH_INIT_PHARSE_II_DELAY_START_MIN (1000)      //Unit:ms
#define GENIE_MESH_INIT_PHARSE_II_DELAY_START_MAX (10 * 1000) //Unit:ms
#endif

#define GENIE_MESH_INIT_PHARSE_II_CHECK_APPKEY_INTERVAL (1000)                                                //Unit:ms
#define GENIE_MESH_INIT_PHARSE_II_CHECK_APPKEY_TIMEOUT (20 * GENIE_MESH_INIT_PHARSE_II_CHECK_APPKEY_INTERVAL) //Unit:ms

typedef enum _genie_mesh_init_state_e
{
    GENIE_MESH_INIT_STATE_PROVISION = 1,
    GENIE_MESH_INIT_STATE_HW_RESET,
    GENIE_MESH_INIT_STATE_NORMAL_BOOT,
} genie_mesh_init_state_e;

struct bt_mesh_elem *genie_mesh_get_primary_element(void);

int genie_mesh_init(struct bt_mesh_elem *p_mesh_elem, unsigned int mesh_elem_counts);
int genie_mesh_start(genie_provision_t *p_genie_provision);

int genie_mesh_init_pharse_ii(void);

void genie_mesh_ready_checktimer_restart();

uint8_t genie_mesh_get_init_state(void);

int genie_mesh_suspend(bool force);

int genie_mesh_resume(void);

void genie_mesh_load_group_addr(void);

int genie_mesh_report(void);

#endif
