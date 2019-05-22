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

void systick_iterrupt_init()
{

    systick_set_clocksource(STK_CSR_CLKSOURCE_AHB_DIV8);
    /* SysTick interrupt every N clock pulses: set reload to N-1 */
    systick_set_reload(99999);
    systick_counter_enable();
}

int main(void)
{
    hw_init();
    usb_setup();

    systick_iterrupt_init();

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

void fill_input_report(struct ControllerDataReport *controllerDataReport)
{
    static int x = 0;
    static int dir = 1;
    x += dir;
    if (x > 30)
        dir = -dir;
    if (x < -30)
        dir = -dir;

    // increment tick by 3
    tick += 3;

    controllerDataReport->controller_data.timestamp = tick;

    controllerDataReport->controller_data.analog[4] = x;
    controllerDataReport->controller_data.analog[5] = x;

    controllerDataReport->controller_data.button_r = x > 0;
    controllerDataReport->controller_data.button_l = x > 0;

    controllerDataReport->controller_data.battery_level = battery_level_charging | battery_level_full;
    controllerDataReport->controller_data.connection_info = joycon_connexion_usb;
    controllerDataReport->controller_data.vibrator_input_report = 0x70;
}

// Standard full mode - input reports with IMU data instead of subcommand replies. Pushes current state @60Hz, or @120Hz if Pro Controller.
void input_report_0x30()
{
    // report ID
    usb_out_buf[0x00] = kReportIdInput30;

    fill_input_report((struct ControllerDataReport *)&usb_out_buf[0x01]);
    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
}

void input_report_0x3F()
{
    static int x = 0;
    static int dir = 1;
    x += dir;
    if (x > 30)
        dir = -dir;
    if (x < -30)
        dir = -dir;

    // report ID
    usb_out_buf[0x00] = 0x3F;

    // btn status
    if (x > 0)
    {
        usb_out_buf[0x01] = 0x10 | 0x20; // sr // sl
        usb_out_buf[0x02] = 0x00;
    }
    else
    {
        usb_out_buf[0x01] = 0x00;
        usb_out_buf[0x02] = 0x00;
    }

    // hat data
    usb_out_buf[0x03] = 0x08;

    // filler
    usb_out_buf[0x04] = 0x00;
    usb_out_buf[0x05] = 0x80;
    usb_out_buf[0x06] = 0x00;
    usb_out_buf[0x07] = 0x80;
    usb_out_buf[0x08] = 0x00;
    usb_out_buf[0x09] = 0x80;
    usb_out_buf[0x10] = 0x00;
    usb_out_buf[0x11] = 0x80;

    memset(usb_out_buf, 0xFF, 0x40);

    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
}

// Subcommand 0x50: Get regulated voltage
void input_sub_cmd_0x50()
{
    struct Report81Response resp = {};
    usb_out_buf[0x00] = kUsbReportIdInput81;

    fill_input_report(&resp.controller_data);

    resp.subcommand_ack = 0xD0;
    resp.subcommand = 0x50;
    resp.cmd_0x50.voltage = voltage_level_full;

    memcpy(&usb_out_buf[1], &resp, sizeof(struct Report81Response));
    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
}

// Subcommand 0x02: Request device info
void input_sub_cmd_0x02()
{
    struct Report81Response resp = {};
    usb_out_buf[0x00] = kUsbReportIdInput81;

    fill_input_report(&resp.controller_data);

    resp.subcommand_ack = 0x82;
    resp.subcommand = 0x02;
    resp.cmd_0x02.firmware_version = 0x4803;
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

    fill_input_report(&resp.controller_data);

    usb_out_buf[0x00] = 0x21;

    resp.subcommand_ack = 0x90;
    resp.subcommand = 0x10;
    resp.addr = offset;
    resp.length = len;

    memcpy(&usb_out_buf[1], &resp, sizeof(struct SpiReadReport));
    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
}

// Subcommand 0x03: Set input report mode
void input_sub_cmd_0x03()
{
    struct brcm_cmd_01 *joycmd = (struct brcm_cmd_01 *)&usb_in_buf[kSubCmdOffset];
    struct ResponseX81 resp = {};
    usb_out_buf[0x00] = kUsbReportIdInput81;

    resp.subcommand_ack = ACK;
    resp.subcommand = 0x03;

    joyStickMode = usb_in_buf[11];

    fill_input_report(&resp.controller_data);

    memcpy(&usb_out_buf[1], &resp, sizeof(struct ResponseX81));
    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);

#if 0
    // dbg !
    char dbg[0x40];
    sprintf(dbg, "joyStickMode: %02x\n", joyStickMode);
    usb_poll();
    usb_send_serial_data(dbg, strlen(dbg));
    usb_poll();
#endif
}

void input_sub_cmd_unk()
{
    struct brcm_cmd_01 *joycmd = (struct brcm_cmd_01 *)&usb_in_buf[kSubCmdOffset];
    struct ResponseX81 resp = {};
    usb_out_buf[0x00] = kUsbReportIdInput81;

    resp.subcommand_ack = ACK;
    resp.subcommand = usb_in_buf[kSubCmdOffset];
    fill_input_report(&resp.controller_data);

    memcpy(&usb_out_buf[1], &resp, sizeof(struct ResponseX81));
    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
}

void output_mac_addr()
{
    const uint8_t mac_addr[] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    
    struct MacAddressReport *report = &usb_out_buf[0x01];
    
    usb_out_buf[0x00] = kUsbReportIdInput81;


    report->subtype = 0x01;
    report->device_type = kUsbDeviceTypeChargingGripJoyConL;
    memcpy(report->mac_data , mac_addr, sizeof(mac_addr));

    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
}

void output_passthrought()
{
    usb_out_buf[0x00] = kUsbReportIdInput81;
    usb_out_buf[0x01] = usb_in_buf[1];

    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
}

void output_report_0x80()
{
    switch (usb_in_buf[1])
    {
    case 0x01: // mac addr
        hw_led_toggle();
        output_mac_addr();
        break;
        //handshake//baudrate
    case 0x02:
    case 0x03:
    // usb timeout
    case 0x04:
    case 0x05:
        output_passthrought();
        break;
        // custom ?
    case 0x91:
    case 0x92:
        output_passthrought();
        break;
    default:
        output_passthrought();
        break;
    }
}

void output_report_0x10()
{
    /** nothing **/
}

void output_report_0x01()
{
    /** nothing **/
}

void sys_tick_handler(void)
{
    uint8_t cmd = kReportIdInput30;
    int len = usb_read_packet(ENDPOINT_HID_OUT, usb_in_buf, 0x40);
    if (len > 1)
    {
        // dump_hex(usb_in_buf, 0x40);
        cmd = usb_in_buf[0];
        //usb_send_serial_data("battery\n", strlen("battery\n"));
        //usb_poll();
    }

#if 1
    // dbg !
    char dbg[0x40];
    sprintf(dbg, "joyStickMode: %02x\n", cmd);
    usb_poll();
    usb_send_serial_data(dbg, strlen(dbg));
    usb_poll();
#endif

    switch (cmd)
    {
    case kReportIdOutput01:
        output_report_0x01();
        break;
    case kReportIdOutput10:
        output_report_0x10();
        break;

    case kUsbReportIdOutput80:
        output_report_0x80();
        break;

    case kReportIdInput30:
    default:
        input_report_0x30();
        break;
    }
}

uint8_t joyStickMode = kReportIdInput30;