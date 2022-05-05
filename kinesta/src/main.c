#include <zephyr.h>
#include <device.h>
#include <devicetree.h>
#include <usb/usb_device.h>

#include "kinesta_functional_block.h"
// #include "usb_midi.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(app);

#define N_KFBS ARRAY_SIZE(kfbs)

kinesta_functional_block kfbs[] = {
    {
        .distance_sensor = DEVICE_DT_GET(DT_NODELABEL(vl53l0x_l)),
        .pad = TOUCHPAD_PWM_DT_SPEC(pad1),
        .pad2 = TOUCHPAD_GPIO_DT_SPEC(pad3),
        .rgb_encoder = ENCODER_DT_SPEC(encoder1),
    },
    {
        .distance_sensor = DEVICE_DT_GET(DT_NODELABEL(vl53l0x_r)),
        .pad = TOUCHPAD_PWM_DT_SPEC(pad2),
        .pad2 = TOUCHPAD_GPIO_DT_SPEC(pad4),
        .rgb_encoder = ENCODER_DT_SPEC(encoder2),
    },
};

struct gpio_callback encoders_irq_cb;
struct gpio_dt_spec encoders_irq = GPIO_DT_SPEC_GET(DT_NODELABEL(encoders_irq), gpios);

static void refresh_encoders()
{
    int i;
    for (i=0; i<N_KFBS; i++){
        kfb_update_encoder(&kfbs[i]);
    }
}

K_WORK_DEFINE(encoders_irq_work, refresh_encoders);

static void on_encoder_irq(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    k_work_submit(&encoders_irq_work);
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

    for (i=0; i<N_KFBS; i++){
        if (kfb_init(&kfbs[i])){
            LOG_ERR("Unable to initialize functional block %d", i);
            return;
        }
    }

    gpio_pin_configure_dt(&encoders_irq, GPIO_INPUT);
    gpio_pin_interrupt_configure_dt(&encoders_irq, GPIO_INT_EDGE_TO_ACTIVE);
    gpio_init_callback(&encoders_irq_cb, on_encoder_irq, BIT(encoders_irq.pin));
    gpio_add_callback(encoders_irq.port, &encoders_irq_cb);

    LOG_INF("Starting mainloop !");

    while (1){
        for (i=0; i<N_KFBS; i++){
            if (kfb_update(&kfbs[i])){
                LOG_ERR("Unable to update functional block %d", i);
                return;
            }
        }
        k_sleep(K_MSEC(100));
    }
}
