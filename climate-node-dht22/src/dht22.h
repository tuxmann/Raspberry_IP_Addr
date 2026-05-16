#ifndef DHT22_H
#define DHT22_H

/*
 * AVR DHT22 driver; bit timing matches Adafruit DHT_sensor_library (DHT.cpp).
 */

#include <stdint.h>

/*
 * One DHT22 reading.
 * The sensor reports values in tenths, so:
 * - 253 means 25.3
 * - -45 means -4.5
 */
typedef struct {
    /* Temperature in tenths of degrees Celsius. */
    int16_t temperature_tenths_c;
    /* Relative humidity in tenths of percent. */
    uint16_t humidity_tenths;
} dht22_reading_t;

/* Configure the DHT22 data pin (input with pull-up). */
void dht22_init(void);
/* Return 1 on success, 0 on timeout/checksum/plausibility failure. */
uint8_t dht22_read(dht22_reading_t *out);
/* Reject obvious bad frames (e.g. 0% RH / ~0 C). */
uint8_t dht22_reading_is_plausible(const dht22_reading_t *reading);

#endif
