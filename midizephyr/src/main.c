#include <zephyr.h>
#include <device.h>
#include <devicetree.h>
#include <drivers/gpio.h>
#include <drivers/uart.h>
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

#define MIDI_UART_NODE   DT_ALIAS(midiport)
#define MIDI_UART_LABEL  DT_LABEL(MIDI_UART_NODE)

#define STATUS_LED_NODE  DT_NODELABEL(blue_led_1)
#define STATUS_LED_LABEL DT_GPIO_LABEL(STATUS_LED_NODE, gpios)
#define STATUS_LED_PIN   DT_GPIO_PIN(STATUS_LED_NODE, gpios)
#define STATUS_LED_FLAGS DT_GPIO_FLAGS(STATUS_LED_NODE, gpios)

#define ACT_LED_NODE  DT_ALIAS(midiled2)
#define ACT_LED_LABEL DT_GPIO_LABEL(ACT_LED_NODE, gpios)
#define ACT_LED_PIN   DT_GPIO_PIN(ACT_LED_NODE, gpios)
#define ACT_LED_FLAGS DT_GPIO_FLAGS(ACT_LED_NODE, gpios)

static K_SEM_DEFINE(act_led_sem, 0, 1);

static void do_act_led()
{
    k_sem_give(&act_led_sem);
}

static struct k_delayed_work act_led_work;
static void act_led_task()
{
    static const struct device *led_dev = NULL;

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
    static const struct device *led_dev = NULL;

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
            if (usb_midi_is_configured()){
                app_state = RUNNING;
            }
            break;
        case RUNNING:
            is_on = ! is_on;
            if (! usb_midi_is_configured()){
                app_state = USB_ENABLED;
                is_on = false;
            }
            break;
        default: break;
    }

    gpio_pin_set(led_dev, STATUS_LED_PIN, (int) is_on);
    k_delayed_work_submit(&status_led_work, K_MSEC(SLEEP_TIME_MS));
}


static void to_external_midi_out_func(void *p1, void *p2, void *p3)
{
    const struct uart_config midi_uart_config = {
        .baudrate=31250,
        .parity= UART_CFG_PARITY_NONE,
        .stop_bits=UART_CFG_STOP_BITS_1,
        .data_bits=UART_CFG_DATA_BITS_8,
        .flow_ctrl=UART_CFG_FLOW_CTRL_NONE,
    };

    const struct device *midi_uart = device_get_binding(MIDI_UART_LABEL);
    if (! midi_uart){
        LOG_ERR("Unable to open MIDI uart");
        return;
    }

    if (uart_configure(midi_uart, &midi_uart_config)){
        LOG_ERR("Error configuring MIDI uart");
        return;
    }

    uint8_t cable_id;
    uint8_t midi_pkt[3] = {0, 0, 0};
    while (1){
        if (usb_midi_read(&cable_id, midi_pkt)){
            continue;
        }

        uint8_t cmd = midi_pkt[0] >> 4;

        for (size_t i=0; i<midi_datasize(cmd); i++){
            uart_poll_out(midi_uart, midi_pkt[i]);
        }
        do_act_led();
    }
}

K_THREAD_DEFINE(to_external_midi_out_tid, 512,
                to_external_midi_out_func, NULL, NULL, NULL,
                5, 0, 0);


#define kick 0
#define hat 1
#define hat_open 2
#define snare 3

static void midi_beat(uint8_t chan, int steps)
{
    const uint8_t noteOn[3] = MIDI_NOTE_ON(chan, 0, 127);
    if (usb_midi_write(1, noteOn) == 0){
        do_act_led();
    }

    k_sleep(K_MSEC(steps*62));

    const uint8_t noteOff[3] = MIDI_NOTE_OFF(chan, 0, 127);
    if (usb_midi_write(1, noteOff) == 0){
        do_act_led();
    }
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

    unsigned long beat = 0;
    while (1){
        beat++;
        midi_beat(kick, 2);
        midi_beat(hat_open, 2);

        midi_beat(hat, 4);
        
        midi_beat(snare, 2);
        midi_beat(hat_open, 2);
        
        if (beat % 4){
            midi_beat(hat, 4);
        } else {
            midi_beat(hat_open, 2);
            midi_beat(kick, 1);
            midi_beat(hat, 1);
        }
    }
}
