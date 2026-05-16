# climate-node-dht22

Standalone **ATtiny88** firmware for the Raspberry Pi IP Address board: read a **DHT22** and show **temperature** (°F) and **humidity** on the onboard 3-digit display—no Raspberry Pi required for normal operation.

**Full setup guide:** [docs/CLIMATE_NODE.md](../docs/CLIMATE_NODE.md)

## Quick reference

| Item | Value |
|------|--------|
| DHT22 data pin | **PC0** — P3 pin 1 (U1 pin 23) |
| Sample interval | 5 seconds (one bus transaction) |
| Build output | `bin/climate_node.hex` |
| DHT algorithm | Ported from `DHT_sensor_library/DHT.cpp` (Adafruit) |

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
| `src/main.c` | Display multiplexing, 5 s schedule, temp/humidity formatting |
| `src/dht22.c` | DHT22 single-wire read (Adafruit-style timing) |
| `src/dht22.h` | Reading struct and API |
| `DHT_sensor_library/` | Reference Adafruit library (Arduino); logic ported in `dht22.c` |

## License

Project firmware follows the same GPL terms as the parent repository. `DHT_sensor_library/` is MIT (Adafruit)—see `DHT_sensor_library/license.txt`.
