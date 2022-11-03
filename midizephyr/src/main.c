#include <stdio.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/usb/usb_device.h>
#include <usb_midi.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

#define FORWARD_TO_MIDI_PORT midi_in

static const struct device *midi_in = DEVICE_DT_GET(DT_NODELABEL(midi_in));
static const struct device *midi_in2 = DEVICE_DT_GET(DT_NODELABEL(midi_in2));
static const struct device *midi_out = DEVICE_DT_GET(DT_NODELABEL(midi_out));
static const struct device *midi_out2 = DEVICE_DT_GET(DT_NODELABEL(midi_out2));

static const uint8_t midi_msg[] = {
	0x99, 0x42, 0x42,
	0x99, 0x46, 0x42,
	0x89, 0x42, 0x00,
	0x89, 0x46, 0x7f,
};

void forward_midi(const struct device *from, struct net_buf *buf, size_t len)
{
	for (size_t i=0; i<len; i++){
		LOG_INF("  [%d] %02X", (int) i, (int) buf->data[i]);
	}
	int r = usb_midi_write_buf(FORWARD_TO_MIDI_PORT, buf, len);
	LOG_INF("Fwding %dB from %s to %s -> %d", len, from->name, FORWARD_TO_MIDI_PORT->name, r);
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
		ret = usb_midi_write(midi_in2, midi_msg, sizeof(midi_msg));
		LOG_INF("Sending static midi msg: %d", ret);
		k_sleep(K_MSEC(100));
	}
}
