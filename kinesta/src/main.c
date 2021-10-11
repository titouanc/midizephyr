#include <zephyr.h>
#include <device.h>
#include <devicetree.h>
#include <drivers/gpio.h>
#include <drivers/sensor.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(app);

#include "touchpad.h"
#include "encoder.h"

const touchpad pad = TOUCHPAD_DT_SPEC(pad);

const encoder enc = ENCODER_DT_SPEC(encoder1);

const int THRESHOLD_CM = 60;

void main(void)
{
    int distance_cm, measured_distance_cm, color_intensity;
    struct sensor_value value;
    const struct device *sensor = device_get_binding("VL53L0X_C");

    if (! sensor){
        return;
    }
    if (touchpad_init(&pad)){
        return;
    }
    if (encoder_init(&enc)){
        return;
    }

    touchpad_set_color(&pad, COLOR_RGB(0, 0, 0));
    encoder_set_color(&enc, COLOR_BLUE);
    k_sleep(K_MSEC(3000));

    bool is_in_tracking_zone = false;

    printk("Starting mainloop !\n");
    int i=0;
    while (1){
        i++;
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

        color_t color;
        if (gpio_pin_get_dt(&pad.out) > 0){
            color = COLOR_RGB(color_intensity, color_intensity, 0);
        } else if (is_in_tracking_zone){
            color = color_map(COLOR_RED, COLOR_GREEN, (float) distance_cm / THRESHOLD_CM);
        } else if (distance_cm < 150) {
            color = COLOR_RGB(0, 0, COLOR_CHAN_MAX / 2);
        } else {
            color = COLOR_RGB(0, 0, COLOR_CHAN_MAX / 64);
        }

        if (i%100 == 0){
            printk("%d cm (#%06X)\n", distance_cm, color);
        }
        
        touchpad_set_color(&pad, color);
        encoder_set_color(&enc, color);
    }
}
