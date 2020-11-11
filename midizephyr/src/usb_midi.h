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

#define MIDI_BULK_SIZE 64

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


struct usb_midi_if_descriptor {
    struct usb_if_descriptor if0;
    uint8_t cs_if0[];
};

// Class-Specific MS Interface Header Descriptor (midi10, 6.1.2.1)
#define MIDI_CS_IF(...) \
    7, \
    CS_INTERFACE, \
    MS_HEADER, \
    0x01, 0x00, \
    (7 + sizeof((uint8_t[]) {__VA_ARGS__})) >> 8, \
    7 + sizeof((uint8_t[]) {__VA_ARGS__})


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


// Standard MS Bulk Data Endpoint Descriptor (midi10, 6.2.1)
#define MIDI_STD_BULK_ENDPOINT(bEndpointAddress) \
    9, \
    USB_ENDPOINT_DESC, \
    (bEndpointAddress), \
    USB_DC_EP_BULK, \
    MIDI_BULK_SIZE, 0, \
    0, \
    0, \
    0


// Class-Specific MS Bulk Data Endpoint Descriptor (midi10, 6.2.2)
#define MIDI_CS_BULK_ENDPOINT(bNumEmbMIDIJack, ...) \
    4 + (bNumEmbMIDIJack), \
    CS_ENDPOINT, \
    MS_GENERAL, \
    (bNumEmbMIDIJack), \
    __VA_ARGS__

// Standard + Class-Specific MS Bulk Data Endpoint Descriptor
#define MIDI_BULK_ENDPOINT(bEndpointAddress, bNumEmbMIDIJack, ...) \
    MIDI_STD_BULK_ENDPOINT(bEndpointAddress), \
    MIDI_CS_BULK_ENDPOINT(bNumEmbMIDIJack, __VA_ARGS__)


// MIDIStreaming interface definition (midi10, 6)
#define MIDISTREAMING_CONFIG(...) \
    { \
        MIDI_CS_IF(__VA_ARGS__), \
        __VA_ARGS__ \
    }

bool midi_is_configured();

void midi_to_host(uint8_t cableNumber, const uint8_t *event, size_t eventsize);

#endif
