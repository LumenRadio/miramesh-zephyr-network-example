# Miramesh Zephyr network example

This example demonstrates the integration of
[MiraMesh](https://docs.lumenrad.io/miraos/latest/)
into the Zephyr environment using the nRF Connect SDK. The produced
binary is a Zephyr application that continuously transmits messages
via MiraMesh.

This repository follows the
"Workspace application repository" workflow recommended by
Nordic Semiconductors. This is easy to setup but requires more
disk space and will take longer initially due to the need to download
ncs and zephyr.

For more information about nRF Connect SDK workflows [read here](https://docs.nordicsemi.com/bundle/ncs-latest/page/nrf/dev_model_and_contributions/adding_code.html).

## Tags and tested versions

This repository uses version tags that map to corresponding libmira versions. For example, a user of libmira version 2.9.0 should use the tag v2.9.0. Each version tag has a recommended nRF Connect Toolchain version.

The following configurations have been tested:

| Libmira version | Version tag         | nRF Connect toolchain |
| --------------- | --------------------| ----------------------|
| 2.9.0           | v2.9.0              | v2.5.0                |
| 2.10.0          | v2.10.0             | v2.5.1                |
| 2.11.0-beta1    | 2.11.0-beta1        | v3.0.1                |

Other versions may work as well. For example, v2.9.0 is likely to be compatible with other 2.9.x versions of libmira, but these have not been tested or verified. Similarly, different versions of the toolchain may work, but they are not tested.

## Creating a new west workspace

1. [Install nRF Connect toolchain](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/installation/install_ncs.html)
2. Open a shell set up for the nRF Connect toolchain:

    `nrfutil toolchain-manager launch --ncs-version <nRF Connect toolchain> --shell`

3. Initialize the project workspace:

    `west init -m https://github.com/LumenRadio/miramesh-zephyr-network-example --mr <Version tag> <path_to_west_workspace_to_create>`<br>
    where \<nRF Connect toolchain\> and \<Version tag\> are set based on the libmira version according to the table above.

## Building

1. Open a shell set up for nRF Connect toolchain if you haven't already done so:

    `nrfutil toolchain-manager launch --ncs-version <nRF Connect toolchain> --shell`

2. Navigate to the workspace:

    `cd <path_to_west_workspace>`

3. Update the workspace:

    `west update`

4. Extract the libmira archive into `<path_to_west_workspace>/vendor/libmira`.
5. Build the application with one of these commands:

    * For nRF54l15: `west build -b nrf54l15dk/nrf54l15/cpuapp -s miramesh-zephyr-network-example`
    * For nRF52840: `west build -b nrf52840dk/nrf52840 -s miramesh-zephyr-network-example`
    * For nRF52832: `west build -b nrf52dk/nrf52832 -s miramesh-zephyr-network-example`

6. Connect the dev board, nRF52DK or nRF52840DK or nRF54L15DK.
7. Recover the device in case it is in APPROTECT mode.

    Find the serial number (snr) of the programmer with `nrfutil device list`, then:
    `nrfutil device recover --serial-number <snr>`

8. Program a Mira license onto the device.

    Use the `mira-license` west extension provided by miramesh-zephyr module:
    `west mira-license -s <snr> -I <lic-file>`

    The extension provides additional options, which may be required when building
    through other ways. For more details, run:
    `west mira-license --help`

    Alternatively, you can use the `mira_license.py` script provided in the archive:

    * For nRF52840: `./vendor/libmira/tools/mira_license.py license -s <snr> -P nrfjprog -a 0xFF000 -l 0x1000 -I <lic-file>`
    * For nRF52832: `./vendor/libmira/tools/mira_license.py license -s <snr> -P nrfjprog -a 0x7F000 -l 0x1000 -I <lic-file>`
    * For nRF54L15: `./vendor/libmira/tools/mira_license.py license -s <snr> -P jlink -T nrf54l15 -a 0x164000 -l 0x1000 -I <lic-file>`

    Note: this may require running `pip install -r vendor/libmira/tools/requirements.txt'
    Read more about licensing in the [documentation](https://docs.lumenrad.io/miraos/latest/description/licensing/licensing_tool.html).

9. Flash the firmware:

    `west flash --dev-id <snr>`

10. Connect to the development kit's serial port. Some Nordic Semiconductor kits expose multiple virtual serial ports. If no output appears, try the other port. 
After a reset, you should see a message like:  
    ```
    -------------MiraMesh network example-------------
    ```

To receive the messages sent by the sender set up in previous steps, a root/receiver device is required.
The network_receiver example from MiraOS examples can be used,
but it is also possible to use this example's firmware:

Configure one device to be a root/receiver, see instructions below.
1. Install requirements for configuration generation tool: `pip install -r miramesh-zephyr-network-example/requirements.txt`
2. Generate hex file from the config files
    * For nRF52840: `./miramesh-zephyr-network-example/config2hex.py -f miramesh-zephyr-network-example/mira_network_configs/root.config -o root.hex -a 0xfe000 -l 0x1000`
    * For nRF52832: `./miramesh-zephyr-network-example/config2hex.py -f miramesh-zephyr-network-example/mira_network_configs/root.config -o root.hex -a 0x7e000 -l 0x1000`
    * For nRF54L15: `./miramesh-zephyr-network-example/config2hex.py -f miramesh-zephyr-network-example/mira_network_configs/root.config -o root.hex -a 0x163000 -l 0x1000`
3. Flash hex file to the device
`nrfutil device program --firmware root.hex --options chip_erase_mode=ERASE_RANGES_TOUCHED_BY_FIRMWARE,reset=RESET_SYSTEM --serial-number <snr>`

Note: MiraMesh connection times can improve if you reset the network sender after initializing the receiver.

## Firmware update

The example by default uses MCUboot to support firmware updates locally through BLE and also over the network using FOTA.

To enable FOTA, enable the Kconfig option `CONFIG_MIRA_FOTA_INIT`. This will enable MiraMesh FOTA transfer
and the possibility to upload the new firmware over BLE through nRF Device Manager. [See instructions.](#local-firmware-update)

The firmware installed over BLE can be further propagated to other
devices in the network using
[Miramesh's firmware transfer method](#propagating-the-installed-firmware-with-mirameshs-built-in-firmware-transfer).

### Local firmware update

To create a upgrade image and install it:
1. Build the new application with west.

2. Transfer dfu_application.zip to a mobile phone with nRF Device Manager app installed.

3. Press button 0 on the DK and see that LED0 lights up, now the device is advertising

4. In nRF Device Manager App there should now be a device called "Zephyr", press on the device.

5. In the bottom pane, select the Image category and select dfu_application.zip as the application to upload.

6. Press start. In the prompt select either "Confirm Only" or "No revert". The upload will now start.

7. When upload is complete close the app and turn off advertising by pressing button 0 again.

9. The firmware will now be distributed by MiraMesh to other devices in the network.

8. On the next restart the new firmware will be installed by MCUboot.

#### Update using a Mira Gateway
It is also possible to do FOTA updates when using the Mira Gateway. The Mira gateway accepts binary files
directly to use for FOTA updates. To obtain the binary file, extract the `dfu_application.zip` archive, copy the `bin` file
it contains to the Mira Gateway's `firmwares/` folder, and rename it to `0.bin`.

## Common problems

### python scripts, like mira_license.py, fails with ncs

nRF Connect SDK's toolchain has its own version of python.
If the locally installed python version is of a different version than
the installed one, python modules sometimes doesn't work correctly.

Either run python scripts from a different shell (one without the nRF Connect's toolchain in its path)
or only use modules installed in a virtual env created by the ncs' python installation.

### "Configuring incomplete, errors occurred!" during first build.

Further up in the text there will be information about the actual
error. Forgotting to put `libmira` below the `vendor` directory is
a common reason for this.

### "ninja: error: loading 'build.ninja': No such file or directory"

If the initial build failed, it often didn't make all the needed
files. Just delete the `build` directory and try again after fixing
the error.

### "ERROR: update failed for project miramesh-zephyr" or git error <repo> did not send all necessary objects

This is likely due to the correct toolchain not being launched. Make sure you execute
west commands in an nRF connect toolchain shell by running:

`nrfutil toolchain-manager launch --ncs-version <nRF Connect toolchain> --shell`

### "ERROR: Could not find a package configuration file provided by "Zephyr"...

This is likely due to missing reference to the Zephyr CMake package. This  can be
resolved by running:

`west zephyr-export`
