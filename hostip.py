#!/usr/bin/env python3
# HostIP.py
#
# Copyright 2015 Jason <aztuxmann@gmail.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
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
# Dependencies (Raspberry Pi OS):
#   sudo apt-get install python3-smbus i2c-tools
# Or use smbus2 (pip): pip install smbus2

import subprocess
import time

try:
    from smbus2 import SMBus
except ImportError:
    from smbus import SMBus

i2caddr = "5A"		# Must match show_ip.c ADDR (0x5A); 7-bit I2C address
refresh = 120		# Sets the time between sending data to the LED display


# Changes the IP addr to a 32-bit integer.
def get_primary_ip_and_interface():
	"""Get primary IP address, its interface (eth/wlan), and convert IP to 32-bit integer for I2C."""
	# Default interface from default route (e.g. eth0 or wlan0)
	result = subprocess.run(
		["ip", "route", "show", "default"],
		capture_output=True,
		text=True,
		check=False,
	)
	interface = "eth0"
	if result.returncode == 0 and result.stdout.strip():
		# "default via 192.168.1.1 dev eth0 ..." or "... dev wlan0 ..."
		parts = result.stdout.strip().split()
		for i, p in enumerate(parts):
			if p == "dev" and i + 1 < len(parts):
				interface = parts[i + 1]
				break
	# Primary IP on that interface
	result = subprocess.run(
		["ip", "-4", "addr", "show", interface],
		capture_output=True,
		text=True,
		check=False,
	)
	ip_addr = ""
	if result.returncode == 0 and "inet " in result.stdout:
		# "inet 192.168.1.5/24 ..."
		for line in result.stdout.splitlines():
			if "inet " in line:
				ip_addr = line.strip().split()[1].split("/")[0]
				break
	if not ip_addr:
		# Fallback: hostname --all-ip-addresses (first address)
		result = subprocess.run(
			["hostname", "--all-ip-addresses"],
			capture_output=True,
			text=True,
			check=False,
		)
		ip_addr = result.stdout.strip().split()[0] if result.stdout else ""
	print(ip_addr, interface)

	if not ip_addr:
		return 0xFF00FF00, 0  # no IP; report as eth

	octets = ip_addr.split(".")
	if len(octets) != 4:
		return 0xFF00FF00, 0

	dword = int("%02x%02x%02x%02x" % (
		int(octets[0]), int(octets[1]), int(octets[2]), int(octets[3])
	), 16)
	# 0 = eth, 1 = wlan
	iface_byte = 1 if interface.startswith("wlan") else 0
	return dword, iface_byte

# Check to see which I2C port the Pi IP Adress board is attached to.
# Doing this makes the script more compatible with the Orange Pi, Old 
# Raspberry Pi, Newer Raspberry Pis, Banana Pi, Odroid and other 
# pin compatible SBCs.

# Detect which I2C bus has the Pi IP Address board (0x5A)
port_number = None
for bus_num in range(3):
	try:
		result = subprocess.run(
			["/usr/sbin/i2cdetect", "-y", str(bus_num)],
			capture_output=True,
			text=True,
			timeout=5,
		)
		if result.returncode != 0:
			continue
		if i2caddr.lower() in result.stdout.lower():
			port_number = bus_num
			print(f"Found board at I2C address 0x{i2caddr} on bus {bus_num}")
			break
	except (subprocess.TimeoutExpired, FileNotFoundError, OSError) as e:
		print(f"Bus {bus_num}: {e}")
		continue

if port_number is None:
	print("\033[1;31mPi IP Address board not detected. Ensure board is attached and I2C is enabled.")
	print("Run: sudo raspi-config → Interface Options → I2C → Enable\033[0m")
	raise SystemExit(1)

bus = SMBus(port_number)

while True:
	bus = SMBus(port_number)
	dword_integer, iface_byte = get_primary_ip_and_interface()

	# Separate the 32-bit integer into 8-bit octets for I2C transmission
	MSO = (dword_integer >> 24) & 0xFF   # Most Significant Octet
	SMSO = (dword_integer >> 16) & 0xFF  # Second Most Significant Octet
	SLSO = (dword_integer >> 8) & 0xFF   # Second Least Significant Octet
	LSO = dword_integer & 0xFF           # Least Significant Octet

	# Send 5 bytes: interface (0=eth, 1=wlan) then 4 IP octets.
	# write_i2c_block_data sends: addr, register_byte, [data...]
	# So register = iface_byte, data = [MSO, SMSO, SLSO, LSO]
	ip_values = [MSO, SMSO, SLSO, LSO]
	bus.write_i2c_block_data(int(i2caddr, 16), iface_byte, ip_values)

	time.sleep(refresh)
