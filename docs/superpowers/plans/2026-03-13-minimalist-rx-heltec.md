# Minimalist RX (Heltec HT-CT62) Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a streamlined, single-output receiver firmware for the Heltec HT-CT62 (ESP32-C3) that eliminates legacy hardware dependencies while establishing a Hardware Abstraction Layer (HAL).

**Architecture:** A "Clean Fork" in `Source/V2_Minimalist_Rx_Heltec`. Business logic (Radio, VESC, Config) will be ported from the existing RX codebase and modified to interface strictly through `HAL.h`.

**Tech Stack:** C++ / Arduino Framework (ESP32-C3 Core), RadioLib, FreeRTOS.

---

## Chunk 1: Foundation and HAL

### Task 1: Scaffolding and Minimal Configuration Header

**Files:**
- Create: `Source/V2_Minimalist_Rx_Heltec/BREmote_V2_Rx_Heltec.h`
- Create: `Source/V2_Minimalist_Rx_Heltec/tests/mock_test_task1.py`

- [ ] **Step 1: Write the failing test**
```python
# Source/V2_Minimalist_Rx_Heltec/tests/mock_test_task1.py
import os, sys
header_path = 'Source/V2_Minimalist_Rx_Heltec/BREmote_V2_Rx_Heltec.h'
if not os.path.exists(header_path):
    print("FAIL: File not found")
    sys.exit(1)
with open(header_path, 'r') as f:
    content = f.read()
    if 'struct confStruct' not in content or 'dummy_delete_me' in content:
        print("FAIL: confStruct not correctly defined or legacy fields present")
        sys.exit(1)
print("PASS")
```

- [ ] **Step 2: Run test to verify it fails**
Run: `python Source/V2_Minimalist_Rx_Heltec/tests/mock_test_task1.py`
Expected: FAIL: File not found

- [ ] **Step 3: Write minimal implementation**
Create `Source/V2_Minimalist_Rx_Heltec/BREmote_V2_Rx_Heltec.h`. Copy the structure from `Source/V2_Integration_Rx/BREmote_V2_Rx.h` but remove fields related to `steering_type`, `steering_influence`, `steering_inverted`, `trim`, `PWM1_min`, `PWM1_max`, `bms_det_active`, `wet_det_active`, `dummy_delete_me`, `ubat_cal`, and `ubat_offset`. Retain the core variables (version, radio_preset, rf_power, PWM0_min, PWM0_max, failsafe_time, foil_num_cells, gps_en, logger_en, etc.). Retain the `TelemetryPacket` struct.

- [ ] **Step 4: Run test to verify it passes**
Run: `python Source/V2_Minimalist_Rx_Heltec/tests/mock_test_task1.py`
Expected: PASS

- [ ] **Step 5: Commit**
```bash
git add Source/V2_Minimalist_Rx_Heltec/BREmote_V2_Rx_Heltec.h Source/V2_Minimalist_Rx_Heltec/tests/mock_test_task1.py
git commit -m "feat(rx-minimal): create minimalist configuration header"
```

### Task 2: Implement the Hardware Abstraction Layer (HAL)

**Files:**
- Create: `Source/V2_Minimalist_Rx_Heltec/HAL.h`
- Create: `Source/V2_Minimalist_Rx_Heltec/HAL_Heltec_CT62.h`
- Create: `Source/V2_Minimalist_Rx_Heltec/tests/mock_test_task2.py`

- [ ] **Step 1: Write the failing test**
```python
# Source/V2_Minimalist_Rx_Heltec/tests/mock_test_task2.py
import os, sys
hal_path = 'Source/V2_Minimalist_Rx_Heltec/HAL_Heltec_CT62.h'
if not os.path.exists(hal_path):
    print("FAIL: File not found")
    sys.exit(1)
with open(hal_path, 'r') as f:
    content = f.read()
    if 'void hal_init()' not in content or 'void hal_esc_write(uint16_t value)' not in content or 'hal_get_vesc_uart' not in content:
        print("FAIL: HAL interfaces missing")
        sys.exit(1)
print("PASS")
```

- [ ] **Step 2: Run test to verify it fails**
Run: `python Source/V2_Minimalist_Rx_Heltec/tests/mock_test_task2.py`
Expected: FAIL: File not found

