#include "kinesta_functional_block.h"
#include "config.h"
#include "usb_midi.h"
#include "kinesta_midi.h"
#include "lookup.h"

#include <zephyr.h>
#include <device.h>
#include <devicetree.h>
#include <drivers/sensor.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(kfb);

static int kfb_measure_distance_cm(kinesta_functional_block *self, double *res)
{
    int r = sensor_sample_fetch(self->distance_sensor);
    if (r){
        return r;
    }
    
    struct sensor_value distance_value;
    r = sensor_channel_get(self->distance_sensor, SENSOR_CHAN_DISTANCE, &distance_value);
    if (r){
        return r;
    }
    
    *res = 100 * sensor_value_to_double(&distance_value);
    return 0;
}

/* Distance remapped on 0..1
 *   Below 0 is above the tracking zone
 *   0 is the highest position in the tracking zone
 *   1 is the lowest position
 */
static inline double kfb_get_distance_t(kinesta_functional_block *self)
{
    return 1 - (self->filtered_distance_cm / DISTANCE_SENSOR_TRACKING_ZONE_CM);
}

static int kfb_update_distance(kinesta_functional_block *self)
{
    double measured_distance_cm;
    int r = kfb_measure_distance_cm(self, &measured_distance_cm);
    if (r){
        return r;
    }

    if (self->is_in_tracking_zone){
        self->filtered_distance_cm = (
            DISTANCE_SENSOR_FILTERING_ALPHA * measured_distance_cm +
            (1 - DISTANCE_SENSOR_FILTERING_ALPHA) * self->filtered_distance_cm
        );
    } else {
        self->filtered_distance_cm = measured_distance_cm;
    }

    // Hysteresis on enter/exit tracking zone
    double threshold = self->is_in_tracking_zone ?
        DISTANCE_SENSOR_TRACKING_ZONE_CM :
        DISTANCE_SENSOR_TRACKING_ZONE_CM - DISTANCE_SENSOR_TRACKING_HYSTERESIS_CM;
    self->is_in_tracking_zone = self->filtered_distance_cm < threshold;

    double t = kfb_get_distance_t(self);
    uint8_t distance_midi_cc_value = (t < 0) ? 0 : (127 * t);
    if (distance_midi_cc_value != self->distance_midi_cc_value && ! self->is_frozen){
        const uint8_t pkt[] = MIDI_CONTROL_CHANGE(0, self->midi_cc_group | 1, distance_midi_cc_value);
        kinesta_midi_out(pkt);
        self->distance_midi_cc_value = distance_midi_cc_value;
    }
    return 0;
}

static int kfb_update_primary_touchpad(kinesta_functional_block *self)
{
    int64_t now = k_uptime_get();
    self->was_primary_pad_touched = self->is_primary_pad_touched;
    self->is_primary_pad_touched = touchpad_is_touched(self->primary_touchpad) > 0;

    // Update touchpad color
    color_t color;
    if (self->is_primary_pad_touched){
        // Pad touched: white
        color = COLOR_WHITE;
        if (! self->was_primary_pad_touched){
            self->is_frozen = ! self->is_frozen;
        }
    } else if (self->is_frozen) {
        // Frozen to a MIDI value: blink in the color map
        float t = (float) self->distance_midi_cc_value / 127;
        color = color_map(COLOR_GREEN, COLOR_RED, t);
        color = color_mul(color, cos256f32[(now >> 1) & 0xff]);
    } else if (self->is_in_tracking_zone){
        // In tracking zone: colormap green to red
        color = color_map(COLOR_GREEN, COLOR_RED, kfb_get_distance_t(self));
    } else if (self->filtered_distance_cm < 3 * DISTANCE_SENSOR_TRACKING_ZONE_CM) {
        // Above the sensor but out of tracking zone: blue
        color = COLOR_BLUE;
    } else if (usb_midi_is_configured()) {
        // Otherwise: light cyan if USB is configured
        color = color_mul(COLOR_CYAN, 0.05);
    } else {
        // Otherwise: light magenta if USB not configured
        color = color_mul(COLOR_MAGENTA, 0.05);
    }
    touchpad_set_color(self->primary_touchpad, color);
    return 0;
}

static int kfb_update_secondary_touchpad(kinesta_functional_block *self)
{
    self->was_secondary_pad_touched = self->is_secondary_pad_touched;
    self->is_secondary_pad_touched = touchpad_is_touched(self->secondary_touchpad);
    if (self->was_secondary_pad_touched != self->is_secondary_pad_touched) {
        const uint8_t pkt[] = MIDI_CONTROL_CHANGE(0, self->midi_cc_group | 2, 127 * self->is_secondary_pad_touched);
        kinesta_midi_out(pkt);
    }

    color_t color = self->is_secondary_pad_touched ? COLOR_WHITE : 0;
    touchpad_set_color(self->secondary_touchpad, color);
    return 0;
}

static int kfb_update_encoder(kinesta_functional_block *self, int evt)
{
    int r;
    if (evt & ENCODER_EVT_PRESS){
        // Click on the encoder: reset value
        r = encoder_get_value(self->encoder, &self->encoder_value);
        if (r){
            return r;
        }
        // If the actual encoder value is 0: set to 1, otherwise set to 0
        self->encoder_value = (self->encoder_value == 0) ? 1 : 0;
        r = encoder_set_value(self->encoder, self->encoder_value);
        if (r){
            return r;
        }
    } else {
        // Otherwise get actual value
        r = encoder_get_value(self->encoder, &self->encoder_value);
        if (r){
            return r;
        }
    }

    uint8_t encoder_midi_cc_value = 127 * self->encoder_value;
    if (encoder_midi_cc_value != self->encoder_midi_cc_value){
        const uint8_t pkt[] = MIDI_CONTROL_CHANGE(0, self->midi_cc_group | 3, encoder_midi_cc_value);
        kinesta_midi_out(pkt);
        self->encoder_midi_cc_value = encoder_midi_cc_value;
    }
    color_t color = color_map(COLOR_GREEN, COLOR_RED, self->encoder_value);
    return encoder_set_color(self->encoder, color_mul(color, 0.66));
}

static void kfb_encoder_changed(struct encoder_callback_t *callback, int event)
{
    kinesta_functional_block *self = CONTAINER_OF(callback, kinesta_functional_block, encoder_change);
    kfb_update_encoder(self, event);
}

int kfb_init(kinesta_functional_block *self)
{
    if (! device_is_ready(self->distance_sensor)){
        LOG_ERR("Invalid device for distance sensor");
        // return -1;
    }

    if (! device_is_ready(self->encoder)){
        LOG_ERR("Invalid device for encoder");
        // return -1;
    }

    self->encoder_change.func = kfb_encoder_changed;
    encoder_set_callback(self->encoder, &self->encoder_change);

    int r = kfb_update_encoder(self, 0);
    if (r){
        // return r;
    }

    return kfb_update(self);
}

int kfb_update(kinesta_functional_block *self)
{
    if (self->soft_disable){
        touchpad_set_color(self->primary_touchpad, 0);
        touchpad_set_color(self->secondary_touchpad, 0);
        encoder_set_color(self->encoder, 0);
        return 0;
    }

    int r = kfb_update_distance(self);
    if (r){
        return r;
    }

    r = kfb_update_primary_touchpad(self);
    if (r){
        return r;
    }

    return kfb_update_secondary_touchpad(self);
}
