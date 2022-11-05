# SPDX-License-Identifier: Apache-2.0

set(DTC_OVERLAY_FILE src/usb-midi.dts)

list(APPEND ZEPHYR_EXTRA_MODULES ${CMAKE_CURRENT_SOURCE_DIR}/../usb_midi)

cmake_minimum_required(VERSION 3.20)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(midizephyr)

target_sources(app PRIVATE src/main.c src/strerrno.c)
