#include <math.h>
#include <stdio.h>

#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/usb/usb_device.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

static const struct device *const midi_in = DEVICE_DT_GET(DT_NODELABEL(midi_in));
static const struct device *const midi_out = DEVICE_DT_GET(DT_NODELABEL(midi_out));


void main()
{
	uint8_t databyte;

	int ret = usb_enable(NULL);
	if (ret != 0) {
		LOG_ERR("Failed to enable USB");
		return;
	}

	LOG_INF("USB enabled");

	while (1){
		if (uart_poll_in(midi_out, &databyte) == 0){
			uart_poll_out(midi_in, databyte);
		} else {
			k_sleep(K_MSEC(1));
		}
	}
}
