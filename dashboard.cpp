#include "dashboard.h"
#include "pump.h"
#include "dosing.h"
#include "scheduler.h"
#include "apex.h"
#include "logger.h"
#include <WiFi.h>

SystemStatus Dashboard::getStatus() {
  SystemStatus s;
  s.uptimeHours   = millis() / 3600000.0f;
  s.totalDoses    = 0;
  s.totalVolume   = 0;
  s.freeHeap      = ESP.getFreeHeap();
  s.wifiConnected = WiFi.isConnected();
  s.rssi          = s.wifiConnected ? WiFi.RSSI() : 0;

  if (s.wifiConnected) {
    strncpy(s.ip, WiFi.localIP().toString().c_str(), sizeof(s.ip) - 1);
    s.ip[sizeof(s.ip) - 1] = '\0';
  } else {
    strncpy(s.ip, "0.0.0.0", sizeof(s.ip) - 1);
  }

  for (uint8_t i = 0; i < PUMP_COUNT; i++) {
    PumpConfig* cfg = Pump::getConfig(i);
    if (cfg) {
      s.totalDoses  += cfg->runTimeSec > 0 ? 1 : 0;
      s.totalVolume += cfg->totalDosed;
    }
  }

  return s;
}

String Dashboard::renderStatusJSON() {
  SystemStatus s = getStatus();
  String json = F("{");
  json += String(F("\"uptime\":")) + String(s.uptimeHours, 1) + F(",");
  json += String(F("\"totalDoses\":")) + String(s.totalDoses) + F(",");
  json += String(F("\"totalVolume\":")) + String(s.totalVolume, 1) + F(",");
  json += String(F("\"freeHeap\":")) + String(s.freeHeap) + F(",");
  json += String(F("\"rssi\":")) + String(s.rssi) + F(",");
  json += String(F("\"wifiConnected\":")) + String(s.wifiConnected ? F("true") : F("false")) + F(",");
  json += String(F("\"ip\":\"")) + String(s.ip) + F("\"");
  json += F("}");
  return json;
}

String Dashboard::renderPumpJSON() {
  String json = F("[");
  for (uint8_t i = 0; i < PUMP_COUNT; i++) {
    PumpConfig* cfg = Pump::getConfig(i);
    if (!cfg) continue;
    if (i > 0) json += F(",");
    json += String(F("{\"index\":")) + String(i) + F(",");
    json += String(F("\"name\":\"")) + _jsonEscape(String(cfg->name)) + F("\",");
    json += String(F("\"rate\":")) + String(cfg->rateMLperMin, 1) + F(",");
    json += String(F("\"active\":")) + String(cfg->active ? F("true") : F("false")) + F(",");
    json += String(F("\"totalDosed\":")) + String(cfg->totalDosed, 1) + F(",");
    json += String(F("\"runTimeSec\":")) + String(cfg->runTimeSec) + F(",");
    json += String(F("\"state\":")) + String((int)Dosing::getState(i)) + F(",");
    json += String(F("\"pin\":")) + String(cfg->pin) + F(",");
    json += String(F("\"capacity\":")) + String(cfg->capacity, 1) + F(",");
    json += String(F("\"reservoirLevel\":")) + String(cfg->reservoirLevel);
    json += F("}");
  }
  json += F("]");
  return json;
}

String Dashboard::renderScheduleJSON() {
  String json = F("[");
  for (uint8_t i = 0; i < Scheduler::scheduleCount(); i++) {
    const Schedule* s = Scheduler::getSchedule(i);
    if (!s) continue;
    if (i > 0) json += F(",");
    json += String(F("{\"index\":")) + String(i) + F(",");
    json += String(F("\"pumpIndex\":")) + String(s->pumpIndex) + F(",");
    json += String(F("\"hour\":")) + String(s->hour) + F(",");
    json += String(F("\"minute\":")) + String(s->minute) + F(",");
    json += String(F("\"doseML\":")) + String(s->doseML, 1) + F(",");
    json += String(F("\"enabled\":")) + String(s->enabled ? F("true") : F("false")) + F(",");
    json += String(F("\"days\":")) + String(s->days);
    json += F("}");
  }
  json += F("]");
  return json;
}

String Dashboard::renderLogJSON() {
  String json = F("[");
  uint8_t count = Logger::logCount();
  const String* logs = Logger::getLogs();
  for (uint8_t i = 0; i < count; i++) {
    if (logs[i].length() == 0) continue;
    if (json.length() > 1) json += F(",");
    json += String(F("\"")) + _jsonEscape(logs[i]) + F("\"");
  }
  json += F("]");
  return json;
}

String Dashboard::renderApexJSON() {
  String json = F("{\"units\":[");
  for (uint8_t u = 0; u < APEX_UNIT_COUNT; u++) {
    if (u > 0) json += F(",");
    json += String(F("{\"ip\":\"")) + String(Apex::getConfig(u).ip) + F("\",");
    json += String(F("\"enabled\":")) + String(Apex::getConfig(u).enabled ? F("true") : F("false")) + F(",");
    json += String(F("\"connected\":")) + String(Apex::isConnected(u) ? F("true") : F("false")) + F(",");
    json += String(F("\"lastUpdate\":")) + String(Apex::lastUpdate(u)) + F(",");
    json += String(F("\"probes\":["));
    for (uint8_t i = 0; i < Apex::probeCount(u); i++) {
      const ApexProbe* p = Apex::getProbes(u);
      if (!p) break;
      if (i > 0) json += F(",");
      json += String(F("{\"name\":\"")) + _jsonEscape(String(p[i].name)) + F("\",");
      json += String(F("\"value\":")) + String(p[i].value, 2) + F(",");
      json += String(F("\"label\":\"")) + _jsonEscape(String(p[i].label)) + F("\"}");
    }
    json += F("]}");
  }
  json += F("]}");
  return json;
}

String Dashboard::_jsonEscape(const String& s) {
  String out;
  for (size_t i = 0; i < s.length(); i++) {
    char c = s.charAt(i);
    switch (c) {
      case '"':  out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n";  break;
      case '\r': out += "\\r";  break;
      case '\t': out += "\\t";  break;
      default:   out += c;
    }
  }
  return out;
}
