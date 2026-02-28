# Technology Stack

**Project:** BREmote V2 — Extended VESC Telemetry
**Researched:** 2026-02-28
**Confidence:** HIGH (bitmask verified against existing working code + community documentation)

---

## Context: What This Covers

This document covers the specific technical stack decisions for extending the VESC UART telemetry
pipeline on the Rx unit. The existing system architecture (ESP32, RadioLib, FreeRTOS, hand-rolled
VESC UART) is not re-evaluated here — those decisions are settled. This research answers the three
narrowly-scoped questions this milestone depends on:

1. Exact `COMM_GET_VALUES_SELECTIVE` bitmask positions for all relevant fields
2. How to encode VESC float/int values into single bytes (0-255) for LoRa transmission
3. FreeRTOS/UART timing implications of requesting more values in the existing 100ms cycle

---

## COMM_GET_VALUES_SELECTIVE Bitmask — Complete Field Map

### How the 32-Bit Mask Is Sent

The command payload is 5 bytes: `[COMM_GET_VALUES_SELECTIVE (1 byte), mask MSB..LSB (4 bytes)]`

In the existing code the mask bytes are labeled as `vesc_command[1..4]` where `[1]` is the most
significant byte and `[4]` is the least significant byte. Bit positions below are given in
standard notation where bit 0 is the LSB of the 32-bit word (i.e., bit 0 lives in
`vesc_command[4]`, bit 8 lives in `vesc_command[3]`, etc.).

**Verification anchor:** The existing working code uses:
- `vesc_command[4] = (1 << FET_TEMP)` where `#define FET_TEMP 0` → bit 0 works in production
- `vesc_command[3] = (1 << BatVolt)` where `#define BatVolt 0` → bit 8 works in production
- VESC_PACK_LEN = 9 (no extras) or 19 (with MotCurrent+BatCurrent+Duty) — byte counts confirmed

### Response Format

The VESC echoes the 32-bit mask back in the response. The payload layout is:
`[cmd_id (1), mask (4), field_data... (variable)]`

This is why the existing parser uses `int32_t cnt = 5` — it skips the command byte plus the 4-byte
echoed mask before reading field data.

**Do not skip the echoed mask.** Some older parsing examples forget this and corrupt all
subsequent reads. The current BREmote code handles this correctly.

### Complete Bitmask Table (Firmware-Authoritative Order)

Fields appear in the response in the same bit-ascending order they are listed here.
Only requested bits appear in the response payload.

| Bit | vesc_command byte | Field | Wire Type | Scale | Raw Range | Notes |
|-----|-------------------|-------|-----------|-------|-----------|-------|
| 0 | `[4]` bit 0 | `temp_fet_filtered` | int16 | ×10 | e.g. 250 = 25.0°C | FET/MOSFET temperature |
| 1 | `[4]` bit 1 | `temp_motor_filtered` | int16 | ×10 | e.g. 350 = 35.0°C | Motor winding temperature |
| 2 | `[4]` bit 2 | `current_motor` (avg) | int32 | ×100 | e.g. 5000 = 50.00A | Reset-avg motor current |
| 3 | `[4]` bit 3 | `current_in` (avg) | int32 | ×100 | e.g. 2000 = 20.00A | Reset-avg battery/input current |
| 4 | `[4]` bit 4 | `id` (avg) | int32 | ×100 | Direct-axis current | Usually zero in FOC |
| 5 | `[4]` bit 5 | `iq` (avg) | int32 | ×100 | Quadrature-axis current | Torque-producing component |
| 6 | `[4]` bit 6 | `duty_cycle_now` | int16 | ×1000 | -1000..+1000 = -100%..100% | Current duty cycle |
| 7 | `[4]` bit 7 | `rpm` (ERPM) | int32 | ×1 | raw ERPM (signed) | Divide by pole pairs for mech RPM |
| 8 | `[3]` bit 0 | `v_in` (battery voltage) | int16 | ×10 | e.g. 500 = 50.0V | Input/battery voltage |
| 9 | `[3]` bit 1 | `amp_hours` | int32 | ×10000 | Consumed Ah ×10000 | Energy accounting |
| 10 | `[3]` bit 2 | `amp_hours_charged` | int32 | ×10000 | Regenerated Ah ×10000 | Energy accounting |
| 11 | `[3]` bit 3 | `watt_hours` | int32 | ×10000 | Consumed Wh ×10000 | Energy accounting |
| 12 | `[3]` bit 4 | `watt_hours_charged` | int32 | ×10000 | Regenerated Wh ×10000 | Energy accounting |
| 13 | `[3]` bit 5 | `tachometer` | int32 | ×1 | Cumulative ERPM steps | Direction-aware odometer |
| 14 | `[3]` bit 6 | `tachometer_abs` | int32 | ×1 | Absolute ERPM steps | Always positive |
| 15 | `[3]` bit 7 | `fault_code` | uint8 | ×1 | 0=NONE, see mc_fault_code | 1 byte in response |
| 16 | `[2]` bit 0 | `pid_pos_now` | int32 | ×1000000 | PID position | Rarely needed for telemetry |
| 17 | `[2]` bit 1 | `app_controller_id` | uint8 | ×1 | VESC CAN ID | 1 byte in response |

