#ifndef KINESTA_FUNCTIONAL_BLOCK_H
#define KINESTA_FUNCTIONAL_BLOCK_H

#include "encoder.h"
#include "touchpad.h"

typedef struct {
    const struct device *distance_sensor;
    const touchpad pad;
    const encoder rgb_encoder;

    double filtered_distance_cm;
    float encoder_value;
    bool is_in_tracking_zone;
    bool is_pad_touched;
} kinesta_functional_block;

int kfb_init(kinesta_functional_block *self);

int kfb_update(kinesta_functional_block *self);

#endif
