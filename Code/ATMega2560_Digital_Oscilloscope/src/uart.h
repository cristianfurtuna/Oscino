#ifndef UART_H
#define UART_H
#include <stdio.h>
#include <stdint.h>

#define MYUBRR	F_CPU/8/BAUD-1	//formula for double speed (higher precision)
#define UART_RX_BUFSIZE 128

void uart_init (unsigned int ubrr);
int uart_putchar(char c, FILE *stream);
int  uart_getchar(FILE *stream);

uint8_t uart_available(void);      // number of bytes currently buffered
int     uart_read_byte(void);      // returns 0..255, or -1 if none
int     uart_peek_byte(void);      // like read but doesn’t consume; -1 if none
void    uart_flush_rx(void);       // drop all pending RX data

extern FILE uart_output;
extern FILE uart_input;

#endif // UART_H