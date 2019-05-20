#include <stdlib.h>
#include <stdint.h>

#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/systick.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>

#include "compat.h"
#include "joystick.h"
#include "hwinit.h"
#include "usb_setup.h"

void dump_hex(const void *data, size_t size)
{
    char ascii[0x40];
    char *ptr = ascii;
    size_t i;
    size = max(size, 12);
    for (i = 0; i < size; ++i)
    {
        unsigned char b = ((unsigned char *)data)[i];
        *ptr++ = (HEX_CHAR((b >> 4) & 0xf));
        *ptr++ = (HEX_CHAR(b & 0xf));
        *ptr++ = ' ';
    }

    *ptr++ = '\0';
    usb_send_serial_data(ptr, ptr - ascii);
}

int main(void)
{
    hw_init();
    usb_setup();

    systick_set_clocksource(STK_CSR_CLKSOURCE_AHB_DIV8);
    /* SysTick interrupt every N clock pulses: set reload to N-1 */
    systick_set_reload(99999);
    systick_interrupt_enable();
    systick_counter_enable();

    usb_send_serial_data("Ready\n\0", 7);
    while (1)
        usb_poll();
}

static uint8_t usb_in_buf[0x40];
static uint8_t usb_out_buf[0x40];
static struct ControllerDataReport controllerDataReport;
static uint8_t tick = 0;

void hid_rx_cb(uint8_t *buf, uint16_t len)
{
    dump_hex(buf, len);
}

void input_report_0x30()
{
    static int x = 0;
    static int dir = 1;
    x += dir;
    if (x > 30)
        dir = -dir;
    if (x < -30)
        dir = -dir;

    uint8_t *ptr = (uint8_t *)&controllerDataReport;
    memset(&controllerDataReport, 0, sizeof(struct ControllerDataReport));

    controllerDataReport.controller_data.analog[4] = x;
    controllerDataReport.controller_data.analog[5] = x;

    controllerDataReport.controller_data.battery_level = 0x6;
    controllerDataReport.controller_data.connection_info = 0x1;
    controllerDataReport.controller_data.vibrator_input_report = 0x82;

    // report ID
    usb_out_buf[0x00] = kReportIdInput30;
    //memcpy(&usbbuf[1], &ptr[2], sizeof(struct ControllerDataReport) - 2);
    memcpy(&usb_out_buf[1], ptr, sizeof(struct ControllerDataReport));
    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
}

struct UsbInputReport81 usbInputReport81;

void input_sub_cmd_0x50()
{
    usb_send_serial_data("battery\n", strlen("battery\n"));
    usb_poll();


    uint8_t *ptr = (uint8_t *)&usbInputReport81;
    usb_out_buf[0x00] = kUsbReportIdInput81;
    memcpy(&usb_out_buf[1], ptr, sizeof(struct UsbInputReport81));

    usb_out_buf[0x01] = 0xD0;
    usb_out_buf[0x02] = 0x50;
    usb_out_buf[0x03] = 0x06;
    usb_out_buf[0x04] = 0x90;
    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
}

void input_sub_cmd_unk()
{
    uint8_t *ptr = (uint8_t *)&usbInputReport81;
    usb_out_buf[0x00] = kUsbReportIdInput81;
    memcpy(&usb_out_buf[1], ptr, sizeof(struct UsbInputReport81));

    usb_out_buf[0x01] = 0x80;
    usb_out_buf[0x02] = 0x03;
    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
}

void input_report_0x81_sub0x01()
{

    switch (usb_in_buf[0x01])
    {
    case 0x50: // battery
        input_sub_cmd_0x50();
        break;
    default:
        input_sub_cmd_unk();
        break;
    }
}

void sys_tick_handler(void)
{
    uint8_t cmd = kReportIdInput30;
    int len = usb_read_packet(ENDPOINT_HID_OUT, usb_in_buf, 0x40);
    if (len)
    {
       // dump_hex(usb_in_buf, 0x40);
    }

    switch (cmd)
    {
    case kReportIdOutput01:
        input_report_0x81_sub0x01();
        break;
    case kReportIdOutput10:
    case kUsbReportIdOutput80:
    case kReportIdInput30:
    default:
        input_report_0x30();
        break;
    }
}
