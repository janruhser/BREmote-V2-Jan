# Phase 3: Tx Display - Context

**Gathered:** 2026-03-03
**Status:** Ready for planning

<domain>
## Phase Boundary

Tx parses the incoming `foil_power` telemetry index and renders power in watts on the 7x10 LED matrix when the rider cycles through display modes. No new telemetry fields, no struct changes, no Rx changes — purely Tx display logic.

</domain>

<decisions>
## Implementation Decisions

### Value formatting
- Display power as **hectowatts** (watts ÷ 100): "25" = 2500W, "08" = 800W
- Conversion math: `foil_power * 50 / 100` = `foil_power / 2` (since Rx encodes as watts/50)
- Maximum displayable: 9900W (shows "99") — sufficient headroom for all typical eFoil motors
- No smoothing — show raw value, consistent with temp/speed/battery modes
- 200ms display refresh provides natural dampening

### Mode label & position
- Power mode inserted **after Speed** in the cycle: Temp → Speed → **Power** → Battery → Throttle → Internal Battery
- Groups performance metrics (speed, power) together, then board state (battery), then remote-local values
- Mode only visible when `foil_power != 0xFF` — same availability pattern as temp/speed/battery

### Unavailable state
- When power becomes unavailable (0xFF) while rider is viewing it: show "--" briefly, then auto-advance to next available mode
- Uses the existing `isDisplayModeAvailable()` fallback already in `renderOperationalDisplay()`

### Claude's Discretion
- 2-letter mode label (pick from available letters: A,B,C,D,E,F,H,I,L,P,T,U,V,X,Y)
- Low power handling (sub-100W shows "00" or clamp to minimum 1)

</decisions>

<code_context>
## Existing Code Insights

### Reusable Assets
- `displayDigits(dig1, dig2)`: Renders any 2-digit value on the LED matrix — the core display primitive
- `displayShowTwoDigitOrDash(value)`: Shows 2-digit number or "--" when value is 0xFF — exact pattern needed for power
- `isDisplayModeAvailable(mode)`: Controls whether a mode appears in the cycle — add power check here
- `renderOperationalDisplay()`: Main display switch statement — add power case here
- `cycleDisplayMode(direction)`: Hall.ino function that cycles modes and shows labels

### Established Patterns
- Display modes defined as `#define DISPLAY_MODE_X N` in `BREmote_V2_Tx.h`
- `DISPLAY_MODE_COUNT` controls cycle wrap-around
- Each mode has: define constant, availability check, label display (2-letter), value display (2-digit)
- 0xFF sentinel = "not available" = show dashes

### Integration Points
- `BREmote_V2_Tx.h`: Add `DISPLAY_MODE_POWER` define, renumber subsequent modes, increment `DISPLAY_MODE_COUNT`
- `Hall.ino`: Add `isDisplayModeAvailable()` case and `cycleDisplayMode()` label case
- `Display.ino`: Add `renderOperationalDisplay()` switch case

</code_context>

<specifics>
## Specific Ideas

No specific requirements — follow existing display mode patterns exactly. The implementation should be indistinguishable from the other modes in code style and behavior.

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 03-tx-display*
*Context gathered: 2026-03-03*
