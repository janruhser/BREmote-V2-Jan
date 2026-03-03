# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-02-28)

**Core value:** Tx remote displays real-time power data from the VESC without breaking compatibility with existing firmware
**Current focus:** Phase 2 — Telemetry Struct Wire

## Current Position

Phase: 2 of 3 (Telemetry Struct Wire)
Plan: 1 of 1 in current phase
Status: Plan 02-01 complete
Last activity: 2026-03-03 — Plan 02-01 executed (foil_power added to Tx TelemetryPacket)

Progress: [██████░░░░] 66%

## Performance Metrics

**Velocity:**
- Total plans completed: 0
- Average duration: -
- Total execution time: -

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| - | - | - | - |

**Recent Trend:**
- Last 5 plans: -
- Trend: -

*Updated after each plan completion*

## Accumulated Context

### Decisions

Decisions are logged in PROJECT.md Key Decisions table.
Recent decisions affecting current work:

- Keep hand-rolled VESC code (lean, ~3 lines per new value, no library bloat)
- Backwards-compatible 6-byte LoRa packets (existing Tx units in field must not break)
- Rx-side power calculation (Rx has VESC config context; keeps Tx simple)
- Compile-time value selection via `#define` (reflash acceptable; avoids confStruct complexity)
- Power as first new value (highest community demand; derived from already-parsed fields)
- No Radio.ino changes needed: ptr[rcvArray[3]] pointer-cast pattern and sizeof(TelemetryPacket) bounds check adapt automatically when struct grows from 5 to 6 bytes (02-01)
- foil_power initialized to 0xFF (not 0) matching not-available sentinel convention used by all other telemetry fields (02-01)

### Pending Todos

None yet.

### Blockers/Concerns

- Power scale factor not yet validated on hardware: SUMMARY.md recommends `/50` (0-12,750W range) vs `/20` (0-5,100W). Resolve during Phase 1 testing with actual hardware.
- `VESC_MORE_VALUES` currently not enabled in production firmware — Phase 1 activates an untested code path. Verify with `DEBUG_VESC` before flashing Tx.
- ERPM bitmask bit position discrepancy (bit 5 vs bit 7) between research files — does not affect v1 scope but must be resolved before any v2 speed work.

## Session Continuity

Last session: 2026-03-03
Stopped at: Completed 02-01-PLAN.md (foil_power added to Tx TelemetryPacket, struct synced with Rx)
Resume file: None