**Sources:** Cross-referenced from:
- Existing BREmote VESC.ino (working code confirms bits 0, 2, 3, 6, 8)
- VescUartLite library implementation (github.com/gitcnd/VescUartLite)
- vedderb/bldc comm/commands.c COMM_GET_VALUES_SELECTIVE handler
- GitHub issue #94 on vedderb/bldc (confirms mask echo structure)

**Confidence:** HIGH for bits 0-8 (multiple independent confirmations). MEDIUM for bits 9-17
(consistent across documentation sources but not directly verified in running code).

### Priority Fields for This Milestone

| Field | Bit | Priority | Why |
|-------|-----|----------|-----|
| `temp_motor_filtered` | 1 | High | Motor overtemp protection feedback on remote |
| `current_in` (battery current) | 3 | High | Required to compute power (W = V × I_battery) |
| `duty_cycle_now` | 6 | High | Direct rider feedback; 0-100% display |
| `rpm` (ERPM) | 7 | High | Speed calculation source |
| `v_in` | 8 | Already queried | Existing — battery voltage |
| `temp_fet_filtered` | 0 | Already queried | Existing — FET temperature |
| `current_motor` | 2 | Low | Less useful than battery current for riders |
| `id`, `iq` | 4, 5 | Exclude | FOC internals; not rider-relevant |

### Extended Bitmask for This Milestone

```c
// Add to vesc_command[] in getValuesSelective():
// Existing:
vesc_command[1] = 0;
vesc_command[2] = 0;
vesc_command[3] = (1 << BatVolt);     // bit 8 = v_in
vesc_command[4] = (1 << FET_TEMP);    // bit 0 = temp_fet

// Extended (add motor temp, battery current, duty, ERPM):
#define MotorTemp   1   // bit 1
#define MotCurrent  2   // bit 2  (already defined in existing code)
#define BatCurrent  3   // bit 3  (already defined)
#define Duty        6   // bit 6  (already defined)
#define ERPM        7   // bit 7  — NEW

vesc_command[4] = (1 << FET_TEMP)
                | (1 << MotorTemp)
                | (1 << BatCurrent)
                | (1 << Duty)
                | (1 << ERPM);
```

**Do NOT request `current_motor` (bit 2) unless needed.** Each int32 field adds 4 bytes to the
response and increases the UART round-trip time. Battery current (bit 3) is what matters for
power computation; motor current is redundant.

---

## Response Byte Accounting

**Why byte counts matter:** The existing code asserts a fixed expected packet length via
`VESC_PACK_LEN` and the `receiveFromVESC` buffer is sized accordingly.

| Requested Bits | Fields | Wire Bytes per Field | Total Payload Bytes | VESC_PACK_LEN |
|---------------|--------|---------------------|---------------------|---------------|
| 0, 8 (existing, no MORE_VALUES) | temp_fet(int16), v_in(int16) + mask(4) + cmd(1) | 2+2 | 4+4+1 = 9 | 9 — confirmed |
| 0,2,3,6, 8 (existing MORE_VALUES) | +motCur(int32)+batCur(int32)+duty(int16) | +4+4+2 | 9+10 = 19 | 19 — confirmed |
| 0,1,3,6,7, 8 (milestone target) | +motorTemp(int16)+ERPM(int32) vs. -motCur | vs existing MORE_VALUES: -int32(motCur)+int16(motorTemp)+int32(ERPM) | 19-4+2+4=21 | Must update to 21 |

