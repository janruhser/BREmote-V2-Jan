#ifndef ESP_NOW_GPS_COMMON_H
#define ESP_NOW_GPS_COMMON_H

#include <Arduino.h>

struct __attribute__((packed)) EspNowGpsPacket {
    uint32_t magic = 0x47505321; // "GPS!"
    float lat;
    float lon;
    float speed_kmph;
    float course_deg;
    float hdop;
    uint8_t satellites;
    uint32_t timestamp;
};

#endif
