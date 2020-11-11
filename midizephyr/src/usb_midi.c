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
#define EMBEDDED_MIDI_OUT_ID 3
#define EXTERNAL_MIDI_OUT_ID 4

USBD_CLASS_DESCR_DEFINE(primary, midistreaming) struct usb_midi_config midi_cfg = MIDISTREAMING_CONFIG(2,
    MIDI_JACKIN_DESCRIPTOR(JACK_EMBEDDED, EMBEDDED_MIDI_IN_ID, 0),
    MIDI_JACKIN_DESCRIPTOR(JACK_EXTERNAL, EXTERNAL_MIDI_IN_ID, 0),
    MIDI_JACKOUT_DESCRIPTOR(JACK_EMBEDDED, EMBEDDED_MIDI_OUT_ID, 0, 1, EXTERNAL_MIDI_IN_ID, 1),
    MIDI_JACKOUT_DESCRIPTOR(JACK_EXTERNAL, EXTERNAL_MIDI_OUT_ID, 0, 1, EMBEDDED_MIDI_IN_ID, 1),
    MIDI_BULK_ENDPOINT(MIDI_OUT_EP_ADDR, 1, EMBEDDED_MIDI_IN_ID),
    MIDI_BULK_ENDPOINT(MIDI_IN_EP_ADDR, 1, EMBEDDED_MIDI_OUT_ID),
);

static void midi_interface_configure(struct usb_desc_header *head, uint8_t bInterfaceNumber)
{
    ARG_UNUSED(head);
    midi_cfg.if0.bInterfaceNumber = bInterfaceNumber;
    LOG_INF("USB MIDI Interface config (wTotalLength=%d)",
            (int) midi_cfg.ms0.wTotalLength);
}

static void midi_status_callback(struct usb_cfg_data *cfg, enum usb_dc_status_code status, const uint8_t *param)
{
    ARG_UNUSED(cfg);
    switch (status){
        case USB_DC_INTERFACE:
            LOG_INF("USB interface configured");
            break;
        case USB_DC_SET_HALT:
            LOG_INF("Set Feature ENDPOINT_HALT");
            break;
        case USB_DC_CLEAR_HALT:
            LOG_INF("Clear Feature ENDPOINT_HALT");
            break;
        default: break;
    }
}

static void midi_out_cb(uint8_t ep, enum usb_dc_ep_cb_status_code ep_status)
{
    // uint32_t bytes_to_read;

    // usb_read(ep, NULL, 0, &bytes_to_read);
    // LOG_DBG("ep 0x%x, bytes to read %d ", ep, bytes_to_read);
    // usb_read(ep, loopback_buf, bytes_to_read, NULL);
}

static void midi_in_cb(uint8_t ep, enum usb_dc_ep_cb_status_code ep_status)
{
    // if (usb_write(ep, loopback_buf, CONFIG_LOOPBACK_BULK_EP_MPS, NULL)) {
    //     LOG_DBG("ep 0x%x", ep);
    // }
}

static struct usb_ep_cfg_data ep_cfg[] = {
    {.ep_cb=midi_in_cb, .ep_addr=MIDI_IN_EP_ADDR},
    {.ep_cb=midi_out_cb, .ep_addr=MIDI_OUT_EP_ADDR},
};

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
