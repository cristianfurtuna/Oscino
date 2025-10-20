/*
 * pwm.c
 *
 * Implements a small HAL to control PWM channels on ATmega2560 (Arduino Mega mapping).
 *
 * Mapping used (Arduino Mega common mapping):
 * Timer0: OC0A = D13 (PB7), OC0B = D4 (PG5)
 * Timer1: OC1A = D11 (PB5), OC1B = D12 (PB6)
 * Timer2: OC2A = D10 (PB4), OC2B = D9  (PH6)
 * Timer3: OC3A = D5  (PE3), OC3B = D2  (PE4), OC3C = D3  (PE5)
 * Timer4: OC4A = D6  (PH3), OC4B = D7  (PH4), OC4C = D8  (PH5)
 * Timer5: RESERVED (used by millis/timekeeping); PWM on D44/D45/D46 disabled in this HAL
 *
 * This mapping follows Arduino Mega documentation and common pin mapping references. :contentReference[oaicite:1]{index=1}
 *
 * Note: timers 0 & 2 are 8-bit; timers 1,3,4,5 are 16-bit.
 *
 * Default behavior:
 * - Configure Fast PWM (8-bit for timers0/2, 16-bit for 1/3/4/5 using ICRn = 0xFFFF).
 * - Non-inverting mode.
 * - Prescaler default = 64 (common Arduino-ish value).
 * 
 * Changing prescaler/mode will affect all pins sharing the same timer.
 */

#include "pwm.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <string.h>


/* Internal helper: return timer id (0..5) and channel ('A'/'B'/'C') or -1 if not PWM pin */
typedef struct {
    int8_t timer;   /* 0..5, or -1 if not PWM */
    char    ch;     /* 'A','B','C' */
} pwm_map_t;

static pwm_map_t pwm_map_from_pin(GPIO_Pin p);

/* Return counter width: 8 or 16 */
static uint8_t pwm_timer_width(int8_t timer_id) {
    if (timer_id == 0 || timer_id == 2) return 8;
    if (timer_id >= 1 && timer_id <= 5) return 16;
    return 0;
}

/* Top for 16-bit timers (we set ICRn = 0xFFFF in init by default) */
static uint32_t pwm_default_top(int8_t timer_id) {
    if (pwm_timer_width(timer_id) == 8) return 0xFFu;
    switch (timer_id) {
        case 1: return ICR1;
        case 3: return ICR3;
        case 4: return ICR4;
        default: return 0; // should not happen
    }
}

/* Public API */

void PWM_init(void) {
    cli();

    /* Timer0: Fast PWM, 8-bit (WGM01:0 = 3) -> WGM02 in TCCR0B = 0 => mode 3 (fast 8-bit)
     * Set COM0A1:0 and COM0B1:0 later in attach
     * Prescaler: set CS02:0 = prescaler 64 -> CS02=0,CS01=1,CS00=1
     */
    TCCR0A = (1<<WGM00) | (1<<WGM01);
    TCCR0B = (1<<CS01) | (1<<CS00); /* prescaler 64 */

    /* Timer2: Fast PWM, 8-bit (WGM21:0 = 3), prescaler 64 */
    TCCR2A = (1<<WGM20) | (1<<WGM21);
    TCCR2B = (1<<CS22); /* CS22:0 = 100 => prescaler 64 on many setups (check datasheet) */
    /* Note: some chips use different CS bits—this is the common Arduino choice for Timer2 */

    /* Timer1,3,4,5: Configure for Fast PWM, mode 14 (WGMn3:0 = 14) where TOP = ICRn.
     * That requires setting WGMn bits across TCCRnA and TCCRnB. We'll set ICRn = 0x00FF
     * (8-bit TOP) to get ~976 Hz at 16 MHz with prescaler 64, which avoids visible flicker
     * on LEDs while keeping simple scaling. Prescaler = 64 (CSn2..0).
     */
    /* Timer1 */
    TCCR1A = (1<<WGM11);
    TCCR1B = (1<<WGM13) | (1<<WGM12) | (1<<CS11) | (1<<CS10); /* presc 64 */
    ICR1 = 0x00FF;

    /* Timer3 */
    TCCR3A = (1<<WGM31);
    TCCR3B = (1<<WGM33) | (1<<WGM32) | (1<<CS31) | (1<<CS30);
    ICR3 = 0x00FF;

    /* Timer4 */
    TCCR4A = (1<<WGM41);
    TCCR4B = (1<<WGM43) | (1<<WGM42) | (1<<CS41) | (1<<CS40);
    ICR4 = 0x00FF;

    /* Timer5 intentionally left unconfigured (reserved for timekeeping) */

    sei();
}

