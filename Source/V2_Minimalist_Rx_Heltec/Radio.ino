void radioErrorHalt(int type)
{
  while(1) {
    delay(1000); // Wait loop
  }
}

void radioInitSuccess()
{
  // No extra init needed on RX side
}

void startupRadio()
{
  initRadioHardware();
}

void ICACHE_RAM_ATTR packetReceived(void) 
{
  if(rxIsrState && !rfInterrupt)
  {
    rfInterrupt = true;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(triggerReceiveSemaphore, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  }
  else
  {
    rfInterrupt = true;
  }
}

// Function for waiting node to pair
bool waitForPairing() 
{
  usrConf.paired = false;
  
  uint8_t responsePacket[8];
  unsigned long startTime = millis();

  while (millis() - startTime < PAIRING_TIMEOUT) 
  {
    radio.implicitHeader(5);
    hal_radio_switch_mode(false); // RX
    radio.startReceive();
    rxprintln("Waiting for pairing packet...");
    uint8_t buffer[15];
    
    while(!rfInterrupt && millis() - startTime < PAIRING_TIMEOUT) delay(10);
    delay(10);
    if (rfInterrupt && radio.readData(buffer, 15) == RADIOLIB_ERR_NONE) 
    {
      rfInterrupt = false;
      rxprintln("Received response");

      if (buffer[0] == 0xAB) 
      {
        rxprintln("1st byte matches");
        uint8_t receivedCRC = buffer[4];
        uint8_t calculatedCRC = esp_crc8(buffer, 4);
        
        if (receivedCRC == calculatedCRC) 
        {
          rxprintln("CRC ok");
          uint8_t temp_addr[3];
          memcpy(temp_addr, buffer + 1, 3);
          
          responsePacket[0] = 0xBA;
          memcpy(responsePacket + 1, temp_addr, 3);
          memcpy(responsePacket + 4, usrConf.own_address, 3);
          responsePacket[7] = esp_crc8(responsePacket, 7);
          
          delay(100);
          rxprintln("Sending response: ");
          
          radio.implicitHeader(8);
          hal_radio_switch_mode(true); // TX
          radio.startTransmit(responsePacket, 8);
          delay(10);
          hal_radio_switch_mode(false); // RX
          radio.startReceive();
          rfInterrupt = false;

          while (millis() - startTime < PAIRING_TIMEOUT) 
          {
            while(!rfInterrupt && millis() - startTime < PAIRING_TIMEOUT) delay(10);
            delay(10);
            if (rfInterrupt && radio.readData(buffer, 15) == RADIOLIB_ERR_NONE) 
            {
              rfInterrupt = false;
              rxprintln("Received response");
              if (buffer[0] == 0xAC && memcmp(buffer + 1, usrConf.own_address, 3) == 0) 
              {
                rxprintln("Address matches");
                receivedCRC = buffer[7];
                calculatedCRC = esp_crc8(buffer, 7);
                
                if (receivedCRC == calculatedCRC) 
                {
                  rxprintln("CRC ok, pairing success");
                  memcpy(usrConf.dest_address, buffer + 4, 3);
                  usrConf.paired = true;
                  saveConfToSPIFFS(usrConf);
                  return true;
                }
              }
            }
            else rfInterrupt = false;
          }
        }
      }
    }
    else rfInterrupt = false;
  }  
  return false;
}

void triggeredReceive(void *parameter) {
  while (1)
  {
    // Wait for semaphore from ISR
    if (xSemaphoreTake(triggerReceiveSemaphore, portMAX_DELAY) == pdTRUE) 
    {
      uint8_t rcvArray[6];
      if (radio.readData(rcvArray, 6) == RADIOLIB_ERR_NONE) 
      {
        rxprint("Received packet: ");

        if (memcmp(rcvArray, usrConf.own_address, 3) == 0) 
        {
          rxprintln("Address matches");
          
          if (rcvArray[5] == esp_crc8(rcvArray, 5)) 
          {
            last_packet = millis();
#ifdef WIFI_ENABLED
            webCfgNotifyRxConnected();
#endif
            rxprintln("CRC ok");

            thr_received = rcvArray[3];
            
            // Single ESC output mapped directly from throttle
            uint16_t pwm_val = map(thr_received, 0, 255, usrConf.PWM0_min, usrConf.PWM0_max);
            hal_esc_write(pwm_val);
            PWM_active = true;

            telemetry.link_quality = getLinkQuality(radio.getRSSI(), radio.getSNR());

            rxprintln("Sending response");

            uint8_t sendArray[6];
            memcpy(sendArray, usrConf.dest_address, 3);
            uint8_t* ptr = (uint8_t*)&telemetry;
            sendArray[3] = telemetry_index;
            sendArray[4] = ptr[telemetry_index];

            telemetry_index++;
            if(telemetry_index >= sizeof(TelemetryPacket))
            {
              telemetry_index = 0;
            }

            sendArray[5] = esp_crc8(sendArray, 5);

            vTaskDelay(pdMS_TO_TICKS(10));
            radio.implicitHeader(6);
            
            hal_radio_switch_mode(true); // TX
            radio.startTransmit(sendArray, 6);
            vTaskDelay(pdMS_TO_TICKS(10));
            
            radio.implicitHeader(6);
            hal_radio_switch_mode(false); // RX
            radio.startReceive();
          }
        }
      }
      else
      {
        rxprintln("Rx err");
      }
      hal_radio_switch_mode(false); // RX
      radio.startReceive();
      rfInterrupt = false;
    }
  }
}
