---
phase: 01-rx-vesc-extension
plan: 01
status: complete
started: 2026-03-02
completed: 2026-03-02
---

# Plan 01-01 Summary: Rx VESC Extension

## What Was Built

Enabled `VESC_MORE_VALUES` compile flag on the Rx and extended `TelemetryPacket` with a new `foil_power` field. Added power calculation (battery voltage × battery current) encoded as a single byte at `/50` scale factor (0–12,750W range).

## Changes

### Source/V2_Integration_Rx/BREmote_V2_Rx.h
- Uncommented `#define VESC_MORE_VALUES` — activates extended VESC query for motor current, battery current, duty cycle
- Added `uint8_t foil_power = 0xFF;` to `TelemetryPacket` at index 4 (between `error_code` and `link_quality`)
- `sizeof(TelemetryPacket)` now 6 (was 5) — telemetry cycling automatically includes new field

### Source/V2_Integration_Rx/VESC.ino
- Added power calculation inside `#ifdef VESC_MORE_VALUES` block:
  - `batCur / 100000.0f` converts mA×100 to Amps
  - `fbatVolt * batCur_amps` computes watts (uses previous-cycle voltage, one-cycle lag)
  - Regen clamped to 0 (negative current ignored)
  - Encoded: `constrain(watts / 50.0f, 0.0f, 255.0f)` → `telemetry.foil_power`
- Added `DEBUG_VESC` serial output: `V= I= W= encoded=`

## Commits

| Task | Commit | Message |
|------|--------|---------|
| 1 | f8cf316 | feat(01-01): enable VESC_MORE_VALUES and extend TelemetryPacket |
| 2 | 09c0c8e | feat(01-01): add power calculation and foil_power assignment in VESC.ino |

## Key Files

**Modified:**
- `Source/V2_Integration_Rx/BREmote_V2_Rx.h`
- `Source/V2_Integration_Rx/VESC.ino`

## Deviations

None — executed as planned.

## Hardware Verification

**Status:** Deferred — user cannot test with load at this time. Will validate later.
**Note:** Scale factor `/50` vs `/20` remains an open question until hardware testing confirms appropriate range.

## Self-Check: PASSED (with deferred hardware validation)

- [x] `#define VESC_MORE_VALUES` is active
- [x] `foil_power` field exists in TelemetryPacket at index 4
- [x] Power calculation uses correct unit conversion (batCur / 100000.0f)
- [x] Regen clamp present
- [x] Scale factor `/50` applied with constrain
- [x] DEBUG_VESC output added
- [ ] Hardware validation with live VESC (deferred)
