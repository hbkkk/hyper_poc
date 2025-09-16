#ifndef HYPER_POC_UART_H
#define HYPER_POC_UART_H

void uart_putc(char c);
void uart_puts(char *s);
int uart_getc(void);
void uart_init(void);
void clear_uart_interrupt(void);

#endif