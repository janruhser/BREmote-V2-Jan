# Project Research Summary

**Project:** BREmote V2 — Extended VESC Telemetry
**Domain:** Embedded wireless remote control — LoRa telemetry pipeline on ESP32
**Researched:** 2026-02-28
**Confidence:** HIGH

## Executive Summary

BREmote V2 is a mature, working embedded system targeting a focused milestone: extending VESC motor controller telemetry so that the handheld Tx remote can display additional real-time values (power, duty cycle, motor current, motor temperature, and speed). The architecture is already proven in production — the research found no need to change fundamental technology choices. The entire telemetry mechanism is structurally self-extending: adding a field to the `TelemetryPacket` struct on both sides is sufficient to transmit and receive a new value. This is the primary lever and the recommended approach throughout every phase.

The recommended delivery sequence is Rx first, then Tx. Enabling `VESC_MORE_VALUES` on the Rx side and routing `batCur`, `duty`, and `motCur` into the `TelemetryPacket` struct (they are already parsed but discarded) is low risk, additive, and backwards-compatible with old Tx firmware. Power wattage (derived as `batCur × v_in` on the Rx) is the highest-value first deliverable for eFoil riders. Motor temperature, ERPM-derived speed, and battery current as standalone display modes are secondary deliverables with modestly higher implementation risk.

The dominant risk class across all phases is silent failure: wrong `VESC_PACK_LEN` makes all VESC telemetry go dark with no visible error; wrong encoding scale factors produce systematically incorrect numbers that look plausible; struct field insertion before existing indices corrupts the wire protocol on mixed-firmware units. All three of these are preventable through discipline (update byte count comment immediately when changing the bitmask, encode-by-name and document the scale factor in both files, append-only struct additions). Speed via ERPM carries the most structural risk — it requires `confStruct` changes, a `SW_VERSION` bump, and a carefully named `motor_pole_pairs` field — and should be decoupled from the earlier, simpler telemetry additions.

## Key Findings

### Recommended Stack

The existing stack is not changing. All stack research was scoped to the VESC UART extension specifically — exact bitmask positions, encoding schemes for single-byte LoRa transmission, and timing implications of requesting additional fields. Key findings: VESC `COMM_GET_VALUES_SELECTIVE` response is a packed byte stream in bitmask-bit-ascending order, with the 32-bit request mask echoed back as the first four bytes of the payload (skip with `cnt = 5`). The extended bitmask for the milestone target fields {FET_TEMP, MotorTemp, BatCurrent, Duty, ERPM, BatVolt} produces a 21-byte payload (up from 19), raising `VESC_PACK_LEN` from 19 to 21. Timing impact is under 0.2ms additional — no concern within the 100ms radio cycle.

**Core technologies:**
- `COMM_GET_VALUES_SELECTIVE` (VESC UART): selective field query — keeps wire payload small; bits 0, 1, 3, 6, 7, 8 are the target set for this milestone
- Single-byte LoRa telemetry slot: forces scale-then-truncate encoding per field; encode on Rx, decode label on Tx; `uint8_t` mandatory for all fields
- `VESC_MORE_VALUES` compile flag: existing gate for extended bitmask; controls both bitmask sent and expected response length; must remain consistent with `VESC_PACK_LEN`
- ERPM-to-speed formula: `km/h = (abs(erpm) / motor_pole_pairs / gear_ratio) * (PI * wheel_diameter_m) * 60 / 1000`; hardcode initially, add to `confStruct` in a later phase with `SW_VERSION` bump

### Expected Features

**Must have (table stakes — already implemented):**
- Battery % — primary safety signal; already at index 0
- FET temperature — ESC overtemp is leading eFoil fault; already at index 1
- Speed (km/h) — core performance metric; GPS path already in place
- Fault code — safety alert; already at index 3
- Link quality — radio dropout is a safety event; already at index 4

