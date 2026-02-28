# Architecture Patterns: VESC Telemetry Extension

**Domain:** Embedded wireless remote control — LoRa telemetry pipeline
**Researched:** 2026-02-28
**Confidence:** HIGH (all findings derived directly from source code, not inference)

---

## Current Telemetry Cycling Mechanism (Code-Level)

### The TelemetryPacket Struct (Both Sides — Identical)

Defined in `BREmote_V2_Rx.h` and `BREmote_V2_Tx.h` with `__attribute__((packed))`:

```c
struct __attribute__((packed)) TelemetryPacket {
    uint8_t foil_bat    = 0xFF;  // index 0: battery percent (0-100)
    uint8_t foil_temp   = 0xFF;  // index 1: FET temp in °C
    uint8_t foil_speed  = 0xFF;  // index 2: speed (currently unused/zero)
    uint8_t error_code  = 0;     // index 3: error flags

    // MUST be last entry (comment in source)
    uint8_t link_quality = 0;    // index 4: link quality score 0-10
} telemetry;
```

The struct is treated as a raw byte array. `sizeof(TelemetryPacket)` == 5.

### Rx Side: Transmitting Telemetry (Rx/Radio.ino, triggeredReceive)

On every received control packet, Rx sends one telemetry byte back:

```c
uint8_t* ptr = (uint8_t*)&telemetry;

sendArray[3] = telemetry_index;       // which field (0..N-1)
sendArray[4] = ptr[telemetry_index];  // raw byte value at that offset

telemetry_index++;
if (telemetry_index >= sizeof(TelemetryPacket))
{
    telemetry_index = 0;              // wrap at struct size
}
```

- `telemetry_index` is a module-level `volatile uint8_t`, initialized to 0.
- It increments every 100ms (once per control packet from Tx).
- At 5 fields and 100ms period: one full cycle takes 500ms.
- The wrap bound is `sizeof(TelemetryPacket)` — **this is the extension seam**.

### Tx Side: Receiving Telemetry (Tx/Radio.ino, waitForTelemetry)

```c
uint8_t* ptr = (uint8_t*)&telemetry;
if (rcvArray[3] < sizeof(TelemetryPacket))   // bounds check
{
    ptr[rcvArray[3]] = rcvArray[4];           // write byte into local struct
}
if (telemetry.error_code)
{
    remote_error = telemetry.error_code;
}
```

The bounds check `rcvArray[3] < sizeof(TelemetryPacket)` is the **backwards-compatibility gate**. Any index equal to or beyond the Tx's `sizeof(TelemetryPacket)` is silently discarded. Old Tx firmware with a 5-field struct will ignore indices 5+.

### Rx Side: Populating the Struct (Rx/VESC.ino, getValuesSelective)

Current VESC query uses `COMM_GET_VALUES_SELECTIVE` with a 32-bit bitmask:

```c
// Byte 4 of bitmask:
#define FET_TEMP    0  // bit 0
#define MotCurrent  2  // bit 2
#define BatCurrent  3  // bit 3
#define Duty        6  // bit 6
// Byte 3 of bitmask:
#define BatVolt     0  // bit 0

#ifdef VESC_MORE_VALUES
    vesc_command[4] = (1<<FET_TEMP) + (1<<MotCurrent) + (1<<BatCurrent) + (1<<Duty);
#else
    vesc_command[4] = (1<<FET_TEMP);
#endif
```

Fields populated into `telemetry` today:
- `telemetry.foil_bat  = getUbatPercent(fbatVolt)` — battery % from voltage
- `telemetry.foil_temp = (uint8_t)(fetTemp / 10)` — FET temp in °C (raw int16 / 10)

Fields decoded when `VESC_MORE_VALUES` is defined but **not relayed** (local variables, discarded):
- `motCur` (int32, milliAmps × 100)
- `batCur` (int32, milliAmps × 100)
- `duty`   (int16, percent × 10)

`foil_speed` and `link_quality` are set outside VESC.ino:
- `link_quality` is set in `triggeredReceive()` from RSSI/SNR
- `foil_speed` has no current assignment (remains 0xFF)

