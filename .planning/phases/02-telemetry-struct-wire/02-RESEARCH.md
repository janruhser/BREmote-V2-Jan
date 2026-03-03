# Phase 2: Telemetry Struct + Wire - Research

**Researched:** 2026-03-03
**Domain:** Embedded C struct extension + LoRa telemetry cycling — synchronizing TelemetryPacket across two ESP32 firmware units (Rx and Tx)
**Confidence:** HIGH

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| TELE-01 | New `foil_power` field appended to `TelemetryPacket` struct on Rx (before `link_quality`) | Already done in Phase 1 — Rx struct has foil_power at index 4, link_quality at index 5. No Rx struct change needed in Phase 2. |
| TELE-02 | Matching `foil_power` field appended to `TelemetryPacket` struct on Tx | Tx struct currently has 5 fields (indices 0-4: foil_bat, foil_temp, foil_speed, error_code, link_quality). foil_power must be inserted at index 4 (between error_code and link_quality), shifting link_quality to index 5. This is the sole code change in Phase 2. |
</phase_requirements>

---

## Summary

Phase 2 has a deceptively small implementation surface: one struct change in one file on the Tx side. Phase 1 already completed the Rx struct extension (foil_power at index 4, link_quality at index 5, sizeof==6). The Rx also already cycles all telemetry indices automatically via `sizeof(TelemetryPacket)` — so the Rx sends index 4 (foil_power) every sixth packet without any code change. The LoRa wire protocol is already carrying the foil_power index. The only thing missing is the Tx struct recognizing index 4 as foil_power rather than silently discarding it (because old Tx sizeof==5 accepts index 4 into link_quality).

The key structural fact: the Tx `waitForTelemetry()` function writes `ptr[rcvArray[3]] = rcvArray[4]` using the raw byte offset of the received index. This means the Tx TelemetryPacket struct layout IS the wire protocol on the receive side. When Tx sizeof grows from 5 to 6 (adding foil_power at index 4), index 4 packets from Rx write into `telemetry.foil_power` correctly, and index 5 packets write into `telemetry.link_quality` correctly. No changes needed to Radio.ino, Display.ino, or any other Tx file in Phase 2.

The one subtlety worth calling out explicitly: the Tx currently has a `link_quality` display mode that reads from `telemetry.link_quality`. After Phase 2, `telemetry.link_quality` moves from byte offset 4 to byte offset 5. If any Tx code references `link_quality` by hard-coded byte offset (not by field name), it will break. Code review confirms this is not the case — all references use the named field `telemetry.link_quality`.

**Primary recommendation:** Add `uint8_t foil_power = 0xFF;` to `TelemetryPacket` in `Source/V2_Integration_Tx/BREmote_V2_Tx.h` at index 4 (before `link_quality`), matching the Rx struct exactly. No other files need to change in Phase 2.

---

## Standard Stack

### Core

| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| `BREmote_V2_Tx.h` | Project-local | Tx-side `TelemetryPacket` struct definition | Central header; single source of truth for Tx types |
| `BREmote_V2_Rx.h` | Project-local | Rx-side `TelemetryPacket` struct definition (already updated in Phase 1) | Central header; struct layout drives LoRa cycling |
| ESP32 Arduino framework | Core bundled | `sizeof()`, `uint8_t`, struct layout with `__attribute__((packed))` | Existing; no change |

### Supporting

| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| `debug_rx` / serial output | Project-local | Verify packet reception on Tx in USB mode | Enable during verification; not needed for implementation |

### Alternatives Considered

| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Struct field (named) | Hard-coded byte offset in Radio.ino | Named field is correct — the existing pointer-cast pattern `ptr[rcvArray[3]]` is already offset-based; field name is for compile-time type safety and readability only |
| Single shared header (Common/) | Dual independent headers (Rx + Tx) | A shared header would prevent struct divergence. But Rx and Tx are separate Arduino sketches with different compile environments — sharing is not currently architecturally supported and is out of scope per REQUIREMENTS.md |

**Installation:** No new libraries. All work is in existing source files.

---

## Architecture Patterns

### Files to Change (Phase 2 Scope)

```
Source/V2_Integration_Tx/
└── BREmote_V2_Tx.h     # Add foil_power field to TelemetryPacket — ONLY change in Phase 2
```

No changes to Rx files (struct already updated in Phase 1). No changes to Tx Radio.ino, Display.ino, Hall.ino, or any other file.

### Pattern 1: Struct-as-Wire-Protocol on Tx (Named Field Offset Write)

