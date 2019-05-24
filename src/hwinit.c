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

void hw_led_toggle()
{
    gpio_toggle(GPIOB, GPIO12);
}

void hw_init()
{
#if 0
    rcc_clock_setup_in_hsi_out_48mhz();
	//rcc_clock_setup_in_hse_8mhz_out_72mhz();
    rcc_periph_clock_enable(RCC_GPIOA);
    rcc_periph_clock_enable(RCC_GPIOB);
    
	rcc_periph_reset_pulse(RST_USB);

    gpio_set_mode(GPIOA,
                  GPIO_MODE_OUTPUT_10_MHZ,
                  GPIO_CNF_OUTPUT_PUSHPULL,
                  GPIO12);

    gpio_clear(GPIOA, GPIO12);

    gpio_set_mode(GPIOB,
                  GPIO_MODE_OUTPUT_10_MHZ,
                  GPIO_CNF_OUTPUT_PUSHPULL,
                  GPIO12);

    gpio_clear(GPIOB, GPIO12);


	gpio_set_mode(GPIOA, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT, GPIO8);


    for (unsigned i = 0; i < 800000; i++)
    {
        __asm__("nop");
    }
    gpio_set(GPIOB, GPIO12);
#else
    rcc_clock_setup_in_hse_8mhz_out_72mhz();

	rcc_periph_clock_enable(RCC_GPIOA);
	rcc_periph_clock_enable(RCC_GPIOB);
	rcc_periph_clock_enable(RCC_GPIOC);

    gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_2_MHZ,
                  GPIO_CNF_OUTPUT_PUSHPULL, GPIO12);
    gpio_clear(GPIOA, GPIO12);
    for (unsigned i = 0; i < 800000; i++)
    {
        __asm__("nop");
    }

#endif
}