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

static bool din_btn_was_pressed = false;
static bool usb_btn_was_pressed = false;
static const struct gpio_dt_spec din_btn_pressed = GPIO_DT_SPEC_GET(DT_NODELABEL(midi_din_btn_pressed), gpios);
static const struct gpio_dt_spec usb_btn_pressed = GPIO_DT_SPEC_GET(DT_NODELABEL(midi_usb_btn_pressed), gpios);
static const struct gpio_dt_spec din_btn_led = GPIO_DT_SPEC_GET(DT_NODELABEL(midi_din_btn_led), gpios);
static const struct gpio_dt_spec usb_btn_led = GPIO_DT_SPEC_GET(DT_NODELABEL(midi_usb_btn_led), gpios);

void kinesta_midi_init()
{

	gpio_pin_configure_dt(&din_btn_pressed, GPIO_INPUT);
	gpio_pin_configure_dt(&usb_btn_pressed, GPIO_INPUT);
	gpio_pin_configure_dt(&din_btn_led, GPIO_OUTPUT);
	gpio_pin_configure_dt(&usb_btn_led, GPIO_OUTPUT);

	if (N_MIDI_DINS){
		gpio_pin_set_dt(&din_btn_led, midi_dins[0].enabled);
	}
	gpio_pin_set_dt(&usb_btn_led, kinesta_midi_usb_enabled);

    for (size_t i=0; i<N_MIDI_DINS; i++){
        gpio_pin_configure_dt(&midi_dins[i].tx_led, GPIO_OUTPUT);
        gpio_pin_configure_dt(&midi_dins[i].rx_led, GPIO_OUTPUT);
    }
}

void kinesta_midi_update()
{
	if (N_MIDI_DINS){
		bool din_btn_is_pressed = gpio_pin_get_dt(&din_btn_pressed);
		if (din_btn_was_pressed && ! din_btn_is_pressed){
			midi_dins[0].enabled = !midi_dins[0].enabled;
			gpio_pin_set_dt(&din_btn_led, midi_dins[0].enabled);
		}
		din_btn_was_pressed = din_btn_is_pressed;
	}

	bool usb_btn_is_pressed = gpio_pin_get_dt(&usb_btn_pressed);
	if (usb_btn_was_pressed && ! usb_btn_is_pressed){
		kinesta_midi_usb_enabled = !kinesta_midi_usb_enabled;
		gpio_pin_set_dt(&usb_btn_led, kinesta_midi_usb_enabled);
	}
	usb_btn_was_pressed = usb_btn_is_pressed;
}
