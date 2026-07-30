#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>

#define LOG_BUFFER_SIZE 64

class Logger {
public:
  static void init();
  static void info(const String& msg);
  static void warn(const String& msg);
  static void error(const String& msg);

  static const String* getLogs();
  static uint8_t logCount();

private:
  static String _buffer[LOG_BUFFER_SIZE];
  static uint8_t _head;
  static uint8_t _count;
  static void _log(const String& prefix, const String& msg);
};

#endif
