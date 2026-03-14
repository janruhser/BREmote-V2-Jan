#include <WiFi.h>
#include <esp_now.h>
#include <TinyGPS++.h>
#include "../Common/EspNowGpsCommon.h"

#define RX_PIN 20
#define TX_PIN 21

TinyGPSPlus gps;
EspNowGpsPacket packet;
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }

  // Auto-baud detection
  const long bauds[] = {9600, 38400, 115200};
  bool found = false;
  for (int i = 0; i < 3; i++) {
    Serial.printf("Trying GPS at %ld baud...\n", bauds[i]);
    Serial1.begin(bauds[i], SERIAL_8N1, RX_PIN, TX_PIN);
    unsigned long start = millis();
    while (millis() - start < 2000) {
      if (Serial1.available() > 0) {
        Serial.printf("GPS found at %ld baud!\n", bauds[i]);
        found = true;
        break;
      }
      delay(10);
    }
    if (found) break;
    Serial1.end();
  }

  if (!found) {
    Serial.println("GPS not found, defaulting to 9600.");
    Serial1.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
  }
}

void loop() {
  while (Serial1.available() > 0) {
    if (gps.encode(Serial1.read())) {
      if (gps.location.isUpdated()) {
        packet.lat = (float)gps.location.lat();
        packet.lon = (float)gps.location.lng();
        packet.speed_kmph = (float)gps.speed.kmph();
        packet.course_deg = (float)gps.course.deg();
        packet.satellites = (uint8_t)gps.satellites.value();
        packet.hdop = (float)gps.hdop.hdop();
        packet.timestamp = millis();
        
        esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &packet, sizeof(packet));
        if (result != ESP_OK) {
          Serial.println("Error sending the data");
        }
      }
    }
  }
}
