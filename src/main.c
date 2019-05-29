#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifndef TEST
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/systick.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#endif

#include "compat.h"
#include "joystick.h"
#include "hwinit.h"
#include "usb_setup.h"
#include "usart.h"

static const uint8_t mac_addr[0x06] = {0x57, 0x30, 0xea, 0x8a, 0xbb, 0x7c};

static char ascii_buffer[0x100] = {};

void dump_hex(const void *data, size_t size)
{

    char *ptr = ascii_buffer;
    size_t i;
    size = min(size, 0x100);
    for (i = 0; i < size; ++i)
    {
        unsigned char b = ((unsigned char *)data)[i];
        //*ptr++ = (HEX_CHAR((b >> 4) & 0xf));
        //*ptr++ = (HEX_CHAR(b & 0xf));
        //*ptr++ = ' ';
        ptr += sprintf(ptr, "%02x ", b);
    }

    *ptr++ = '\r';
    *ptr++ = '\n';
    *ptr++ = 0;

    //usb_send_serial_data(ptr, ptr - ascii);
    //usb_poll();
    usart_send_str(ascii_buffer);
}

#ifndef TEST
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
    usart_init();

    //while(1)
    //    usart_send_str("Bam\n");
    usb_setup();
    systick_iterrupt_init();

    usart_send_str("========== start =========\r\n====================\r\n====================\r\n");

    while (1)
    {
        usb_poll();
    }
}
#endif

static uint8_t usb_out_buf[0x40];
static uint8_t tick = 0;

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
    tick++;

    controller_data->timestamp = tick;

    controller_data->analog[4] = x;
    controller_data->analog[5] = x;

    controller_data->button_r = x > 0;
    controller_data->button_l = x > 0;

    controller_data->button_zl = x > 0;
    controller_data->button_zr = x > 0;

    controller_data->button_left_sl = x > 0;
    controller_data->button_left_sr = x > 0;

    controller_data->button_right_sl = x > 0;
    controller_data->button_right_sr = x > 0;

    controller_data->battery_level = /*battery_level_charging | */ battery_level_full;
    controller_data->connection_info = 0x1;
    controller_data->vibrator_input_report = 0x70;
}

// Standard full mode - input reports with IMU data instead of subcommand replies. Pushes current state @60Hz, or @120Hz if Pro Controller.
void input_report_0x30(uint8_t *usb_in)
{
    // report ID
    usb_out_buf[0x00] = kReportIdInput30;

    fill_input_report((struct ControllerData *)&usb_out_buf[0x01]);
    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
}

// Subcommand 0x10: SPI flash read
void input_sub_cmd_0x10(uint8_t *usb_in)
{
    struct SpiReadReport resp = {};
    struct brcm_cmd_01 *spi_cmd = (struct brcm_cmd_01 *)&usb_in[kSubCmdOffset];

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

// 80 01
void output_mac_addr(uint8_t *usb_in)
{
    usart_send_str("output_mac_addr");
    // Verified
    // hard coded response !!!
    const uint8_t response_h[] = {
        kUsbReportIdInput81, 0x01, 0x00, kUsbDeviceTypeProController,
        mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]};
    memcpy(usb_out_buf, response_h, sizeof(response_h));
    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
}

