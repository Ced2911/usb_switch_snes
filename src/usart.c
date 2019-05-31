#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/usart.h>

#define UART_BUFF_SIZE 512
static uint8_t uart_buffer[UART_BUFF_SIZE];
static uint8_t *uart_current_ptr;

void usart_init()
{
    rcc_periph_clock_enable(RCC_AFIO);
    rcc_periph_clock_enable(RCC_USART2);

    /* Setup GPIO pin GPIO_USART1_TX. */
    gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_50_MHZ,
                  GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, GPIO_USART2_TX);

    /* Setup UART parameters. */
    usart_set_baudrate(USART2, 115200);
    usart_set_databits(USART2, 8);
    usart_set_stopbits(USART2, USART_STOPBITS_1);
    usart_set_mode(USART2, USART_MODE_TX);
    usart_set_parity(USART2, USART_PARITY_NONE);
    usart_set_flow_control(USART2, USART_FLOWCONTROL_NONE);

    usart_enable(USART2);
    memset(uart_buffer, 0x00, UART_BUFF_SIZE);
    uart_current_ptr = uart_buffer;
}

void usart_send_str(char *p)
{
    int len = strlen(p);
    memcpy(uart_current_ptr, p, len);
    uart_current_ptr += len;
}

void uart_flush()
{
    char *ptr = (char *)uart_buffer;
    char *end = (char *)uart_current_ptr;
    do
    {
        usart_send_blocking(USART2, *ptr);
    } while (*ptr++ < end);

    uart_current_ptr = uart_buffer;
}
