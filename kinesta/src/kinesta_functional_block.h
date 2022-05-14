#ifndef KINESTA_FUNCTIONAL_BLOCK_H
#define KINESTA_FUNCTIONAL_BLOCK_H

#include "encoder.h"
#include "touchpad.h"


typedef struct {
    const char *name;
    const uint8_t midi_cc_group;
    const struct device *distance_sensor;
    const struct device *primary_pad;
    const struct device *secondary_pad;
    const struct device *rgb_encoder;

    // Events
    struct encoder_callback_t encoder_change;

    // Sensor input values
    double filtered_distance_cm;
    float encoder_value;
    bool is_in_tracking_zone;
    bool is_pad_touched;
    bool was_pad_touched;

    // MIDI-CC values
    uint8_t distance_midi_cc_value;
    uint8_t encoder_midi_cc_value;

    // Software functions
    bool is_frozen;
} kinesta_functional_block;

int kfb_init(kinesta_functional_block *self);

int kfb_update(kinesta_functional_block *self);

#endif
