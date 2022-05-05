#include "kinesta_midi.h"
#include "config.h"
#include "usb_midi.h"

#include <drivers/uart.h>

const struct device *midi_din_uart = DEVICE_DT_GET(DT_NODELABEL(midi_din_uart));

void kinesta_midi_out(const uint8_t midi_pkt[3])
{
    uint8_t midi_cmd = midi_pkt[0] >> 4;
    for (size_t i=0; i<midi_datasize(midi_cmd); i++){
        uart_poll_out(midi_din_uart, midi_pkt[i]);
    }

    usb_midi_write(USB_MIDI_SENSORS_JACK_ID, midi_pkt);
}
