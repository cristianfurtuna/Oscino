#ifndef SPI_H
#define SPI_H

#include <avr/io.h>

void SPI_MasterInit(void);
uint8_t SPI_MasterTransmit(uint8_t data);
void SPI_SlaveInit(void);
uint8_t SPI_SlaveReceive(void);
// void readADC_packed(uint8_t channel);
//  uint16_t readADC(uint8_t channel);
// void ADC_pack_flush(void);
uint8_t SPI_Read_MCP3008_8bit(uint8_t channel);

#endif