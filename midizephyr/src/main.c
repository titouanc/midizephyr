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

#define ACT_LED_NODE  DT_ALIAS(mled2)
#define ACT_LED_LABEL DT_GPIO_LABEL(ACT_LED_NODE, gpios)
#define ACT_LED_PIN   DT_GPIO_PIN(ACT_LED_NODE, gpios)
#define ACT_LED_FLAGS DT_GPIO_FLAGS(ACT_LED_NODE, gpios)

static const uint8_t noteOn[] = {0x90, 0x42, 0x7F};
static const uint8_t noteOff[] = {0x80, 0x42, 0x7F};

static K_SEM_DEFINE(act_led_sem, 0, 1);

static void do_act_led()
{
    k_sem_give(&act_led_sem);
}

static struct k_delayed_work act_led_work;
static void act_led_task()
{
    static struct device *led_dev = NULL;

    if (! led_dev){
        led_dev = device_get_binding(ACT_LED_LABEL);
        if (! led_dev){
            return;
        }
        if (gpio_pin_configure(led_dev, ACT_LED_PIN, GPIO_OUTPUT_ACTIVE|ACT_LED_FLAGS) < 0){
            return;
        }
        gpio_pin_set(led_dev, ACT_LED_PIN, 0);
    }

    // Wait for some activity
    if (k_sem_take(&act_led_sem, K_MSEC(1)) == 0){
        // Blink for 50ms
        gpio_pin_set(led_dev, ACT_LED_PIN, 1);
        k_sleep(K_MSEC(50));
        gpio_pin_set(led_dev, ACT_LED_PIN, 0);
    }

    // Then redo
    k_delayed_work_submit(&act_led_work, K_NO_WAIT);
}

/**
 * Status LED:
 * - Off when the board is not initialized
 * - Slow blink when MIDI-USB is not connected
 * - Fast blink when MIDI-USB is connected
 */
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

    k_delayed_work_init(&act_led_work, act_led_task);
    k_delayed_work_submit(&act_led_work, K_NO_WAIT);

    while (1){
        if (midi_to_host(0, noteOn, sizeof(noteOn))){
            do_act_led();
        }
        k_sleep(K_MSEC(1000));

        if (midi_to_host(0, noteOff, sizeof(noteOff))){
            do_act_led();
        }
        k_sleep(K_MSEC(1000));
    }
}
