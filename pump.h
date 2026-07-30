#ifndef PUMP_H
#define PUMP_H

#include <Arduino.h>
#include "types.h"
#include "config.h"

class Pump {
public:
  static void init();
  static void loop();

  static bool start(uint8_t index);
  static void stop(uint8_t index, bool complete = false);
  static bool isRunning(uint8_t index);

  static PumpConfig* getConfig(uint8_t index);
  static void setConfig(uint8_t index, const PumpConfig& cfg);
  static void resetTotal(uint8_t index);

  static uint8_t count() { return PUMP_COUNT; }

private:
  static PumpConfig _pumps[PUMP_COUNT];
  static bool _running[PUMP_COUNT];
  static unsigned long _startMillis[PUMP_COUNT];
};

#endif
