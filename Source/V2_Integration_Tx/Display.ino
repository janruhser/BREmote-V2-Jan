void startupDisplay()
{
  Serial.print("Starting Display...");
  if(!beginDisplay())
  {
    Serial.println(" Failed");
    while(1) delay(100);
  }

  clearDigitBuffer();
  clearDisplayBuffer();
  clearDisplay();
  initDisplay();
  Serial.println(" Done");
}

bool beginDisplay()
{
  Wire.beginTransmission(DISPLAY_ADDRESS);
  return (0 == Wire.endTransmission());
}

void clearDigitBuffer()
{
  for(int i = 0; i < 6; i++)
  {
    digitBuffer[i] = 0x00;
  }
}

void displayDigits(uint8_t dig1, uint8_t dig2)
{
  //Delete whole number field
  for(int i = 1; i < 7; i++)
  {
    displayBuffer[i] &= 0xFF00;
  }

  uint8_t digitBuffer[7];

  for(int i = 0; i < 7; i++)
  {
    digitBuffer[i] = 0;
  }

  digitBuffer[0] = num0[dig1][0];
  digitBuffer[1] = num0[dig1][1];
  digitBuffer[2] = num0[dig1][2];

  digitBuffer[4] = num0[dig2][0];
  digitBuffer[5] = num0[dig2][1];
  digitBuffer[6] = num0[dig2][2];


  for(int j = 5; j >= 0; j--)
  {
    for(int i = 0; i < 7; i++)
    {
      displayBuffer[j] |= ((digitBuffer[i]>>5-j)&0x01)<<i;
    }
  }
}

void initDisplay()
{
  Wire.beginTransmission(DISPLAY_ADDRESS);
  //System Oscillator on
  Wire.write(0x21);
  Wire.endTransmission();

  Wire.beginTransmission(DISPLAY_ADDRESS);
  //On, no blinking
  Wire.write(0x81);
  Wire.endTransmission();
  
  setBrightness(0x0F);
}

void setBrightness(uint8_t level)
{
  //Set brightness x00..x0F
  if(level > 0x0F) level = 0x0F;
  Wire.beginTransmission(DISPLAY_ADDRESS);
  //Full brightness
  Wire.write(0xE0 | level);
  Wire.endTransmission();
}

void updateDisplay()
{
  //This is where the mapping takes place
  //displayBuffer keeps the desired matrix config,
  //but the connection is not 1:1
  //See the row_mapper[] and col_mapper[] arrays

  uint8_t sendBuffer[15];

  for(int i=0; i < 15; i++)
  {
    sendBuffer[i] = 0x00;
  }

  //Go through the 8 columns
  for(int j = 0; j < 7; j++)
  {
    //And map the 8 bits + 2 bits (in total 10)
    for( int k=0; k<8; k++)
    {
      sendBuffer[2*j+1] |= ((displayBuffer[col_mapper[j]]>>row_mapper[k])&0x01)<<k;
    }
    for( int k=0; k<2; k++)
    {
      sendBuffer[2*j+2] |= ((displayBuffer[col_mapper[j]]>>row_mapper[k+8])&0x01)<<k;
    }
  }

  Wire.beginTransmission(DISPLAY_ADDRESS);
  Wire.write(sendBuffer,17);
  Wire.endTransmission();
}

void clearDisplayBuffer()
{
  for(int i = 0; i < 8; i++)
  {
    displayBuffer[i] = 0x0000;
  }
}

void clearDisplay()
{  
  Wire.beginTransmission(DISPLAY_ADDRESS);
  uint8_t buffer[17];
  for (uint8_t i = 0; i < 17; i++) {
    buffer[i] = 0x00;
  }
  Wire.write(buffer,17);
  Wire.endTransmission();  
}

void displayVertBargraph(uint8_t location, uint8_t length)
{
  if(location > 9) location = 9;
  if(length > 7) length = 7;
  
  int i = 0;

  for(; i < length; i++)
  {
    displayBuffer[7-i] |= 0x01<<location; 
  }
  for(; i < 7; i++)
  {
    displayBuffer[7-i] &= ~(0x01<<location); 
  }
}

