#include "web_server.h"
#include "dashboard.h"
#include "dosing.h"
#include "pump.h"
#include "scheduler.h"
#include "logger.h"

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
  else if (path == "logs")   json = Dashboard::renderLogJSON();
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
  _server.send(200, "application/json", F("{\"ok\":true}"));
}

void HttpServer::_handleRefill() {
  for (uint8_t i = 0; i < PUMP_COUNT; i++) {
    PumpConfig* cfg = Pump::getConfig(i);
    if (cfg) {
      cfg->reservoirLevel = 100;
      Pump::setConfig(i, *cfg);
    }
  }
  _server.send(200, "application/json", F("{\"ok\":true}"));
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
  _server.send(200, "application/json",
               ok ? F("{\"ok\":true}") : F("{\"ok\":false}"));
}

void HttpServer::_handleScheduleRemove() {
  int idx = _server.arg(F("index")).toInt();
  Scheduler::removeSchedule(idx);
  _server.send(200, "application/json", F("{\"ok\":true}"));
}

void HttpServer::_handleResetTotals() {
  for (uint8_t i = 0; i < PUMP_COUNT; i++) {
    Pump::resetTotal(i);
  }
  _server.send(200, "application/json", F("{\"ok\":true}"));
}

void HttpServer::_handleNotFound() {
  _server.send(404, "application/json", F("{\"error\":\"not found\"}"));
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
  --bg:#0a0e17;--bg2:#111827;--bg3:#1a2332;
  --border:#1f2937;--border2:#2a3a5c;
  --text:#c8d0dc;--text2:#e5e7eb;--text3:#9ca3af;--text4:#4b5563;--text5:#374151;
  --blue:#3b82f6;--green:#10b981;--red:#ef4444;
}
body.light{
  --bg:#f5f7fa;--bg2:#fff;--bg3:#f0f4ff;
  --border:#e5e7eb;--border2:#d1d5db;
  --text:#374151;--text2:#111827;--text3:#6b7280;--text4:#9ca3af;--text5:#d1d5db;
}
body{
  font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,Oxygen,sans-serif;
  background:var(--bg);color:var(--text);padding:32px;
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
  display:flex;align-items:center;justify-content:space-between;margin-bottom:28px;flex-wrap:wrap;gap:12px;
}
.brand h1{font-size:1.3rem;font-weight:700;color:var(--text2);letter-spacing:-.02em}
.brand h1 span{color:var(--blue)}
.brand .tag{font-size:.75rem;color:var(--text4);margin-top:2px}
.user-info{display:flex;align-items:center;gap:14px;font-size:.8rem;color:var(--text3)}
.header-clock{display:flex;flex-direction:column;align-items:flex-end;line-height:1.3}
.header-clock #clockDisplay{font-size:.9rem;font-weight:600;color:var(--text2);font-family:monospace}
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
/* KPI */
.kpi-grid{display:grid;grid-template-columns:repeat(4,1fr);gap:14px;margin-bottom:24px}
.kpi-card{
  background:var(--bg2);border-radius:12px;padding:18px 20px;
  border:1px solid var(--border);transition:all .2s;
}
.kpi-card:hover{border-color:var(--blue);box-shadow:0 0 20px rgba(59,130,246,.05)}
.kpi-card .kpi-label{font-size:.7rem;font-weight:600;text-transform:uppercase;letter-spacing:.05em;color:var(--text4);margin-bottom:5px}
.kpi-card .kpi-value{font-size:1.4rem;font-weight:700;color:var(--text2)}
.kpi-card .kpi-change{font-size:.72rem;margin-top:4px}
.kpi-card .kpi-change.up{color:var(--green)}
.kpi-card .kpi-change.down{color:var(--red)}
/* Grid */
.cols-2{display:grid;grid-template-columns:1fr 1fr;gap:14px;margin-bottom:24px}
.cols-2-66{display:grid;grid-template-columns:1.6fr 1fr;gap:14px;margin-bottom:24px}
.cols-2-35{display:grid;grid-template-columns:1fr 1.3fr;gap:14px;margin-bottom:24px}
/* Cards */
.card{
  background:var(--bg2);border-radius:12px;border:1px solid var(--border);
  overflow:hidden;transition:border-color .2s;
}
.card:hover{border-color:var(--border2)}
.card-header{
  padding:14px 20px;border-bottom:1px solid var(--border);
  display:flex;align-items:center;justify-content:space-between;flex-wrap:wrap;gap:8px;
}
.card-header h2{font-size:.82rem;font-weight:600;color:var(--text2)}
.card-header .action{font-size:.72rem;color:var(--blue);cursor:pointer;font-weight:500}
.card-header .action:hover{color:#60a5fa}
.card-body{padding:16px 20px}
/* Tables */
table{width:100%;border-collapse:collapse;font-size:.78rem}
th{
  text-align:left;padding:8px 10px;color:var(--text4);font-weight:500;
  font-size:.68rem;text-transform:uppercase;letter-spacing:.04em;
  border-bottom:1px solid var(--border);
}
td{padding:8px 10px;border-bottom:1px solid var(--border);color:var(--text3)}
tr:last-child td{border-bottom:none}
tr:hover td{background:rgba(59,130,246,.03)}
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
.badge{display:inline-block;padding:2px 10px;border-radius:4px;font-size:.65rem;font-weight:600}
.badge.active{background:rgba(16,185,129,.1);color:var(--green);border:1px solid rgba(16,185,129,.2)}
.badge.idle{background:rgba(75,85,99,.15);color:var(--text3);border:1px solid rgba(75,85,99,.2)}
.badge.error{background:rgba(239,68,68,.1);color:var(--red);border:1px solid rgba(239,68,68,.2)}
.badge.progress{background:rgba(59,130,246,.1);color:var(--blue);border:1px solid rgba(59,130,246,.2)}
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
  background:var(--text3);border-radius:50%;transition:.25s;
}
.toggle-sm input:checked+.slider{background:var(--green)}
.toggle-sm input:checked+.slider::before{background:#fff;transform:translateX(14px)}
/* Pump controls */
.pump-actions{display:flex;gap:4px;align-items:center;flex-wrap:wrap}
.pump-actions input{
  width:44px;padding:3px 6px;background:var(--bg);border:1px solid var(--border);
  border-radius:4px;font-size:.7rem;text-align:center;color:var(--text2);
}
.pump-actions input:focus{outline:none;border-color:var(--blue)}
/* Buttons */
.btn{
  padding:4px 12px;border-radius:6px;font-size:.7rem;font-weight:500;
  border:none;cursor:pointer;transition:all .15s;font-family:inherit;white-space:nowrap;
}
.btn-primary{background:var(--blue);color:#fff}
.btn-primary:hover{background:#2563eb}
.btn-success{background:var(--green);color:#fff}
.btn-success:hover{background:#059669}
.btn-danger{background:var(--red);color:#fff}
.btn-danger:hover{background:#dc2626}
.btn-warning{background:#f59e0b;color:#fff}
.btn-warning:hover{background:#d97706}
.btn-outline{background:transparent;border:1px solid var(--border);color:var(--text3)}
.btn-outline:hover{background:rgba(255,255,255,.03);border-color:var(--blue);color:var(--text2)}
.btn-sm{padding:2px 8px;font-size:.65rem}
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
  padding:5px 0;border-bottom:1px solid var(--border);font-size:.75rem;
}
.sched-row:last-child{border-bottom:none}
.sched-time{font-weight:600;color:var(--blue);min-width:50px;font-feature-settings:'tnum'1}
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
/* Chart */
.chart-placeholder{height:80px;display:flex;align-items:flex-end;gap:4px;padding:8px 0}
.chart-placeholder .bar{
  flex:1;background:linear-gradient(180deg,var(--blue),#1d4ed8);
  border-radius:2px 2px 0 0;min-height:6px;opacity:.7;
}
.chart-placeholder .bar:nth-child(odd){opacity:1}
/* Log */
.log-list{font-size:.7rem;line-height:1.7;color:var(--text4);max-height:190px;overflow-y:auto}
.log-list .log-time{color:var(--text5);margin-right:6px}
.log-list .log-info{color:var(--text3)}
.log-list .log-warn{color:#d97706}
/* Quick actions */
.qa-grid{display:grid;grid-template-columns:1fr 1fr;gap:6px}
.qa-grid button{width:100%}
.time-row{display:flex;gap:6px;align-items:center;margin-top:8px;padding-top:8px;border-top:1px solid var(--border)}
.time-row input{flex:1;padding:4px 8px;background:var(--bg);border:1px solid var(--border);border-radius:4px;font-size:.7rem;color:var(--text2);font-family:inherit}
.time-row input:focus{outline:none;border-color:var(--blue)}
.timezone-label{font-size:.65rem;color:var(--text4);margin-top:4px}
.timezone-label select{background:var(--bg);border:1px solid var(--border);color:var(--text2);padding:2px 4px;border-radius:4px;font-size:.65rem;font-family:inherit}
/* Modal */
.modal-overlay{
  position:fixed;inset:0;background:rgba(0,0,0,.6);z-index:9998;
  display:none;align-items:center;justify-content:center;
}
.modal{
  background:var(--bg2);border:1px solid var(--border);border-radius:14px;
  padding:24px;max-width:420px;width:90%;box-shadow:0 16px 48px rgba(0,0,0,.4);
  max-height:90vh;overflow-y:auto;
}
.modal h3{font-size:1rem;font-weight:600;color:var(--text2);margin-bottom:16px}
.modal label{font-size:.75rem;color:var(--text4);display:block;margin-bottom:4px}
.modal input,.modal select{
  width:100%;padding:7px 10px;background:var(--bg);border:1px solid var(--border);
  border-radius:6px;color:var(--text2);font-size:.8rem;font-family:inherit;margin-bottom:12px;
}
.modal input:focus,.modal select:focus{outline:none;border-color:var(--blue)}
.modal .day-chips{display:flex;gap:4px;flex-wrap:wrap;margin-bottom:12px}
.modal .day-chips .chip{
  padding:4px 10px;border-radius:6px;font-size:.72rem;font-weight:500;
  border:1px solid var(--border);cursor:pointer;transition:all .15s;
  color:var(--text3);background:transparent;
}
.modal .day-chips .chip.active{background:var(--blue);color:#fff;border-color:var(--blue)}
.modal .modal-actions{display:flex;gap:8px;justify-content:flex-end;margin-top:8px}
/* Pump count grid */
.pump-count-grid{display:grid;grid-template-columns:repeat(4,1fr);gap:8px;margin-bottom:16px}
.pump-count-grid .pc-chip{
  padding:10px;border-radius:8px;font-size:.9rem;font-weight:600;text-align:center;
  border:2px solid var(--border);cursor:pointer;transition:all .15s;
  color:var(--text3);background:transparent;
}
.pump-count-grid .pc-chip:hover{border-color:var(--blue);color:var(--text2)}
.pump-count-grid .pc-chip.active{background:var(--blue);color:#fff;border-color:var(--blue)}
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
      <span class="chip active" data-ampm="AM" style="padding:4px 14px;border-radius:6px;font-size:.72rem;font-weight:500;border:1px solid var(--border);cursor:pointer;background:var(--blue);color:#fff;border-color:var(--blue)">AM</span>
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

<div class="app">
  <div class="top-bar">
    <div class="brand">
      <h1>Pro-Simple <span>|</span> Dosing Controller</h1>
      <div class="tag">ESP32 &middot; v1.0.0 &middot; <span id="onlineDot" style="color:var(--green)">&#9679;</span> <span id="onlineLabel">Online</span></div>
    </div>
    <div class="user-info">
      <div class="theme-toggle-wrap">
        <span id="themeLabel" style="font-size:.7rem;color:var(--text4);user-select:none">Dark</span>
        <div class="theme-toggle" id="themeToggle" onclick="toggleTheme()">
          <div class="knob"></div>
        </div>
      </div>
      <div class="header-clock"><span id="clockDisplay">12:00:00 PM</span><span id="dateDisplay">Jan 1, 2026</span></div>
      <span id="ipDisplay">--</span>
      <div class="avatar">PS</div>
    </div>
  </div>

  <!-- Row 1: KPI -->
  <div class="kpi-grid">
    <div class="kpi-card">
      <div class="kpi-label">System Uptime</div>
      <div class="kpi-value" id="kpiUptime">--</div>
      <div class="kpi-change up" id="kpiUptimeSub">--</div>
    </div>
    <div class="kpi-card">
      <div class="kpi-label">Total Dispensed</div>
      <div class="kpi-value" id="kpiVolume">--</div>
      <div class="kpi-change up" id="kpiVolumeSub">--</div>
    </div>
    <div class="kpi-card">
      <div class="kpi-label">Total Doses</div>
      <div class="kpi-value" id="kpiDoses">--</div>
      <div class="kpi-change up" id="kpiDosesSub">--</div>
    </div>
    <div class="kpi-card">
      <div class="kpi-label">Active Alarms</div>
      <div class="kpi-value" id="kpiAlarms">0</div>
      <div class="kpi-change" style="color:var(--text4);visibility:hidden">-</div>
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

  <!-- Row 4: Daily Usage + Quick Actions -->
  <div class="cols-2-35" style="margin-bottom:0">
    <div class="card">
      <div class="card-header"><h2>Daily Usage (mL)</h2><span class="action">Week &rarr;</span></div>
      <div class="card-body">
        <div class="chart-placeholder">
          <div class="bar" style="height:55px"></div><div class="bar" style="height:70px"></div><div class="bar" style="height:42px"></div><div class="bar" style="height:85px"></div><div class="bar" style="height:50px"></div><div class="bar" style="height:75px"></div><div class="bar" style="height:60px"></div>
        </div>
        <div style="display:flex;justify-content:space-between;font-size:.63rem;color:var(--text4);margin-top:2px"><span>M</span><span>T</span><span>W</span><span>T</span><span>F</span><span>S</span><span>S</span></div>
      </div>
    </div>
    <div class="card">
      <div class="card-header"><h2>Quick Actions</h2></div>
      <div class="card-body">
        <div class="qa-grid">
          <button class="btn btn-primary" onclick="doseAll()">Dose All</button>
          <button class="btn btn-danger" onclick="estop()">E-Stop</button>
          <button class="btn btn-warning" onclick="primeAll()">Prime All</button>
          <button class="btn btn-outline" onclick="reboot()">Reboot</button>
          <button class="btn btn-outline" onclick="syncNTP()">Sync NTP</button>
          <button class="btn btn-outline" onclick="openPumpCountModal()">Pump Config</button>
        </div>
        <div class="time-row">
          <input type="text" id="manualTime" placeholder="YYYY-MM-DD HH:MM">
          <button class="btn btn-primary btn-sm" onclick="setManualTime()">Set</button>
        </div>
        <div class="timezone-label">&#9200; <select id="tzSelect" onchange="changeTimezone(this.value)"></select></div>
      </div>
    </div>
  </div>
</div>

<script>
const API='/api?path=';
const MAX_SCHED_PER_PUMP=4;
const defaultPins=[32,33,25,26,27,14,12,13];
const timezones=[
  'PST (UTC-8)','MST (UTC-7)','CST (UTC-6)','EST (UTC-5)',
  'AST (UTC-4)','BRT (UTC-3)','UTC','CET (UTC+1)','EET (UTC+2)',
  'MSK (UTC+3)','GST (UTC+4)','IST (UTC+5:30)','ICT (UTC+7)',
  'CST China (UTC+8)','JST (UTC+9)','AEST (UTC+10)','NZST (UTC+12)'
];
let currentTimezone='PST (UTC-8)';
let pumps=[], schedules=[], pumpCount=4;

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
function changeTimezone(val){currentTimezone=val;toast('Timezone set to '+val,'success')}

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
      '<td><div class="pump-actions"><input type="number" value="10" id="pumpVol'+i+'" style="width:42px"><button class="btn btn-success btn-sm" onclick="dosePump('+i+')">Dose</button><button class="btn btn-primary btn-sm" onclick="calibratePump('+i+')">Cal</button><button class="btn btn-warning btn-sm" onclick="primePump('+i+')">Prime</button></div></td>';
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
qsa('#ampmChips .chip').forEach(c=>{c.addEventListener('click',function(){qsa('#ampmChips .chip').forEach(x=>{x.style.background='transparent';x.style.color='var(--text3)';x.style.borderColor='var(--border)'});this.style.background='var(--blue)';this.style.color='#fff';this.style.borderColor='var(--blue)'})});
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
    const p=pumps[i]||{name:'Pump '+(i+1),capacity:5000,reservoirLevel:100};
    const pct=p.reservoirLevel===0?0:(p.reservoirLevel||100);
    const col=colorsR[i%colorsR.length];
    const cap=p.capacity||5000;
    const div=document.createElement('div');
    div.className='reservoir-row';
    div.innerHTML=
      '<div class="r-name"><span class="r-name-text" onclick="editReservoirName(this,'+i+')">'+p.name+'</span><div class="r-meta"><span class="r-cap-text" onclick="editReservoirCap(this,'+i+')">'+cap.toFixed(0)+' mL</span> capacity</div></div>'+
      '<div class="r-level"><div class="bar"><span style="width:'+pct+'%;background:'+col+'"></span></div></div>'+
      '<span class="r-pct" style="color:'+col+'">'+pct+'%</span>';
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

// === Logs ===
function renderLogs(data){
  const c=document.getElementById('logContainer');
  c.innerHTML=data.map(l=>'<div><span class="log-time">--</span><span class="log-info">'+l+'</span></div>').join('');
}

// === Status ===
function renderStatus(data){
  document.getElementById('kpiUptime').textContent=data.uptime.toFixed(1)+'h';
  const days=Math.floor(data.uptime/24);
  document.getElementById('kpiUptimeSub').textContent=days+' days';
  document.getElementById('kpiVolume').textContent=data.totalVolume.toFixed(0)+' mL';
  document.getElementById('kpiDoses').textContent=data.totalDoses;
  document.getElementById('kpiAlarms').textContent='0';
  document.getElementById('ipDisplay').textContent=data.ip;
  document.getElementById('onlineDot').style.color=data.wifiConnected?'var(--green)':'var(--red)';
  document.getElementById('onlineLabel').textContent=data.wifiConnected?'Online':'Offline';
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
function setManualTime(){
  const val=document.getElementById('manualTime').value;
  if(!val){toast('Enter time','error');return}
  fetch('/api/time?t='+encodeURIComponent(val)).then(()=>{toast('Time set: '+val,'success')});
}

// === Load All ===
function loadAll(){
  // skip if inline edit is active (input focused)
  if(document.querySelector('.pump-name-input'))return;
  fetchJSON(API+'status',renderStatus);
  fetchJSON(API+'pumps',function(data){pumps=data;pumpCount=data.length;renderPumps();renderReservoirs()});
  fetchJSON(API+'schedules',function(data){schedules=data;renderSchedules()});
  fetchJSON(API+'logs',renderLogs);
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
