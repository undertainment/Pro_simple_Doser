#include "scheduler.h"
#include "dosing.h"
#include "logger.h"

Schedule Scheduler::_schedules[MAX_SCHEDULES];
uint8_t Scheduler::_count = 0;
unsigned long Scheduler::_lastCheck = 0;

void Scheduler::init() {
  _count = 0;
  _lastCheck = 0;
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
  _schedules[_count++] = sched;
  return true;
}

bool Scheduler::removeSchedule(uint8_t index) {
  if (index >= _count) return false;
  for (uint8_t i = index; i < _count - 1; i++) {
    _schedules[i] = _schedules[i + 1];
  }
  _count--;
  return true;
}

bool Scheduler::updateSchedule(uint8_t index, const Schedule& sched) {
  if (index >= _count) return false;
  _schedules[index] = sched;
  return true;
}

const Schedule* Scheduler::getSchedule(uint8_t index) {
  if (index >= _count) return nullptr;
  return &_schedules[index];
}

uint8_t Scheduler::scheduleCount() {
  return _count;
}

void Scheduler::_checkSchedules() {
  // time_t now = time(nullptr);      // requires NTP sync in real usage
  // struct tm* ti = localtime(&now);
  // uint8_t h = ti->tm_hour;
  // uint8_t m = ti->tm_min;
  // uint8_t wd = ti->tm_wday;        // 0=Sun

  for (uint8_t i = 0; i < _count; i++) {
    if (!_schedules[i].enabled) continue;
    // if (_schedules[i].hour == h && _schedules[i].minute == m) {
    //   if (_dayMatches(1 << wd)) {
    //     Dosing::startDose(_schedules[i].pumpIndex, _schedules[i].doseML);
    //   }
    // }
  }
}

bool Scheduler::_dayMatches(uint8_t dayMask) {
  return (_schedules[0].days & dayMask) != 0;
}