**Concrete payload for milestone target bits {0,1,3,6,7,8}:**
- cmd (1) + mask (4) + temp_fet int16 (2) + temp_motor int16 (2) + batCur int32 (4) + duty int16 (2) + ERPM int32 (4) + v_in int16 (2) = 21 bytes payload
- Full VESC packet = 2 (framing start + length) + 21 + 2 (CRC) + 1 (end) = 26 bytes total on wire

**Update `VESC_PACK_LEN` to 21 and the `vescRelayBuffer` array size to 30 when enabling the
extended mask.**

---

## Encoding Schemes: VESC Values into Single Bytes (0-255)

The LoRa telemetry packet uses one byte per value. These are the encoding decisions for each
new field.

### Principle

- Scale so that the maximum expected value maps close to 255.
- Accept resolution loss — riders care about "50A" not "50.13A".
- Encode on Rx (VESC side), decode display interpretation on Tx.
- Document the scale as a comment in the code — future maintainers will need it.

### Per-Field Encoding Table

| Field | Raw Wire Value | Decode Formula | Useful Range | Encoding for 1 Byte | Scale Factor | Resolution |
|-------|---------------|----------------|-------------|---------------------|--------------|-----------|
| `temp_fet` (existing) | int16 ×10 | `val / 10.0` °C | 0–100°C | `(uint8_t)(fetTemp / 10)` | /10 | 1°C — already done |
| `temp_motor` | int16 ×10 | `val / 10.0` °C | 0–120°C | `(uint8_t)(motorTemp / 10)` | /10 | 1°C |
| `current_in` (battery) | int32 ×100 | `val / 100.0` A | 0–200A | `(uint8_t)(batCur / 100)` | /100 | 1A |
| `duty_cycle_now` | int16 ×1000 | `val / 10.0` % | -100%–100% | `(uint8_t)(abs(duty) / 10)` | /10 abs | 0.1% |
| `v_in` (existing) | int16 ×10 | `val / 10.0` V | 30–60V | `getUbatPercent(fbatVolt)` | custom — already done |
| Power (derived) | computed | `abs(batCur/100.0 × fbatVolt)` W | 0–10000W | `(uint8_t)(power / 50)` | /50 | 50W |
| Speed (derived) | from ERPM | see formula below | 0–80 km/h | `(uint8_t)(speed_kmh)` | ×1 | 1 km/h |

### Power Encoding Detail

```c
// On Rx, after parsing batCur and batVolt:
float power_w = (float)(batCur / 100.0f) * fbatVolt;
if (power_w < 0) power_w = 0;  // ignore regeneration for now
telemetry.foil_power = (uint8_t)(power_w / 50.0f);  // 0-255 = 0-12750W
// On Tx display: watts = (int)value * 50
```

Scale factor 50 chosen because: 255 × 50 = 12,750W which covers a 60V × 200A = 12,000W pack.
A 10S battery at 150A peak would be 4,200W = byte value 84. Resolution is 50W — adequate for
rider display.

### Speed Encoding and ERPM Conversion

```c
// ERPM to km/h:
// mechanical_rpm = ERPM / pole_pairs
// wheel_rpm = mechanical_rpm / gear_ratio
// speed_kmh = wheel_rpm * wheel_circumference_m * 60 / 1000
//           = (ERPM / pole_pairs / gear_ratio) * (PI * wheel_diameter_m) * 60 / 1000

float speed_kmh = ((float)abs(erpm) / conf.pole_pairs / conf.gear_ratio)
                  * (M_PI * conf.wheel_diameter_m)
                  * 60.0f / 1000.0f;
telemetry.foil_speed = (uint8_t)constrain(speed_kmh, 0, 255);
// On Tx display: speed = value km/h (1:1, no scaling needed)
```

**Config parameters needed:** `pole_pairs` (motor magnet count / 2), `gear_ratio` (motor to
prop/wheel reduction ratio), `wheel_diameter_m` (propeller or wheel diameter in meters). These
must be added to `confStruct` or hardcoded as `#define` constants for the first implementation.

