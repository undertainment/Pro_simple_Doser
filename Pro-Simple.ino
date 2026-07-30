#include "config.h"
#include "types.h"
#include "pump.h"
#include "dosing.h"
#include "scheduler.h"
#include "web_server.h"
#include "storage.h"
#include "logger.h"

unsigned long lastWiFiCheck = 0;
unsigned long lastSave = 0;

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println(F("\n\n=== Pro-Simple Dosing Pump ==="));

  Logger::init();
  Storage::init();
  Pump::init();
  Dosing::init();
  Scheduler::init();
  HttpServer::init();

  Logger::info(F("System ready"));
}

void loop() {
  unsigned long now = millis();

  Pump::loop();
  Dosing::loop();
  Scheduler::loop();
  HttpServer::loop();

  if (now - lastWiFiCheck > 30000) {
    lastWiFiCheck = now;
    HttpServer::checkWiFi();
  }

  if (now - lastSave > 60000) {
    lastSave = now;
    Storage::autoSave();
  }
}
