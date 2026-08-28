/*
 * Debug firmware: show ADC or PWM% on PC1 and dim via Timer0 PWM.
 * DEBUG_SHOW_PWM_PCT 1 = duty 0-100, 0 = raw ADC.
 * Tune ADC_BRIGHT / ADC_DARK / PWM_MIN below, reflash.
 *
 * Build:  make debug
 * Flash:  sudo make flash-debug
 */

#ifndef F_CPU
#define F_CPU 8000000UL
#endif

#include <avr/io.h>
#include <stdint.h>

#include "display_pwm.h"

#define ADC_BRIGHT          200U
#define ADC_DARK            450U
#define PWM_MAX             255U
#define PWM_MIN             20U

/* 0 = show raw ADC (0-1023), 1 = show PWM duty as 0-100 percent. */
#define DEBUG_SHOW_PWM_PCT  1

#define AMBIENT_ADC_CHANNEL 1U
#define SAMPLE_MS           1000U

static uint8_t char_led[3];

static uint8_t digit_to_segment(uint8_t v)
{
    switch (v) {
        case 0: return 0xC0;
        case 1: return 0xF9;
        case 2: return 0xA4;
        case 3: return 0xB0;
        case 4: return 0x99;
        case 5: return 0x92;
        case 6: return 0x82;
        case 7: return 0xF8;
        case 8: return 0x80;
        case 9: return 0x90;
        default: return 0xFF;
    }
}

static void adc_init_pc1(void)
{
    DDRC &= (uint8_t)~(1U << PC1);
    PORTC &= (uint8_t)~(1U << PC1);
    ADMUX = (uint8_t)((1U << REFS0) | (AMBIENT_ADC_CHANNEL & 0x0FU));
    ADCSRA = (1U << ADEN) | (1U << ADPS2) | (1U << ADPS1);
}

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

static void set_number_display(uint16_t value)
{
    if (value > 999U) {
        value = 999U;
    }

    char_led[0] = digit_to_segment((uint8_t)(value / 100U));
    char_led[1] = digit_to_segment((uint8_t)((value / 10U) % 10U));
    char_led[2] = digit_to_segment((uint8_t)(value % 10U));

    if (value < 100U) {
        char_led[0] = 0xFF;
    }
    if (value < 10U) {
        char_led[1] = 0xFF;
    }
}

int main(void)
{
    uint16_t last_ms = 0;
    uint16_t adc = 0U;

    display_pwm_init();
    adc_init_pc1();
    set_number_display(0U);
    display_pwm_set_segments(char_led[0], char_led[1], char_led[2]);
    last_ms = display_pwm_millis();

    while (1) {
        uint16_t now = display_pwm_millis();

        if ((uint16_t)(now - last_ms) >= SAMPLE_MS) {
            uint8_t pwm;

            last_ms = now;
            adc = adc_read_pc1();
            pwm = map_adc_to_pwm(adc);
            display_pwm_set_brightness(pwm);
#if DEBUG_SHOW_PWM_PCT
            set_number_display((uint16_t)(((uint16_t)pwm * 100U) / PWM_MAX));
#else
            set_number_display(adc);
#endif
            display_pwm_set_segments(char_led[0], char_led[1], char_led[2]);
        }
    }
}