**What:** The Tx `waitForTelemetry()` casts `&telemetry` to `uint8_t*` and writes received data at `ptr[rcvArray[3]]`. The byte layout of the struct IS the wire receive mapping. No separate dispatch table exists.

**Code reference (Tx Radio.ino, line 343-347 — no change needed):**

```c
// Source: Source/V2_Integration_Tx/Radio.ino:waitForTelemetry()
uint8_t* ptr = (uint8_t*)&telemetry;
if (rcvArray[3] < sizeof(TelemetryPacket))  // bounds check — no change needed
{
    ptr[rcvArray[3]] = rcvArray[4];
}
```

When Tx `sizeof(TelemetryPacket)` grows from 5 to 6:
- Received index 4 → writes into `telemetry.foil_power` (new field, correct)
- Received index 5 → writes into `telemetry.link_quality` (moved from index 4, correct)
- The `5 < 6` check passes for index 5 — link_quality will now be correctly written

### Pattern 2: Struct Append / Insert Before Sentinel

**What:** The comment `//This must be the last entry` marks `link_quality` as the sentinel that must remain at the highest index. New fields are always inserted before it.

**Current Tx struct (BEFORE Phase 2):**

```c
// Source: Source/V2_Integration_Tx/BREmote_V2_Tx.h lines 95-103
struct __attribute__((packed)) TelemetryPacket {
    uint8_t foil_bat = 0xFF;    // index 0
    uint8_t foil_temp = 0xFF;   // index 1
    uint8_t foil_speed = 0xFF;  // index 2
    uint8_t error_code = 0;     // index 3
    //This must be the last entry
    uint8_t link_quality = 0;   // index 4 (currently)
} telemetry;
```

**Target Tx struct (AFTER Phase 2):**

```c
// Source: Source/V2_Integration_Tx/BREmote_V2_Tx.h
struct __attribute__((packed)) TelemetryPacket {
    uint8_t foil_bat = 0xFF;    // index 0 — battery % 0-100
    uint8_t foil_temp = 0xFF;   // index 1 — FET temp degC
    uint8_t foil_speed = 0xFF;  // index 2 — speed km/h
    uint8_t error_code = 0;     // index 3 — fault flags
    // Phase 2: SCALE=watts/50  DECODE=foil_power*50  range 0-12750W at 50W resolution
    uint8_t foil_power = 0xFF;  // index 4 — power (watts/50); 0xFF = not available
    //This must be the last entry
    uint8_t link_quality = 0;   // index 5 (was 4)
} telemetry;
```

### Pattern 3: Rx Cycling — Already Complete, No Changes Needed

**What:** The Rx `triggeredReceive()` cycles through `telemetry_index` 0..sizeof(TelemetryPacket)-1. Since Phase 1 set Rx sizeof to 6, the Rx already transmits indices 0,1,2,3,4,5 — including foil_power at index 4 — every six packets. No Radio.ino changes are needed on either side.

**Rx Radio.ino lines 228-236 (confirmed — no change):**

```c
sendArray[3] = telemetry_index;
sendArray[4] = ptr[telemetry_index];
telemetry_index++;
if(telemetry_index >= sizeof(TelemetryPacket))  // wraps at 6 (Rx) — already correct
{
    telemetry_index = 0;
}
```

### Pattern 4: Initialization Value Convention (0xFF = Not Available)

**What:** All telemetry fields except `error_code` and `link_quality` are initialized to `0xFF` as a "not available" sentinel. Phase 2 must follow this convention for `foil_power`.

**Rx precedent (BREmote_V2_Rx.h line 137):**
```c
uint8_t foil_power = 0xFF;  // index 4 — power (watts/50); 0xFF = not available
```

**Tx must match:** `uint8_t foil_power = 0xFF;`

The `0xFF` value means "VESC not connected or power data unavailable." Phase 3 (display) must handle `0xFF` as a no-display case (show `---` or skip) rather than displaying 12,750W.

### Anti-Patterns to Avoid

