#ifndef TOUCHPAD_H
#define TOUCHPAD_H

#include <device.h>
#include <drivers/gpio.h>

#include "color.h"

__subsystem struct touchpad_driver_api {
    void (*set_color_channel)(const struct device *dev, color_channel_t channel, unsigned value);
};

__syscall void touchpad_set_color(const struct device *dev, color_t color)
{
    const struct touchpad_driver_api *api = dev->api;
    __ASSERT(api->set_color, "Missing api function set_color_channel");
    api->set_color_channel(dev, CHANNEL_RED, color_get_r(color));
    api->set_color_channel(dev, CHANNEL_GREEN, color_get_g(color));
    api->set_color_channel(dev, CHANNEL_BLUE, color_get_b(color));
}

__syscall bool touchpad_is_touched(const struct device *dev)
{
    /*
     * This assumes that all touchpad driver configs start with a gpio_dt_spec
     * as first member !
     */
    const struct gpio_dt_spec *touch = dev->config;
    return gpio_pin_get_dt(touch);
}

#endif
