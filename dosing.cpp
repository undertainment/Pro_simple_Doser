#include "dosing.h"
#include "pump.h"
#include "storage.h"
#include "logger.h"

DoseJob Dosing::_jobs[PUMP_COUNT];

void Dosing::init() {
  for (uint8_t i = 0; i < PUMP_COUNT; i++) {
    _jobs[i].pumpIndex = i;
    _jobs[i].volumeML  = 0;
    _jobs[i].state     = DoseState::Idle;
    _jobs[i].startTime = 0;
  }
  Logger::info(F("Dosing initialized"));
}

void Dosing::loop() {
  for (uint8_t i = 0; i < PUMP_COUNT; i++) {
    // reset Complete/Error back to Idle so dashboard shows current state
    if (_jobs[i].state == DoseState::Complete || _jobs[i].state == DoseState::Error) {
      _jobs[i].state = DoseState::Idle;
      continue;
    }
    if (_jobs[i].state != DoseState::Dosing) continue;

    PumpConfig* cfg = Pump::getConfig(_jobs[i].pumpIndex);
    if (!cfg || cfg->rateMLperMin <= 0) {
      _jobs[i].state = DoseState::Error;
      Pump::stop(_jobs[i].pumpIndex);
      continue;
    }

    unsigned long elapsed = (millis() - _jobs[i].startTime);
    float dosed = (elapsed / 60000.0f) * cfg->rateMLperMin;

    if (dosed >= _jobs[i].volumeML) {
      _completeJob(i);
    }
  }
}

bool Dosing::startDose(uint8_t pumpIndex, float volumeML) {
  if (pumpIndex >= PUMP_COUNT) return false;
  if (volumeML < PUMP_MIN_DOSE_ML || volumeML > PUMP_MAX_DOSE_ML) return false;
  if (_jobs[pumpIndex].state == DoseState::Dosing) return false;

  PumpConfig* cfg = Pump::getConfig(pumpIndex);
  if (!cfg || !cfg->active) return false;

  _jobs[pumpIndex].pumpIndex = pumpIndex;
  _jobs[pumpIndex].volumeML  = volumeML;
  _jobs[pumpIndex].state     = DoseState::Dosing;
  _jobs[pumpIndex].startTime = millis();

  Pump::start(pumpIndex);

  Logger::info(String(F("Dose started: pump=")) + pumpIndex +
               F(" vol=") + volumeML + F("mL"));
  return true;
}

void Dosing::cancelDose(uint8_t pumpIndex) {
  if (pumpIndex >= PUMP_COUNT) return;
  _jobs[pumpIndex].state = DoseState::Idle;
  Pump::stop(pumpIndex);

  Logger::info(String(F("Dose cancelled: pump=")) + pumpIndex);
}

DoseState Dosing::getState(uint8_t pumpIndex) {
  if (pumpIndex >= PUMP_COUNT) return DoseState::Error;
  return _jobs[pumpIndex].state;
}

const DoseJob* Dosing::getActiveJobs() {
  return _jobs;
}

uint8_t Dosing::activeJobCount() {
  uint8_t count = 0;
  for (uint8_t i = 0; i < PUMP_COUNT; i++) {
    if (_jobs[i].state == DoseState::Dosing) count++;
  }
  return count;
}

void Dosing::_completeJob(uint8_t i) {
  _jobs[i].state = DoseState::Complete;
  Pump::stop(i, true);
  Storage::markDirty();

  Logger::info(String(F("Dose complete: pump=")) + i +
               F(" vol=") + _jobs[i].volumeML + F("mL"));
}
