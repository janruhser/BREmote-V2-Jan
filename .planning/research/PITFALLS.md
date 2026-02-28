# Domain Pitfalls: VESC Telemetry Extension

**Domain:** Embedded wireless remote — VESC UART + LoRa telemetry pipeline on ESP32
**Researched:** 2026-02-28
**Confidence:** HIGH (code-derived) / MEDIUM (verified against external sources)

---

## Critical Pitfalls

Mistakes that cause silent data corruption, protocol breaks, or rewrites.

---

### Pitfall 1: VESC_MORE_VALUES Changes VESC_PACK_LEN — Parse Order Must Match Bitmask Exactly

**What goes wrong:** `VESC_PACK_LEN` is `#defined` to 9 (FET_TEMP + BatVolt only) or 19 (+ MotCurrent + BatCurrent + Duty). The `receiveFromVESC` return value is compared against `VESC_PACK_LEN` as a gate. If the bitmask in `vesc_command[4]` is extended to add a new field (e.g., MotorTemp at bit 1, ERPM at bit 5) but `VESC_PACK_LEN` is not updated to match the new response length, the gate condition `receiveFromVESC() == VESC_PACK_LEN` silently fails — `getValuesSelective` returns 0 and every VESC query looks like a communication failure. No error is shown; `telemetry.foil_bat` and `telemetry.foil_temp` stay at `0xFF` indefinitely.

Separately, the response fields are packed in bitmask bit-order with no gaps. The `buffer_get_*` parse calls must appear in the exact same order as bits in the bitmask, lowest bit first within each byte. Inserting a new requested field in the bitmask without adding the corresponding `buffer_get_*` call in the parser — or adding the call in the wrong position — corrupts all subsequent field reads silently, since the parser advances a shared index (`cnt`).

**Why it happens:** `VESC_PACK_LEN` is a hardcoded constant, not computed from the bitmask. Adding a bit to the bitmask without updating `VESC_PACK_LEN` is easy to miss. The response payload carries no field labels — it is a raw packed byte stream.

**Consequences:** All VESC telemetry goes dark (0xFF sentinel). `last_uart_packet` is never updated. After 20 seconds, the display shows "no VESC" state. No compile-time error, no assert, no log entry (unless `DEBUG_VESC` is defined).

**Prevention:**
- Every time a bit is added to the bitmask, immediately update `VESC_PACK_LEN` with the new expected byte count. Use a comment showing the calculation: `// FET_TEMP(2) + MotTemp(2) + MotCurrent(4) + BatCurrent(4) + Duty(2) + BatVolt(2) + cmd(1) + mask(4) = 21`.
- Add a `static_assert` or runtime check comparing the received length to the expected length and logging when they differ (enable `DEBUG_VESC` during development).
- Parse fields in bitmask bit order. Add a comment above each `buffer_get_*` call noting which bitmask bit it corresponds to.

**Detection:** Enable `DEBUG_VESC` — the full raw packet hex is printed. Compare actual byte count to `VESC_PACK_LEN`. If counts diverge, the gate is failing.

**Phase:** Phase 1 (initial VESC extension — adding MotorTemp or ERPM to the query).

---

### Pitfall 2: Struct Field Insertion Before Existing Indices Breaks the Wire Protocol

**What goes wrong:** The telemetry cycling mechanism treats `TelemetryPacket` as a raw byte array: `ptr[telemetry_index]` on Rx transmits, and `ptr[rcvArray[3]]` on Tx receives. The index value sent over LoRa is literally the byte offset in the struct. Index 0 = `foil_bat`, index 1 = `foil_temp`, index 2 = `foil_speed`, index 3 = `error_code`. Any field inserted before an existing field shifts all subsequent byte offsets. An old Tx receiving index 3 from a new Rx will write the value into `error_code` even if the Rx is now sending something else at that position.

This also breaks mixed-firmware deployments in the field: an Rx updated to new firmware paired with a Tx still on old firmware will write wrong data into the Tx's struct silently.

**Why it happens:** The struct layout encodes the protocol. Developers adding fields often insert them "logically" (e.g., next to `foil_temp`) rather than appending. The `link_quality` field carries an explicit source comment "This must be the last entry" — this constraint applies to all new additions but is easy to overlook.

