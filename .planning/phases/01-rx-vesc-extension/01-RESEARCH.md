# Phase 1: Rx VESC Extension - Research

**Researched:** 2026-03-02
**Domain:** Embedded UART telemetry extension — VESC COMM_GET_VALUES_SELECTIVE on ESP32/Arduino
**Confidence:** HIGH

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| VESC-01 | Rx enables `VESC_MORE_VALUES` to request battery current and voltage from VESC via `COMM_GET_VALUES_SELECTIVE` | Bitmask positions confirmed from source code; `VESC_MORE_VALUES` path already partially implemented in VESC.ino |
| VESC-02 | Rx computes power in watts from battery voltage x battery current locally | `fbatVolt` already available as global float; `batCur` already parsed inside `#ifdef VESC_MORE_VALUES` block; formula: `float watts = fbatVolt * (batCur / 100000.0f)` |
| VESC-03 | Rx encodes power to single byte with appropriate scale factor | Recommended: `(uint8_t)constrain(watts / 50.0f, 0, 255)` for 0-12,750W range; alternatives documented |
| TELE-03 | `VESC_PACK_LEN` updated to match new VESC response size when `VESC_MORE_VALUES` is enabled | Current MORE_VALUES path: VESC_PACK_LEN=19; Phase 1 does NOT add new VESC bits so VESC_PACK_LEN stays 19; foil_power uses already-parsed batCur and fbatVolt |
| TELE-04 | Old Tx firmware ignores new telemetry index gracefully | Confirmed: `waitForTelemetry()` uses `if (rcvArray[3] < sizeof(TelemetryPacket))` bounds check — new index 4 is silently discarded by old Tx with 5-field struct |
</phase_requirements>

---

## Summary

Phase 1 is entirely contained within the Rx firmware. The goal is to enable `VESC_MORE_VALUES`, route already-parsed-but-discarded VESC fields (`batCur`, `fbatVolt`) into a derived power calculation, encode the result as a single byte, and add `foil_power` as a new field in the `TelemetryPacket` struct on the Rx side only. The Tx struct extension is deferred to Phase 2.

The existing code in `VESC.ino` already parses `batCur` inside a `#ifdef VESC_MORE_VALUES` block and discards it as a local variable. The `fbatVolt` global float is already computed and available. Phase 1 requires: (1) uncomment `#define VESC_MORE_VALUES` in `BREmote_V2_Rx.h`, (2) add `foil_power` field to `TelemetryPacket` in `BREmote_V2_Rx.h`, (3) add power calculation and assignment in `VESC.ino` after the existing `batCur` parse. That is the complete implementation scope.

Backwards compatibility is guaranteed by the existing bounds check in `Tx/Radio.ino:waitForTelemetry`: `if (rcvArray[3] < sizeof(TelemetryPacket))`. Old Tx firmware has `sizeof(TelemetryPacket) == 5` (indices 0-4). New Rx sends index 4 (`foil_power`). Old Tx will silently discard it because index 4 is NOT less than 5 (the old Tx struct does not have an index 4 slot beyond `link_quality`). Wait — re-reading: current struct has `link_quality` at index 4, so `sizeof` is 5 and indices 0-4 are valid. When Rx sends new index 4 (`foil_power`) and old Tx has index 4 as `link_quality`, the old Tx will write `foil_power`'s byte value into its `link_quality` slot. This is the one behavioral difference with old Tx: `link_quality` on the old Tx will intermittently receive the power byte instead of the real link quality. This is cosmetic-only and does not affect safety. The planner must document this clearly.

**Primary recommendation:** Uncomment `VESC_MORE_VALUES`, add `foil_power` field to Rx `TelemetryPacket` before `link_quality`, compute watts from `fbatVolt * (batCur / 100000.0f)`, encode as `(uint8_t)constrain(watts / 50.0f, 0, 255)`, verify with `DEBUG_VESC` on hardware.

---

## Standard Stack

### Core

| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| ESP32 Arduino framework (arduino-esp32) | Core bundled with project | `HardwareSerial`, `FreeRTOS`, `millis()`, `constrain()` | Existing; no change |
| `vesc_buffer.h` / `vesc_crc.h` | Project-local (Source/V2_Integration_Rx/) | `buffer_get_int32`, `buffer_get_int16`, VESC CRC16 | Existing hand-rolled VESC protocol helpers |
| `BREmote_V2_Rx.h` | Project-local | `TelemetryPacket` struct, `VESC_MORE_VALUES`, `VESC_PACK_LEN`, `fbatVolt` global | Central config/globals for Rx |

