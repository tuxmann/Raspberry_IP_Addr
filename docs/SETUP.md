# Raspberry Pi IP Address Display - Setup Guide

This guide covers setting up the Pi IP Address board on a Raspberry Pi, including running the Python script and programming the ATtiny88 firmware.

## Hardware Requirements

- Raspberry Pi (any model with 40-pin GPIO)
- Pi IP Address add-on board (ATtiny88 + 3-digit 7-segment display)
- Board connected via GPIO header

## 1. Enable I2C and SPI

```bash
sudo raspi-config
```

Navigate to: **Interface Options** → **I2C** → **Enable**  
Then: **Interface Options** → **SPI** → **Enable**

Reboot after enabling.

## 2. Install Dependencies

### Python script (hostip.py)

```bash
# Option A: System packages (Raspberry Pi OS)
sudo apt update
sudo apt install python3-smbus i2c-tools

# Option B: Use smbus2 (recommended for broader compatibility)
pip install smbus2
```

### AVR programming (for flashing ATtiny88)

```bash
sudo apt install avrdude
```

> **Note:** Some Raspberry Pi OS versions may ship avrdude without `linuxspi` support. If `avrdude -c linuxspi` fails, you may need to [build avrdude from source](https://github.com/avrdudes/avrdude) with linuxspi enabled.

## 3. Verify I2C Board Detection

```bash
i2cdetect -y 1
```

You should see `5a` in the output when the board is connected. On older Pi models, try `-y 0` instead.

## 4. Run the IP Display Script

```bash
# From the project directory
python3 hostip.py
```

The script will:
- Detect the I2C bus with the board (0x5A)
- Read the Pi's IP address
- Send it to the display every 120 seconds

## 5. Run at Startup (Optional)

A systemd service runs the script automatically after boot:

```bash
# From your development machine, after deploying:
./deploy-to-pi.sh --install-service

# Or manually on the Pi:
sudo cp ~/Raspberry_IP_Addr/hostip.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable hostip
sudo systemctl start hostip
```

Useful commands:
- `sudo systemctl status hostip` — check status
- `sudo systemctl stop hostip` — stop the service
- `sudo journalctl -u hostip -f` — view logs

## 6. Build Firmware (on development machine)

If you need to rebuild the ATtiny88 firmware:

```bash
# Install avr-gcc (Ubuntu/Debian)
sudo apt install gcc-avr avr-libc

# Build
make
```

This produces `bin/show_ip.hex`.

## 7. Flash Firmware to ATtiny88 (on Raspberry Pi)

The board must be connected to the Pi's SPI pins (CLK, MISO, MOSI, GND). **Important:** The ATtiny88 reset pin must be connected to a GPIO (e.g., GPIO 25 via gpiochip0), not ground—otherwise the chip cannot be programmed and I2C will not respond.

Flash with:

```bash
avrdude -c linuxspi -p t88 -P /dev/spidev0.0:/dev/gpiochip0 -B 10 -U flash:w:show_ip.hex:i
```

Or from the project root:
```bash
make flash
```

The `:i` suffix (or `-U flash:w:file.hex:i` in Intel hex format) ensures proper parsing.

## 8. Remote Setup via SSH

### One-time: Enable SSH key login (optional)

```bash
ssh-copy-id jason@raspberrypi.local
# Enter Pi password when prompted; future logins won't need it
```

### Deploy and run from your development machine

```bash
# Copy project to Pi
scp -r /path/to/Raspberry_IP_Addr jason@raspberrypi.local:~/

# SSH in and run
ssh jason@raspberrypi.local "cd Raspberry_IP_Addr && python3 hostip.py"
```

Or use the deploy script: `./deploy-to-pi.sh` (defaults to jason@raspberrypi.local)

### Build and flash firmware from your development machine

Deploy source, compile on the Pi, and flash in one step (avoids flashing stale hex):

```bash
./build-and-flash-on-pi.sh
```

The Pi needs build tools: `sudo apt install gcc-avr avr-libc avrdude`

## 9. Climate node firmware (DHT22, optional)

To use the board as a **standalone** temp/humidity display (no `hostip.py`), build and flash the sub-project on the Pi:

```bash
cd ~/Raspberry_IP_Addr/climate-node-dht22
make clean && make
sudo make flash
```

See **[CLIMATE_NODE.md](CLIMATE_NODE.md)** for DHT22 wiring (P3 pin 1), power, display format, and troubleshooting.

## Troubleshooting

| Issue | Solution |
|-------|----------|
| "Pi IP Address board not detected" | Enable I2C in raspi-config, check connections. Ensure reset pin is **not** tied to ground. |
| Board at 0x5A not found / avrdude fails | Reset pin must connect to GPIO 25 (or gpiochip0 reset), not GND. With reset grounded, the ATtiny88 cannot run or respond on I2C. |
| "Permission denied" on /dev/i2c-* | Add user to i2c group: `sudo usermod -aG i2c $USER` |
| avrdude "linuxspi" not found | Build avrdude from source or use USBasp programmer |
| Wrong IP displayed | `hostname --all-ip-addresses` returns first interface; use `ip addr` to verify |
