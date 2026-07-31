#include "web_server.h"
#include "dashboard.h"
#include "dosing.h"
#include "pump.h"
#include "scheduler.h"
#include "apex.h"
#include "storage.h"
#include "logger.h"
#include <time.h>
#include <sys/time.h>
#include <ArduinoJson.h>
#include <Esp.h>

static String _escapeJson(const String& s) {
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

WebServer HttpServer::_server(HTTP_PORT);
bool HttpServer::_apMode = false;

void HttpServer::init() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  Logger::info(String(F("Connecting to WiFi: ")) + WIFI_SSID);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_TIMEOUT_MS) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.isConnected()) {
    Logger::info(String(F("WiFi connected: ")) + WiFi.localIP().toString());
  } else {
    Logger::warn(F("WiFi failed, starting AP mode"));
    _apMode = true;
    WiFi.mode(WIFI_AP);
    WiFi.softAP("Pro-Simple", "config123");
    Logger::info(String(F("AP: Pro-Simple @ ")) + WiFi.softAPIP().toString());
  }

  _server.on("/", _handleRoot);
  _server.on("/api", _handleAPI);
  _server.on("/api/dose", _handleDose);
  _server.on("/api/pump", _handlePumpConfig);
  _server.on("/api/schedule", _handleScheduleAdd);
  _server.on("/api/schedule/remove", _handleScheduleRemove);
  _server.on("/api/reset", _handleResetTotals);
  _server.on("/api/refill", _handleRefill);
  _server.on("/api/apex", _handleApexConfig);
  _server.on("/api/ntp", _handleNTP);
  _server.on("/api/timezone", _handleTimeZone);
  _server.on("/api/config/export", _handleConfigExport);
  _server.on("/api/config/import", HTTP_POST, _handleConfigImport);
  _server.onNotFound(_handleNotFound);

  _server.begin();
  Logger::info(F("Web server started"));
}

void HttpServer::loop() {
  _server.handleClient();
}

void HttpServer::checkWiFi() {
  if (_apMode) return;
  if (!WiFi.isConnected()) {
    Logger::warn(F("WiFi lost, reconnecting..."));
    WiFi.reconnect();
  }
}

void HttpServer::_handleRoot() {
  _server.send(200, "text/html", _buildDashboardHTML());
}

void HttpServer::_handleAPI() {
  String path = _server.arg(F("path"));
  String json;
  if (path == "status")   json = Dashboard::renderStatusJSON();
  else if (path == "pumps")  json = Dashboard::renderPumpJSON();
  else if (path == "schedules") json = Dashboard::renderScheduleJSON();
  else if (path == "logs")      json = Dashboard::renderLogJSON();
  else if (path == "apex")      json = Dashboard::renderApexJSON();
  else json = F("{\"error\":\"unknown\"}");

  _server.send(200, "application/json", json);
}

void HttpServer::_handleDose() {
  int idx = _server.arg(F("pump")).toInt();
  float vol = _server.arg(F("vol")).toFloat();
  bool ok = Dosing::startDose(idx, vol);
  _server.send(200, "application/json",
               ok ? F("{\"ok\":true}") : F("{\"ok\":false}"));
}

void HttpServer::_handlePumpConfig() {
  int idx = _server.arg(F("pump")).toInt();
  if (idx < 0 || idx >= PUMP_COUNT) return;

  PumpConfig cfg;
  PumpConfig* cur = Pump::getConfig(idx);
  if (cur) cfg = *cur;

  if (_server.hasArg(F("name"))) {
    String n = _server.arg(F("name"));
    strncpy(cfg.name, n.c_str(), sizeof(cfg.name) - 1);
    cfg.name[sizeof(cfg.name) - 1] = '\0';
  }
  if (_server.hasArg(F("rate"))) cfg.rateMLperMin = _server.arg(F("rate")).toFloat();
  if (_server.hasArg(F("active"))) cfg.active = _server.arg(F("active")) == "true";
  if (_server.hasArg(F("capacity"))) cfg.capacity = _server.arg(F("capacity")).toFloat();

  Pump::setConfig(idx, cfg);
  Storage::save();
  _server.send(200, "application/json", F("{\"ok\":true}"));
}

void HttpServer::_handleRefill() {
  Pump::refillAll();
  Storage::save();
  _server.send(200, "application/json", F("{\"ok\":true}"));
}

void HttpServer::_handleApexConfig() {
  if (_server.hasArg(F("ip"))) {
    uint8_t unit = _server.arg(F("unit")).toInt();
    if (unit >= APEX_UNIT_COUNT) unit = 0;
    ApexConfig cfg = Apex::getConfig(unit);
    String ip = _server.arg(F("ip"));
    strncpy(cfg.ip, ip.c_str(), sizeof(cfg.ip) - 1);
    cfg.ip[sizeof(cfg.ip) - 1] = '\0';
    if (_server.hasArg(F("port"))) cfg.port = _server.arg(F("port")).toInt();
    if (_server.hasArg(F("username"))) {
      String u = _server.arg(F("username"));
      strncpy(cfg.username, u.c_str(), sizeof(cfg.username) - 1);
      cfg.username[sizeof(cfg.username) - 1] = '\0';
    }
    if (_server.hasArg(F("password"))) {
      String p = _server.arg(F("password"));
      strncpy(cfg.password, p.c_str(), sizeof(cfg.password) - 1);
      cfg.password[sizeof(cfg.password) - 1] = '\0';
    }
    if (_server.hasArg(F("enabled"))) cfg.enabled = _server.arg(F("enabled")) == "true";
    if (_server.hasArg(F("probeMask"))) cfg.probeMask = _server.arg(F("probeMask")).toInt();
    Apex::setConfig(unit, cfg);
    if (_server.hasArg(F("pollIntervalMs"))) {
      uint32_t ms = _server.arg(F("pollIntervalMs")).toInt();
      if (ms >= 10000) Apex::setPollIntervalMs(unit, ms);
    }
    Storage::markDirty();
    _server.send(200, "application/json", F("{\"ok\":true}"));
  } else {
    _server.send(200, "application/json", Dashboard::renderApexJSON());
  }
}

void HttpServer::_handleScheduleAdd() {
  Schedule s;
  s.pumpIndex = _server.arg(F("pump")).toInt();
  s.hour      = _server.arg(F("hour")).toInt();
  s.minute    = _server.arg(F("minute")).toInt();
  s.doseML    = _server.arg(F("vol")).toFloat();
  s.enabled   = _server.arg(F("enabled")) != "false";
  s.days      = _server.arg(F("days")).toInt();

  bool ok = Scheduler::addSchedule(s);
  if (ok) Storage::save();
  _server.send(200, "application/json",
               ok ? F("{\"ok\":true}") : F("{\"ok\":false}"));
}

void HttpServer::_handleScheduleRemove() {
  int idx = _server.arg(F("index")).toInt();
  Scheduler::removeSchedule(idx);
  Storage::save();
  _server.send(200, "application/json", F("{\"ok\":true}"));
}

void HttpServer::_handleResetTotals() {
  if (_server.arg(F("reboot")) == "1") {
    Logger::info(F("Rebooting via web dashboard"));
    _server.send(200, "application/json", F("{\"ok\":true}"));
    delay(200);
    ESP.restart();
    return;
  }
  for (uint8_t i = 0; i < PUMP_COUNT; i++) {
    Pump::resetTotal(i);
  }
  Storage::save();
  _server.send(200, "application/json", F("{\"ok\":true}"));
}

void HttpServer::_handleNTP() {
  Scheduler::syncTime();
  Logger::info(F("NTP sync requested"));
  _server.send(200, "application/json", F("{\"ok\":true}"));
}

void HttpServer::_handleTimeZone() {
  int16_t offsetMin = _server.arg(F("min")).toInt();
  Scheduler::setTimeZoneOffsetMin(offsetMin);
  Storage::save();
  _server.send(200, "application/json", F("{\"ok\":true}"));
}

void HttpServer::_handleNotFound() {
  _server.send(404, "application/json", F("{\"error\":\"not found\"}"));
}

