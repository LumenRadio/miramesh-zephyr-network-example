# Miramesh Zephyr network sender
This example demonstrates the integration of [MiraMesh](https://docs.lumenrad.io/miraos/latest/) into the Zephyr environment using the nRF Connect SDK. The produced binary is a Zephyr application that continuously transmits messages via MiraMesh. To begin, you can either use this repository as a manifest repository or import the project into another west manifest. Using this repository directly is easier but requires more disk space and will take longer initially due to the need to download ncs and zephyr. This project is compatible and has been tested with nRF Connect SDK version 2.5.0. Other versions might work as well.


##  MiraMesh Zephyr network sender as an imported project
If you have an existing nRF Connect SDK west workspace follow these steps:

1. Add the LumenRadio github as a remote to `nrf/west.yml`
    ```
    remotes:
        ...
        - name: lumenradio
        url-base: https://github.com/LumenRadio
    ```

2. Add the project and its dependency [miramesh-zephyr](https://github.com/LumenRadio/miramesh-zephyr) to `nrf/west.yml`
    ```
        projects:
        ...
        - name: miramesh-zephyr-network-example
          repo-path: miramesh-zephyr-network-example
          remote: lumenradio
          revision: main
          path: app
          import:
            name-allowlist: miramesh-zephyr
    ```

## MiraMesh Zephyr network sender as the manifest repository
1. [Install nRF Connect toolchain](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/installation/install_ncs.html)
2. Create a shell set up for nRF connect toolchain
    ```
    nrfutil toolchain-manager launch --ncs-version v2.5.0 --shell
    ```
    Other toolkit versions might work. This has been tested and verified with v.2.5.0.
3. Initialize the project workspace
    ```
    west init -m https://github.com/LumenRadio/miramesh-zephyr-network-example --mr main <path_to_west_workspace>
    ```

## Building
1. Create a shell set up for nRF connect toolchain if you haven't already done so:
    ```
    nrfutil toolchain-manager launch --ncs-version v2.5.0 --shell
    ```
2. Navigate to the workspace
    ```
    cd <path_to_west_workspace>
    ```
3. Update the repository
    ```
    west update
    ```
4. Extract the libmira archive into vendor/libmira
5. Build the application. For nRF52840:
    ```
    west build -b nrf52840dk_nrf52840 -s app
    ```
    For nRF52832:
    ```
    west build -b nrf52dk_nrf52832 -s app
    ```
6. Connect either an nRF52DK or an nRF52840DK
7. Recover the device in case it is in APPROTECT mode
    ```
    nrfjprog --recover -s <snr>
    ```
    Find the serial number (snr) of the programmer with nrfjprog --ids.
8. Program a Mira license onto the device. For nRF52840:
    ```
    ./mira_license.py license -s <snr> -P nrfjprog -a 0xFF000 -l 0x1000 -I <lic-file>
    ```
    For nRF52832:
    ```
    ./mira_license.py license -s <snr> -P nrfjprog -a 0x7F000 -l 0x1000 -I <lic-file>
    ```
    For more on licensing, see the [documentation](https://docs.lumenrad.io/miraos/latest/description/licensing/licensing_tool.html).
9. Flash the firmware
    ```
    west flash --dev-id <snr>
    ```
10. Connect to the development kit's serial port. After a reset, you should see a message like "-------------------MiraMesh Sender-------------------." A continuous stream of "Waiting for network..." indicates a successful license programming. To send messages, a receiver device is required, which you can set up using the network-receiver example [here](https://github.com/LumenRadio/mira-examples). Note: MiraMesh connection times can improve if you reset the network sender after initializing the receiver.

## Update via MCUboot
The example by default uses MCUboot to support updates via FOTA.

To create a upgrade image and install it:
1. Build the new application with west.
2. Run `create_update_image.py` to create an upgrade image.
   The python requirements are in the requirements.txt file,
   install via `pip3 install -r requirements.txt`.
   Use `--help` as argument to `create_update_image.py` to see its
   possible arguments.
   Its default arguments are suitable for nRF52840.
   For nRF52832 use:
   ```
   ./create_update_image.py --backup-header-start 0x7d000 --backup-trailer-start 0x7e000
   ```
3. Install it.
   Run:
   ```
   nrfjprog --program update_app.hex --sectorerase --verify -r
   ```
