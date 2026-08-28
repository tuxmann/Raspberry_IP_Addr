#ifndef DISPLAY_PWM_H
#define DISPLAY_PWM_H

#include <stdint.h>

void display_pwm_init(void);
void display_pwm_set_segments(uint8_t left, uint8_t middle, uint8_t right);
void display_pwm_set_brightness(uint8_t pwm);
/* Wall-clock milliseconds from Timer0 (not busy-wait). */
uint16_t display_pwm_millis(void);
/* Stop Timer0 so DHT bit-bang delays are accurate; blank the display. */
void display_pwm_pause(void);
void display_pwm_resume(void);

#endif
