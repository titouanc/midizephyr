#ifndef ZEPHYR_INCLUDE_USB_CLASS_AUDIO_INTERNAL_H_
#define ZEPHYR_INCLUDE_USB_CLASS_AUDIO_INTERNAL_H_

#include <zephyr/sys/byteorder.h>
#include <zephyr/usb/class/usb_audio.h>

#define USB_MIDI_BULK_SIZE USB_MAX_FS_BULK_MPS
#define USB_MIDI_FROM_HOST_ENDPOINT_ID 0x01
#define USB_MIDI_TO_HOST_ENDPOINT_ID   0x81

// MidiStreaming Class-Specific Interface Descriptor Subtypes (midi10, A.1)
#define MS_DESCRIPTOR_UNDEFINED 0x00
#define MS_HEADER               0x01
#define MIDI_IN_JACK            0x02
#define MIDI_OUT_JACK           0x03
#define ELEMENT                 0x04

// MidiStreaming Class-Specific Endpoint Descriptor Subtypes (midi10, A.2)
#define DESCRIPTOR_UNDEFINED 0x00
#define MS_GENERAL           0x01


// MidiStreaming MIDI IN and OUT Jack types (midi10, A.3)
#define JACK_TYPE_UNDEFINED 0x00
#define JACK_EMBEDDED       0x01
#define JACK_EXTERNAL       0x02

// Code Index Number Classifications (midi10, table 4-1)
#define CIN_MISC           0x0
#define CIN_CABLE_EVENT    0x1
#define CIN_SYS_COMMON_2B  0x2
#define CIN_SYS_COMMON_3B  0x3
#define CIN_SYSEX_START    0x4
#define CIN_SYS_COMMON_1B  0x5
#define CIN_SYSEX_END_1B   0x5
#define CIN_SYSEX_END_2B   0x6
#define CIN_SYSEX_END_3B   0x7
#define CIN_NOTE_OFF       0x8
#define CIN_NOTE_ON        0x9
#define CIN_POLY_KEYPRESS  0xA
#define CIN_CONTROL_CHANGE 0xB
#define CIN_PROGRAM_CHANGE 0xC
#define CIN_CHAN_PRESSURE  0xD
#define CIN_PITCH_BEND     0xE
#define CIN_SYSTEM_COMMON  0xF

struct usb_ac_cs_descriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint8_t bDescriptorSubtype;
	uint16_t bcdADC;
	uint16_t wTotalLength;
	uint8_t bInCollection;
	uint8_t baInterfaceNr1;
} __packed;

// Class-Specific MS Interface Header Descriptor (midi10 , 6.1.2.1)
struct usb_midi_cs_if_descriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint8_t bDescriptorSubtype;
	uint16_t bcdADC;
	uint16_t wTotalLength;
} __packed;

// MIDI IN Jack Descriptor (midi10, 6.1.2.2)
struct usb_midi_in_jack_descriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint8_t bDescriptorSubtype;
	uint8_t bJackType;
	uint8_t bJackID;
	uint8_t iJack;
} __packed;

// MIDI OUT Jack Descriptor (midi10, 6.1.2.3)
// NB: only supports 1 input pin
struct usb_midi_out_jack_descriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint8_t bDescriptorSubtype;
	uint8_t bJackType;
	uint8_t bJackID;
	uint8_t bNrInputPins;
	uint8_t BASourceID1;
	uint8_t BASourcePin1;
	uint8_t iJack;
} __packed;

// Standard MS Bulk Data Endpoint Descriptor (midi10, 6.2.1)
struct usb_midi_std_ep_descriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint8_t bEndpointAddress;
	uint8_t bmAttributes;
	uint16_t wMaxPacketSize;
	uint8_t bInterval;
	uint8_t bRefresh;
	uint8_t bSynchAddress;
} __packed;

// Class-Specific MS Bulk Data Endpoint Descriptor (midi10, 6.2.2)
struct usb_midi_cs_ep_descriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint8_t bDescriptorSubtype;
	uint8_t bNumEmbMIDIJack;
	uint8_t BAAssocJackID1;
} __packed;

struct usb_midi_io_descriptor {
	struct usb_midi_in_jack_descriptor in_jack;
	struct usb_midi_out_jack_descriptor out_jack;
} __packed;

#define USB_MIDI_CS_EP_DESCRIPTOR_DECLARE(num_jacks) \
	struct { \
	    	uint8_t bLength; \
		uint8_t bDescriptorType; \
		uint8_t bDescriptorSubtype; \
		uint8_t bNumEmbMIDIJack; \
		uint8_t BAAssocJackID[num_jacks]; \
	} __packed

