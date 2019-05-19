/*
 * This file is part of the libopencm3 project.
 *
 * Copyright (C) 2010 Gareth McMullin <gareth@blacksphere.co.nz>
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/systick.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/usb/usbd.h>
#include <libopencm3/usb/hid.h>

/* Define this to include the DFU APP interface. */
#define INCLUDE_DFU_INTERFACE

#ifdef INCLUDE_DFU_INTERFACE
#include <libopencm3/cm3/scb.h>
#include <libopencm3/usb/dfu.h>
#endif

#include "compat.h"
#include "joystick.h"

static usbd_device *usbd_dev;

const struct usb_device_descriptor dev_descr = {
    .bLength = USB_DT_DEVICE_SIZE,
    .bDescriptorType = USB_DT_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = 0,
    .bDeviceSubClass = 0,
    .bDeviceProtocol = 0,
    .bMaxPacketSize0 = 64,
    .idVendor = 0x0F0D,
    .idProduct = 0x0092,
    .bcdDevice = 0x0200,
    .iManufacturer = 1,
    .iProduct = 2,
    .iSerialNumber = 3,
    .bNumConfigurations = 1,
};
#if 0
static const uint8_t hid_report_descriptor[] = {
    HID_RI_USAGE_PAGE(8, 1), /* Generic Desktop */
    HID_RI_USAGE(8, 5),      /* Joystick */
    HID_RI_COLLECTION(8, 1), /* Application */
    // Buttons (2 bytes)
    HID_RI_LOGICAL_MINIMUM(8, 0),
    HID_RI_LOGICAL_MAXIMUM(8, 1),
    HID_RI_PHYSICAL_MINIMUM(8, 0),
    HID_RI_PHYSICAL_MAXIMUM(8, 1),
    // The Switch will allow us to expand the original HORI descriptors to a full 16 buttons.
    // The Switch will make use of 14 of those buttons.
    HID_RI_REPORT_SIZE(8, 1),
    HID_RI_REPORT_COUNT(8, 16),
    HID_RI_USAGE_PAGE(8, 9),
    HID_RI_USAGE_MINIMUM(8, 1),
    HID_RI_USAGE_MAXIMUM(8, 16),
    HID_RI_INPUT(8, 2),
    // HAT Switch (1 nibble)
    HID_RI_USAGE_PAGE(8, 1),
    HID_RI_LOGICAL_MAXIMUM(8, 7),
    HID_RI_PHYSICAL_MAXIMUM(16, 315),
    HID_RI_REPORT_SIZE(8, 4),
    HID_RI_REPORT_COUNT(8, 1),
    HID_RI_UNIT(8, 20),
    HID_RI_USAGE(8, 57),
    HID_RI_INPUT(8, 66),
    // There's an additional nibble here that's utilized as part of the Switch Pro Controller.
    // I believe this -might- be separate U/D/L/R bits on the Switch Pro Controller, as they're utilized as four button descriptors on the Switch Pro Controller.
    HID_RI_UNIT(8, 0),
    HID_RI_REPORT_COUNT(8, 1),
    HID_RI_INPUT(8, 1),
    // Joystick (4 bytes)
    HID_RI_LOGICAL_MAXIMUM(16, 255),
    HID_RI_PHYSICAL_MAXIMUM(16, 255),
    HID_RI_USAGE(8, 48),
    HID_RI_USAGE(8, 49),
    HID_RI_USAGE(8, 50),
    HID_RI_USAGE(8, 53),
    HID_RI_REPORT_SIZE(8, 8),
    HID_RI_REPORT_COUNT(8, 4),
    HID_RI_INPUT(8, 2),
    // ??? Vendor Specific (1 byte)
    // This byte requires additional investigation.
    HID_RI_USAGE_PAGE(16, 65280),
    HID_RI_USAGE(8, 32),
    HID_RI_REPORT_COUNT(8, 1),
    HID_RI_INPUT(8, 2),
    // Output (8 bytes)
    // Original observation of this suggests it to be a mirror of the inputs that we sent.
    // The Switch requires us to have these descriptors available.
    HID_RI_USAGE(16, 9761),
    HID_RI_REPORT_COUNT(8, 8),
    HID_RI_OUTPUT(8, 2),
    HID_RI_END_COLLECTION(0),
};
#else