// passthrough
void output_passthrough(uint8_t *usb_in)
{
    usart_send_str("passthrough");
#if 0
    usb_out_buf[0x00] = kUsbReportIdInput81;
    usb_out_buf[0x01] = usb_in_buf[1];

    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
#else
    const uint8_t response_h[] = {0x81, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    memcpy(usb_out_buf, response_h, sizeof(response_h));
    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
#endif
}

// passthrough
// Verified
void output_handshake(uint8_t *usb_in)
{
    usart_send_str("output_handshake");
#if 0
    usb_out_buf[0x00] = kUsbReportIdInput81;
    usb_out_buf[0x01] = usb_in_buf[1];

    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
#else
    const uint8_t response_h[] = {0x81, 0x02};
    memset(usb_out_buf, 0, 0x40);
    memcpy(usb_out_buf, response_h, sizeof(response_h));
    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
#endif
}

// baudrate
void output_baudrate(uint8_t *usb_in)
{
    usart_send_str("output_baudrate");
#if 0
    usb_out_buf[0x00] = kUsbReportIdInput81;
    usb_out_buf[0x01] = usb_in_buf[1];

    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
#else
    const uint8_t response_h[] = {0x81, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    memcpy(usb_out_buf, response_h, sizeof(response_h));
    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
#endif
}

// baudrate
void output_hid(uint8_t *usb_in)
{
    usart_send_str("output_hid");
#if 0
    usb_out_buf[0x00] = kUsbReportIdInput81;
    usb_out_buf[0x01] = usb_in_buf[1];

    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
#else
    const uint8_t response_h[] = {0x81, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    memcpy(usb_out_buf, response_h, sizeof(response_h));
    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
#endif
}

void output_report_0x80(uint8_t *buf)
{
    switch (buf[1])
    {
    case 0x01: // mac addr
        hw_led_toggle();
        output_mac_addr(buf);
        break;
        //handshake//baudrate
    case 0x02:
        output_handshake(buf);
        break;
    case 0x03:
        output_baudrate(buf);
        break;
    // usb timeout
    case 0x04:
        output_hid(buf);
        break;
    case 0x05:
        output_passthrough(buf);
        break;
        // custom ?
    case 0x91:
    case 0x92:
        output_passthrough(buf);
        break;
    default:
        output_passthrough(buf);
        break;
    }
}

void output_report_0x10(uint8_t *buf)
{
    /** nothing **/
}

static uint8_t joyStickMode = 0;

void output_report_0x01_unknown_subcmd(uint8_t *buf)
{
    // usart_send_str("output_report_0x01_unknown_subcmd");
    char dbg[0x20] = {};
    sprintf(dbg, "output_report_0x01_unknown_subcmd 0x%02x", buf[10]);
    usart_send_str(dbg);

    struct Report81Response *resp = (struct Report81Response *)&usb_out_buf[0x01];
    // report ID
    usb_out_buf[0x00] = kReportIdInput21;
    fill_input_report(&resp->controller_data);
    resp->subcommand_ack = 0x80;
    resp->subcommand = 0x00;
    resp->data[0] = 0x03;
    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
}

// Subcommand 0x08: Set shipment low power state
void output_report_0x01_0x08_lowpower_state(uint8_t *buf)
{
    usart_send_str("output_report_0x01_0x08_lowpower_state");
    unsigned char rawData[64] = {
        0x21,
        0x06,
        0x8E,
        0x84,
        0x00,
        0x12,
        0x01,
        0x18,
        0x80,
        0x01,
        0x18,
        0x80,
        0x80,
        0x80,
        0x08,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
    };
    static int iii = 0;
    rawData[0x01] = iii++;
    memcpy(usb_out_buf, rawData, 0x40);
    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
}

// Subcommand 0x02: Request device info
void output_report_0x01_get_device_info(uint8_t *buf)
{
    usart_send_str("output_report_0x01_get_device_info");
#if 0
    struct Report81Response *resp = (struct Report81Response *)&usb_out_buf[0x01];
    usb_out_buf[0x00] = kUsbReportIdInput81;

    resp->subcommand_ack = 0x82;
    resp->subcommand = 0x02;
    resp->cmd_0x02.firmware_version = 0x4803;
    resp->cmd_0x02.device_type = 0x03;
    resp->cmd_0x02.unk_0 = 0x02;

    // mac address
    // for (int i = 0; i < 6; i++)
    //    resp->cmd_0x02.mac[i] = i;

    const uint8_t response_h[] = {0x7c, 0xbb, 0x8a, 0xea, 0x30, 0x57 };
    memcpy(resp->cmd_0x02.mac, response_h, sizeof(response_h));

    resp->cmd_0x02.unk_1 = 0x01;
    resp->cmd_0x02.use_spi_colors = 0x01;

    
    fill_input_report(&resp->controller_data);

    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
#else
    unsigned char rawData[64] = {
        0x21, 0x05, 0x8E, 0x84, 0x00, 0x12, 0x01, 0x18, 0x80, 0x01, 0x18, 0x80, 0x80,
        0x82, 0x02, 0x03, 0x48, 0x01, 0x02, 0xA2, 0x55, 0x79, 0xAB, 0x78, 0xCC, 0x01, 0x01};
    static int iii = 0;
    rawData[0x01] = iii++;
    memcpy(usb_out_buf, rawData, 0x40);
    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
#endif
}

// Subcommand 0x03: Set input report mode
/* todo */
void output_report_0x01_set_report_mode(uint8_t *buf)
{
    usart_send_str("output_report_0x01_set_report_mode");
    struct Report81Response *resp = (struct Report81Response *)&usb_out_buf[0x01];
    // report ID
    usb_out_buf[0x00] = kReportIdInput21;

    // acknowledge
    resp->subcommand_ack = 0x80;
    resp->subcommand = 0x03;

    fill_input_report(&resp->controller_data);

    // joyStickMode = usb_in_buf[11];
    joyStickMode = 0x30;

    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
}




unsigned char spi0x6000[256] = {
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x03, 0xA0, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x90, 0x00, 0x7F, 0xFE,
	0xC9, 0x00, 0x00, 0x40, 0x00, 0x40, 0x00, 0x40, 0xFB, 0xFF, 0xD0, 0xFF,
	0xC0, 0xFF, 0x3B, 0x34, 0x3B, 0x34, 0x3B, 0x34, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xA1, 0xA5, 0x67, 0x8A, 0x88, 0x7B, 0xD5, 0xE5, 0x59, 0x52, 0x18,
	0x7D, 0x07, 0xF6, 0x5D, 0xB8, 0x85, 0x61, 0xFF, 0x32, 0x32, 0x32, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x50, 0xFD, 0x00, 0x00,
	0xC6, 0x0F, 0x0F, 0x30, 0x61, 0x96, 0x30, 0xF3, 0xD4, 0x14, 0x54, 0x41,
	0x15, 0x54, 0xC7, 0x79, 0x9C, 0x33, 0x36, 0x63, 0x0F, 0x30, 0x61, 0x96,
	0x30, 0xF3, 0xD4, 0x14, 0x54, 0x41, 0x15, 0x54, 0xC7, 0x79, 0x9C, 0x33,
	0x36, 0x63, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF
};

unsigned char spi0x2000[144] = {
	0x00, 0x22, 0x54, 0x5E, 0x5C, 0x52, 0x1E, 0x0A, 0x51, 0x99, 0x1A, 0xD3,
	0x27, 0x14, 0x6F, 0x7E, 0x4F, 0xD7, 0x5D, 0x14, 0x6B, 0xEB, 0x17, 0x5D,
	0x7C, 0xE7, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x68, 0x00, 0x95, 0x22, 0x70, 0x72, 0x9C, 0xB6, 0xD0, 0xF7, 0xF8, 0x10,
	0x86, 0x78, 0x8D, 0xA2, 0xDB, 0x6D, 0x0F, 0x6F, 0x5C, 0xEE, 0x3F, 0xFD,
	0x76, 0x05, 0x65, 0xA3, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x08, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};




/* todo */
void output_report_0x01_readspi(uint8_t *buf)
{
    usart_send_str("output_report_0x01_readspi");
#if 0
    struct ResponseX81 *resp = (struct ResponseX81 *)&usb_out_buf[0x01];
    // report ID
    usb_out_buf[0x00] = kReportIdInput21;
    fill_input_report(&resp->controller_data);
    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
#elif 1
    struct SpiReadReport *resp = (struct SpiReadReport *)&usb_out_buf[0x01];
    uint16_t addr = *(uint16_t *)(&buf[kSubCommandDataOffset]);
    uint8_t len = buf[kSubCommandDataOffset + 4];
    uint8_t * data = NULL;

    switch (addr & 0xF000)
    {
    case 0x6000:
        data = &spi0x6000[addr & 0xFF];
        usart_send_str("Factory Configuration and Calibration");
        break;
    case 0x2000:
        usart_send_str("Paring info");
        data = &spi0x2000[addr & 0x7F];
        break;

    default:
        usart_send_str("Unknown spi area");
        break;
    }

    char dbg[0x20] = {};
    sprintf(dbg, "0x%04x 0x%02x", addr, len);
    usart_send_str(dbg);

    memset(usb_out_buf, 0x00, 0x40);

    fill_input_report(&resp->controller_data);

    usb_out_buf[0x00] = kReportIdInput21;
    resp->subcommand_ack = 0x90;
    resp->subcommand = 0x10;

    resp->addr = addr;

    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
#elif 0
    struct Report81Response *resp = (struct Report81Response *)&usb_out_buf[0x01];
    uint16_t addr = *(uint16_t *)(&buf[kSubCommandDataOffset]);
    uint8_t len = buf[kSubCommandDataOffset + 4];

    char dbg[0x20] = {};
    sprintf(dbg, "0x%04x 0x%02x", addr, len);
    usart_send_str(dbg);

    usb_out_buf[0x00] = kReportIdInput21;

    memcpy(&usb_out_buf[0x00], &buf[kSubCommandDataOffset], 0x04);

    /*
    resp->subcommand_ack = 0x90;
    resp->subcommand = len;

    resp->cmd_0x10.address = addr;
    //resp->cmd_0x10.length = len;

    memset(resp->cmd_0x10.spi_data, 0xff, 0x1D);
    */

    fill_input_report(&resp->controller_data);

    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
#else
    unsigned char rawData[64] = {
        0x21,
        0x07,
        0x8E,
        0x84,
        0x00,
        0x12,
        0x01,
        0x18,
        0x80,
        0x01,
        0x18,
        0x80,
        0x80,
        0x90,
        0x10,
        0x00,
        0x60,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
    };
    static int iii = 0;
    rawData[0x01] = iii++;
    memcpy(usb_out_buf, rawData, 0x40);
    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
#endif
}

/* todo */
void output_report_0x01_writespi(uint8_t *buf)
{
    usart_send_str("output_report_0x01_writespi");
    struct ResponseX81 *resp = (struct ResponseX81 *)&usb_out_buf[0x01];
    // report ID
    usb_out_buf[0x00] = kReportIdInput21;
    fill_input_report(&resp->controller_data);
    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
}

/* todo */
void output_report_0x01_erasespi(uint8_t *buf)
{
    usart_send_str("output_report_0x01_erasespi");
    struct ResponseX81 *resp = (struct ResponseX81 *)&usb_out_buf[0x01];
    // report ID
    usb_out_buf[0x00] = kReportIdInput21;
    fill_input_report(&resp->controller_data);
    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
}

/* todo */
void output_report_0x01_set_lights(uint8_t *buf)
{
    usart_send_str("output_report_0x01_set_lights");
    struct ResponseX81 *resp = (struct ResponseX81 *)&usb_out_buf[0x01];
    // report ID
    usb_out_buf[0x00] = kReportIdInput21;
    fill_input_report(&resp->controller_data);
    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
}

/* todo */
void output_report_0x01_set_homelight(uint8_t *buf)
{
    usart_send_str("output_report_0x01_set_homelight");
    struct ResponseX81 *resp = (struct ResponseX81 *)&usb_out_buf[0x01];
    // report ID
    usb_out_buf[0x00] = kReportIdInput21;
    fill_input_report(&resp->controller_data);
    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
}

/* todo */
void output_report_0x01_set_immu(uint8_t *buf)
{
    usart_send_str("output_report_0x01_set_immu");
    struct ResponseX81 *resp = (struct ResponseX81 *)&usb_out_buf[0x01];
    // report ID
    usb_out_buf[0x00] = kReportIdInput21;
    fill_input_report(&resp->controller_data);
    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
}

/* todo */
void output_report_0x01_set_immu_sensitivity(uint8_t *buf)
{
    usart_send_str("output_report_0x01_set_immu_sensitivity");
    struct ResponseX81 *resp = (struct ResponseX81 *)&usb_out_buf[0x01];
    // report ID
    usb_out_buf[0x00] = kReportIdInput21;
    fill_input_report(&resp->controller_data);
    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
}

/* todo */
void output_report_0x01_set_vibration(uint8_t *buf)
{
    usart_send_str("output_report_0x01_set_vibration");
    struct ResponseX81 *resp = (struct ResponseX81 *)&usb_out_buf[0x01];
    // report ID
    usb_out_buf[0x00] = kReportIdInput21;
    fill_input_report(&resp->controller_data);
    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
}

/* todo */
void output_report_0x01_bt_pairing(uint8_t *buf)
{
    usart_send_str("output_report_0x01_bt_pairing");
    struct ResponseX81 *resp = (struct ResponseX81 *)&usb_out_buf[0x01];
    struct subcommand *data = (struct subcommand *)&buf[1];

    // report ID
    usb_out_buf[0x00] = kReportIdInput21;
    fill_input_report(&resp->controller_data);

#if 0
    // acknowledge
    resp->subcommand_ack = 0x80;
    resp->subcommand = 0x01;

    //memset(resp->data, 0xff, 0x10);
    const uint8_t p[] = {0x99, 0x51, 0x0a, 0x1e, 0x52, 0x5c};
    memcpy(resp->data, p, 6);

    resp->data[0] = 0x01;

    uint8_t pairing_type = buf[11] /* data->pairing.type */;

    char dbg[0x20] = {};
    sprintf(dbg, "pairing_type 0x%02x, 0x%02x", pairing_type, data->pairing.type);
    usart_send_str(dbg);


    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
#elif 1
    uint8_t pairing_type = buf[11] /* data->pairing.type */;
    char dbg[0x20] = {};
    sprintf(dbg, "pairing_type 0x%02x, 0x%02x", pairing_type, data->pairing.type);
    usart_send_str(dbg);

    unsigned char rawData[] = {
        0x21, 0x01, 0x8E, 0x84, 0x00, 0x12, 0x01, 0x18, 0x80, 0x01, 0x18, 0x80, 0x80, 0x82, 0x02,
        0x03, 0x48, 0x01, 0x02, 0xA2, 0x55, 0x79, 0xAB, 0x78, 0xCC, 0x01, 0x01};

    static int iii = 0;
    rawData[0x01] = iii++;

    rawData[13] = 0x81;
    rawData[14] = 0x01;
    unsigned char *ptr = &rawData[15];

    /* ex: pairing_type == 0x01
01 0b 00 00 00 00 00 00 00 00 01 01 99 51 0a 1e 52 5c 00 04 3c 4e 69 6e 74 65 6e 64 6f 20 53 77 69 74 63 68 00 00 00 00 00 68 00 60 d0 b1 e2 13 00
*/
    if (pairing_type == 0x01)
    {
        // Ok
        //*ptr =
        *ptr++ = 0x01;
        *ptr++ = 0x01;
        memcpy(ptr, mac_addr, 6);

        memcpy(usb_out_buf, rawData, sizeof(rawData));

        usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
    }
    /* ex: pairing_type == 0x02
01 0c 00 00 00 00 00 00 00 00 01 02 ba d8 e2 13 00 00 00 00 3b f8 cd 6a 00 00 00 04 ed ac e2 13 00 00 00 78 b9 d8 e2 13 00 00 00 50 ba d8 e2 13 00
*/

    else if (pairing_type == 0x02)
    {
        uint8_t ltkHash[] = {
            0x1A, 0xD3, 0x27, 0x14, 0x6F, 0x7E, 0x4F, 0xD7, 0x5D, 0x14, 0x6B, 0xEB,
            0x17, 0x5D, 0x7C, 0xE7};
        // xor it
        for (int i = 0; i < sizeof(ltkHash); i++)
        {
            ltkHash[i] = ltkHash[i] ^ 0xAA;
        }

        rawData[13] = 0x80;
        rawData[14] = 0x02;
        *ptr++ = 0x01;
        memcpy(ptr, ltkHash, sizeof(ltkHash));
        memcpy(usb_out_buf, rawData, sizeof(rawData));
        usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
    }
    else
    {
        usart_send_str("No packed sent");
    }

#else // crash switch !!

    uint8_t pairing_type = buf[11] /* data->pairing.type */;

    char dbg[0x20] = {};
    sprintf(dbg, "pairing_type 0x%02x, 0x%02x", pairing_type, data->pairing.type);
    usart_send_str(dbg);

    switch (pairing_type)
    {
    case 0x01:
        usart_send_str("request x01 ");
        resp->subcommand_ack = 0x80;
        resp->subcommand = 0x01;

        const uint8_t p[] = {0x99, 0x51, 0x0a, 0x1e, 0x52, 0x5c};

        memcpy(resp->data, p, 6);
        //memcpy(resp->data, data->pairing.host_bt_addr, 6);
        break;
    case 0x02:
        usart_send_str("request x02");
        resp->subcommand_ack = 0x80;
        resp->subcommand = 0x02;
        break;
    case 0x03:
        usart_send_str("request x03");
        resp->subcommand_ack = 0x80;
        resp->subcommand = 0x03;
        break;
    default:
        usart_send_str("request unknown");
        resp->subcommand_ack = 0x80;
        resp->subcommand = 0x03;
        break;
    }
    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
#endif
}

// Sub command !
void output_report_0x01(uint8_t *buf)
{
    uint8_t subCmd = buf[10];

    switch (subCmd)
    {
    case 0x01:
        output_report_0x01_bt_pairing(buf);
        break;
        // get device info
    case 0x02:
        output_report_0x01_get_device_info(buf);
        break;
        // Set input report mode
    case 0x03:
        output_report_0x01_set_report_mode(buf);
        break;
        // unknown ?
    case 0x08:
        output_report_0x01_0x08_lowpower_state(buf);
        break;
    // Read Spi
    case 0x10:
        output_report_0x01_readspi(buf);
        break;
    case 0x11:
        output_report_0x01_writespi(buf);
        break;
    case 0x12:
        output_report_0x01_erasespi(buf);
        break;
    // Set Lights
    case 0x30:
        output_report_0x01_set_lights(buf);
        break;
    // Set Home Light
    case 0x38:
        output_report_0x01_set_homelight(buf);
        break;
    // Set Immu
    case 0x40:
        output_report_0x01_set_immu(buf);
        break;
    // Set Immu Sensitivity
    case 0x41:
        output_report_0x01_set_immu_sensitivity(buf);
        break;
    // Set Vibration
    case 0x48:
        output_report_0x01_set_vibration(buf);
        break;

    case 0x00:
    case 0x33:
    default:
        output_report_0x01_unknown_subcmd(buf);
        break;
    }
}

void hid_rx_cb(uint8_t *buf, uint16_t len)
{
    usart_send_str("Recv: ");
    dump_hex(buf, len);

    uint8_t cmd = buf[0];
    switch (cmd)
    {
    case kReportIdOutput01:
        output_report_0x01(buf);
        break;
    case kReportIdOutput10:
        output_report_0x10(buf);
        break;

    case kUsbReportIdOutput80:
        output_report_0x80(buf);
        break;

    case kReportIdInput30:
    default:
        input_report_0x30(buf);
        break;
    }

    usart_send_str("Response: ");
    dump_hex(usb_out_buf, 0x40);
    usart_send_str(" ===== ");
}

void sys_tick_handler(void)
{
#if 0
    uint8_t cmd = joyStickMode;
    int len = usb_read_packet(ENDPOINT_HID_OUT, usb_in_buf, 0x40);
    if (len > 1)
    {
        cmd = usb_in_buf[0];
        
        usart_send_str("Packet received from switch: ");
        dump_hex(usb_in_buf, 0x40);
    }

#if 0
    // dbg !
    char dbg[0x40];
    sprintf(dbg, "joyStickMode: %02x", cmd);
    usart_send_str(dbg);
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

    if (cmd != joyStickMode) {
        
        usart_send_str("Response: ");
        dump_hex(usb_out_buf, 0x40);
    }
#else
    if (joyStickMode == 0x30)
    {
        input_report_0x30(NULL);
    }

#endif
}