void displayHorzBargraph(int location, int length)
{
  if(location > 7) location = 7;
  if(length > 7) length = 7;
      
  for(int i = 0; i < length; i++)
  {
    displayBuffer[location] |= 0x01<<i;
  }
  for(int i = length; i < 7; i++)
  {
    displayBuffer[location] &= ~(0x01<<i);
  }
}

void showNewGear()
{
  displayDigits(LET_L, gear);
  updateDisplay();
}

void displayError(int err)
{
  displayDigits(LET_E, err);
  updateDisplay();
}

void scroll3Digits(uint8_t dig1, uint8_t dig2, uint8_t dig3, int del)
{
  uint8_t digitBuffer[14];

  for(int i = 0; i < 14; i++)
  {
    digitBuffer[i] = 0;
  }

  digitBuffer[0] = num0[dig1][0];
  digitBuffer[1] = num0[dig1][1];
  digitBuffer[2] = num0[dig1][2];

  digitBuffer[4] = num0[dig2][0];
  digitBuffer[5] = num0[dig2][1];
  digitBuffer[6] = num0[dig2][2];

  digitBuffer[8] = num0[dig3][0];
  digitBuffer[9] = num0[dig3][1];
  digitBuffer[10] = num0[dig3][2];

  digitBuffer[12] = 0x04;

  for(int k = 0; k < 14; k++)
  {
    //Delete whole number field
    for(int i = 1; i < 7; i++)
    {
      displayBuffer[i] &= 0xFF00;
    }

    for(int j = 5; j >= 0; j--)
    {
      for(int i = 0; i < 7; i++)
      {
        displayBuffer[j] |= ((digitBuffer[(i+k)%14]>>5-j)&0x01)<<i;
      }
    }
    updateDisplay();
    delay(del);
    checkSerial();
  }
}

void scroll4Digits(uint8_t dig1, uint8_t dig2, uint8_t dig3, uint8_t dig4, int del)
{
  uint8_t digitBuffer[18];

  for(int i = 0; i < 18; i++)
  {
    digitBuffer[i] = 0;
  }

  digitBuffer[0] = num0[dig1][0];
  digitBuffer[1] = num0[dig1][1];
  digitBuffer[2] = num0[dig1][2];

  digitBuffer[4] = num0[dig2][0];
  digitBuffer[5] = num0[dig2][1];
  digitBuffer[6] = num0[dig2][2];

  digitBuffer[8] = num0[dig3][0];
  digitBuffer[9] = num0[dig3][1];
  digitBuffer[10] = num0[dig3][2];

  digitBuffer[12] = num0[dig4][0];
  digitBuffer[13] = num0[dig4][1];
  digitBuffer[14] = num0[dig4][2];

  digitBuffer[16] = 0x04;

  for(int k = 0; k < 18; k++)
  {
    //Delete whole number field
    for(int i = 1; i < 7; i++)
    {
      displayBuffer[i] &= 0xFF00;
    }

    for(int j = 5; j >= 0; j--)
    {
      for(int i = 0; i < 7; i++)
      {
        displayBuffer[j] |= ((digitBuffer[(i+k)%18]>>5-j)&0x01)<<i;
      }
    }
    updateDisplay();
    delay(del);
    checkSerial();
  }
}

void bootAnimation()
{
  scroll4Digits(LET_B, 0, 0, LET_T, 100);
  scroll4Digits(LET_B, 0, 0, LET_T, 100);
  displayDigits(LET_V,LET_I);
  updateDisplay();
  delay(500);

  uint8_t temp_volt = uint8_t(int_bat_volt*10);

  displayDigits(temp_volt/10,temp_volt-10*(temp_volt/10));
  updateDisplay();
  delay(1000);
}

uint8_t arrowPos = 0;
void advanceArrow()
{
  //Delete whole number field
  for(int i = 1; i < 7; i++)
  {
    displayBuffer[i] &= 0xFF00;
  }

  arrowPos ++;
  if(arrowPos >=2) arrowPos = 0;

  displayBuffer[0+arrowPos] |= 0x3E;
  displayBuffer[1+arrowPos] |= 0x3E;
  displayBuffer[2+arrowPos] |= 0x1C;
  displayBuffer[3+arrowPos] |= 0x1C;
  displayBuffer[4+arrowPos] |= 0x08;

  updateDisplay();
}

