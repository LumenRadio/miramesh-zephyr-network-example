/*
 * MIT License
 *
 * Copyright (c) 2025 LumenRadio AB
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#include <zephyr/kernel.h>
#include <zephyr/console/console.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/device.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/types.h>
#include <zephyr/irq.h>
#include <zephyr/logging/log.h>
#include <dk_buttons_and_leds.h>

#include <miramesh.h>

#if CONFIG_MIRA_FOTA_INIT
#include "ble.h"
#include "fota_driver.h"
#include "image_handling.h"
#endif /* CONFIG_MIRA_FOTA_INIT */

#define UDP_PORT 456
#define CONFIG_LENGTH_TO_READ 2
#define MIRA_MODE_LOCATION_CONFIG_AREA 0

/* Values stored in config area for the different modes */
#define NETWORK_MODE_ROOT 1
#define NETWORK_MODE_ROOT_NO_RECONNECT 2
#define NETWORK_MODE_LEAF 3

#define CONFIG_AREA_DEVICE FIXED_PARTITION_DEVICE(CONFIG_AREA)
#define CONFIG_AREA_OFFSET FIXED_PARTITION_OFFSET(CONFIG_AREA)
#define CONFIG_AREA_SIZE FIXED_PARTITION_SIZE(CONFIG_ARE)

#if CONFIG_MIRA_FOTA_INIT
static mira_bool_t previous_fota_is_valid = false;
#endif /* CONFIG_MIRA_FOTA_INIT */

static mira_net_config_t net_config = {
    .pan_id = 0x13243546,
    .key = { 0x11,
             0x12,
             0x13,
             0x14,
             0x21,
             0x22,
             0x23,
             0x24,
             0x31,
             0x32,
             0x33,
             0x34,
             0x41,
             0x42,
             0x43,
             0x44 },
    .mode = MIRA_NET_MODE_MESH,
    .rate = MIRA_NET_RATE_FAST,
    .antenna = 0,
    .prefix = NULL
};

static volatile mira_net_state_t current_net_state = MIRA_NET_STATE_NOT_ASSOCIATED;

static void network_state_callback(
    mira_net_state_t net_state)
{
    current_net_state = net_state;
}

static void set_network_mode_from_flash(
    void)
{
    static char buf[CONFIG_LENGTH_TO_READ];
    const struct device *config_area_flash_device = CONFIG_AREA_DEVICE;
    flash_read(config_area_flash_device,
        CONFIG_AREA_OFFSET,
        buf,
        CONFIG_LENGTH_TO_READ);

    uint8_t val = atoi(&buf[MIRA_MODE_LOCATION_CONFIG_AREA]);
    if (val == NETWORK_MODE_ROOT) {
        printf("Starting as root\n");
        net_config.mode = MIRA_NET_MODE_ROOT;
    } else if (val == NETWORK_MODE_ROOT_NO_RECONNECT) {
        printf("Starting as root no reconnect\n");
        net_config.mode = MIRA_NET_MODE_ROOT_NO_RECONNECT;
    } else if (val == NETWORK_MODE_LEAF) {
        printf("Starting as leaf\n");
        net_config.mode = MIRA_NET_MODE_LEAF;
    } else {
        printf("Starting as mesh\n");
        net_config.mode = MIRA_NET_MODE_MESH;
    }
}

static void udp_listen_callback(
    mira_net_udp_connection_t *connection,
    const void *data,
    uint16_t data_len,
    const mira_net_udp_callback_metadata_t *metadata,
    void *storage)
{
    char buffer[MIRA_NET_MAX_ADDRESS_STR_LEN];
    uint16_t i;

    printf("Received message from [%s]:%u: ",
        mira_net_toolkit_format_address(buffer, metadata->source_address),
        metadata->source_port);
    for (i = 0; i < data_len; i++) {
        printf("%c", ((char *) data)[i]);
    }
    printf("\n");
}