**Must have (table stakes — missing, highest priority):**
- Power (watts) — most-requested missing value; derived from `batCur × v_in`; both sources already parsed in `VESC_MORE_VALUES` path; tells rider rate of drain and thermal approach
- Duty cycle (%) — zero additional VESC parsing work; already decoded in `VESC_MORE_VALUES` and discarded; shows ESC loading directly

**Should have (differentiators):**
- Motor temperature — distinguishes FET vs winding overtemp; requires new bitmask bit (bit 1) and one new `buffer_get_int16` parse call inserted in correct position
- Motor current (A) — useful for tuning and cavitation detection; already parsed in `VESC_MORE_VALUES`; needs routing to struct
- Battery current (A) as standalone mode — subsumed by power display; lower priority; add only on rider request

**Defer (v2+ or separate sprint):**
- ERPM-derived speed — requires `confStruct` additions and `SW_VERSION` bump; GPS already covers the need for most builds; decouple from other telemetry features
- Amp-hours / watt-hours — resets on every VESC power cycle; needs session management; two-byte range issue
- Range estimate — requires pack capacity, SoC curve, efficiency model; out of scope
- Multi-VESC / CAN bus aggregation — out of scope

### Architecture Approach

The telemetry protocol is structurally encode as byte-array-indexed struct. `TelemetryPacket` is treated as a raw `uint8_t[]` on the wire — Rx transmits `ptr[telemetry_index]` and increments, Tx receives and writes `ptr[received_index]`. Adding a field to the struct on both sides is the only protocol change needed; the cycling and backwards-compatibility (bounds check `rcvArray[3] < sizeof(TelemetryPacket)`) work automatically. The ordering constraint is strict: existing indices 0-3 must not move; new fields append before `link_quality` (which must remain last). All struct fields must remain `uint8_t` to preserve atomic access across FreeRTOS tasks on dual-core ESP32 without additional synchronization.

**Major components:**
1. `Rx/VESC.ino:getValuesSelective` — VESC UART query with `COMM_GET_VALUES_SELECTIVE`; parses packed response; computes derived values (power); populates `telemetry` struct by name
2. `Rx/Radio.ino:triggeredReceive` — fires on each inbound Tx control packet (100ms); walks `telemetry` struct byte-by-byte via `telemetry_index`; transmits one index/value pair per cycle over LoRa
3. LoRa 6-byte wire packet — `[DestAddr(3), DataIndex, DataValue, CRC8]`; index is struct byte offset; CRC8 protected; 250kHz BW, SF6, ~4ms time-on-air
4. `Tx/Radio.ino:waitForTelemetry` — receives packet; bounds-checks index; writes into local `telemetry` struct by offset; silently discards indices beyond local struct size (backwards compatibility)
5. `Tx/Display.ino` + `loop()` — reads `telemetry` fields by name; renders on HT16K33 7x10 LED matrix; display modes cycle via Hall.ino; `isDisplayModeAvailable()` skips modes with 0xFF sentinel

**Proposed extended struct (both sides, identical):**
```
index 0: foil_bat         (battery %, existing)
index 1: foil_temp        (FET temp °C, existing)
index 2: foil_speed       (speed km/h, existing/GPS)
index 3: error_code       (fault flags, existing)
index 4: foil_power       (watts / scale, NEW)
index 5: foil_motor_current (motor A, NEW)
index 6: foil_bat_current  (battery A, NEW)
index 7: foil_duty         (duty %, NEW)
--- link_quality MUST remain last ---
index 8: link_quality     (was 4, NEW position)
```

Full cycle time at 9 indices: 900ms (acceptable for eFoil telemetry display).

### Critical Pitfalls

1. **VESC_PACK_LEN mismatch after bitmask extension** — If the bitmask gains new bits but `VESC_PACK_LEN` is not updated to match the new response byte count, the gate `receiveFromVESC() == VESC_PACK_LEN` silently fails and all VESC telemetry goes dark (shows 0xFF forever). Prevention: update the byte count comment and `VESC_PACK_LEN` in the same commit that changes the bitmask; enable `DEBUG_VESC` during development to print raw received byte count.

