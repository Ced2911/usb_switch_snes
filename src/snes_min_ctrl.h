#pragma once
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/i2c.h>
#include <libopencm3/stm32/gpio.h>

#define NUNCHUK_DEVICE_ID 0x52

#define I2C_N I2C2

void sns_init()
{
    rcc_periph_clock_enable(RCC_I2C2);
    rcc_periph_clock_enable(RCC_AFIO);

    /* Set alternate functions for the SCL and SDA pins of I2C2. */
    gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_50_MHZ,
                  GPIO_CNF_OUTPUT_ALTFN_OPENDRAIN,
                  GPIO_I2C2_SCL | GPIO_I2C2_SDA);

    /* Disable the I2C before changing any configuration. */
    i2c_peripheral_disable(I2C_N);

    /* APB1 is running at 36MHz. */
    i2c_set_clock_frequency(I2C_N, I2C_CR2_FREQ_36MHZ);

    /* 400KHz - I2C Fast Mode */
    i2c_set_fast_mode(I2C_N);

    /*
	 * fclock for I2C is 36MHz APB2 -> cycle time 28ns, low time at 400kHz
	 * incl trise -> Thigh = 1600ns; CCR = tlow/tcycle = 0x1C,9;
	 * Datasheet suggests 0x1e.
	 */
    i2c_set_ccr(I2C_N, 0x1e);

    /*
	 * fclock for I2C is 36MHz -> cycle time 28ns, rise time for
	 * 400kHz => 300ns and 100kHz => 1000ns; 300ns/28ns = 10;
	 * Incremented by 1 -> 11.
	 */
    i2c_set_trise(I2C_N, 0x0b);

    /*
	 * This is our slave address - needed only if we want to receive from
	 * other masters.
	 */
    // i2c_set_own_7bit_slave_address(I2C_N, OWN_SLAVE_ADDRESS);

    /* If everything is configured -> enable the peripheral. */
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