**Hardcode first, make configurable later.** For a typical eFoil (e.g., 14-pole motor = 7 pole
pairs, 1:1 gear, 0.15m prop): `speed_kmh = erpm / 7 / 1 * (PI * 0.15) * 60 / 1000`.

### Duty Cycle Encoding Detail

```c
int16_t duty = buffer_get_int16(message, &cnt);  // arrives as ×1000
// duty range: -1000 to +1000 (= -100% to +100%)
telemetry.foil_duty = (uint8_t)(abs(duty) / 10);  // 0-255 maps to 0-100% (actually 0..100)
// On Tx: duty_percent = value  (already 0-100)
```

Note: Sending absolute value. Reverse direction information is not useful on the Tx display
for eFoil; if needed, a separate sign bit could be encoded in a high bit later.

---

## UART Timing: Fitting Extended Query into 100ms Radio Cycle

### Existing Timing (Measured/Documented)

- `getValuesSelective()` total: **~4.5ms** (documented in VESC.ino comment)
- UART baud rate: 115200
- Query payload: 9 bytes out → 19 bytes response (MORE_VALUES) with framing overhead ~26 bytes
- Time at 115200 baud: 26 bytes × 10 bits/byte / 115200 ≈ **2.3ms wire time**
- VESC processing + response latency: ~2ms
- Total: ~4.5ms measured — consistent

### Extended Query Timing Estimate

Milestone target response: 26 bytes on wire.
Wire time at 115200: 26 × 10 / 115200 ≈ **2.3ms** (same as existing MORE_VALUES)
VESC processing: ~2ms (unchanged — computation is trivial)
**Estimated total: ~4.5ms** — no meaningful change from current timing.

The ERPM and motor temp fields add only 6 more bytes (2+4) to the response compared to the
existing MORE_VALUES payload (19 bytes → 21 bytes). The UART overhead increase is < 0.2ms.

### Timing Budget in 100ms Radio Cycle

```
100ms radio cycle breakdown (Rx):
  ~4.5ms   VESC UART query + response parse
  ~2.0ms   LoRa receive window wait (interrupt-driven)
  ~0.5ms   LoRa telemetry transmit (4-byte implicit, TOA ~0.5ms at SF6/250kHz)
  ~1.0ms   GPS UART parse (if enabled, muxed on Serial1)
  ~92ms    Available slack
```

**Timing is not a constraint for this milestone.** The extended VESC query adds < 0.2ms.

### FreeRTOS Task Interaction

The existing `getVescLoop()` is called from the main loop (not a dedicated FreeRTOS task). It
uses `vTaskDelay(10)` after `setUartMux(0)` to let the mux settle. This pattern does not
need to change for the extended query.

**Do not move VESC polling to a separate FreeRTOS task.** It is already well-integrated as a
periodic call in the loop with its own `get_vesc_timer` millis() guard. Adding task overhead
and synchronization would increase complexity with no benefit.

The UART mux (`setUartMux(0)` for VESC, presumably `setUartMux(1)` for GPS) means VESC and GPS
share Serial1. Each share Serial1 exclusively during their query window — no concurrent access
risk. The existing mux settling delay (10ms) must be maintained.

---

## Telemetry Index Assignment (New LoRa Indices)

The existing `TelemetryPacket` struct is iterated byte-by-byte via `telemetry_index`:

```c
// Current struct (4 fields + link_quality):
struct TelemetryPacket {
    uint8_t foil_bat = 0xFF;       // index 0
    uint8_t foil_temp = 0xFF;      // index 1
    uint8_t foil_speed = 0xFF;     // index 2
    uint8_t error_code = 0;        // index 3
    uint8_t link_quality = 0;      // index 4  (LAST — always cycles)
};
```

**Proposed extension** — append new fields before `link_quality`:

