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
    size = min(size, 10);
    for (i = 0; i < size; ++i)
    {
        unsigned char b = ((unsigned char *)data)[i];
        //*ptr++ = (HEX_CHAR((b >> 4) & 0xf));
        //*ptr++ = (HEX_CHAR(b & 0xf));
        //*ptr++ = ' ';
        ptr += sprintf(ptr, "%02x ", b);
    }

    *ptr++ = '\r';
    *ptr++ = '\0';
    usb_send_serial_data(ptr, ptr - ascii);
    usb_poll();
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

// Subcommand 0x50: Get regulated voltage
void input_sub_cmd_0x50()
{
    struct Report81Response resp = {};
    usb_out_buf[0x00] = kUsbReportIdInput81;

    resp.subcommand_ack = 0xD0;
    resp.subcommand = 0x50;
    resp.cmd_0x50.voltage = battery_level_full;

    memcpy(&usb_out_buf[1], &resp, sizeof(struct Report81Response));
    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
}

// Subcommand 0x02: Request device info
void input_sub_cmd_0x02()
{
    struct Report81Response resp = {};
    usb_out_buf[0x00] = kUsbReportIdInput81;

    resp.subcommand_ack = 0x82;
    resp.subcommand = 0x02;
    resp.cmd_0x02.firmware_version = 0x0348;
    resp.cmd_0x02.device_type = 0x03;
    resp.cmd_0x02.unk_0 = 0x02;

    // mac address
    for (int i = 0; i < 6; i++)
        resp.cmd_0x02.mac[i] = i;

    resp.cmd_0x02.unk_1 = 0x01;
    resp.cmd_0x02.use_spi_colors = 0x00;

    memcpy(&usb_out_buf[1], &resp, sizeof(struct Report81Response));
    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
}

// Subcommand 0x10: SPI flash read
void input_sub_cmd_0x10()
{
    struct SpiReadReport resp = {};
    struct brcm_cmd_01 *spi_cmd = (struct brcm_cmd_01 *)&usb_in_buf[kSubCmdOffset];

    uint8_t len = spi_cmd->spi_data.size & 0x1D;
    uint32_t offset = spi_cmd->spi_data.offset;

    usb_out_buf[0x00] = 0x21;

    resp.subcommand_ack = 0x90;
    resp.subcommand = 0x10;
    resp.addr = offset;
    resp.length = len;

    memcpy(&usb_out_buf[1], &resp, sizeof(struct SpiReadReport));
    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
}

void input_sub_cmd_unk()
{
    struct ResponseX81 resp = {};
    usb_out_buf[0x00] = kUsbReportIdInput81;

    resp.subcommand_ack = ACK;
    resp.subcommand = usb_in_buf[kSubCmdOffset];

    memcpy(&usb_out_buf[1], &resp, sizeof(struct ResponseX81));
    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
}

void input_report_0x81_sub0x01()
{

    switch (usb_in_buf[kSubCmdOffset])
    {
    case 0x50: // battery
        input_sub_cmd_0x50();
        break;
    case 0x02: // dev info
        hw_led_toggle();
        input_sub_cmd_0x02();
        break;
    // Spi read
    case 0x10:
        input_sub_cmd_0x10();
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
        cmd = usb_in_buf[0];
        //usb_send_serial_data("battery\n", strlen("battery\n"));
        //usb_poll();
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
