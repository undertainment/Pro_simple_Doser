#ifndef APEX_H
#define APEX_H

#include <Arduino.h>
#include "types.h"
#include "config.h"

class Apex {
public:
  static void init();
  static void loop();

  static void setConfig(const ApexConfig& cfg);
  static const ApexConfig& getConfig();

  static bool isConnected();
  static unsigned long lastUpdate();
  static const ApexProbe* getProbes();
  static uint8_t probeCount();

private:
  static ApexConfig _config;
  static ApexProbe _probes[APEX_MAX_PROBES];
  static uint8_t _probeCount;
  static unsigned long _lastUpdate;
  static unsigned long _lastPoll;
  static bool _connected;

  static void _poll();
};

#endif
