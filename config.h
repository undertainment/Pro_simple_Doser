#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// =====================================================
// WiFi — include wifi_config.h (not tracked by git)
//       Copy wifi_config.example.h → wifi_config.h and edit
// =====================================================
#include "wifi_config.h"
#define WIFI_TIMEOUT_MS    15000

// =====================================================
// Pin Assignments — change to match your wiring
// =====================================================
#define PIN_PUMP_1         32
#define PIN_PUMP_2         33
#define PIN_PUMP_3         25
#define PIN_PUMP_4         26

// =====================================================
// Pump Defaults
// =====================================================
#define PUMP_COUNT         4
#define PUMP_DEFAULT_RATE  100.0f   // mL/min
#define PUMP_MIN_DOSE_ML   1.0f
#define PUMP_MAX_DOSE_ML   9999.0f

// =====================================================
// Neptune Apex
// =====================================================
#define APEX_UNIT_COUNT        2
#define APEX_POLL_INTERVAL_MS  30000
#define APEX_TIMEOUT_MS        10000
#define APEX_MAX_PROBES        8

// =====================================================
// Scheduling
// =====================================================
#define MAX_SCHEDULES      16
#define SCHEDULE_CHECK_MS  1000

// =====================================================
// Storage (EEPROM)
// =====================================================
#define EEPROM_SIZE        8192
#define SAVE_INTERVAL_MS   60000

// =====================================================
// Web Server
// =====================================================
#define HTTP_PORT          80

#endif
