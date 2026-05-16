// Raw Source code for the ATtiny88
// Clock: F_CPU in Makefile must match chip. Default 8MHz (CKDIV8 fuse off).
/*
 * show_ip.c
 * Version 1.1
 * Aug 18, 2016
  * 
 * Copyright 2015 Jason <aztuxmann@gmail.com>
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
 * 
 * 
 */

// Flashing command on the RPi (enable SPI in raspi-config first):
// avrdude -c linuxspi -p t88 -P /dev/spidev0.0:/dev/gpiochip0 -B 10 -U flash:w:show_ip.hex 

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
 * 	
 */

/* F_CPU must match your chip's actual clock. ATtiny88 internal oscillator:
 * - CKDIV8 ON (default): 1 MHz -> F_CPU=1000000
 * - CKDIV8 OFF: 8 MHz -> F_CPU=8000000
 * If scroll is too fast, F_CPU is too low (e.g. chip at 8MHz but F_CPU=2000000).
 * If scroll is too slow, F_CPU is too high. */
#ifndef F_CPU
#define F_CPU 8000000UL
#endif

#include <avr/io.h>
#include <util/twi.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <string.h>

#define SET(x,y) (x|=(1<<y))
#define CLR(x,y) (x&=(~(1<<y)))
#define CHK(x,y) (x&(1<<y)) 
#define TOG(x,y) (x^=(1<<y))

//setup the I2C hardware to ACK the next transmission
#define TWACK (TWCR=(1<<TWINT)|(1<<TWEN)|(1<<TWEA)|(1<<TWIE))		//and indicate that we've handled the last one.
#define TWNACK (TWCR=(1<<TWINT)|(1<<TWEN)|(1<<TWIE))				//setup the I2C hardware to NACK the next transmission
#define TWRESET (TWCR=(1<<TWINT)|(1<<TWEN)|(1<<TWSTO)|(1<<TWEA)|(1<<TWIE))	//reset the I2C hardware (used when the bus is in a illegal state)

#define LEFT	0x80	// Left   7-segment digit
#define MIDDLE  0x40	// Middle 7-segment digit
#define RIGHT	0x20	// Right  7-segment digit
#define ADDR	0x5A	// I2C or TWI address for the Pi IP board.

/* Scroll speed: ms to hold each character step. Lower = faster.
 * With ~35 steps, 150ms => ~5 sec full scroll. Rebuild and reflash after changing. */
#define SCROLL_MS_PER_STEP  333

#define SCROLL_MSG_LEN 48   /* max length of power-on/timeout scroll message */
#define IP_STR_LEN 28      /* eth/wlan + IP with merged decimals + leading/trailing spaces */

//global variables
const char *pwr_on_msg   = "rPI IP by JAY vEr 1.1 gPL 3";
const char *timeout_msg  = "rPI. Y you no get IP Adr For uSr";
char *message;            /* points to pwr_on_msg or timeout_msg */
char charLED[3];

/* Scroll buffers: power-on message and IP (eth/wlan + digits, decimals merged into digits) */
uint8_t scroll_pwr_segments[SCROLL_MSG_LEN];  /* power-on: 7-seg values, '.' merged into prev char */
uint8_t scroll_pwr_len;
uint8_t ip_segments[IP_STR_LEN];  /* precomputed 7-seg values (DP merged into digit before dot) */
uint8_t ip_seg_len;

// I2C variables
#define BUFLEN_RECV 12
volatile uint8_t ip_received = 0;         					// flag for I2C transaction started
volatile uint8_t ip_count = 0;
volatile uint8_t recv[BUFLEN_RECV]; 						//buffer to store received bytes
volatile uint8_t ipaddr[4];
volatile uint8_t interface_type = 0;  /* 0 = eth, 1 = wlan */
uint8_t led1, led2, led3, length;

#define BUFLEN_TRAN 3
volatile uint8_t t_index=0;					// sudo i2cget 1 0x03 0x00b
volatile uint8_t tran[BUFLEN_TRAN] = {0x02, 0x34, 0x56}; 	//test bytes to transmit
volatile uint8_t reset=0;					//variable to indicate if something went horribly wrong

 //prototypes
int ip_to_led (	uint8_t IPvar );
int hex_to_segment ( uint8_t hex_value );
int letter_to_segment ( char letter_value );
void build_pwr_scroll_string(void);
void build_ip_scroll_string(void);
void scroll_to_led(const char *str, uint8_t len, uint8_t pos);
void scroll_segments_to_led(const uint8_t *seg, uint8_t len, uint8_t pos);
void display_scroll_frame(uint16_t cycles);
void scroll_step_delay(uint16_t ms_total);

