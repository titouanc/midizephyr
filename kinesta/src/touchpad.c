#include "touchpad.h"

#include <drivers/pwm.h>
#include <logging/log.h>
LOG_MODULE_REGISTER(touchpad);

int touchpad_init(const touchpad *pad)
{
    int r;

    LOG_DBG("pad->out: %s %d\n", pad->out.port->name, pad->out.pin);
    r = gpio_pin_configure_dt(&pad->out, GPIO_INPUT | pad->out.dt_flags);
    if (r){
        LOG_ERR("Unable to configure pad->out");
        return r;
    }

    LOG_DBG("pad->r: %s %d\n", pad->r.port->name, pad->r.pin);
    if (!device_is_ready(pad->r.port)) {
        LOG_ERR("PWM device %s is not ready\n", pad->r.port->name);
        return -EAGAIN;
    }

    LOG_DBG("pad->g: %s %d\n", pad->g.port->name, pad->g.pin);
    if (!device_is_ready(pad->g.port)) {
        LOG_ERR("PWM device %s is not ready\n", pad->g.port->name);
        return -EAGAIN;
    }

    LOG_DBG("pad->b: %s %d\n", pad->b.port->name, pad->b.pin);
    if (!device_is_ready(pad->b.port)) {
        LOG_ERR("PWM device %s is not ready\n", pad->b.port->name);
        return -EAGAIN;
    }

    return 0;
}

#define PWM_OVERSAMPLE_BITS 2
#define PWM_PERIOD (1 << (PWM_OVERSAMPLE_BITS + COLOR_CHAN_BITS))

static int touchpad_set_color_channel(const struct gpio_dt_spec *spec, int value)
{
    return pwm_pin_set_usec(
        spec->port, spec->pin,
        PWM_PERIOD,
        (value << PWM_OVERSAMPLE_BITS),
        spec->dt_flags
    );
}

int touchpad_set_color(const touchpad *pad, color_t color)
{
    if (touchpad_set_color_channel(&pad->r, COLOR_GET_R(color))) {
        LOG_ERR("Unable to set pad->r");
        return -EIO;
    }
    if (touchpad_set_color_channel(&pad->g, COLOR_GET_G(color))) {
        LOG_ERR("Unable to set pad->g");
        return -EIO;
    }
    if (touchpad_set_color_channel(&pad->b, COLOR_GET_B(color))) {
        LOG_ERR("Unable to set pad->b");
        return -EIO;
    }
    return 0;
}
