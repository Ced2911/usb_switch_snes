#pragma once
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/i2c.h>
#include <libopencm3/stm32/gpio.h>

#define NUNCHUK_DEVICE_ID 0x52

#define I2C_TIMEOUT 1000
#define ERR_TIMEOUT -1

enum
{
    CTRLR_UNINITILIZED,
    CTRLR_PASS_1,
    CTRLR_INITILIZED,
    CTRLR_PRESENT,
    CTRLR_TIMEOUT,
    CTRLR_UNPLUGGED
};

typedef struct
{
    uint32_t i2c;
    uint16_t clk;
    uint16_t gpios;
    uint8_t state;
    uint8_t packet[8];
} snes_i2c_state;

static snes_i2c_state controller_1 = {
    .i2c = I2C1,
    .clk = RCC_I2C1,
    .gpios = GPIO_I2C1_SCL | GPIO_I2C1_SDA,
    .state = CTRLR_UNINITILIZED};

static snes_i2c_state controller_2 = {
    .i2c = I2C2,
    .clk = RCC_I2C2,
    .gpios = GPIO_I2C2_SCL | GPIO_I2C2_SDA,
    .state = CTRLR_UNINITILIZED};

static const uint8_t _packet_0a[] = {0xf0, 0x55};
static const uint8_t _packet_0b[] = {0xfb, 0x00};
static const uint8_t _packet_id[] = {0xfa};
static const uint8_t _packet_read[] = {0};

void sns_init(snes_i2c_state *controller)
{
    rcc_periph_clock_enable(controller->clk);

    gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_50_MHZ,
                  GPIO_CNF_OUTPUT_ALTFN_OPENDRAIN,
                  controller->gpios);

    i2c_reset(controller->i2c);
    i2c_peripheral_disable(controller->i2c);
    i2c_set_speed(controller->i2c, i2c_speed_fm_400k, rcc_apb1_frequency / 1e6);
    i2c_peripheral_enable(controller->i2c);
}

#define I2C_CHECK_TIMEOUT(b)                                      \
    {                                                             \
        uint32_t _timeout = I2C_TIMEOUT;                          \
        while (b)                                                 \
        {                                                         \
            _timeout--;                                           \
            if (_timeout == 0)                                    \
            {                                                     \
                usart_send_direct("i2c_check_timeout timeout\n"); \
                return ERR_TIMEOUT;                               \
            }                                                     \
        }                                                         \
    }

static int i2c_write7_timeout(uint32_t i2c, int addr, uint8_t *data, size_t n)
{

    I2C_CHECK_TIMEOUT((I2C_SR2(i2c) & I2C_SR2_BUSY));

    i2c_send_start(i2c);

    /* Wait for master mode selected */
    I2C_CHECK_TIMEOUT(!((I2C_SR1(i2c) & I2C_SR1_SB) & (I2C_SR2(i2c) & (I2C_SR2_MSL | I2C_SR2_BUSY))));

    i2c_send_7bit_address(i2c, addr, I2C_WRITE);

    /* Waiting for address is transferred. */
    I2C_CHECK_TIMEOUT(!(I2C_SR1(i2c) & I2C_SR1_ADDR));

    /* Clearing ADDR condition sequence. */
    (void)I2C_SR2(i2c);

    for (size_t i = 0; i < n; i++)
    {
        i2c_send_data(i2c, data[i]);
        I2C_CHECK_TIMEOUT(!(I2C_SR1(i2c) & (I2C_SR1_BTF)));
    }

    return n;
}

