#include "storage.h"
#include "pump.h"
#include "scheduler.h"
#include "apex.h"
#include "logger.h"
#include <EEPROM.h>

#define MAGIC 0x5053   // "PS" magic bytes

struct StorageHeader {
  uint16_t magic;
  uint16_t version;
  uint16_t crc;
  uint16_t scheduleCount;
};

struct StorageBlock {
  StorageHeader header;
  PumpConfig    pumps[PUMP_COUNT];
  Schedule      schedules[MAX_SCHEDULES];
  ApexConfig    apex[APEX_UNIT_COUNT];
  int16_t       tzOffsetMin;
  uint32_t      apexPollMs[APEX_UNIT_COUNT];  // v5+
};

bool Storage::_dirty = false;

void Storage::init() {
  EEPROM.begin(EEPROM_SIZE);
  load();
  Logger::info(F("Storage initialized"));
}

void Storage::save() {
  StorageBlock block;
  memset(&block, 0, sizeof(block));
  block.header.magic    = MAGIC;
  block.header.version  = 5;
  block.header.scheduleCount = Scheduler::scheduleCount();

  for (uint8_t i = 0; i < PUMP_COUNT; i++) {
    PumpConfig* cfg = Pump::getConfig(i);
    if (cfg) block.pumps[i] = *cfg;
  }

  for (uint8_t i = 0; i < block.header.scheduleCount; i++) {
    const Schedule* s = Scheduler::getSchedule(i);
    if (s) block.schedules[i] = *s;
  }

  for (uint8_t u = 0; u < APEX_UNIT_COUNT; u++) {
    block.apex[u] = Apex::getConfig(u);
    block.apexPollMs[u] = Apex::pollIntervalMs(u);
  }

  block.tzOffsetMin = Scheduler::timeZoneOffsetMin();

  block.header.crc = _crc16((uint8_t*)&block.pumps, sizeof(block) - sizeof(StorageHeader));
  EEPROM.put(0, block);
  EEPROM.commit();
  _dirty = false;

  Logger::info(F("Settings saved"));
}

void Storage::load() {
  StorageBlock block;
  EEPROM.get(0, block);

  if (block.header.magic != MAGIC || block.header.version < 1) {
    Logger::warn(F("No saved settings, using defaults"));
    return;
  }

  uint16_t calc = _crc16((uint8_t*)&block.pumps, sizeof(block) - sizeof(StorageHeader));
  if (calc != block.header.crc) {
    Logger::error(F("Settings CRC mismatch, using defaults"));
    return;
  }

  for (uint8_t i = 0; i < PUMP_COUNT; i++) {
    Pump::setConfig(i, block.pumps[i]);
    Pump::setReservoirRemaining(i, block.pumps[i].capacity * block.pumps[i].reservoirLevel / 100.0f);
  }

  for (uint8_t i = 0; i < block.header.scheduleCount; i++) {
    Scheduler::addSchedule(block.schedules[i]);
  }

  if (block.header.version >= 3) {
    for (uint8_t u = 0; u < APEX_UNIT_COUNT; u++) {
      Apex::setConfig(u, block.apex[u]);
    }
  }

  if (block.header.version >= 4) {
    Scheduler::setTimeZoneOffsetMin(block.tzOffsetMin);
  }

  if (block.header.version >= 5) {
    for (uint8_t u = 0; u < APEX_UNIT_COUNT; u++) {
      if (block.apexPollMs[u] >= 10000) Apex::setPollIntervalMs(u, block.apexPollMs[u]);
    }
  }

  Logger::info(String(F("Settings loaded (")) + block.header.scheduleCount + F(" schedules)"));
}

void Storage::autoSave() {
  if (_dirty) save();
}

void Storage::resetDefaults() {
  EEPROM.write(0, 0);
  EEPROM.commit();
  Logger::info(F("Settings reset to defaults"));
}

void Storage::getPumpConfig(uint8_t index, PumpConfig& cfg) {
  PumpConfig* p = Pump::getConfig(index);
  if (p) cfg = *p;
}

void Storage::setPumpConfig(uint8_t index, const PumpConfig& cfg) {
  Pump::setConfig(index, cfg);
  _dirty = true;
}

void Storage::markDirty() {
  _dirty = true;
}

uint8_t Storage::getScheduleCount() {
  return Scheduler::scheduleCount();
}

void Storage::getSchedule(uint8_t index, Schedule& sched) {
  const Schedule* s = Scheduler::getSchedule(index);
  if (s) sched = *s;
}

void Storage::setSchedule(uint8_t index, const Schedule& sched) {
  Scheduler::updateSchedule(index, sched);
  _dirty = true;
}

void Storage::setScheduleCount(uint8_t count) {
  // used after loading only
}

void Storage::getApexConfig(uint8_t unit, ApexConfig& cfg) {
  cfg = Apex::getConfig(unit);
}

void Storage::setApexConfig(uint8_t unit, const ApexConfig& cfg) {
  Apex::setConfig(unit, cfg);
  _dirty = true;
}

uint16_t Storage::_crc16(const uint8_t* data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t j = 0; j < 8; j++) {
      if (crc & 1) crc = (crc >> 1) ^ 0xA001;
      else crc >>= 1;
    }
  }
  return crc;
}
