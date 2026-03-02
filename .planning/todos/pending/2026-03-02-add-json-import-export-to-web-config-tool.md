---
created: 2026-03-02T14:35:38.691Z
title: Add JSON import/export to web config tool
area: tooling
files:
  - Source/V2_Integration_Tx/WebConfig.ino
  - Source/V2_Integration_Tx/BREmote_V2_Tx.h
---

## Problem

The web configuration tool currently uses Base64-encoded `confStruct` for config transfer (`?setconf`/`?conf`). There's no human-readable way to export, inspect, or share device configurations. Users need a JSON import/export option in the web UI.

## Solution

- Add JSON export endpoint that serializes `confStruct` fields to named key/value JSON
- Add JSON import endpoint that parses JSON and populates `confStruct`
- On import, display a **warning notification** that importing calibration-specific values (`cal_ok`, `cal_offset`, throttle/toggle idle/pull values) may cause incorrect behavior since calibration is device-specific and should be re-done after import
- Consider a checkbox option to skip calibration fields during import
- Same JSON import/export should also be available on the TX module web interface (see related todo: port RX web interface to TX)