**Consequences:** Silent wrong-data corruption — the Tx displays a garbage value for existing telemetry fields with no error indication. Regression only detectable by testing mixed firmware versions.

**Prevention:**
- All new fields are appended after `error_code` (index 3) and before `link_quality`. Never insert before existing fields. Treat the struct layout as an append-only protocol contract.
- Add a static comment block at the top of `TelemetryPacket` listing indices as a protocol table, updated with every addition.
- The backwards-compatibility test (Step 3 in the recommended build sequence) is mandatory before any Tx deployment.

**Detection:** Compare `sizeof(TelemetryPacket)` before and after the change on the Tx side. If it grew but the base indices shifted, older Tx firmware will display wrong values. Test with serial `?printpackets` to observe index values in live traffic.

**Phase:** Every phase that adds new telemetry fields.

---

### Pitfall 3: VESC_MORE_VALUES Compile Flag Must Be Consistent — One Flag Controls Two Independent Things

**What goes wrong:** `VESC_MORE_VALUES` in `BREmote_V2_Rx.h` simultaneously controls (a) the bitmask sent to the VESC and (b) the expected response length via `VESC_PACK_LEN`. It also conditionally gates the `buffer_get_*` parse calls. If the flag is defined but then new values are added outside the `#ifdef` block — or if individual values need to be conditionally enabled separately — the flag/parse/length coupling breaks.

A related mistake: adding a new value (e.g., ERPM for speed) that requires a different bitmask byte than the existing ones (`byte 4` vs `byte 3`) and updating the wrong byte in `vesc_command[]`.

**Why it happens:** The bitmask is split across `vesc_command[3]` (byte 3, for BatVolt) and `vesc_command[4]` (byte 4, for FET_TEMP/MotCurrent/BatCurrent/Duty). The split is documented in a comment referencing VESC firmware `commands.c` line 377 but is non-obvious. ERPM is at bit 5 of byte 4; Motor temp is at bit 1 of byte 4. Both are in the same byte as the existing flags, but a developer consulting the VESC datasheet independently might confuse byte numbering (LSB-first vs MSB-first).

**Consequences:** Wrong bitmask sends incorrect field requests. VESC returns a response of unexpected length. Gate fails or wrong fields are parsed. All telemetry silently zeros.

**Prevention:**
- Document the bitmask byte layout explicitly in a comment block at the top of `getValuesSelective`, listing each bit position, field name, VESC data type, and byte contribution to response length.
- If individual values need independent `#define` control, use separate flags (`VESC_WANT_ERPM`, `VESC_WANT_MOTOR_TEMP`) rather than extending the single `VESC_MORE_VALUES` flag. Recompute `VESC_PACK_LEN` from the union of active flags.

**Detection:** Inspect `vesc_command[3]` and `vesc_command[4]` with `DEBUG_VESC` enabled and verify against the VESC firmware source `commands.c`.

**Phase:** Phase 1 (VESC query extension). Phase 2 (speed — requires ERPM bit 5).

---

### Pitfall 4: Single-Byte Encoding — Wrong Scale Factor Produces Silent Saturation or Loss of Useful Range

**What goes wrong:** All telemetry values are encoded as `uint8_t` (0-255). The scale factor converts the VESC's raw value to a byte and is decoded on the Tx side. If the scale factor is chosen without considering the actual operational range, values saturate at 255 continuously (the operator sees max always), or the entire useful range is compressed into a few counts (low resolution). Worse, if Rx and Tx use different assumed scale factors — which happens if the scale is baked into display code rather than being consistent — decoded values are systematically wrong with no error signal.

Specific risk for power: VESC `batCur` is `int32` in units of milliAmps × 100. A 10A battery current = 1,000,000 (one million). Dividing by 100,000 gives amps. Then `volts × amps = watts`. If the `/ 100000.0f` divisor is off by a factor of 10 (e.g., `/ 10000.0f`), computed watts are 10× too high, foil_power saturates at 255 immediately and the display always shows maximum.

