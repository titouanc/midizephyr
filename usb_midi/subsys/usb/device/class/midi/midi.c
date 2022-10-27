#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/usb/usb_device.h>

#include "midi10.h"
#include <usb_midi.h>
#include <usb_descriptor.h>

LOG_MODULE_REGISTER(usb_midi, CONFIG_USB_MIDI_LOG_LEVEL);

#define EXTERNAL_JACK_ID(inst) (0x01 + DT_NODE_CHILD_IDX(inst))
#define EMBEDDED_JACK_ID(inst) (0x41 + DT_NODE_CHILD_IDX(inst))
#define JACK_ISTR(inst)        (0x04 + DT_NODE_CHILD_IDX(inst))

#define SEP_EXTERNAL_JACK_ID(inst) ,(0x01 + DT_NODE_CHILD_IDX(inst))
#define SEP_EMBEDDED_JACK_ID(inst) ,(0x41 + DT_NODE_CHILD_IDX(inst))

// External jack in -> embedded jack out
#define USB_MIDI_INPUT_SEP(inst) \
	USB_MIDI_JACKIN_DESCRIPTOR(JACK_EXTERNAL, EXTERNAL_JACK_ID(inst), JACK_ISTR(inst)), \
	USB_MIDI_JACKOUT_DESCRIPTOR(JACK_EMBEDDED, EMBEDDED_JACK_ID(inst), JACK_ISTR(inst), \
				    EXTERNAL_JACK_ID(inst), 1),

// Embedded jack in -> external jack out
#define USB_MIDI_OUTPUT_SEP(inst) \
	USB_MIDI_JACKIN_DESCRIPTOR(JACK_EMBEDDED, EMBEDDED_JACK_ID(inst), JACK_ISTR(inst)), \
	USB_MIDI_JACKOUT_DESCRIPTOR(JACK_EXTERNAL, EXTERNAL_JACK_ID(inst), JACK_ISTR(inst), \
				    EMBEDDED_JACK_ID(inst), 1),

#define USB_MIDI_JACK_STRING_DECLARE(inst) \
	struct inst##_str_desc { \
		uint8_t bLength; uint8_t bDescriptorType; \
		uint8_t bString[USB_BSTRING_LENGTH(DT_NODE_FULL_NAME(inst))]; \
	} __packed;

#define USB_MIDI_JACK_STRING_FILL(inst) \
	USBD_STRING_DESCR_USER_DEFINE(primary) struct inst##_str_desc inst##_str = { \
		.bLength = USB_STRING_DESCRIPTOR_LENGTH(DT_NODE_FULL_NAME(inst)), \
		.bDescriptorType = USB_DESC_STRING, \
		.bString = DT_NODE_FULL_NAME(inst), \
	};

