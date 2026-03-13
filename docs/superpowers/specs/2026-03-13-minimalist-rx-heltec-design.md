# BRemote2 Minimalist RX (Heltec HT-CT62) Design Specification

## 1. Overview and Purpose
This document outlines the design and architecture for a minimalist receiver (RX) module for the BRemote2 project, specifically targeting the Heltec HT-CT62 (ESP32-C3 + SX1262) development board. The goal is to create a streamlined, single-output receiver that eliminates the hardware complexity of the original RX (e.g., AW9523 I/O expander, dual motor mixing) while establishing a Hardware Abstraction Layer (HAL) to facilitate future refactoring of the main codebase.

## 2. Architecture: The "Clean Fork" Strategy
To prevent destabilizing the existing `V2_Integration_Rx` codebase and avoid overly complex conditional compilation, this project will be implemented as a separate directory: `Source/V2_Minimalist_Rx_Heltec`.

### 2.1 Directory Structure
*   `V2_Minimalist_Rx_Heltec.ino`: The main Arduino entry point (Setup/Loop).
*   `HAL.h`: The abstract interface defining hardware interactions.
*   `HAL_Heltec_CT62.h`: The specific implementation of `HAL.h` for the HT-CT62.
*   `Radio.ino`, `VESC.ino`, `ConfigService.ino`: Business logic ported from the original RX, modified strictly to use the `HAL` interface rather than direct hardware calls.
*   `SPIFFS.ino`, `WebConfig.ino`: Utility files retained for configuration.

### 2.2 The Hardware Abstraction Layer (HAL)
The HAL acts as a contract between the high-level logic (e.g., "send throttle value") and the low-level hardware (e.g., "configure RMT peripheral on GPIO 9"). This separation ensures the core logic is agnostic to the underlying board, fulfilling the user's long-term goal of code reuse.

**Core HAL Interfaces:**
*   `void hal_init()`: Initializes all hardware peripherals (UARTs, SPI, GPIOs).
*   `void hal_esc_init()`: Configures the ESC output pin.
*   `void hal_esc_write(uint16_t value)`: Outputs the control signal. This interface is designed to support standard PWM currently, with the explicit architectural intent to support DSHOT protocols in the future without modifying higher-level logic.
*   `void hal_radio_switch_mode(bool tx)`: Specifically addresses the HT-CT62's requirement to manually toggle the antenna switch (GPIO 0).
*   `HardwareSerial& hal_get_vesc_uart()`: Returns the initialized UART instance for VESC communication.
*   `HardwareSerial& hal_get_gps_uart()`: Returns the initialized UART instance for GPS communication.

## 3. Hardware Mapping and Pinout (ESP32-C3 Native)
The ESP32-C3 features a flexible GPIO matrix and a native USB Serial/JTAG controller, allowing for an optimal pin configuration that supports all required peripherals without resorting to SoftwareSerial.

| Peripheral / Function | ESP32-C3 GPIO | Notes |
| :--- | :--- | :--- |
| **USB Console (Debug)** | GPIO 18 (D-), 19 (D+) | Native USB CDC; no hardware UART required. |
| **VESC UART (UART1)** | GPIO 2 (RX), GPIO 4 (TX) | High-speed telemetry link. |
| **GPS UART (UART0)** | GPIO 20 (RX), GPIO 21 (TX) | Standard hardware UART pins. |
| **ESC Output (PWM/DSHOT)** | GPIO 9 | Configured via ESP32 RMT peripheral; Boot pin (safe as output post-boot). |
| **LoRa NSS (CS)** | GPIO 8 | Internal HT-CT62 connection. |
| **LoRa SCK** | GPIO 10 | Internal HT-CT62 connection. |
| **LoRa MOSI** | GPIO 6 | Internal HT-CT62 connection. |
| **LoRa MISO** | GPIO 7 | Internal HT-CT62 connection. |
| **LoRa BUSY** | GPIO 1 | Internal HT-CT62 connection. |
| **LoRa RESET** | GPIO 5 | Internal HT-CT62 connection. |
| **LoRa DIO1** | GPIO 3 | Internal HT-CT62 connection. |
| **LoRa Antenna Switch** | GPIO 0 | Managed via `hal_radio_switch_mode()` (High = TX, Low = RX). |

## 4. Feature Reductions (Minimalist Scope)
To optimize for the ESP32-C3 and the minimalist use case, the following features from the original `V2_Integration_Rx` will be removed or significantly altered:

1.  **AW9523 I/O Expander:** All code and I2C dependencies related to the AW9523 are removed.
2.  **Analog Battery Measurement:** The direct ADC battery measurement logic is removed. The system will rely exclusively on VESC telemetry for voltage data.
3.  **Dual Motor / Steering Mixing:** The complex steering logic and secondary PWM outputs are removed in favor of a single, direct ESC output.
4.  **Configuration Structure (`confStruct`):** The data structure used in `ConfigService.ino` and stored in SPIFFS will be simplified. Fields related to steering, dual PWM limits, and the AW9523 will be removed to save memory and simplify the Web UI.

## 5. Testing and Validation
*   **Compile-Time:** Ensure the project compiles successfully for the `esp32c3` target using the standard Arduino core or PlatformIO.
*   **Hardware Interface:** Verify that `hal_radio_switch_mode()` correctly toggles GPIO 0 during SX1262 transmit/receive cycles.
*   **Signal Output:** Validate that `hal_esc_write()` produces a stable, jitter-free PWM signal on GPIO 9 across the full throttle range using an oscilloscope or logic analyzer, confirming readiness for future DSHOT implementation.
