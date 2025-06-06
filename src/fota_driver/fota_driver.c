#include "fota_driver.h"
#if CONFIG_MIRA_FOTA_INIT
#include <miramesh.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/logging/log.h>
#include <zephyr/storage/flash_map.h>
#include <devicetree_generated.h>

#define SWAP slot1_partition
#define SWAP_DEVICE FIXED_PARTITION_DEVICE(SWAP)
#define SWAP_OFFSET FIXED_PARTITION_OFFSET(SWAP)
#define SWAP_SIZE FIXED_PARTITION_SIZE(SWAP)

#define IMAGE_HEADER_PAGE_DEVICE FIXED_PARTITION_DEVICE(IMAGE_HEADER_PAGE)
#define IMAGE_HEADER_PAGE_OFFSET FIXED_PARTITION_OFFSET(IMAGE_HEADER_PAGE)
#define IMAGE_HEADER_PAGE_SIZE FIXED_PARTITION_SIZE(IMAGE_HEADER_PAGE)

#define IMAGE_TRAILER_PAGE_DEVICE FIXED_PARTITION_DEVICE(IMAGE_TRAILER_PAGE)
#define IMAGE_TRAILER_PAGE_OFFSET FIXED_PARTITION_OFFSET(IMAGE_TRAILER_PAGE)
#define IMAGE_TRAILER_PAGE_SIZE FIXED_PARTITION_SIZE(IMAGE_TRAILER_PAGE)

#define SWAP_ADDRESS(slot_address) (SWAP_OFFSET + (slot_address))
#define HEADER_PAGE_ADDRESS(slot_address) (IMAGE_HEADER_PAGE_OFFSET \
                                           + (slot_address))
#define TRAILER_PAGE_ADDRESS(slot_address) \
    (IMAGE_TRAILER_PAGE_OFFSET + (slot_address - (SWAP_SIZE - FLASH_PAGE_SIZE)))

#define FLASH_PAGE_SIZE 0x1000
#define HEADER_PAGE_DATA_SIZE (FLASH_PAGE_SIZE)

#define MCU_BOOT_HEADER_LOCATION PM_MCUBOOT_PAD_SIZE

#define MIRA_HEADER_LOCATION (MCU_BOOT_HEADER_LOCATION - MIRA_FOTA_HEADER_SIZE)

#if CONFIG_MIRA_FOTA_LOGGING
LOG_MODULE_REGISTER(fota_driver, CONFIG_MIRA_FOTA_DRIVER_LOG_LEVEL);
#else
LOG_MODULE_REGISTER(fota_driver, 0);
#endif

extern const k_tid_t fota_swap_erase_worker_thread_id;

static bool address_in_header_page(
    uint32_t address)
{
    return address < HEADER_PAGE_DATA_SIZE;
}

static bool address_in_trailer_page(
    uint32_t address,
    uint32_t length)
{
    return ((address + length > SWAP_SIZE - FLASH_PAGE_SIZE));
}

static uint32_t flash_check_overlapping(
    uint32_t address,
    uint32_t length)
{
    if (!address_in_header_page(address + length - 1)
        && address_in_header_page(address)) {
        // In first page and overlapping, return overlap
        return (address + length) - HEADER_PAGE_DATA_SIZE;
        ;
    }
    if (!address_in_trailer_page(address - 1,
        0) && address_in_trailer_page(address, length)) {
        // In second to last page and overlapping into last page, return overlap
        return (address + length) - (SWAP_SIZE - FLASH_PAGE_SIZE);
    }
    return 0;
}

static int fota_driver_get_size(
    uint16_t slot_id,
    uint32_t *size,
    void (*done_callback)(void *storage),
    void *storage)
{
    if (slot_id == 0) {
        // The driver has its own functions to read/write the
        // mira-fota-header and the header is stored in an otherwise
        // unused part of the first page.
        // This makes the whole swap area's size available:
        *size = (uint32_t) SWAP_SIZE;
        LOG_DBG("Slot 0 size: %d", SWAP_SIZE);
        done_callback(storage);
        return 0;
    } else {
        LOG_DBG("Slot %d not available", slot_id);
        return -1;
    }
}

static bool is_overlapping_mira_fota_header(
    uint32_t address,
    uint32_t length)
{
    if ((address + length <= MIRA_HEADER_LOCATION)
        || (address >= MIRA_HEADER_LOCATION + MIRA_FOTA_HEADER_SIZE)) {
        return false;
    } else {
        return true;
    }
}

