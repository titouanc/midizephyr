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

// static const struct device *encoder = DEVICE_DT_GET(DT_NODELABEL(encoder1));
static const struct device *const midi_in = DEVICE_DT_GET(DT_NODELABEL(midi_in));
static const struct device *const midi_out = DEVICE_DT_GET(DT_NODELABEL(midi_out));
static const struct device *const midi_in2 = DEVICE_DT_GET(DT_NODELABEL(midi_in2));
static const struct device *const midi_out2 = DEVICE_DT_GET(DT_NODELABEL(midi_out2));
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);


static const struct device *midi_ports[] = {midi_in, midi_out, midi_in2, midi_out2};

void main()
{
	gpio_pin_configure_dt(&led, GPIO_OUTPUT);
	gpio_pin_set_dt(&led, 0);

	for (size_t i=0; i<ARRAY_SIZE(midi_ports); i++){
		if (! device_is_ready(midi_ports[i])) {
			LOG_ERR("Device is not ready: %s", midi_ports[i]->name);
		}
	}

	int ret = usb_enable(NULL);
	if (ret != 0) {
		LOG_ERR("Failed to enable USB");
		return;
	}

	LOG_INF("USB enabled");

	const uint8_t *data;
	while (1){
		int r = usb_midi_peak(midi_out, &data, 10);
		if (r < 0){
			LOG_ERR("usb_midi_peak() -> %d", r);
			continue;
		}

		if (r > 0){
			for (int i=0; i<r; i++){
				printf("%02hhX ", data[i]);
			}
			printf("\n");
		}

		usb_midi_read_continue(midi_out, r);
	}
}