#define USB_MIDI_IF_DESCRIPTOR_DECLARE(num_inputs, num_outputs) \
	struct { \
	    struct usb_if_descriptor std_ac; \
	    struct usb_ac_cs_descriptor cs_ac; \
	    struct usb_if_descriptor std_ms; \
	    struct usb_midi_cs_if_descriptor cs_ms; \
	    struct usb_midi_io_descriptor inputs[num_inputs]; \
	    struct usb_midi_io_descriptor outputs[num_outputs]; \
	    struct usb_midi_std_ep_descriptor std_ep_inputs; \
	    USB_MIDI_CS_EP_DESCRIPTOR_DECLARE(num_inputs) cs_ep_inputs; \
	    struct usb_midi_std_ep_descriptor std_ep_outputs; \
	    USB_MIDI_CS_EP_DESCRIPTOR_DECLARE(num_outputs) cs_ep_outputs; \
	} __packed

#define USB_MIDI_IO_DESCRIPTOR(is_input, embedded_jack_id, external_jack_id) \
	{\
		.in_jack = { \
			.bLength=6, \
			.bDescriptorType=USB_DESC_CS_INTERFACE, \
			.bDescriptorSubtype=MIDI_IN_JACK, \
			.bJackType=COND_CODE_1(is_input, (JACK_EXTERNAL), (JACK_EMBEDDED)), \
			.bJackID=COND_CODE_1(is_input, (external_jack_id), (embedded_jack_id)), \
			.iJack=0, \
		},\
		.out_jack = { \
			.bLength=9, \
			.bDescriptorType=USB_DESC_CS_INTERFACE, \
			.bDescriptorSubtype=MIDI_OUT_JACK, \
			.bJackType=COND_CODE_0(is_input, (JACK_EXTERNAL), (JACK_EMBEDDED)), \
			.bJackID=COND_CODE_0(is_input, (external_jack_id), (embedded_jack_id)), \
			.bNrInputPins=1, \
			.BASourceID1=COND_CODE_1(is_input, (external_jack_id), (embedded_jack_id)), \
			.BASourcePin1=1, \
			.iJack=0, \
		},\
	}

#define USB_MIDI_CS_EP_DESCRIPTOR(...) \
	{ \
		.bLength = 4 + sizeof((uint8_t []) {__VA_ARGS__}), \
		.bDescriptorType = USB_DESC_CS_ENDPOINT, \
		.bDescriptorSubtype = MS_GENERAL, \
		.bNumEmbMIDIJack = sizeof((uint8_t []) {__VA_ARGS__}), \
		.BAAssocJackID = {__VA_ARGS__}, \
	}

#define USB_MIDI_IF_DESCRIPTOR_INIT(num_inputs, num_outputs) \
	.std_ac = { \
		.bLength = 9, \
		.bDescriptorType = USB_DESC_INTERFACE, \
		.bInterfaceNumber = 0, \
		.bAlternateSetting = 0, \
		.bNumEndpoints = 0, \
		.bInterfaceClass = USB_BCC_AUDIO, \
		.bInterfaceSubClass = USB_AUDIO_AUDIOCONTROL, \
		.bInterfaceProtocol = 0, \
		.iInterface = 0, \
	}, \
	.cs_ac = { \
		.bLength = 9, \
		.bDescriptorType = USB_DESC_CS_INTERFACE, \
		.bDescriptorSubtype = USB_AUDIO_HEADER, \
		.bcdADC = sys_cpu_to_le16(0x0100), \
		.wTotalLength = sys_cpu_to_le16(9), \
		.bInCollection = 1, \
		.baInterfaceNr1 = 1, \
	}, \
	.std_ms = { \
		.bLength = 9, \
		.bDescriptorType = USB_DESC_INTERFACE, \
		.bInterfaceNumber = 1, \
		.bAlternateSetting = 0, \
		.bNumEndpoints = 2, \
		.bInterfaceClass = USB_BCC_AUDIO, \
		.bInterfaceSubClass = USB_AUDIO_MIDISTREAMING, \
		.bInterfaceProtocol = 0, \
		.iInterface = 0 \
	}, \
	.cs_ms = { \
		.bLength = 7, \
		.bDescriptorType = USB_DESC_CS_INTERFACE, \
		.bDescriptorSubtype = MS_HEADER, \
		.bcdADC = sys_cpu_to_le16(0x0100), \
		.wTotalLength = ( \
			sizeof(USB_MIDI_IF_DESCRIPTOR_DECLARE(num_inputs, num_outputs)) \
			- 2*sizeof(struct usb_if_descriptor) \
			- sizeof(struct usb_ac_cs_descriptor) \
		), \
	}, \
	.std_ep_inputs = {\
		.bLength = 9, \
		.bDescriptorType = USB_DESC_ENDPOINT, \
		.bEndpointAddress = USB_MIDI_TO_HOST_ENDPOINT_ID, \
		.bmAttributes = USB_DC_EP_BULK, \
		.wMaxPacketSize = sys_cpu_to_le16(USB_MIDI_BULK_SIZE), \
	},\
	.std_ep_outputs = {\
		.bLength = 9, \
		.bDescriptorType = USB_DESC_ENDPOINT, \
		.bEndpointAddress = USB_MIDI_FROM_HOST_ENDPOINT_ID, \
		.bmAttributes = USB_DC_EP_BULK, \
		.wMaxPacketSize = sys_cpu_to_le16(USB_MIDI_BULK_SIZE), \
	}

#endif
