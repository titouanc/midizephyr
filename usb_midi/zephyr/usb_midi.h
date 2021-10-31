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

#define N_ELEMS(...) sizeof((uint8_t[]) {__VA_ARGS__})

// Class-Specific MS Interface Header Descriptor (midi10, 6.1.2.1)
#define MIDI_CS_IF(...) \
    7, \
    USB_DESC_CS_INTERFACE, \
    MS_HEADER, \
    0x01, 0x00, \
    (7 + N_ELEMS(__VA_ARGS__)) >> 8, \
    7 + N_ELEMS(__VA_ARGS__)


// MIDI IN Jack Descriptor (midi10, 6.1.2.2)
#define MIDI_JACKIN_DESCRIPTOR(bJackType, bJackID, iJack) \
    6, \
    USB_DESC_CS_INTERFACE, \
    MIDI_IN_JACK, \
    (bJackType), \
    (bJackID), \
    (iJack)


// MIDI OUT Jack Descriptor (midi10, 6.1.2.3)
#define MIDI_JACKOUT_DESCRIPTOR(bJackType, bJackID, iJack, ...) \
    (7 + N_ELEMS(__VA_ARGS__)), \
    USB_DESC_CS_INTERFACE, \
    MIDI_OUT_JACK, \
    (bJackType), \
    (bJackID), \
    N_ELEMS(__VA_ARGS__)/2, \
    ##__VA_ARGS__, \
    (iJack)


// Standard MS Bulk Data Endpoint Descriptor (midi10, 6.2.1)
#define MIDI_STD_BULK_ENDPOINT(bEndpointAddress) \
    9, \
    USB_DESC_ENDPOINT, \
    (bEndpointAddress), \
    USB_DC_EP_BULK, \
    MIDI_BULK_SIZE, 0, \
    0, \
    0, \
    0


// Class-Specific MS Bulk Data Endpoint Descriptor (midi10, 6.2.2)
#define MIDI_CS_BULK_ENDPOINT(...) \
    4 + N_ELEMS(__VA_ARGS__), \
    USB_DESC_CS_ENDPOINT, \
    MS_GENERAL, \
    N_ELEMS(__VA_ARGS__), \
    __VA_ARGS__

// Standard + Class-Specific MS Bulk Data Endpoint Descriptor
#define MIDI_BULK_ENDPOINT(bEndpointAddress, ...) \
    MIDI_STD_BULK_ENDPOINT(bEndpointAddress), \
    MIDI_CS_BULK_ENDPOINT(__VA_ARGS__)


// MIDIStreaming interface definition (midi10, 6)
#define MIDISTREAMING_CONFIG(...) \
    { \
        MIDI_CS_IF(__VA_ARGS__), \
        __VA_ARGS__ \
    }

#define MIDI_CMD_SYS_COMMON2    0x02
#define MIDI_CMD_SYS_COMMON3    0x03
#define MIDI_CMD_SYSEX_START    0x04
#define MIDI_CMD_SYS_COMMON1    0x05
#define MIDI_CMD_SYSEX_2B       0x06
#define MIDI_CMD_SYSEX_3B       0x07
#define MIDI_CMD_NOTE_OFF       0x08
#define MIDI_CMD_NOTE_ON        0x09
#define MIDI_CMD_POLY_KEYPRESS  0x0A
#define MIDI_CMD_CONTROL_CHANGE 0x0B
#define MIDI_CMD_PROGRAM_CHANGE 0x0C
#define MIDI_CMD_CHAN_PRESSURE  0x0D
#define MIDI_CMD_PITCH_BEND     0x0E
#define MIDI_CMD_SINGLE_BYTE    0x0F

#define MIDI_NOTE_ON(channel, note, velocity)  \
    {(MIDI_CMD_NOTE_ON  << 4) | (channel), note, velocity}
#define MIDI_NOTE_OFF(channel, note, velocity) \
    {(MIDI_CMD_NOTE_OFF << 4) | (channel), note, velocity}

static inline size_t midi_datasize(uint8_t evt_type)
{
    switch (evt_type){
        case MIDI_CMD_SYS_COMMON1:
        case MIDI_CMD_SINGLE_BYTE:
            return 1;
        case MIDI_CMD_SYS_COMMON2:
        case MIDI_CMD_SYSEX_2B:
        case MIDI_CMD_PROGRAM_CHANGE:
        case MIDI_CMD_CHAN_PRESSURE:
            return 2;
        default:
            return 3;
    }
}

bool usb_midi_is_configured();

int usb_midi_read(uint8_t *cable_number, uint8_t midi_pkt[3]);

int usb_midi_write(uint8_t cable_number, const uint8_t midi_pkt[3]);

#endif
