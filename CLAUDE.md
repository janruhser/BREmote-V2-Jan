# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

BREmote V2 is an open-source wireless remote control system for electric foils (eFoils) and electric skateboards. It is a dual-unit system: a handheld **Transmitter (Tx)** and a board-mounted **Receiver (Rx)** communicating over LoRa 868/915MHz radio, interfacing with VESC motor controllers.

**Current version:** V2.2.4 (SW_VERSION = 2) | **License:** GPL 3.0 | **Platform:** ESP32 (Arduino framework)

## Build & Flash

This is an Arduino IDE / PlatformIO project targeting ESP32. There are no Makefiles or test suites.

- **Open** `Source/V2_Integration_Tx/V2_Integration_Tx.ino` (or `Source/V2_Integration_Rx/V2_Integration_Rx.ino`) in Arduino IDE
- **Board:** ESP32 WROOM-32 variant
- **Upload speed:** 921600 baud
- **Export binary:** Sketch > Export Compiled Binary (produces `.bin` in sketch directory)
- **Flash via esptool:** `esptool.py --chip esp32 -p COM3 --baud 921600 write_flash -z 0x1000 <firmware>.bin`

### Required Libraries
| Library | Version | Purpose |
|---------|---------|---------|
| RadioLib | 7.1.2 | SX1262 LoRa transceiver driver |
| Adafruit_ADS1X15 | 2.5.0 | ADS1115 16-bit ADC |
| Wire, SPI, Ticker, SPIFFS, FreeRTOS, mbedtls | ESP32 core | Built-in |

## Architecture

### Dual-Unit Design

Both Tx and Rx are Arduino sketches split into multiple `.ino` files (Arduino IDE auto-concatenates them). Each file owns a functional domain:

**Transmitter (`Source/V2_Integration_Tx/`):**
| File | Responsibility |
|------|---------------|
| `V2_Integration_Tx.ino` | Entry point: `setup()` / `loop()`, peripheral init sequence, FreeRTOS task creation |
| `BREmote_V2_Tx.h` | All configuration (`confStruct`), pin definitions, globals, display character bitmaps |
| `Radio.ino` | SX1262 LoRa init, pairing protocol (3-step handshake), 100ms data TX, telemetry RX |
| `Display.ino` | HT16K33 7x10 LED matrix driver over I2C, bargraphs, animations, scrolling text |
| `Hall.ino` | Throttle/toggle input processing, exponential curve shaping, gear/menu system, lock/unlock |
| `Analog.ino` | ADS1115 ADC continuous sampling, 6-sample ring buffer filtering, battery voltage |
| `SPIFFS.ino` | Config persistence (Base64-encoded `confStruct`), MAC-derived address generation |
| `System.ino` | Serial command interface, hardware variant detection, deep sleep, charger handling |

**Receiver (`Source/V2_Integration_Rx/`):**
| File | Responsibility |
|------|---------------|
| `V2_Integration_Rx.ino` | Entry point |
| `BREmote_V2_Rx.h` | Rx-side configuration struct and pin definitions |
| `Radio.ino` | LoRa reception, pairing response, telemetry transmission |
| `VESC.ino` | VESC UART protocol (custom implementation with `vesc_buffer.*`, `vesc_crc.*`) |
| `PWM.ino` | Motor speed PWM output |
| `GPS.ino` | Optional GPS module (NMEA parsing) |
| `SPIFFS.ino` | Config persistence (mirrors Tx) |
| `System.ino` | Init, bind LED patterns |

### FreeRTOS Tasks (Transmitter)
| Task | Period | Priority | Purpose |
|------|--------|----------|---------|
| `sendData` | 100ms | 5 | Transmit throttle+steering to Rx |
| `waitForTelemetry` | Triggered after TX | 4 | Receive Rx telemetry response |
| `measBufCalc` | 10ms | 6 | ADC sampling & ring buffer filtering |
| `updateBargraphs` | 200ms | 6 | Refresh display status bars |
| `loop()` | Main | Default | Menu system, serial commands, display |

### Radio Protocol
- **Physical:** SX1262, implicit header, 250kHz BW, SF6, CRC8 validation
- **Pairing:** 3-step handshake (Tx sends 0xAB, Rx responds 0xBA, Tx confirms 0xAC) with bidirectional address exchange
- **Control packet (6 bytes):** `[DestAddr(3), Throttle(0-255), Steering(0-255), CRC8]` at 10Hz
- **Telemetry response (6 bytes):** `[SrcAddr(3), DataIndex, DataValue, CRC8]` — cycles through battery, temp, speed, error

### Configuration System
All settings live in `confStruct` (defined in the header file). The struct is Base64-encoded and stored to SPIFFS at `/data.txt`. Configuration can be modified via:
- Serial commands: `?setConf:<base64>` then `?applyConf` then `?reboot`
- Web config tool: https://lbre.de/BREmote/struct.html

Key config parameters: `radio_preset` (1=868MHz EU, 2=915MHz US/AU), `rf_power`, `max_gears`, `thr_expo` (throttle curve), `no_lock`, `no_gear`, `steer_enabled`, `gps_en`.

## Serial Debug Commands (Tx)
Connect at 115200 baud. Enter USB mode at startup with THR+Left toggle.
```
?conf          — Print current config
?setConf:XXX   — Load Base64 config
?applyConf     — Apply SPIFFS config to RAM
?clearSPIFFS   — Delete stored config
?reboot        — Restart
?printRSSI     — Live signal strength (type "quit" to stop)
?printInputs   — Live throttle/steering values
?printTasks    — FreeRTOS stack usage
?printPackets  — TX/RX packet counts
```

## Startup Button Combinations
**Tx:** Left toggle=Calibrate, Right toggle=Pair, THR+Left=USB mode, THR+Right=Delete SPIFFS
**Rx:** Bind button=Pair, Both buttons=Delete config

## Key Design Patterns
- **All globals are `volatile`** because they are shared across FreeRTOS tasks
- **Ring buffer filtering** (size 6) on all ADC channels for noise rejection
- **Throttle expo curve** blends linear and quadratic: `thr_expo` 0-49 = soft start, 50 = linear, 51-100 = aggressive
- **Toggle blocking** prevents accidental gear changes while steering is active (governed by `tog_block_time`)
- **5-minute failsafe** deep sleep if no radio connection (emergency battery saver)
- **`serialOff` flag** disables Serial to prevent voltage on USB magnet pins when not connected
- **Config versioning** — `SW_VERSION` must match stored config version or defaults are loaded (shows `ESV` error)
