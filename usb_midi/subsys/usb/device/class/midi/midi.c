#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/sys/ring_buffer.h>
#include <usb_descriptor.h>

#include <usb_midi.h>

#include "usb_midi_internal.h"

LOG_MODULE_REGISTER(usb_midi, CONFIG_USB_MIDI_LOG_LEVEL);

#define COMPAT_MIDI_INPUT  usb_midi_input
#define COMPAT_MIDI_OUTPUT usb_midi_output

#define MIDI_OUTPUT_DEVICE_COUNT DT_NUM_INST_STATUS_OKAY(COMPAT_MIDI_OUTPUT)
#define MIDI_INPUT_DEVICE_COUNT DT_NUM_INST_STATUS_OKAY(COMPAT_MIDI_INPUT)


#if CONFIG_USB_DEVICE_MIDI_NODELABEL_AS_NAME
#define USB_MIDI_IO_STR_DECLARE(inst) \
	struct { \
		uint8_t bLength; \
		uint8_t bDescriptorType; \
		uint8_t bString[USB_BSTRING_LENGTH(DT_NODE_FULL_NAME(inst))]; \
	} __packed

#define USB_MIDI_IO_STR_DESCRIPTOR(inst) \
	USBD_STRING_DESCR_USER_DEFINE(primary) USB_MIDI_IO_STR_DECLARE(inst) inst##_str = { \
		.bLength = USB_STRING_DESCRIPTOR_LENGTH(DT_NODE_FULL_NAME(inst)), \
		.bDescriptorType = USB_DESC_STRING, \
		.bString = DT_NODE_FULL_NAME(inst), \
	};

DT_FOREACH_STATUS_OKAY(COMPAT_MIDI_INPUT, USB_MIDI_IO_STR_DESCRIPTOR)
DT_FOREACH_STATUS_OKAY(COMPAT_MIDI_OUTPUT, USB_MIDI_IO_STR_DESCRIPTOR)

struct usb_str_descriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint8_t bString[];
} __packed;

#define USB_MIDI_IO_STR_REF(inst) &inst##_str,

const struct usb_str_descriptor *input_names[] = {
	DT_FOREACH_STATUS_OKAY(COMPAT_MIDI_INPUT, USB_MIDI_IO_STR_REF)
};

const struct usb_str_descriptor *output_names[] = {
	DT_FOREACH_STATUS_OKAY(COMPAT_MIDI_OUTPUT, USB_MIDI_IO_STR_REF)
};
#endif // CONFIG_USB_DEVICE_MIDI_NODELABEL_AS_NAME


#define INPUT_INDEX(dt_index) (dt_index)
#define OUTPUT_INDEX(dt_index) (dt_index + MIDI_INPUT_DEVICE_COUNT)

#define INPUT_EXT_JACK_ID(dt_index) (2 * INPUT_INDEX(dt_index) + 1)
#define INPUT_EMB_JACK_ID(dt_index) (2 * INPUT_INDEX(dt_index) + 2)
#define OUTPUT_EXT_JACK_ID(dt_index) (2 * OUTPUT_INDEX(dt_index) + 1)
#define OUTPUT_EMB_JACK_ID(dt_index) (2 * OUTPUT_INDEX(dt_index) + 2)

#define INPUT_EMB_JACK_ID_(x, _) INPUT_EMB_JACK_ID(x)
#define OUTPUT_EMB_JACK_ID_(x, _) OUTPUT_EMB_JACK_ID(x)

struct midi_input_descriptor {
	// External jack (outside -> midistreaming)
	struct usb_ms_in_jack_descriptor ext;
	// Embedded jack (midistreaming -> host)
	struct usb_ms_out_jack_descriptor emb;
} __packed;

struct midi_output_descriptor {
	// Embedded jack (host -> midistreaming)
	struct usb_ms_in_jack_descriptor emb;
	// External jack (midistreaming -> outside)
	struct usb_ms_out_jack_descriptor ext;
} __packed;

