/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 * Copyright (c) 2020 Prevas A/S
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/mgmt/mcumgr/transport/smp_bt.h>
#include <dk_buttons_and_leds.h>

#define LOG_LEVEL LOG_LEVEL_DBG
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(smp_bt_sample);

static struct k_work advertise_work;
static struct k_work stop_advertise_work;

static bool advertising = false;

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, SMP_BT_SVC_UUID_VAL),
};

static const struct bt_data sd[] = {
    BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME,
        sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

static void start_smp_bluetooth_adverts(
    void);

void ble_button_handler_cb(
    uint32_t button_state,
    uint32_t has_changed)
{
    if (button_state & DK_BTN1_MSK) {
        if (advertising) {
            k_work_submit(&stop_advertise_work);
            dk_set_led(0, 0);
            advertising = false;
        } else {
            k_work_submit(&advertise_work);
            dk_set_led(0, 1);
            advertising = true;
        }
    }
}

static void stop_advertise(
    struct k_work *work)
{
    int rc;

    rc = bt_le_adv_stop();
    if (rc) {
        LOG_ERR("Advertising failed to stop (rc %d)", rc);
        return;
    }

    LOG_INF("Advertising successfully stopped");
}

static void advertise(
    struct k_work *work)
{
    int rc;

    rc =
        bt_le_adv_start(BT_LE_ADV_PARAM(BT_LE_ADV_OPT_CONNECTABLE,
            BT_GAP_ADV_FAST_INT_MIN_2, BT_GAP_ADV_FAST_INT_MAX_2, NULL),
            ad,
            ARRAY_SIZE(ad),
            sd,
            ARRAY_SIZE(sd));
    if (rc) {
        LOG_ERR("Advertising failed to start (rc %d)", rc);
        return;
    }

    LOG_INF("Advertising successfully started");
}

static void connected(
    struct bt_conn *conn,
    uint8_t err)
{
    if (err) {
        LOG_ERR("Connection failed, err 0x%02x %s", err, bt_hci_err_to_str(err));
        k_work_submit(&advertise_work);
    } else {
        LOG_INF("Connected");
    }
}

static void disconnected(
    struct bt_conn *conn,
    uint8_t reason)
{
    LOG_INF("Disconnected, reason 0x%02x %s", reason, bt_hci_err_to_str(reason));
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected
};

static void bt_ready(
    int err)
{
    if (err != 0) {
        LOG_ERR("Bluetooth failed to initialise: %d", err);
    }
}

static void start_smp_bluetooth_adverts(
    void)
{
    int rc;

    k_work_init(&advertise_work, advertise);
    k_work_init(&stop_advertise_work, stop_advertise);
    rc = bt_enable(bt_ready);

    if (rc != 0) {
        LOG_ERR("Bluetooth enable failed: %d", rc);
    }
}

void ble_init(
    void)
{
    start_smp_bluetooth_adverts();
    dk_buttons_init(ble_button_handler_cb);
}
