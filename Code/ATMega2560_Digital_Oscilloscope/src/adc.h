#ifndef ADC_H
#define ADC_H

#include <avr/io.h>
#include <stdint.h>
#include <stdbool.h>
#include "gpio.h"

/* ---- Reference selections (ADMUX: REFS1:REFS0) ----
 * Datasheet: Voltage Reference Selection */
typedef enum {
    ADC_REF_AREF           = 0,                          // AREF pin, internal ref off
    ADC_REF_AVCC           = _BV(REFS0),                 // AVCC with external cap at AREF
    ADC_REF_INTERNAL_1V1   = _BV(REFS1),                 // Internal 1.1V
    ADC_REF_INTERNAL_2V56  = _BV(REFS1) | _BV(REFS0)     // Internal 2.56V
} ADC_Reference;

/* ---- Prescaler selections (ADCSRA: ADPS2:ADPS0) ----
 * Datasheet: ADC Prescaler Selections */
#define ADC_PRESCALER_2     (_BV(ADPS0))                           /* 2   */
#define ADC_PRESCALER_4     (_BV(ADPS1))                           /* 4   */
#define ADC_PRESCALER_8     (_BV(ADPS1) | _BV(ADPS0))              /* 8   */
#define ADC_PRESCALER_16    (_BV(ADPS2))                           /* 16  */
#define ADC_PRESCALER_32    (_BV(ADPS2) | _BV(ADPS0))              /* 32  */
#define ADC_PRESCALER_64    (_BV(ADPS2) | _BV(ADPS1))              /* 64  */
#define ADC_PRESCALER_128   (_BV(ADPS2) | _BV(ADPS1) | _BV(ADPS0)) /* 128 */

/* ---------- Init / reference ---------- */

/* Initialize the ADC in single-conversion, interrupt-driven mode.
 * Typical: ref=ADC_REF_AVCC, prescaler=ADC_PRESCALER_128 for 16 MHz. */
void ADC_init(ADC_Reference ref, uint8_t prescaler_bits);

/* Change reference at runtime (first result after switching may be inaccurate). */
void ADC_set_reference(ADC_Reference ref);

/* ---------- Non-blocking API ---------- */

/* Start a conversion on ADC channel [0..15]. Returns false if ADC is busy or ch invalid. */
bool ADC_start(uint8_t channel);

/* Start a conversion by GPIO pin (A0..A15). Returns false if not an analog pin or ADC busy. */
bool ADC_start_pin(GPIO_Pin p);

/* True while ADC hardware is converting. */
bool ADC_busy(void);

/* True if a fresh result is available (since last ADC_get()). */
bool ADC_result_ready(void);

/* If a fresh result is available, writes it to *out and returns true; otherwise returns false.
 * This call is atomic w.r.t. the ISR. */
bool ADC_get(uint16_t *out);

/* Optional: set a callback to be invoked from the ADC ISR when a conversion completes.
 * Keep it extremely short/ISR-safe. Pass NULL to disable. */
typedef void (*ADC_Callback)(uint16_t value, uint8_t channel);
void ADC_set_callback(ADC_Callback cb);

/* ---------- Compatibility (blocking wrappers) ---------- */
/* These block until a result is ready, but they still use the ISR under the hood. */
uint16_t ADC_read(uint8_t channel);
uint16_t ADC_read_pin(GPIO_Pin p);

/* Helper to convert raw ADC code to millivolts (0..1023 maps to 0..Vref_mV). */
static inline uint16_t ADC_to_millivolts(uint16_t code, uint16_t vref_mV) {
    return (uint32_t)code * vref_mV / 1023u;
}

#endif /* ADC_H */
