# Miramesh Zephyr network example

This example demonstrates the integration of
[MiraMesh](https://docs.lumenrad.io/miraos/latest/)
into the Zephyr environment using the nRF Connect SDK. The produced
binary is a Zephyr application that continuously transmits messages
via MiraMesh.

This project is compatible with nRF Connect SDK version 2.5.0.
Other versions might work as well, but that has not been verified.

You can [create a new west workspace](#creating-a-new-west-workspace)
from this project's manifest file.

Using this repository's manifest directly is easier but requires more
disk space and will take longer initially due to the need to download
ncs and zephyr.

You can also [import the project](#using-an-existing-west-workspace)
into an existing west workspace to avoid having multiple workspaces.

## Creating a new west workspace

1. [Install nRF Connect toolchain](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/installation/install_ncs.html)
2. Open a shell set up for the nRF Connect toolchain:

    `nrfutil toolchain-manager launch --ncs-version v2.5.0 --shell`  
    Other toolkit versions might work. This has been tested and verified with v2.5.0.

3. Initialize the project workspace:

    `west init -m https://github.com/LumenRadio/miramesh-zephyr-network-example --mr main <path_to_west_workspace_to_create>`

## Using an existing west workspace

If you have an existing nRF Connect SDK west workspace follow these
steps:

1. Add the LumenRadio github as a remote to `ncs/v2.5.0/nrf/west.yml`:

        remotes:
            ...
            - name: lumenradio
            url-base: https://github.com/LumenRadio

2. Add the project and its dependency [miramesh-zephyr](https://github.com/LumenRadio/miramesh-zephyr) to the same `west.yml` file:

        projects:
        ...
        - name: miramesh-zephyr-network-example
          repo-path: miramesh-zephyr-network-example
          remote: lumenradio
          revision: main
          path: miramesh-network-example
          import:
            name-allowlist: miramesh-zephyr


This repo and its dependencies will then be downloaded in
the [build](#building) step.

## Building

1. Open a shell set up for nRF Connect toolchain if you haven't already done so:

    `nrfutil toolchain-manager launch --ncs-version v2.5.0 --shell`

2. Navigate to the workspace:

    `cd <path_to_west_workspace>` (below the `v2.5.0` dir, if using the existing ncs installation).

3. Update the workspace:

    `west update`

4. Extract the libmira archive into `<path_to_west_workspace>/vendor/libmira`.
5. Build the application with one of these commands:

    For nRF52840: `west build -b nrf52840dk_nrf52840 -s miramesh-network-example`  
    For nRF52832: `west build -b nrf52dk_nrf52832 -s miramesh-network-example`

6. Connect the dev board, either nRF52DK or nRF52840DK.
7. Recover the device in case it is in APPROTECT mode.  

    Find the serial number (snr) of the programmer with `nrfjprog --ids`, then:  
    `nrfjprog --recover -s <snr>`

8. Program a Mira license onto the device.  

    For nRF52840: `mira_license.py license -s <snr> -P nrfjprog -a 0xFF000 -l 0x1000 -I <lic-file>`  
    For nRF52832: `mira_license.py license -s <snr> -P nrfjprog -a 0x7F000 -l 0x1000 -I <lic-file>`  
    Read more about licensing in the [documentation](https://docs.lumenrad.io/miraos/latest/description/licensing/licensing_tool.html).

9. Flash the firmware:

    `west flash --dev-id <snr>`

10. Connect to the development kit's serial port.

After a reset, you should see a message like:
`"-------------MiraMesh network example-------------"`.

A continuous stream of `"Waiting for network..."` indicates that everything works as it should.

To receive the messages, a root/receiver device is required.
The network_receiver example from MiraOS examples can be used,
but it is also possible to use this example's firmware:

Configure one device to be a root/receiver by running:
```
nrfjprog --eraseuicr -s <snr>
nrfjprog --memwr 0x100010C0 --val 0x1234 -s <snr>
nrfjprog --reset -s <snr>
```
Which erases the UICR's memory, writes 0x1234 to `UICR->CUSTOMER[16]` register, then resets the device.

Note: MiraMesh connection times can improve if you reset the network
sender after initializing the receiver.

## Firmware update via MCUboot

The example by default uses MCUboot to support firmware updates locally and also over the network using FOTA.

The firmware update can be done by [installing an updated firmware image](#local-firmware-update)
in swap memory region using nrfjprog.

The firmware installed with nrfjprog can be further propagated to other
devices in the network using
[Miramesh's firmware transfer method](#propagating-the-installed-firmware-with-mirameshs-built-in-firmware-transfer).

### Local firmware update

To create a upgrade image and install it:
1. Build the new application with west.
2. Run `create_update_image.py` to create an upgrade image.  

    The python requirements are in the requirements.txt file,
    install via `pip3 install -r requirements.txt`.  
    Use `--help` as argument to `create_update_image.py` to see its
    possible arguments.  
    Its default arguments are suitable for nRF52840.  
    For nRF52832 use: `./create_update_image.py --backup-header-start 0x7d000 --backup-trailer-start 0x7e000`

3. To install it, run:

    `nrfjprog --program update_app.hex --sectorerase --verify -r`

### Propagating the installed firmware with Miramesh's built in firmware transfer

To enable the firmware transfer, the following config needs to be
added to prj.conf: `CONFIG_MIRA_FOTA_INIT=y`.

The updated firmware has to be installed locally on the root. The root will
then propagate the firmware to the rest of the nodes in the network.

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

`nrfutil toolchain-manager launch --ncs-version v2.5.0 --shell`

### "ERROR: Could not find a package configuration file provided by "Zephyr"...

This is likely due to missing reference to the Zephyr CMake package. This  can be
resolved by running:

`west zephyr-export`