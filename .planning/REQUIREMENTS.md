# Requirements: BREmote V2 — Extended VESC Telemetry

**Defined:** 2026-02-28
**Core Value:** The Tx remote must display real-time power data from the VESC without breaking compatibility with existing firmware

## v1 Requirements

Requirements for initial release. Each maps to roadmap phases.

### VESC Query

- [ ] **VESC-01**: Rx enables `VESC_MORE_VALUES` to request battery current and voltage from VESC via `COMM_GET_VALUES_SELECTIVE`
- [ ] **VESC-02**: Rx computes power in watts from battery voltage × battery current locally
- [ ] **VESC-03**: Rx encodes power to single byte with appropriate scale factor (e.g., watts÷20 for 0–5100W range at 20W resolution)

### Telemetry

- [ ] **TELE-01**: New `foil_power` field appended to `TelemetryPacket` struct on Rx (before `link_quality`)
- [ ] **TELE-02**: Matching `foil_power` field appended to `TelemetryPacket` struct on Tx
- [ ] **TELE-03**: `VESC_PACK_LEN` updated to match new VESC response size when `VESC_MORE_VALUES` is enabled
- [ ] **TELE-04**: Old Tx firmware ignores new telemetry index gracefully (backwards compatible — verified by existing bounds check)

### Display

- [ ] **DISP-01**: Tx displays power value when cycling through display modes
- [ ] **DISP-02**: Power displayed in watts with appropriate formatting for 7x10 LED matrix (e.g., "2.5" for 2500W or "P50" for 50W×scale)

## v2 Requirements

Deferred to future release. Tracked but not in current roadmap.

### Extended Telemetry

- **VESC-04**: Rx relays motor current to Tx via new telemetry index
- **VESC-05**: Rx relays battery current to Tx via new telemetry index
- **VESC-06**: Rx relays duty cycle to Tx via new telemetry index
- **VESC-07**: Rx requests and relays motor temperature via new VESC bitmask bit
- **VESC-08**: Rx calculates speed from ERPM using pole pairs, gear ratio, wheel diameter config

### Configuration

- **CONF-01**: Add `motor_pole_pairs`, `gear_ratio`, `wheel_diameter` to `confStruct` for speed calculation
- **CONF-02**: Bump `SW_VERSION` to handle config struct changes

## Out of Scope

| Feature | Reason |
|---------|--------|
| Switch to VescUart library | Current hand-rolled code is lean, efficient, uses selective query — library adds overhead for no gain |
| Runtime-configurable telemetry | Compile-time `#defines` sufficient; avoids confStruct complexity |
| Larger LoRa packets | Breaks backwards compatibility with existing remotes in the field |
| Watt-hours / amp-hours tracking | Requires session state management; deferred |
| VESC fault detail reporting | Existing error index sufficient for now |

## Traceability

Which phases cover which requirements. Updated during roadmap creation.

| Requirement | Phase | Status |
|-------------|-------|--------|
| VESC-01 | Phase 1 | Pending |
| VESC-02 | Phase 1 | Pending |
| VESC-03 | Phase 1 | Pending |
| TELE-01 | Phase 2 | Pending |
| TELE-02 | Phase 2 | Pending |
| TELE-03 | Phase 1 | Pending |
| TELE-04 | Phase 1 | Pending |
| DISP-01 | Phase 3 | Pending |
| DISP-02 | Phase 3 | Pending |

**Coverage:**
- v1 requirements: 9 total
- Mapped to phases: 9
- Unmapped: 0

---
*Requirements defined: 2026-02-28*
*Last updated: 2026-02-28 — traceability populated after roadmap creation*
