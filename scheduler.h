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

private:
  static Schedule _schedules[MAX_SCHEDULES];
  static uint8_t _count;
  static unsigned long _lastCheck;

  static void _checkSchedules();
  static bool _dayMatches(uint8_t dayMask);
};

#endif
