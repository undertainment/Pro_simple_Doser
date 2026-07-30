#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// =====================================================
// WiFi Credentials — HARDCODED, edit before flashing
// =====================================================
#define WIFI_SSID          "YOUR_SSID"
#define WIFI_PASS          "YOUR_PASSWORD"
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
// Scheduling
// =====================================================
#define MAX_SCHEDULES      16
#define SCHEDULE_CHECK_MS  1000

// =====================================================
// Storage (EEPROM)
// =====================================================
#define EEPROM_SIZE        4096
#define SAVE_INTERVAL_MS   60000

// =====================================================
// Web Server
// =====================================================
#define HTTP_PORT          80

#endif