### Supporting

| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| `DEBUG_VESC` define | Project-local | Prints raw VESC packet hex to Serial | Enable during development; disable before release |

### Alternatives Considered

| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Hand-rolled VESC parser | VescUartLite or SolidGeek/VescUart | Libraries add overhead, don't support COMM_GET_VALUES_SELECTIVE in most cases — out of scope per REQUIREMENTS.md |
| `/50` scale factor | `/20` scale factor | `/20` gives 5,100W max (may saturate on 60V×150A setups); `/50` gives 12,750W max; validate on actual hardware |

**Installation:** No new libraries required. All work is in existing source files.

---

## Architecture Patterns

### Files Modified (Phase 1 Scope)

```
Source/V2_Integration_Rx/
├── BREmote_V2_Rx.h     # Enable VESC_MORE_VALUES; add foil_power to TelemetryPacket
└── VESC.ino            # Add power calculation; assign telemetry.foil_power
```

No changes to Tx files in Phase 1. Tx struct extension and display are Phase 2 and Phase 3.

### Pattern 1: Compile-Flag Gate for Extended VESC Query

**What:** `VESC_MORE_VALUES` simultaneously gates the VESC bitmask sent, the parser calls, and `VESC_PACK_LEN`. All three must stay consistent.

**When to use:** Always; the flag is already the pattern in the codebase.

**Current state (disabled):**

```c
// BREmote_V2_Rx.h line 170-177
//#define VESC_MORE_VALUES
#ifdef VESC_MORE_VALUES
  #define VESC_PACK_LEN 19
  uint8_t vescRelayBuffer[25];
#else
  #define VESC_PACK_LEN 9
  uint8_t vescRelayBuffer[15];
#endif
```

**Phase 1 change:** Uncomment `#define VESC_MORE_VALUES`. This activates the existing VESC_PACK_LEN=19 path and the existing parser for `motCur`, `batCur`, `duty`. No bitmask change is needed — Phase 1 does NOT request any new VESC bits. The existing bitmask under `VESC_MORE_VALUES` already requests `BatCurrent` (bit 3), which is what we need for power calculation.

**Concrete bitmask (VESC_MORE_VALUES enabled, no new bits):**

```c
// VESC.ino - existing code, no change needed
vesc_command[4] = (1<<FET_TEMP) + (1<<MotCurrent) + (1<<BatCurrent) + (1<<Duty);
// Byte 4: bit 0 (FET_TEMP) + bit 2 (MotCurrent) + bit 3 (BatCurrent) + bit 6 (Duty)
vesc_command[3] = (1<<BatVolt);
// Byte 3: bit 0 (BatVolt)
// VESC_PACK_LEN = 19: cmd(1)+mask(4)+fetTemp(2)+motCur(4)+batCur(4)+duty(2)+batVolt(2) = 19
```

### Pattern 2: Struct Append for Protocol Extension

**What:** `TelemetryPacket` is a raw byte array over the wire. Adding a field appends a new index without changing existing indices.

**When to use:** Every time a new telemetry value is added.

**Example — Rx header change (BREmote_V2_Rx.h):**

```c
// Source: Source/V2_Integration_Rx/BREmote_V2_Rx.h (current, line 99-107)
// BEFORE:
struct __attribute__((packed)) TelemetryPacket {
    uint8_t foil_bat = 0xFF;    // index 0
    uint8_t foil_temp = 0xFF;   // index 1
    uint8_t foil_speed = 0xFF;  // index 2
    uint8_t error_code = 0;     // index 3
    //This must be the last entry
    uint8_t link_quality = 0;   // index 4
} telemetry;

// AFTER Phase 1 (Rx only):
struct __attribute__((packed)) TelemetryPacket {
    uint8_t foil_bat = 0xFF;    // index 0 — battery % (existing)
    uint8_t foil_temp = 0xFF;   // index 1 — FET temp °C (existing)
    uint8_t foil_speed = 0xFF;  // index 2 — speed km/h (existing)
    uint8_t error_code = 0;     // index 3 — fault code (existing)
    // NEW — Phase 1: SCALE foil_power = watts/50; DECODE watts = foil_power*50 (range 0-12750W)
    uint8_t foil_power = 0xFF;  // index 4 — power in watts/50
    //This must be the last entry
    uint8_t link_quality = 0;   // index 5 (was 4)
} telemetry;
```

