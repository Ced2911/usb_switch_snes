#pragma once
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/i2c.h>
#include <libopencm3/stm32/gpio.h>

#define NUNCHUK_DEVICE_ID 0x52


#define CTRLR_UNINITILIZED 0
#define CTRLR_PASS_1 1
#define CTRLR_INITILIZED 2
#define CTRLR_PRESENT 3

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

void sns_update(snes_i2c_state *controller)
{
    if (controller->state == CTRLR_UNINITILIZED)
    {
        i2c_transfer7(controller->i2c, NUNCHUK_DEVICE_ID, _packet_0a, sizeof(_packet_0a), NULL, 0);

        controller->state = CTRLR_PASS_1;
        usart_send_direct("CTRLR_PASS_1 ok");
    }
    if (controller->state == CTRLR_PASS_1)
    {
        i2c_transfer7(controller->i2c, NUNCHUK_DEVICE_ID, _packet_0b, sizeof(_packet_0b), NULL, 0);

        controller->state = CTRLR_INITILIZED;
    }
    else if (controller->state == CTRLR_INITILIZED)
    {
        i2c_transfer7(controller->i2c, NUNCHUK_DEVICE_ID, _packet_id, sizeof(_packet_id), controller->packet, 4);

        usart_send_direct("CTRLR_PRESENT ok");
        controller->state = CTRLR_PRESENT;
    }
    else if (controller->state == CTRLR_PRESENT)
    {
        i2c_transfer7(controller->i2c, NUNCHUK_DEVICE_ID, NULL, 0, controller->packet, 6);
        // xor...
        controller->packet[4] ^= 0xFF;
        controller->packet[5] ^= 0xFF;
        
        dump_hex(controller->packet, 6);

        i2c_transfer7(controller->i2c, NUNCHUK_DEVICE_ID, _packet_read, sizeof(_packet_read), NULL, 0);
    }
    uart_flush();
}

// pinout
//power nc   data
//clk   nc gpd
//clk power  nc  data  gnd

// ok
// pb6 3v scl gnd