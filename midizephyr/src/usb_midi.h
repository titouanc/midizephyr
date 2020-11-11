/**
 * See "Universal Serial Bus Device Class Definition for MIDI Devices", v1.0
 * https://www.usb.org/sites/default/files/midi10.pdf
 * 
 * See "Universal Serial Bus Device Class Definition for Audio Devices", v1.0
 * https://www.usb.org/sites/default/files/audio10.pdf
 */

#ifndef ZEPHYR_INCLUDE_USB_CLASS_USB_MIDI_H_
#define ZEPHYR_INCLUDE_USB_CLASS_USB_MIDI_H_

#include <zephyr.h>
#include <usb/class/usb_audio.h>

// Audio Class-Specific Descriptor Types (audio10, A.4)
#define CS_UNDEFINED     0x20
#define CS_DEVICE        0x21
#define CS_CONFIGURATION 0x22
#define CS_STRING        0x23
#define CS_INTERFACE     0x24
#define CS_ENDPOINT      0x25


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

// MIDI IN Jack Descriptor (midi10, 6.1.2.2)
#define MIDI_JACKIN_DESCRIPTOR(bJackType, bJackID, iJack) \
    6, \
    CS_INTERFACE, \
    MIDI_IN_JACK, \
    (bJackType), \
    (bJackID), \
    (iJack)

// MIDI OUT Jack Descriptor (midi10, 6.1.2.3)
#define MIDI_JACKOUT_DESCRIPTOR(bJackType, bJackID, iJack, bNrInputPins, ...) \
    (7 + 2*(bNrInputPins)), \
    CS_INTERFACE, \
    MIDI_OUT_JACK, \
    (bJackType), \
    (bJackID), \
    (bNrInputPins), \
    __VA_ARGS__, \
    (iJack)

// Standard + class-specific MS Bulk Data Endpoint Descriptor (midi10, 6.2.*)
#define MIDI_BULK_ENDPOINT(bEndpointAddress, bNumEmbMIDIJack, ...) \
    9, \
    USB_ENDPOINT_DESC, \
    (bEndpointAddress), \
    USB_DC_EP_BULK, \
    64, \
    0, \
    0, \
    0, \
    0, \
    4 + (bNumEmbMIDIJack), \
    CS_ENDPOINT, \
    MS_GENERAL, \
    (bNumEmbMIDIJack), \
    __VA_ARGS__


#define MIDISTREAMING_CONFIG(numEndpoints, ...) \
    { \
        .if0={ \
            .bLength=sizeof(struct usb_if_descriptor), \
            .bDescriptorType=USB_INTERFACE_DESC, \
            .bInterfaceNumber=0, \
            .bAlternateSetting=0, \
            .bNumEndpoints=(numEndpoints), \
            .bInterfaceClass=AUDIO_CLASS, \
            .bInterfaceSubClass=USB_AUDIO_MIDISTREAMING, \
            .bInterfaceProtocol=0, \
            .iInterface=0, \
        }, \
        .ms0={ \
            .bLength=sizeof(struct usb_midi_if_descriptor), \
            .bDescriptorType=CS_INTERFACE, \
            .bDescriptorSubtype=MS_HEADER, \
            .BcdADC=0x0100, \
            .wTotalLength=7 + sizeof((uint8_t[]) {__VA_ARGS__}), \
        }, \
        .elements={__VA_ARGS__}, \
    }

// Class-specific MS Interface Descriptor (midi10, 6.1.2.1)
struct usb_midi_if_descriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubtype;
    uint16_t BcdADC;
    uint16_t wTotalLength;
} __packed;


struct usb_midi_config {
    // MIDI Streaming standard && class specific interface
    struct usb_if_descriptor if0;
    struct usb_midi_if_descriptor ms0;

    // MIDI elements
    uint8_t elements[];
} __packed;

#endif
