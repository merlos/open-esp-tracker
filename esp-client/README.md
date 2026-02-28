# Open ESP Tracker – ESP32-S3 Client Firmware

Firmware for the Open ESP Tracker hardware based on an **ESP32-S3** SoC paired
with an **A7670E** 4G LTE + GPS module.  The device wakes from deep sleep at a
configurable interval, acquires a GNSS fix, and POSTs a JSON location report to
your back-end server over HTTPS.

---

## Table of Contents

1. [Hardware Requirements](#hardware-requirements)
2. [Pin Assignments](#pin-assignments)
3. [Required Arduino Libraries](#required-arduino-libraries)
4. [Development Toolchain](#development-toolchain)
5. [Building and Uploading](#building-and-uploading)
6. [Interactive Serial Interface (ISI)](#interactive-serial-interface-isi)
7. [Configuration Reference](#configuration-reference)
8. [LED Status Codes](#led-status-codes)
9. [JSON Payload Reference](#json-payload-reference)
10. [Troubleshooting](#troubleshooting)

---

## Hardware Requirements

| Component | Details |
|-----------|---------|
| MCU board | Waveshare ESP32-S3-A7670E (or compatible ESP32-S3 + A7670E carrier) |
| Modem | SIMCom A7670E (4G LTE Cat-1, integrated GNSS) |
| SIM card | Any 4G-capable SIM with data plan; configure APN via ISI |
| Antenna | LTE antenna + GPS/GNSS antenna (both typically included with the board) |
| Power | 3.7 V LiPo battery connected to the VBAT pads, **or** USB-C |

---

## Pin Assignments

| Signal | GPIO | Notes |
|--------|------|-------|
| Modem UART TX (ESP→Modem) | 17 | `MODEM_TX_PIN` |
| Modem UART RX (Modem→ESP) | 18 | `MODEM_RX_PIN` |
| Battery ADC | 1 | `VBAT_ADC_PIN` – resistor-divider input |
| RGB LED (WS2812B) | 48 | `LED_PIN` – onboard NeoPixel |
| USB Serial | USB-OTG (native) | ISI console |

> **Note:** Pin numbers reflect the Waveshare ESP32-S3-A7670E reference design.
> Adjust the `#define` values in `esp-client.ino` if you use a different
> carrier board.

---

## Required Arduino Libraries

Install all libraries before compiling.  Use the Arduino Library Manager
(**Sketch → Include Library → Manage Libraries…**) or `arduino-cli lib install`.

| Library | Version tested | Purpose |
|---------|----------------|---------|
| [TinyGSM](https://github.com/vshymanskyy/TinyGSM) | ≥ 0.11.7 | A7670E modem driver (AT commands, GPRS, GPS, HTTPS) |
| [ArduinoJson](https://arduinojson.org/) | ≥ 7.x | JSON serialisation of location payloads |
| [Adafruit NeoPixel](https://github.com/adafruit/Adafruit_NeoPixel) | ≥ 1.11 | WS2812B RGB LED control |

`Preferences.h`, `esp_sleep.h`, and `driver/rtc_io.h` are part of the
**ESP32 Arduino core** and do not need separate installation.

### arduino-cli (quick install)

```bash
# Install arduino-cli (Linux / macOS)
curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh

# Add the ESP32 board package index
arduino-cli config init
arduino-cli config add board_manager.additional_urls \
  https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json

# Update index and install ESP32 core
arduino-cli core update-index
arduino-cli core install esp32:esp32

# Install required libraries
arduino-cli lib install "TinyGSM"
arduino-cli lib install "ArduinoJson"
arduino-cli lib install "Adafruit NeoPixel"
```

---

## Development Toolchain

### Option A – Arduino IDE 2.x (recommended for beginners)

1. Open **Arduino IDE 2.x**.
2. Go to **File → Preferences** and add the ESP32 board URL:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
3. Go to **Tools → Board → Boards Manager**, search for `esp32`, and install
   the **Espressif Systems esp32** package.
4. Install the three libraries listed above via **Sketch → Include Library →
   Manage Libraries**.
5. Open `esp-client.ino`.

### Option B – arduino-cli (CI / command-line)

See the [Building and Uploading](#building-and-uploading) section below.

---

## Building and Uploading

### With Arduino IDE

1. Select **Tools → Board → ESP32S3 Dev Module** (or the specific Waveshare
   variant if available).
2. Set **Tools → USB Mode → USB-OTG (TinyUSB)** for native USB CDC serial.
3. Set **Tools → Upload Speed → 921600**.
4. Click **Upload** (Ctrl+U).

### With arduino-cli

```bash
# Compile (replace /dev/ttyACM0 with your port)
arduino-cli compile \
  --fqbn esp32:esp32:esp32s3 \
  --build-property "build.partitions=default_8MB" \
  esp-client/

# Upload
arduino-cli upload \
  --fqbn esp32:esp32:esp32s3 \
  --port /dev/ttyACM0 \
  esp-client/

# Monitor serial output (115200 baud)
arduino-cli monitor --port /dev/ttyACM0 --config baudrate=115200
```

> **Tip:** On Windows the port is usually `COM3`, `COM4`, etc.  On macOS it
> resembles `/dev/cu.usbmodem*`.

---

## Interactive Serial Interface (ISI)

The ISI lets you configure the tracker without recompiling the firmware.

### Entering the ISI

Connect a USB cable between the tracker and your computer, open a serial
terminal at **115200 baud**, then **reset** the ESP32-S3.  Within the first
**3 seconds** of boot, send any character (e.g. press Enter).  The ISI prompt
will appear.

```
=============================================
  Open ESP Tracker  –  Interactive Serial Interface
  Firmware: 1.0.0
=============================================

No ISI password is set.  Please create one now.
New password: ****
Confirm password: ****
Password saved.

Authenticated.  Type 'help' for available commands.

tracker>
```

On subsequent connections you will be asked for the password you created.

### ISI Commands

| Command | Description |
|---------|-------------|
| `help` | Show all available commands |
| `status` | Display current configuration and live device status |
| `get <param>` | Print the current value of a configuration parameter |
| `set <param> <value>` | Update and persist a configuration parameter |
| `reset` | Erase all NVS settings and restore compile-time defaults |
| `reboot` | Reboot the device |
| `exit` | Leave the ISI and resume normal tracker operation |

### Example Session

```
tracker> status

--- Device Status ---
  Firmware version   : 1.0.0
  Battery voltage    : 3.85 V
  Battery level      : 69 %

--- Configuration ---
  server_url         : https://your-server.example.com
  server_port        : 443
  api_token          : (not set)
  apn                : internet
  interval           : 60 s
  accuracy           : 20.0 m
  speed_threshold    : 0.50 m/s
  battery_low        : 20 %

tracker> set server_url https://tracker.example.com
  server_url = https://tracker.example.com  (saved)

tracker> set api_token mysecrettoken123
  api_token = mysecrettoken123  (saved)

tracker> set apn hologram
  apn = hologram  (saved)

tracker> exit
Exiting ISI.  Continuing normal operation.
```

---

## Configuration Reference

All parameters are persisted in the ESP32 NVS flash partition under the
`tracker` namespace.  Compile-time defaults are defined in `default_config.h`.

| Parameter | ISI name | Default | Description |
|-----------|----------|---------|-------------|
| Server URL | `server_url` | `https://your-server.example.com` | Base URL of the tracking back-end |
| Server port | `server_port` | `443` | HTTPS port |
| API token | `api_token` | _(empty)_ | Bearer token for `Authorization` header |
| APN | `apn` | `internet` | Cellular APN of your SIM |
| Wake interval | `interval` | `60` | Seconds between location reports |
| GPS accuracy | `accuracy` | `20.0` | Minimum fix accuracy in metres |
| Speed threshold | `speed_threshold` | `0.5` | Speed (m/s) below which device is considered stationary |
| Battery low | `battery_low` | `20` | Battery % that triggers low-battery flag |

### Compile-time-only Flags (edit `default_config.h`)

| Define | Default | Description |
|--------|---------|-------------|
| `SKIP_PASSWORD_SETUP` | `false` | Skip ISI password prompt (development only) |
| `DEBUG_MESSAGES` | `false` | Print verbose debug output to USB Serial |
| `DEFAULT_GPS_WAIT_SEC` | `60` | Maximum seconds to wait for a GPS fix |
| `FIRMWARE_VERSION` | `"1.0.0"` | Reported in the JSON payload and ISI status |

---

## LED Status Codes

The onboard WS2812B LED (GPIO 48) indicates the current device state:

| Colour | State |
|--------|-------|
| **White** | Booting / initialising |
| **Yellow** | Searching for GPS fix or setting up modem |
| **Green** | GPS fix acquired / data sent successfully |
| **Blue** | Transmitting data to server |
| **Purple** | ISI / configuration mode active |
| **Red** | Error (modem failure, GPRS failure, send failure) |
| **Off** | Deep sleep |

---

## JSON Payload Reference

Each successful report POSTs the following JSON to `POST /api/v1/locations`:

```json
{
  "device_id":       "AABBCCDDEEFF",
  "recorded_at":     "2024-06-01T12:34:56Z",
  "latitude":        51.507351,
  "longitude":       -0.127758,
  "altitude":        42.3,
  "speed":           1.38,
  "accuracy":        8.5,
  "battery_level":   72,
  "battery_voltage": 3.92,
  "battery_low":     false,
  "satellites":      9,
  "hdop":            1.7,
  "firmware_version":"1.0.0"
}
```

| Field | Type | Unit | Description |
|-------|------|------|-------------|
| `device_id` | string | – | Unique ID derived from ESP32 MAC address |
| `recorded_at` | string | ISO-8601 UTC | Time of GPS fix |
| `latitude` | float | degrees | WGS-84 latitude |
| `longitude` | float | degrees | WGS-84 longitude |
| `altitude` | float | metres | Altitude above sea level |
| `speed` | float | m/s | Ground speed |
| `accuracy` | float | metres | Estimated horizontal accuracy (HDOP × 5) |
| `battery_level` | integer | % | Battery state of charge |
| `battery_voltage` | float | V | Raw battery voltage |
| `battery_low` | boolean | – | `true` when battery level ≤ configured threshold |
| `satellites` | integer | – | Number of satellites used in fix |
| `hdop` | float | – | Horizontal dilution of precision |
| `firmware_version` | string | – | Firmware version string |

---

## Troubleshooting

### Modem does not respond

* Check that the A7670E is powered (PWR LED on carrier board).
* Verify `MODEM_TX_PIN` / `MODEM_RX_PIN` match your board's schematic.
* Try reducing `MODEM_BAUD` to `9600` as a diagnostic step.

### No GPS fix

* Ensure the GNSS antenna is connected and has a clear view of the sky.
* Increase `DEFAULT_GPS_WAIT_SEC` for indoor testing.
* Enable `DEBUG_MESSAGES` to see raw NMEA / AT output.

### GPRS connection fails

* Confirm the APN is correct for your SIM (`set apn <your-apn>` in ISI).
* Check that the SIM card is properly seated and has an active data plan.

### HTTPS POST fails

* Verify `server_url` and `server_port` in the ISI (`status` command).
* Check that `api_token` is set if the back-end requires authentication.
* The A7670E's TLS stack validates server certificates; ensure your server
  has a valid certificate chain.

### Battery percentage seems wrong

* Calibrate `VBAT_DIVIDER_RATIO`, `VBAT_FULL_VOLTAGE`, and
  `VBAT_EMPTY_VOLTAGE` in `esp-client.ino` to match your hardware.
