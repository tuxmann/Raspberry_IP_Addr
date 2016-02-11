#
# show_ip.c
#
# Copyright 2015 Jason <aztuxmann@gmail.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
# MA 02110-1301, USA.
#
#
#

import os
import time
import re
import numpy as np
import smbus
import commands


i2caddr = 0x5A			# 	Address must correspond with shop_ip.c's address #7 bit address (will be left shifted to add the read write bit)
refresh = 120			#	Sets the time between sending data to the LED display

global bus

# Changes the IP addr to a 32-bit integer.
def ip_to_dword():

	st = commands.getoutput("/sbin/ifconfig | grep Bcast")

	if not st: 	# Let the user know that we don't see an IP address
		time.sleep (5)
		return int(4278255360)	# Returns 255.0.255.0
	
	else:		# Let the user know that we see an IP address
		# The line below takes the output of the terminal command, segregates the 
		# IP address, and then removes the dots to create an array.
		st = commands.getoutput("/sbin/ifconfig | grep Bcast").split()[1][5:].split(".")
		return int("%02x%02x%02x%02x" % (int(st[0]),int(st[1]),int(st[2]),int(st[3])),16)

while True:
	bus = smbus.SMBus(1)	# 0 = /dev/i2c-0 (port I2C0), 1 = /dev/i2c-1 (port I2C1)
	dword_integer = ip_to_dword()
	
	# Separate the 32-bit integer into 8 bit integers.
	# Also use np.int to change type of data to an integer before sending.
	MSO = np.int(dword_integer / 16777216)				#	Most Significant Octet
	SMSO = np.int((dword_integer % 16777216) / 65536)		#	Second Most Significant Octet
	SLSO = np.int(((dword_integer % 16777216) % 65536) / 256)	#	Second Most Significant Octect
	LSO = np.int(((dword_integer % 16777216) % 65536) % 256)	#	Least Significant Octet
		
	# Display separated octets for debugging.
	# print MSO
	# print SMSO
	# print TMSO
	# print LMSO
	
###	This portion takes the IP address and sends it to the add-on board	###
	
	# This is kind of rube goldberg-ish because the MSO in the "char cmd" or register parameter.
	ip_values = [SMSO, SLSO, LSO]
	
	bus.write_i2c_block_data(i2caddr, MSO, ip_values)

	time.sleep(refresh)
