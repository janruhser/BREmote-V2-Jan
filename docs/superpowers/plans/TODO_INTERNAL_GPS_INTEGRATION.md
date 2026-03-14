# TODO: ESP-NOW GPS Integration

This plan outlines the steps needed to integrate the External ESP-NOW GPS module into the main BRemote2 Tx/Rx firmware.

## 1. Config Updates
- [ ] Add `uint16_t gps_source` to `confStruct` in `BREmote_V2_Rx.h` and `BREmote_V2_Tx.h`.
  - `0`: Local UART
  - `1`: External ESP-NOW
- [ ] Update `ConfigService.ino` on both sides to include the new field.
- [ ] Update `WebUiEmbedded.h` to add a dropdown for GPS Source.

## 2. ESP-NOW Receiver Implementation
- [ ] In `V2_Integration_Rx.ino` and `V2_Integration_Tx.ino`, initialize ESP-NOW in `setup()`.
- [ ] Implement `OnDataRecv` callback:
  - Check if packet size matches `EspNowGpsPacket`.
  - Verify magic number.
  - If `usrConf.gps_source == 1`, copy received data to the local `gps` or `telemetry` objects.

## 3. Parser Logic
- [ ] In `GPS.ino` (Rx) and `Throttle.ino` (Tx), wrap the Serial/TinyGPS logic:
  ```cpp
  if (usrConf.gps_source == 0) {
    // Current Serial read logic
  } else {
    // Data is already updated via ESP-NOW callback
  }
  ```
