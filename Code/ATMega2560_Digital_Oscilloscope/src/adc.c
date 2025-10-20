#include "adc.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>

/* ---------- Private state ---------- */
static volatile uint16_t s_result;
static volatile uint8_t  s_last_channel;
static volatile uint8_t  s_ready;           /* 0 = no new sample, 1 = new sample available */
static ADC_Callback      s_cb = 0;

/* ---------- Helpers ---------- */
static void adc_disable_digital_buffer(uint8_t channel) {
    if (channel < 8) {
        DIDR0 |= _BV(channel);          // ADC0D..ADC7D
    } else if (channel < 16) {
        DIDR2 |= _BV(channel - 8);      // ADC8D..ADC15D
    }
}

/* Map your GPIO_Pin (from gpio.h) to ADC channel (0..15).
 * On Mega: A0..A7 = PF0..PF7 => ADC0..ADC7; A8..A15 = PK0..PK7 => ADC8..ADC15. */
static int8_t adc_channel_from_pin(GPIO_Pin p) {
    if (p.port == &PORTF) return (int8_t)p.bit;          // 0..7
    if (p.port == &PORTK) return (int8_t)(8 + p.bit);    // 8..15
    return -1;
}

/* Select channel safely when ADC is idle. */
static inline void adc_select_channel(uint8_t channel) {
    if (channel & 0x08) ADCSRB |=  _BV(MUX5);  // channels 8..15
    else                ADCSRB &= ~_BV(MUX5);  // channels 0..7
    /* Keep REFS1:0 and ADLAR; set low 3 MUX bits (0..7 within bank). */
    ADMUX = (ADMUX & 0xE0) | (channel & 0x07);
}

/* ---------- Public API ---------- */
void ADC_set_reference(ADC_Reference ref) {
    /* Keep ADLAR=0 (right-adjust). Only change REFS1:0. */
    ADMUX = (ADMUX & ~(_BV(REFS1) | _BV(REFS0))) | ref;
    /* Note: first sample after changing Vref may be inaccurate. */
}

void ADC_init(ADC_Reference ref, uint8_t prescaler_bits) {
    /* Power up ADC clock domain. */
    PRR0 &= ~_BV(PRADC);

    /* Reference + right-adjust + clear MUX bits initially. */
    ADMUX  = (ref & (_BV(REFS1) | _BV(REFS0))); /* ADLAR=0 */
    ADCSRB = 0;                                 /* MUX5=0, ADTS=0 (no auto trigger) */

    /* Prescaler & enable; also enable interrupt (ADIE). */
    ADCSRA = _BV(ADEN) | _BV(ADIE) |
             (prescaler_bits & (_BV(ADPS2)|_BV(ADPS1)|_BV(ADPS0)));

    /* Clear any pending flag before first start. */
    ADCSRA |= _BV(ADIF);

    s_ready = 0;
}

bool ADC_start(uint8_t channel) {
    if (channel > 15) return false;

    /* If a conversion is ongoing, don't disturb ADMUX (datasheet: ADMUX is locked during conversion). */
    if (ADCSRA & _BV(ADSC)) return false;

    adc_disable_digital_buffer(channel);
    adc_select_channel(channel);

    s_last_channel = channel;
    s_ready = 0;

    /* Clear completion flag; then start single conversion. */
    ADCSRA |= _BV(ADIF);
    ADCSRA |= _BV(ADSC);
    return true;
}

bool ADC_start_pin(GPIO_Pin p) {
    int8_t ch = adc_channel_from_pin(p);
    if (ch < 0) return false;
    return ADC_start((uint8_t)ch);
}

bool ADC_busy(void) {
    return (ADCSRA & _BV(ADSC)) != 0;
}

bool ADC_result_ready(void) {
    return s_ready != 0;
}

bool ADC_get(uint16_t *out) {
    uint16_t v;
    uint8_t have = 0;

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        if (s_ready) {
            v = s_result;
            s_ready = 0;         /* consume the sample */
            have = 1;
        }
    }

    if (have && out) *out = v;
    return (bool)have;
}

void ADC_set_callback(ADC_Callback cb) {
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        s_cb = cb;
    }
}

/* ---------- Compatibility (blocking wrappers) ----------
 * These block until a fresh result arrives via the ISR.
 * REQUIREMENT: global interrupts must be enabled (sei()). */
uint16_t ADC_read(uint8_t channel) {
    while (!ADC_start(channel)) {
        /* Busy; wait for current conversion to finish. */
        /* If interrupts are disabled, this would deadlock — ensure sei() is used by the caller. */
    }
    while (!ADC_result_ready()) { /* wait for ISR to complete */ }
    uint16_t v = 0;
    (void)ADC_get(&v);
    return v;
}

uint16_t ADC_read_pin(GPIO_Pin p) {
    int8_t ch = adc_channel_from_pin(p);
    if (ch < 0) return 0xFFFFu;
    return ADC_read((uint8_t)ch);
}

/* ---------- ISR ---------- */
ISR(ADC_vect) {
    /* Read result (ADCL first, then ADCH). Using ADCW performs correct sequence. */
    uint16_t v = ADCW;

    s_result = v;
    s_ready  = 1;

    /* Clear interrupt flag explicitly (write 1 to ADIF). */
    ADCSRA |= _BV(ADIF);

    /* Optional user callback (keep it very short/ISR-safe). */
    if (s_cb) {
        s_cb(v, s_last_channel);
    }
}