#define MIDI_INPUT_JACKS(index, _) \
	{\
		.ext = { \
			.bLength=sizeof(struct usb_ms_in_jack_descriptor), \
			.bDescriptorType=USB_DESC_CS_INTERFACE, \
			.bDescriptorSubtype=MIDI_IN_JACK, \
			.bJackType=JACK_EXTERNAL, \
			.bJackID=INPUT_EXT_JACK_ID(index), \
		},\
		.emb = { \
			.bLength=sizeof(struct usb_ms_out_jack_descriptor), \
			.bDescriptorType=USB_DESC_CS_INTERFACE, \
			.bDescriptorSubtype=MIDI_OUT_JACK, \
			.bJackType=JACK_EMBEDDED, \
			.bJackID=INPUT_EMB_JACK_ID(index), \
			.bNrInputPins=1, \
			.BASourceID1=INPUT_EXT_JACK_ID(index), \
			.BASourcePin1=1, \
		},\
	}

#define MIDI_OUTPUT_JACKS(index, _) \
	{\
		.emb = { \
			.bLength=sizeof(struct usb_ms_in_jack_descriptor), \
			.bDescriptorType=USB_DESC_CS_INTERFACE, \
			.bDescriptorSubtype=MIDI_IN_JACK, \
			.bJackType=JACK_EMBEDDED, \
			.bJackID=OUTPUT_EMB_JACK_ID(index), \
		},\
		.ext = { \
			.bLength=sizeof(struct usb_ms_out_jack_descriptor), \
			.bDescriptorType=USB_DESC_CS_INTERFACE, \
			.bDescriptorSubtype=MIDI_OUT_JACK, \
			.bJackType=JACK_EXTERNAL, \
			.bJackID=OUTPUT_EXT_JACK_ID(index), \
			.bNrInputPins=1, \
			.BASourceID1=OUTPUT_EMB_JACK_ID(index), \
			.BASourcePin1=1, \
		},\
	}

struct usb_ms_if_descriptor {
	/* AudioControl class header */
	struct usb_if_descriptor std_ac;
	struct usb_ac_cs_descriptor cs_ac;
	/* MidiStreaming class header */
	struct usb_if_descriptor std_ms;
	struct usb_ms_cs_if_descriptor cs_ms;
	/* Jacks */
	struct midi_input_descriptor inputs[MIDI_INPUT_DEVICE_COUNT];
	struct midi_output_descriptor outputs[MIDI_OUTPUT_DEVICE_COUNT];
	/* Endpoints */
	struct usb_ep_descriptor std_ep_inputs;
	USB_MS_CS_EP_DESCRIPTOR(MIDI_INPUT_DEVICE_COUNT) cs_ep_inputs;
	struct usb_ep_descriptor std_ep_outputs;
	USB_MS_CS_EP_DESCRIPTOR(MIDI_OUTPUT_DEVICE_COUNT) cs_ep_outputs;
} __packed;


static void usb_midi_rx(uint8_t ep, enum usb_dc_ep_cb_status_code ep_status);

static struct usb_ep_cfg_data ep_cfg[] = {
	{.ep_cb = usb_transfer_ep_callback, .ep_addr = USB_MIDI_TO_HOST_ENDPOINT_ID},
	{.ep_cb = usb_midi_rx, .ep_addr = USB_MIDI_FROM_HOST_ENDPOINT_ID},
};

