#ifndef ZEPHYR_INCLUDE_USB_MIDI_H
#define ZEPHYR_INCLUDE_USB_MIDI_H

#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/net/buf.h>
#include <zephyr/usb/class/usb_audio.h>

#define MIDI_CMD_NOTE_OFF       0x80
#define MIDI_CMD_NOTE_ON        0x90
#define MIDI_CMD_POLY_KEYPRESS  0xA0
#define MIDI_CMD_CONTROL_CHANGE 0xB0
#define MIDI_CMD_PROGRAM_CHANGE 0xC0
#define MIDI_CMD_CHAN_PRESSURE  0xD0
#define MIDI_CMD_PITCH_BEND     0xE0
#define MIDI_CMD_SYSTEM_COMMON  0xF0

#define MIDI_SYSEX_START       0xF0
#define MIDI_SYSEX_STOP        0xF7
#define MIDI_SYS_TIMECODE      0xF1
#define MIDI_SYS_SONG_POSITION 0xF2
#define MIDI_SYS_SONG_SELECT   0xF3
#define MIDI_SYS_TUNE_REQUEST  0xF6
#define MIDI_SYS_CLOCK         0xF8
#define MIDI_SYS_START         0xFA
#define MIDI_SYS_CONTINUE      0xFB
#define MIDI_SYS_STOP          0xFC
#define MIDI_SYS_ACTIVE_SENSE  0xFE
#define MIDI_SYS_RESET         0xFF

#define MIDI_COMMAND_3B(cmd, chan, p1, p2) ((cmd) | (chan)), (p1), (p2)

#define MIDI_CONTROL_CHANGE(chan, cc, value) \
	MIDI_COMMAND_3B(MIDI_CMD_CONTROL_CHANGE, chan, cc, value)

typedef void (*usb_midi_rx_callback)(const struct device *dev, struct net_buf *data, size_t len);

/**
 * @brief      Get the number of MIDI data bytes that follow a given a leading
 *             MIDI Status byte
 * @param[in]  status_byte  The status byte
 * @return     * The number of following data bytes (>=0)
 *             * INT_MAX for SysEx (variable length ending with 0xF7)
 *             * -EINVAL if not a valid MIDI status byte
 */
static inline int midi_datasize(const uint8_t status_byte)
{
	switch (status_byte & 0xf0) {
	case MIDI_CMD_NOTE_OFF: return 2;
	case MIDI_CMD_NOTE_ON: return 2;
	case MIDI_CMD_POLY_KEYPRESS: return 2;
	case MIDI_CMD_CONTROL_CHANGE: return 2;
	case MIDI_CMD_PROGRAM_CHANGE: return 1;
	case MIDI_CMD_CHAN_PRESSURE: return 1;
	case MIDI_CMD_PITCH_BEND: return 2;
	case MIDI_CMD_SYSTEM_COMMON:
		switch (status_byte){
		case MIDI_SYSEX_START: return INT_MAX;
		case MIDI_SYSEX_STOP: return 0;
		case MIDI_SYS_TIMECODE: return 1;
		case MIDI_SYS_SONG_POSITION: return 2;
		case MIDI_SYS_SONG_SELECT: return 1;
		case MIDI_SYS_TUNE_REQUEST:
		case MIDI_SYS_CLOCK:
		case MIDI_SYS_START:
		case MIDI_SYS_CONTINUE:
		case MIDI_SYS_STOP:
		case MIDI_SYS_ACTIVE_SENSE:
		case MIDI_SYS_RESET:
			return 0;
		default: break;
		}
	default:
		return -EINVAL;
	}
}

int usb_midi_write(const struct device *dev, const uint8_t *data, size_t len);

int usb_midi_write_buf(const struct device *dev, struct net_buf *buf, size_t len);

int usb_midi_register(const struct device *dev, usb_midi_rx_callback cb);

#endif
