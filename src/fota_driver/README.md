## Driver specifics

MCUboot erases the first and last flash page of `slot1_partition` (MCUboot secondary slot) after copying the image to `slot0_partition` (MCUboot primary slot). In order to still be able to distribute an intact image after updating, the device needs to backup the first and last flash page of the image. For this to work, there is two extra flash regions with the size of one flash page to act as backup for the first and last flash page of the image.

Mira's FOTA uses its own header to store image specific information, the header is expected to be placed before the transferred image in flash. However that conflicts with MCUboot's header. This driver instead stores the Mira FOTA header in the backup page for the first flash page of the image. The Mira FOTA header is stored in the end of the MCUboot header which MCUboot currently does not use. When the driver reads from the address where the Mira FOTA header is stored, the driver returns `0xFF` as that is the value it actually has.

So the flash layout is as following, assuming `n` flash pages of 4096 bytes:
```
slot1_partition
            |----||----|     |----||----|
            |    ||    | ... |    ||    |
            |----||----|     |----||----|
Flash page    0     1         n-1    n

IMAGE_HEADER_PAGE
            |----|
            |    |
            |----|


IMAGE_TRAILER_PAGE
            |----|
            |    |
            |----|
```

The fota-driver.c automatically does a backup of flash page `0` and flash page `n` when receiving the image,
the Mira FOTA header is also stored in the `IMAGE_HEADER_PAGE` flash page at the end of the MCUboot image header.
When distributing the image, flash page `0` and flash page `n` is instead read from the backup pages `IMAGE_HEADER_PAGE` and `IMAGE_TRAILER_PAGE`.
