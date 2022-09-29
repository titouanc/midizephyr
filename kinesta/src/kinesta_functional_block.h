#ifndef KINESTA_FUNCTIONAL_BLOCK_H
#define KINESTA_FUNCTIONAL_BLOCK_H

#include "encoder.h"
#include "touchpad.h"

/*
#define KFB_FROM_DT(inst) \
    {\
        .name=DT_INST_PROP(inst, label),\
        .midi_cc_group=DT_INST_PROP(inst, midi_cc_group),\
        .distance_sensor=DEVICE_DT_GET(DT_INST_PROP(inst, distance_sensor)),\
        .primary_touchpad=DEVICE_DT_GET(DT_INST_PROP(inst, primary_touchpad)),\
        .secondary_touchpad=DEVICE_DT_GET(DT_INST_PROP(inst, secondary_touchpad)),\
        .encoder=DEVICE_DT_GET(DT_INST_PROP(inst, encoder)),\
    },

DT_FOREACH_STATUS_OKAY(kinesta_functional_block, KFB_FROM_DT)
*/

typedef struct {
    bool soft_disable;

    const char *name;
    const uint8_t midi_cc_group;
    const struct device *distance_sensor;
    const struct device *primary_touchpad;
    const struct device *secondary_touchpad;
    const struct device *encoder;

    // Events
    struct encoder_callback_t encoder_change;

    // Sensor input values
    double filtered_distance_cm;
    float encoder_value;
    bool is_in_tracking_zone;
    bool is_primary_pad_touched;
    bool was_primary_pad_touched;
    bool is_secondary_pad_touched;
    bool was_secondary_pad_touched;

    // MIDI-CC values
    uint8_t distance_midi_cc_value;
    uint8_t encoder_midi_cc_value;

    // Software functions
    bool is_frozen;
} kinesta_functional_block;

int kfb_init(kinesta_functional_block *self);

int kfb_update(kinesta_functional_block *self);

#endif
