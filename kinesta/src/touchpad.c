#include "touchpad.h"

#include <drivers/pwm.h>
#include <logging/log.h>
LOG_MODULE_REGISTER(touchpad);

int touchpad_init(const touchpad *pad)
{
    int r;

    printk("pad->out: %s %d\n", pad->out.port->name, pad->out.pin);
    r = gpio_pin_configure_dt(&pad->out, GPIO_INPUT | pad->out.dt_flags);
    if (r){
        LOG_ERR("Unable to configure pad->out");
        return r;
    }

    printk("pad->r: %s %d\n", pad->r.port->name, pad->r.pin);
    if (!device_is_ready(pad->r.port)) {
        printk("Error: PWM device %s is not ready\n", pad->r.port->name);
        return -EAGAIN;
    }

    printk("pad->g: %s %d\n", pad->g.port->name, pad->g.pin);
    if (!device_is_ready(pad->g.port)) {
        printk("Error: PWM device %s is not ready\n", pad->g.port->name);
        return -EAGAIN;
    }

    printk("pad->b: %s %d\n", pad->b.port->name, pad->b.pin);
    if (!device_is_ready(pad->b.port)) {
        printk("Error: PWM device %s is not ready\n", pad->b.port->name);
        return -EAGAIN;
    }

    return 0;
}

#define PWM_OVERSAMPLE_BITS 2
#define PWM_PERIOD (1 << (PWM_OVERSAMPLE_BITS + TOUCHPAD_COLOR_CHAN_BITS))

static int touchpad_set_color_channel(const struct gpio_dt_spec *spec, int value)
{
    return pwm_pin_set_usec(
        spec->port, spec->pin,
        PWM_PERIOD,
        (value << PWM_OVERSAMPLE_BITS),
        spec->dt_flags
    );
}

int touchpad_set_color(const touchpad *pad, uint32_t color)
{
    int r = (color >> (2 * TOUCHPAD_COLOR_CHAN_BITS)) & TOUCHPAD_COLOR_CHAN_MAX;
    int g = (color >> TOUCHPAD_COLOR_CHAN_BITS) & TOUCHPAD_COLOR_CHAN_MAX;
    int b = (color >>  0) & TOUCHPAD_COLOR_CHAN_MAX;

    if (touchpad_set_color_channel(&pad->r, r)) {
        LOG_ERR("Unable to set pad->r");
        return -EIO;
    }
    if (touchpad_set_color_channel(&pad->g, g)) {
        LOG_ERR("Unable to set pad->g");
        return -EIO;
    }
    if (touchpad_set_color_channel(&pad->b, b)) {
        LOG_ERR("Unable to set pad->b");
        return -EIO;
    }
    return 0;
}
