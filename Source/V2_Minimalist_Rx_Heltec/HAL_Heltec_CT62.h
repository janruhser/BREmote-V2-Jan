#pragma once
#include "HAL.h"
#include "driver/rmt_tx.h"

// Pin definitions
#define P_LORA_NSS 8
#define P_LORA_SCK 10
#define P_LORA_MOSI 6
#define P_LORA_MISO 7
#define P_LORA_BUSY 1
#define P_LORA_RST 5
#define P_LORA_DIO 3
#define P_LORA_ANT_SW 0

// Map common SPI pins for RadioCommon.h
#define P_SPI_SCK P_LORA_SCK
#define P_SPI_MISO P_LORA_MISO
#define P_SPI_MOSI P_LORA_MOSI

#define P_ESC_OUT 9
#define P_VESC_RX 2
#define P_VESC_TX 4
#define P_GPS_RX 20
#define P_GPS_TX 21

// RMT Globals
static rmt_channel_handle_t tx_channel = NULL;
static rmt_encoder_handle_t copy_encoder = NULL;
static rmt_symbol_word_t pulse_symbol;

inline void hal_init() {
    Serial.begin(115200); // Native CDC
    
    pinMode(P_LORA_ANT_SW, OUTPUT);
    digitalWrite(P_LORA_ANT_SW, LOW);
    
    Serial1.begin(115200, SERIAL_8N1, P_VESC_RX, P_VESC_TX);
    Serial0.begin(9600, SERIAL_8N1, P_GPS_RX, P_GPS_TX); // Standard GPS baud rate
}

inline void hal_esc_init() {
    // Initialize RMT TX channel
    rmt_tx_channel_config_t tx_chan_config = {
        .gpio_num = (gpio_num_t)P_ESC_OUT,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 1000000,         // 1MHz, 1 tick = 1μs
        .mem_block_symbols = 64,
        .trans_queue_depth = 4,
    };

    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_config, &tx_channel));
    rmt_copy_encoder_config_t copy_encoder_config = {};
    ESP_ERROR_CHECK(rmt_new_copy_encoder(&copy_encoder_config, &copy_encoder));
    ESP_ERROR_CHECK(rmt_enable(tx_channel));
}

inline void hal_esc_write(uint16_t value) {
    pulse_symbol.level0 = 1;
    pulse_symbol.duration0 = value;  // High time in microseconds
    pulse_symbol.level1 = 0;
    pulse_symbol.duration1 = 1;      // Low time in microseconds
    
    rmt_transmit_config_t tx_config = {
        .loop_count = 0, // No infinite loop, just one pulse
    };
    tx_config.flags.eot_level = 0; // End-of-transmission level (LOW)
    
    rmt_transmit(tx_channel, copy_encoder, &pulse_symbol, 1, &tx_config);
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
