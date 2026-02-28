# BREmote V2 — Extended VESC Telemetry

## What This Is

Extending BREmote V2's VESC communication and telemetry system to transmit richer motor controller data from the Rx to the Tx over LoRa. The existing hand-rolled VESC UART protocol code will be extended (not replaced with a library), and the LoRa telemetry channel will support additional data indices while maintaining backwards compatibility with the existing 6-byte packet format.

## Core Value

The Tx remote must display real-time motor data (power, current, temperatures, speed) from the VESC without breaking compatibility with existing Tx/Rx firmware.

## Requirements

### Validated

<!-- Existing capabilities confirmed from codebase -->

- ✓ Rx queries VESC via `COMM_GET_VALUES_SELECTIVE` for FET temp and battery voltage — existing
- ✓ Rx sends telemetry to Tx in 6-byte LoRa packets cycling through indices (battery%, temp, speed, error) — existing
- ✓ Tx displays telemetry values on LED matrix bargraphs — existing
- ✓ VESC UART protocol implemented with custom `sendToVESC`/`receiveFromVESC` using extracted VESC firmware helpers — existing
- ✓ Configuration persisted via Base64-encoded `confStruct` in SPIFFS — existing

### Active

<!-- Current scope — building toward these -->

- [ ] Rx requests additional VESC values: motor current, battery current, duty cycle, motor temp via extended `COMM_GET_VALUES_SELECTIVE` bitmask
- [ ] Rx calculates power (voltage × battery current) locally
- [ ] Rx calculates speed from ERPM (using pole pairs, gear ratio, wheel diameter config) locally
- [ ] New telemetry indices added to LoRa cycling for: power, motor current, battery current, duty, motor temp, speed
- [ ] Backwards-compatible packet format: same 6-byte structure, new index values that old Tx firmware ignores
- [ ] Tx parses and displays new telemetry indices when present
- [ ] Power is the first new value to implement end-to-end (Rx VESC query → Rx calculation → LoRa → Tx display)
- [ ] Compile-time `#define` flags control which additional VESC values are requested and relayed

### Out of Scope

- Switching to a VESC UART library (VescUart etc.) — current hand-rolled code is lean, efficient, and easy to extend (~3 lines per new value)
- Runtime-configurable telemetry selection — compile-time `#defines` are sufficient for now
- Increasing LoRa packet size beyond 6 bytes — maintaining backwards compatibility with existing remotes
- Watt-hours / amp-hours energy tracking — deferred, not needed for initial telemetry extension
- Fault code detail reporting — deferred to future work

## Context

- The VESC `COMM_GET_VALUES_SELECTIVE` command uses a 32-bit bitmask to request specific fields. The response only contains requested fields in order, making it bandwidth-efficient on UART.
- Current telemetry cycling uses indices 0-3 (battery%, temp, speed, error). New indices (4+) can be added without affecting old firmware — unrecognized indices are simply ignored by older Tx versions.
- Values transmitted as single bytes (0-255) which means scaling/encoding decisions are needed per value type (e.g., power in watts needs a scale factor, current needs a fixed-point encoding).
- The Rx has `VESC_MORE_VALUES` compile flag that already conditionally requests motor current, battery current, and duty — but these values are parsed and discarded (not relayed to Tx).
- Speed calculation requires motor pole pairs, gear ratio, and wheel diameter — these would need to be added to `confStruct` or hardcoded initially.
- UART mux on Rx switches between VESC and GPS on Serial1 — timing matters.

## Constraints

- **Packet compatibility**: 6-byte LoRa telemetry format must be preserved (3 addr + 1 index + 1 value + 1 CRC8)
- **Value resolution**: Single byte (0-255) per telemetry value limits resolution — need appropriate scaling per data type
- **UART timing**: VESC query + response takes ~4.5ms; must fit within 100ms radio cycle alongside GPS (if enabled)
- **Platform**: ESP32 Arduino framework, FreeRTOS tasks, existing task priorities must be respected
- **Compile-time config**: New values controlled by `#define` flags, not runtime config

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Keep hand-rolled VESC code | Lean (~3 lines to add a value), uses efficient selective command, no bloat from unused library features | — Pending |
| Backwards-compatible 6-byte packets | Existing Tx units in the field must not break when Rx sends new indices | — Pending |
| Rx-side speed/power calculation | Keep Tx simple; Rx has VESC config context and can compute derived values | — Pending |
| Compile-time value selection | Simpler than runtime config; reflash is acceptable for this use case | — Pending |
| Power as first new value | User priority — most useful immediate feedback for eFoil riding | — Pending |

---
*Last updated: 2026-02-28 after initialization*
