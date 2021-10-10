/**
 * See https://www.usb.org/document-library/usb-midi-devices-10
 */

#include <kernel.h>
#include <sys/byteorder.h>
#include <sys/ring_buffer.h>
#include <usb/usb_common.h>
#include <usb/usb_device.h>

#include "usb_midi.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(usb_midi);

/*             
MIDI sockets   USB-MIDI function                USB function (host)
------------   -----------------                -------------------

               ---------------------------
               |                         |
               *   EMB_OUT_INT_MIDI_IN >-*-0-+
               |                         |   +---> MIDI_IN_ENDPOINT
EXT_MIDI_IN  >-*-> EMB_OUT_EXT_MIDI_IN >-*-1-+
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

#define EXTERNAL_MIDI_IN_NAME "MIDI IN socket"
#define EXTERNAL_MIDI_OUT_NAME "MIDI OUT socket"
#define EMBEDDED_OUT_INTERNAL_MIDI_IN_NAME "Int. MIDI IN (sensors)"
#define EMBEDDED_OUT_EXTERNAL_MIDI_IN_NAME "Ext. MIDI IN"
#define EMBEDDED_IN_EXTERNAL_MIDI_OUT_NAME "Ext. MIDI OUT"

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
        /* === USB-MIDI elements === */
        /* Embedded MIDI from host */
        MIDI_JACKIN_DESCRIPTOR(JACK_EMBEDDED, EMBEDDED_IN_EXTERNAL_MIDI_OUT_ID, 0),
        /* External MIDI OUT socket */
        MIDI_JACKOUT_DESCRIPTOR(JACK_EXTERNAL, EXTERNAL_MIDI_OUT_ID, 0,
            EMBEDDED_IN_EXTERNAL_MIDI_OUT_ID, 1
        ),
        /* External MIDI IN socket */
        MIDI_JACKIN_DESCRIPTOR( JACK_EXTERNAL, EXTERNAL_MIDI_IN_ID, 0),
        /* Embedded MIDI to host from external MIDI */
        MIDI_JACKOUT_DESCRIPTOR(JACK_EMBEDDED, EMBEDDED_OUT_EXTERNAL_MIDI_IN_ID, 0,
            EXTERNAL_MIDI_IN_ID, 1
        ),
        /* Embedded MIDI to host from internal MIDI (sensors) */
        MIDI_JACKOUT_DESCRIPTOR(JACK_EMBEDDED, EMBEDDED_OUT_INTERNAL_MIDI_IN_ID, 0),
        
        /* === USB-MIDI endpoints === */
        /* Bulk endpoint MIDI_IN with 2 embedded MIDI to host */
        MIDI_BULK_ENDPOINT(MIDI_IN_ENDPOINT_ID,
            EMBEDDED_OUT_INTERNAL_MIDI_IN_ID,
            EMBEDDED_OUT_EXTERNAL_MIDI_IN_ID
        ),
        /* Bulk endpointMIDI_OUT with 1 embedded MIDI from host */
        MIDI_BULK_ENDPOINT(MIDI_OUT_ENDPOINT_ID,
            EMBEDDED_IN_EXTERNAL_MIDI_OUT_ID
        ),
    )
};