#define USB_MIDI_QUEUE_DEFINE(name) \
	RING_BUF_ITEM_DECLARE_POW2(name##_buf, CONFIG_USB_DEVICE_MIDI_QUEUE_SIZE_POW2); \
	K_SEM_DEFINE(name##_read_sem, 1, 1); \
	K_SEM_DEFINE(name##_write_sem, 1, 1); \
	K_SEM_DEFINE(name##_data_ready_sem, 1, 1); \
	static struct usb_midi_queue name = {\
		.buf = &name##_buf,\
		.read_sem = &name##_read_sem,\
		.write_sem = &name##_write_sem,\
		.data_ready_sem = &name##_data_ready_sem,\
	}

struct usb_midi_queue {
	struct ring_buf *buf;
	struct k_sem *read_sem;
	struct k_sem *write_sem;
	struct k_sem *data_ready_sem;
};

struct usb_midi_jack {
	uint8_t jack_id;
	struct usb_midi_queue *queue;
};

struct usb_midi_api {
	int (*write)(const struct device *dev, const uint8_t *buf, size_t size);
	int (*read)(const struct device *dev, uint8_t *buf, size_t size, k_timeout_t timeout);
};

USB_MIDI_QUEUE_DEFINE(usb_midi_to_host_queue);

///////////////////////////////////////////////////////////////////////////////
static int usb_midi_input_init(const struct device *dev)
{
	return 0;
}

static const int CIN_DATASIZE[16] = {-1, -1, 2, 3, 3, 1, 2, 3, 3, 3, 3, 3, 2, 2, 3, 1};

static int usb_midi_input_write(const struct device *dev, const uint8_t *buf, size_t size)
{
	const struct usb_midi_jack *jack = dev->config;
	int r = k_sem_take(jack->queue->write_sem, K_NO_WAIT);
	if (r){
		LOG_WRN("Unable to take write semaphore");
		return -EBUSY;
	}

	size_t i = 0;
	uint8_t *dest;

	while (i < size){
		size_t claimed = 0;
		size_t written = 0;
		size_t read = 0;

		claimed = ring_buf_put_claim(jack->queue->buf, &dest, 4);
		if (claimed < 4){
			LOG_WRN("Unable to reserve 4B");
		} else {
			uint8_t cmd = buf[i] >> 4;
			int len = CIN_DATASIZE[cmd];
			__ASSERT(cmd & 0b1000, "Invalid MIDI command");
			__ASSERT(len > 0, "Unknown MIDI data size");
			if (i + len > size) {
				LOG_WRN("Not enough space for USB MIDI packet of size %d", len);
			} else {
				// dest[0] = (jack->jack_id << 4) | cmd;
				dest[0] = cmd;
				memcpy(&dest[1], &buf[i], len);
				written = 4;
				read = len;
				i += read;
			}

		}

		ring_buf_put_finish(jack->queue->buf, written);
		if (written > 0){
			k_sem_give(jack->queue->data_ready_sem);
		} else {
			break;
		}
	}

	k_sem_give(jack->queue->write_sem);
	return i;
}

static const struct usb_midi_api usb_midi_input_api = {
	.write = usb_midi_input_write,
};

#define USB_MIDI_INPUT_INIT(inst)                                                       \
	static const struct usb_midi_jack inst##_config = {                \
		.jack_id = 0x41 + DT_NODE_CHILD_IDX(inst), \
		.queue = &usb_midi_to_host_queue,                                                     \
	};                                                                                         \
                                                                                                   \
	DEVICE_DT_DEFINE(inst, usb_midi_input_init, NULL, NULL, &inst##_config,        \
			 POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE, &usb_midi_input_api);


DT_FOREACH_STATUS_OKAY(usb_midi_input, USB_MIDI_INPUT_INIT)
///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
static int usb_midi_output_init(const struct device *dev)
{
	return 0;
}

static int usb_midi_output_read(const struct device *dev, uint8_t *buf, size_t size, k_timeout_t timeout)
{
	const struct usb_midi_jack *jack = dev->config;
	int r = k_sem_take(jack->queue->data_ready_sem, timeout);
	if (r) {
		return r;
	}

	r = k_sem_take(jack->queue->read_sem, timeout);
	if (r){
		k_sem_give(jack->queue->data_ready_sem);
		LOG_WRN("Unable to take read semaphore");
		return r;
	}

	r = ring_buf_get(jack->queue->buf, buf, size);
	if (! ring_buf_is_empty(jack->queue->buf)){
		k_sem_give(jack->queue->data_ready_sem);
	}

	k_sem_give(jack->queue->read_sem);
	return r;
}

static const struct usb_midi_api usb_midi_output_api = {
	.read = usb_midi_output_read,
};

#define USB_MIDI_OUTPUT_INIT(inst)                                                       \
	USB_MIDI_QUEUE_DEFINE(inst##_queue); \
	static const struct usb_midi_jack inst##_config = {                \
		.jack_id = 0x41 + DT_NODE_CHILD_IDX(inst), \
		.queue = &inst##_queue,                                                     \
	};                                                                                         \
                                                                                                   \
	DEVICE_DT_DEFINE(inst, usb_midi_output_init, NULL, NULL, &inst##_config,        \
			 POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE, &usb_midi_output_api);

DT_FOREACH_STATUS_OKAY(usb_midi_output, USB_MIDI_OUTPUT_INIT)
///////////////////////////////////////////////////////////////////////////////

int usb_midi_write(const struct device *dev, const uint8_t *buf, size_t size)
{
	const struct usb_midi_api *api = dev->api;
	if (api == NULL || api->write == NULL) {
		return -ENOTSUP;
	}
	int written = 0;
	int r;
	while (written < size) {
		r = api->write(dev, &buf[written], size - written);
		LOG_DBG("write(%s, %p, %d) -> %d", dev->name, &buf[written], size-written, r);
		if (r < 0){
			return r;
		}
		if (r == 0){
			break;
		}
		written += r;
	}
	return written;
}

int usb_midi_read(const struct device *dev, uint8_t *buf, size_t size, k_timeout_t timeout)
{
	const struct usb_midi_api *api = dev->api;
	if (api == NULL || api->read == NULL) {
		return -ENOTSUP;
	}
	return api->read(dev, buf, size, timeout);
}

static void usb_midi_tx(uint8_t ep, enum usb_dc_ep_cb_status_code ep_status);
static void usb_midi_rx(uint8_t ep, enum usb_dc_ep_cb_status_code ep_status);

static struct usb_ep_cfg_data ep_cfg[] = {
	{.ep_cb = usb_midi_tx, .ep_addr = USB_MIDI_TO_HOST_ENDPOINT_ID},
	{.ep_cb = usb_midi_rx, .ep_addr = USB_MIDI_FROM_HOST_ENDPOINT_ID},
};

DT_FOREACH_STATUS_OKAY(usb_midi_input, USB_MIDI_JACK_STRING_DECLARE)
DT_FOREACH_STATUS_OKAY(usb_midi_output, USB_MIDI_JACK_STRING_DECLARE)
DT_FOREACH_STATUS_OKAY(usb_midi_input, USB_MIDI_JACK_STRING_FILL)
DT_FOREACH_STATUS_OKAY(usb_midi_output, USB_MIDI_JACK_STRING_FILL)

USBD_CLASS_DESCR_DEFINE(primary, 0)
struct usb_midi_if_descriptor midistreaming_cfg = {
	.std_audiocontrol = {
		.bLength = 9,
		.bDescriptorType = USB_DESC_INTERFACE,
		.bInterfaceNumber = 0,
		.bAlternateSetting = 0,
		.bNumEndpoints = 0,
		.bInterfaceClass = USB_BCC_AUDIO,
		.bInterfaceSubClass = USB_AUDIO_AUDIOCONTROL,
		.bInterfaceProtocol = 0,
		.iInterface = 0,
	},
	.cs_audiocontrol = {
		.bLength = 9,
		.bDescriptorType = USB_DESC_CS_INTERFACE,
		.bDescriptorSubtype = USB_AUDIO_HEADER,
		.bcdADC = sys_cpu_to_le16(0x100),
		.wTotalLength = sys_cpu_to_le16(9),
		.bInCollection = 1,
		.baInterfaceNr = 1,
	},
	.std_midistreaming = {
		.bLength = 9,
		.bDescriptorType = USB_DESC_INTERFACE,
		.bInterfaceNumber = 1,
		.bAlternateSetting = 0,
		.bNumEndpoints = ARRAY_SIZE(ep_cfg),
		.bInterfaceClass = USB_BCC_AUDIO,
		.bInterfaceSubClass = USB_AUDIO_MIDISTREAMING,
		.bInterfaceProtocol = 0,
		.iInterface = 0
	},
	.cs_midistreaming = USB_MIDISTREAMING_CONFIG(
		DT_FOREACH_STATUS_OKAY(usb_midi_input, USB_MIDI_INPUT_SEP)
		DT_FOREACH_STATUS_OKAY(usb_midi_output, USB_MIDI_OUTPUT_SEP)
		USB_MIDI_BULK_ENDPOINT(
			USB_MIDI_TO_HOST_ENDPOINT_ID
			DT_FOREACH_STATUS_OKAY(usb_midi_input, SEP_EMBEDDED_JACK_ID)
		),
		USB_MIDI_BULK_ENDPOINT(
			USB_MIDI_FROM_HOST_ENDPOINT_ID
			DT_FOREACH_STATUS_OKAY(usb_midi_output, SEP_EMBEDDED_JACK_ID)
		),
	),
};

static inline int usb_midi_fixup_descriptor(uint8_t *desc, uint8_t **next_desc)
{
	uint8_t bLength = desc[0];
	uint8_t bDescriptorType = desc[1];
	uint8_t bDescriptorSubtype = desc[2];
	uint8_t bJackID;
	uint8_t *iJack = NULL;
	*next_desc = &desc[bLength];

	if (bDescriptorType == USB_DESC_ENDPOINT) {
		return 0;
	}

	if (bDescriptorType != USB_DESC_CS_INTERFACE){
		return 0;
	}

	if (bDescriptorSubtype == MIDI_IN_JACK){
		bJackID = (desc[4] & 0x3f) - 1;
		iJack = &desc[5];
	} else if (bDescriptorSubtype == MIDI_OUT_JACK){
		bJackID = (desc[4] & 0x3f) - 1;
		iJack = &desc[6 + 2*desc[5]];
	}

	#define MAP_JACK_NAME(inst) \
		case DT_NODE_CHILD_IDX(inst): \
			*iJack = usb_get_str_descriptor_idx(&inst##_str); \
			LOG_DBG("Fixing up MIDI jack #%d descriptor string = %d", \
				bJackID, *iJack);\
			break;

	if (iJack){
		switch (bJackID) {
			DT_FOREACH_STATUS_OKAY(usb_midi_input, MAP_JACK_NAME)
			DT_FOREACH_STATUS_OKAY(usb_midi_output, MAP_JACK_NAME)
		}
	}

	return 1;
}

static void usb_midi_configure_interface(struct usb_desc_header *head, uint8_t bInterfaceNumber)
{
	struct usb_if_descriptor *iface = (struct usb_if_descriptor *) head;
	LOG_DBG("Configuring interface: %d", (int)bInterfaceNumber);

	iface->bInterfaceNumber = bInterfaceNumber;
	midistreaming_cfg.cs_audiocontrol.baInterfaceNr = bInterfaceNumber + 1;

	uint8_t *desc = midistreaming_cfg.cs_midistreaming;
	while (usb_midi_fixup_descriptor(desc, &desc));

	LOG_DBG("USB MIDI Interface configured: %d", (int)bInterfaceNumber);
}

static enum usb_dc_status_code current_status = 0;

enum usb_dc_status_code usb_midi_status()
{
	return current_status;
}

static void usb_midi_status_callback(struct usb_cfg_data *cfg, enum usb_dc_status_code status,
				     const uint8_t *param)
{
	ARG_UNUSED(cfg);
	current_status = status;
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
		usb_midi_tx(USB_MIDI_TO_HOST_ENDPOINT_ID, 0);
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

static void usb_midi_tx(uint8_t ep, enum usb_dc_ep_cb_status_code ep_status)
{
	LOG_DBG("USB MIDI tx");

	int len;
	uint8_t *buf;
	uint32_t written;
	int r = k_sem_take(usb_midi_to_host_queue.data_ready_sem, K_FOREVER);
	if (r){
		return;
	}

	LOG_DBG("Waiting for read sem");

	r = k_sem_take(usb_midi_to_host_queue.read_sem, K_FOREVER);
	if (r){
		LOG_DBG("Stop waiting for read sem");
		k_sem_give(usb_midi_to_host_queue.read_sem);
		return;
	}

	while (! ring_buf_is_empty(usb_midi_to_host_queue.buf)){
		LOG_DBG("Transmit");
		len = ring_buf_get_claim(usb_midi_to_host_queue.buf, &buf, USB_MIDI_BULK_SIZE);
		if (len > 0){
			r = usb_write(USB_MIDI_TO_HOST_ENDPOINT_ID, buf, len, &written);
			if (r != 0){
				LOG_WRN("Error sending usb data");
			}
			ring_buf_get_finish(usb_midi_to_host_queue.buf, written);
			LOG_DBG("Transmitted %dB to host", written);
		}
	}
	k_sem_give(usb_midi_to_host_queue.read_sem);
}

#define USB_MIDI_JACK_CONFIGPTR_SEP(inst) &inst##_config,

static const struct usb_midi_jack *midistreaming_inputs[] = {
	DT_FOREACH_STATUS_OKAY(usb_midi_output, USB_MIDI_JACK_CONFIGPTR_SEP)
};

static void usb_midi_rx_dispatch(const uint8_t buf[4])
{
	uint8_t cn = buf[0] >> 4;
	uint8_t cin = buf[0] & 0xf;
	int len = CIN_DATASIZE[cin];

	if (len == -1){
		LOG_WRN("Unknown MIDI length; dropping packet");
		return;
	}

	if (cn >= ARRAY_SIZE(midistreaming_inputs)){
		LOG_WRN("Unknown cable number %d", cn);
		return;
	}

	struct usb_midi_jack *jack = midistreaming_inputs[cn];

	if (k_sem_take(jack->queue->write_sem, K_NO_WAIT)){
		LOG_WRN("Dropping packet for Jack #%d because it is busy", jack->jack_id);
		return;
	}
	ring_buf_put(jack->queue->buf, &buf[1], len);
	k_sem_give(jack->queue->data_ready_sem);
	k_sem_give(jack->queue->write_sem);
}

static void usb_midi_rx(uint8_t ep, enum usb_dc_ep_cb_status_code ep_status)
{
	uint32_t read = 0;
	uint8_t buf[USB_MIDI_BULK_SIZE];
	int r = usb_ep_read_wait(USB_MIDI_FROM_HOST_ENDPOINT_ID, buf, sizeof(buf), &read);
	if (r){
		LOG_WRN("USB read error %d", r);
		return;
	}

	__ASSERT(read % 4 == 0, "Unaligned USB read");

	LOG_DBG("READ %dB from host:", read);
	for (int i=0; i<read; i+=4){
		usb_midi_rx_dispatch(&buf[i]);
	}

	r = usb_ep_read_continue(USB_MIDI_FROM_HOST_ENDPOINT_ID);
	if (r){
		LOG_WRN("Unable to pursue reading from host: %d", r);
	}
}


static void usb_midi_tx_thread_main(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (1) {
		while (current_status != USB_DC_CONFIGURED){
			k_sleep(K_MSEC(10));
		}

		usb_midi_tx(USB_MIDI_TO_HOST_ENDPOINT_ID, 0);
	}
}

K_THREAD_DEFINE(usb_midi_tx_thread, 2048, usb_midi_tx_thread_main, NULL, NULL, NULL, 5, 0, 0);
