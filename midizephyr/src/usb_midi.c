/**
 * See https://www.usb.org/document-library/usb-midi-devices-10
 */

#include <kernel.h>
#include <usb/usb_common.h>
#include <usb/usb_device.h>

#include "usb_midi.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(usb_midi);

#define MIDI_OUT_EP_ADDR 0x01
#define MIDI_IN_EP_ADDR  0x81

#define EMBEDDED_MIDI_IN_ID  1
#define EXTERNAL_MIDI_IN_ID  2
#define INTERNAL_MIDI_IN_ID  3
#define EMBEDDED_MIDI_OUT_ID 4
#define EXTERNAL_MIDI_OUT_ID 5

static void midi_out_cb(uint8_t ep, enum usb_dc_ep_cb_status_code ep_status);
static void midi_in_cb(uint8_t ep, enum usb_dc_ep_cb_status_code ep_status);

static struct usb_ep_cfg_data ep_cfg[] = {
    {.ep_cb=usb_transfer_ep_callback, .ep_addr=MIDI_IN_EP_ADDR},
    {.ep_cb=usb_transfer_ep_callback, .ep_addr=MIDI_OUT_EP_ADDR},
};

static bool is_configured = false;

USBD_CLASS_DESCR_DEFINE(primary, midistreaming) struct usb_midi_if_descriptor midi_cfg = {
    .if0={
        .bLength=9,
        .bDescriptorType=USB_INTERFACE_DESC,
        .bInterfaceNumber=0,
        .bAlternateSetting=0,
        .bNumEndpoints=ARRAY_SIZE(ep_cfg),
        .bInterfaceClass=AUDIO_CLASS,
        .bInterfaceSubClass=USB_AUDIO_MIDISTREAMING,
        .bInterfaceProtocol=0,
        .iInterface=0
    },
    .cs_if0=MIDISTREAMING_CONFIG(
        MIDI_JACKIN_DESCRIPTOR(JACK_EMBEDDED, EMBEDDED_MIDI_IN_ID, 0),
        MIDI_JACKIN_DESCRIPTOR(JACK_EXTERNAL, EXTERNAL_MIDI_IN_ID, 0),
        MIDI_JACKIN_DESCRIPTOR(JACK_EXTERNAL, INTERNAL_MIDI_IN_ID, 0),
        MIDI_JACKOUT_DESCRIPTOR(JACK_EMBEDDED, EMBEDDED_MIDI_OUT_ID, 0, 2, EXTERNAL_MIDI_IN_ID, 1, INTERNAL_MIDI_IN_ID, 2),
        MIDI_JACKOUT_DESCRIPTOR(JACK_EXTERNAL, EXTERNAL_MIDI_OUT_ID, 0, 1, EMBEDDED_MIDI_IN_ID, 1),
        MIDI_BULK_ENDPOINT(MIDI_OUT_EP_ADDR, 1, EMBEDDED_MIDI_IN_ID),
        MIDI_BULK_ENDPOINT(MIDI_IN_EP_ADDR, 1, EMBEDDED_MIDI_OUT_ID),
    )
};


static void midi_interface_configure(struct usb_desc_header *head, uint8_t bInterfaceNumber)
{
    ARG_UNUSED(head);
    midi_cfg.if0.bInterfaceNumber = bInterfaceNumber;
    LOG_INF("USB MIDI Interface config");
}


static void midi_status_callback(struct usb_cfg_data *cfg, enum usb_dc_status_code status, const uint8_t *param)
{
    ARG_UNUSED(cfg);
    switch (status){
    case USB_DC_ERROR:
        LOG_INF("USB error reported by the controller");
        break;
    case USB_DC_RESET:
        LOG_INF("USB reset");
        is_configured = false;
        break;
    case USB_DC_CONNECTED:
        LOG_INF("USB connection established, hardware enumeration is completed");
        break;
    case USB_DC_CONFIGURED:
        LOG_INF("USB configuration done");
        is_configured = true;
        break;
    case USB_DC_DISCONNECTED:
        LOG_INF("USB connection lost");
        is_configured = false;
        break;
    case USB_DC_SUSPEND:
        LOG_INF("USB connection suspended by the HOST");
        is_configured = false;
        break;
    case USB_DC_RESUME:
        LOG_INF("USB connection resumed by the HOST");
        break;
    case USB_DC_INTERFACE:
        LOG_INF("USB interface selected");
        break;
    case USB_DC_SET_HALT:
        LOG_INF("Set Feature ENDPOINT_HALT received");
        break;
    case USB_DC_CLEAR_HALT:
        LOG_INF("Clear Feature ENDPOINT_HALT received");
        break;
    case USB_DC_SOF:
        LOG_INF("Start of Frame received");
        break;
    case USB_DC_UNKNOWN:
        LOG_INF("Initial USB connection status");
        break;
    }
}


USBD_CFG_DATA_DEFINE(primary, midistreaming) struct usb_cfg_data midi_config = {
    .usb_device_description=NULL,
    .interface_config=midi_interface_configure,
    .interface_descriptor=&midi_cfg,
    .cb_usb_status=midi_status_callback,
    .interface={
        .class_handler=NULL,
        .custom_handler=NULL,
        .vendor_handler=NULL,
    },
    .num_endpoints=ARRAY_SIZE(ep_cfg),
    .endpoint=ep_cfg,
};


bool midi_is_configured()
{
    return is_configured;
}


void midi_to_host(uint8_t cableNumber, const uint8_t *event, size_t eventsize)
{
    if (eventsize < 2){
        return;
    }
    uint8_t midi_cmd = event[0] >> 4;
    // We only support note-off or above for now
    if (midi_cmd < 0x8){
        return;
    }

    // Put into fixed 32b usb-midi packet
    uint8_t pkt[4] = {(cableNumber << 4) | midi_cmd};
    memcpy(&pkt[1], event, MIN(3, eventsize));
    usb_transfer(MIDI_IN_EP_ADDR, pkt, 4, USB_TRANS_WRITE, NULL, NULL);
    LOG_INF("<< {%02hhX %02hhX %02hhX %02hhX}", pkt[0], pkt[1], pkt[2], pkt[3]);
}