#if CONFIG_MIRA_FOTA_INIT
static void check_fota_image_status(
    uint16_t slot_id)
{
    if (mira_fota_is_valid(FOTA_SLOT_ID)) {
        printf("FOTA image valid\n");
        if (!previous_fota_is_valid) {
            image_handling_mark_for_swap();
        }
        previous_fota_is_valid = true;
    } else {
        printf("FOTA image invalid!\n");
        previous_fota_is_valid = false;
    }
}
#endif /* CONFIG_MIRA_FOTA_INIT */

static void poll_for_image(
    void)
{
#if CONFIG_MIRA_FOTA_INIT
    printf("Requesting firmware\n");
    int ret = mira_fota_force_request();
    if (ret != 0) {
        printf("Forced fota request failed: %d\n", ret);
    }
#endif /* CONFIG_MIRA_FOTA_INIT */
}

void network_init(
    void)
{
    set_network_mode_from_flash();
    mira_net_register_net_state_cb(network_state_callback);
    mira_net_init(&net_config);
#if CONFIG_MIRA_FOTA_INIT
    fota_driver_set_custom_driver();
    int ret = mira_fota_init();
    printf("mira_fota_init(): %d\n", ret);
#endif /* CONFIG_MIRA_FOTA_INIT */
}

void send_hello_world(
    void)
{
    mira_net_udp_connection_t *conn;
    mira_status_t res;
    mira_net_address_t addr;
    bool requested_fota_from_root = false;
    char buffer[MIRA_NET_MAX_ADDRESS_STR_LEN];
    char *message = "Hello world from Zephyr!";
    conn = mira_net_udp_connect(NULL, 0, udp_listen_callback, NULL);

    while (1) {
        if (current_net_state != MIRA_NET_STATE_JOINED) {
            printf("Waiting for network (state is %s)\n",
                current_net_state == MIRA_NET_STATE_NOT_ASSOCIATED ? "not associated"
                : current_net_state == MIRA_NET_STATE_ASSOCIATED ? "associated"
                : current_net_state == MIRA_NET_STATE_JOINED ? "joined"
                : "UNKNOWN");
            k_sleep(K_SECONDS(1));
        } else {
            res = mira_net_get_root_address(&addr);
            if (res != MIRA_SUCCESS) {
                printf("Waiting for root address\n");
                k_sleep(K_SECONDS(1));
            } else {
                // Force a early fota request as soon as joined, useful for testing
                if (!requested_fota_from_root) {
                    poll_for_image();
                    requested_fota_from_root = true;
                }
                printf("Sending to address: %s\n",
                    mira_net_toolkit_format_address(buffer, &addr));
                mira_net_udp_send_to(conn, &addr, UDP_PORT, message,
                    strlen(message));
#if CONFIG_MIRA_FOTA_INIT
                check_fota_image_status(FOTA_SLOT_ID);
#endif /* CONFIG_MIRA_FOTA_INIT */
                k_sleep(K_SECONDS(60));
            }
        }
    }
}

void receieve_hello_world(
    void)
{
    mira_net_udp_listen(UDP_PORT, udp_listen_callback, NULL);
    while (1) {
#if CONFIG_MIRA_FOTA_INIT
        check_fota_image_status(FOTA_SLOT_ID);
#endif /* CONFIG_MIRA_FOTA_INIT */
        k_sleep(K_SECONDS(60));
    }
}

int main(
    void)
{
    printf("-------------MiraMesh network example-------------\n");

    mira_sys_device_id_t devid;

    mira_sys_get_device_id(&devid);
    printf("Device ID: %02x%02x%02x%02x%02x%02x%02x%02x\n",
        devid.u8[0],
        devid.u8[1],
        devid.u8[2],
        devid.u8[3],
        devid.u8[4],
        devid.u8[5],
        devid.u8[6],
        devid.u8[7]);

    network_init();

#if CONFIG_MIRA_FOTA_INIT
    dk_leds_init();
    ble_init();
    image_handling_init();
#endif /* CONFIG_MIRA_FOTA_INIT */

    if (net_config.mode == MIRA_NET_MODE_ROOT
        || net_config.mode == MIRA_NET_MODE_ROOT_NO_RECONNECT) {
        receieve_hello_world();
    } else {
        send_hello_world();
    }
    while (1) {
        k_sleep(K_FOREVER);
    }
}
