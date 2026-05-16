/*
 * DHT22 driver for ATtiny88 (PC0 / P3 pin 1).
 *
 * Read algorithm ported from Adafruit DHT-sensor-library (DHT.cpp):
 *   https://github.com/adafruit/DHT-sensor-library
 * Uses relative low/high cycle counts per bit instead of a fixed us threshold.
 */

#ifndef F_CPU
#define F_CPU 8000000UL
#endif

#include "dht22.h"

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>

#define DHT_DDR   DDRC
#define DHT_PORT  PORTC
#define DHT_PINR  PINC
#define DHT_BIT   PC0
#define DHT_MASK  (1U << DHT_BIT)

/* DHT22 start/low time and post-release settle (Adafruit defaults). */
#define DHT_START_LOW_US   1100U
#define DHT_PULLUP_WAIT_US 55U

/*
 * Pulse wait timeout ~= 1 ms of CPU cycles (see DHT.cpp _maxcycles).
 * On 8 MHz AVR, 16-bit cycle counts are enough (Adafruit does the same).
 */
#define DHT_MAX_CYCLES     ((uint16_t)(F_CPU / 1000UL))
#define DHT_PULSE_TIMEOUT  UINT16_MAX

static uint16_t expect_pulse(uint8_t level)
{
    uint16_t count = 0;
    uint8_t want_state = level ? DHT_MASK : 0;

    /* Count cycles while the line stays at the requested level. */
    while ((DHT_PINR & DHT_MASK) == want_state) {
        if (++count >= DHT_MAX_CYCLES) {
            return DHT_PULSE_TIMEOUT;
        }
    }
    return count;
}

uint8_t dht22_reading_is_plausible(const dht22_reading_t *reading)
{
    if (reading == 0) {
        return 0U;
    }

    if (reading->humidity_tenths < 100U || reading->humidity_tenths > 1000U) {
        return 0U;
    }
    if (reading->temperature_tenths_c < -400 || reading->temperature_tenths_c > 850) {
        return 0U;
    }
    return 1U;
}

void dht22_init(void)
{
    DHT_DDR &= (uint8_t)~DHT_MASK;
    DHT_PORT |= DHT_MASK;
}

uint8_t dht22_read(dht22_reading_t *out)
{
    uint8_t data[5];
    uint16_t cycles[80];
    uint8_t sreg;
    uint8_t i;

    if (out == 0) {
        return 0U;
    }

    data[0] = data[1] = data[2] = data[3] = data[4] = 0;

    /* Begin(): INPUT_PULLUP, short settle. */
    DHT_DDR &= (uint8_t)~DHT_MASK;
    DHT_PORT |= DHT_MASK;
    _delay_ms(1);

    /* Start signal: line low >= 1 ms for DHT22. */
    DHT_DDR |= DHT_MASK;
    DHT_PORT &= (uint8_t)~DHT_MASK;
    _delay_us(DHT_START_LOW_US);

    /* Release line, wait for sensor to take over. */
    DHT_DDR &= (uint8_t)~DHT_MASK;
    DHT_PORT |= DHT_MASK;
    _delay_us(DHT_PULLUP_WAIT_US);

    sreg = SREG;
    cli();

    /* ACK: ~80 us low, ~80 us high. */
    if (expect_pulse(0U) == DHT_PULSE_TIMEOUT) {
        SREG = sreg;
        return 0U;
    }
    if (expect_pulse(1U) == DHT_PULSE_TIMEOUT) {
        SREG = sreg;
        return 0U;
    }

    /* 40 bits: measure each low and high pulse length. */
    for (i = 0; i < 80; i += 2) {
        cycles[i] = expect_pulse(0U);
        cycles[i + 1U] = expect_pulse(1U);
    }

    SREG = sreg;

    /* Decode: bit is 1 when high-phase cycles > low-phase cycles. */
    for (i = 0; i < 40; i++) {
        uint16_t low_cycles = cycles[(uint8_t)(2U * i)];
        uint16_t high_cycles = cycles[(uint8_t)(2U * i + 1U)];

        if (low_cycles == DHT_PULSE_TIMEOUT || high_cycles == DHT_PULSE_TIMEOUT) {
            return 0U;
        }

        data[i / 8U] <<= 1;
        if (high_cycles > low_cycles) {
            data[i / 8U] |= 1U;
        }
    }

    if (data[4] != (uint8_t)((data[0] + data[1] + data[2] + data[3]) & 0xFF)) {
        return 0U;
    }

    /* DHT22: humidity and temperature in tenths (see DHT::readHumidity/readTemperature). */
    out->humidity_tenths = (uint16_t)(((uint16_t)data[0] << 8) | data[1]);

    {
        uint16_t temp_raw = (uint16_t)(((uint16_t)(data[2] & 0x7FU) << 8) | data[3]);
        if (data[2] & 0x80U) {
            out->temperature_tenths_c = -(int16_t)temp_raw;
        } else {
            out->temperature_tenths_c = (int16_t)temp_raw;
        }
    }

    return 1U;
}
