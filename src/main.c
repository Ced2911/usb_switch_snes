#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifndef TEST
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/systick.h>
#include <libopencm3/cm3/sync.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#endif

#include "compat.h"
#include "joystick.h"
#include "hwinit.h"
#include "usb_setup.h"
#include "usart.h"
#include "spi_func.h"

static const uint8_t mac_addr[0x06] = {0x57, 0x30, 0xea, 0x8a, 0xbb, 0x7c};

static char ascii_buffer[0x100] = {};

// #define usart_send_str(X)

void dump_hex(const void *data, size_t size)
{
#if 1
    char *ptr = ascii_buffer;
    size_t i;
    size = min(size, 0x100);
    for (i = 0; i < size; ++i)
    {
        unsigned char b = ((unsigned char *)data)[i];
        ptr += sprintf(ptr, "%02x ", b);
    }

    //usb_send_serial_data(ptr, ptr - ascii);
    //usb_poll();
    usart_send_str(ascii_buffer);
#else
    uint32_t *u32_data = (uint32_t *)data;
    char *ptr = ascii_buffer;
    size_t i;
    size = min(size, 0x100) / sizeof(uint32_t);
    for (i = 0; i < size; ++i)
    {
        ptr += sprintf(ptr, "0x%x,", u32_data[i]);
    }

    *ptr++ = '\r';
    *ptr++ = '\n';
    *ptr++ = 0;

    //usb_send_serial_data(ptr, ptr - ascii);
    //usb_poll();
    usart_send_str(ascii_buffer);
#endif
}
//#define dump_hex(...)

#ifndef TEST
static mutex_t m;
void mlock()
{
    mutex_lock(&m);
}

void munlock()
{
    mutex_unlock(&m);
}

void handle_packet();

void handle_input_0x30();

void systick_iterrupt_init()
{

    //systick_set_clocksource(STK_CSR_CLKSOURCE_AHB_DIV8);
    /* SysTick interrupt every N clock pulses: set reload to N-1 */
    //systick_set_reload(((9000000 / 9000) * 15) - 1); // 15 ms ?
    systick_set_frequency(60, 72000000);
    systick_counter_enable();
}

int main(void)
{
    hw_init();
    hw_led_off();
    usart_init();

    usb_setup();
    systick_iterrupt_init();

    usart_send_str("========== start =========\r\n====================\r\n====================\r\n");
    uart_flush();

    hw_led_on();

    while (1)
    {
        handle_input_0x30();
        usb_poll();
        uart_flush();
        // handle_packet();
    }
}
#endif

//#define usart_send_str(...)

//static uint8_t usb_out_buf[0x40];
static uint8_t tick = 0;

void fill_input_report(struct ControllerData *controller_data)
{
#if 1
    static int x = 0;
    static int dir = 1;
    x += dir;
    if (x > 30)
        dir = -dir;
    if (x < -30)
        dir = -dir;

    // increment tick by 3
    tick += 3;
    //tick++;

    controller_data->timestamp = tick;

    controller_data->analog[4] = x;
    controller_data->analog[5] = x;

    controller_data->button_r = x > 0;
    controller_data->button_l = x > 0;

    controller_data->button_zl = x > 0;
    controller_data->button_zr = x > 0;
    /*
    controller_data->button_left_sl = x > 0;
    controller_data->button_left_sr = x > 0;

    controller_data->button_right_sl = x > 0;
    controller_data->button_right_sr = x > 0;
    */

    controller_data->battery_level = /*battery_level_charging | */ battery_level_full;
    controller_data->connection_info = 0x1;
    //controller_data->vibrator_input_report = 0x80;
    controller_data->vibrator_input_report = 0x07;
#else
    unsigned char rawData[12] = {
        0x83, 0x71, 0x00, 0x80, 0x00, 0xBA, 0x07, 0x6B, 0x47, 0xF7, 0x72, 0x0C};
    memcpy(controller_data, rawData, sizeof(rawData));

    tick += 3;
    controller_data->timestamp = tick;
#endif
}

