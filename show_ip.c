// Raw Source code for the ATtiny88
// Divide by 8 clock fuse needs to be turned off.
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

// Flashing command on the RPi
// sudo avrdude -c linuxspi -p t88 -P /dev/spidev0.0 -U flash:w:show_ip.hex 

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

#define F_CPU 2000000UL

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

#define pwr_word_count 8
#define time_out_count 8

//global variables
char *pwr_on_msg[pwr_word_count]  = {"rPI"," IP"," by","JAY","vEr"," 1.1","gPL"," 3 "};
char *timeout_msg[time_out_count] = {"rPI.","y_U"," no","get"," IP","Adr","For","uSr"};
char **message;		// Point of a pointer.
char charLED[3];

// I2C variables
#define BUFLEN_RECV 12
volatile uint8_t ip_received = 0;         					// flag for I2C transaction started
volatile uint8_t ip_count = 0;
volatile uint8_t recv[BUFLEN_RECV]; 						//buffer to store received bytes
uint8_t led1, led2, led3;
volatile uint8_t ipaddr[4];
uint8_t led1, led2, led3, length;

#define BUFLEN_TRAN 3
volatile uint8_t t_index=0;					// sudo i2cget 1 0x03 0x00b
volatile uint8_t tran[BUFLEN_TRAN] = {0x02, 0x34, 0x56}; 	//test bytes to transmit
volatile uint8_t reset=0;					//variable to indicate if something went horribly wrong

 //prototypes
int ip_to_led (	uint8_t IPvar );
int str_to_led ();
int hex_to_segment ( uint8_t hex_value );
int letter_to_segment ( char letter_value );

//---------------MAIN---------------------------------------------
int main()
{
	TWAR = (ADDR<<1); 	  			//load slave address 0x5A
	TWCR = (1<<TWINT) | (1<<TWEA) | (1<<TWEN) | (1<<TWIE);		//enable I2C hardware
	
	DDRB = 0xFF;	// 7 segments with decimal point	PORTB
	DDRD = 0x07;	// Digits to be selected.		PORTD

	sei();					// Enable Global Interrupts

	/* Power On & Timeout Display Message */
	message = pwr_on_msg;	// Set "message" as the power on message.
	while(1)
	{
		int timeout;
		int count;
		int i;
		
		for (timeout = 20; timeout > 0; timeout--)
		{
			if (ip_received == 1)	// If the IP address is received it 
			{			// will begin displaying the IP addr
				break;			// Break out of pwr_on_msg loop
			}
			
			for (count = 0; count < pwr_word_count; count++) 
			{
				PORTB = 0xFF;			// Blank the display in
				_delay_ms(900);			// between numbers
				str_to_led( count ); 		// Convert the string into three 7-seg digits
				for (i =2000; i > 1; i--)	// Cycle through the LED digits
				{
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
		}		// At the end of 20 loops, if no IP address is sent, fall through
	
		message = timeout_msg;	// Set "message" as the timeout message.
	
		if (ip_received == 1)	// If the IP address is received it 
		{			// we avoid the timeout loop below and 
		break;			// break out of the while loop.
		}
	}
	/* End Power On & Timeout Display Message */

	// Main program to cycle through IP Addr Octets.
	while(1)
	{
		PORTB = 0xFF;			// Blank the display
		_delay_ms(4000);		// for about 0.5 seconds.
		int count1 = 0;
		int i;		
		for (count1 = 0; count1 <4; count1++)
		{
			PORTB = 0xFF;			// Blank the display in
			_delay_ms(900);			// between numbers
			ip_to_led( ipaddr[count1] );	// Convert the current octet
			// ip_to_led ( i2c_value );
			for (i =2000; i > 1; i--)	// Cycle through the LED digits
			{
				PORTB = led1;
				PORTD = LEFT;
				_delay_ms(1);
			
				PORTB = led2;
				PORTD = MIDDLE;
				_delay_ms(1);
			
				PORTB = led3;
				PORTD = RIGHT;
				_delay_ms(1);
			}
		};
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
			ipaddr[ip_count] = TWDR;
			ip_count++;
			//i2c_value = TWDR;
			//don't ack next data if buffer is full
			if(ip_count >= BUFLEN_RECV)
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

/* -----CHANGE STRINGS INTO LED VALUES-----------------------
		
-------------------------------------------------------------*/
int str_to_led (word_count)
{
	length = strlen(message[word_count]);	// See how many characters in a word

	int letter = 1;
	int digit = 1;
	int mask_char = 0x00;
	charLED[0] = letter_to_segment ( message[word_count][0] ); // First char always processed
	
	for (letter = 1; letter < length; letter++) // Count starts at 1 for 2nd char in the word
	{
		if (message[word_count][letter] == '.')	// Is this char a "."?
		{
			mask_char = mask_char & (~ message[word_count][letter-1]); // mask decimal point of previous char
			if ( mask_char != 0x80)		// Does the previous char have no decimal point?
			{
				charLED[digit-1] = charLED[digit-1] & 0x7F; // Add a decimal point to the previous char
			}
		}

		else 	// Convert the letter to the proper 7-segment hex value
		{
			charLED[digit] = letter_to_segment ( message[word_count][letter] );
			digit++;	// move to the next 7-seg digit
			mask_char = 0x00;	// Reset the mask char
		}
	}
	return 0;
}

// ----------------------------------------------------------
//		CHANGE EACH OCTET TO LED VALUES
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
3. Make optional text scrolling across the digits
4. Include CPU load, RAM usage, etc.
*/
