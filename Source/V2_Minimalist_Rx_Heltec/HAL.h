#pragma once
#include <Arduino.h>
#include <HardwareSerial.h>

void hal_init();
void hal_esc_init();
void hal_esc_write(uint16_t value);
void hal_radio_switch_mode(bool tx);
HardwareSerial& hal_get_vesc_uart();
HardwareSerial& hal_get_gps_uart();
