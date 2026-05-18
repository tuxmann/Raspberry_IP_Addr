# climate-node-dht22

Standalone **ATtiny88** firmware for the Raspberry Pi IP Address board: read a **DHT22** and show **temperature** (°F) and **humidity** on the onboard 3-digit display—no Raspberry Pi required for normal operation.

**Full setup guide:** [docs/CLIMATE_NODE.md](../docs/CLIMATE_NODE.md)

## Quick reference

| Item | Value |
|------|--------|
| DHT22 data pin | **PC0** — P3 pin 1 (U1 pin 23) |
| Sample interval | 6 seconds (one bus transaction) |
| Display timing | 3 s temperature, then 3 s humidity per cycle |
| Build output | `bin/climate_node.hex` |
| DHT algorithm | Ported from [Adafruit DHT-sensor-library](https://github.com/adafruit/DHT-sensor-library) `DHT.cpp` |

## Wiring

- DHT22 **data** → P3 pin 1 (PC0)
- DHT22 **VCC** / **GND** → board power
- **4.7k–10k** pull-up: data → VCC

## Build

```bash
make clean && make
```

## Flash (on Raspberry Pi, board on GPIO for SPI programming)

```bash
sudo make flash
```

Slower ISP if needed: see [docs/CLIMATE_NODE.md](../docs/CLIMATE_NODE.md).

## Source layout

| File | Role |
|------|------|
| `src/main.c` | Display multiplexing, 6 s sample / 3 s per value, temp/humidity formatting |
| `src/dht22.c` | DHT22 single-wire read (Adafruit-style timing) |
| `src/dht22.h` | Reading struct and API |

## Third-party credit

DHT22 timing logic in `src/dht22.c` is derived from Adafruit’s **DHT-sensor-library** (MIT). See [github.com/adafruit/DHT-sensor-library](https://github.com/adafruit/DHT-sensor-library).
