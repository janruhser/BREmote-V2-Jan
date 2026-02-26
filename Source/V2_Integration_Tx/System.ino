void enterSetup()
{
  Serial.begin(115200);
  delay(100);
  Serial.println("");
  Serial.println("Entering Setup...");  

  Serial.println("**************************************");
  Serial.println("**          BREmote V2 TX           **");
  Serial.printf("**        MAC: %012llX         **\n", ESP.getEfuseMac());
  //Serial.print("**     HW Identifier: "); Serial.print(checkHWConfig()); Serial.println("       **");
  Serial.printf("**          SW Version: %-10d  **\n", SW_VERSION);
  Serial.printf("**  Compiled: %s %s  **\n", __DATE__, __TIME__);
  Serial.println("**************************************");
}

void exitSetup()
{
  Serial.println("");
  Serial.println("...Leaving Setup");
  Serial.println("");
}

void deepSleep()
{
  if(!isDisplayActivityEnabled())
  {
    setDisplayActivityEnabled(true);
  }
  displayDigits(LET_X, LET_X);
  updateDisplay();
  setBrightness(0x00);
  Serial.println("Going to sleep now");
  setRadioActivityEnabled(false);
  Serial.flush();
  esp_deep_sleep_start();
}

String checkHWConfig()
{
  //Not sure why this is necessary, otherwise pullup is too strong
  pinMode(18, OUTPUT);
  digitalWrite(18, HIGH);
  digitalWrite(18, LOW);

  pinMode(19, OUTPUT);
  digitalWrite(19, HIGH);
  digitalWrite(19, LOW);

  uint8_t pin_id = 0;

  pinMode(18, INPUT_PULLUP);
  pinMode(19, INPUT_PULLUP);

  pin_id |= (digitalRead(18)&0x01)<<0;
  pin_id |= (digitalRead(19)&0x01)<<4;

  pinMode(18, INPUT_PULLDOWN);
  pinMode(19, INPUT_PULLDOWN);

  pin_id |= (digitalRead(18)&0x01)<<1;
  pin_id |= (digitalRead(19)&0x01)<<5;

  if(pin_id == 0x11)
  {
    return "Unknown";
  }

  pinMode(18, OUTPUT);
  digitalWrite(18, HIGH);

  pin_id |= (digitalRead(19)&0x01)<<6;

  digitalWrite(18, LOW);

  pin_id |= (digitalRead(19)&0x01)<<7;

  pinMode(18, INPUT_PULLUP);
  pinMode(19, OUTPUT);
  digitalWrite(19, HIGH);

  pin_id |= (digitalRead(18)&0x01)<<2;

  digitalWrite(19, LOW);

  pin_id |= (digitalRead(18)&0x01)<<3;

  pinMode(18, INPUT_PULLUP);
  pinMode(19, INPUT_PULLUP);

  //Serial.println(pin_id, HEX);

  switch (pin_id)
  {
    case 0x77:
      // b8
      return "Beta_8M";
      break;
    case 0x44:
      // b9
      return "Beta_9M";
      break;
    case 0x40:
      // g18
      return "Gen1_8M";
      break;
    case 0x04:
      // g19
      return "Gen1_9M";
      break;
    case 0xF7:
      // g28
      return "Gen2_8M";
      break;
    case 0x7F:
      // g29
      return "Gen2_9M";
      break;
    case 0xFF:
      // o18
      return "Diy1_8M";
      break;
    case 0x00:
      // o19
      return "Diy1_9M";
      break;
    case 0x0F:
      // o28
      return "Diy2_8M";
      break;
    case 0xF0:
      // o29
      return "Diy2_9M";
      break;
  
    default:
      return "Unknown";
      break;
  }
}