static uint32_t length_before_mira_header(
    uint32_t address,
    uint32_t length)
{
    return MIRA_HEADER_LOCATION - address;
}

static uint32_t length_after_mira_header(
    uint32_t address,
    uint32_t length)
{
    return (address + length) - MCU_BOOT_HEADER_LOCATION;
}

static int fota_driver_read(
    uint16_t slot_id,
    void *data,
    uint32_t address,
    uint32_t length,
    void (*done_callback)(void *storage),
    void *storage)
{
    if (slot_id == 0
        && ((address + length) <= SWAP_SIZE)) {
        uint8_t *img_fragment = data;
        const struct device *swap_dev = SWAP_DEVICE;
        if (address_in_header_page(address)) {
            const struct device *header_dev = IMAGE_HEADER_PAGE_DEVICE;
            uint32_t overlap = flash_check_overlapping(address, length);
            if (overlap == 0) {
                LOG_DBG("Read from slot 0, header, addr: %d, length: %d",
                    address,
                    length);
                if (is_overlapping_mira_fota_header(address, length)) {
                    flash_read(header_dev,
                        HEADER_PAGE_ADDRESS(address),
                        img_fragment,
                        length);
                    for (uint32_t i = 0; i < length; i++) {
                        if (is_overlapping_mira_fota_header(address + i, 1)) {
                            img_fragment[i] = 0xff;
                        }
                    }

                } else {
                    flash_read(header_dev,
                        HEADER_PAGE_ADDRESS(address),
                        img_fragment,
                        length);
                }
            } else {
                LOG_DBG(
                    "Read from slot 0, header overlapping, addr: %d, length: %d",
                    address,
                    length);
                uint32_t length_in_header = length - overlap;
                LOG_DBG("Reading from header, address: %d, length: %d",
                    address,
                    length_in_header);
                flash_read(header_dev,
                    HEADER_PAGE_ADDRESS(address),
                    data,
                    length_in_header);
                img_fragment += length_in_header;
                flash_read(swap_dev,
                    SWAP_ADDRESS(HEADER_PAGE_DATA_SIZE),
                    img_fragment,
                    overlap);
            }
        } else if (address_in_trailer_page(address, length)) {
            const struct device *trailer_dev = IMAGE_TRAILER_PAGE_DEVICE;
            uint32_t overlap = flash_check_overlapping(address, length);
            if (overlap == 0) {
                LOG_DBG("Read from slot 0, trailer, addr: %d, length: %d",
                    address,
                    length);
                flash_read(trailer_dev,
                    TRAILER_PAGE_ADDRESS(address),
                    img_fragment,
                    length);
            } else {
                LOG_DBG(
                    "Read from slot 0, trailer, overlapping addr: %d, length: %d",
                    address,
                    length);
                uint32_t length_in_swap = length - overlap;
                flash_read(swap_dev,
                    SWAP_ADDRESS(address),
                    img_fragment,
                    length_in_swap);
                img_fragment += length_in_swap;
                flash_read(trailer_dev,
                    TRAILER_PAGE_ADDRESS(address + length_in_swap),
                    img_fragment,
                    overlap);
            }
        } else {
            LOG_DBG("Read from slot 0, addr: %d, length: %d", address, length);
            flash_read(swap_dev, SWAP_ADDRESS(address), img_fragment, length);
        }
        done_callback(storage);
        return 0;
    } else {
        LOG_DBG("Slot %d not available or length out of bounds", slot_id);
        return -1;
    }
}