//---------------MAIN---------------------------------------------
int main()
{
	TWAR = (ADDR<<1); 	  			//load slave address 0x5A
	TWCR = (1<<TWINT) | (1<<TWEA) | (1<<TWEN) | (1<<TWIE);		//enable I2C hardware
	
	DDRB = 0xFF;	// 7 segments with decimal point	PORTB
	DDRD = 0x07;	// Digits to be selected.		PORTD

	sei();					// Enable Global Interrupts

	/* Power On & Timeout Display Message */
	message = (char *)pwr_on_msg;
	while(1)
	{
		uint8_t scroll_pos;
		int timeout;

		for (timeout = 20; timeout > 0; timeout--)
		{
			if (ip_received == 1)
				break;

			build_pwr_scroll_string();
			for (scroll_pos = 0; scroll_pos < scroll_pwr_len; scroll_pos++)
			{
				if (ip_received == 1)
					break;
				scroll_segments_to_led(scroll_pwr_segments, scroll_pwr_len, scroll_pos);
				scroll_step_delay(SCROLL_MS_PER_STEP);
			}
		}

		message = (char *)timeout_msg;

		if (ip_received == 1)
			break;
	}
	/* End Power On & Timeout Display Message */

	// Main program: scroll IP address (eth/wlan + digits) across the display.
	build_ip_scroll_string();
	while(1)
	{
		uint8_t scroll_pos;

		for (scroll_pos = 0; scroll_pos < ip_seg_len; scroll_pos++)
		{
			scroll_segments_to_led(ip_segments, ip_seg_len, scroll_pos);
			scroll_step_delay(SCROLL_MS_PER_STEP);
		}
		PORTB = 0xFF;
		_delay_ms(800);
	}
}
//-----------END MAIN---------------------------------------------


ISR(TWI_vect)
{
		ip_received = 1;
		switch(TW_STATUS)
		{
			int i; 

//--------------Slave receiver------------------------------------
		case 0x60:  	//SLA_W received and acked, prepare for data receiving
			for (i =10; i > 1; i--)
			TWACK;
			ip_count =0;
			break;
			
		case 0x80:  	//a byte was received, store it and 
			for (i =10; i > 1; i--)			
			//setup the buffer to recieve another
			if (ip_count == 0) {
				interface_type = TWDR;  /* 0 = eth, 1 = wlan */
			} else {
				ipaddr[ip_count - 1] = TWDR;
			}
			ip_count++;
			//don't ack next data if buffer is full (need 5 bytes: interface + 4 octets)
			if(ip_count >= 5)
			{
				TWNACK;
			}
			else
			{
				TWACK;
			}
			break;
			
		case 0x68:		//adressed as slave while in master mode.
						//should never happen, better reset;
			reset=1;
		case 0xA0: 		//Stop or repeat start, reset state machine
			TWACK;
			break;

//---------------Slave Transmitter--------------------------------
		case 0xA8:  //SLA R received, prep for transmission
					//and load first data
			// Count to ten to prevent the RPi from being confused.
			for (i =10; i > 1; i--)
			t_index=1;
			TWDR = tran[0];
			TWACK;
			break;
		case 0xB8:  //data transmitted and acked by master, load next
			TWDR = tran[t_index];
			t_index++;
			//designate last byte if we're at the end of the buffer
			if(t_index >= BUFLEN_TRAN) TWNACK;
			else TWACK;
			break;
		case 0xC8: //last byte send and acked by master
			//last bytes should not be acked, ignore till start/stop
			reset=1;
		case 0xC0: //last byte send and nacked by master 
			//(as should be)
			TWACK;
			break;
			
//-------------- error recovery ----------------------------------
		case 0x88: //data received  but not acked
			//should not happen if the master is behaving as expected
			//switch to not adressed mode
			for (i =500; i > 1; i--)
			{
				PORTB = 0x86;
				PORTD = LEFT;
				_delay_ms(1);
			
				PORTB = 0xAF;
				PORTD = MIDDLE;
				_delay_ms(1);
			
				PORTB = 0xAF;
				PORTD = RIGHT;
				_delay_ms(1);
			}
			TWACK;
			break;

//--------------------- bus error---------------------------------
		//illegal start or stop received, reset the I2C hardware
		case 0x00: 
			TWRESET;
			break;
		//TWCR=(1<<TWINT);
	}
}

