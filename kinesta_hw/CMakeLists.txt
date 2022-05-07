# SPDX-License-Identifier: Apache-2.0

if(CONFIG_KINESTA_HW)
  zephyr_include_directories(include)

  zephyr_library()
  zephyr_library_sources(
    drivers/encoder.c
    drivers/touchpad_gpio.c
    drivers/touchpad_pwm.c
  )
endif()
