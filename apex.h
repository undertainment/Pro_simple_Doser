#ifndef APEX_H
#define APEX_H

#include <Arduino.h>
#include "types.h"
#include "config.h"

class Apex {
public:
  static void init();
  static void loop();

  static void setConfig(uint8_t unit, const ApexConfig& cfg);
  static const ApexConfig& getConfig(uint8_t unit);

  static bool isConnected(uint8_t unit);
  static unsigned long lastUpdate(uint8_t unit);
  static const ApexProbe* getProbes(uint8_t unit);
  static uint8_t probeCount(uint8_t unit);

private:
  struct UnitState {
    ApexConfig   config;
    ApexProbe    probes[APEX_MAX_PROBES];
    uint8_t      probeCount;
    unsigned long lastUpdate;
    unsigned long lastPoll;
    bool         connected;
  };

  static UnitState _units[APEX_UNIT_COUNT];

  static void _poll(uint8_t unit);
};

#endif
