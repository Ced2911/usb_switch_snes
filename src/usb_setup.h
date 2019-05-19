#pragma once

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

