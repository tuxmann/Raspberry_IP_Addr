# Climate Node (DHT22) — Setup Guide

Standalone use of the Raspberry Pi IP Address board **without** a Raspberry Pi host: an ATtiny88 reads a **DHT22** sensor and shows **temperature** and **humidity** on the built-in 3-digit display.

Firmware lives in [`climate-node-dht22/`](../climate-node-dht22/).

## Hardware

| Connection | Detail |
|------------|--------|
| DHT22 data | **PC0** — **U1 pin 23**, header **P3 pin 1** |
| Ambient light (optional) | **PC1 (ADC1)** — **P3 pin 2** |
| DHT22 VCC / GND | Board power (see power section below) |
| Data pull-up | **4.7k–10k** from data to VCC (required) |

**Do not use** pins already used by the 7-segment display, I2C (SDA/SCL), SPI (ISP), or UART if you may add those interfaces later.

### Optional photoresistor dimming (P3 pin 2)

Divider from **VCC** to **GND** with tap to **PC1**:

```text
VCC ---- R1 ----+---- PC1 (P3 pin 2)
                |
                +---- R2 ---- GND
                |
                LDR (optional, to GND, parallel with R2)
```

- Install **R1** and **R2** on boards that use dimming.
- **Bright room** → lower ADC → brighter display. **Dark room** → higher ADC → dimmer display.
- In `src/ambient_dim.c`:
  - `USE_PHOTOSENSOR 1` — read LDR once per second (`ADC_BRIGHT` **200**, `ADC_DARK` **450**, same as debug).
  - `USE_PHOTOSENSOR 0` — no LDR; set `PWM_MANUAL` **0–255** (255 = full brightness).

Tune `ADC_BRIGHT`, `ADC_DARK`, `PWM_MIN`, and `PWM_MANUAL` in `src/ambient_dim.c`.

### Standalone power (no Pi)

- Feed **5 V** and ground from USB (or similar).
- Many builds tie **P1 pins 1, 2, and 4** as 5 V tie points and use **P1 ground pins** (e.g. 6, 9, 14, 20) for return.
- Confirm your board’s actual rail (3.3 V vs 5 V for the MCU) matches how you power the DHT22.

### Programming (Pi + SPI)

To **flash** firmware, stack the board on a Raspberry Pi with **SPI enabled** and use the same **linuxspi** wiring as the original IP-display project (reset **not** held to ground). See [SETUP.md](SETUP.md) §7.

## Display behavior

| Phase | Display | Duration |
|-------|---------|----------|
| Power-up | `rdy` | 2 s |
| Each 6 s cycle | Temperature (°F), then humidity | 3 s each |
| Read failed (including disconnected sensor) | `Err` | Until next good read; replaces last values on screen |

**Temperature (°F)**

- Below 100 °F: one decimal (e.g. `78.3`)
- 100 °F and above: integer only (e.g. `104`)

**Humidity**

- `H` + two digits (e.g. `H38` for 38%)

**Calibration** (`src/main.c`):

- `TEMP_OFFSET` — whole degrees of the displayed scale (currently °F). Example: `-2` shows `76.0` instead of `78.0`.
- `HUMIDITY_OFFSET` — whole percent RH. Example: `-5` shows `H33` instead of `H38`.

Sensor is polled **once every 6 seconds** (`SAMPLE_PERIOD_MS` in `src/main.c`). Temperature and humidity each stay on screen for **3 seconds** (`SHOW_TEMP_MS`). DHT22 needs quiet time between reads; do not poll faster than about every 2 s.

**Brightness:** Timer0 ISR software PWM (`src/display_pwm.c`). With `USE_PHOTOSENSOR 1`, duty follows ambient ADC (**200** bright → **450** dark) about once per second. With `USE_PHOTOSENSOR 0`, duty is fixed at `PWM_MANUAL`.

## DHT driver

Bit timing is ported from **[Adafruit DHT-sensor-library](https://github.com/adafruit/DHT-sensor-library)** (`DHT.cpp`). The AVR driver is in `src/dht22.c` (cycle-count pulse compare, same algorithm).

## Build and flash

### On the Raspberry Pi (recommended)

```bash
cd ~/Raspberry_IP_Addr/climate-node-dht22
make clean && make
sudo make flash
```

If avrdude reports *device not responding*, use a slower ISP clock:

```bash
sudo avrdude -c linuxspi -P /dev/spidev0.0:/dev/gpiochip0 -p t88 -B 125kHz \
  -U flash:w:bin/climate_node.hex:i
```

Or slower:

```bash
sudo avrdude -c linuxspi -P /dev/spidev0.0:/dev/gpiochip0 -p t88 -B 50kHz \
  -U flash:w:bin/climate_node.hex:i
```

### On a development PC (compile only)

```bash
sudo apt install gcc-avr avr-libc
cd climate-node-dht22
make
```

Copy `bin/climate_node.hex` to the Pi and flash with `sudo make flash` or the `avrdude` line above.

### Deploy source from your PC

```bash
rsync -avz --exclude '.git' ./ jason@raspberrypi.local:~/Raspberry_IP_Addr/
```

Or from repo root: `./deploy-to-pi.sh` (syncs the whole tree, including `climate-node-dht22/`).

> **Note:** `deploy-to-pi.sh` option “build + flash” still targets the root **`show_ip`** firmware. For the climate node, run `make` / `sudo make flash` inside **`climate-node-dht22/`** on the Pi.

## Debug firmware (ADC level + live dimming)

Build and flash the debug image to tune **ADC_BRIGHT** / **ADC_DARK** in `src/debug_brightness.c` (defaults **200** / **450**, same as `src/ambient_dim.c`):

```bash
cd ~/Raspberry_IP_Addr/climate-node-dht22
make debug
sudo make flash-debug
```

Display shows **PWM percent 0–100** (`DEBUG_SHOW_PWM_PCT 1`) or **raw ADC** (`0`). Brightness tracks the reading in real time.

## Troubleshooting

| Symptom | Things to check |
|---------|------------------|
| `Err` on display | Data on **P3 pin 1**, pull-up, common GND; failed read shows within ~6 s; reflash latest hex |
| `rdy` forever | No successful read yet; wiring, power, sensor type (DHT22 not DHT11) |
| Wrong values | `F_CPU` in Makefile must match ATtiny fuse clock (default **8 MHz**) |
| avrdude no response | Board on Pi header, SPI on, reset not grounded; try `-B 125kHz` or `-B 50kHz` |
| Scope shows frequent DHT starts | Old firmware with retry bursts; use current build (one read per 6 s) |
| Display always dim / always bright | `USE_PHOTOSENSOR` / `PWM_MANUAL`; or `ADC_BRIGHT` / `ADC_DARK` in `ambient_dim.c` |
| No dimming with LDR installed | Set `USE_PHOTOSENSOR 1`; confirm LDR on **P3 pin 2** |

## Related docs

- [Main setup (Pi IP display)](SETUP.md)
- [Project README](../README.md)
- [climate-node-dht22 README](../climate-node-dht22/README.md)