static int fota_driver_write(
    uint16_t slot_id,
    const void *data,
    uint32_t address,
    uint32_t length,
    void (*done_callback)(void *storage),
    void *storage)
{
    if (slot_id == 0
        && ((address + length) <= SWAP_SIZE)) {
        const uint8_t *img_fragment = data;
        const struct device *swap_dev = SWAP_DEVICE;
        if (address_in_header_page(address)) {
            const struct device *header_dev = IMAGE_HEADER_PAGE_DEVICE;
            uint32_t overlap = flash_check_overlapping(address, length);
            if (overlap == 0) {
                if (is_overlapping_mira_fota_header(address, length)) {
                    uint32_t before = length_before_mira_header(address, length);
                    uint32_t after = length_after_mira_header(address, length);
                    if (before != 0) {
                        flash_write(header_dev,
                            HEADER_PAGE_ADDRESS(address),
                            img_fragment,
                            before);
                    }

                    if (after != 0) {
                        flash_write(header_dev,
                            HEADER_PAGE_ADDRESS(address + before
                                + MIRA_FOTA_HEADER_SIZE),
                            &img_fragment[before + MIRA_FOTA_HEADER_SIZE],
                            after);
                    }

                    flash_write(swap_dev,
                        SWAP_ADDRESS(address),
                        img_fragment,
                        length);

                } else {
                    LOG_DBG("Write to slot 0, header, offset: %d, length: %d",
                        address,
                        length);
                    flash_write(swap_dev,
                        SWAP_ADDRESS(address),
                        img_fragment,
                        length);
                    LOG_DBG("Write to: %u, length: %u", address, length);
                    flash_write(header_dev,
                        HEADER_PAGE_ADDRESS(address),
                        img_fragment,
                        length);
                }
            } else {
                LOG_DBG("Write to slot 0, header, offset: %d, length: %d",
                    address,
                    length);
                flash_write(swap_dev, SWAP_ADDRESS(address), img_fragment, length);
                uint32_t length_in_header = length - overlap;
                flash_write(
                    header_dev,
                    HEADER_PAGE_ADDRESS(address),
                    img_fragment,
                    length_in_header);
            }
        } else if (address_in_trailer_page(address, length)) {
            const struct device *trailer_dev = IMAGE_TRAILER_PAGE_DEVICE;
            uint32_t overlap = flash_check_overlapping(address, length);
            if (overlap == 0) {
                LOG_DBG("Write to slot 0, trailer, offset: %d, length: %d",
                    address,
                    length);
                flash_write(swap_dev, SWAP_ADDRESS(address), img_fragment, length);
                flash_write(trailer_dev,
                    TRAILER_PAGE_ADDRESS(address),
                    img_fragment,
                    length);
            } else {
                LOG_DBG(
                    "Write to slot 0, trailer overlapping, offset: %d, length: %d",
                    address,
                    length);
                flash_write(swap_dev, SWAP_ADDRESS(address), img_fragment, length);
                uint32_t length_in_swap = length - overlap;
                img_fragment += length_in_swap;
                LOG_DBG("length_in_swap: %u, overlap: %u", length_in_swap, overlap);
                flash_write(trailer_dev,
                    TRAILER_PAGE_ADDRESS(address + length_in_swap),
                    img_fragment,
                    overlap);
            }
        } else {
            LOG_DBG("Write to slot 0, offset: %d, length: %d", address, length);
            flash_write(swap_dev, SWAP_ADDRESS(address), img_fragment, length);
        }
        done_callback(storage);
        return 0;
    } else {
        LOG_DBG("Slot %d not available or length out of bounds", slot_id);
        return -1;
    }
}

static int fota_driver_read_header(
    uint16_t slot_id,
    void *data,
    void (*done_callback)(void *storage),
    void *storage)
{
    if (slot_id == 0) {
        uint8_t *img_fragment = data;
        const struct device *header_dev = IMAGE_HEADER_PAGE_DEVICE;
        LOG_DBG(
            "Read from slot 0, Mira header, addr: %d, length: %d",
            MIRA_HEADER_LOCATION,
            MIRA_FOTA_HEADER_SIZE);
        flash_read(header_dev,
            HEADER_PAGE_ADDRESS(MIRA_HEADER_LOCATION),
            img_fragment,
            MIRA_FOTA_HEADER_SIZE);
        done_callback(storage);
        return 0;
    } else {
        LOG_DBG("Slot %d not available", slot_id);
        return -1;
    }
}

static int fota_driver_write_header(
    uint16_t slot_id,
    const void *data,
    void (*done_callback)(void *storage),
    void *storage)
{
    if (slot_id == 0) {
        const uint8_t *img_fragment = data;
        const struct device *header_dev = IMAGE_HEADER_PAGE_DEVICE;
        LOG_DBG(
            "Write to slot 0, Mira header, addr: %d, length: %d",
            MIRA_HEADER_LOCATION,
            MIRA_FOTA_HEADER_SIZE);
        flash_write(header_dev,
            HEADER_PAGE_ADDRESS(MIRA_HEADER_LOCATION),
            img_fragment,
            MIRA_FOTA_HEADER_SIZE);
        done_callback(storage);
        return 0;
    } else {
        LOG_DBG("Slot %d not available", slot_id);
        return -1;
    }
}

static void (*done_callback_erase)(
    void *storage) = NULL;
static void *storage_erase = NULL;