2. **Struct field insertion before existing indices** — The wire protocol is the struct's byte offset layout. Inserting a new field before `error_code` (index 3) shifts all subsequent indices and silently corrupts paired units running mixed firmware. Prevention: append-only additions after index 3, always before `link_quality`; run the backwards-compatibility test (old Tx + new Rx) before any Tx flash.

3. **Wrong encoding scale factor — silent saturation or systematic error** — `batCur` from VESC is `int32` in units of milliAmps × 100; dividing by 10,000 instead of 100,000 produces 10× inflated watts, saturating `foil_power` at 255 immediately. Rx and Tx scale factors must agree exactly; they live in different files. Prevention: document scale factor as co-located comment in both `VESC.ino` and `Display.ino`; verify with known VESC Tool readings.

4. **Echoed mask in VESC response — parsing starts at `cnt = 5`, not 1** — `COMM_GET_VALUES_SELECTIVE` echoes the 32-bit request mask at the start of the response payload. The current parser correctly skips with `cnt = 5`. Any parser extension that resets `cnt` or adds fields without preserving this offset will silently misparse all fields. Prevention: preserve the `// Don't care about the Mask` comment and the `cnt = 5` start; add fields by appending `buffer_get_*` calls after existing ones.

5. **`confStruct` addition without SW_VERSION bump** — Adding `pole_pairs`, `gear_ratio`, and `wheel_diameter` to `confStruct` for ERPM speed calculation extends the struct's binary layout. If `SW_VERSION` is not incremented, old blobs load garbage into new fields with no visible error. If it is bumped, all users lose calibration (throttle expo, deadzone, pairing). Prevention: bump `SW_VERSION` on every struct change; set safe defaults that produce `foil_speed = 0xFF` (no display) rather than a wrong value when the new fields are uninitialized.

## Implications for Roadmap

Based on research, suggested phase structure:

### Phase 1: Core Telemetry Extension (Rx-side — Power, Duty, Motor Current)

**Rationale:** Three of the four new display values (`foil_power`, `foil_duty`, `foil_motor_current`) require zero new VESC bitmask work — they are already parsed by `VESC_MORE_VALUES` and discarded as local variables. This phase routes them to the `TelemetryPacket` struct, adds the power calculation (`batCur × v_in`), and extends both Rx and Tx structs. This is the highest value-to-effort ratio work and establishes the pattern for all subsequent phases. Build Rx first, flash and verify via `DEBUG_VESC`; then extend Tx and verify display.

**Delivers:** Power (watts), duty cycle (%), and motor current (A) visible on Tx display. Rx compatible with old Tx. `VESC_MORE_VALUES` enabled and fully exercised.

**Features:** `foil_power` (table stakes — top community request), `foil_duty` (differentiator, zero parsing work), `foil_motor_current` (differentiator, zero parsing work).

**Display modes added:** `DISPLAY_MODE_POWER` ("Po"), `DISPLAY_MODE_DUTY` ("dU"), `DISPLAY_MODE_MOTCUR` ("AM").

**Pitfalls to avoid:** Update `VESC_PACK_LEN` in same commit as bitmask (Pitfall 1); append struct fields after index 3 (Pitfall 2); document scale factors in both files (`watts/20`, `abs(mA×100)/100000`) (Pitfall 3); run backwards-compat test before Tx flash.

**Research flag:** No additional research needed. All bitmask positions, byte counts, and encoding schemes are confirmed HIGH confidence from source code.

---

### Phase 2: Motor Temperature

**Rationale:** Motor temperature requires one new bitmask bit (bit 1, `MotorTemp`) and a new `buffer_get_int16` parse call that must be inserted between `fetTemp` and `motCur` in the parser — in bit-ascending order. This is the only phase that requires modifying the parse order of existing fields, making it slightly higher risk than Phase 1. It is sequenced after Phase 1 because Phase 1 establishes the pattern and validates `VESC_MORE_VALUES` is working correctly first.

