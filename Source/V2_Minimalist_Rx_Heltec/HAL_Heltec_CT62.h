#pragma once
#include "HAL.h"

// Pin definitions
#define P_LORA_NSS 8
#define P_LORA_SCK 10
#define P_LORA_MOSI 6
#define P_LORA_MISO 7
#define P_LORA_BUSY 1
#define P_LORA_RST 5
#define P_LORA_DIO 3
#define P_LORA_ANT_SW 0

#define P_ESC_OUT 9
#define P_VESC_RX 2
#define P_VESC_TX 4
#define P_GPS_RX 20
#define P_GPS_TX 21

// PWM constants for ESC
#define ESC_PWM_FREQ 50
#define ESC_PWM_RES 16
#define ESC_PWM_CHANNEL 0

inline void hal_init() {
    Serial.begin(115200); // Native CDC
    
    pinMode(P_LORA_ANT_SW, OUTPUT);
    digitalWrite(P_LORA_ANT_SW, LOW);
    
    Serial1.begin(115200, SERIAL_8N1, P_VESC_RX, P_VESC_TX);
    Serial0.begin(9600, SERIAL_8N1, P_GPS_RX, P_GPS_TX);
}

inline void hal_esc_init() {
    ledcSetup(ESC_PWM_CHANNEL, ESC_PWM_FREQ, ESC_PWM_RES);
    ledcAttachPin(P_ESC_OUT, ESC_PWM_CHANNEL);
}

inline void hal_esc_write(uint16_t value) {
    // Convert pulse width in micros to 16-bit duty cycle for 50Hz (20ms period)
    uint32_t duty = (value * 65535) / 20000;
    ledcWrite(ESC_PWM_CHANNEL, duty);
}

inline void hal_radio_switch_mode(bool tx) {
    digitalWrite(P_LORA_ANT_SW, tx ? HIGH : LOW);
}

inline HardwareSerial& hal_get_vesc_uart() {
    return Serial1;
}

inline HardwareSerial& hal_get_gps_uart() {
    return Serial0;
}
