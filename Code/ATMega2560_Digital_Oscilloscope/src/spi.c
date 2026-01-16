#include <stdio.h>
#include <avr/io.h>
#include "spi.h"
#include "uart.h"

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

// Read data from ADC
uint8_t SPI_Read_MCP3008_8bit(uint8_t channel)
{
    // CS Low
    PORTB &= ~(1 << PB0);

    // Send Start Byte (0x01)
    SPDR = 0x01;
    while (!(SPSR & (1 << SPIF)))
        ;

    // Send config byte si select channel
    SPDR = 0x80 | (channel << 4);
    while (!(SPSR & (1 << SPIF)))
        ;
    uint8_t high = SPDR; // MSB data

    // Send dummy byte pentru a primii ultimii biti ai conversiei
    SPDR = 0x00;
    while (!(SPSR & (1 << SPIF)))
        ;
    uint8_t low = SPDR; // LSB data

    // CS High
    PORTB |= (1 << PB0);

    // Concatenam high si low
    uint16_t result = ((high & 0x03) << 8) | low;
    // Shiftare la dreapta pentru a pastra doar 1 byte
    // La frecvente mari, de regula ultimii 2 biti sunt zgomotosi
    return (uint8_t)(result >> 2);
}
