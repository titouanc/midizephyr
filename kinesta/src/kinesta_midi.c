#include "kinesta_midi.h"
#include "config.h"
#include "usb_midi.h"

#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/gpio.h>

struct kinesta_midi_din {
    bool enabled;
    const struct device *uart;
    const struct gpio_dt_spec rx_led;
    const struct gpio_dt_spec tx_led;
};

#define KMIDI_FROM_DT(node) \
    {.enabled=true, .uart=DEVICE_DT_GET(DT_PROP(node, uart)), \
     .rx_led=GPIO_DT_SPEC_GET(node, rx_led_gpios), \
     .tx_led=GPIO_DT_SPEC_GET(node, tx_led_gpios)},

#define N_MIDI_DINS ARRAY_SIZE(midi_dins)

static struct kinesta_midi_din midi_dins[] = {
    DT_FOREACH_STATUS_OKAY(kinesta_midi_din, KMIDI_FROM_DT)
};

static void kinesta_midi_din_transmit(struct kinesta_midi_din *self, const uint8_t midi_pkt[3]) {
    if (! self->enabled){
        gpio_pin_set_dt(&self->tx_led, 0);
        gpio_pin_set_dt(&self->rx_led, 0);
        return;
    }

    gpio_pin_set_dt(&self->tx_led, 1);

    uint8_t midi_cmd = midi_pkt[0] >> 4;
    for (size_t i=0; i<midi_datasize(midi_cmd); i++){
        uart_poll_out(self->uart, midi_pkt[i]);
    }

    gpio_pin_set_dt(&self->tx_led, 0);
}

static bool kinesta_midi_usb_enabled = true;

void kinesta_midi_out(const uint8_t midi_pkt[3])
{
    for (size_t i=0; i<N_MIDI_DINS; i++){
        kinesta_midi_din_transmit(&midi_dins[i], midi_pkt);
    }

    if (kinesta_midi_usb_enabled) {
        usb_midi_write(USB_MIDI_SENSORS_JACK_ID, midi_pkt);
    }
}
