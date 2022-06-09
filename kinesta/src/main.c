#include <zephyr.h>
#include <usb/usb_device.h>

#include "touchpad.h"
#include "kinesta_functional_block.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(app);

#define KFB_FROM_DT(inst) \
    {\
        .name=DT_PROP(inst, label),\
        .midi_cc_group=DT_PROP(inst, midi_cc_group),\
        .distance_sensor=DEVICE_DT_GET(DT_PROP(inst, distance_sensor)),\
        .primary_touchpad=DEVICE_DT_GET(DT_PROP(inst, primary_touchpad)),\
        .secondary_touchpad=DEVICE_DT_GET(DT_PROP(inst, secondary_touchpad)),\
        .encoder=DEVICE_DT_GET(DT_PROP(inst, encoder)),\
    },

#define N_KFBS ARRAY_SIZE(kfbs)
static kinesta_functional_block kfbs[] = {
    DT_FOREACH_STATUS_OKAY(kinesta_functional_block, KFB_FROM_DT)
};

void test_touchpads()
{
    color_t color;
    while (true){
        for (float t=0; t<=1; t+=0.01){
            for (int i=0; i<N_KFBS; i++){
                color = color_map(COLOR_RED, COLOR_GREEN, t);
                touchpad_set_color(kfbs[i].primary_touchpad, color);
                touchpad_set_color(kfbs[i].secondary_touchpad, color);
            }
            k_sleep(K_MSEC(100));
        }

        for (float t=0; t<=1; t+=0.01){
            for (int i=0; i<N_KFBS; i++){
                color = color_map(COLOR_GREEN, COLOR_BLUE, t);
                touchpad_set_color(kfbs[i].primary_touchpad, color);
                touchpad_set_color(kfbs[i].secondary_touchpad, color);
            }
            k_sleep(K_MSEC(100));
        }

        for (float t=0; t<=1; t+=0.01){
            for (int i=0; i<N_KFBS; i++){
                color = color_map(COLOR_BLUE, COLOR_RED, t);
                touchpad_set_color(kfbs[i].primary_touchpad, color);
                touchpad_set_color(kfbs[i].secondary_touchpad, color);
            }
            k_sleep(K_MSEC(100));
        }
    }
}

void main(void)
{
    int i;

    if (usb_enable(NULL) == 0){
        LOG_INF("USB enabled");
    } else {
        LOG_ERR("Failed to enable USB");
        return;
    }

    LOG_INF("Initializing %d KFB(s)", (int) N_KFBS);
    // test_touchpads();

    for (i=0; i<N_KFBS; i++){
        LOG_INF("Initializing KFB %s", kfbs[i].name);
        if (kfb_init(&kfbs[i])){
            return;
        }
    }

    LOG_INF("Starting mainloop");
    while (true){
        for (i=0; i<N_KFBS; i++){
            kfb_update(&kfbs[i]);
        }
        k_sleep(K_MSEC(1));
    }
}
