// General-purpose message display firmware for the ATtiny88
// Clock: F_CPU in Makefile must match chip. Default 8MHz (CKDIV8 fuse off).
/*
 * show_msg.c
 * Version 0.1
 *
 * Copyright 2026 Jason <aztuxmann@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

/*
 * The ELT-316 is a 3-digit 7-segment common annode display. In order to
 * drive an LED, the corresponding bit must be set to zero and not a one.
 *
 * LED Diagram reference
 * 	DP	G	F	E	D	C	B	A	:: Segments
 * 	 7	6	5	4	3	2	1	0	:: Bits
 *
 *     -  A
 *  F | | B
 *     _  G
 *  E | | C
 *     _  D
 */

#ifndef F_CPU
#define F_CPU 8000000UL
#endif

#include <avr/io.h>
#include <util/twi.h>
#include <util/delay.h>
#include <avr/interrupt.h>

#define SET(x,y) (x|=(1<<y))
#define CLR(x,y) (x&=(~(1<<y)))
#define CHK(x,y) (x&(1<<y))
#define TOG(x,y) (x^=(1<<y))

// I2C/TWI helpers
#define TWACK   (TWCR=(1<<TWINT)|(1<<TWEN)|(1<<TWEA)|(1<<TWIE))
#define TWNACK  (TWCR=(1<<TWINT)|(1<<TWEN)|(1<<TWIE))
#define TWRESET (TWCR=(1<<TWINT)|(1<<TWEN)|(1<<TWSTO)|(1<<TWEA)|(1<<TWIE))

// Digit select lines on PORTD
#define LEFT    0x80
#define MIDDLE  0x40
#define RIGHT   0x20

// I2C address for the Pi IP board (must match host script)
#define ADDR    0x5A

// Maximum number of message bytes we accept over I2C (excluding control byte)
#define MSG_MAX_LEN 32

// Globals
volatile uint8_t msg_buf[MSG_MAX_LEN];
volatile uint8_t msg_len = 0;
volatile uint8_t msg_ready = 0;
volatile uint8_t rx_count = 0;   // includes control byte

// 7-seg values for current display (left, middle, right)
volatile uint8_t charLED[3];

// Prototypes
static void set_default_message(void);
static void build_display_from_message(void);
static void display_static_frame(void);
static int hex_to_segment(uint8_t hex_value);
static int letter_to_segment(char letter_value);

// -------------------- MAIN --------------------
int main(void)
{
    // Set up I2C slave
    TWAR = (ADDR << 1);  // load slave address 0x5A
    TWCR = (1<<TWINT) | (1<<TWEA) | (1<<TWEN) | (1<<TWIE);

    // Set up ports (same as show_ip.c)
    DDRB = 0xFF;  // 7 segments with decimal point on PORTB
    DDRD = 0xE0;  // Digits select on PORTD (bits 5,6,7)

    sei();        // Enable global interrupts

    set_default_message();  // show "rdy" until first message arrives

    while (1) {
        if (msg_ready) {
            uint8_t local_len;
            uint8_t local_buf[MSG_MAX_LEN];
            uint8_t i;

            // Copy message buffer atomically
            cli();
            local_len = msg_len;
            if (local_len > MSG_MAX_LEN) {
                local_len = MSG_MAX_LEN;
            }
            for (i = 0; i < local_len; i++) {
                local_buf[i] = msg_buf[i];
            }
            msg_ready = 0;
            sei();

            // Build 3-digit display from ASCII message
            // Rules:
            //  - Up to 3 characters are displayed.
            //  - '.' sets the decimal point on the previous digit.
            //  - Unsupported characters show as blank.
            charLED[0] = 0xFF;
            charLED[1] = 0xFF;
            charLED[2] = 0xFF;

            uint8_t out_idx = 0;
            for (i = 0; i < local_len && out_idx < 3; i++) {
                char c = (char)local_buf[i];
                if (c == '.') {
                    if (out_idx > 0) {
                        charLED[out_idx - 1] &= 0x7F;  // turn on DP
                    }
                } else {
                    charLED[out_idx] = (uint8_t)letter_to_segment(c);
                    out_idx++;
                }
            }
        }

        // Continuously multiplex the 3 digits.
        display_static_frame();
    }
}
// ----------------- END MAIN -------------------

// I2C/TWI interrupt handler
ISR(TWI_vect)
{
    switch (TW_STATUS) {
        // Slave receiver: SLA+W received, prepare for data
        case 0x60:
        case 0x68:  // addressed as slave while in master mode (not expected)
            rx_count = 0;
            TWACK;
            break;

        // Data received; ACK returned
        case 0x80:
        case 0x90: {
            uint8_t data = TWDR;
            if (rx_count == 0) {
                // First byte after address = "control"/register from host.
                // We ignore it for now, but it could select a mode later.
            } else {
                uint8_t idx = rx_count - 1;
                if (idx < MSG_MAX_LEN) {
                    msg_buf[idx] = data;
                }
            }
            rx_count++;

            // Stop ACKing if buffer is full (control + MSG_MAX_LEN)
            if (rx_count >= (MSG_MAX_LEN + 1)) {
                TWNACK;
            } else {
                TWACK;
            }
            break;
        }

        // STOP or repeated START condition
        case 0xA0:
            if (rx_count > 0) {
                uint8_t len = rx_count - 1;  // exclude control byte
                if (len > MSG_MAX_LEN) {
                    len = MSG_MAX_LEN;
                }
                msg_len = len;
                msg_ready = 1;
            }
            TWACK;
            break;

        // Bus error: illegal START/STOP
        case 0x00:
        default:
            TWRESET;
            break;
    }
}

