# Feature Landscape

**Domain:** VESC-based eFoil / electric skateboard wireless remote telemetry
**Researched:** 2026-02-28
**Milestone:** Extended VESC telemetry for BREmote V2

---

## Context: Display and Protocol Constraints

All feature decisions must be filtered through two hard constraints:

**7x10 LED matrix display (Tx):**
- Renders two 7-segment digits only (positions 0–9 and letters A/B/P/T/U etc.)
- No decimals, no units, no labels — just the number
- Mode label shown briefly during mode switch (e.g., "Po" for power, "AP" for amps)
- Values must compress to 0–99 or 0–255 with meaningful resolution

**Single-byte LoRa telemetry channel:**
- One index byte + one value byte per 100ms radio cycle
- All new values must fit in uint8_t (0–255), no multi-byte values
- Values are indexed by position in `TelemetryPacket` struct — adding a field appends a new index
- Existing indices 0–4 are occupied: foil_bat(0), foil_temp(1), foil_speed(2), error_code(3), link_quality(4)
- Old Tx firmware safely ignores unknown indices (bounds check: `rcvArray[3] < sizeof(TelemetryPacket)`)

**VESC bitmask fields already in code (Rx VESC.ino):**
- Byte 4 bit 0 = FET temperature (temp_mos, scale /10, int16 → signed °C×10)
- Byte 4 bit 2 = Motor current (current_motor, int32 → A×100, signed)
- Byte 4 bit 3 = Battery current / input current (current_in, int32 → A×100, signed)
- Byte 4 bit 6 = Duty cycle (duty_now, int16 → %×1000)
- Byte 3 bit 0 = Battery voltage (v_in, int16 → V×10)

**Fields available in mc_values but not yet requested:**
- temp_motor (motor temperature)
- rpm (ERPM — requires pole pairs + gear ratio to convert to speed)
- amp_hours / watt_hours (energy counters, reset on VESC power cycle)
- fault_code (mc_fault_code enum, 0–26)
- tachometer / tachometer_abs (odometer-style pulse count)

---

## Table Stakes

Features that any VESC-based remote must provide. Missing = product feels incomplete
or unsafe for eFoil use. These are the non-negotiables.

| Feature | Why Expected | Source VESC Value | Encoding | Display | Complexity |
|---------|--------------|-------------------|----------|---------|------------|
| Battery % | Primary ride-ending safety signal — rider must know when to return to shore | `v_in` → converted via lookup table `getUbatPercent()` | Already 0–100 uint8 | "BA" + 2-digit % | Already done (index 0) |
| FET temperature | ESC overtemp is the #1 VESC fault in eFoil; rider needs to back off | `temp_mos` / 10 → already truncates to 0–255°C | Already direct cast | "TP" + 2-digit °C | Already done (index 1) |
| Speed | Rider's core performance metric; also needed for lift-off awareness | GPS `foil_speed` OR `rpm` → km/h calc on Rx | Already 0–99 km/h | "SP" + 2-digit km/h | Already done (index 2) |
| Error / fault indication | FAULT_CODE_OVER_TEMP_FET, OVER_VOLTAGE, etc. must surface to rider | `fault_code` enum (0–26) | uint8 direct; 0 = OK | Handled separately via `error_code` field + display blink | Already done (index 3) |
| Link quality | Radio dropout is a safety event on water | RSSI + SNR → normalized 0–10 | Already done | Bargraph row | Already done (index 4) |
| Motor wattage / power | Most-requested new value in eFoil communities; lets rider manage battery draw and sustain flight efficiently | `current_in × v_in` calculated on Rx | Scale: W/10 → 0–250 = 0–2500W; covers all practical eFoil builds | "Po" + 2-digit (decawatts) | Low — 3 lines Rx, new TelemetryPacket field |

**Note on power:** The eFoil community (Foil Drive documentation, BREmote forum discussions) and VESC remote ecosystem consistently identify real-time power draw as the top missing telemetry value. It is the single most actionable number for a rider: too high = approaching thermal limit and wasting battery; optimal range = efficient flight. Battery % alone gives no indication of *rate* of drain. Power is derived (not a direct VESC value) but `current_in` and `v_in` are both already available in the `VESC_MORE_VALUES` path.

---

## Differentiators

Features that set BREmote V2 apart from basic VESC remotes. Not strictly needed, but
high value for riders who want deeper insight.

