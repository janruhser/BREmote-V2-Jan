# Roadmap: BREmote V2 — Extended VESC Telemetry

## Overview

Three phases following the power data flow from source to screen: Rx queries VESC and computes watts, both sides extend the telemetry struct and the new index cycles over LoRa, then Tx parses and renders power on the LED matrix. Each phase delivers a verifiable end-point before the next phase begins.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

- [ ] **Phase 1: Rx VESC Extension** - Enable VESC_MORE_VALUES, update VESC_PACK_LEN, compute power (voltage × battery current), encode to single byte
- [ ] **Phase 2: Telemetry Struct + Wire** - Add foil_power field to TelemetryPacket on both Rx and Tx; Rx cycles new index over LoRa
- [ ] **Phase 3: Tx Display** - Tx parses incoming foil_power index and renders watts on the LED matrix display

## Phase Details

### Phase 1: Rx VESC Extension
**Goal**: Rx successfully requests battery current from VESC, computes power in watts, and encodes it as a single byte ready for transmission
**Depends on**: Nothing (first phase)
**Requirements**: VESC-01, VESC-02, VESC-03, TELE-03, TELE-04
**Success Criteria** (what must be TRUE):
  1. With VESC connected and `DEBUG_VESC` enabled, serial output shows battery current and voltage being parsed from the VESC response (not 0 or garbage)
  2. `VESC_PACK_LEN` matches the actual byte count of the VESC response — `receiveFromVESC()` returns success, not timeout
  3. Computed power value (voltage × battery current) prints to serial at a plausible watt figure for the connected load
  4. Encoded byte (`watts / scale_factor`) is in range 0-255 and does not saturate at 255 under normal operating loads
  5. Old Tx firmware (without foil_power field) continues to operate correctly — new telemetry index is silently ignored
**Plans**: 1 plan

Plans:
- [x] 01-01-PLAN.md — Enable VESC_MORE_VALUES, extend TelemetryPacket with foil_power, compute and encode power in watts

### Phase 2: Telemetry Struct + Wire
**Goal**: foil_power field exists in TelemetryPacket on both Rx and Tx, and the Rx is cycling the new index through the LoRa telemetry channel
**Depends on**: Phase 1
**Requirements**: TELE-01, TELE-02
**Success Criteria** (what must be TRUE):
  1. `TelemetryPacket` struct on Rx and Tx are identical in field order and count — `sizeof(TelemetryPacket)` matches on both sides
  2. Rx cycles through telemetry indices including the new foil_power index — captured LoRa packets show the new index byte appearing in sequence alongside existing indices (0-3)
  3. Tx receives the foil_power index packet and writes the value into its local telemetry struct without corrupting adjacent fields (battery %, temp, speed, error remain correct)
**Plans**: 1 plan

Plans:
- [ ] 02-01-PLAN.md — Add foil_power field to Tx TelemetryPacket, synchronizing struct with Rx

### Phase 3: Tx Display
**Goal**: Tx remote shows power in watts when the rider cycles through display modes, with correct scaling and formatting on the 7x10 LED matrix
**Depends on**: Phase 2
**Requirements**: DISP-01, DISP-02
**Success Criteria** (what must be TRUE):
  1. Cycling through display modes on the Tx includes a power mode — the display shows a power reading rather than skipping or showing garbage
  2. The displayed watt value matches the expected load within 10% (validated against VESC Tool or a known load)
  3. Display formatting fits the 7x10 LED matrix — no digit truncation or overflow for typical eFoil power range (0-5000W)
**Plans**: TBD

Plans:
- [ ] 03-01: TBD

## Progress

**Execution Order:**
Phases execute in numeric order: 1 → 2 → 3

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Rx VESC Extension | 1/1 | Plans complete | - |
| 2. Telemetry Struct + Wire | 0/1 | Planned | - |
| 3. Tx Display | 0/? | Not started | - |
