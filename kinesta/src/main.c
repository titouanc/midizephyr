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

void scan_slices_rgb()
{
    color_t colors[] = {COLOR_RED, COLOR_GREEN, COLOR_BLUE};

    for (int i=0; i<N_KFBS; i++){
        LOG_INF("Slice %d", i+1);
        for (int j=0; j<ARRAY_SIZE(colors); j++){
            touchpad_set_color(kfbs[i].primary_touchpad, colors[j]);
            k_sleep(K_MSEC(250));
            touchpad_set_color(kfbs[i].primary_touchpad, 0);

            encoder_set_color(kfbs[i].encoder, colors[j]);
            k_sleep(K_MSEC(250));
            encoder_set_color(kfbs[i].encoder, 0);

            touchpad_set_color(kfbs[i].secondary_touchpad, colors[j]);
            k_sleep(K_MSEC(250));
            touchpad_set_color(kfbs[i].secondary_touchpad, 0);

            if (touchpad_is_touched(kfbs[i].primary_touchpad)){
                touchpad_set_color(kfbs[i].primary_touchpad, COLOR_WHITE);
            }

            if (touchpad_is_touched(kfbs[i].secondary_touchpad)){
                touchpad_set_color(kfbs[i].secondary_touchpad, COLOR_WHITE);
            }

            k_sleep(K_MSEC(250));
            touchpad_set_color(kfbs[i].primary_touchpad, 0);
            touchpad_set_color(kfbs[i].secondary_touchpad, 0);
        }
    }
}

void autotest()
{
    for (int i=0; i<N_KFBS; i++){
        LOG_INF("Initializing KFB %s", kfbs[i].name);
        kfb_init(&kfbs[i]);
    }

    while (true) {
        scan_slices_rgb();
    }
}

/* On the first Kinesta SMD devboard, there is a schematic error where the MIDI
 * UART Tx is wired to PD7 instead of PD5, but only PD5 can be used as UART2_TX.
 * To circumvent the problem, we put a jumper between PD5 and PD7 on the Nucleo,
 * and we set PD7 as input so that it doesn't interfere with the signal sent
 * by PD5
 * 
 *  +---jumper---+
 *  |            |
 * PD7 [NUCLEO] PD5
 *  |
 *  +---kinesta--->MIDI OUT
 */
void setup_wiring_workaround()
{
    const struct device *dev = device_get_binding("GPIOD");
    if (! dev){
        LOG_ERR("Unable to open GPIOD");
    } else {
        gpio_pin_configure(dev, 7, GPIO_INPUT);
    }
}

void main(void)
{
    // autotest();
    int i;

    // Problems with components on the first slice, so software disable them
    kfbs[0].soft_disable = true;

    setup_wiring_workaround();

    if (usb_enable(NULL) == 0){
        LOG_INF("USB enabled");
    } else {
        LOG_ERR("Failed to enable USB");
        return;
    }

    LOG_INF("Initializing %d KFB(s)", (int) N_KFBS);

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