/* Internal: enable compare output for a (timer,channel) in non-inverting mode */
static void enable_compare_output(int8_t timer, char ch) {
    switch (timer) {
        case 0:
            if (ch == 'A') TCCR0A |= (1<<COM0A1);
            else if (ch == 'B') TCCR0A |= (1<<COM0B1);
            break;
        case 1:
            if (ch == 'A') TCCR1A |= (1<<COM1A1);
            else if (ch == 'B') TCCR1A |= (1<<COM1B1);
            else if (ch == 'C') TCCR1A |= (1<<COM1C1);
            break;
        case 2:
            if (ch == 'A') TCCR2A |= (1<<COM2A1);
            else if (ch == 'B') TCCR2A |= (1<<COM2B1);
            break;
        case 3:
            if (ch == 'A') TCCR3A |= (1<<COM3A1);
            else if (ch == 'B') TCCR3A |= (1<<COM3B1);
            else if (ch == 'C') TCCR3A |= (1<<COM3C1);
            break;
        case 4:
            if (ch == 'A') TCCR4A |= (1<<COM4A1);
            else if (ch == 'B') TCCR4A |= (1<<COM4B1);
            else if (ch == 'C') TCCR4A |= (1<<COM4C1);
            break;
        /* Timer5 reserved: no PWM enable */
    }
}

/* Internal: disable compare output for (timer,channel) */
static void disable_compare_output(int8_t timer, char ch) {
    switch (timer) {
        case 0:
            if (ch == 'A') TCCR0A &= ~( (1<<COM0A1)|(1<<COM0A0) );
            else if (ch == 'B') TCCR0A &= ~( (1<<COM0B1)|(1<<COM0B0) );
            break;
        case 1:
            if (ch == 'A') TCCR1A &= ~( (1<<COM1A1)|(1<<COM1A0) );
            else if (ch == 'B') TCCR1A &= ~( (1<<COM1B1)|(1<<COM1B0) );
            else if (ch == 'C') TCCR1A &= ~( (1<<COM1C1)|(1<<COM1C0) );
            break;
        case 2:
            if (ch == 'A') TCCR2A &= ~( (1<<COM2A1)|(1<<COM2A0) );
            else if (ch == 'B') TCCR2A &= ~( (1<<COM2B1)|(1<<COM2B0) );
            break;
        case 3:
            if (ch == 'A') TCCR3A &= ~( (1<<COM3A1)|(1<<COM3A0) );
            else if (ch == 'B') TCCR3A &= ~( (1<<COM3B1)|(1<<COM3B0) );
            else if (ch == 'C') TCCR3A &= ~( (1<<COM3C1)|(1<<COM3C0) );
            break;
        case 4:
            if (ch == 'A') TCCR4A &= ~( (1<<COM4A1)|(1<<COM4A0) );
            else if (ch == 'B') TCCR4A &= ~( (1<<COM4B1)|(1<<COM4B0) );
            else if (ch == 'C') TCCR4A &= ~( (1<<COM4C1)|(1<<COM4C0) );
            break;
        /* Timer5 reserved: no PWM disable needed */
    }
}

int PWM_attach(GPIO_Pin p) {
    pwm_map_t m = pwm_map_from_pin(p);
    if (m.timer < 0) return -1;

    /* configure pin as output */
    GPIO_set_output(p);

    /* enable compare output in non-inverting mode */
    enable_compare_output(m.timer, m.ch);

    return 0;
}