static int i2c_read7_timeout(uint32_t i2c, int addr, uint8_t *res, size_t n)
{
    i2c_send_start(i2c);
    i2c_enable_ack(i2c);

    /* Wait for master mode selected */
    I2C_CHECK_TIMEOUT(!((I2C_SR1(i2c) & I2C_SR1_SB) & (I2C_SR2(i2c) & (I2C_SR2_MSL | I2C_SR2_BUSY))));

    i2c_send_7bit_address(i2c, addr, I2C_READ);

    /* Waiting for address is transferred. */
    I2C_CHECK_TIMEOUT(!(I2C_SR1(i2c) & I2C_SR1_ADDR));

    /* Clearing ADDR condition sequence. */
    (void)I2C_SR2(i2c);

    for (size_t i = 0; i < n; ++i)
    {
        if (i == n - 1)
        {
            i2c_disable_ack(i2c);
        }
        I2C_CHECK_TIMEOUT(!(I2C_SR1(i2c) & I2C_SR1_RxNE));
        res[i] = i2c_get_data(i2c);
    }
    i2c_send_stop(i2c);

    return n;
}

/**
 * Run a write/read transaction to a given 7bit i2c address
 * If both write & read are provided, the read will use repeated start.
 * Both write and read are optional
 * There are likely still issues with repeated start/stop condtions!
 * @param i2c peripheral of choice, eg I2C1
 * @param addr 7 bit i2c device address
 * @param w buffer of data to write
 * @param wn length of w
 * @param r destination buffer to read into
 * @param rn number of bytes to read (r should be at least this long)
 */
static int i2c_transfer7_timeout(uint32_t i2c, uint8_t addr, uint8_t *w, size_t wn, uint8_t *r, size_t rn)
{
    if (wn)
    {
        if (i2c_write7_timeout(i2c, addr, w, wn) == ERR_TIMEOUT)
        {
            return ERR_TIMEOUT;
        }
    }
    if (rn)
    {
        if (i2c_read7_timeout(i2c, addr, r, rn) == ERR_TIMEOUT)
        {
            return ERR_TIMEOUT;
        }
    }
    else
    {
        i2c_send_stop(i2c);
    }

    return 0;
}

// disable timeout
// #define i2c_transfer7_timeout i2c_transfer7
void sns_request(snes_i2c_state *controller)
{
    if (controller->state == CTRLR_PRESENT)
    {
        if (i2c_transfer7_timeout(controller->i2c, NUNCHUK_DEVICE_ID, NULL, 0, controller->packet, 6) == ERR_TIMEOUT)
        {
            controller->state = CTRLR_TIMEOUT;
        }
        else
        {
            // xor...
            controller->packet[4] ^= 0xFF;
            controller->packet[5] ^= 0xFF;

            dump_hex(controller->packet, 6);
            uart_flush();
        }
    }
}

void sns_poll(snes_i2c_state *controller)
{
    switch (controller->state)
    {
    case CTRLR_UNINITILIZED:
        if (i2c_transfer7_timeout(controller->i2c, NUNCHUK_DEVICE_ID, _packet_0a, sizeof(_packet_0a), NULL, 0) != ERR_TIMEOUT)
        {
            controller->state = CTRLR_PASS_1;
        }
        break;
    case CTRLR_PASS_1:
        usart_send_direct("CTRLR_PASS_1\n");
        if (i2c_transfer7_timeout(controller->i2c, NUNCHUK_DEVICE_ID, _packet_0b, sizeof(_packet_0b), NULL, 0) != ERR_TIMEOUT)
            controller->state = CTRLR_INITILIZED;
        break;
    case CTRLR_INITILIZED:
        usart_send_direct("CTRLR_INITILIZED\n");
        if (i2c_transfer7_timeout(controller->i2c, NUNCHUK_DEVICE_ID, _packet_id, sizeof(_packet_id), controller->packet, 4) != ERR_TIMEOUT)
            controller->state = CTRLR_PRESENT;
        break;
    case CTRLR_PRESENT:
        i2c_transfer7_timeout(controller->i2c, NUNCHUK_DEVICE_ID, _packet_read, sizeof(_packet_read), NULL, 0);
        break;
    }
}

// pinout
//power nc   data
//clk   nc gpd
//clk power  nc  data  gnd

// ok
// pb6 3v scl gnd