// ----------------------------------------------------------
// Build power-on scroll as segment buffer. Any '.' immediately after a
// letter or digit is merged into that character (DP set); standalone '.' -> 0x7F.
// ----------------------------------------------------------
void build_pwr_scroll_string(void)
{
	uint8_t i, k;
	uint8_t seg;
	const char *msg = message;
	if (!msg) {
		scroll_pwr_len = 0;
		return;
	}
	k = 0;
	scroll_pwr_segments[k++] = 0xFF;
	scroll_pwr_segments[k++] = 0xFF;
	scroll_pwr_segments[k++] = 0xFF;
	for (i = 0; msg[i] != '\0' && k < SCROLL_MSG_LEN - 3; ) {
		if (msg[i] == '.') {
			scroll_pwr_segments[k++] = 0x7F;  /* standalone decimal point */
			i++;
		} else {
			seg = (uint8_t)letter_to_segment(msg[i]);
			if (msg[i + 1] == '.') {
				seg &= 0x7F;  /* merge period into this letter/digit */
				i += 2;
			} else {
				i++;
			}
			scroll_pwr_segments[k++] = seg;
		}
	}
	scroll_pwr_segments[k++] = 0xFF;
	scroll_pwr_segments[k++] = 0xFF;
	scroll_pwr_segments[k++] = 0xFF;
	scroll_pwr_len = k;
}

// ----------------------------------------------------------
// Build IP scroll as segment buffer: "   eth" or "   wlan" then digits with
// decimal point merged into the previous digit (no space between number and dot).
// ----------------------------------------------------------
void build_ip_scroll_string(void)
{
	uint8_t k = 0;
	uint8_t o, seg;
	const char *iface = (interface_type == 1) ? "wlan" : "eth";

	ip_segments[k++] = 0xFF;
	ip_segments[k++] = 0xFF;
	ip_segments[k++] = 0xFF;
	for (o = 0; iface[o] != '\0' && k < IP_STR_LEN - 1; o++)
		ip_segments[k++] = letter_to_segment(iface[o]);
	for (o = 0; o < 4 && k < IP_STR_LEN - 6; o++) {
		uint8_t n = ipaddr[o];
		if (n >= 100) {
			ip_segments[k++] = hex_to_segment(n / 100);
			ip_segments[k++] = hex_to_segment((n / 10) % 10);
			seg = hex_to_segment(n % 10);
			if (o < 3) seg &= 0x7F;  /* decimal point after this digit */
			ip_segments[k++] = seg;
		} else if (n >= 10) {
			ip_segments[k++] = hex_to_segment(n / 10);
			seg = hex_to_segment(n % 10);
			if (o < 3) seg &= 0x7F;
			ip_segments[k++] = seg;
		} else {
			seg = hex_to_segment(n);
			if (o < 3) seg &= 0x7F;
			ip_segments[k++] = seg;
		}
	}
	ip_segments[k++] = 0xFF;
	ip_segments[k++] = 0xFF;
	ip_segments[k++] = 0xFF;
	ip_seg_len = k;
}

// ----------------------------------------------------------
// Set charLED[0..2] from 3-character window at position pos in string (wraps)
// ----------------------------------------------------------
void scroll_to_led(const char *str, uint8_t len, uint8_t pos)
{
	uint8_t i;
	if (len == 0) {
		charLED[0] = charLED[1] = charLED[2] = 0xFF;
		return;
	}
	for (i = 0; i < 3; i++)
		charLED[i] = letter_to_segment(str[(pos + i) % len]);
}

// ----------------------------------------------------------
// Set charLED[0..2] from 3 segment values at position pos (for IP with merged decimals)
// ----------------------------------------------------------
void scroll_segments_to_led(const uint8_t *seg, uint8_t len, uint8_t pos)
{
	uint8_t i;
	if (len == 0) {
		charLED[0] = charLED[1] = charLED[2] = 0xFF;
		return;
	}
	for (i = 0; i < 3; i++)
		charLED[i] = seg[(pos + i) % len];
}

// ----------------------------------------------------------
// Multiplex the current charLED[] for a number of cycles (1 cycle = 3 digits)
// ----------------------------------------------------------
void display_scroll_frame(uint16_t cycles)
{
	uint16_t i;
	for (i = cycles; i > 0; i--) {
		PORTB = (charLED[0]);
		PORTD = LEFT;
		_delay_ms(1);
		PORTB = (charLED[1]);
		PORTD = MIDDLE;
		_delay_ms(1);
		PORTB = (charLED[2]);
		PORTD = RIGHT;
		_delay_ms(1);
	}
}

