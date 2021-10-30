#include <zephyr.h>
#include <device.h>
#include <devicetree.h>

#include "kinesta_functional_block.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(app);

#define N_KFBS ARRAY_SIZE(kfbs)

kinesta_functional_block kfbs[] = {
    {
        .distance_sensor = DEVICE_DT_GET(DT_NODELABEL(vl53l0x_l)),
        .pad = TOUCHPAD_DT_SPEC(pad1),
        .rgb_encoder = ENCODER_DT_SPEC(encoder1),
    }
};

void main(void)
{
    int i;
    for (i=0; i<N_KFBS; i++){
        if (kfb_init(&kfbs[i])){
            LOG_ERR("Unable to initialize functional block %d", i);
            return;
        }
    }

    while (1){
        for (i=0; i<N_KFBS; i++){
            if (kfb_update(&kfbs[i])){
                LOG_ERR("Unable to update functional block %d", i);
                return;
            }
        }
    }
}
