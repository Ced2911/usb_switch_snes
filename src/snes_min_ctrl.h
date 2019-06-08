#pragma once
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/i2c.h>
#include <libopencm3/stm32/gpio.h>

#define NUNCHUK_DEVICE_ID 0x52

#define I2C_N I2C1

typedef struct
{
    uint8_t left_stick_x : 6;
    uint8_t right_stick_x1 : 2;

    uint8_t left_stick_y : 6;
    uint8_t right_stick_x2 : 2;

    uint8_t right_stick_y : 5;
    uint8_t left_trigger1 : 2;
    uint8_t right_stick_x3 : 1;

    uint8_t right_trigger : 5;
    uint8_t left_trigger2 : 3;

    uint8_t dummy : 1; // bit is always set?
    uint8_t button_right_trigger : 1;
    uint8_t button_plus : 1;
    uint8_t button_home : 1;
    uint8_t button_minus : 1;
    uint8_t button_left_trigger : 1;
    uint8_t button_down : 1;
    uint8_t button_right : 1;

    uint8_t button_up : 1;
    uint8_t button_left : 1;
    uint8_t button_zr : 1;
    uint8_t button_x : 1;
    uint8_t button_a : 1;
    uint8_t button_y : 1;
    uint8_t button_b : 1;
    uint8_t button_zl : 1;
} snes_controller;

#define usart_send_direct
void sns_init()
{
    usart_send_direct("sns_init..");
    rcc_periph_clock_enable(RCC_I2C1);

    gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_50_MHZ,
                  GPIO_CNF_OUTPUT_ALTFN_OPENDRAIN,
                  GPIO_I2C1_SCL | GPIO_I2C1_SDA);

    i2c_reset(I2C1);
    i2c_peripheral_disable(I2C_N);
    // i2c_set_own_7bit_slave_address(I2C_N, 0x29);
    i2c_set_speed(I2C_N, i2c_speed_fm_400k, rcc_apb1_frequency / 1e6);
    i2c_peripheral_enable(I2C_N);
    usart_send_direct("sns_init ok");
}

void sns_plug()
{
    usart_send_direct("sns_plug _packet_0...");
    uint8_t _packet_0[] = {0xF0, 0x55};
    i2c_transfer7(I2C_N, NUNCHUK_DEVICE_ID, _packet_0, sizeof(_packet_0), NULL, 0);

    usart_send_direct("sns_plug _packet_1...");
    uint8_t _packet_1[] = {0xFB, 0x00};
    i2c_transfer7(I2C_N, NUNCHUK_DEVICE_ID, _packet_1, sizeof(_packet_1), NULL, 0);

    // Get id !
    usart_send_direct("sns_plug _packet_2...");
    uint8_t _packet_2[] = {0xFA};
    i2c_transfer7(I2C_N, NUNCHUK_DEVICE_ID, _packet_2, sizeof(_packet_2), NULL, 0);

    usart_send_direct("sns_plug _packet_id...");
    uint8_t _packet_id[4] = {0};
    i2c_transfer7(I2C_N, NUNCHUK_DEVICE_ID, NULL, 0, _packet_id, sizeof(_packet_id));

    dump_hex(_packet_id, sizeof(_packet_id));
}

void sns_update(uint8_t *_packet)
{
    uint8_t _packet_0[] = {0};
    usart_send_direct("sns_update _packet_0...");
    i2c_transfer7(I2C_N, NUNCHUK_DEVICE_ID, _packet_0, sizeof(_packet_0), NULL, 0);
    usart_send_direct("sns_update _packet...");
    i2c_transfer7(I2C_N, NUNCHUK_DEVICE_ID, NULL, 0, _packet, 6);
}

// pinout
//power nc   data
//clk   nc gpd
//clk power  nc  data  gnd
#undef usart_send_direct