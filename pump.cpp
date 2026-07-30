#include "pump.h"
#include "logger.h"

PumpConfig Pump::_pumps[PUMP_COUNT];
bool Pump::_running[PUMP_COUNT];
unsigned long Pump::_startMillis[PUMP_COUNT];

static const uint8_t defaultPins[PUMP_COUNT] = {
  PIN_PUMP_1, PIN_PUMP_2, PIN_PUMP_3, PIN_PUMP_4
};

static const char* defaultNames[PUMP_COUNT] = {
  "Pump 1", "Pump 2", "Pump 3", "Pump 4"
};

void Pump::init() {
  for (uint8_t i = 0; i < PUMP_COUNT; i++) {
    _pumps[i].pin          = defaultPins[i];
    strncpy(_pumps[i].name, defaultNames[i], sizeof(_pumps[i].name) - 1);
    _pumps[i].name[sizeof(_pumps[i].name) - 1] = '\0';
    _pumps[i].rateMLperMin = PUMP_DEFAULT_RATE;
    _pumps[i].active       = true;
    _pumps[i].totalDosed   = 0;
    _pumps[i].runTimeSec   = 0;
    _pumps[i].capacity     = 5000.0f;
    _pumps[i].reservoirLevel = 100;
    _running[i]            = false;
    _startMillis[i]        = 0;

    pinMode(_pumps[i].pin, OUTPUT);
    digitalWrite(_pumps[i].pin, LOW);
  }
  Logger::info(F("Pumps initialized"));
}

void Pump::loop() {
  for (uint8_t i = 0; i < PUMP_COUNT; i++) {
    if (!_running[i]) continue;
  }
}

bool Pump::start(uint8_t index) {
  if (index >= PUMP_COUNT) return false;
  if (_running[index]) return true;

  digitalWrite(_pumps[index].pin, HIGH);
  _running[index] = true;
  _startMillis[index] = millis();
  return true;
}

void Pump::stop(uint8_t index, bool complete) {
  if (index >= PUMP_COUNT) return;
  if (!_running[index]) return;

  digitalWrite(_pumps[index].pin, LOW);
  _running[index] = false;

  unsigned long elapsed = (millis() - _startMillis[index]) / 1000;
  _pumps[index].runTimeSec += elapsed;

  if (complete && _pumps[index].rateMLperMin > 0) {
    float dosed = (elapsed / 60.0f) * _pumps[index].rateMLperMin;
    _pumps[index].totalDosed += dosed;
  }
}

bool Pump::isRunning(uint8_t index) {
  if (index >= PUMP_COUNT) return false;
  return _running[index];
}

PumpConfig* Pump::getConfig(uint8_t index) {
  if (index >= PUMP_COUNT) return nullptr;
  return &_pumps[index];
}

void Pump::setConfig(uint8_t index, const PumpConfig& cfg) {
  if (index >= PUMP_COUNT) return;
  _pumps[index] = cfg;
}

void Pump::resetTotal(uint8_t index) {
  if (index >= PUMP_COUNT) return;
  _pumps[index].totalDosed = 0;
  _pumps[index].runTimeSec = 0;
}
