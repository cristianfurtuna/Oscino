#include <stdio.h>
#include <avr/io.h>
#include "spi.h"
#include "uart.h"

static uint8_t pack_buf = 0;
static uint8_t pack_bits = 0;

void SPI_MasterInit(void)
{
    // Set MOSI, SS and SCK output, all others input
    DDRB |= (1 << PB2) | (1 << PB1) | (1 << PB0); // MOSI - PB2, SCK - PB1
    // Enable SPI, Master, set clock rate fck/16 (was 1 << SPR0)
    PORTB |= (1 << PB0);                           // Keep board in Master mode
    SPCR = (1 << SPE) | (1 << MSTR) | (1 << SPR0); // SPCR - SPI control register
    SPSR |= (1 << SPI2X);
}

uint8_t SPI_MasterTransmit(uint8_t data)
{
    // Start transmission; load byte into data register
    SPDR = data; // SPDR - SPI data register
    // Wait for transmission to complete
    while (!(SPSR & (1 << SPIF)))
        ;        // SPSR - SPI status register ; SPIF - SPI Interrupt Flag
    return SPDR; // read received byte after transmission
}

void SPI_SlaveInit(void)
{
    // SET MISO output, all others input
    DDRB |= (1 << PB3); // MISO - PB3
    // Enable SPI
    SPCR = (1 << SPE); // SPCR - SPI control register
}

uint8_t SPI_SlaveReceive(void)
{
    // Wait for reception complete
    while (!(SPSR & (1 << SPIF)))
        ;        // SPSR - SPI status register ; SPIF - SPI Interrupt Flag
    return SPDR; // SPDR - SPI data register
}

/*uint16_t readADC(uint8_t channel)
{
    PORTB &= ~(1 << PB0);                                       // Set CS low
    SPI_MasterTransmit(0x01);                                   // Send start bit to ADC
    uint8_t high = SPI_MasterTransmit((0x80 | (channel << 4))); // config bits: 1 S C2 C1 C0 x x x
                                                                // 1 -single ended mode; S -start bit; C2 C1 C0 - channel select
    uint8_t low = SPI_MasterTransmit(0x00);                     // low byte
    PORTB |= (1 << PB0);                                        // Set CS high
    return ((high & 0x03) << 8) | low;                          // data received across two bytes, 1 byte SPI not enough
} */

/*void readADC_packed(uint8_t channel)
{
    PORTB &= ~(1 << PB0);                                       // Set CS low
    SPI_MasterTransmit(0x01);                                   // Send start bit to ADC
    uint8_t high = SPI_MasterTransmit((0x80 | (channel << 4))); // config bits: 1 S C2 C1 C0 x x x
                                                                // 1 -single ended mode; S -start bit; C2 C1 C0 - channel select
    uint8_t low = SPI_MasterTransmit(0x00);                     // low byte
    PORTB |= (1 << PB0);                                        // Set CS high

    uint16_t sample = ((high & 0x03) << 8) | low;

    for (uint8_t i = 0; i < 10; i++)
    {
        uint8_t bit = (sample >> i) & 1; // extract each bit

        pack_buf |= (bit << pack_bits); // insert into pack_buf in current position
        pack_bits++;

        if (pack_bits == 8)
        {
            uart_putchar(pack_buf, stdout); // once we have 8 bits, send one byte over UART and reset buffer
            pack_buf = 0;
            pack_bits = 0;
        }
    }
}

void ADC_pack_flush(void)
{
    if (pack_bits > 0)
    {
        uart_putchar(pack_buf, stdout);
        pack_buf = 0;
        pack_bits = 0;
    }
}  */

// Funcție optimizată: face toată tranzacția "inline"
uint8_t SPI_Read_MCP3008_8bit(uint8_t channel)
{
    // 1. CS Low
    PORTB &= ~(1 << PB0);

    // 2. Trimite Start Bit (0x01)
    SPDR = 0x01;
    while (!(SPSR & (1 << SPIF)))
        ;

    // 3. Trimite Config (Single Ended) -> Primește High Byte
    SPDR = 0x80 | (channel << 4);
    while (!(SPSR & (1 << SPIF)))
        ;
    uint8_t high = SPDR;

    // 4. Trimite Dummy -> Primește Low Byte
    SPDR = 0x00;
    while (!(SPSR & (1 << SPIF)))
        ;
    uint8_t low = SPDR;

    // 5. CS High
    PORTB |= (1 << PB0);

    // 6. Conversie rapidă:
    // Combinăm cei 10 biți: ((high & 0x03) << 8) | low
    // Shiftăm dreapta cu 2 pentru a păstra cei mai importanți 8 biți (0-255)
    uint16_t result = ((high & 0x03) << 8) | low;

    return (uint8_t)(result >> 2);
}