// ----------------------------------------------------------
// Hold current scroll position for exactly ms_total ms. One knob for scroll speed.
// (Multiplexing takes 3ms per cycle; remainder is a delay.)
// ----------------------------------------------------------
void scroll_step_delay(uint16_t ms_total)
{
	uint16_t cycles = ms_total / 3;
	if (cycles == 0) cycles = 1;
	display_scroll_frame(cycles);
	{
		uint16_t remainder = ms_total - (cycles * 3);
		while (remainder > 0) {
			_delay_ms(1);
			remainder--;
		}
	}
}
// ----------------------------------------------------------
int ip_to_led ( uint8_t IPvar )
{
	// Parse each digit so the digit can have 
	// it's corresponding segment value
	led1 = IPvar / 100;		// Separate the  most  significant digit
	led2 = (IPvar % 100) / 10;	// Separate the middle significant digit
	led3 = (IPvar % 100) % 10;	// Separate the least  significant digit
    
	// Change each digit into a segment value
	led1 = hex_to_segment (led1);
	led2 = hex_to_segment (led2);
	led3 = hex_to_segment (led3);
	return 0;
}

// ----------------------------------------------------------
//		TRANSLATE HEX VALUE TO 7 SEGMENT VALUE
// ----------------------------------------------------------
int hex_to_segment ( uint8_t hex_value )
{
	switch ( hex_value )
	{
		/*
		0 = 0xC0	3 = 0xB0	6 = 0x82	9 = 0x90
		1 = 0xF9	4 = 0x99	7 = 0xF8
		2 = 0xA4	5 = 0x92	8 = 0x80	*/
	
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
	return hex_value;
}

int letter_to_segment ( char letter_value )
{
	switch ( letter_value )
	{
		/*
		A = 0x88	E = 0x86	h = 0x8B	n = 0xAB	t = 0x87	_ = 0xF7
		b = 0x83	F = 0x8E	I = 0xCF	o = 0xA3	U = 0xC1   ' '= 0xFF
		C = 0xC6	g = 0x90	J = 0xE1	P = 0x8C	u = 0xE3	- = 0xBF
		d = 0xA1	H = 0x89	L = 0xC7	r = 0xAF	y = 0x91	*/
	
		case 'A': letter_value = 0x88; break;		case 'a': letter_value = 0x88; break;
		case 'B': letter_value = 0x83; break;		case 'b': letter_value = 0x83; break;
		case 'C': letter_value = 0xC6; break;		case 'c': letter_value = 0xA7; break;
		case 'D': letter_value = 0xA1; break;		case 'd': letter_value = 0xA1; break;
		case 'E': letter_value = 0x86; break;		case 'e': letter_value = 0x86; break;
		case 'F': letter_value = 0x8E; break;		case 'f': letter_value = 0x8E; break;
		case 'G': letter_value = 0x90; break;		case 'g': letter_value = 0x90; break;
		case 'H': letter_value = 0x89; break;		case 'h': letter_value = 0x8B; break;
		case 'I': letter_value = 0xF9; break;		case 'i': letter_value = 0xCF; break;
		case 'J': letter_value = 0xE1; break;		case 'j': letter_value = 0xE1; break;
		case 'L': letter_value = 0xC7; break;		case 'l': letter_value = 0xCF; break;
		case 'N': letter_value = 0xAB; break;		case 'n': letter_value = 0xAB; break;
		case 'O': letter_value = 0xC0; break;		case 'o': letter_value = 0xA3; break;
		case 'P': letter_value = 0x8C; break;		case 'p': letter_value = 0x8C; break;
		case 'R': letter_value = 0xAF; break;		case 'r': letter_value = 0xAF; break;
		case 'S': letter_value = 0x92; break;		case 's': letter_value = 0x92; break;
		case 'T': letter_value = 0x87; break;		case 't': letter_value = 0x87; break;
		case 'U': letter_value = 0xC1; break;		case 'u': letter_value = 0xE3; break;
		case 'V': letter_value = 0xC1; break;		case 'v': letter_value = 0xE3; break;
		case 'W': letter_value = 0x8A; break;		case 'w': letter_value = 0x8A; break;
		case 'Y': letter_value = 0x91; break;		case 'y': letter_value = 0x91; break;
		case '0': letter_value = 0xC0; break;		case '5': letter_value = 0x92; break;
		case '1': letter_value = 0xF9; break;		case '6': letter_value = 0x82; break;
		case '2': letter_value = 0xA4; break;		case '7': letter_value = 0xF8; break;
		case '3': letter_value = 0xB0; break;		case '8': letter_value = 0x80; break;
		case '4': letter_value = 0x99; break;		case '9': letter_value = 0x90; break;
		case '_': letter_value = 0xF7; break;		case ' ': letter_value = 0xFF; break;
		case '.': letter_value = 0x7F; break;		case '-': letter_value = 0xBF; break;
		default : letter_value = 0xC9; break;
	}
	return letter_value;
}

/*/////  TODO LIST  /////
-----------------------
2. Include temperature display (CPU Temp or other)
3. Include CPU load, RAM usage, etc.
*/
