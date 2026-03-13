#include "BREmote_V2_Rx_Heltec.h"

SX1262 radio = new Module(P_LORA_NSS, P_LORA_DIO, P_LORA_RST, P_LORA_BUSY);
Ticker ticksrc;
TinyGPSPlus gps;

/*
** HAL Implementation
*/
void hal_init() {
    Serial.begin(115200); // Native CDC
    
    pinMode(P_LORA_ANT_SW, OUTPUT);
    digitalWrite(P_LORA_ANT_SW, LOW);
    
    Serial1.begin(115200, SERIAL_8N1, P_U1_RX, P_U1_TX);
    Serial0.begin(9600, SERIAL_8N1, P_U0_RX, P_U0_TX);
}

void hal_esc_init() {
    // Initialize RMT TX channel
    rmt_tx_channel_config_t tx_chan_config = {
        .gpio_num = (gpio_num_t)P_PWM_OUT,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 1000000,         // 1MHz, 1 tick = 1μs
        .mem_block_symbols = 64,
        .trans_queue_depth = 4,
    };

    tx_chan_config.flags.io_od_mode = 0; // standard mode
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_config, &tx_channel));
    rmt_copy_encoder_config_t copy_encoder_config = {};
    ESP_ERROR_CHECK(rmt_new_copy_encoder(&copy_encoder_config, &copy_encoder));
    ESP_ERROR_CHECK(rmt_enable(tx_channel));
}

void hal_esc_write(uint16_t value) {
    pulse_symbol.level0 = 1;
    pulse_symbol.duration0 = value;  // High time in microseconds
    pulse_symbol.level1 = 0;
    pulse_symbol.duration1 = 1;      // Low time in microseconds
    
    rmt_transmit_config_t tx_config = {
        .loop_count = 0, // One pulse
    };
    tx_config.flags.eot_level = 0; // LOW
    
    rmt_transmit(tx_channel, copy_encoder, &pulse_symbol, 1, &tx_config);
}

void hal_radio_switch_mode(bool tx) {
    digitalWrite(P_LORA_ANT_SW, tx ? HIGH : LOW);
}

HardwareSerial& hal_get_vesc_uart() {
    return Serial1;
}

HardwareSerial& hal_get_gps_uart() {
    return Serial0;
}

void initTasks()
{
  loopTaskHandle = xTaskGetCurrentTaskHandle();

  //Semaphore for triggered task
  triggerReceiveSemaphore = xSemaphoreCreateBinary();

  //Runs upon RF interrupt and reads packet & responds, medium-high prio
  xTaskCreatePinnedToCore(triggeredReceive, "RF_ReceiveTask_triggered", 2048, NULL, 5, &triggeredReceiveHandle, 0);
}

void setup()
{
  hal_init();
  hal_esc_init();

  initSPIFFS();
  getBCFromSPIFFS();
  getConfFromSPIFFS();
  
#ifdef WIFI_ENABLED
  webCfgInit();
#endif

  initTasks();

  startupRadio();
  radio.setPacketReceivedAction(packetReceived);
  radio.implicitHeader(6);
  
  rxIsrState = 1;
  hal_radio_switch_mode(false); // RX
  radio.startReceive();
  
  PWM_active = 1;
}

unsigned long loop_timer = 0;

void loop()
{
  esp_task_wdt_reset();
#ifdef WIFI_ENABLED
  webCfgLoop();
#endif

  if(millis() - loop_timer > 1000)
  {
    loop_timer = millis();

    if(usrConf.data_src == 2)
    {
      getVescLoop();
    }
  }

  vTaskDelay(pdMS_TO_TICKS(10));
}