Specific risk for motor/battery current: Motor current can be negative (regenerative braking, sign from VESC is negative int32). If encoded as `uint8_t` via direct cast without `abs()` or offset encoding, negative values wrap to large positive numbers. The Tx displays 200A when actual current is -50A.

**Why it happens:** Scale factors are embedded in `VESC.ino` assignment expressions without explicit documentation. Rx and Tx are separate codebases — the decoding multiplier in `Display.ino` must match the encoding divisor in `VESC.ino` exactly. These values are not co-located in source, making drift likely across edits.

**Consequences:** All display values systematically wrong. No error flag raised. User trusts incorrect data. For safety-relevant values (temperature, battery current), this could mask dangerous conditions.

**Prevention:**
- Define encoding constants once in a shared header or as comments co-located in both files:
  ```c
  // SCALE: foil_power = watts / 20   → display: watts = foil_power * 20   (range 0-5100W, 20W resolution)
  // SCALE: foil_motor_current = abs(mA×100) / 100000  → display: A = foil_motor_current  (1A resolution)
  ```
- Use absolute value for current fields in this milestone. Document the convention in both headers.
- Add a compile-time or runtime sanity check: if `fbatVolt > 0 && batCur > 0 && foil_power == 255`, emit a debug warning that power may be saturating.
- Test encoding with known VESC test vectors: inject a synthetic batCur and verify foil_power equals the expected byte value.

**Detection:** Serial `?state json` does not currently show computed telemetry. Add a temporary `DEBUG_VESC` print of encoded values during development. Verify against VESC Tool readings for the same moment.

**Phase:** Phase 1 (power encoding). Phase 2 (current/duty encoding). Phase 3 (speed encoding).

---

### Pitfall 5: `confStruct` Addition Without SW_VERSION Bump Silently Loads Defaults or Corrupts New Fields

**What goes wrong:** `confStruct` is stored as a raw Base64-encoded binary blob in SPIFFS. When the struct is extended (e.g., adding `pole_pairs`, `gear_ratio`, `wheel_diameter` for speed calculation), the stored blob is the old size. On reboot, `readConfFromSPIFFS` decodes the blob into the current `confStruct` via memcpy. If the new fields happen to land at offsets within the old blob size, they receive garbage byte values from flash. If they land beyond the blob, they are uninitialized (whatever was in the heap allocation).

The existing mitigation is `SW_VERSION`: if the decoded struct's `version` field does not match `SW_VERSION`, defaults are loaded. But this only works if `SW_VERSION` is incremented with every struct change. If a developer adds fields and forgets to bump `SW_VERSION`, the version check passes, new fields are garbage, and derived calculations (speed, power) produce wrong values silently.

Additionally, the existing `CONCERNS.md` notes: "Config Versioning Misses Backward Compatibility — Config struct version mismatch triggers full reset with no upgrade path." A `SW_VERSION` bump wipes all user calibration (throttle expo, deadzone, pairing). This is a UX cliff.

**Why it happens:** `SW_VERSION` is defined in the header file and must be manually incremented. There is no enforcement. Base64 round-trip of a binary struct is fragile to any layout change (padding, alignment, field order). ESP32 is little-endian but struct packing (`__attribute__((packed))`) is inconsistently applied — `confStruct` lacks `__attribute__((packed))`, meaning adding fields with different alignment can shift existing fields.

**Consequences:** If `SW_VERSION` is NOT bumped: garbage values in new fields, wrong calculations, device appears to work until an edge-case calculation saturates. If `SW_VERSION` IS bumped: all users lose calibration on update. Both outcomes are bad.

**Prevention:**
- Bump `SW_VERSION` on every `confStruct` change. Make this a checklist item in the commit message template.
- New speed/calibration fields (`pole_pairs`, `gear_ratio`, `wheel_diameter`) should have valid defaults that produce a safe "no speed display" state (e.g., `pole_pairs = 0` → speed calculation skipped, `foil_speed` stays `0xFF`).
- Apply `__attribute__((packed))` to `confStruct` or use `static_assert(sizeof(confStruct) == EXPECTED_SIZE)` to catch unexpected layout changes at compile time.
- Long-term: migrate to key-value config storage, but this is out of scope for this milestone.

