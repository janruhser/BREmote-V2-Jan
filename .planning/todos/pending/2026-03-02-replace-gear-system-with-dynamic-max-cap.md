---
created: 2026-03-02T14:35:38.691Z
title: "Throttle Refactoring: Dynamic Max Cap Mode"
area: general
files:
  - Source/V2_Integration_Tx/BREmote_V2_Tx.h
  - Source/V2_Integration_Tx/Throttle.ino
  - Source/V2_Integration_Tx/SPIFFS.ino
  - Source/V2_Integration_Tx/Radio.ino
  - Source/V2_Integration_Tx/V2_Integration_Tx.ino
  - Source/V2_Integration_Tx/Hall.ino
  - Source/V2_Integration_Tx/ConfigService.ino
  - Source/V2_Integration_Tx/Display.ino
  - Source/V2_Integration_Tx/System.ino
---

## Problem

The current throttle system uses discrete gears (1-10 steps) to limit max power. This is coarse — users can't set "exactly 50% max power" and must remember which gear maps to what.

## Solution

Centralize all throttle logic into a new `Throttle.ino` module and add a **dynamic max cap mode** where the toggle adjusts max power in real-time during riding, with full haptic resolution preserved at any cap level.

**Two modes after refactoring:**
- **Mode 0 (Gear):** Classic gear-based limiting (backward compatible, default)
- **Mode 1 (Cap):** Dynamic max cap — toggle adjusts cap in real-time with fixed 5% steps, `max_power_percent` sets starting value

**TODO (future):** Make cap step size a config parameter instead of hardcoded 5%.

Training mode is deferred — dynamic cap with a low default covers the use case.

## Implementation Plan

### Phase 1: Config Infrastructure

#### 1.1 Add fields to confStruct
**File:** `BREmote_V2_Tx.h`
- Append after `dest_address[3]` (end of struct):
  ```cpp
  uint16_t throttle_mode;       // 0=gear (classic), 1=dynamic cap
  uint16_t max_power_percent;   // 10-100, starting cap for mode 1 (default 100)
  ```
- Bump `SW_VERSION` from 2 to 3
- Update `defaultConf` initializer: append `, 0, 100`
- Add compile-time size check: `static_assert(sizeof(confStruct) == EXPECTED, "...")`

#### 1.2 Config migration (V2 → V3)
**File:** `SPIFFS.ino`

**readConfFromSPIFFS()**: Replace hard size check with flexible migration-aware loading:
```cpp
#define MIN_CONF_SIZE 80  // V2 struct size
if (decodedLen < MIN_CONF_SIZE) { /* reject as corrupted */ }
memset(&data, 0, sizeof(confStruct));
memcpy(&data, decodedData, min(decodedLen, sizeof(confStruct)));
```

**getConfFromSPIFFS()**: Replace hard-fail with migration:
```cpp
if (SW_VERSION != usrConf.version) {
    if (usrConf.version == 2) {
        usrConf.throttle_mode = 0;
        usrConf.max_power_percent = 100;
        if (usrConf.no_gear) {
            usrConf.throttle_mode = 1;
            usrConf.max_power_percent = 100;
        }
        usrConf.version = SW_VERSION;
        saveConfToSPIFFS(usrConf);
    } else {
        while(1) scroll3Digits(LET_E, 5, LET_V, 200);
    }
}
```

#### 1.3 ConfigService field table
**File:** `ConfigService.ino`
- Add `throttle_mode` and `max_power_percent` to `kCfgFields[]`
- Add cross-validation: if `no_gear==1` and `throttle_mode==0`, auto-set `throttle_mode=1`

#### 1.4 Serial status output
**File:** `System.ino`
- Add `throttle_mode` and `max_power_percent` to `printConfStruct()` and `serPrintStatus()` JSON

### Phase 2: Throttle Module

#### 2.1 Create Throttle.ino (NEW)
Key functions:
- `calcFinalThrottle()` — replaces inline Radio.ino calc, handles both modes
- `throttleGetStartupGear()` — startup gear selection
- `throttleResetCap()` — reset cap to configured default (boot/unlock)
- `throttleAdjustCap(direction)` — adjust cap by 5% step
- `throttleUsesGears()` — whether toggle changes gears
- `throttleForceToggleBlock()` — whether toggle is permanently blocked

#### 2.2 Wire into Radio.ino
Replace inline throttle calc with `sendArray[3] = calcFinalThrottle();`

#### 2.3 Wire into V2_Integration_Tx.ino
Replace startup gear block with `throttleGetStartupGear()` + `throttleResetCap()`

#### 2.4 Wire into Hall.ino
- Toggle handler: route to gear change, cap adjust, or display cycle based on mode
- Toggle blocking: replace `usrConf.no_gear && usrConf.no_lock` with `throttleForceToggleBlock()`
- Unlock: add `throttleResetCap()` after gear reset

#### 2.5 Display helper
**File:** `Display.ino`
- Add `showCapPercent()` — displays current power_cap_percent as 2-digit number
- Update `DISPLAY_MODE_THR` to show effective throttle when in cap mode

### Phase 3: Verification
1. Compile check
2. Static tests: `python Tools/bremote_test.py`
3. Migration test (V2 config on V3 firmware)
4. Mode 0 backward compatibility
5. Mode 1 functional test
6. `no_gear` legacy alias test
