# climate-node-dht22

Standalone **ATtiny88** firmware for the Raspberry Pi IP Address board: read a **DHT22** and show **temperature** (°F) and **humidity** on the onboard 3-digit display—no Raspberry Pi required for normal operation.

**Full setup guide:** [docs/CLIMATE_NODE.md](../docs/CLIMATE_NODE.md)

## Quick reference

| Item | Value |
|------|--------|
| DHT22 data pin | **PC0** — P3 pin 1 (U1 pin 23) |
| Ambient dim (optional) | **PC1** — P3 pin 2; or set `USE_PHOTOSENSOR 0` and `PWM_MANUAL` |
| Sample interval | 6 seconds (one bus transaction) |
| Display timing | 3 s temperature, then 3 s humidity per cycle |
| Build output | `bin/climate_node.hex` |
| DHT algorithm | Ported from [Adafruit DHT-sensor-library](https://github.com/adafruit/DHT-sensor-library) `DHT.cpp` |

## Wiring

- DHT22 **data** → P3 pin 1 (PC0)
- DHT22 **VCC** / **GND** → board power
- **4.7k–10k** pull-up: data → VCC
- Optional dimmer: **R1/R2** divider + LDR to **P3 pin 2** (PC1) — see [CLIMATE_NODE.md](../docs/CLIMATE_NODE.md)

Brightness is set in `src/ambient_dim.c`:

- `USE_PHOTOSENSOR 1` — LDR on PC1, sampled ~1 s (`ADC_BRIGHT` 200, `ADC_DARK` 450)
- `USE_PHOTOSENSOR 0` — ignore LDR; set `PWM_MANUAL` 0–255 (255 = full bright)

Calibration in `src/main.c` (whole display units):

- `TEMP_OFFSET` — added to displayed temperature (currently °F), e.g. `-2` makes `78.0` → `76.0`
- `HUMIDITY_OFFSET` — added to displayed %RH, e.g. `-5` makes `H38` → `H33`

## Build

```bash
make clean && make
```

### Debug: ambient ADC level + live dimming

Shows **raw ADC** or **PWM percent (0–100)** on the display. Switch with `DEBUG_SHOW_PWM_PCT` in `src/debug_brightness.c` (`1` = percent, `0` = raw ADC).

- `ADC_BRIGHT` — ADC at or below → PWM 255 (brightest)
- `ADC_DARK` — ADC at or above → PWM 20 (dimmes; tune `PWM_MIN`)

```bash
make debug
sudo make flash-debug
```

Cover the LDR: percent drops (dimmer). Shine light: percent rises toward **100**. Set `DEBUG_SHOW_PWM_PCT` to `0` to see raw ADC instead.

## Flash (on Raspberry Pi, board on GPIO for SPI programming)

```bash
sudo make flash
```

Slower ISP if needed: see [docs/CLIMATE_NODE.md](../docs/CLIMATE_NODE.md).

## Source layout

| File | Role |
|------|------|
| `src/main.c` | Main loop, calibration offsets, 6 s sample / 3 s per value |
| `src/debug_brightness.c` | Debug image: raw ADC or PWM % on display |
| `src/dht22.c` | DHT22 single-wire read (Adafruit-style timing) |
| `src/dht22.h` | Reading struct and API |
| `src/display_pwm.c` | Timer0 ISR PWM brightness for 7-seg multiplex |
| `src/ambient_dim.c` | Photoresistor ADC → PWM duty (optional hardware) |

## Third-party credit

DHT22 timing logic in `src/dht22.c` is derived from Adafruit’s **DHT-sensor-library** (MIT). See [github.com/adafruit/DHT-sensor-library](https://github.com/adafruit/DHT-sensor-library).
