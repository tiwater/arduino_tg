/*
 * Copyright (C) 2019-2020 Alibaba Group Holding Limited
 */
#include "stdio.h"
#include "stdint.h"
#include "stdlib.h"
#include <string.h>
#include <aos/kernel.h>
#include <yoc/partition.h>
#include <drv/pmu.h>
#include "ble/dut_cmd.h"
#include "dut_utility.h"
#include "dut/hal/common.h"
#include "dut/hal/ble.h"

#define    OK              0

static const dut_at_cmd_t ble_dut_commands[]  = {
    { "SLEEP", dut_cmd_sleep, ""},
    { "MAC", dut_cmd_opt_mac, "='xx:xx:xx:xx:xx:xx'"},
    { "XTALCAP", dut_cmd_xtal_cap, "='value'"},
    { "TXSINGLETONE", dut_cmd_tx_single_tone, "='phy_fmt', 'rf_chn_idx', 'xtalcap','txpower'"},
    { "TXMODBURST", dut_cmd_tx_mod_burst, "='phy_fmt', 'rf_chn_idx', 'xtalcap','txpower', 'pkt_type'"},
    { "RXDEMODBURST", dut_cmd_rx_demod_burst, "='phy_fmt', 'rf_chn_idx', 'xtalcap'"},
    { "TRANSTOP", dut_cmd_transmit_stop, ""},
};

int dut_ble_default_cmds_reg(void)
{
    int ret;
    ret = dut_service_cmds_reg(ble_dut_commands, sizeof(ble_dut_commands) / sizeof(dut_at_cmd_t));
    return ret;
}

int  dut_cmd_tx_single_tone(dut_cmd_type_e type, int argc, char *argv[])
{
    uint8_t phy_fmt;
    uint8_t txpower;
    uint8_t rf_chn_idx;
    uint32_t xtalcap;
    int ret = -1;

    if (argc != 5 || type != DUT_CMD_EXECUTE) {
        return -1;
    }

    phy_fmt = (uint8_t)(atoi(argv[1]));
    rf_chn_idx = (uint8_t)(atoi(argv[2]));
    xtalcap = (uint8_t)(atoi(argv[3]));
    txpower = (uint8_t)(atoi(argv[4]));

    ret = dut_hal_ble_tx_single_tone(phy_fmt, rf_chn_idx, xtalcap, txpower);

    return ret;
}

int  dut_cmd_tx_mod_burst(dut_cmd_type_e type, int argc, char *argv[])
{
    uint8_t phy_fmt;
    uint8_t txpower;
    uint8_t rf_chn_idx;
    uint32_t xtalcap;
    uint8_t pkt_type;
    int ret = -1;

    if (argc != 6 || type != DUT_CMD_EXECUTE) {
        return -1;
    }

    phy_fmt = (uint8_t)(atoi(argv[1]));
    rf_chn_idx = (uint8_t)(atoi(argv[2]));
    xtalcap = (uint8_t)(atoi(argv[3]));
    txpower = (uint8_t)(atoi(argv[4]));
    pkt_type = (uint8_t)(atoi(argv[5]));

    ret = dut_hal_ble_tx_mod_burst(phy_fmt, rf_chn_idx, xtalcap, txpower, pkt_type);

    return ret;
}

int  dut_cmd_rx_demod_burst(dut_cmd_type_e type, int argc, char *argv[])
{
    uint8_t phy_fmt;
    uint8_t rf_chn_idx;
    uint32_t xtalcap;
    int16_t rx_freq_off = 0;
    uint8_t rx_rssi = 0;
    uint8_t rx_carr_sens = 0;
    uint16_t rx_pkt_num = 0;
    int ret = -1;

    if (argc != 4 || type != DUT_CMD_EXECUTE) {
        return -1;
    }

    phy_fmt = (uint8_t)(atoi(argv[1]));
    rf_chn_idx = (uint8_t)(atoi(argv[2]));
    xtalcap = (uint8_t)(atoi(argv[3]));

    ret = dut_hal_ble_rx_demod_burst(phy_fmt, rf_chn_idx, xtalcap, &rx_freq_off, &rx_rssi, &rx_carr_sens, &rx_pkt_num);

    if (ret == 0) {
        dut_at_send("+RXDEMODBURST=%d,-%d,%d,%d", rx_freq_off, rx_rssi, rx_carr_sens, rx_pkt_num);
    }

    return ret;
}

int  dut_cmd_opt_mac(dut_cmd_type_e type, int argc, char *argv[])
{
    int ret;
    uint8_t addr[6];

    if (type == DUT_CMD_EXECUTE) {

        if (argc != 2) {
            return -1;
        }

        ret = str2_char(argv[1], addr);
        if (ret < 0) {
            return ret;
        }

        ret = dut_hal_mac_store(addr);
        if (ret < 0) {
            return ret;
        }
    } else {
        if (argc != 1) {
            return -1;
        }

        ret = dut_hal_mac_get(addr);
        if (ret < 0) {
            return ret;
        }

        dut_at_send("+MAC:%02x:%02x:%02x:%02x:%02x:%02x", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
    }

    return OK;
}

int dut_cmd_sleep(dut_cmd_type_e type, int argc, char *argv[])
{
    if (argc != 1 || type != DUT_CMD_EXECUTE) {
        return -1;
    }
    int sleepMode = atoi(argv[1]);
    int ret = 0;
    dut_at_send("OK");
    ret = dut_hal_sleep(sleepMode);
    if (ret != 0) {
        return -1;
    }
    return OK;
}

int  dut_cmd_xtal_cap(dut_cmd_type_e type, int argc, char *argv[])
{
    int ret;
    uint32_t xtalcap = 0;

    if (type == DUT_CMD_EXECUTE) {
        if (argc != 2) {
            return -1;
        }

        xtalcap = atoi(argv[1]);

        ret = dut_hal_xtalcap_store(xtalcap);
        if (ret < 0) {
            return ret;
        }
    } else {
        if (argc != 1) {
            return -1;
        }

        ret = dut_hal_xtalcap_get(&xtalcap);
        if (ret < 0) {
            return ret;
        }

        dut_at_send("+XTALCAP=%d", xtalcap);
    }
    return  OK;
}

int dut_cmd_transmit_stop(dut_cmd_type_e type, int argc, char *argv[])
{
    int ret;

    ret = dut_hal_ble_transmit_stop();
    return ret;
}