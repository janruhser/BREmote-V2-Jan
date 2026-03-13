#include "BREmote_V2_Rx_Heltec.h"
#include "HAL_Heltec_CT62.h"

SX1262 radio = new Module(P_LORA_NSS, P_LORA_DIO, P_LORA_RST, P_LORA_BUSY);
Ticker ticksrc;
TinyGPSPlus gps;

void checkSerial() {
  if (Serial.available()) {
    // Basic serial checking logic if needed
    Serial.read();
  }
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
  ensureWebUiInSPIFFS();
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
