#include "kinesta_midi.h"
#include "config.h"
#include "usb_midi.h"

#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/gpio.h>

#define MIN_TEMPO         15
#define MAX_TEMPO         400
#define MIDI_CLK_PER_NOTE 6

#define MS_PER_CLK(bpm) (60000.0f / bpm / MIDI_CLK_PER_NOTE)
#define MIN_MS_PER_CLK MS_PER_CLK(MAX_TEMPO)
#define MAX_MS_PER_CLK MS_PER_CLK(MIN_TEMPO)

static size_t clk_ticks = 0;
static int64_t last_midi_clk = 0;
static int64_t ms_per_midi_clk = 83;

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

static void kinesta_midi_usb_rx(const struct device *jack, const uint8_t *buf, size_t len)
{
	if (len == 0){
		return;
	}
	int64_t now = k_uptime_get();
	int64_t dt = now - last_midi_clk;
	switch (buf[0]){
	case MIDI_SYS_CLOCK:
		if (MIN_MS_PER_CLK <= dt && dt <= MAX_MS_PER_CLK){
			ms_per_midi_clk = dt;
		}
		last_midi_clk = now;
		clk_ticks++;
		break;
	case MIDI_SYS_START:
	case MIDI_SYS_STOP:
		clk_ticks = 0;
		break;
	}
}

static bool kinesta_midi_usb_enabled = true;
const struct device *kinesta_sensors_midi = DEVICE_DT_GET(DT_NODELABEL(kinesta_sensors));
const struct device *kinesta_clock_midi = DEVICE_DT_GET(DT_NODELABEL(kinesta_clock));

void kinesta_midi_out(const uint8_t midi_pkt[3])
{
    for (size_t i=0; i<N_MIDI_DINS; i++){
        kinesta_midi_din_transmit(&midi_dins[i], midi_pkt);
    }

    if (kinesta_midi_usb_enabled) {
        usb_midi_write(kinesta_sensors_midi, midi_pkt, 3);
    }
}

void kinesta_midi_init()
{
    usb_midi_register(kinesta_clock_midi, kinesta_midi_usb_rx);
    for (size_t i=0; i<N_MIDI_DINS; i++){
        gpio_pin_configure_dt(&midi_dins[i].tx_led, GPIO_OUTPUT);
        gpio_pin_configure_dt(&midi_dins[i].rx_led, GPIO_OUTPUT);
    }
}

float kinesta_midi_get_beat()
{
	int64_t now = k_uptime_get();
	float dt = now - last_midi_clk;
	float t = clk_ticks + (dt / ms_per_midi_clk);
	return t / MIDI_CLK_PER_NOTE;
}