| Feature | Value Proposition | Source VESC Value | Encoding | Display | Complexity |
|---------|-------------------|-------------------|----------|---------|------------|
| Motor current (A) | Shows motor loading independent of voltage; useful for tuning and detecting prop/foil cavitation events | `current_motor` int32, A×100 | Scale: value/100 clamped 0–200A → direct uint8; 0xFF = invalid | "AM" + 2-digit amps | Low — already parsed in VESC_MORE_VALUES, just needs TelemetryPacket field |
| Battery current / input current (A) | Shows draw from battery; combined with voltage gives power; independently useful for BMS monitoring | `current_in` int32, A×100 | Scale: value/100 clamped 0–200A → direct uint8; negative = regen | "AI" + 2-digit amps | Low — already parsed, needs routing |
| Duty cycle (%) | Tells rider how hard ESC is working; high duty + high temp = danger zone | `duty_now` int16, %×1000 | Scale: value/10 → 0–100%; direct uint8 | "dU" + 2-digit % | Low — already parsed, needs routing |
| Motor temperature | Distinguishes overtemp source: FET vs motor windings. Inrunner motors heat up differently than FETs | `temp_motor` int16, °C×10 | Scale: value/10 → 0–150°C direct uint8 | "TM" + 2-digit °C | Low — new bitmask bit + parser + field |
| ERPM-derived speed (motor-based) | Speed without GPS dependency; always available; useful for indoor testing | `rpm` int32 (ERPM) → speed = ERPM / (pole_pairs × gear_ratio) × wheel_circ | Config params needed in confStruct | "SP" + 2-digit | Medium — requires new confStruct fields (pole_pairs, gear_ratio, wheel_dia) |

**Note on ERPM-derived speed:** GPS is already implemented for speed. Motor-derived speed is a differentiator for non-GPS builds or GPS-free indoor/test bench use. It requires adding 3 float config parameters to `confStruct` (motor pole pairs, gear ratio, wheel/prop diameter), which bumps `SW_VERSION` and will trigger config migration for existing users. This is medium complexity and should be a separate sub-task.

---

## Anti-Features

Features to explicitly NOT build in this milestone. For a compact LED-matrix remote,
these create more problems than they solve.

| Anti-Feature | Why Avoid | What to Do Instead |
|--------------|-----------|-------------------|
| Amp-hours / watt-hours energy tracking | Resets on every VESC power cycle unless actively tracked; misleading without session management; requires 2+ bytes for useful range (0–65535 Wh); does not fit uint8 | Defer. Requires session-aware state on Rx and larger packet format |
| Watt-hours remaining (range estimate) | Requires knowing pack capacity, current SoC curve, and average efficiency — none of which BREmote tracks reliably | Battery % already captures SoC; power wattage captures current draw |
| Fault code text display | "FAULT_CODE_OVER_TEMP_FET" cannot be shown on a 2-digit LED display | Show numeric error code 1–26 via existing error_code field; fault bitmapping already in System.ino |
| Tachometer / odometer | Needs session reset logic, rollover handling, 2+ bytes; 7x10 display cannot show meaningful distance values | Out of scope for this milestone |
| IMU / pitch / roll data | Not in COMM_GET_VALUES_SELECTIVE; requires separate VESC command; no safety relevance for remote display | Not applicable to eFoil use case |
| Live UART relay (vescRelayBuffer) | Current code captures `vescRelayBuffer` for future relay use — do not activate this without a defined protocol consumer | The relay path exists in code but is unused; leave it dormant |
| Multi-VESC / CAN bus aggregation | Some eFoils use dual-ESC setups via CAN; querying both requires CAN bus integration on Rx | Not in scope; single VESC UART target only |
| Runtime-configurable telemetry selection | Adds UI complexity on a button-only remote; overkill when reflash is accessible | Compile-time `#define` flags per PROJECT.md decision |

---

## Feature Dependencies

```
Power display (watts) → current_in (BatCurrent bit) + v_in (BatVolt bit) both requested
Motor current display  → current_motor (MotCurrent bit) already in VESC_MORE_VALUES
Battery current display → current_in (BatCurrent bit) already in VESC_MORE_VALUES
Duty cycle display     → duty_now (Duty bit) already in VESC_MORE_VALUES
Motor temp display     → temp_motor (new bitmask bit needed, new parser field)

ERPM-derived speed     → rpm bit in VESC bitmask + confStruct changes (pole pairs etc.)
                       → SW_VERSION bump → config migration path required
                       → Decouple from other telemetry features: implement separately

All new display modes  → DISPLAY_MODE_COUNT must increase + Hall.ino mode labels + Display.ino cases
All new telemetry      → TelemetryPacket struct gets new uint8_t fields (Rx and Tx must match)
All Tx changes         → Backwards compat: old Rx only sends indices 0–4; new Tx must handle 0xFF gracefully
All Rx changes         → VESC_MORE_VALUES must be enabled (currently commented out)
```

---

## Byte Encoding Reference

For each new value, a uint8_t encoding scheme is needed. Values outside range are
clamped; 0xFF is reserved as "data not available" sentinel (matches existing convention).

| Value | VESC Raw | Scale Factor | uint8 Range | Real-World Range | Resolution | Notes |
|-------|----------|--------------|-------------|-----------------|------------|-------|
| Power (W) | `current_in × v_in` float | ÷10 | 0–250 | 0–2500W | 10W steps | 2500W covers 40V×60A, typical eFoil max; use 0xFF for negative/regen |
| Motor current (A) | int32 ÷100 | ÷1 | 0–200 | 0–200A | 1A | Clamped; 0xFF = invalid |
| Battery current (A) | int32 ÷100 | ÷1 | 0–200 | 0–200A | 1A | Signed source; negative = regen; encode as 0 if regen |
| Duty cycle (%) | int16 ÷1000 | ÷1 | 0–100 | 0–100% | 1% | Direct; 0xFF = invalid |
| Motor temp (°C) | int16 ÷10 | ÷1 | 0–150 | 0–150°C | 1°C | 0xFF = no sensor / invalid |