- [ ] **Step 3: Write minimal implementation**
Create `HAL.h` declaring the external functions: `hal_init`, `hal_esc_init`, `hal_esc_write`, `hal_radio_switch_mode`, `hal_get_vesc_uart`, `hal_get_gps_uart`.
Create `HAL_Heltec_CT62.h` with the actual definitions for the ESP32-C3:
- Pin definitions: `P_LORA_NSS 8`, `P_LORA_SCK 10`, `P_LORA_MOSI 6`, `P_LORA_MISO 7`, `P_LORA_BUSY 1`, `P_LORA_RST 5`, `P_LORA_DIO 3`, `P_LORA_ANT_SW 0`, `P_ESC_OUT 9`, `P_VESC_RX 2`, `P_VESC_TX 4`, `P_GPS_RX 20`, `P_GPS_TX 21`.
- Implement `hal_init()` to set up `Serial` (native CDC), initialize the LoRa pins (specifically `pinMode(P_LORA_ANT_SW, OUTPUT)`), and begin `Serial1` for VESC and `Serial0` for GPS.
- Implement `hal_esc_init()` using `ledcSetup` and `ledcAttachPin` for standard PWM on `P_ESC_OUT`.
- Implement `hal_esc_write(uint16_t value)`.
- Implement `hal_radio_switch_mode(bool tx)` (write `P_LORA_ANT_SW` HIGH for TX, LOW for RX).
- Implement `HardwareSerial& hal_get_vesc_uart()` returning `Serial1`.
- Implement `HardwareSerial& hal_get_gps_uart()` returning `Serial0`.

- [ ] **Step 4: Run test to verify it passes**
Run: `python Source/V2_Minimalist_Rx_Heltec/tests/mock_test_task2.py`
Expected: PASS

- [ ] **Step 5: Commit**
```bash
git add Source/V2_Minimalist_Rx_Heltec/HAL*.h Source/V2_Minimalist_Rx_Heltec/tests/mock_test_task2.py
git commit -m "feat(rx-minimal): implement hardware abstraction layer for HT-CT62"
```

## Chunk 2: Business Logic Porting (Radio and Output)

### Task 3: Port Radio Logic

**Files:**
- Create: `Source/V2_Minimalist_Rx_Heltec/Radio.ino`
- Modify: `Source/V2_Minimalist_Rx_Heltec/tests/mock_test_task3.py`

- [ ] **Step 1: Write the failing test**
```python
# Source/V2_Minimalist_Rx_Heltec/tests/mock_test_task3.py
import os, sys
radio_path = 'Source/V2_Minimalist_Rx_Heltec/Radio.ino'
if not os.path.exists(radio_path):
    print("FAIL: File not found")
    sys.exit(1)
with open(radio_path, 'r') as f:
    content = f.read()
    if 'hal_radio_switch_mode' not in content:
        print("FAIL: Radio not using HAL switch mode")
        sys.exit(1)
print("PASS")
```

- [ ] **Step 2: Run test to verify it fails**
Run: `python Source/V2_Minimalist_Rx_Heltec/tests/mock_test_task3.py`
Expected: FAIL: File not found

- [ ] **Step 3: Write minimal implementation**
Copy `Source/V2_Integration_Rx/Radio.ino` to `Source/V2_Minimalist_Rx_Heltec/Radio.ino`.
Modify it to use the new `hal_radio_switch_mode()` function right before `radio.transmit()` and `radio.startReceive()`. Update the `SX1262 radio = new Module(...)` initialization to use the HAL pin macros. Ensure `triggeredReceive()` logic only deals with single ESC/throttle updates (`hal_esc_write`) and ignores dual motor steering logic.

- [ ] **Step 4: Run test to verify it passes**
Run: `python Source/V2_Minimalist_Rx_Heltec/tests/mock_test_task3.py`
Expected: PASS

- [ ] **Step 5: Commit**
```bash
git add Source/V2_Minimalist_Rx_Heltec/Radio.ino Source/V2_Minimalist_Rx_Heltec/tests/mock_test_task3.py
git commit -m "feat(rx-minimal): port radio logic using HAL"
```

### Task 4: Port Main Application and VESC Loop

**Files:**
- Create: `Source/V2_Minimalist_Rx_Heltec/V2_Minimalist_Rx_Heltec.ino`
- Copy: `Source/V2_Integration_Rx/vesc_datatypes.h`, `vesc_buffer.h`, `vesc_buffer.cpp`, `vesc_crc.h`, `vesc_crc.cpp`
- Create: `Source/V2_Minimalist_Rx_Heltec/VESC.ino`

