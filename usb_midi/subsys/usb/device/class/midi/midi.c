#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/usb/usb_device.h>
#include <usb_descriptor.h>

#include <usb_midi.h>

#include "usb_midi_internal.h"

LOG_MODULE_REGISTER(usb_midi, CONFIG_USB_MIDI_LOG_LEVEL);

#define COMPAT_MIDI_INPUT  usb_midi_input
#define COMPAT_MIDI_OUTPUT usb_midi_output

#define MIDI_OUTPUT_DEVICE_COUNT DT_NUM_INST_STATUS_OKAY(COMPAT_MIDI_OUTPUT)
#define MIDI_INPUT_DEVICE_COUNT DT_NUM_INST_STATUS_OKAY(COMPAT_MIDI_INPUT)

#define INPUT_INDEX(dt_index) (dt_index)
#define OUTPUT_INDEX(dt_index) (dt_index + MIDI_INPUT_DEVICE_COUNT)

#define INPUT_EXT_JACK_ID(dt_index) (2 * INPUT_INDEX(dt_index) + 1)
#define INPUT_EMB_JACK_ID(dt_index) (2 * INPUT_INDEX(dt_index) + 2)
#define OUTPUT_EXT_JACK_ID(dt_index) (2 * OUTPUT_INDEX(dt_index) + 1)
#define OUTPUT_EMB_JACK_ID(dt_index) (2 * OUTPUT_INDEX(dt_index) + 2)

#define INPUT_EMB_JACK_ID_(x, _) INPUT_EMB_JACK_ID(x)
#define OUTPUT_EMB_JACK_ID_(x, _) OUTPUT_EMB_JACK_ID(x)

#define MIDI_INPUT_DESCRIPTOR(index, _) \
	USB_MIDI_IO_DESCRIPTOR(1, INPUT_EMB_JACK_ID(index), INPUT_EXT_JACK_ID(index))

#define MIDI_OUTPUT_DESCRIPTOR(index, _) \
	USB_MIDI_IO_DESCRIPTOR(0, OUTPUT_EMB_JACK_ID(index), OUTPUT_EXT_JACK_ID(index))

#define USB_MIDI_IO_STR_DECLARE(inst) \
	struct inst##_str_desc { \
		uint8_t bLength; \
		uint8_t bDescriptorType; \
		uint8_t bString[USB_BSTRING_LENGTH(DT_NODE_FULL_NAME(inst))]; \
	} __packed;

#define USB_MIDI_IO_STR_DESCRIPTOR(inst) \
	USBD_STRING_DESCR_USER_DEFINE(primary) struct inst##_str_desc inst##_str = { \
		.bLength = USB_STRING_DESCRIPTOR_LENGTH(DT_NODE_FULL_NAME(inst)), \
		.bDescriptorType = USB_DESC_STRING, \
		.bString = DT_NODE_FULL_NAME(inst), \
	};

#define DEFINE_USB_MIDI_IO_STR(inst) \
	USB_MIDI_IO_STR_DECLARE(inst); \
	USB_MIDI_IO_STR_DESCRIPTOR(inst);

DT_FOREACH_STATUS_OKAY(COMPAT_MIDI_INPUT, DEFINE_USB_MIDI_IO_STR)
DT_FOREACH_STATUS_OKAY(COMPAT_MIDI_OUTPUT, DEFINE_USB_MIDI_IO_STR)

static void usb_midi_rx(uint8_t ep, enum usb_dc_ep_cb_status_code ep_status);

static struct usb_ep_cfg_data ep_cfg[] = {
	{.ep_cb = usb_transfer_ep_callback, .ep_addr = USB_MIDI_TO_HOST_ENDPOINT_ID},
	{.ep_cb = usb_midi_rx, .ep_addr = USB_MIDI_FROM_HOST_ENDPOINT_ID},
};

typedef USB_MIDI_IF_DESCRIPTOR_DECLARE(MIDI_OUTPUT_DEVICE_COUNT, MIDI_INPUT_DEVICE_COUNT)
        midistreaming_class;

USBD_CLASS_DESCR_DEFINE(primary, 0) midistreaming_class midistreaming_cfg = {
	USB_MIDI_IF_DESCRIPTOR_INIT(MIDI_OUTPUT_DEVICE_COUNT, MIDI_INPUT_DEVICE_COUNT),
	.inputs = {
		LISTIFY(MIDI_INPUT_DEVICE_COUNT, MIDI_INPUT_DESCRIPTOR, (,))
	},
	.outputs = {
		LISTIFY(MIDI_OUTPUT_DEVICE_COUNT, MIDI_OUTPUT_DESCRIPTOR, (,))
	},
	.cs_ep_inputs = USB_MIDI_CS_EP_DESCRIPTOR(
		LISTIFY(MIDI_INPUT_DEVICE_COUNT, INPUT_EMB_JACK_ID_, (,))
	),
	.cs_ep_outputs = USB_MIDI_CS_EP_DESCRIPTOR(
		LISTIFY(MIDI_OUTPUT_DEVICE_COUNT, OUTPUT_EMB_JACK_ID_, (,))
	),
};

