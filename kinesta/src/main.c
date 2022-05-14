#include <zephyr.h>

#include "touchpad.h"
#include "kinesta_functional_block.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(app);

#define N_KFBS ARRAY_SIZE(kfbs)
static kinesta_functional_block kfbs[] = {                                                                       \
    {
        .name="slice",
        .midi_cc_group=0x10,
        .distance_sensor=DEVICE_DT_GET(DT_NODELABEL(vl53l0x_c)),
        .primary_pad=DEVICE_DT_GET(DT_NODELABEL(touchpad1)),
        .secondary_pad=DEVICE_DT_GET(DT_NODELABEL(touchpad2)),
        .rgb_encoder=DEVICE_DT_GET(DT_NODELABEL(encoder1)),
    },
};

void main(void)
{
    int i;

    if (usb_enable(NULL) == 0){
        LOG_INF("USB enabled");
    } else {
        LOG_ERR("Failed to enable USB");
        return;
    }


    for (i=0; i<N_KFBS; i++){
        kfb_init(&kfbs[i]);
    }

    while (true){
        for (i=0; i<N_KFBS; i++){
            kfb_update(&kfbs[i]);
        }
        k_sleep(K_MSEC(10));
    }
}