// Standard full mode - input reports with IMU data instead of subcommand replies. Pushes current state @60Hz, or @120Hz if Pro Controller.
void input_report_0x30(uint8_t *usb_in, uint8_t *usb_out_buf)
{
    //usart_send_str(__func__);
    // report ID
    usb_out_buf[0x00] = kReportIdInput30;
    /*
    uint8_t p[] = {
        0x21, 0x09, 0x8E, 0x84, 0x00, 0x12, 0x01, 0x18, 0x80, 0x01, 0x18, 0x80, 0x80,
0x80, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 
    };

    memcpy(usb_out_buf, p, sizeof(p));
    */

    fill_input_report((struct ControllerData *)&usb_out_buf[0x01]);
    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);

    hw_led_toggle();
}

// Subcommand 0x10: SPI flash read
void input_sub_cmd_0x10(uint8_t *usb_in, uint8_t *usb_out_buf)
{
    usart_send_str(__func__);
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

// passthrough
void output_passthrough(uint8_t *usb_in, uint8_t *usb_out_buf)
{
    char dbg[0x40] = {};
    sprintf(dbg, "output_passthrough 0x%02x", usb_in[1]);
    usart_send_str(dbg);

    const uint8_t response_h[] = {0x81, usb_in[1], 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    memcpy(usb_out_buf, response_h, sizeof(response_h));
    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
}

// 80 01
void output_mac_addr(uint8_t *usb_in, uint8_t *usb_out_buf)
{
    usart_send_str(__func__);
    /*
    // Verified
    // hard coded response !!!
    const uint8_t response_h[] = {
        kUsbReportIdInput81, kSubTypeRequestMac, 0x00, kUsbDeviceTypeProController,
        mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]};
    memcpy(usb_out_buf, response_h, sizeof(response_h));
    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
    */
    const uint8_t response_h[] = {
        0x81, 0x01, 0x00, 0x03, 0xc1, 0xc9, 0x3e, 0xe9, 0xb6, 0x98, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* ......>......... */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* ................ */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* ................ */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* ................ */
    };
    memcpy(usb_out_buf, response_h, sizeof(response_h));
    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
}

// passthrough
// Verified
void output_handshake(uint8_t *usb_in, uint8_t *usb_out_buf)
{
    usart_send_str(__func__);
    memset(usb_out_buf, 0, 0x40);
    usb_out_buf[0] = 0x81;
    usb_out_buf[1] = kSubTypeHandshake;
    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
}

// baudrate
// Verified
void output_baudrate(uint8_t *usb_in, uint8_t *usb_out_buf)
{
    usart_send_str(__func__);
    memset(usb_out_buf, 0, 0x40);
    usb_out_buf[0] = 0x81;
    usb_out_buf[1] = 0x03;
    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
}

// baudrate
// Verified
void output_enable_usb_timeout(uint8_t *usb_in, uint8_t *usb_out_buf)
{
    usart_send_str(__func__);
    memset(usb_out_buf, 0, 0x40);
    usb_out_buf[0] = 0x81;
    usb_out_buf[1] = kSubTypeDisableUsbTimeout;
    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
}

// baudrate
// Verified
void output_disable_usb_timeout(uint8_t *usb_in, uint8_t *usb_out_buf)
{
    usart_send_str(__func__);
    memset(usb_out_buf, 0, 0x40);
    usb_out_buf[0] = 0x81;
    usb_out_buf[1] = kSubTypeEnableUsbTimeout;
    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
}

void output_report_0x80(uint8_t *buf, uint8_t *usb_out_buf)
{
    switch (buf[1])
    {
    case 0x01: // mac addr
        hw_led_toggle();
        output_mac_addr(buf, usb_out_buf);
        break;
        //handshake//baudrate
    case 0x02:
        output_handshake(buf, usb_out_buf);
        break;
    case 0x03:
        output_baudrate(buf, usb_out_buf);
        break;
    // usb timeout
    case 0x04:
        output_enable_usb_timeout(buf, usb_out_buf);
        break;
    case 0x05:
        output_disable_usb_timeout(buf, usb_out_buf);
        break;
    case 0x91:
    case 0x92:
        output_passthrough(buf, usb_out_buf);
        break;
    default:
        output_passthrough(buf, usb_out_buf);
        break;
    }
}

static uint8_t joyStickMode = 0;

void output_report_0x01_unknown_subcmd(uint8_t *buf, uint8_t *usb_out_buf)
{
    // usart_send_str("output_report_0x01_unknown_subcmd");
    char dbg[0x40] = {};
    sprintf(dbg, "output_report_0x01_unknown_subcmd 0x%02x", buf[10]);
    usart_send_str(dbg);

    struct Report81Response *resp = (struct Report81Response *)&usb_out_buf[0x01];
    // report ID
    usb_out_buf[0x00] = kReportIdInput21;
    fill_input_report(&resp->controller_data);
    resp->subcommand_ack = 0x80;
    resp->subcommand = buf[10];
    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
}

// Subcommand 0x08: Set shipment low power state
void output_report_0x01_0x08_lowpower_state(uint8_t *buf, uint8_t *usb_out_buf)
{
    usart_send_str(__func__);
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
    memcpy(usb_out_buf, rawData, sizeof(rawData));
    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
}

// Subcommand 0x02: Request device info
void output_report_0x01_get_device_info(uint8_t *buf, uint8_t *usb_out_buf)
{
    usart_send_str(__func__);
    unsigned char rawData[64] = {
        0x21, 0xf8, 0x81, 0x00, 0x80, 0x00, 0x8f, 0xf8, 0x7a, 0x53, 0x78, 0x78, 0x00, 0x82, 0x02, 0x03, /* !.......zSxx.... */
        0x48, 0x03, 0x02, 0x98, 0xb6, 0xe9, 0x3e, 0xc9, 0xc1, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, /* H.....>......... */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* ................ */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* ................ */
    };

    memcpy(usb_out_buf, rawData, 0x40);
    //struct Report81Response *resp = (struct Report81Response *)&usb_out_buf[0x01];
    //fill_input_report(&resp->controller_data);

    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
}

// Subcommand 0x03: Set input report mode
/* todo */
void output_report_0x01_set_report_mode(uint8_t *buf, uint8_t *usb_out_buf)
{
    usart_send_str(__func__);
#if 1
    struct Report81Response *resp = (struct Report81Response *)&usb_out_buf[0x01];
    // report ID
    usb_out_buf[0x00] = kReportIdInput21;

    // acknowledge
    resp->subcommand_ack = 0x80;
    resp->subcommand = 0x03;

    fill_input_report(&resp->controller_data);

    joyStickMode = buf[11];

    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
#else
    joyStickMode = buf[11];

    memset(usb_out_buf, 0, 0x40);
    usb_out_buf[0] = 0x81;
    usb_out_buf[1] = 0x03;
    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
#endif
}

// Subcommand 0x04: Trigger buttons elapsed time
/* todo */
void output_report_0x01_trigger_elapsed(uint8_t *buf, uint8_t *usb_out_buf)
{
    usart_send_str(__func__);
    struct Report81Response *resp = (struct Report81Response *)&usb_out_buf[0x01];
    // report ID
    usb_out_buf[0x00] = kReportIdInput21;

    // acknowledge
    resp->subcommand_ack = 0x83;
    resp->subcommand = 0x04;

    fill_input_report(&resp->controller_data);
    // usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);

    // ba
    const uint8_t resp_[] = {
        0x21, 0x0A, 0x8E, 0x84, 0x00, 0x12, 0x01, 0x18, 0x80, 0x01, 0x18, 0x80, 0x80,
        0x83, 0x04, 0x00, 0xCC, 0x00, 0xEE, 0x00, 0xFF, 0x00, 0x00, 0x00};

    usb_write_packet(ENDPOINT_HID_IN, resp_, sizeof(resp_));
}

/* todo */
void output_report_0x01_readspi(uint8_t *buf, uint8_t *usb_out_buf)
{
    usart_send_str(__func__);

    struct SpiReadReport *resp = (struct SpiReadReport *)&usb_out_buf[0x01];
    uint16_t addr = *(uint16_t *)(&buf[kSubCommandDataOffset]);
    uint8_t len = buf[kSubCommandDataOffset + 4];

#if 1
    char dbg[0x40] = {};
    sprintf(dbg, "0x%04x 0x%02x", addr, len);
    usart_send_str(dbg);
#endif

    memset(usb_out_buf, 0x00, 0x40);
    usb_out_buf[0x00] = kReportIdInput21;

    fill_input_report(&resp->controller_data);

    resp->subcommand_ack = 0x90;
    resp->subcommand = 0x10;
    resp->addr = addr;

    spi_read(addr, len, resp->spi_data);

    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
}

/* todo */
void output_report_0x01_writespi(uint8_t *buf, uint8_t *usb_out_buf)
{
    usart_send_str(__func__);
    struct ResponseX81 *resp = (struct ResponseX81 *)&usb_out_buf[0x01];
    uint16_t addr = *(uint16_t *)(&buf[kSubCommandDataOffset]);
    uint8_t len = buf[kSubCommandDataOffset + 4];

    // report ID
    usb_out_buf[0x00] = kReportIdInput21;
    fill_input_report(&resp->controller_data);

    spi_write(addr, len, &buf[kSubCommandDataOffset + 5]);

    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
}

/* todo */
void output_report_0x01_erasespi(uint8_t *buf, uint8_t *usb_out_buf)
{
    usart_send_str(__func__);
    struct ResponseX81 *resp = (struct ResponseX81 *)&usb_out_buf[0x01];
    uint16_t addr = *(uint16_t *)(&buf[kSubCommandDataOffset]);
    uint8_t len = buf[kSubCommandDataOffset + 4];

    // report ID
    usb_out_buf[0x00] = kReportIdInput21;
    fill_input_report(&resp->controller_data);
    spi_erase(addr, len);
    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
}

/* todo */
void output_report_0x01_set_lights(uint8_t *buf, uint8_t *usb_out_buf)
{
    usart_send_str(__func__);
    struct ResponseX81 *resp = (struct ResponseX81 *)&usb_out_buf[0x01];
    // report ID
    usb_out_buf[0x00] = kReportIdInput21;
    fill_input_report(&resp->controller_data);
    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
}

/* todo */
void output_report_0x01_set_homelight(uint8_t *buf, uint8_t *usb_out_buf)
{
    usart_send_str(__func__);
    struct ResponseX81 *resp = (struct ResponseX81 *)&usb_out_buf[0x01];
    // report ID
    usb_out_buf[0x00] = kReportIdInput21;
    fill_input_report(&resp->controller_data);
    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
}

/* todo */
void output_report_0x01_set_immu(uint8_t *buf, uint8_t *usb_out_buf)
{
    usart_send_str(__func__);
    struct ResponseX81 *resp = (struct ResponseX81 *)&usb_out_buf[0x01];
    // report ID
    usb_out_buf[0x00] = kReportIdInput21;
    fill_input_report(&resp->controller_data);
    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
}

/* todo */
void output_report_0x01_set_immu_sensitivity(uint8_t *buf, uint8_t *usb_out_buf)
{
    usart_send_str(__func__);
    struct ResponseX81 *resp = (struct ResponseX81 *)&usb_out_buf[0x01];
    // report ID
    usb_out_buf[0x00] = kReportIdInput21;
    fill_input_report(&resp->controller_data);
    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
}

/* todo */
void output_report_0x01_set_vibration(uint8_t *buf, uint8_t *usb_out_buf)
{
    usart_send_str(__func__);
    struct ResponseX81 *resp = (struct ResponseX81 *)&usb_out_buf[0x01];
    // report ID
    usb_out_buf[0x00] = kReportIdInput21;

    resp->subcommand_ack = 0x80;
    resp->subcommand = 0x48;

    fill_input_report(&resp->controller_data);
    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
}

#if 0
/* todo */
void output_report_0x01_bt_pairing(uint8_t *buf, uint8_t *usb_out_buf)
{
    usart_send_str(__func__);
    struct ResponseX81 *resp = (struct ResponseX81 *)&usb_out_buf[0x01];
    struct subcommand *data = (struct subcommand *)&buf[1];

    // report ID
    usb_out_buf[0x00] = kReportIdInput21;
    fill_input_report(&resp->controller_data);

    uint8_t pairing_type = buf[11] /* data->pairing.type */;
    char dbg[0x40] = {};
    sprintf(dbg, "pairing_type 0x%02x, 0x%02x", pairing_type, data->pairing.type);
    usart_send_str(dbg);

    unsigned char rawData[64] = {
        0x21, 0x91, 0x91, 0x00, 0x80, 0x00, 0xBB, 0xE7, 0x6A, 0x45, 0x07, 0x73,
        0x09, 0x81, 0x01, 0x01, 0x36, 0x2E, 0xC6, 0x8A, 0xBB, 0x7C, 0x00, 0x25,
        0x08, 0x50, 0x72, 0x6F, 0x20, 0x43, 0x6F, 0x6E, 0x74, 0x72, 0x6F, 0x6C,
        0x6C, 0x65, 0x72, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x68, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00};

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
        /*
        // Ok
        //*ptr =
        *ptr++ = 0x01;
        *ptr++ = 0x01;
        memcpy(ptr, mac_addr, 6);

        memcpy(usb_out_buf, rawData, sizeof(rawData));

        usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
        */
        unsigned char paring_1[64] = {
            0x21, 0x91, 0x91, 0x00, 0x80, 0x00, 0xBB, 0xE7, 0x6A, 0x45, 0x07, 0x73,
            0x09, 0x81, 0x01, 0x01, 0x36, 0x2E, 0xC6, 0x8A, 0xBB, 0x7C, 0x00, 0x25,
            0x08, 0x50, 0x72, 0x6F, 0x20, 0x43, 0x6F, 0x6E, 0x74, 0x72, 0x6F, 0x6C,
            0x6C, 0x65, 0x72, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x68, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00};

        usb_write_packet(ENDPOINT_HID_IN, paring_1, 0x40);
    }
    /* ex: pairing_type == 0x02
01 0c 00 00 00 00 00 00 00 00 01 02 ba d8 e2 13 00 00 00 00 3b f8 cd 6a 00 00 00 04 ed ac e2 13 00 00 00 78 b9 d8 e2 13 00 00 00 50 ba d8 e2 13 00
*/

    else if (pairing_type == 0x02)
    {
        unsigned char paring_2[64] = {
            0x21, 0x98, 0x91, 0x00, 0x80, 0x00, 0xB9, 0x07, 0x6B, 0x46, 0x17, 0x73,
            0x09, 0x81, 0x01, 0x02, 0xE5, 0x51, 0x63, 0x7E, 0xE3, 0x34, 0x82, 0x1E,
            0xE5, 0x99, 0x24, 0x76, 0xCE, 0x2B, 0xA8, 0xB7, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00};
        usb_write_packet(ENDPOINT_HID_IN, paring_2, 0x40);

        /*
        
        uint8_t ltkHash[] = {
            0x1A, 0xD3, 0x27, 0x14, 0x6F, 0x7E, 0x4F, 0xD7, 0x5D, 0x14, 0x6B, 0xEB,
            0x17, 0x5D, 0x7C, 0xE7};
        // xor it
        for (int i = 0; i < sizeof(ltkHash); i++)
        {
            ltkHash[i] = ltkHash[i] ^ 0xAA;
        }

        rawData[13] = 0x81;
        rawData[14] = 0x01;
        *ptr++ = 0x02;
        memcpy(ptr, ltkHash, sizeof(ltkHash));
        memcpy(usb_out_buf, rawData, sizeof(rawData));
        usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
        */
    }
    else if (pairing_type == 0x03)
    {
        /*
        uint8_t ltkHash[] = {
            0x1A, 0xD3, 0x27, 0x14, 0x6F, 0x7E, 0x4F, 0xD7, 0x5D, 0x14, 0x6B, 0xEB,
            0x17, 0x5D, 0x7C, 0xE7};
        // xor it
        for (int i = 0; i < sizeof(ltkHash); i++)
        {
            ltkHash[i] = ltkHash[i] ^ 0xAA;
        }

        rawData[13] = 0x81;
        rawData[14] = 0x01;
        *ptr++ = 0x03;
        memcpy(ptr, ltkHash, sizeof(ltkHash));
        memcpy(usb_out_buf, rawData, sizeof(rawData));
        usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
        */
        /* Untitled4 (02/06/2019 21:16:35)
   StartOffset(h): 00000000, EndOffset(h): 0000003F, Length(h): 00000040 */

        unsigned char paring_3[64] = {
            0x21, 0x9E, 0x91, 0x00, 0x80, 0x00, 0xBB, 0xF7, 0x6A, 0x45, 0x07, 0x73,
            0x09, 0x81, 0x01, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00};
        usb_write_packet(ENDPOINT_HID_IN, paring_3, 0x40);
    }
    else
    {
        usart_send_str("No packed sent");
    }
}
#else

void output_report_0x01_bt_pairing(uint8_t *buf, uint8_t *usb_out_buf)
{
    usart_send_str(__func__);
    const uint8_t data_01[] = {
        0x21, 0xf6, 0x81, 0x00, 0x80, 0x00, 0x8f, 0xf8, 0x7a, 0x54, 0x58, 0x78, 0x00, 0x81, 0x01, 0x01, /* !.......zTXx.... */
        0xc1, 0xc9, 0x3e, 0xe9, 0xb6, 0x98, 0x00, 0x25, 0x08, 0x50, 0x72, 0x6f, 0x20, 0x43, 0x6f, 0x6e, /* ..>....%.Pro Con */
        0x74, 0x72, 0x6f, 0x6c, 0x6c, 0x65, 0x72, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x68, 0x00, 0x00, /* troller......h.. */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* ................ */
    };

    const uint8_t data_02[] = {
        0x21, 0xfd, 0x81, 0x00, 0x80, 0x00, 0x90, 0xe8, 0x7a, 0x53, 0x58, 0x78, 0x00, 0x81, 0x01, 0x02, /* !.......zSXx.... */
        0xe5, 0xc8, 0xe4, 0x92, 0x05, 0xff, 0xc9, 0x8a, 0x7d, 0xea, 0x15, 0xf6, 0x19, 0xba, 0x82, 0x13, /* ........}....... */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* ................ */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    const uint8_t data_03[] = {
        0x21,
        0x05,
        0x81,
        0x00,
        0x80,
        0x00,
        0x90,
        0xc8,
        0x7a,
        0x53,
        0x58,
        0x78,
        0x00,
        0x81,
        0x01,
        0x03, /* !.......zSXx.... */
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00, /* ................ */
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00, /* ................ */
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
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

    uint8_t pairing_type = buf[11] /* data->pairing.type */;
    char dbg[0x40] = {};
    sprintf(dbg, "pairing_type 0x%02x", pairing_type);
    usart_send_str(dbg);

    uint8_t *data = (uint8_t *)data_01;
    switch (pairing_type)
    {
    case 1:
        data = (uint8_t *)data_01;
        break;
    case 2:
        data = (uint8_t *)data_02;
        break;
    default:
    case 3:
        data = (uint8_t *)data_03;
        break;
    }
    memcpy(usb_out_buf, data, 0x40);
    usb_write_packet(ENDPOINT_HID_IN, usb_out_buf, 0x40);
}
#endif

void output_report_0x10(uint8_t *buf, uint8_t *usb_out_buf)
{
    /** nothing **/
    // Joy-con does not reply when Output Report is 0x10
    usart_send_str(__func__);

    // output_report_0x01_unknown_subcmd(buf, usb_out_buf);
}

// Sub command !
void output_report_0x01(uint8_t *buf, uint8_t *usb_out_buf)
{
    uint8_t subCmd = buf[10];

    switch (subCmd)
    {
    case 0x01:
        output_report_0x01_bt_pairing(buf, usb_out_buf);
        break;
        // get device info
    case 0x02:
        output_report_0x01_get_device_info(buf, usb_out_buf);
        break;
        // Set input report mode
    case 0x03:
        output_report_0x01_set_report_mode(buf, usb_out_buf);
        break;
        // unknown ?
    case 0x04:
        output_report_0x01_trigger_elapsed(buf, usb_out_buf);
        break;
        // unknown ?
    case 0x08:
        output_report_0x01_0x08_lowpower_state(buf, usb_out_buf);
        break;
    // Read Spi
    case 0x10:
        output_report_0x01_readspi(buf, usb_out_buf);
        break;
    case 0x11:
        output_report_0x01_writespi(buf, usb_out_buf);
        break;
    case 0x12:
        output_report_0x01_erasespi(buf, usb_out_buf);
        break;
    // Set Lights
    case 0x30:
        output_report_0x01_set_lights(buf, usb_out_buf);
        break;
    // Set Home Light
    case 0x38:
        output_report_0x01_set_homelight(buf, usb_out_buf);
        break;
    // Set Immu
    case 0x40:
        output_report_0x01_set_immu(buf, usb_out_buf);
        break;
    // Set Immu Sensitivity
    case 0x41:
        output_report_0x01_set_immu_sensitivity(buf, usb_out_buf);
        break;
    // Set Vibration
    case 0x48:
        output_report_0x01_set_vibration(buf, usb_out_buf);
        break;

    case 0x00:
    case 0x33:
    default:
        output_report_0x01_unknown_subcmd(buf, usb_out_buf);
        break;
    }
}

volatile bool working = false;

uint8_t last_usb_buf[0x40];
volatile uint8_t usb_packet_flags = 0;

void do_work(uint8_t *current_usb_buf, uint8_t len)
{

    uint8_t cmd = current_usb_buf[0];
    uint8_t usb_out_buf[0x40];

    usart_send_str("Recv: ");
    dump_hex(current_usb_buf, 0x40);

    switch (cmd)
    {
    case kReportIdOutput01:
        output_report_0x01(current_usb_buf, usb_out_buf);
        break;
    case kReportIdOutput10:
        output_report_0x10(current_usb_buf, usb_out_buf);
        break;

    case kUsbReportIdOutput80:
        output_report_0x80(current_usb_buf, usb_out_buf);
        break;

    case kReportIdInput30:
        //default:
        input_report_0x30(current_usb_buf, usb_out_buf);
        break;
    }

    if (kReportIdOutput10 != cmd)
    {
        usart_send_str("Response: ");
        dump_hex(usb_out_buf, 0x40);
    }
    usart_send_str(" ===== ");
}

void handle_packet()
{
    // packet received handle it...
    if (usb_packet_flags & 0x01)
    {
        usb_packet_flags = 0;

        uint8_t current_usb_buf[0x40];
        mlock();
        memcpy(current_usb_buf, last_usb_buf, 0x40);
        munlock();

        do_work(current_usb_buf, 0x40);
    }
}

void hid_rx_cb(uint8_t *buf, uint16_t len)
{
#if 0
    mlock();
    memcpy(last_usb_buf, buf, len);
    usb_packet_flags = 0x01;
    munlock();
#else
    if (buf[0] != 0x00)
        do_work(buf, len);
#endif
}

static int sys_0x30 = 0;

void handle_input_0x30()
{

    uint8_t usb_out_buf[0x40];
    if (sys_0x30 == 1)
    {
        input_report_0x30(NULL, usb_out_buf);
        sys_0x30 = 0;
    }
}

void sys_tick_handler(void)
{
#if 1
    if (joyStickMode == 0x30)
    {
        sys_0x30 = 1;
    }
#else
    mlock();
    last_usb_buf[0] = 0x30;
    usb_packet_flags = 0x01;
    munlock();
#endif
}