```c
struct TelemetryPacket {
    uint8_t foil_bat = 0xFF;         // index 0 — battery %
    uint8_t foil_temp = 0xFF;        // index 1 — FET temp (°C)
    uint8_t foil_speed = 0xFF;       // index 2 — speed (km/h)
    uint8_t error_code = 0;          // index 3 — fault code
    uint8_t foil_power = 0xFF;       // index 4 — power (×50 W) — NEW
    uint8_t foil_motor_temp = 0xFF;  // index 5 — motor temp (°C) — NEW
    uint8_t foil_duty = 0xFF;        // index 6 — duty cycle (%) — NEW
    uint8_t foil_bat_current = 0xFF; // index 7 — battery current (A) — NEW
    // link_quality MUST remain last
    uint8_t link_quality = 0;        // index 8 (was 4)
};
```

**Backwards compatibility:** Old Tx firmware only reads indices 0-4. Indices 5-8 (new) are
silently ignored by old firmware. Old Tx firmware will see `link_quality` shift from index 4 to
index 8. This means old Tx will intermittently see `link_quality` as 0xFF (from foil_power)
one cycle out of five — this is cosmetic and does not affect safety.

**Alternative (strict backwards compatibility):** Insert new fields after `link_quality`.
Then old Tx always gets correct link_quality at index 4. Recommended if mixed Tx/Rx firmware
is a real-world concern.

---

## What NOT To Do

### Do Not Request Unnecessary Fields

Requesting bits 4 (`id`), 5 (`iq`), 9 (`amp_hours`), 10 (`amp_hours_charged`) adds 4-8 extra bytes
per response with no display value for eFoil. Each unused int32 wastes 4ms of UART round-trips
per 100ms cycle.

### Do Not Use `COMM_GET_VALUES` Instead of `COMM_GET_VALUES_SELECTIVE`

`COMM_GET_VALUES` returns the full 58-byte value dump unconditionally. At 115200 baud that is
~5ms wire time alone. The selective command was added precisely to avoid this. The existing code
correctly uses selective — do not regress to the full dump.

### Do Not Change UART Baud Rate

115200 is confirmed working. Some community posts suggest 250000 but the existing hardware has
been validated at 115200. The timing budget has 92ms of slack — there is no need to risk
introducing baud-rate compatibility issues.

### Do Not Forget the Echoed Mask in Response Parsing

The response payload is: `[cmd_id(1)] + [mask(4)] + [field data...]`
The `cnt = 5` start index in the existing parser accounts for this. When adding new fields,
always start parsing from `cnt = 5`, never from 1 (cmd only) or 0.

### Do Not Hardcode VESC_PACK_LEN Across Compilation Paths

The existing #ifdef VESC_MORE_VALUES pattern is correct but fragile — if bits change, PACK_LEN
must be manually updated. Consider computing it from the number of requested fields at compile
time, or at minimum add a clear comment showing the byte accounting.

### Do Not Attempt to Send Raw Float Values Over LoRa

The 1-byte telemetry slot cannot hold a raw IEEE 754 float. Always apply scale-then-truncate.
The encoding schemes in this document have been chosen to give useful rider feedback within
0-255 range. Do not attempt to use two consecutive indices for a single high-resolution value
without a protocol version flag — old Tx firmware will misinterpret the second byte.

---

## Sources

- **vedderb/bldc (VESC firmware):** https://github.com/vedderb/bldc/blob/master/comm/commands.c
  - Authoritative source for bitmask field order. Confidence: HIGH (referenced by all community
    implementations, confirmed by working BREmote code).

- **vedderb/bldc issue #94:** https://github.com/vedderb/bldc/issues/94
  - Documents the mask-is-echoed behavior and a known spurious-field bug in old firmware (3.56).
    Current VESC firmware does not have this issue.

- **VescUartLite library:** https://github.com/gitcnd/VescUartLite
  - Arduino implementation that parses COMM_GET_VALUES_SELECTIVE with `get_uint32_t` for mask
    skip. Cross-confirms bit positions 0-17.

- **BREmote VESC.ino (existing):** Source/V2_Integration_Rx/VESC.ino
  - Production code confirming bits 0 (FET_TEMP), 2 (MotCurrent), 3 (BatCurrent), 6 (Duty),
    8 (BatVolt) with working byte counts PACK_LEN=9 and PACK_LEN=19.

- **VESC speed calculation community:** https://vesc-project.com/node/2932 (ERPM to RPM)
  - Confirms ERPM = pole_pairs × mechanical RPM formula.
