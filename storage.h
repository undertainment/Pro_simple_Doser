#ifndef STORAGE_H
#define STORAGE_H

#include <Arduino.h>
#include "types.h"
#include "config.h"

class Storage {
public:
  static void init();
  static void save();
  static void load();
  static void autoSave();
  static void resetDefaults();

  static void getPumpConfig(uint8_t index, PumpConfig& cfg);
  static void setPumpConfig(uint8_t index, const PumpConfig& cfg);
  static uint8_t getScheduleCount();
  static void getSchedule(uint8_t index, Schedule& sched);
  static void setSchedule(uint8_t index, const Schedule& sched);
  static void setScheduleCount(uint8_t count);

  static void getApexConfig(uint8_t unit, ApexConfig& cfg);
  static void setApexConfig(uint8_t unit, const ApexConfig& cfg);

private:
  static bool _dirty;
  static uint16_t _crc16(const uint8_t* data, size_t len);
};

#endif