static void midi_interface_configure(struct usb_desc_header *head, uint8_t bInterfaceNumber)
{
    ARG_UNUSED(head);
    midi_cfg.if0.bInterfaceNumber = bInterfaceNumber;
    LOG_INF("USB MIDI Interface configured: %d", (int) bInterfaceNumber);
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


bool usb_midi_is_configured()
{
    return current_status == USB_DC_CONFIGURED;
}

/* === Output stream === */
RING_BUF_ITEM_DECLARE_POW2(usb_midi_to_host_buf, 5);

static void usb_midi_to_host_done(uint8_t ep, int size, void *data)
{
    ARG_UNUSED(ep);
    ARG_UNUSED(data);
    if (size > 0){
        ring_buf_get_finish(&usb_midi_to_host_buf, size);
    } else {
        ring_buf_get_finish(&usb_midi_to_host_buf, 0);
    }
}


bool usb_midi_to_host(uint8_t cable_number, const uint8_t midi_pkt[3])
{
    // Deny sending MIDI to host if USB not configured
    if (! usb_midi_is_configured()){
        return false;
    }

    // Determine the USB-MIDI prefix
    uint8_t cmd = midi_pkt[0] >> 4;
    uint8_t prefix = (cable_number << 4) | cmd;
    uint8_t *queued_data = NULL;

    // Allocate space in tx buffer
    uint32_t requested = 1 + midi_datasize(cmd);
    uint32_t allocated = ring_buf_put_claim(&usb_midi_to_host_buf, &queued_data, requested);
    if (allocated < requested){
        ring_buf_put_finish(&usb_midi_to_host_buf, 0);
        return false;
    }

    // Copy to tx buffer
    queued_data[0] = prefix;
    memcpy(&queued_data[1], midi_pkt, midi_datasize(cmd));

    // Commit transaction
    ring_buf_put_finish(&usb_midi_to_host_buf, allocated);

    // Then initiate USB transfer with all available data
    if (! usb_transfer_is_busy(MIDI_IN_ENDPOINT_ID)){
        uint32_t data_ready = ring_buf_get_claim(&usb_midi_to_host_buf, &queued_data, MIDI_BULK_SIZE);
        if (data_ready > 0){
            usb_transfer(MIDI_IN_ENDPOINT_ID, queued_data, data_ready, USB_TRANS_WRITE, usb_midi_to_host_done, NULL);
        } else {
            ring_buf_get_finish(&usb_midi_to_host_buf, 0);
        }
    }
    return true;
}


/* === Input stream === */
static K_SEM_DEFINE(usb_midi_from_host_sem, 0, 1);
RING_BUF_ITEM_DECLARE_POW2(usb_midi_from_host_buf, 5);

static void usb_midi_from_host_done(uint8_t ep, int size, void *data)
{
    ARG_UNUSED(ep);
    ARG_UNUSED(data);
    if (size > 0){
        ring_buf_put_finish(&usb_midi_from_host_buf, size);
    } else {
        ring_buf_put_finish(&usb_midi_from_host_buf, 0);
    }
    k_sem_give(&usb_midi_from_host_sem);
}

static bool usb_midi_from_host(uint8_t *cable_number, uint8_t midi_pkt[3])
{
    // Attempt to get 1 MIDI packet
    uint8_t *queued_data = NULL;
    uint32_t data_ready = ring_buf_get_claim(&usb_midi_from_host_buf, &queued_data, 4);
    if (data_ready < 2){
        ring_buf_get_finish(&usb_midi_from_host_buf, 0);
        return false;
    }

    // Get packet, and commit transaction
    uint8_t cmd = queued_data[0] & 0xff;
    *cable_number = queued_data[0] >> 4;
    memcpy(midi_pkt, &queued_data[1], midi_datasize(cmd));
    ring_buf_get_finish(&usb_midi_from_host_buf, 1 + midi_datasize(cmd));
    return true;
}

bool usb_midi_wait_from_host(uint8_t *cable_number, uint8_t midi_pkt[3])
{
    if (! usb_midi_is_configured()){
        return false;
    }

    while (! usb_midi_from_host(cable_number, midi_pkt)){
        uint8_t *queued_data = NULL;
        uint32_t allocated = ring_buf_put_claim(&usb_midi_from_host_buf, &queued_data, MIDI_BULK_SIZE);
        if (allocated == 0){
            ring_buf_put_finish(&usb_midi_from_host_buf, 0);
        } else {
            usb_transfer(MIDI_OUT_ENDPOINT_ID, queued_data, allocated, USB_TRANS_READ, usb_midi_from_host_done, NULL);
            k_sem_take(&usb_midi_from_host_sem, K_FOREVER);
        }
    }

    return true;
}
