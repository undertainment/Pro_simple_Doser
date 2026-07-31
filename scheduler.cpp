#include "scheduler.h"
#include "dosing.h"
#include "logger.h"
#include "storage.h"
#include <time.h>

Schedule Scheduler::_schedules[MAX_SCHEDULES];
uint8_t Scheduler::_count = 0;
unsigned long Scheduler::_lastCheck = 0;
unsigned long Scheduler::_lastNtpRetry = 0;
unsigned long Scheduler::_lastTimeLog = 0;
int16_t Scheduler::_tzOffsetMin = DEFAULT_TZ_OFFSET_MIN;
uint32_t Scheduler::_lastFiredEpochMinute[MAX_SCHEDULES];
bool Scheduler::_clockSynced = false;

void Scheduler::init() {
  _count = 0;
  _lastCheck = 0;
  _lastNtpRetry = 0;
  _lastTimeLog = 0;
  _clockSynced = false;
  for (uint8_t i = 0; i < MAX_SCHEDULES; i++) _lastFiredEpochMinute[i] = 0;
  syncTime();
  Logger::info(F("Scheduler initialized"));
}

void Scheduler::loop() {
  unsigned long now = millis();
  if (now - _lastCheck < SCHEDULE_CHECK_MS) return;
  _lastCheck = now;
  _checkSchedules();
}

bool Scheduler::addSchedule(const Schedule& sched) {
  if (_count >= MAX_SCHEDULES) return false;
  _schedules[_count] = sched;
  _lastFiredEpochMinute[_count] = 0;
  _count++;
  return true;
}

bool Scheduler::removeSchedule(uint8_t index) {
  if (index >= _count) return false;
  for (uint8_t i = index; i < _count - 1; i++) {
    _schedules[i] = _schedules[i + 1];
    _lastFiredEpochMinute[i] = _lastFiredEpochMinute[i + 1];
  }
  _count--;
  return true;
}

bool Scheduler::updateSchedule(uint8_t index, const Schedule& sched) {
  if (index >= _count) return false;
  _schedules[index] = sched;
  _lastFiredEpochMinute[index] = 0;
  return true;
}

const Schedule* Scheduler::getSchedule(uint8_t index) {
  if (index >= _count) return nullptr;
  return &_schedules[index];
}

uint8_t Scheduler::scheduleCount() {
  return _count;
}

int16_t Scheduler::timeZoneOffsetMin() {
  return _tzOffsetMin;
}

bool Scheduler::clockSynced() {
  return _clockSynced;
}

void Scheduler::setTimeZoneOffsetMin(int16_t offsetMin) {
  _tzOffsetMin = offsetMin;
  syncTime();
  Logger::info(String(F("Timezone set to offset ")) + offsetMin + F(" min"));
}

void Scheduler::syncTime() {
  configTime(_tzOffsetMin * 60L, 0, NTP_SERVER);
}

void Scheduler::_checkSchedules() {
  time_t now = time(nullptr);

  if (now < 1000000000) {
    _clockSynced = false;
    if (millis() - _lastNtpRetry > NTP_RETRY_MS) {
      _lastNtpRetry = millis();
      syncTime();
      Logger::warn(F("Clock not synced, retrying NTP..."));
    }
    return;
  }

  if (!_clockSynced) {
    _clockSynced = true;
    Logger::info(F("Clock synced via NTP"));
  }

  struct tm* ti = localtime(&now);
  uint8_t h  = ti->tm_hour;
  uint8_t m  = ti->tm_min;
  uint8_t wd = ti->tm_wday;
  uint32_t epochMinute = (uint32_t)(now / 60);

  if (millis() - _lastTimeLog > 300000) {
    _lastTimeLog = millis();
    char buf[24];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d day=%d", h, m, ti->tm_sec, wd);
    Logger::info(String(F("Clock: ")) + buf + F(" sched=") + _count);
  }

  for (uint8_t i = 0; i < _count; i++) {
    const Schedule& s = _schedules[i];
    if (!s.enabled) continue;
    if (s.hour != h || s.minute != m) continue;
    if (_lastFiredEpochMinute[i] == epochMinute) continue;
    if (!_dayMatches(s.days, wd)) continue;

    _lastFiredEpochMinute[i] = epochMinute;
    Logger::info(String(F("Schedule fired: pump=")) + s.pumpIndex +
                 F(" vol=") + s.doseML + F("mL"));
    if (!Dosing::startDose(s.pumpIndex, s.doseML)) {
      Logger::error(String(F("Schedule fire rejected: pump=")) + s.pumpIndex +
                    F(" inactive/busy/invalid"));
    }
  }
}

bool Scheduler::_dayMatches(uint8_t dayMask, uint8_t wday) {
  return (dayMask & (1 << wday)) != 0;
}
