<p align="center">
  <img src="img/banner.png" alt="BREmote" width="600">
</p>

<h3 align="center">Open-Source Wireless Remote for eFoils & Electric Skateboards</h3>

<p align="center">
  <a href="#features">Features</a> •
  <a href="#hardware">Hardware</a> •
  <a href="#getting-started">Getting Started</a> •
  <a href="#configuration">Configuration</a> •
  <a href="#tools">Tools</a> •
  <a href="#license">License</a>
</p>

---

## Features

- **Long-range LoRa radio** — SX1262 at 868 MHz (EU) or 915 MHz (US/AU), 10 Hz control rate
- **VESC compatible** — Direct UART interface with VESC motor controllers
- **Dual-unit system** — Handheld transmitter (Tx) + board-mounted receiver (Rx)
- **Real-time telemetry** — Battery, temperature, speed, and signal strength displayed on the remote
- **Configurable throttle curves** — Exponential curve shaping from soft start to aggressive response
- **Steering support** — Optional second axis for differential steering setups
- **Multi-gear system** — Up to 5 configurable power levels
- **Safety features** — Throttle lock, 5-minute failsafe deep sleep, toggle blocking during steering
- **Over-the-air config** — WiFi Access Point with web-based configuration interface
- **LED matrix display** — HT16K33 7x10 dot matrix for status, animations, and bargraphs

## Hardware

| Component | Tx (Remote) | Rx (Board) |
|-----------|------------|------------|
| MCU | ESP32 WROOM-32 | ESP32 WROOM-32 |
| Radio | SX1262 LoRa | SX1262 LoRa |
| ADC | ADS1115 16-bit | — |
| Display | HT16K33 LED matrix | — |
| Motor | — | VESC (UART) |
| GPS | — | Optional NMEA module |
| Input | Hall throttle + 2 toggles | Bind button |

## Getting Started

### Prerequisites

- [Arduino IDE](https://www.arduino.cc/en/software) or PlatformIO
- ESP32 board support package
- Required libraries:

| Library | Version |
|---------|---------|
| [RadioLib](https://github.com/jgromes/RadioLib) | 7.1.2 |
| [Adafruit_ADS1X15](https://github.com/adafruit/Adafruit_ADS1X15) | 2.5.0 |

### Build & Flash

1. Open `Source/V2_Integration_Tx/V2_Integration_Tx.ino` (or `V2_Integration_Rx.ino` for the receiver) in Arduino IDE
2. Select board: **ESP32 WROOM-32**
3. Set upload speed to **921600 baud**
4. Click Upload

Or flash a precompiled binary:
```bash
esptool.py --chip esp32 -p COM3 --baud 921600 write_flash -z 0x1000 firmware.bin
```

### Pairing

1. Power on the Rx while holding the **Bind button**
2. Power on the Tx while holding the **Right toggle**
3. The 3-step handshake completes automatically — both units store each other's address

## Configuration

Settings are stored in a `confStruct` persisted to SPIFFS. There are three ways to configure:

### 1. WiFi Web Interface
Power on the Tx in USB mode (hold THR + Left toggle at startup). Run `?wifi on` over serial to start the WiFi AP, then connect and open the config page.

### 2. Serial Commands
Connect via USB at 115200 baud. Enter USB mode at startup (THR + Left toggle).
```
?conf              — Print current config
?get <key>         — Get a config value
?set <key> <value> — Set a config value
?save              — Persist to SPIFFS
?keys              — List all config fields
?reboot            — Reboot
```
Type `?` for the full command list.

### 3. Config Converter Tool

**[Open Config Converter](https://raw.githack.com/janruhser/BREmote-V2-Jan/main/Tools/config_converter.html)** — Browser-based tool to decode, edit, and re-encode BREmote config strings. Paste a Base64 config from `?conf` to inspect or modify individual fields.

### Key Parameters

| Parameter | Description | Default |
|-----------|-------------|---------|
| `radio_preset` | 1 = 868 MHz (EU), 2 = 915 MHz (US/AU) | 1 |
| `rf_power` | Transmit power | 22 |
| `max_gears` | Number of power levels (1–5) | 5 |
| `thr_expo` | Throttle curve (0–49 soft, 50 linear, 51–100 aggressive) | 50 |
| `no_lock` | Disable throttle lock | 0 |
| `steer_enabled` | Enable steering axis | 0 |

## Project Structure

```
Source/
├── V2_Integration_Tx/       # Transmitter firmware
│   ├── V2_Integration_Tx.ino   # Entry point (setup/loop)
│   ├── BREmote_V2_Tx.h         # Config struct, pins, globals
│   ├── Radio.ino                # LoRa communication & pairing
│   ├── Display.ino              # LED matrix driver
│   ├── Hall.ino                 # Throttle & toggle input
│   ├── Analog.ino               # ADC sampling & filtering
│   ├── SPIFFS.ino               # Config persistence
│   ├── System.ino               # Serial commands, sleep, USB
│   ├── ConfigService.ino        # Config engine
│   └── WebConfig.ino            # WiFi AP config interface
│
├── V2_Integration_Rx/       # Receiver firmware
│   ├── V2_Integration_Rx.ino   # Entry point
│   ├── BREmote_V2_Rx.h         # Config struct, pins, globals
│   ├── Radio.ino                # LoRa reception & telemetry
│   ├── VESC.ino                 # VESC UART protocol
│   ├── PWM.ino                  # Motor PWM output
│   ├── GPS.ino                  # Optional GPS module
│   ├── SPIFFS.ino               # Config persistence
│   ├── System.ino               # Init, bind patterns
│   ├── ConfigService.ino        # Config engine
│   └── WebConfig.ino            # WiFi AP config interface
│
Tools/
├── bremote_test.py          # Hardware test suite (GUI & CLI)
├── config_converter.html    # Browser-based config editor
└── README.md                # Test suite documentation
```

## Tools

### Hardware Test Suite

Automated and interactive testing for Tx and Rx units over USB serial. See [Tools/README.md](Tools/README.md) for full documentation.

```bash
pip install pyserial
python Tools/bremote_test.py          # Auto tests
python Tools/bremote_test.py --gui    # GUI mode
python Tools/bremote_test.py -i       # Interactive tests
```

### Config Converter

**[Open in Browser](https://raw.githack.com/janruhser/BREmote-V2-Jan/main/Tools/config_converter.html)** — Decode and edit BREmote config strings without a device connected.

## License

This project is licensed under the [GNU General Public License v3.0](LICENSE).
