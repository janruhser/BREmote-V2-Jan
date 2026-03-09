# BREmote V2 Fork

**Fork of [Luddi96/BREmote-V2](https://github.com/Luddi96/BREmote-V2)**

This repository contains custom modifications and additional features. See the [original repository](https://github.com/Luddi96/BREmote-V2) for base documentation, hardware specs, and flashing instructions.

## ⚠️ Experimental

This fork is under active development. Use at your own risk.

---

## New Features in This Fork

### 🌐 WiFi Setup
WiFi Access Point starts automatically at boot for wireless configuration:
- Starts automatically on every power-on
- Auto-generated password from device MAC address
- Web interface for all settings
- Auto-shutdown after configurable timeout if no client connects (default: 120s)
- Can be manually controlled via serial `?wifi on/off`

### 🔌 Serial Setup (USB)
Full configuration over USB serial without WiFi:
- All settings accessible via serial commands
- Works even when TX is locked
- Compatible with automated tooling and scripts

### 📊 JSON Config Export
Machine-readable configuration output:

```
?conf       → Human-readable config (original)
?conf json  → JSON format (NEW)
```

Returns all configuration values as parseable JSON.

### 🛠️ Config Converter Tool
Browser-based configuration editor:

**[Open Config Converter](Tools/config_converter.html)**

- Decode Base64 config strings from `?conf` output
- Edit individual fields with validation
- Re-encode and upload back to device
- Works offline without device connection

### 📺 Additional Display Info (TX)
Extended telemetry display modes:
- **Temperature** (T/P): Motor/controller temperature
- **Speed** (5/P): GPS speed in km/h or knots  
- **Power** (P/V): Power consumption in watts
- **Battery** (B/A): Foil battery percentage
- **Throttle** (T/H): Current throttle percentage
- **Internal Battery** (U/B): TX battery voltage

### ⚡ Dynamic Power Mode (TX)
New throttle mode for gradual power control:

- **Mode 0**: Traditional gears (fixed steps)
- **Mode 1**: No gears (direct throttle)
- **Mode 2**: Dynamic power cap (NEW)
  - Adjustable power ceiling (10-100%)
  - Real-time adjustment with toggle buttons
  - Configurable step size per press
  - Visual feedback on display

### 🔧 Hardware Test Suite
Python-based automated testing framework:

```bash
cd Tools
python -m bremote              # Run all tests
python -m bremote --link       # Radio link test
python -m bremote --wifi       # WiFi config tests
python -m bremote --interactive # Interactive tests
```

**Tests:**
- Device auto-detection
- Radio TX/RX functionality
- Display, Hall sensors, ADC (TX)
- VESC, PWM, battery (RX)
- Config validation
- Radio link correlation

---

## Files Added/Modified

```
Tools/
├── bremote/                    # NEW: Python test framework
│   ├── __main__.py
│   ├── device.py
│   ├── runner.py
│   └── tests/
│       ├── tx_tests.py
│       ├── rx_tests.py
│       ├── config_tests.py
│       ├── wifi_tests.py
│       └── link_test.py
│
├── bremote_test.py            # Standalone test script
└── config_converter.html      # Browser config editor

Source/
├── V2_Integration_Tx/         # MODIFIED: Added new features
├── V2_Integration_Rx/         # MODIFIED: Added JSON config
└── Common/                    # MODIFIED: Shared config engine
```

---

## License

GNU General Public License v3.0 (same as original)
