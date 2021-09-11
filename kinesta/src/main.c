#include <zephyr.h>
#include <device.h>
#include <devicetree.h>
#include <drivers/gpio.h>
#include <drivers/sensor.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(app);

#include "rgb_pad.h"

const rgb_pad pad = RGB_PAD_DT_SPEC(pad);

void main(void)
{
    int distance_cm, color_intensity;
    struct sensor_value value;
    const struct device *sensor = device_get_binding("VL53L0X_C");
    if (! sensor){
        return;
    }
    if (configure_pad(&pad)){
        return;
    }

    set_pad_color(&pad, COLOR_RGB(0, 0, 0));

    while (1){
        if (sensor_sample_fetch(sensor)){
            return;
        }

        if (sensor_channel_get(sensor, SENSOR_CHAN_DISTANCE, &value)){
            return;
        }
        distance_cm = 100 * sensor_value_to_double(&value);
        if (distance_cm < 64){
            color_intensity = (64 - distance_cm) << 4;
            set_pad_color(&pad, COLOR_RGB(0, 0, color_intensity));
        } else {
            set_pad_color(&pad, COLOR_RGB(0, 0, 0));
        }
    }
}
