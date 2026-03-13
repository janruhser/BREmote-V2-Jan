// Minimalist RX Utility functions
// Replaces functions previously relying on I/O expander LEDs

void blinkErr(int num, uint8_t pin) {
  for(int i = 0; i < num; i++) {
    Serial.printf("ERR_BLINK: %d/%d\n", i+1, num);
    delay(200);
    delay(200);
  }
  delay(500);
}

void blinkBind(int num) {
  for(int i = 0; i < num; i++) {
    Serial.printf("BIND_BLINK: %d/%d\n", i+1, num);
    delay(50);
    delay(50);
  }
}

uint8_t getUbatPercent(float pack_voltage)
{
  if(millis() - percent_last_thr_change > 5000)
  {
    uint8_t thr_state = (thr_received < 10);

    if( thr_state != percent_last_thr)
    {
      percent_last_thr = thr_state;
      percent_last_thr_change = millis();
      return percent_last_val;
    }

    uint16_t upackvolt = 0;

    // Minimalist build uses only noload_offset (from VESC) or straight mapping
    if(thr_state)
    {
      float fpackvolt = (((pack_voltage / usrConf.foil_num_cells)-2.0-noload_offset) * 100.0);
      if(fpackvolt > 0) upackvolt = (uint16_t)fpackvolt;
      else upackvolt = 0;
    }
    else
    {
      upackvolt = (uint16_t)(((pack_voltage / usrConf.foil_num_cells)-2.0) * 100.0);
    }

    uint8_t percent_rem = 100;
    while(bc_arr[100-percent_rem] > upackvolt && percent_rem > 0) percent_rem--;

    if(percent_rem < 100 && percent_rem > 0)
    {
      if((upackvolt-bc_arr[100-percent_rem]) > (bc_arr[100-percent_rem-1]-upackvolt))
      {
        percent_rem += 1;
      }
    }

    percent_last_val = percent_rem;
    return percent_rem;
  }
  else
  {
    return percent_last_val;
  }
}
