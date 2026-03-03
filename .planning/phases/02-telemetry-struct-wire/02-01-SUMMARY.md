---
phase: 02-telemetry-struct-wire
plan: 01
subsystem: radio
tags: [lora, telemetry, struct, esp32, vesc, foil_power]

# Dependency graph
requires:
  - phase: 01-rx-vesc-extension
    provides: Rx TelemetryPacket extended to 6 fields with foil_power at index 4; Rx already cycles foil_power over LoRa at 10Hz
provides:
  - Tx TelemetryPacket updated to 6 fields matching Rx layout (foil_bat, foil_temp, foil_speed, error_code, foil_power, link_quality)
  - Tx now correctly receives and stores foil_power at index 4; link_quality correctly at index 5
affects: [03-tx-display-power]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Packed struct byte-index alignment: TelemetryPacket uses __attribute__((packed)) so rcvArray[3] byte-offset matches struct field index exactly — sizeof() bounds check adapts automatically when struct grows"

key-files:
  created: []
  modified:
    - Source/V2_Integration_Tx/BREmote_V2_Tx.h

key-decisions:
  - "No changes to Radio.ino required — ptr[rcvArray[3]] pointer-cast pattern and sizeof(TelemetryPacket) bounds check adapt automatically when struct grows from 5 to 6 bytes"
  - "foil_power initialized to 0xFF (not 0) matching not-available sentinel convention used by all other telemetry fields"

patterns-established:
  - "Struct sync discipline: Tx and Rx TelemetryPacket must have identical field order and count; link_quality enforced as last field via comment"

requirements-completed: [TELE-01, TELE-02]

# Metrics
duration: 1min
completed: 2026-03-03
---

# Phase 2 Plan 01: Telemetry Struct Wire Summary

**6-byte TelemetryPacket synchronized between Tx and Rx: foil_power uint8_t added at index 4, enabling Tx to correctly receive watts/50 power data from VESC via LoRa**

## Performance

- **Duration:** ~1 min
- **Started:** 2026-03-03T12:47:30Z
- **Completed:** 2026-03-03T12:48:17Z
- **Tasks:** 1
- **Files modified:** 1

## Accomplishments

- Added `uint8_t foil_power = 0xFF` at index 4 in Tx TelemetryPacket, between `error_code` and `link_quality`
- Struct now 6 fields matching Rx layout exactly: foil_bat(0), foil_temp(1), foil_speed(2), error_code(3), foil_power(4), link_quality(5)
- link_quality correctly moves from index 4 to index 5 — no data corruption on receive
- Zero other files changed; Radio.ino waitForTelemetry() adapts automatically via `sizeof(TelemetryPacket)` bounds check

## Task Commits

Each task was committed atomically:

1. **Task 1: Add foil_power field to Tx TelemetryPacket** - `5dd7742` (feat)

**Plan metadata:** _(docs commit follows)_

## Files Created/Modified

- `Source/V2_Integration_Tx/BREmote_V2_Tx.h` - Added foil_power field at index 4 with 0xFF init and scale-factor comment; added index comments to all fields

## Decisions Made

- No changes to Radio.ino: the existing `ptr[rcvArray[3]] = rcvArray[4]` pattern writes received bytes directly into the struct by byte offset. When sizeof grows from 5 to 6, index 5 (link_quality) passes the `rcvArray[3] < sizeof(TelemetryPacket)` bounds check automatically. Zero Radio.ino changes needed.
- foil_power initialized to 0xFF matching all other telemetry field convention (0xFF = not available sentinel), not 0 which would falsely indicate 0W power.

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None. Pre-change verification confirmed no hard-coded byte offset references (grep for `ptr[4]`, `ptr[5]`, `telemetry + 4`, `telemetry + 5` returned no results).

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Tx now correctly receives foil_power at index 4 and link_quality at index 5
- Phase 3 (Tx display) can access `telemetry.foil_power` directly; decode: `telemetry.foil_power * 50` = watts; guard: `telemetry.foil_power != 0xFF`
- No hardware validation blockers for this struct change — it is a pure layout fix with no logic changes

---
*Phase: 02-telemetry-struct-wire*
*Completed: 2026-03-03*

## Self-Check: PASSED

- FOUND: Source/V2_Integration_Tx/BREmote_V2_Tx.h
- FOUND: commit 5dd7742
- FOUND: .planning/phases/02-telemetry-struct-wire/02-01-SUMMARY.md
