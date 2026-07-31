#include "pump.h"
#include "logger.h"

PumpConfig Pump::_pumps[PUMP_COUNT];
bool Pump::_running[PUMP_COUNT];
unsigned long Pump::_startMillis[PUMP_COUNT];
float Pump::_reservoirRemaining[PUMP_COUNT];

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
    _reservoirRemaining[i] = _pumps[i].capacity;
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

    _reservoirRemaining[index] -= dosed;
    if (_reservoirRemaining[index] < 0) _reservoirRemaining[index] = 0;

    if (_pumps[index].capacity > 0) {
      _pumps[index].reservoirLevel =
          (uint8_t)constrain((_reservoirRemaining[index] / _pumps[index].capacity) * 100.0f, 0, 100);
    }
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
  float ratio = 1.0f;
  if (_pumps[index].capacity > 0) {
    ratio = _reservoirRemaining[index] / _pumps[index].capacity;
  }
  _pumps[index] = cfg;
  _reservoirRemaining[index] = cfg.capacity * ratio;
  if (_reservoirRemaining[index] < 0) _reservoirRemaining[index] = 0;
  if (_reservoirRemaining[index] > cfg.capacity) _reservoirRemaining[index] = cfg.capacity;
}

float Pump::reservoirRemaining(uint8_t index) {
  if (index >= PUMP_COUNT) return 0;
  return _reservoirRemaining[index];
}

void Pump::setReservoirRemaining(uint8_t index, float ml) {
  if (index >= PUMP_COUNT) return;
  if (ml < 0) ml = 0;
  _reservoirRemaining[index] = ml;
  if (_pumps[index].capacity > 0) {
    _pumps[index].reservoirLevel =
        (uint8_t)constrain((ml / _pumps[index].capacity) * 100.0f, 0, 100);
  }
}

void Pump::refillAll() {
  for (uint8_t i = 0; i < PUMP_COUNT; i++) {
    _reservoirRemaining[i] = _pumps[i].capacity;
    _pumps[i].reservoirLevel = 100;
  }
  Logger::info(F("All reservoirs refilled"));
}

void Pump::resetTotal(uint8_t index) {
  if (index >= PUMP_COUNT) return;
  _pumps[index].totalDosed = 0;
  _pumps[index].runTimeSec = 0;
}