static const uint8_t hid_report_descriptor[] = {
    0x05, 0x01,                   // Usage Page (Generic Desktop Ctrls)
    0x15, 0x00,                   // Logical Minimum (0)
    0x09, 0x04,                   // Usage (Joystick)
    0xA1, 0x01,                   // Collection (Application)
    0x85, 0x30,                   //   Report ID (48)
    0x05, 0x01,                   //   Usage Page (Generic Desktop Ctrls)
    0x05, 0x09,                   //   Usage Page (Button)
    0x19, 0x01,                   //   Usage Minimum (0x01)
    0x29, 0x0A,                   //   Usage Maximum (0x0A)
    0x15, 0x00,                   //   Logical Minimum (0)
    0x25, 0x01,                   //   Logical Maximum (1)
    0x75, 0x01,                   //   Report Size (1)
    0x95, 0x0A,                   //   Report Count (10)
    0x55, 0x00,                   //   Unit Exponent (0)
    0x65, 0x00,                   //   Unit (None)
    0x81, 0x02,                   //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x05, 0x09,                   //   Usage Page (Button)
    0x19, 0x0B,                   //   Usage Minimum (0x0B)
    0x29, 0x0E,                   //   Usage Maximum (0x0E)
    0x15, 0x00,                   //   Logical Minimum (0)
    0x25, 0x01,                   //   Logical Maximum (1)
    0x75, 0x01,                   //   Report Size (1)
    0x95, 0x04,                   //   Report Count (4)
    0x81, 0x02,                   //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x75, 0x01,                   //   Report Size (1)
    0x95, 0x02,                   //   Report Count (2)
    0x81, 0x03,                   //   Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x0B, 0x01, 0x00, 0x01, 0x00, //   Usage (0x010001)
    0xA1, 0x00,                   //   Collection (Physical)
    0x0B, 0x30, 0x00, 0x01, 0x00, //     Usage (0x010030)
    0x0B, 0x31, 0x00, 0x01, 0x00, //     Usage (0x010031)
    0x0B, 0x32, 0x00, 0x01, 0x00, //     Usage (0x010032)
    0x0B, 0x35, 0x00, 0x01, 0x00, //     Usage (0x010035)
    0x15, 0x00,                   //     Logical Minimum (0)
    0x27, 0xFF, 0xFF, 0x00, 0x00, //     Logical Maximum (65534)
    0x75, 0x10,                   //     Report Size (16)
    0x95, 0x04,                   //     Report Count (4)
    0x81, 0x02,                   //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,                         //   End Collection
    0x0B, 0x39, 0x00, 0x01, 0x00, //   Usage (0x010039)
    0x15, 0x00,                   //   Logical Minimum (0)
    0x25, 0x07,                   //   Logical Maximum (7)
    0x35, 0x00,                   //   Physical Minimum (0)
    0x46, 0x3B, 0x01,             //   Physical Maximum (315)
    0x65, 0x14,                   //   Unit (System: English Rotation, Length: Centimeter)
    0x75, 0x04,                   //   Report Size (4)
    0x95, 0x01,                   //   Report Count (1)
    0x81, 0x02,                   //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x05, 0x09,                   //   Usage Page (Button)
    0x19, 0x0F,                   //   Usage Minimum (0x0F)
    0x29, 0x12,                   //   Usage Maximum (0x12)
    0x15, 0x00,                   //   Logical Minimum (0)
    0x25, 0x01,                   //   Logical Maximum (1)
    0x75, 0x01,                   //   Report Size (1)
    0x95, 0x04,                   //   Report Count (4)
    0x81, 0x02,                   //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x75, 0x08,                   //   Report Size (8)
    0x95, 0x34,                   //   Report Count (52)
    0x81, 0x03,                   //   Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x06, 0x00, 0xFF,             //   Usage Page (Vendor Defined 0xFF00)
    0x85, 0x21,                   //   Report ID (33)
    0x09, 0x01,                   //   Usage (0x01)
    0x75, 0x08,                   //   Report Size (8)
    0x95, 0x3F,                   //   Report Count (63)
    0x81, 0x03,                   //   Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x85, 0x81,                   //   Report ID (-127)
    0x09, 0x02,                   //   Usage (0x02)
    0x75, 0x08,                   //   Report Size (8)
    0x95, 0x3F,                   //   Report Count (63)
    0x81, 0x03,                   //   Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x85, 0x01,                   //   Report ID (1)
    0x09, 0x03,                   //   Usage (0x03)
    0x75, 0x08,                   //   Report Size (8)
    0x95, 0x3F,                   //   Report Count (63)
    0x91, 0x83,                   //   Output (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Volatile)
    0x85, 0x10,                   //   Report ID (16)
    0x09, 0x04,                   //   Usage (0x04)
    0x75, 0x08,                   //   Report Size (8)
    0x95, 0x3F,                   //   Report Count (63)
    0x91, 0x83,                   //   Output (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Volatile)
    0x85, 0x80,                   //   Report ID (-128)
    0x09, 0x05,                   //   Usage (0x05)
    0x75, 0x08,                   //   Report Size (8)
    0x95, 0x3F,                   //   Report Count (63)
    0x91, 0x83,                   //   Output (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Volatile)
    0x85, 0x82,                   //   Report ID (-126)
    0x09, 0x06,                   //   Usage (0x06)
    0x75, 0x08,                   //   Report Size (8)
    0x95, 0x3F,                   //   Report Count (63)
    0x91, 0x83,                   //   Output (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Volatile)
    0xC0,                         // End Collection
};
#endif

