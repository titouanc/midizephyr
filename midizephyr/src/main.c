#include <stdio.h>

#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/usb/usb_device.h>
#include <usb_midi.h>

#include "strerrno.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

#define FORWARD_TO_MIDI_PORT midi_in

static const struct device *midi_in = DEVICE_DT_GET(DT_NODELABEL(midi_in));
static const struct device *midi_out = DEVICE_DT_GET(DT_NODELABEL(midi_out));
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);

size_t tick = 0;

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


void forward_midi(const struct device *from, const uint8_t *data, size_t len)
{
	// Tempo LED
	if (len > 0 && data[0] == 0xF8){
		tick++;
		gpio_pin_set_dt(&led, tick % 6 == 0);
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

void main()
{
	gpio_pin_configure_dt(&led, GPIO_OUTPUT);
	gpio_pin_set_dt(&led, 0);

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
		k_sleep(K_MSEC(100));
	}
}
