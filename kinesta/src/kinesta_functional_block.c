#include "kinesta_functional_block.h"
#include "config.h"
#include "usb_midi.h"

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

int kfb_init(kinesta_functional_block *self)
{
    int r = kfb_measure_distance_cm(self, &self->filtered_distance_cm);
    if (r){
        return r;
    }
    
    r = touchpad_init(&self->pad);
    if (r){
        return r;
    }

    return encoder_init(&self->rgb_encoder);
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
    if (distance_midi_cc_value != self->distance_midi_cc_value){
        const uint8_t pkt[] = MIDI_CONTROL_CHANGE(0, 1, distance_midi_cc_value);
        usb_midi_write(USB_MIDI_SENSORS_JACK_ID, pkt);
        self->distance_midi_cc_value = distance_midi_cc_value;
    }
    return 0;
}

static int kfb_update_pad(kinesta_functional_block *self)
{
    self->is_pad_touched = gpio_pin_get_dt(&self->pad.out) > 0;

    // Update touchpad color
    color_t color;
    if (self->is_pad_touched){
        // Pad touched: white
        color = COLOR_WHITE;
    } else if (self->is_in_tracking_zone){
        // In tracking zone: colormap green to red
        color = color_map(COLOR_GREEN, COLOR_RED, kfb_get_distance_t(self));
    } else if (self->filtered_distance_cm < 3 * DISTANCE_SENSOR_TRACKING_ZONE_CM) {
        // Above the sensor but out of tracking zone: blue
        color = COLOR_BLUE;
    } else if (usb_midi_is_configured()) {
        // Otherwise: light cyan if USB is configured
        color = COLOR_RGB(0, COLOR_CHAN_MAX / 64, COLOR_CHAN_MAX / 64);
    } else {
        // Otherwise: light red if USB not configured
        color = COLOR_RGB(COLOR_CHAN_MAX / 64, 0, 0);
    }
    touchpad_set_color(&self->pad, color);
    return 0;
}

static int kfb_update_encoder(kinesta_functional_block *self)
{
    int r = encoder_get_value(&self->rgb_encoder, &self->encoder_value);
    if (r){
        return r;
    }
    uint8_t encoder_midi_cc_value = 127 * self->encoder_value;
    if (encoder_midi_cc_value != self->encoder_midi_cc_value){
        const uint8_t pkt[] = MIDI_CONTROL_CHANGE(0, 2, encoder_midi_cc_value);
        usb_midi_write(USB_MIDI_SENSORS_JACK_ID, pkt);
        self->encoder_midi_cc_value = encoder_midi_cc_value;
    }
    color_t color = color_map(COLOR_GREEN, COLOR_RED, self->encoder_value);
    return encoder_set_color(&self->rgb_encoder, color);
}

int kfb_update(kinesta_functional_block *self)
{
    int r = kfb_update_distance(self);
    if (r){
        return r;
    }

    r = kfb_update_pad(self);
    if (r){
        return r;
    }

    return kfb_update_encoder(self);    
}
