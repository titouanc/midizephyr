#ifndef TOUCHPAD_H
#define TOUCHPAD_H

#include <drivers/gpio.h>


typedef struct {
    struct gpio_dt_spec out;
    struct gpio_dt_spec r;
    struct gpio_dt_spec g;
    struct gpio_dt_spec b;
} touchpad;

#define PWM_DT_SPEC_GET(node) {              \
    .port=DEVICE_DT_GET(DT_PWMS_CTLR(node)), \
    .pin=DT_PWMS_CHANNEL(node),              \
    .dt_flags=DT_PWMS_FLAGS(node),           \
}

#define TOUCHPAD_DT_SPEC(name) {                               \
    .out = GPIO_DT_SPEC_GET(DT_NODELABEL(name##_out), gpios), \
    .r = PWM_DT_SPEC_GET(DT_NODELABEL(name##_pwm_r)),         \
    .g = PWM_DT_SPEC_GET(DT_NODELABEL(name##_pwm_g)),         \
    .b = PWM_DT_SPEC_GET(DT_NODELABEL(name##_pwm_b)),         \
}


#define TOUCHPAD_COLOR_CHAN_BITS 10
#define TOUCHPAD_COLOR_CHAN_MAX ((1 << TOUCHPAD_COLOR_CHAN_BITS) - 1)
#define TOUCHPAD_COLOR_RGB(r, g, b)               \
    (                                    \
        ((r) << (2 * TOUCHPAD_COLOR_CHAN_BITS)) | \
        ((g) << TOUCHPAD_COLOR_CHAN_BITS) |       \
        (b)                              \
    )

int touchpad_init(const touchpad *pad);

int touchpad_set_color(const touchpad *pad, uint32_t color);

#endif