- **Inserting `foil_power` before `error_code` (at index 3 or earlier):** Shifts `error_code` and downstream fields — breaks all paired Tx/Rx units silently. Always insert at index 4 (between `error_code` and `link_quality`).
- **Omitting `__attribute__((packed))` on the struct:** Already present on both Rx and Tx structs. If it were removed, struct padding could shift byte offsets even though all fields are `uint8_t`. With all-uint8_t structs, packed vs unpacked produces the same layout, but the attribute is there for documentation intent — keep it.
- **Forgetting to update the index comment on `link_quality`:** The comment `// index 4` on `link_quality` must become `// index 5 (was 4)` to prevent future maintainers from using wrong offsets.
- **Any Tx code referencing `telemetry.link_quality` by hard-coded offset:** Already verified — all references use the named field. No risk here, but must confirm during implementation.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Index dispatch on Tx | Switch/case on received index | Existing pointer-cast pattern `ptr[rcvArray[3]]` | Already correct; any dispatch table would duplicate struct layout information and drift |
| Struct size verification | Runtime assert | `static_assert(sizeof(TelemetryPacket) == 6, "...")` in header | Compile-time; catches mismatch immediately |
| Compatibility shim | Version-checking handshake in LoRa protocol | Existing bounds check `rcvArray[3] < sizeof(TelemetryPacket)` | Already handles unknown indices gracefully |

**Key insight:** The entire protocol extension infrastructure already exists. Phase 2 is a single struct field insertion.

---

## Common Pitfalls

### Pitfall 1: Struct Mismatch Between Rx and Tx After Phase 2

**What goes wrong:** If Rx has 6 fields and Tx has 5 (or vice versa), the index assignments diverge. Example: Rx sends index 5 (link_quality) but old Tx sizeof==5 drops it (5 < 5 is FALSE). Link quality on Tx stays stale forever.

**Why it happens:** Phase 1 was Rx-only. If Phase 2 is forgotten or partially executed, Tx struct remains 5 fields and fails to receive link_quality updates.

**How to avoid:** The Phase 2 success criterion checks `sizeof(TelemetryPacket)` matches on both sides. Verify by adding a temporary `Serial.println(sizeof(TelemetryPacket));` startup print on both Rx and Tx in USB debug mode.

**Warning signs:** After Phase 2, link_quality on Tx stays at 0 permanently (not updating). Battery, temp, speed, error continue to update correctly because they are at indices 0-3 (which pass the 4<5 check even on old Tx).

### Pitfall 2: link_quality Display Code Breaks if Referenced by Hardcoded Offset

**What goes wrong:** If any Tx code accesses telemetry byte 4 directly (e.g., `ptr[4]`) to get link_quality, it will now get foil_power after Phase 2.

**How to avoid:** Search for any `ptr[4]` or `&telemetry + 4` patterns in Tx source files before committing. All confirmed uses in the codebase reference `telemetry.link_quality` (named field), so this is a verification step only.

**Confirmed safe:** `waitForTelemetry()` uses `ptr[rcvArray[3]]` — the index comes from the wire, not from a hardcoded offset. `local_link_quality` is computed directly from RSSI/SNR in the Tx (not from the struct). The `telemetry.link_quality` struct field is used by display/bargraph code — which references it by name, not offset.

**Warning signs:** Link quality bargraph shows garbage after Phase 2 while other telemetry fields update correctly.

### Pitfall 3: foil_power Initialized to Wrong Sentinel on Tx

**What goes wrong:** If `foil_power = 0` (instead of `0xFF`), Phase 3 display code sees a valid-looking 0W reading before any VESC data arrives. This could show "0W" at startup rather than "---" or nothing.

**How to avoid:** Initialize `foil_power = 0xFF` to match the Rx convention and all other "not available" fields (`foil_bat`, `foil_temp`, `foil_speed`).

**Warning signs:** Display shows 0W at startup before VESC connects, rather than suppressing the reading.

### Pitfall 4: Tx Displays Incorrect link_quality During Transition

**What goes wrong:** In a mixed-firmware scenario where Rx has Phase 1 code (6-field struct, sends index 5 for link_quality) and Tx has pre-Phase-2 code (5-field struct, sizeof==5), index 5 is silently dropped by Tx (`5 < 5` is FALSE). Tx link_quality stays 0. However, index 4 (foil_power) IS accepted by old Tx into its link_quality slot (4 < 5 is TRUE), polluting the link quality display with power data.

**This is documented expected behavior from Phase 1 verification (cosmetic, not a safety issue).** After Phase 2 completes on Tx, both sides have sizeof==6 and this ambiguity resolves.

**How to avoid:** Flash Tx with Phase 2 firmware promptly after Rx Phase 1 is deployed. During the transition period, expect link_quality on Tx to show garbage values.

### Pitfall 5: Scale Factor Comment Inconsistency

**What goes wrong:** The `foil_power` encoding scale factor (`watts/50`, decode as `foil_power * 50`) must be documented consistently across:
- `BREmote_V2_Rx.h` (Rx struct comment — already has the comment from Phase 1)
- `BREmote_V2_Tx.h` (Tx struct comment — must be added in Phase 2)
- `VESC.ino` (assignment comment — already present from Phase 1)
- Future Phase 3 `Display.ino` (decode comment)

