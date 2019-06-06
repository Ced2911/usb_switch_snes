#pragma once
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/i2c.h>
#include <libopencm3/stm32/gpio.h>

#define NUNCHUK_DEVICE_ID 0x52

#define I2C_N I2C1

void sns_init()
{
    rcc_periph_clock_enable(RCC_I2C1);

    gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_50_MHZ,
                  GPIO_CNF_OUTPUT_ALTFN_OPENDRAIN,
                  GPIO_I2C1_SCL | GPIO_I2C1_SDA);

    i2c_peripheral_disable(I2C_N);

    i2c_set_speed(I2C_N, i2c_speed_fm_400k, 8);
    i2c_peripheral_enable(I2C_N);
}

void sns_plug()
{
    usart_send_direct("sns_plug _packet_0...");
    uint8_t _packet_0[] = {0xF0, 0x55};
    i2c_transfer7(I2C_N, NUNCHUK_DEVICE_ID, _packet_0, sizeof(_packet_0), NULL, 0);

    usart_send_direct("sns_plug _packet_1...");
    uint8_t _packet_1[] = {0xFB, 0x00};
    i2c_transfer7(I2C_N, NUNCHUK_DEVICE_ID, _packet_1, sizeof(_packet_1), NULL, 0);

    usart_send_direct("sns_plug _packet_2...");
    uint8_t _packet_2[] = {0xFA};
    i2c_transfer7(I2C_N, NUNCHUK_DEVICE_ID, _packet_2, sizeof(_packet_2), NULL, 0);

    usart_send_direct("sns_plug _packet_id...");
    uint8_t _packet_id[4] = {0};
    i2c_transfer7(I2C_N, NUNCHUK_DEVICE_ID, NULL, 0, _packet_id, sizeof(_packet_id));
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