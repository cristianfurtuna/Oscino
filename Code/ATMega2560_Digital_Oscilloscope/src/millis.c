#include "millis.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>


#define OCR5A_1KHZ             ((F_CPU/64UL/1000UL) - 1UL) //1kHz at prescaler 64

static volatile uint32_t g_ms = 0;

void millis_init(void)
{
    /* Ensure Timer5 is powered (on MCUs with PRR1) */
#ifdef PRR1
    PRR1 &= (uint8_t)~_BV(PRTIM5);
#endif

    /* Stop timer while configuring */
    TCCR5A = 0;
    TCCR5B = 0;
    TCCR5C = 0;

    /* CTC mode (WGM52=1), compare A at 1 kHz */
    OCR5A  = (uint16_t)OCR5A_1KHZ;
    TCNT5  = 0;

    /* Clear any pending flag; enable compare A interrupt */
    TIFR5  = _BV(OCF5A);
    TIMSK5 = _BV(OCIE5A);

    /* Start timer: CTC + prescaler /64 (CS51 | CS50) */
    TCCR5B = _BV(WGM52) | _BV(CS51) | _BV(CS50);
}

uint32_t millis_now(void)
{
    uint32_t t;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { t = g_ms; }
    return t;
}

void delay_ms(uint32_t ms)
{
    uint32_t until = millis_now() + ms;
    while ((int32_t)(millis_now() - until) < 0) {
        /* Busy-wait; for lower power you could sleep in IDLE here */
    }
}

/* 1 kHz tick ISR */
ISR(TIMER5_COMPA_vect)
{
    g_ms++;
}