If the Tx struct comment says `/20` but Rx encodes `/50`, Phase 3 implementor will decode with wrong scale factor.

**How to avoid:** Copy the exact comment from Rx to Tx verbatim: `// Phase 2: SCALE=watts/50  DECODE=foil_power*50  range 0-12750W at 50W resolution`

---

## Code Examples

Verified patterns from actual source code:

### Complete Tx Struct Change (BREmote_V2_Tx.h — the only Phase 2 code change)

```c
// Source: Source/V2_Integration_Tx/BREmote_V2_Tx.h lines 94-103
// BEFORE Phase 2:
//Telemetry to receive, MUST BE 8-bit!!
struct __attribute__((packed)) TelemetryPacket {
    uint8_t foil_bat = 0xFF;
    uint8_t foil_temp = 0xFF;
    uint8_t foil_speed = 0xFF;
    uint8_t error_code = 0;

    //This must be the last entry
    uint8_t link_quality = 0;
} telemetry;

// AFTER Phase 2:
//Telemetry to receive, MUST BE 8-bit!!
struct __attribute__((packed)) TelemetryPacket {
    uint8_t foil_bat = 0xFF;    // index 0 — battery % 0-100
    uint8_t foil_temp = 0xFF;   // index 1 — FET temp degC
    uint8_t foil_speed = 0xFF;  // index 2 — speed km/h
    uint8_t error_code = 0;     // index 3 — fault flags
    // Phase 2: SCALE=watts/50  DECODE=foil_power*50  range 0-12750W at 50W resolution
    uint8_t foil_power = 0xFF;  // index 4 — power (watts/50); 0xFF = not available
    //This must be the last entry
    uint8_t link_quality = 0;   // index 5 (was 4)
} telemetry;
```

### Tx sizeof Verification (startup print — temporary)

```c
// Add temporarily to setup() in V2_Integration_Tx.ino for verification:
Serial.print("sizeof(TelemetryPacket) = ");
Serial.println(sizeof(TelemetryPacket));
// Expected: 6 after Phase 2 (was 5 before)
```

### Compile-Time Assert (optional safety net)

```c
// Can add to BREmote_V2_Tx.h after the struct:
static_assert(sizeof(TelemetryPacket) == 6, "TelemetryPacket size mismatch — Rx/Tx structs may be out of sync");
// This is not strictly necessary for Phase 2 but prevents regression if fields are accidentally removed
```

### Confirming No Hard-Coded Offsets in Tx (search pattern)

```bash
# From repository root — verify no Tx code uses hard-coded byte offset 4 for link_quality
grep -n "ptr\[4\]\|telemetry + 4\|\[4\].*link" Source/V2_Integration_Tx/*.ino Source/V2_Integration_Tx/*.h
# Expected: no results (all access through named field telemetry.link_quality)
```

---

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| 5-field TelemetryPacket on Tx (indices 0-4) | 6-field TelemetryPacket on Tx (indices 0-5) | Phase 2 | Tx correctly receives foil_power at index 4; link_quality correctly at index 5 |
| foil_power silently written into old Tx link_quality slot (Phase 1 cosmetic side-effect) | foil_power written into dedicated slot; link_quality to correct slot | Phase 2 | Eliminates link_quality pollution from power data; link_quality display stabilizes |
| Rx-only struct extension (Phase 1) | Struct synchronized on both sides | Phase 2 | sizeof(TelemetryPacket) matches on both sides — cycle time for all fields equals 600ms |

**Deprecated/outdated after Phase 2:**
- The documented cosmetic side-effect of Phase 1 (foil_power byte polluting old Tx link_quality one cycle in five) is resolved by Phase 2. No longer a concern once Tx is updated.

---

## Current State Assessment (Pre-Phase 2)

Based on reading the actual source files:

**Rx (BREmote_V2_Rx.h lines 131-140):** TelemetryPacket has 6 fields. `foil_power` at index 4, `link_quality` at index 5. `sizeof == 6`. Rx cycles indices 0-5 (confirmed: `Radio.ino:233` wraps at `sizeof(TelemetryPacket)`). VESC.ino assigns `telemetry.foil_power` inside `#ifdef VESC_MORE_VALUES`. STATUS: **Complete from Phase 1.**

**Tx (BREmote_V2_Tx.h lines 95-103):** TelemetryPacket has 5 fields. `link_quality` at index 4. `sizeof == 5`. `waitForTelemetry()` accepts index 4 (foil_power from Rx) into link_quality slot (documented cosmetic side-effect). STATUS: **Needs Phase 2 change — this is the only work item.**