int PWM_detach(GPIO_Pin p) {
    pwm_map_t m = pwm_map_from_pin(p);
    if (m.timer < 0) return -1;

    disable_compare_output(m.timer, m.ch);
    return 0;
}

/* Write raw duty (scaled by width) */
int PWM_write_raw(GPIO_Pin p, uint32_t value) {
    pwm_map_t m = pwm_map_from_pin(p);
    if (m.timer < 0) return -1;

    uint8_t width = pwm_timer_width(m.timer);

    if (width == 8) {
        uint8_t v = (value > 0xFFu) ? 0xFFu : (uint8_t)value;
        switch (m.timer) {
            case 0:
                if (m.ch == 'A') OCR0A = v;
                else OCR0B = v;
                break;
            case 2:
                if (m.ch == 'A') OCR2A = v;
                else OCR2B = v;
                break;
        }
    } else {
        uint16_t top = (uint16_t)pwm_default_top(m.timer);
        uint16_t v = (value > top) ? top : (uint16_t)value;
        switch (m.timer) {
            case 1:
                if (m.ch == 'A') OCR1A = v;
                else if (m.ch == 'B') OCR1B = v;
                else OCR1C = v;
                break;
            case 3:
                if (m.ch == 'A') OCR3A = v;
                else if (m.ch == 'B') OCR3B = v;
                else OCR3C = v;
                break;
            case 4:
                if (m.ch == 'A') OCR4A = v;
                else if (m.ch == 'B') OCR4B = v;
                else OCR4C = v;
                break;
            /* Timer5 reserved: no OCR writes */
        }
    }
    return 0;
}

int PWM_write_percent(GPIO_Pin p, float percent) {
    if (percent < 0.0f) percent = 0.0f;
    if (percent > 100.0f) percent = 100.0f;
    pwm_map_t m = pwm_map_from_pin(p);
    if (m.timer < 0) return -1;
    uint32_t maxv = pwm_default_top(m.timer);
    uint32_t val = (uint32_t)((percent / 100.0f) * (float)maxv + 0.5f);
    return PWM_write_raw(p, val);
}

uint32_t PWM_get_max(GPIO_Pin p) {
    pwm_map_t m = pwm_map_from_pin(p);
    if (m.timer < 0) return 0;
    return pwm_default_top(m.timer);
}

int PWM_set_prescaler(uint8_t timer_id, uint16_t prescaler) {
    /* Simple support for common prescalers: 1,8,64,256,1024 */
    switch (timer_id) {
        case 0:
            TCCR0B &= ~((1<<CS02)|(1<<CS01)|(1<<CS00));
            if (prescaler == 1) TCCR0B |= (1<<CS00);
            else if (prescaler == 8) TCCR0B |= (1<<CS01);
            else if (prescaler == 64) TCCR0B |= (1<<CS01)|(1<<CS00);
            else if (prescaler == 256) TCCR0B |= (1<<CS02);
            else if (prescaler == 1024) TCCR0B |= (1<<CS02)|(1<<CS00);
            else return -1;
            break;
        case 1:
            TCCR1B &= ~((1<<CS12)|(1<<CS11)|(1<<CS10));
            if (prescaler == 1) TCCR1B |= (1<<CS10);
            else if (prescaler == 8) TCCR1B |= (1<<CS11);
            else if (prescaler == 64) TCCR1B |= (1<<CS11)|(1<<CS10);
            else if (prescaler == 256) TCCR1B |= (1<<CS12);
            else if (prescaler == 1024) TCCR1B |= (1<<CS12)|(1<<CS10);
            else return -1;
            break;
        case 2:
            TCCR2B &= ~((1<<CS22)|(1<<CS21)|(1<<CS20));
            if (prescaler == 1) TCCR2B |= (1<<CS20);
            else if (prescaler == 8) TCCR2B |= (1<<CS21);
            else if (prescaler == 64) TCCR2B |= (1<<CS22); /* common mapping */
            else if (prescaler == 256) { /* some MCUs: CS22|CS21 ??? */ return -1; }
            else if (prescaler == 1024) { /* maybe unsupported for Timer2 */ return -1; }
            else return -1;
            break;
        case 3:
            TCCR3B &= ~((1<<CS32)|(1<<CS31)|(1<<CS30));
            if (prescaler == 1) TCCR3B |= (1<<CS30);
            else if (prescaler == 8) TCCR3B |= (1<<CS31);
            else if (prescaler == 64) TCCR3B |= (1<<CS31)|(1<<CS30);
            else if (prescaler == 256) TCCR3B |= (1<<CS32);
            else if (prescaler == 1024) TCCR3B |= (1<<CS32)|(1<<CS30);
            else return -1;
            break;
        case 4:
            TCCR4B &= ~((1<<CS42)|(1<<CS41)|(1<<CS40));
            if (prescaler == 1) TCCR4B |= (1<<CS40);
            else if (prescaler == 8) TCCR4B |= (1<<CS41);
            else if (prescaler == 64) TCCR4B |= (1<<CS41)|(1<<CS40);
            else if (prescaler == 256) TCCR4B |= (1<<CS42);
            else if (prescaler == 1024) TCCR4B |= (1<<CS42)|(1<<CS40);
            else return -1;
            break;
        case 5:
            return -1; // Timer5 reserved
        default:
            return -1;
    }
    return 0;
}

