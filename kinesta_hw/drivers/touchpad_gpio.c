#include "touchpad.h"

#include <drivers/gpio.h>
#include <logging/log.h>
LOG_MODULE_REGISTER(touchpad_gpio);

#define DT_DRV_COMPAT kinesta_rgb_touchpad_gpio

struct touchpad_gpio_config {
    struct gpio_dt_spec touch;
    struct gpio_dt_spec led_r;
    struct gpio_dt_spec led_g;
    struct gpio_dt_spec led_b;
};

static int touchpad_gpio_init(const struct device *dev)
{
    const struct touchpad_gpio_config *const config = dev->config;
    const struct gpio_dt_spec *gpios = &config->led_r;

    gpio_pin_configure_dt(&config->touch, GPIO_INPUT | config->touch.dt_flags);

    for (int i=0; i<3; i++){
        if (gpio_pin_configure_dt(&gpios[i], GPIO_OUTPUT | gpios[i].dt_flags)){
            LOG_ERR("Unable to configure %s%d as output, unable to init touchpad_gpio", gpios[i].port->name, gpios[i].pin);
        }
    }

    return 0;
}

static void touchpad_gpio_set_color_channel(const struct device *dev, color_channel_t channel, unsigned value)
{
    __ASSERT(channel < 3, "Invalid color channel !");
    const struct touchpad_gpio_config *const config = dev->config;
    const struct gpio_dt_spec *gpio = &(&config->led_r)[channel];
    if (gpio_pin_set_dt(gpio, value > COLOR_CHAN_MAX/2)){
        LOG_ERR("[%s] Unable to set channel %d (%s%d)", dev->name, channel, gpio->port->name, gpio->pin);
    }
}

const struct touchpad_driver_api touchpad_gpio_api_funcs = {
    .set_color_channel = touchpad_gpio_set_color_channel,
};

#define TOUCHPAD_PWM_INIT(inst) \
    static const struct touchpad_gpio_config touchpad_##inst##_config = {   \
        .touch = GPIO_DT_SPEC_INST_GET(inst, touch_gpios),                  \
        .led_r = GPIO_DT_SPEC_INST_GET(inst, r_gpios),                      \
        .led_g = GPIO_DT_SPEC_INST_GET(inst, g_gpios),                      \
        .led_b = GPIO_DT_SPEC_INST_GET(inst, b_gpios),                      \
    };                                                                      \
                                                                            \
    DEVICE_DT_INST_DEFINE(inst, touchpad_gpio_init, NULL,                   \
                          NULL,                                             \
                          &touchpad_##inst##_config,                        \
                          POST_KERNEL,                                      \
                          CONFIG_GPIO_INIT_PRIORITY,                        \
                          &touchpad_gpio_api_funcs);

DT_INST_FOREACH_STATUS_OKAY(TOUCHPAD_PWM_INIT)