**Delivers:** Motor temperature (°C) visible on Tx display, distinct from FET temperature. Riders can distinguish winding overtemp from ESC overtemp.

**Features:** `foil_motor_temp` (differentiator — distinguishes FET vs motor winding overtemp).

**Display modes added:** `DISPLAY_MODE_MOTTEMP` ("TM").

**Pitfalls to avoid:** Insert `buffer_get_int16(message, &cnt)` for motor temp at bit position 1 — between `fetTemp` (bit 0) and `motCur` (bit 2) parse calls; update `VESC_PACK_LEN` by +2 bytes (int16); verify field order with `DEBUG_VESC` hex dump (Pitfalls 1, 12).

**Research flag:** No additional research needed. Bitmask bit position confirmed HIGH confidence. Parse insertion point documented in PITFALLS.md.

---

### Phase 3: ERPM-Derived Speed (Motor-Based, GPS-Independent)

**Rationale:** Separated from earlier phases because it requires `confStruct` additions (`motor_pole_pairs`, `gear_ratio`, `wheel_diameter_m`), a `SW_VERSION` bump triggering user calibration loss, and careful naming to avoid the pole count vs pole pairs confusion that consistently produces 2× speed errors. GPS already covers the speed need for most builds — this phase is valuable for non-GPS builds and test bench use. It is the highest-risk phase due to `confStruct` versioning implications.

**Delivers:** Motor-derived speed (km/h) that works without GPS. Enables indoor/bench testing without GPS lock.

**Features:** ERPM-derived speed (differentiator — GPS-independent speed source).

**Stack:** Adds `motor_pole_pairs`, `gear_ratio`, `wheel_diameter_m` to `confStruct`; increment `SW_VERSION`; add `?get`/`?set` and web config support for new fields.

**Pitfalls to avoid:** Name field `motor_pole_pairs` (not `motor_poles`) to prevent 2× error (Pitfall 9); bump `SW_VERSION` with defaults that produce `foil_speed = 0xFF` for uninitialised state (Pitfall 5); add range validation warning if `motor_pole_pairs` > 20.

**Research flag:** May benefit from a brief spike to confirm: (a) which VESC bitmask byte/bit ERPM lives at (confirmed bit 7 in STACK.md; cross-check against current `vesc_command[]` byte ordering before coding), (b) whether `confStruct` currently uses `__attribute__((packed))` — if not, adding float fields may shift existing fields due to alignment (Pitfall 5 mitigation).

---

### Phase Ordering Rationale

- Phase 1 first because three of four values are already parsed and need only routing — it is the safest possible extension and delivers the highest-value output (power display).
- Phase 2 second because it shares the same VESC query infrastructure validated in Phase 1 but introduces a new field with a parse-order constraint — better to validate the infrastructure is working before adding complexity.
- Phase 3 last because it is architecturally orthogonal (config system, not telemetry cycling) and carries the highest risk of user-visible regression (calibration loss on `SW_VERSION` bump). Deferring it ensures the core telemetry is stable before touching `confStruct`.
- Battery current as a standalone display mode is not assigned its own phase — it is already decoded in Phase 1 (`foil_bat_current`) and can be added as an optional display mode within Phase 1 or a minor follow-up if rider feedback requests it.

### Research Flags

Phases likely needing additional research spikes during planning:
- **Phase 3:** Confirm ERPM bitmask bit position in the current `vesc_command[]` byte ordering (bit 7 of byte 4 per STACK.md vs bit 5 per ARCHITECTURE.md — minor discrepancy between research files that must be resolved against the actual VESC firmware source before coding). Confirm `confStruct` packing behavior before adding float fields.