**Critical:** The scale factor comment must appear co-located in the header for the field AND in VESC.ino assignment AND in the Tx Display.ino (Phase 3). They must be identical.

### Pattern 3: Named-Field Write, Indexed Transmit

**What:** Rx writes to `telemetry.foil_power` by name; the cycling loop transmits by byte offset automatically. No change to Radio.ino is needed.

**Example — VESC.ino addition:**

```c
// Source: Source/V2_Integration_Rx/VESC.ino (addition inside #ifdef VESC_MORE_VALUES block)
#ifdef VESC_MORE_VALUES
    motCur = buffer_get_int32(message, &cnt);
    batCur = buffer_get_int32(message, &cnt);
    duty   = buffer_get_int16(message, &cnt);

    // Power calculation: fbatVolt is global float (V), batCur is int32 (mA×100)
    // batCur / 100000.0f converts mA×100 to Amps
    float batCur_amps = (float)batCur / 100000.0f;
    float watts = fbatVolt * batCur_amps;
    if (watts < 0) watts = 0;  // Ignore regeneration
    // SCALE: foil_power = watts/50  (range 0-255 = 0-12750W, 50W resolution)
    // DECODE (Tx Display.ino): watts = foil_power * 50
    telemetry.foil_power = (uint8_t)constrain(watts / 50.0f, 0.0f, 255.0f);

    #ifdef DEBUG_VESC
    Serial.print("batCur_amps="); Serial.print(batCur_amps);
    Serial.print(" watts="); Serial.print(watts);
    Serial.print(" foil_power="); Serial.println(telemetry.foil_power);
    #endif
#endif
```

**Note on batVolt parsing order:** In `getValuesSelective`, `batVolt` is parsed AFTER the `#ifdef VESC_MORE_VALUES` block. The power calculation uses `fbatVolt` which is a global float set at the end of the same function: `fbatVolt = (float)batVolt / 10.0;`. Since both `batCur` and `fbatVolt` are needed for the power calc, there are two options:

- Option A: Use `fbatVolt` from the previous VESC cycle (already computed, always valid if VESC is connected). Simple, introduces one cycle of lag (100ms) — acceptable.
- Option B: Parse `batVolt` first (reorder parse calls) and use the fresh value. Requires moving `batVolt` parse up. More complex; not necessary.

**Recommendation: Option A.** Use global `fbatVolt` from the previous cycle. The comment in source confirms `fbatVolt` is set at the bottom of the same function on each successful call, so it will always hold the most recent valid voltage when the next call runs.

### Pattern 4: Backwards Compatibility Gate (Tx Side — Unchanged)

**What:** The Tx bounds check automatically discards unknown indices. No Tx change required in Phase 1.

**Confirmed code (Tx/Radio.ino, line 344):**

```c
// Source: Source/V2_Integration_Tx/Radio.ino:waitForTelemetry()
uint8_t* ptr = (uint8_t*)&telemetry;
if (rcvArray[3] < sizeof(TelemetryPacket))   // bounds check — unchanged
{
    ptr[rcvArray[3]] = rcvArray[4];
}
```

Old Tx `sizeof(TelemetryPacket) == 5`. New Rx sends index 4 (`foil_power`). Old Tx test: `4 < 5` is TRUE — old Tx WILL write the `foil_power` byte into its `link_quality` slot (index 4). This means old Tx link quality display will cycle through a garbage value one cycle in five. This is cosmetic, not a safety issue, and matches the documented expected behavior in SUMMARY.md.

### Anti-Patterns to Avoid

- **Inserting `foil_power` before `error_code`:** Shifts `error_code` to index 4 — breaks all deployed Tx units silently. Always append after index 3.
- **Changing VESC_PACK_LEN without changing the bitmask:** Phase 1 does not add new VESC bits, so VESC_PACK_LEN stays 19. Do not change it unless a new bit is added to `vesc_command[4]`.
- **Using raw `batCur` int32 without the 100000 divisor:** `batCur` is in units of milliAmps×100. At 50A: `batCur = 5,000,000`. Dividing by `100` gives 50,000A (wrong). Dividing by `100,000` gives 50A (correct).
- **Forgetting `watts < 0` guard:** During regenerative braking, `batCur` is negative. Casting negative float to uint8_t is undefined behavior in C (wraps to large number). Always clamp to 0 before encoding.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| VESC UART protocol | Custom UART state machine | Existing `getValuesSelective` + `buffer_get_int32` helpers | Already correct, CRC-validated, tested |
| Byte packing of float values | Custom float-to-byte encoding | `constrain(value / scale, 0, 255)` + cast to uint8_t | Sufficient precision for display; simpler than bitfield tricks |

