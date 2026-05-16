#!/usr/bin/env python3
"""
demo_status.py

Demo script for the general-purpose ATtiny88 firmware (show_msg.c).
It gathers real system data on a Raspberry Pi (or similar SBC) and
displays short 2–3 character messages on the 3-digit 7-seg display.

Ideas demonstrated (from ideas-additional.txt, lines 3–10):
  - Scrolling message index (shows a mode/index number)
  - CPU load (average load × 100, or "Idl" when idle)
  - RAM usage (percent used)
  - CPU temperature (e.g. "C45" or "52.")
  - Disk usage (root filesystem percent full)
  - Wi‑Fi signal (RSSI, e.g. "-65" or "65 ")
  - Simple notifications ("Ok", "Err", "Net", "IP")
  - Speedtest result (download Mbps as 3 digits, if speedtest-cli is available)

The ATtiny88 side accepts an I2C block write:
  - I2C address: 0x5A (must match show_msg.c ADDR)
  - First byte = control/register (ignored by firmware, use 0x00)
  - Remaining bytes = ASCII message (e.g. b"125", b"52.", b"Err")

Message rules (implemented in show_msg.c):
  - Up to 3 characters are displayed.
  - '.' sets the decimal point on the previous digit.
  - Unsupported characters show as blank.
"""

import os
import shutil
import subprocess
import time
from typing import Optional, Tuple

try:
    from smbus2 import SMBus  # type: ignore[import]
except ImportError:
    from smbus import SMBus  # type: ignore[no-redef]

I2C_ADDR_HEX_STR = "5A"   # must match ADDR in show_msg.c (0x5A)
I2C_ADDR = int(I2C_ADDR_HEX_STR, 16)


def detect_i2c_bus(addr_hex_str: str) -> int:
    """Detect which I2C bus has the board at the given hex address."""
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
            if addr_hex_str.lower() in result.stdout.lower():
                print(f"Found board at I2C address 0x{addr_hex_str} on bus {bus_num}")
                return bus_num
        except (subprocess.TimeoutExpired, FileNotFoundError, OSError) as e:
            print(f"Bus {bus_num}: {e}")
            continue
    raise SystemExit(
        "Pi IP Address / status board not detected. "
        "Ensure board is attached and I2C is enabled (raspi-config → Interface Options → I2C)."
    )


def send_display_message(bus: SMBus, text: str) -> None:
    """
    Send a short ASCII message to the ATtiny88 firmware.

    - First byte: control/register (0x00, ignored by firmware for now)
    - Following bytes: ASCII characters (message).
    """
    # Limit message length; firmware uses only the first 3 characters (+ decimal points)
    text = text[:8]
    data = [ord(c) & 0xFF for c in text]
    if not data:
        data = [ord(" ")]

    # Register/command byte is currently unused; set to 0x00
    control = 0x00
    bus.write_i2c_block_data(I2C_ADDR, control, data)


def get_cpu_load_x100() -> int:
    """
    Return 1-minute average load × 100, clamped to 0–999.
    Example: load 1.25 -> 125.
    """
    try:
        load1, _, _ = os.getloadavg()
        value = int(load1 * 100)
    except (OSError, ValueError):
        value = 0
    return max(0, min(999, value))


def get_ram_usage_percent() -> Optional[int]:
    """
    Return RAM usage percent (0–100) using 'free -m', or None on error.
    """
    try:
        result = subprocess.run(
            ["free", "-m"],
            capture_output=True,
            text=True,
            check=False,
        )
        if result.returncode != 0:
            return None
        for line in result.stdout.splitlines():
            if line.lower().startswith("mem:"):
                parts = line.split()
                if len(parts) >= 3:
                    total = float(parts[1])
                    used = float(parts[2])
                    if total > 0:
                        pct = int((used / total) * 100)
                        return max(0, min(100, pct))
        return None
    except Exception:
        return None


def get_cpu_temp_c() -> Optional[float]:
    """
    Return CPU temperature in °C as a float, or None if not available.
    On Raspberry Pi, /sys/class/thermal/thermal_zone0/temp is typical.
    """
    paths = [
        "/sys/class/thermal/thermal_zone0/temp",
        "/sys/class/hwmon/hwmon0/temp1_input",
    ]
    for path in paths:
        try:
            with open(path, "r", encoding="ascii") as f:
                raw = f.read().strip()
            milli_c = float(raw)
            # Some kernels expose temp directly in °C, others in milli-°C
            if milli_c > 200.0:
                return milli_c / 1000.0
            return milli_c
        except (FileNotFoundError, ValueError, OSError):
            continue
    return None


