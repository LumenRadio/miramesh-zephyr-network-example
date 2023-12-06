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
#include <zephyr/sys/printk.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/types.h>
#include <zephyr/irq.h>
#include <zephyr/logging/log.h>

#include <miramesh.h>

static const mira_net_config_t net_config = {
    .pan_id = 0x13243546,
    .key = {
        0x11, 0x12, 0x13, 0x14,
        0x21, 0x22, 0x23, 0x24,
        0x31, 0x32, 0x33, 0x34,
        0x41, 0x42, 0x43, 0x44
    },
    .mode = MIRA_NET_MODE_MESH,
    .rate = MIRA_NET_RATE_MID,
    .antenna = 0,
    .prefix = NULL
};

void network_sender_init(
    void)
{
    mira_net_init(&net_config);

}

void send_hello_world(
    void)
{
    mira_net_udp_connection_t *conn;
    mira_net_state_t net_state;
    mira_status_t res;
    mira_net_address_t addr;
    char buffer[MIRA_NET_MAX_ADDRESS_STR_LEN];
    char *message = "Hello world from Zephyr!";
    conn = mira_net_udp_connect(NULL, 0, NULL, NULL);

    while (1) {
        net_state = mira_net_get_state();
        if (net_state != MIRA_NET_STATE_JOINED) {
            printf(
                "Waiting for network (state is %s)\n",
                net_state == MIRA_NET_STATE_NOT_ASSOCIATED ? "not associated"
                : net_state == MIRA_NET_STATE_ASSOCIATED ? "associated"
                : net_state == MIRA_NET_STATE_JOINED ? "joined"
                : "UNKNOWN"
            );
            k_sleep(K_SECONDS(1));
        } else {
            res = mira_net_get_root_address(&addr);
            if (res != MIRA_SUCCESS) {
                printf("Waiting for root address\n");
                k_sleep(K_SECONDS(1));
            } else {
                printf("Sending to address: %s\n",
                    mira_net_toolkit_format_address(buffer, &addr));
                mira_net_udp_send_to(conn, &addr, 456, message, strlen(message));
                k_sleep(K_SECONDS(60));
            }
        }
    }
}

K_THREAD_DEFINE(send_hello_world_thread_id, 1024,
    send_hello_world, NULL, NULL, NULL, 8, 0, 10000);

int main(
    void)
{
    printf("-------------------MiraMesh Sender-------------------\n");

    mira_sys_device_id_t devid;

    mira_sys_get_device_id(&devid);
    printf("Device ID: %02x%02x%02x%02x%02x%02x%02x%02x\n",
        devid.u8[0], devid.u8[1], devid.u8[2], devid.u8[3],
        devid.u8[4], devid.u8[5], devid.u8[6], devid.u8[7]
    );

    network_sender_init();
    while (1) {
        k_sleep(K_FOREVER);
    }

}