void HttpServer::_handleConfigExport() {
  String json = F("{\"version\":1,\"tzOffsetMin\":");
  json += String(Scheduler::timeZoneOffsetMin());
  json += F(",\"pumps\":[");
  for (uint8_t i = 0; i < PUMP_COUNT; i++) {
    PumpConfig* cfg = Pump::getConfig(i);
    if (!cfg) continue;
    if (i > 0) json += F(",");
    json += F("{\"name\":\"");
    json += _escapeJson(String(cfg->name));
    json += F("\",\"rate\":");
    json += String(cfg->rateMLperMin, 2);
    json += F(",\"active\":");
    json += String(cfg->active ? F("true") : F("false"));
    json += F(",\"capacity\":");
    json += String(cfg->capacity, 1);
    json += F(",\"reservoirRemaining\":");
    json += String(Pump::reservoirRemaining(i), 1);
    json += F(",\"totalDosed\":");
    json += String(cfg->totalDosed, 1);
    json += F(",\"runTimeSec\":");
    json += String(cfg->runTimeSec);
    json += F("}");
  }
  json += F("],\"schedules\":[");
  for (uint8_t i = 0; i < Scheduler::scheduleCount(); i++) {
    const Schedule* s = Scheduler::getSchedule(i);
    if (!s) continue;
    if (i > 0) json += F(",");
    json += F("{\"pumpIndex\":");
    json += String(s->pumpIndex);
    json += F(",\"hour\":");
    json += String(s->hour);
    json += F(",\"minute\":");
    json += String(s->minute);
    json += F(",\"doseML\":");
    json += String(s->doseML, 2);
    json += F(",\"enabled\":");
    json += String(s->enabled ? F("true") : F("false"));
    json += F(",\"days\":");
    json += String(s->days);
    json += F("}");
  }
  json += F("],\"apex\":[");
  for (uint8_t u = 0; u < APEX_UNIT_COUNT; u++) {
    ApexConfig cfg = Apex::getConfig(u);
    if (u > 0) json += F(",");
    json += F("{\"ip\":\"");
    json += _escapeJson(String(cfg.ip));
    json += F("\",\"port\":");
    json += String(cfg.port);
    json += F(",\"username\":\"");
    json += _escapeJson(String(cfg.username));
    json += F("\",\"password\":\"");
    json += _escapeJson(String(cfg.password));
    json += F("\",\"enabled\":");
    json += String(cfg.enabled ? F("true") : F("false"));
    json += F(",\"probeMask\":");
    json += String(cfg.probeMask);
    json += F("}");
  }
  json += F("]}");
  _server.send(200, "application/json", json);
}

void HttpServer::_handleConfigImport() {
  String body = _server.arg(F("plain"));
  if (body.length() == 0) {
    _server.send(400, "application/json", F("{\"ok\":false,\"error\":\"empty body\"}"));
    return;
  }

  JsonDocument doc;
  if (deserializeJson(doc, body)) {
    _server.send(400, "application/json", F("{\"ok\":false,\"error\":\"invalid json\"}"));
    return;
  }

  if (doc[F("tzOffsetMin")].is<int>()) {
    Scheduler::setTimeZoneOffsetMin(doc[F("tzOffsetMin")].as<int16_t>());
  }

  JsonArray pumps = doc[F("pumps")].as<JsonArray>();
  for (uint8_t i = 0; i < PUMP_COUNT && i < pumps.size(); i++) {
    JsonObject p = pumps[i];
    PumpConfig cfg;
    PumpConfig* cur = Pump::getConfig(i);
    if (cur) cfg = *cur;

    if (p[F("name")].is<const char*>()) {
      strncpy(cfg.name, p[F("name")].as<const char*>(), sizeof(cfg.name) - 1);
      cfg.name[sizeof(cfg.name) - 1] = '\0';
    }
    if (!p[F("rate")].isNull()) cfg.rateMLperMin = p[F("rate")].as<float>();
    if (p[F("active")].is<bool>()) cfg.active = p[F("active")].as<bool>();
    if (!p[F("capacity")].isNull()) cfg.capacity = p[F("capacity")].as<float>();
    if (!p[F("totalDosed")].isNull()) cfg.totalDosed = p[F("totalDosed")].as<float>();
    if (!p[F("runTimeSec")].isNull()) cfg.runTimeSec = p[F("runTimeSec")].as<uint32_t>();

    Pump::setConfig(i, cfg);
    if (!p[F("reservoirRemaining")].isNull()) {
      Pump::setReservoirRemaining(i, p[F("reservoirRemaining")].as<float>());
    }
  }

  // rebuild schedules
  while (Scheduler::scheduleCount() > 0) Scheduler::removeSchedule(0);
  JsonArray scheds = doc[F("schedules")].as<JsonArray>();
  for (JsonObject s : scheds) {
    Schedule sch;
    sch.pumpIndex = s[F("pumpIndex")] | 0;
    sch.hour      = s[F("hour")] | 0;
    sch.minute    = s[F("minute")] | 0;
    sch.doseML    = s[F("doseML")] | 0;
    sch.enabled   = s[F("enabled")] | true;
    sch.days      = s[F("days")] | 0xFF;
    Scheduler::addSchedule(sch);
  }

  JsonArray apex = doc[F("apex")].as<JsonArray>();
  for (uint8_t u = 0; u < APEX_UNIT_COUNT && u < apex.size(); u++) {
    JsonObject a = apex[u];
    ApexConfig cfg = Apex::getConfig(u);
    if (a[F("ip")].is<const char*>()) {
      strncpy(cfg.ip, a[F("ip")].as<const char*>(), sizeof(cfg.ip) - 1);
      cfg.ip[sizeof(cfg.ip) - 1] = '\0';
    }
    if (a[F("port")].is<int>()) cfg.port = a[F("port")].as<uint16_t>();
    if (a[F("username")].is<const char*>()) {
      strncpy(cfg.username, a[F("username")].as<const char*>(), sizeof(cfg.username) - 1);
      cfg.username[sizeof(cfg.username) - 1] = '\0';
    }
    if (a[F("password")].is<const char*>()) {
      strncpy(cfg.password, a[F("password")].as<const char*>(), sizeof(cfg.password) - 1);
      cfg.password[sizeof(cfg.password) - 1] = '\0';
    }
    if (a[F("enabled")].is<bool>()) cfg.enabled = a[F("enabled")].as<bool>();
    if (a[F("probeMask")].is<int>()) cfg.probeMask = a[F("probeMask")].as<uint8_t>();
    Apex::setConfig(u, cfg);
  }

  Storage::save();
  Logger::info(F("Config imported from backup"));
  _server.send(200, "application/json", F("{\"ok\":true}"));
}