---

## Display Mode Extension Plan

Current display modes in `BREmote_V2_Tx.h` (DISPLAY_MODE_COUNT = 5):

```
0: DISPLAY_MODE_TEMP    (FET °C)
1: DISPLAY_MODE_SPEED   (km/h)
2: DISPLAY_MODE_BAT     (battery %)
3: DISPLAY_MODE_THR     (throttle %, local)
4: DISPLAY_MODE_INTBAT  (remote battery voltage ×10)
```

Proposed additions:

```
5: DISPLAY_MODE_POWER   (decawatts, e.g., "23" = 230W)  — label: "Po"
6: DISPLAY_MODE_MOTCUR  (motor amps)                     — label: "AM"
7: DISPLAY_MODE_DUTY    (duty %)                         — label: "dU"
8: DISPLAY_MODE_MOTTEMP (motor °C)                       — label: "TM"
```

Battery current (AI) is lower priority for a dedicated display mode — power subsumes
the key information. Motor temp (TM) is more actionable than battery current (AI).

Modes that lack valid telemetry data are automatically skipped during cycling
(existing `isDisplayModeAvailable()` logic in Hall.ino handles this gracefully).

---

## MVP Recommendation

For the first implementation sprint:

1. **Power (watts)** — Enable `VESC_MORE_VALUES`, compute `current_in × v_in` on Rx, add `foil_power` to `TelemetryPacket`, add `DISPLAY_MODE_POWER` to Tx. End-to-end, maximally useful.

2. **Duty cycle (%)** — Zero additional VESC parsing work (already in `VESC_MORE_VALUES`), single new struct field, single new display mode. Highest value-to-effort ratio after power.

3. **Motor current (A)** — Same zero-parsing-work situation. Useful for advanced riders diagnosing motor loading.

Defer to subsequent sprints:
- **Motor temperature** — Requires new bitmask bit and parser extension; lower rider priority than power/duty
- **ERPM-derived speed** — Requires confStruct changes and SW_VERSION bump; higher risk; GPS already covers this need
- **Battery current as standalone mode** — Subsumed by power display; add only if riders request it explicitly

---

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| VESC available data | HIGH | Confirmed from vesc_datatypes.h mc_values struct + VESC.ino bitmask defines |
| Encoding schemes | HIGH | Cross-validated against existing foil_temp encoding (°C ÷10 pattern) |
| Rider priority (power first) | MEDIUM | Supported by Foil Drive docs, VESC community forums, BREmote project context; not directly surveyed |
| Display mode extension | HIGH | Confirmed from Display.ino + Hall.ino source; DISPLAY_MODE_COUNT pattern is clear |
| ERPM-speed complexity | HIGH | confStruct versioning impact confirmed from PROJECT.md + codebase constraints |
| Anti-feature exclusions | HIGH | All exclusions derive from hard packet format and display constraints, not opinion |

---

## Sources

- `Source/V2_Integration_Rx/vesc_datatypes.h` — mc_values struct (lines 1170–1194), mc_fault_code enum (lines 125–153)
- `Source/V2_Integration_Rx/VESC.ino` — COMM_GET_VALUES_SELECTIVE bitmask, VESC_MORE_VALUES parse path
- `Source/V2_Integration_Rx/BREmote_V2_Rx.h` — TelemetryPacket struct, telemetry_index cycling
- `Source/V2_Integration_Tx/BREmote_V2_Tx.h` — DISPLAY_MODE_* constants, TelemetryPacket (Tx copy)
- `Source/V2_Integration_Tx/Radio.ino` — Telemetry reception: `ptr[rcvArray[3]] = rcvArray[4]` pattern
- `Source/V2_Integration_Tx/Display.ino` — Display mode switch, bargraph rendering
- `.planning/PROJECT.md` — Constraints, key decisions, out-of-scope items
- [Foil Drive Controller Features](https://help.foildrive.com/en-US/foil-drive-controller-features-201587) — Commercial eFoil remote telemetry reference (throttle %, RPM, battery voltage, temp, battery %)
- [BREmote Future Development Topic — FOIL.zone](https://foil.zone/t/bremote-future-development-topic/20820) — Community feature discussion
- [BREmote V2 Open Source Remote — FOIL.zone](https://foil.zone/t/bremote-v2-open-source-remote/23689) — Active development thread
- [Super VESC Display — Endless Sphere](https://endless-sphere.com/sphere/threads/super-vesc-display-advanced-telemetry-display-for-vesc-based-ev-projects.129327/) — VESC telemetry ecosystem reference: speed, voltage, current, power, motor/controller temp, Wh, Ah
- [VESC COMM_GET_VALUES_SELECTIVE issue #94](https://github.com/vedderb/bldc/issues/94) — Bitmask field parsing behavior
- [VescUartLite](https://github.com/gitcnd/VescUartLite) — VESC UART field scale factors reference (LOW confidence — single source)
