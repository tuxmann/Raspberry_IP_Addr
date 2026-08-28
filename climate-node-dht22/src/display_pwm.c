/*
 * Timer0 ISR: software PWM for common-anode 3-digit multiplex on PORTB/PORTD.
 * Brightness 0-255 sets duty cycle per digit (255 = brightest).
 *
 * ATtiny88: prescaler CS0[2:0] and CTC0 live in TCCR0A (no TCCR0B).
 */

#ifndef F_CPU
#define F_CPU 8000000UL
#endif

#include "display_pwm.h"

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>

#define LEFT    0x80
#define MIDDLE  0x40
#define RIGHT   0x20

/*
 * CTC clk/8, OCR0A=20 -> ~48 kHz ISR.
 * 32 ticks/digit * 3 digits -> ~500 Hz frame rate (no visible scan flicker).
 * Lower than 200 kHz so the main loop can still run on time.
 */
#define TICKS_PER_DIGIT 32U
#define ISR_PER_MS      48U

static const uint8_t digit_sel[3] = { LEFT, MIDDLE, RIGHT };

static volatile uint8_t seg_left = 0xFF;
static volatile uint8_t seg_middle = 0xFF;
static volatile uint8_t seg_right = 0xFF;
static volatile uint8_t pwm_brightness = 255U;
static volatile uint8_t cur_digit = 0U;
static volatile uint8_t tick = 0U;
static volatile uint16_t millis_count = 0U;
static uint8_t ms_div = 0U;
static uint8_t tccr0a_saved = 0U;

ISR(TIMER0_COMPA_vect)
{
    uint8_t on_ticks;
    uint8_t pattern;

    on_ticks = (uint8_t)(((uint16_t)pwm_brightness * TICKS_PER_DIGIT) / 255U);

    if (cur_digit == 0U) {
        pattern = seg_left;
    } else if (cur_digit == 1U) {
        pattern = seg_middle;
    } else {
        pattern = seg_right;
    }

    if (tick < on_ticks) {
        PORTB = pattern;
    } else {
        PORTB = 0xFF;
    }

    tick++;
    if (tick >= TICKS_PER_DIGIT) {
        tick = 0U;
        cur_digit = (uint8_t)((cur_digit + 1U) % 3U);
        PORTD = digit_sel[cur_digit];
    }

    ms_div++;
    if (ms_div >= ISR_PER_MS) {
        ms_div = 0U;
        millis_count++;
    }
}

void display_pwm_init(void)
{
    DDRB = 0xFF;
    DDRD = (uint8_t)(DDRD | 0xE0);

    seg_left = 0xFF;
    seg_middle = 0xFF;
    seg_right = 0xFF;
    PORTD = LEFT;
    millis_count = 0U;
    ms_div = 0U;

    /* CTC: clk/8 (1 MHz), OCR0A=20 -> interrupt every ~21 us (~48 kHz). */
    TCCR0A = (1U << CTC0) | (1U << CS01);
    OCR0A = 20U;
    TIMSK0 = (1U << OCIE0A);

    sei();
}

void display_pwm_set_segments(uint8_t left, uint8_t middle, uint8_t right)
{
    seg_left = left;
    seg_middle = middle;
    seg_right = right;
}

void display_pwm_set_brightness(uint8_t pwm)
{
    pwm_brightness = pwm;
}

uint16_t display_pwm_millis(void)
{
    uint16_t ms;
    uint8_t sreg = SREG;

    cli();
    ms = millis_count;
    SREG = sreg;
    return ms;
}

void display_pwm_pause(void)
{
    uint8_t sreg = SREG;

    cli();
    tccr0a_saved = TCCR0A;
    TCCR0A &= (uint8_t)~((1U << CS02) | (1U << CS01) | (1U << CS00));
    PORTB = 0xFF;
    SREG = sreg;
}

void display_pwm_resume(void)
{
    TCCR0A = tccr0a_saved;
}
