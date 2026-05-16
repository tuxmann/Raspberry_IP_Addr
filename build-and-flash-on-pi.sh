#!/bin/bash
# Deploy source to Pi, compile there, and flash the ATtiny88.
# This ensures you always flash firmware built from the source you just deployed.
# Usage: ./build-and-flash-on-pi.sh [user] [host]
# Prerequisites: ssh-copy-id, Pi has gcc-avr avr-libc avrdude (apt install gcc-avr avr-libc avrdude)

set -e
PI_USER="${1:-jason}"
PI_HOST="${2:-raspberrypi.local}"
PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"

echo "Deploying to ${PI_USER}@${PI_HOST}..."
rsync -avz --exclude '.git' --exclude '__pycache__' --exclude '*.pyc' \
    --exclude '.venv' \
    "$PROJECT_DIR/" "${PI_USER}@${PI_HOST}:~/Raspberry_IP_Addr/"

echo ""
echo "Building on Pi and flashing ATtiny88..."
ssh "${PI_USER}@${PI_HOST}" 'cd ~/Raspberry_IP_Addr && \
  (command -v avr-gcc >/dev/null 2>&1 || { echo "Install build tools on Pi: sudo apt install gcc-avr avr-libc"; exit 1; }) && \
  make && \
  sudo avrdude -c linuxspi -P /dev/spidev0.0:/dev/gpiochip0 -p t88 -B 10 -U flash:w:bin/show_ip.hex:i'

echo ""
echo "Done. Firmware built from current source and flashed."
