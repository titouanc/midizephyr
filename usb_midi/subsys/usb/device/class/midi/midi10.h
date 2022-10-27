#ifndef ZEPHYR_INCLUDE_USB_MIDI10_H
#define ZEPHYR_INCLUDE_USB_MIDI10_H

/*
 * See "Universal Serial Bus Device Class Definition for MIDI Devices", v1.0
 * https://www.usb.org/sites/default/files/midi10.pdf
 * 
 * See "Universal Serial Bus Device Class Definition for Audio Devices", v1.0
 * https://www.usb.org/sites/default/files/audio10.pdf
 */

#include <zephyr/kernel.h>
#include <zephyr/usb/class/usb_audio.h>

#define USB_MIDI_BULK_SIZE 64

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
#define CIN_SYS_START_CONT 0x4
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

struct usb_midi_if_descriptor {
    struct usb_if_descriptor std_audiocontrol;
    struct {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint8_t bDescriptorSubtype;
	uint16_t bcdADC;
	uint16_t wTotalLength;
	uint8_t bInCollection;
	uint8_t baInterfaceNr;
    } __packed cs_audiocontrol;
    struct usb_if_descriptor std_midistreaming;
    uint8_t cs_midistreaming[];
} __packed;

#define N_ELEMS(...) sizeof((uint8_t[]) {__VA_ARGS__})

#define USB_MIDI_TO_HOST_ENDPOINT_ID   0x81
#define USB_MIDI_FROM_HOST_ENDPOINT_ID 0x01

// Class-Specific MS Interface Header Descriptor (midi10, 6.1.2.1)
#define USB_MIDI_CS_IF(...) \
    7, \
    USB_DESC_CS_INTERFACE, \
    MS_HEADER, \
    0x00, 0x01, \
    7 + N_ELEMS(__VA_ARGS__), \
    (7 + N_ELEMS(__VA_ARGS__)) >> 8


// MIDI IN Jack Descriptor (midi10, 6.1.2.2)
#define USB_MIDI_JACKIN_DESCRIPTOR_BLENGTH 6
#define USB_MIDI_JACKIN_DESCRIPTOR(bJackType, bJackID, iJack) \
    6, \
    USB_DESC_CS_INTERFACE, \
    MIDI_IN_JACK, \
    (bJackType), \
    (bJackID), \
    (iJack)


// MIDI OUT Jack Descriptor (midi10, 6.1.2.3)
#define USB_MIDI_JACKOUT_DESCRIPTOR_BLENGTH(n_inputs) (7 + n_inputs)
#define USB_MIDI_JACKOUT_DESCRIPTOR(bJackType, bJackID, iJack, ...) \
    __USB_MIDI_JACKOUT_DESCRIPTOR(bJackType, bJackID, iJack, ##__VA_ARGS__)
#define __USB_MIDI_JACKOUT_DESCRIPTOR(bJackType, bJackID, iJack, ...) \
    USB_MIDI_JACKOUT_DESCRIPTOR_BLENGTH(N_ELEMS(__VA_ARGS__)), \
    USB_DESC_CS_INTERFACE, \
    MIDI_OUT_JACK, \
    (bJackType), \
    (bJackID), \
    N_ELEMS(__VA_ARGS__)/2, \
    ##__VA_ARGS__, \
    (iJack)

// Standard MS Bulk Data Endpoint Descriptor (midi10, 6.2.1)
#define USB_MIDI_STD_BULK_ENDPOINT(bEndpointAddress) \
    9, \
    USB_DESC_ENDPOINT, \
    (bEndpointAddress), \
    USB_DC_EP_BULK, \
    USB_MIDI_BULK_SIZE, \
    0, \
    0, \
    0, \
    0


// Class-Specific MS Bulk Data Endpoint Descriptor (midi10, 6.2.2)
#define USB_MIDI_CS_BULK_ENDPOINT(...) \
    4 + N_ELEMS(__VA_ARGS__), \
    USB_DESC_CS_ENDPOINT, \
    MS_GENERAL, \
    N_ELEMS(__VA_ARGS__), \
    __VA_ARGS__

// Standard + Class-Specific MS Bulk Data Endpoint Descriptor
#define USB_MIDI_BULK_ENDPOINT(bEndpointAddress, ...) \
    __USB_MIDI_BULK_ENDPOINT(bEndpointAddress, ##__VA_ARGS__)
#define __USB_MIDI_BULK_ENDPOINT(bEndpointAddress, ...) \
    USB_MIDI_STD_BULK_ENDPOINT(bEndpointAddress), \
    USB_MIDI_CS_BULK_ENDPOINT(__VA_ARGS__)



// MIDIStreaming interface definition (midi10, 6)
#define USB_MIDISTREAMING_CONFIG(...) \
    { \
        USB_MIDI_CS_IF(__VA_ARGS__), \
        __VA_ARGS__ \
    }

#endif
