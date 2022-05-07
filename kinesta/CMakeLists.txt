# SPDX-License-Identifier: Apache-2.0

# set(DTC_OVERLAY_FILE src/kinesta-mini.dts)
set(DTC_OVERLAY_FILE src/kinesta.dts)

set(SHIELD x_nucleo_53l0a1)

list(APPEND ZEPHYR_EXTRA_MODULES ${CMAKE_CURRENT_SOURCE_DIR}/../usb_midi)
list(APPEND ZEPHYR_EXTRA_MODULES ${CMAKE_CURRENT_SOURCE_DIR}/../kinesta_hw)

cmake_minimum_required(VERSION 3.13.1)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(kinesta)
FILE(GLOB app_sources src/*.c)
target_sources(app PRIVATE ${app_sources})