static void usb_midi_configure_interface(struct usb_desc_header *head, uint8_t bInterfaceNumber)
{
	LOG_DBG("Configuring interface: %d (head=%p, ms=%p)", (int)bInterfaceNumber, head, &midistreaming_cfg);

	if (head == (struct usb_desc_header *) &midistreaming_cfg){
		#define JACK_STR_VAR_NAME(inst) JACK_STR_VAR_NAME_(inst)
		#define JACK_STR_VAR_NAME_(inst) inst##_str
		#define SET_IJACK(idx, array, compat) \
			midistreaming_cfg.array[idx].in_jack.iJack = usb_get_str_descriptor_idx(& JACK_STR_VAR_NAME(DT_INST(idx, compat))); \
			midistreaming_cfg.array[idx].out_jack.iJack = usb_get_str_descriptor_idx(& JACK_STR_VAR_NAME(DT_INST(idx, compat)));
		LISTIFY(MIDI_INPUT_DEVICE_COUNT, SET_IJACK, (), inputs, COMPAT_MIDI_INPUT)
		LISTIFY(MIDI_OUTPUT_DEVICE_COUNT, SET_IJACK, (), outputs, COMPAT_MIDI_OUTPUT)
		#undef JACK_STR_VAR_NAME
		#undef JACK_STR_VAR_NAME_
		#undef SET_IJACK
	}

}

static void usb_midi_status_callback(struct usb_cfg_data *cfg, enum usb_dc_status_code status,
				     const uint8_t *param)
{
	ARG_UNUSED(cfg);
	switch (status) {
	case USB_DC_ERROR:
		LOG_DBG("USB error reported by the controller");
		break;
	case USB_DC_RESET:
		LOG_DBG("USB reset");
		break;
	case USB_DC_CONNECTED:
		LOG_DBG("USB connection established, hardware enumeration is completed");
		break;
	case USB_DC_CONFIGURED:
		LOG_DBG("USB configuration done");
		break;
	case USB_DC_DISCONNECTED:
		LOG_DBG("USB connection lost");
		break;
	case USB_DC_SUSPEND:
		LOG_DBG("USB connection suspended by the HOST");
		break;
	case USB_DC_RESUME:
		LOG_DBG("USB connection resumed by the HOST");
		break;
	case USB_DC_INTERFACE:
		LOG_DBG("USB interface selected");
		break;
	case USB_DC_SET_HALT:
		LOG_DBG("Set Feature ENDPOINT_HALT received");
		break;
	case USB_DC_CLEAR_HALT:
		LOG_DBG("Clear Feature ENDPOINT_HALT received");
		break;
	case USB_DC_SOF:
		LOG_DBG("Start of Frame received");
		break;
	case USB_DC_UNKNOWN:
		LOG_DBG("Initial USB connection status");
		break;
	}
}

USBD_DEFINE_CFG_DATA(primary) = {
	.usb_device_description = NULL,
	.interface_config = usb_midi_configure_interface,
	.interface_descriptor = &midistreaming_cfg,
	.cb_usb_status = usb_midi_status_callback,
	.interface =
		{
			.class_handler = NULL,
			.custom_handler = NULL,
			.vendor_handler = NULL,
		},
	.num_endpoints = ARRAY_SIZE(ep_cfg),
	.endpoint = ep_cfg,
};

struct usb_midi_io_config {
	struct usb_midi_io_descriptor *desc;
	uint8_t cable_number;
	bool is_input;
};

struct usb_midi_io_data {
	usb_midi_rx_callback callback;
};

static int usb_midi_io_init(const struct device *dev)
{
	LOG_DBG("%s: cfg=%p data=%p", dev->name, dev->config, dev->data);
	return 0;
}

#define DEFINE_USB_MIDI_INPUT_DEVICE(index, _) \
	static const struct usb_midi_io_config usb_midi_input##index##_config = { \
		.desc = &midistreaming_cfg.inputs[index], \
		.cable_number = index, \
		.is_input = true, \
	}; \
	DEVICE_DT_DEFINE(DT_INST(index, COMPAT_MIDI_INPUT),\
		         usb_midi_io_init, \
		         NULL,\
		         NULL, \
		         &usb_midi_input##index##_config,        \
			 APPLICATION, \
			 CONFIG_KERNEL_INIT_PRIORITY_DEVICE, \
			 NULL);

