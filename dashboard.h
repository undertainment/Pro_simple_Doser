#ifndef DASHBOARD_H
#define DASHBOARD_H

#include <Arduino.h>
#include "types.h"

class Dashboard {
public:
  static SystemStatus getStatus();
  static String renderStatusJSON();
  static String renderPumpJSON();
  static String renderScheduleJSON();
  static String renderLogJSON();
  static String renderApexJSON();

private:
  static String _jsonEscape(const String& s);
};

#endif
