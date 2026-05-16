# Raspberry Pi IP Address Display

![Image of IP_Addr](https://github.com/tuxmann/Raspberry_IP_Addr/blob/master/images/IMG_1232-sm.JPG)

![Image of IP_Addr side](https://github.com/tuxmann/Raspberry_IP_Addr/blob/master/images/IMG_1226-sm.JPG)

A hardware add-on that displays your Raspberry Pi's IP address on a 3-digit 7-segment LED display. Compatible with all Raspberry Pi models and other pin-compatible SBCs (Orange Pi, Banana Pi, Odroid, etc.).

## Quick Start

1. **Enable I2C and SPI:** `sudo raspi-config` → Interface Options
2. **Install:** `sudo apt install python3-smbus i2c-tools` (or `pip install smbus2`)
3. **Run:** `python3 hostip.py`
4. **Auto-start at boot:** `./deploy-to-pi.sh --install-service` (after deploying)

See **[docs/SETUP.md](docs/SETUP.md)** for the full setup guide, including ATtiny88 programming and the reset pin requirement.

## Hardware

- **Board:** ATtiny88 drives a 3-digit ELT-316 common-anode 7-segment display
- **Communication:** I2C at address 0x5A
- **Size:** Sits 0.6" (1.5cm) above the Pi; total height ~1" (2.5cm) with headers
- **Expansion:** P3 (left) and P4 (right) connectors bring out Port C & D (ADC, analog comparator, external interrupts)

## Features

- Power-on message ("rPI IP by JAY v1.1 GPL 3")
- Timeout message if no IP received within ~5 minutes
- Cycles through IP octets (e.g., 192 → 168 → 1 → 100)

## Climate node (DHT22, standalone)

The board can run **without a Pi** as a temperature/humidity display using a **DHT22** on header **P3 pin 1 (PC0)**.

- Firmware: [`climate-node-dht22/`](climate-node-dht22/)
- Setup, wiring, build, and flash: **[docs/CLIMATE_NODE.md](docs/CLIMATE_NODE.md)**

Shows °F (e.g. `78.3`) and humidity (e.g. `H38`), one sensor read every 5 seconds.

## Documentation

- [Setup Guide](docs/SETUP.md) — Pi IP display: installation, configuration, and flashing
- [Climate Node (DHT22)](docs/CLIMATE_NODE.md) — Standalone temp/humidity firmware
- [Releases](https://github.com/tuxmann/Raspberry_IP_Addr/releases) — Pre-built binaries and documents
- [Schematic](docs/Pi.IP.Addr.Schematic.pdf)
