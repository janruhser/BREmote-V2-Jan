# TODO

- [ ] **Web Interface & Config Structure:** Update `WebUiEmbedded.h` and the underlying config struct (`confStruct` in `BREmote_V2_Rx_Heltec.h` and `kCfgFields` in `ConfigService.ino`) to strictly match the minimalist capabilities. Remove UI elements and logic for dual motors, steering, AW9523, and analog sensors.
- [ ] **Compile-Time Feature Flags:** Add a `#define` flag system (e.g., `#define USE_GPS` or `#define USE_WETNESS_DETECTION`) in `BREmote_V2_Rx_Heltec.h`. Since GPIO 20 and 21 are the only remaining pins, this flag should conditionally compile either the GPS logic or the Wetness Detection logic and map the pins accordingly.
- [ ] **Test Suite Integration:** Update the Python-based test suite in `Tools/` to include support for the new minimalist RX module.
- [ ] **Link Test Support:** Add a dedicated link test command or script to verify robust communication between a BRemote TX and the minimalist Heltec RX.

## Functional Testing
- [ ] **Radio Link:** Verify the module can successfully enter pairing mode and receive packets from a BRemote TX.
- [ ] **ESC Output:** Use an oscilloscope or logic analyzer on **GPIO 9** to verify a stable 50Hz PWM signal that responds correctly to throttle inputs.
- [ ] **VESC Telemetry:** Connect a VESC to **GPIO 2 (RX) and GPIO 4 (TX)** and verify that voltage, temperature, and power data are correctly retrieved and sent back to the TX.
- [ ] **Web Configuration:** Verify that the Wi-Fi Access Point starts up and the configuration page is accessible at `192.168.4.1`.
- [ ] **Native USB Debugging:** Verify that `Serial.print` output is correctly received over the USB-C connection.
