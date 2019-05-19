#pragma once


// #define INCLUDE_DFU_INTERFACE
#define INCLUDE_CDC_INTERFACE

enum
{
    ENDPOINT_HID_IN = 0x81,
    ENDPOINT_HID_OUT = 0x01,
    ENDPOINT_CDC_COMM_IN = 0x83,
    ENDPOINT_CDC_DATA_IN = 0x82,
    ENDPOINT_CDC_DATA_OUT = 0x02
};

void usb_setup();
void usb_poll();
uint32_t usb_send_serial_data(void *buf, int len);
uint16_t usb_write_packet(uint8_t ep, void * buf, uint16_t len);

void hid_rx_cb(uint8_t * buf, uint16_t len);
