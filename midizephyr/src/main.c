#include <stdio.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <usb_midi.h>

#define SW0_NODE	DT_ALIAS(sw0)
#if !DT_NODE_HAS_STATUS(SW0_NODE, okay)
#error "Unsupported board: sw0 devicetree alias is not defined"
#endif
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(SW0_NODE, gpios);
static struct gpio_callback button_cb_data;

struct midi_loopback {
	const struct device *input;
	const struct device *output;
};

static int loopback_forward(const struct midi_loopback *lb)
{
	int r, res = 0;
	uint8_t buf[16];

	r = usb_midi_read(lb->output, buf, 1, K_NO_WAIT);
	if (r <= 0){
		return 0;
	}

	size_t len = midi_datasize(buf[0] >> 4);
	__ASSERT((len > 0 && len < sizeof(buf)), "Unknown MIDI message length");

	r = usb_midi_read(lb->output, &buf[1], len, K_NO_WAIT);
	if (r != len){
		printf("Not enough data to forward\n");
		return 0;
	}

	printf("Forwarding %dB %s -> %s\n", len+1, lb->output->name, lb->input->name);
	return usb_midi_write(lb->input, buf, len+1);
}

static const struct midi_loopback loopbacks[] = {
	{
		.input = DEVICE_DT_GET(DT_NODELABEL(midi_in)),
		.output = DEVICE_DT_GET(DT_NODELABEL(midi_out2)),
	},
	{
		.input = DEVICE_DT_GET(DT_NODELABEL(midi_in2)),
		.output = DEVICE_DT_GET(DT_NODELABEL(midi_out)),
	},
};

void main()
{
	size_t i;
	int r;
	if (usb_enable(NULL) == 0){
		printf("USB enabled\n");
	} else {
		printf("Cannot enable USB !\n");
	}

	while (1) {
		r = 0;
		for (i=0; i<ARRAY_SIZE(loopbacks); i++){
			r += loopback_forward(&loopbacks[i]);
		}
		if (r == 0){
			k_sleep(K_MSEC(10));
		}
	}
}