USBD_CLASS_DESCR_DEFINE(primary, 0) struct usb_ms_if_descriptor midistreaming_cfg = {
	.std_ac = {
		.bLength = sizeof(struct usb_if_descriptor),
		.bDescriptorType = USB_DESC_INTERFACE,
		.bInterfaceNumber = 0,
		.bAlternateSetting = 0,
		.bNumEndpoints = 0,
		.bInterfaceClass = USB_BCC_AUDIO,
		.bInterfaceSubClass = USB_AUDIO_AUDIOCONTROL,
		.bInterfaceProtocol = 0,
		.iInterface = 0,
	},
	.cs_ac = {
		.bLength = sizeof(struct usb_ac_cs_descriptor),
		.bDescriptorType = USB_DESC_CS_INTERFACE,
		.bDescriptorSubtype = USB_AUDIO_HEADER,
		.bcdADC = sys_cpu_to_le16(0x0100),
		.wTotalLength = sys_cpu_to_le16(9),
		.bInCollection = 1,
		.baInterfaceNr1 = 1,
	},
	.std_ms = {
		.bLength = sizeof(struct usb_if_descriptor),
		.bDescriptorType = USB_DESC_INTERFACE,
		.bInterfaceNumber = 1,
		.bAlternateSetting = 0,
		.bNumEndpoints = 2,
		.bInterfaceClass = USB_BCC_AUDIO,
		.bInterfaceSubClass = USB_AUDIO_MIDISTREAMING,
		.bInterfaceProtocol = 0,
		.iInterface = 0
	},
	.cs_ms = {
		.bLength = sizeof(struct usb_ms_cs_if_descriptor),
		.bDescriptorType = USB_DESC_CS_INTERFACE,
		.bDescriptorSubtype = MS_HEADER,
		.bcdADC = sys_cpu_to_le16(0x0100),
		.wTotalLength = (
			sizeof(struct usb_ms_if_descriptor)
			- 2*sizeof(struct usb_if_descriptor)
			- sizeof(struct usb_ac_cs_descriptor)
		),
	},
	.inputs = {
		LISTIFY(MIDI_INPUT_DEVICE_COUNT, MIDI_INPUT_JACKS, (,))
	},
	.outputs = {
		LISTIFY(MIDI_OUTPUT_DEVICE_COUNT, MIDI_OUTPUT_JACKS, (,))
	},
	.std_ep_inputs = {
		.bLength = sizeof(struct usb_ep_descriptor),
		.bDescriptorType = USB_DESC_ENDPOINT,
		.bEndpointAddress = USB_MIDI_TO_HOST_ENDPOINT_ID,
		.bmAttributes = USB_DC_EP_BULK,
		.wMaxPacketSize = sys_cpu_to_le16(USB_MIDI_BULK_SIZE),
	},
	.cs_ep_inputs = {
		.bLength = sizeof(USB_MS_CS_EP_DESCRIPTOR(MIDI_INPUT_DEVICE_COUNT)),
		.bDescriptorType = USB_DESC_CS_ENDPOINT,
		.bDescriptorSubtype = MS_GENERAL,
		.bNumEmbMIDIJack = MIDI_INPUT_DEVICE_COUNT,
		.BAAssocJackID = {
			LISTIFY(MIDI_INPUT_DEVICE_COUNT, INPUT_EMB_JACK_ID_, (,))
		}
	},
	.std_ep_outputs = {
		.bLength = sizeof(struct usb_ep_descriptor),
		.bDescriptorType = USB_DESC_ENDPOINT,
		.bEndpointAddress = USB_MIDI_FROM_HOST_ENDPOINT_ID,
		.bmAttributes = USB_DC_EP_BULK,
		.wMaxPacketSize = sys_cpu_to_le16(USB_MIDI_BULK_SIZE),
	},
	.cs_ep_outputs = {
		.bLength = sizeof(USB_MS_CS_EP_DESCRIPTOR(MIDI_OUTPUT_DEVICE_COUNT)),
		.bDescriptorType = USB_DESC_CS_ENDPOINT,
		.bDescriptorSubtype = MS_GENERAL,
		.bNumEmbMIDIJack = MIDI_OUTPUT_DEVICE_COUNT,
		.BAAssocJackID = {
			LISTIFY(MIDI_OUTPUT_DEVICE_COUNT, OUTPUT_EMB_JACK_ID_, (,))
		}
	},
};

static void usb_midi_configure_interface(struct usb_desc_header *head, uint8_t bInterfaceNumber)
{
	size_t i;
	if (head != (struct usb_desc_header *) &midistreaming_cfg){
		return;
	}

	LOG_DBG("Configuring interface: %d (%p)", (int)bInterfaceNumber, &midistreaming_cfg);

#if CONFIG_USB_DEVICE_MIDI_NODELABEL_AS_NAME
	for (i=0; i<ARRAY_SIZE(midistreaming_cfg.inputs); i++){
		midistreaming_cfg.inputs[i].emb.iJack = usb_get_str_descriptor_idx(input_names[i]);
	}
	for (i=0; i<ARRAY_SIZE(midistreaming_cfg.outputs); i++){
		midistreaming_cfg.outputs[i].emb.iJack = usb_get_str_descriptor_idx(output_names[i]);
	}
#endif // CONFIG_USB_DEVICE_MIDI_NODELABEL_AS_NAME
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
	.interface = {
		.class_handler = NULL,
		.custom_handler = NULL,
		.vendor_handler = NULL,
	},
	.num_endpoints = ARRAY_SIZE(ep_cfg),
	.endpoint = ep_cfg,
};

