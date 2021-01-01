/**
 * See https://www.usb.org/document-library/usb-midi-devices-10
 */

#include <kernel.h>
#include <sys/byteorder.h>
#include <usb/usb_common.h>
#include <usb/usb_device.h>

#include "usb_midi.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(usb_midi);

/*             
MIDI sockets   USB-MIDI function                USB function   
------------   -----------------                ------------

               ---------------------------
               |                         |
               *   EMB_OUT_INT_MIDI_IN >-*--+ 0
               |                         |  +---> MIDI_IN_ENDPOINT
EXT_MIDI_IN  >-*-> EMB_OUT_EXT_MIDI_IN >-*--+ 1
               |                         |
EXT_MIDI_OUT <-*-< EMB_IN_EXT_MIDI_OUT <-*------< MIDI_OUT_ENDPOINT
               |                         |
               ---------------------------
 */


#define EXTERNAL_MIDI_IN_ID  1
#define EXTERNAL_MIDI_OUT_ID 2
#define EMBEDDED_OUT_INTERNAL_MIDI_IN_ID 3
#define EMBEDDED_OUT_EXTERNAL_MIDI_IN_ID 4
#define EMBEDDED_IN_EXTERNAL_MIDI_OUT_ID 5
#define MIDI_IN_ENDPOINT_ID  0x81
#define MIDI_OUT_ENDPOINT_ID 0x01

static struct usb_ep_cfg_data ep_cfg[] = {
    {.ep_cb=usb_transfer_ep_callback, .ep_addr=MIDI_IN_ENDPOINT_ID},
    {.ep_cb=usb_transfer_ep_callback, .ep_addr=MIDI_OUT_ENDPOINT_ID},
};

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
        /* USB-MIDI elements */
        /* Embedded MIDI from host */
        MIDI_JACKIN_DESCRIPTOR( JACK_EMBEDDED, EMBEDDED_IN_EXTERNAL_MIDI_OUT_ID, 0),
        /* External MIDI OUT socket */
        MIDI_JACKOUT_DESCRIPTOR(JACK_EXTERNAL,             EXTERNAL_MIDI_OUT_ID, 0, EMBEDDED_IN_EXTERNAL_MIDI_OUT_ID, 1),
        /* External MIDI IN socket */
        MIDI_JACKIN_DESCRIPTOR( JACK_EXTERNAL,              EXTERNAL_MIDI_IN_ID, 0),
        /* Embedded MIDI to host from external MIDI */
        MIDI_JACKOUT_DESCRIPTOR(JACK_EMBEDDED, EMBEDDED_OUT_EXTERNAL_MIDI_IN_ID, 0,              EXTERNAL_MIDI_IN_ID, 1),
        /* Embedded MIDI to host from internal MIDI (sensors) */
        MIDI_JACKOUT_DESCRIPTOR(JACK_EMBEDDED, EMBEDDED_OUT_INTERNAL_MIDI_IN_ID, 0),
        
        /* USB-MIDI endpoints */
        /* Bulk endpoint MIDI_IN with 2 embedded MIDI to host */
        MIDI_BULK_ENDPOINT( MIDI_IN_ENDPOINT_ID, EMBEDDED_OUT_INTERNAL_MIDI_IN_ID, EMBEDDED_OUT_EXTERNAL_MIDI_IN_ID),
        /* Bulk endpointMIDI_OUT with 1 embedded MIDI from host */
        MIDI_BULK_ENDPOINT(MIDI_OUT_ENDPOINT_ID, EMBEDDED_IN_EXTERNAL_MIDI_OUT_ID),
    )
};

static void midi_interface_configure(struct usb_desc_header *head, uint8_t bInterfaceNumber)
{
    ARG_UNUSED(head);
    midi_cfg.if0.bInterfaceNumber = bInterfaceNumber;
    LOG_INF("USB MIDI Interface config");
}


static enum usb_dc_status_code current_status = 0;

static void midi_status_callback(struct usb_cfg_data *cfg, enum usb_dc_status_code status, const uint8_t *param)
{
    ARG_UNUSED(cfg);
    current_status = status;
    switch (status){
    case USB_DC_ERROR:
        LOG_INF("USB error reported by the controller");
        break;
    case USB_DC_RESET:
        LOG_INF("USB reset");
        break;
    case USB_DC_CONNECTED:
        LOG_INF("USB connection established, hardware enumeration is completed");
        break;
    case USB_DC_CONFIGURED:
        LOG_INF("USB configuration done");
        break;
    case USB_DC_DISCONNECTED:
        LOG_INF("USB connection lost");
        break;
    case USB_DC_SUSPEND:
        LOG_INF("USB connection suspended by the HOST");
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

static int midi_vendor_handler(struct usb_setup_packet *setup, int32_t *len, uint8_t **data)
{
    LOG_INF("Class request: bRequest 0x%x bmRequestType 0x%x len %d",
        setup->bRequest, setup->bmRequestType, *len);
    return 0;
}


USBD_CFG_DATA_DEFINE(primary, midistreaming) struct usb_cfg_data midi_config = {
    .usb_device_description=NULL,
    .interface_config=midi_interface_configure,
    .interface_descriptor=&midi_cfg,
    .cb_usb_status=midi_status_callback,
    .interface={
        .class_handler=NULL,
        .custom_handler=NULL,
        .vendor_handler=midi_vendor_handler,
    },
    .num_endpoints=ARRAY_SIZE(ep_cfg),
    .endpoint=ep_cfg,
};


bool midi_is_configured()
{
    return current_status == USB_DC_CONFIGURED;
}


bool midi_to_host(uint8_t cableNumber, const uint8_t *event, size_t eventsize)
{
    if (! midi_is_configured() || eventsize < 2){
        LOG_WRN("Dropping MIDI pkt to host: not ready or packet invalid");
        return false;
    }
    uint8_t midi_cmd = event[0] >> 4;
    // We only support note-off or above for now
    if (midi_cmd < 0x8){
        LOG_WRN("Dropping MIDI pkt to host: command not supported");
        return false;
    }

    // Put into fixed 32b usb-midi packet
    uint8_t pkt[4] = {(cableNumber << 4) | midi_cmd};
    memcpy(&pkt[1], event, MIN(3, eventsize));
    usb_transfer_sync(MIDI_IN_ENDPOINT_ID, pkt, 4, USB_TRANS_WRITE);
    return true;
}

void midi_from_host()
{
    uint8_t pkt[4];
    if (midi_is_configured()){
        LOG_INF(">> ... (waiting)");
        int r = usb_transfer_sync(MIDI_OUT_ENDPOINT_ID, pkt, 4, USB_TRANS_READ);
        if (r > 0){
            LOG_INF(">> [%d] {%02hhX %02hhX %02hhX %02hhX}", r, pkt[0], pkt[1], pkt[2], pkt[3]);
        }
    }
}