def get_root_disk_usage_percent() -> Optional[int]:
    """Return root filesystem usage percent (0–100), or None on error."""
    try:
        usage = shutil.disk_usage("/")
        total = float(usage.total)
        used = float(usage.used)
        if total <= 0:
            return None
        pct = int((used / total) * 100)
        return max(0, min(100, pct))
    except Exception:
        return None


def get_wifi_rssi_dbm() -> Optional[int]:
    """
    Try to get Wi‑Fi RSSI (signal level in dBm).
    Returns an integer like -65, or None if not connected / unavailable.
    """
    # Prefer 'iw dev wlan0 link' if present
    try:
        result = subprocess.run(
            ["iw", "dev", "wlan0", "link"],
            capture_output=True,
            text=True,
            timeout=3,
        )
        if result.returncode == 0 and "signal:" in result.stdout:
            for line in result.stdout.splitlines():
                if "signal:" in line:
                    parts = line.strip().split()
                    for p in parts:
                        if p.endswith("dBm"):
                            try:
                                return int(p.replace("dBm", ""))
                            except ValueError:
                                pass
        # Fall back to iwconfig
    except (FileNotFoundError, subprocess.TimeoutExpired, OSError):
        pass

    try:
        result = subprocess.run(
            ["iwconfig", "wlan0"],
            capture_output=True,
            text=True,
            timeout=3,
        )
        if result.returncode == 0:
            for line in result.stdout.splitlines():
                if "Signal level=" in line:
                    # Example: "Signal level=-65 dBm"
                    parts = line.split("Signal level=")[1].split()
                    if parts:
                        val = parts[0]
                        try:
                            return int(val)
                        except ValueError:
                            pass
    except (FileNotFoundError, subprocess.TimeoutExpired, OSError):
        pass
    return None


def run_speedtest_mbps(timeout_sec: int = 45) -> Optional[int]:
    """
    Run speedtest-cli (if installed) and return approximate download Mbps
    as an integer (0–999). Returns None if speedtest-cli is unavailable
    or fails.
    """
    speedtest_cmd = shutil.which("speedtest-cli") or shutil.which("speedtest")
    if not speedtest_cmd:
        return None

    try:
        result = subprocess.run(
            [speedtest_cmd, "--simple"],
            capture_output=True,
            text=True,
            timeout=timeout_sec,
        )
    except (subprocess.TimeoutExpired, OSError):
        return None

    if result.returncode != 0:
        return None

    # Parse line like: "Download: 125.34 Mbit/s"
    for line in result.stdout.splitlines():
        if line.lower().startswith("download:"):
            parts = line.split()
            if len(parts) >= 2:
                try:
                    mbps = float(parts[1])
                    value = int(mbps)
                    return max(0, min(999, value))
                except ValueError:
                    continue
    return None


def fmt_int_3(value: int) -> str:
    """Format an integer 0–999 as a 3-character, zero-padded string."""
    value = max(0, min(999, value))
    return f"{value:03d}"


def fmt_signed_3(value: int) -> str:
    """
    Format a signed integer into 3 characters:
      -99..-10  -> "-10", "-65"
      -9..-1    -> " -5" (space + sign + digit)
       0..99    -> " 65" (space + 2 digits)
      100..999  -> "999"
    """
    if value < -99:
        value = -99
    if value > 999:
        value = 999

    if value >= 0:
        if value < 100:
            return f"{value:2d}"  # e.g. " 5", "65"
        return "999"
    # negative
    if value <= -10:
        return f"{value:3d}"  # "-10", "-65"
    # -9..-1
    return f" {value:+2d}"[-3:]  # " -5"


def build_cpu_temp_display(temp_c: Optional[float]) -> str:
    """
    Return a 2–3 char string for CPU temperature.
    Examples:
      45.2 -> "C45"
      52.8 -> "52."
    """
    if temp_c is None:
        return "Tmp"
    # Round to nearest whole degree
    t = int(round(temp_c))
    if 0 <= t < 100:
        # Option 1: "C45"
        return f"C{t:02d}"
    if 100 <= t <= 199:
        # Option 2 for higher temps, show 3 digits
        return fmt_int_3(t)
    return "Ht "  # hot