---

## Recommended Architecture for Extension

### Core Principle: Struct Extension = Protocol Extension

The entire telemetry protocol is encoded in `sizeof(TelemetryPacket)`. Adding a field to the struct on the Rx side causes Rx to transmit that index. Adding the same field to the Tx struct causes Tx to accept and store it. Old Tx firmware with a smaller struct silently discards unknown indices. This is the correct, zero-overhead backwards compatibility mechanism — use it as designed.

### New Struct Layout (Both Sides, Extended)

```c
struct __attribute__((packed)) TelemetryPacket {
    // --- Existing indices (DO NOT REORDER) ---
    uint8_t foil_bat     = 0xFF;  // index 0: battery percent 0-100
    uint8_t foil_temp    = 0xFF;  // index 1: FET temp °C (raw/10)
    uint8_t foil_speed   = 0xFF;  // index 2: speed km/h (scaled TBD)
    uint8_t error_code   = 0;     // index 3: error flags

    // --- New indices (added in this milestone) ---
    uint8_t foil_power   = 0xFF;  // index 4: power in watts / scale factor
    uint8_t foil_motor_current = 0xFF;  // index 5: motor current A (scaled)
    uint8_t foil_bat_current   = 0xFF;  // index 6: battery current A (scaled)
    uint8_t foil_duty          = 0xFF;  // index 7: duty cycle 0-100%

    // --- MUST remain last (wrap sentinel) ---
    uint8_t link_quality = 0;     // index 8 (was 4): link quality 0-10
} telemetry;
```

**Critical constraint:** `link_quality` carries the comment "This must be the last entry" in the original source. The Rx's `telemetry.link_quality = getLinkQuality(...)` assignment is positionally independent of the index — it always writes to the named field. The named-field write is safe regardless of struct position. The constraint on ordering is that existing indices 0-3 must not move.

### Per-Value Encoding

Single-byte (0-255) encoding requires a scale decision per value. These are chosen to maximize useful range for eFoil use:

| Field | VESC Raw Type | Encoding | Range | Resolution |
|-------|--------------|----------|-------|------------|
| `foil_bat` | int16 (mV) | `getUbatPercent(V)` → 0-100 | 0-100% | 1% |
| `foil_temp` | int16 (°C × 10) | `fetTemp / 10` → 0-255 | 0-255°C | 1°C |
| `foil_speed` | int32 ERPM | `erpm / poles / ratio * circ / 60` → 0-255 | 0-255 km/h | 1 km/h |
| `foil_power` | derived (V × A) | `watts / 20` → 0-255 | 0-5100W | 20W |
| `foil_motor_current` | int32 (mA × 100) | `(motCur/100) + 127` or `motCur/1000` | signed or 0-255A | 1A |
| `foil_bat_current` | int32 (mA × 100) | `batCur / 100000 * 255 / max` | 0-255 | ~0.4A |
| `foil_duty` | int16 (percent × 10) | `duty / 10` → 0-100 | 0-100% | 1% |

Power encoding recommendation: `foil_power = (uint8_t)constrain(watts / 20, 0, 255)` gives 0-5100W range at 20W resolution. Decoding on Tx: `watts = foil_power * 20`. This is appropriate for eFoil motors (typical max ~4000W).

Motor current: VESC `motCur` is int32 in units of milliAmps × 100 (i.e., 10A = 1000000). Use `(uint8_t)constrain(abs(motCur) / 100000, 0, 255)` for 0-255A absolute value. For eFoils this is typically 0-100A.

Battery current: Same scale as motor current but from battery side. Same encoding.

Duty cycle: `duty` is int16, units percent × 10 (100% = 1000). Use `(uint8_t)constrain(duty / 10, 0, 100)`.

---

## Data Flow Diagram: New Value End-to-End (Power Example)