**Detection:** After adding struct fields, compare `sizeof(confStruct)` before and after. Use `?keys` and `?get` to verify new fields load correctly after a save/reboot cycle.

**Phase:** Phase 2 or 3 (if speed calculation parameters are added to `confStruct`).

---

## Moderate Pitfalls

Mistakes that cause incorrect behavior detectable through testing.

---

### Pitfall 6: `Serial1.flush()` in `getVescLoop` Discards Both TX and RX Buffers on ESP32

**What goes wrong:** The ESP32 Arduino framework's `HardwareSerial::flush()` does not behave the same as standard Arduino: it flushes the TX output buffer on standard Arduino but can also flush the RX input buffer on ESP32 depending on the framework version. Specifically, `uartFlush()` (the underlying ESP-IDF call) has been documented to flush both directions erroneously in some versions of the arduino-esp32 core.

In `getVescLoop()`, `Serial1.flush()` is called before `getValuesSelective()` to discard stale data. If this flush discards bytes that arrived as a response to a previous command that hadn't been read yet (e.g., from a GPS sentence during mux switch), it could corrupt the GPS read on the next mux cycle. More critically, if the VESC responds quickly and some response bytes arrive before the code reaches `receiveFromVESC`, those bytes could be flushed, causing the 200ms timeout to expire with a partial read, returning 0 and declaring a comms failure.

**Why it happens:** UART mux switches between VESC (mux 0) and GPS (mux 1) on Serial1. The 10ms `vTaskDelay` after `setUartMux(0)` is intended to let the mux settle, but VESC may begin responding as soon as the command arrives and bytes may reach Serial1 RX during that delay window.