static const struct
{
    struct usb_hid_descriptor hid_descriptor;
    struct
    {
        uint8_t bReportDescriptorType;
        uint16_t wDescriptorLength;
    } __attribute__((packed)) hid_report;
} __attribute__((packed)) hid_function = {
    .hid_descriptor = {
        .bLength = sizeof(hid_function),
        .bDescriptorType = USB_DT_HID,
        .bcdHID = 0x0100,
        .bCountryCode = 0,
        .bNumDescriptors = 1,
    },
    .hid_report = {
        .bReportDescriptorType = USB_DT_REPORT,
        .wDescriptorLength = sizeof(hid_report_descriptor),
    }};

const struct usb_endpoint_descriptor hid_endpoint[] = {
    {
        .bLength = USB_DT_ENDPOINT_SIZE,
        .bDescriptorType = USB_DT_ENDPOINT,
        .bEndpointAddress = 0x81,
        .bmAttributes = USB_ENDPOINT_ATTR_INTERRUPT,
        .wMaxPacketSize = 64,
        .bInterval = 0x20,
    },
    {
        .bLength = USB_DT_ENDPOINT_SIZE,
        .bDescriptorType = USB_DT_ENDPOINT,
        .bEndpointAddress = 0x01,
        .bmAttributes = USB_ENDPOINT_ATTR_INTERRUPT,
        .wMaxPacketSize = 64,
        .bInterval = 0x20,
    }};

const struct usb_interface_descriptor hid_iface = {
    .bLength = USB_DT_INTERFACE_SIZE,
    .bDescriptorType = USB_DT_INTERFACE,
    .bInterfaceNumber = 0,
    .bAlternateSetting = 0,
    .bNumEndpoints = 2,
    .bInterfaceClass = USB_CLASS_HID,
    .bInterfaceSubClass = 0, /* noboot */
    .bInterfaceProtocol = 0, /* Joystick */
    .iInterface = 0,

    .endpoint = hid_endpoint,

    .extra = &hid_function,
    .extralen = sizeof(hid_function),
};

#ifdef INCLUDE_DFU_INTERFACE
const struct usb_dfu_descriptor dfu_function = {
    .bLength = sizeof(struct usb_dfu_descriptor),
    .bDescriptorType = DFU_FUNCTIONAL,
    .bmAttributes = USB_DFU_CAN_DOWNLOAD | USB_DFU_WILL_DETACH,
    .wDetachTimeout = 255,
    .wTransferSize = 1024,
    .bcdDFUVersion = 0x011A,
};

const struct usb_interface_descriptor dfu_iface = {
    .bLength = USB_DT_INTERFACE_SIZE,
    .bDescriptorType = USB_DT_INTERFACE,
    .bInterfaceNumber = 1,
    .bAlternateSetting = 0,
    .bNumEndpoints = 0,
    .bInterfaceClass = 0xFE,
    .bInterfaceSubClass = 1,
    .bInterfaceProtocol = 1,
    .iInterface = 0,

    .extra = &dfu_function,
    .extralen = sizeof(dfu_function),
};
#endif

const struct usb_interface ifaces[] = {{
                                           .num_altsetting = 1,
                                           .altsetting = &hid_iface,
#ifdef INCLUDE_DFU_INTERFACE
                                       },
                                       {
                                           .num_altsetting = 1,
                                           .altsetting = &dfu_iface,
#endif
                                       }};

const struct usb_config_descriptor config = {
    .bLength = USB_DT_CONFIGURATION_SIZE,
    .bDescriptorType = USB_DT_CONFIGURATION,
    .wTotalLength = 0,
#ifdef INCLUDE_DFU_INTERFACE
    .bNumInterfaces = 2,
#else
    .bNumInterfaces = 1,
#endif
    .bConfigurationValue = 1,
    .iConfiguration = 0,
    .bmAttributes = 0xC0,
    .bMaxPower = 0x32,

    .interface = ifaces,
};

