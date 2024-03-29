/**
 * See https://www.usb.org/document-library/usb-midi-devices-10
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/usb/usb_ch9.h>
#include <zephyr/usb/usb_device.h>

#include "usb_midi.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(usb_midi, CONFIG_USB_MIDI_LOG_LEVEL);

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

static void usb_midi_send_to_host();
static void usb_midi_receive_from_host();

K_SEM_DEFINE(data_to_host_ready, 0, 1);
K_SEM_DEFINE(data_from_host_ready, 0, 1);

K_WORK_DEFINE(usb_midi_to_host_work, usb_midi_send_to_host);
K_WORK_DEFINE(usb_midi_from_host_work, usb_midi_receive_from_host);

RING_BUF_ITEM_DECLARE_POW2(usb_midi_to_host_buf, 6);
RING_BUF_ITEM_DECLARE_POW2(usb_midi_from_host_buf, 6);

K_THREAD_STACK_DEFINE(usb_midi_work_queue_stack, 1024);

static struct k_work_q usb_midi_work_queue;

static bool usb_midi_work_queue_initialized = false;

static struct usb_ep_cfg_data ep_cfg[] = {
    {.ep_cb=usb_transfer_ep_callback, .ep_addr=MIDI_IN_ENDPOINT_ID},
    {.ep_cb=usb_transfer_ep_callback, .ep_addr=MIDI_OUT_ENDPOINT_ID},
};

USBD_CLASS_DESCR_DEFINE(primary, midistreaming) struct usb_midi_if_descriptor midi_cfg = {
    .if0={
        .bLength=9,
        .bDescriptorType=USB_DESC_INTERFACE,
        .bInterfaceNumber=0,
        .bAlternateSetting=0,
        .bNumEndpoints=ARRAY_SIZE(ep_cfg),
        .bInterfaceClass=USB_BCC_AUDIO,
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

static inline void usb_midi_submit_work(struct k_work *work)
{
    if (! usb_midi_work_queue_initialized){
        k_work_queue_init(&usb_midi_work_queue);
        k_work_queue_start(
            &usb_midi_work_queue,
            usb_midi_work_queue_stack,
            K_THREAD_STACK_SIZEOF(usb_midi_work_queue_stack),
            5,
            NULL
        );
        usb_midi_work_queue_initialized = true;
    }
    k_work_submit_to_queue(&usb_midi_work_queue, work);
}

static void midi_interface_configure(struct usb_desc_header *head, uint8_t bInterfaceNumber)
{
    ARG_UNUSED(head);
    midi_cfg.if0.bInterfaceNumber = bInterfaceNumber;
    LOG_DBG("USB MIDI Interface configured: %d", (int) bInterfaceNumber);
}

static enum usb_dc_status_code current_status = 0;

static void midi_status_callback(struct usb_cfg_data *cfg, enum usb_dc_status_code status, const uint8_t *param)
{
    ARG_UNUSED(cfg);
    current_status = status;
    switch (status){
    case USB_DC_ERROR:
        LOG_DBG("USB error reported by the controller");
        break;
    case USB_DC_RESET:
        LOG_DBG("USB reset");
        break;
    case USB_DC_CONNECTED:
        LOG_DBG("USB connection established, hardware enumeration is completed");
        break;
    case USB_DC_CONFIGURED:
        LOG_DBG("USB configuration done");
        break;
    case USB_DC_DISCONNECTED:
        LOG_DBG("USB connection lost");
        break;
    case USB_DC_SUSPEND:
        LOG_DBG("USB connection suspended by the HOST");
        break;
    case USB_DC_RESUME:
        LOG_DBG("USB connection resumed by the HOST");
        break;
    case USB_DC_INTERFACE:
        LOG_DBG("USB interface selected");
        break;
    case USB_DC_SET_HALT:
        LOG_DBG("Set Feature ENDPOINT_HALT received");
        break;
    case USB_DC_CLEAR_HALT:
        LOG_DBG("Clear Feature ENDPOINT_HALT received");
        break;
    case USB_DC_SOF:
        LOG_DBG("Start of Frame received");
        break;
    case USB_DC_UNKNOWN:
        LOG_DBG("Initial USB connection status");
        break;
    }
}

static int midi_vendor_handler(struct usb_setup_packet *setup, int32_t *len, uint8_t **data)
{
    LOG_DBG("Class request: bRequest 0x%x bmRequestType 0x%x len %d",
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

static void usb_midi_transfer_done(uint8_t ep, int size, void *data)
{
    ARG_UNUSED(data);

    if (ep == MIDI_IN_ENDPOINT_ID){
        if (size > 0){
            ring_buf_get_finish(&usb_midi_to_host_buf, size);
        } else {
            ring_buf_get_finish(&usb_midi_to_host_buf, 0);
        }
        usb_midi_submit_work(&usb_midi_to_host_work);
    }

    if (ep == MIDI_OUT_ENDPOINT_ID){
        if (size > 0){
            ring_buf_put_finish(&usb_midi_from_host_buf, size);
            k_sem_give(&data_from_host_ready);
        } else {
            ring_buf_put_finish(&usb_midi_from_host_buf, 0);
        }
        usb_midi_submit_work(&usb_midi_from_host_work);
    }
}

static void usb_midi_send_to_host()
{
    k_sem_take(&data_to_host_ready, K_FOREVER);

    uint8_t *queued_data;
    size_t data_ready = ring_buf_get_claim(&usb_midi_to_host_buf, &queued_data, MIDI_BULK_SIZE);
    if (data_ready > 0){
        usb_transfer(MIDI_IN_ENDPOINT_ID, queued_data, data_ready,
                     USB_TRANS_WRITE, usb_midi_transfer_done, NULL);
    } else {
        LOG_DBG("No pending data to host");
        ring_buf_get_finish(&usb_midi_to_host_buf, 0);
    }
}

static void usb_midi_receive_from_host()
{
    uint8_t *rxdata;
    size_t rxsize = ring_buf_put_claim(&usb_midi_from_host_buf, &rxdata, MIDI_BULK_SIZE);
    if (rxsize > 0){
        usb_transfer(MIDI_OUT_ENDPOINT_ID, rxdata, rxsize,
                     USB_TRANS_READ, usb_midi_transfer_done, NULL);
    } else {
        LOG_WRN("No space available for data from host");
        ring_buf_put_finish(&usb_midi_from_host_buf, 0);
    }
}

int usb_midi_write(uint8_t cable_number, const uint8_t midi_pkt[3])
{
    if (! usb_midi_is_configured()){
        return -EAGAIN;
    }

    int r = 0;
    uint8_t *buf;

    uint8_t midi_cmd = midi_pkt[0] >> 4;
    size_t requested_size = 1 + midi_datasize(midi_cmd);

    size_t claimed_size = ring_buf_put_claim(&usb_midi_to_host_buf, &buf, requested_size);
    if (claimed_size < requested_size){
        r = -EAGAIN;
        claimed_size = 0;
        LOG_WRN("No available space in write buffer");
    } else {
        buf[0] = (cable_number << 4) | midi_cmd;
        memcpy(&buf[1], midi_pkt, midi_datasize(midi_cmd));
    }
    
    ring_buf_put_finish(&usb_midi_to_host_buf, claimed_size);
    if (claimed_size) {
        usb_midi_submit_work(&usb_midi_to_host_work);
    }

    k_sem_give(&data_to_host_ready);
    return r;
}

int usb_midi_read(uint8_t *cable_number, uint8_t midi_pkt[3])
{
    int r;
    uint8_t *buf;

    usb_midi_submit_work(&usb_midi_from_host_work);
    k_sem_take(&data_from_host_ready, K_FOREVER);

    size_t requested_size = 4;
    size_t claimed_size = ring_buf_get_claim(&usb_midi_from_host_buf, &buf, requested_size);
    if (claimed_size < 2){
        // Not enough data available for a USB-MIDI packet
        r = -EAGAIN;
        claimed_size = 0;
        LOG_WRN("Not enough data in the read buffer");
    } else {
        uint8_t midi_cmd = buf[0] & 0x0f;
        if (claimed_size < 1 + midi_datasize(midi_cmd)){
            // Not enough data according to MIDI command
            r = -EAGAIN;
            claimed_size = 0;
            LOG_WRN("Not enough data in the read buffer");
        } else {
            *cable_number = buf[0] >> 4;
            memcpy(midi_pkt, &buf[1], midi_datasize(midi_cmd));
        }
    }

    ring_buf_get_finish(&usb_midi_from_host_buf, claimed_size);
    return r;
}