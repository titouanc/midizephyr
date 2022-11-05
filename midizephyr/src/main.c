#include <stdio.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/usb/usb_device.h>
#include <usb_midi.h>

#include "strerrno.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

#define FORWARD_TO_MIDI_PORT midi_in

static const struct device *midi_in = DEVICE_DT_GET(DT_NODELABEL(midi_in));
static const struct device *midi_in2 = DEVICE_DT_GET(DT_NODELABEL(midi_in2));
static const struct device *midi_out = DEVICE_DT_GET(DT_NODELABEL(midi_out));
static const struct device *midi_out2 = DEVICE_DT_GET(DT_NODELABEL(midi_out2));

void forward_midi(const struct device *from, struct net_buf *buf, size_t len)
{
	int r = usb_midi_write_buf(FORWARD_TO_MIDI_PORT, buf, len);
	if (r < 0){
		LOG_ERR("Fwding %dB from %s to %s -> %d (%s)", len, from->name, FORWARD_TO_MIDI_PORT->name, r, strerrno(r));
	} else {
		LOG_DBG("Fwding %dB from %s to %s -> %d", len, from->name, FORWARD_TO_MIDI_PORT->name, r);
	}
}

void main()
{
	if (! device_is_ready(midi_in)){
		LOG_ERR("midi in is not ready");
	}

	if (! device_is_ready(midi_out)){
		LOG_ERR("midi out is not ready");
	}

	usb_midi_register(midi_out, forward_midi);
	usb_midi_register(midi_out2, forward_midi);

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
