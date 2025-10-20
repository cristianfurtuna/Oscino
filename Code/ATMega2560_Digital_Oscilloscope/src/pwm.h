#ifndef PWM_H
#define PWM_H

#include <stdint.h>
#include "gpio.h"

/*
 * Public API
 */

/* Initialize internal data structures (call once at startup) */
void PWM_init(void);

/* Attach a pin for PWM output. Configures DDR and timer compare mode.
 * Returns 0 on success, -1 if the pin is not a PWM-capable pin.
 */
int PWM_attach(GPIO_Pin p);

/* Detach PWM from pin (turns off compare output bits). */
int PWM_detach(GPIO_Pin p);

/* Write duty cycle.
 * For 8-bit timers: value range 0..255
 * For 16-bit timers: value range 0..65535
 * The function automatically scales/clamps.
 * Returns 0 on success, -1 on bad pin.
 */
int PWM_write_raw(GPIO_Pin p, uint32_t value);

/* Write duty cycle as percent (0.0 - 100.0) */
int PWM_write_percent(GPIO_Pin p, float percent);

/* Helper: get max counter value for the timer controlling this pin
 * returns 255 or 65535 on success, 0 on unknown pin.
 */
uint32_t PWM_get_max(GPIO_Pin p);

/* Optional: change prescaler for a particular timer (affects all pins on that timer).
 * timer_id: 0..5  (timer0..timer5)
 * prescaler value: one of: 1,8,64,256,1024 (others exist for some timers)
 * returns 0 on success, -1 on invalid timer or unsupported prescaler
 */
int PWM_set_prescaler(uint8_t timer_id, uint16_t prescaler);

#endif // PWM_H
