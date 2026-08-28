#ifndef AMBIENT_DIM_H
#define AMBIENT_DIM_H

#include <stdint.h>

void ambient_dim_init(void);
void ambient_dim_update(void);
/* PWM duty 0-255 for display_pwm (255 = brightest). */
uint8_t ambient_dim_get_pwm(void);

#endif