- [ ] **Step 1: Write the failing test**
```python
# Source/V2_Minimalist_Rx_Heltec/tests/mock_test_task4.py
import os, sys
vesc_path = 'Source/V2_Minimalist_Rx_Heltec/VESC.ino'
if not os.path.exists(vesc_path):
    print("FAIL: File not found")
    sys.exit(1)
with open(vesc_path, 'r') as f:
    if 'hal_get_vesc_uart' not in f.read():
        print("FAIL: Not using HAL UART")
        sys.exit(1)
print("PASS")
```

- [ ] **Step 2: Run test to verify it fails**
Run: `python Source/V2_Minimalist_Rx_Heltec/tests/mock_test_task4.py`
Expected: FAIL: File not found

- [ ] **Step 3: Write minimal implementation**
Copy the VESC auxiliary files into the new directory.
Copy `Source/V2_Integration_Rx/VESC.ino` to `Source/V2_Minimalist_Rx_Heltec/VESC.ino`. Modify it to retrieve the UART instance by calling `HardwareSerial& serial = hal_get_vesc_uart();` instead of using the globally defined `Serial1`.
Create `V2_Minimalist_Rx_Heltec.ino`. It should `#include "BREmote_V2_Rx_Heltec.h"` and `"HAL_Heltec_CT62.h"`. Implement `setup()` to call `hal_init()`, `hal_esc_init()`, initialize the radio, and start the FreeRTOS tasks. Implement `loop()` to handle the VESC telemetry polling and watchdog resets.

- [ ] **Step 4: Run test to verify it passes**
Run: `python Source/V2_Minimalist_Rx_Heltec/tests/mock_test_task4.py`
Expected: PASS

- [ ] **Step 5: Commit**
```bash
git add Source/V2_Minimalist_Rx_Heltec/V2_Minimalist_Rx_Heltec.ino Source/V2_Minimalist_Rx_Heltec/VESC.ino Source/V2_Minimalist_Rx_Heltec/vesc_* Source/V2_Minimalist_Rx_Heltec/tests/mock_test_task4.py
git commit -m "feat(rx-minimal): implement main loop and VESC integration using HAL"
```

## Chunk 3: Configuration Services

### Task 5: Port Configuration and Storage Files

**Files:**
- Copy: `Source/V2_Integration_Rx/SPIFFS.ino`, `WebConfig.ino`, `ConfigService.ino`

- [ ] **Step 1: Write the failing test**
```python
# Source/V2_Minimalist_Rx_Heltec/tests/mock_test_task5.py
import os, sys
cfg_path = 'Source/V2_Minimalist_Rx_Heltec/ConfigService.ino'
if not os.path.exists(cfg_path):
    print("FAIL: File not found")
    sys.exit(1)
with open(cfg_path, 'r') as f:
    if 'aw.digitalWrite' in f.read() or 'steering_type' in f.read():
        print("FAIL: Legacy hardware calls or struct fields remain in ConfigService")
        sys.exit(1)
print("PASS")
```

- [ ] **Step 2: Run test to verify it fails**
Run: `python Source/V2_Minimalist_Rx_Heltec/tests/mock_test_task5.py`
Expected: FAIL: File not found

- [ ] **Step 3: Write minimal implementation**
Copy `SPIFFS.ino`, `WebConfig.ino`, and `ConfigService.ino` from `V2_Integration_Rx` to `V2_Minimalist_Rx_Heltec`.
Modify `ConfigService.ino` to remove all usages of the `aw` (AW9523) object and references to removed configuration variables (`steering_type`, `ubat_cal`, etc.). Ensure `getConfFromSPIFFS()` and `saveConfToSPIFFS()` align strictly with the new minimalist `confStruct`.

- [ ] **Step 4: Run test to verify it passes**
Run: `python Source/V2_Minimalist_Rx_Heltec/tests/mock_test_task5.py`
Expected: PASS

- [ ] **Step 5: Commit**
```bash
git add Source/V2_Minimalist_Rx_Heltec/SPIFFS.ino Source/V2_Minimalist_Rx_Heltec/WebConfig.ino Source/V2_Minimalist_Rx_Heltec/ConfigService.ino Source/V2_Minimalist_Rx_Heltec/tests/mock_test_task5.py
git commit -m "feat(rx-minimal): port and strip configuration and storage logic"
```