```
VESC Motor Controller
        |
        | UART (Serial1, 115200 baud)
        | COMM_GET_VALUES_SELECTIVE
        | bitmask: FET_TEMP | MotCurrent | BatCurrent | Duty | BatVolt
        v
Rx: getValuesSelective() in VESC.ino
        |
        | Parse response buffer:
        | fetTemp  (int16)
        | motCur   (int32)   <-- now used
        | batCur   (int32)   <-- now used
        | duty     (int16)   <-- now used
        | batVolt  (int16)
        |
        | Compute derived values:
        | float watts = (fbatVolt) * (batCur / 100000.0f)
        | telemetry.foil_power = constrain(watts / 20, 0, 255)
        v
Rx: telemetry struct in RAM (BREmote_V2_Rx.h)
  [foil_bat, foil_temp, foil_speed, error_code,
   foil_power, foil_motor_current, foil_bat_current,
   foil_duty, link_quality]
        |
        | triggeredReceive() fires on each Tx control packet (100ms)
        | telemetry_index cycles 0..8, wraps at sizeof(TelemetryPacket)
        | One byte sent per 100ms cycle
        v
LoRa 6-byte packet [DestAddr(3), DataIndex, DataValue, CRC8]
  e.g., [AA, BB, CC, 04, 78, XX]  (index 4 = foil_power, value 120 = 2400W)
        |
        | 250kHz BW, SF6, implicit header
        | ~4ms time-on-air
        v
Tx: waitForTelemetry() in Tx/Radio.ino
        |
        | Bounds check: rcvArray[3] < sizeof(TelemetryPacket)
        | If index is within Tx struct size: write ptr[index] = value
        | If index >= Tx struct size (old firmware): silently discard
        v
Tx: telemetry struct in RAM (BREmote_V2_Tx.h)
  [foil_bat, foil_temp, foil_speed, error_code,
   foil_power, foil_motor_current, foil_bat_current,
   foil_duty, link_quality]           (new Tx firmware)
        |
        | Display task reads telemetry fields
        | updateBargraphs() at 200ms, loop() for menu display
        v
LED Matrix (HT16K33 7x10)
  Display power as bargraph or numeric value
```

Cycle timing for 9-field struct at 100ms per index: full rotation = 900ms. All values refresh in under 1 second. Acceptable for eFoil telemetry.

---

## Component Boundaries

| Component | Responsibility | Communicates With |
|-----------|---------------|-------------------|
| `Rx/VESC.ino:getValuesSelective` | Query VESC, parse response, compute derived values, populate `telemetry` struct | `telemetry` struct (shared memory) |
| `Rx/Radio.ino:triggeredReceive` | Read `telemetry` struct byte-by-byte, transmit via LoRa on each inbound control packet | `telemetry_index` counter, radio driver |
| LoRa 6-byte packet | Wire protocol — index + value byte, CRC8 protected | Rx transmitter → Tx receiver |
| `Tx/Radio.ino:waitForTelemetry` | Receive LoRa packet, bounds-check index, write into local `telemetry` struct | `telemetry` struct (shared memory) |
| `Tx/Display.ino` + `loop()` | Read `telemetry` fields, render on LED matrix | `telemetry` struct (read-only consumer) |

---

## Build Order (Which Side First)

**Build Rx first, then Tx.**

Rationale:

1. Rx is the data source. The Rx must populate new `telemetry` fields before Tx has anything to display. Rx changes in isolation are safe — an old Tx receiving new indices beyond its struct size silently discards them (confirmed by bounds check `rcvArray[3] < sizeof(TelemetryPacket)`).

2. Tx changes in isolation are harmless too (new fields in struct just stay at their default values), but there is no point deploying a Tx that can display power before the Rx sends power.

3. The `VESC_MORE_VALUES` flag already gates the extended VESC query on the Rx side. Enabling it and populating the new `telemetry` fields is additive, not breaking.

**Sequence:**

```
Step 1 — Rx: Enable VESC_MORE_VALUES, populate telemetry fields
  a. Add new fields to TelemetryPacket in BREmote_V2_Rx.h
  b. In VESC.ino: assign motCur, batCur, duty to telemetry fields with encoding
  c. Compute power = fbatVolt * batCur, encode to foil_power
  d. Flash Rx, verify with serial debug that telemetry indices 4+ appear in packets

Step 2 — Tx: Add matching fields and display logic
  a. Add same new fields to TelemetryPacket in BREmote_V2_Tx.h (identical layout)
  b. In Display.ino / loop(): read new fields and render
  c. Flash Tx, verify display shows new values

Step 3 — Validate backwards compatibility
  a. Flash only Rx with new firmware, keep Tx on old firmware
  b. Confirm old Tx continues operating normally (no crash, no display corruption)
```