**Key insight:** The entire VESC MORE_VALUES parsing path already exists in production code. Phase 1 is routing, not protocol work.

---

## Common Pitfalls

### Pitfall 1: VESC_PACK_LEN / bitmask mismatch causes silent VESC communication failure

**What goes wrong:** If the bitmask changes but VESC_PACK_LEN does not, `receiveFromVESC() == VESC_PACK_LEN` fails silently. All VESC telemetry goes dark (stays 0xFF). No visible error.

**Phase 1 relevance:** Phase 1 does NOT add new bitmask bits, so VESC_PACK_LEN=19 remains correct. Risk is LOW if the implementation follows the spec exactly. Risk becomes HIGH if someone also adds MotorTemp or ERPM to the bitmask during Phase 1 (those are Phase 2 scope).

**How to avoid:** Enable `DEBUG_VESC` and verify the raw packet hex prints after flashing. Check that `receiveFromVESC` returns 19, not 0.

**Warning signs:** `telemetry.foil_bat` stays 0xFF after connecting VESC. Serial with `DEBUG_VESC` shows empty or partial hex dump.

### Pitfall 2: batCur unit error causes 10× or 100× power inflation

**What goes wrong:** `batCur` (int32) from VESC is in units of milliAmps×100. Dividing by wrong constant:
- `/ 100` → result is milliAmps (100A becomes 10,000 → power saturates immediately)
- `/ 1000` → result is Amps×100 (10× too high — 5,000W at 50A → saturates)
- `/ 100000` → correct (result is Amps)

**How to avoid:** Add explicit unit comment: `// batCur unit: int32 in mA*100. Divide by 100000.0f to get Amps.`

**Warning signs:** `foil_power` byte is immediately 255 at any load. Watts printed in DEBUG_VESC are 10× or 100× the VESC Tool reading.

### Pitfall 3: Struct field inserted before existing index shifts wire protocol

**What goes wrong:** Inserting `foil_power` before `error_code` shifts `error_code` from index 3 to index 4. All deployed Tx units misinterpret the error field.

**How to avoid:** Always append. New field goes between `error_code` (index 3) and `link_quality` (now index 5). Never before index 3.

**Warning signs:** Mixed firmware test shows wrong values at existing display positions.

### Pitfall 4: Serial1.flush() may discard incoming VESC response bytes

**What goes wrong:** `getVescLoop()` calls `Serial1.flush()` before `getValuesSelective()`. On some arduino-esp32 versions, `flush()` flushes both TX and RX buffers. If VESC responds while the 10ms `vTaskDelay` is running (mux settle), those response bytes may be flushed, causing a 200ms timeout and returning 0.

**How to avoid:** If VESC communication fails after enabling `VESC_MORE_VALUES`, replace `Serial1.flush()` with `while(Serial1.available()) Serial1.read();` as a diagnostic step.

**Warning signs:** `getValuesSelective` consistently returns 0 (timeout), but VESC is known to be connected and powered.

### Pitfall 5: Power computed using stale fbatVolt = 0.0f on first boot cycle

**What goes wrong:** On the very first `getValuesSelective` call after boot, `fbatVolt` is initialized to `0.0f`. If power is computed before `fbatVolt` is updated (i.e., at the top of the function before `batVolt` is parsed), `watts = 0.0f * batCur_amps = 0`, and `foil_power = 0` instead of the correct value.

**How to avoid:** Use `fbatVolt` from the PREVIOUS cycle (it will be 0 only on the first call, then valid). Or restructure the parse to compute `batVolt` fresh before the power calc. Using the previous cycle's value is simplest and introduces only a one-VESC-cycle lag.

**Warning signs:** `foil_power` is 0 for the first 100ms after boot, then correct — acceptable. If it stays 0 permanently, check the `batCur` parsing.

---

## Code Examples

Verified patterns from source code:

### Complete VESC.ino Change (getValuesSelective — Phase 1 only)

```c
// Source: Source/V2_Integration_Rx/VESC.ino
// CHANGE 1: Uncomment VESC_MORE_VALUES in BREmote_V2_Rx.h (line 170)
// #define VESC_MORE_VALUES   <-- remove the leading //

// In getValuesSelective(), the #ifdef VESC_MORE_VALUES block (lines 66-70) becomes active:
// EXISTING (no change needed to parse calls):
#ifdef VESC_MORE_VALUES
    motCur = buffer_get_int32(message, &cnt);
    batCur = buffer_get_int32(message, &cnt);
    duty   = buffer_get_int16(message, &cnt);
    // ADD AFTER duty parse:
    float batCur_amps = (float)batCur / 100000.0f;  // batCur unit: mA*100
    float watts = fbatVolt * batCur_amps;            // fbatVolt: global float in Volts
    if (watts < 0.0f) watts = 0.0f;                 // Clamp: ignore regen
    // SCALE: foil_power = watts/50  → DECODE on Tx: watts = foil_power * 50  (range 0-12750W)
    telemetry.foil_power = (uint8_t)constrain(watts / 50.0f, 0.0f, 255.0f);
    #ifdef DEBUG_VESC
    Serial.print("V="); Serial.print(fbatVolt);
    Serial.print(" I="); Serial.print(batCur_amps);
    Serial.print(" W="); Serial.print(watts);
    Serial.print(" encoded="); Serial.println(telemetry.foil_power);
    #endif
#endif
```

### TelemetryPacket Struct Change (BREmote_V2_Rx.h only — Phase 1)

```c
// Source: Source/V2_Integration_Rx/BREmote_V2_Rx.h (line 99)
// Telemetry to send, MUST BE 8-bit!!
struct __attribute__((packed)) TelemetryPacket {
    uint8_t foil_bat = 0xFF;    // index 0 — battery % 0-100
    uint8_t foil_temp = 0xFF;   // index 1 — FET temp °C
    uint8_t foil_speed = 0xFF;  // index 2 — speed km/h
    uint8_t error_code = 0;     // index 3 — fault flags
    // NEW Phase 1: SCALE=watts/50  DECODE=foil_power*50  range 0-12750W at 50W resolution
    uint8_t foil_power = 0xFF;  // index 4 — power (watts/50)
    // This must be the last entry
    uint8_t link_quality = 0;   // index 5 (was 4)
} telemetry;
```

### VESC_PACK_LEN Verification (no change needed in Phase 1)

```c
// Source: Source/V2_Integration_Rx/BREmote_V2_Rx.h (line 171)
// Phase 1: VESC_MORE_VALUES enabled, bitmask unchanged, VESC_PACK_LEN stays 19
// Byte count: cmd(1)+mask(4)+fetTemp(2)+motCur(4)+batCur(4)+duty(2)+batVolt(2) = 19  CORRECT
#define VESC_MORE_VALUES        // <-- uncomment this line only
#ifdef VESC_MORE_VALUES
  #define VESC_PACK_LEN 19      // <-- no change
  uint8_t vescRelayBuffer[25];  // <-- no change
#else
  #define VESC_PACK_LEN 9
  uint8_t vescRelayBuffer[15];
#endif
```

### Backwards Compatibility Verification (Tx side — no change, confirmed working)

```c
// Source: Source/V2_Integration_Tx/Radio.ino:waitForTelemetry() line 344
// Old Tx sizeof(TelemetryPacket) = 5, valid indices 0-4
// New Rx sends index 4 (foil_power)
// Old Tx: 4 < 5 → TRUE → writes foil_power byte into link_quality slot
// This is cosmetic (link quality display flickers one in five cycles) — not a safety issue
uint8_t* ptr = (uint8_t*)&telemetry;
if (rcvArray[3] < sizeof(TelemetryPacket))  // No change — works correctly
{
    ptr[rcvArray[3]] = rcvArray[4];
}
```

---

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `VESC_MORE_VALUES` commented out | Enable the flag | Phase 1 | Activates existing parse path; raises VESC_PACK_LEN from 9 to 19 |
| `batCur`, `motCur`, `duty` parsed but discarded as local vars | Route to `telemetry` struct fields | Phase 1 | No parsing change; just assignment added |
| 5-field TelemetryPacket (cycle time 500ms) | 6-field TelemetryPacket (cycle time 600ms) | Phase 1 | +100ms cycle time; all values still refresh under 1 second |

