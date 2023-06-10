#ifndef ZEPHYR_INCLUDE_USB_CLASS_AUDIO_INTERNAL_H_
#define ZEPHYR_INCLUDE_USB_CLASS_AUDIO_INTERNAL_H_

#include <zephyr/sys/byteorder.h>
#include <zephyr/usb/class/usb_audio.h>

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
#define CIN_SYS_COMMON_1B  0xF

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
struct usb_ms_cs_if_descriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint8_t bDescriptorSubtype;
	uint16_t bcdADC;
	uint16_t wTotalLength;
} __packed;

// MIDI IN Jack Descriptor (midi10, 6.1.2.2)
struct usb_ms_in_jack_descriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint8_t bDescriptorSubtype;
	uint8_t bJackType;
	uint8_t bJackID;
	uint8_t iJack;
} __packed;

// MIDI OUT Jack Descriptor (midi10, 6.1.2.3)
// NB: only supports 1 input pin
struct usb_ms_out_jack_descriptor {
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

// Class-Specific MS Bulk Data Endpoint Descriptor (midi10, 6.2.2)
#define USB_MS_CS_EP_DESCRIPTOR(num_jacks) \
	struct { \
	    	uint8_t bLength; \
		uint8_t bDescriptorType; \
		uint8_t bDescriptorSubtype; \
		uint8_t bNumEmbMIDIJack; \
		uint8_t BAAssocJackID[num_jacks]; \
	} __packed

#endif
