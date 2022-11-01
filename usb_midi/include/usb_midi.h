#ifndef ZEPHYR_INCLUDE_USB_MIDI_H
#define ZEPHYR_INCLUDE_USB_MIDI_H

#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/usb/class/usb_audio.h>

#define MIDI_CMD_NOTE_OFF       0x8
#define MIDI_CMD_NOTE_ON        0x9
#define MIDI_CMD_POLY_KEYPRESS  0xA
#define MIDI_CMD_CONTROL_CHANGE 0xB
#define MIDI_CMD_PROGRAM_CHANGE 0xC
#define MIDI_CMD_CHAN_PRESSURE  0xD
#define MIDI_CMD_PITCH_BEND     0xE
#define MIDI_CMD_SYSTEM_COMMON  0xF

#define MIDI_SYS_EXCL_START    0xF0
#define MIDI_SYS_TIMECODE      0xF1
#define MIDI_SYS_SONG_POSITION 0xF2
#define MIDI_SYS_SONG_SELECT   0xF3
#define MIDI_SYS_EXCL_STOP     0xF7
#define MIDI_SYS_CLOCK         0xF8
#define MIDI_SYS_START         0xFA
#define MIDI_SYS_CONTINUE      0xFB
#define MIDI_SYS_STOP          0xFC
#define MIDI_SYS_ACTIVE_SENSE  0xFE
#define MIDI_SYS_RESET         0xFF

#define MIDI_COMMAND_3B(cmd, chan, p1, p2) (((cmd) << 4) | (chan)), (p1), (p2)

#define MIDI_CONTROL_CHANGE(chan, cc, value) \
	MIDI_COMMAND_3B(MIDI_CMD_CONTROL_CHANGE, chan, cc, value)

typedef void (*usb_midi_rx_callback)(const struct device *dev, const uint8_t *data, size_t len);

static inline size_t midi_datasize(const uint8_t cmd)
{
	switch (cmd) {
	case MIDI_CMD_NOTE_OFF: return 2;
	case MIDI_CMD_NOTE_ON: return 2;
	case MIDI_CMD_POLY_KEYPRESS: return 2;
	case MIDI_CMD_CONTROL_CHANGE: return 2;
	case MIDI_CMD_PROGRAM_CHANGE: return 1;
	case MIDI_CMD_CHAN_PRESSURE: return 1;
	case MIDI_CMD_PITCH_BEND: return 2;
	case MIDI_CMD_SYSTEM_COMMON:
	default:
		return 0;

	}
}

int usb_midi_write(const struct device *dev, const uint8_t *data, size_t len);

int usb_midi_register(const struct device *dev, usb_midi_rx_callback cb);

#endif
