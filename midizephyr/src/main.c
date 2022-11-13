#include <math.h>
#include <stdio.h>

#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/usb/usb_device.h>
#include <usb_midi.h>

#include "encoder.h"

#include "strerrno.h"
#include "approxmath.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

#define FORWARD_TO_MIDI_PORT midi_in

static const struct device *encoder = DEVICE_DT_GET(DT_NODELABEL(encoder1));
static const struct device *midi_in = DEVICE_DT_GET(DT_NODELABEL(midi_in));
static const struct device *midi_out = DEVICE_DT_GET(DT_NODELABEL(midi_out));
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);

#define MIN_TEMPO        15
#define MAX_TEMPO        400
#define CLOCK_PER_NOTE   6

#define MS_PER_CLK(bpm) (60000.0f / bpm / CLOCK_PER_NOTE)
#define BPM(ms_per_clk) MS_PER_CLK(ms_per_clk)
#define MIN_MS_PER_CLK MS_PER_CLK(MAX_TEMPO)
#define MAX_MS_PER_CLK MS_PER_CLK(MIN_TEMPO)

static size_t clk_ticks = 0;
static int64_t last_clk = 0;
static int64_t ms_per_clk = 83;

static int usb_midi_write_all(const struct device *dev, const uint8_t *data, size_t len)
{
	int r = 0;
	int written = 0;
	while (written < len){
		r = usb_midi_write(dev, &data[written], len-written);
		if (r < 0){
			written = r;
			break;
		}
		written += r;
	}
	return written;
}

float get_midi_beat()
{
	int64_t now = k_uptime_get();
	float dt = now - last_clk;
	float t = clk_ticks + (dt / ms_per_clk);
	return t / CLOCK_PER_NOTE;
}

void forward_midi(const struct device *from, const uint8_t *data, size_t len)
{
	int64_t now, dt;
	// Tempo LED
	if (len > 0 && data[0] == 0xF8){
		now = k_uptime_get();
		dt = now - last_clk;
		if (MIN_MS_PER_CLK <= dt && dt <= MAX_MS_PER_CLK){
			LOG_INF("BPM: %d", (int) BPM(ms_per_clk));
			ms_per_clk = dt;
		}
		last_clk = now;
		clk_ticks++;
	}

	int r = usb_midi_write_all(FORWARD_TO_MIDI_PORT, data, len);
	if (r < 0){
		LOG_ERR("Fwding %dB from %s to %s -> %d (%s)",
			len, from->name, FORWARD_TO_MIDI_PORT->name, r, strerrno(r));
	} else {
		LOG_DBG("Fwding %dB from %s to %s -> %d",
			len, from->name, FORWARD_TO_MIDI_PORT->name, r);
	}
}

static const color_t tempo_color = 0x396FFFFF;

static inline void show_tempo()
{
	float k = approxcos_normalized(get_midi_beat());
	encoder_set_color(encoder, color_mul(tempo_color, k));
	gpio_pin_set_dt(&led, clk_ticks % CLOCK_PER_NOTE == 0);
}

void main()
{
	gpio_pin_configure_dt(&led, GPIO_OUTPUT);
	gpio_pin_set_dt(&led, 0);
	encoder_set_color(encoder, 0);

	if (! device_is_ready(midi_in)){
		LOG_ERR("midi in is not ready");
	}

	if (! device_is_ready(midi_out)){
		LOG_ERR("midi out is not ready");
	}

	usb_midi_register(midi_out, forward_midi);

	int ret = usb_enable(NULL);
	if (ret != 0) {
		LOG_ERR("Failed to enable USB");
		return;
	}

	LOG_INF("USB enabled");

	while (1){
		show_tempo();
		k_sleep(K_MSEC(5));
	}
}
