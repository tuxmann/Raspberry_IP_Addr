#ifndef F_CPU
#define F_CPU 8000000UL
#endif

#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>

#include "dht22.h"

/* Digit select masks on PORTD for left/middle/right digit. */
#define LEFT    0x80
#define MIDDLE  0x40
#define RIGHT   0x20

/*
 * DHT22: wait at least 2 s between reads; we use 5 s.
 * One start pulse per period (no rapid retries).
 */
#define SAMPLE_PERIOD_MS 5000U
#define SHOW_TEMP_MS     2500U
#define STARTUP_DELAY_MS 2000U

/* Segment bytes currently being shown on the 3-digit display. */
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

static uint8_t letter_to_segment(char c)
{
    switch (c) {
        case 'H': return 0x89;
        case 'E': return 0x86;
        case '-': return 0xBF;
        case 'r': return 0xAF;
        case 'd': return 0xA1;
        case 'y': return 0x91;
        default: return 0xFF;
    }
}

static void display_frame(void)
{
    PORTB = char_led[0];
    PORTD = LEFT;
    _delay_ms(1);

    PORTB = char_led[1];
    PORTD = MIDDLE;
    _delay_ms(1);

    PORTB = char_led[2];
    PORTD = RIGHT;
    _delay_ms(1);
}

static void set_ready_display(void)
{
    char_led[0] = letter_to_segment('r');
    char_led[1] = letter_to_segment('d');
    char_led[2] = letter_to_segment('y');
}

static void set_error_display(void)
{
    char_led[0] = letter_to_segment('E');
    char_led[1] = letter_to_segment('r');
    char_led[2] = letter_to_segment('r');
}

static void set_temperature_display(int16_t temp_c_tenths)
{
    int16_t temp_f_tenths = (int16_t)(((int32_t)temp_c_tenths * 9 + 25) / 5 + 320);

    if (temp_f_tenths < 0) {
        char_led[0] = letter_to_segment('-');
        char_led[1] = digit_to_segment((uint8_t)((-temp_f_tenths / 10) % 10));
        char_led[2] = digit_to_segment((uint8_t)((-temp_f_tenths) % 10));
        return;
    }

    if (temp_f_tenths >= 1000) {
        uint16_t rounded = (uint16_t)((temp_f_tenths + 5) / 10);
        if (rounded > 999U) {
            rounded = 999U;
        }

        char_led[0] = digit_to_segment((uint8_t)(rounded / 100U));
        char_led[1] = digit_to_segment((uint8_t)((rounded / 10U) % 10U));
        char_led[2] = digit_to_segment((uint8_t)(rounded % 10U));
        return;
    }

    {
        uint8_t tens = (uint8_t)((temp_f_tenths / 100) % 10);
        uint8_t ones = (uint8_t)((temp_f_tenths / 10) % 10);
        uint8_t tenths = (uint8_t)(temp_f_tenths % 10);

        char_led[0] = (tens == 0U) ? 0xFF : digit_to_segment(tens);
        char_led[1] = digit_to_segment(ones) & 0x7F;
        char_led[2] = digit_to_segment(tenths);
    }
}

static void set_humidity_display(uint16_t humidity_tenths)
{
    uint16_t humidity = (uint16_t)((humidity_tenths + 5U) / 10U);
    if (humidity > 99U) {
        humidity = 99U;
    }

    char_led[0] = letter_to_segment('H');
    char_led[1] = digit_to_segment((uint8_t)(humidity / 10U));
    char_led[2] = digit_to_segment((uint8_t)(humidity % 10U));
}

int main(void)
{
    uint16_t elapsed_since_sample = 0;
    uint16_t elapsed_in_cycle = 0;
    uint16_t startup_ms = 0;

    uint16_t humidity_tenths = 0;
    int16_t temperature_tenths_c = 0;
    uint8_t have_reading = 0;
    uint8_t last_read_failed = 0;

    DDRB = 0xFF;
    DDRD = 0xE0;
    set_ready_display();
    dht22_init();

    while (1) {
        /*
         * After power-up, wait STARTUP_DELAY_MS while multiplexing "rdy",
         * then sample every SAMPLE_PERIOD_MS (exactly one DHT transaction).
         */
        if (startup_ms >= STARTUP_DELAY_MS &&
            elapsed_since_sample >= SAMPLE_PERIOD_MS) {
            dht22_reading_t reading;

            if (dht22_read(&reading) && dht22_reading_is_plausible(&reading)) {
                temperature_tenths_c = reading.temperature_tenths_c;
                humidity_tenths = reading.humidity_tenths;
                have_reading = 1U;
                last_read_failed = 0U;
            } else {
                last_read_failed = 1U;
            }

            elapsed_since_sample = 0U;
            elapsed_in_cycle = 0U;
        }

        if (have_reading) {
            if (elapsed_in_cycle < SHOW_TEMP_MS) {
                set_temperature_display(temperature_tenths_c);
            } else {
                set_humidity_display(humidity_tenths);
            }
        } else if (last_read_failed) {
            set_error_display();
        } else {
            set_ready_display();
        }

        display_frame();

        if (startup_ms < STARTUP_DELAY_MS) {
            startup_ms += 3U;
        }
        elapsed_since_sample += 3U;
        elapsed_in_cycle += 3U;
        if (elapsed_in_cycle >= SAMPLE_PERIOD_MS) {
            elapsed_in_cycle = 0U;
        }
    }
}
