#ifndef TYPES_H
#define TYPES_H

#include <Arduino.h>
#include "config.h"

struct PumpConfig {
  uint8_t  pin;
  char     name[24];
  float    rateMLperMin;
  bool     active;
  float    totalDosed;
  uint32_t runTimeSec;
};

struct Schedule {
  uint8_t  pumpIndex;
  uint8_t  hour;
  uint8_t  minute;
  float    doseML;
  bool     enabled;
  uint8_t  days;       // bitmask: Sun=1, Mon=2, Tue=4, Wed=8, Thu=16, Fri=32, Sat=64
};

struct SystemStatus {
  float    uptimeHours;
  uint32_t totalDoses;
  float    totalVolume;
  uint32_t freeHeap;
  int8_t   rssi;
  bool     wifiConnected;
  char     ip[16];
};

enum class DoseState : uint8_t {
  Idle,
  Priming,
  Dosing,
  Complete,
  Error
};

#endif