def pick_notification(
    load_x100: int,
    ram_pct: Optional[int],
    disk_pct: Optional[int],
    wifi_rssi: Optional[int],
) -> str:
    """
    Choose a simple notification based on system health:
      - "Err" if disk > 90% or RAM > 90%
      - "Net" if Wi‑Fi RSSI is weak
      - "Run" otherwise
    """
    if disk_pct is not None and disk_pct > 90:
        return "Err"
    if ram_pct is not None and ram_pct > 90:
        return "Err"
    if wifi_rssi is not None and wifi_rssi < -75:
        return "Net"
    if load_x100 > 200:  # load > 2.0
        return "Run"
    return "Ok "


def cycle_demo(bus: SMBus, delay: float = 3.0) -> None:
    """
    Cycle through the ideas 3–10 with real data and send them to the display.
    """
    while True:
        # 3. Scrolling message index — here we just show an index value
        for idx in (1, 2, 3):
            msg = fmt_int_3(idx)
            print(f"[Scrolling index] mode={idx} -> '{msg}'")
            send_display_message(bus, msg)
            time.sleep(1.5)

        # 4. CPU load — Average load × 100 or "Idl"
        load_x100 = get_cpu_load_x100()
        if load_x100 < 5:  # idle-ish
            msg = "Idl"
        else:
            msg = fmt_int_3(load_x100)
        print(f"[CPU load] load_x100={load_x100} -> '{msg}'")
        send_display_message(bus, msg)
        time.sleep(delay)

        # 5. RAM usage — Percent used
        ram_pct = get_ram_usage_percent()
        if ram_pct is None:
            msg = "RAM"
        else:
            msg = fmt_int_3(ram_pct)
        print(f"[RAM usage] ram_pct={ram_pct} -> '{msg}'")
        send_display_message(bus, msg)
        time.sleep(delay)

        # 6. CPU temperature — "C45" or "52."
        temp_c = get_cpu_temp_c()
        msg = build_cpu_temp_display(temp_c)
        print(f"[CPU temp] temp_c={temp_c} -> '{msg}'")
        send_display_message(bus, msg)
        time.sleep(delay)

        # 7. Disk usage — root filesystem percent full
        disk_pct = get_root_disk_usage_percent()
        if disk_pct is None:
            msg = "dSK"
        else:
            msg = fmt_int_3(disk_pct)
        print(f"[Disk usage] disk_pct={disk_pct} -> '{msg}'")
        send_display_message(bus, msg)
        time.sleep(delay)

        # 8. Wi‑Fi signal — RSSI as a number
        wifi_rssi = get_wifi_rssi_dbm()
        if wifi_rssi is None:
            msg = "nWi"
        else:
            msg = fmt_signed_3(wifi_rssi)
        print(f"[Wi‑Fi] rssi={wifi_rssi} -> '{msg}'")
        send_display_message(bus, msg)
        time.sleep(delay)

        # 9. Simple notifications — “Err”, “Ok”, “Run”, “Net”, “IP”
        note = pick_notification(load_x100, ram_pct, disk_pct, wifi_rssi)
        print(f"[Notification] -> '{note}'")
        send_display_message(bus, note)
        time.sleep(delay)

        # 10. Speedtest result — Download Mbps as 3 digits
        speed_mbps = run_speedtest_mbps()
        if speed_mbps is None:
            msg = "Stp"
        else:
            msg = fmt_int_3(speed_mbps)
        print(f"[Speedtest] mbps={speed_mbps} -> '{msg}'")
        send_display_message(bus, msg)
        # Longer pause after speedtest
        time.sleep(max(delay, 5.0))


def main() -> None:
    bus_num = detect_i2c_bus(I2C_ADDR_HEX_STR)
    print(f"Using I2C bus {bus_num} for address 0x{I2C_ADDR_HEX_STR}")

    with SMBus(bus_num) as bus:
        try:
            cycle_demo(bus)
        except KeyboardInterrupt:
            print("\nExiting on Ctrl+C.")


if __name__ == "__main__":
    main()

