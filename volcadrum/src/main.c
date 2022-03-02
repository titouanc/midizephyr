#include <zephyr.h>
#include <device.h>
#include <devicetree.h>
#include <usb/usb_device.h>
#include <drivers/uart.h>

#include "usb_midi.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(app);

void main(void)
{
    const struct uart_config midi_uart_config = {
        .baudrate=31250,
        .parity= UART_CFG_PARITY_NONE,
        .stop_bits=UART_CFG_STOP_BITS_1,
        .data_bits=UART_CFG_DATA_BITS_8,
        .flow_ctrl=UART_CFG_FLOW_CTRL_NONE,
    };
    const struct device *midi_uart = DEVICE_DT_GET(DT_NODELABEL(arduino_serial));

    if (uart_configure(midi_uart, &midi_uart_config)){
        LOG_ERR("Error configuring MIDI uart");
        return;
    }

    if (usb_enable(NULL) == 0){
        LOG_INF("USB enabled");
    } else {
        LOG_ERR("Failed to enable USB");
        return;
    }

    while (1){
        uint8_t cable_id;
        uint8_t midi_pkt[3] = {0, 0, 0};
        while (1){
            if (usb_midi_read(&cable_id, midi_pkt)){
                k_sleep(K_MSEC(1));
                continue;
            }

            printf("[%hhd] %02hhX %02hhX %02hhX\n", cable_id, midi_pkt[0], midi_pkt[1], midi_pkt[2]);

            uint8_t cmd = midi_pkt[0] >> 4;
            if (cmd == MIDI_CMD_NOTE_ON || cmd == MIDI_CMD_NOTE_OFF){
                // The volca drum has 1 instrument per channel, so we convert the
                // note to a channel number
                uint8_t note = midi_pkt[1] & 0x0f;
                midi_pkt[0] = (cmd << 4) | note;
            }

            for (size_t i=0; i<3; i++){
                uart_poll_out(midi_uart, midi_pkt[i]);
            }
        }
    }
}
