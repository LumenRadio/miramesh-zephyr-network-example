#ifndef FOTA_DRIVER_H
#define FOTA_DRIVER_H

#define FOTA_SLOT_ID 0
/**
 * Sets this driver as the FOTA driver in MiraMesh.
 *
 * Must be used before calling mira_fota_init.
 */
void fota_driver_set_custom_driver(
    void);

/**
 * Make a backup of the first page in the SWAP area.
 *
 * MCUboot will update the first page during updates,
 * so a backup of the first page is maintained for FOTA
 * transfers.
 * If the swap area is updated outside of the driver,
 * ie updated via BLE DFU uploads, the first page needs
 * to be copied to the backup page. This function does that.
 */
void fota_driver_copy_header_page(
    void);

/**
 * Make a backup of the last page in the SWAP area.
 *
 * MCUboot will update the last page during updates,
 * so a backup of the last page is maintained for FOTA
 * transfers.
 * If the swap area is updated outside of the driver,
 * ie updated via BLE DFU uploads, the last page needs
 * to be copied to the backup page. This function does that.
 */
void fota_driver_copy_trailer_page(
    void);

/**
 * Create a valid Mira FOTA header for the current image in the SWAP
 * area.
 */
void fota_driver_write_new_header(
    void);

#endif /* FOTA_DRIVER_H */