// ----------------- Display helpers -----------------
static void set_default_message(void)
{
    // Show "rdy" at power-on
    charLED[0] = (uint8_t)letter_to_segment('r');
    charLED[1] = (uint8_t)letter_to_segment('d');
    charLED[2] = (uint8_t)letter_to_segment('y');
}

static void display_static_frame(void)
{
    // One multiplex cycle (3 ms total)
    PORTB = charLED[0];
    PORTD = LEFT;
    _delay_ms(1);

    PORTB = charLED[1];
    PORTD = MIDDLE;
    _delay_ms(1);

    PORTB = charLED[2];
    PORTD = RIGHT;
    _delay_ms(1);
}

// ---------------- Segment translation ----------------
static int hex_to_segment(uint8_t hex_value)
{
    switch (hex_value) {
        /*
        0 = 0xC0    3 = 0xB0    6 = 0x82    9 = 0x90
        1 = 0xF9    4 = 0x99    7 = 0xF8
        2 = 0xA4    5 = 0x92    8 = 0x80
        */
        case 0x00: hex_value = 0xC0; break;
        case 0x01: hex_value = 0xF9; break;
        case 0x02: hex_value = 0xA4; break;
        case 0x03: hex_value = 0xB0; break;
        case 0x04: hex_value = 0x99; break;
        case 0x05: hex_value = 0x92; break;
        case 0x06: hex_value = 0x82; break;
        case 0x07: hex_value = 0xF8; break;
        case 0x08: hex_value = 0x80; break;
        case 0x09: hex_value = 0x90; break;
    }
    return (int)hex_value;
}

static int letter_to_segment(char letter_value)
{
    switch (letter_value) {
        /*
        A = 0x88  E = 0x86  h = 0x8B  n = 0xAB  t = 0x87  _ = 0xF7
        b = 0x83  F = 0x8E  I = 0xCF  o = 0xA3  U = 0xC1   ' '= 0xFF
        C = 0xC6  g = 0x90  J = 0xE1  P = 0x8C  u = 0xE3   - = 0xBF
        d = 0xA1  H = 0x89  L = 0xC7  r = 0xAF  y = 0x91
        */

        case 'A': letter_value = 0x88; break;  case 'a': letter_value = 0x88; break;
        case 'B': letter_value = 0x83; break;  case 'b': letter_value = 0x83; break;
        case 'C': letter_value = 0xC6; break;  case 'c': letter_value = 0xA7; break;
        case 'D': letter_value = 0xA1; break;  case 'd': letter_value = 0xA1; break;
        case 'E': letter_value = 0x86; break;  case 'e': letter_value = 0x86; break;
        case 'F': letter_value = 0x8E; break;  case 'f': letter_value = 0x8E; break;
        case 'G': letter_value = 0x90; break;  case 'g': letter_value = 0x90; break;
        case 'H': letter_value = 0x89; break;  case 'h': letter_value = 0x8B; break;
        case 'I': letter_value = 0xF9; break;  case 'i': letter_value = 0xCF; break;
        case 'J': letter_value = 0xE1; break;  case 'j': letter_value = 0xE1; break;
        case 'L': letter_value = 0xC7; break;  case 'l': letter_value = 0xCF; break;
        case 'N': letter_value = 0xAB; break;  case 'n': letter_value = 0xAB; break;
        case 'O': letter_value = 0xC0; break;  case 'o': letter_value = 0xA3; break;
        case 'P': letter_value = 0x8C; break;  case 'p': letter_value = 0x8C; break;
        case 'R': letter_value = 0xAF; break;  case 'r': letter_value = 0xAF; break;
        case 'S': letter_value = 0x92; break;  case 's': letter_value = 0x92; break;
        case 'T': letter_value = 0x87; break;  case 't': letter_value = 0x87; break;
        case 'U': letter_value = 0xC1; break;  case 'u': letter_value = 0xE3; break;
        case 'V': letter_value = 0xC1; break;  case 'v': letter_value = 0xE3; break;
        case 'W': letter_value = 0x8A; break;  case 'w': letter_value = 0x8A; break;
        case 'Y': letter_value = 0x91; break;  case 'y': letter_value = 0x91; break;
        case '0': letter_value = 0xC0; break;  case '5': letter_value = 0x92; break;
        case '1': letter_value = 0xF9; break;  case '6': letter_value = 0x82; break;
        case '2': letter_value = 0xA4; break;  case '7': letter_value = 0xF8; break;
        case '3': letter_value = 0xB0; break;  case '8': letter_value = 0x80; break;
        case '4': letter_value = 0x99; break;  case '9': letter_value = 0x90; break;
        case '_': letter_value = 0xF7; break;  case ' ': letter_value = 0xFF; break;
        case '.': letter_value = 0x7F; break;  case '-': letter_value = 0xBF; break;
        default : letter_value = 0xC9; break;
    }
    return (int)letter_value;
}

