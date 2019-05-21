#include <stdlib.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/systick.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include "hwinit.h"

void hw_led_on()
{

    gpio_clear(GPIOB, GPIO12);
}
void hw_led_off()
{

    gpio_set(GPIOB, GPIO12);
}
void hw_led_toggle() {
    gpio_toggle(GPIOB, GPIO12);
}

void hw_init()
{
    rcc_clock_setup_in_hsi_out_48mhz();

    rcc_periph_clock_enable(RCC_GPIOA);
    gpio_set_mode(GPIOA,
                  GPIO_MODE_OUTPUT_2_MHZ,
                  GPIO_CNF_OUTPUT_PUSHPULL,
                  GPIO12);
    gpio_clear(GPIOA, GPIO12);

    rcc_periph_clock_enable(RCC_GPIOB);
    gpio_set_mode(GPIOB,
                  GPIO_MODE_OUTPUT_2_MHZ,
                  GPIO_CNF_OUTPUT_PUSHPULL,
                  GPIO12);

    gpio_clear(GPIOB, GPIO12);
    for (uint32_t j = 0; j < 10; j++)
        for (unsigned i = 0; i < 800000; i++)
        {
            __asm__("nop");
        }
    gpio_set(GPIOB, GPIO12);
}