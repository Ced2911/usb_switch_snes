#pragma once


void usart_init();
void usart_send_str(char *p);

// should be called once per loop
void uart_flush();
