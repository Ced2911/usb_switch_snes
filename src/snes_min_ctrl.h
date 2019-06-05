#pragma once
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/i2c.h>
#include <libopencm3/stm32/gpio.h>

#define NUNCHUK_DEVICE_ID 0x52

#define I2C_N I2C2

void sns_init()
{
    rcc_periph_clock_enable(RCC_I2C2);

    /* Set alternate functions for the SCL and SDA pins of I2C2. */
    gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_50_MHZ,
                  GPIO_CNF_OUTPUT_ALTFN_OPENDRAIN,
                  GPIO_I2C2_SCL | GPIO_I2C2_SDA);

    /* Disable the I2C before changing any configuration. */
    i2c_peripheral_disable(I2C_N);

    i2c_set_speed(I2C_N, i2c_speed_sm_100k, 8);
    i2c_peripheral_enable(I2C_N);
}

void i2c_init(uint32_t port)
{
}

void i2c_start(uint32_t i2c)
{
    i2c_send_start(i2c);
}

void i2c_stop(uint32_t i2c)
{
    i2c_send_stop(i2c);
}

void i2c_write_u8(uint32_t i2c, uint8_t data)
{
    i2c_send_data(i2c, data);
    while (!(I2C_SR1(i2c) & (I2C_SR1_BTF | I2C_SR1_TxE)))
    {
    };
}

void i2c_write_buff(uint32_t i2c, const uint8_t *data, uint32_t len)
{
    for (uint32_t i = 0; i < len; ++i)
    {
        i2c_write_u8(i2c, data[i]);
    }
}

void i2c_write(uint32_t i2c, uint8_t addr, uint8_t *data, uint32_t len)
{
    i2c_start(i2c);

    while ((I2C_SR1(i2c) & I2C_SR1_SB) == 0)
        ;

    i2c_send_7bit_address(i2c, addr, I2C_READ);
    while (!(I2C_SR1(i2c) & I2C_SR1_ADDR))
        ;

    uint32_t reg32 = I2C_SR2(i2c); //Read status register.

    i2c_write_buff(i2c, data, len);

    i2c_stop(i2c);
}

void i2c_read(uint32_t i2c, uint8_t addr, uint8_t *data, uint32_t len)
{
    i2c_start(i2c);
    while ((I2C_SR1(i2c) & I2C_SR1_SB) == 0)
        ;

    i2c_send_7bit_address(i2c, addr, I2C_READ);
    while (!(I2C_SR1(i2c) & I2C_SR1_ADDR))
        ;

    uint32_t reg32 = I2C_SR2(i2c); //Read status register.

    for (int byteRead = 0; byteRead < len; ++byteRead)
    {
        while ((I2C_SR1(i2c) & I2C_SR1_BTF) == 0)
            ;
        data[byteRead] = I2C_DR(i2c);
    }

    i2c_stop(i2c);
}

void sns_plug()
{
    uint8_t _packet[] = {0x40, 0x00};
    i2c_write(I2C_N, NUNCHUK_DEVICE_ID, _packet, sizeof(_packet));
}

void sns_update(uint8_t * _packet)
{
    uint8_t _update_packet[] = {0x00};

    i2c_read(I2C_N, NUNCHUK_DEVICE_ID, _packet, sizeof(_packet));

    i2c_write(I2C_N, NUNCHUK_DEVICE_ID, _update_packet, sizeof(_update_packet));
}


// pinout
//power nc   data 
//clk   nc gpd
//clk power  nc  data  gnd