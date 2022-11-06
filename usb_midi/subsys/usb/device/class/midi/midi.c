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

struct usb_midi_output_data {
	usb_midi_rx_callback callback;
};

struct usb_midi_input_data {
	bool in_sysex;
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
	static struct usb_midi_input_data usb_midi_input##index##_data; \
	DEVICE_DT_DEFINE(DT_INST(index, COMPAT_MIDI_INPUT),\
		         usb_midi_io_init, \
		         NULL,\
		         &usb_midi_input##index##_data, \
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
	static struct usb_midi_output_data usb_midi_output##index##_data; \
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

/* USB MIDI Event packet: USB-MIDI header + 3 MIDI Bytes (padding if needed)
 * (midi10, Figure 8)
 */
NET_BUF_POOL_FIXED_DEFINE(tx_pool, 10, USB_MIDI_BULK_SIZE, 4, NULL);
/* Plain MIDI Bytes */
NET_BUF_POOL_FIXED_DEFINE(rx_pool, 10, 3 * USB_MIDI_BULK_SIZE / 4, 4, NULL);

/* Number of MIDI bytes in a USB MIDI packet given its Code Index Number */
static const int CIN_DATASIZE[16] = {-1, -1, 2, 3, 3, 1, 2, 3, 3, 3, 3, 3, 2, 2, 3, 1};

#define LOG_USB_MIDI_PACKET(index, packet) \
	switch (CIN_DATASIZE[(packet)[0] & 0x0f]){ \
	case 1: \
		LOG_DBG("%2d | %02X %02X", (index), (packet)[0], (packet)[1]); \
		break; \
	case 2: \
		LOG_DBG("%2d | %02X %02X %02X", (index), (packet)[0], (packet)[1], (packet)[2]); \
		break; \
	case 3: \
		LOG_DBG("%2d | %02X %02X %02X %02X", (index), (packet)[0], (packet)[1], (packet)[2], (packet)[3]); \
		break; \
	default: \
		break; \
	}

static void usb_midi_rx(uint8_t ep, enum usb_dc_ep_cb_status_code ep_status)
{
	uint32_t read = 0;
	size_t len[MIDI_OUTPUT_DEVICE_COUNT] = {0};
	struct net_buf *per_dev[MIDI_OUTPUT_DEVICE_COUNT] = {NULL};

	uint8_t buf[USB_MIDI_BULK_SIZE];
	int r = usb_ep_read_wait(USB_MIDI_FROM_HOST_ENDPOINT_ID, buf, sizeof(buf), &read);
	if (r){
		LOG_ERR("USB read error %d", r);
		return;
	}

	__ASSERT(read % 4 == 0, "Unaligned USB read");

	LOG_DBG("Rx %dB from host", read);
	for (int i=0; i<read; i+=4){
		uint8_t cable_number = buf[i] >> 4;
		int datasize = CIN_DATASIZE[buf[i] & 0x0f];
		if (datasize < 0 || cable_number > MIDI_OUTPUT_DEVICE_COUNT){
			continue;
		}
		const struct device *dev = outputs[cable_number];
		struct usb_midi_output_data *drv_data = (struct usb_midi_output_data *) dev->data;
		if (! (drv_data && drv_data->callback)){
			continue;
		}

		if (per_dev[cable_number] == NULL){
			per_dev[cable_number] = net_buf_alloc(&rx_pool, K_NO_WAIT);
			if (per_dev[cable_number] == NULL){
				LOG_WRN("Cannot allocate rx buffer");
				continue;
			}
		}

		memcpy(&per_dev[cable_number]->data[len[cable_number]], &buf[i+1], datasize);
		len[cable_number] += datasize;
		LOG_USB_MIDI_PACKET(i, &buf[i]);
	}

	for (int i=0; i<MIDI_OUTPUT_DEVICE_COUNT; i++){
		if (per_dev[i]){
			const struct device *dev = outputs[i];
			struct usb_midi_output_data *drv_data = (struct usb_midi_output_data *) dev->data;
			if (drv_data && drv_data->callback){
				drv_data->callback(dev, per_dev[i]->data, len[i]);
			}
			net_buf_unref(per_dev[i]);
		}
	}
	r = usb_ep_read_continue(USB_MIDI_FROM_HOST_ENDPOINT_ID);
	if (r){
		LOG_ERR("Cannot pursue reading from USB");
	}
}

int usb_midi_register(const struct device *dev, usb_midi_rx_callback cb)
{
	const struct usb_midi_io_config *config = dev->config;
	if (config->is_input){
		LOG_ERR("Can only register to USB-MIDI output");
		return -ENOTSUP;
	}
	struct usb_midi_output_data *drv_data = (struct usb_midi_output_data *) dev->data;
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

static inline int cin_for_midi_status_byte(uint8_t status_byte)
{
	if (MIDI_CMD_NOTE_OFF <= status_byte && status_byte < MIDI_CMD_SYSTEM_COMMON) {
		return status_byte >> 4;
	}

	switch (status_byte){
	case MIDI_SYS_TIMECODE:
	case MIDI_SYS_SONG_SELECT:
		return CIN_SYS_COMMON_2B;
	case MIDI_SYS_SONG_POSITION:
		return CIN_SYS_COMMON_3B;
	case MIDI_SYS_TUNE_REQUEST:
	case MIDI_SYS_CLOCK:
	case MIDI_SYS_START:
	case MIDI_SYS_CONTINUE:
	case MIDI_SYS_STOP:
	case MIDI_SYS_ACTIVE_SENSE:
	case MIDI_SYS_RESET:
		return CIN_SYS_COMMON_1B;
	}

	return -1;
}

int usb_midi_write(const struct device *dev, const uint8_t *data, size_t len)
{
	int err;
	int cin;
	struct net_buf *packet = NULL;
	size_t consumed, written;
	const struct usb_midi_io_config *config = dev->config;
	struct usb_midi_input_data *drv_data = dev->data;
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

	if (usb_transfer_is_busy(USB_MIDI_TO_HOST_ENDPOINT_ID)){
		return -EBUSY;
	}

	packet = net_buf_alloc(&tx_pool, K_NO_WAIT);
	if (packet == NULL) {
		err = -ENOMEM;
		goto fail;
	}

	consumed = 0;
	for (written=0; written<packet->size && consumed<len; written+=4) {
		size_t remaining = len - consumed;
		if (data[consumed] == MIDI_SYSEX_START){
			LOG_DBG("Entering SysEx");
			drv_data->in_sysex = true;
		}

		if (! drv_data->in_sysex){
			cin = cin_for_midi_status_byte(data[consumed]);
		} else if (data[consumed] == MIDI_SYSEX_STOP){
			cin = CIN_SYSEX_END_1B;
			drv_data->in_sysex = false;
			LOG_DBG("Leaving SysEx");
		} else if (remaining > 1 && data[consumed+1] == MIDI_SYSEX_STOP){
			cin = CIN_SYSEX_END_2B;
			drv_data->in_sysex = false;
			LOG_DBG("Leaving SysEx");
		} else if (remaining > 2 && data[consumed+2] == MIDI_SYSEX_STOP){
			cin = CIN_SYSEX_END_3B;
			drv_data->in_sysex = false;
			LOG_DBG("Leaving SysEx");
		} else {
			cin = CIN_SYSEX_START;
		}

		if (cin < 0){
			LOG_ERR("No Code Index Number for midi status byte %02X", data[consumed]);
			err = -EINVAL;
			goto fail;
		}

		int datasize = CIN_DATASIZE[cin];
		if (datasize < 0){
			LOG_ERR("Unknown datasize for CIN %X", cin);
			err = -ENOTSUP;
			goto fail;
		}

		if (datasize > remaining){
			LOG_ERR("Not enough data for midi status byte %02X (expected %d but got only %d)", data[consumed], datasize, len-consumed);
			err = -EINVAL;
			goto fail;
		}

		packet->data[written] = (config->cable_number << 4) | cin;
		memcpy(&packet->data[written+1], &data[consumed], datasize);
		consumed += datasize;

		LOG_USB_MIDI_PACKET(written, &packet->data[written]);
	}

	usb_transfer(USB_MIDI_TO_HOST_ENDPOINT_ID, packet->data, written,
		     USB_TRANS_WRITE | USB_TRANS_NO_ZLP,
		     usb_midi_transfer_done, packet);
	return consumed;

	fail:
		if (packet != NULL){
			net_buf_unref(packet);
		}
		return err;
}