Phases with standard, well-documented patterns (skip research-phase):
- **Phase 1:** All patterns confirmed from source code; VESC_MORE_VALUES path already exists and is partially implemented.
- **Phase 2:** Parse insertion point explicitly documented; motor temp byte type (int16) confirmed.

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | Bitmask positions verified against multiple independent sources including running BREmote production code; encoding schemes validated against existing foil_temp pattern |
| Features | HIGH | Feature priority derived from eFoil community sources, Foil Drive commercial reference, and hard display/packet constraints; encoding schemes cross-validated |
| Architecture | HIGH | All findings derived directly from source code at commit ef2ec4c; no inference required |
| Pitfalls | HIGH (code-derived) / MEDIUM (external) | Critical pitfalls come from source code audit; firmware/framework behavior pitfalls (Serial1.flush, volatile) are MEDIUM from community issue trackers |

**Overall confidence:** HIGH

### Gaps to Address

- **ERPM bitmask bit position discrepancy:** STACK.md states ERPM is bit 7 of `vesc_command[4]`; ARCHITECTURE.md states bit 5. Both reference the same VESC firmware source (`commands.c`). This must be resolved by reading `commands.c` directly before Phase 3 coding. The current BREmote code does not request ERPM, so there is no production reference to check against.

- **`confStruct` packing status:** PITFALLS.md notes that `confStruct` lacks `__attribute__((packed))` unlike `TelemetryPacket`, making it vulnerable to alignment shifts when new fields are added. Verify before Phase 3 whether packing is applied or needs to be added safely (with a `static_assert` on `sizeof(confStruct)`).

- **Power scale factor for production eFoils:** STACK.md recommends scale `/50` (range 0-12,750W); ARCHITECTURE.md recommends `/20` (range 0-5,100W). The appropriate scale depends on the actual peak wattage of the target hardware. The `/20` scale (5,100W max) may saturate for high-voltage, high-current setups (e.g., 60V × 150A = 9,000W). Recommend `/50` for broader hardware compatibility; validate during Phase 1 testing with actual hardware.

- **`VESC_MORE_VALUES` currently commented out:** Both ARCHITECTURE.md and FEATURES.md confirm that `VESC_MORE_VALUES` is not enabled in current production firmware. Enabling it in Phase 1 activates parsing of three fields that have been tested only at the code level, not in a running unit. Verify with `DEBUG_VESC` on actual hardware before Tx flash.

## Sources

### Primary (HIGH confidence)
- `Source/V2_Integration_Rx/VESC.ino` — bitmask, VESC_PACK_LEN, parse loop, telemetry population (commit ef2ec4c)
- `Source/V2_Integration_Rx/Radio.ino` — triggeredReceive, telemetry cycling mechanism
- `Source/V2_Integration_Tx/Radio.ino` — waitForTelemetry, bounds check, backwards compatibility gate
- `Source/V2_Integration_Rx/BREmote_V2_Rx.h` — TelemetryPacket layout, confStruct, VESC_PACK_LEN conditionals
- `Source/V2_Integration_Tx/BREmote_V2_Tx.h` — TelemetryPacket (Tx copy), DISPLAY_MODE constants
- `vedderb/bldc comm/commands.c` — authoritative VESC bitmask field order
- `vedderb/bldc issue #94` — confirms mask-is-echoed behavior in response payload

### Secondary (MEDIUM confidence)
- VescUartLite (github.com/gitcnd/VescUartLite) — cross-confirms bit positions 0-17 and mask-skip behavior
- `.planning/codebase/CONCERNS.md` — config versioning risk, volatile race conditions, VESC CRC silent failure
- VESC Project community (vesc-project.com/node/2932) — ERPM to RPM pole pairs formula
- ESP32 arduino-esp32 issue #3095 — Serial1.flush() bidirectional flush behavior

### Tertiary (LOW confidence)
- Foil Drive controller documentation — commercial eFoil remote telemetry reference for feature priority
- BREmote FOIL.zone community threads — community feature demand signals (power, speed priority)
- Endless Sphere VESC display thread — VESC telemetry ecosystem: speed, voltage, current, power, temp

---
*Research completed: 2026-02-28*
*Ready for roadmap: yes*
