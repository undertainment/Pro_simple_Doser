#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <Arduino.h>
#include "types.h"
#include "config.h"

class Scheduler {
public:
  static void init();
  static void loop();

  static bool addSchedule(const Schedule& sched);
  static bool removeSchedule(uint8_t index);
  static bool updateSchedule(uint8_t index, const Schedule& sched);
  static const Schedule* getSchedule(uint8_t index);
  static uint8_t scheduleCount();

  static int16_t timeZoneOffsetMin();
  static void setTimeZoneOffsetMin(int16_t offsetMin);
  static void syncTime();
  static bool clockSynced();

private:
  static Schedule _schedules[MAX_SCHEDULES];
  static uint8_t _count;
  static unsigned long _lastCheck;
  static unsigned long _lastNtpRetry;
  static unsigned long _lastTimeLog;
  static int16_t _tzOffsetMin;
  static uint32_t _lastFiredEpochMinute[MAX_SCHEDULES];
  static bool _clockSynced;

  static void _checkSchedules();
  static bool _dayMatches(uint8_t dayMask, uint8_t wday);
};

#endif
