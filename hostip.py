#
# HostIP.py
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

global bus, port_number

i2caddr = 0x5A		# Address must correspond with shop_ip.c's address #7 bit address (will be left shifted to add the read write bit)
refresh = 120		# Sets the time between sending data to the LED display


# Changes the IP addr to a 32-bit integer.
def ip_to_dword():

	ip_addr = commands.getoutput("/sbin/ifconfig | grep Bcast")

	if not ip_addr: 	# Let the user know that we don't see an IP address
		return int(4278255360)	# Returns 255.0.255.0
	
	else:	# Let the user know that we see an IP address
		# The line below takes the output of the terminal command, segregates the 
		# IP address, and then removes the dots to create an array.
		ip_addr = ip_addr.split()[1][5:].split(".")
		return int("%02x%02x%02x%02x" % (int(ip_addr[0]),int(ip_addr[1]),int(ip_addr[2]),int(ip_addr[3])),16)

# Check to see which I2C port the Pi IP Adress board is attached to. Doing this makes the script more compatible
# with the Orange Pi, Old Raspberry Pi, Newer Raspberry Pis, Banana Pi, Odroid and other pin compatible SBCs.
check_port0 = commands.getoutput("i2cdetect -y 0").split(" ")[0]	# Get the first string for each command.
check_port1 = commands.getoutput("i2cdetect -y 1").split(" ")[0]	# The result should yield "error" or nothing.
check_port2 = commands.getoutput("i2cdetect -y 2").split(" ")[0]	# We want to see nothing as the returned value.

if check_port0 == '':
	check_port0 = commands.getoutput("i2cdetect -y 0").split(" ")[137]	# Checking to see if 5a is detected on port zero
	if check_port0 == '5a':
		port_number = 0

elif check_port1 == '':
	check_port1 = commands.getoutput("i2cdetect -y 1").split(" ")[137]	# Checking to see if 5a is detected on port one
	if check_port1 == '5a':
		port_number = 1

elif check_port2 == '':
	check_port2 = commands.getoutput("i2cdetect -y 2").split(" ")[137]	# Checking to see if 5a is detected on port two
	if check_port2 == '5a':
        	port_number = 2
if port_number == 42:
	print "\033[1;31;40m Pi IP Address board not detected. Please ensure board is properly attached."
	print ""
	quit()

bus = smbus.SMBus(port_number)	# 0 = /dev/i2c-0 (port I2C0), 1 = /dev/i2c-1 (port I2C1)

while True:
	bus = smbus.SMBus(1)	# 0 = /dev/i2c-0 (port I2C0), 1 = /dev/i2c-1 (port I2C1)
	dword_integer = ip_to_dword()
	
	# Separate the 32-bit integer into 8 bit integers.
	# Also use np.int to change type of data to an integer before sending.
	MSO = np.int(dword_integer / 16777216)				#	Most Significant Octet
	SMSO = np.int((dword_integer % 16777216) / 65536)		#	Second Most Significant Octet
	SLSO = np.int(((dword_integer % 16777216) % 65536) / 256)	#	Second Least Significant Octect
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
