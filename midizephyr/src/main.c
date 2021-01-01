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


static const struct device *midi_uart = NULL;
static void midi_send(const uint8_t *pkt, size_t pktsize)
{
    if (midi_to_host(0, pkt, pktsize)){
        for (size_t i=0; i<pktsize; i++){
            uart_poll_out(midi_uart, pkt[i]);
        }
        do_act_led();
    }
}

#define MIDI_HIT(seq, steps) {midi_send(seq, sizeof(seq)); k_sleep(K_MSEC(steps*62));}

void main(void)
{
    LOG_INF("Starting...");

    int r = 0;
    const struct uart_config midi_uart_config = {
        .baudrate=31250,
        .parity= UART_CFG_PARITY_NONE,
        .stop_bits=UART_CFG_STOP_BITS_1,
        .data_bits=UART_CFG_DATA_BITS_8,
        .flow_ctrl=UART_CFG_FLOW_CTRL_NONE,
    };
    midi_uart = device_get_binding(MIDI_UART_LABEL);
    if (! midi_uart){
        LOG_ERR("Unable to open MIDI uart");
        return;
    }
    if (r = uart_configure(midi_uart, &midi_uart_config)){
        LOG_ERR("Error configuring MIDI uart: %d", r);
        return;
    }

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

    uint8_t kick[] = {0x90, 0x00, 0x7f};
    uint8_t snare[] = {0x93, 0x00, 0x50};
    uint8_t hat[] = {0x91, 0x00, 0x50};
    uint8_t hat_open[] = {0x92, 0x00, 0x50};

    unsigned long beat = 0;
    while (1){
        beat++;
        MIDI_HIT(kick, 2);
        MIDI_HIT(hat_open, 2);

        MIDI_HIT(hat, 4);
        
        MIDI_HIT(snare, 2);
        MIDI_HIT(hat_open, 2);
        
        if (beat % 4){
            MIDI_HIT(hat, 4);
        } else {
            MIDI_HIT(hat_open, 2);
            MIDI_HIT(kick, 1);
            MIDI_HIT(hat, 1);
        }
    }
}
