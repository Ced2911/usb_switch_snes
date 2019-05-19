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

uint8_t usbbuf[0x40];
struct ControllerDataReport controllerDataReport;
uint8_t tick = 0;

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

void hid_rx_cb(uint8_t *buf, uint16_t len)
{
    dump_hex(*buf, len);
}

void sys_tick_handler(void)
{
    static int x = 0;
    static int dir = 1;
    tick++;

    // report ID
    usbbuf[0x0] = 0x30;
    // usbbuf[1] = dir;
    x += dir;
    if (x > 30)
        dir = -dir;
    if (x < -30)
        dir = -dir;

    uint8_t *ptr = (uint8_t *)&controllerDataReport;
    memset(&controllerDataReport, 0, sizeof(struct ControllerDataReport));
    //controllerDataReport.controller_data.button_y = (x > 10);
    //controllerDataReport.controller_data.analog[0] = x;
    //controllerDataReport.controller_data.analog[1] = x;
    //controllerDataReport.controller_data.analog[2] = x;
    //controllerDataReport.controller_data.analog[3] = x;
    controllerDataReport.controller_data.analog[4] = x;
    controllerDataReport.controller_data.analog[5] = x;
    /*
    if (tick & 0x80)
    {
        controllerDataReport.controller_data.button_left_sl = 1;
        controllerDataReport.controller_data.button_left_sr = 1;
        controllerDataReport.controller_data.dpad_down = 1;
        controllerDataReport.controller_data.dpad_right = 1;
    }

    controllerDataReport.controller_data.button_left_sl = 1;
    controllerDataReport.controller_data.button_left_sr = 1;
    controllerDataReport.controller_data.dpad_down = 1;
    controllerDataReport.controller_data.dpad_right = 1;
    */

    // usb_send_serial_data("Bim\0", 4);

    memcpy(&usbbuf[1], &ptr[2], sizeof(struct ControllerDataReport) - 2);
    usb_write_packet(ENDPOINT_HID_IN, usbbuf, 0x40);
}
