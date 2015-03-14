/*
 * happy.c
 * 14.03.2015 15:14:00
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



#define F_CPU 1000000UL		// CPU frequency

#include <avr/io.h>
#include <util/delay.h>

#define LEFT	0x80
#define MIDDLE	0x40
#define RIGHT	0x20

// PROTOTYPES

int main (void)
{
	DDRB = 0xFF;	// 7 segments with decimal point
	DDRD = 0x07;	// Digits to be selected.


	for (;;)
	{
		PORTB = 0xFF;
		_delay_ms(950);
		int i;		
		PORTB = 0xFF;
		_delay_ms(50);
		for (i =2000; i > 1; i--)
		{
			PORTB = 0x89;	// H = 0x89
			PORTD = LEFT;
			_delay_ms(1);
		
			PORTB = 0x88;	// A = 0x88
			PORTD = MIDDLE;
			_delay_ms(1);
		
			PORTB = 0x8C;	// P = 0x8C
			PORTD = RIGHT;
			_delay_ms(1);
		}

		PORTB = 0xFF;
		_delay_ms(50);
		for (i =2000; i > 1; i--)
		{
			PORTB = 0x8C;	// P = 0x8C
			PORTD = LEFT;
			_delay_ms(1);
		
			PORTB = 0x91;	// y = 0x91
			PORTD = MIDDLE;
			_delay_ms(1);
		
			PORTB = 0xFF;	// BLANK = 0xFF
			PORTD = RIGHT;
			_delay_ms(1);
		}

		PORTB = 0xFF;
		_delay_ms(50);
		for (i =2000; i > 1; i--)
		{
			PORTB = 0x8C;	// P = 0x8C
			PORTD = LEFT;
			_delay_ms(1);
		
			PORTB = 0xCF;	// I = 0xCF
			PORTD = MIDDLE;
			_delay_ms(1);
		
			PORTB = 0xFF;
			PORTD = RIGHT;
			_delay_ms(1);
		}

		PORTB = 0xFF;
		_delay_ms(50);
		for (i =2000; i > 1; i--)
		{
			PORTB = 0xA1;	// d = 0xA1
			PORTD = LEFT;
			_delay_ms(1);
		
			PORTB = 0x88;	// A = 0x88
			PORTD = MIDDLE;
			_delay_ms(1);
		
			PORTB = 0x91;	// y = 0x91
			PORTD = RIGHT;
			_delay_ms(1);
		}

		PORTB = 0xFF;
		_delay_ms(50);
		for (i =2000; i > 1; i--)
		{
			PORTB = 0x30;	// 3. = 0x30
			PORTD = LEFT;
			_delay_ms(1);
		
			PORTB = 0xF9;	// 1 = 0xF9
			PORTD = MIDDLE;
			_delay_ms(1);
		
			PORTB = 0x99;	// 4 = 0x99
			PORTD = RIGHT;
			_delay_ms(1);
		}
	}
return 0;
}

/*
0 = 0xC0
1 = 0xF9
2 = 0xA4
3 = 0xB0
4 = 0x99
5 = 0x92
6 = 0x82
7 = 0xF8
8 = 0x80
9 = 0x90

Add 0x80 Then x
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
