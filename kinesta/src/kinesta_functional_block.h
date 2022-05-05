#ifndef KINESTA_FUNCTIONAL_BLOCK_H
#define KINESTA_FUNCTIONAL_BLOCK_H

#include "encoder.h"
#include "touchpad.h"

typedef struct {
    const struct device *distance_sensor;
    const touchpad pad;
    const touchpad pad2;
    const encoder rgb_encoder;
    const uint8_t cc_high_byte;

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
    int64_t last_pad_blink;
    bool is_blink_on;
} kinesta_functional_block;

int kfb_init(kinesta_functional_block *self);

int kfb_update(kinesta_functional_block *self);

int kfb_update_encoder(kinesta_functional_block *self);

#endif
