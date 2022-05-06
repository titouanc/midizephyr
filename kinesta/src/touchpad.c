#include "touchpad.h"

#include <drivers/pwm.h>
#include <logging/log.h>
LOG_MODULE_REGISTER(touchpad);

static int touchpad_init_pwm(const touchpad *pad)
{
    int r;

    LOG_DBG("pad->r: %s %d", pad->r.port->name, pad->r.pin);
    if (!device_is_ready(pad->r.port)) {
        LOG_ERR("PWM device %s is not ready", pad->r.port->name);
        return -EAGAIN;
    }

    LOG_DBG("pad->g: %s %d", pad->g.port->name, pad->g.pin);
    if (!device_is_ready(pad->g.port)) {
        LOG_ERR("PWM device %s is not ready", pad->g.port->name);
        return -EAGAIN;
    }

    LOG_DBG("pad->b: %s %d", pad->b.port->name, pad->b.pin);
    if (!device_is_ready(pad->b.port)) {
        LOG_ERR("PWM device %s is not ready", pad->b.port->name);
        return -EAGAIN;
    }

    return 0;
}

static int touchpad_init_gpio(const touchpad *pad)
{
    int r;

    LOG_DBG("pad->r: %s %d", pad->r.port->name, pad->r.pin);
    if (r = gpio_pin_configure_dt(&pad->r, GPIO_OUTPUT | pad->out.dt_flags)){
        LOG_ERR("Unable to configure pad->r");
        return r;
    }

    LOG_DBG("pad->g: %s %d", pad->g.port->name, pad->g.pin);
    if (r = gpio_pin_configure_dt(&pad->g, GPIO_OUTPUT | pad->out.dt_flags)){
        LOG_ERR("Unable to configure pad->g");
        return r;
    }

    LOG_DBG("pad->b: %s %d", pad->b.port->name, pad->b.pin);
    if (r = gpio_pin_configure_dt(&pad->b, GPIO_OUTPUT | pad->out.dt_flags)){
        LOG_ERR("Unable to configure pad->b");
        return r;
    }

    return 0;
}

int touchpad_init(const touchpad *pad)
{
    LOG_DBG("pad->out: %s %d", pad->out.port->name, pad->out.pin);
    
    int r = gpio_pin_configure_dt(&pad->out, GPIO_INPUT | pad->out.dt_flags);
    if (r){
        LOG_ERR("Unable to configure pad->out");
        return r;
    }

    return pad->is_pwm ? touchpad_init_pwm(pad) : touchpad_init_gpio(pad);
}

#define PWM_OVERSAMPLE_BITS 2
#define PWM_PERIOD (1 << (PWM_OVERSAMPLE_BITS + COLOR_CHAN_BITS))

static inline int touchpad_set_color_channel_pwm(const struct gpio_dt_spec *spec, int value)
{
    return pwm_pin_set_usec(
        spec->port, spec->pin,
        PWM_PERIOD,
        (value << PWM_OVERSAMPLE_BITS),
        spec->dt_flags
    );
}

static inline int touchpad_set_color_channel_gpio(const struct gpio_dt_spec *spec, int value)
{
    return gpio_pin_set_dt(spec, (value & COLOR_CHAN_HIGHEST_BIT_MASK) ? 1 : 0);
}

static inline int touchpad_set_color_channel(const touchpad *pad, const struct gpio_dt_spec *spec, int value)
{
    return pad->is_pwm ? touchpad_set_color_channel_pwm(spec, value) : touchpad_set_color_channel_gpio(spec, value);
}

#warning One of the pads is getting REAAAAALY HOT. You should check the electrical connection

int touchpad_set_color(const touchpad *pad, color_t color)
{
    if (touchpad_set_color_channel(pad, &pad->r, color_get_r(color))) {
        LOG_ERR("Unable to set pad->r");
        return -EIO;
    }
    if (touchpad_set_color_channel(pad, &pad->g, color_get_g(color))) {
        LOG_ERR("Unable to set pad->g");
        return -EIO;
    }
    if (touchpad_set_color_channel(pad, &pad->b, color_get_b(color))) {
        LOG_ERR("Unable to set pad->b");
        return -EIO;
    }
    return 0;
}