---

## Patterns to Follow

### Pattern 1: Named-Field Write on Rx, Indexed Read Over Wire, Named-Field Write on Tx

The Rx always writes `telemetry.foil_power = computed_value` by name — it never cares about byte position for assignment. The index sent over wire is implicitly the struct offset via the `ptr[telemetry_index]` pointer walk. The Tx writes `ptr[received_index] = received_value` by offset. This keeps the encode/decode pipeline zero-maintenance: adding a field to the struct automatically adds it to the cycle with no other code changes.

```c
// Rx: populate (VESC.ino) — always by name
telemetry.foil_power = (uint8_t)constrain((int)(fbatVolt * batCur_amps), 0, 255 * 20) / 20;

// Rx: transmit (Radio.ino) — no change needed, automatic
sendArray[3] = telemetry_index;
sendArray[4] = ptr[telemetry_index];
telemetry_index++;
if (telemetry_index >= sizeof(TelemetryPacket)) telemetry_index = 0;

// Tx: receive (Radio.ino) — no change needed if struct matches
if (rcvArray[3] < sizeof(TelemetryPacket)) ptr[rcvArray[3]] = rcvArray[4];

// Tx: consume (Display.ino) — always by name
uint16_t watts = telemetry.foil_power * 20;
```

### Pattern 2: Compile-Time Gating with #define

New VESC fields requested and relayed only when `VESC_MORE_VALUES` is defined. Mirror this for each new value group:

```c
#ifdef VESC_MORE_VALUES
    motCur = buffer_get_int32(message, &cnt);
    batCur = buffer_get_int32(message, &cnt);
    duty   = buffer_get_int16(message, &cnt);

    // Compute and store — only when values are actually requested
    float batCur_amps = batCur / 100000.0f;
    float watts = fbatVolt * batCur_amps;
    telemetry.foil_power = (uint8_t)constrain(watts / 20.0f, 0, 255);
    telemetry.foil_motor_current = (uint8_t)constrain(abs(motCur) / 100000, 0, 255);
    telemetry.foil_bat_current   = (uint8_t)constrain(abs(batCur) / 100000, 0, 255);
    telemetry.foil_duty          = (uint8_t)constrain(duty / 10, 0, 100);
#endif
```

Without `VESC_MORE_VALUES`, the struct fields remain at their default `0xFF` (sentinel "no data"), and those indices still cycle over the wire — Tx receives 0xFF for unknown values and can treat it as "not available."

---

## Anti-Patterns to Avoid

### Anti-Pattern 1: Reordering Existing Struct Fields

**What:** Moving or inserting fields before existing ones (e.g., putting `foil_power` before `error_code`).

**Why bad:** The index values are byte offsets in the struct. Index 3 is `error_code` in all deployed firmware. Moving it breaks the meaning of index 3 in any paired Tx/Rx running mixed firmware versions. Existing units in the field would display the wrong data silently.

**Instead:** Always append new fields. Never reorder. The `link_quality` comment "This must be the last entry" is exactly this constraint — enforce it for every new addition.

### Anti-Pattern 2: Changing the 6-Byte Packet Format

**What:** Increasing packet size to send multiple values per transmission, or changing byte layout.

**Why bad:** Both sides use `radio.implicitHeader(6)` and `radio.readData(rcvArray, 6)`. Any size change requires simultaneous firmware update on both units, destroying independent upgrade capability. The existing format has room for 256 indices — no pressure to expand.

**Instead:** Keep transmitting one index/value pair per 100ms cycle. Add indices to the struct.

### Anti-Pattern 3: Resetting telemetry_index on Reconnect or Config Change

**What:** Setting `telemetry_index = 0` whenever a new connection is established or config applied.