uint8_t chargeAnimationPos = 0;
void advanceChargeAnimation()
{
  //Delete whole number field
  for(int i = 1; i < 7; i++)
  {
    displayBuffer[i] &= 0xFF00;
  }

  displayBuffer[1] = 0x1F;
  displayBuffer[4] = 0x1F;
  
  chargeAnimationPos++;
  if(chargeAnimationPos > 4) chargeAnimationPos = 0;

  if(chargeAnimationPos == 0)
  {
    displayBuffer[2] = 0x21;
    displayBuffer[3] = 0x21;
  }
  if(chargeAnimationPos == 1)
  {
    displayBuffer[2] = 0x23;
    displayBuffer[3] = 0x23;
  }
  if(chargeAnimationPos == 2)
  {
    displayBuffer[2] = 0x27;
    displayBuffer[3] = 0x27;
  }
  if(chargeAnimationPos == 3)
  {
    displayBuffer[2] = 0x2F;
    displayBuffer[3] = 0x2F;
  }
  if(chargeAnimationPos == 4)
  {
    displayBuffer[2] = 0x3F;
    displayBuffer[3] = 0x3F;
  }
}

void displayLock()
{
  //Delete whole number field
  for(int i = 1; i < 7; i++)
  {
    displayBuffer[i] &= 0xFF00;
  }

  displayBuffer[5] |= 0x7F;
  displayBuffer[4] |= 0x41;
  displayBuffer[3] |= 0x7F;
  displayBuffer[2] |= 0x22;
  displayBuffer[1] |= 0x1C;
  updateDisplay();
}

#define ANIMATION_DELAY 80
void unlockAnimation()
{
  for(int i = 1; i < 7; i++)
  {
    displayBuffer[i] &= 0xFF00;
  }

  displayBuffer[0] |= 0x3E;
  displayBuffer[1] |= 0x3E;
  displayBuffer[2] |= 0x1C;
  displayBuffer[3] |= 0x1C;
  displayBuffer[4] |= 0x08;
  updateDisplay();

  delay(ANIMATION_DELAY);

  displayBuffer[1] |= 0x3E;
  displayBuffer[2] |= 0x3E;
  displayBuffer[3] |= 0x1C;
  displayBuffer[4] |= 0x1C;
  displayBuffer[5] |= 0x08;
  updateDisplay();

  delay(ANIMATION_DELAY);

  displayBuffer[2] |= 0x3E;
  displayBuffer[3] |= 0x3E;
  displayBuffer[4] |= 0x1C;
  displayBuffer[5] |= 0x1C;
  updateDisplay();

  delay(ANIMATION_DELAY);

  displayBuffer[3] |= 0x3E;
  displayBuffer[4] |= 0x3E;
  displayBuffer[5] |= 0x1C;
  updateDisplay();

  delay(ANIMATION_DELAY);

  displayBuffer[4] |= 0x3E;
  displayBuffer[5] |= 0x3E;
  updateDisplay();

  delay(ANIMATION_DELAY);
  
  arrowPos = 0;
}

void updateBargraphs(void *parameter)
{
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(200);

  while (1) 
  {
    if(millis()-last_packet < 1000)
    {
      if(telemetry.link_quality)
      {
        blink_bargraphs ^= 1;
        if(telemetry.foil_temp != 0xFF)
        {
          last_known_temp_graph = map( telemetry.foil_temp, 0, 80, 0, 7);
          displayVertBargraph(8,last_known_temp_graph);
        }
        else
        {
          if(blink_bargraphs)
          {
            displayVertBargraph(8,last_known_temp_graph);
          }
          else
          {
            displayVertBargraph(8,0);
          }
        }

        if(telemetry.foil_bat != 0xFF)
        {
          last_known_bat_graph = map( telemetry.foil_bat, 0, 100, 0, 7);
          displayVertBargraph(9,last_known_bat_graph);
        }
        else
        {
          if(blink_bargraphs)
          {
            displayVertBargraph(9,last_known_bat_graph);
          }
          else
          {
            displayVertBargraph(9,0);
          }
        }
        sq_graph = map( telemetry.link_quality + local_link_quality, 0, 20, 0, 7);
      }
    }
    else
    {
      if(sq_graph)
      {
        sq_graph = 0;
      }
      else
      {
        sq_graph = 1;
      }
    }
    displayHorzBargraph(7, sq_graph);
    updateDisplay();
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}