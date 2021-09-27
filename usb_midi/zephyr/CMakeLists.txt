# SPDX-License-Identifier: Apache-2.0

if(CONFIG_USB_MIDI)
  zephyr_include_directories(.)

  zephyr_library()
  zephyr_library_sources(usb_midi.c)
endif()
