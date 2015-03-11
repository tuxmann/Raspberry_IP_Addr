// Raw Source code for the ATtiny88
// Divide by 8 clock fuse needs to be turned off.
/*
 * show_ip.c
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

/******************************************
 * 
 * Have the text on the IP addr scroll across
 * rather than having it change all 3 digits
 * at once.
 *
******************************************/

#define F_CPU 1000000UL
// 13.8s for two IP loops at 1000000

#include <avr/io.h>
#include <util/twi.h>
#include <util/delay.h>
#include <avr/interrupt.h>


#define SET(x,y) (x|=(1<<y))
#define CLR(x,y) (x&=(~(1<<y)))
#define CHK(x,y) (x&(1<<y)) 
#define TOG(x,y) (x^=(1<<y))

//setup the I2C hardware to ACK the next transmission
#define TWACK (TWCR=(1<<TWINT)|(1<<TWEN)|(1<<TWEA)|(1<<TWIE))		//and indicate that we've handled the last one.
#define TWNACK (TWCR=(1<<TWINT)|(1<<TWEN)|(1<<TWIE))				//setup the I2C hardware to NACK the next transmission
#define TWRESET (TWCR=(1<<TWINT)|(1<<TWEN)|(1<<TWSTO)|(1<<TWEA)|(1<<TWIE))	//reset the I2C hardware (used when the bus is in a illegal state)

#define LEFT	0x80
#define MIDDLE  0x40
#define RIGHT	0x20
#define ADDR	0x5A

//global variables
#define BUFLEN_RECV 12
//volatile uint8_t r_index =0;
volatile uint8_t ip_count = 0;
volatile uint8_t recv[BUFLEN_RECV]; 							//buffer to store received bytes
uint8_t led1, led2, led3;
volatile uint8_t ipaddr[4] = { 1, 2, 3, 4 };
// volatile uint8_t i2c_value = 0x00;

#define BUFLEN_TRAN 3
volatile uint8_t t_index=0;
								// sudo i2cget 1 0x03 0x00b
volatile uint8_t tran[BUFLEN_TRAN] = {0x02, 0x34, 0x56}; 	//test bytes to transmit

volatile uint8_t reset=0;									//variable to indicate if something went horribly wrong

 //prototypes
int ip_to_led (	uint8_t IPvar );
int hex_to_segment ( uint8_t hex_value );

//---------------MAIN---------------------------------------------
int main()
{
	TWAR = (ADDR<<1); 	  			//load slave address 0x5A
	TWCR = (1<<TWINT) | (1<<TWEA) | (1<<TWEN) | (1<<TWIE);		//enable I2C hardware
	
	DDRB = 0xFF;	// 7 segments with decimal point
	DDRD = 0x07;	// Digits to be selected.

	// Enable Global Interrupts
	sei();
	while(1)
	{
		PORTB = 0xFF;
		_delay_ms(2000);
		int count1 = 0;
		int i;		
		for (count1 = 0; count1 <4; count1++)
		{
			PORTB = 0xFF;
			_delay_ms(900);
			ip_to_led( ipaddr[count1] );
			// ip_to_led ( i2c_value );
			for (i =4000; i > 1; i--)
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

int ip_to_led ( uint8_t IPvar )
{
	// Parse each digit so the digit can have 
	// it's corresponding segment value
    led1 = IPvar / 100;
    led2 = (IPvar % 100) / 10;
    led3 = (IPvar % 100) % 10;
    
    // Change each digit into a segment value
    led1 = hex_to_segment (led1);
    led2 = hex_to_segment (led2);
    led3 = hex_to_segment (led3);
    return IPvar;
}

/*
0 = 0xC0	4 = 0x99	7 = 0xF8
1 = 0xF9	5 = 0x92	8 = 0x80
2 = 0xA4	6 = 0x82	9 = 0x90
3 = 0xB0	*/

int hex_to_segment ( uint8_t hex_value )
{
	switch ( hex_value )
	{
		case 0x00:
			hex_value = 0xC0;
			break;

		case 0x01:
			hex_value = 0xF9;
			break;

		case 0x02:
			hex_value = 0xA4;
			break;

		case 0x03:
			hex_value = 0xB0;
			break;

		case 0x04:
			hex_value = 0x99;
			break;

		case 0x05:
			hex_value = 0x92;
			break;

		case 0x06:
			hex_value = 0x82;
			break;

		case 0x07:
			hex_value = 0xF8;
			break;

		case 0x08:
			hex_value = 0x80;
			break;

		case 0x09:
			hex_value = 0x90;
			break;
	}
	return hex_value;
}

/*
--------
A = 0x88
b = 0x83
C = 0xC6
d = 0xA1
E = 0x86
F = 0x8E
g = 0x90
H = 0x89
h = 0x8B
I = 0xCF
J = 0xE1
L = 0xC7
n = 0xAB
o = 0xA3
P = 0x8C
r = 0xAF
t = 0x87
U = 0xC1x
u = 0xE3
y = 0x91
_ = 0xF7
*/
