#include <zephyr.h>
#include <device.h>
#include <devicetree.h>
#include <drivers/gpio.h>
#include <drivers/sensor.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(app);

#include "rgb_pad.h"

const rgb_pad pad = RGB_PAD_DT_SPEC(pad);

const int THRESHOLD_CM = 60;

void main(void)
{
    int distance_cm, measured_distance_cm, color_intensity;
    struct sensor_value value;
    const struct device *sensor = device_get_binding("VL53L0X_L");

    if (! sensor){
        return;
    }
    if (configure_pad(&pad)){
        return;
    }

    set_pad_color(&pad, COLOR_RGB(0, 0, 0));

    bool is_in_tracking_zone = false;

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

        bool is_now_in_tracking_zone = false;
        if (is_in_tracking_zone){
            is_now_in_tracking_zone = distance_cm < THRESHOLD_CM;
        } else {
            is_now_in_tracking_zone = distance_cm < THRESHOLD_CM - 5;
        }
        is_in_tracking_zone = is_now_in_tracking_zone;

        if (gpio_pin_get_dt(&pad.out) > 0){
            set_pad_color(&pad, COLOR_RGB(color_intensity, color_intensity, 0));
        } else if (is_in_tracking_zone){
            int red = (THRESHOLD_CM - distance_cm) * COLOR_CHAN_MAX / THRESHOLD_CM;
            int green = distance_cm * COLOR_CHAN_MAX / THRESHOLD_CM;
            set_pad_color(&pad, COLOR_RGB(red, green, 0));
        } else if (distance_cm < 150) {
            set_pad_color(&pad, COLOR_RGB(0, 0, COLOR_CHAN_MAX/4));
        } else {
            set_pad_color(&pad, COLOR_RGB(0, 0, 0));
        }
    }
}
