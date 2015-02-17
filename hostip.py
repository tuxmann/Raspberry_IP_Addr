# Raw Source code, needs to be cleaned up.
import os
import re
from subprocess import *
import numpy as np
import smbus

# 	Linux command to get IP addres
cmd = "hostname -I"

#	Function to Change the IP addr to a 32-bit integer.
### 	I think I can send this to the ATMEL directly	###
def ip_to_int(cmd):
	
	p = Popen(cmd, shell=True, stdout=PIPE)
	st = p.communicate()[0]
	st = st.split(".")
	return int("%02x%02x%02x%02x" % (int(st[0]),int(st[1]),int(st[2]),int(st[3])),16)

# Change the IP addr to a 32-bit integer.
dword_integer = ip_to_int(cmd)

#	Most Significant Octet
MSO = dword_integer / 16777216

#	The remainder of after the first Octet is removed.
MSO_rem = dword_integer % 16777216

#	Second Most Significant Octet
SMSO = MSO_rem / 65536

#	Remainder from Second Most Significant Octet
SMSO_rem = MSO_rem % 65536

#	Third Most Significant Octect
TMSO = SMSO_rem / 256

#	Least Significant Octet
LMSO = SMSO_rem % 256


# 	Print the IP addr and separated numbers.
print MSO
print SMSO
print TMSO
print LMSO

MSO = np.int(MSO)
SMSO = np.int(SMSO)
TMSO = np.int(TMSO)
LMSO = np.int(LMSO)

###		This portion takes the IP address and sends it to the add-on board 		###

i2caddr = 0x5A			# address must correspond with shop_ip.c's address
bus = smbus.SMBus(1)    # 0 = /dev/i2c-0 (port I2C0), 1 = /dev/i2c-1 (port I2C1)

DEVICE_ADDRESS = i2caddr      #7 bit address (will be left shifted to add the read write bit)

# This is kind of rube goldergish. 
DEVICE_REG_LEDOUT0 = MSO
ledout_values = [SMSO, TMSO, LMSO, 0x00]

bus.write_i2c_block_data(DEVICE_ADDRESS, DEVICE_REG_LEDOUT0, ledout_values)
