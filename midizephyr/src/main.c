#include <zephyr.h>
#include <device.h>
#include <devicetree.h>
#include <drivers/gpio.h>
#include <usb/usb_device.h>

#include "usb_midi.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(app);

enum {
    INIT,
    USB_ENABLED,
    RUNNING,
} app_state = INIT;

#define SLEEP_TIME_MS   100

#define STATUS_LED_NODE  DT_ALIAS(mled1)
#define STATUS_LED_LABEL DT_GPIO_LABEL(STATUS_LED_NODE, gpios)
#define STATUS_LED_PIN   DT_GPIO_PIN(STATUS_LED_NODE, gpios)
#define STATUS_LED_FLAGS DT_GPIO_FLAGS(STATUS_LED_NODE, gpios)

static const uint8_t noteOn[] = {0x90, 0x80, 0x7F};
static const uint8_t noteOff[] = {0x80, 0x80, 0x7F};

static struct k_delayed_work status_led_work;
static void status_led_task()
{
    static unsigned int tick = 0;
    static bool is_on = true;
    static struct device *led_dev = NULL;

    if (! led_dev){
        led_dev = device_get_binding(STATUS_LED_LABEL);
        if (! led_dev){
            return;
        }
        if (gpio_pin_configure(led_dev, STATUS_LED_PIN, GPIO_OUTPUT_ACTIVE|STATUS_LED_FLAGS) < 0){
            return;
        }
    }

    tick++;

    switch (app_state){
        case INIT:
            is_on = false;
            break;
        case USB_ENABLED:
            if (tick % 10 == 0){
                is_on = ! is_on;
            }
            if (midi_is_configured()){
                app_state = RUNNING;
            }
            break;
        case RUNNING:
            is_on = ! is_on;
            if (! midi_is_configured()){
                app_state = USB_ENABLED;
                is_on = false;
            }
            if (tick % 10 == 0){
                if (is_on){
                    midi_to_host(1, noteOn, sizeof(noteOn));
                } else {
                    midi_to_host(1, noteOff, sizeof(noteOff));
                }
            }
            break;
        default: break;
    }

    gpio_pin_set(led_dev, STATUS_LED_PIN, (int) is_on);
    k_delayed_work_submit(&status_led_work, K_MSEC(SLEEP_TIME_MS));
}

void main(void)
{
    LOG_INF("Starting...");

    k_delayed_work_init(&status_led_work, status_led_task);
    k_delayed_work_submit(&status_led_work, K_NO_WAIT);

    if (usb_enable(NULL) == 0){
        LOG_INF("USB enabled");
        app_state = USB_ENABLED;
    } else {
        LOG_ERR("Failed to enable USB");
        return;
    }
}
