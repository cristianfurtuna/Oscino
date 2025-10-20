#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include "uart.h"

static volatile uint8_t rxbuf[UART_RX_BUFSIZE];
static volatile uint8_t rx_head = 0;   // next write
static volatile uint8_t rx_tail = 0;   // next read

static inline uint8_t _rb_next(uint8_t i) {
    uint8_t n = i + 1;
    return (n >= UART_RX_BUFSIZE) ? 0 : n;
}

uint8_t uart_available(void) {
    uint8_t h = rx_head, t = rx_tail;
    if (h >= t) return (uint8_t)(h - t);
    return (uint8_t)(UART_RX_BUFSIZE - (t - h));
}

int uart_peek_byte(void) {
    if (!uart_available()) return -1;
    return rxbuf[rx_tail];
}

int uart_read_byte(void) {
    if (!uart_available()) return -1;
    uint8_t b = rxbuf[rx_tail];
    rx_tail = _rb_next(rx_tail);
    return b;
}

void uart_flush_rx(void) {
    uint8_t sreg = SREG; cli();
    rx_head = rx_tail = 0;
    SREG = sreg;
}

int uart_putchar(char c, FILE *stream) {
    if (c == '\n') uart_putchar('\r', stream);   // add CR for terminal
    while (!(UCSR0A & (1 << UDRE0)));           // wait for TX buffer empty
    UDR0 = c;
    return 0;
}

/* ---------- RX (stdio) ---------- */
/* Blocking getchar for scanf/getchar; echoes are optional (off by default). */
int uart_getchar(FILE *stream) {
    (void)stream;
    int ch;
    while ((ch = uart_read_byte()) < 0) {
        /* spin until a byte arrives; low CPU usage if you sleep elsewhere */
    }
    /* Normalize CR to LF if a terminal sends CR only */
    if (ch == '\r') ch = '\n';
    return ch;
}

/* ---------- Config / init ---------- */
static void uart_config (unsigned int ubrr) {
    UBRR0H = (unsigned char)(ubrr >> 8);				   // Baud rate register high byte
    UBRR0L = (unsigned char)(ubrr);				  		   // Baud rate register low byte					
    UCSR0A |= (1 << U2X0);								   //Double speed (reduces baud error)
    UCSR0B = (1 << RXEN0) | (1 << TXEN0) | (1 << RXCIE0);  //Enable RX, TX and RX complete interrupt
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00); 			   // 8 data bits, no parity, 1 stop bit
}

/* stdio FILEs */
FILE uart_output = FDEV_SETUP_STREAM(uart_putchar, NULL, _FDEV_SETUP_WRITE);
FILE uart_input  = FDEV_SETUP_STREAM(NULL, uart_getchar, _FDEV_SETUP_READ);

void uart_init(unsigned int ubrr) {
   cli();

    uart_config(ubrr);
    uart_flush_rx();

    /* Hook stdio */
    stdout = &uart_output;
    stdin  = &uart_input;

	sei();
}

/* ---------- ISR ---------- */
ISR(USART0_RX_vect) {
    uint8_t data = UDR0;                // read ASAP to clear RXC
    uint8_t next = _rb_next(rx_head);

    if (next != rx_tail) {
        rxbuf[rx_head] = data;
        rx_head = next;
    } else {
        /* Buffer full: drop the byte (or advance tail to overwrite oldest) */
        /* Overwrite-oldest policy (uncomment if preferred):
           rxbuf[rx_head] = data;
           rx_head = next;
           rx_tail = _rb_next(rx_tail);
        */
    }
}