void printHexArray(const uint8_t* buffer, size_t size) {
  for (size_t i = 0; i < size; i++) {
    // Print leading zero for values less than 0x10
    if (buffer[i] < 0x10) {
      Serial.print("0");
    }
    Serial.print(buffer[i], HEX);
    // Add space between bytes (except after the last one)
    if (i < size - 1) {
      Serial.print(" ");
    }
  }
  // Add newline at the end
  Serial.println();
}

void printHexArray16(const volatile uint16_t* buffer, size_t size) {
  for (size_t i = 0; i < size; i++) {
    // Print leading zeros for values less than 0x1000 and 0x100
    if (buffer[i] < 0x1000) {
      Serial.print("0");
    }
    if (buffer[i] < 0x100) {
      Serial.print("0");
    }
    if (buffer[i] < 0x10) {
      Serial.print("0");
    }
    Serial.print(buffer[i], HEX);
    // Add space between values (except after the last one)
    if (i < size - 1) {
      Serial.print(" ");
    }
  }
  // Add newline at the end
  Serial.println();
}

uint8_t esp_crc8(uint8_t *data, uint8_t length) {
    uint8_t crc = 0xFF;
    for (uint8_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x31;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

void checkStartupButtons()
{
  if(thr_scaled > 100)
  {
    if(tog_input == 1)
    {
      //Delete SPIFFS
      Serial.println("Deleting conf from SPIFFS & rebooting...");
      deleteConfFromSPIFFS();
      scroll3Digits(LET_D, LET_E, LET_L, 200);
      scroll3Digits(LET_D, LET_E, LET_L, 200);
      scroll3Digits(LET_D, LET_E, LET_L, 200);
      ESP.restart();
    }
    else if (tog_input == -1)
    {
      //USB Mode
      Serial.println("USB Mode, type '?' for info...");
      while(1)
      {
        scroll3Digits(LET_U, 5, LET_B, 200);
        checkSerial();
      }
    }
  }
  else
  {
    if(tog_input == 1)
    {
      //Pairing
      usrConf.paired = 0;
      checkPairing();
    }
    else if (tog_input == -1)
    {
      //Calib
      usrConf.cal_ok = 0;
      checkCal();
    }
  }
}

void checkSerial()
{
  if(!serialOff)
  {
    // Check if data is available on the serial port
    if (Serial.available() > 0) {
      String command = Serial.readStringUntil('\n');  // Read input until newline

      // Trim leading and trailing spaces
      command.trim();
      String commandLower = command;
      commandLower.toLowerCase();
      
      // Process the command
      if (commandLower.startsWith("?")) {
        if (commandLower == "?conf") {
          serPrintConf();  // Call function for ?conf
        } 
        else if (commandLower.startsWith("?setconf")) {
          String data = command.substring(command.indexOf(":") + 1);
          serSetConf(data);
        } 
        else if (commandLower == "?clearspiffs") {
          serClearConf();  // Call function for ?clearSPIFFS
        }
        else if (commandLower == "?applyconf") {
          serApplyConf();
        }
        else if (commandLower == "?reboot")
        {
          Serial.println("Rebooting now...");
          delay(1000);
          ESP.restart();
        }
        else if (commandLower == "?exitchg")
        {
          Serial.println(" Exit by user");
          exitChargeScreen = 1;
        }
        else if(commandLower == "?printrssi")
        {
          serPrintRSSI();
        }
        else if(commandLower == "?printinputs")
        {
          serPrintInputs();
        }
        else if(commandLower == "?printtasks")
        {
          serPrintTasks();
        }
        else if(commandLower == "?printpackets")
        {
          serPrintPackets();
        }
        else if(commandLower == "?state")
        {
          Serial.println(webCfgGetStateLine());
        }
        else if(commandLower == "?err")
        {
          Serial.println(webCfgGetLastError());
        }
        else if(commandLower == "?webdbg")
        {
          Serial.print("webdbg=");
          Serial.println(webCfgGetDebugModeName());
        }
        else if(commandLower.startsWith("?webdbg "))
        {
          String mode = command.substring(8);
          mode.trim();
          if(webCfgSetDebugMode(mode))
          {
            Serial.print("webdbg=");
            Serial.println(webCfgGetDebugModeName());
          }
          else
          {
            Serial.println("ERR_WEBDBG_MODE");
          }
        }
        else if(commandLower == "?wifips")
        {
          Serial.print("wifips_ms=");
          Serial.println(webCfgGetStartupTimeoutMs());
        }
        else if(commandLower.startsWith("?wifips "))
        {
          String value = command.substring(8);
          value.trim();
          if(value.equalsIgnoreCase("off"))
          {
            if(webCfgSetStartupTimeoutMs(0)) Serial.println("wifips_ms=0");
            else Serial.println("ERR_WIFIPS_VALUE");
          }
          else
          {
            long ms = value.toInt();
            bool isDigitsOnly = value.length() > 0;
            for(size_t i = 0; i < value.length(); i++)
            {
              if(!isDigit((unsigned char)value[i])) { isDigitsOnly = false; break; }
            }
            if(!isDigitsOnly || ms < 0 || !webCfgSetStartupTimeoutMs((uint32_t)ms)) Serial.println("ERR_WIFIPS_VALUE");
            else { Serial.print("wifips_ms="); Serial.println(ms); }
          }
        }
        else if(commandLower == "?txunlock")
        {
          webCfgNotifyTxUnlocked();
          Serial.println("TX unlock notified: AP will stop.");
        }
        else if(commandLower == "?uiversion")
        {
          Serial.print("ui_target=");
          Serial.println(getTargetWebUiVersion());
          Serial.print("ui_installed=");
          Serial.println(getInstalledWebUiVersion());
        }
        else if(commandLower == "?uiupdate")
        {
          if(forceUpdateWebUiInSPIFFS())
          {
            Serial.print("UI updated to ");
            Serial.println(getTargetWebUiVersion());
          }
          else
          {
            Serial.println("ERR_UI_UPDATE");
          }
        }
        else if(commandLower == "?wifion")
        {
          webCfgEnableService();
          Serial.println("WiFi/AP config service enabled.");
        }
        else if(commandLower == "?wifioff")
        {
          webCfgDisableService();
          Serial.println("WiFi/AP config service disabled.");
        }
        else if(commandLower == "?displayoff")
        {
          setDisplayActivityEnabled(false);
          Serial.println("Display activity disabled.");
        }
        else if(commandLower == "?displayon")
        {
          setDisplayActivityEnabled(true);
          Serial.println("Display activity enabled.");
        }
        else if(commandLower == "?radiooff")
        {
          setRadioActivityEnabled(false);
          Serial.println("Radio activity disabled.");
        }
        else if(commandLower == "?radioon")
        {
          setRadioActivityEnabled(true);
          Serial.println("Radio activity enabled.");
        }
        else if(commandLower == "?halloff")
        {
          setHallActivityEnabled(false);
          Serial.println("Hall activity disabled.");
        }
        else if(commandLower == "?hallon")
        {
          setHallActivityEnabled(true);
          Serial.println("Hall activity enabled.");
        }
        else if(commandLower == "?allon")
        {
          setHallActivityEnabled(true);
          setRadioActivityEnabled(true);
          setDisplayActivityEnabled(true);
          Serial.println("All activity gateways enabled.");
        }
        else if(commandLower == "?alloff")
        {
          setDisplayActivityEnabled(false);
          setRadioActivityEnabled(false);
          setHallActivityEnabled(false);
          Serial.println("All activity gateways disabled.");
        }
        else if (commandLower == "?") {
          // List all possible inputs
          Serial.println("Possible commands:");
          Serial.println("?conf - print info, usrConf");
          Serial.println("?setConf:<data> - write B64 to SPIFFS");
          Serial.println("?applyConf - read conf from SPIFFS and write to usrConf");
          Serial.println("?clearSPIFFS - Clear usrConf from SPIFFS");
          Serial.println("?reboot - Reboot the remote");
          Serial.println("?exitChg - Exit the charge screen");
          Serial.println("?printRSSI - Print RSSI and SNR values until sent 'quit'");
          Serial.println("?printInputs - Print raw input values until sent 'quit'");
          Serial.println("?printTasks - Print task stack usage until sent 'quit'");
          Serial.println("?printPackets - Print current state of tx,rx packets and relation");
          Serial.println("?state - print web config state/counters");
          Serial.println("?err - print last web config error");
          Serial.println("?webdbg - print web debug mode");
          Serial.println("?webdbg some|full|off - set web debug mode");
          Serial.println("?wifips - print startup no-client timeout in ms");
          Serial.println("?wifips <ms|off> - set timeout (off=0)");
          Serial.println("?txunlock - notify TX unlock and stop AP");
          Serial.println("?uiVersion - print target and installed UI version");
          Serial.println("?uiUpdate - force embedded UI update to SPIFFS");
          Serial.println("?wifiOn - start WiFi/AP config service");
          Serial.println("?wifiOff - stop WiFi/AP config service");
          Serial.println("?displayOff - Disable display activity");
          Serial.println("?displayOn - Enable display activity");
          Serial.println("?radioOff - Disable radio activity");
          Serial.println("?radioOn - Enable radio activity");
          Serial.println("?hallOff - Disable hall sampling activity");
          Serial.println("?hallOn - Enable hall sampling activity");
          Serial.println("?allOff - Disable hall/radio/display activity");
          Serial.println("?allOn - Enable hall/radio/display activity");
        }
        else {
          Serial.println("Unknown command. Type '?' for help.");
        }
      }
      else {
        Serial.println("Unknown command. Type '?' for help.");
      }
    }
  }
}

void serApplyConf()
{
  Serial.print("Reading conf from SPIFFS and applying to usrConf");
  readConfFromSPIFFS(usrConf);
}

void serSetConf(String data) {
  Serial.print("Setting configuration to: ");
  Serial.println(data);
  
  uint8_t* encodedData = new uint8_t[data.length()];
  // Call the function to fill the encodedData array
  for (size_t i = 0; i < data.length(); i++) {
    encodedData[i] = data[i];  // Convert each character to uint8_t
  }

  // Save to SPIFFS via temp file to prevent corruption on power loss
  File file = SPIFFS.open("/data.tmp", FILE_WRITE);
  if (!file) {
      Serial.println("Failed to open temp file for writing");
      delete[] encodedData;
      return;
  }
  file.write(encodedData, data.length());
  file.close();
  SPIFFS.remove(CONF_FILE_PATH);
  SPIFFS.rename("/data.tmp", CONF_FILE_PATH);
  Serial.println("Struct saved to SPIFFS as Base64");
  delete[] encodedData;
}

void serClearConf()
{
  Serial.println("Deleting conf from SPIFFS");
  deleteConfFromSPIFFS();
}

// Returns true if "quit" was received on Serial
bool checkSerialQuit()
{
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    if (input.equals("quit")) {
      Serial.println("Stopping print loop.");
      return true;
    }
  }
  return false;
}

void serPrintTasks()
{
  while (true)
  {
    if(checkSerialQuit()) break;

    Serial.println("\n=== Task Stack Usage ===");

    Serial.printf("sendData stack left: %u words\n", uxTaskGetStackHighWaterMark(sendDataHandle));
    Serial.printf("telemetry stack left: %u words\n", uxTaskGetStackHighWaterMark(triggeredWaitForTelemetryHandle));
    Serial.printf("measBufCalc stack left: %u words\n", uxTaskGetStackHighWaterMark(measBufCalcHandle));
    Serial.printf("bargraph stack left: %u words\n", uxTaskGetStackHighWaterMark(updateBargraphsHandle));
    Serial.printf("loop() stack left: %u words\n", uxTaskGetStackHighWaterMark(loopTaskHandle));


    Serial.println("========================\n");

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void serPrintPackets()
{
  Serial.print("Sent: ");
  Serial.println(num_sent_packets);
  Serial.print("Received: ");
  Serial.println(num_rcv_packets);
  Serial.print("Ratio: ");
  if(num_sent_packets > 0)
  {
    Serial.print(((float)num_rcv_packets/(float)num_sent_packets)*100);
    Serial.println(" %");
  }
  else
  {
    Serial.println("N/A");
  }
}

void serPrintRSSI()
{
  while (true)
  {
    if(checkSerialQuit()) break;

    if(!isRadioActivityEnabled())
    {
      Serial.println("Radio activity is disabled.");
    }
    else if(millis()-last_packet < 1000)
    {
      Serial.print("RSSI: ");
      Serial.print(radio.getRSSI());
      Serial.print(", SNR: ");
      Serial.println(radio.getSNR());
      
    }
    else
    {
      Serial.print("Failsafe since (ms) ");
      Serial.println(millis()-last_packet);
    }
    delay(50);
  }
}

void serPrintInputs()
{
  while (true)
  {
    if(checkSerialQuit()) break;
    if(in_menu > 0) in_menu--;

    Serial.print("Throttle: ");
    Serial.print(thr_scaled);
    Serial.print(", Steering: ");
    Serial.print(steer_scaled);
    Serial.print(", Toggle: ");
    Serial.print(tog_scaled);
    Serial.print(", ToggleInput: ");
    Serial.print(tog_input);
    Serial.print(", Locked: ");
    Serial.print(system_locked ? 1 : 0);
    Serial.print(", InMenu: ");
    Serial.print(in_menu);
    Serial.print(", SteerEn: ");
    Serial.print(usrConf.steer_enabled);
    Serial.print(", HallEn: ");
    Serial.println(isHallActivityEnabled() ? 1 : 0);
    delay(50);
  }
}

void serPrintConf()
{
  Serial.println("**************************************");
  Serial.println("**          BREmote V2 TX           **");
  Serial.printf("**        MAC: %012llX         **\n", ESP.getEfuseMac());
  //Serial.print("**     HW Identifier: "); Serial.print(checkHWConfig()); Serial.println("       **");
  Serial.printf("**          SW Version: %-10d  **\n", SW_VERSION);
  Serial.printf("**  Compiled: %s %s  **\n", __DATE__, __TIME__);
  Serial.println("**************************************");

  // Read file from SPIFFS
  File file = SPIFFS.open(CONF_FILE_PATH, FILE_READ);
  if (!file) {
      Serial.println("Failed to open file for reading");
      return;
  }

  String encodedString = file.readString();
  Serial.println("Encoded Data Read: " + encodedString);
  file.close();

  printConfStruct(usrConf);
}

void checkCharger()
{
  uint8_t chg_err_cnt = 0;
  Serial.print("Checking if charging...");

  while(!exitChargeScreen)
  {
    webCfgLoop();
    ads.startADCReading(MUX_BY_CHANNEL[P_CHGSTAT],false);
    while(!ads.conversionComplete())
    {
      webCfgLoop();
      delay(1);
    }
    uint16_t chgstat = ads.getLastConversionResults();

    ads.startADCReading(MUX_BY_CHANNEL[P_UBAT_MEAS],false);
    while(!ads.conversionComplete())
    {
      webCfgLoop();
      delay(1);
    }
    uint16_t bat_volt = ads.getLastConversionResults();
    uint16_t c_bat_volt = (uint16_t)((float)bat_volt * usrConf.ubat_cal * 100.0);


    if(chgstat < 1000)
    {
      //Not charging
      Serial.println(" Done");
      serialOff = true;
      break;
    }
    else if(chgstat > 6000 && chgstat < 10000)
    {
      setBrightness(0x01);
      advanceChargeAnimation();
      uint8_t chglevel = map(c_bat_volt, 330, 420, 0, 10);
      displayHorzBargraph(7,chglevel);
      updateDisplay();
      checkSerial();
      webCfgLoop();
      delay(200);
    }
    else if(chgstat > 10000 && chgstat < 18000)
    {
      setBrightness(0x01);
      displayBuffer[1] = 0x1F;
      displayBuffer[4] = 0x1F;
      displayBuffer[2] = 0x3F;
      displayBuffer[3] = 0x3F;
      displayHorzBargraph(7,10);
      updateDisplay();
      checkSerial();
      webCfgLoop();
      delay(200);
    }
    else
    {
      chg_err_cnt++;
      Serial.print("Count: ");
      Serial.println(chg_err_cnt);
      Serial.print("Stat: ");
      Serial.println(chgstat);
      webCfgLoop();
      delay(10);
      if(chg_err_cnt > 10)
      {
        setBrightness(0x01);
        Serial.println("CHG ERR!");
        Serial.print("Stat: ");
        Serial.println(chgstat);
        int timeout = 0;
        while(timeout < 3)
        {
          timeout++;
          scroll3Digits(LET_E, LET_C, LET_H, 100);
        }
        exitChargeScreen = 1;
        serialOff = true;
      }
    }
  }
  displayHorzBargraph(7,0);
  setBrightness(0x0F);
}

void printConfStruct(const confStruct &data) {
    Serial.println("Configuration Struct Values:");
    Serial.print("Version: "); Serial.println(data.version);

    Serial.print("Radio Preset: "); Serial.println(data.radio_preset);
    Serial.print("RF Power: "); Serial.println(data.rf_power);

    Serial.print("Calibration OK: "); Serial.println(data.cal_ok);
    Serial.print("Calibration Offset: "); Serial.println(data.cal_offset);

    Serial.print("Throttle Idle: "); Serial.println(data.thr_idle);
    Serial.print("Throttle Pull: "); Serial.println(data.thr_pull);

    Serial.print("Toggle Left: "); Serial.println(data.tog_left);
    Serial.print("Toggle Middle: "); Serial.println(data.tog_mid);
    Serial.print("Toggle Right: "); Serial.println(data.tog_right);

    Serial.print("Toggle Deadzone: "); Serial.println(data.tog_deadzone);
    Serial.print("Toggle Difference: "); Serial.println(data.tog_diff);
    Serial.print("Toggle Block Time: "); Serial.println(data.tog_block_time);
    
    Serial.print("Trigger Unlock Timeout: "); Serial.println(data.trig_unlock_timeout);
    Serial.print("Lock Wait Time: "); Serial.println(data.lock_waittime);
    Serial.print("Gear Change Wait Time: "); Serial.println(data.gear_change_waittime);
    Serial.print("Gear Display Time: "); Serial.println(data.gear_display_time);
    Serial.print("Menu Timeout: "); Serial.println(data.menu_timeout);
    Serial.print("Error Delete Time: "); Serial.println(data.err_delete_time);

    Serial.print("No Lock: "); Serial.println(data.no_lock);
    Serial.print("No Gear: "); Serial.println(data.no_gear);
    Serial.print("Max Gears: "); Serial.println(data.max_gears);
    Serial.print("Start Gear: "); Serial.println(data.startgear);
    Serial.print("Steer Enabled: "); Serial.println(data.steer_enabled);

    Serial.print("Battery Calibration (Ubat Cal): "); Serial.println(data.ubat_cal, 15); // Floating-point precision

    Serial.print("Paired: "); Serial.println(data.paired);

    Serial.print("Own Address: ");
    for (int i = 0; i < 3; i++) {
        Serial.print(data.own_address[i], HEX);
        Serial.print(i < 2 ? ":" : "\n");
    }

    Serial.print("Destination Address: ");
    for (int i = 0; i < 3; i++) {
        Serial.print(data.dest_address[i], HEX);
        Serial.print(i < 2 ? ":" : "\n");
    }

    Serial.println("----------------------");
}
