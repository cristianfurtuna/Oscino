#ifndef MILLIS_H
#define MILLIS_H

#include <stdint.h>


void     millis_init(void);
uint32_t millis_now(void);
void     delay_ms(uint32_t ms);

static inline uint32_t millis_since(uint32_t t0) { 
    return (uint32_t)(millis_now() - t0); }
static inline uint8_t  millis_elapsed(uint32_t t0, uint32_t dt) {
    return (int32_t)(millis_now() - (t0 + dt)) >= 0;
}

#endif // MILLIS_H
