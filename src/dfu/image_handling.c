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
#include "image_handling.h"

#include <zephyr/dfu/mcuboot.h>

#include <zephyr/mgmt/mcumgr/mgmt/mgmt.h>
#include <zephyr/mgmt/mcumgr/mgmt/callbacks.h>

#include "fota_driver.h"

struct mgmt_callback dfu_done_cb;
struct mgmt_callback block_reset_cb;

enum mgmt_cb_return dfu_done_checker(
    uint32_t event,
    enum mgmt_cb_return prev_status,
    int32_t *rc,
    uint16_t *group,
    bool *abort_more,
    void *data,
    size_t data_size)
{
    if (event == MGMT_EVT_OP_IMG_MGMT_DFU_PENDING) {
        printf("Pending OK!\n");
        fota_driver_copy_trailer_page();
        fota_driver_copy_header_page();
        fota_driver_write_new_header();

    }
    return MGMT_CB_OK;
}

/* Block reset requests by connected peer */
enum mgmt_cb_return block_reset_checker(
    uint32_t event,
    enum mgmt_cb_return prev_status,
    int32_t *rc,
    uint16_t *group,
    bool *abort_more,
    void *data,
    size_t data_size)
{
    if (event == MGMT_EVT_OP_OS_MGMT_RESET) {
        *rc = MGMT_ERR_EACCESSDENIED;
        return MGMT_CB_ERROR_RC;
    }
    return MGMT_CB_OK;
}

void image_handling_mark_for_swap(
    void)
{
    int retval = boot_request_upgrade(BOOT_UPGRADE_PERMANENT);
    printf("img: mark: %d\n", retval);
}

void image_handling_init(
    void)
{
    dfu_done_cb.callback = dfu_done_checker;
    dfu_done_cb.event_id = MGMT_EVT_OP_IMG_MGMT_DFU_PENDING;
    mgmt_callback_register(&dfu_done_cb);
    block_reset_cb.callback = block_reset_checker;
    block_reset_cb.event_id = MGMT_EVT_OP_OS_MGMT_RESET;
    mgmt_callback_register(&block_reset_cb);
}
