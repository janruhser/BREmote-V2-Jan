// ===== Hardware Initialization =====

void initHardware()
{
  Wire.begin(P_I2C_SDA, P_I2C_SCL);
  Wire.setClock(400000);
  startupAW();
}

// ===== Storage & Config =====

void initStorage()
{
  initSPIFFS();
  ensureWebUiInSPIFFS();
  getConfFromSPIFFS();
  getBCFromSPIFFS();
  webCfgInit();

  // Semaphore must exist before radio can receive (ISR uses it)
  triggerReceiveSemaphore = xSemaphoreCreateBinary();

  // Radio init requires usrConf (radio_preset, rf_power) — must come after config load
  startupRadio();
  radio.setPacketReceivedAction(packetReceived);
  radio.implicitHeader(6);
}

// ===== FreeRTOS Tasks =====

void initTasks()
{
  loopTaskHandle = xTaskGetCurrentTaskHandle();

  //Runs every 10ms to generate both PWM signals, high prio
  xTaskCreatePinnedToCore(generatePWM, "Generate_PWM_10ms", 2048, NULL, 10, &generatePWMHandle, 0);
  //Runs upon RF interrupt and reads packet & responds, medium-high prio
  xTaskCreatePinnedToCore(triggeredReceive, "RF_ReceiveTask_triggered", 2048, NULL, 5, &triggeredReceiveHandle, 0);
  //Checks if there is connection and blinks LED, low prio
  xTaskCreatePinnedToCore(checkConnStatus, "Check_conn_staus_200ms", 2048, NULL, 2, &checkConnStatusHandle, 0);
}

// ===== Peripherals & Boot Sequence =====

void runBootSequence()
{
  checkButtons();
  rxIsrState = 1;
  radio.startReceive();

  initRMT();
  Serial1.begin(115200, SERIAL_8N1, P_U1_RX, P_U1_TX);

  aw.digitalWrite(AP_EN_PWM0, 1);
  aw.digitalWrite(AP_EN_PWM1, 1);

  configureGPS();
}
