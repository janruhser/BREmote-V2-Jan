---
created: 2026-03-02T14:35:38.691Z
title: Replace gear system with dynamic max cap
area: general
files:
  - Source/V2_Integration_Tx/Hall.ino
  - Source/V2_Integration_Tx/BREmote_V2_Tx.h
---

## Problem

The current throttle system uses discrete gears to limit max throttle output (e.g., gear 1 = 33%, gear 2 = 66%, gear 3 = 100%). This is coarse and doesn't allow fine-grained control. The left-right toggle movement currently changes gears.

## Solution

New logic: Instead of discrete gears dividing the 0-100% range, use a **dynamic maximum cap** that the rider adjusts in real-time with the left-right toggle:

- The toggle movement regulates the maximum throttle cap (e.g., max_cap starts at some default like 78%)
- The full throttle lever range (0-100%) always maps to 0 → max_cap, maintaining full haptic resolution regardless of the cap setting
- Example: if max_cap = 50%, full throttle pull gives 50% output but with the same physical travel distance — smooth control at any cap level
- The cap value (e.g., 78%) maps to the RX output range (e.g., 0-2500 PWM), so the rider still gets proportional control across the full physical throw
- Useful for compensating different battery charge levels during a session
- Consider keeping discrete gears as a fallback mode via config flag (`no_gear` or new `throttle_mode` config)