void fota_swap_erase_worker(
    void)
{
    k_thread_suspend(fota_swap_erase_worker_thread_id);

    const struct device *swap_dev = SWAP_DEVICE;
    while (1) {
        for (int i = 0; i < SWAP_SIZE / FLASH_PAGE_SIZE; i++) {
            flash_erase(swap_dev, SWAP_ADDRESS(FLASH_PAGE_SIZE * i),
                FLASH_PAGE_SIZE);
#if defined(CONFIG_SOC_SERIES_NRF52X)
            k_sleep(K_MSEC(100));
#endif
        }
        const struct device *trailer_dev = IMAGE_TRAILER_PAGE_DEVICE;
        flash_erase(trailer_dev, TRAILER_PAGE_ADDRESS(0), FLASH_PAGE_SIZE);
#if defined(CONFIG_SOC_SERIES_NRF52X)
        k_sleep(K_MSEC(100));
#endif
        const struct device *header_dev = IMAGE_HEADER_PAGE_DEVICE;
        flash_erase(header_dev, HEADER_PAGE_ADDRESS(0), FLASH_PAGE_SIZE);
        if (done_callback_erase != NULL && storage_erase != NULL) {
            done_callback_erase(storage_erase);
        }
        done_callback_erase = NULL;
        storage_erase = NULL;
        k_thread_suspend(fota_swap_erase_worker_thread_id);
    }
}

K_THREAD_DEFINE(fota_swap_erase_worker_thread_id,
    2048,
    fota_swap_erase_worker,
    NULL,
    NULL,
    NULL,
    8,
    0,
    0);

static int fota_driver_erase(
    uint16_t slot_id,
    void (*done_callback)(void *storage),
    void *storage)
{
    if (slot_id == 0) {
        LOG_DBG("Erasing slot: %d", slot_id);
        k_thread_resume(fota_swap_erase_worker_thread_id);
        done_callback_erase = done_callback;
        storage_erase = storage;
        return 0;
    } else {
        LOG_DBG("Slot %d not available", slot_id);
        return -1;
    }
}

void fota_driver_init(
    void)
{
    /* Nothing special needs to be done here */
}

void fota_driver_set_custom_driver(
    void)
{
    /* Zero out the struct to set unassigned callback to NULL */
    mira_fota_driver_t fota_driver = {
        .init = fota_driver_init,
        .get_size = fota_driver_get_size,
        .read = fota_driver_read,
        .write = fota_driver_write,
        .erase = fota_driver_erase,
        .read_header = fota_driver_read_header,
        .write_header = fota_driver_write_header
    };

    mira_fota_set_driver(&fota_driver);
}

static uint8_t flash_page_cache[FLASH_PAGE_SIZE];
void fota_driver_copy_trailer_page(
    void)
{
    const struct device *swap_dev = SWAP_DEVICE;
    const struct device *trailer_dev = IMAGE_TRAILER_PAGE_DEVICE;
    flash_read(swap_dev,
        SWAP_OFFSET + SWAP_SIZE - FLASH_PAGE_SIZE,
        flash_page_cache,
        FLASH_PAGE_SIZE);
    flash_write(trailer_dev, IMAGE_TRAILER_PAGE_OFFSET, flash_page_cache,
        IMAGE_TRAILER_PAGE_SIZE);
}

void fota_driver_copy_header_page(
    void)
{
    const struct device *swap_dev = SWAP_DEVICE;
    const struct device *trailer_dev = IMAGE_HEADER_PAGE_DEVICE;
    flash_read(swap_dev, SWAP_OFFSET, flash_page_cache, FLASH_PAGE_SIZE);
    flash_write(trailer_dev,
        IMAGE_HEADER_PAGE_OFFSET,
        flash_page_cache,
        IMAGE_HEADER_PAGE_SIZE);
}

void fota_driver_write_new_header(
    void)
{
    mira_crc_ctx_t ctx;
    mira_crc_init(&ctx);
    uint8_t buf[32];
    const struct device *swap_dev = SWAP_DEVICE;
    for (int offset = 0; offset < SWAP_SIZE; offset += sizeof(buf)) {
        flash_read(swap_dev, SWAP_OFFSET + offset, buf, sizeof(buf));
        mira_crc_update(&ctx, buf, sizeof(buf));
    }
    mira_fota_header_t header = {
        0
    };
    mira_crc_get(&ctx, &header.checksum);
    header.flags = 0;
    header.size = SWAP_SIZE;
    header.type = 0;
    header.version = 100;
    mira_status_t retval;
    retval = mira_fota_write_start(0);
    retval = mira_fota_write_header(SWAP_SIZE,
        header.checksum,
        header.type,
        header.flags,
        header.version);
    retval = mira_fota_write_end();
}
#endif
