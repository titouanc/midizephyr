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

static enum usb_dc_status_code current_usb_status = USB_DC_UNKNOWN;

static void usb_midi_status_callback(struct usb_cfg_data *cfg, enum usb_dc_status_code status,
				     const uint8_t *param)
{
	ARG_UNUSED(cfg);
	current_usb_status = status;
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

NET_BUF_POOL_FIXED_DEFINE(buf_pool, 10, USB_MIDI_BULK_SIZE, 4, NULL);

static const int CIN_DATASIZE[16] = {-1, -1, 2, 3, 3, 1, 2, 3, 3, 3, 3, 3, 2, 2, 3, 1};

static void usb_midi_rx(uint8_t ep, enum usb_dc_ep_cb_status_code ep_status)
{
	uint32_t read = 0;
	size_t len[MIDI_OUTPUT_DEVICE_COUNT] = {0};
	struct net_buf *per_dev[MIDI_OUTPUT_DEVICE_COUNT] = {NULL};

	uint8_t buf[USB_MIDI_BULK_SIZE];
	int r = usb_read(USB_MIDI_FROM_HOST_ENDPOINT_ID, buf, sizeof(buf), &read);
	if (r){
		LOG_ERR("USB read error %d", r);
		return;
	}

	__ASSERT(read % 4 == 0, "Unaligned USB read");

	LOG_DBG("Rx %dB from host", read);
	for (int i=0; i<read; i+=4){
		uint8_t cable_number = buf[i] >> 4;
		uint8_t datasize = CIN_DATASIZE[buf[i] & 0x0f];
		if (cable_number > MIDI_OUTPUT_DEVICE_COUNT){
			continue;
		}
		if (per_dev[cable_number] == NULL){
			per_dev[cable_number] = net_buf_alloc(&buf_pool, K_NO_WAIT);
		}

		LOG_DBG("CIN=%d CN=%d | %02X %02X %02X", buf[i] & 0x0f, cable_number, buf[i+1], buf[i+2], buf[i+3]);
		memcpy(&per_dev[cable_number]->data[len[cable_number]], &buf[i+1], datasize);
		len[cable_number] += datasize;
	}

	for (int i=0; i<MIDI_OUTPUT_DEVICE_COUNT; i++){
		if (len[i]){
			const struct device *dev = outputs[i];
			struct usb_midi_io_data *drv_data = (struct usb_midi_io_data *) dev->data;
			if (drv_data && drv_data->callback){
				drv_data->callback(dev, per_dev[i], len[i]);
			}
		}
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

static void usb_midi_transfer_done(uint8_t ep, int size, void *priv)
{
	net_buf_unref((struct net_buf *) priv);
}

int usb_midi_write_buf(const struct device *dev, struct net_buf *buf, size_t len)
{
	int r = 0;
	int written = 0;
	while (written < len){
		r = usb_midi_write(dev, &buf->data[written], len-written);
		if (r < 0){
			return r;
		}
		written += r;
	}
	net_buf_unref(buf);
	return written;
}

int usb_midi_write(const struct device *dev, const uint8_t *data, size_t len)
{
	struct net_buf *packet = NULL;
	size_t read, written;
	uint8_t midi_cmd, datasize;
	const struct usb_midi_io_config *config = dev->config;
	if (! config->is_input){
		LOG_ERR("Can only write to USB-MIDI input");
		return -ENOTSUP;
	}

	if (current_usb_status != USB_DC_CONFIGURED){
		LOG_ERR("USB not ready");
		return -EIO;
	}

	if (len < 1){
		return 0;
	}

	packet = net_buf_alloc(&buf_pool, K_NO_WAIT);
	if (packet == NULL) {
		return -ENOMEM;
	}

	read = 0;
	for (written=0; written<packet->size && read<len; written+=4) {
		midi_cmd = data[read] >> 4;
		if (! (midi_cmd & 0b1000)){
			LOG_ERR("Invalid MIDI command");
			return -EINVAL;
		}

		datasize = midi_datasize(midi_cmd);
		if (read+datasize+1 > len){
			LOG_ERR("Not enough data for MIDI cmd %X (expecting %dB but got only %dB)", midi_cmd, datasize, len-read);
			return -EINVAL;
		}

		packet->data[written] = (config->cable_number << 4) | midi_cmd;
		memcpy(&packet->data[written+1], &data[read], datasize+1);
		read = read + 1 + datasize;

		LOG_DBG("%2X | %02X %02X %02X %02X", written, packet->data[written], packet->data[written+1], packet->data[written+2], packet->data[written+3]);
	}

	usb_transfer(USB_MIDI_TO_HOST_ENDPOINT_ID, packet->data, written,
		     USB_TRANS_WRITE | USB_TRANS_NO_ZLP,
		     usb_midi_transfer_done, packet);
	return read;
}
