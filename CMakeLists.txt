cmake_minimum_required(VERSION 3.20.0)

set(SUPPORTED_BOARDS  "nrf52840dk_nrf52840"  "nrf52dk_nrf52832")

# Check if the selected board is supported
list(FIND SUPPORTED_BOARDS ${BOARD} BOARD_INDEX)
if (${BOARD_INDEX} EQUAL -1)
	set(ERROR_MESSAGE "  chosen --board ${BOARD} not supported! Supported boards are:\n")
	foreach(board IN LISTS SUPPORTED_BOARDS)
		set(ERROR_MESSAGE "${ERROR_MESSAGE}  ${board}\n")
	endforeach()

	message(FATAL_ERROR "${ERROR_MESSAGE}")
endif()

set(DTC_OVERLAY_FILE ${CMAKE_CURRENT_SOURCE_DIR}/boards/${BOARD}.overlay)

find_package(Zephyr)# HINTS $ENV{ZEPHYR_BASE})
project(my_project)

target_sources(app PRIVATE
  src/main.c
)

zephyr_library_include_directories(.)
