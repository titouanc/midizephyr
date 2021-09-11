#ifndef RGB_PAD_H
#define RGB_PAD_H

#include <drivers/gpio.h>


typedef struct {
    struct gpio_dt_spec out;
    struct gpio_dt_spec r;
    struct gpio_dt_spec g;
    struct gpio_dt_spec b;
} rgb_pad;

#define PWM_DT_SPEC_GET(node) {              \
    .port=DEVICE_DT_GET(DT_PWMS_CTLR(node)), \
    .pin=DT_PWMS_CHANNEL(node),              \
    .dt_flags=DT_PWMS_FLAGS(node),           \
}

#define RGB_PAD_DT_SPEC(name) {                               \
    .out = GPIO_DT_SPEC_GET(DT_NODELABEL(name##_out), gpios), \
    .r = PWM_DT_SPEC_GET(DT_NODELABEL(name##_pwm_r)),         \
    .g = PWM_DT_SPEC_GET(DT_NODELABEL(name##_pwm_g)),         \
    .b = PWM_DT_SPEC_GET(DT_NODELABEL(name##_pwm_b)),         \
}

int configure_pad(const rgb_pad *pad);

#define COLOR_CHAN_BITS 10
#define COLOR_CHAN_MAX ((1 << COLOR_CHAN_BITS) - 1)
#define COLOR_RGB(r, g, b)               \
    (                                    \
        ((r) << (2 * COLOR_CHAN_BITS)) | \
        ((g) << COLOR_CHAN_BITS) |       \
        (b)                              \
    )

int set_pad_color(const rgb_pad *pad, uint32_t color);

#endif
