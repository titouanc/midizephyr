#ifndef ENCODER_H
#define ENCODER_H

#include <zephyr.h>
#include "color.h"

#define ENCODER_DT_SPEC(name) {\
    .i2c=DEVICE_DT_GET(DT_BUS(DT_NODELABEL(name))),\
    .addr=DT_REG_ADDR(DT_NODELABEL(name))\
}

/*
}
*/

typedef struct {
    const struct device *i2c;
    uint8_t addr;
} encoder;

int encoder_init(const encoder *enc);

int encoder_set_color(const encoder *enc, color_t color);

int encoder_get_value(const encoder *enc, float *value);

#endif
