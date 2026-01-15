#define BAUD 2000000UL // desired BAUD rate (was 115200)

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
	GPIO_set_output(D28);		  // D28 as output
	GPIO_set_output(D11);		  // D11 as output

	uart_init(MYUBRR);
	SPI_MasterInit();
	// SPI_SlaveInit();
	millis_init();

	PWM_init();
	PWM_attach(D11);
	ADC_init(ADC_REF_AVCC, ADC_PRESCALER_128);

	set_sleep_mode(SLEEP_MODE_IDLE); // Set sleep mode to power down

	EICRA |= (1 << ISC00); // Any logical change on INT0 generates interrupt
	EIMSK |= (1 << INT0);  // Enable INT0
	sei();				   // Enable global interrupts

	// uint8_t duty = 0;

	while (1)
	{
		// sleep_enable();
		// sleep_cpu();
		// sleep_disable();
		static uint32_t last_pwm_update = 0;

		if (millis_now() - last_pwm_update >= 50)
		{
			uint16_t pot = ADC_read_pin(A0); // 0–1023
			uint32_t duty = (uint32_t)pot * PWM_get_max(D11) / 1023;

			PWM_write_raw(D11, duty); // hardware PWM
			last_pwm_update = millis_now();
		}

		// uart_putchar(0xAA, stdout);		  // SYNC BYTE 1
		// uart_putchar(0x55, stdout);		  // SYNC BYTE 2
		cli();
		while (!(UCSR0A & (1 << UDRE0)))
			;
		UDR0 = 0xAA;
		while (!(UCSR0A & (1 << UDRE0)))
			;
		UDR0 = 0x55;
		/*for (uint16_t i = 0; i < 64; i++) // capture 64 samples per batch; 64 x 10 = 640bits = 80bytes
		{
			readADC_packed(0);
			//_delay_us(50);
		}
		ADC_pack_flush(); // send any leftover bits and reset the buffer after sending a batch */

		for (uint16_t i = 0; i < BATCH_SIZE; i++)
		{
			// Citim de la MCP3008 (Canal 0) pe 8 biți
			uint8_t val = SPI_Read_MCP3008_8bit(0);

			// Trimitem byte-ul pe Serial
			while (!(UCSR0A & (1 << UDRE0)))
				;		// Așteptăm buffer gol
			UDR0 = val; // Trimitem data
		}

		sei();
		_delay_us(100);

		//_delay_ms(500);
		//   uint16_t raw = ADC_read_pin(A15);
		//   uint16_t mv = ADC_to_millivolts(raw, 5000);
		//    Test SPI communication
		// uint16_t spi_data = readADC(0); // read channel 0
		// printf("Data received: %u\r\n", spi_data);
		//_delay_ms(500);
		/*_delay_ms(500);
		printf("A15: %u (≈ %u mV)\r\n", raw, mv);
		_delay_ms(500);
		printf("Button state: %d \n", button_pressed);
		if (!button_pressed)
		{
			PWM_attach(D11);
			for (duty = 0; duty < 100; duty++)
			{
				PWM_write_percent(D11, duty); // duty cycle
				_delay_ms(5);
			}

			GPIO_set_high(D28);
			printf("LED on \n");
			_delay_ms(500);

			// Turn LED off
			for (duty = 100; duty > 0; duty--)
			{
				PWM_write_percent(D11, duty); // duty cycle
				_delay_ms(5);
			}
			PWM_write_percent(D11, 0); // duty cycle
			PWM_detach(D11);

			GPIO_set_low(D28);
			printf("LED off \n");
			_delay_ms(500);
		} */
	}
}

/*ISR(INT0_vect) // interrupt priority hardcoded (hi: 0 lo:7) (external interrupts)
{
	if (GPIO_read(D21))
		button_pressed = 1;
	else
		button_pressed = 0;
}

ISR(TIMER1_COMPA_vect)
{
	GPIO_toggle(D11);
} */