**Why bad:** No harm functionally, but creates unnecessary coupling between state machines. The Tx does not depend on index ordering — it writes to whichever offset arrives. Index cycling is stateless from the Tx's perspective.

**Instead:** Let `telemetry_index` free-run indefinitely. Tx handles arbitrary index ordering correctly.

### Anti-Pattern 4: Encoding Signed Values Without a Convention

**What:** Encoding motor current as a signed value without documenting the convention, then decoding as unsigned on the Tx side (or vice versa).

**Why bad:** Motor current can be negative (regenerative braking). If Rx encodes as `int8_t` cast to `uint8_t` but Tx treats it as unsigned, displayed values wrap around 128 to large numbers.

**Instead:** For this milestone, encode as absolute value (magnitude only) — eFoil riders care about current draw, not direction. If signed representation is needed later, define a clear convention: `0 = -127A, 127 = 0A, 254 = +127A` (offset encoding) and document it in both headers.

---

## Scalability Considerations

| Concern | Current (5 indices) | Extended (9 indices) | At limit (256 indices) |
|---------|--------------------|-----------------------|------------------------|
| Cycle time | 500ms | 900ms | 25.6 seconds |
| Memory | 5 bytes | 9 bytes | 256 bytes |
| Wire overhead | None (same 6-byte packet) | None | None |
| Tx bounds check cost | O(1) comparison | O(1) comparison | O(1) comparison |

The 256-index limit is theoretical (uint8_t index field). Practical limit is whatever cycle time is acceptable for the slowest-updating value. At 9 indices, 900ms cycle means each value refreshes ~1.1 Hz — adequate for battery%, temp, and power display.

If cycle time becomes a concern at higher index counts, a future optimization is to skip indices whose values have not changed, but this adds complexity and should not be done now.

---

## VESC Bitmask Reference (COMM_GET_VALUES_SELECTIVE)

For adding new VESC fields in the future, the bitmask layout from the current code and VESC firmware (`commands.c` line 377):

```
Bitmask byte 4 (vesc_command[4]):
  bit 0: FET temp (int16, °C × 10)
  bit 1: Motor temp (int16, °C × 10)        <-- not yet requested
  bit 2: Motor current (int32, mA × 100)
  bit 3: Battery current (int32, mA × 100)
  bit 4: (reserved)
  bit 5: ERPM (int32)                        <-- speed source
  bit 6: Duty cycle (int16, % × 10)
  bit 7: Amp-hours (float)                   <-- deferred

Bitmask byte 3 (vesc_command[3]):
  bit 0: Battery voltage (int16, V × 10)
```

Response fields are packed in bitmask order with no padding. Any new field requires updating the bitmask, adding a `buffer_get_*` parse call in the correct order, and assigning to the telemetry struct.

Motor temperature (bit 1 of byte 4) would be requested by changing:
```c
vesc_command[4] = (1<<FET_TEMP) + (1<<MotTemp) + (1<<MotCurrent) + ...
```
And parsing it between fetTemp and motCur in the response loop.

ERPM for speed (bit 5 of byte 4) requires adding `#define ERPM 5` and requesting it. Speed calculation:
```
km/h = erpm / pole_pairs / gear_ratio * wheel_circumference_m * 60 / 1000
```
This requires pole_pairs, gear_ratio, and wheel_diameter in `confStruct`.

---

## Sources

All findings are HIGH confidence — derived directly from source code at commit ef2ec4c.

- `Source/V2_Integration_Rx/VESC.ino` — VESC query, parse, telemetry population
- `Source/V2_Integration_Rx/Radio.ino` — `triggeredReceive()`, telemetry cycling loop
- `Source/V2_Integration_Tx/Radio.ino` — `waitForTelemetry()`, bounds check, struct write
- `Source/V2_Integration_Tx/BREmote_V2_Tx.h` — `TelemetryPacket` definition (Tx)
- `Source/V2_Integration_Rx/BREmote_V2_Rx.h` — `TelemetryPacket` definition (Rx), `telemetry_index`
- `.planning/PROJECT.md` — milestone requirements, constraints, key decisions
