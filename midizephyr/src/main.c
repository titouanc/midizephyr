/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <device.h>
#include <devicetree.h>
#include <drivers/gpio.h>
#include <usb/usb_device.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(app);

enum {
    INIT,
    USB_ENABLED,
    RUNNING,
} app_state = INIT;

#define SLEEP_TIME_MS   100

#define LED2_NODE  DT_ALIAS(mled2)
#define LED2_LABEL DT_GPIO_LABEL(LED2_NODE, gpios)
#define LED2_PIN   DT_GPIO_PIN(LED2_NODE, gpios)
#define LED2_FLAGS DT_GPIO_FLAGS(LED2_NODE, gpios)

static struct k_delayed_work status_led_work;
static void status_led_task()
{
    static unsigned int tick = 0;
    static bool is_on = true;
    static struct device *led_dev = NULL;

    if (! led_dev){
        led_dev = device_get_binding(LED2_LABEL);
        if (! led_dev){
            return;
        }
        if (gpio_pin_configure(led_dev, LED2_PIN, GPIO_OUTPUT_ACTIVE|LED2_FLAGS) < 0){
            return;
        }
    }

    tick++;

    switch (app_state){
        case INIT:
            is_on = false;
            break;
        case USB_ENABLED:
            if (tick % 10 == 0){
                is_on = ! is_on;
            }
            break;
        case RUNNING:
            is_on = ! is_on;
            break;
        default: break;
    }

    gpio_pin_set(led_dev, LED2_PIN, (int) is_on);
    k_delayed_work_submit(&status_led_work, K_MSEC(SLEEP_TIME_MS));
}

void main(void)
{
    LOG_INF("Starting...");

    k_delayed_work_init(&status_led_work, status_led_task);
    k_delayed_work_submit(&status_led_work, K_NO_WAIT);

    if (usb_enable(NULL) == 0){
        LOG_INF("USB enabled");
        app_state = USB_ENABLED;
    } else {
        LOG_ERR("Failed to enable USB");
        return;
    }
}
