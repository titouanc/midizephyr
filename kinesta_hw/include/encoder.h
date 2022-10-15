#ifndef ENCODER_H
#define ENCODER_H

#include <zephyr/device.h>
#include "color.h"

#define ENCODER_EVT_PRESS         (1 << 0)
#define ENCODER_EVT_RELEASE       (1 << 1)
#define ENCODER_EVT_DOUBLE_CLICK  (1 << 2)
#define ENCODER_EVT_VALUE_CHANGED (1 << 3)

struct encoder_callback_t {
    void (*func)(struct encoder_callback_t *callback, int evt);
};

int encoder_set_color(const struct device *dev, color_t color);

int encoder_get_value(const struct device *dev, float *value);

int encoder_set_value(const struct device *dev, float value);

int encoder_get_event(const struct device *dev, int *evt);

void encoder_set_callback(const struct device *dev, struct encoder_callback_t *cb);

#endif