LISTIFY(MIDI_INPUT_DEVICE_COUNT, DEFINE_USB_MIDI_INPUT_DEVICE, ())

#define DEFINE_USB_MIDI_OUTPUT_DEVICE(index, _) \
	static const struct usb_midi_io_config usb_midi_output##index##_config = { \
		.desc = &midistreaming_cfg.outputs[index], \
		.cable_number = index, \
		.is_input = false, \
	}; \
	static struct usb_midi_io_data usb_midi_output##index##_data; \
	DEVICE_DT_DEFINE(DT_INST(index, COMPAT_MIDI_OUTPUT), \
			 usb_midi_io_init, \
		         NULL, \
		         &usb_midi_output##index##_data, \
		         &usb_midi_output##index##_config,        \
			 APPLICATION, \
			 CONFIG_KERNEL_INIT_PRIORITY_DEVICE, \
			 NULL);

LISTIFY(MIDI_OUTPUT_DEVICE_COUNT, DEFINE_USB_MIDI_OUTPUT_DEVICE, ())

#define DEVICE_DT_GET_DT_INST(index, compat) DEVICE_DT_GET(DT_INST(index, compat))
const struct device *outputs[] = {LISTIFY(MIDI_OUTPUT_DEVICE_COUNT, DEVICE_DT_GET_DT_INST, (,), COMPAT_MIDI_OUTPUT)};

static const int CIN_DATASIZE[16] = {-1, -1, 2, 3, 3, 1, 2, 3, 3, 3, 3, 3, 2, 2, 3, 1};

static inline void usb_midi_dispatch(const struct device *dev, uint8_t *packet)
{
	struct usb_midi_io_data *drv_data = (struct usb_midi_io_data *) dev->data;
	if (drv_data && drv_data->callback){
		drv_data->callback(dev, &packet[1], CIN_DATASIZE[packet[0] & 0x0f]);
	}
}

static void usb_midi_rx(uint8_t ep, enum usb_dc_ep_cb_status_code ep_status)
{
	uint32_t read = 0;
	uint8_t buf[USB_MIDI_BULK_SIZE];
	int r = usb_ep_read_wait(USB_MIDI_FROM_HOST_ENDPOINT_ID, buf, sizeof(buf), &read);
	if (r){
		LOG_ERR("USB read error %d", r);
		return;
	}

	__ASSERT(read % 4 == 0, "Unaligned USB read");

	LOG_DBG("READ %dB from host:", read);
	for (int i=0; i<read; i+=4){
		uint8_t cable_number = buf[i] >> 4;
		if (cable_number > MIDI_OUTPUT_DEVICE_COUNT){
			continue;
		}
		usb_midi_dispatch(outputs[cable_number], &buf[i]);
	}

	r = usb_ep_read_continue(USB_MIDI_FROM_HOST_ENDPOINT_ID);
	if (r){
		LOG_ERR("Unable to pursue reading from host: %d", r);
	}
}

int usb_midi_register(const struct device *dev, usb_midi_rx_callback cb)
{
	struct usb_midi_io_data *drv_data = (struct usb_midi_io_data *) dev->data;
	if (drv_data == NULL){
		return -ENOTSUP;
	}
	drv_data->callback = cb;
	return 0;
}

int usb_midi_write(const struct device *dev, const uint8_t *data, size_t len)
{
	uint8_t packet[USB_MIDI_BULK_SIZE];
	size_t read, written;
	uint8_t midi_cmd, datasize;
	const struct usb_midi_io_config *config = dev->config;
	if (! config->is_input){
		return -ENOTSUP;
	}

	if (len < 1){
		return 0;
	}

	read = 0;
	for (written=0; written<sizeof(packet) && read<len; written+=4) {
		midi_cmd = data[read] >> 4;
		if (! (midi_cmd & 0b1000)){
			return -EINVAL;
		}

		datasize = midi_datasize(midi_cmd);
		if (read+datasize+1 > len){
			return -EINVAL;
		}

		packet[written] = (config->cable_number << 4) | midi_cmd;
		memcpy(&packet[written+1], &data[read], datasize+1);
		read = read + 1 + datasize;
	}

	int r = usb_write(USB_MIDI_TO_HOST_ENDPOINT_ID, packet, written, NULL);
	if (r < 0){
		return -EIO;
	}
	return read;
}