String HttpServer::_buildDashboardHTML() {
  return String(F(
R"rawliteral(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Pro-Simple | Dosing Controller</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
:root{
  --bg:#1a1d23;--bg2:#14161b;--bg3:#1e2128;
  --border:#2a2e35;--border2:#3a3f48;
  --text:#d4d7dd;--text2:#f0f1f3;--text3:#9ca3af;--text4:#6b7280;--text5:#4b5563;
  --accent:#ff8c42;--accent2:#f59e0b;
  --green:#34d399;--blue:#3b82f6;--red:#ef4444;
}
body.light{
  --bg:#f5f7fa;--bg2:#fff;--bg3:#f0f4ff;
  --border:#e5e7eb;--border2:#d1d5db;
  --text:#374151;--text2:#111827;--text3:#6b7280;--text4:#9ca3af;--text5:#d1d5db;
  --accent:#f97316;--accent2:#eab308;
}
body{
  font-family:system-ui,-apple-system,'Segoe UI',sans-serif;
  background:var(--bg);color:var(--text);padding:20px;
  transition:background .3s,color .3s;
}
.app{max-width:1200px;margin:0 auto}
/* Toast */
.toast-container{
  position:fixed;top:16px;left:50%;transform:translateX(-50%);
  z-index:9999;display:flex;flex-direction:column;gap:8px;align-items:center;pointer-events:none;
}
.toast{
  background:var(--bg2);border:1px solid var(--border);border-radius:10px;
  padding:10px 20px;font-size:.82rem;box-shadow:0 8px 32px rgba(0,0,0,.3);
  animation:toastIn .3s ease;pointer-events:auto;display:flex;align-items:center;gap:10px;
}
.toast .t-icon{font-size:1rem}
.toast.success{border-left:3px solid var(--green)}
.toast.error{border-left:3px solid var(--red)}
.toast.info{border-left:3px solid var(--blue)}
@keyframes toastIn{from{opacity:0;transform:translateY(-16px)}}
@keyframes toastOut{to{opacity:0;transform:translateY(-16px)}}
/* Top bar */
.top-bar{
  display:flex;align-items:center;justify-content:space-between;margin-bottom:20px;
  padding:10px 18px;border-radius:8px 8px 0 0;
  background:var(--bg2);border:1px solid var(--border);border-bottom:none;
}
.brand h1{font-size:.85rem;font-weight:600;color:var(--accent);letter-spacing:.04em}
.brand h1 span{color:var(--text);font-weight:400}
.brand .tag{font-size:.72rem;color:var(--text4);margin-top:2px;display:flex;align-items:center;gap:6px}
.hmi-led{display:inline-block;width:8px;height:8px;border-radius:50%;margin-right:2px}
.hmi-led.on{background:var(--green);box-shadow:0 0 6px var(--green)}
.hmi-led.off{background:var(--red);box-shadow:0 0 6px var(--red)}
.user-info{display:flex;align-items:center;gap:14px;font-size:.8rem;color:var(--text3)}
.header-clock{display:flex;flex-direction:column;align-items:flex-end;line-height:1.3}
.header-clock #clockDisplay{font-size:.82rem;font-weight:600;color:var(--text2);font-family:'SF Mono','Fira Code',monospace}
.header-clock #dateDisplay{font-size:.68rem;color:var(--text4)}
.theme-toggle-wrap{display:flex;align-items:center;gap:6px}
.theme-toggle{
  width:42px;height:22px;background:var(--border);border-radius:12px;
  position:relative;cursor:pointer;transition:background .3s;
}
.theme-toggle .knob{
  width:18px;height:18px;background:var(--blue);border-radius:50%;
  position:absolute;top:2px;left:2px;transition:all .3s;
}
body.light .theme-toggle{background:var(--blue)}
body.light .theme-toggle .knob{left:22px;background:#fff}
.avatar{
  width:36px;height:36px;border-radius:8px;
  background:linear-gradient(135deg,var(--blue),#1d4ed8);
  display:flex;align-items:center;justify-content:center;
  color:#fff;font-weight:600;font-size:.82rem;
  box-shadow:0 0 20px rgba(59,130,246,.2);
}
/* KPI Gauges */
.kpi-grid{display:grid;grid-template-columns:repeat(4,1fr);gap:12px;margin-bottom:20px}
.kpi-card{
  background:var(--bg2);border:1px solid var(--border);border-radius:6px;
  padding:14px 16px;position:relative;border-left:3px solid var(--accent);transition:all .2s;
}
.kpi-card:hover{border-color:var(--border2)}
.kpi-card .kpi-label{font-size:.65rem;text-transform:uppercase;letter-spacing:.06em;color:var(--text4);margin-bottom:4px}
.kpi-card .kpi-value{font-size:1.5rem;font-weight:700;color:var(--text2);margin-top:2px;font-feature-settings:'tnum'1}
.kpi-card .kpi-value .kpi-unit{font-size:.7rem;color:var(--text4);font-weight:400;margin-left:2px}
.kpi-card .kpi-sub{font-size:.68rem;margin-top:4px;color:var(--text3)}
.kpi-card .kpi-bar{margin-top:8px;height:3px;background:var(--border);border-radius:2px;overflow:hidden}
.kpi-card .kpi-bar span{display:block;height:100%;border-radius:2px;background:linear-gradient(90deg,var(--accent),var(--accent2))}
.kpi-card.green{border-left-color:var(--green)}
.kpi-card.green .kpi-bar span{background:linear-gradient(90deg,var(--green),#6ee7b7)}
.kpi-card.blue{border-left-color:var(--blue)}
.kpi-card.blue .kpi-bar span{background:linear-gradient(90deg,var(--blue),#60a5fa)}
/* Grid */
.cols-2{display:grid;grid-template-columns:1fr 1fr;gap:12px;margin-bottom:16px}
.cols-2-66{display:grid;grid-template-columns:1.6fr 1fr;gap:12px;margin-bottom:16px}
.cols-2-35{display:grid;grid-template-columns:1fr 1.3fr;gap:12px;margin-bottom:16px}
/* Cards / Panels */
.card,.hmi-panel{
  background:var(--bg2);border-radius:6px;border:1px solid var(--border);
  overflow:hidden;transition:border-color .2s;
}
.card:hover,.hmi-panel:hover{border-color:var(--border2)}
.card-header,.hp-header{
  background:var(--bg3);padding:8px 14px;
  border-bottom:1px solid var(--border);
  display:flex;align-items:center;justify-content:space-between;flex-wrap:wrap;gap:8px;
}
.card-header h2,.hp-header h3{font-size:.72rem;font-weight:600;text-transform:uppercase;letter-spacing:.05em;color:var(--text3)}
.card-header .action,.hp-header .action{font-size:.68rem;color:var(--accent);cursor:pointer;font-weight:500}
.card-header .action:hover,.hp-header .action:hover{color:var(--accent2)}
.hp-badge{font-size:.62rem;padding:2px 8px;border-radius:3px;background:var(--border);color:var(--text3)}
.card-body,.hp-body{padding:10px 14px}
/* Tables */
table{width:100%;border-collapse:collapse;font-size:.75rem}
th{
  text-align:left;padding:7px 10px;color:var(--text4);font-weight:500;
  font-size:.65rem;text-transform:uppercase;letter-spacing:.04em;
  border-bottom:1px solid var(--border);background:var(--bg3);
}
td{padding:7px 10px;border-bottom:1px solid rgba(42,46,53,.5);color:var(--text3)}
tr:last-child td{border-bottom:none}
tr:hover td{background:var(--bg3)}
td strong{color:var(--text2)}
/* Pump row disabled */
tr.disabled td{opacity:.35}
tr.disabled td .btn{cursor:not-allowed;opacity:.5;pointer-events:none}
tr.disabled td .pump-name{border-color:transparent!important;cursor:default}
/* Pump name inline edit */
.pump-name{cursor:pointer;border-bottom:1px dashed transparent;transition:border-color .2s}
.pump-name:hover{border-color:var(--blue)}
.pump-name-input{
  background:var(--bg);border:1px solid var(--blue);color:var(--text2);
  padding:2px 6px;border-radius:4px;font-size:.78rem;font-family:inherit;width:100px;
}
.pump-name-input:focus{outline:none}
/* Badges */
.badge{display:inline-block;padding:2px 10px;border-radius:3px;font-size:.65rem;font-weight:600}
.badge.idle{background:rgba(107,114,128,.15);color:var(--text4);border:1px solid rgba(107,114,128,.2)}
.badge.priming{background:rgba(245,158,11,.1);color:var(--accent2);border:1px solid rgba(245,158,11,.2)}
.badge.dosing{background:rgba(52,211,153,.1);color:var(--green);border:1px solid rgba(52,211,153,.2)}
.badge.complete{background:rgba(52,211,153,.1);color:var(--green);border:1px solid rgba(52,211,153,.2)}
.badge.error{background:rgba(239,68,68,.1);color:var(--red);border:1px solid rgba(239,68,68,.2)}
/* Toggle switch */
.toggle-sm{
  position:relative;width:32px;height:18px;display:inline-block;cursor:pointer;
}
.toggle-sm input{opacity:0;width:0;height:0}
.toggle-sm .slider{
  position:absolute;inset:0;background:var(--border);border-radius:18px;transition:.25s;
}
.toggle-sm .slider::before{
  content:'';position:absolute;height:12px;width:12px;left:3px;bottom:3px;
  background:var(--text4);border-radius:50%;transition:.25s;
}
.toggle-sm input:checked+.slider{background:var(--green)}
.toggle-sm input:checked+.slider::before{background:#fff;transform:translateX(14px)}
/* Pump controls */
.pump-actions{display:flex;gap:4px;align-items:center;flex-wrap:wrap}
.pump-actions input{
  width:44px;padding:3px 6px;background:var(--bg);border:1px solid var(--border);
  border-radius:3px;font-size:.7rem;text-align:center;color:var(--text2);
}
.pump-actions input:focus{outline:none;border-color:var(--accent)}
/* Buttons */
.btn{
  padding:5px 12px;border-radius:4px;font-size:.72rem;font-weight:500;
  border:1px solid var(--border);cursor:pointer;transition:all .15s;font-family:inherit;white-space:nowrap;
  background:var(--border);color:var(--text);
}
.btn:hover{background:var(--border2);border-color:var(--accent)}
.btn-primary{background:var(--accent);border-color:var(--accent);color:#14161b}
.btn-primary:hover{background:var(--accent2);border-color:var(--accent2)}
.btn-success{background:var(--green);border-color:var(--green);color:#14161b}
.btn-success:hover{background:#059669}
.btn-danger{background:var(--red);border-color:var(--red);color:#fff}
.btn-danger:hover{background:#dc2626}
.btn-warning{background:var(--accent2);border-color:var(--accent2);color:#14161b}
.btn-warning:hover{background:#d97706}
.btn-outline{background:transparent;border:1px solid var(--border2);color:var(--text3)}
.btn-outline:hover{background:var(--border);border-color:var(--accent);color:var(--text2)}
.btn-sm{padding:3px 8px;font-size:.65rem}
/* Schedules */
.sched-per-pump{margin-bottom:8px}
.sched-per-pump:last-child{margin-bottom:0}
.sched-per-pump .spp-header{
  font-size:.68rem;font-weight:600;color:var(--text2);
  padding:4px 0;display:flex;justify-content:space-between;
  border-bottom:1px solid var(--border);margin-bottom:4px;
}
.sched-per-pump .spp-header .spp-count{font-size:.62rem;color:var(--text4);font-weight:400}
.sched-row{
  display:flex;align-items:center;justify-content:space-between;
  padding:5px 0;border-bottom:1px solid rgba(42,46,53,.3);font-size:.75rem;
}
.sched-row:last-child{border-bottom:none}
.sched-time{font-weight:600;color:var(--accent);min-width:50px;font-feature-settings:'tnum'1}
.sched-detail{color:var(--text3);font-size:.72rem}
.sched-days{color:var(--text4);font-size:.62rem}
/* Reservoirs */
.reservoir-row{
  display:flex;align-items:center;padding:7px 0;border-bottom:1px solid var(--border);
}
.reservoir-row:last-child{border-bottom:none}
.reservoir-row .r-name{flex:1}
.reservoir-row .r-name .r-name-text{color:var(--text2);font-size:.8rem;font-weight:600;cursor:pointer;border-bottom:1px dashed transparent;transition:border-color .2s}
.reservoir-row .r-name .r-name-text:hover{border-color:var(--blue)}
.reservoir-row .r-meta{color:var(--text4);font-size:.65rem;margin-top:1px}
.reservoir-row .r-level{width:90px}
.reservoir-row .r-level .bar{
  height:5px;background:var(--border);border-radius:4px;overflow:hidden;
}
.reservoir-row .r-level .bar span{display:block;height:100%;border-radius:4px}
.reservoir-row .r-pct{font-size:.7rem;font-weight:600;min-width:34px;text-align:right}
/* Log */
.log-list{font-family:'SF Mono','Fira Code',monospace;font-size:.68rem;line-height:1.6;color:var(--text4);max-height:190px;overflow-y:auto;padding:4px 0}
.log-list .log-time{color:var(--text5);margin-right:4px}
.log-list .log-info{color:var(--green)}
.log-list .log-warn{color:var(--accent2)}
.log-list .log-err{color:var(--red)}
/* Apex card */
.apex-grid{display:grid;grid-template-columns:1fr 1fr;gap:10px}
.apex-probe{padding:12px;border-radius:8px;background:var(--bg);border:1px solid var(--border)}
.apex-probe .p-label{font-size:.63rem;text-transform:uppercase;color:var(--text4);letter-spacing:.05em}
.apex-probe .p-value{font-size:1.15rem;font-weight:700;color:var(--text2);margin-top:2px}
.apex-status{font-size:.68rem;color:var(--text4);margin-top:8px;display:flex;align-items:center;gap:6px}
.apex-status .dot{width:8px;height:8px;border-radius:50%;display:inline-block}
.apex-status .dot.on{background:var(--green);box-shadow:0 0 6px rgba(16,185,129,.5)}
.apex-status .dot.off{background:var(--text4)}
.apex-status .conn{color:var(--green);font-weight:600}
.apex-status .disc{color:var(--text4)}
/* Quick actions */
.qa-grid{display:grid;grid-template-columns:1fr 1fr;gap:6px}
.qa-grid button{width:100%}
.timezone-label{font-size:.65rem;color:var(--text4);margin-top:4px}
.timezone-label select{background:var(--bg);border:1px solid var(--border);color:var(--text2);padding:2px 4px;border-radius:4px;font-size:.65rem;font-family:inherit}
/* Modal */
.modal-overlay{
  position:fixed;inset:0;background:rgba(0,0,0,.6);z-index:9998;
  display:none;align-items:center;justify-content:center;
}
.modal{
  background:var(--bg2);border:1px solid var(--border);border-radius:8px;
  padding:20px;max-width:420px;width:90%;box-shadow:0 16px 48px rgba(0,0,0,.4);
  max-height:90vh;overflow-y:auto;
}
.modal h3{font-size:.9rem;font-weight:600;color:var(--text2);margin-bottom:14px}
.modal label{font-size:.72rem;color:var(--text4);display:block;margin-bottom:4px}
.modal input,.modal select{
  width:100%;padding:6px 10px;background:var(--bg);border:1px solid var(--border);
  border-radius:4px;color:var(--text2);font-size:.78rem;font-family:inherit;margin-bottom:10px;
}
.modal input:focus,.modal select:focus{outline:none;border-color:var(--accent)}
.modal .day-chips{display:flex;gap:4px;flex-wrap:wrap;margin-bottom:10px}
.modal .day-chips .chip{
  padding:4px 10px;border-radius:4px;font-size:.72rem;font-weight:500;
  border:1px solid var(--border);cursor:pointer;transition:all .15s;
  color:var(--text3);background:transparent;
}
.modal .day-chips .chip.active{background:var(--accent);color:#14161b;border-color:var(--accent)}
.modal .modal-actions{display:flex;gap:8px;justify-content:flex-end;margin-top:8px}
/* Pump count grid */
.pump-count-grid{display:grid;grid-template-columns:repeat(4,1fr);gap:8px;margin-bottom:16px}
.pump-count-grid .pc-chip{
  padding:10px;border-radius:8px;font-size:.9rem;font-weight:600;text-align:center;
  border:2px solid var(--border);cursor:pointer;transition:all .15s;
  color:var(--text3);background:transparent;
}
.pump-count-grid .pc-chip:hover{border-color:var(--accent);color:var(--text2)}
.pump-count-grid .pc-chip.active{background:var(--accent);color:#14161b;border-color:var(--accent)}
@media(max-width:700px){
  body{padding:16px}
  .kpi-grid{grid-template-columns:repeat(2,1fr)}
  .cols-2,.cols-2-66,.cols-2-35{grid-template-columns:1fr}
  .top-bar{flex-direction:column;align-items:flex-start}
}
</style>
</head>
<body>

<div class="toast-container" id="toastContainer"></div>

<!-- Pump Count Modal -->
<div class="modal-overlay" id="pumpCountModal">
  <div class="modal" style="max-width:320px">
    <h3>Configure Pump Count</h3>
    <label>Number of pumps</label>
    <div class="pump-count-grid" id="pumpCountGrid"></div>
    <div class="modal-actions">
      <button class="btn btn-outline" onclick="closePumpCountModal()">Cancel</button>
      <button class="btn btn-primary" onclick="applyPumpCount()">Apply</button>
    </div>
  </div>
</div>

<!-- Schedule Modal -->
<div class="modal-overlay" id="schedModal">
  <div class="modal">
    <h3>New Schedule</h3>
    <label>Pump</label>
    <select id="sPump"></select>
    <div style="display:flex;gap:10px">
      <div style="flex:1"><label>Hour</label><input type="number" id="sHour" value="8" min="1" max="12"></div>
      <div style="flex:1"><label>Minute</label><input type="number" id="sMin" value="0" min="0" max="59"></div>
      <div style="flex:1"><label>Dose (mL)</label><input type="number" id="sVol" value="10" min="1" step="0.5"></div>
    </div>
    <div style="display:flex;gap:6px;margin-bottom:12px" id="ampmChips">
      <span class="chip active" data-ampm="AM" style="padding:4px 14px;border-radius:6px;font-size:.72rem;font-weight:500;border:1px solid var(--border);cursor:pointer;background:var(--accent);color:#14161b;border-color:var(--accent)">AM</span>
      <span class="chip" data-ampm="PM" style="padding:4px 14px;border-radius:6px;font-size:.72rem;font-weight:500;border:1px solid var(--border);cursor:pointer;color:var(--text3);background:transparent">PM</span>
    </div>
    <label style="margin-top:4px">Days</label>
    <div class="day-chips" id="dayChips">
      <span class="chip active" data-d="1">Sun</span>
      <span class="chip active" data-d="2">Mon</span>
      <span class="chip active" data-d="4">Tue</span>
      <span class="chip active" data-d="8">Wed</span>
      <span class="chip active" data-d="16">Thu</span>
      <span class="chip active" data-d="32">Fri</span>
      <span class="chip" data-d="64">Sat</span>
    </div>
    <div class="modal-actions">
      <button class="btn btn-outline" onclick="closeSchedModal()">Cancel</button>
      <button class="btn btn-primary" onclick="saveSchedule()">Save Schedule</button>
    </div>
  </div>
</div>

<!-- Calibrate Modal -->
<div class="modal-overlay" id="calModal">
  <div class="modal" style="max-width:360px">
    <h3>Calibrate Pump <span id="calPumpName"></span></h3>
    <div id="calStep1">
      <label>Run duration (seconds)</label>
      <input type="number" id="calDuration" value="30" min="5" max="300" step="5">
      <div class="modal-actions">
        <button class="btn btn-outline" onclick="closeCalModal()">Cancel</button>
        <button class="btn btn-primary" onclick="calRun()">Run Pump</button>
      </div>
    </div>
    <div id="calStep2" style="display:none">
      <p style="font-size:.82rem;color:var(--text3);margin-bottom:12px">Pump ran for <strong id="calActualSec">0</strong> seconds. Enter the measured output.</p>
      <label>Measured volume (mL)</label>
      <input type="number" id="calVolume" value="10" min="0.1" step="0.5">
      <div id="calResult" style="display:none;font-size:.82rem;color:var(--green);margin-bottom:8px">New rate: <strong id="calNewRate">0</strong> mL/min</div>
      <div class="modal-actions">
        <button class="btn btn-outline" onclick="closeCalModal()">Cancel</button>
        <button class="btn btn-primary" onclick="calSave()">Save Calibration</button>
      </div>
    </div>
  </div>
</div>

<!-- Apex Config Modal -->
<div class="modal-overlay" id="apexModal">
  <div class="modal" style="max-width:340px">
    <h3>Apex Classic Config</h3>
    <div id="apexConfigFields"></div>
    <div class="modal-actions">
      <button class="btn btn-outline" onclick="closeApexModal()">Cancel</button>
      <button class="btn btn-primary" onclick="saveApexConfig()">Save</button>
    </div>
  </div>
</div>

<div class="app">
  <div class="top-bar">
    <div class="brand">
      <h1>PRO-SIMPLE <span>| DOSING CONTROLLER</span></h1>
      <div class="tag"><span class="hmi-led on" id="onlineDot"></span> <span id="onlineLabel">Online</span> &middot; <span id="ipDisplay">--</span></div>
    </div>
    <div class="user-info">
      <div class="theme-toggle-wrap">
        <span id="themeLabel" style="font-size:.7rem;color:var(--text4);user-select:none">Dark</span>
        <div class="theme-toggle" id="themeToggle" onclick="toggleTheme()">
          <div class="knob"></div>
        </div>
      </div>
      <div class="header-clock"><span id="clockDisplay">12:00:00 PM</span><span id="dateDisplay">Jan 1, 2026</span><span id="deviceTimeDisplay" style="font-size:.62rem;color:var(--text4)">device clock: --</span></div>
    </div>
  </div>

  <!-- Row 1: KPI Gauges -->
  <div class="kpi-grid">
    <div class="kpi-card">
      <div class="kpi-label">System Uptime</div>
      <div class="kpi-value" id="kpiUptime">--<span class="kpi-unit">h</span></div>
      <div class="kpi-sub" id="kpiUptimeSub">--</div>
      <div class="kpi-bar"><span style="width:0%" id="kpiUptimeBar"></span></div>
    </div>
    <div class="kpi-card green">
      <div class="kpi-label">Total Dispensed</div>
      <div class="kpi-value" id="kpiVolume">--<span class="kpi-unit">mL</span></div>
      <div class="kpi-sub" id="kpiVolumeSub">--</div>
      <div class="kpi-bar"><span style="width:0%" id="kpiVolumeBar"></span></div>
    </div>
    <div class="kpi-card blue">
      <div class="kpi-label">Active Pumps</div>
      <div class="kpi-value" id="kpiDoses">--<span class="kpi-unit">/4</span></div>
      <div class="kpi-sub" id="kpiDosesSub">--</div>
      <div class="kpi-bar"><span style="width:0%" id="kpiDosesBar"></span></div>
    </div>
    <div class="kpi-card">
      <div class="kpi-label">WiFi Signal</div>
      <div class="kpi-value" id="kpiSignal">--<span class="kpi-unit">dBm</span></div>
      <div class="kpi-sub" id="kpiSignalSub">--</div>
      <div class="kpi-bar"><span style="width:0%" id="kpiSignalBar"></span></div>
    </div>
  </div>

  <!-- Row 2: Pump Overview + Schedules -->
  <div class="cols-2-66" style="margin-bottom:24px">
    <div class="card">
      <div class="card-header"><h2>Pump Overview</h2></div>
      <div class="card-body" style="padding:0">
        <table>
          <thead><tr><th style="width:32px">On</th><th>Pump</th><th style="width:58px">Pin</th><th>Rate</th><th>Dosed</th><th>Status</th><th style="min-width:200px"></th></tr></thead>
          <tbody id="pumpTableBody"></tbody>
        </table>
      </div>
    </div>
    <div class="card">
      <div class="card-header"><h2>Schedules <span style="font-size:.65rem;color:var(--text4);font-weight:400">(max 4/pump)</span></h2><span class="action" onclick="openSchedModal()">+ Add</span></div>
      <div class="card-body" id="schedContainer"></div>
    </div>
  </div>

  <!-- Row 3: Reservoirs + Recent Activity -->
  <div class="cols-2" style="margin-bottom:24px">
    <div class="card">
      <div class="card-header"><h2>Reservoirs</h2><span class="action" onclick="refillAll()">Refill All</span></div>
      <div class="card-body" id="reservoirContainer"></div>
    </div>
    <div class="card">
      <div class="card-header"><h2>Recent Activity</h2></div>
      <div class="card-body">
        <div class="log-list" id="logContainer"></div>
      </div>
    </div>
  </div>

  <!-- Row 4: Apex Classic -->
  <div style="margin-bottom:16px">
    <div class="card">
      <div class="card-header"><h2>Apex Classic</h2><span class="action" onclick="openApexModal()">Config &rarr;</span></div>
      <div class="card-body" id="apexContainer"></div>
    </div>
  </div>

  <!-- Quick Actions -->
    <div class="card" style="margin-bottom:0">
      <div class="card-header"><h2>Quick Actions</h2></div>
      <div class="card-body">
        <div class="qa-grid">
          <button class="btn btn-primary" onclick="doseAll()">Dose All</button>
          <button class="btn btn-danger" onclick="estop()">E-Stop</button>
          <button class="btn btn-warning" onclick="primeAll()">Prime All</button>
          <button class="btn btn-outline" onclick="reboot()">Reboot</button>
          <button class="btn btn-outline" onclick="syncNTP()">Sync NTP</button>
          <button class="btn btn-outline" onclick="openPumpCountModal()">Pump Config</button>
          <button class="btn btn-outline" onclick="exportConfig()">&#11015; Backup</button>
          <button class="btn btn-outline" onclick="document.getElementById('configFileInput').click()">&#11014; Restore</button>
          <input type="file" id="configFileInput" accept=".json,application/json" style="display:none" onchange="importConfig(this)">
        </div>
        <div class="timezone-label">&#9200; <span id="tzLabelText"></span> <select id="tzSelect" onchange="changeTimezone(this.value)"></select></div>
      </div>
    </div>
</div>

<script>
const API='/api?path=';
const MAX_SCHED_PER_PUMP=4;
const defaultPins=[32,33,25,26,27,14,12,13];
const timezones=[
  'PST (UTC-8)','PDT (UTC-7)',
  'MST (UTC-7)','MDT (UTC-6)',
  'CST (UTC-6)','CDT (UTC-5)',
  'EST (UTC-5)','EDT (UTC-4)',
  'AST (UTC-4)','BRT (UTC-3)','UTC','CET (UTC+1)','EET (UTC+2)',
  'MSK (UTC+3)','GST (UTC+4)','IST (UTC+5:30)','ICT (UTC+7)',
  'CST China (UTC+8)','JST (UTC+9)','AEST (UTC+10)','NZST (UTC+12)'
];
let currentTimezone='PST (UTC-8)';
let pumps=[], schedules=[], pumpCount=parseInt(localStorage.getItem('pumpCount'))||4;
let doseVols={};

function qs(s){return document.querySelector(s)}
function qsa(s){return document.querySelectorAll(s)}

function fetchJSON(url,cb){
  fetch(url).then(r=>r.json()).then(cb).catch(()=>setTimeout(()=>fetchJSON(url,cb),2000));
}

// === Toast ===
function toast(msg,type){
  type=type||'info';
  const c=document.getElementById('toastContainer');
  const icons={success:'&#10003;',error:'&#10007;',info:'&#9679;'};
  const t=document.createElement('div');
  t.className='toast '+type;
  t.innerHTML='<span class="t-icon">'+icons[type]+'</span>'+msg;
  c.appendChild(t);
  setTimeout(()=>{t.style.animation='toastOut .3s ease forwards';setTimeout(()=>t.remove(),300)},2800);
}

// === Theme ===
function toggleTheme(){
  document.body.classList.toggle('light');
  const light=document.body.classList.contains('light');
  document.getElementById('themeLabel').textContent=light?'Light':'Dark';
}

// === Clock ===
function updateClock(){
  const now=new Date();
  let h=now.getHours(),m=now.getMinutes(),s=now.getSeconds();
  const ampm=h>=12?'PM':'AM';
  h=h%12||12;
  const hh=String(h).padStart(2,'0'),mm=String(m).padStart(2,'0'),ss=String(s).padStart(2,'0');
  document.getElementById('clockDisplay').textContent=hh+':'+mm+':'+ss+' '+ampm;
  const months=['Jan','Feb','Mar','Apr','May','Jun','Jul','Aug','Sep','Oct','Nov','Dec'];
  document.getElementById('dateDisplay').textContent=months[now.getMonth()]+' '+now.getDate()+', '+now.getFullYear();
}

// === Timezone ===
function populateTimezone(){
  const sel=document.getElementById('tzSelect');
  sel.innerHTML=timezones.map(t=>'<option value="'+t+'"'+(t===currentTimezone?' selected':'')+'>'+t+'</option>').join('');
}
function tzOffsetLabel(min){
  const sign=min<0?'-':'+';
  const abs=Math.abs(min);
  const h=Math.floor(abs/60), m=abs%60;
  return 'UTC'+sign+h+(m?':'+String(m).padStart(2,'0'):'');
}
function tzLabelToMin(label){
  const m=label.match(/UTC([+-]\d{1,2}(?::\d{2})?)/);
  if(!m)return 0;
  const neg=m[1][0]==='-';
  const parts=m[1].slice(1).split(':').map(Number);
  let min=parts[0]*60+(parts[1]||0);
  return neg?-min:min;
}
function changeTimezone(val){
  currentTimezone=val;
  const min=tzLabelToMin(val);
  fetch('/api/timezone?min='+min).then(r=>r.json()).then(()=>{
    toast('Timezone set to '+val,'success');
  });
}
function applyDeviceTimezone(offsetMin){
  let best=timezones[0];
  let bestMin=tzLabelToMin(timezones[0]);
  for(const t of timezones){
    const m=tzLabelToMin(t);
    if(Math.abs(m-offsetMin)<Math.abs(bestMin-offsetMin)){best=t;bestMin=m}
  }
  currentTimezone=best;
  const sel=document.getElementById('tzSelect');
  sel.value=best;
  document.getElementById('tzLabelText').textContent=tzOffsetLabel(offsetMin)+' ('+best.split(' ')[0]+')';
}

// === Render ===
function renderAll(){renderPumps();renderSchedules();renderReservoirs()}

function renderPumps(){
  const tbody=document.getElementById('pumpTableBody');
  tbody.innerHTML='';
  for(let i=0;i<pumpCount;i++){
    const p=pumps[i]||{name:'Pump '+(i+1),rate:0,totalDosed:0,pin:defaultPins[i]||0,state:0,active:false};
    const states=['idle','priming','dosing','complete','error'];
    const labels=['Idle','Priming','Dosing','Complete','Error'];
    const stateCls=states[p.state]||'idle';
    const stateTxt=labels[p.state]||'Idle';
    const active=p.active!==false;
    const tr=document.createElement('tr');
    if(!active)tr.className='disabled';
    tr.innerHTML=
      '<td><label class="toggle-sm"><input type="checkbox" '+(active?'checked':'')+' onchange="togglePump('+i+',this.checked)"><span class="slider"></span></label></td>'+
      '<td><strong><span class="pump-name" onclick="editPumpName(this,'+i+')">'+p.name+'</span></strong></td>'+
      '<td style="font-size:.7rem;font-family:monospace">GPIO'+p.pin+'</td>'+
      '<td>'+p.rate.toFixed(1)+' mL/min</td>'+
      '<td>'+p.totalDosed.toFixed(0)+' mL</td>'+
      '<td><span class="badge '+stateCls+'">'+stateTxt+'</span></td>'+
      '<td><div class="pump-actions"><input type="number" value="'+(doseVols[i]||10)+'" id="pumpVol'+i+'" style="width:42px" oninput="doseVols['+i+']=this.value"><button class="btn btn-success btn-sm" onclick="dosePump('+i+')">Dose</button><button class="btn btn-primary btn-sm" onclick="calibratePump('+i+')">Cal</button><button class="btn btn-warning btn-sm" onclick="primePump('+i+')">Prime</button></div></td>';
    tbody.appendChild(tr);
  }
}
function togglePump(idx,on){
  fetch('/api/pump?pump='+idx+'&active='+on).then(()=>{loadAll()});
  toast((on?'Enabled':'Disabled')+' '+(pumps[idx]?pumps[idx].name:'Pump '+(idx+1)),on?'success':'info');
}
function dosePump(idx){
  if(pumps[idx]&&pumps[idx].active===false){toast('Pump is disabled','error');return}
  const vol=document.getElementById('pumpVol'+idx).value;
  fetch('/api/dose?pump='+idx+'&vol='+vol).then(r=>r.json()).then(d=>{
    if(d.ok)toast('Dosing '+(pumps[idx]?pumps[idx].name:'Pump '+(idx+1))+' '+vol+'mL','success');
    else toast('Dose failed','error');
    loadAll();
  });
}
function primePump(idx){
  if(pumps[idx]&&pumps[idx].active===false){toast('Pump is disabled','error');return}
  fetch('/api/dose?pump='+idx+'&vol=5').then(()=>{toast('Priming '+(pumps[idx]?pumps[idx].name:'Pump '+(idx+1)),'info');loadAll()});
}

// === Calibrate ===
let calPumpIdx=0, calStartTime=0, calTimer=null;
function calibratePump(idx){
  if(pumps[idx]&&pumps[idx].active===false){toast('Pump is disabled','error');return}
  calPumpIdx=idx;
  document.getElementById('calPumpName').textContent=pumps[idx]?pumps[idx].name:'Pump '+(idx+1);
  document.getElementById('calStep1').style.display='block';
  document.getElementById('calStep2').style.display='none';
  document.getElementById('calResult').style.display='none';
  document.getElementById('calModal').style.display='flex';
}
function closeCalModal(){
  if(calTimer){clearTimeout(calTimer);calTimer=null}
  document.getElementById('calModal').style.display='none';
}
document.getElementById('calModal').addEventListener('click',function(e){if(e.target===this)closeCalModal()});
function calRun(){
  const sec=parseInt(document.getElementById('calDuration').value)||30;
  const p=pumps[calPumpIdx]||{rate:100};
  const vol = (p.rate * sec / 60).toFixed(1);
  fetch('/api/dose?pump='+calPumpIdx+'&vol=9999').then(r=>r.json()).then(d=>{
    if(!d.ok){toast('Failed to start pump','error');return}
    calStartTime=Date.now();
    toast('Pump running for '+sec+'s...','info');
    calTimer=setTimeout(function(){
      fetch('/api/dose?pump='+calPumpIdx+'&vol=0&cancel=1').then(()=>{
        const actualSec=((Date.now()-calStartTime)/1000).toFixed(1);
        document.getElementById('calActualSec').textContent=actualSec;
        document.getElementById('calStep1').style.display='none';
        document.getElementById('calStep2').style.display='block';
        toast('Pump stopped, enter measured volume','success');
      });
    },sec*1000);
  });
}
function calSave(){
  const actualSec=parseFloat(document.getElementById('calActualSec').textContent)||30;
  const measured=parseFloat(document.getElementById('calVolume').value);
  if(!measured||measured<0.1){toast('Enter a valid volume','error');return}
  const newRate=(measured/(actualSec/60)).toFixed(1);
  document.getElementById('calNewRate').textContent=newRate;
  document.getElementById('calResult').style.display='block';
  fetch('/api/pump?pump='+calPumpIdx+'&rate='+newRate).then(()=>{
    toast('Calibration saved: '+newRate+' mL/min','success');
    closeCalModal();
    loadAll();
  });
}

// === Pump Name Edit ===
function editPumpName(el,idx){
  if(el.querySelector('input'))return;
  const orig=el.textContent;
  const inp=document.createElement('input');
  inp.className='pump-name-input';
  inp.value=orig;
  inp.onblur=function(){
    const val=this.value.trim()||orig;
    fetch('/api/pump?pump='+idx+'&name='+encodeURIComponent(val)).then(()=>{loadAll();toast('Pump renamed to "'+val+'"','info')});
  };
  inp.onkeydown=function(e){
    if(e.key==='Enter')this.blur();
    if(e.key==='Escape'){this.value=orig;this.blur();return}
  };
  el.textContent='';el.appendChild(inp);inp.focus();inp.select();
}

// === Pump Count ===
function openPumpCountModal(){
  const grid=document.getElementById('pumpCountGrid');
  grid.innerHTML='';
  for(let i=1;i<=8;i++){
    const d=document.createElement('div');
    d.className='pc-chip'+(i===pumpCount?' active':'');
    d.dataset.n=i;d.textContent=i;
    d.addEventListener('click',function(){
      qsa('#pumpCountGrid .pc-chip').forEach(x=>x.classList.remove('active'));
      this.classList.add('active');
    });
    grid.appendChild(d);
  }
  document.getElementById('pumpCountModal').style.display='flex';
}
function closePumpCountModal(){document.getElementById('pumpCountModal').style.display='none'}
document.getElementById('pumpCountModal').addEventListener('click',function(e){if(e.target===this)closePumpCountModal()});
function applyPumpCount(){
  const n=parseInt(document.querySelector('#pumpCountGrid .pc-chip.active').dataset.n);
  if(n===pumpCount){closePumpCountModal();return}
  pumpCount=n;
  localStorage.setItem('pumpCount',n);
  closePumpCountModal();
  renderAll();
  toast('Pump count set to '+n,'success');
}

// === Schedules ===
function renderSchedules(){
  const c=document.getElementById('schedContainer');
  c.innerHTML='';
  let any=false;
  for(let p=0;p<pumpCount;p++){
    const pScheds=schedules.filter(s=>s.pumpIndex===p);
    if(!pScheds.length)continue;
    any=true;
    const days=['Sun','Mon','Tue','Wed','Thu','Fri','Sat'];
    const div=document.createElement('div');
    div.className='sched-per-pump';
    div.innerHTML=
      '<div class="spp-header"><span>'+(pumps[p]?pumps[p].name:'Pump '+(p+1))+'</span><span class="spp-count">'+pScheds.length+'/'+MAX_SCHED_PER_PUMP+'</span></div>'+
      pScheds.map(s=>{
        let h24=s.hour;const ampm=h24>=12?'PM':'AM';let h12=h24%12||12;const hh=String(h12).padStart(2,'0'),m=String(s.minute).padStart(2,'0');
        const dayStr=days.filter((_,d)=>(s.days>>d)&1).join(', ')||'None';
        const sIdx=schedules.indexOf(s);
        return '<div class="sched-row"><span class="sched-time">'+hh+':'+m+' '+ampm+'</span><span class="sched-detail">'+s.doseML.toFixed(1)+' mL</span><span class="sched-days">'+dayStr+'</span><button class="btn btn-outline btn-sm" onclick="removeSchedule('+sIdx+')">Remove</button></div>';
      }).join('');
    c.appendChild(div);
  }
  if(!any)c.innerHTML='<div style="font-size:.75rem;color:var(--text4);padding:8px 0">No schedules configured</div>';
}
function removeSchedule(idx){
  fetch('/api/schedule/remove?index='+idx).then(()=>{toast('Schedule removed','info');loadAll()});
}

// === Schedule Modal ===
function openSchedModal(){
  const sel=document.getElementById('sPump');
  sel.innerHTML='';
  for(let i=0;i<pumpCount;i++){
    const sCount=schedules.filter(s=>s.pumpIndex===i).length;
    const disabled=sCount>=MAX_SCHED_PER_PUMP;
    const opt=document.createElement('option');
    opt.value=i;
    opt.textContent=(pumps[i]?pumps[i].name:'Pump '+(i+1))+(disabled?' (full)':'');
    opt.disabled=disabled;
    sel.appendChild(opt);
  }
  document.getElementById('schedModal').style.display='flex';
}
function closeSchedModal(){document.getElementById('schedModal').style.display='none'}
document.getElementById('schedModal').addEventListener('click',function(e){if(e.target===this)closeSchedModal()});
qsa('#dayChips .chip').forEach(c=>{c.addEventListener('click',function(){this.classList.toggle('active')})});
qsa('#ampmChips .chip').forEach(c=>{c.addEventListener('click',function(){qsa('#ampmChips .chip').forEach(x=>{x.style.background='transparent';x.style.color='var(--text3)';x.style.borderColor='var(--border)'});this.style.background='var(--accent)';this.style.color='#14161b';this.style.borderColor='var(--accent)'})});
function saveSchedule(){
  const pump=parseInt(document.getElementById('sPump').value);
  let h=parseInt(document.getElementById('sHour').value)||8;
  const m=document.getElementById('sMin').value.padStart(2,'0');
  const v=document.getElementById('sVol').value;
  const isPM=document.querySelector('#ampmChips .chip.active').dataset.ampm==='PM';
  if(isPM&&h<12)h+=12;
  if(!isPM&&h===12)h=0;
  const hh=String(h).padStart(2,'0');
  const days=[...qsa('#dayChips .chip.active')].map(c=>c.dataset.d).join(',');
  const sCount=schedules.filter(s=>s.pumpIndex===pump).length;
  if(sCount>=MAX_SCHED_PER_PUMP){toast('Max '+MAX_SCHED_PER_PUMP+' schedules for this pump','error');return}
  const dayMask=[...qsa('#dayChips .chip.active')].reduce((m,c)=>m|parseInt(c.dataset.d),0);
  fetch('/api/schedule?pump='+pump+'&hour='+hh+'&minute='+m+'&vol='+v+'&days='+dayMask)
    .then(r=>r.json()).then(d=>{
      if(d.ok){toast('Schedule saved','success');closeSchedModal();loadAll()}
      else toast('Failed to save','error');
    });
}

// === Reservoirs ===
function renderReservoirs(){
  const c=document.getElementById('reservoirContainer');
  c.innerHTML='';
  const colorsR=['var(--blue)','var(--green)','#f59e0b','#8b5cf6','#ec4899','#14b8a6','#f97316','#6366f1'];
  for(let i=0;i<pumpCount;i++){
    const p=pumps[i]||{name:'Pump '+(i+1),capacity:5000,reservoirLevel:100,reservoirRemaining:5000};
    const cap=p.capacity||5000;
    const rem=typeof p.reservoirRemaining==='number'?p.reservoirRemaining:cap;
    const pct=cap>0?Math.round(rem/cap*100):0;
    const col=colorsR[i%colorsR.length];
    const div=document.createElement('div');
    div.className='reservoir-row';
    div.innerHTML=
      '<div class="r-name"><span class="r-name-text" onclick="editReservoirName(this,'+i+')">'+p.name+'</span><div class="r-meta"><span class="r-cap-text" onclick="editReservoirCap(this,'+i+')">'+cap.toFixed(0)+' mL</span> capacity</div></div>'+
      '<div class="r-level"><div class="bar"><span style="width:'+pct+'%;background:'+col+'"></span></div></div>'+
      '<span class="r-pct" style="color:'+col+'">'+pct+'% <small>('+rem.toFixed(0)+' mL)</small></span>';
    c.appendChild(div);
  }
}
function editReservoirName(el,idx){
  if(el.querySelector('input'))return;
  const orig=el.textContent;
  const inp=document.createElement('input');
  inp.className='pump-name-input';
  inp.value=orig;
  inp.style.width='90px';
  inp.onblur=function(){
    const val=this.value.trim()||orig;
    el.textContent=val;
    fetch('/api/pump?pump='+idx+'&name='+encodeURIComponent(val)).then(()=>{loadAll();toast('Reservoir renamed to "'+val+'"','info')});
  };
  inp.onkeydown=function(e){
    if(e.key==='Enter')this.blur();
    if(e.key==='Escape'){this.value=orig;this.blur();return}
  };
  el.textContent='';el.appendChild(inp);inp.focus();inp.select();
}
function editReservoirCap(el,idx){
  if(el.querySelector('input'))return;
  const orig=parseFloat(el.textContent)||5000;
  const inp=document.createElement('input');
  inp.className='pump-name-input';
  inp.value=orig.toFixed(0);
  inp.style.width='70px';
  inp.type='number';
  inp.step='100';
  inp.min='100';
  inp.onblur=function(){
    const val=parseFloat(this.value);
    if(!val||val<100){el.textContent=orig.toFixed(0)+' mL';return}
    el.textContent=val.toFixed(0)+' mL';
    fetch('/api/pump?pump='+idx+'&capacity='+val).then(()=>{loadAll();toast('Capacity set to '+val.toFixed(0)+' mL','info')});
  };
  inp.onkeydown=function(e){
    if(e.key==='Enter')this.blur();
    if(e.key==='Escape'){this.value=orig;this.blur();return}
  };
  el.textContent='';el.appendChild(inp);inp.focus();inp.select();
}
function refillAll(){
  fetch('/api/refill').then(r=>r.json()).then(d=>{
    if(d.ok){toast('All reservoirs refilled','success');loadAll()}
    else toast('Refill failed','error');
  });
}

// === Apex ===
function renderApex(data){
  const c=document.getElementById('apexContainer');
  let html='';
  for(let u=0;u<data.units.length;u++){
    const unit=data.units[u];
    const anyOn=data.units.some(x=>x.enabled);
    if(!anyOn){
      html='<div style="text-align:center;padding:20px;color:var(--text4);font-size:.85rem">Apex not configured. <span class="action" onclick="openApexModal()">Configure now</span></div>';
      break;
    }
    if(!unit.enabled) continue;
    const ago=Math.floor((Date.now()-unit.lastUpdate)/1000);
    const dotClass=unit.connected?'on':'off';
    const connClass=unit.connected?'conn':'disc';
    const connText=unit.connected?'Connected':'Disconnected';
    html+='<div style="margin-bottom:'+(u<data.units.length-1?'12':'0')+'px"><div style="font-size:.7rem;font-weight:600;color:var(--text4);text-transform:uppercase;letter-spacing:.05em;margin-bottom:6px">Apex '+(u+1)+' &mdash; '+unit.ip+'</div>';
    html+='<div class="apex-grid">';
    const vis=unit.probes.filter(p=>p.display);
    if(vis.length===0){
      html+='<div style="grid-column:1/-1;text-align:center;padding:12px;color:var(--text4);font-size:.78rem">No probes shown</div>';
    }else{
      html+=vis.map(p=>'<div class="apex-probe"><div class="p-label">'+p.label+'</div><div class="p-value">'+p.value.toFixed(2)+'</div></div>').join('');
    }
    html+='</div>';
    html+='<div class="apex-status"><span class="dot '+dotClass+'"></span><span class="'+connClass+'">'+connText+'</span><span> &middot; '+ago+'s ago</span></div>';
    html+='</div>';
  }
  c.innerHTML=html;
}
function openApexModal(){
  fetchJSON(API+'apex',function(data){
    let html='';
    for(let u=0;u<data.units.length;u++){
      const unit=data.units[u];
      if(u>0)html+='<hr style="border:none;border-top:1px solid var(--border);margin:14px 0">';
      html+='<label style="font-weight:600;color:var(--text3)">Apex '+(u+1)+'</label>';
      html+='<label>IP Address</label><input type="text" class="apex-ip" data-unit="'+u+'" value="'+unit.ip+'" placeholder="192.168.1.100">';
      html+='<label>Port</label><input type="number" class="apex-port" data-unit="'+u+'" value="80" min="1" max="65535">';
      html+='<label>Username</label><input type="text" class="apex-user" data-unit="'+u+'" value="admin" placeholder="admin">';
      html+='<label>Password</label><input type="password" class="apex-pass" data-unit="'+u+'" value="" placeholder="password">';
      html+='<label>Sync interval (minutes)</label><input type="number" class="apex-poll" data-unit="'+u+'" value="'+Math.round((unit.pollIntervalMs||3600000)/60000)+'" min="1" max="1440">';
      html+='<div style="display:flex;align-items:center;gap:8px;margin-top:6px">';
      html+='<input type="checkbox" class="apex-enabled" data-unit="'+u+'" style="width:auto"'+(unit.enabled?' checked':'')+'>';
      html+='<label style="margin:0;font-size:.8rem">Enable Apex '+(u+1)+'</label></div>';
      // probe visibility toggles
      if(unit.probes && unit.probes.length>0){
        html+='<div style="margin-top:8px;font-size:.72rem;color:var(--text4)">Show probes:</div>';
        html+='<div style="display:flex;flex-wrap:wrap;gap:6px;margin-top:4px">';
        for(let pi=0;pi<unit.probes.length;pi++){
          const p=unit.probes[pi];
          html+='<label style="display:flex;align-items:center;gap:4px;font-size:.72rem;background:var(--bg);padding:3px 8px;border-radius:4px;border:1px solid var(--border);cursor:pointer">';
          html+='<input type="checkbox" class="apex-probe-toggle" data-unit="'+u+'" data-idx="'+pi+'"'+(p.display?' checked':'')+'>';
          html+=p.label+'</label>';
        }
        html+='</div>';
      }
    }
    document.getElementById('apexConfigFields').innerHTML=html;
  });
  document.getElementById('apexModal').style.display='flex';
}
function closeApexModal(){
  document.getElementById('apexModal').style.display='none';
}
function saveApexConfig(){
  const ips=document.querySelectorAll('.apex-ip');
  const ports=document.querySelectorAll('.apex-port');
  const users=document.querySelectorAll('.apex-user');
  const passes=document.querySelectorAll('.apex-pass');
  const enableds=document.querySelectorAll('.apex-enabled');
  const polls=document.querySelectorAll('.apex-poll');
  let pending=0,ok=true;
  for(let u=0;u<ips.length;u++){
    const ip=ips[u].value.trim();
    if(!ip){toast('Enter IP for Apex '+(u+1),'error');ok=false;continue}
    const port=ports[u].value;
    const user=users[u].value.trim();
    const pass=passes[u].value;
    const enabled=enableds[u].checked?'true':'false';
    const pollMin=parseInt(polls[u].value)||60;
    // compute probe mask
    const toggles=document.querySelectorAll('.apex-probe-toggle[data-unit="'+u+'"]');
    let mask=0;
    toggles.forEach(function(t,i){if(t.checked)mask|=1<<i});
    pending++;
    let url='/api/apex?unit='+u+'&ip='+encodeURIComponent(ip)+'&port='+port+'&enabled='+enabled+'&probeMask='+mask+'&pollIntervalMs='+(pollMin*60000);
    if(user)url+='&username='+encodeURIComponent(user);
    if(pass)url+='&password='+encodeURIComponent(pass);
    fetch(url).then(r=>r.json()).then(d=>{
      if(!d.ok)ok=false;
      pending--;
      if(pending===0){
        if(ok){toast('Apex config saved','success');closeApexModal();loadAll()}
        else toast('Failed to save Apex config','error');
      }
    });
  }
}

// === Logs ===
function renderLogs(data){
  const c=document.getElementById('logContainer');
  c.innerHTML=data.slice(-9).map(function(l){
    let cls='log-info';
    if(l.indexOf('[W]')>=0)cls='log-warn';
    else if(l.indexOf('[E]')>=0)cls='log-err';
    return '<div><span class="log-time">[...]</span><span class="'+cls+'">'+l+'</span></div>';
  }).join('');
}

// === Status ===
function renderStatus(data){
  const days=Math.floor(data.uptime/24);
  document.getElementById('kpiUptime').innerHTML=data.uptime.toFixed(1)+'<span class="kpi-unit">h</span>';
  document.getElementById('kpiUptimeSub').textContent=days+' days';
  document.getElementById('kpiUptimeBar').style.width=Math.min(data.uptime/10,100)+'%';
  const vol=data.totalVolume||0;
  document.getElementById('kpiVolume').innerHTML=vol>=1000?(vol/1000).toFixed(1)+'<span class="kpi-unit">L</span>':vol.toFixed(0)+'<span class="kpi-unit">mL</span>';
  document.getElementById('kpiVolumeSub').textContent=vol.toFixed(0)+' mL total';
  document.getElementById('kpiVolumeBar').style.width=Math.min(vol/1000*100,100)+'%';
  let activePumps=0;for(let i=0;i<pumpCount;i++){if(pumps[i]&&pumps[i].active!==false)activePumps++}
  document.getElementById('kpiDoses').innerHTML=activePumps+'<span class="kpi-unit">/'+pumpCount+'</span>';
  document.getElementById('kpiDosesSub').textContent=pumpCount+' channels';
  document.getElementById('kpiDosesBar').style.width=(pumpCount>0?activePumps/pumpCount*100:0)+'%';
  const rssi=typeof data.rssi==='number'?data.rssi:0;
  const pct=Math.max(0,Math.min(100,Math.round((rssi+90)*100/60)));
  document.getElementById('kpiSignal').innerHTML=rssi+'<span class="kpi-unit">dBm</span>';
  document.getElementById('kpiSignalSub').textContent=data.wifiConnected?'Connected':'Disconnected';
  document.getElementById('kpiSignalBar').style.width=pct+'%';
  const dot=document.getElementById('onlineDot');
  const lbl=document.getElementById('onlineLabel');
  if(dot){dot.className='hmi-led '+(data.wifiConnected?'on':'off')}
  if(lbl){lbl.textContent=data.wifiConnected?'Online':'Offline'}
  const ipEl=document.getElementById('ipDisplay');
  if(ipEl)ipEl.textContent=data.ip||'--';
  const dtEl=document.getElementById('deviceTimeDisplay');
  if(dtEl){
    const synced=data.clockSynced;
    dtEl.textContent='device clock: '+(data.deviceTime||'--')+(synced?'':' (NOT synced!)');
    dtEl.style.color=synced?'var(--text4)':'var(--red)';
  }
  if(typeof data.tzOffsetMin==='number'&&data.tzOffsetMin!==window._lastTz){window._lastTz=data.tzOffsetMin;applyDeviceTimezone(data.tzOffsetMin)}
}

// === Quick Actions ===
function doseAll(){
  for(let i=0;i<pumpCount;i++){
    const vol=document.getElementById('pumpVol'+i);
    const v=vol?vol.value:10;
    fetch('/api/dose?pump='+i+'&vol='+v);
  }
  toast('Dosing all pumps','success');
  setTimeout(loadAll,1000);
}
function estop(){
  for(let i=0;i<pumpCount;i++){fetch('/api/dose?pump='+i+'&vol=0&cancel=1')}
  toast('Emergency stop activated','error');
  loadAll();
}
function primeAll(){
  for(let i=0;i<pumpCount;i++){fetch('/api/dose?pump='+i+'&vol=5')}
  toast('Priming all pumps','info');
  setTimeout(loadAll,1000);
}
function reboot(){
  toast('Rebooting controller...','info');
  fetch('/api/reset?reboot=1').then(()=>{});
}
function syncNTP(){
  toast('Syncing NTP...','info');
  fetch('/api/ntp').then(()=>{toast('NTP sync initiated','success')});
}
function exportConfig(){
  toast('Downloading backup...','info');
  fetch('/api/config/export').then(r=>r.json()).then(data=>{
    const blob=new Blob([JSON.stringify(data,null,2)],{type:'application/json'});
    const a=document.createElement('a');
    const d=new Date();
    const stamp=d.getFullYear()+'-'+String(d.getMonth()+1).padStart(2,'0')+'-'+String(d.getDate()).padStart(2,'0');
    a.href=URL.createObjectURL(blob);
    a.download='pro-simple-config-'+stamp+'.json';
    a.click();
    URL.revokeObjectURL(a.href);
    toast('Backup downloaded','success');
  }).catch(()=>toast('Backup failed','error'));
}
function importConfig(input){
  const file=input.files[0];
  if(!file)return;
  const reader=new FileReader();
  reader.onload=function(){
    toast('Restoring config...','info');
    fetch('/api/config/import',{method:'POST',headers:{'Content-Type':'application/json'},body:reader.result})
      .then(r=>r.json()).then(d=>{
        if(d.ok){toast('Config restored','success');loadAll()}
        else toast('Restore failed: '+(d.error||'unknown'),'error');
      }).catch(()=>toast('Restore failed','error'));
  };
  reader.readAsText(file);
  input.value='';
}

// === Load All ===
function loadAll(){
  // skip if inline edit is active (input focused)
  if(document.querySelector('.pump-name-input'))return;
  if(document.activeElement&&document.activeElement.id&&document.activeElement.id.indexOf('pumpVol')===0)return;
  fetchJSON(API+'status',renderStatus);
  fetchJSON(API+'pumps',function(data){pumps=data;renderPumps();renderReservoirs()});
  fetchJSON(API+'schedules',function(data){schedules=data;renderSchedules()});
  fetchJSON(API+'logs',renderLogs);
  fetchJSON(API+'apex',renderApex);
}

// === Init ===
populateTimezone();
updateClock();
setInterval(updateClock,1000);
loadAll();
setInterval(loadAll,5000);
</script>
</body>
</html>)rawliteral"));
}
