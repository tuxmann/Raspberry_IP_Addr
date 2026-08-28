#ifndef F_CPU
#define F_CPU 8000000UL
#endif

#include <avr/io.h>
#include <stdint.h>

#include "ambient_dim.h"
#include "display_pwm.h"
#include "dht22.h"

/*
 * DHT22: wait at least 2 s between reads; we use 6 s.
 * One start pulse per period (no rapid retries).
 */
#define SAMPLE_PERIOD_MS 6000U
#define SHOW_TEMP_MS     3000U
#define STARTUP_DELAY_MS 2000U

/*
 * Calibration: whole display units (not tenths).
 * TEMP_OFFSET is applied after C→F conversion (display is °F today), so
 * -2 makes 78.0F show as 76.0F. If you later display °C, the same define
 * would subtract 2 °C.
 * HUMIDITY_OFFSET is whole percent RH points (-5 makes H38 → H33).
 */
#define TEMP_OFFSET      -1
#define HUMIDITY_OFFSET   0

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
    /* Convert to °F tenths, then apply whole-degree calibration. */
    int16_t temp_f_tenths = (int16_t)(((int32_t)temp_c_tenths * 9 + 25) / 5 + 320);
    temp_f_tenths = (int16_t)(temp_f_tenths + ((int16_t)TEMP_OFFSET * 10));

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
    int16_t humidity = (int16_t)((humidity_tenths + 5U) / 10U) + (int16_t)HUMIDITY_OFFSET;

    if (humidity < 0) {
        humidity = 0;
    }
    if (humidity > 99) {
        humidity = 99;
    }

    char_led[0] = letter_to_segment('H');
    char_led[1] = digit_to_segment((uint8_t)(humidity / 10));
    char_led[2] = digit_to_segment((uint8_t)(humidity % 10));
}

int main(void)
{
    uint16_t last_ms;
    uint16_t elapsed_since_sample = 0;
    uint16_t elapsed_in_cycle = 0;
    uint16_t startup_ms = 0;

    uint16_t humidity_tenths = 0;
    int16_t temperature_tenths_c = 0;
    uint8_t have_reading = 0;
    uint8_t last_read_failed = 0;

    display_pwm_init();
    set_ready_display();
    display_pwm_set_segments(char_led[0], char_led[1], char_led[2]);
    dht22_init();
    ambient_dim_init();
    last_ms = display_pwm_millis();

    while (1) {
        uint16_t now = display_pwm_millis();
        uint16_t dt = (uint16_t)(now - last_ms);

        if (dt > 0U) {
            last_ms = now;
            if (startup_ms < STARTUP_DELAY_MS) {
                startup_ms = (uint16_t)(startup_ms + dt);
            }
            elapsed_since_sample = (uint16_t)(elapsed_since_sample + dt);
            elapsed_in_cycle = (uint16_t)(elapsed_in_cycle + dt);
            if (elapsed_in_cycle >= SAMPLE_PERIOD_MS) {
                elapsed_in_cycle = 0U;
            }
        }

        if (startup_ms >= STARTUP_DELAY_MS &&
            elapsed_since_sample >= SAMPLE_PERIOD_MS) {
            dht22_reading_t reading;

            display_pwm_pause();
            if (dht22_read(&reading) && dht22_reading_is_plausible(&reading)) {
                temperature_tenths_c = reading.temperature_tenths_c;
                humidity_tenths = reading.humidity_tenths;
                have_reading = 1U;
                last_read_failed = 0U;
            } else {
                last_read_failed = 1U;
            }
            display_pwm_resume();

            elapsed_since_sample = 0U;
            elapsed_in_cycle = 0U;
        }

        if (last_read_failed) {
            set_error_display();
        } else if (have_reading) {
            if (elapsed_in_cycle < SHOW_TEMP_MS) {
                set_temperature_display(temperature_tenths_c);
            } else {
                set_humidity_display(humidity_tenths);
            }
        } else {
            set_ready_display();
        }

        display_pwm_set_segments(char_led[0], char_led[1], char_led[2]);
        ambient_dim_update();
        display_pwm_set_brightness(ambient_dim_get_pwm());
    }
}
