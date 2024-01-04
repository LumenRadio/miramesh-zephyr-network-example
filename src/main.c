/*
 * MIT License
 *
 * Copyright (c) 2023 LumenRadio AB
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
#include <nrf.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/types.h>
#include <zephyr/irq.h>
#include <zephyr/logging/log.h>
#include "fota-driver.h"
#include <miramesh.h>

#define UDP_PORT 456
#define UICR_MIRA_MODE_LOCATION 16
#define UICR_ROOT_VALUE 0x1234

static mira_net_config_t net_config = { .pan_id = 0x13243546,
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
                                        .rate = MIRA_NET_RATE_MID,
                                        .antenna = 0,
                                        .prefix = NULL };

static void set_net_config(void)
{
    uint32_t value;
    value = NRF_UICR->CUSTOMER[UICR_MIRA_MODE_LOCATION];
    if (value == UICR_ROOT_VALUE) {
        printf("Setting up node as root\n");
        net_config.mode = MIRA_NET_MODE_ROOT_NO_RECONNECT;
        net_config.rate = MIRA_NET_RATE_FAST;
    } else {
        printf("Setting up node as mesh\n");
        net_config.mode = MIRA_NET_MODE_MESH;
        net_config.rate = MIRA_NET_RATE_FAST;
    }
}

static void udp_listen_callback(mira_net_udp_connection_t* connection,
                                const void* data,
                                uint16_t data_len,
                                const mira_net_udp_callback_metadata_t* metadata,
                                void* storage)
{
    char buffer[MIRA_NET_MAX_ADDRESS_STR_LEN];
    uint16_t i;

    printf("Received message from [%s]:%u: ",
           mira_net_toolkit_format_address(buffer, metadata->source_address),
           metadata->source_port);
    for (i = 0; i < data_len; i++) {
        printf("%c", ((char*)data)[i]);
    }
    printf("\n");
}

static void print_fota_image_status(uint16_t slot_id)
{
#if CONFIG_MIRA_FOTA_INIT
    if (mira_fota_is_valid(FOTA_SLOT_ID)) {
        printf("FOTA image valid\n");
    } else {
        printf("FOTA image invalid!\n");
    }
#endif /* CONFIG_MIRA_FOTA_INIT */
}

static void poll_for_image(void)
{
#if CONFIG_MIRA_FOTA_INIT
    printf("Requesting firmware\n");
    int ret = mira_fota_force_request();
    if (ret != 0) {
        printf("Forced fota request failed: %d\n", ret);
    }
#endif /* CONFIG_MIRA_FOTA_INIT */
}

void network_init(void)
{
    set_net_config();
    mira_net_init(&net_config);
#if CONFIG_MIRA_FOTA_INIT
    fota_driver_set_custom_driver();
    int ret = mira_fota_init();
    printf("mira_fota_init(): %d\n", ret);
#endif /* CONFIG_MIRA_FOTA_INIT */
}

void send_hello_world(void)
{
    mira_net_udp_connection_t* conn;
    mira_net_state_t net_state;
    mira_status_t res;
    mira_net_address_t addr;
    bool requested_fota_from_root = false;
    char buffer[MIRA_NET_MAX_ADDRESS_STR_LEN];
    char* message = "Hello world from Zephyr!";
    conn = mira_net_udp_connect(NULL, 0, udp_listen_callback, NULL);

    while (1) {
        net_state = mira_net_get_state();
        if (net_state != MIRA_NET_STATE_JOINED) {
            printf("Waiting for network (state is %s)\n",
                   net_state == MIRA_NET_STATE_NOT_ASSOCIATED ? "not associated"
                   : net_state == MIRA_NET_STATE_ASSOCIATED   ? "associated"
                   : net_state == MIRA_NET_STATE_JOINED       ? "joined"
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
                printf("Sending to address: %s\n", mira_net_toolkit_format_address(buffer, &addr));
                mira_net_udp_send_to(conn, &addr, UDP_PORT, message, strlen(message));
                print_fota_image_status(FOTA_SLOT_ID);
                k_sleep(K_SECONDS(60));
            }
        }
    }
}

void receieve_hello_world(void)
{
    mira_net_udp_listen(UDP_PORT, udp_listen_callback, NULL);
    while (1) {
        print_fota_image_status(FOTA_SLOT_ID);
        k_sleep(K_SECONDS(60));
    }
}

int main(void)
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
    if (net_config.mode == MIRA_NET_MODE_ROOT ||
        net_config.mode == MIRA_NET_MODE_ROOT_NO_RECONNECT) {
        receieve_hello_world();
    } else {
        send_hello_world();
    }
    while (1) {
        k_sleep(K_FOREVER);
    }
}
