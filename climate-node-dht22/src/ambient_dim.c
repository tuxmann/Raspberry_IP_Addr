/*
 * Display brightness: photoresistor on PC1, or a fixed PWM duty.
 *
 * USE_PHOTOSENSOR 1 = read LDR once per second (same map as debug firmware).
 * USE_PHOTOSENSOR 0 = ignore LDR; use PWM_MANUAL (0-255, 255 = brightest).
 */

#ifndef F_CPU
#define F_CPU 8000000UL
#endif

#include "ambient_dim.h"
#include "display_pwm.h"

#include <avr/io.h>
#include <stdint.h>

#define USE_PHOTOSENSOR         1
#define PWM_MANUAL              185U

#define AMBIENT_ADC_CHANNEL     1U
#define AMBIENT_DDR_BIT         PC1
#define AMBIENT_SAMPLE_MS       1000U

#define ADC_BRIGHT              200U
#define ADC_DARK                450U
#define PWM_MAX                 255U
#define PWM_MIN                 20U

static uint8_t pwm_duty = PWM_MAX;

#if USE_PHOTOSENSOR
static uint16_t last_sample_ms = 0;

static uint16_t adc_read_pc1(void)
{
    uint8_t i;
    uint32_t sum = 0;

    ADMUX = (uint8_t)((1U << REFS0) | (AMBIENT_ADC_CHANNEL & 0x0FU));

    for (i = 0; i < 4U; i++) {
        ADCSRA |= (1U << ADSC);
        while (ADCSRA & (1U << ADSC)) {
        }
        sum += (uint16_t)ADC;
    }

    return (uint16_t)(sum / 4U);
}

static uint8_t map_adc_to_pwm(uint16_t adc)
{
    int32_t pwm;

    if (adc <= ADC_BRIGHT) {
        return PWM_MAX;
    }
    if (adc >= ADC_DARK) {
        return PWM_MIN;
    }

    pwm = (int32_t)PWM_MAX -
          ((int32_t)(adc - ADC_BRIGHT) * (PWM_MAX - PWM_MIN)) /
          (int32_t)(ADC_DARK - ADC_BRIGHT);
    return (uint8_t)pwm;
}
#endif

void ambient_dim_init(void)
{
#if USE_PHOTOSENSOR
    DDRC &= (uint8_t)~(1U << AMBIENT_DDR_BIT);
    PORTC &= (uint8_t)~(1U << AMBIENT_DDR_BIT);

    ADMUX = (uint8_t)((1U << REFS0) | (AMBIENT_ADC_CHANNEL & 0x0FU));
    ADCSRA = (1U << ADEN) | (1U << ADPS2) | (1U << ADPS1);

    pwm_duty = PWM_MAX;
    last_sample_ms = display_pwm_millis();
#else
    pwm_duty = PWM_MANUAL;
#endif
}

void ambient_dim_update(void)
{
#if USE_PHOTOSENSOR
    uint16_t now = display_pwm_millis();

    if ((uint16_t)(now - last_sample_ms) < AMBIENT_SAMPLE_MS) {
        return;
    }
    last_sample_ms = now;
    pwm_duty = map_adc_to_pwm(adc_read_pc1());
#else
    pwm_duty = PWM_MANUAL;
#endif
}

uint8_t ambient_dim_get_pwm(void)
{
    return pwm_duty;
}
