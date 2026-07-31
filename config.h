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
// Neptune Apex  ←  EDIT POLL INTERVAL BELOW
// =====================================================
#define APEX_UNIT_COUNT        2
#define APEX_POLL_INTERVAL_MS  3600000  // milliseconds between polls (1 hour = 3600000)
#define APEX_TIMEOUT_MS        10000
#define APEX_MAX_PROBES        8

// =====================================================
// Scheduling & Time
// =====================================================
#define MAX_SCHEDULES      16
#define SCHEDULE_CHECK_MS  1000
#define NTP_RETRY_MS       30000

// NTP time sync — used so scheduled doses fire at your local time.
// DEFAULT_TZ_OFFSET_MIN is the local timezone offset in minutes
// (e.g. PDT = UTC-7 -> -420, PST = UTC-8 -> -480, EST = UTC-5 -> -300, UTC = 0).
#define NTP_SERVER         "pool.ntp.org"
#define DEFAULT_TZ_OFFSET_MIN  -420

// Apex Classic polling — default interval per unit (can be overridden per-unit in UI)
// Minimum 10000 ms (10 sec); stored in EEPROM v5 as apexPollMs[]

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
