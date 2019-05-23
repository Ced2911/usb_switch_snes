#include <stdlib.h>
#include <stdint.h>
#include <string.h>

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

    //systick_iterrupt_init();

    while (1)
        usb_poll();
}

static uint8_t usb_in_buf[0x40];
static uint8_t usb_out_buf[0x40];
static uint8_t tick = 0;

void hid_rx_cb(uint8_t *buf, uint16_t len)
{
    dump_hex(buf, len);
}

void fill_input_report(struct ControllerData *controller_data)
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

    controller_data->timestamp = tick;

    controller_data->analog[4] = x;
    controller_data->analog[5] = x;

    controller_data->button_r = x > 0;
    controller_data->button_l = x > 0;

    controller_data->battery_level = battery_level_charging | battery_level_full;
    controller_data->connection_info = joycon_connexion_usb;
    controller_data->vibrator_input_report = 0x70;
}

// Standard full mode - input reports with IMU data instead of subcommand replies. Pushes current state @60Hz, or @120Hz if Pro Controller.
void input_report_0x30()
{
    // report ID
    usb_out_buf[0x00] = kReportIdInput30;

    fill_input_report((struct ControllerData *)&usb_out_buf[0x01]);
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

    usb_out_buf[0x00] = kReportIdInput21;

    resp.subcommand_ack = 0x90;
    resp.subcommand = 0x10;
    resp.addr = offset;
    resp.length = len;

    memcpy(&usb_out_buf[1], &resp, sizeof(struct SpiReadReport));
    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
}

void output_mac_addr()
{
    const uint8_t mac_addr[] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};

    struct MacAddressReport *report = &usb_out_buf[0x01];

    usb_out_buf[0x00] = kUsbReportIdInput81;

    report->subtype = 0x01;
    report->device_type = kUsbDeviceTypeChargingGripJoyConL;
    memcpy(report->mac_data, mac_addr, sizeof(mac_addr));

    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
}

// passthrough
void output_passthrough()
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
        output_passthrough();
        break;
        // custom ?
    case 0x91:
    case 0x92:
        output_passthrough();
        break;
    default:
        output_passthrough();
        break;
    }
}

void output_report_0x10()
{
    /** nothing **/
}

static uint8_t joyStickMode = kReportIdInput30;

void output_report_0x01_unknown_subcmd()
{
    struct Report81Response *resp = (struct Report81Response *)&usb_out_buf[0x01];
    // report ID
    usb_out_buf[0x00] = kReportIdInput21;
    fill_input_report(&resp->controller_data);
    resp->subcommand_ack = 0x80;
    resp->subcommand = 0x03;
    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
}

// Subcommand 0x02: Request device info
void output_report_0x01_get_device_info()
{
    struct Report81Response resp = {};
    usb_out_buf[0x00] = kUsbReportIdInput81;

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

/* todo */
void output_report_0x01_set_report_mode()
{
    struct ResponseX81 *resp = (struct ResponseX81 *)&usb_out_buf[0x01];
    // report ID
    usb_out_buf[0x00] = kReportIdInput21;
    fill_input_report(&resp->controller_data);
    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
}

/* todo */
void output_report_0x01_readspi()
{
    struct ResponseX81 *resp = (struct ResponseX81 *)&usb_out_buf[0x01];
    // report ID
    usb_out_buf[0x00] = kReportIdInput21;
    fill_input_report(&resp->controller_data);
    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
}

/* todo */
void output_report_0x01_writespi()
{
    struct ResponseX81 *resp = (struct ResponseX81 *)&usb_out_buf[0x01];
    // report ID
    usb_out_buf[0x00] = kReportIdInput21;
    fill_input_report(&resp->controller_data);
    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
}

/* todo */
void output_report_0x01_set_lights()
{
    struct ResponseX81 *resp = (struct ResponseX81 *)&usb_out_buf[0x01];
    // report ID
    usb_out_buf[0x00] = kReportIdInput21;
    fill_input_report(&resp->controller_data);
    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
}

/* todo */
void output_report_0x01_set_homelight()
{
    struct ResponseX81 *resp = (struct ResponseX81 *)&usb_out_buf[0x01];
    // report ID
    usb_out_buf[0x00] = kReportIdInput21;
    fill_input_report(&resp->controller_data);
    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
}

/* todo */
void output_report_0x01_set_immu()
{
    struct ResponseX81 *resp = (struct ResponseX81 *)&usb_out_buf[0x01];
    // report ID
    usb_out_buf[0x00] = kReportIdInput21;
    fill_input_report(&resp->controller_data);
    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
}

/* todo */
void output_report_0x01_set_immu_sensitivity()
{
    struct ResponseX81 *resp = (struct ResponseX81 *)&usb_out_buf[0x01];
    // report ID
    usb_out_buf[0x00] = kReportIdInput21;
    fill_input_report(&resp->controller_data);
    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
}

/* todo */
void output_report_0x01_set_vibration()
{
    struct ResponseX81 *resp = (struct ResponseX81 *)&usb_out_buf[0x01];
    // report ID
    usb_out_buf[0x00] = kReportIdInput21;
    fill_input_report(&resp->controller_data);
    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
}

// Sub command !
void output_report_0x01()
{
    uint8_t subCmd = usb_in_buf[10];

    switch (subCmd)
    {
        // get device info
    case 0x02:
        output_report_0x01_get_device_info();
        break;
        // Set input report mode
    case 0x03:
        output_report_0x01_set_report_mode();
        break;
    // Read Spi
    case 0x10:
        output_report_0x01_readspi();
        break;
    case 0x11:
        output_report_0x01_writespi();
        break;
    // Set Lights
    case 0x30:
        output_report_0x01_set_lights();
        break;
    // Set Home Light
    case 0x38:
        output_report_0x01_set_homelight();
        break;
    // Set Immu
    case 0x40:
        output_report_0x01_set_immu();
        break;
    // Set Immu Sensitivity
    case 0x41:
        output_report_0x01_set_immu_sensitivity();
        break;
    // Set Vibration
    case 0x48:
        output_report_0x01_set_vibration();
        break;

    case 0x00:
    case 0x33:
    default:
        output_report_0x01_unknown_subcmd();
        break;
    }
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
