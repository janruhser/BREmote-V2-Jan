// ===== Hardware Initialization =====

void initHardware()
{
  Wire.begin(P_I2C_SDA, P_I2C_SCL);
  Wire.setClock(400000);
  startupDisplay();
  startupADS();
}

// ===== Storage & Config =====

void initStorage()
{
  initSPIFFS();
  if(!ensureWebUiInSPIFFS())
  {
    Serial.println("WARNING: Web UI seed failed.");
  }
  getConfFromSPIFFS();
  webCfgInit();

  if(usrConf.max_gears <= 0) usrConf.max_gears = 1;

  // Radio init requires usrConf (radio_preset, rf_power) — must come after config load
  startupRadio();
  radio.setDio1Action(packetReceived);
}

// ===== FreeRTOS Tasks =====

void initTasks()
{
  xTaskCreatePinnedToCore(sendData, "Send_Data_100ms", 2048, NULL, 5, &sendDataHandle, 0);
  xTaskCreatePinnedToCore(waitForTelemetry, "wait_for_telem_triggered", 2048, NULL, 4, &triggeredWaitForTelemetryHandle, 0);
  xTaskCreatePinnedToCore(measBufCalc, "wait_for_telem_triggered_10ms", 2048, NULL, 6, &measBufCalcHandle, 0);
  xTaskCreatePinnedToCore(updateBargraphs, "wait_for_telem_triggered_200ms", 2048, NULL, 6, &updateBargraphsHandle, 0);
}

// ===== Boot Sequence =====

void runBootSequence()
{
  bootAnimation();
  checkCal();
  checkStartupButtons();
  checkPairing();
}

// ===== Apply Config Settings =====

void applyConfigSettings()
{
  if(usrConf.no_lock)
  {
    while(thr_scaled > 10)
    {
      advanceArrow();
      delay(100);
    }
    system_locked = 0;
    webCfgNotifyTxUnlocked();
  }

  if(usrConf.no_gear)
  {
    gear = usrConf.max_gears - 1;
  }
  else
  {
    if(usrConf.startgear >= usrConf.max_gears) usrConf.startgear = usrConf.max_gears - 1;
    gear = usrConf.startgear;
  }
}
