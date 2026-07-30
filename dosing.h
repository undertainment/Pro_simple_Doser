#ifndef DOSING_H
#define DOSING_H

#include <Arduino.h>
#include "types.h"
#include "config.h"

struct DoseJob {
  uint8_t  pumpIndex;
  float    volumeML;
  DoseState state;
  unsigned long startTime;
};

class Dosing {
public:
  static void init();
  static void loop();

  static bool startDose(uint8_t pumpIndex, float volumeML);
  static void cancelDose(uint8_t pumpIndex);
  static DoseState getState(uint8_t pumpIndex);

  static const DoseJob* getActiveJobs();
  static uint8_t activeJobCount();

private:
  static DoseJob _jobs[PUMP_COUNT];
  static void _completeJob(uint8_t i);
};

#endif