**Deprecated/outdated:**
- None for Phase 1. The existing code pattern is correct; Phase 1 is purely additive.

---

## Open Questions

1. **Scale factor: `/50` vs `/20` for foil_power**
   - What we know: `/50` gives 0-12,750W range (covers 60V×200A = 12,000W peak). `/20` gives 0-5,100W range (covers typical eFoil ~4,000W peak at 50V×80A).
   - What's unclear: The actual peak wattage of the target hardware is not confirmed. Both the REQUIREMENTS.md example (`watts÷20`) and STACK.md (`watts/50`) are present in the documents.
   - Recommendation: Use `/50` as the default for broader hardware compatibility. Validate against actual VESC Tool readings during Phase 1 hardware testing. The scale factor can be changed without protocol impact before Phase 3 (Tx display) is implemented.

2. **batVolt for power calc: previous cycle vs same cycle**
   - What we know: `fbatVolt` is set at the bottom of `getValuesSelective`. Power calc uses `fbatVolt` from the previous successful VESC call (one 100ms cycle lag).
   - What's unclear: Whether one-cycle-lag voltage is acceptable (voltage changes slowly, so it is).
   - Recommendation: Use previous-cycle `fbatVolt`. No restructuring needed.

3. **`foil_power = 0xFF` sentinel meaning on Tx**
   - What we know: All new fields are initialized to `0xFF`. This is the "not available" sentinel used for `foil_bat`, `foil_temp`, `foil_speed`.
   - What's unclear: Phase 3 (Tx display) needs to handle the case where `foil_power == 0xFF` (VESC not connected or `VESC_MORE_VALUES` not enabled on Rx). The display should skip or show "---" rather than "12750W".
   - Recommendation: Note in Phase 3 task: treat `foil_power == 0xFF` as "unavailable"; do not display.

---

## Sources

### Primary (HIGH confidence)

- `Source/V2_Integration_Rx/VESC.ino` — Actual bitmask values, parse order, `cnt=5` skip, `getVescLoop`, `receiveFromVESC`, buffer size. Read directly from source (commit ef2ec4c line refs valid).
- `Source/V2_Integration_Rx/BREmote_V2_Rx.h` — `TelemetryPacket` definition, `VESC_MORE_VALUES` compile flag, `VESC_PACK_LEN`, `fbatVolt` global, `vescRelayBuffer` size.
- `Source/V2_Integration_Rx/Radio.ino` — `triggeredReceive`, telemetry cycling, `telemetry_index`, `sizeof(TelemetryPacket)` wrap.
- `Source/V2_Integration_Tx/Radio.ino` — `waitForTelemetry`, bounds check `rcvArray[3] < sizeof(TelemetryPacket)`, backwards-compatibility behavior confirmed at line 344.
- `Source/V2_Integration_Tx/BREmote_V2_Tx.h` — Tx `TelemetryPacket` definition (identical layout to Rx; `sizeof == 5` confirmed).
- `.planning/research/STACK.md` — Bitmask table, byte accounting, encoding schemes (HIGH confidence from cross-referenced sources).
- `.planning/research/ARCHITECTURE.md` — Data flow diagram, struct extension seam, component boundaries (HIGH confidence from source code).
- `.planning/research/PITFALLS.md` — All pitfalls with root causes and prevention strategies.

### Secondary (MEDIUM confidence)

- `.planning/research/SUMMARY.md` — Executive summary, feature priority, phase structure rationale.
- `vedderb/bldc comm/commands.c` (referenced in STACK.md) — Authoritative bitmask field order; confirmed by working BREmote code.
- `vedderb/bldc issue #94` — Mask-is-echoed behavior confirmation.

### Tertiary (LOW confidence)

- None for Phase 1 scope. All critical implementation details are confirmed from source code.

---

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — all libraries and file locations confirmed from actual source files
- Architecture patterns: HIGH — all code examples derived from read source files, not inference
- Pitfalls: HIGH (code-derived) / MEDIUM (Serial.flush behavior, arduino-esp32 version dependent)
- Scale factor choice: MEDIUM — `/50` recommended based on hardware range analysis; validate on hardware

**Research date:** 2026-03-02
**Valid until:** 2026-04-02 (stable embedded codebase; no external library dependencies changing)