**Radio.ino (Tx):** `waitForTelemetry()` at line 344 uses `if (rcvArray[3] < sizeof(TelemetryPacket))` — dynamically uses sizeof. After Phase 2, sizeof==6 and index 5 (link_quality) will be accepted. No code change needed.

**DISPLAY_MODE constants (BREmote_V2_Tx.h lines 181-186):** `DISPLAY_MODE_COUNT = 5`. These are display cycling modes (temp, speed, bat, thr, intbat), not telemetry indices. Unaffected by Phase 2 struct change. The foil_power display mode will be added in Phase 3.

---

## Open Questions

1. **DEBUG_VESC still enabled on Rx (from Phase 1)**
   - What we know: Phase 1 verification noted `#define DEBUG_VESC` is uncommented at `BREmote_V2_Rx.h:217` — a WARNING flag.
   - What's unclear: Whether the user wants to disable this before Phase 2 execution or keep it for hardware validation.
   - Recommendation: Comment out `DEBUG_VESC` at the start of Phase 2 execution to keep serial output clean. It was intentionally left enabled for Phase 1 hardware validation. Phase 2 does not need it.

2. **Hardware validation of Phase 1 is still deferred**
   - What we know: Phase 1 verification status is `human_needed` — live VESC test with serial readout not yet done.
   - What's unclear: Whether Phase 2 should proceed before Phase 1 hardware validation, or wait.
   - Recommendation: Phase 2 is a pure struct synchronization change with no hardware risk. It can proceed in parallel with deferred Phase 1 hardware validation. The Phase 2 code change is trivially verifiable without hardware (sizeof check). Note in the plan that Phase 1 hardware validation remains open.

3. **static_assert for struct size: include or not?**
   - What we know: `static_assert(sizeof(TelemetryPacket) == 6, "...")` would give a compile-time error if Rx/Tx structs diverge in future.
   - What's unclear: Whether the project convention includes defensive static_asserts in headers.
   - Recommendation: Include it as a comment in the plan as optional. The planner can make it a task or not. It is not required for Phase 2 success criteria.

---

## Sources

### Primary (HIGH confidence)

- `Source/V2_Integration_Tx/BREmote_V2_Tx.h` — Read directly. TelemetryPacket lines 95-103: 5 fields confirmed, `link_quality` at index 4.
- `Source/V2_Integration_Rx/BREmote_V2_Rx.h` — Read directly. TelemetryPacket lines 131-140: 6 fields confirmed, `foil_power` at index 4, `link_quality` at index 5.
- `Source/V2_Integration_Tx/Radio.ino` — Read directly. `waitForTelemetry()` lines 314-368: pointer-cast pattern, `rcvArray[3] < sizeof(TelemetryPacket)` bounds check confirmed at line 344. `sizeof` is evaluated at compile-time from current struct definition — updates automatically with Phase 2 change.
- `Source/V2_Integration_Rx/Radio.ino` — Read directly. `triggeredReceive()` lines 184-265: `telemetry_index` cycling at lines 228-236, wraps at `sizeof(TelemetryPacket)` — currently 6 on Rx.
- `.planning/phases/01-rx-vesc-extension/01-VERIFICATION.md` — Phase 1 verification confirms: `sizeof(TelemetryPacket) == 6` on Rx, indices 0-5 cycle correctly, Phase 2 Tx struct extension explicitly identified as the remaining gap.
- `.planning/phases/01-rx-vesc-extension/01-01-SUMMARY.md` — Phase 1 execution summary confirms foil_power at index 4 in Rx struct, link_quality at index 5.

### Secondary (MEDIUM confidence)

- `.planning/phases/01-rx-vesc-extension/01-RESEARCH.md` — Phase 1 research documents the struct extension pattern, backwards compatibility behavior, and wire protocol mechanics in detail.
- `.planning/REQUIREMENTS.md` — TELE-01 and TELE-02 requirements text confirmed.

### Tertiary (LOW confidence)

- None. All critical implementation details are confirmed from source code.

---

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — all file paths, line numbers, and code patterns confirmed from direct source reads
- Architecture patterns: HIGH — confirmed from actual source code; no inference required
- Pitfalls: HIGH (code-derived) — pitfalls derive from confirmed struct mechanics and wire protocol behavior
- Implementation scope: HIGH — confirmed single file, single struct change

**Research date:** 2026-03-03
**Valid until:** 2026-04-03 (stable embedded codebase; no external library dependencies)
