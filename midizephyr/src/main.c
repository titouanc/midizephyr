#include <stdio.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/usb/usb_device.h>
#include <usb_midi.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

const struct device *midi_in = DEVICE_DT_GET(DT_NODELABEL(midi_in));
const struct device *midi_out = DEVICE_DT_GET(DT_NODELABEL(midi_out));

void send_to_midi_in(const struct device *from, const uint8_t *data, size_t len)
{
	usb_midi_write(midi_in, data, len);
}

void main()
{
	if (! device_is_ready(midi_in)){
		LOG_ERR("midi in is not ready");
	}

	if (! device_is_ready(midi_out)){
		LOG_ERR("midi out is not ready");
	}

	usb_midi_register(midi_out, send_to_midi_in);

	int ret = usb_enable(NULL);
	if (ret != 0) {
		LOG_ERR("Failed to enable USB");
		return;
	}

	LOG_INF("USB enabled");

	while (1){
		k_sleep(K_MSEC(1000));
	}
}
