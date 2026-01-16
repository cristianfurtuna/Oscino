#define BAUD 2000000UL // desired BAUD rate

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdio.h>
#include <stdint.h>
#include "uart.h"
#include "gpio.h"
#include "avr/portpins.h"
#include "pwm.h"
#include "avr/sleep.h"
#include "millis.h"
#include "adc.h"
#include "spi.h"

#define PWM_FREQ 200 // Hz
#define BATCH_SIZE 600

volatile uint8_t button_pressed = 1;

int main(void)
{

	GPIO_set_output(ONBOARD_LED); // onboard LED
	GPIO_set_output(D11);		  // D11 as output

	uart_init(MYUBRR);
	SPI_MasterInit();
	millis_init();

	// Generate a square signal on pin D11
	PWM_init();
	PWM_attach(D11);
	ADC_init(ADC_REF_AVCC, ADC_PRESCALER_128);

	set_sleep_mode(SLEEP_MODE_IDLE); // Set sleep mode to power down

	EICRA |= (1 << ISC00); // Any logical change on INT0 generates interrupt
	EIMSK |= (1 << INT0);  // Enable INT0
	sei();				   // Enable global interrupts

	while (1)
	{
		// Modify duty cycle of pwm with a potentiometer
		static uint32_t last_pwm_update = 0;

		if (millis_now() - last_pwm_update >= 50)
		{
			uint16_t pot = ADC_read_pin(A0); // 0–1023
			uint32_t duty = (uint32_t)pot * PWM_get_max(D11) / 1023;

			PWM_write_raw(D11, duty); // hardware PWM
			last_pwm_update = millis_now();
		}

		cli(); // clear interrupts for full focus on sending ADC data
		// Send batch sync bytes 0xAA start byte, 0x55 stop byte, useful for python script
		while (!(UCSR0A & (1 << UDRE0)))
			;
		UDR0 = 0xAA;
		while (!(UCSR0A & (1 << UDRE0)))
			;
		UDR0 = 0x55;

		// Sending BATCH_SIZE = 600 bytes; total number of bits sent = estimated 6000 bits per for
		// Adding the start and stop bits to a byte -> 10 bits per batch
		// 6020 if we take into consideration the sync bytes for a batch
		for (uint16_t i = 0; i < BATCH_SIZE; i++)
		{
			// Read channel 0 from ADC
			uint8_t val = SPI_Read_MCP3008_8bit(0);

			// Send byte through UART
			while (!(UCSR0A & (1 << UDRE0)))
				;		// Wait for buffer to empty
			UDR0 = val; // Send the data
		}

		sei(); // enable interrupts
		_delay_us(100);
	}
}
