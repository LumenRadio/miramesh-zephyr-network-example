cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr)# HINTS $ENV{ZEPHYR_BASE})

project(my_project)

target_sources(app PRIVATE
  src/main.c
)

if (CONFIG_MIRA_FOTA_INIT)
  target_sources(app PRIVATE
    src/fota_driver/fota_driver.c
    src/dfu/ble.c
    src/dfu/image_handling.c
  )
endif ()

zephyr_library_include_directories(.
  src/fota_driver
  src/dfu)
