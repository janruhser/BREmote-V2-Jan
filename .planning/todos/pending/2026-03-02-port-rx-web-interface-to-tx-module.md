---
created: 2026-03-02T14:35:38.691Z
title: Port RX web interface to TX module
area: general
files:
  - Source/V2_Integration_Rx/WebConfig.ino
  - Source/V2_Integration_Tx/WebConfig.ino
  - Source/V2_Integration_Tx/System.ino
---

## Problem

The RX module has a web interface for configuration, but the TX module needs an analogous one with similar structure and functionality. The serial command interface on TX (`System.ino`) has a rich set of `?get`/`?set`/`?keys` commands that could serve as the backend for a web UI.

## Solution

- Port the RX web interface pattern to the TX module
- Keep the structure similar between RX and TX web interfaces for consistency
- Build an abstraction layer for the serial command structure on TX so that both serial commands and web UI share the same config access logic (avoid duplicating get/set logic)
- Include JSON import/export in the TX web interface (see related todo: JSON import/export)
- Reuse the `WebUiEmbedded.h` pattern for embedding the TX web UI
