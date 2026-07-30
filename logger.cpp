#include "logger.h"

String Logger::_buffer[LOG_BUFFER_SIZE];
uint8_t Logger::_head = 0;
uint8_t Logger::_count = 0;

void Logger::init() {
  for (uint8_t i = 0; i < LOG_BUFFER_SIZE; i++) {
    _buffer[i] = String();
  }
}

void Logger::info(const String& msg) {
  _log(F("[I]"), msg);
}

void Logger::warn(const String& msg) {
  _log(F("[W]"), msg);
}

void Logger::error(const String& msg) {
  _log(F("[E]"), msg);
}

const String* Logger::getLogs() {
  return _buffer;
}

uint8_t Logger::logCount() {
  return _count;
}

void Logger::_log(const String& prefix, const String& msg) {
  unsigned long t = millis();
  char buf[48];
  snprintf(buf, sizeof(buf), "[%lu.%03lu] %s %s",
           t / 1000, t % 1000, prefix.c_str(), msg.c_str());

  Serial.println(buf);
  _buffer[_head] = String(buf);
  _head = (_head + 1) % LOG_BUFFER_SIZE;
  if (_count < LOG_BUFFER_SIZE) _count++;
}