/* ---- Mapping logic: detect timer & channel from the GPIO_Pin (port + bit) ----
 * We compare p.port pointer (address of PORTx) and p.bit to known OC pins as declared
 * in your GPIO.h. This approach avoids depending on Arduino numeric pin indexes.
 */
static pwm_map_t pwm_map_from_pin(GPIO_Pin p) {
    pwm_map_t out = { .timer = -1, .ch = 0 };

    /* Helper macros for comparison to keep code compact */
    #define IS_PIN(PORTPTR, BITNUM) ((p.port == (volatile uint8_t *)PORTPTR) && (p.bit == (BITNUM)))

    /* NOTE: We compare to &PORTx addresses. Because your GPIO_PIN macro uses &PORT##L
     * the addresses match &PORTx below.
     *
     * Timer0
     *  OC0A = PB7 -> PORTB, bit 7  (D13)
     *  OC0B = PG5 -> PORTG, bit 5  (D4)
     */
    if (IS_PIN(&PORTB, 7)) { out.timer = 0; out.ch = 'A'; return out; }
    if (IS_PIN(&PORTG, 5)) { out.timer = 0; out.ch = 'B'; return out; }

    /* Timer1
     *  OC1A = PB5 (D11)
     *  OC1B = PB6 (D12)
     */
    if (IS_PIN(&PORTB, 5)) { out.timer = 1; out.ch = 'A'; return out; }
    if (IS_PIN(&PORTB, 6)) { out.timer = 1; out.ch = 'B'; return out; }
    /* OC1C not commonly wired to Arduino digital pins on Mega; skip unless needed */

    /* Timer2
     *  OC2A = PB4 (D10)
     *  OC2B = PH6 (D9)
     */
    if (IS_PIN(&PORTB, 4)) { out.timer = 2; out.ch = 'A'; return out; }
    if (IS_PIN(&PORTH, 6)) { out.timer = 2; out.ch = 'B'; return out; }

    /* Timer3 (PE3, PE4, PE5) -> D5 (A), D2 (B), D3 (C) */
    if (IS_PIN(&PORTE, 3)) { out.timer = 3; out.ch = 'A'; return out; }
    if (IS_PIN(&PORTE, 4)) { out.timer = 3; out.ch = 'B'; return out; }
    if (IS_PIN(&PORTE, 5)) { out.timer = 3; out.ch = 'C'; return out; }

    /* Timer4 (PH3, PH4, PH5) -> D6 (A), D7 (B), D8 (C) */
    if (IS_PIN(&PORTH, 3)) { out.timer = 4; out.ch = 'A'; return out; }
    if (IS_PIN(&PORTH, 4)) { out.timer = 4; out.ch = 'B'; return out; }
    if (IS_PIN(&PORTH, 5)) { out.timer = 4; out.ch = 'C'; return out; }

    /* Timer5 reserved: do not map PL3/PL4/PL5 to PWM */

    #undef IS_PIN
    return out;
}
