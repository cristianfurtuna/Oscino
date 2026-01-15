#ifndef GPIO_H
#define GPIO_H

#include <avr/io.h>
#include <stdint.h>

typedef struct
{
    volatile uint8_t *ddr;
    volatile uint8_t *port;
    volatile uint8_t *pinReg;
    uint8_t bit;
} GPIO_Pin;

static void GPIO_set_output(GPIO_Pin p) { *(p.ddr) |= _BV(p.bit); }
static void GPIO_set_input(GPIO_Pin p) { *(p.ddr) &= ~_BV(p.bit); }
static void GPIO_set_high(GPIO_Pin p) { *(p.port) |= _BV(p.bit); }
static void GPIO_set_low(GPIO_Pin p) { *(p.port) &= ~_BV(p.bit); }
static void GPIO_toggle(GPIO_Pin p) { *(p.port) ^= _BV(p.bit); }
static uint8_t GPIO_read(GPIO_Pin p) { return (*(p.pinReg) & _BV(p.bit)) != 0; }

#define GPIO_PIN(L, N) ((GPIO_Pin){&DDR##L, &PORT##L, &PIN##L, P##L##N})

// Pin definitions for Arduino Mega 2560
#define ONBOARD_LED GPIO_PIN(B, 7)

#define D0 GPIO_PIN(E, 0)  // USART0_RX
#define D1 GPIO_PIN(E, 1)  // USART0_TX
#define D2 GPIO_PIN(E, 4)  // PWM2
#define D3 GPIO_PIN(E, 5)  // PWM3
#define D4 GPIO_PIN(G, 5)  // PWM4
#define D5 GPIO_PIN(E, 3)  // PWM5
#define D6 GPIO_PIN(H, 3)  // PWM6
#define D7 GPIO_PIN(H, 4)  // PWM7
#define D8 GPIO_PIN(H, 5)  // PWM8
#define D9 GPIO_PIN(H, 6)  // PWM9
#define D10 GPIO_PIN(B, 4) // PWM10
#define D11 GPIO_PIN(B, 5) // PWM11
#define D12 GPIO_PIN(B, 6) // PWM12
#define D13 GPIO_PIN(B, 7) // PWM13 / LED
#define D14 GPIO_PIN(J, 1) // USART3_TX
#define D15 GPIO_PIN(J, 0) // USART3_RX
#define D16 GPIO_PIN(H, 1) // USART2_TX
#define D17 GPIO_PIN(H, 0) // USART2_RX
#define D18 GPIO_PIN(D, 3) // USART1_TX
#define D19 GPIO_PIN(D, 2) // USART1_RX
#define D20 GPIO_PIN(D, 1) // I2C_SDA
#define D21 GPIO_PIN(D, 0) // I2C_SCL

#define D22 GPIO_PIN(A, 0)
#define D23 GPIO_PIN(A, 1)
#define D24 GPIO_PIN(A, 2)
#define D25 GPIO_PIN(A, 3)
#define D26 GPIO_PIN(A, 4)
#define D27 GPIO_PIN(A, 5)
#define D28 GPIO_PIN(A, 6)
#define D29 GPIO_PIN(A, 7)

#define D30 GPIO_PIN(C, 7)
#define D31 GPIO_PIN(C, 6)
#define D32 GPIO_PIN(C, 5)
#define D33 GPIO_PIN(C, 4)
#define D34 GPIO_PIN(C, 3)
#define D35 GPIO_PIN(C, 2)
#define D36 GPIO_PIN(C, 1)
#define D37 GPIO_PIN(C, 0)

#define D38 GPIO_PIN(D, 7)
#define D39 GPIO_PIN(G, 2)
#define D40 GPIO_PIN(G, 1)
#define D41 GPIO_PIN(G, 0)

#define D42 GPIO_PIN(L, 7)
#define D43 GPIO_PIN(L, 6)
#define D44 GPIO_PIN(L, 5) // PWM (OC5C)
#define D45 GPIO_PIN(L, 4) // PWM (OC5B)
#define D46 GPIO_PIN(L, 3) // PWM (OC5A)
#define D47 GPIO_PIN(L, 2)
#define D48 GPIO_PIN(L, 1)
#define D49 GPIO_PIN(L, 0)

#define D50 GPIO_PIN(B, 3) // SPI_MISO
#define D51 GPIO_PIN(B, 2) // SPI_MOSI
#define D52 GPIO_PIN(B, 1) // SPI_SCK
#define D53 GPIO_PIN(B, 0) // SPI_SS

#define A0 GPIO_PIN(F, 0)
#define A1 GPIO_PIN(F, 1)
#define A2 GPIO_PIN(F, 2)
#define A3 GPIO_PIN(F, 3)
#define A4 GPIO_PIN(F, 4)
#define A5 GPIO_PIN(F, 5)
#define A6 GPIO_PIN(F, 6)
#define A7 GPIO_PIN(F, 7)
#define A8 GPIO_PIN(K, 0)
#define A9 GPIO_PIN(K, 1)
#define A10 GPIO_PIN(K, 2)
#define A11 GPIO_PIN(K, 3)
#define A12 GPIO_PIN(K, 4)
#define A13 GPIO_PIN(K, 5)
#define A14 GPIO_PIN(K, 6)
#define A15 GPIO_PIN(K, 7)

#endif // GPIO_H