**Prevention:**
- After `setUartMux(0)`, call `while(Serial1.available()) Serial1.read();` instead of `Serial1.flush()` to drain only what's in the buffer without risking TX flush.
- Verify ESP32 Arduino core version behavior for `flush()`. The github issue [#3095](https://github.com/espressif/arduino-esp32/issues/3095) specifically tracks this; behavior has changed across versions.
- Increase the mux settle delay or verify with a logic analyzer that VESC is not responding during the settle window.

**Detection:** Add `DEBUG_VESC` and observe whether `receiveFromVESC` times out immediately (cnt == 0 when 200ms expires) — this indicates bytes were flushed before they could be read.

**Phase:** Phase 1.

---

### Pitfall 7: The VESC Response Payload Starts with a 4-Byte Echo of the Request Mask — Parsing Must Skip It

**What goes wrong:** `COMM_GET_VALUES_SELECTIVE` echoes the 32-bit request mask at the beginning of the response payload. The current `getValuesSelective` skips this with `int32_t cnt = 5; // Don't care about the Mask`. The `5` skips: 1 byte command ID + 4 bytes mask = 5 bytes. This is correct for the current implementation.

If a developer extends the parser and mistakenly believes the mask is not echoed (it's not in the request payload, only in the response), they may set `cnt = 1` (skip command ID only) and misparse all fields. Every `buffer_get_*` call would read 4 bytes behind the actual field start, yielding garbage values that are plausible-looking numbers (not obviously wrong).

This is confirmed by the VescUartLite library source which explicitly documents `ind = 4; // Skip the mask` and SolidGeek's VescUart which has the selective case commented out as never implemented.

**Why it happens:** The VESC protocol documentation is sparse. The echo-mask behavior is only discoverable from the VESC firmware source (`commands.c`) or by sniffing the actual UART traffic. Most third-party libraries skip or comment out the selective implementation.

**Prevention:**
- The existing `cnt = 5` offset is correct and well-commented. Preserve it in any parser extension. Add a comment: `// Skip 1-byte command ID + 4-byte echoed mask = 5 bytes total`.
- When adding a new field: insert the `buffer_get_*` call after existing parse calls, never before. The parse order equals the bitmask bit order (bit 0 first in byte 4, then bit 1, ..., then byte 3 fields).

**Detection:** If any field reads a value that seems wrong relative to VESC Tool, print `cnt` at each parse step and compare expected byte offsets against a hex dump from `DEBUG_VESC`.

**Phase:** Phase 1.

---

### Pitfall 8: FreeRTOS Timing — VESC Query Takes ~4.5ms and Must Not Exceed 100ms Radio Cycle Budget

**What goes wrong:** The Rx's LoRa telemetry response is triggered by `triggeredReceive`, which fires immediately on each received Tx control packet (semaphore from ISR). `getVescLoop` is called from the Rx `loop()` and must complete before the next control packet arrives (100ms interval). The current single-field query (FET_TEMP + BatVolt) takes ~4.5ms as documented in the source comment.

Extending the bitmask to include more fields increases the response payload size and therefore the serial receive time. At 115200 baud, each byte takes ~87 microseconds. The current 9-byte payload = ~780µs wire time. A 19-byte payload (with VESC_MORE_VALUES) = ~1650µs. Adding ERPM (4 bytes), MotorTemp (2 bytes) = 25-byte payload ≈ ~2200µs. Plus VESC internal processing time and serial buffer fill latency, total could approach 6-8ms.

At 10Hz (100ms cycle), even 8ms UART time is fine. The real risk is if `getVescLoop` also contends with GPS Serial1 mux time. If GPS is enabled (`gps_en = 1`), Serial1 is multiplexed — the GPS task also reads Serial1. If both `getVescLoop` and GPS parsing are in the same `loop()` execution, and GPS NMEA sentences arrive during the VESC read window (or vice versa), the mux may switch mid-packet.

**Prevention:**
- Measure actual `getVescLoop` wall-clock time with `millis()` before and after the call during development. Verify it stays well under 50ms to leave margin for GPS.
- If GPS is enabled, ensure mux switching (`setUartMux`) is coordinated — VESC queries when mux is at 0, GPS reads when mux is at 1. Verify the ordering in the Rx `loop()`.
- The 200ms VESC read timeout in `receiveFromVESC` is the worst case. If VESC doesn't respond, the loop blocks for 200ms — enough to miss 2 Tx control packets. After the extension, verify this timeout still fails fast (VESC not connected) vs slow (partial response).

**Detection:** Add timing instrumentation around `getVescLoop()` in `loop()`. Print elapsed time via serial in debug builds. Use `?printtasks` to check FreeRTOS stack and CPU usage.

**Phase:** Phase 1 (initial extension). Monitor throughout.

---

### Pitfall 9: ERPM-to-Speed Calculation Requires Pole Pairs (Not Pole Count) — Off-By-Factor-Of-2

**What goes wrong:** The VESC reports ERPM (electrical RPM). Converting to mechanical RPM requires dividing by the number of pole *pairs*, not the number of poles. Common eFoil motors have 14 magnets = 7 pole pairs. Using `poles = 14` instead of `pole_pairs = 7` produces a speed exactly half the actual speed. Users will see the remote report 20 km/h while going 40 km/h. This is a persistent, silent, plausible-looking error.

The VESC forum documents this confusion explicitly: "ERPM = pole_pairs × mechanical_RPM" — the community frequently sees reports of speeds being exactly half or double expected.

**Why it happens:** Motor specifications often list "number of poles" (magnets), not "pole pairs." The VESC firmware uses pole pairs internally. The config parameter in VESC Tool is labeled "Motor Poles" but expects the number of poles (which it halves internally). If `confStruct` stores `pole_pairs` as a raw value entered by the user, they may enter 14 (poles) instead of 7 (pole pairs).

**Consequences:** Speed display persistently wrong by 2×. No error detection. Rider calibrates riding instincts to wrong display, which is worse than no display.

**Prevention:**
- Name the config field unambiguously: `motor_pole_pairs` (not `motor_poles`, not `num_poles`).
- Add a validation check: if `motor_pole_pairs` is even and greater than 10, emit a debug warning that this may be pole count (not pairs).
- Document the calculation formula with units in the code comment: `km/h = (erpm / motor_pole_pairs / gear_ratio) * (wheel_circumference_m * 60 / 1000)`.
- Include a sanity range: expected eFoil motor pole pairs are typically 4-10. Values outside 2-20 should warn.

**Detection:** Cross-check computed speed against GPS speed (if GPS is enabled) or a phone GPS during a test run. A 2× discrepancy points to pole count vs pole pairs confusion.

**Phase:** Phase 2 (speed calculation).

---

### Pitfall 10: Telemetry Cycle Time Grows Proportionally With New Indices — Slowest-Updating Values May Feel Stale

**What goes wrong:** At 9 telemetry indices (existing 4 + link_quality + 4 new), one full rotation takes 9 × 100ms = 900ms. Battery percentage updates once per 900ms. Power updates once per 900ms. For an eFoil rider hitting a ramp, power can swing from 500W to 4000W in under a second — the display may lag 900ms behind reality.

This is not a bug (the architecture document notes this is acceptable), but it becomes a usability pitfall if developers add more indices (motor temp, battery current, duty, motor current, speed = 5 more = 14 total = 1.4 second cycle) without considering the display latency for rapidly-changing values.

**Why it happens:** Each new field added to `TelemetryPacket` automatically extends the cycle with no code change required. The automation that makes extension easy also hides the latency cost.

**Prevention:**
- Before adding each new index, evaluate its update rate requirement. For rapidly-changing values (power, duty), prefer to place them early in the struct (low index number) so they appear first in each cycle when the remote is first connected.
- The practical limit for "feels live" is ~500ms (5 indices). Beyond 10 indices, consider whether all values need to be in the main telemetry cycle or if some (motor temp, trip data) can be lower frequency.
- Document cycle time explicitly: add a comment to `TelemetryPacket` with `// Cycle time = sizeof(TelemetryPacket) × 100ms`.

**Detection:** Time display updates with a stopwatch while monitoring serial output. Verify battery % refreshes within the expected window.

**Phase:** Every phase adding indices. Worst case during Phase 1 (adding 4+ indices at once).

---

## Minor Pitfalls

---

### Pitfall 11: `VESC_PACK_LEN` Is Defined With `#define`, Not `const` — Cannot Use `static_assert`

**What goes wrong:** `VESC_PACK_LEN` is a preprocessor `#define` inside the conditional block in `BREmote_V2_Rx.h`. This means it cannot be used with `static_assert` to catch size mismatches at compile time without careful macro ordering. If a developer tries to add a `static_assert(VESC_PACK_LEN == expected, "...")`, it may not fire correctly if `VESC_MORE_VALUES` is not defined in the translation unit where the assert is written.

**Prevention:** Convert `VESC_PACK_LEN` to a `constexpr` or compute it from a sum of sizeof() constants for each requested field type. This enables `static_assert` enforcement.

**Phase:** Phase 1 (low priority — not a blocker).

---

### Pitfall 12: Motor Temperature Bit Position Must Be Requested Before MotorCurrent in Bitmask — Insertion Order Matters

**What goes wrong:** Motor temperature is bit 1 of vesc_command[4]. Motor current is bit 2. If both are requested, motor temp bytes precede motor current bytes in the response. The current code requests FET_TEMP (bit 0), MotCurrent (bit 2), BatCurrent (bit 3), Duty (bit 6). If motor temperature is added later (bit 1), its `buffer_get_int16` must be inserted between `fetTemp` and `motCur` parse calls — even though visually it might seem to belong "at the end."

**Prevention:** Maintain a comment table in `getValuesSelective` listing all requested bits and their parse order explicitly. Cross-reference against VESC firmware `commands.c` before adding any new bit.

**Phase:** Phase 2 (motor temp).

---

### Pitfall 13: `volatile` on `telemetry` Struct Does Not Guarantee Atomic Multi-Byte Reads

**What goes wrong:** The `telemetry` struct is `volatile` and accessed from both `getVescLoop` (via `VESC.ino` assignments) and `triggeredReceive` (via `Radio.ino` byte reads). `volatile` only prevents compiler optimization — it does not provide mutual exclusion. On an ESP32 dual-core, `triggeredReceive` on Core 0 and `loop()` on Core 1 could simultaneously access the struct, with the radio task reading a partially-written value.

In practice, each telemetry field is a single `uint8_t` — byte writes and reads are atomic on ARM Cortex-M. So the current code is safe for `uint8_t` fields. This would become a real bug only if multi-byte telemetry fields were introduced (e.g., `uint16_t` for higher-precision power). The current architecture (all `uint8_t`, packed struct) is intentionally safe by design.

**Prevention:** Keep all `TelemetryPacket` fields as `uint8_t`. If higher precision is ever needed, use a semaphore or double-buffer around the write in `getVescLoop`. Do not silently introduce `uint16_t` fields.

**Phase:** Any phase adding fields (ongoing constraint).

---

## Phase-Specific Warnings

| Phase Topic | Likely Pitfall | Mitigation |
|-------------|---------------|------------|
| Enable VESC_MORE_VALUES, parse extra fields | VESC_PACK_LEN not updated → all VESC data silently fails | Update VESC_PACK_LEN first, verify with DEBUG_VESC |
| Add power calculation from batCur × voltage | batCur unit is mA×100, not amps → scale error 100,000× | Divide by 100,000.0f; verify with known current reading |
| Encode power as uint8_t | Saturation at 255 = 5100W → eFoil motors hitting max display immediately | Choose scale factor after measuring actual peak watts |
| Add new fields to TelemetryPacket | Insertion before error_code (index 3) breaks wire protocol | Append only; run backwards-compat test before Tx flash |
| Add speed parameters to confStruct | SW_VERSION not bumped → garbage values in new fields | Bump SW_VERSION; provide valid defaults; test save/reload |
| Speed from ERPM | Pole count vs pole pairs confusion → 2× speed error | Use `motor_pole_pairs` name; document formula with units |
| Motor temp addition to bitmask | Bit 1 inserted before bit 2 in parse order → batCur reads wrong field | Maintain parse order table; re-verify all field values |
| Increasing number of telemetry indices | Cycle time > 1s for fastest-changing values | Place high-rate values at low indices; document cycle time |

---

## Sources

- `Source/V2_Integration_Rx/VESC.ino` — Bitmask, VESC_PACK_LEN, parse loop, flush behavior (HIGH confidence — primary source)
- `Source/V2_Integration_Rx/BREmote_V2_Rx.h` — VESC_PACK_LEN conditional defines, TelemetryPacket layout, confStruct (HIGH confidence — primary source)
- `.planning/research/ARCHITECTURE.md` — Telemetry cycling mechanism, extension seam, struct byte-offset protocol (HIGH confidence — code-derived)
- `.planning/codebase/CONCERNS.md` — Config versioning risk, volatile race conditions, VESC CRC silent failure (HIGH confidence — code audit)
- [VESC bldc Issue #63: COMM_GET_VALUES packet length 64-byte truncation](https://github.com/vedderb/bldc/issues/63) — 64-byte serial buffer limit, COMM_GET_VALUES_SELECTIVE motivation (HIGH confidence — official VESC project)
- [SolidGeek VescUart: COMM_GET_VALUES_SELECTIVE commented out](https://github.com/SolidGeek/VescUart/blob/master/src/VescUart.cpp) — Confirms mask echo in response payload; selective case unimplemented in major library (MEDIUM confidence — community library)
- [VESC Project: ERPM to RPM confusion](https://vesc-project.com/node/2932) — Pole count vs pole pairs community documentation (MEDIUM confidence — community forum)
- [ESP32 arduino-esp32 Issue #3095: flush() flushes input](https://github.com/espressif/arduino-esp32/issues/3095) — Serial1.flush() behavior difference from standard Arduino (MEDIUM confidence — framework issue tracker)
- [ESP32 arduino-esp32 Issue #4206: UART flush timing 27ms](https://github.com/espressif/arduino-esp32/issues/4206) — flush() latency behavior (MEDIUM confidence — framework issue tracker)
- [ESP32 Forum: volatile vs mutex for shared data](https://www.esp32.com/viewtopic.php?t=20164) — volatile not sufficient for multi-core shared data (MEDIUM confidence — community forum)
- [thelinuxcode.com: Packed struct alignment pitfalls](https://thelinuxcode.com/how-i-pack-structs-in-c-without-getting-burned-by-alignment-abi-and-performance/) — confStruct alignment and binary serialization risks (MEDIUM confidence — technical article)