static const char *usb_strings[] = {
    "Black Sphere Technologies",
    "HID Demo",
    "DEMO",
};

/* Buffer to be used for control requests. */
uint8_t usbd_control_buffer[128];

static enum usbd_request_return_codes hid_control_request(usbd_device *dev, struct usb_setup_data *req, uint8_t **buf, uint16_t *len,
                                                          void (**complete)(usbd_device *, struct usb_setup_data *))
{
    (void)complete;
    (void)dev;

    if ((req->bmRequestType != 0x81) ||
        (req->bRequest != USB_REQ_GET_DESCRIPTOR) ||
        (req->wValue != 0x2200))
        return USBD_REQ_NOTSUPP;

    /* Handle the HID report descriptor. */
    *buf = (uint8_t *)hid_report_descriptor;
    *len = sizeof(hid_report_descriptor);

    return USBD_REQ_HANDLED;
}

#ifdef INCLUDE_DFU_INTERFACE
static void dfu_detach_complete(usbd_device *dev, struct usb_setup_data *req)
{
    (void)req;
    (void)dev;

    gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_2_MHZ,
                  GPIO_CNF_OUTPUT_PUSHPULL, GPIO10);
    gpio_set(GPIOA, GPIO10);
    scb_reset_core();
}

static enum usbd_request_return_codes dfu_control_request(usbd_device *dev, struct usb_setup_data *req, uint8_t **buf, uint16_t *len,
                                                          void (**complete)(usbd_device *, struct usb_setup_data *))
{
    (void)buf;
    (void)len;
    (void)dev;

    if ((req->bmRequestType != 0x21) || (req->bRequest != DFU_DETACH))
        return USBD_REQ_NOTSUPP; /* Only accept class request. */

    *complete = dfu_detach_complete;

    return USBD_REQ_HANDLED;
}
#endif

static void hid_set_config(usbd_device *dev, uint16_t wValue)
{
    (void)wValue;
    (void)dev;

    usbd_ep_setup(dev, 0x81, USB_ENDPOINT_ATTR_INTERRUPT, 4, NULL);

    usbd_register_control_callback(
        dev,
        USB_REQ_TYPE_STANDARD | USB_REQ_TYPE_INTERFACE,
        USB_REQ_TYPE_TYPE | USB_REQ_TYPE_RECIPIENT,
        hid_control_request);
#ifdef INCLUDE_DFU_INTERFACE
    usbd_register_control_callback(
        dev,
        USB_REQ_TYPE_CLASS | USB_REQ_TYPE_INTERFACE,
        USB_REQ_TYPE_TYPE | USB_REQ_TYPE_RECIPIENT,
        dfu_control_request);
#endif

    systick_set_clocksource(STK_CSR_CLKSOURCE_AHB_DIV8);
    /* SysTick interrupt every N clock pulses: set reload to N-1 */
    systick_set_reload(99999);
    systick_interrupt_enable();
    systick_counter_enable();
}

int main(void)
{
    rcc_clock_setup_in_hsi_out_48mhz();

    rcc_periph_clock_enable(RCC_GPIOA);
    /*
	 * This is a somewhat common cheap hack to trigger device re-enumeration
	 * on startup.  Assuming a fixed external pullup on D+, (For USB-FS)
	 * setting the pin to output, and driving it explicitly low effectively
	 * "removes" the pullup.  The subsequent USB init will "take over" the
	 * pin, and it will appear as a proper pullup to the host.
	 * The magic delay is somewhat arbitrary, no guarantees on USBIF
	 * compliance here, but "it works" in most places.
	 */
    gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_2_MHZ,
                  GPIO_CNF_OUTPUT_PUSHPULL, GPIO12);
    gpio_clear(GPIOA, GPIO12);
    for (unsigned i = 0; i < 800000; i++)
    {
        __asm__("nop");
    }

    usbd_dev = usbd_init(&st_usbfs_v1_usb_driver, &dev_descr, &config, usb_strings, 3, usbd_control_buffer, sizeof(usbd_control_buffer));
    usbd_register_set_config_callback(usbd_dev, hid_set_config);

    while (1)
        usbd_poll(usbd_dev);
}

void sys_tick_handler(void)
{
    static int x = 0;
    static int dir = 1;
    uint8_t buf[4] = {0x30, 0, 0, 0};

    buf[1] = dir;
    x += dir;
    if (x > 30)
        dir = -dir;
    if (x < -30)
        dir = -dir;

    usbd_ep_write_packet(usbd_dev, 0x81, buf, 4);
}