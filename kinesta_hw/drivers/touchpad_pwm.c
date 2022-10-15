#include "touchpad.h"

#include <zephyr/drivers/pwm.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(touchpad_pwm);

#define DT_DRV_COMPAT kinesta_rgb_touchpad_pwm

struct touchpad_pwm_config {
    struct gpio_dt_spec touch;
    struct pwm_dt_spec led_r;
    struct pwm_dt_spec led_g;
    struct pwm_dt_spec led_b;
};

static int touchpad_pwm_init(const struct device *dev)
{
    const struct touchpad_pwm_config *const config = dev->config;
    const struct pwm_dt_spec *pwms = &config->led_r;

    gpio_pin_configure_dt(&config->touch, GPIO_INPUT | config->touch.dt_flags);

    for (int i=0; i<COLOR_N_CHANS; i++){
        if (! device_is_ready(pwms[i].dev)){
            LOG_ERR("PWM device %s is not ready, unable to init touchpad_pwm", pwms[i].dev->name);
        }
    }

    return 0;
}

static void touchpad_pwm_set_color_channel(const struct device *dev, color_channel_t channel, unsigned value)
{
    __ASSERT(channel < 3, "Invalid color channel !");
    const struct touchpad_pwm_config *const config = dev->config;
    const struct pwm_dt_spec *pwm = &(&config->led_r)[channel];
    if (pwm_set_dt(pwm, 7000*COLOR_CHAN_MAX, 7000*value)){
        LOG_ERR("[%s] Unable to set channel %d (%s%d)", dev->name, channel, pwm->dev->name, pwm->channel);
    }
}

const struct touchpad_driver_api touchpad_pwm_api_funcs = {
    .set_color_channel = touchpad_pwm_set_color_channel,
};

#define TOUCHPAD_PWM_INIT(inst) \
    static const struct touchpad_pwm_config touchpad_##inst##_config = {    \
        .touch = GPIO_DT_SPEC_INST_GET(inst, touch_gpios),                  \
        .led_r = PWM_DT_SPEC_GET_BY_NAME(DT_DRV_INST(inst), red),           \
        .led_g = PWM_DT_SPEC_GET_BY_NAME(DT_DRV_INST(inst), green),         \
        .led_b = PWM_DT_SPEC_GET_BY_NAME(DT_DRV_INST(inst), blue),          \
    };                                                                      \
                                                                            \
    DEVICE_DT_INST_DEFINE(inst, touchpad_pwm_init, NULL,                    \
                          NULL,                                             \
                          &touchpad_##inst##_config,                        \
                          POST_KERNEL,                                      \
                          CONFIG_SYSTEM_CLOCK_INIT_PRIORITY,                \
                          &touchpad_pwm_api_funcs);

DT_INST_FOREACH_STATUS_OKAY(TOUCHPAD_PWM_INIT)