struct usb_midi_io_config {
	bool is_input;
	struct usb_midi_input_descriptor *desc;
	uint8_t cable_number;
};

struct usb_midi_output_data {
	struct k_sem sem;
	struct ring_buf fifo;
	uint8_t fifo_data[16];
};

struct usb_midi_input_data {
	bool in_sysex;
};

static int usb_midi_io_init(const struct device *dev)
{
	int r = 0;
	struct usb_midi_output_data *drv_data;
	LOG_DBG("%s: cfg=%p data=%p", dev->name, dev->config, dev->data);
	struct usb_midi_io_config *cfg = dev->config;

	if (! cfg->is_input){
		drv_data = dev->data;
		ring_buf_init(&drv_data->fifo, sizeof(drv_data->fifo_data), drv_data->fifo_data);
		r = k_sem_init(&drv_data->sem, 1, 1);
	}
	return r;

}

#define DEFINE_USB_MIDI_INPUT_DEVICE(index, _) \
	static const struct usb_midi_io_config usb_midi_input##index##_config = { \
		.is_input = true, \
		.desc = &midistreaming_cfg.inputs[index], \
		.cable_number = index, \
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
		.is_input = false, \
		.desc = &midistreaming_cfg.outputs[index], \
		.cable_number = index, \
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

/* Number of MIDI bytes in a USB MIDI packet given its Code Index Number */
static const int CIN_DATASIZE[16] = {-1, -1, 2, 3, 3, 1, 2, 3, 3, 3, 3, 3, 2, 2, 3, 1};

#define LOG_USB_MIDI_PACKET(dev, index, packet) \
	switch (CIN_DATASIZE[(packet)[0] & 0x0f]){ \
	case 1: \
		LOG_DBG("[%s] %2d | %02X %02X", (dev)->name, (index), (packet)[0], (packet)[1]); \
		break; \
	case 2: \
		LOG_DBG("[%s] %2d | %02X %02X %02X", (dev)->name, (index), (packet)[0], (packet)[1], (packet)[2]); \
		break; \
	case 3: \
		LOG_DBG("[%s] %2d | %02X %02X %02X %02X", (dev)->name, (index), (packet)[0], (packet)[1], (packet)[2], (packet)[3]); \
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
		if (datasize < 1 || datasize > (read-i) || cable_number > MIDI_OUTPUT_DEVICE_COUNT){
			continue;
		}

		const struct device *dev = outputs[cable_number];
		struct usb_midi_output_data *out = (struct usb_midi_output_data *) dev->data;
		r = ring_buf_put(&out->fifo, &buf[i+1], datasize);
		if (r <= 0){
			LOG_ERR("[%s] ring_buf_put -> %d", dev->name, r);
		} else {
			LOG_DBG("[%s] ring_buf_space_get: %d", dev->name, ring_buf_space_get(&out->fifo));
		}
		k_sem_give(&out->sem);
		LOG_USB_MIDI_PACKET(dev, i/4, &buf[i]);
	}
	r = usb_ep_read_continue(USB_MIDI_FROM_HOST_ENDPOINT_ID);
	if (r){
		LOG_ERR("Cannot pursue reading from USB");
	}
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
	int r;
	const struct usb_midi_input_data *drv_data;
	const struct usb_midi_io_config *const config = dev->config;

	// Cannot write to an output
	if (! config->is_input) {
		return -ENOTSUP;
	}

	return 0;
}

int usb_midi_peak(const struct device *dev, const uint8_t **data, size_t len)
{
	int r;
	const struct usb_midi_output_data *drv_data;
	const struct usb_midi_io_config *const config = dev->config;

	// Cannot read from an input
	if (config->is_input) {
		return -ENOTSUP;
	}

	drv_data = dev->data;
	if (r = k_sem_take(&drv_data->sem, K_FOREVER)){
		return r;
	}
	r = ring_buf_get_claim(&drv_data->fifo, data, len);
	return r;
}

int usb_midi_read_continue(const struct device *dev, size_t len)
{
	int r;
	const struct usb_midi_output_data *drv_data;
	const struct usb_midi_io_config *const config = dev->config;

	// Cannot read from an input
	if (config->is_input) {
		return -ENOTSUP;
	}

	drv_data = dev->data;
	r = ring_buf_get_finish(&drv_data->fifo, len);
	return r;
}
