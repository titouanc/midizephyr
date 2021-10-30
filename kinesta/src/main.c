#include <zephyr.h>
#include <device.h>
#include <devicetree.h>
#include <drivers/gpio.h>
#include <drivers/sensor.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(app);

#include "touchpad.h"
#include "encoder.h"

const struct device *sensor;

const touchpad pad = TOUCHPAD_DT_SPEC(pad1);

const encoder encoders[] = {
    ENCODER_DT_SPEC(encoder1),
};

const int THRESHOLD_CM = 60;

void main(void)
{
    int i;
    int distance_cm, measured_distance_cm, color_intensity;
    struct sensor_value value;
    sensor = device_get_binding("VL53L0X_L");

    if (! sensor){
        return;
    }

    if (touchpad_init(&pad)){
        return;
    }
    
    for (i=0; i<ARRAY_SIZE(encoders); i++){
        if (encoder_init(&encoders[i])){
            return;
        }
    }

    touchpad_set_color(&pad, COLOR_RGB(0, 0, 0));

    color_t touchpad_color;
    float encoder_value;
    bool is_in_tracking_zone = false;
    bool is_now_in_tracking_zone = false;

    printk("Starting mainloop !\n");
    while (1){
        if (sensor_sample_fetch(sensor)){
            return;
        }

        if (sensor_channel_get(sensor, SENSOR_CHAN_DISTANCE, &value)){
            return;
        }
        measured_distance_cm = 100 * sensor_value_to_double(&value);

        if (is_in_tracking_zone){
            distance_cm = (measured_distance_cm + 3*distance_cm) / 4;
        } else {
            distance_cm = measured_distance_cm;
        }

        // Hysteresis on enter/exit tracking zone
        is_now_in_tracking_zone = false;
        if (is_in_tracking_zone){
            is_now_in_tracking_zone = distance_cm < THRESHOLD_CM;
        } else {
            is_now_in_tracking_zone = distance_cm < THRESHOLD_CM - 5;
        }
        is_in_tracking_zone = is_now_in_tracking_zone;

        // Update touchpad color
        if (gpio_pin_get_dt(&pad.out) > 0){
            // Pad touched: yellow with intensity = current intensity
            touchpad_color = COLOR_WHITE;
        } else if (is_in_tracking_zone){
            // In tracking zone: colormap green to red
            touchpad_color = color_map(COLOR_RED, COLOR_GREEN, (float) distance_cm / THRESHOLD_CM);
        } else if (distance_cm < 150) {
            // Above the sensor but out of tracking zone: blue
            touchpad_color = COLOR_BLUE;
        } else {
            // Otherwise: light cyan
            touchpad_color = COLOR_RGB(0, COLOR_CHAN_MAX / 64, COLOR_CHAN_MAX / 64);
        }
        touchpad_set_color(&pad, touchpad_color);

        for (i=0; i<ARRAY_SIZE(encoders); i++){
            if (encoder_get_value(&encoders[i], &encoder_value)){
                printk("Error getting encoder value !\n");
                return;
            }
            encoder_set_color(&encoders[i], color_map(COLOR_GREEN, COLOR_RED, encoder_value));
        }
    }
}
